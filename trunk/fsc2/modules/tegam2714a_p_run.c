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


#include "tegam2714a_p.h"


static void tegam2714a_p_commit( void );

void tegam2714a_p_set_amplitude( double ampl );

/*-------------------------------------------------------------------------*
 * Function is called in the experiment after pulses have been changed to
 * update the pulser accordingly. No checking has to be done except in the
 * test run.
 *-------------------------------------------------------------------------*/

void tegam2714a_p_do_update( void )
{
    bool restart = UNSET;


    if ( ! tegam2714a_p.is_needed || ! tegam2714a_p.function.is_used )
        return;

    /* Set the amplitude to keep the device form outputting garbled pulse
       sequences while we're in the process of updating the pulses */

    if ( tegam2714a_p.is_running && FSC2_MODE == EXPERIMENT )
    {
        restart = SET;
        tegam2714a_p_set_amplitude( 0.0 );
    }

    /* Resort the pulses and check that the new pulse settings are reasonable,
       then commit all changes */

    tegam2714a_p_do_checks( UNSET );
    tegam2714a_p_commit( );

    /* During the test run we may have to write out information about the
       current pulse settings */

    if ( FSC2_MODE == TEST )
    {
        if ( tegam2714a_p.dump_file != NULL )
            tegam2714a_p_write_pulses( tegam2714a_p.dump_file );
        if ( tegam2714a_p.show_file != NULL )
            tegam2714a_p_write_pulses( tegam2714a_p.show_file );
    }

    if ( restart )
        tegam2714a_p_set_amplitude(   tegam2714a_p.function.high_level
                                    - tegam2714a_p.function.low_level );
}


/*-------------------------------------------------------------------------*
 * Function is called after pulses have been changed to check if the new
 * settings are still ok, i.e. that they still fit into the pulsers memory
 * and don't overlap.
 *-------------------------------------------------------------------------*/

void tegam2714a_p_do_checks( bool in_setup )
{
    Function_T *f = &tegam2714a_p.function;
    Pulse_T *p;
    int i;


    /* Sort the pulses according to their start positions (pulses with
       zero length end up at the end of the list!) */

    qsort( f->pulses, f->num_pulses, sizeof * f->pulses,
           tegam2714a_p_start_compare );

    /* Check that the active pulses fit into the memory and don't overlap */

    for ( i = 0; i < f->num_pulses; i++ )
    {
        p = f->pulses[ i ];

        /* Check that pulse still fits into the pulser s memory */

        if ( p->is_active )
        {
            f->max_seq_len = Ticks_max( f->max_seq_len, p->pos + p->len );
            if ( f->max_seq_len + f->delay >
                ( FSC2_MODE == TEST ?
                                 MAX_PULSER_BITS : tegam2714a_p.max_seq_len ) )
            {
                if ( FSC2_MODE == TEST )
                    print( FATAL, "Pulse sequence for function '%s' does not "
                           "fit into the pulsers memory.\n", f->name );
                else
                    print( FATAL, "Pulse sequence for function '%s' is too "
                           "long.\n", f->name );
                THROW( EXCEPTION );
            }

            f->num_active_pulses = i + 1;
        }

        /* Check that pulse doesn't overlap with the next one - if their
           distance becomes zero print out a warning in the test run */

        if ( i + 1 < f->num_pulses && f->pulses[ i + 1 ]->is_active )
        {
            if ( p->pos + p->len > f->pulses[ i + 1 ]->pos )
            {
                if ( in_setup )
                    print( FATAL, "Pulses %ld and %ld overlap.\n", p->num,
                           f->pulses[ i + 1 ]->num );
                else
                    print( FATAL, "Pulses %ld and %ld start to overlap.\n",
                           p->num, f->pulses[ i + 1 ]->num );
                THROW( EXCEPTION );
            }
            else if ( p->pos + p->len == f->pulses[ i + 1 ]->pos )
            {
                if ( in_setup )
                    print( SEVERE, "Distance between pulses %ld and %ld is "
                           "zero.\n", p->num, f->pulses[ i + 1 ]->num );
                else
                    print( SEVERE, "Distance between pulses %ld and %ld "
                           "becomes zero.\n",
                           p->num, f->pulses[ i + 1 ]->num );
            }
        }
    }

    if ( FSC2_MODE == TEST )
        tegam2714a_p.max_seq_len = Ticks_max( tegam2714a_p.max_seq_len,
                                              f->max_seq_len + f->delay );
}


/*---------------------------------------------------------------------*
 * Function creates all active pulses when the experiment gets started
 *---------------------------------------------------------------------*/

void tegam2714a_p_set_pulses( void )
{
    Pulse_T *p;


    /* Get some memory for comparing the current and the new stat of the
       pulsers memory in later updates (used in order to minimze the number
       of commands to be send to the pulser) */

    tegam2714a_p.old_arena = CHAR_P T_malloc( 2 * tegam2714a_p.max_seq_len );
    tegam2714a_p.new_arena = tegam2714a_p.old_arena + tegam2714a_p.max_seq_len;
    memset( tegam2714a_p.new_arena, 0, tegam2714a_p.max_seq_len );

    /* Set up the pulser as well as the representation of the pulsers
       memory */

    tegam2714a_p_set_constant( 0, tegam2714a_p.max_seq_len, 0 );

    for ( p = tegam2714a_p.pulses; p != NULL; p = p->next )
        if ( p->is_active )
        {
			tegam2714a_p_set( tegam2714a_p.new_arena, p->pos, p->len,
                              p->function->delay );
            tegam2714a_p_set_constant( p->pos + p->function->delay,
                                       p->len, 1 );
        }
}


/*-------------------------------------------------------------------*
 * Function is called after the test run to reset all the variables
 * describing the state of the pulser to their initial values
 *-------------------------------------------------------------------*/

void tegam2714a_p_full_reset( void )
{
    Pulse_T *p;


    for ( p = tegam2714a_p.pulses; p != NULL; p = p->next )
    {
        /* First we check if the pulse has been used at all print a warning
           otherwise */

        if ( ! p->has_been_active )
        {
            print( WARN, "Pulse %ld is never used.\n", p->num );
            continue;
        }

        /* Reset pulse properties to their initial values */

        p->pos = p->initial_pos;
        p->is_pos = p->initial_is_pos;
        p->len = p->initial_len;
        p->is_len = p->initial_is_len;
        p->dpos = p->initial_dpos;
        p->is_dpos = p->initial_is_dpos;
        p->dlen = p->initial_dlen;
        p->is_dlen = p->initial_is_dlen;

        p->is_old_pos = p->is_old_len = UNSET;
        p->is_active = ( p->is_pos && p->is_len && p->len > 0 );
    }
}


/*--------------------------------------------------------------------*
 * Changes the pulse pattern so that the data in the pulser are again
 * in sync with the its internal representation. Some care has taken
 * to minimize the number of commands and their length.
 *--------------------------------------------------------------------*/

void tegam2714a_p_commit( void )
{
    Pulse_T *p;
    Ticks start, len;
    int what;


    /* In a test run just reset the flags of all pulses that say that they
       have and old value for the position and length. i.e. got changed */

    if ( FSC2_MODE == TEST )
    {
        for ( p = tegam2714a_p.pulses; p != NULL; p = p->next )
            p->is_old_pos = p->is_old_len = UNSET;
        return;
    }

    /* In a real run we now have to change the pulses. To keep the amount of
       data to be sent to the device to a minimum we just send commands for the
       bits in the pulsers memory that needs changing. */

    memcpy( tegam2714a_p.old_arena, tegam2714a_p.new_arena,
            tegam2714a_p.max_seq_len );
    memset( tegam2714a_p.new_arena, 0, tegam2714a_p.max_seq_len );

    /* Now loop over all pulses and set the bits for the new settings of the
       pulses */

    for ( p = tegam2714a_p.pulses; p != NULL; p = p->next )
    {
        if ( p->is_active )
			tegam2714a_p_set( tegam2714a_p.new_arena, p->pos, p->len,
                              p->function->delay );
        p->is_old_len = p->is_old_pos = UNSET;
    }

    /* Find all differences between the old and the new state by repeatedly
       calling tegam2714a_p_diff() - it returns +1 or -1 for setting or
       resetting plus the start and length of the different area or 0 if no
       differences are found anymore. For each difference set the real pulser
       channel accordingly. */

    while ( ( what = tegam2714a_p_diff( tegam2714a_p.old_arena,
                                        tegam2714a_p.new_arena,
                                        &start, &len ) ) != 0 )
        tegam2714a_p_set_constant( start, len, what == -1 ? 0 : 1 );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
