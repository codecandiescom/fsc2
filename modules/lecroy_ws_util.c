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


#include "lecroy_ws.h"


/* Structure for storing the relevant state information if a check of
   the trigger delay can't be done during the test run */

typedef struct LECROY_WS_PTC_DELAY LECROY_WS_PTC_DELAY_T;

struct LECROY_WS_PTC_DELAY {
    double delay;
    LECROY_WS_PTC_DELAY_T * next;
};

static LECROY_WS_PTC_DELAY_T * lecroy_ws_ptc_delay = NULL;


/* Structure for storing the relevant state information if a window check
   can't be done during the test run */

typedef struct LECROY_WS_PTC_WINDOW LECROY_WS_PTC_WINDOW_T;

struct LECROY_WS_PTC_WINDOW {
    bool is_timebase;              /* is timebase already set ? */
    double timebase;               /* timebase (if set) */
    int tb_index;                  /* index in timebase table */
    HORI_RES_T *cur_hres;          /* time resolution settings */
    bool is_trigger_delay;         /* is trigger delay already set ? */
    double trigger_delay;          /* trigger delay (if set) */
    long num;                      /* window number */
    double start;                  /* window start position */
    double width;                  /* window length */
    bool show_num;                 /* add window number in error message ? */
    LECROY_WS_PTC_WINDOW_T * next; /* next structure in list */
};

static LECROY_WS_PTC_WINDOW_T * lecroy_ws_ptc_window = NULL;


static void lecroy_ws_soe_trigger_delay_check( double delay );
static void lecroy_ws_soe_window_check( LECROY_WS_PTC_WINDOW_T * p );
static long lecroy_ws_calc_pos( double t );


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
lecroy_ws_soe_checks( void )
{
    LECROY_WS_PTC_DELAY_T *pd;
    LECROY_WS_PTC_WINDOW_T *pw;


    /* Test the trigger delay and window settings we couldn't check during
       the test run */

    for ( pd = lecroy_ws_ptc_delay; pd != NULL; pd = pd->next )
        lecroy_ws_soe_trigger_delay_check( pd->delay ); 

    for ( pw = lecroy_ws_ptc_window; pw != NULL; pw = pw->next )
        lecroy_ws_soe_window_check( pw ); 
}


/*-----------------------------------------------------------------------*
 * Some memory may still be allocated for the tests of trigger delays
 * and window positions to be done at the start of the experiment. This
 * function must be called in the exit handler to get rid of the memory.
 *-----------------------------------------------------------------------*/

void
lecroy_ws_exit_cleanup( void )
{
    LECROY_WS_PTC_DELAY_T *pd;
    LECROY_WS_PTC_WINDOW_T *pw;

    
    while ( lecroy_ws_ptc_delay != NULL )
    {
        pd = lecroy_ws_ptc_delay;
        lecroy_ws_ptc_delay = lecroy_ws_ptc_delay->next;
        T_free( pd );
    }

    while ( lecroy_ws_ptc_window != NULL )
    {
        pw = lecroy_ws_ptc_window;
        lecroy_ws_ptc_window = lecroy_ws_ptc_window->next;
        T_free( pw );
    }
}


/*----------------------------------------------------------------------*
 * Function for checking the trigger delay. The only argument is a bool,
 * if set the call comes from directly setting the trigger delay while,
 * if unset, it comes from a change of the timebase, te interleave mode
 * or the memory size.
 *-----------------------------------------------------------------------*/

double
lecroy_ws_trigger_delay_check( bool from )
{
    double delay = lecroy_ws.trigger_delay;
    double real_delay;


    /* Nothing needs to be done if the trigger delay never was set */

    if ( ! lecroy_ws.is_trigger_delay )
        return delay;

    /* We can't do the check during the test run if the timebase isn't set, so
       store the relevant state information for the time when the experiment
       gets started and we can ask the device for the timebase */

    if ( FSC2_MODE == TEST && ! lecroy_ws.is_timebase )
    {
        LECROY_WS_PTC_DELAY_T *p = lecroy_ws_ptc_delay;

        if ( lecroy_ws_ptc_delay == NULL )
            p = lecroy_ws_ptc_delay = T_malloc( sizeof *lecroy_ws_ptc_delay );
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

        p->delay = delay;
        return delay;
    }

    /* The delay can only be set with a certain resolution (1/10) of the
       current timebase, so make it a integer multiple of this */

    real_delay = LECROY_WS_TRIG_DELAY_RESOLUTION * lecroy_ws.timebase
                 * lrnd( delay / ( LECROY_WS_TRIG_DELAY_RESOLUTION *
                                   lecroy_ws.timebase ) );

    /* Check that the trigger delay is within the limits (taking rounding
       errors of the order of the current time resolution into account) */

    if (    real_delay > 0.0
         && real_delay >   5.0 * lecroy_ws.timebase
                         + 0.5 * lecroy_ws_time_per_point( ) )
    {
        print( FATAL, "Pre-trigger delay of %s %sis too long, can't be "
               "longer than 5 times the timebase.\n",
               lecroy_ws_ptime( real_delay ), from ? "" : "now " );
        THROW( EXCEPTION );
    }

    if (    real_delay < 0.0
         && real_delay <   -1.0e4 * lecroy_ws.timebase
                         -    0.5 * lecroy_ws_time_per_point( ) )
    {
        print( FATAL, "Post-triger delay of %s %sis too long, can't be "
               "longer than 10,000 times the timebase.\n",
               lecroy_ws_ptime( real_delay ), from ? "" : "now " );
        THROW( EXCEPTION );
    }

    /* If the difference between the requested trigger delay and the one
       that can be set is larger than the time resolution warn the user */

    if ( fabs( real_delay - delay ) > lecroy_ws_time_per_point( ) )
        print( WARN, "Trigger delay had to be adjusted from %s to %s.\n",
               lecroy_ws_ptime( delay ), lecroy_ws_ptime( real_delay ) );

    return real_delay;
}


/*-----------------------------------------------------------*
 * Function for testing trigger delay settings that couldn't
 * be checked during the test run.
 *-----------------------------------------------------------*/

static void
lecroy_ws_soe_trigger_delay_check( double delay )
{
    double real_delay = LECROY_WS_TRIG_DELAY_RESOLUTION * lecroy_ws.timebase
                        * lrnd( delay / ( LECROY_WS_TRIG_DELAY_RESOLUTION *
                                          lecroy_ws.timebase ) );

    /* Check that the trigger delay is within the limits (taking rounding
       errors of the order of the current time resolution into account) */

    if (    real_delay > 0.0
         && real_delay >   5.0 * lecroy_ws.timebase
                         + 0.5 * lecroy_ws_time_per_point( ) )
    {
        print( FATAL, "During the experiment a pre-trigger delay of %s is "
               "going to be used which is too long, it can't be longer than "
               "5 times the timebase.\n", lecroy_ws_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    if (    real_delay < 0.0
         && real_delay <   -1.0e4 * lecroy_ws.timebase
                         -    0.5 * lecroy_ws_time_per_point( ) )
    {
        print( FATAL, "During the experiment the post-triger delay of %s is "
               "going to be used wich is too long, it can't be longer than "
               "10,000 times the timebase.\n", lecroy_ws_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    /* If the difference between the requested trigger delay and the one
       that can be set is larger than the time resolution warn the user */

    if ( fabs( real_delay - delay ) > lecroy_ws_time_per_point( ) )
        print( WARN, "During the experiment the trigger delay will have to be "
               "adjusted from %s to %s.\n",
               lecroy_ws_ptime( delay ), lecroy_ws_ptime( real_delay ) );
}


/*---------------------*
 * Deletes all windows
 *---------------------*/

void
lecroy_ws_delete_windows( LECROY_WS_T * s )
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
lecroy_ws_get_window_by_number( long wid )
{
    Window_T *w;
    

    if ( wid >= WINDOW_START_NUMBER )
        for ( w = lecroy_ws.w; w != NULL; w = w->next )
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
lecroy_ws_all_windows_check( void )
{
    Window_T *w = lecroy_ws.w;


    while ( w != NULL )
    {
        lecroy_ws_window_check( w, SET );
        w = w->next;
    }
}


/*------------------------------------------------------*
 * Checks if the window fits into the recorded data set
 *------------------------------------------------------*/

void
lecroy_ws_window_check( Window_T * w,
                        bool       show_num )
{
    long start;
    long end;
    long max_len = lecroy_ws_curve_length( );


    /* During the test run the window position and width can only be checked
       if we know the timebase and the trigger delay, so if they haven't been
       explicitely set by the user just store them together with the window
       settings for a test when the experiment has been started. */

    if (    FSC2_MODE == TEST
         && ( ! lecroy_ws.is_timebase || ! lecroy_ws.is_trigger_delay ) )
    {
        LECROY_WS_PTC_WINDOW_T *p = lecroy_ws_ptc_window;

        if ( lecroy_ws_ptc_window == NULL )
            p = lecroy_ws_ptc_window =
                                      T_malloc( sizeof *lecroy_ws_ptc_window );
        else
        {
            while ( p->next != NULL )
                p = p->next;

            p->next = T_malloc( sizeof *p );
            p = p->next;
        }

        /* Store the current settings as far as they are known */

        p->is_timebase = lecroy_ws.is_timebase;
        if ( lecroy_ws.is_timebase )
        {
            p->timebase = lecroy_ws.timebase;
            p->tb_index = lecroy_ws.tb_index;
            p->cur_hres = lecroy_ws.cur_hres;
        }

        p->is_trigger_delay = lecroy_ws.is_trigger_delay;
        if ( lecroy_ws.is_trigger_delay )
            p->trigger_delay = lecroy_ws.trigger_delay;

        p->num = w->num;
        p->start = w->start;
        p->width = w->width;
        p->show_num = show_num;
        return;
    }

    /* Start with calculating the start and end position of the window
       in points */

    start = lecroy_ws_calc_pos( w->start );
    end   = lecroy_ws_calc_pos( w->start + w->width );

    if ( start < 0 )
    {
        if ( show_num > 0 )
            print( FATAL, "%ld. window starts too early, earliest possible "
                   "time is %s\n", w->num - WINDOW_START_NUMBER + 1,
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay
                                    - 5 * lecroy_ws.timebase ) );
        else
            print( FATAL, "Window starts too early, earliest possible time "
                   "is %s\n",
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay
                                    - 5 * lecroy_ws.timebase ) );
        THROW( EXCEPTION );
    }

    if ( start >= max_len - 1 )
    {
        if ( show_num > 0 )
            print( FATAL, "%ld. window starts too late, last possible time "
                   "is %s.\n", w->num - WINDOW_START_NUMBER + 1,
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay
                                    + 5 * lecroy_ws.timebase
                                    - 2 * lecroy_ws_time_per_point( ) ) );
        else
            print( FATAL, "Window starts too late, last possible time "
                   "is %s.\n",
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay
                                    + 5 * lecroy_ws.timebase
                                    - 2 * lecroy_ws_time_per_point( ) ) );
        THROW( EXCEPTION );
    }

    if ( end >= max_len )
    {
        if ( show_num > 0 )
            print( FATAL, "%d. window ends too late, largest possible width "
                   "is %s.\n", w->num - WINDOW_START_NUMBER + 1,
                   lecroy_ws_ptime( lecroy_ws_time_per_point( ) *
                                    ( max_len - 1 - start ) ) );
        else
            print( FATAL, "Window ends too late, largest possible width "
                   "is %s.\n",
                   lecroy_ws_ptime( lecroy_ws_time_per_point( ) *
                                    ( max_len - 1 - start ) ) );
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
lecroy_ws_soe_window_check( LECROY_WS_PTC_WINDOW_T * p )
{
    double real_timebase = lecroy_ws.timebase;
    int real_tb_index = lecroy_ws.tb_index;
    HORI_RES_T *real_cur_hres = lecroy_ws.cur_hres;
    double real_trigger_delay = lecroy_ws.trigger_delay;
    long start;
    long end;
    long max_len;
    bool show_num = p->show_num;


    /* Restore the internal representation of the state of the device as far
       as necessary to the one we had when the window settings couldn't be
       tested */

    if ( p->is_timebase )
    {
        lecroy_ws.timebase = p->timebase;
        lecroy_ws.tb_index = p->tb_index;
        lecroy_ws.cur_hres = p->cur_hres;
    }

    if ( p->is_trigger_delay )
        lecroy_ws.trigger_delay = p->trigger_delay;

    /* Now do the required tests */

    max_len = lecroy_ws_curve_length( );
    start = lecroy_ws_calc_pos( p->start );
    end   = lecroy_ws_calc_pos( p->start + p->width );

    if ( start < 0 )
    {
        if ( show_num > 0 )
            print( FATAL, "During the experiment the %ld. window is going to "
                   "start too early, earliest possible time is %s\n",
                   p->num - WINDOW_START_NUMBER + 1,
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay
                                    - 5 * lecroy_ws.timebase ) );
        else
            print( FATAL, "During the experiment the window is going to start "
                   "too early, earliest possible time is %s\n",
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay
                                    - 5 * lecroy_ws.timebase ) );
        THROW( EXCEPTION );
    }

    if ( start >= max_len - 1 )
    {
        if ( show_num > 0 )
            print( FATAL, "During the experiment the %ld. window is going to "
                   "start too late, latest possible time is %s.\n",
                   p->num - WINDOW_START_NUMBER + 1,
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay
                                    + 5 * lecroy_ws.timebase
                                    - 2 * lecroy_ws_time_per_point( ) ) );
        else
            print( FATAL, "During the experiment the window is going to start "
                   "too late, latest possible time is %s.\n",
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay
                                    + 5 * lecroy_ws.timebase
                                    - 2 * lecroy_ws_time_per_point( ) ) );
        THROW( EXCEPTION );
    }

    if ( end >= max_len )
    {
        if ( show_num > 0 )
            print( FATAL, "During the experiment the %d. window is going to "
                   "end too late, largest possible width is %s.\n",
                   p->num - WINDOW_START_NUMBER + 1,
                   lecroy_ws_ptime( lecroy_ws_time_per_point( ) *
                                    ( max_len - 1 - start ) ) );
        else
            print( FATAL, "During the experiment the window is going to end "
                   "too late, largest possible width is %s.\n",
                   lecroy_ws_ptime( lecroy_ws_time_per_point( ) *
                                    ( max_len - 1 - start ) ) );
        THROW( EXCEPTION );
    }

    /* Reset the internal representation back to the real one */

    if ( p->is_timebase )
    {
        lecroy_ws.timebase = real_timebase;
        lecroy_ws.tb_index = real_tb_index;
        lecroy_ws.cur_hres = real_cur_hres;
    }

    if ( p->is_trigger_delay )
        lecroy_ws.trigger_delay = real_trigger_delay;
}


/*----------------------------------------------------------*
 * Returns a string with a time value with a resonable unit
 *----------------------------------------------------------*/

const char *
lecroy_ws_ptime( double p_time )
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


/*-------------------------------------------------------------*
 * Returns a windows start or end position in points given the
 * position relative to the trigger position
 *-------------------------------------------------------------*/

static long
lecroy_ws_calc_pos( double t )
{
    return  lrnd( ( t + lecroy_ws.trigger_delay + 5 * lecroy_ws.timebase ) /
                   lecroy_ws_time_per_point( ) );
}


/*-----------------------------------------------------------------*
 * Returns the number of samples in a curve for the current setting
 * of the timebase and interleaved mode
 *-----------------------------------------------------------------*/

long
lecroy_ws_curve_length( void )
{
    return 10 *
           ( ( lecroy_ws.cur_hres->ppd_ris > 0 && lecroy_ws.interleaved ) ?
             lecroy_ws.cur_hres->ppd_ris : lecroy_ws.cur_hres->ppd );
}


/*------------------------------------------------------------------*
 * Returns the time distance between two points of a curve for the
 * current setting of the timebase and interleave mode
 *------------------------------------------------------------------*/

double
lecroy_ws_time_per_point( void )
{
    return ( lecroy_ws.cur_hres->ppd_ris > 0 && lecroy_ws.interleaved ) ?
           lecroy_ws.cur_hres->tpp_ris : lecroy_ws.cur_hres->tpp;
}


/*-------------------------------------------------------------------------*
 * The function translates back and forth between the channel numbers the
 * way the user specifies them in the EDL program and the channel numbers
 * as specified in the header file. If the channel number can't be mapped
 * correctly, the way the function reacts depends on the value of the
 * third argument: if this is UNSET, an error message gets printed and an
 * exception is thrown. If it is SET -1 is returned to indicate the error.
 *-------------------------------------------------------------------------*/

long
lecroy_ws_translate_channel( int  dir,
                             long channel,
                             bool flag )
{
    if ( dir == GENERAL_TO_LECROY_WS )
    {
        switch ( channel )
        {
            case CHANNEL_CH1 :
                return LECROY_WS_CH1;

            case CHANNEL_CH2 :
                return LECROY_WS_CH2;
#ifdef LECROY_WS_CH3
            case CHANNEL_CH3 :
                return LECROY_WS_CH3;
#endif
#ifdef LECROY_WS_CH4
            case CHANNEL_CH4 :
                return LECROY_WS_CH4;
#endif
            case CHANNEL_MATH :
            case CHANNEL_F1 :
                return LECROY_WS_MATH;

            case CHANNEL_M1 :
                return LECROY_WS_M1;

            case CHANNEL_M2 :
                return LECROY_WS_M2;

            case CHANNEL_M3 :
                return LECROY_WS_M3;

            case CHANNEL_M4 :
                return LECROY_WS_M4;

            case CHANNEL_Z1 :
                return LECROY_WS_Z1;

            case CHANNEL_Z2 :
                return LECROY_WS_Z2;

            case CHANNEL_Z3 :
                return LECROY_WS_Z3;

            case CHANNEL_Z4 :
                return LECROY_WS_Z4;

            case CHANNEL_LINE :
                return LECROY_WS_LIN;

            case CHANNEL_EXT :
                return LECROY_WS_EXT;

            case CHANNEL_EXT10 :
                return LECROY_WS_EXT10;
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
            case LECROY_WS_CH1 :
                return CHANNEL_CH1;

            case LECROY_WS_CH2 :
                return CHANNEL_CH2;
#ifdef LECROY_WS_CH3
            case LECROY_WS_CH3 :
                return CHANNEL_CH3;
#endif
#ifdef LECROY_WS_CH4
            case LECROY_WS_CH4 :
                return CHANNEL_CH4;
#endif
            case LECROY_WS_MATH :
                return CHANNEL_MATH;

            case LECROY_WS_M1 :
                return CHANNEL_M1;

            case LECROY_WS_M2 :
                return CHANNEL_M2;

            case LECROY_WS_M3 :
                return CHANNEL_M3;

            case LECROY_WS_M4 :
                return CHANNEL_M4;

            case LECROY_WS_Z1 :
                return CHANNEL_Z1;

            case LECROY_WS_Z2 :
                return CHANNEL_Z2;

            case LECROY_WS_Z3 :
                return CHANNEL_Z3;

            case LECROY_WS_Z4 :
                return CHANNEL_Z4;

            case LECROY_WS_LIN :
                return CHANNEL_LINE;

            case LECROY_WS_EXT :
                return CHANNEL_EXT;

            case LECROY_WS_EXT10 :
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
lecroy_ws_store_state( LECROY_WS_T * dest,
                       LECROY_WS_T * src )
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
