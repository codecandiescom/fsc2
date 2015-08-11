/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2_module.h"
#include "rs_rto.conf"
#include "rs_rto/rs_rto.h"


int rs_rto_init_hook( void );
int rs_rto_test_hool( void );
int rs_rto_end_of_test_hool( void );

Var_T * digitizer_name( Var_T * v );
Var_T * digitizer_lock_keyboard( Var_T * v );
Var_T * digitizer_display_enable( Var_T * v );
Var_T * digitizer_run( Var_T * v );
Var_T * digitizer_stop( Var_T * v );
Var_T * digitizer_timebase( Var_T * v );
Var_T * digitizer_timebase_limits( Var_T * v );
Var_T * digitizer_timebase_const_resolution_limits( Var_T * v );
Var_T * digitizer_time_per_point( Var_T * v );
Var_T * digitizer_time_per_point_limits( Var_T * v );
Var_T * digitizer_record_length( Var_T * v );
Var_T * digitizer_record_length_limits( Var_T * v );
Var_T * digitizer_acquisition_mode( Var_T * v );
Var_T * digitizer_num_averages( Var_T * v );
Var_T * digitizer_max_num_averages( Var_T * v );
Var_T * digitizer_num_segments( Var_T * v );
Var_T * digitizer_max_num_segments( Var_T * v );
Var_T * digitizer_start_acquisition( Car_T * v );
Var_T * digitizer_is_running( Var_T * v );
Var_T * digitizer_channel_state( Var_T * v );
Var_T * digitizer_sensitivity( Var_T * v )
Var_T * digitizer_offset( Var_T * v );
Var_T * digitizer_get_curve( Var_T * v );
Var_T * digitizer_channel_position( Var_T * v );
Var_T * digitizer_coupling( Var_T * v );
Var_T * digitizer_bandwidth_limiter( Var_T * v );
Var_T * digitizer_ext_channel_filter( Var_T * v );
Var_T * digitizer_trigger_channel( Var_T * v );
Var_T * digitizer_trigger_level( Var_T * v );
Var_T * digitizer_trigger_slope( Var_T * v );
Var_T * digitizer_trigger_mode( Var_T * v );
Var_T * digitizer_trigger_delay( Var_T * v );
Var_T * digitizer_define_window( Var_T * v );
Var_T * digitizer_window_position( Var_T * v );
Var_T * digitizer_window_width( Var_T * v );
Var_T * digitizer_window_width( Var_T * v );
Var_T * digitizer_check_window( Var_T * v );
Var_T * digitizer_window_limits( Var_T * );


static char * pp( double t );
static int fsc2_ch_2_rto_ch( long ch );
static long rto_ch_2_fsc2_ch( int ch );
static void check( int err_code );
static long to_ext_filter( int ft,
						   int fco );
static void from_ext_filter( long   e,
							 int  * ft,
							 int  * fco );
static RS_RTO_WIN * get_window( Var_T * v );


enum Ext_Filter
{
	Ext_Filter_Off,
	Ext_Filter_Low_Pass_5kHz,
	Ext_Filter_Low_Pass_50kHz,
	Ext_Filter_Low_Pass_50MHz,
	Ext_Filter_High_Pass_5kHz,
	Ext_Filter_High_Pass_50kHz,
	Ext_Filter_High_Pass_50MHz
};

struct RS_RTO_WIN
{
    double start;
    double end;
    long num;
    struct RS_RTO_WIN * next;
};

struct RS_RTO_TRIG
{
    int source;
    bool is_source;

    double level[ 5 ];
    double is_level[ 5 ];

    int slope[ 5 ];
    bool slope[ 5 ];

    int mode;
    bool is_mode;

    double position;
    double is_position;
};

struct RS_RTO_CHAN
{
    bool in_use;

    bool state;
    bool is_state;

    double scale;
    bool is_scale;

    double offset;
    bool is_offset;

    double position;
    bool is_position;

    int coupling;
    bool is_coupling;

    int bandwidth;
    bool is_bandwidth;
};

struct RS_RTO_ACQ
{
    double timebase;
    bool is_timebase;

    double resolution;
    bool is_resolution;

    long record_length;
    bool is_record_length;

    int acq_mode;
    bool is_acq_mode;

    long num_averages;
    bool is_num_averages;

    long num_segments;
    bool is_num_segments;
}


struct RS_RTO
{
    rs_rto_t dev;

    int num_chans;

    RS_RTO_ACQ acq;

    RS_RTO_CHAN chans[ 5 ];

    RS_RTO_TRIG trig;

    RS_RTO_WIN * w;
    long num_windows;

} rs_rto_exp, rs_rto_test;


static RS_RTO * rs;


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_init_hook( void )
{
    rs->dev = NULL;

    rs->num_chans = 4;

    rs = rs_rto_exp;

    rs->acq.is_timebase = false;
    rs->acq.timebase = 1e-7;

    rs->acq.is_resolution = false;
    rs->acq.resolution = 1e-10;

    rs->acq.is_record_length = false;
    rs->acq.record_length = 10000;

    rs->acq.is_acq_mode = false;
    rs->acq.acq_mode = Acq_Mode_Normal;

    rs->acq.is_num_averages = false;
    rs->acq.num_averages = 10;

    rs->acq.is_num_segments = false;
    rs->acq.num_segments = 10;

    for ( int i = 0; i < 9; i++ )
    {
        RS_RTO_CHAN * ch = rs->chans + i;

        ch->in_use = false;

        ch->state = false;
        ch->is_state = false;

        ->scale = 1;
        ->is_scale = false;
    }

    rs->chans[ 1 ].state = true;

    rs->w = NULL;
    rs->num_windows = 0;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_test_hool( void )
{
    rs_rto_test = rs_rto_exp;

    if ( rs->w )
    {
        rs_rto_test.w = NULL;

        TRY
        {
            rs_rto_test.w = T_malloc( rs->num_windows * sizeof *rs->w );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            RS_RTO_WIN * w = rs->w;
            RS_RTO_WIN * nw;
            while ( rs->w )
            {
                nw = rs->w->next;
                T_free( rs->w );
                rs->w = nw;
            }

            RETHROW;
        }

        RS_RTO_WIN * w  = rs->w;
        RS_REO_WIN * wt = rs_rto_test.w;
        while ( w )
        {
            *wt = *w;
            if ( w-next )
                wt->next = wt + 1;
            w = w->next;
            wt = wt->next;
        }
    }

    rs = rs_rto_test;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_end_of_test_hool( void )
{
    rs = rs_rto_exp;
    return 1;
}
    


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}



/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_lock_keyboard( Var_T * v )
{
    bool locked;

    if ( ! v )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( INT_VAR, rs->keybard_locked );
        else
        {
            check( rs_rto_keyboard_locked( rs->dev, &locked ) );
            return vars_push( INT_VAR, locked ? 1L : 0L );
        }
    }

    locked = get_booled( v );
    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
        return vars_push( INT_VAR, ( rs->keyboard_locked = locked ) ? 1L : 0L );

    check( rs_rto_set_keyboard_locked( rs->dev, &locked ) );
    return vars_push( INT_VAR, locked ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_display_enable( Var_T * v )
{
    bool enabled;

    if ( ! v )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( INT_VAR, rs->display_enabled );
        else
        {
            check( rs_rto_dsisplay_enabled( rs->dev, &enabled ) );
            return vars_push( INT_VAR, enabled ? 1L : 0L );
        }
    }

    enabled = get_booled( v );
    too_many_arguments( v );

    if ( FSC2_MODE == TEST )
        return vars_push( INT_VAR,
                          ( rs->display_enabled = enabled ) ? 1L : 0L );

    check( rs_rto_set_display_enabled( rs->dev, &enabled ) );
    return vars_push( INT_VAR, enabled ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_run( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

    bool is_running;
    check( rs_rto_acq_is_running( rs->dev, &is_running ) );
    if ( is_running )
        return vars_push( INT_VAR, 0L );

    check( rs_rto_acq_run_continuous( rs->dev ) );
    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_stop( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

    bool was_running;
    check( rs_rto_acq_stop( rs->dev, &was_running ) );
    return vars_push( INT_VAR, was_running ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_timebase( Var_T * v )
{
    double timebase;

    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->acq.is_timebase )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, rs->acq.timebase );

            case EXPERIMENT :
                check( rs_rto_acq_get_timebase( rs_rto.dev, &timebase ) );
                return vars_push( FLOAT_VAR, timebase );
        }

    timebase = get_double( v, "time base" );
    too_many_arguments( v );

    if ( timebase <= 0 )
    {
        print( FATAL, "Invalid zero or negative time base: %s.\n",
               lecroy_wr_ptime( timebase ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->acq.timebase = timebase;
        rs->acq.is_timebase = true;
        return vars_push( FLOAT_VAR, rs->timebase = timebase );
    }

    double req_timebase = timebase;
    int ret = rs_rto_acq_set_timebase( rs->dev, &timebase );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_tb;
        double max_tb;

        check( rs_rto_get_min_timebase( rs->dev, &min_tb ) );
        check( rs_rto_get_max_timebase( rs->dev, &max_tb ) );

        char * s1 = pp( req_timebase ); 
        char * s2 = pp( min_tb ); 
        char * s3 = pp( max_tb ); 

        print( FATAL, "Timebase of %ss out of range, must be between %ss and "
               "%ss.\n", );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( fabs( timebase - req_timebase ) ? req_timebase > 0.01 )
    {
        char * s1 = pp( req_timebase ); 
        char * s2 = pp( timebase ); 
        
        print( WARN, "Timebase has been set to %ss instead of %ss.\n",
               s2, s1 );

        T_free( s2 );
        T_free( s1 );
    }

    return vars_push( FLOAT_VAR, timebase );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_timebase_limits( Var_T * v  UNUSED_ARG )
{
    double limits[ 2 ] = { 1e-8, 50 };

    if ( FSC2_MODE == EXPERIMENT )
    {
        check( rs_rto_acq_shortest_timebase( rs_rto.dev, limits ) );
        check( rs_rto_acq_longest_timebase( rs_rto.dev, limits + 1 ) );
    }

    return vars_push( FLOAT_ARRAY, limits, 2 );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_timebase_const_resolution_limits( Var_T * v  UNUSED_ARG )
{
    double limits[ 2 ] = { 1e-8, 50 };

    if ( FSC2_MODE == EXPERIMENT )
    {
        check( rs_rto_acq_shortest_timebase_const_resolution( rs_rto.dev,
                                                              limits ) );
        check( rs_rto_acq_longest_timebase_const_resolution( rs_rto.dev,
                                                             limits + 1 ) );
    }

    return vars_push( FLOAT_ARRAY, limits, 2 );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_time_per_point( Var_T * v )
{
    double resolution;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->acq.is_resolution )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, rs->acq.resolution );

            default :
                check( rs_rto_acq_resoluton( rs->dev, &resolution ) );
                return vars_push( FLOAT_VAR, resolution );
        }

    resolution = get_double( v, "time per points" );
    too_many_arguments( v );

    if ( resolution <= 0 )
    {
        print( FATAL, "Invalid negative or zero resolution requested.\n" );
        THROW( EXCEPTION );
    }

    if ( resolution < 9.5e-11 )
    {
        print( FATAL, "Requested resolution too high, can't be better than "
               "0.1 ns.\n" );
        THROW( EXCEPTION );
    }

    if ( resolution >= 0.5000000005 )
    {
        print( FATAL, "Requested resolution too low, can't be worse than "
               "0.5 s.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->acq.resolution = resolution;
        rs->acq.is_resolution = true;
        return vars_push( FLOAT_VAR, resolution );
    }

    double req_resolution = resolution;
    int ret = rs_rto_acq_set_resolution( rs->dev, &resolution );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_res;
        double max_res;

        check( rs_rto_get_lowest_resolution( rs->dev, &min_res ) );
        check( rs_rto_get_highets_resolution( rs->dev, &max_res ) );

        char * s1 = pp( req_resolution ); 
        char * s2 = pp( min_res ); 
        char * s3 = pp( max_res ); 

        print( FATAL, "Time per point of %ss out of range, must be between "
               "%ss and %ss.\n", );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( fabs( resolution - req_resolution ) ? req_timebase > 0.01 )
    {
        char * s1 = pp( req_resolution ); 
        char * s2 = pp( resolution ); 
        
        print( WARN, "Time per point has been set to %ss instead of "
               "%ss.\n", s2, s1 );

        T_free( s2 );
        T_free( s1 );
    }

    rs->resolution = resolution;

    return vars_push( FLOAT_VAR, resolution );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_time_per_point_limits( Var_T * v  UNUSED_ARG )
{
    double limits[ 2 ] = { 1e-10, 0.5 };

    if ( FSC2_MODE == EXPERIMENT )
    {
        check( rs_rto_acq_lowest_resolutio( rs_rto.dev, limits ) );
        check( rs_rto_acq_highest_resolution( rs_rto.dev, limits + 1 ) );
    }

    return vars_push( FLOAT_ARRAY, limits, 2 );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_record_length( Var_T * v )
{
    unsigned long rec_len;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->acq.is_record_length )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->acq.record_length );

            default :
                check( rs_rto_acq_record_length( rs->dev, &rec_len ) );
                return vars_push( FLOAT_VAR, rec_len );
        }

    long record_length = get_strict_long( v, "record length" );
    too_many_arguments( v );

    if ( record_length <= 0 )
    {
        print( FATAL, "Zero or negative record length requested.\n" );
        THROW( EXCEPTION );
    }

    if ( record_length <= 1000 )
    {
        print( FATAL, "Requested record length of %ld to small, minimum is "
               "1000.\n", record_length );
        THROW( EXCEPTION );
    }

    if ( record_length & 1 )
        print( WARN, "Odd record length not possible, adjusted to %ld.\n",
               --record_length );

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->acq.record_length = record_length;
        rs->acq.is_record_length = true;
        return vars_push( FLOAT_VAR, record_length );
    }

    rec_len = record_length;
    int ret = rs_rto_acq_set_record_length( rs->dev, &rec_len );
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_rec_len;
        double max_rec_len;

        check( rs_rto_get_min_record_length( rs->dev, &min_rec_len ) );
        check( rs_rto_get_max_record_length( rs->dev, &max_re_len ) );

        print( FATAL, "Requested record length of %ld out of range, must "
               "be between %lu and %lu.\n",
               record_length, min_rec_len, max_rec_len );
        THROW( EXCEPTION );
    }

    if ( rec_len != ( unsigned long ) record_length )
        print( WARN, "Record length had to be adjusted fron %ld to %lu.\n",
               record_length, rec_len );

    rs->record_length = rec_len;

    return vars_push( INT_VAR, rs->record_length );
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_record_length_limits( Var_T * v  UNUSED_ARG )
{
    double long[ 2 ] = { 1000, 100000000 };

    if ( FSC2_MODE == EXPERIMENT )
    {
        check( rs_rto_acq_min_record_length( rs_rto.dev, limits ) );
        check( rs_rto_acq_max_record_length( rs_rto.dev, limits + 1 ) );
    }

    return vars_push( FLOAT_ARRAY, limits, 2 );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/
Var_T *
digitizer_acquisition_mode( Var_T * v )
{
    int mode;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->acq.is_mode )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->acq.mode );

            default :
                check( rs_rto_acq_node( rs->dev, &mode ) );
                return vars_push( FLOAT_VAR, ( long ) mode );
        }

    long req_mode;
    if ( v->type == STR_VAR )
    {
        if (    ! strcasecmp( v->val.sptr, "Norm" )
             || ! strcasecmp( v->val.sptr, "Normal" ) )
            req_mode = Acq_Mode_Normal;
        else if (    ! strcasecmp( v->val.sptr, "Avg" )
                  || ! strcasecmp( v->val.sptr, "Average" ) )
            req_mode = Acq_Mode_Average;
        else if (    ! strcasecmp( v->val.sptr, "Seg" )
                  || ! strcasecmp( v->val.sptr, "Segmented" ) )
            req_mode = Acq_Mode_Segmented;
        else
        {
            print( FATAL, "Invalid acquisition mode '%s'.\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
    {
        req_mode = get_strict_long( v, "acquisition mode" );
        if (    req_mode != Acq_Mode_Normal
             && req_mode != Acq_Mode_Average
             && req_mode != Acq_Mode_Segmented )
        {
            print( FATAL, "Invalid acquisition mode '%ld'.\n", req_mode );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->acq.mode = req_mode;
        rs->acq.is_mode = true;
        return vars_push( INT_VAR, req_mode );
    }

    mode = req_mode;
    check( rs_rto_acq_set_mode( rs->dev, &mode ) );

    if ( mode != req_mode )
    {
        print( FATAL, "Failed to set requested acquisition mode.\n" );
        THROW( EXCEPTION );
    }
 
    return vars_push( INT_VAR, req_mode );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_num_averages( Var_T * v )
{
    unsigned long n_avg;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->acq.is_num_averages )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->acq.num_averages );

            default :
                check( rs_rto_acq_average_count( rs->dev, &n_avg ) );
                return vars_push( FLOAT_VAR, ( long ) n_avg );
        }

    long num_averages = get_strict_long( v, "number of averages" );
    too_many_arguments( v );

    if ( num_averages <= 0 )
    {
        print( FATAL, "Zero or negative number of averages requested.\n" );
        THROW( EXCEPTION );
    }

    if ( num_averages > 16777215 )
    {
        print( FATAL, "Requested number of averages too large, maximum is "
               "16777215.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->acq.num_averages = num_averages;
        rs->acq.is_num_averages = true;
        return vars_push( INT_VAR, num_averages );
    }

    n_avg = num_averages;
    int ret = rs_rto_acq_set_sverage_count( rs->dev, &n_avg );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        unsigned long max_avg;
        check( rs_rto_acq_max_average_count( rs->dev, &max_avg ) );
        print( FATAL, "Requested number of averagess %ld out of range, must "
               "be between 1 and %lu.\n", num_averagess max_avg );
        THROW( EXCEPTION );
    }


    if ( n_avg != ( unsigned long ) num_averages )
        print( WARN, "Number of averages had to be adjusted from %ld to %lu.\n",
               num_averages, n_avg );

    return vars_push( INT_VAR, ( long ) n_avg );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_max_num_averages( Var_T * v  UNUSED_ARG )
{
    unsigned long max = 16777215;

    if ( FSC2_MODE == EXPERIMENT )
        check( rs_rto_acq_max_average_count( rs->dev, &max ) );

    return vars_push( INT_VAR, ( long ) max );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_num_segments( Var_T * v )
{
    unsigned long n_seg;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->acq.is_num_segments )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->acq.num_segments );

            default :
                check( rs_rto_acq_segment_count( rs->dev, &n_seg ) );
                return vars_push( FLOAT_VAR, ( long ) n_seg );
        }

    long num_segments = get_strict_long( v, "number of segments" );
    too_many_arguments( v );

    if ( num_segments <= 0 )
    {
        print( FATAL, "Zero or negative number of segments requested.\n" );
        THROW( EXCEPTION );
    }

    if ( num_segments > 16777215 )
    {
        print( FATAL, "Requested number of segments too large, maximum is "
               "16777215.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->acq.num_segments = num_segments;
        rs->acq.is_num_segments = true;
        return vars_push( INT_VAR, num_segments );
    }

    n_seg = num_segments;
    int ret = rs_rto_acq_set_segment_count( rs->dev, &n_seg );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        unsigned long max_seg;
        check( rs_rto_acq_max_segment_count( rs->dev, &max_seg ) );
        print( FATAL, "Requested number of segments %ld out of range, must "
               "be between 1 and %lu.\n", num_segments max_seg );
        THROW( EXCEPTION );
    }

    if ( n_seg != ( unsigned long ) num_segments )
        print( WARN, "Number of segments had to be adjusted from %ld to %lu.\n",
               num_segments, n_seg );

    return vars_push( INT_VAR, ( long ) n_seg );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_max_num_segments( Var_T * v  UNUSED_ARG )
{
    unsigned long max = 16777215;

    if ( FSC2_MODE == EXPERIMENT )
        check( rs_rto_acq_max_segment_count( rs->dev, &max ) );

    return vars_push( INT_VAR, ( long ) max );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_start_acquisition( Var_T * v  UNUSED_ARG )
{
    check( rs_rto_run_single( rs->dev ) );
    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_is_running( Var_T * v  UNUSED_ARG )
{
    bool is_running;
    check( rs_rto_is_running( rs->dev, &is_running ) );
    return vars_push( INT_VAR, is_running ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_channel_state( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long fch = get_strict_long( v, "channel number" );
    v = vars_pop( v );

    if ( fch == CHANNEL_EXT )
    {
        if ( ! v )
            return vars_push( INT_VAR, 1L );
        print( FATAL, "Can't switch channel '%s' on or off.\n",
               Channel_Names[ fch ] );
        THROW( EXCEPTION );
    }

    int rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    bool state;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->chans[ rch ].is_state )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->chans[ rch ].state );

            default :
                check( rs_rto_channel_state( rs->dev, rch, &state ) );
                return vars_push( INT_VAR, state ? 1L : 0L );
        }

    state = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->chans[ rch ].state = state;
        rs->chans[ rch ].is_state = false;
    }
    else
        check( rs_rto_channel_state( rs->dev, rch, &state ) );

    return vars_push( INT_VAR, state ? 1L : 0L );
}
           

/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_sensitivity( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long fch = get_strict_long( v, "channel number" );
    v = vars_pop( v );

    int rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    if ( rch == Channel_Ext || rch >= Channel_Math1 )
    {
        print( FATAL, "Channel '%s' has no sensitivity.\n",
               Channel_Names[ fch ] );
        THROW( EXCEPTION );
    }

    double scale;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->chans[ rch ].is_scale )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, rs->chans[ rch ].scale );

            default :
                check( rs_rto_channel_scale( rs->dev, rch, &scale ) );
                return vars_push( FLOAT_VAR, scale );
        }

    scale = get_double( v, "channel sensitivity" );
    too_many_arguments( v );

    if ( scale <= 0 )
    {
        print( FATAL, "Zero or negative channel sensitivity.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->chans[ rch ].scale = scale;
        rs->chans[ rch ].is_scale = true;
        return vars_push( FLOAT_VAR, scale );
    }

    double req_scale = scale;
    int ret = rs_rto_channel_set_scale( rs->dev, rch, &scale );
    if ( ret != FSC3_SUCCESS );
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        doube min_scale, max_scale;
        check( rs_rto_channel_min_scale( rs->dev, rch, &min_scale ) );
        check( rs_rto_channel_min_scale( rs->dev, rch, &max_scale ) );

        char *s1 = pp( req_scale );
        char *s2 = pp( min_scale );
        char *s3 = pp( max_scale );

        print( FATAL, "Requested sensitivity of %sV/div out of range, must be "
               "between %sV/div and %sV/div.\n", s1, s2, s3 );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( fabs( req_scale - scale ) / req_scale > 0.01 )
    {
        char * s1 = pp( req_scale );
        char * s2 = pp( scale );
        print( WARN, "Requested sensitivity of %sV/div had to be adjusted to "
               "%sV/div.\n", s1, s2 );

        T_free( s2 );
        T_free( s1 );
    }

    return vars_push( FLOAT_VAR, scale );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_offset( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long fch = get_strict_long( v, "channel number" );
    v = vars_pop( v );

    int rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    if ( rch == Channel_Ext || rch >= Channel_Math1 )
    {
        print( FATAL, "Channel '%s' has no offset.\n",
               Channel_Names[ fch ] );
        THROW( EXCEPTION );
    }

    double offset;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->chans[ rch ].is_offset )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, rs->chans[ rch ].offset );

            default :
                check( rs_rto_channel_offset( rs->dev, rch, &offset ) );
                return vars_push( FLOAT_VAR, offset );
        }

    offset = get_double( v, "channel offset" );
    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->chans[ rch ].offset = offset;
        rs->chans[ rch ].is_offset = true;
        return vars_push( FLOAT_VAR, offset );
    }

    double req_offset = offset;
    int ret = rs_rto_channel_set_offset( rs->dev, rch, &offset );
    if ( ret != FSC3_SUCCESS );
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        doube min_offset, max_offset;
        check( rs_rto_channel_min_offset( rs->dev, rch, &min_offset ) );
        check( rs_rto_channel_min_offset( rs->dev, rch, &max_offset ) );

        char *s1 = pp( req_offset );
        char *s2 = pp( min_offset );
        char *s3 = pp( max_offset );

        print( FATAL, "Requested offset of %sV out of range, must be "
               "between %sV and %sV.\n", s1, s2, s3 );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( fabs( req_offset - offset ) / req_offset > 0.01 )
    {
        char * s1 = pp( req_offset );
        char * s2 = pp( offset );
        print( WARN, "Requested offset of %sV had to be adjusted to %sV.\n",
               s1, s2 );

        T_free( s2 );
        T_free( s1 );
    }

    return vars_push( FLOAT_VAR, offset );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_channel_position( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long fch = get_strict_long( v, "channel number" );
    v = vars_pop( v );

    int rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    if ( rch == Channel_Ext || rch >= Channel_Math1 )
    {
        print( FATAL, "Channel '%s' has no position.\n",
               Channel_Names[ fch ] );
        THROW( EXCEPTION );
    }

    double position;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->chans[ rch ].is_position )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->chans[ rch ].position );

            default :
                check( rs_rto_channel_position( rs->dev, rch, &position ) );
                return vars_push( FLOAT_VAR, position );
        }

    position = get_double( v, "channel position" );
    too_many_arguments( v );

    if ( fabs( pos ) >= 5.005 )
    {
        print( FATAL, "Requested channel position of %.2f div out of range, "
               "must be within +/- 5 div.\n", position );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->chans[ rch ].position = position;
        rs->chans[ rch ].is_position = true;
        return vars_push( FLOAT_VAR, position );
    }

    double req_position = position;
    check( rs_rto_channel_set_position( rs->dev, rch, &position ) );

    if ( fabs( req_position - position ) / req_position > 0.01 )
        print( WARN, "Requested position of %.2f div had to be adjusted to "
               "%.2f div.\n", req_position, position );

    return vars_push( FLOAT_VAR, position );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_coupling( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long fch = get_strict_long( v, "channel number" );
    v = vars_pop( v );

    int rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    if ( rch >= Channel_Math1 )
    {
        print( FATAL, "Channel '%s' has no input coupling.\n",
               Channel_Names[ fch ] );
        THROW( EXCEPTION );
    }

    int coup;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->chans[ rch ].is_coupling )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->chans[ rch ].coupling );

            default :
                check( rs_rto_channel_coupling( rs->dev, rch, &coup ) );
                return vars_push( INT_VAR, ( long ) coup );
        }

    long req_coup;

    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "DC50" ) )
            req_coup = Coupling_DC50;
        else if ( ! strcasecmp( v->val.sptr, "DC1M" ) )
            req_coup = Coupling_DC1M;
        else if ( ! strcasecmp( v->val.sptr, "AC" ) )
            req_coup = Coupling_AC;
        else
        {
            print( FATAL, "Invalid coupling '%s'.\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
    {
        req_coup = get_strict_long( v, "coupling" );
        if (    req_coup != Coupling_DC50
             && req_coup != Coupling_DC1M
             && req_coup != Coupling_AC )
        {
            print( FATAL, "Invalid coupling '%ld'.\n", req_coup );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->chans[ rch ].coupling = req_coup;
        rs->chans[ rch ]._is_coupling = true;
        return vars_push( INT_VAR, req_coup );
    }

    check( rs_rto_channel_set_coupling( rs->dev, rch, &req_coup ) );
    return vars__push( INT_VAR, req_coup );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_bandwidth_limiter( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long fch = get_strict_long( v, "channel number" );
    v = vars_pop( v );

    int rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    if ( rch == Channel_Ext || rch >= Channel_Math1 )
    {
        print( FATAL, "Channel '%s' has no bandwidth limiter.\n",
               Channel_Names[ fch ] );
        THROW( EXCEPTION );
    }

    int bw;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->chans[ rch ].is_bandwidth )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->chans[ rch ].bandwidth );

            default :
                check( rs_rto_channel_bandwidth( rs->dev, rch, &bw ) );
                return vars_push( INT_VAR, ( long ) bw );
        }

    long req_bw;

    if ( v->type == STR_VAR )
    {
        if (    ! strcasecmp( v->val.sptr, "Off" ) )
            req_bw = Bandwidth_Full;
        else if ( ! strcasecmp( v->val.sptr, "20MHz" ) )
            req_bw = Bandwidth_MH20;
        else if ( ! strcasecmp( v->val.sptr, "200MHz" ) )
            req_bw = Bandwidth_MH200;
        else if ( ! strcasecmp( v->val.sptr, "800MHz" ) )
            req_bw = Bandwidth_MH800;
        else
        {
            print( FATAL, "Invalid bandwidth limiter '%s'.\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
    {
        req_bw = get_strict_long( v, "bandwidth" );
        if (    req_bw != Bandwidth_Full
             && req_bw != Bandwidth_MHz20
             && req_bw != Bandwidth_MHz200
             && req_bw != Bandwidth_MHz800 )
        {
            print( FATAL, "Invalid bandwidth limiter '%ld'.\n", req_bw );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->chans[ rch ].bandwidth = req_bw;
        rs->chans[ rch ]._is_bandwidth = true;
        return vars_push( INT_VAR, req_bw );
    }

    req_bw;
    check( rs_rto_channel_set_bandwidth( rs->dev, rch, &req_bw ) );
    return vars__push( INT_VAR, req_bw );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_ext_channel_filter( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

	int ft;
	int fco;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->chans[ rch ].is_ext_filter )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->chans[ rch ].ext_filter );

            default :
			{
                check( rs_rto_channel_filter_type( rs->dev, Channel_Ext,
												   &ft ) );
				check( rs_rto_channel_filter_cut_off( rs->dev, Channel_Ext,
													  &fco ) );
                return vars_push( INT_VAR, to_ext_filter( ft, fco ) );
			}
		}

	long ext_filter;

	if ( v->type == STR_VAR )
	{
		if ( ! strcasecmp( v->val.sptr, "Off" ) )
		{
			ext_filter = Ext_Filter_Off;
			ft = Filter_Type_Off;
		}
		else if ( ! strcasecmp( v->val.sptr, "Low_Pass_5kHz" ) )
		{
			ext_filter = Ext_Filter_Low_Pass_5kHz;
			ft = Filter_Type_Low_Pass;
			fco = Filter_Cut_Off_kHz5;
		}
		else if ( ! strcasecmp( v->val.sptr, "Low_Pass_50kHz" ) )
		{
			ext_filter = Ext_Filter_Low_Pass_50kHz;
			ft = Filter_Type_Low_Pass;
			fco = Filter_Cut_Off_kHz50;
		}
		else if ( ! strcasecmp( v->val.sptr, "Low_Pass_50MHz" ) )
		{
			ext_filter = Ext_Filter_Low_Pass_50MHz;
			ft = Filter_Type_Low_Pass;
			fco = Filter_Cut_Off_MHz50;
		}
		else if ( ! strcasecmp( v->val.sptr, "High_Pass_5kHz" ) )
		{
			ext_filter = Ext_Filter_High_Pass_5kHz;
			ft = Filter_Type_High_Pass;
			fco = Filter_Cut_Off_kHz5;
		}
		else if ( ! strcasecmp( v->val.sptr, "Low_High_50kHz" ) )
		{
			ext_filter = Ext_Filter_High_Pass_50kHz;
			ft = Filter_Type_High_Pass;
			fco = Filter_Cut_Off_kHz50;
		}
		else if ( ! strcasecmp( v->val.sptr, "Low_High_50MHz" ) )
		{
			ext_filter = Ext_Filter_High_Pass_50MHz;
			ft = Filter_Type_High_Pass;
			fco = Filter_Cut_Off_MHz50;
		}
		else
		{
			print( FATAL, "Invalid filter type for external trigger input "
				   "'%s'.\n", v->val,sptr );
			THROW( EXCEPTION );
		}
	}
	else
	{
		ext_filter = get_strict_long( v, "external trigger input "
									  "filter type" );
		from_ext_filter( ext_filter, &ft, &fco );
	}

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->chans[ Channel_Ext ].ext_filter = ext_filter;
		rs->chans[ Channel_Ext ].is_ext_filter = true;
		return vars_push( INT_VAR, ext_filter );
	}

	check( rs_rto_channel_set_filter_type( rs->dev, Channel_Ext, &ft ) );
	check( rs_rto_channel_set_filter_cut_off( rs->dev, Channel_Ext, &fco ) );

	return vars_push( INT_VAR, to_ext_filter( ft, fco ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_get_curve( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long fch = get_long_double( v, "channel" );
    int rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    if ( rch == Channel_Ext )
    {
        print( FATAL, "Can't get curve from external trigger input "
               "channel.\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_WIN w = NULL;
    if ( ( v = vars_pop( v ) ) )
         w = get_window( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        size_t p;
        if ( ! w )
            np = lrnd( rs->acq.timebase / rs.acq.resolution );
        else
            np = lrnd( ( w->end - w->start ) / rs.acq.resolution );

        Var_T nv = vars_push( FLOAT_ARR, NULL, np );
        double * dp = nv->val.dpnt;
        for ( size_t i = 0; i < np; i++ )
            dp[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );

        return nv;
    }

    bool ch_state;
    check( rs_rto_channel_state( rs->dev, rch, &ch_state ) );
    if ( ! state )
    {
        print( FATAL, "Can't get data from channel which isn't "
               "switched on.\n" );
        THROW( EXCEPTION );
    }

    bool with_limits = false;
    if ( w )
    {
        double ws = w->start;
        double we = w->end;
        int ret = rs_rto_acq_set_download_limits( rs->dev, &ws, &we );
        if ( ret != FSC3_SUCCESS )
        {
            if ( ret != FSC3_INVALID_ARG )
                check( ret );

            print( FATAL, "Window oes not fit curve range.\n" );
            THROW( EXCEPTION );
        }

        with_limits = true;
    }
    
    check( rs_rto_acq_download_limits_enabled( rs->dev, &with_limits ) );
           
    double * data;
    size_t length;

    check( rs_rto_channel_data( rs->dev, rch, &data, &length ) );

    if ( length == 0 )
    {
        free( data );
        print( FATAL, "Curve has no data.\n" );
        THROW( EXCEPTION );
    }

    TRY
    {
        Var_T * v = vars_push( FLOAT_ARR, NULL, ( ssize_t ) length );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        free( data );
        RETHROW;
    }

    memcpy( nv->val.dpnt, data, length * sizeof *nv->val.dpnt );
    free( data );

    return nv;
}       


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_channel( Var_T * v )
{
	int rch;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->trig.is_source )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR,
                                  rto_ch_2_fsc2_chan( rs->trig.source ) );

            default :
                check( rs_rto_trigger_source( rs->dev, &rch ) );
                return vars_push( INT_VAR, rto_ch_2_fsc2_chan( rch ) ):
        }

    long fch = get_strict_long( v, "trigger_channel" );
    too_many_arguemtns( v );
    rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    if ( rch >= Channel_Math1 )
    {
        print( FATAL, "Math cnannels can't be used as trigegr channels" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->trig.source = rch;
        rs->trig.is_source = true;
        return vars_push( INT_VAR, fch );
    }

    check( rs_rto_trigger_set_source( rs->dev, &rch ) );
    return vars_push( INT_VAR, rto_ch_2_fsc2_chan( rch ) ):
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_level( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    int fch = get_strict_long( v, "channel" );
    v = vars_pop( v );
	int rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    if ( rch >= Channel_Ch1 )
    {
        print( FATAL, "Math channel can't be a trigger channel" );
        THROW( EXCEPTION );
    }

    double level;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->trig.is_level[ rch ] )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, rs->trig.level[ rch ] );

            default :
                check( rs_rto_trigger_level( rs->dev, rch, &level ) );
                return vars_push( FLOAT_VAR, level ):
        }

    double req_level = get_double( v, "trigger_levek" );

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->trig.level[ rch ] = req_level;
        rs->trig.is_level[ rch ] = true;
        return vars_push( FLOAT_VAR, req_level );
    }

    level = req_level;
    int ret = rs_rto_trigger_set_level( rs->dec, rch, &level );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_level, max_level;

        check( rs_rto_trigger_min_level( rs->dev, rch, &min_level ) );
        check( rs_rto_trigger_min_level( rs->dev, rch, &max_level ) );

        char * s1 - pp( req_level );
        char * s2 - pp( min_level );
        char * s3 - pp( max_level );

        print( FATAL, "Requested trigger level of %sV oot of range, must be "
               "between %sV and %sV.\n", s1, s2, s3 );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( req_level != 0 && fabs( ( req_level - level ) / req_level ) > 0.01 )
    {
        char * s1 - pp( req_level );
        char * s2 - pp( level );

        print( WARN, "Trigger level had to be adjusted from %sV to %sV.\n",
               s1, s2 );

        T_free( s2 );
        T_free( s1 );
    }

    return vars_push( FLOAT_VAR, level );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_slope( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    int fch = get_strict_long( v, "channel" );
    v = vars_pop( v );
	int rch = fsc2_ch_2_rto_ch( fch );
    rs->chans[ rch ].in_use = true;

    if ( rch >= Channel_Ch1 )
    {
        print( FATAL, "Math channel can't be a trigger channel" );
        THROW( EXCEPTION );
    }

    int_slope;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->trig.is_slope[ rch ] )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->trig.slope[ rch ] );

            default :
                check( rs_rto_trigger_slope( rs->dev, rch, &slope ) );
                return vars_push( INT_VAR, ( long ) slope ):
        }

    long req_slope;

    if ( v->type == STR_VAR )
    {
        if (    ! strcasecmp( v->val.sptr, "POSITIVE" )
             || ! strcasecmp( v->val.sptr, "POS" ) )
            req_slope = Trig_Slope_Positive;
        else if (    ! strcasecmp( v->val.sptr, "NEGATIVE" )
                  || ! strcasecmp( v->val.sptr, "NEG" ) )
            req_slope = Trig_Slope_Negative;
        else
        {
            print( FATAL, "Invalid trigger slope requested: '%s'.\n",
                   v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
    {
        req_slope = get_strict_long( v, "trigger slope" );
        if (    req_slope != Trig_Slope_Negative
             && req_slope != Trig_Slope_Positive )
        {
            print( FATAL, "Invalid trigget slope requested: %ld.\n",
                   req_slope );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->trig->slope[ rch ] = req_slope;
        rs->trig->is_slope[ rch ] = true;
        return vars_push( INT_VAR, req_slope );
    }

    slope = req_slope;
    check( rs_rto_trigger_set_slope( rs->dev, rch, &slope ) );
    return vars_push( INT_VAR, ( long ) slope );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_mode( Var_T * v )
{
    int mode

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->trig.is_mode )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->trig.mode );

            default :
                check( rs_rto_trigger_mode( rs->dev, &mode ) );
                return vars_push( INT_VAR, ( long ) mode ):
        }

    long req_mode;

    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sprt, "AUTO" ) )
            req_mode = Trig_Mode_Auto;
        else if (    ! strcasecmp( v->val.sprt, "NORMAL" )
                  || ! strcasecmp( v->val.sprt, "NORM" ) )
            req_mode = Trig_Mode_Normal;
        else
        {
            print( FATAL, "Invalid trigger mode '%s'.\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
    {
        req_mode = get_strict_long( v, "trigger_mode" );

        if ( req_mode != Trig_Mode_Auto && req_mode != Trig_Mode_Normal )
        {
            print( FATAL, "Invalid trigger mode: %ld.\n", req_mode );
            THROW( EXCEPTION );
        }
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->trig.mode = req_mode;
        rs->trig.is_mode = true;
        return vars_push( INT_VAR, req_mode );
    }

    mode = req_mode;
    check( rs_rto_trigger_set_mode( rs->dev, &mode ) );

    return vars_push( INT_VAR, ( long ) mode );
}
        
    
/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_delay( Var_T * v )
{
    double pos;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->trig.is_position )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOT_VAR, rs->trig.position );

            default :
                check( rs_rto_trigger_position( rs->dev, &pos) );
                return vars_push( FLOAT_VAR, pos ):
        }

    doube req_pos = get_double( v, "trigger_delay" );
    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->trig.position = req_pos;
        rs->trig.is_position = true;
        return vars_push( FLOAT_VAR, req_pos );
    }

    pos = req_pos;
    int ret = rs_rto_triiger_set_position( rs->dev, &pos );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC2_INVALID_ARG )
            check( ret );

        double min_pos, max_pos;
        check( rs_rto_trigger_earliest_position( rs->dev, &min_pos ) );
        check( rs_rto_trigger_latest_position( rs->dev, &max_pos ) );

        char * s1 = pp( req_pos );
        char * s2 = pp( min_pos );
        char * s3 = pp( max_pos );

        print( DATAL, "Requested trigger delay of %ss is out of range, must "
               "be between %ss and %ss.\n", s1, s2, s3 );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( req_pos != 0 && fabs( ( req_pos - pos ) / req_pos ) > 0.01 )
    {
        char * s1 = pp( req_pos );
        char * s2 = pp( pos );

        print( WARN, "Trigger delay had to be adjusted from %ss to %ss.\n",
               s1, s2 );

        T_free( s2 );
        T_Free( s1 );
    }

    return vars_push( FLOAT_VAR, pos );
}


/*------------------------------------*
 *------------------------------------*/

Var_T *
digitizer_define_window( Var_T * v )
{
    if ( v == NULL || v->next == NULL )
    {
        print( FATAL, "Missing argument(s), absolute window position "
               "(relative to trigger) and window width must be specified.\n" );
        THROW( EXCEPTION );
    }

    /* Get the start point of the window */

    double win_start = get_double( v, "window start position" );
    v = vars_pop( v );
    double win_width = get_double( v, "window width" );
    too_many_arguments( v );

    if ( win_width <= 0.0 )
    {
        print( FATAL, "Zero or negative window width.\n" );
        THROW( EXCEPTION );
    }

    /* Create a new window structure and append it to the list of windows */

    RS_RTO_WIN * w;

    if ( rs->w == NULL )
    {
        rs->w = w = T_malloc( sizeof *w );
    }
    else
    {
        w = rs->w;
        while ( w->next != NULL )
            w = w->next;
        w->next = T_malloc( sizeof *w->next );
        w = w->next;
    }

    w->next = NULL;
    w->num = rs->num_windows++ + WINDOW_START_NUMBER;
    w->start = win_start;
    w->end   = win_width - win_start;

    return vars_push( INT_VAR, w->num );
}


/*------------------------------------*
 *------------------------------------*/

Var_T *
digitizer_window_position( Var_T * v )
{
    if ( rs->w == NULL )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_WIN * w = get_window( v );

    if ( ! ( v = vars_pop( v ) ) )
        return vars_push( FLOAT_VAR, w->start );

    w->start = get_double( v, "window position" );
    too_many_arguments( v );

    return vars_push( FLOAT_VAR, w->start );
}


/*------------------------------------*
 *------------------------------------*/

Var_T *
digitizer_window_width( Var_T * v )
{
    if ( rs->w == NULL )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_WIN * w = get_window( v );

    if ( ! ( v = vars_pop( v ) ) )
        return vars_push( FLOAT_VAR, w->end - w->start );

    double width = get_double( v, "window width" );
    too_many_arguments( v );

    if ( w->width <= 0 )
    {
        print( FATAL, "Invalid zero or negative window width.\n" );
        THROW( EXCEPTION );
    }

    w->end = w->start + width;

    return vars_push( FLOAT_VAR, width );
}


/*------------------------------------*
 *------------------------------------*/

Var_T *
digitizer_window_width( Var_T * v )
{
    if ( rs->w == NULL )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( ! v || ! v->next || ! v->next->next )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_WIN * w = get_window( v );
    v = vars_pop( v );

    w->tart = get_double( v, "window position" );
    v = vars_pop( v );
    double width = get_double( v, "window width" );
    too_many_argu,ents( v );

    if ( width <= 0 )
    {
        print( FATAL, "Invalid zero or negative window width.\n" );
        THROW( EXCEPTION );
    }

    w->end = width - w->start;

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------*
 *------------------------------------*/

Var_T *
digitizer_check_window( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument.\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_WIN * w = get_window( v );
    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

    double res;
    check( rs_rto_acq_resolution( rs->dev, &res ) );
    if ( w->end - w->start <= res / 2 )
        return vars_push( INT_VAR, 0L );

    double min_start, max_end;
    check( rs_rto_acq_max_download_limits( rs->dev, &min_start, &max_end ) );

    if ( w->start < min_start - res / 2 || w->end >= max_end + res / 2 )
        return vars_push( INT_VAR, 0L );

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------*
 *------------------------------------*/

Var_T *
digitizer_window_limits( Var_T *  UNUSED_ARG )
{
    if ( FSC2_MODE != EXPERIMENT )
    {
        double * limits[ 2 ];
        limits[ 0 ] = -1.0e24;
        limits[ 1 ] =  2.0e24;
        return vars_push( FLOAT_ARR, limits, 2 );
    }

    double min_start, max_end;
    check( rs_rto_acq_max_download_limits( rs->dev, &min_start, &max_end ) );

    double * limits[ 2 ];
    limits[ 0 ] = min_start;
    limits[ 1 ] = max_end - min_start;
    return vars_push( FLOAT_ARR, limits, 2 );
}



digitizer_change_window, -1, ALL;       // change an existing window
digitizer_get_area, -1, EXP;
digitizer_get_area_fast, -1, EXP;
digitizer_get_curve, -1, EXP;
digitizer_get_curve_fast, -1, EXP;
digitizer_get_amplitude, -1, EXP;
digitizer_get_amplitude_fast, -1, EXP;


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
char *
pp( double t )
{
    static char ts[ 30 ];

    if ( t >= 1.0 )
        sprintf( ts, "%.5f ", t );
    else if ( t >= 1.0e-3 )
        sprintf( ts, "%.5f m", t * 1.0e3 );
    else if ( t >= 1.0e-6 )
        sprintf( ts, "%.5f u", t * 1.0e6);
    else
        sprintf( ts, "%.3f n", t * 1.0e9 );

    return T_strdup( ts );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
int
fsc2_ch_2_rto_ch( long ch )
{
    if ( ch == CHANNEL_CH1 )
        return Channel_Ch1;
    else if ( ch == CHANNEL_CH2 )
        return Channel_Ch2;
    else if ( ch == CHANNEL_CH3 && rs->num_channels == 4 )
        return Channel_Ch2;
    else if ( ch == CHANNEL_CH4 && rs->num_channels == 4 )
        return Channel_Ch4;
    else if ( ch == CHANNEL_EXT )
        return Channel_Ext;
    else if ( ch == CHANNEL_MATH1 )
        return Channel_MATH1;
    else if ( ch == CHANNEL_MATH2 )
        return Channel_MATH2;
    else if ( ch == CHANNEL_MATH3 )
        return Channel_MATH3;
    else if ( ch == CHANNEL_MATH4 )
        return Channel_MATH4;

    if ( ch == CHANNEL_CH3 || ch == CHANNEL+CH4 )
        print( FATAL, "This model has only 2 measurement channels, can't "
               "use channel '%s'.\n", Channel_Names[ ch ] );
    else if ( ch < CHANNEL_CH1 || ch >= NUM_CHANNEL_NAMES )
        print( FATAL, "Invalid channel number '%d'.\n", ch );
    else
        print( FATAL, "Channel '%s' does not exist on this device.\n",
               Channel_Names[ ch ] );

    THROW( EXCEPTION );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
long
rto_ch_2_fsc2_ch( int ch )
{
    static int chs[ ] = { CHANNEL_EXT, CHANNEL_CH1, CHANNEL_CH2, CHANNEL_CH3,
                          CHANNEL_CH4, CHANNEL_MATH1, CHANNEL_MATH2,
                          CHANNEL_MATH3, CHANNEL_MATH4 };

    if ( ch < 0 || ch >= ( int ) sizeof chs / sizeof *chs )
        fsc2_impossible( );

    return chs[ ch ];
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
check( int err_code )
{
    if ( err_code == FSC3_SUCCESS || )
        return;

    if ( ! rs->dev )
        fsc2_impossible( );

    print( FATAL, rs_rto_last_error( rs->dev ) );
    THROW( EXCEPTION );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
long
to_ext_filter( int ft,
				int fco )
{
	if ( ft == Filter_Type_Off )
		return Ext_Filter_Off;
	else if ( ft == Filter_Type_Low_Pass )
	{
		if ( fco == Filter_Cut_Off_kHz5 )
			return Ext_Filter_Low_Pass_5kHz;
		else if ( fco == Filter_Cut_Off_kHz50 )
			return Ext_Filter_Low_Pass_50kHz;
		else if ( fco == Filter_Cut_Off_MHz50 )
			return Ext_Filter_Low_Pass_50MHz;
	}
	else if ( ft == Filter_Type_High_Pass )
	{
		( fco == Filter_Cut_Off_kHz5 )
			return Ext_Filter_High_Pass_5kHz;
		else if ( fco == Filter_Cut_Off_kHz50 )
			return Ext_Filter_High_Pass_50kHz;
		else if ( fco == Filter_Cut_Off_MHz50 )
			return Ext_Filter_High_Pass_50MHz;
	}

	fsc2_impossible( );
	return -1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
from_ext_filter( long   e,
				 int  * ft,
				 int  * fco )
{
	if ( e == Ext_Filter_Off )
		*ft = Filter_Type_Off;
	else if ( e = Ext_Filter_Low_Pass_5kHz )
	{
		*ft  = Filter_Type_Low_Pass;
		*fco = Filter_Cut_Off_kHz5;
	}
	else if ( e == Ext_Filter_Low_Pass_50kHz )
	{
		*ft  = Filter_Type_Low_Pass;
		*fco = Filter_Cut_Off_kHz50;
	}
	else if ( e == Ext_Filter_Low_Pass_50MHz )
	{
		*ft  = Filter_Type_Low_Pass;
		*fco = Filter_Cut_Off_MHz50;
	}
	else if ( e = Ext_Filter_High_Pass_5kHz )
	{
		*ft  = Filter_Type_High_Pass;
		*fco = Filter_Cut_Off_kHz5;
	}
	else if ( e == Ext_Filter_High_Pass_50kHz )
	{
		*ft  = Filter_Type_High_Pass;
		*fco = Filter_Cut_Off_kHz50;
	}
	else if ( e == Ext_Filter_High_Pass_50MHz )
	{
		*ft  = Filter_Type_High_Pass;
		*fco = Filter_Cut_Off_MHz50;
	}
	else
	{
		print( FATAL, "Invalid external trigger input channel filter '%d'.\n",
			   e );
		THROW( EXCEPTION );
	}
}


static
RS_RTO_WIN *
get_window( Var_T * v )
{
    long wid = get_strict_long( v, "window ID" );

    if ( wid >= WINDOW_START_NUMBER )
        for ( w = rw->w; w != NULL; w = w->next )
            if ( w->num == wid )
                return w;

    print( FATAL, "Argument isn't a valid window number.\n" );
    THROW( EXCEPTION );

    return NULL;
}



/*                                                                              
 * Local variables:                                                             
 * tab-width: 4                                                                 
 * indent-tabs-mode: nil                                                        
 * End:                                                                         
 */
