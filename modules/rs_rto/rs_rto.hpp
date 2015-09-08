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


#pragma once
#if ! defined rs_rto_hpp_
#define rs_rto_hpp_


#include "rs_rto_base.hpp"
#include "rs_rto_chans.hpp"
#include "rs_rto_chan.hpp"
#include "rs_rto_trig.hpp"
#include "rs_rto_acq.hpp"


namespace rs_rto
{

/*----------------------------------------------------*
 *----------------------------------------------------*/

class RS_RTO : public rs_rto_base
{
    friend class rs_rto_chans;
    friend class rs_rto_chan;
    friend class rs_rto_ext_chan;
    friend class rs_rto_inp_chan;
    friend class rs_rto_math_chan;
    friend class rs_rto_trig;
    friend class rs_rto_acq;


  public:

    RS_RTO( std::string const & ip_address,
            Log_Level           log_level = Log_Level::High );


    std::string const &
    name( ) const
    {
        return m_device_name;
    }

    // Everyone can get references to the subsystems

  private:
    rs_rto_chans m_chans;
  public:
    rs_rto_chans & chans;
  private:
    rs_rto_trig m_trig;
  public:
    rs_rto_trig & trig;
  private:
    rs_rto_acq m_acq;
  public:
    rs_rto_acq & acq;

    // The python interface seems to need some methods to get at them

    rs_rto_trig &
    py_trig( )
    {
        return trig;
    }

    rs_rto_chans &
    py_chans( )
    {
        return chans;
    }

    rs_rto_acq &
    py_acq( )
    {
        return acq;
    }

    bool
    display_enabled( ) const
    {
        return m_display_state;
    }

    bool
    set_display_enabled( bool state );

    bool
    keyboard_locked( ) const
    {
        return m_keyboard_lock_state;
    }

    bool
    set_keyboard_locked( bool state );


  private:

    /* Disable copy constructor and assignment operator - there should be
       never more than a single instance for a single device (using the
       normal constructor for the same device fails due to device locking) */

    RS_RTO( RS_RTO const & ) = delete;

    RS_RTO &
    operator = ( RS_RTO const & ) = delete;

    bool m_display_state;

    bool m_keyboard_lock_state;
};

}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
