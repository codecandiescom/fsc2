/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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
#include "rs_rto/rs_rto_c.h"
#include "lan.h"
#include "rs_rto.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


// Some estimates for the overhead for downloading a data set
// and the data transfer rate.

#define CURVE_DELAY   0.03     // 30 ms
#define TRANSFER_RATE 10e6     // 10 MB/s


// Enumeration for settings of filter for externnal trigger input
// channel, combining type of filter and cut-off frequency

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

typedef struct RS_RTO_Win
{
    double start;
    double end;
    long num;
    struct RS_RTO_Win * next;
}  RS_RTO_Win;

typedef struct  // RS_RTO_Trig
{
    int source;
    bool is_source;

    double level[ 5 ];       // one setting for each possible source channel
    bool is_level[ 5 ];

    int slope[ 5 ];          // one setting for each possible source channel
    bool is_slope[ 5 ];

    int mode;
    bool is_mode;

    double position;
    double is_position;

    bool out_pulse_state;
    bool is_out_pulse_state;

    double out_pulse_length;
    bool is_out_pulse_length;

    int out_pulse_polarity;
    bool is_out_pulse_polarity;

    double out_pulse_delay;
    bool is_out_pulse_delay;
} RS_RTO_Trig;

typedef struct  // RS_RT_Chan
{
    bool in_use;

    bool state;
    bool is_state;

    double scale;                // Channel_Ch[1-4] only
    bool is_scale;

    double offset;               // Channel_Ch[1-4] only
    bool is_offset;

    double position;             // Channel_Ch[1-4] only
    bool is_position;

    int coupling;                // Channel_Ch[1-4] and Channel_Ext only
    bool is_coupling;

    int bandwidth;               // Channel_Ch[1-4] only
    bool is_bandwidth;

    int ext_filter;              // Channnel_ext only
    bool is_ext_filter;

    char * function;             // Channel_Math[1-4] only
} RS_RT_Chan;

typedef struct  // RS_RTO_Acq
{
    double timebase;
    bool is_timebase;

    double resolution;
    bool is_resolution;

    long record_length;
    bool is_record_length;

    int mode;
    bool is_mode;

    long num_averages;
    bool is_num_averages;

    long num_segments;
    bool is_num_segments;
}  RS_RTO_Acq;


typedef struct
{
    rs_rto_t * dev;

    int num_channels;

    bool keyboard_locked;

    bool display_enabled;

    RS_RTO_Acq acq;

    RS_RT_Chan chans[ Channel_Math4 + 1 ];

    RS_RTO_Trig trig;

    RS_RTO_Win * w;
    long num_windows;

}  RS_RTO;


int rs_rto_init_hook( void );
int rs_rto_test_hool( void );
int rs_rto_end_of_test_hool( void );
int rs_rto_exp_hook( void );
int rs_rto_end_of_exp_hook( void );
void rs_rto_exit_hook( void );

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
Var_T * digitizer_start_acquisition( Var_T * v );
Var_T * digitizer_channel_state( Var_T * v );
Var_T * digitizer_sensitivity( Var_T * v );
Var_T * digitizer_offset( Var_T * v );
Var_T * digitizer_channel_position( Var_T * v );
Var_T * digitizer_coupling( Var_T * v );
Var_T * digitizer_bandwidth_limiter( Var_T * v );
Var_T * digitizer_ext_channel_filter( Var_T * v );
Var_T * digitizer_get_curve( Var_T * v );
Var_T * digitizer_get_area( Var_T * v );
Var_T * digitizer_get_amplitude( Var_T * v );
Var_T * digitizer_get_segments( Var_T * v );
Var_T * digitizer_available_segments( Var_T * v );
Var_T * digitizer_trigger_channel( Var_T * v );
Var_T * digitizer_trigger_level( Var_T * v );
Var_T * digitizer_trigger_slope( Var_T * v );
Var_T * digitizer_trigger_mode( Var_T * v );
Var_T * digitizer_trigger_delay( Var_T * v );
Var_T * digitizer_define_window( Var_T * v );
Var_T * digitizer_window_position( Var_T * v );
Var_T * digitizer_window_width( Var_T * v );
Var_T * digitizer_change_window( Var_T * v );
Var_T * digitizer_check_window( Var_T * v );
Var_T * digitizer_window_limits( Var_T * );
Var_T * digitizer_math_function( Var_T * v );
Var_T * digitizer_trigger_out_pulse_state( Var_T * v );
Var_T * digitizer_trigger_out_pulse_length( Var_T * v );
Var_T * digitizer_trigger_out_pulse_polarity( Var_T * v );
Var_T * digitizer_trigger_out_pulse_delay( Var_T * v );
Var_T * digitizer_trigger_out_pulse_delay_limits( Var_T * v );


static char * pp( double t );
static int fsc2_ch_2_rto_ch( long ch );
static long rto_ch_2_fsc2_ch( int ch );
static void check( int err_code );
static long to_ext_filter( int ft,
                           int fco );
static void from_ext_filter( long   e,
                             int  * ft,
                             int  * fco );
static RS_RTO_Win * get_window( Var_T * v );
static RS_RTO_Win * get_window_from_long( long wid );
static void get_waveform( int           rch,
                          RS_RTO_Win  * w,
                          double     ** data,
                          size_t      * length );
static void get_segments( int            rch,
                          RS_RTO_Win   * w,
                          double     *** data,
                          size_t       * num_segments,
                          size_t       * length );
static Var_T * get_calculated_curve_data( Var_T  * v,
                                          double   ( handler )( double *,
                                                                size_t ) );
static Var_T * get_subcurve_data( int rch,
                                  RS_RTO_Win ** wins,
                                  long          win_count,
                                  double        ( *handler )( double *,
                                                              size_t ) );
static double area( double * data,
                    size_t   length );
static double amplitude( double * data,
                         size_t   length );
static void init_prep_acq( void );
static void init_prep_trig( void );
static void init_prep_chans( void );
static void init_exp_acq( void );
static void init_exp_trig( void );
static void init_exp_chans( void );
static
void copy_windows( RS_RTO_Win       ** dst,
                   RS_RTO_Win const  * volatile src );
static void delete_windows( RS_RTO_Win ** wp );




static  RS_RTO rs_rto_exp, rs_rto_test;
static RS_RTO * rs;

static RS_RTO_Win * prep_wins;
static char const * err_prefix = "";


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_init_hook( void )
{
    rs = &rs_rto_exp;

    rs->dev = NULL;

    rs->num_channels = 4;

    rs = &rs_rto_exp;

    rs->w = NULL;
    rs->num_windows = 0;

    init_prep_acq( );
    init_prep_trig( );
    init_prep_chans( );

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_test_hool( void )
{
    // Make a copy of the windows that may have been created during the
    // preparations section - they will have to be copied over to rs_rto_exp
    // each time a new experiment is started.

    copy_windows( &prep_wins, rs_rto_exp.w );
    delete_windows( &rs_rto_exp.w );

    rs_rto_test = rs_rto_exp;

    // Make sure the strings in the test structure are real copies of
    // the strings in the other

    for ( int i = Channel_Math1; i < Channel_Math4; i++ )
    {
        if ( rs_rto_test.chans[ i ].function )
            rs_rto_test.chans[ i ].function =
                                T_strdup( rs_rto_test.chans[ i ].function );
        else
            rs_rto_test.chans[ i ].function = T_strdup( "" );
    }

    // Make a copy of the windows created during preparations

    copy_windows( &rs_rto_test.w, prep_wins );

    rs = &rs_rto_test;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_end_of_test_hool( void )
{

    // Delete all math function strings and all windows from the test run,
    // they will never be used again

    for ( int i = Channel_Math1; i <= Channel_Math4; ++i )
        rs->chans[ i ].function = T_free( rs->chans[ i ].function );

    delete_windows( &rs_rto_test.w );

    rs = &rs_rto_exp;
    return 1;
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_exp_hook( void )
{
    char *err;

    if ( ! ( rs->dev = rs_rto_open( NETWORK_ADDRESS, LAN_LOG_LEVEL, &err ) ) )
    {
        print( FATAL, "%s", err );
        free( err );
        THROW( EXCEPTION );
    }

    // Check that neither during the preparations nor the test phase
    // any non-existing channels were used

    check( rs_rto_num_channels( rs->dev, &rs->num_channels ) );

    if ( rs->num_channels != 4 )
    {
        if (    rs->chans[ Channel_Ch3 ].in_use
             || rs_rto_test.chans[ Channel_Ch3 ].in_use
             || rs->chans[ Channel_Ch4 ].in_use
             || rs_rto_test.chans[ Channel_Ch4 ].in_use )
        {
            print( FATAL, "EDL script uses measirement channels 3 or 4, but "
                   "the device has only 2.\n" );
            
            THROW( EXCEPTION );
        }
    }

    // Get a copy of the windows created during the preparations section

    copy_windows( &rs_rto_exp.w, prep_wins );

    // Now set up the device the way it was requested during the preparations
    // phase - extend the error message a bit to make clear that errors
    // encountered are from this phase.

    err_prefix = "Durings device initialization: ";

    init_exp_acq( );
    init_exp_trig( );
    init_exp_chans( );

    err_prefix = "";

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_end_of_exp_hook( void )
{
    // Delete all windows used during the experiment

    delete_windows( &rs_rto_exp.w );

    // Disconnect from the device

    if ( rs->dev )
    {
        check( rs_rto_close( rs->dev ) );
        rs->dev = NULL;
    }

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_exit_hook( void )
{
    // Make sure we're disconnected (in case the end-of-experiment hook
    // never was run)

    if ( rs->dev )
    {
               rs_rto_close( rs->dev )  ;
        rs->dev = NULL;
    }

    // Delete all windows

    delete_windows( &rs_rto_test.w );
    delete_windows( &rs_rto_exp.w );
    delete_windows( &prep_wins );
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
            return vars_push( INT_VAR, rs->keyboard_locked ? 1L : 0L );
        else
        {
            check( rs_rto_keyboard_locked( rs->dev, &locked ) );
            return vars_push( INT_VAR, locked ? 1L : 0L );
        }
    }

    locked = get_boolean( v );
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
            check( rs_rto_display_enabled( rs->dev, &enabled ) );
            return vars_push( INT_VAR, enabled ? 1L : 0L );
        }
    }

    enabled = get_boolean( v );
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

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->acq.is_timebase )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, rs->acq.timebase );

            case EXPERIMENT :
                check( rs_rto_acq_timebase( rs->dev, &timebase ) );
                return vars_push( FLOAT_VAR, timebase );
        }

    timebase = get_double( v, "time base" );
    too_many_arguments( v );

    if ( timebase <= 0 )
    {
        print( FATAL, "Invalid zero or negative time base: %s.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->acq.is_timebase )
        {
            print( SEVERE, "Time base has already been set in preparations "
                   "section, discarding new value.\n" );
            return vars_push( FLOAT_VAR, rs->acq.timebase );
        }

        rs->acq.timebase = timebase;
        rs->acq.is_timebase = true;
        return vars_push( FLOAT_VAR, rs->acq.timebase = timebase );
    }

    double req_timebase = timebase;
    int ret = rs_rto_acq_set_timebase( rs->dev, &timebase );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_tb;
        double max_tb;

        check( rs_rto_acq_shortest_timebase( rs->dev, &min_tb ) );
        check( rs_rto_acq_longest_timebase( rs->dev, &max_tb ) );

        char * s1 = pp( req_timebase ); 
        char * s2 = pp( min_tb ); 
        char * s3 = pp( max_tb ); 

        print( FATAL, "Timebase of %ss out of range, must be between %ss and "
               "%ss.\n", s1, s2, s3 );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( fabs( timebase - req_timebase ) / req_timebase > 0.01 )
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
        check( rs_rto_acq_shortest_timebase( rs->dev, limits ) );
        check( rs_rto_acq_longest_timebase( rs->dev, limits + 1 ) );
    }

    return vars_push( FLOAT_ARR, limits, 2 );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_timebase_const_resolution_limits( Var_T * v  UNUSED_ARG )
{
    double limits[ 2 ] = { 1e-8, 50 };

    if ( FSC2_MODE == EXPERIMENT )
    {
        check( rs_rto_acq_shortest_timebase_const_resolution( rs->dev,
                                                              limits ) );
        check( rs_rto_acq_longest_timebase_const_resolution( rs->dev,
                                                             limits + 1 ) );
    }

    return vars_push( FLOAT_ARR, limits, 2 );
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
                check( rs_rto_acq_resolution( rs->dev, &resolution ) );
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

        check( rs_rto_acq_lowest_resolution( rs->dev, &min_res ) );
        check( rs_rto_acq_highest_resolution( rs->dev, &max_res ) );

        char * s1 = pp( req_resolution ); 
        char * s2 = pp( min_res ); 
        char * s3 = pp( max_res ); 

        print( FATAL, "Time per point of %ss out of range, must be between "
               "%ss and %ss.\n", s1, s2, s3 );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( fabs( resolution - req_resolution ) / req_resolution > 0.01 )
    {
        char * s1 = pp( req_resolution ); 
        char * s2 = pp( resolution ); 
        
        print( WARN, "Time per point has been set to %ss instead of "
               "%ss.\n", s2, s1 );

        T_free( s2 );
        T_free( s1 );
    }

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
        check( rs_rto_acq_lowest_resolution( rs->dev, limits ) );
        check( rs_rto_acq_highest_resolution( rs->dev, limits + 1 ) );
    }

    return vars_push( FLOAT_ARR, limits, 2 );
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
        if ( FSC2_MODE == PREPARATION && rs->acq.is_record_length )
        {
            print( SEVERE, "Record length has already been set in "
                   "preparations section, discarding new value.\n" );
            return vars_push( INT_VAR, rs->acq.record_length );
        }

        rs->acq.record_length = record_length;
        rs->acq.is_record_length = true;
        return vars_push( FLOAT_VAR, record_length );
    }

    rec_len = record_length;
    int ret = rs_rto_acq_set_record_length( rs->dev, &rec_len );
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        unsigned long min_rec_len;
        unsigned long max_rec_len;

        check( rs_rto_acq_min_record_length( rs->dev, &min_rec_len ) );
        check( rs_rto_acq_max_record_length( rs->dev, &max_rec_len ) );

        print( FATAL, "Requested record length of %ld out of range, must "
               "be between %lu and %lu.\n",
               record_length, min_rec_len, max_rec_len );
        THROW( EXCEPTION );    }

    if ( rec_len != ( unsigned long ) record_length )
        print( WARN, "Record length had to be adjusted fron %ld to %lu.\n",
               record_length, rec_len );

    return vars_push( INT_VAR, rec_len );
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_record_length_limits( Var_T * v  UNUSED_ARG )
{
    unsigned long limits[ 2 ] = { 1000, 100000000 };

    if ( FSC2_MODE == EXPERIMENT )
    {
        check( rs_rto_acq_min_record_length( rs->dev, limits ) );
        check( rs_rto_acq_max_record_length( rs->dev, limits + 1 ) );
    }

    return vars_push( INT_ARR, limits, 2 );
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
                check( rs_rto_acq_mode( rs->dev, &mode ) );
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
        if ( FSC2_MODE == PREPARATION && rs->acq.is_mode )
        {
            print( SEVERE, "Acquisition mode has already been set in "
                   "preparations section, discarding new value.\n" );
            return vars_push( INT_VAR, rs->acq.mode );
        }

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
        if ( FSC2_MODE == PREPARATION && rs->acq.is_num_averages )
        {
            print( SEVERE, "Number of averages has alredy been set "
                   "in preparations section, discarding new value.\n" );
            return vars_push( INT_VAR, rs->acq.num_averages );
        }

        rs->acq.num_averages = num_averages;
        rs->acq.is_num_averages = true;
        return vars_push( INT_VAR, num_averages );
    }

    n_avg = num_averages;
    int ret = rs_rto_acq_set_average_count( rs->dev, &n_avg );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        unsigned long max_avg;
        check( rs_rto_acq_max_average_count( rs->dev, &max_avg ) );
        print( FATAL, "Requested number of averagess %ld out of range, must "
               "be between 1 and %lu.\n", num_averages, max_avg );
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
        if ( FSC2_MODE == PREPARATION && rs->acq.is_num_segments )
        {
            print( SEVERE, "Number of segments has already been set in "
                   "preparations, discarding new value.\n" );
            return vars_push( INT_VAR, rs->acq.num_segments );
        }

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
               "be between 1 and %lu.\n", num_segments, max_seg );
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
    if ( FSC2_MODE == EXPERIMENT )
        check( rs_rto_acq_run_single( rs->dev ) );
    return vars_push( INT_VAR, 1L );
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
        if ( FSC2_MODE == PREPARATION && rs->chans[ rch ].is_state )
        {
            print( SEVERE, "State of channel %s has already been set "
                   "preparations, leaving value unchanged.\n",
                   Channel_Names[ fch ] );
            return vars_push( INT_VAR, rs->chans[ rch ].state ? 1L : 0L );
        }

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
        if ( FSC2_MODE == PREPARATION && rs->chans[ rch ].is_scale )
        {
            print( SEVERE, "Sensitivity of channel %s has alredy been set "
                   "in preparations section, discarding new value.\n",
                   Channel_Names[ fch ] );
            return vars_push( FLOAT_VAR, rs->chans[ rch ].scale );
        }

        rs->chans[ rch ].scale = scale;
        rs->chans[ rch ].is_scale = true;
        return vars_push( FLOAT_VAR, scale );
    }

    double req_scale = scale;
    int ret = rs_rto_channel_set_scale( rs->dev, rch, &scale );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_scale, max_scale;
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
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_offset, max_offset;
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

    if ( fabs( position ) >= 5.005 )
    {
        print( FATAL, "Requested channel position of %.2f div out of range, "
               "must be within +/- 5 div.\n", position );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->chans[ rch ].is_position )
        {
            print( SEVERE, "Position channel %s has already been set "
                   "preparations, leaving value unchanged.\n",
                   Channel_Names[ fch ] );
            return vars_push( FLOAT_VAR, rs->chans[ rch ].position );
        }

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
        if ( FSC2_MODE == PREPARATION && rs->chans[ rch ].is_coupling )
        {
            print( SEVERE, "Coupling of channel %s has alredy been set "
                   "in preparations section, discarding new value.\n",
                   Channel_Names[ fch ] );
            return vars_push( INT_VAR, rs->chans[ rch ].coupling );
        }

        rs->chans[ rch ].coupling = req_coup;
        rs->chans[ rch ].is_coupling = true;
        return vars_push( INT_VAR, req_coup );
    }

    coup = req_coup;
    check( rs_rto_channel_set_coupling( rs->dev, rch, &coup ) );
    return vars_push( INT_VAR, ( long ) coup );
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
            req_bw = Bandwidth_MHz20;
        else if ( ! strcasecmp( v->val.sptr, "200MHz" ) )
            req_bw = Bandwidth_MHz200;
        else if ( ! strcasecmp( v->val.sptr, "800MHz" ) )
            req_bw = Bandwidth_MHz800;
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
        rs->chans[ rch ].is_bandwidth = true;
        return vars_push( INT_VAR, req_bw );
    }

    bw = req_bw;
    check( rs_rto_channel_set_bandwidth( rs->dev, rch, &bw ) );
    return vars_push( INT_VAR, ( long ) bw );
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
                if ( ! rs->chans[ Channel_Ext ].is_ext_filter )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR,
                                  rs->chans[ Channel_Ext ].ext_filter );

            default :
            {
                check( rs_rto_channel_filter_type( rs->dev, Channel_Ext,
                                                   &ft ) );
                check( rs_rto_channel_cut_off( rs->dev, Channel_Ext, &fco ) );
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
                   "'%s'.\n", v->val.sptr );
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
        if (    FSC2_MODE == PREPARATION
             && rs->chans[ Channel_Ext ].is_ext_filter )
        {
            print( SEVERE, "Filter for external trigger input channel has "
                   "already been set preparations, leaving value "
                   "unchanged.\n" );
            return vars_push( INT_VAR,rs->chans[ Channel_Ext ].ext_filter );
        }

        rs->chans[ Channel_Ext ].ext_filter = ext_filter;
        rs->chans[ Channel_Ext ].is_ext_filter = true;
        return vars_push( INT_VAR, ext_filter );
    }

    check( rs_rto_channel_set_filter_type( rs->dev, Channel_Ext, &ft ) );
    check( rs_rto_channel_set_cut_off( rs->dev, Channel_Ext, &fco ) );

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

    long fch = get_strict_long( v, "channel" );
    int rch = fsc2_ch_2_rto_ch( fch );

    if ( rch == Channel_Ext )
    {
        print( FATAL, "Can't get curve from external trigger input "
               "channel.\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_Win * w = NULL;
    if ( ( v = vars_pop( v ) ) )
         w = get_window( v );

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        size_t np;
        if ( ! w )
            np = lrnd( rs->acq.timebase / rs->acq.resolution );
        else
            np = lrnd( ( w->end - w->start ) / rs->acq.resolution );

        Var_T * nv = vars_push( FLOAT_ARR, NULL, np );
        double * dp = nv->val.dpnt;
        for ( size_t i = 0; i < np; i++ )
            dp[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );

        return nv;
    }

    double * data;
    size_t length;

    get_waveform( rch, w, &data, &length );

    Var_T * nv;
    CLOBBER_PROTECT( data );

    TRY
    {
        nv = vars_push( FLOAT_ARR, data, ( ssize_t ) length );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        free( data );
        RETHROW;
    }

    free( data );

    return nv;
}       


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_get_area( Var_T * v )
{
    return get_calculated_curve_data( v, area );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_get_amplitude( Var_T * v )
{
    return get_calculated_curve_data( v, amplitude );
}




/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_get_segments( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long fch = get_strict_long( v, "channel" );
    int rch = fsc2_ch_2_rto_ch( fch );

    if ( rch == Channel_Ext )
    {
        print( FATAL, "Can't get curve from external trigger input "
               "channel.\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_Win * w = NULL;
    if ( ( v = vars_pop( v ) ) )
         w = get_window( v );

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        long ns = rs->acq.num_segments;
        long np;
        if ( ! w )
            np = lrnd( rs->acq.timebase / rs->acq.resolution );
        else
            np = lrnd( ( w->end - w->start ) / rs->acq.resolution );

        Var_T * nv = vars_push_matrix( FLOAT_REF, 2, ns, np );

        for ( long i = 0; i < ns; i++ )
        {
            double * dp = nv->val.vptr[ i ]->val.dpnt;
            for ( long j = 0; j < np; j++ )
                *dp++ = 1.0e-7 * sin( M_PI * j / 122.0 );
        }

        return nv;
    }

    double ** data;
    size_t num_segments;
    size_t length;

    CLOBBER_PROTECT( data );

    get_segments( rch, w, &data, &num_segments, &length );

    Var_T * nv;

    TRY
    {
        nv = vars_push_matrix( FLOAT_REF,  2,
                               ( long ) num_segments, ( long ) length );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        free( *data );
        free( data );
        RETHROW;
    }

    for ( size_t i = 0; i < num_segments; i++ )
        memcpy( nv->val.vptr[ i ]->val.dpnt, data[ i ],
                length * sizeof **data );

    free( *data );
    free( data );

    return nv;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_available_segments( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, rs->acq.num_segments );

    unsigned long count;
    check( rs_rto_acq_available_segments( rs->dev, &count ) );
    return vars_push( INT_VAR, ( long ) count );
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
                                  rto_ch_2_fsc2_ch( rs->trig.source ) );

            default :
                check( rs_rto_trigger_source( rs->dev, &rch ) );
                return vars_push( INT_VAR, rto_ch_2_fsc2_ch( rch ) );
        }

    long fch = get_strict_long( v, "trigger_channel" );
    too_many_arguments( v );
    rch = fsc2_ch_2_rto_ch( fch );

    if ( rch >= Channel_Math1 )
    {
        print( FATAL, "Math cnannels can't be used as trigegr channels" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->trig.is_source )
        {
            print( SEVERE, "Trigger channel has alredy been set "
                   "in preparations section, discarding new value.\n" );
            return vars_push( INT_VAR, rs->trig.source );
        }

        rs->trig.source = rch;
        rs->trig.is_source = true;
        return vars_push( INT_VAR, fch );
    }

    check( rs_rto_trigger_set_source( rs->dev, &rch ) );
    return vars_push( INT_VAR, rto_ch_2_fsc2_ch( rch ) );
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
                check( rs_rto_trigger_channel_level( rs->dev, rch, &level ) );
                return vars_push( FLOAT_VAR, level );
        }

    double req_level = get_double( v, "trigger_levek" );

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->trig.is_level )
        {
            print( SEVERE, "Trigger level for channel %s has already been "
                   "set in preparations section, discarding new value.\n",
                   Channel_Names[ fch ] );
            return vars_push( FLOAT_VAR, rs->trig.level[ rch ] );
        }

        rs->trig.level[ rch ] = req_level;
        rs->trig.is_level[ rch ] = true;
        return vars_push( FLOAT_VAR, req_level );
    }

    level = req_level;
    int ret = rs_rto_trigger_set_channel_level( rs->dev, rch, &level );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_level, max_level;

        check( rs_rto_trigger_channel_min_level( rs->dev, rch, &min_level ) );
        check( rs_rto_trigger_channel_min_level( rs->dev, rch, &max_level ) );

        char * s1 = pp( req_level );
        char * s2 = pp( min_level );
        char * s3 = pp( max_level );

        print( FATAL, "Requested trigger level of %sV oot of range, must be "
               "between %sV and %sV.\n", s1, s2, s3 );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( req_level != 0 && fabs( ( req_level - level ) / req_level ) > 0.01 )
    {
        char * s1 = pp( req_level );
        char * s2 = pp( level );

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

    if ( rch >= Channel_Ch1 )
    {
        print( FATAL, "Math channel can't be a trigger channel" );
        THROW( EXCEPTION );
    }

    int slope;

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
                check( rs_rto_trigger_channel_slope( rs->dev, rch, &slope ) );
                return vars_push( INT_VAR, ( long ) slope );
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
        if ( FSC2_MODE == PREPARATION && rs->trig.is_slope[ rch ] )
        {
            print( SEVERE, "Trigger slope for channel %s has alredy been set "
                   "in preparations section, discarding new value.\n",
                   Channel_Names[ fch ] );
            return vars_push( INT_VAR, rs->trig.slope[ rch ] );
        }

        rs->trig.slope[ rch ] = req_slope;
        rs->trig.is_slope[ rch ] = true;
        return vars_push( INT_VAR, req_slope );
    }

    slope = req_slope;
    check( rs_rto_trigger_set_channel_slope( rs->dev, rch, &slope ) );
    return vars_push( INT_VAR, ( long ) slope );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_mode( Var_T * v )
{
    int mode;

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
                return vars_push( INT_VAR, ( long ) mode );
        }

    long req_mode;

    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "AUTO" ) )
            req_mode = Trig_Mode_Auto;
        else if (    ! strcasecmp( v->val.sptr, "NORMAL" )
                  || ! strcasecmp( v->val.sptr, "NORM" ) )
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
        if ( FSC2_MODE == PREPARATION && rs->trig.is_mode )
        {
            print( SEVERE, "Trigger mode has alredy been set "
                   "in preparations section, discarding new value.\n" );
            return vars_push( INT_VAR, rs->trig.mode );
        }

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
                return vars_push( FLOAT_VAR, rs->trig.position );

            default :
                check( rs_rto_trigger_position( rs->dev, &pos) );
                return vars_push( FLOAT_VAR, pos );
        }

    double req_pos = get_double( v, "trigger_delay" );
    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->trig.is_position )
        {
            print( SEVERE, "Trigger dekay has alredy been set "
                   "in preparations section, discarding new value.\n" );
            return vars_push( FLOAT_VAR, rs->trig.position );
        }

        rs->trig.position = req_pos;
        rs->trig.is_position = true;
        return vars_push( FLOAT_VAR, req_pos );
    }

    pos = req_pos;
    int ret = rs_rto_trigger_set_position( rs->dev, &pos );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_pos, max_pos;
        check( rs_rto_trigger_earliest_position( rs->dev, &min_pos ) );
        check( rs_rto_trigger_latest_position( rs->dev, &max_pos ) );

        char * s1 = pp( req_pos );
        char * s2 = pp( min_pos );
        char * s3 = pp( max_pos );

        print( FATAL, "Requested trigger delay of %ss is out of range, must "
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
        T_free( s1 );
    }

    return vars_push( FLOAT_VAR, pos );
}


/*------------------------------------*
 *------------------------------------*/

Var_T *
digitizer_define_window( Var_T * v )
{
    if ( ! v || ! v->next )
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

    RS_RTO_Win * w;

    if ( ! rs->w )
    {
        rs->w = w = T_malloc( sizeof *w );
    }
    else
    {
        w = rs->w;
        while ( w->next )
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
    if ( ! rs->w )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_Win * w = get_window( v );

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
    if ( ! rs->w )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_Win * w = get_window( v );

    if ( ! ( v = vars_pop( v ) ) )
        return vars_push( FLOAT_VAR, w->end - w->start );

    double width = get_double( v, "window width" );
    too_many_arguments( v );

    if ( width <= 0 )
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
digitizer_change_window( Var_T * v )
{
    if ( ! rs->w )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( ! v || ! v->next || ! v->next->next )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    RS_RTO_Win * w = get_window( v );
    v = vars_pop( v );

    w->start = get_double( v, "window position" );
    v = vars_pop( v );
    double width = get_double( v, "window width" );
    too_many_arguments( v );

    if ( width <= 0 )
    {
        print( FATAL, "Invalid zero or negative window width.\n" );
        THROW( EXCEPTION );
    }

    w->end = w->start + width;

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

    RS_RTO_Win * w = get_window( v );
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
digitizer_window_limits( Var_T * v  UNUSED_ARG )
{
    double limits[ 2 ];

    if ( FSC2_MODE != EXPERIMENT )
    {
        limits[ 0 ] = -1.0e24;
        limits[ 1 ] =  2.0e24;
        return vars_push( FLOAT_ARR, limits, 2 );
    }

    double min_start, max_end;
    check( rs_rto_acq_max_download_limits( rs->dev, &min_start, &max_end ) );

    limits[ 0 ] = min_start;
    limits[ 1 ] = max_end - min_start;
    return vars_push( FLOAT_ARR, limits, 2 );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_math_function( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "Missing argument.\n" );
        THROW( EXCEPTION );
    }

    long fch = get_strict_long( v, "math channel" );
    v = vars_pop( v );
    int rch = fsc2_ch_2_rto_ch( fch );

    if ( rch < Channel_Math4 )
    {
        print( FATAL, "Function can only be used with math channels.\n" );
        THROW( EXCEPTION );
    }

    char * func = NULL;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->chans[ rch ].function )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( STR_VAR, rs->chans[ rch ].function );

            default :
                check( rs_rto_channel_function( rs->dev, rch, &func ) );
                Var_T * nv = vars_push( STR_VAR, func );
                free( func );
                return nv;

        }

    if ( v->type != STR_VAR )
    {
        print( FATAL, "Function to set for a math channel must be a "
               "string.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->chans[ rch ].function )
        {
            print( SEVERE, "Funcion for math channel %s already has been "
                   "set in proparations, leaving it unchanged.\n",
                Channel_Names[ fch ] );
            return vars_push( STR_VAR, rs->chans[ rch ].function );
        }

        T_free( rs->chans[ rch ].function );
        rs->chans[ rch ].function = T_strdup( v->val.sptr );
    }
    else
        check( rs_rto_channel_set_function( rs->dev, rch, v->val.sptr ) );

    return vars_push( INT_VAR, 1L );
}   


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_out_pulse_state( Var_T * v )
{
    bool state;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->trig.is_out_pulse_state )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->trig.out_pulse_state ? 1L : 0L );

            case EXPERIMENT :
                check( rs_rto_trigger_out_pulse_state( rs->dev, &state ) );
                return vars_push( INT_VAR, state );
        }

    state = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->trig.is_out_pulse_state )
        {
            print( SEVERE, "Trigger out pulse state has already been set in "
                   "propartions section, discarding new value.\n" );
            return vars_push( INT_VAR, rs->trig.out_pulse_state ? 1L : 0L );
        }

        rs->trig.out_pulse_state = state;
        rs->trig.is_out_pulse_state = true;
    }
    else
        check( rs_rto_trigger_out_pulse_state( rs->dev, &state ) );

    return vars_push( INT_VAR, state ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_out_pulse_length( Var_T * v )
{
    double len;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->trig.is_out_pulse_length )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, rs->trig.out_pulse_length );

            case EXPERIMENT :
                check( rs_rto_trigger_out_pulse_length( rs->dev, &len ) );
                return vars_push( FLOAT_VAR, len );
        }

    double req_len = get_double( v, "trigger out pulse length" );
    too_many_arguments( v );

    if ( req_len < 2e-9 || req_len >= 1.000002e-3 )
    {
        char *s1 = pp( req_len );

        print( FATAL, "Requested trigger out pulse length of %ss is out of "
               "range, absolute limits are 4 ns to 1 ms.\n", s1 );
        T_free( s1 );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->trig.is_out_pulse_length )
        {
            print( SEVERE, "Trigger out pulse length has already been set in "
                   "propartions section, discarding new value.\n" );
            return vars_push( FLOAT_VAR, rs->trig.out_pulse_length );
        }
        
        rs->trig.out_pulse_length = req_len;
        rs->trig.is_out_pulse_length = true;
            return vars_push( FLOAT_VAR, req_len );
    }

    len = req_len;
    check( rs_rto_trigger_set_out_pulse_length( rs->dev, &len ) );

    if ( fabs( req_len - len ) / req_len > 0.01 )
    {
        char * s1 = pp( req_len );
        char * s2 = pp( len );

        print( WARN, "Trigger out pulse length had to be adjusted from %ss "
               "to %ss.\n", s1, s2 );

        T_free( s2 );
        T_free( s1 );
    }

    return vars_push( FLOAT_VAR, len );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_out_pulse_polarity( Var_T * v )
{
    int pol;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->trig.is_out_pulse_polarity )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, rs->trig.out_pulse_polarity );

            case EXPERIMENT :
                check( rs_rto_trigger_out_pulse_polarity( rs->dev, &pol ) );
                return vars_push( INT_VAR, pol );
        }

    if ( v->type == STR_VAR )
    {
        if (    ! strcasecmp( v->val.sptr, "POSITIVE" )
             || ! strcasecmp( v->val.sptr, "POS" ) )
            pol = Polarity_Positive;
        else if (    ! strcasecmp( v->val.sptr, "NEGATIVE" )
                  || ! strcasecmp( v->val.sptr, "NEG" ) )
            pol = Polarity_Negative;
        else
        {
            print( FATAL, "Invalid trigger out pulse polarity: '%s'.\n",
                   v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
    {
        long req_pol = get_strict_long( v, "trigger out pulse polarity" );
        if ( req_pol == 0 )
            pol = Polarity_Negative;
        else if ( req_pol == 1 )
            pol = Polarity_Positive;
        else
        {
            print( FATAL, "Invalid trigger out pulse polarity: %ld.\n",
                   req_pol );
            THROW( EXCEPTION );
        }
    }
            
    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->trig.is_out_pulse_polarity )
        {
            print( SEVERE, "Trigger out pulse polarity has already been set in "
                   "propartions section, discarding new value.\n" );
            return vars_push( INT_VAR, rs->trig.out_pulse_polarity );
        }
        
        rs->trig.out_pulse_polarity = pol;
        rs->trig.is_out_pulse_polarity = true;
        return vars_push( INT_VAR, ( long ) pol );
    }

    check( rs_rto_trigger_set_out_pulse_polarity( rs->dev, &pol ) );
    return vars_push( INT_VAR, ( long ) pol );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_out_pulse_delay( Var_T * v )
{
    double delay;

    if ( ! v )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs->trig.is_out_pulse_delay )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, rs->trig.out_pulse_delay );

            case EXPERIMENT :
                check( rs_rto_trigger_out_pulse_delay( rs->dev, &delay ) );
                return vars_push( FLOAT_VAR, delay );
        }

    double req_delay = get_double( v, "trigger out pulse delay" );
    too_many_arguments( v );

    if ( req_delay <= 0 )
    {
        print( FATAL, "Invalid zero or negative trigger out pulse delay.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( FSC2_MODE == PREPARATION && rs->trig.is_out_pulse_delay )
        {
            print( SEVERE, "Trigger out pulse delay has already been set in "
                   "propartions section, discarding new value.\n" );
            return vars_push( FLOAT_VAR, rs->trig.out_pulse_delay );
        }
        
        rs->trig.out_pulse_delay = req_delay;
        rs->trig.is_out_pulse_delay = true;
            return vars_push( FLOAT_VAR, req_delay );
    }

    delay = req_delay;
    int ret = rs_rto_trigger_set_out_pulse_delay( rs->dev, &delay );
    if ( ret != FSC3_SUCCESS )
    {
        if ( ret != FSC3_INVALID_ARG )
            check( ret );

        double min_delay, max_delay;

        check( rs_rto_trigger_min_out_pulse_delay( rs->dev, &min_delay ) );
        check( rs_rto_trigger_max_out_pulse_delay( rs->dev, &max_delay ) );

        char * s1 = pp( req_delay );
        char * s2 = pp( min_delay );
        char * s3 = pp( max_delay );

        print( FATAL, "Requested trigger out pulse delay of %ss is out of "
               "range, must be between %ss and %ss.\n", s1, s2, s3 );

        T_free( s3 );
        T_free( s2 );
        T_free( s1 );

        THROW( EXCEPTION );
    }

    if ( fabs( req_delay - delay ) / req_delay > 0.01 )
    {
        char * s1 = pp( req_delay );
        char * s2 = pp( delay );

        print( WARN, "Trigger out pulse delay had to be adjusted from %ss "
               "to %ss.\n", s1, s2 );

        T_free( s2 );
        T_free( s1 );
    }

    return vars_push( FLOAT_VAR, delay );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_trigger_out_pulse_delay_limits( Var_T * v  UNUSED_ARG )
{
    double limits[ 2 ] = { 8e-7, 1 };

    if ( FSC2_MODE == EXPERIMENT )
    {
        check( rs_rto_trigger_min_out_pulse_delay( rs->dev, limits ) );
        check( rs_rto_trigger_max_out_pulse_delay( rs->dev, limits + 1 ) );
    }

    return vars_push( FLOAT_ARR, limits, 2 );
}


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
    int rch = -1;

    if ( ch == CHANNEL_CH1 )
        return Channel_Ch1;
    else if ( ch == CHANNEL_CH2 )
        rch = Channel_Ch2;
    else if ( ch == CHANNEL_CH3 && rs->num_channels == 4 )
        rch = Channel_Ch3;
    else if ( ch == CHANNEL_CH4 && rs->num_channels == 4 )
        rch = Channel_Ch4;
    else if ( ch == CHANNEL_EXT )
        rch = Channel_Ext;
    else if ( ch == CHANNEL_MATH1 )
        rch = Channel_Math1;
    else if ( ch == CHANNEL_MATH2 )
        rch = Channel_Math2;
    else if ( ch == CHANNEL_MATH3 )
        rch = Channel_Math3;
    else if ( ch == CHANNEL_MATH4 )
        rch = Channel_Math4;
    else
    {
        if ( ch == CHANNEL_CH3 || ch == CHANNEL_CH4 )
            print( FATAL, "This model has only 2 measurement channels, can't "
                   "use channel '%s'.\n", Channel_Names[ ch ] );
        else if ( ch < CHANNEL_CH1 || ch >= NUM_CHANNEL_NAMES )
            print( FATAL, "Invalid channel number '%d'.\n", ch );
        else
            print( FATAL, "Channel '%s' does not exist on this device.\n",
                   Channel_Names[ ch ] );

        THROW( EXCEPTION );
    }

    rs->chans[ rch ].in_use = true;
    return rch;
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

    if ( ch < 0 || ch >= ( int ) ( sizeof chs / sizeof *chs ) )
        fsc2_impossible( );

    return chs[ ch ];
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
check( int err_code )
{
    if ( err_code == FSC3_SUCCESS )
        return;

    if ( ! rs->dev )
        fsc2_impossible( );

    print( FATAL, "%s%s", err_prefix, rs_rto_last_error( rs->dev ) );
    err_prefix = "";

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
        if ( fco == Filter_Cut_Off_kHz5 )
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
    else if ( e == Ext_Filter_Low_Pass_5kHz )
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
    else if ( e == Ext_Filter_High_Pass_5kHz )
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


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
get_waveform( int           rch,
              RS_RTO_Win  * w,
              double     ** data,
              size_t      * length )
{
    bool ch_state;
    check( rs_rto_channel_state( rs->dev, rch, &ch_state ) );
    if ( ! ch_state )
    {
        print( FATAL, "Can't get data from channel which isn't "
               "switched on.\n" );
        THROW( EXCEPTION );
    }

    bool with_limits = w != NULL;
    if ( with_limits )
    {
        double ws = w->start;
        double we = w->end;
        int ret = rs_rto_acq_set_download_limits( rs->dev, &ws, &we );
        if ( ret != FSC3_SUCCESS )
        {
            if ( ret != FSC3_INVALID_ARG )
                check( ret );

            print( FATAL, "Window does not fit waveform range.\n" );
            THROW( EXCEPTION );
        }
    }
    
    while ( 1 )
    {
        stop_on_user_request( );
        bool is_running;
        check( rs_rto_acq_is_running( rs->dev, &is_running ) );
        if ( ! is_running )
            break;
    }

    check( rs_rto_acq_set_download_limits_enabled( rs->dev, &with_limits ) );
    check( rs_rto_channel_data( rs->dev, rch, data, length ) );

    if ( *length == 0 )
    {
        free( *data );
        print( FATAL, "Curve has no data.\n" );
        THROW( EXCEPTION );
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
get_segments( int            rch,
              RS_RTO_Win   * w,
              double     *** data,
              size_t       * num_segments,
              size_t       * length )
{
    bool ch_state;
    check( rs_rto_channel_state( rs->dev, rch, &ch_state ) );
    if ( ! ch_state )
    {
        print( FATAL, "Can't get data from channel which isn't "
               "switched on.\n" );
        THROW( EXCEPTION );
    }

    bool with_limits = w != NULL;
    if ( with_limits )
    {
        double ws = w->start;
        double we = w->end;
        int ret = rs_rto_acq_set_download_limits( rs->dev, &ws, &we );
        if ( ret != FSC3_SUCCESS )
        {
            if ( ret != FSC3_INVALID_ARG )
                check( ret );

            print( FATAL, "Window does not fit waveform range.\n" );
            THROW( EXCEPTION );
        }
    }

    check( rs_rto_acq_set_download_limits_enabled( rs->dev, &with_limits ) );

    while ( 1 )
    {
        stop_on_user_request( );
        bool is_running;
        check( rs_rto_acq_is_running( rs->dev, &is_running ) );
        if ( ! is_running )
            break;
    }

    check( rs_rto_channel_segment_data( rs->dev, rch, data,
                                        num_segments, length ) );

    if ( ! *num_segments || ! *length )
    {
        free( **data );
        free( *data );
        print( FATAL, "Segments have no data.\n" );
        THROW( EXCEPTION );
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
RS_RTO_Win *
get_window( Var_T * v )
{
    return get_window_from_long( get_strict_long( v, "window ID" ) );

}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
RS_RTO_Win *
get_window_from_long( long wid )
{
    if ( ! rs->w )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( wid >= WINDOW_START_NUMBER )
        for ( RS_RTO_Win * w = rs->w; w != NULL; w = w->next )
            if ( w->num == wid )
                return w;

    print( FATAL, "Argument isn't a valid window ID.\n" );
    THROW( EXCEPTION );

    return NULL;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
get_window_list( Var_T       ** v,
                 RS_RTO_Win *** wins,
                 long         * win_count )
{
    CLOBBER_PROTECT( wins );

    if ( *v )
    {
        if ( ( *v )->type == INT_VAR )
        {
            int count = 1;
            Var_T * vn = ( *v )->next;
            while ( vn )
            {
                count++;
                vn = vn->next;
            }

            *wins = T_malloc( count * sizeof **wins );

            TRY
            {
                do
                {
                    ( *wins )[ *win_count ] = get_window( *v );
                    *win_count += 1;
                } while ( ( *v = vars_pop( *v ) ) );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                T_free( *wins );
                RETHROW;
            }
        }
        else if ( ( *v )->type == INT_ARR )
        {
            *wins = T_malloc( ( *v )->len * sizeof *wins );

            TRY
            {
                for ( ssize_t i = 0; i < ( *v )->len; ++i )
                {
                    ( *wins )[ *win_count ] =
                                  get_window_from_long( ( *v )->val.lpnt[ i ] );
                    *win_count += 1;
                }
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                T_free( *wins );
                RETHROW;
            }

            *v = vars_pop( *v );
        }
        else
        {
            print( FATAL, "Argument neither a list nor an array of window "
                   "IDs.\n" );
            THROW( EXCEPTION );
        }
    }
}


/*----------------------------------------------------*
 * Helper function for digitizer_get_area() and
 * digitizer_get_amplitude - they just differ in the
 * math done on the data which can easily be dealt
 * with using a handler function for this/
 *----------------------------------------------------*/

Var_T *
get_calculated_curve_data( Var_T  * v,
                           double   ( handler )( double *, size_t ) )
{
    // We need at least a channel

    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long fch = get_strict_long( v, "channel" );
    v = vars_pop( v );
    int rch = fsc2_ch_2_rto_ch( fch );

    if ( rch == Channel_Ext )
    {
        print( FATAL, "Can't get curve from external trigger input "
               "channel.\n" );
        THROW( EXCEPTION );
    }

    // Check for windows - this could be either an array with windo w IDs
    // or simply a list of them. get_window_list() leaves the 'wins'
    // argument unchanged if there are no windows.

    RS_RTO_Win ** wins = NULL;
    long win_count = 0;

    CLOBBER_PROTECT( wins );

    get_window_list( &v, &wins, &win_count );

    too_many_arguments( v );

    // Just return some dummy data during a test run

    if ( FSC2_MODE != EXPERIMENT )
    {
        T_free( wins );

        if ( win_count <= 1 )
            return vars_push( FLOAT_VAR, handler( NULL, 0 ) );

        Var_T * nv = vars_push( FLOAT_ARR, NULL, win_count );

        for ( long i = 0; i < win_count; ++i )
            nv->val.dpnt[ i ] = handler( NULL, 0 );

        return nv;
    }
        
    // If the number of windows isn't larger than 1 things are straight
    // forward

    if ( win_count <= 1 )
    {
        double * data;
        size_t length;

        TRY
        {
            get_waveform( rch, wins ? *wins : NULL, &data, &length );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( wins );
            RETHROW;
        }

        double res = handler( data, length );

        free( data );
        T_free( wins );
        return vars_push( FLOAT_VAR, res );
    }

    // With several windows things get a bit more involved, use a helper
    // function for dealing with that.

    Var_T * nv;

    TRY
    {
        nv = get_subcurve_data( rch, wins, win_count, handler );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( wins );
        RETHROW;
    }

    T_free( wins );
    return nv;
}


/*----------------------------------------------------*
 * Helper function for getting area/amplitude data when
 * more than 1 window is involved. Due to the overhead of
 * fetching a curve (even a partial one) it might be a lot
 * faster getting only one, covering all windows and then
 * picking data from that. Only of the windows are far
 * apart with lots of data in between it might be better
 * doing several fetches.
 *----------------------------------------------------*/

static
Var_T *
get_subcurve_data( int rch,
                   RS_RTO_Win ** wins,
                   long          win_count,
                   double        ( *handler )( double *, size_t ) )
{
    // Make an estimate if fetches of several partial curve will be faster
    // then a single fetch of the part that covers the whole range of
    // windows.

    double reso;
    check( rs_rto_acq_resolution( rs->dev, &reso ) );

    double min_pos = wins[ 0 ]->start;
    double max_pos = wins[ 0 ]->end;
    double max_width = max_pos - min_pos;
    double tot_width = max_width;

    for ( long i = 1; i < win_count; ++i )
    {
        min_pos = d_min( min_pos, wins[ i ]->start );
        max_pos = d_max( max_pos, wins[ i ]->end );
        max_width = d_max( max_width, wins[ i ]->end - wins[ i ]->start );
        tot_width += wins[ i ]->end - wins[ i ]->start;
    }

    double single_time =   CURVE_DELAY
                         + ( max_pos - min_pos )
                         / ( 0.25 * TRANSFER_RATE * reso );
    double multi_time  =   CURVE_DELAY * win_count
                         + tot_width /  ( 0.25 * TRANSFER_RATE * reso );

    Var_T * nv = vars_push( FLOAT_ARR, NULL, win_count );
    double * data;
    size_t length;

    if ( single_time <= multi_time )
    {
        RS_RTO_Win win = { min_pos, max_pos, 0, NULL };

        get_waveform( rch, &win, &data, &length );

        for ( long i = 0; i < win_count; ++i )
        {
            long start_ind = lrnd( ( wins[ i ]->start - min_pos ) / reso );
            long end_ind   =   length
                             - lrnd( ( max_pos - wins[ i ]->end ) / reso );
            nv->val.dpnt[ i ] = handler( data + start_ind,
                                         end_ind - start_ind );
        }

        free( data );
    }
    else
    {
        for ( long i = 0; i < win_count; ++i )
        {
            get_waveform( rch, wins[ i ], &data, &length );
            nv->val.dpnt[ i ] = handler( data, length );
            free( data );
        }
    }

    return nv;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
double
area( double * data,
      size_t   length )
{
    if ( FSC2_MODE != EXPERIMENT )
        return 1.234e-8;

    double res = 0;
    for ( size_t i = 0; i < length; i++ )
        res += *data++;
    return res;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
double
amplitude( double * data,
           size_t   length )
{
    if ( FSC2_MODE != EXPERIMENT )
        return 1.234e-7;

    double min = HUGE_VAL;
    double max = - HUGE_VAL;
    for ( size_t i = 0; i < length; i++ )
    {
        min = d_min( min, *data );
        max = d_max( max, *data++ );
    }

    return max - min;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
init_prep_acq( void )
{
    rs->acq.is_timebase = false;
    rs->acq.timebase = 1e-7;

    rs->acq.is_resolution = false;
    rs->acq.resolution = 1e-10;

    rs->acq.is_record_length = false;
    rs->acq.record_length = 10000;

    rs->acq.is_mode = false;
    rs->acq.mode = Acq_Mode_Normal;

    rs->acq.is_num_averages = false;
    rs->acq.num_averages = 10;

    rs->acq.is_num_segments = false;
    rs->acq.num_segments = 10;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
init_prep_trig( void )
{
    rs->trig.source = Channel_Ch1;
    rs->trig.is_source = false;

    for ( int i = Channel_Ext; i <= Channel_Ch4; i++ )
    {
        rs->trig.level[  i ] = 0.0;
        rs->trig.is_level[ i ] = false;

        rs->trig.slope[ i ] = Trig_Slope_Positive;
        rs->trig.is_slope[ i ] =  false;
    }

    rs->trig.mode = Trig_Mode_Normal;
    rs->trig.is_mode = false;

    rs->trig.position = 5.0e-8;
    rs->trig.is_position = false;

    rs->trig.out_pulse_state = false;
    rs->trig.is_out_pulse_state = false;

    rs->trig.out_pulse_length = 1e-7;
    rs->trig.is_out_pulse_length = false;

    rs->trig.out_pulse_polarity = Polarity_Positive;
    rs->trig.is_out_pulse_polarity = false;

    rs->trig.out_pulse_delay = 8e-7;
    rs->trig.is_out_pulse_delay = false;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
init_prep_chans( void )
{
    for ( int i = 0; i <= Channel_Math4; i++ )
    {
        RS_RT_Chan * ch = rs->chans + i;

        ch->in_use = false;

        ch->state = false;
        ch->is_state = false;

        ch->scale = 0.05;
        ch->is_scale = false;

        ch->offset = 0;
        ch->is_offset = false;

        ch->position = 0;
        ch->is_position = false;

        ch->coupling = Coupling_AC;
        ch->is_coupling = false;

        ch->bandwidth = Bandwidth_Full;
        ch->is_bandwidth = false;

        ch->ext_filter = Ext_Filter_Off;
        ch->is_ext_filter = true;

        ch->function = NULL;
    }

    rs->chans[ Channel_Ch1 ].state = true;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
init_exp_acq( void )
{
    if ( rs->acq.is_timebase )
    {
        double tb = rs->acq.timebase;
        check( rs_rto_acq_set_timebase( rs->dev, &tb ) );
    }

    if ( rs->acq.is_record_length )
    {
        unsigned long rl = rs->acq.record_length;
        check( rs_rto_acq_set_record_length( rs->dev, &rl ) );
    }

    if ( rs->acq.is_mode )
    {
        int mode = rs->acq.mode;
        check( rs_rto_acq_set_mode( rs->dev, &mode ) );
    }

    if ( rs->acq.is_num_averages )
    {
        unsigned long na = rs->acq.num_averages;
        check( rs_rto_acq_set_average_count( rs->dev, &na ) );
    }

    if ( rs->acq.is_num_segments )
    {
        unsigned long ns = rs->acq.num_segments;
        check( rs_rto_acq_set_segment_count( rs->dev, &ns ) );
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
init_exp_trig( void )
{
    if ( rs->trig.is_source )
    {
        int source = rs->trig.source;
        check( rs_rto_trigger_set_source( rs->dev, &source ) );
    }

    for ( int i = Channel_Ext; i <= Channel_Ch4; i++ )
    {
        if ( rs->trig.is_level[ i ] )
        {
            double lvl = rs->trig.level[ i ];
            check( rs_rto_trigger_set_channel_level( rs->dev, i, &lvl ) );
        }
                
        if ( rs->trig.is_slope[ i ] )
        {
            int slope = rs->trig.slope[ i ];
            check( rs_rto_trigger_set_channel_slope( rs->dev, i, &slope ) );
        }
    }

    if ( rs->trig.is_mode )
    {
        int mode = rs->trig.mode;
        check( rs_rto_trigger_set_mode( rs->dev, &mode ) );
    }

    if ( rs->trig.is_position )
    {
        double pos = rs->trig.position;
        check( rs_rto_trigger_set_position( rs->dev, &pos ) );
    }

    if ( rs->trig.is_out_pulse_state )
    {
        bool state = rs->trig.out_pulse_state;
        check( rs_rto_trigger_set_out_pulse_state( rs->dev, &state ) );
    }

    if ( rs->trig.is_out_pulse_length )
    {
        double len = rs->trig.out_pulse_length;
        check( rs_rto_trigger_set_out_pulse_length( rs->dev, &len ) );
    }

    if ( rs->trig.is_out_pulse_polarity )
    {
        int pol = rs->trig.out_pulse_polarity;
        check( rs_rto_trigger_set_out_pulse_polarity( rs->dev, &pol ) );
    }

    if ( rs->trig.is_out_pulse_delay )
    {
        double delay = rs->trig.out_pulse_delay;
        check( rs_rto_trigger_set_out_pulse_delay( rs->dev, &delay ) );
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
init_exp_chans( void )
{
    for ( int i = Channel_Ext; i <= Channel_Math4; i++ )
    {
        RS_RT_Chan * ch = rs->chans + i;

        if ( ! ch->in_use )
            continue;

        if ( ch->is_state )
        {
            bool state = ch->state;
            check( rs_rto_channel_set_state( rs->dev, i, &state ) );
        }

        if ( ch->is_scale )
        {
            double scale = ch->scale;
            check( rs_rto_channel_set_scale( rs->dev, i, &scale ) );
        }

        if ( ch->is_offset )
        {
            double offs = ch->offset;
            check( rs_rto_channel_set_offset( rs->dev, i, &offs ) );
        }

        if ( ch->is_position )
        {
            double pos = ch->position;
            check( rs_rto_channel_set_position( rs->dev, i, &pos ) );
        }

        if ( ch->is_coupling )
        {
            int coup = ch->coupling;
            check( rs_rto_channel_set_coupling( rs->dev, i, &coup ) );
        }

        if ( ch->is_bandwidth )
        {
            int bw = ch->bandwidth;
            check( rs_rto_channel_set_bandwidth( rs->dev, i, &bw ) );
        }

        if ( ch->is_ext_filter )
        {
            int ft, fco;

            from_ext_filter( ch->ext_filter, &ft, &fco );

            check( rs_rto_channel_set_filter_type( rs->dev, i, &ft ) );
            if ( ft != Filter_Type_Off )
                check( rs_rto_channel_set_cut_off( rs->dev, i, &fco ) );
        }

        if ( ch->function )
            check( rs_rto_channel_set_function( rs->dev, i, ch->function ) );
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
copy_windows( RS_RTO_Win       ** volatile dst,
              RS_RTO_Win const  * volatile src )
{
    if ( ! src )
        return;

    RS_RTO_Win ** orig_dst = dst;
    CLOBBER_PROTECT( orig_dst );

    TRY
    {
        RS_RTO_Win * cur;

        while ( src )
        {
            cur = T_malloc( sizeof *cur );

            if ( dst == orig_dst )
                *dst = cur;
            else
                ( *dst )->next = cur;

            *cur = *src;
            cur->next = NULL;

            dst = &cur->next;
            src = src->next;
        }

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        delete_windows( orig_dst );
        RETHROW;
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
delete_windows( RS_RTO_Win ** wp )
{
    if ( ! *wp )
        return;

    RS_RTO_Win * cur = *wp;

    while ( cur )
    {
        RS_RTO_Win * nxt = cur->next;
        T_free( cur );
        cur = nxt;
    }

    *wp = NULL;
}


/*                                                                              
 * Local variables:                                                             
 * tab-width: 4                                                                 
 * indent-tabs-mode: nil                                                        
 * End:                                                                         
 */
