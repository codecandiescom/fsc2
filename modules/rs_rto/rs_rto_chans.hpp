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
#if ! defined rs_rto_chans_hpp_
#define rs_rto_chans_hpp_


#include <memory>
#include "rs_rto.hpp"


namespace rs_rto
{

class rs_rto_chan;
class RS_RTO;

/*----------------------------------------------------*
 *----------------------------------------------------*/

class rs_rto_chans
{
	friend class RS_RTO;
    friend class rs_rto_acq;


  public:

    rs_rto_chan &
    operator [ ] ( Channel ch )
    {
        return *m_channels[ ch2index( ch ) ];
    }

    rs_rto_chan const &
    operator [ ] ( Channel ch ) const
    {
        return *m_channels[ ch2index( ch ) ];
    }

    // Helper method for the Python interface for overloading '[]'

    rs_rto_chan &
    __getitem__( Channel ch )
    {
        return *m_channels[ ch2index( ch ) ];
    }


  private:

	rs_rto_chans( RS_RTO & rs );

    rs_rto_chans( rs_rto_chans const & ) = delete;

    rs_rto_chans &
    operator = ( rs_rto_chans const & ) = delete;

    size_t
    ch2index( Channel ch ) const;

    bool
    avg_mode( ) const
    {
        return m_avg_mode;
    }

    bool
    set_avg_mode( bool mode );


    RS_RTO & m_rs;

	std::vector< std::unique_ptr< rs_rto_chan > > m_channels;

    bool m_avg_mode;
};

}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
