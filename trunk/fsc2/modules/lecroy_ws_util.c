/* -*-C-*-
 *
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


static long lecroy_ws_calc_pos( double t );



/*----------------------------------------------------------*
 * Returns a string with a time value with a resonable unit
 *----------------------------------------------------------*/

const char *lecroy_ws_ptime( double p_time )
{
    static char buffer[ 128 ];


    if ( fabs( p_time ) >= 1.0 )
        sprintf( buffer, "%.3f s", p_time );
    else if ( fabs( p_time ) >= 1.0e-3 )
        sprintf( buffer, "%.3f ms", 1.0e3 * p_time );
    else if ( fabs( p_time ) >= 1.0e-6 )
        sprintf( buffer, "%.3f us", 1.0e6 * p_time );
    else
        sprintf( buffer, "%.3f ns", 1.0e9 * p_time );

    return buffer;
}


/*-----------------------------------------------------------*
 * Function for checking the trigger delay when the timebase
 * gets changed
 *-----------------------------------------------------------*/

double lecroy_ws_trigger_delay_check( void )
{
    double delay = lecroy_ws.trigger_delay;
    double real_delay;


    /* Nothing needs to be done if the trigger delay never was set */

    if ( ! lecroy_ws.is_trigger_delay )
        return delay;

    /* The delay can only be set with a certain resolution (1/10) of the
       current timebase, so make it a integer multiple of this */

    real_delay = LECROY_WS_TRIG_DELAY_RESOLUTION * lecroy_ws.timebase
                 * lrnd( delay / ( LECROY_WS_TRIG_DELAY_RESOLUTION *
                                   lecroy_ws.timebase ) );

    /* Check that the trigger delay is within the limits (taking rounding
       errors of the order of the current time resolution into account) */

    if ( real_delay > 0.0 &&
         real_delay >   10.0 * lecroy_ws.timebase
                      +  0.5 * lecroy_ws_time_per_point( ) )
    {
        print( FATAL, "Pre-trigger delay of %s now is too long, can't be "
               "longer than 10 times the timebase.\n",
               lecroy_ws_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    if ( real_delay < 0.0 &&
         real_delay <   -1.0e4 * lecroy_ws.timebase
                      -  0.5 * lecroy_ws_time_per_point( ) )
    {
        print( FATAL, "Post-triger delay of %s now is too long, can't be "
               "longer that 10,000 times the timebase.\n",
               lecroy_ws_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    /* If the difference between the requested trigger delay and the one
       that can be set is larger than the time resolution warn the user */

    if ( fabs( real_delay - delay ) > lecroy_ws_time_per_point( ) )
    {
        char *cp = T_strdup( lecroy_ws_ptime( delay ) );

        print( WARN, "Trigger delay had to be adjusted from %s to %s.\n",
               cp, lecroy_ws_ptime( real_delay ) );
        T_free( cp );
    }

    return real_delay;
}


/*---------------------*
 * Deletes all windows
 *---------------------*/

void lecroy_ws_delete_windows( LECROY_WS_T * s )
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

Window_T *lecroy_ws_get_window_by_number( long wid )
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

void lecroy_ws_all_windows_check( void )
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

void lecroy_ws_window_check( Window_T * w,
                             bool       show_num )
{
    long start;
    long end;
    long max_len = lecroy_ws_curve_length( );
    double max_width = max_len * lecroy_ws_time_per_point( );
    double max_time = max_width - lecroy_ws.trigger_delay;


    /* Start with calculating the start and end position of the window
       in points */

    start = lecroy_ws_calc_pos( w->start );
    end   = lecroy_ws_calc_pos( w->start + w->width );

    if ( start < 0 )
    {
        if ( show_num > 0 )
            print( FATAL, "%ld. window starts too early, earliest possible "
                   "time is %s\n", w->num - WINDOW_START_NUMBER + 1,
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay ) );
        else
            print( FATAL, "Window starts too early, earliest possible time "
                   "is %s\n",
                   lecroy_ws_ptime( - lecroy_ws.trigger_delay ) );
        THROW( EXCEPTION );
    }

    if ( start > max_len )
    {
        if ( show_num > 0 )
            print( FATAL, "%ld. window starts too late, last possible time "
                   "is %s.\n", w->num - WINDOW_START_NUMBER + 1,
                   lecroy_ws_ptime( max_time ) );
        else
            print( FATAL, "Window starts too late, last possible time "
                   "is %s.\n", lecroy_ws_ptime( max_time ) );
        THROW( EXCEPTION );
    }

    if ( end > max_len )
    {
        if ( show_num > 0 )
            print( FATAL, "%d. window ends too late, largest possible width "
                   "is %s.\n", w->num - WINDOW_START_NUMBER + 1,
                   lecroy_ws_ptime( max_width ) );
        else
            print( FATAL, "Window ends too late, largest possible width "
                   "is %s.\n", lecroy_ws_ptime( max_width ) );
        THROW( EXCEPTION );
    }

    w->start_num  = start;
    w->end_num    = end;
    w->num_points = end - start + 1;
}


/*-------------------------------------------------------------*
 * Returns a windows start or end position in points given the
 * position relative to the trigger position
 *-------------------------------------------------------------*/

static long lecroy_ws_calc_pos( double t )
{
    return lrnd( ( t + lecroy_ws.trigger_delay ) /
                 lecroy_ws_time_per_point( ) );

}


/*-----------------------------------------------------------------*
 * Returns the number of samples in a curve for the current setting
 * of the timebase and interleaved mode
 *-----------------------------------------------------------------*/

long lecroy_ws_curve_length( void )
{
    return 10 *
           ( ( lecroy_ws.cur_hres->ppd_ris > 0 && lecroy_ws.interleaved ) ?
             lecroy_ws.cur_hres->ppd_ris : lecroy_ws.cur_hres->ppd );
}


/*-----------------------------------------------------------------*
 * Returns the time distance between to points of a curve for the
 * current setting of the timebase and interleaved mode
 *-----------------------------------------------------------------*/

double lecroy_ws_time_per_point( void )
{
    return ( lecroy_ws.cur_hres->ppd_ris > 0 && lecroy_ws.interleaved ) ?
           lecroy_ws.cur_hres->tpp_ris : lecroy_ws.cur_hres->tpp;
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

long lecroy_ws_translate_channel( int  dir,
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

void lecroy_ws_store_state( LECROY_WS_T * dest,
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
    dest->w = WINDOW_P T_malloc( src->num_windows * sizeof *dest->w );

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
