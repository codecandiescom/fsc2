/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#include "rb_pulser_w.h"


/*-------------------------------------------------------------------*
 * Comparison function for two pulses: returns 0 if both pulses are
 * inactive, -1 if only the second pulse is inactive or starts at a
 * later time and 1 if only the first pulse is inactive pulse or the
 * second pulse starts earlier.
 *-------------------------------------------------------------------*/

int
rb_pulser_w_start_compare( const void * A,
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
 * called Ticks, i.e. an integer multiple of the pulsers timebase
 *-----------------------------------------------------------------*/

Ticks
rb_pulser_w_double2ticks( double p_time )
{
    double ticks;


    if ( ! rb_pulser_w.is_timebase )
    {
        print( FATAL, "Can't set a time because no pulser time base has been "
               "set.\n" );
        THROW( EXCEPTION );
    }

    ticks = p_time / rb_pulser_w.timebase;

    if ( ticks > MAX_TICKS || ticks < - MAX_TICKS )
    {
        print( FATAL, "Specified time is too long for time base of %s.\n",
               rb_pulser_w_ptime( rb_pulser_w.timebase ) );
        THROW( EXCEPTION );
    }

    if (    fabs( ( Ticks_rnd( ticks ) - ticks ) / ticks ) > 1.0e-2
         || ( p_time > 0.99e-9 && Ticks_rnd( ticks ) == 0 ) )
    {
        print( FATAL, "Specified time of %s is not an integer multiple of the "
               "pulser time base of %s.\n",
               rb_pulser_w_ptime( p_time ),
               rb_pulser_w_ptime( rb_pulser_w.timebase ) );
        THROW( EXCEPTION );
    }

    return Ticks_rnd( ticks );
}


/*-----------------------------------------------------*
 * Does the exact opposite of the previous function...
 *-----------------------------------------------------*/

double
rb_pulser_w_ticks2double( Ticks ticks )
{
    fsc2_assert( rb_pulser_w.is_timebase );
    return rb_pulser_w.timebase * ticks;
}


/*------------------------------------------------------------------------*
 * Returns pointer to the pulses structure if given a valid pulse number.
 *------------------------------------------------------------------------*/

Pulse_T *
rb_pulser_w_get_pulse( long pnum )
{
    Pulse_T *cp = rb_pulser_w.pulses;


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
 * Converts a time (passed to the function as a double variable) into
 * a well-formated string (i.e. with proper units for printing)
 *--------------------------------------------------------------------*/

const char *
rb_pulser_w_ptime( double p_time )
{
    static char buffer[ 2 ][ 128 ];
    static size_t i = 1;


    i = ( i + 1 ) % 2;

    if ( p_time == - 0.0 )
        p_time = 0.0;

    if ( fabs( p_time ) >= 1.0 )
        sprintf( buffer[ i ], "%g s", p_time );
    else if ( fabs( p_time ) >= 1.0e-3 )
        sprintf( buffer[ i ], "%g ms", 1.e3 * p_time );
    else if ( fabs( p_time ) >= 1.0e-6 )
        sprintf( buffer[ i ], "%g us", 1.0e6 * p_time );
    else
        sprintf( buffer[ i ], "%g ns", 1.0e9 * p_time );

    return buffer[ i ];
}


/*--------------------------------------------------------------------*
 * Converts a time passed to the function in Ticks of the pulser into
 * a well-formated time string (i.e. with proper units for printing)
 *--------------------------------------------------------------------*/

const char *
rb_pulser_w_pticks( Ticks ticks )
{
    return rb_pulser_w_ptime( rb_pulser_w_ticks2double( ticks ) );
}


/*------------------------------------------------------------*
 * Function for starting a process that graphically shows the
 * settings of the pulses as determined during the test run
 *------------------------------------------------------------*/

void
rb_pulser_w_start_show_pulses( void )
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
    rb_pulser_w.show_file = fdopen( pd[ 1 ], "w" );
}


/*------------------------------------------------------------------*
 * Function for opening the file for writing information about all
 * pulse settings as required by the function pulser_sump_pulses().
 *------------------------------------------------------------------*/

void
rb_pulser_w_open_dump_file( void )
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

        if ( ( rb_pulser_w.dump_file = fopen( name, "w+" ) ) == NULL )
        {
            switch( errno )
            {
                case EMFILE :
                    show_message( "Sorry, the program had has too many open "
                                  "files,\ncan't open another one!\n" );
                    break;

                case ENFILE :
                    show_message( "Sorry, system limit for open files "
                                  "exceeded!\nPlease try to close some "
                                  "files and retry." );
                break;

                case ENOSPC :
                    show_message( "Sorry, no space left on device for more "
                                  "file!\nPlease delete some files and "
                                  "retry." );
                    break;

                default :
                    show_message( "Sorry, can't open selected file for "
                                  "writing!\nPlease select a different "
                                  "file." );
            }

            name = T_free( name );
            continue;
        }
    } while ( rb_pulser_w.dump_file == NULL );

    T_free( name );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

void
rb_pulser_w_write_pulses( FILE * fp )
{
    Function_T *f;
    int i, j;
    const char *plist[ ] = { "+X", "-X", "+Y", "-Y" };


    if ( fp == NULL )
        return;

    fprintf( fp, "===\n" );

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_w.function + i;

        if ( ! f->is_used )
            continue;

        fprintf( fp, "%s: ", f->name );
        for ( j = 0; j < f->num_active_pulses; j++ )
        {
            fprintf( fp, " %ld %ld %ld", f->pulses[ j ]->num,
                     Ticks_rnd( f->pulses[ j ]->pos / rb_pulser_w.timebase ),
                     f->delay_card->next != NULL ? f->pulses[ j ]->len  :
                     Ticks_rnd( f->last_pulse_len / rb_pulser_w.timebase ) );
            if ( i == PULSER_CHANNEL_MW && rb_pulser_w.needs_phases )
            {
                if ( f->pulses[ j ]->pc == NULL )
                    fprintf( fp, " +X" );
                else
                    fprintf( fp, " %s", plist[ f->pulses[ j ]->pc->
                                        sequence[ rb_pulser_w.cur_phase ] ] );
            }
        }

        fprintf( fp, "\n" );
    }
}


/*-------------------------------------------------------------------*
 * Function for determining the earliest possible position for the
 * first microwave pulse or the minimum distance to the previous
 * pulse.
 *-------------------------------------------------------------------*/

double
rb_pulser_mw_min_specs( Pulse_T * p )
{
    Function_T *f = p->function;
    Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + MW_DELAY_0;
    Rulbus_Delay_Card_T *cur_card;
    double t;
    int i;
    double min =
            (   Ticks_ceil( rb_pulser_w.psd / rb_pulser_w.timebase )
              + Ticks_ceil( rb_pulser_w.grace_period / rb_pulser_w.timebase ) )
            * rb_pulser_w.timebase
            + rb_pulser_w.psd
            + rb_pulser_w.grace_period;


    fsc2_assert( f == rb_pulser_w.function + PULSER_CHANNEL_MW );

    cur_card = card;

    for ( i = 0; i < MAX_MW_PULSES; i++ )
    {
        fsc2_assert( cur_card != NULL && cur_card->next != NULL );

        t = cur_card->intr_delay + cur_card->next->intr_delay;

        if ( p == f->pulses[ i ] )
        {
            if ( i == 0 )
                t += rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay + f->delay;
            else
                t -= cur_card->intr_delay;
            break;
        }
    }

    if ( i == 0 )
    {
        if ( t <   rb_pulser_w.delay_card[ PHASE_DELAY_0 ].intr_delay
                 + rb_pulser_w.psd + MINIMUM_PHASE_PULSE_LENGTH )
            t =   rb_pulser_w.delay_card[ PHASE_DELAY_0 ].intr_delay
                + rb_pulser_w.psd
                + MINIMUM_PHASE_PULSE_LENGTH;
    }
    else
    {
        if ( t < min )
            t = min;
    }

    return t;
}


/*-----------------------------------------------------------------------*
 * Function for determining the earliest possible position for the first
 * RF pulse or the minimum separation to the previous one for the second
 *-----------------------------------------------------------------------*/

double
rb_pulser_rf_min_specs( Pulse_T * p )
{
    fsc2_assert( p->function == rb_pulser_w.function + PULSER_CHANNEL_RF );

    if ( p == p->function->pulses[ 0 ] )
        return   Ticks_ceil( (   rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay
                               + SYNTHESIZER_INTRINSIC_DELAY
                               + p->function->delay ) / rb_pulser_w.timebase )
               * rb_pulser_w.timebase;
    else
        return SYNTHESIZER_MIN_PULSE_SEPARATION;
}


/*---------------------------------------------------------*
 * Function for determining the earliest possible position
 * of the laser pulse
 *---------------------------------------------------------*/

double
rb_pulser_laser_min_specs( Pulse_T * p )
{
    fsc2_assert( p->function == rb_pulser_w.function + PULSER_CHANNEL_LASER );

    return
       Ticks_ceil( (   rb_pulser_w.delay_card[ ERT_DELAY     ].intr_delay
                     + rb_pulser_w.delay_card[ LASER_DELAY_0 ].intr_delay
                     + rb_pulser_w.delay_card[ LASER_DELAY_0 ].next->intr_delay
                     + p->function->delay ) / rb_pulser_w.timebase )
       * rb_pulser_w.timebase;
}


/*---------------------------------------------------------*
 * Function for determining the earliest possible position
 * of the detection pulse
 *---------------------------------------------------------*/

double
rb_pulser_det_min_specs( Pulse_T * p )
{
    fsc2_assert( p->function == rb_pulser_w.function + PULSER_CHANNEL_DET );

    return Ticks_ceil( (   rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay
                         + rb_pulser_w.delay_card[ DET_DELAY ].intr_delay
                         + p->function->delay ) / rb_pulser_w.timebase )
           * rb_pulser_w.timebase;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
