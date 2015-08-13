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


#include "rs_rto.hpp"

using namespace rs_rto;


/*----------------------------------------------------*
 *----------------------------------------------------*/

RS_RTO::RS_RTO( std::string const & ip_address,
				Log_Level           log_level )
    : rs_rto_base( ip_address, log_level )
    , m_chans( *this    )
    , chans(    m_chans )
    , m_trig(  *this    )
    , trig(     m_trig  )
    , m_acq(   *this    )
    , acq(      m_acq  )
    , m_display_state( query< bool >( "SYST:DISP:UPD?" ) )
    , m_keyboard_lock_state( query< bool >( "SYST:KLOC?" ) )
{
}


/*----------------------------------------------------*
 * Enables or disables the device's display
 *----------------------------------------------------*/

bool
RS_RTO::set_display_enabled( bool state )
{
    std::string cmd = "SYST:DISP:UPD ";
    write( cmd + ( state ? '1' : '0' ) );
    return m_display_state = state;
}


/*----------------------------------------------------*
 * Enables or disables the device's keyboard
 *----------------------------------------------------*/

bool
RS_RTO::set_keyboard_locked( bool state )
{
    std::string cmd = "SYST:KLOC ";
    write( cmd + ( state ? '1' : '0' ) );
    return m_keyboard_lock_state = state;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
