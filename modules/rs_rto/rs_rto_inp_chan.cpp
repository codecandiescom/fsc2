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
#include <cstdio>
#include "rs_rto_inp_chan.hpp"

using namespace rs_rto;


/*----------------------------------------------------*
 *----------------------------------------------------*/

rs_rto_inp_chan::rs_rto_inp_chan( RS_RTO  & rs,
								  Channel   ch )
    : rs_rto_chan( rs, ch )
{
	int chan_number =   enum_to_value( m_channel )
		              - enum_to_value( Channel::Ch1 ) + 1;
	char buf[ 7 ];
	sprintf( buf, "CHAN%d:", chan_number );
	m_prefix = buf;

	reset( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_inp_chan::reset( )
{
	m_coupling     = m_rs.query< Coupling >( m_prefix + "COUP?" );
	m_position     = m_rs.query< double >( m_prefix + "POS?" );
	m_impedance    = m_rs.query< double >( m_prefix + "IMP?" );
	m_bandwidth    = m_rs.query< Bandwidth >( m_prefix + "BAND?" );

    // If the ADC resolution is higher than the one we're asked to use
    // average the points the ADC produces for a single waveform point.

    m_rs.write( m_prefix + "TYPE HRES" );

    m_rs.write( m_prefix + "WAV2:STATE 0" );
    m_rs.write( m_prefix + "WAV3:STATE 0" );

    m_arith_mode = m_rs.query< Arith_Mode >( m_prefix + "ARIT?" );
    if ( m_arith_mode == Arith_Mode::Envelope )
        set_arith_mode( Arith_Mode::Off );

    m_rs.write( m_prefix + "HIST:STAT 0" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_inp_chan::state( )
{
    return m_rs.query< bool >( m_prefix + "STAT?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_inp_chan::set_state( bool on_off )
{
    std::string cmd = m_prefix + "STAT ";
	m_rs.write( cmd + ( on_off ? '1' : '0' ) );
    return state( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_inp_chan::is_overloaded( bool reset )
{
	bool ovl = m_rs.query< bool >( m_prefix + "OVER?" );
	if ( reset && ovl )
		m_rs.write( m_prefix + "OVER OFF" );
	return ovl;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Coupling
rs_rto_inp_chan::set_coupling( Coupling coupling )
{
	std::string cmd = m_prefix + "COUP ";
	if ( coupling == Coupling::DC50 )
		cmd += "DC";
	else if ( coupling == Coupling::DC1M )
		cmd += "DCL";
	else if ( coupling == Coupling::AC )
		cmd += "AC";

	m_rs.write( cmd );

	// Change of coupling to something else than DC also may change the
	// bandwidth setting (800 MHz bandwidth only being possible with
	// 50 Ohm DC coupling)

	if ( coupling != Coupling::DC50 )
		m_bandwidth = m_rs.query< Bandwidth >( m_prefix + "BAND?" );

	return m_coupling = coupling;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::scale( )
{
    return m_rs.query< double >( m_prefix + "SCAL?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::set_scale( double sc )
{
	if (    sc < min_scale( ) - m_scale_resolution / 2
         || sc >= max_scale( ) + m_scale_resolution / 2 )
		throw std::invalid_argument( "Channel scale out of range" );

	std::string cmd = m_prefix + "SCAL ";
	char buf[ 20 ];
	sprintf( buf, "%.4f", sc );
	m_rs.write( cmd + buf );

	return scale( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::min_scale( )
{
    return m_rs.query< double >( m_prefix + "SCAL? MIN" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::max_scale( )
{
    return m_rs.query< double >( m_prefix + "SCAL? MAX" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::offset( )
{
    return m_rs.query< double >( m_prefix + "OFFS?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::set_offset( double offs )
{
    double max = m_rs.query< double >( m_prefix + "OFFS? MAX" );
    double min = m_rs.query< double >( m_prefix + "OFFS? MIN" );

    if (    offs < min - m_offset_resolution / 2
         || offs >= max + m_offset_resolution / 2 )
		throw std::invalid_argument( "Channel offset out of range" );

	std::string cmd = m_prefix + "OFFS ";
	char buf[ 20 ];
	sprintf( buf, "%.6f", offs );
	m_rs.write( cmd + buf );

	return offset( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::min_offset( )
{
    return m_rs.query< double >( m_prefix + "OFFS? MIN" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::max_offset( )
{
    return m_rs.query< double >( m_prefix + "OFFS? MAX" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::set_position( double position )
{
	if ( std::fabs( position ) >= m_max_position + m_position_resolution / 2 )
		throw std::invalid_argument( "Channel position out of range" );

	std::string cmd = m_prefix + "POS ";
	char buf[ 20 ];
	sprintf( buf, "%.3f", position );

	m_rs.write( cmd + buf );
	return m_position = position;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_inp_chan::set_impedance( double impedance )
{
	if (    impedance <  m_min_impedance - m_impedance_resolution / 2
		 || impedance >= m_max_impedance + m_impedance_resolution / 2 )
		throw std::invalid_argument( "Channel impedance out of range" );

	impedance =   m_impedance_resolution
		        * round( impedance / m_impedance_resolution );

	std::string cmd = m_prefix + "IMP ";
	char buf[ 20 ];
	sprintf( buf, "%.1f", impedance );

	m_rs.write( cmd + buf );
	return m_impedance = impedance;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Bandwidth
rs_rto_inp_chan::set_bandwidth( Bandwidth bandwidth )
{
	Model model = m_rs.model( );

	if ( bandwidth == Bandwidth::MHz800 )
	{
		if (    model == Model::RTO1002
			 || model == Model::RTO1004 )
			throw std::invalid_argument( "800 MHz bandwidth only possible with "
										 "with models with bandwidth >= 1GHz" );
		if ( m_coupling != Coupling::DC50 )
			throw std::invalid_argument( "800 MHz bandwidth only possible with "
									 "DC50 coupling" );
	}

	std::string cmd = m_prefix + "BAND ";
	if ( bandwidth == Bandwidth::Full )
		cmd += "FULL";
	else if ( bandwidth == Bandwidth::MHz20 )
		cmd += "B20";
	else if ( bandwidth == Bandwidth::MHz200 )
		cmd += "B200";
	else if ( bandwidth == Bandwidth::MHz800 )
		cmd += "B800";

	m_rs.write( cmd );
	return m_bandwidth = bandwidth;
}


/*----------------------------------------------------*
 * Fetches the header for a channels waveform - the 'start'
 * and 'end' fields in the 'Data_Header' structure are the
 * start and end time (in s) of the waveform relative to the
 * trigger event, the length field is the number of data
 * points in the waveform.
 *----------------------------------------------------*/

Data_Header
rs_rto_inp_chan::header( )
{
    if ( ! state( ) )
        throw std::invalid_argument( "Can't fetch header for inactive "
                                     "channel" );

    return m_rs.query< Data_Header >( m_prefix + "DATA:HEAD?" );
}


/*----------------------------------------------------*
 * Fetches the data from a channels waveform
 *----------------------------------------------------*/

std::vector< double >
rs_rto_inp_chan::data( )
{
    if ( ! state( ) )
        throw std::invalid_argument( "Can't fetch data for inactive channel" );

    return m_rs.query< std::vector< double > >( m_prefix + "DATA?" );
}


std::vector< std::vector< double > >
rs_rto_inp_chan::segment_data( unsigned long start,
                               unsigned long count )
{
    if ( ! state( ) )
        throw std::invalid_argument( "Can't fetch segment data for inactive "
                                     "channel" );

    if ( m_rs.acq.is_running( ) )
        throw operational_error( "Segmented data can't be fetched while "
                                 "acquisition is still underway" );

    std::vector< std::vector< double > > res;

    unsigned long avail = m_rs.acq.available_segments( );
    if ( avail == 0 )
        throw operational_error( "No data available" );

    if ( start >= avail )
        throw std::invalid_argument( "Start index argument not less than "
                                     "number of avaiable segments" );

    if ( count == 0 )
        count = avail - start;
    else if ( start + count > avail )
        throw std::invalid_argument( "Start index plus count larger than "
                                     "number of avaiable segments" );

    // To get at the data history display must be switched on

    m_rs.write( m_prefix + "HIST:STAT 1" );

    // Iterate over all history curves (starting with the oldest one): get
    // them to be displayed and thus copied to the memory we can read them
    // from and get the data. Before reading the data tell the device only
    // to reply once it has finished all previous commands - while this,
    // unfortunately, requires some extra time, it was observed that leaving
    // this out resulted in pairs of downloaded segments being identical.

    char buf[ 40 ] = "HIST:CURR ";
    for ( unsigned long i = start; i < start + count; ++i )
    {
        sprintf( buf + 10, "%ld;*WAI", 1 - avail + i );
        m_rs.write( m_prefix + buf );
        res.push_back( m_rs.query< std::vector< double > >(   m_prefix
                                                            + "DATA?" ) );
    }
    
    // Switch off history display

    m_rs.write( m_prefix + "HIST:STAT 0" );
    return res;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Arith_Mode
rs_rto_inp_chan::set_arith_mode( Arith_Mode mode )
{
    if ( mode == Arith_Mode::Off )
        m_rs.write( m_prefix + "ARIT OFF" );
    else if ( mode == Arith_Mode::Average )
        m_rs.write( m_prefix + "ARIT AVER" );
    else
        throw std::invalid_argument( "Envelope arithmetic mode not supported" );

    return m_arith_mode = mode;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
