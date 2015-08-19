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
#include "rs_rto_ext_chan.hpp"
#include "rs_rto_inp_chan.hpp"
#include "rs_rto_math_chan.hpp"

using namespace rs_rto;


/*----------------------------------------------------*
 *----------------------------------------------------*/

rs_rto_chans::rs_rto_chans( RS_RTO & rs )
	: m_rs( rs )
{
    // Create the external trigger channel

    std::unique_ptr< rs_rto_chan > c( new rs_rto_ext_chan( m_rs,
                                                           Channel::Ext ) );
    m_channels.push_back( std::move( c ) );

    // Create as many input channels as there are and then, if necessary
    // dummy channels for non-existing input channels

    bool mode_found = false;

	for ( int i = enum_to_value( Channel::Ch1 ); i <= m_rs.m_num_channels; ++i )
	{
		Channel ch = m_rs.s_channel_mapper.v2e( i );
		std::unique_ptr< rs_rto_chan > c( new rs_rto_inp_chan( m_rs, ch ) );
		m_channels.push_back( std::move( c ) );

        if ( ! mode_found && m_channels.back( )->state( ) )
        {
            mode_found = true;
            m_avg_mode =
                      m_channels.back( )->arith_mode( ) == Arith_Mode::Average;
        }
	}

    // Create a set of dummy channels for non-existing measurement channels

    for ( int i = m_rs.m_num_channels + 1;
          i <= enum_to_value( Channel::Ch4 ); ++i )
    {
		Channel ch = m_rs.s_channel_mapper.v2e( i );
		std::unique_ptr< rs_rto_chan > c( new rs_rto_chan( m_rs, ch ) );
		m_channels.push_back( std::move( c ) );
	}

    for ( int i = enum_to_value( Channel::Math1 );
          i <= enum_to_value( Channel::Math4 ); ++i )
    {
		Channel ch = m_rs.s_channel_mapper.v2e( i );
		std::unique_ptr< rs_rto_chan > c( new rs_rto_math_chan( m_rs, ch ) );
		m_channels.push_back( std::move( c ) );
	}

    if ( ! mode_found )
        m_avg_mode = false;

    set_avg_mode( m_avg_mode );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

size_t
rs_rto_chans::ch2index( Channel ch ) const
{
    return m_rs.s_channel_mapper.e2v( ch );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_chans::set_avg_mode( bool mode )
{
    for ( int i = 1; i <= m_rs.num_channels( ); ++i )
        m_channels[ i ]->set_arith_mode( mode
                                         ? Arith_Mode::Average
                                         : Arith_Mode::Off );

    return m_avg_mode = mode;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
