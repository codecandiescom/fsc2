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

rs_rto_acq::rs_rto_acq( RS_RTO & rs )
    : m_rs( rs )
{
    reset( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_acq::reset( )
{
    // Let's try to keep the resolution constant as far as possible (and
    // as long as we're not asked to change it). Switch off the auto-adjust
    // feature as the documentation is completely unclear about what it
    // does (and we don't need any more undocumented "automatic help", it's
    // already hard enough to keep track of what the device does behind our
    // backs). And only use the real time data, not some made-up data - we
    // want to do real measurements and not get artificially created stuff
    // that just looks nice.

    m_rs.write( "ACQ:POIN:AUTO RES" );
    m_rs.write( "ACQ:POIN:AADJ 0" );
    m_rs.write( "ACQ:MODE RTIM" );

    // Get some, hopefully fixed data: the ADC rate, the smallest possible
    // record length and the upper limit for the time scale

    m_adc_rate = m_rs.query< double >( "ACQ:POIN:ARATE?" );
    m_max_scale = m_rs.query< double >( "TIM:SCAL? MAX" );
    m_min_scale = 100.0 /  m_adc_rate;

    // Set both the average and the segment count to the value of the
    // acquisition count "register" - it controls both

    m_avg_count = m_seg_count = m_rs.query< unsigned long >( "ACQ:COUNT?" );

    // Figure out what acquisition mode the device seems to be set to

    if ( m_rs.query< bool >( "ACQ:SEGM:STAT?" ) )
        m_mode = Acq_Mode::Segmented;
    else if ( m_rs.chans.avg_mode( ) )
        m_mode = Acq_Mode::Average;
    else
        m_mode = Acq_Mode::Normal;

#if 0
    for ( int i = enum_to_value( Channel::Math1 );
          i <= enum_to_value( Channel::Math4 ); ++i )
                m_rs.chans[ m_rs.s_channel_mapper.v2e( i ) ].set_arith_mode(
                                     m_mode == Acq_Mode::Normal ?
                                     Arith_Mode::Off : Arith_Mode::Average );
#endif

    // Never try to actually display segment data

    m_rs.write( "ACQ:SEGM:AUT 0" );

    // Make sure download limit setting is in a reasonable state

    download_limits_enabled( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_acq::timebase( )
{
    return m_rs.query< double >( "TIM:SCAL?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_acq::set_timebase( double tb )
{
    if (    tb <  shortest_timebase( ) - 0.5 / m_adc_rate
         || tb >= longest_timebase( )  + 0.5 / m_adc_rate )
        throw std::invalid_argument( "Time base out of range" );

    char buf[ 30 ];
    sprintf( buf, "TIM:SCAL %.10f", tb );
    m_rs.write( buf );
\
    return timebase( );
}


double
rs_rto_acq::shortest_timebase_const_resolution( )
{
    return std::max( shortest_timebase( ),
                     0.1 * min_record_length( ) * resolution( ) );
}


double
rs_rto_acq::longest_timebase_const_resolution( )
{
    return std::min( longest_timebase( ),
                     0.1 * max_record_length( ) * resolution( ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_acq::resolution( )
{
    return m_rs.query< double >( "ACQ:RES?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_acq::set_resolution( double res )
{
    if (    res >= lowest_resolution( )  + m_resolution_increment / 2
         || res <  highest_resolution( ) - m_resolution_increment / 2 )
        throw std::invalid_argument( "Resolution out of range" );

    char buf[ 30 ];
    sprintf( buf, "ACQ:RES %.10f", res );
    m_rs.write( buf );

    return resolution( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_acq::lowest_resolution( )
{
    return 10 * timebase( ) / min_record_length( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_acq::highest_resolution( )
{
    return std::max( 1 / m_adc_rate, 10 * timebase( ) / max_record_length( ) );
}


/*----------------------------------------------------*
 * Returns the current value of the record length
 *----------------------------------------------------*/

unsigned long
rs_rto_acq::record_length( )
{
    return m_rs.query< unsigned long >( "ACQ:POIN?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

unsigned long
rs_rto_acq::set_record_length( unsigned long rec_len )
{
    if (    rec_len < min_record_length( )
         || rec_len > max_record_length( ) )
        throw std::invalid_argument( "Record length out of range" );

    // Record length can only be set when "auto mode" is set to "RECL",
    // otherwise we'd get a "Function not available" error

    m_rs.write( "ACQ:POIN:AUTO RECL" );

    char buf[ 30 ];
    sprintf( buf, "ACQ:POIN %lu", rec_len );
    m_rs.write( buf );

    // Switch back "auto mode" to keeping the resolution constant

    m_rs.write( "ACQ:POIN:AUTO RES" );

    return record_length( );
}


/*----------------------------------------------------*
 * Returns the maximum record length possible under the
 * current conditions
 *----------------------------------------------------*/

unsigned long
rs_rto_acq::max_record_length( )
{
    m_rs.write( "ACQ:POIN:AUTO RECL" );
    unsigned long len = m_rs.query< unsigned long >( "ACQ:POIN? MAX" );
    m_rs.write( "ACQ:POIN:AUTO RES" );
    return len;
}


/*----------------------------------------------------*
 * Returns the maximum record length possible under the
 * current conditions
 *----------------------------------------------------*/

unsigned long
rs_rto_acq::min_record_length( )
{
    m_rs.write( "ACQ:POIN:AUTO RECL" );
    unsigned long len = m_rs.query< unsigned long >( "ACQ:POIN? MIN" );
    m_rs.write( "ACQ:POIN:AUTO RES" );
    return len;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

unsigned long
rs_rto_acq::set_average_count( unsigned long cnt )
{
    if (    cnt < 1
         || cnt > m_max_avg_count )
        throw std::invalid_argument( "Average count out of range" );

    return m_avg_count = cnt;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

unsigned long
rs_rto_acq::set_segment_count( unsigned long cnt )
{
    if ( cnt < 1 || cnt > max_segment_count( ) )
        throw std::invalid_argument( "Segment count out of range" );

    return m_seg_count = cnt;
}


/*----------------------------------------------------*
 * Returns the maximum number of segments that can be used
 * under the current circumstances
 *----------------------------------------------------*/

unsigned long
rs_rto_acq::max_segment_count( )
{
    if ( m_mode != Acq_Mode::Segmented )
        m_rs.write( "ACQ:SEGM:STAT 1" );

    unsigned long max_count = m_rs.query< unsigned long >( "ACQ:COUNT? MAX" );

    if ( m_mode != Acq_Mode::Segmented )
        m_rs.write( "ACQ:SEGM:STAT 0" );
    
    return max_count;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_acq::is_running( )
{
    return m_rs.query< unsigned int >( "STAT:OPER:COND?" ) & 0x10;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_acq::is_waiting_for_trigger( )
{
    return m_rs.query< unsigned int >( "STAT:OPER:COND?" ) & 0x8;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_acq::run( bool continuous )
{
    if ( is_running( ) )
        stop( );

    if ( m_mode == Acq_Mode::Average )
    {
        std::string cmd = "ACQ:COUNT ";
        char buf[ 20 ];
        sprintf( buf, " %zu", m_avg_count );
        m_rs.write( cmd + buf );

        if ( ! continuous )
            m_rs.write( "ACQ:ARES:MODE NONE" );
        else
        {
            m_rs.write( "ACQ:ARES:MODE WFMS" );
            cmd = "ACQ:ARES:WFMC ";
            m_rs.write( cmd + buf );
        }

        m_rs.write( "ACQ:ARES:IMM" );
    }
    else if ( m_mode == Acq_Mode::Segmented )
    {
        if ( continuous )
            throw operational_error( "Continuous not possible in segmented "
                                     "acquisition mode" );

        if ( m_seg_count > max_segment_count( ) )
            throw operational_error( "Maximum number of segments has dropped "
                                     "below the previously requested number" );

        char buf[ 30 ];
        sprintf( buf, "ACQ:COUNT %zu", m_seg_count );
        m_rs.write( buf );
    }

    m_rs.write( continuous ? "RUN" : "RUNS" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_acq::run_continuous( )
{
    run( true );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_acq::run_single( )
{
    run( false );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_acq::stop( )
{
    if ( ! is_running( ) )
        return false;

    m_rs.write( "STOP" );
    return true;
}


/*----------------------------------------------------*
 * Returns the reference position as the time after the
 * start of the waveform
 *----------------------------------------------------*/

double
rs_rto_acq::reference_position( )
{
    return 0.1 * m_rs.query< unsigned int >( "TIM:REF?" ) * timebase( );
}


/*----------------------------------------------------*
 * Sets the reference position as the time after the
 * start of the waveform
 *----------------------------------------------------*/

double
rs_rto_acq::set_reference_position( double pos )
{
    double tb = timebase( );
    pos /= 0.1 * tb;

    if ( pos < -0.5 || pos >= 100.5 )
        throw::std::invalid_argument( "Reference position out of range" );

    double old_ref_pos = 0.1 * m_rs.query< unsigned int >( "TIM:REF?" ) * tb;

    char buf[ 30 ];
    sprintf( buf, "TIM:REF %d", static_cast< int >( std::round( pos ) ) );
    m_rs.write( buf );

    double new_ref_pos = 0.1 * m_rs.query< unsigned int >( "TIM:REF?" ) * tb;

    double new_trig_pos = m_rs.trig.position( ) - new_ref_pos + old_ref_pos;

    double min_trig_pos = m_rs.trig.min_position( new_ref_pos );
    if ( new_trig_pos <   min_trig_pos
                        - m_rs.trig.m_position_resolution / 2 )
        new_trig_pos = min_trig_pos;
    else
    {
        double max_trig_pos = m_rs.trig.max_position( new_ref_pos );
        if ( new_trig_pos >=   max_trig_pos
                             + m_rs.trig.m_position_resolution / 2 )
            new_trig_pos = max_trig_pos;
    }

    m_rs.trig.set_position( new_trig_pos );

    return new_ref_pos;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

unsigned long
rs_rto_acq::available_segments( )
{
    return m_rs.query< unsigned long >( "ACQ:AVA?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Acq_Mode
rs_rto_acq::set_mode( Acq_Mode mode )
{
    if ( m_mode == mode )
        return m_mode;

    if ( m_mode == Acq_Mode::Average )
        m_rs.chans.set_avg_mode( false );
    else if ( mode == Acq_Mode::Average )
        m_rs.chans.set_avg_mode( true );

    if ( m_mode == Acq_Mode::Segmented )
        m_rs.write( "ACQ:SEGM:STAT 0" );
    else if ( mode == Acq_Mode::Segmented )
        m_rs.write( "ACQ:SEGM:STAT 1" );

#if 0
    if (    ( m_mode == Acq_Mode::Average && mode != Acq_Mode::Average )
         || ( m_mode != Acq_Mode::Average && mode == Acq_Mode::Average ) )
        for ( int i = enum_to_value( Channel::Math1 );
              i <= enum_to_value( Channel::Math4 ); ++i )
            m_rs.chans[ m_rs.s_channel_mapper.v2e( i ) ].set_arith_mode(
                                    mode == Acq_Mode::Normal ?
                                    Arith_Mode::Off : Arith_Mode::Average );
#endif

    return m_mode = mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_acq::download_limits_enabled( )
{
    Download_Mode mode = m_rs.query< Download_Mode >( "EXP:WAV:SCOP?" );

    if ( mode != Download_Mode::Waveform && mode != Download_Mode::Manual )
        return set_download_limits_enabled( false );

    return mode == Download_Mode::Manual;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_acq::set_download_limits_enabled( bool state )
{
    std::string cmd = "EXP:WAV:SCOP ";
    m_rs.write( cmd + ( state ? "MAN" : "WFM" ) );
    return state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

std::vector< double >
rs_rto_acq::download_limits( )
{
    std::string cmd = "EXP:WAV:";
    return std::vector< double >{ m_rs.query< double >( cmd + "START?" ),
                                  m_rs.query< double >( cmd + "STOP?"  ) };
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

std::vector< double >
rs_rto_acq::set_download_limits( std::vector< double > limits )
{
    if ( limits.size( ) < 2 )
        throw std::invalid_argument( "List contains not enough elements" );

    return set_download_limits( limits[ 0 ], limits[ 1 ] );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

std::vector< double >
rs_rto_acq::set_download_limits( double start,
                                 double end )
{
    double res = resolution( );

    if ( start >= end )
        throw std::invalid_argument( "Start not before end position" );

    if ( end - start <= res / 2 )
        throw std::invalid_argument( "Start and end position too near" );

    std::vector< double > limits = max_download_limits( );

    if (    start <= limits[ 0 ] - res / 2
         || end   >= limits[ 1 ] + res / 2 )
        throw std::invalid_argument( "Start and/or end value out of range" );

    char buf[ 40 ];
    sprintf( buf, "EXP:WAV:START %.10f", start );
    m_rs.write( buf );
    sprintf( buf, "EXP:WAV:STOP %.10f", end );
    m_rs.write( buf );

    return download_limits( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

std::vector< double >
rs_rto_acq::max_download_limits( )
{
    std::string cmd = "EXP:WAV:";
    return std::vector< double >{ m_rs.query< double >( cmd + "START? MIN" ),
                                  m_rs.query< double >( cmd + "STOP? MAX"  ) };
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
