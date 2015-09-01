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


#if ! defined rs_rto_inp_chan_hpp_
#define rs_rto_inp_chan_hpp_


#include "rs_rto.hpp"
#include "rs_rto_chan.hpp"


namespace rs_rto
{

class RS_RTO;

/*----------------------------------------------------*
 *----------------------------------------------------*/


class rs_rto_inp_chan : public rs_rto_chan
{
	friend class rs_rto_chans;
	friend class rs_rto_chan;


  public:

	bool
	state( ) override;

	bool
	set_state( bool state ) override;

	bool
	is_overloaded( bool reset = true ) override;

	Coupling
	coupling( ) const override
	{
		return m_coupling;
	}

	Coupling
	set_coupling( Coupling coupling ) override;

	double
	scale( ) override;

	double
	set_scale( double sc ) override;

	double
	min_scale( ) override;

	double
	max_scale( ) override;

	double
	offset( ) override;

	double
	set_offset( double offs ) override;

	double
	min_offset( ) override;

	double
	max_offset( ) override;

	double
	position( ) const override
	{
		return m_position;
	}

	double
	set_position( double position ) override;

	double
	impedance( ) const override
	{
		return m_impedance;
	}

	double
	set_impedance( double impedance ) override;

    double
    min_impedance( ) const override
    {
        return m_min_impedance;
    }

    double
    max_impedance( ) const override
    {
        return m_max_impedance;
    }

	Bandwidth
	bandwidth( ) const override
	{
		return m_bandwidth;
	}

    Bandwidth
	set_bandwidth( Bandwidth bandwidth ) override;

    Data_Header
    header( ) override;

    std::vector< double >
    data( ) override;

	std::vector< std::vector< double > >
    segment_data( ) override
    {
        return segment_data( 0, 0 );
    }

	std::vector< std::vector< double > >
    segment_data( unsigned long start )
    {
        return segment_data( start, 0 );
    }

	std::vector< std::vector< double > >
    segment_data( unsigned long start,
                  unsigned long count ) override;


  private:

	rs_rto_inp_chan( RS_RTO  & rs,
					 Channel   ch );
    
    rs_rto_inp_chan( rs_rto_inp_chan const & ) = delete;

    rs_rto_inp_chan &
    operator = ( rs_rto_inp_chan const & ) = delete;

	void
	reset( );

    Arith_Mode
    arith_mode( ) const
    {
        return m_arith_mode;
    }

    Arith_Mode
    set_arith_mode( Arith_Mode mode );

	std::string m_prefix;

	bool m_state;

    double const m_scale_resolution    = 1e-3;
    double const m_offset_resolution   = 1e-3;
    double const m_position_resolution = 1e-2;

	Coupling m_coupling;
	
	double m_position;
	double const m_max_position = 5;

	double m_impedance;
	double const m_min_impedance = 1.0;
	double const m_max_impedance = 1.0e5;
	double const m_impedance_resolution = 0.1;

	Bandwidth m_bandwidth;

    Arith_Mode m_arith_mode;
};

}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
