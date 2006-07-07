/*
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


#include "ep385.h"


/*-----------------------------------------------------------------*
 * At many places a correctly set timebase is required, but it can
 * also be set implicitely to the default timebase of the pulser
 * by simply not setting it. All functions that require a timebase
 * should call this function top make sure it is set correctly.
 *-----------------------------------------------------------------*/

void ep385_timebase_check( void )
{
    double tmp;


    if ( ! ep385.is_timebase )
    {
        ep385.timebase = FIXED_TIMEBASE;
        ep385.timebase_mode = INTERNAL;
        ep385.is_timebase = SET;

        tmp = SHAPE_2_DEFENSE_DEFAULT_MIN_DISTANCE / FIXED_TIMEBASE;
        tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
        ep385.shape_2_defense = Ticksrnd( tmp );

        tmp = DEFENSE_2_SHAPE_DEFAULT_MIN_DISTANCE / FIXED_TIMEBASE;
        tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
        ep385.defense_2_shape = Ticksrnd( tmp );

        tmp = MINIMUM_TWT_PULSE_DISTANCE / FIXED_TIMEBASE;
        tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
        ep385.minimum_twt_pulse_distance = Ticksrnd( tmp );
    }
}


/*-----------------------------------------------------------------*
 * Converts a time into the internal type of a time specification,
 * i.e. an integer multiple of the time base
 *-----------------------------------------------------------------*/

Ticks ep385_double2ticks( double p_time )
{
    double ticks;


    /* If the time base hasn't been set yet this indicates that we should
       use the built-in time base (by having *no* TIMEBASE command in the
       PREPARATIONS section) */

    ep385_timebase_check( );

    ticks = p_time / ep385.timebase;

    if ( ticks > TICKS_MAX || ticks < TICKS_MIN )
    {
        print( FATAL, "Specified time is too long for time base of %s.\n",
               ep385_ptime( ep385.timebase ) );
        THROW( EXCEPTION );
    }

    if ( fabs( Ticksrnd( ticks ) - p_time / ep385.timebase ) > 1.0e-2 ||
         ( p_time > 0.99e-9 && Ticksrnd( ticks ) == 0 ) )
    {
        char *t = T_strdup( ep385_ptime( p_time ) );
        print( FATAL, "Specified time of %s is not an integer multiple of the "
               "fixed pulser time base of %s.\n",
               t, ep385_ptime( ep385.timebase ) );
        T_free( t );
        THROW( EXCEPTION );
    }

    return Ticksrnd( ticks );
}


/*-----------------------------------------------------------------*
 * Converts a time into the internal type of a time specification,
 * i.e. an integer multiple of the time base
 *-----------------------------------------------------------------*/

Ticks ep385_double2ticks_simple( double p_time )
{
    double ticks;


    /* If the time base hasn't been set yet this indicates that we should
       use the built-in time base (by having *no* TIMEBASE command in the
       PREPARATIONS section) */

    ep385_timebase_check( );

    ticks = p_time / ep385.timebase;

    if ( ticks > TICKS_MAX || ticks < TICKS_MIN )
    {
        print( FATAL, "Specified time is too long for time base of %s.\n",
               ep385_ptime( ep385.timebase ) );
        THROW( EXCEPTION );
    }

    return Ticksrnd( ticks );
}


/*-----------------------------------------------------*
 * Does the exact opposite of the previous function...
 *-----------------------------------------------------*/

double ep385_ticks2double( Ticks ticks )
{
    return ( double ) ( ep385.timebase * ticks );
}


/*-----------------------------------------------*
 * Returns the structure for pulse numbered pnum
 *-----------------------------------------------*/

Pulse_T *ep385_get_pulse( long pnum )
{
    Pulse_T *cp = ep385.pulses;


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

const char *ep385_ptime( double p_time )
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

const char *ep385_pticks( Ticks ticks )
{
    return ep385_ptime( ep385_ticks2double( ticks ) );
}


/*-------------------------------------------------------------------*
 * Comparison function for two pulses: returns 0 if both pulses are
 * inactive, -1 if only the second pulse is inactive or starts at a
 * later time and 1 if only the first pulse is inactive pulse or the
 * second pulse starts earlier.
 *-------------------------------------------------------------------*/

int ep385_pulse_compare( const void * A,
                         const void * B )
{
    Pulse_Params_T *a = ( Pulse_Params_T * ) A,
                   *b = ( Pulse_Params_T * ) B;

    return a->pos <= b->pos ? -1 : 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void ep385_show_pulses( void )
{
    int pd[ 2 ];
    pid_t pid;


    if ( pipe( pd ) == -1 )
    {
        if ( errno == EMFILE || errno == ENFILE )
            print( FATAL, "Failure, running out of system resources.\n" );
        return;
    }

    if ( ( pid =  fork( ) ) < 0 )
    {
        if ( errno == ENOMEM || errno == EAGAIN )
            print( FATAL, "Failure, running out of system resources.\n" );
        return;
    }

    /* Here's the childs code */

    if ( pid == 0 )
    {
        char *cmd = NULL;


        CLOBBER_PROTECT( cmd );

        close( pd[ 1 ] );

        if ( dup2( pd[ 0 ], STDIN_FILENO ) == -1 )
        {
            goto filter_failure;
            close( pd[ 0 ] );
        }

        close( pd[ 0 ] );

        TRY
        {
            cmd = get_string( "%s%sfsc2_pulses", bindir, slash( bindir ) );
            TRY_SUCCESS;
        }
        OTHERWISE
            goto filter_failure;

        execl( cmd, "fsc2_pulses", NULL );

    filter_failure:

        T_free( cmd );
        _exit( EXIT_FAILURE );
    }

    /* And finally the code for the parent */

    close( pd[ 0 ] );
    ep385.show_file = fdopen( pd[ 1 ], "w" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void ep385_dump_pulses( void )
{
    char *name;
    char *m;
    struct stat stat_buf;


    do
    {
        TRY
        {
            name = T_strdup( fl_show_fselector( "File for dumping pulses:",
                                                "./", "*.pls", NULL ) );
            TRY_SUCCESS;
        }
        OTHERWISE
            return;

        if ( name == NULL || *name == '\0' )
        {
            T_free( name );
            return;
        }

        if  ( 0 == stat( name, &stat_buf ) )
        {
            m = get_string( "The selected file does already exist:\n%s\n"
                            "\nDo you really want to overwrite it?", name );
            if ( 1 != show_choices( m, 2, "Yes", "No", NULL, 2 ) )
            {
                T_free( m );
                name = CHAR_P T_free( name );
                continue;
            }
            T_free( m );
        }

        if ( ( ep385.dump_file = fopen( name, "w+" ) ) == NULL )
        {
            switch( errno )
            {
                case EMFILE :
                    show_message( "Sorry, you have too many open files!\n"
                                  "Please close at least one and retry." );
                    break;

                case ENFILE :
                    show_message( "Sorry, system limit for open files "
                                  "exceeded!\n Please try to close some "
                                  "files and retry." );
                break;

                case ENOSPC :
                    show_message( "Sorry, no space left on device for more "
                                  "file!\n    Please delete some files and "
                                  "retry." );
                    break;

                default :
                    show_message( "Sorry, can't open selected file for "
                                  "writing!\n       Please select a "
                                  "different file." );
            }

            name = CHAR_P T_free( name );
            continue;
        }
    } while ( ep385.dump_file == NULL );

    T_free( name );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

void ep385_dump_channels( FILE * fp )
{
    Function_T *f;
    Channel_T *ch;
    int i, j, k;
    const char *plist[ ] = { "+X", "-X", "+Y", "-Y" };


    if ( fp == NULL )
        return;

    fprintf( fp, "===\n" );

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = ep385.function + i;

        if ( ! f->is_needed && f->num_channels == 0 )
            continue;

        for ( j = 0; j < f->num_channels; j++ )
        {
            ch = f->channel[ j ];

            if ( ! ch->needs_update )
                continue;

            fprintf( fp, "%s:%d", f->name, ch->self );

            for ( k = 0; k < ch->num_active_pulses; k++ )
            {
                if ( f->self == PULSER_CHANNEL_PULSE_SHAPE &&
                     ch->pulse_params[ k ].pulse->sp != NULL )
                    fprintf( fp, " (%ld) %ld %ld",
                             ch->pulse_params[ k ].pulse->sp->num,
                             ch->pulse_params[ k ].pos,
                             ch->pulse_params[ k ].len );
                else if ( f->self == PULSER_CHANNEL_TWT &&
                          ch->pulse_params[ k ].pulse->tp != NULL )
                    fprintf( fp, " (%ld) %ld %ld",
                             ch->pulse_params[ k ].pulse->tp->num,
                             ch->pulse_params[ k ].pos,
                             ch->pulse_params[ k ].len );
                else
                    fprintf( fp, " %ld %ld %ld",
                             ch->pulse_params[ k ].pulse->num,
                             ch->pulse_params[ k ].pos,
                             ch->pulse_params[ k ].len );

                if ( f->phase_setup != NULL )
                {
                    if ( ch->pulse_params[ k ].pulse->pc == NULL )
                        fprintf( fp, " +X" );
                    else
                        fprintf( fp, " %s",
                                 plist[ ch->pulse_params[ k ].pulse->
                                             pc->sequence[ f->next_phase ] ] );
                }
            }

            fprintf( fp, "\n" );
        }
    }
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

void ep385_duty_check( void )
{
    Function_T *f;
    int i;
    int fns[ ] = { PULSER_CHANNEL_TWT, PULSER_CHANNEL_TWT_GATE };

    if ( ! ep385.is_repeat_time )
        return;


    for ( i = 0; i < 2; i++ )
    {
        f = ep385.function + fns[ i ];
        if ( f->is_used && f->num_channels > 0 &&
             ep385_calc_max_length( f ) > MAX_TWT_DUTY_CYCLE
                              * ( MAX_MEMORY_BLOCKS * BITS_PER_MEMORY_BLOCK +
                                  REPEAT_TICKS * ep385.repeat_time ) &&
                 f->max_duty_warning++ == 0 )
                print( SEVERE, "Duty cycle of TWT exceeded due to length of "
                       "%s pulses.\n", f->name );
    }
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Ticks ep385_calc_max_length( Function_T * f )
{
    int i, j;
    Channel_T *ch;
    Ticks max_len = 0;


    if ( ! f->is_needed || f->num_channels == 0 )
        return 0;

    for ( j = 0; j < f->num_channels; j++ )
    {
        ch = f->channel[ j ];

        for ( i = 0; i < ch->num_active_pulses; i++ )
            max_len += ch->pulse_params[ i ].len;
    }

    return max_len;
}


/*-----------------------------------------------*
 * This is a no-op function and only implemented
 * because some other pulsers also have it.
 *-----------------------------------------------*/

bool ep385_set_max_seq_len( double seq_len  UNUSED_ARG )
{
    print( WARN, "Pulser doesn't allow setting a maximum pattern length.\n" );
    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
