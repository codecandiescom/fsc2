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
#if ! defined rs_rto_trig_hpp_
#define rs_rto_trig_hpp_


#include "rs_rto.hpp"


namespace rs_rto
{

class RS_RTO;


/*----------------------------------------------------*
 *----------------------------------------------------*/

class rs_rto_trig
{
	friend class RS_RTO;
    friend class rs_rto_inp_chan;
    friend class rs_rto_acq;


  public:

    Trig_Mode
    mode( ) const
    {
        return m_mode;
    }

    Trig_Mode
    set_mode( Trig_Mode mode );

	Channel
	source( ) const
	{
		return m_source;
	}

	Channel
	set_source( Channel source );

    Trig_Slope
	slope( ) const;

    Trig_Slope
	slope( Channel ch ) const;

	Trig_Slope
	set_slope( Trig_Slope slope );

	Trig_Slope
	set_slope( Channel    ch,
               Trig_Slope slope );

	double
	level( );

    double
    level( Channel ch );

	double
	set_level( double level );

    double
    set_level( Channel ch,
               double  level );

    double
    min_level( );

    double
    min_level( Channel ch );

    double
    max_level( );

    double
    max_level( Channel ch );

    bool
    is_overloaded( );

    double
    position( );

    double
    set_position( double pos );

    double
    earliest_position( );

    double
    latest_position( );

    bool
    out_pulse_state( ) const
    {
        return m_state;
    }

    bool
    set_out_pulse_state( bool state );

    Polarity
    out_pulse_polarity( ) const
    {
        return m_polarity;
    }

    Polarity
    set_out_pulse_polarity( Polarity pol );

    double
    out_pulse_length( ) const
    {
        return m_pulse_length;
    }

    double
    set_out_pulse_length( double len );

    double
    out_pulse_delay( );

    double
    set_out_pulse_delay( double delay );

    double
    min_out_pulse_delay( );

    double
    max_out_pulse_delay( );

    void
    raise( );


  private:

	rs_rto_trig( RS_RTO & rs );

	void
	reset( );


    double
    min_position( double ref_pos );

    double
    max_position( double ref_pos );

    int
    check_channel( Channel ch ) const;

    RS_RTO & m_rs;

    Trig_Mode m_mode;

	Channel m_source;

    bool m_state;

    Trig_Slope m_slope[ 5 ];

    Polarity m_polarity;

    double const m_level_resolution = 1e-3;

    double m_pulse_length;
    double const m_min_pulse_length = 4.0e-9;
    double const m_max_pulse_length = 1.0e-3;
    double const m_pulse_resolution = 2.0e-8;

    double const m_out_delay_resolution = 1.0e-9;

    double const m_position_resolution = 1e-12;
};

}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
