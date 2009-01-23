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


#include "rb_pulser_j.h"


/*-------------------------------------------------------------------*
 * Comparison function for two pulses: returns 0 if both pulses are
 * inactive, -1 if only the second pulse is inactive or starts at a
 * later time and 1 if only the first pulse is inactive pulse or the
 * second pulse starts earlier.
 *-------------------------------------------------------------------*/

int
rb_pulser_j_start_compare( const void * A,
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


/*-----------------------------------------------------------------*
 * Converts a time into the internal type of a time specification,
 * i.e. a integer multiple of the time base
 *-----------------------------------------------------------------*/

Ticks
rb_pulser_j_double2ticks( double p_time )
{
    double ticks;


    if ( ! rb_pulser_j.is_timebase )
    {
        print( FATAL, "Can't set a time because no pulser time base has been "
               "set.\n" );
        THROW( EXCEPTION );
    }

    ticks = p_time / rb_pulser_j.timebase;

    if ( ticks > MAX_TICKS || ticks < - MAX_TICKS )
    {
        print( FATAL, "Specified time is too long for time base of %s.\n",
               rb_pulser_j_ptime( rb_pulser_j.timebase ) );
        THROW( EXCEPTION );
    }

    if (    fabs( ( Ticks_rnd( ticks ) - ticks ) / ticks ) > 1.0e-2
         || ( p_time > 0.99e-9 && Ticks_rnd( ticks ) == 0 ) )
    {
        print( FATAL, "Specified time of %s is not an integer multiple of the "
               "pulser time base of %s.\n",
               rb_pulser_j_ptime( p_time ),
               rb_pulser_j_ptime( rb_pulser_j.timebase ) );
        THROW( EXCEPTION );
    }

    return Ticks_rnd( ticks );
}


/*-----------------------------------------------------*
 * Does the exact opposite of the previous function...
 *-----------------------------------------------------*/

double
rb_pulser_j_ticks2double( Ticks ticks )
{
    fsc2_assert( rb_pulser_j.is_timebase );
    return rb_pulser_j.timebase * ticks;
}


/*------------------------------------------------------------------------*
 * Returns pointer to the pulses structure if given a valid pulse number.
 *------------------------------------------------------------------------*/

Pulse_T *
rb_pulser_j_get_pulse( long pnum )
{
    Pulse_T *cp = rb_pulser_j.pulses;


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
rb_pulser_j_ptime( double p_time )
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


/*----------------------------------------------------*
 *----------------------------------------------------*/

const char *
rb_pulser_j_pticks( Ticks ticks )
{
    return rb_pulser_j_ptime( rb_pulser_j_ticks2double( ticks ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rb_pulser_j_show_pulses( void )
{
    int pd[ 2 ];
    pid_t pid;


    if ( pipe( pd ) == -1 )
    {
        if ( errno == EMFILE || errno == ENFILE )
            print( FATAL, "Failure, running out of system resources.\n" );
        return;
    }

    if ( ( pid = fork( ) ) < 0 )
    {
        if ( errno == ENOMEM || errno == EAGAIN )
            print( FATAL, "Failure, running out of system resources.\n" );
        return;
    }

    /* Here's the child's code */

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
            execl( cmd, "fsc2_pulses", NULL );
        }

    filter_failure:

        if ( cmd != NULL )
            T_free( cmd );
        _exit( EXIT_FAILURE );
    }

    /* And finally the code for the parent */

    close( pd[ 0 ] );
    rb_pulser_j.show_file = fdopen( pd[ 1 ], "w" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rb_pulser_j_dump_pulses( void )
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
            if ( name != NULL )
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
                name = T_free( name );
                continue;
            }
            T_free( m );
        }

        if ( ( rb_pulser_j.dump_file = fopen( name, "w+" ) ) == NULL )
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

            name = T_free( name );
            continue;
        }
    } while ( rb_pulser_j.dump_file == NULL );

    T_free( name );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

void
rb_pulser_j_write_pulses( FILE * fp )
{
    Function_T *f;
    int i, j;


    if ( fp == NULL )
        return;

    fprintf( fp, "===\n" );

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_j.function + i;

        if ( ! f->is_used )
            continue;

        fprintf( fp, "%s: ", f->name );
        for ( j = 0; j < f->num_active_pulses; j++ )
            fprintf( fp, " %ld %ld %ld", f->pulses[ j ]->num,
                     Ticks_rnd( f->pulses[ j ]->pos / rb_pulser_j.timebase ),
                     f->delay_card->next != NULL ? f->pulses[ j ]->len  :
                     Ticks_rnd( f->last_pulse_len / rb_pulser_j.timebase ) );

        fprintf( fp, "\n" );
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
