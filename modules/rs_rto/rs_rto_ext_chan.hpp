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
#if ! defined rs_rto_ext_chan_hpp_
#define rs_rto_ext_chan_hpp_

#include "rs_rto.hpp"


namespace rs_rto
{

/*----------------------------------------------------*
 * Class for the EXT trigger input channel, in combination
 * with controlling the EXT TRIGGER OUT settings.
 *----------------------------------------------------*/

class rs_rto_ext_chan : public rs_rto_chan
{
	friend class rs_rto_chans;


  public:

	Coupling
	coupling( ) const override
	{
		return m_coupling;
	}

	Coupling
	set_coupling( Coupling coupling ) override;

    Filter_Type
    filter_type( ) const override
    {
        return m_filter_type;
    }

    Filter_Type
    set_filter_type( Filter_Type type ) override;

    Filter_Cut_Off
    cut_off( ) const override
    {
        return m_cut_off;
    }

    Filter_Cut_Off
    set_cut_off( Filter_Cut_Off cut_off ) override;

    bool
    is_overloaded( bool confirm ) override;

  private:

	rs_rto_ext_chan( RS_RTO & rs,
					 Channel  ch )
		: rs_rto_chan( rs, ch )
	{
		reset( );
	}

    rs_rto_ext_chan( rs_rto_ext_chan const & ) = delete;

    rs_rto_ext_chan &
    operator = ( rs_rto_ext_chan const & ) = delete;

	void
	reset( );

	Coupling m_coupling;

    Filter_Type m_filter_type;

    Filter_Cut_Off m_cut_off;
};

}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
