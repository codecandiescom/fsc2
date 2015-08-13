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


#include "rs_rto_ext_chan.hpp"

using namespace rs_rto;


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_ext_chan::reset( )
{
	m_coupling     = m_rs.query< Coupling >( "TRIG:ANED:COUP?" );

    m_filter_type = m_rs.query< Filter_Type >( "TRIG:ANED:FILT?" );

    if ( m_filter_type != Filter_Type::High_Pass )
        m_cut_off = m_rs.query< Filter_Cut_Off >( "TRIG:ANED:CUT:LOWP?" );
    else
        m_cut_off = m_rs.query< Filter_Cut_Off >( "TRIG:ANED:CUT:HIGH?" );

    set_cut_off( m_cut_off );
}


/*----------------------------------------------------*
 * Sets the coupling for external trigger (DC at 50 or
 * 1M Ohm or AC)
 *----------------------------------------------------*/

Coupling
rs_rto_ext_chan::set_coupling( Coupling coupling )
{
	std::string cmd = "TRIG1:ANED:COUP ";
	if ( coupling == Coupling::DC50 )
		cmd += "DC";
	else if ( coupling == Coupling::DC1M )
		cmd += "DCL";
	else if ( coupling == Coupling::AC )
		cmd += "AC";

	m_rs.write( cmd );
	return m_coupling = coupling;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_ext_chan::is_overloaded( bool confirm )
{
	if ( ! m_rs.query< bool >( "TRIG1:EXT:OVER?" ) )
		return false;

	if ( confirm )
 		m_rs.write( "TRIG1:EXT:OVER 0" );

	return true;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Filter_Type
rs_rto_ext_chan::set_filter_type( Filter_Type type )
{
    std::string cmd = "TRIG:ANED:FILT ";
    switch ( type )
    {
        case Filter_Type::Off :
            cmd += "OFF";
            break;

        case Filter_Type::Low_Pass :
            cmd += "RFR";
            break;

        case Filter_Type::High_Pass :
            cmd += "LFR";
            break;
    }

    m_rs.write( cmd );
    return m_filter_type = type;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Filter_Cut_Off
rs_rto_ext_chan::set_cut_off( Filter_Cut_Off cut_off )
{
    std::string cmd = "TRIG:ANED:CUT:LOWP ";
    switch ( cut_off )
    {
        case Filter_Cut_Off::kHz5 :
            cmd += "KHZ5";
            break;

        case Filter_Cut_Off::kHz50 :
            cmd += "KHZ50";
            break;

        case Filter_Cut_Off::MHz50 :
            cmd += "MHZ50";
            break;

    }

    m_rs.write( cmd );

    cmd.replace( cmd.find( "LOWP" ), 4, "HIGH" );
    m_rs.write( cmd );

    return m_cut_off = cut_off;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
