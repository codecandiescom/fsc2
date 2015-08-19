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
#include "rs_rto_math_chan.hpp"

using namespace rs_rto;


/*----------------------------------------------------*
 *----------------------------------------------------*/

rs_rto_math_chan::rs_rto_math_chan( RS_RTO  & rs,
									Channel   ch )
    : rs_rto_chan( rs, ch )
{
	int chan_number =   enum_to_value( m_channel )
		              - enum_to_value( Channel::Math1 ) + 1;
	char buf[ 7 ];
	sprintf( buf, "CALC:MATH%d", chan_number );
	m_prefix = buf;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_math_chan::state( )
{
    return m_rs.query< bool >( m_prefix + ":STAT?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_math_chan::set_state( bool on_off )
{
    std::string cmd = m_prefix + ":STAT ";
	m_rs.write( cmd + ( on_off ? '1' : '0' ) );
    return state( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Arith_Mode
rs_rto_math_chan::set_arith_mode( Arith_Mode mode )
{
    std::string cmd = m_prefix + ":ARIT ";
    m_rs.write( cmd + ( mode == Arith_Mode::Off ? "OFF" : "AVER" ) );
    return mode;
}



/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_math_chan::scale( )
{
    return m_rs.query< double >( m_prefix + ":VERT:SCAL?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_math_chan::set_scale( double sc )
{
 	if ( sc < m_min_scale || sc > m_max_scale )
		throw std::invalid_argument( "Math channel scale out of range" );

    char buf[ 40 ];
    sprintf( buf, "%s:VERT:SCAL %.8g", m_prefix.c_str( ), sc );
    m_rs.write( buf );

    return scale( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_math_chan::offset( )
{
    return m_rs.query< double >( m_prefix + ":VERT:OFFS?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
rs_rto_math_chan::set_offset( double offs )
{
 	if ( offs < m_min_offset || offs > m_max_offset )
		throw std::invalid_argument( "Math channel offset out of range" );

    char buf[ 40 ];
    sprintf( buf, "%s:VERT:OFFS %.8g", m_prefix.c_str( ), offs );
    m_rs.write( buf );

    return offset( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

std::string
rs_rto_math_chan::function( )
{
	std::string reply;
	m_rs.talk( m_prefix + '?', reply );
	return reply;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

std::string
rs_rto_math_chan::set_function( std::string const & f )
{
    if ( f.empty( ) )
        throw std::invalid_argument( "Can't set empty string as function" );

	if (    f.find( "'" ) != std::string::npos
		 || f.find( '"' ) != std::string::npos )
		throw std::invalid_argument( "Function may not contain quotes" );

	m_rs.write( m_prefix + " '" + f + "'" );

	int ec = 0;
	do
	{
		int ec = m_rs.query< int >( "SYST:ERR:CODE?" );
		if ( ec == -150 )
			throw std::invalid_argument( "Invalid function" );
	} while ( ec != 0 );

	return f;
}


/*----------------------------------------------------*
 * Fetches the header for a channels waveform - the 'start'
 * and 'end' fields in the 'Data_Header' structure are the
 * start and end time (in s) of the waveform relative to the
 * trigger event, the length field is the number of data
 * points in the waveform.
 *----------------------------------------------------*/

Data_Header
rs_rto_math_chan::header( )
{
    if ( ! state( ) )
        throw std::invalid_argument( "Can't fetch header for inactive "
                                     "channel" );

    return m_rs.query< Data_Header >( m_prefix + ":DATA:HEAD?" );
}


/*----------------------------------------------------*
 * Fetches the data from a channels waveform
 *----------------------------------------------------*/

std::vector< double >
rs_rto_math_chan::data( )
{
    if ( ! state( ) )
        throw std::invalid_argument( "Can't fetch data for inactive channel" );

    return m_rs.query< std::vector< double > >( m_prefix + ":DATA?" );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
