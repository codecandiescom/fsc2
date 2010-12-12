/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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


#include "lecroy_wr_l.h"


/* Structure for storing the relevant state information if a check of
   the trigger delay can't be done during the test run */

typedef struct LECROY_WR_PTC_DELAY LECROY_WR_PTC_DELAY_T;

struct LECROY_WR_PTC_DELAY {
    double delay;
    LECROY_WR_PTC_DELAY_T * next;
};

static LECROY_WR_PTC_DELAY_T * lecroy_wr_ptc_delay = NULL;


/* Structure for storing the relevant state information if a window check
   can't be done during the test run */

typedef struct LECROY_WR_PTC_WINDOW LECROY_WR_PTC_WINDOW_T;

struct LECROY_WR_PTC_WINDOW {
    bool is_timebase;               /* is timebase already set ? */
    double timebase;                /* timebase (if set) */
    int tb_index;                   /* index in timebase table */
    HORI_RES_T *cur_hres;           /* time resolution settings */
    bool is_trigger_delay;          /* is trigger delay already set ? */
    double trigger_delay;           /* trigger delay (if set) */
    long num;                       /* window number */
    double start;                   /* window start position */
    double width;                   /* window length */
    bool show_num;                  /* add window number in error message ? */
    LECROY_WR_PTC_WINDOW_T * next;  /* next structure in list */
};

static LECROY_WR_PTC_WINDOW_T * lecroy_wr_ptc_window = NULL;


static void lecroy_wr_soe_trigger_delay_check( double delay );
static void lecroy_wr_soe_window_check( LECROY_WR_PTC_WINDOW_T * p );
static long lecroy_wr_calc_pos( double t );



/*--------------------------------------------------------------------------*
 * Some checks can't be done during the test stage - they require knowledge
 * of e.g. the timebase etc. which may still be unknown at that time (at
 * least if the user didn't set it explecitely). So the checks have to be
 * postponed until the start of the exeriment when we can talk with the
 * device and thus figure them out (and they have to be repeated on each
 * start of an experiment since the settings could have been changed during
 * the previous experiment or afterwards). This function is for doing these
 * tests.
 *--------------------------------------------------------------------------*/

void
lecroy_wr_soe_checks( void )
{
    LECROY_WR_PTC_DELAY_T *pd;
    LECROY_WR_PTC_WINDOW_T *pw;


    /* Test the trigger delay and window settings we couldn't check during
       the test run */

    for ( pd = lecroy_wr_ptc_delay; pd != NULL; pd = pd->next )
        lecroy_wr_soe_trigger_delay_check( pd->delay );

    for ( pw = lecroy_wr_ptc_window; pw != NULL; pw = pw->next )
        lecroy_wr_soe_window_check( pw );
}


/*-----------------------------------------------------------------------*
 * Some memory may still be allocated for the tests of trigger delays
 * and window positions to be done at the start of the experiment. This
 * function must be called in the exit handler to get rid of the memory.
 *-----------------------------------------------------------------------*/

void
lecroy_wr_exit_cleanup( void )
{
    LECROY_WR_PTC_DELAY_T *pd;
    LECROY_WR_PTC_WINDOW_T *pw;


    while ( lecroy_wr_ptc_delay != NULL )
    {
        pd = lecroy_wr_ptc_delay;
        lecroy_wr_ptc_delay = lecroy_wr_ptc_delay->next;
        T_free( pd );
    }

    while ( lecroy_wr_ptc_window != NULL )
    {
        pw = lecroy_wr_ptc_window;
        lecroy_wr_ptc_window = lecroy_wr_ptc_window->next;
        T_free( pw );
    }
}


/*----------------------------------------------------------*
 * Returns a string with a time value with a resonable unit
 *----------------------------------------------------------*/

const char *
lecroy_wr_ptime( double p_time )
{
    static char buffer[ 2 ][ 128 ];
    static size_t i = 1;


    i = ( i + 1 ) % 2;

    if ( fabs( p_time ) >= 1.0 )
        sprintf( buffer[ i ], "%.3f s", p_time );
    else if ( fabs( p_time ) >= 1.0e-3 )
        sprintf( buffer[ i ], "%.3f ms", 1.0e3 * p_time );
    else if ( fabs( p_time ) >= 1.0e-6 )
        sprintf( buffer[ i ], "%.3f us", 1.0e6 * p_time );
    else
        sprintf( buffer[ i ], "%.3f ns", 1.0e9 * p_time );

    return buffer[ i ];
}


/*-----------------------------------------------------------*
 * Function for checking the trigger delay when the timebase
 * gets changed
 *-----------------------------------------------------------*/

double
lecroy_wr_trigger_delay_check( void )
{
    double delay = lecroy_wr.trigger_delay;
    double real_delay;


    /* Nothing needs to be done if the trigger delay never was set */

    if ( ! lecroy_wr.is_trigger_delay )
        return delay;

    /* We can't do the check during the test run if the timebase isn't set, so
       store the relevant state information for the time when the experiment
       gets started and we can ask the device for the timebase */

    if ( FSC2_MODE == TEST && ! lecroy_wr.is_timebase )
    {
        LECROY_WR_PTC_DELAY_T *p = lecroy_wr_ptc_delay;

        if ( lecroy_wr_ptc_delay == NULL )
            p = lecroy_wr_ptc_delay = T_malloc( sizeof *lecroy_wr_ptc_delay );
        else
            while ( 1 )
            {
                if ( delay == p->delay )
                    return delay;

                if ( p->next == NULL )
                {
                    p->next = T_malloc( sizeof *p );
                    p = p->next;
                    break;
                }

                p = p->next;
            }

        p->next  = NULL;
        p->delay = delay;
        return delay;
    }

    /* The delay can only be set with a certain resolution (1/10) of the
       current timebase, so make it a integer multiple of this */

    real_delay = LECROY_WR_TRIG_DELAY_RESOLUTION * lecroy_wr.timebase
                 * lrnd( delay / ( LECROY_WR_TRIG_DELAY_RESOLUTION *
                                   lecroy_wr.timebase ) );

    /* Check that the trigger delay is within the limits (taking rounding
       errors of the order of the current time resolution into account) */

    if (    real_delay > 0.0
         && real_delay >   10.0 * lecroy_wr.timebase
                         +  0.5 * lecroy_wr_time_per_point( ) )
    {
        print( FATAL, "Pre-trigger delay of %s now is too long, can't be "
               "longer than 10 times the timebase.\n",
               lecroy_wr_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    if (    real_delay < 0.0
         && real_delay <   -1.0e4 * lecroy_wr.timebase
                         -  0.5 * lecroy_wr_time_per_point( ) )
    {
        print( FATAL, "Post-triger delay of %s now is too long, can't be "
               "longer than 10,000 times the timebase.\n",
               lecroy_wr_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    /* If the difference between the requested trigger delay and the one
       that can be set is larger than the time resolution warn the user */

    if ( fabs( real_delay - delay ) > lecroy_wr_time_per_point( ) )
        print( WARN, "Trigger delay had to be adjusted from %s to %s.\n",
               lecroy_wr_ptime( delay ), lecroy_wr_ptime( real_delay ) );

    return real_delay;
}


/*-----------------------------------------------------------*
 * Function for testing trigger delay settings that couldn't
 * be checked during the test run.
 *-----------------------------------------------------------*/

static void
lecroy_wr_soe_trigger_delay_check( double delay )
{
    double real_delay = LECROY_WR_TRIG_DELAY_RESOLUTION * lecroy_wr.timebase
                        * lrnd( delay / ( LECROY_WR_TRIG_DELAY_RESOLUTION *
                                          lecroy_wr.timebase ) );

    /* Check that the trigger delay is within the limits (taking rounding
       errors of the order of the current time resolution into account) */

    if (    real_delay > 0.0
         && real_delay >   10.0 * lecroy_wr.timebase
                         +  0.5 * lecroy_wr_time_per_point( ) )
    {
        print( FATAL, "During the experiment a pre-trigger delay of %s is "
               "going to be used which is too long, it can't be longer than "
               "10 times the timebase.\n", lecroy_wr_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    if (    real_delay < 0.0
         && real_delay <   -1.0e4 * lecroy_wr.timebase
                         -  0.5 * lecroy_wr_time_per_point( ) )
    {
        print( FATAL, "During the experiment the post-triger delay of %s is "
               "going to be used wich is too long, it can't be longer than "
               "10,000 times the timebase.\n",
               lecroy_wr_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    /* If the difference between the requested trigger delay and the one
       that can be set is larger than the time resolution warn the user */

    if ( fabs( real_delay - delay ) > lecroy_wr_time_per_point( ) )
        print( WARN, "During the experiment the trigger delay will have to be "
               "adjusted from %s to %s.\n",
               lecroy_wr_ptime( delay ), lecroy_wr_ptime( real_delay ) );
}


/*---------------------*
 * Deletes all windows
 *---------------------*/

void
lecroy_wr_delete_windows( LECROY_WR_T * s )
{
    Window_T *w;


    while ( s->w != NULL )
    {
        w = s->w;
        s->w = w->next;
        T_free( w );
    }
}


/*-----------------------------------------------*
 * Returns a pointer to the window given it's ID
 *-----------------------------------------------*/

Window_T *
lecroy_wr_get_window_by_number( long wid )
{
    Window_T *w;


    if ( wid >= WINDOW_START_NUMBER )
        for ( w = lecroy_wr.w; w != NULL; w = w->next )
            if ( w->num == wid )
                return w;

    print( FATAL, "Argument isn't a valid window number.\n" );
    THROW( EXCEPTION );

    return NULL;
}


/*-----------------------------------------------------------*
 * Function for checking that all windows still fit into the
 * recorded data set after changing the time resolution or
 * switching interleaved mode on or off
 *-----------------------------------------------------------*/

void
lecroy_wr_all_windows_check( void )
{
    Window_T *w = lecroy_wr.w;


    while ( w != NULL )
    {
        lecroy_wr_window_check( w, SET );
        w = w->next;
    }
}


/*------------------------------------------------------*
 * Checks if the window fits into the recorded data set
 *------------------------------------------------------*/

void
lecroy_wr_window_check( Window_T * w,
                        bool       show_num )
{
    long start;
    long end;
    long max_len = lecroy_wr_curve_length( );
    double max_width = max_len * lecroy_wr_time_per_point( );
    double max_time = max_width - lecroy_wr.trigger_delay;


    /* During the test run the window position and width can only be checked
       if we know the timebase and the trigger delay, so if they haven't been
       explicitely set by the user just store them together with the window
       settings for a test when the experiment has been started. */

    if (    FSC2_MODE == TEST
         && ( ! lecroy_wr.is_timebase || ! lecroy_wr.is_trigger_delay ) )
    {
        LECROY_WR_PTC_WINDOW_T *p = lecroy_wr_ptc_window;

        if ( lecroy_wr_ptc_window == NULL )
            p = lecroy_wr_ptc_window =
                					  T_malloc( sizeof *lecroy_wr_ptc_window );
        else
        {
            while ( p->next != NULL )
                p = p->next;

            p->next = T_malloc( sizeof *p );
            p = p->next;
        }

        p->next = NULL;

        /* Store the current settings as far as they are known */

        p->is_timebase = lecroy_wr.is_timebase;
        if ( lecroy_wr.is_timebase )
        {
            p->timebase = lecroy_wr.timebase;
            p->tb_index = lecroy_wr.tb_index;
            p->cur_hres = lecroy_wr.cur_hres;
        }

        p->is_trigger_delay = lecroy_wr.is_trigger_delay;
        if ( lecroy_wr.is_trigger_delay )
            p->trigger_delay = lecroy_wr.trigger_delay;

        p->num = w->num;
        p->start = w->start;
        p->width = w->width;
        p->show_num = show_num;
        return;
    }

    /* Start with calculating the start and end position of the window
       in points */

    start = lecroy_wr_calc_pos( w->start );
    end   = lecroy_wr_calc_pos( w->start + w->width );

    if ( start < 0 )
    {
        if ( show_num > 0 )
            print( FATAL, "%ld. window starts too early, earliest possible "
                   "time is %s\n", w->num - WINDOW_START_NUMBER + 1,
                   lecroy_wr_ptime( - lecroy_wr.trigger_delay ) );
        else
            print( FATAL, "Window starts too early, earliest possible time "
                   "is %s\n",
                   lecroy_wr_ptime( - lecroy_wr.trigger_delay ) );
        THROW( EXCEPTION );
    }

    if ( start > max_len )
    {
        if ( show_num > 0 )
            print( FATAL, "%ld. window starts too late, last possible time "
                   "is %s.\n", w->num - WINDOW_START_NUMBER + 1,
                   lecroy_wr_ptime( max_time ) );
        else
            print( FATAL, "Window starts too late, last possible time "
                   "is %s.\n", lecroy_wr_ptime( max_time ) );
        THROW( EXCEPTION );
    }

    if ( end > max_len )
    {
        if ( show_num > 0 )
            print( FATAL, "%d. window ends too late, largest possible width "
                   "is %s.\n", w->num - WINDOW_START_NUMBER + 1,
                   lecroy_wr_ptime( max_width ) );
        else
            print( FATAL, "Window ends too late, largest possible width "
                   "is %s.\n", lecroy_wr_ptime( max_width ) );
        THROW( EXCEPTION );
    }

    w->start_num  = start;
    w->end_num    = end;
    w->num_points = end - start + 1;
}


/*---------------------------------------------------------------*
 * Function for testing window settings that couldn't be checked
 * during the test run.
 *---------------------------------------------------------------*/

static void
lecroy_wr_soe_window_check( LECROY_WR_PTC_WINDOW_T * p )
{
    double real_timebase = lecroy_wr.timebase;
    int real_tb_index = lecroy_wr.tb_index;
    HORI_RES_T *real_cur_hres = lecroy_wr.cur_hres;
    double real_trigger_delay = lecroy_wr.trigger_delay;
    long start;
    long end;
    long max_len;
    double max_width;
    double max_time;
    bool show_num = p->show_num;


    /* Restore the internal representation of the state of the device as far
       as necessary to the one we had when the window settings couldn't be
       tested */

    if ( p->is_timebase )
    {
        lecroy_wr.timebase = p->timebase;
        lecroy_wr.tb_index = p->tb_index;
        lecroy_wr.cur_hres = p->cur_hres;
    }

    if ( p->is_trigger_delay )
        lecroy_wr.trigger_delay = p->trigger_delay;

    /* Now do the required tests */

    max_len = lecroy_wr_curve_length( );
    max_width = max_len * lecroy_wr_time_per_point( );
    max_time = max_width - lecroy_wr.trigger_delay;
    start = lecroy_wr_calc_pos( p->start );
    end   = lecroy_wr_calc_pos( p->start + p->width );

    if ( start < 0 )
    {
        if ( show_num > 0 )
            print( FATAL, "During the experiment the %ld. window is going to "
                   "start too early, earliest possible time is %s\n",
                   p->num - WINDOW_START_NUMBER + 1,
                   lecroy_wr_ptime( - lecroy_wr.trigger_delay ) );
        else
            print( FATAL, "During the experiment the window is going to start "
                   "too early, earliest possible time is %s\n",
                   lecroy_wr_ptime( - lecroy_wr.trigger_delay ) );
        THROW( EXCEPTION );
    }

    if ( start > max_len )
    {
        if ( show_num > 0 )
            print( FATAL, "During the experiment the %ld. window is going to "
                   "start too late, latest possible time is %s.\n",
                   p->num - WINDOW_START_NUMBER + 1,
                   lecroy_wr_ptime( max_time ) );
        else
            print( FATAL, "During the experiment the window is going to start "
                   "too late, latest possible time is %s.\n",
                   lecroy_wr_ptime( max_time ) );
        THROW( EXCEPTION );
    }

    if ( end > max_len )
    {
        if ( show_num > 0 )
            print( FATAL, "During the experiment the %d. window is going to "
                   "end too late, largest possible width is %s.\n",
                   p->num - WINDOW_START_NUMBER + 1,
                   lecroy_wr_ptime( max_width ) );
        else
            print( FATAL, "During the experiment the window is going to end "
                   "too late, largest possible width is %s.\n",
                   lecroy_wr_ptime( max_width ) );
        THROW( EXCEPTION );
    }

    /* Reset the internal representation back to the real one */

    if ( p->is_timebase )
    {
        lecroy_wr.timebase = real_timebase;
        lecroy_wr.tb_index = real_tb_index;
        lecroy_wr.cur_hres = real_cur_hres;
    }

    if ( p->is_trigger_delay )
        lecroy_wr.trigger_delay = real_trigger_delay;
}


/*-------------------------------------------------------------*
 * Returns a windows start or end position in points given the
 * position relative to the trigger position
 *-------------------------------------------------------------*/

static long
lecroy_wr_calc_pos( double t )
{
    return lrnd( ( t + lecroy_wr.trigger_delay ) /
                 lecroy_wr_time_per_point( ) );

}


/*-------------------------------------------------------------*
 * Function allocates and sets up the array with the possible
 * memory sizes, using the LECROY_WR_MAX_MEMORY_SIZE macro
 * defined in the configuration file. Assumes that the memory
 * sizes always follow an 1-2.5-5 scheme (except for the very
 * last one) and that the smallest memory size is 500 samples
 * (LECROY_WR_MIN_MEMORY_SIZE).
 *-------------------------------------------------------------*/

void
lecroy_wr_numpoints_prep( void )
{
    long cur_mem_size = LECROY_WR_MIN_MEMORY_SIZE;
    long len;


    for ( len = 0; cur_mem_size <= LECROY_WR_MAX_MEMORY_SIZE; len++ )
        if ( len % 3 == 1 )
            cur_mem_size = ( 5 * cur_mem_size ) / 2;
        else
            cur_mem_size *= 2;

    lecroy_wr.num_mem_sizes = len;
    lecroy_wr.mem_sizes = T_malloc( len * sizeof *lecroy_wr.mem_sizes );

    cur_mem_size = LECROY_WR_MIN_MEMORY_SIZE;

    for ( len = 0; len < lecroy_wr.num_mem_sizes; len++ )
    {
        lecroy_wr.mem_sizes[ len ] = cur_mem_size;

        if ( len % 3 == 1 )
            cur_mem_size = ( 5 * cur_mem_size ) / 2;
        else
            cur_mem_size *= 2;
    }

    lecroy_wr.mem_sizes[ len - 1 ] = LECROY_WR_MAX_MEMORY_SIZE;
}


/*-----------------------------------------------------------------*
 * Function allocates and sets up the array of possible timebases.
 *-----------------------------------------------------------------*/

void
lecroy_wr_tbas_prep( void )
{
    double cur_tbas = LECROY_WR_MAX_TIMEBASE;
    long i;
    long k = 0;


    lecroy_wr.num_tbas = LECROY_WR_NUM_TBAS;

    lecroy_wr.tbas = T_malloc( lecroy_wr.num_tbas  * sizeof *lecroy_wr.tbas );

    /* All timebase settings follow a 1-2-5 scheme with LECROY_WR_MAX_TIMEBASE
       (in s/div) being the largest possible setting */

    for ( i = lecroy_wr.num_tbas - 1; i >= 0; i--, k++ )
    {
        lecroy_wr.tbas[ i ] = cur_tbas;
        if ( k % 3 == 1 )
            cur_tbas *= 0.4;
        else
            cur_tbas *= 0.5;
    }
}


/*---------------------------------------------------------------*
 * Function to allocate and set up the table with the points per
 * division and time resolutions for the different record length
 * and timebase settings, both in single shot and random inter-
 * leaved sampling mode.
 *---------------------------------------------------------------*/

void
lecroy_wr_hori_res_prep( void )
{
    long i;
    long j;
    long k;
    double ris_res;
    double ss_res;


    lecroy_wr.hres = T_malloc(   lecroy_wr.num_mem_sizes
                               * sizeof *lecroy_wr.hres );

    TRY
    {
        *lecroy_wr.hres = T_malloc(   lecroy_wr.num_mem_sizes
                                    * LECROY_WR_NUM_TBAS
                                    * sizeof **lecroy_wr.hres );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( lecroy_wr.hres );
        RETHROW( );
    }

    for ( i = 0; i < lecroy_wr.num_mem_sizes; i++ )
    {
        if ( i != 0 )
            lecroy_wr.hres[ i ] = lecroy_wr.hres[ i - 1 ] + LECROY_WR_NUM_TBAS;

        /* Set up entries for Random Interleaved Sampling (RIS) mode */

        ris_res = LECROY_WR_RIS_SAMPLE_RATE * 1.0e9;
        for ( k = LECROY_WR_RIS_SAMPLE_RATE; k > 10; k = k / 10 )
            /* empty */ ;

        for ( j = 0; j < LECROY_WR_NUM_RIS_TBAS; j++ )
        {
            if ( lrnd( 10 * lecroy_wr.tbas[ j ] * ris_res ) >
                                                     lecroy_wr.mem_sizes[ i ] )
            {
                if ( k == 5 )
                {
                    ris_res *= 0.4;
                    k = 2;
                }
                else
                {
                    ris_res *= 0.5;
                    k = k == 2 ? 1 : 5;
                }
            }

            lecroy_wr.hres[ i ][ j ].tpp_ris = 1.0 / ris_res;
            lecroy_wr.hres[ i ][ j ].cl_ris =
                                    lrnd( 10 * lecroy_wr.tbas[ j ] * ris_res );
        }

        for ( ; j < LECROY_WR_NUM_TBAS; j++ )
        {
            lecroy_wr.hres[ i ][ j ].tpp_ris = 0.0;
            lecroy_wr.hres[ i ][ j ].cl_ris = 0;
        }

        /* Set up entries for Single Shot (SS) mode */

        ss_res = LECROY_WR_SS_SAMPLE_RATE * 1.0e6;
        for ( k = LECROY_WR_SS_SAMPLE_RATE; k > 10; k = k / 10 )
            /* empty */ ;

        for ( j = 0; j < LECROY_WR_NUM_TBAS - LECROY_WR_NUM_SS_TBAS; j++ )
        {
            lecroy_wr.hres[ i ][ j ].tpp = 0.0;
            lecroy_wr.hres[ i ][ j ].cl = 0;
        }

        for ( ; j < LECROY_WR_NUM_TBAS; j++ )
        {
            while ( lrnd( 10 * lecroy_wr.tbas[ j ] * ss_res ) >
                                                     lecroy_wr.mem_sizes[ i ] )
            {
                /* Note: Waverunners with a maximum sample rate of 2 GHz
                   (0.5 ns) switch from there to 1 Gz (1 ns) while XStreams,
                   with a maximum rate of 5 GHz (0.2 Hz) switch to 2.5 GHz
                   (0.4 ns) and only then to 1 GHz (1 ns). Afterwards for
                   both use a 5-2-1 scheme is used, e.g. 500 MHz (2 ns),
                   200 MHz (5 ns), 100 MHz (10 ns) etc. */

                if ( lrnd( 1.0e-6 * ss_res ) > 1000 )
                {
                    if ( k == 5 || k == 2 )
                        ss_res *= 0.5;
                    else
                        ss_res *= 0.4;

                    k = 1;
                }
                else
                {
                    if ( k == 5 )
                    {
                        ss_res *= 0.4;
                        k = 2;
                    }
                    else
                    {
                        ss_res *= 0.5;
                        k = k == 2 ? 1 : 5;
                    }
                }
            }

            lecroy_wr.hres[ i ][ j ].tpp = 1.0 / ss_res;
            lecroy_wr.hres[ i ][ j ].cl =
                                     lrnd( 10 * lecroy_wr.tbas[ j ] * ss_res );
        }
    }
}


/*--------------------------------------------------------------*
 * Function for getting rid of the memory for the record length
 * and timebase arrays as well as the table for the points per
 * division and time resolutions. Must be called on unloading
 * the module.
 *--------------------------------------------------------------*/

void
lecroy_wr_clean_up( void )
{
    if ( lecroy_wr.hres )
    {
        if ( lecroy_wr.hres[ 0 ] != NULL )
            T_free( lecroy_wr.hres[ 0 ] );

        T_free( lecroy_wr.hres );
    }

    if ( lecroy_wr.tbas )
        T_free( lecroy_wr.tbas );

    if ( lecroy_wr.mem_sizes )
        T_free( lecroy_wr.mem_sizes );
}


/*-----------------------------------------------------------------*
 * Returns the number of samples in a curve for the current setting
 * of the timebase and interleaved mode
 *-----------------------------------------------------------------*/

long
lecroy_wr_curve_length( void )
{
    return ( lecroy_wr.cur_hres->cl_ris > 0 && lecroy_wr.interleaved ) ?
           lecroy_wr.cur_hres->cl_ris : lecroy_wr.cur_hres->cl;
}


/*-----------------------------------------------------------------*
 * Returns the time distance between to points of a curve for the
 * current setting of the timebase and interleaved mode
 *-----------------------------------------------------------------*/

double
lecroy_wr_time_per_point( void )
{
    return ( lecroy_wr.cur_hres->cl_ris > 0 && lecroy_wr.interleaved ) ?
           lecroy_wr.cur_hres->tpp_ris : lecroy_wr.cur_hres->tpp;
}


/*--------------------------------------------------------------*
 * The function is used to translate back and forth between the
 * channel numbers the way the user specifies them in the EDL
 * program and the channel numbers as specified in the header
 * file. When the channel number can't be maped correctly, the
 * way the function reacts depends on the value of the third
 * argument: If this is UNSET, an error message gets printed
 * and an exception is thrown. If it is SET -1 is returned to
 * indicate the error.
 *--------------------------------------------------------------*/

long
lecroy_wr_translate_channel( int  dir,
                             long channel,
                             bool flag )
{
    if ( dir == GENERAL_TO_LECROY_WR )
    {
        switch ( channel )
        {
            case CHANNEL_CH1 :
                return LECROY_WR_CH1;

            case CHANNEL_CH2 :
                return LECROY_WR_CH2;
#if LECROY_WR_CH_MAX >= LECROY_WR_CH3
            case CHANNEL_CH3 :
                return LECROY_WR_CH3;
#endif
#if LECROY_WR_CH_MAX >= LECROY_WR_CH4
            case CHANNEL_CH4 :
                return LECROY_WR_CH4;
#endif
            case CHANNEL_TRACE_A :
            case CHANNEL_F1 :
                return LECROY_WR_TA;

            case CHANNEL_TRACE_B :
            case CHANNEL_F2 :
                return LECROY_WR_TB;

            case CHANNEL_TRACE_C :
            case CHANNEL_F3 :
                return LECROY_WR_TC;

            case CHANNEL_TRACE_D :
            case CHANNEL_F4 :
                return LECROY_WR_TD;

            case CHANNEL_M1 :
                return LECROY_WR_M1;

            case CHANNEL_M2 :
                return LECROY_WR_M2;

            case CHANNEL_M3 :
                return LECROY_WR_M3;

            case CHANNEL_M4 :
                return LECROY_WR_M4;

            case CHANNEL_EXT :
                return LECROY_WR_EXT;

            case CHANNEL_EXT10 :
                return LECROY_WR_EXT10;
        }

        if ( channel > CHANNEL_INVALID && channel < NUM_CHANNEL_NAMES )
        {
            if ( flag )
                return -1;
            print( FATAL, "Digitizer has no channel %s.\n",
                   Channel_Names[ channel ] );
            THROW( EXCEPTION );
        }

        if ( flag )
            return -1;
        print( FATAL, "Invalid channel number %ld.\n", channel );
        THROW( EXCEPTION );
    }
    else
    {
        switch ( channel )
        {
            case LECROY_WR_CH1 :
                return CHANNEL_CH1;

            case LECROY_WR_CH2 :
                return CHANNEL_CH2;
#if LECROY_WR_CH_MAX >= LECROY_WR_CH3
            case LECROY_WR_CH3 :
                return CHANNEL_CH3;
#endif
#if LECROY_WR_CH_MAX >= LECROY_WR_CH4
            case LECROY_WR_CH4 :
                return CHANNEL_CH4;
#endif
            case LECROY_WR_TA :
#if ! defined LECROY_WR_IS_XSTREAM
                return CHANNEL_TRACE_A;
#else
                return CHANNEL_F1;
#endif
            case LECROY_WR_TB :
#if ! defined LECROY_WR_IS_XSTREAM
                return CHANNEL_TRACE_B;
#else
                return CHANNEL_F2;
#endif
            case LECROY_WR_TC :
#if ! defined LECROY_WR_IS_XSTREAM
                return CHANNEL_TRACE_C;
#else
                return CHANNEL_F3;
#endif
            case LECROY_WR_TD :
#if ! defined LECROY_WR_IS_XSTREAM
                return CHANNEL_TRACE_D;
#else
                return CHANNEL_F4;
#endif
            case LECROY_WR_M1 :
                return CHANNEL_M1;

            case LECROY_WR_M2 :
                return CHANNEL_M2;

            case LECROY_WR_M3 :
                return CHANNEL_M3;

            case LECROY_WR_M4 :
                return CHANNEL_M4;

            case LECROY_WR_LIN :
                return CHANNEL_LINE;

            case LECROY_WR_EXT :
                return CHANNEL_EXT;

            case LECROY_WR_EXT10 :
                return CHANNEL_EXT10;

            default :
                print( FATAL, "Internal error detected at %s:%d.\n",
                        __FILE__, __LINE__ );
                THROW( EXCEPTION );
        }
    }

    return -1;
}


/*-----------------------------------------------------------*
 * Function makes a copy of the stucture holding information
 * about the current state of the oscilloscope
 *-----------------------------------------------------------*/

void
lecroy_wr_store_state( LECROY_WR_T * dest,
                       LECROY_WR_T * src )
{
    Window_T *w;
    int i;


    while ( dest->w != NULL )
    {
        w = dest->w;
        dest->w = w->next;
        T_free( w );
    }

    *dest = *src;

    if ( src->num_windows == 0 )
    {
        dest->w = NULL;
        return;
    }

    dest->w = NULL;
    dest->w = T_malloc( src->num_windows * sizeof *dest->w );

    for ( i = 0, w = src->w; w != NULL; i++, w = w->next )
    {
        *( dest->w + i ) = *w;
        if ( i != 0 )
            dest->w->prev = dest->w - 1;
        if ( w->next != NULL )
            dest->w->next = dest->w + 1;
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
