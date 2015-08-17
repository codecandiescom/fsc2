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


#if ! defined rs_rto_acq_hpp_
#define rs_rto_acq_hpp_


#include "rs_rto_acq.hpp"


namespace rs_rto
{

class RS_RTO;

/*----------------------------------------------------*
 *----------------------------------------------------*/

class rs_rto_acq
{
	friend class RS_RTO;


  public:

	double
	timebase( );

	double
	set_timebase( double tb );

    double
    shortest_timebase( ) const
    {
        return 0.1 * m_min_rec_len / m_adc_rate;
    }

    double
    longest_timebase( ) const
    {
        return m_max_scale;
    }

    double
    shortest_timebase_const_resolution( );

    double
    longest_timebase_const_resolution( );

    unsigned long
    record_length( );

    unsigned long
    set_record_length( unsigned long rec_len );

    unsigned long
    min_record_length( ) const
    {
        return m_min_rec_len;
    }

    unsigned long
    max_record_length( );

    double
    resolution( );

    double
    set_resolution( double res );

    double
    lowest_resolution( );

    double
    highest_resolution( );

    unsigned long
    average_count( ) const
    {
        return m_avg_count;
    }

    unsigned long
    set_average_count( unsigned long cnt );

    unsigned long
    max_average_count( ) const
    {
        return m_max_avg_count;
    }

    unsigned long
    segment_count( ) const
    {
        return m_seg_count;
    }

    unsigned long
    set_segment_count( unsigned long cnt );

    unsigned long
    max_segment_count( );

    bool
    is_running( );

    bool
    is_waiting_for_trigger( );

    void
    run( )
    {
        run_single( );
    }

    void
    run_single( );

    void
    run_continuous( );

    bool
    stop( );

    double
    reference_position( );

    double
    set_reference_position( double pos );

    Acq_Mode
    mode( ) const
    {
        return m_mode;
    }

    Acq_Mode
    set_mode( Acq_Mode mode );

    unsigned long
    available_segments( );

    bool
    download_limits_enabled( );

    bool
    set_download_limits_enabled( bool state );

    std::vector< double >
    download_limits( );

    std::vector< double >
    set_download_limits( std::vector< double > limits );

    std::vector< double >
    set_download_limits( double start,
                         double end );

    std::vector< double >
    max_download_limits( );


  private:

	rs_rto_acq( RS_RTO & rs );

    rs_rto_acq( rs_rto_acq const & ) = delete;

    rs_rto_acq &
    operator = ( rs_rto_acq const & ) = delete;

	void
	reset( );

    void
    run( bool continuous );

	RS_RTO & m_rs;

    double m_max_scale;
    unsigned long m_min_rec_len;
    double m_adc_rate;

    // Note: with disabled iinterpolation time scale can never be below
    // 10 ns - ADC rate is 10 GSa/s and there are at least 100 points per
    // division

    double const m_timebase_increment   = 1e-12;
    double const m_resolution_increment = 1e-8;

    Acq_Mode m_mode;
    unsigned long m_avg_count;
    unsigned long m_seg_count;

    unsigned long const m_max_avg_count = 16777215;
};

}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
