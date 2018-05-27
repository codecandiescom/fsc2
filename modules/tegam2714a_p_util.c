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


#include "tegam2714a_p.h"


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
tegam2714a_p_check_levels( double high,
                           double low )
{
    double ampl;
    double offset;


    ampl = 2.0 * ( high - low );

    if ( ! tegam2714a_p.function.is_inverted )
        offset = low;
    else
        offset = high;

    if ( high < low )
    {
        print( FATAL, "Low voltage level isn't below high level, use keyword "
               "INVERT to invert the polarity.\n" );
        THROW( EXCEPTION );
    }

    if ( ampl > MAX_AMPLITUDE )
    {
        print( FATAL, "Requested pulse amplitude too large, maximum amplitude "
               "is %g V.\n", MAX_AMPLITUDE );
        THROW( EXCEPTION );
    }

    /* The device has three ranges with different maximum offsets */

    if ( ampl > 0.999 )
    {
        if ( ampl + fabs( offset ) > MAX_AMPLITUDE )
        {
            print( FATAL, "With the selected pulse level difference the low "
                   "voltage level can't be higher than %g V.\n",
                   MAX_AMPLITUDE - ampl );
            THROW( EXCEPTION );
        }
    }
    else if ( ampl > 0.099 )
    {
        if ( ampl + fabs( offset ) > 1.0 )
        {
            print( FATAL, "With the selected pulse level difference the low "
                   "voltage level must be in the +/%g V range.\n",
                   1.0 - ampl );
            THROW( EXCEPTION );
        }
    }
    else if ( ampl + fabs( offset ) > 0.1 )
    {
        print( FATAL, "With the selected pulse level difference the low "
               "voltage level must be in the +%g mV range.\n",
               100.0 * ( 1.0 - ampl ) );
        THROW( EXCEPTION );
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

const char *
tegam2714a_p_ptime( double p_time )
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
tegam2714a_p_pticks( Ticks ticks )
{
    return tegam2714a_p_ptime( tegam2714a_p_ticks2double( ticks ) );
}


/*-----------------------------------------------------------------*
 * Converts a time into the internal type of a time specification,
 * i.e. an integer multiple of the time base
 *-----------------------------------------------------------------*/

Ticks
tegam2714a_p_double2ticks( double p_time )
{
    double ticks;


    if ( ! tegam2714a_p.is_timebase )
    {
        print( FATAL, "Can't set a time because no pulser time base has been "
               "set.\n" );
        THROW( EXCEPTION );
    }

    ticks = p_time / tegam2714a_p.timebase;

    if ( ticks > MAX_PULSER_BITS )
    {
        print( FATAL, "Specified time is too long for time base of %s.\n",
               tegam2714a_p_ptime( tegam2714a_p.timebase ) );
        THROW( EXCEPTION );
    }

    if (    fabs( Ticksrnd( ticks ) - p_time / tegam2714a_p.timebase ) > 1.0e-2
         || ( p_time > 0.99e-9 && Ticksrnd( ticks ) == 0 ) )
    {
        print( FATAL, "Specified time of %s is not an integer multiple of the "
               "pulser time base of %s.\n",
               tegam2714a_p_ptime( p_time ),
               tegam2714a_p_ptime( tegam2714a_p.timebase ) );
        THROW( EXCEPTION );
    }

    return Ticksrnd( ticks );
}


/*-----------------------------------------------------*
 * Does the exact opposite of the previous function...
 *-----------------------------------------------------*/

double
tegam2714a_p_ticks2double( Ticks ticks )
{
    fsc2_assert( tegam2714a_p.is_timebase );
    return tegam2714a_p.timebase * ticks;
}


/*-----------------------------------------------*
 * Calculates the number of digits in a long int
 *-----------------------------------------------*/

int
tegam2714a_p_num_len( long num )
{
    int cnt;

    for ( cnt = 1; num > 9; num /= 10, cnt++ )
        /* empty */ ;

    return cnt;
}


/*-----------------------------------------------*
 * Returns the structure for pulse numbered pnum
 *-----------------------------------------------*/

Pulse_T *
tegam2714a_p_get_pulse( long pnum )
{
    Pulse_T *cp = tegam2714a_p.pulses;


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


/*--------------------------------------------------------------------*
 * Comparison function for two pulses: returns 0 if both pulses are
 * inactive, -1 if only the second pulse is inactive or starts at a
 * later time and 1 if only the first pulse is inactive pulse or the
 * second pulse starts earlier.
 *--------------------------------------------------------------------*/

int
tegam2714a_p_start_compare( const void * A,
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


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
tegam2714a_p_set( char * arena,
                  Ticks  start,
                  Ticks  len,
                  Ticks  offset )
{
    fsc2_assert( start + len + offset <= tegam2714a_p.max_seq_len );

    memset( arena + offset + start, SET, len );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

int
tegam2714a_p_diff( char *  old_p,
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

    while ( where < tegam2714a_p.max_seq_len && *a == *b )
    {
        where++;
        a++;
        b++;
    }

    /* If none was found anymore */

    if ( where == tegam2714a_p.max_seq_len )
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
    while ( where < tegam2714a_p.max_seq_len && *a != *b && *a == cur_state )
    {
        where++;
        a++;
        b++;
        ( *length )++;
    }

    /* Set a marker that we reached the end of the arena */

    if ( where == tegam2714a_p.max_seq_len )
        where = -1;

    return ret;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
tegam2714a_p_dump_pulses( void )
{
    char *name;
    char *m;
    struct stat stat_buf;


    do
    {
        TRY
        {
            name = T_strdup( fsc2_show_fselector( "File for dumping pulses:",
                                                  NULL, "*.pls", NULL ) );
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

        if ( ( tegam2714a_p.dump_file = fopen( name, "w+" ) ) == NULL )
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
    } while ( tegam2714a_p.dump_file == NULL );

    T_free( name );
}


/*------------------------------------------------------------*
 * Function for starting a process that graphically shows the
 * settings of the pulses as determined during the test run
 *------------------------------------------------------------*/

void
tegam2714a_p_show_pulses( void )
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
        char * volatile cmd = NULL;

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
    tegam2714a_p.show_file = fdopen( pd[ 1 ], "w" );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

void
tegam2714a_p_write_pulses( FILE * fp )
{
    Function_T *f = &tegam2714a_p.function;
    int k;


    if ( fp == NULL )
        return;

    fprintf( fp, "===\n" );

    if ( ! f->is_used )
            return;

    fprintf( fp, "%s:%d", f->name, tegam2714a_p.function.channel );
    for ( k = 0; k < f->num_active_pulses; k++ )
        fprintf( fp, " %ld %ld %ld",
                 f->pulses[ k ]->num,
                 f->pulses[ k ]->pos,
                 f->pulses[ k ]->len );
    fprintf( fp, "\n" );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
