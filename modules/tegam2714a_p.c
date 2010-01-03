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


#include "tegam2714a_p.h"


/*--------------------------------*
 * global variables of the module *
 *--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


TEGAM2714A_P_T tegam2714a_p;


/*------------------------------------------------------------------------*
 * Function that gets called immediately after the module has been loaded
 *------------------------------------------------------------------------*/

int
tegam2714a_p_init_hook( void )
{
    Function_T *f = &tegam2714a_p.function;


    Pulser_Struct.name     = DEVICE_NAME;
    Pulser_Struct.has_pods = UNSET;

    /* Set global variable to indicate that Rulbus is needed */

#if ! defined TEGAM2714A_P_TEST
    Need_GPIB = SET;
#endif

    Pulser_Struct.set_timebase = tegam2714a_p_store_timebase;
    Pulser_Struct.set_trigger_mode = tegam2714a_p_set_trigger_mode;
    Pulser_Struct.set_repeat_time = NULL;
    Pulser_Struct.set_trig_in_slope = NULL;
    Pulser_Struct.set_function_delay = tegam2714a_p_set_function_delay;

    Pulser_Struct.new_pulse = tegam2714a_p_new_pulse;
    Pulser_Struct.set_pulse_function = tegam2714a_p_set_pulse_function;
    Pulser_Struct.set_pulse_position = tegam2714a_p_set_pulse_position;
    Pulser_Struct.set_pulse_length = tegam2714a_p_set_pulse_length;
    Pulser_Struct.set_pulse_position_change =
                                        tegam2714a_p_set_pulse_position_change;
    Pulser_Struct.set_pulse_length_change =
                                          tegam2714a_p_set_pulse_length_change;

    Pulser_Struct.get_pulse_function = tegam2714a_p_get_pulse_function;
    Pulser_Struct.get_pulse_position = tegam2714a_p_get_pulse_position;
    Pulser_Struct.get_pulse_length = tegam2714a_p_get_pulse_length;
    Pulser_Struct.get_pulse_position_change =
                                        tegam2714a_p_get_pulse_position_change;
    Pulser_Struct.get_pulse_length_change =
                                          tegam2714a_p_get_pulse_length_change;

    Pulser_Struct.assign_channel_to_function =
                                       tegam2714a_p_assign_channel_to_function;
    Pulser_Struct.invert_function = tegam2714a_p_invert_function;
    Pulser_Struct.set_max_seq_len = tegam2714a_p_set_max_seq_len;
    Pulser_Struct.set_function_high_level =
                                          tegam2714a_p_set_function_high_level;
    Pulser_Struct.set_function_low_level = tegam2714a_p_set_function_low_level;

    tegam2714a_p.is_needed = SET;
    tegam2714a_p.is_timebase = UNSET;
    tegam2714a_p.is_running = UNSET;
    tegam2714a_p.max_seq_len = MIN_PULSER_BITS;
    tegam2714a_p.is_max_seq_len = UNSET;
    tegam2714a_p.max_seq_len = MIN_PULSER_BITS;
    tegam2714a_p.old_arena = NULL;

    tegam2714a_p.dump_file = NULL;
    tegam2714a_p.show_file = NULL;
    tegam2714a_p.do_show_pulses = UNSET;
    tegam2714a_p.do_dump_pulses = UNSET;

    f->self = PULSER_CHANNEL_NO_TYPE;
    f->num_pulses = 0;
    f->num_active_pulses = 0;
    f->channel = DEFAULT_WAVEFORM_NUMBER;
    f->is_channel = UNSET;
    f->pulses = NULL;
    f->delay = 0.0;
    f->is_delay = UNSET;
    f->is_used = UNSET;
    f->max_seq_len = 0;
    f->is_high_level = UNSET;
    f->is_low_level = UNSET;
    f->is_inverted = UNSET;

    return 1;
}


/*----------------------------------------------------------*
 * Function gets called just before the test run is started
 *----------------------------------------------------------*/

int
tegam2714a_p_test_hook( void )
{
    /* Check consistency of pulse settings and do everything to setup the
       pulser for the test run */

    TRY
    {
        if ( tegam2714a_p.do_show_pulses )
            tegam2714a_p_show_pulses( );
        if ( tegam2714a_p.do_dump_pulses )
            tegam2714a_p_dump_pulses( );
        tegam2714a_p_init_setup( );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( tegam2714a_p.dump_file )
        {
            fclose( tegam2714a_p.dump_file );
            tegam2714a_p.dump_file = NULL;
        }

        if ( tegam2714a_p.show_file )
        {
            fclose( tegam2714a_p.show_file );
            tegam2714a_p.show_file = NULL;
        }

        RETHROW( );
    }

    /* After the initial setup is done we need some different functions for
       the setting of pulse properties */

    Pulser_Struct.set_pulse_position = tegam2714a_p_change_pulse_position;
    Pulser_Struct.set_pulse_length = tegam2714a_p_change_pulse_length;
    Pulser_Struct.set_pulse_position_change =
                                     tegam2714a_p_change_pulse_position_change;
    Pulser_Struct.set_pulse_length_change =
                                       tegam2714a_p_change_pulse_length_change;

    if ( tegam2714a_p.pulses == NULL )
        tegam2714a_p.is_needed = UNSET;

    tegam2714a_p.requested_max_seq_len = tegam2714a_p.is_max_seq_len ?
                                    tegam2714a_p.max_seq_len : MAX_PULSER_BITS;

    return 1;
}


/*----------------------------------------------------------------------*
 * Function called after the end of the test run, mostly used to finish
 * output for the functions for displaying and/or logging what pulses
 * have been set to during the experiment
 *----------------------------------------------------------------------*/

int
tegam2714a_p_end_of_test_hook( void )
{
    if ( ! tegam2714a_p.is_needed )
        return 1;

    if ( tegam2714a_p.dump_file != NULL )
    {
        fclose( tegam2714a_p.dump_file );
        tegam2714a_p.dump_file = NULL;
    }

    if ( tegam2714a_p.show_file != NULL )
    {
        fclose( tegam2714a_p.show_file );
        tegam2714a_p.show_file = NULL;
    }

    /* Check that the either both pulse levels are set or none at all */

    if (    (    tegam2714a_p.function.is_high_level
              && ! tegam2714a_p.function.is_low_level )
         || (    ! tegam2714a_p.function.is_high_level
              && tegam2714a_p.function.is_low_level ) )
    {
        print( FATAL, "Only high or low pulse level has been set, either set "
               "both or none.\n" );
        THROW( EXCEPTION );
    }

    if (    tegam2714a_p.function.is_high_level
         && tegam2714a_p.function.is_low_level )
        tegam2714a_p_check_levels( tegam2714a_p.function.high_level,
                                   tegam2714a_p.function.low_level );

    /* Tell the user if the requested maximum pattern length wasn't long
       enough to create all pulses during the test run */

    if (    tegam2714a_p.is_max_seq_len
         && tegam2714a_p.max_seq_len > tegam2714a_p.requested_max_seq_len )
        print( SEVERE, "Maximum pattern length had to be adjusted from %s "
               "to %s.\n",
               tegam2714a_p_pticks( tegam2714a_p.requested_max_seq_len ),
               tegam2714a_p_pticks( tegam2714a_p.max_seq_len ) );

    return 1;
}


/*---------------------------------------------------------------*
 * Function gets called at the very start of the experiment - it
 * got to initialize the pulser and, if required, start it
 *---------------------------------------------------------------*/

int
tegam2714a_p_exp_hook( void )
{
    if ( ! tegam2714a_p.is_needed )
        return 1;

    /* Initialize the device */

    tegam2714a_p_init( device_name );

    return 1;
}


/*----------------------------------------------*
 * Function called at the end of the experiment
 *----------------------------------------------*/

int
tegam2714a_p_end_of_exp_hook( void )
{
    if ( ! tegam2714a_p.is_needed )
        return 1;

    if ( tegam2714a_p.old_arena )
        tegam2714a_p.old_arena = T_free( tegam2714a_p.old_arena );

    tegam2714a_p_run( STOP );

    return 1;
}


/*---------------------------------------------------------------*
 * Function called just before the modules gets unloaded, mainly
 * used to get rid of memory allocated for the module
 *---------------------------------------------------------------*/

void
tegam2714a_p_exit_hook( void )
{
    Pulse_T *p, *pn;
    Function_T *f = &tegam2714a_p.function;


    if ( ! tegam2714a_p.is_needed )
        return;

    /* Free all memory allocated within the module */

    f->pulses = T_free( f->pulses );

    for ( p = tegam2714a_p.pulses; p != NULL; p = pn )
    {
        pn = p->next;
        T_free( p );
    }
}


/*-------------------------------------------*
 * EDL function that returns the device name
 *-------------------------------------------*/

Var_T *
pulser_name( Var_T *v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*
 * EDL function for getting the positions and lengths
 * of all pulses used during the experiment displayed
 *----------------------------------------------------*/

Var_T *
pulser_show_pulses( Var_T *v  UNUSED_ARG )
{
    if ( ! FSC2_IS_CHECK_RUN && ! FSC2_IS_BATCH_MODE )
        tegam2714a_p.do_show_pulses = SET;

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------------*
 * Similar to the previous functions, but instead the states
 * of all pulses during the experiment gets written to a file
 *------------------------------------------------------------*/

Var_T *
pulser_dump_pulses( Var_T *v  UNUSED_ARG )
{
    if ( ! FSC2_IS_CHECK_RUN && ! FSC2_IS_BATCH_MODE )
        tegam2714a_p.do_dump_pulses = SET;

    return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------*
 * Switches the output of the pulser on or off
 *---------------------------------------------*/

Var_T *
pulser_state( Var_T *v )
{
    bool state;


    if ( v == NULL )
        return vars_push( INT_VAR, ( long ) tegam2714a_p.is_running );

    state = get_boolean( v );

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR,
                          ( long ) ( tegam2714a_p.is_running = state ) );

    tegam2714a_p_run( state );
    return vars_push( INT_VAR, ( long ) tegam2714a_p.is_running );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_maximum_pattern_length( Var_T * v )
{
    double pl;


    pl = get_double( v, "maximum pattern length" );
    is_mult_ns( pl, "Maximum pattern length" );
    tegam2714a_p_set_max_seq_len( pl );
    return vars_push( FLOAT_VAR,
                      tegam2714a_p.max_seq_len * tegam2714a_p.timebase );
}


/*---------------------------------------------------------*
 * Function that must be called to commit all changes made
 * to the pulses - without calling the function only the
 * internal representation of the pulser is changed but
 * not the state of the "real" pulser
 *---------------------------------------------------------*/

Var_T *
pulser_update( Var_T *v UNUSED_ARG )
{
    if ( ! tegam2714a_p.is_needed )
        return vars_push( INT_VAR, 1L );

    /* Send all changes to the pulser */

    tegam2714a_p_do_update( );

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------------*
 * Public function to change the position of pulses. If called with no
 * argument all active pulses that have a position change time set will
 * be moved, otherwise all pulses passed as arguments to the function.
 * Take care: The changes will only be committed on the next call of
 *            the function pulser_update() !
 *----------------------------------------------------------------------*/

Var_T *
pulser_shift( Var_T *v )
{
    Pulse_T *p;


    if ( ! tegam2714a_p.is_needed )
        return vars_push( INT_VAR, 1L );

    /* An empty pulse list means that we have to shift all active pulses that
       have a position change time value set */

    if ( v == NULL )
        for ( p = tegam2714a_p.pulses; p != NULL; p = p->next )
            if ( p->num >= 0 && p->is_active && p->is_dpos )
                vars_pop( pulser_shift( vars_push( INT_VAR, p->num ) ) );

    /* Otherwise run through the supplied pulse list */

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        p = tegam2714a_p_get_pulse( get_strict_long( v, "pulse number" ) );

        if ( ! p->is_pos )
        {
            print( FATAL, "Pulse #%ld has no position set, so shifting it "
                   "isn't possible.\n", p->num );
            THROW( EXCEPTION );
        }

        if ( ! p->is_dpos )
        {
            print( FATAL, "Amount of position change hasn't been defined for "
                   "pulse #%ld.\n", p->num );
            THROW( EXCEPTION );
        }

        p->pos += p->dpos;

        p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
    }

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------------------------*
 * Public function for incrementing the length of pulses. If called with
 * no argument all active pulses that have a length change defined are
 * incremented, otherwise all pulses passed as arguments to the function.
 *------------------------------------------------------------------------*/

Var_T *
pulser_increment( Var_T *v )
{
    Pulse_T *p;


    if ( ! tegam2714a_p.is_needed )
        return vars_push( INT_VAR, 1L );

    /* An empty pulse list means that we have to increment all active pulses
       that have a length change time value set */

    if ( v == NULL )
        for ( p = tegam2714a_p.pulses; p != NULL; p = p->next )
            if ( p->num >= 0 && p->is_active && p->is_dlen )
                vars_pop( pulser_increment( vars_push( INT_VAR, p->num ) ) );

    /* Otherwise run through the supplied pulse list */

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        p = tegam2714a_p_get_pulse( get_strict_long( v, "pulse number" ) );

        if ( ! p->is_len )
        {
            print( FATAL, "Pulse #%ld has no length set, so incrementing its "
                   "length isn't possible.\n", p->num );
            THROW( EXCEPTION );
        }

        if ( ! p->is_dlen )
        {
            print( FATAL, "Length change time hasn't been defined for pulse "
                   "#%ld.\n", p->num );
            THROW( EXCEPTION );
        }

        if ( ( p->len += p->dlen ) < 0 )
        {
            print( FATAL, "Incrementing the length of pulse #%ld leads to an "
                   "invalid negative pulse length of %s.\n",
                   p->num, tegam2714a_p_pticks( p->len ) );
            THROW( EXCEPTION );
        }

        p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
    }

    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------*
 * Function for resetting the pulser back to its initial state
 *-------------------------------------------------------------*/

Var_T *
pulser_reset( Var_T *v UNUSED_ARG )
{
    if ( ! tegam2714a_p.is_needed )
        return vars_push( INT_VAR, 1L );

    vars_pop( pulser_pulse_reset( NULL ) );

    return pulser_update( NULL );
}


/*---------------------------------------------------------------------*
 * Function for resetting one or more pulses back to the initial state
 *---------------------------------------------------------------------*/

Var_T *
pulser_pulse_reset( Var_T *v )
{
    Pulse_T *p;


    if ( ! tegam2714a_p.is_needed )
        return vars_push( INT_VAR, 1L );

    /* An empty pulse list means that we have to reset all pulses (even the
       inactive ones). Don't explicitely reset automatically created pulses
       (which all have a negative pulse number), they will be reset together
       with the pulses belong to. */

    if ( v == NULL )
    {
        for ( p = tegam2714a_p.pulses; p != NULL; p = p->next )
            if ( p->num >= 0 )
                vars_pop( pulser_pulse_reset( vars_push( INT_VAR, p->num ) ) );
    }

    /* Otherwise run through the supplied pulse list */

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        p = tegam2714a_p_get_pulse( get_strict_long( v, "pulse number" ) );

        /* Reset all changeable properties back to their initial values */

        p->pos = p->initial_pos;
        p->is_pos = p->initial_is_pos;

        p->len = p->initial_len;
        p->is_len = p->initial_is_len;

        if ( p->initial_is_dpos )
            p->dpos = p->initial_dpos;
        p->is_dpos = p->initial_is_dpos;

        if ( p->initial_is_dlen )
            p->dlen = p->initial_dlen;
        p->is_dlen = p->initial_is_dlen;

        p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
    }

    return vars_push( INT_VAR, 1L );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
