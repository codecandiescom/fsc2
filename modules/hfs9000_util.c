/*
 *  $Id$
 * 
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


#include "hfs9000.h"



/*-----------------------------------------------------------------*
 * Converts a time into the internal type of a time specification,
 * i.e. an integer multiple of the time base
 *-----------------------------------------------------------------*/

Ticks
hfs9000_double2ticks( double p_time )
{
    double ticks;


    if ( ! hfs9000.is_timebase )
    {
        print( FATAL, "Can't set a time because no pulser time base has been "
               "set.\n" );
        THROW( EXCEPTION );
    }

    ticks = p_time / hfs9000.timebase;

    if ( ticks > TICKS_MAX || ticks < TICKS_MIN )
    {
        print( FATAL, "Specified time is too long for time base of %s.\n",
               hfs9000_ptime( hfs9000.timebase ) );
        THROW( EXCEPTION );
    }

    if (    fabs( Ticksrnd( ticks ) - p_time / hfs9000.timebase ) > 1.0e-2
         || ( p_time > 0.99e-9 && Ticksrnd( ticks ) == 0 ) )
    {
        print( FATAL, "Specified time of %s is not an integer multiple of the "
               "pulser time base of %s.\n",
               hfs9000_ptime( p_time ), hfs9000_ptime( hfs9000.timebase ) );
        THROW( EXCEPTION );
    }

    return Ticksrnd( ticks );
}


/*-----------------------------------------------------*
 * Does the exact opposite of the previous function...
 *-----------------------------------------------------*/

double
hfs9000_ticks2double( Ticks ticks )
{
    fsc2_assert( hfs9000.is_timebase );
    return hfs9000.timebase * ticks;
}


/*----------------------------------------------------------------------*
 * Checks if the difference of the levels specified for a pod connector
 * are within the valid limits.
 *----------------------------------------------------------------------*/

void
hfs9000_check_pod_level_diff( double high,
                              double low )
{
    if ( low > high )
    {
        print( FATAL, "Low voltage level is above high level, use keyword "
               "INVERT to invert the polarity.\n" );
        THROW( EXCEPTION );
    }

    if ( high - low > MAX_POD_VOLTAGE_SWING + 0.1 * VOLTAGE_RESOLUTION )
    {
        print( FATAL, "Difference between high and low voltage of %g V is too "
               "big, maximum is %g V.\n", high - low, MAX_POD_VOLTAGE_SWING );
        THROW( EXCEPTION );
    }

    if ( high - low < MIN_POD_VOLTAGE_SWING - 0.1 * VOLTAGE_RESOLUTION )
    {
        print( FATAL, "Difference between high and low voltage of %g V is too "
               "small, minimum is %g V.\n",
               high - low, MIN_POD_VOLTAGE_SWING );
        THROW( EXCEPTION );
    }
}


/*-----------------------------------------------*
 * Returns the structure for pulse numbered pnum
 *-----------------------------------------------*/

Pulse_T *
hfs9000_get_pulse( long pnum )
{
    Pulse_T *cp = hfs9000.pulses;


    if ( pnum < 0 )
    {
        print( FATAL, "Invalid pulse number: %ld.\n", pnum );
        THROW( EXCEPTION );
    }

    while ( cp != NULL )
    {
        if ( cp->num == pnum )
            break;
        cp = cp->next;
    }

    if ( cp == NULL )
    {
        print( FATAL, "Referenced pulse #%ld does not exist.\n", pnum );
        THROW( EXCEPTION );
    }

    return cp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

const char *
hfs9000_ptime( double p_time )
{
    static char buffer[ 3 ][ 128 ];
    static size_t i = 2;


    i = ( i + 1 ) % 3;
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


/*----------------------------------------------------*
 *----------------------------------------------------*/

const char *
hfs9000_pticks( Ticks ticks )
{
    return hfs9000_ptime( hfs9000_ticks2double( ticks ) );
}


/*--------------------------------------------------------------------*
 * Comparison function for two pulses: returns 0 if both pulses are
 * inactive, -1 if only the second pulse is inactive or starts at a
 * later time and 1 if only the first pulse is inactive pulse or the
 * second pulse starts earlier.
 *--------------------------------------------------------------------*/

int
hfs9000_start_compare( const void * A,
                       const void * B )
{
    Pulse_T *a = *( Pulse_T ** ) A,
            *b = *( Pulse_T ** ) B;

    if ( ! a->is_active )
    {
        if ( ! b->is_active )
            return 0;
        else
            return 1;
    }

    if ( ! b->is_active || a->pos <= b->pos )
        return -1;

    return 1;
}


/*---------------------------------------------------------*
 * Determines the longest sequence of all pulse functions.
 *---------------------------------------------------------*/

Ticks
hfs9000_get_max_seq_len( void )
{
    int i;
    Ticks max = 0;
    Function_T *f;


    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = hfs9000.function + i;

        /* Nothing to be done for unused functions and the phase functions */

        if (    ! f->is_used
             || f->self == PULSER_CHANNEL_PHASE_1
             || f->self == PULSER_CHANNEL_PHASE_2 )
            continue;

        max = Ticks_max( max, f->max_seq_len + f->delay );
    }

    if ( hfs9000.is_max_seq_len )
        max = Ticks_max( max, hfs9000.max_seq_len );

    return max;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
hfs9000_set( char * arena,
             Ticks  start,
             Ticks  len,
             Ticks  offset )
{
    fsc2_assert( start + len + offset <= hfs9000.max_seq_len );

    memset( arena + offset + start, SET, len );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

int
hfs9000_diff( char *  old_p,
              char *  new_p,
              Ticks * start,
              Ticks * length )
{
    static Ticks where = 0;
    int ret;
    char *a = old_p + where,
         *b = new_p + where;
    char cur_state;


    /* If we reached the end of the arena in the last call return 0 */

    if ( where == -1 )
    {
        where = 0;
        return 0;
    }

    /* Search for next difference in the arena */

    while ( where < hfs9000.max_seq_len && *a == *b )
    {
        where++;
        a++;
        b++;
    }

    /* If none was found anymore */

    if ( where == hfs9000.max_seq_len )
    {
        where = 0;
        return 0;
    }

    /* Store the start position (including the offset and the necessary one
       due to the pulsers firmware bug) and store if we wave to reset (-1)
       or to set (1) */

    *start = where;
    ret = *a == SET ? -1 : 1;
    cur_state = *a;

    /* Now figure out the length of the area we have to set or reset */

    *length = 0;
    while ( where < hfs9000.max_seq_len && *a != *b && *a == cur_state )
    {
        where++;
        a++;
        b++;
        ( *length )++;
    }

    /* Set a marker that we reached the end of the arena */

    if ( where == hfs9000.max_seq_len )
        where = -1;

    return ret;
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

void
hfs9000_dump_channels( FILE * fp )
{
    Function_T *f;
    int i, k;


    if ( fp == NULL )
        return;

    fprintf( fp, "===\n" );

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = hfs9000.function + i;

        if ( ! f->is_needed )
            continue;

        if ( f->channel->self == HFS9000_TRIG_OUT )
            fprintf( fp, "%s:TRIG", f->name );
        else
            fprintf( fp, "%s:%c%c", f->name,
                     CHANNEL_LETTER( f->channel->self ),
                     CHANNEL_NUMBER( f->channel->self ) );
        for ( k = 0; k < f->num_active_pulses; k++ )
            fprintf( fp, " %ld %ld %ld",
                     f->pulses[ k ]->num,
                     f->pulses[ k ]->pos,
                     f->pulses[ k ]->len );
        fprintf( fp, "\n" );
    }
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

long
hfs9000_ch_to_num( long channel )
{
    switch ( channel )
    {
        case CHANNEL_TRIG_OUT :
            return 0;

        case CHANNEL_CH1 : case CHANNEL_A1 :
            return 1;

        case CHANNEL_CH2 : case CHANNEL_A2 :
            return 2;

        case CHANNEL_CH3 : case CHANNEL_A3 :
            return 3;

        case CHANNEL_CH4 : case CHANNEL_A4 :
            return 4;

        case CHANNEL_B1 :
            if ( NUM_CHANNEL_CARDS < 2 )
                break;
            return 5;

        case CHANNEL_B2 :
            if ( NUM_CHANNEL_CARDS < 2 )
                break;
            return 6;

        case CHANNEL_B3 :
            if ( NUM_CHANNEL_CARDS < 2 )
                break;
            return 7;

        case CHANNEL_B4 :
            if ( NUM_CHANNEL_CARDS < 2 )
                break;
            return 8;

        case CHANNEL_C1 :
            if ( NUM_CHANNEL_CARDS < 3 )
                break;
            return 9;

        case CHANNEL_C2 :
            if ( NUM_CHANNEL_CARDS < 3 )
                break;
            return 10;

        case CHANNEL_C3 :
            if ( NUM_CHANNEL_CARDS < 3 )
                break;
            return 11;

        case CHANNEL_C4 :
            if ( NUM_CHANNEL_CARDS < 3 )
                break;
            return 12;
    }

    print( FATAL, "Pulser has no channel named '%s'.\n",
           Channel_Names[ channel ] );
    THROW( EXCEPTION );

    return -1;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
