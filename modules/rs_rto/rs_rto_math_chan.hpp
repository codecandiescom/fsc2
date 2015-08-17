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


#if ! defined rs_rto_math_chan_hpp_
#define rs_rto_math_chan_hpp_


#include "rs_rto.hpp"
#include "rs_rto_chan.hpp"


namespace rs_rto
{

class RS_RTO;

/*----------------------------------------------------*
 *----------------------------------------------------*/


class rs_rto_math_chan : public rs_rto_chan
{
	friend class rs_rto_chans;
	friend class rs_rto_chan;


  public:

	bool
	state( );

	bool
	set_state( bool state );

    Arith_Mode
    set_arith_mode( Arith_Mode mode );

    double scale( );

    double set_scale( double sc );

    double offset( );

    double set_offset( double sc );

	std::string
	function( );

	std::string
	set_function( std::string const & f );

    Data_Header
    header( );

    std::vector< double >
    data( );

  private:

	rs_rto_math_chan( RS_RTO  & rs,
					 Channel   ch );
    
    rs_rto_math_chan( rs_rto_math_chan const & ) = delete;

    rs_rto_math_chan &
    operator = ( rs_rto_math_chan const & ) = delete;

	std::string m_prefix;

    double const m_min_scale = 1.0e-15;
    double const m_max_scale = 1.0e26;

    double const m_min_offset = -1.0e26;
    double const m_max_offset =  1.0e26;
};

}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
