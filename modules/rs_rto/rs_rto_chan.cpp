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


#include "rs_rto_chan.hpp"
#include "rs_rto_ext_chan.hpp"
#include "rs_rto_inp_chan.hpp"

using namespace rs_rto;


/*----------------------------------------------------*
 *----------------------------------------------------*/

rs_rto_chan:: rs_rto_chan( RS_RTO & rs,
						   Channel  ch )
	: m_rs( rs )
	, m_channel( ch )
{
	if (    m_rs.m_num_channels == 2
		 && ( ch == Channel::Ch3 || ch == Channel::Ch4 ) )
		 m_channel_exists = false;

	char const * names[ ] = { "Channel::Ext",
							  "Channel::Ch1",
							  "Channel::Ch2",
							  "Channel::Ch3",
							  "Channel::Ch4",
							  "Channel::Math1",
							  "Channel::Math2",
							  "Channel::Math3",
							  "Channel::Math4" };

	m_name = names[ m_rs.s_channel_mapper.e2v( m_channel ) ];
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
