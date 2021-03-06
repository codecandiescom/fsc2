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


#include "lecroy9400.h"

#if 0
static void lecroy9400_window_check_1( bool * is_start,
                                       bool * is_width );

static void lecroy9400_window_check_2( void );

static void lecroy9400_window_check_3( void );
#endif


/*-----------------------------------------------------------*
 *-----------------------------------------------------------*/

int
lecroy9400_get_tb_index( double timebase )
{
    size_t i;


    for ( i = 0; i < NUM_ELEMS( tb ) - 1; i++ )
        if ( timebase >= tb[ i ] && timebase <= tb[ i + 1 ] )
            return i + ( ( tb[ i ] / timebase > timebase / tb[ i + 1 ] ) ?
                         0 : 1 );

    return i;
}


/*-----------------------------------------------------------*
 * Returns a string with a time value with a resonable unit.
 *-----------------------------------------------------------*/

const char *
lecroy9400_ptime( double p_time )
{
    static char buffer[ 2 ][ 128 ];
    static size_t i = 1;


    i = ( i + 1 ) % 2;
    if ( fabs( p_time ) >= 1.0 )
        sprintf( buffer[ i ], "%g s", p_time );
    else if ( fabs( p_time ) >= 1.e-3 )
        sprintf( buffer[ i ], "%g ms", 1.e3 * p_time );
    else if ( fabs( p_time ) >= 1.e-6 )
        sprintf( buffer[ i ], "%g us", 1.e6 * p_time );
    else
        sprintf( buffer[ i ], "%g ns", 1.e9 * p_time );

    return buffer[ i ];
}


/*-----------------------------------------------------------*
 * Function for checking the trigger delay when the timebase
 * gets changed
 *-----------------------------------------------------------*/

double
lecroy9400_trigger_delay_check( void )
{
    double delay = lecroy9400.trigger_delay;
    double real_delay;


    /* Nothing needs to be done if the trigger delay never was set */

    if ( ! lecroy9400.is_trigger_delay )
        return delay;

    /* The delay can only be set in units of 1/50 of the timebase */

    real_delay = 0.02 * lrnd( 50.0 * delay / lecroy9400.timebase )
                 * lecroy9400.timebase;

    /* Check that the trigger delay is within the limits (taking rounding
       errors of the order of the current time resolution into account) */

    if (    real_delay > 0.0
         && real_delay >   10.0 * lecroy9400.timebase
                         +  0.5 * tpp[ lecroy9400.tb_index ] )
    {
        print( FATAL, "Pre-trigger delay of %s now is too long, can't be "
               "longer than 10 times the timebase.\n",
               lecroy9400_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    if (    real_delay < 0.0
         && real_delay <   -1.0e4 * lecroy9400.timebase
                         -  0.5 * tpp[ lecroy9400.tb_index ] )
    {
        print( FATAL, "Post-triger delay of %s now is too long, can't be "
               "longer that 10,000 times the timebase.\n",
               lecroy9400_ptime( real_delay ) );
        THROW( EXCEPTION );
    }

    /* If the difference between the requested trigger delay and the one
       that can be set is larger than the time resolution warn the user */

    if ( fabs( real_delay - delay ) > tpp[ lecroy9400.tb_index ] )
        print( WARN, "Trigger delay had to be adjusted from %s to %s.\n",
               lecroy9400_ptime( delay ), lecroy9400_ptime( real_delay ) );

    return real_delay;
}


/*-----------------------------------------------------------------*
 * Deletes a window by removing it from the linked list of windows
 *-----------------------------------------------------------------*/

void
lecroy9400_delete_windows( LECROY9400_T * s )
{
    Window_T *w;


    while ( s->w != NULL )
    {
        w = s->w;
        s->w = w->next;
        T_free( w );
    }
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_do_pre_exp_checks( void )
{
#if 0
    Window_T *w;
    bool is_start, is_width;
    double width;
#endif
    int i;


    /* If a trigger channel has been set in the PREPARATIONS section send
       it to the digitizer */

    if ( lecroy9400.is_trigger_channel )
        lecroy9400_set_trigger_source( lecroy9400.trigger_channel );

    /* Switch on all channels that are used in the measurements */

    for ( lecroy9400.num_used_channels = 0, i = 0; i <= LECROY9400_FUNC_F; i++)
        lecroy9400_display( i, lecroy9400.channels_in_use[ i ] );

    /* Remove all unused windows and test if for all other windows the
       start position and width is set */

#if 0
    lecroy9400_window_check_1( &is_start, &is_width);

    /* If there are no windows we're already done */

    if ( lecroy9400.w == NULL )
#endif
        return;

#if 0

    /* If start position isn't set for all windows set it to the position of
       the left cursor */

    if ( ! is_start )
        for ( w = lecroy9400.w; w != NULL; w = w->next )
            if ( ! w->is_start )
            {
                w->start = lecroy9400.cursor_pos;
                w->is_start = SET;
            }

    /* If no width is set for all windows get the distance of the cursors on
       the digitizers screen and use it as the default width. */

    if ( ! is_width )
    {
        lecroy9400_get_cursor_distance( &width );

        width = fabs( width );

        if ( width == 0.0 )
        {
            print( FATAL, "Can't determine a reasonable value for still "
                   "undefined window widths.\n" );
            THROW( EXCEPTION );
        }

        for ( w = lecroy9400.w; w != NULL; w = w->next )
            if ( ! w->is_width )
            {
                w->width = width;
                w->is_width = SET;
            }
    }

    /* Make sure the windows are ok, i.e. cursors can be positioned exactly
       and are still within the range of the digitizers record length */

    lecroy9400_window_check_2( );
    lecroy9400_window_check_3( );

#endif
}

#if 0
/*---------------------------------------------------------------*
 * Removes unused windows and checks if for all the used windows
 * a width is set - this is returned to the calling function
 *---------------------------------------------------------------*/

static void
lecroy9400_window_check_1( bool * is_start,
                           bool * is_width )
{
    Window_T *w;


    *is_start = *is_width = SET;

    for ( w = lecroy9400.w; w != NULL; w = w->next )
    {
        if ( ! w->is_start )
            *is_start = UNSET;

        if ( ! w->is_width )
            *is_width = UNSET;
    }
}


/*---------------------------------------------------------------------*
 * It's not possible to set arbitrary cursor positions and distances -
 * they've got to be multiples of the smallest time resolution of the
 * digitizer, which is the time base divided by TDS_POINTS_PER_DIV.
 * Rhe function tests if the requested cursor position and distance
 * fit this requirement and if not the values are readjusted. While
 * settings for the position and width of the window not being exact
 * multiples of the resultion are probably no serious errors a window
 * width of less than the resolution is a hint for a real problem. And
 * while we're at it we also try to find out if all window widths are
 * equal - than we can use tracking cursors.
 *---------------------------------------------------------------------*/

static void
lecroy9400_window_check_2( void )
{
    Window_T *w;
    double dcs, dcd, dtb, fac;
    long rtb, cs, cd;


    for ( w = lecroy9400.w; w != NULL; w = w->next )
    {
        dcs = w->start;
        dtb = lecroy9400.timebase;
        fac = 1.0;

        while (    ( fabs( dcs ) != 0.0 && fabs( dcs ) < 1.0 )
                || fabs( dtb ) < 1.0 )
        {
            dcs *= 1000.0;
            dtb *= 1000.0;
            fac *= 0.001;
        }
        cs = lrnd( TDS_POINTS_PER_DIV * dcs );
        rtb = lrnd( dtb );

        if ( cs % rtb )       /* window start not multiple of a point ? */
        {
            cs = ( cs / rtb ) * rtb;
            dcs = cs * fac / TDS_POINTS_PER_DIV;
            print( WARN, "Start point of window %ld had to be readjusted from "
                   "%s to %s.\n", w->num, lecroy9400_ptime( w->start ),
                   lecroy9400_ptime( dcs ) );
            w->start = dcs;
        }

        dcd = w->width;
        dtb = lecroy9400.timebase;
        fac = 1.0;

        while ( fabs( dcd ) < 1.0 || fabs( dtb ) < 1.0 )
        {
            dcd *= 1000.0;
            dtb *= 1000.0;
            fac *= 0.001;
        }
        cd = lrnd( TDS_POINTS_PER_DIV * dcd );
        rtb = lrnd( dtb );

        if ( labs( cd ) < rtb )    /* window smaller than one point ? */
        {
            dcd = lecroy9400.timebase / TDS_POINTS_PER_DIV;
            print( SEVERE, "Width of window %ld had to be readjusted from %s "
                   "to %s.\n", w->num, lecroy9400_ptime( w->width ),
                   lecroy9400_ptime( dcd ) );
            w->width = dcd;
        }
        else if ( cd % rtb )       /* window width not multiple of a point ? */
        {
            cd = ( cd / rtb ) * rtb;
            dcd = cd * fac / TDS_POINTS_PER_DIV;
            print( WARN, "Width of window %ld had to be readjusted from %s to "
                   "%s.\n", w->num, lecroy9400_ptime( w->width ),
                   lecroy9400_ptime( dcd ) );
            w->width = dcd;
        }
    }
}


/*-------------------------------------------------------------*
 * This function checks if the windows fit into the digitizers
 * measurement window and calculate the positions of the start
 * and the end of the windows in units of points.
 *-------------------------------------------------------------*/

static void
lecroy9400_window_check_3( void )
{
    Window_T *w;
    double window;


    window = lecroy9400.timebase * lecroy9400.rec_len / TDS_POINTS_PER_DIV;

    for ( w = lecroy9400.w; w != NULL; w = w->next )
    {
        if (    w->start > ( 1.0 - lecroy9400.trig_pos ) * window
             || w->start + w->width > ( 1.0 - lecroy9400.trig_pos ) * window
             || w->start < - lecroy9400.trig_pos * window
             || w->start + w->width < - lecroy9400.trig_pos * window )
        {
            print( FATAL, "Window %ld doesn't fit into current digitizer time "
                   "range.\n", w->num );
            THROW( EXCEPTION );
        }

        /* Take care: Numbers start from 1 ! */

        w->start_num = lrnd( ( w->start + lecroy9400.trig_pos * window )
                             * TDS_POINTS_PER_DIV / lecroy9400.timebase ) + 1;
        w->end_num = lrnd( ( w->start + w->width
                             + lecroy9400.trig_pos * window )
                             * TDS_POINTS_PER_DIV / lecroy9400.timebase ) + 1;

        if ( w->end_num - w->start_num <= 0 )
        {
            print( FATAL, "Window %ld has width of less than 1 point.\n",
                   w->num );
            THROW( EXCEPTION );
        }
    }
}

#endif


/*--------------------------------------------------------------*
 * The function is used to translate back and forth between the
 * channel numbers the way the user specifies them in the EDL
 * program and the channel numbers as specified in the header
 * file. When the channel number can't be maped correctly, the
 * way the function reacts depends on the value of the third
 * argument: If this is UNSET, an error message gets printed
 * and an exception ios thrown. If it is SET -1 is returned to
 * indicate the error.
 *--------------------------------------------------------------*/

long
lecroy9400_translate_channel( int  dir,
                              long channel,
                              bool flag )
{
    if ( dir == GENERAL_TO_LECROY9400 )
    {
        switch ( channel )
        {
            case CHANNEL_CH1 :
                return LECROY9400_CH1;

            case CHANNEL_CH2 :
                return LECROY9400_CH2;

            case CHANNEL_MEM_C :
                return LECROY9400_MEM_C;

            case CHANNEL_MEM_D :
                return LECROY9400_MEM_D;

            case CHANNEL_FUNC_E :
                return LECROY9400_FUNC_E;

            case CHANNEL_FUNC_F :
                return LECROY9400_FUNC_F;

            case CHANNEL_LINE :
                return LECROY9400_LIN;

            case CHANNEL_EXT :
                return LECROY9400_EXT;

            case CHANNEL_EXT10 :
                return LECROY9400_EXT10;
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
            case LECROY9400_CH1 :
                return CHANNEL_CH1;

            case LECROY9400_CH2 :
                return CHANNEL_CH2;

            case LECROY9400_MEM_C :
                return CHANNEL_MEM_C;

            case LECROY9400_MEM_D :
                return CHANNEL_MEM_D;

            case LECROY9400_FUNC_E :
                return CHANNEL_FUNC_E;

            case LECROY9400_FUNC_F :
                return CHANNEL_FUNC_F;

            case LECROY9400_LIN :
                return CHANNEL_LINE;

            case LECROY9400_EXT :
                return CHANNEL_EXT;

            case LECROY9400_EXT10 :
                return CHANNEL_EXT10;

            default :
                print( FATAL, "Internal error detected at %s:%d.\n",
                        __FILE__, __LINE__ );
                THROW( EXCEPTION );
        }
    }

    return -1;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
lecroy9400_store_state( LECROY9400_T * dest,
                        LECROY9400_T * src )
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
        dest->w = 0;
        return;
    }

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
