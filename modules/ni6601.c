/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2_module.h"
#include <ni6601.h>


/* Include configuration information for the device */

#include "ni6601.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;



/* Exported functions */

int ni6601_init_hook(       void );
int ni6601_exp_hook(        void );
int ni6601_end_of_exp_hook( void );


Var_T * counter_name(                     Var_T * v );
Var_T * counter_start_continuous_counter( Var_T * v );
Var_T * counter_start_timed_counter(      Var_T * v );
Var_T * counter_start_buffered_counter(   Var_T * v );
Var_T * counter_get_buffered_counts(      Var_T * v );
Var_T * counter_intermediate_count(       Var_T * v );
Var_T * counter_timed_count(              Var_T * v );
Var_T * counter_final_count(              Var_T * v );
Var_T * counter_stop_counter(             Var_T * v );
Var_T * counter_single_pulse(             Var_T * c );
Var_T * counter_continuous_pulses(        Var_T * v );
Var_T * counter_dio_read(                 Var_T * v );
Var_T * counter_dio_write(                Var_T * v );


static Var_T * ni6601_get_data( long   to_fetch,
                                double wait_secs );

static int ni6601_counter_number( long ch );

static int ni6601_source_number( long ch );

static const char * ni6601_ptime( double p_time );

static double ni6601_time_check( double       duration,
                                 const char * text );


#define COUNTER_IS_BUSY 1

static int states[ 4 ];
static double jiffy_len;
static int buffered_counter = NI6601_COUNTER_0 - 1;
static int buffered_continuous = 1;
static long buffered_remaining = -1;


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int
ni6601_init_hook( void )
{
    int i;


    for ( i = 0; i < 4; i++ )
        states[ i ] = 0;

    /* Get the number of clock ticks per second */

    jiffy_len = 1.0 / ( double ) sysconf( _SC_CLK_TCK );

    return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int
ni6601_exp_hook( void )
{
    int s;
    int ret;


    buffered_counter = NI6601_COUNTER_0 - 1;

    /* Ask for state of the one of the counters of the board to find out
       if we can access the board */

    raise_permissions( );
    ret = ni6601_is_counter_armed( BOARD_NUMBER, NI6601_COUNTER_0, &s );
    lower_permissions( );

    switch ( ret )
    {
        case NI6601_OK :
            break;

        case NI6601_ERR_NSB :
            print( FATAL, "Invalid board number.\n" );
            THROW( EXCEPTION );

        case NI6601_ERR_NDV :
            print( FATAL, "Driver for board not loaded.\n" );
            THROW( EXCEPTION );

        case NI6601_ERR_ACS :
            print( FATAL, "No permissions to open device file for board.\n" );
            THROW( EXCEPTION );

        case NI6601_ERR_DFM :
            print( FATAL, "Device file for board missing.\n" );
            THROW( EXCEPTION );

        case NI6601_ERR_DFP :
            print( FATAL, "Unspecified error when opening device file for "
                   "board.\n" );
            THROW( EXCEPTION );

        case NI6601_ERR_BBS :
            print( FATAL, "Board already in use by another program.\n" );
            THROW( EXCEPTION );

        case NI6601_ERR_INT :
            print( FATAL, "Internal error in board driver or library.\n" );
            THROW( EXCEPTION );

        default :
            print( FATAL, "Unrecognized error when trying to access the "
                   "board.\n" );
            THROW( EXCEPTION );
    }

    return 1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int
ni6601_end_of_exp_hook( void )
{
    raise_permissions( );
    ni6601_close( BOARD_NUMBER );
    lower_permissions( );

    buffered_counter = NI6601_COUNTER_0 - 1;
    return 1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
counter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
counter_start_continuous_counter( Var_T * v )
{
    int counter;
    int source;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

    if ( ( v = vars_pop( v ) ) == NULL )
        source = NI6601_DEFAULT_SOURCE;
    else
    {
        source = ni6601_source_number( get_strict_long( v,
                                                        "source channel" ) );
        too_many_arguments( v );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_start_counter( BOARD_NUMBER, counter, source );
        lower_permissions( );

        switch( ret )
        {
            case NI6601_OK :
                break;

            case NI6601_ERR_CBS :
                print( FATAL, "Counter CH%d is already running.\n", counter );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't start counter.\n" );
                THROW( EXCEPTION );
        }
    }
    else if ( FSC2_MODE == TEST )
    {
        if ( states[ counter ] == COUNTER_IS_BUSY )
        {
            print( FATAL, "Counter CH%d is already running.\n", counter );
            THROW( EXCEPTION );
        }
        states[ counter ] = COUNTER_IS_BUSY;
    }

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
counter_start_timed_counter( Var_T * v )
{
    int counter;
    int source;
    double interval;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing time interval for counting.\n" );
        THROW( EXCEPTION );
    }

    interval = ni6601_time_check( get_double( v, "counting time interval" ),
                                  "counting time interval" );

    if ( ( v = vars_pop( v ) ) == NULL )
        source = NI6601_DEFAULT_SOURCE;
    else
    {
        source = ni6601_source_number( get_strict_long( v,
                                                        "source channel" ) );
        too_many_arguments( v );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_start_gated_counter( BOARD_NUMBER, counter, interval,
                                          source );
        lower_permissions( );

        switch ( ret )
        {
            case NI6601_OK :
                break;

            case NI6601_ERR_CBS :
                print( FATAL, "Counter CH%d is already running.\n", counter );
                THROW( EXCEPTION );

            case NI6601_ERR_NCB :
                print( FATAL, "Required neighbouring counter CH%d is already "
                       "running.\n", counter & 1 ? counter - 1 : counter + 1 );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't start counter.\n" );
                THROW( EXCEPTION );
        }
    }
    else if (    FSC2_MODE == TEST
              && states[ counter ] == COUNTER_IS_BUSY )
    {
        print( FATAL, "Counter CH%d is already running.\n", counter );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------------------*
 * This function is a combination of counter_start_timed_counter()
 * and counter_final_count() - it starts a timed count and waits
 * for the count interval finish and then fetches the count.
 *-----------------------------------------------------------------*/

Var_T *
counter_timed_count( Var_T * v )
{
    int counter;
    int source;
    double interval;
    unsigned long count;
    int state;
    static long dummy_count = 0;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing time interval for counting.\n" );
        THROW( EXCEPTION );
    }

    interval = ni6601_time_check( get_double( v, "counting time interval" ),
                                  "counting time interval" );

    if ( ( v = vars_pop( v ) ) == NULL )
        source = NI6601_DEFAULT_SOURCE;
    else
    {
        source = ni6601_source_number( get_strict_long( v,
                                                        "source channel" ) );
        too_many_arguments( v );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_start_gated_counter( BOARD_NUMBER, counter, interval,
                                          source );
        lower_permissions( );

        switch ( ret )
        {
            case NI6601_OK :
                break;

            case NI6601_ERR_CBS :
                print( FATAL, "Counter CH%d is already running.\n", counter );
                THROW( EXCEPTION );

            case NI6601_ERR_NCB :
                print( FATAL, "Required neighbouring counter CH%d is already "
                       "running.\n", counter & 1 ? counter - 1 : counter + 1 );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't start counter.\n" );
                THROW( EXCEPTION );
        }
    }
    else if (    FSC2_MODE == TEST
              && states[ counter ] == COUNTER_IS_BUSY )
    {
        print( FATAL, "Counter CH%d is already running.\n", counter );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        /* For longer intervals (i.e. longer than the clock rate of the
           machine) sleep instead of waiting (either sleeping or even
           polling) in the kernel for the count interval to finish (as we
           would do when calling ni6601_get_count() immediately with the
           third argument being set to 1). If the user presses the "Stop"
           button while we were sleeping stop the counter and return the
           current count without waiting any longer. */

        if ( ( interval -= jiffy_len ) > 0.0 )
        {
            fsc2_usleep( ( unsigned long ) ( interval * 1.0e6 ), SET );
            if ( check_user_request( ) )
            {
                raise_permissions( );
                counter_stop_counter( vars_push( INT_VAR, counter ) );
                lower_permissions( );
            }
        }

    try_counter_again:

        /* Now ask the board for the value - make the kernel poll for the
           result because, once we get here, the result should be available
           within less than the clock rate. This helps not slowing down the
           program too much when really short counting intervals are used. */

        raise_permissions( );
        ret = ni6601_get_count( BOARD_NUMBER, counter, 1, 1, &count, &state );
        lower_permissions( );

        switch ( ret )
        {
            case NI6601_OK :
                if ( count > LONG_MAX )
                {
                    print( SEVERE, "Counter value too large.\n" );
                    count = LONG_MAX;
                }
                break;

            case NI6601_ERR_WFC :
                print( FATAL, "Can't get final count, counter CH%d is "
                       "running in continuous mode.\n", counter );
                THROW( EXCEPTION );

            case NI6601_ERR_ITR :
                goto try_counter_again;

            default :
                print( FATAL, "Can't get counter value.\n" );
                THROW( EXCEPTION );
        }
    }
    else
        count = ++dummy_count;

    return vars_push( INT_VAR, ( long ) count );
}


/*------------------------------------------------------*
 * Gets a count even while the counter is still running
 *------------------------------------------------------*/

Var_T *
counter_intermediate_count( Var_T * v )
{
    int counter;
    unsigned long count;
    int state;
    static long dummy_count = 0;
    int ret;


    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_get_count( BOARD_NUMBER, counter, 0, 0, &count, &state );
        lower_permissions( );

        if ( ret < 0 )
        {
            print( FATAL, "Can't get counter value.\n" );
            THROW( EXCEPTION );
        }

        if ( count > LONG_MAX )
        {
            print( SEVERE, "Counter value too large.\n" );
            count = LONG_MAX;
        }
    }
    else
        count = ++dummy_count;

    return vars_push( INT_VAR, count );
}


/*------------------------------------------------------------------*
 * Get a count after waiting until the counter is finished counting
 *------------------------------------------------------------------*/

Var_T *
counter_final_count( Var_T * v )
{
    int counter;
    unsigned long count;
    int state;
    static long dummy_count = 0;
    int ret;


    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

    if ( FSC2_MODE == EXPERIMENT )
    {

    try_counter_again:

        raise_permissions( );
        ret = ni6601_get_count( BOARD_NUMBER, counter, 1, 0, &count, &state );
        lower_permissions( );

        switch ( ret )
        {
            case NI6601_OK :
                if ( count > LONG_MAX )
                {
                    print( SEVERE, "Counter value too large.\n" );
                    count = LONG_MAX;
                }
                break;

            case NI6601_ERR_WFC :
                print( FATAL, "Can't get final count, counter CH%d is "
                       "running in continuous mode.\n", counter );
                THROW( EXCEPTION );

            case NI6601_ERR_ITR :
                goto try_counter_again;

            default :
                print( FATAL, "Can't get counter value.\n" );
                THROW( EXCEPTION );
        }
    }
    else
        count = ++dummy_count;

    return vars_push( INT_VAR, ( long ) count );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
counter_start_buffered_counter( Var_T * v )
{
    int counter;
    double interval;
    int source = NI6601_DEFAULT_SOURCE;
    int continuous = 1;
    long num_points = 0;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    if (    buffered_counter >= NI6601_COUNTER_0
         && buffered_counter <= NI6601_COUNTER_3 )
    {
        print( FATAL, "Counter %d is already doing buffered counting, only "
               "one buffered counter is possible at once.\n",
               buffered_counter );
        THROW( EXCEPTION );
    }

    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing time interval for counting.\n" );
        THROW( EXCEPTION );
    }

    interval = ni6601_time_check( get_double( v, "counting time interval" ),
                                  "counting time interval" );

    buffered_continuous = 1;
    buffered_remaining = -1;

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        if ( v->type != STR_VAR )
        {
            source = ni6601_source_number( get_strict_long( v,
                                                          "source channel" ) );
            v = vars_pop( v );
        } else
            source = NI6601_DEFAULT_SOURCE;

        if ( ! strcasecmp( v->val.sptr, "NON-CONTINUOUS" ) )
        {
            continuous = 0;
            buffered_continuous = 0;
        }
        else if ( strcasecmp( v->val.sptr, "CONTINUOUS" ) ) {
            print( FATAL, "Invalid acquisition mode '%s'\n", v->val.sptr );
            THROW( EXCEPTION );
        }

        if ( ( v = vars_pop( v ) ) != NULL )
        {
            num_points = get_long( v, continuous ? "buffer size" : "number of "
                                   "points to acquire" );
            if ( num_points < 0 || ( ! continuous && num_points <= 0 ) )
            {
                print( FATAL, continuous ? "Invalid buffer size.\n" :
                       "Invalid number of points to acquire.\n" );
            }

            buffered_remaining = num_points;
        }

        too_many_arguments( v );
    }

    if ( buffered_remaining < 0 )
        buffered_remaining = ceil( 1.0 / interval );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_start_buffered_counter( BOARD_NUMBER, counter,
                                             interval, source,
                                             num_points, continuous );
        lower_permissions( );

        switch ( ret )
        {
            case NI6601_OK :
                break;

            case NI6601_ERR_CBS :
                print( FATAL, "Counter CH%d is already running.\n", counter );
                THROW( EXCEPTION );

            case NI6601_ERR_NCB :
                print( FATAL, "Required neighbouring counter CH%d is already "
                       "running.\n", counter & 1 ? counter - 1 : counter + 1 );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't start buffered counter.\n" );
                THROW( EXCEPTION );
        }
    }
    else if (    FSC2_MODE == TEST
              && states[ counter ] == COUNTER_IS_BUSY )
    {
        print( FATAL, "Counter CH%d is already running.\n", counter );
        THROW( EXCEPTION );
    }

    buffered_counter = counter;

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
counter_get_buffered_counts( Var_T * v )
{
    int counter;
    long num_points;
    double wait_secs = -1.0;


    CLOBBER_PROTECT( num_points );

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    if (    buffered_counter < NI6601_COUNTER_0
         || buffered_counter > NI6601_COUNTER_3 )
    {
        print( FATAL, "There is no buffered counter running.\n" );
        THROW( EXCEPTION );
    }

    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

    if ( counter != buffered_counter )
    {
        print( FATAL, "Counter %d isn't a buffered counter.\n" );
        THROW( EXCEPTION );
    }

    if ( ! buffered_continuous && buffered_remaining == 0 )
    {
        print( WARN, "All available data have already been fetched.\n" );
        return vars_push( INT_ARR, NULL, 0 );
    }

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing number of points to fetch.\n" );
        THROW( EXCEPTION );
    }

    num_points = get_long( v, "number of points" );

    if ( num_points < 0 )
    {
        print( FATAL, "Invalid negative number of points.\n" );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    /* A zero number of points means to fetch all remaining data for a
       non-continuous acquisition and to get as many as become available
       before the timeout expires for a continuous acquisition (in that
       case the timeout must be given) */

    if ( num_points == 0 && buffered_continuous && v == NULL )
    {
        print( FATAL, "Missing timeout argument with unspecified (zero) "
               "number of points.\n" );
        THROW( EXCEPTION );
    }

    if ( v != NULL )
    {
        wait_secs = get_double( v, "timeout" );

        if ( wait_secs < 0.0 )
        {
            print( FATAL, "Invalid negative timeout.\n" );
            THROW( EXCEPTION );
        }

        if ( floor( wait_secs * 1.0e6 + 0.5 ) > LONG_MAX )
        {
            print( FATAL, "Timeout of %f s is too large, maximum is %f s.\n",
                   wait_secs, 1.0e-6 * LONG_MAX );
            THROW( EXCEPTION );
        }

        too_many_arguments( v );
    }

    /* Now we've got to sort through the possible combinations of arguments */

    if ( ! buffered_continuous )          /* non-continuous mode */
    {
        if ( num_points == 0 )            /* unspecified number of points */
            num_points = buffered_remaining;
        else if ( num_points > buffered_remaining )
            num_points = buffered_remaining;
    }

    if ( wait_secs < 0.0 )            /* unspecified timeout */
        wait_secs = 0.0;
    else if ( wait_secs == 0.0 )
        wait_secs = -1.0;

    if ( FSC2_MODE == TEST )
    {
        long *buf;
        long i;
        Var_T *nv;


        if ( num_points == 0 )
            num_points = buffered_remaining;

        buf = T_malloc( num_points * sizeof *buf );
        for ( i = 0; i < num_points; i++ )
            buf[ i ] = lrnd( INT_MAX * ( rand( ) / ( RAND_MAX + 1.0 ) ) );

        if ( ! buffered_continuous )
            buffered_remaining = 0;

        TRY
        {
            nv = vars_push( INT_ARR, buf, num_points );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( buf );
            RETHROW( );
        }

        T_free( buf );
        return nv;
    }

    return ni6601_get_data( num_points, wait_secs );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static Var_T *
ni6601_get_data( long   to_fetch,
                 double wait_secs )
{
    ssize_t ret = 0;
    int quit_on_signal;
    int eod;
    int timed_out;
    long received = 0;
    unsigned long *buf;
    long *final_buf;
    ssize_t i;
    long us_wait = 0;
    struct timeval before,
                   after;
    Var_T *nv;


    CLOBBER_PROTECT( received );
    CLOBBER_PROTECT( us_wait );
    CLOBBER_PROTECT( to_fetch );
    CLOBBER_PROTECT( wait_secs );

    if ( wait_secs > 0.0 )
        us_wait = lrnd( wait_secs * 1.0e6 );

    if ( to_fetch == 0 )
    {
        raise_permissions( );
        to_fetch = ni6601_get_buffered_available( BOARD_NUMBER );
        lower_permissions( );

        if ( to_fetch < 0 )
        {
            print( FATAL, "Internal error in board driver or library.\n" );
            THROW( EXCEPTION );
        }

        if ( to_fetch == 0 && wait_secs < 0.0 )
            return vars_push( INT_ARR, NULL, 0 );
    }

    buf = T_malloc( to_fetch * sizeof *buf );

    while ( 1 )
    {
        if ( wait_secs > 0.0 )
        {
            if ( us_wait <= 0 )
                break;

            wait_secs = 1.0e-6 * us_wait;
            gettimeofday( &before, NULL );
        }

        quit_on_signal = 1;

        raise_permissions( );
        ret = ni6601_get_buffered_counts( BOARD_NUMBER, buf + received,
                                          to_fetch, wait_secs, &quit_on_signal,
                                          &timed_out, &eod );
        lower_permissions( );

        if ( ret < 0 )
        {
            T_free( buf );

            switch ( ret )
            {
                case NI6601_ERR_OFL :
                    print( FATAL, "Internal buffer overflow.\n" );
                    break;

                case NI6601_ERR_TFS :
                    print( FATAL, "Data acquisition too fast.\n" );
                    break;

                case NI6601_ERR_INT :
                    print( FATAL, "Internal error in board driver or "
                           "library.\n" );
                    break;

                default :
                    fsc2_impossible( );
            }

            THROW( EXCEPTION );
        }
        else if ( ret == 0 )
        {
            if ( eod || timed_out )
                break;

            if ( quit_on_signal )
            {
                TRY
                {
                    stop_on_user_request( );
                    if ( wait_secs > 0.0 )
                    {
                            gettimeofday( &after, NULL );
                            us_wait -=   (   after.tv_sec * 1000000
                                           + after.tv_usec  )
                                       - (   before.tv_sec * 1000000
                                           + before.tv_usec );
                    }
                    TRY_SUCCESS;
                }
                OTHERWISE
                {
                    T_free( buf );
                    RETHROW( );
                }
            }
        }
        else
        {
            received += ret;
            to_fetch -= ret;

            if ( eod || timed_out || to_fetch == 0 )
                break;
        }
    }

    if ( received == 0 && wait_secs == 0.0 )
    {
        T_free( buf );
        return vars_push( INT_ARR, NULL, 0 );
    }

    TRY
    {
        final_buf = T_malloc( received * sizeof *final_buf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( buf );
        RETHROW( );
    }

    for ( i = 0; i < received; i++ )
    {
        if ( buf[ i ] > LONG_MAX )
        {
            print( SEVERE, "Counter value too large.\n" );
            final_buf[ i ] = LONG_MAX;
        }
        else
            final_buf[ i ] = buf[ i ];
    }

    T_free( buf );

    TRY
    {
        nv = vars_push( INT_ARR, final_buf, received );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( final_buf );
        RETHROW( );
    }

    T_free( final_buf );

    return nv;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
counter_stop_counter( Var_T * v )
{
    int counter;
    int ret;


    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_stop_counter( BOARD_NUMBER, counter );
        lower_permissions( );

        if ( ret < 0 )
            print( SEVERE, "Failed to stop counter CH%d.\n", counter );
    }
    else if ( FSC2_MODE == TEST )
        states[ counter ] = 0;

    if ( counter == buffered_counter )
        buffered_counter = NI6601_COUNTER_0 - 1;

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
counter_single_pulse( Var_T * v )
{
    int counter;
    double duration;
    int ret;


    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );
    duration = ni6601_time_check( get_double( v->next, "pulse length" ),
                                  "pulse length" );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_generate_single_pulse( BOARD_NUMBER, counter, duration );
        lower_permissions( );

        switch ( ret )
        {
            case NI6601_OK :
                break;

            case NI6601_ERR_CBS :
                print( FATAL, "Counter CH%d requested for pulse is already "
                       "running.\n", counter );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't create the pulse.\n" );
                THROW( EXCEPTION );
        }
    }
    else if (    FSC2_MODE == TEST
              && states[ counter ] == COUNTER_IS_BUSY )
    {
        print( FATAL, "Counter CH%d requested for pulse is already "
               "running.\n", counter );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
counter_continuous_pulses( Var_T * v )
{
    int counter;
    double len_hi, len_low;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing pulse length.\n" );
        THROW( EXCEPTION );
    }

    len_hi = ni6601_time_check( get_double( v,
                                            v->next != NULL ?
                                            "length of high phase of pulse" :
                                            "pulse length" ),
                                v->next != NULL ?
                                "length of high phase of pulse" :
                                "pulse length" );

    if ( ( v = vars_pop( v ) ) != NULL )
        len_low = ni6601_time_check( get_double( v,
                                              "length of low phase of pulse" ),
                                     "length of low phase of pulse" );
    else
        len_low = len_hi;

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_generate_continuous_pulses( BOARD_NUMBER, counter,
                                                 len_hi, len_low );
        lower_permissions( );

        switch ( ret )
        {
            case NI6601_OK :
                break;

            case NI6601_ERR_CBS :
                print( FATAL, "Counter CH%d requested for continuous pulses "
                       "is already running.\n", counter );
                THROW( EXCEPTION );

            default :
                print( FATAL, "Can't create continuous pulses.\n" );
                THROW( EXCEPTION );
        }
    }
    else if (    FSC2_MODE == TEST
              && states[ counter ] == COUNTER_IS_BUSY )
    {
        print( FATAL, "Counter CH%d requested for continuous pulses is "
               "already running.\n", counter );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

Var_T *
counter_dio_read( Var_T * v )
{
    long mask;
    unsigned char bits = 0;
    int ret;


    if ( v == NULL )
        mask = 0xFF;
    else
    {
        mask = get_strict_long( v, "DIO bit mask" );
        if ( mask < 0 || mask > 0xFF )
        {
            print( FATAL, "Invalid mask of 0x%X, valid range is [0-255].\n",
                   mask );
            THROW( EXCEPTION );
        }
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_dio_read( BOARD_NUMBER, &bits,
                               ( unsigned char ) ( mask & 0xFF ) );
        lower_permissions( );

        if ( ret < 0 )
        {
            print( FATAL, "Can't read data from DIO.\n" );
            THROW( EXCEPTION );
        }
    }

    return vars_push( INT_VAR, ( long ) bits );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

Var_T *
counter_dio_write( Var_T * v )
{
    long bits;
    long mask;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    bits = get_strict_long( v, "DIO value" );

    if ( bits < 0 || bits > 0xFF )
    {
        print( FATAL, "Invalid value of %ld, valid range is [0-255].\n",
               bits );
        THROW( EXCEPTION );
    }

    if ( ( v = vars_pop( v ) ) == NULL )
        mask = 0xFF;
    else
    {
        mask = get_strict_long( v, "DIO bit mask" );
        if ( mask < 0 || mask > 0xFF )
        {
            print( FATAL, "Invalid mask of 0x%X, valid range is [0-255].\n",
                   mask );
            THROW( EXCEPTION );
        }

        too_many_arguments( v );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni6601_dio_write( BOARD_NUMBER,
                                ( unsigned char ) ( bits & 0xFF ),
                                ( unsigned char ) ( mask & 0xFF ) );
        lower_permissions( );

        if ( ret < 0 )
        {
            print( FATAL, "Can't write value to DIO.\n" );
            THROW( EXCEPTION );
        }
    }

    return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 * Converts a channel number as we get it passed from the parser
 * into a real counter number.
 *---------------------------------------------------------------*/

static int
ni6601_counter_number( long ch )
{
    switch ( ch )
    {
        case CHANNEL_CH0 :
            return NI6601_COUNTER_0;

        case CHANNEL_CH1 :
            return NI6601_COUNTER_1;

        case CHANNEL_CH2 :
            return NI6601_COUNTER_2;

        case CHANNEL_CH3 :
            return NI6601_COUNTER_3;
    }

    if ( ch > CHANNEL_INVALID && ch < NUM_CHANNEL_NAMES )
    {
        print( FATAL, "There's no counter channel named %s.\n",
               Channel_Names[ ch ] );
        THROW( EXCEPTION );
    }

    print( FATAL, "Invalid counter channel number %ld.\n", ch );
    THROW( EXCEPTION );

    return -1;
}


/*---------------------------------------------------------------*
 * Converts a channel number as we get it passed from the parser
 * into a real source channel number.
 *---------------------------------------------------------------*/

static int
ni6601_source_number( long ch )
{
    switch ( ch )
    {
        case CHANNEL_DEFAULT_SOURCE :
            return NI6601_DEFAULT_SOURCE;

        case CHANNEL_SOURCE_0 :
            return NI6601_SOURCE_0;

        case CHANNEL_SOURCE_1 :
            return NI6601_SOURCE_1;

        case CHANNEL_SOURCE_2 :
            return NI6601_SOURCE_2;

        case CHANNEL_SOURCE_3 :
            return NI6601_SOURCE_3;

        case CHANNEL_NEXT_GATE :
            return NI6601_NEXT_GATE;

        case CHANNEL_TIMEBASE_1 :
            return NI6601_TIMEBASE_1;

        case CHANNEL_TIMEBASE_2 :
            return NI6601_TIMEBASE_2;
    }

    if ( ch > CHANNEL_INVALID && ch < NUM_CHANNEL_NAMES )
    {
        print( FATAL, "There's no source channel named %s.\n",
               Channel_Names[ ch ] );
        THROW( EXCEPTION );
    }

    print( FATAL, "Invalid source channel number %ld.\n", ch );
    THROW( EXCEPTION );

    return -1;
}

/*----------------------------------------------------*
 *----------------------------------------------------*/

static const char *
ni6601_ptime( double p_time )
{
    static char buffer[ 128 ];

    if ( fabs( p_time ) >= 1.0 )
        sprintf( buffer, "%g s", p_time );
    else if ( fabs( p_time ) >= 1.e-3 )
        sprintf( buffer, "%g ms", 1.e3 * p_time );
    else if ( fabs( p_time ) >= 1.e-6 )
        sprintf( buffer, "%g us", 1.e6 * p_time );
    else
        sprintf( buffer, "%g ns", 1.e9 * p_time );

    return buffer;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static double
ni6601_time_check( double       duration,
                   const char * text )
{
    unsigned long ticks;


    if (    1.01 * duration < 2 * NI6601_TIME_RESOLUTION
         || duration / NI6601_TIME_RESOLUTION - 1 >= ULONG_MAX + 0.5 )
    {
        static char *mint = NULL;
        static char *maxt = NULL;

        maxt = NULL;
        TRY
        {
            mint = T_strdup( ni6601_ptime( 2 * NI6601_TIME_RESOLUTION ) );
            maxt = T_strdup( ni6601_ptime( ( ULONG_MAX + 1.0 )
                                           * NI6601_TIME_RESOLUTION ) );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            if ( mint )
                mint = T_free( mint );
            if ( maxt )
                maxt = T_free( maxt );
            RETHROW( );
        }

        print( FATAL, "Invalid %s of %s, valid values are in the range "
               "between %s and %s.\n", text, ni6601_ptime( duration ),
               mint, maxt );
        mint = T_free( mint );
        maxt = T_free( maxt );
        THROW( EXCEPTION );
    }

    ticks = ( unsigned long ) floor( duration / NI6601_TIME_RESOLUTION + 0.5 );
    if ( fabs( ticks * NI6601_TIME_RESOLUTION - duration ) >
                                                            1.0e-2 * duration )
    {
        static  char *oldv = NULL;

        TRY
        {
            oldv = T_strdup( ni6601_ptime( duration ) );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            if ( oldv )
                oldv = T_free( oldv );
            RETHROW( );
        }

        print( WARN, "Adjusting %s from %s to %s.\n", text, oldv,
               ni6601_ptime( ticks * NI6601_TIME_RESOLUTION ) );
        oldv = T_free( oldv );
    }

    return ticks * NI6601_TIME_RESOLUTION;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
