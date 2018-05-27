/*
 *  Copyright (C) 2015 Jens Thoms Toerring
 *
 *  This file is part of Fsc3.
 *
 *  Fsc3 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 *  Fsc3 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <cmath>
#include "rs_rto.hpp"


using namespace rs_rto;


/*----------------------------------------------------*                         
 *----------------------------------------------------*/

rs_rto_trig::rs_rto_trig( RS_RTO & rs )
    : m_rs( rs )
{
    reset( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_trig::reset( )
{
    // Use only a single trigger event, not sequences

    m_rs.write( "TRIG:SEQ:MODE AONL" );

    // Don't use any hold-off, i.e. react to the next trigger as fast
    // as possible

    m_rs.write( "TRIG:HOLD:MODE OFF" );

    m_mode = m_rs.query< Trig_Mode >( "TRIG1:MODE?" );

    // Switch off "hysteresis", it's not needed for our purposes (we're only
    // interested into simple, single edge triggers) and it introduces too
    // many interactions where the behaviour of the device isn't documented.
    // Hysteresis is disabled by setting the "noise mode" to manual and the
    // (absolute) hysteresis level to 0 for all normal channels as well as the
    // external trigger input channel.

    for ( int i = 1; i <= m_rs.num_channels( ); ++i )
    {
        char buf[ 30 ];
        sprintf( buf, "TRIG:LEV%d:NOIS:MODE ABS", i );
        m_rs.write( buf );

        sprintf( buf, "TRIG:LEV%d:NOIS MAN", i );
        m_rs.write( buf );

        sprintf( buf, "TRIG:LEV%d:NOIS:ABS 0", i );
        m_rs.write( buf );
    }

    m_rs.write( "TRIG:ANED:NREJ 0" );
    m_rs.write( "TRIG:LEV5:NOIS:MODE ABS" );
    m_rs.write( "TRIG:LEV5:NOIS MAN" );

    // Contrary to what's written in the manual the hysteresis noise level
    // can't be set to 0 for the external trigger, there's an undocumented
    // lower limit we have to request from the device.

    double min_noise = m_rs.query< double >( "TRIG:LEV5:NOIS:ABS? MIN" );

    char buf[ 30 ];
    sprintf( buf, "TRIG:LEV5:NOIS:ABS %.3f", min_noise );
    
    // Don't restrict the trigger position to the visible waveform

    m_rs.write( "TRIG:OFFS:LIM 0" );

    // The trigger source might be set to something we don't support, in
    // that case catch the resulting exception and default to external
    // trigger

    try
    {
        m_source = m_rs.query< Channel >( "TRIG1:SOUR?" );
    }
    catch ( std::invalid_argument const & )
    {
        set_source( Channel::Ext );
    }

    // Set trigger type to edge trigger

    std::string cmd;
    if ( m_source == Channel::Ext )
        cmd = "TRIG1:TYPE ANED";
    else
        cmd = "TRIG1:TYPE EDGE";
    m_rs.write( cmd );

    m_slope[ 0 ] = m_rs.query< Trig_Slope >( "TRIG1:ANED:SLOPE?" );
    m_slope[ 1 ] = m_rs.query< Trig_Slope >( "TRIG:EDGE:SLOPE?" );
    for ( int i = 2; i <= 4; ++i )
        m_slope[ i ] = m_slope[ 1 ];

    m_state        = m_rs.query< bool >( "TRIG:OUT:STATE?" );
    m_polarity     = m_rs.query< Polarity >( "TRIG:OUT:POL?" );
    m_pulse_length = m_rs.query< double >( "TRIG:OUT:PLEN?" );
}


/*----------------------------------------------------*
 * Sets the trigger source channel (one of the available
 * input channels or rthe external input)
 *----------------------------------------------------*/

Channel
rs_rto_trig::set_source( Channel source )
{
    int ind = check_channel( source );

    std::string cmd = "TRIG1:SOUR ";
    ind = enum_to_value( source );
    if ( ind == 0 )
        cmd += "EXT";
    else
    {
        char buf[ ] = "CHANx";
        buf[ 4 ] = ind + '0';
        cmd += buf;
    }

    m_rs.write( cmd );

    cmd = source == Channel::Ext ? "TRIG1:TYPE ANED" : "TRIG1:TYPE EDGE";
    m_rs.write( cmd );

    m_source = source;
    set_slope( m_slope[ ind ] );

    return m_source;
}


/*----------------------------------------------------*
 * Sets the trigger mode (Auto/Normal/Free_Runing)
 *----------------------------------------------------*/

Trig_Mode
rs_rto_trig::set_mode( Trig_Mode mode )
{
    std::string cmd = "TRIG1:MODE ";

    if ( mode == Trig_Mode::Auto )
        cmd += "AUTO";
    else if ( mode == Trig_Mode::Normal )
        cmd += "NORM";
    else if ( mode == Trig_Mode::Free_Running )
        cmd += "FRE";

    m_rs.write( cmd );
    return m_mode = mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Trig_Slope
rs_rto_trig::slope( ) const
{
    return slope( m_source );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Trig_Slope
rs_rto_trig::slope( Channel ch ) const
{
    return m_slope[ check_channel( ch ) ];
}


/*----------------------------------------------------*
 * Sets the trigger slope (Positive/Negative/Either)
 *----------------------------------------------------*/

Trig_Slope
rs_rto_trig::set_slope( Trig_Slope slope )
{
    return set_slope( m_source, slope );
}


/*----------------------------------------------------*
 * Sets the trigger slope (Positive/Negative/Either)
 *----------------------------------------------------*/

Trig_Slope
rs_rto_trig::set_slope( Channel    ch,
                        Trig_Slope slope )
{
    int ind = check_channel( ch );

    if ( ch != m_source )
        return m_slope[ ind ] = slope;

    std::string cmd = "TRIG1:";

    if ( ch == Channel::Ext )
        cmd += "ANED:";
    else
        cmd += "EDGE:";
    cmd += "SLOP ";

    switch ( slope )
    {
        case Trig_Slope::Positive :
            cmd += "POS";
            break;

        case Trig_Slope::Negative :
            cmd += "NEG";
            break;

        case Trig_Slope::Either :
            if ( ch == Channel::Ext )
                throw std::invalid_argument( "Triggering on positive and "
                                             "negative slope not possible for "
                                             "external trigger input" );
            cmd += "EITH";
            break;
    }

    m_rs.write( cmd );

    return m_slope[ ind ] = slope;
}


/*----------------------------------------------------*
 * Returns the trigger level for the current rigger source
 * channel - we need to ask for it each time since all kinds
 * of other calls may have changed it as a side-effect.
 *----------------------------------------------------*/

double
rs_rto_trig::level( )
{
    return level( m_source );
}


/*----------------------------------------------------*
 * Returns the trigger level for a certain channel
 *----------------------------------------------------*/

double
rs_rto_trig::level( Channel ch )
{
    int ind = check_channel( ch );

    char level_index[ ] = { '5', '1', '2', '3', '4' };

    std::string cmd = "TRIG1:LEV";
    cmd += level_index[ ind ];
    cmd += '?';

    return m_rs.query< double >( cmd );
}


/*----------------------------------------------------*
 * Sets the trigger level for the current trigger source channel.
 * There's no reliable, documented way of finding out about the
 *range posible under the current circumstances except asking the
 * device - let's pray this is reliable...
 *----------------------------------------------------*/

double
rs_rto_trig::set_level( double level )
{
    return set_level( m_source, level );
}


/*----------------------------------------------------*
 * Sets the trigger level for the current trigger source channel.
 * There's no reliable, documented way of finding out about the
 *range posible under the current circumstances except asking the
 * device - let's pray this is reliable...
 *----------------------------------------------------*/

double
rs_rto_trig::set_level( Channel ch,
                        double  level )
{
    int ind = check_channel( ch );

    if (    level <  min_level( ch ) - m_level_resolution / 2
         || level >= max_level( ch ) + m_level_resolution / 2 )
        throw std::invalid_argument( "Trigger level out of range" );

    std::vector< std::string > lev = { "LEV5", "LEV1", "LEV2", "LEV3", "LEV4" };

    std::string cmd = "TRIG1:" + lev[ ind ];
    char buf[ 10 ];
    sprintf( buf, " %.3f", level );
    m_rs.write( cmd + buf );

    return m_rs.query< double >( cmd + "?" );
}


/*----------------------------------------------------*
 * Returns the minimum trigger level for the current trigger
 * source channel possible under the current circumstances
 *----------------------------------------------------*/

double
rs_rto_trig::min_level( )
{
    return min_level( m_source );
}


/*----------------------------------------------------*
 * Returns the minimum trigger level for the given trigger
 * souce channel  possible under the current circumstances
 *----------------------------------------------------*/

double
rs_rto_trig::min_level( Channel ch )
{
    int ind = check_channel( ch );
    std::vector< std::string > lev = { "LEV5", "LEV1", "LEV2", "LEV3", "LEV4" };
    return m_rs.query< double >( "TRIG1:" + lev[ ind ] + "? MIN" );
}


/*----------------------------------------------------*
 * Returns the maximum trigger level possible under the
 * current circumstances
 *----------------------------------------------------*/

double
rs_rto_trig::max_level( )
{
    return max_level( m_source );
}


/*----------------------------------------------------*
 * Returns the maximum trigger level possible under the
 * current circumstances
 *----------------------------------------------------*/

double
rs_rto_trig::max_level( Channel ch )
{
    int ind = check_channel( ch );
    std::vector< std::string > lev = { "LEV5", "LEV1", "LEV2", "LEV3", "LEV4" };
    return m_rs.query< double >( "TRIG1:" + lev[ ind ] + "? MAX" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_trig::is_overloaded( )
{
    return m_rs.chans[ m_source ].is_overloaded( );
}


/*----------------------------------------------------*
 * Returns the trigger position relative to the start of
 * the waveform. A positive value indicates that the rigger
 * was sometime before the measurement started, a negative
 * value means that the measurement was started before the
 * trigger was received.
 *----------------------------------------------------*/

double
rs_rto_trig::position( )
{
    return   m_rs.acq.reference_position( )
           - m_rs.query< double >( "TIM:HOR:POS?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_trig::set_position( double pos )
{
    double ref_pos = m_rs.acq.reference_position( );

    if (    pos <  min_position( ref_pos ) - m_position_resolution / 2
         || pos >= max_position( ref_pos ) + m_position_resolution / 2 )
        throw std::invalid_argument( "Trigger position out of range" );

    char buf[ 60 ];
    sprintf( buf, "TIM:HOR:POS %.12f", ref_pos - pos );
    m_rs.write( buf );

    return m_rs.query< double >( "TIM:HOR:POS?" ) - ref_pos;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_trig::earliest_position( )
{
    return min_position( m_rs.acq.reference_position( ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_trig::latest_position( )
{
    return max_position( m_rs.acq.reference_position( ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_trig::min_position( double ref_pos )
{
    return ref_pos - m_rs.query< double >( "TIM:HOR:POS? MAX" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_trig::max_position( double ref_pos )
{
    return ref_pos - m_rs.query< double >( "TIM:HOR:POS? MIN" );
}


/*----------------------------------------------------*
 * Forces a trigger when in "NORMAL" trigger mode
 *----------------------------------------------------*/

void
rs_rto_trig::raise( )
{
    m_rs.write( "TRIG:FORC" );
}


/*----------------------------------------------------*
 * Enables or disables the signal at EXT TRIGGER OUT
 * when a trigger occured
 *----------------------------------------------------*/

bool
rs_rto_trig::set_out_pulse_state( bool state )
{
    std::string cmd = "TRIG:OUT:STATE ";
    m_rs.write( cmd + ( state ? "1" : "0" ) );
    return m_state = state;
}                


/*----------------------------------------------------*
 * Sets the polarity of the pulse at EXT TRIGGER OUT
 * when a trigger occured
 *----------------------------------------------------*/

Polarity
rs_rto_trig::set_out_pulse_polarity( Polarity pol )
{
    std::string cmd = "TRIG:OUT:POL ";
    m_rs.write( cmd + ( pol == Polarity::Positive ? "POS" : "NEG" ) );
    return m_polarity = pol;
}                


/*----------------------------------------------------*
 * Sets the length of the pulse at EXT TRIGGER OUT
 * when a trigger occured
 *----------------------------------------------------*/

double
rs_rto_trig::set_out_pulse_length( double len )
{
    if (    len <  m_min_pulse_length - m_pulse_resolution / 2
         || len >= m_max_pulse_length + m_pulse_resolution / 2 )
        throw std::invalid_argument( "Trigger out pulse length out of range" );

    len = m_pulse_resolution * round( len / m_pulse_resolution );

    std::string cmd = "TRIG:OUT:PLEN ";
    char buf[ 30 ];
    sprintf( buf, "%.9f", len );

    m_rs.write( cmd + buf );
    return m_pulse_length = len;
}


/*----------------------------------------------------*
 * Returns the length of the pulse at EXT TRIGGER OUT
 * when a trigger occured
 *----------------------------------------------------*/

double
rs_rto_trig::out_pulse_delay( )
{
    return m_rs.query< double >( "TRIG:OUT:DEL?" );
}


/*----------------------------------------------------*
 * Sets the length of the pulse at EXT TRIGGER OUT
 * when a trigger occured
 *----------------------------------------------------*/

double
rs_rto_trig::set_out_pulse_delay( double delay )
{
    if (    delay <  min_out_pulse_delay( ) - m_out_delay_resolution / 2
         || delay >= max_out_pulse_delay( ) + m_out_delay_resolution / 2 )
        throw std::invalid_argument( "Trigger out delay out of range" );

    delay = m_out_delay_resolution * round( delay / m_out_delay_resolution );

    std::string cmd = "TRIG:OUT:DEL ";
    char buf[ 30 ];
    sprintf( buf, "%.9f", delay );
    m_rs.write( cmd + buf );

    return out_pulse_delay( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_trig::min_out_pulse_delay( )
{
    return m_rs.query< double >( "TRIG:OUT:DEL? MIN" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_trig::max_out_pulse_delay( )
{
    return m_rs.query< double >( "TRIG:OUT:DEL? MAX" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trig::check_channel( Channel ch ) const
{
    switch ( ch )
    {
        case Channel::Ext :
        case Channel::Ch1:
        case Channel::Ch2 :
            return enum_to_value( ch );

        case Channel::Ch3 :
            if ( ! m_rs.chans[ ch ].exists( ) )
                throw std::invalid_argument(   m_rs.chans[ ch ].name( )
                                             + " doesn't exist" );
            return 3;

        case Channel::Ch4 :
            if ( ! m_rs.chans[ ch ].exists( ) )
                throw std::invalid_argument(   m_rs.chans[ ch ].name( )
                                             + " doesn't exist" );
            return 4;

        default :
            throw std::invalid_argument(   m_rs.chans[ ch ].name( ) + "can't "
                                         + "be used as a trigger channel" );
    }
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
