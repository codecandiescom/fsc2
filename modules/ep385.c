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


#include "ep385.h"


/*--------------------------------*/
/* global variables of the module */
/*--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


EP385_T ep385;
static bool In_reset = UNSET;


/*-------------------------------------------------------------------------*
 * This function is called directly after all modules are loaded. Its main
 * function is to initialize all global variables that are needed in the
 * module.
 *-------------------------------------------------------------------------*/

int
ep385_init_hook( void )
{
    int i, j;
    Function_T *f;
    Channel_T *ch;


    fsc2_assert(    SHAPE_2_DEFENSE_DEFAULT_MIN_DISTANCE > 0
                 && DEFENSE_2_SHAPE_DEFAULT_MIN_DISTANCE > 0 );

    Pulser_Struct.name     = DEVICE_NAME;
    Pulser_Struct.has_pods = UNSET;

    /* Set global variable to indicate that GPIB bus is needed */

#ifndef EP385_GPIB_DEBUG
    Need_GPIB = SET;
#endif

    /* We have to set up the global structure for the pulser, especially the
       pointers for the functions that will get called from pulser.c */

    ep385.needs_update = SET;
    ep385.keep_all     = UNSET;

    Pulser_Struct.set_timebase = ep385_store_timebase;
    Pulser_Struct.assign_function = NULL;
    Pulser_Struct.assign_channel_to_function =
                                              ep385_assign_channel_to_function;
    Pulser_Struct.invert_function = NULL;
    Pulser_Struct.set_function_delay = ep385_set_function_delay;
    Pulser_Struct.set_function_high_level = NULL;
    Pulser_Struct.set_function_low_level = NULL;

    Pulser_Struct.set_trigger_mode = ep385_set_trigger_mode;
    Pulser_Struct.set_repeat_time = ep385_set_repeat_time;
    Pulser_Struct.set_trig_in_level = NULL;
    Pulser_Struct.set_trig_in_slope = NULL;
    Pulser_Struct.set_trig_in_impedance = NULL;
    Pulser_Struct.set_max_seq_len = ep385_set_max_seq_len;

    Pulser_Struct.set_phase_reference = ep385_set_phase_reference;

    Pulser_Struct.new_pulse = ep385_new_pulse;
    Pulser_Struct.set_pulse_function = ep385_set_pulse_function;
    Pulser_Struct.set_pulse_position = ep385_set_pulse_position;
    Pulser_Struct.set_pulse_length = ep385_set_pulse_length;
    Pulser_Struct.set_pulse_position_change = ep385_set_pulse_position_change;
    Pulser_Struct.set_pulse_length_change = ep385_set_pulse_length_change;
    Pulser_Struct.set_pulse_phase_cycle = ep385_set_pulse_phase_cycle;
    Pulser_Struct.set_grace_period = NULL;

    Pulser_Struct.get_pulse_function = ep385_get_pulse_function;
    Pulser_Struct.get_pulse_position = ep385_get_pulse_position;
    Pulser_Struct.get_pulse_length = ep385_get_pulse_length;
    Pulser_Struct.get_pulse_position_change = ep385_get_pulse_position_change;
    Pulser_Struct.get_pulse_length_change = ep385_get_pulse_length_change;
    Pulser_Struct.get_pulse_phase_cycle = ep385_get_pulse_phase_cycle;

    Pulser_Struct.phase_setup_prep = ep385_phase_setup_prep;
    Pulser_Struct.phase_setup = ep385_phase_setup;

    Pulser_Struct.set_phase_switch_delay = NULL;

    Pulser_Struct.keep_all_pulses = ep385_keep_all;

    /* Finally, we initialize variables that store the state of the pulser */

    ep385.in_setup = UNSET;
    ep385.is_trig_in_mode = UNSET;
    ep385.is_repeat_time = UNSET;
    ep385.is_neg_delay = UNSET;
    ep385.neg_delay = 0;
    ep385.is_running = SET;
    ep385.has_been_running = SET;
    ep385.is_timebase = UNSET;
    ep385.timebase_mode = EXTERNAL;

    ep385.trig_in_mode = INTERNAL;
    ep385.repeat_time = MIN_REPEAT_TIMES;

    ep385.dump_file = NULL;
    ep385.show_file = NULL;

    ep385.do_show_pulses = UNSET;
    ep385.do_dump_pulses = UNSET;

    ep385.is_shape_2_defense = UNSET;
    ep385.is_defense_2_shape = UNSET;
    ep385.shape_2_defense_too_near = 0;
    ep385.defense_2_shape_too_near = 0;
    ep385.is_confirmation = UNSET;

    ep385.auto_shape_pulses = UNSET;
    ep385.left_shape_warning = ep385.right_shape_warning = 0;

    ep385.auto_twt_pulses = UNSET;
    ep385.left_twt_warning = ep385.right_twt_warning = 0;

    ep385.is_minimum_twt_pulse_distance = UNSET;

    for ( i = 0; i < MAX_CHANNELS; i++ )
    {
        ch = ep385.channel + i;
        ch->self = i;
        ch->function = NULL;
        ch->pulse_params = NULL;
        ch->num_pulses = 0;
        ch->num_active_pulses = 0;
        ch->needs_update = SET;
    }

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = ep385.function + i;
        f->self = i;
        f->name = Function_Names[ i ];
        f->is_used = UNSET;
        f->is_needed = UNSET;
        f->num_pulses = 0;
        f->pulses = NULL;
        f->num_channels = 0;
        f->pm = NULL;
        f->delay = 0;
        f->is_delay = UNSET;
        f->next_phase = 0;
        f->uses_auto_shape_pulses = UNSET;
        f->uses_auto_twt_pulses = UNSET;
        f->max_seq_len = 0;
        for ( j = 0; j <= MAX_CHANNELS; j++ )
            f->channel[ j ] = NULL;
        f->max_duty_warning = 0;
    }

    ep385.is_needed = SET;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
ep385_test_hook( void )
{
    /* Make sure that a timebase is set (shouldn't really be needed) */

    ep385_timebase_check( );

    /* Check consistency of pulse settings and do everything to setup the
       pulser for the test run */

    TRY
    {
        if ( ep385.do_show_pulses )
            ep385_show_pulses( );
        if ( ep385.do_dump_pulses )
            ep385_dump_pulses( );

        ep385.in_setup = SET;
        ep385_init_setup( );
        ep385.in_setup = UNSET;
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        ep385.in_setup = UNSET;
        if ( ep385.dump_file )
        {
            fclose( ep385.dump_file );
            ep385.dump_file = NULL;
        }

        if ( ep385.show_file )
        {
            fclose( ep385.show_file );
            ep385.show_file = NULL;
        }

        RETHROW;
    }

    /* We need some somewhat different functions (or disable some) for
       setting the pulse properties */

    Pulser_Struct.set_pulse_function = NULL;

    Pulser_Struct.set_pulse_position = ep385_change_pulse_position;
    Pulser_Struct.set_pulse_length = ep385_change_pulse_length;
    Pulser_Struct.set_pulse_position_change =
                                            ep385_change_pulse_position_change;
    Pulser_Struct.set_pulse_length_change = ep385_change_pulse_length_change;

    if ( ep385.pulses == NULL )
        ep385.is_needed = UNSET;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
ep385_end_of_test_hook( void )
{
    int i;
    Function_T *f;


    if ( ep385.dump_file != NULL )
    {
        fclose( ep385.dump_file );
        ep385.dump_file = NULL;
    }

    if ( ep385.show_file != NULL )
    {
        fclose( ep385.show_file );
        ep385.show_file = NULL;
    }

    /* Check that TWT duty cycle isn't exceeded due to excessive length of
       TWT and TWT_GATE pulses */

    if ( ep385.is_repeat_time )
    {
        f = ep385.function + PULSER_CHANNEL_TWT;
        if ( f->max_duty_warning != 0 )
            print( SEVERE, "Duty cycle of TWT %ld time%s exceeded due to "
                   "length of %s pulses.\n", f->max_duty_warning,
                   f->max_duty_warning == 1 ? "" : "s", f->name );

        f = ep385.function + PULSER_CHANNEL_TWT_GATE;
        if ( f->max_duty_warning != 0 )
            print( SEVERE, "Duty cycle of TWT %ld time%s exceeded due to "
                   "length of %s pulses.\n", f->max_duty_warning,
                   f->max_duty_warning == 1 ? "" : "s", f->name );
    }

    /* If in the test it was found that shape and defense pulses got too
       near to each other bail out */

    if ( ep385.shape_2_defense_too_near != 0 )
        print( FATAL, "Distance between PULSE_SHAPE and DEFENSE pulses was "
               "%ld times shorter than %s during the test run.\n",
               ep385.shape_2_defense_too_near,
               ep385_pticks( ep385.shape_2_defense ) );

    if ( ep385.defense_2_shape_too_near != 0 )
        print( FATAL, "Distance between DEFENSE and PULSE_SHAPE pulses was "
               "%ld times shorter than %s during the test run.\n",
               ep385.defense_2_shape_too_near,
               ep385_pticks( ep385.defense_2_shape ) );

    if (    ep385.shape_2_defense_too_near != 0
         || ep385.defense_2_shape_too_near != 0 )
        THROW( EXCEPTION );

    /* Tell the users about problems when setting shape pulses */

    if ( ep385.left_shape_warning != 0 )
    {
        print( SEVERE, "For %ld time%s left padding for a pulse with "
               "automatic shape pulse couldn't be set.\n",
               ep385.left_shape_warning,
               ep385.left_shape_warning == 1 ? "" : "s" );

        for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
        {
            f = ep385.function + i;

            if (    ! f->uses_auto_shape_pulses
                 || f->left_shape_padding <= f->min_left_shape_padding )
                continue;

            print( SEVERE, "Minimum left shape padding for function '%s' "
                   "was %s instead of the requested %s.\n", f->name,
                   ep385_pticks( f->min_left_shape_padding ),
                   ep385_pticks( f->left_shape_padding ) );
        }
    }

    if ( ep385.right_shape_warning != 0 )
    {
        print( SEVERE, "For %ld time%s right padding for a pulse with "
               "automatic shape pulse couldn't be set.\n",
               ep385.right_shape_warning,
               ep385.right_shape_warning == 1 ? "" : "s" );

        for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
        {
            f = ep385.function + i;

            if (    ! f->uses_auto_shape_pulses
                 || f->right_shape_padding <= f->min_right_shape_padding )
                continue;

            print( SEVERE, "Minimum right shape padding for function '%s' "
                   "was %s instead of the requested %s.\n",  f->name,
                   ep385_pticks( f->min_right_shape_padding ),
                   ep385_pticks( f->right_shape_padding ) );
        }
    }

    /* Tell the users about problems when setting TWT pulses */

    if ( ep385.left_twt_warning != 0 )
        print( SEVERE, "For %ld time%s a pulse was too early to allow correct "
               "setting of its TWT pulse.\n", ep385.left_twt_warning,
               ep385.left_twt_warning == 1 ? "" : "s" );

    if ( ep385.right_twt_warning != 0 )
        print( SEVERE, "For %ld time%s a pulse was too late or too long to "
               "allow correct setting of its TWT pulse.\n",
               ep385.right_twt_warning,
               ep385.right_twt_warning == 1 ? "" : "s" );

    if ( ep385.twt_distance_warning != 0 )
        print( SEVERE, "Distance between TWT pulses was %ld time%s shorter "
               "than %s.\n", ep385.twt_distance_warning,
               ep385.twt_distance_warning == 1 ? "" : "s",
               ep385_pticks( ep385.minimum_twt_pulse_distance ) );

    /* Reset the internal representation back to its initial state */

    ep385_full_reset( );

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
ep385_exp_hook( void )
{
    /* Extra safety net: If the minimum distances between shape and defense
       pulses have been changed by calling the appropriate functions ask
       the user the first time the experiment gets started if (s)he is 100%
       sure that's what (s)he really wants. This can be switched off by
       commenting out the definition in config/ep385.conf of
       "ASK_FOR_SHAPE_DEFENSE_DISTANCE_CONFORMATION" */

#if defined ASK_FOR_SHAPE_DEFENSE_DISTANCE_CONFORMATION
    if (    ! ep385.is_confirmation
         && ( ep385.is_shape_2_defense || ep385.is_defense_2_shape ) )
    {
        char mstr[ 500 ];

        if ( ep385.is_shape_2_defense && ep385.is_defense_2_shape )
        {
            sprintf( mstr, "Minimum distance between SHAPE and DEFENSE\n"
                     "pulses has been changed to %s",
                     ep385_pticks( ep385.shape_2_defense ) );
            sprintf( mstr + strlen( mstr ), " and %s.\n"
                     "***** Is this really what you want? *****",
                     ep385_pticks( ep385.defense_2_shape ) );
        }
        else if ( ep385.is_shape_2_defense )
            sprintf( mstr, "Minimum distance between SHAPE and DEFENSE\n"
                     "pulses has been changed to %s.\n"
                     "***** Is this really what you want? *****",
                     ep385_pticks( ep385.shape_2_defense ) );
        else
            sprintf( mstr, "Minimum distance between DEFENSE and SHAPE\n"
                     "pulses has been changed to %s.\n"
                     "***** Is this really what you want? *****",
                     ep385_pticks( ep385.defense_2_shape ) );

        if ( 2 != show_choices( mstr, 2, "Abort", "Yes", NULL, 1 ) )
            THROW( EXCEPTION );

        ep385.is_confirmation = SET;
    }
#endif

    /* Initialize the device */

    ep385.in_setup = SET;
    if ( ! ep385_init( DEVICE_NAME ) )
    {
        ep385.in_setup = UNSET;
        print( FATAL, "Failure to initialize the pulser: %s.\n",
               gpib_last_error( ) );
        THROW( EXCEPTION );
    }
    ep385.in_setup = UNSET;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
ep385_end_of_exp_hook( void )
{
    ep385_run( STOP );
    gpib_local( ep385.device );

    /* Reset the internal representation back to its initial state
       in case another experiment is started */

    ep385_full_reset( );

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
ep385_exit_hook( void )
{
    Pulse_T *p;
    Function_T *f;
    int i, j;


    if ( ep385.dump_file != NULL )
    {
        fclose( ep385.dump_file );
        ep385.dump_file = NULL;
    }

    if ( ep385.show_file != NULL )
    {
        fclose( ep385.show_file );
        ep385.show_file = NULL;
    }

    /* Free all memory that may have been allocated for the module */

    for ( p = ep385.pulses; p != NULL;  )
        p= ep385_delete_pulse( p, UNSET );

    ep385.pulses = NULL;

    for ( i = 0; i < MAX_CHANNELS; i++ )
        ep385.channel[ i ].pulse_params =
                                    T_free( ep385.channel[ i ].pulse_params );

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = ep385.function + i;

        if ( f->pulses != NULL )
            f->pulses = T_free( f->pulses );

        if ( f->pm != NULL )
        {
            for ( j = 0; j < f->pc_len * f->num_channels; j++ )
                if ( f->pm[ j ] != NULL )
                    T_free( f->pm[ j ] );
            f->pm = T_free( f->pm );
        }
    }
}


/*----------------------------------------------------*/
/* Here come the EDL functions exported by the driver */
/*----------------------------------------------------*/


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_automatic_shape_pulses( Var_T * v )
{
    long func;
    double dl, dr, tmp;


    /* We need at least one argument, the function shape pulse are to be
       created for. */

    if ( v == NULL )
    {
        print( FATAL, "Missing parameter.\n" );
        THROW( EXCEPTION );
    }

    /* Determine the function number */

    func = get_strict_long( v, "pulser function" );

    /* Check that the function argument is reasonable */

    if ( func < 0 || func >= PULSER_CHANNEL_NUM_FUNC )
    {
        print( FATAL, "Invalid pulser function (%ld).\n", func );
        THROW( EXCEPTION );
    }

    if (    func == PULSER_CHANNEL_PULSE_SHAPE
         || func == PULSER_CHANNEL_TWT
         || func == PULSER_CHANNEL_TWT_GATE
         || func == PULSER_CHANNEL_PHASE_1
         || func == PULSER_CHANNEL_PHASE_2 )
    {
        print( FATAL, "Shape pulses can't be set for function '%s'.\n",
               Function_Names[ func ] );
        THROW( EXCEPTION );
    }

    /* Check that a channel has been set for shape pulses */

    if ( ep385.function[ PULSER_CHANNEL_PULSE_SHAPE ].num_channels == 0 )
    {
        print( FATAL, "No channel has been set for function '%s' needed for "
               "creating shape pulses.\n",
               Function_Names[ PULSER_CHANNEL_PULSE_SHAPE ] );
        THROW( EXCEPTION );
    }

    /* Complain if automatic shape pulses have already been switched on for
       the function */

    if ( ep385.function[ func ].uses_auto_shape_pulses )
    {
        print( FATAL, "Use of automatic shape pulses for function '%s' has "
               "already been switched on.\n", Function_Names[ func ] );
        THROW( EXCEPTION );
    }

    ep385.auto_shape_pulses = SET;
    ep385.function[ func ].uses_auto_shape_pulses = SET;

    /* Look for left and right side padding arguments for the functions
       pulses */

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        dl = get_double( v, "left side shape pulse padding" );
        if ( dl < 0 )
        {
            print( FATAL, "Can't use negative left side shape pulse "
                   "padding.\n" );
            THROW( EXCEPTION );
        }
        ep385.function[ func ].left_shape_padding = ep385_double2ticks( dl );

        if ( ( v = vars_pop( v ) ) != NULL )
        {
            dr = get_double( v, "right side shape pulse padding" );
            if ( dr < 0 )
            {
                print( FATAL, "Can't use negative right side shape pulse "
                       "padding.\n" );
                THROW( EXCEPTION );
            }
            ep385.function[ func ].right_shape_padding =
                                                      ep385_double2ticks( dr );
        }
        else
        {
            ep385_timebase_check( );

            tmp = AUTO_SHAPE_RIGHT_PADDING / ep385.timebase;
            tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
            ep385.function[ func ].right_shape_padding = Ticksrnd( tmp );
        }
    }
    else
    {
        ep385_timebase_check( );

        tmp = AUTO_SHAPE_LEFT_PADDING / ep385.timebase;
        tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
        ep385.function[ func ].left_shape_padding = Ticksrnd( tmp );

        tmp = AUTO_SHAPE_RIGHT_PADDING / ep385.timebase;
        tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
        ep385.function[ func ].right_shape_padding = Ticksrnd( tmp );
    }

    too_many_arguments( v );

    ep385.function[ func ].min_left_shape_padding =
                                     ep385.function[ func ].left_shape_padding;
    ep385.function[ func ].min_right_shape_padding =
                                    ep385.function[ func ].right_shape_padding;

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_automatic_twt_pulses( Var_T * v )
{
    long func;
    double dl, dr, tmp;


    /* We need at least one argument, the function TWT pulse are to be
       created for. */

    if ( v == NULL )
    {
        print( FATAL, "Missing parameter.\n" );
        THROW( EXCEPTION );
    }

    /* Determine the function number */

    func = get_strict_long( v, "pulser function" );

    /* Check that the function argument is reasonable */

    if ( func < 0 || func >= PULSER_CHANNEL_NUM_FUNC )
    {
        print( FATAL, "Invalid pulser function (%ld).\n", func );
        THROW( EXCEPTION );
    }

    if (    func == PULSER_CHANNEL_TWT
         || func == PULSER_CHANNEL_TWT_GATE
         || func == PULSER_CHANNEL_PULSE_SHAPE
         || func == PULSER_CHANNEL_PHASE_1
         || func == PULSER_CHANNEL_PHASE_2 )
    {
        print( FATAL, "TWT pulses can't be set for function '%s'.\n",
               Function_Names[ func ] );
        THROW( EXCEPTION );
    }

    /* Check that a channel has been set for TWT pulses */

    if ( ep385.function[ PULSER_CHANNEL_TWT ].num_channels == 0 )
    {
        print( FATAL, "No channel has been set for function '%s' needed for "
               "creating TWT pulses.\n",
               Function_Names[ PULSER_CHANNEL_TWT ] );
        THROW( EXCEPTION );
    }

    /* Complain if automatic TWT pulses have already been switched on for
       the function */

    if ( ep385.function[ func ].uses_auto_twt_pulses )
    {
        print( FATAL, "Use of automatic TWT pulses for function '%s' has "
               "already been switched on.\n", Function_Names[ func ] );
        THROW( EXCEPTION );
    }

    ep385.auto_twt_pulses = SET;
    ep385.function[ func ].uses_auto_twt_pulses = SET;

    /* Look for left and right side padding arguments for the functions
       pulses */

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        dl = get_double( v, "left side TWT pulse padding" );
        if ( dl < 0 )
        {
            print( FATAL, "Can't use negative left side TWT pulse "
                   "padding.\n" );
            THROW( EXCEPTION );
        }
        ep385.function[ func ].left_twt_padding = ep385_double2ticks( dl );

        if ( ( v = vars_pop( v ) ) != NULL )
        {
            dr = get_double( v, "right side TWT pulse padding" );
            if ( dr < 0 )
            {
                print( FATAL, "Can't use negative right side TWT pulse "
                       "padding.\n" );
                THROW( EXCEPTION );
            }
            ep385.function[ func ].right_twt_padding =
                                                      ep385_double2ticks( dr );
        }
        else
        {
            ep385_timebase_check( );

            tmp = AUTO_TWT_RIGHT_PADDING / ep385.timebase;
            tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
            ep385.function[ func ].right_twt_padding = Ticksrnd( tmp );
        }
    }
    else
    {
        ep385_timebase_check( );

        tmp = AUTO_TWT_LEFT_PADDING / ep385.timebase;
        tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
        ep385.function[ func ].left_twt_padding = Ticksrnd( tmp );

        tmp = AUTO_TWT_RIGHT_PADDING / ep385.timebase;
        tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
        ep385.function[ func ].right_twt_padding = Ticksrnd( tmp );
    }

    too_many_arguments( v );

    ep385.function[ func ].min_left_twt_padding =
                                       ep385.function[ func ].left_twt_padding;
    ep385.function[ func ].min_right_twt_padding =
                                      ep385.function[ func ].right_twt_padding;

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_show_pulses( Var_T * v  UNUSED_ARG )
{
    if ( ! FSC2_IS_CHECK_RUN && ! FSC2_IS_BATCH_MODE )
        ep385.do_show_pulses = SET;

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_dump_pulses( Var_T * v  UNUSED_ARG )
{
    if ( ! FSC2_IS_CHECK_RUN && ! FSC2_IS_BATCH_MODE )
        ep385.do_dump_pulses = SET;

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_shape_to_defense_minimum_distance( Var_T * v )
{
    double s2d;


    if ( ep385.is_shape_2_defense )
    {
        print( FATAL, "SHAPE to DEFENSE pulse minimum distance has already "
               "been set to %s.\n",
               ep385_pticks( ep385.shape_2_defense ) );
        THROW( EXCEPTION );
    }

    s2d = get_double( v, "SHAPE to DEFENSE pulse minimum distance" );

    if ( s2d < 0.0 )
    {
        print( FATAL, "Negative SHAPE to DEFENSE pulse minimum distance.\n" );
        THROW( EXCEPTION );
    }

    ep385.shape_2_defense = ep385_double2ticks( s2d );
    ep385.is_shape_2_defense = SET;

    return vars_push( FLOAT_VAR, s2d );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_defense_to_shape_minimum_distance( Var_T * v )
{
    double d2s;


    if ( ep385.is_defense_2_shape )
    {
        print( FATAL, "DEFENSE to SHAPE pulse minimum distance has already "
               "been set to %s.\n", ep385_pticks( ep385.defense_2_shape ) );
        THROW( EXCEPTION );
    }

    d2s = get_double( v, "DEFENSE to SHAPE pulse minimum distance" );

    if ( d2s < 0.0 )
    {
        print( FATAL, "Negative DEFENSE to SHAPE pulse minimum distance.\n" );
        THROW( EXCEPTION );
    }

    ep385.defense_2_shape = ep385_double2ticks( d2s );
    ep385.is_defense_2_shape = SET;

    return vars_push( FLOAT_VAR, d2s );
}


/*-------------------------------------------------------------------*
 * Function allows to query or change the minimum allowed distance
 * between automatically created TWT pulses (if the distance between
 * the pulses gets to short the pulses get lengthened automatically
 * to avoid having to short distances between the pulses).
 * In the EXPERIMENT section the new setting only gets used at the
 * next call of pulser_update().
 *-------------------------------------------------------------------*/

Var_T *
pulser_minimum_twt_pulse_distance( Var_T * v )
{
    double mtpd;


    if ( v == NULL )
    {
        if ( ep385.is_minimum_twt_pulse_distance )
            return vars_push( FLOAT_VAR,
                      ep385_ticks2double( ep385.minimum_twt_pulse_distance ) );
        return vars_push( FLOAT_VAR, MINIMUM_TWT_PULSE_DISTANCE );
    }

    mtpd = get_double( v, "minimum TWT pulse distance" );

    if ( mtpd < 0.0 )
    {
        print( FATAL, "Negative minimum TWT pulse distance.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( mtpd < MINIMUM_TWT_PULSE_DISTANCE )
        print( SEVERE, "New minimum TWT pulse distance is shorter than the "
               "default value of %s, the TWT might not work correctly.\n",
               ep385_ptime( MINIMUM_TWT_PULSE_DISTANCE ) );

    ep385.minimum_twt_pulse_distance = ep385_double2ticks( mtpd );
    ep385.is_minimum_twt_pulse_distance = SET;

    return vars_push( FLOAT_VAR, mtpd );
}


/*---------------------------------------------*
 * Switches the output of the pulser on or off
 *---------------------------------------------*/

Var_T *
pulser_state( Var_T * v )
{
    bool state;


    if ( v == NULL )
        return vars_push( INT_VAR, 1L );

    state = get_boolean( v );

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, ( long ) ( ep385.is_running = state ) );

    ep385_run( state );
    return vars_push( INT_VAR, ( long ) ep385.is_running );
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

Var_T *
pulser_channel_state( Var_T * v  UNUSED_ARG )
{
    print( SEVERE, "Individual channels can't be switched on or off for "
           "this device.\n" );
    return vars_push( INT_VAR, 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_update( Var_T * v  UNUSED_ARG )
{
    if ( ! ep385.is_needed )
        return vars_push( INT_VAR, 1L );

    /* Send all changes to the pulser */

    if ( ep385.needs_update )
        return vars_push( INT_VAR, ep385_do_update( ) ? 1L : 0L );

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------------*
 * Public function to change the position of pulses. If called with no
 * argument all active pulses that have a position change time set will
 * be moved, otherwise all pulses passed as arguments to the function.
 * Take care: The changes will only be commited on the next call of the
 *            function pulser_update() !
 *----------------------------------------------------------------------*/

Var_T *
pulser_shift( Var_T * v )
{
    Pulse_T *p;


    if ( ! ep385.is_needed )
        return vars_push( INT_VAR, 1L );

    /* An empty pulse list means that we have to shift all active pulses that
       have a position change time value set */

    if ( v == NULL )
        for ( p = ep385.pulses; p != NULL; p = p->next )
            if ( p->num >= 0 && p->is_active && p->is_dpos )
                pulser_shift( vars_push( INT_VAR, p->num ) );

    /* Otherwise run through the supplied pulse list */

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        p = ep385_get_pulse( get_strict_long( v, "pulse number" ) );

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

        if ( ! p->is_old_pos )
        {
            p->old_pos = p->pos;
            p->is_old_pos = SET;
        }

        if ( ( p->pos += p->dpos ) < 0 )
        {
            print( FATAL, "Shifting the position of pulse #%ld leads to an "
                   "invalid  negative position of %s.\n",
                   p->num, ep385_pticks( p->pos ) );
            THROW( EXCEPTION );
        }

        if ( p->pos == p->old_pos )       /* nothing really changed ? */
            p->is_old_pos = UNSET;

        p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
        p->needs_update = NEEDS_UPDATE( p );

        /* Also shift shape and TWT pulses associated with the pulse */

        if ( p->sp != NULL )
        {
            p->sp->pos = p->pos;
            p->sp->old_pos = p->old_pos;
            p->sp->is_old_pos = p->is_old_pos;

            p->sp->has_been_active |= ( p->sp->is_active = p->is_active );
            p->sp->needs_update = p->needs_update;
        }

        if ( p->tp != NULL )
        {
            p->tp->pos = p->pos;
            p->tp->old_pos = p->old_pos;
            p->tp->is_old_pos = p->is_old_pos;

            p->tp->has_been_active |= ( p->tp->is_active = p->is_active );
            p->tp->needs_update = p->needs_update;
        }

        if ( p->needs_update )
            ep385.needs_update = SET;
    }

    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------------*
 * Public function for incrementing the length of pulses. If called with
 * no argument all active pulses that have a length change defined are
 * incremented, oltherwise all pulses passed as arguments to the function.
 * Take care: The changes will only be commited on the next call of the
 *            function pulser_update() !
 *-------------------------------------------------------------------------*/

Var_T *
pulser_increment( Var_T * v )
{
    Pulse_T *p;


    if ( ! ep385.is_needed )
        return vars_push( INT_VAR, 1L );

    /* An empty pulse list means that we have to increment all active pulses
       that have a length change time value set */

    if ( v == NULL )
        for ( p = ep385.pulses; p != NULL; p = p->next )
            if ( p->num >= 0 && p->is_active && p->is_dlen )
                pulser_increment( vars_push( INT_VAR, p->num ) );

    /* Otherwise run through the supplied pulse list */

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        p = ep385_get_pulse( get_strict_long( v, "pulse number" ) );

        if ( ! p->is_len )
        {
            print( FATAL, "Pulse #%ld has no length set, so incrementing its "
                   "length isn't possibe.\n", p->num );
            THROW( EXCEPTION );
        }

        if ( ! p->is_dlen )
        {
            print( FATAL, "Length change time hasn't been defined for pulse "
                   "#%ld.\n", p->num );
            THROW( EXCEPTION );
        }

        if ( ! p->is_old_len )
        {
            p->old_len = p->len;
            p->is_old_len = SET;
        }

        if ( ( p->len += p->dlen ) < 0 )
        {
            print( FATAL, "Incrementing the length of pulse #%ld leads to an "
                   "invalid negative pulse length of %s.\n",
                   p->num, ep385_pticks( p->len ) );
            THROW( EXCEPTION );
        }

        if ( p->old_len == p->len )
            p->is_old_len = UNSET;

        p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
        p->needs_update = NEEDS_UPDATE( p );

        /* Also lengthen shape and TWT pulses associated with the pulse */

        if ( p->sp != NULL )
        {
            p->sp->len = p->len;
            p->sp->old_len = p->old_len;
            p->sp->is_old_len = p->is_old_len;

            p->sp->has_been_active |= ( p->sp->is_active = p->is_active );
            p->sp->needs_update = p->needs_update;
        }

        if ( p->tp != NULL )
        {
            p->tp->len = p->len;
            p->tp->old_len = p->old_len;
            p->tp->is_old_len = p->is_old_len;

            p->tp->has_been_active |= ( p->tp->is_active = p->is_active );
            p->tp->needs_update = p->needs_update;
        }

        /* If the pulse was or is active we've got to update the pulser */

        if ( p->needs_update )
            ep385.needs_update = SET;
    }

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_reset( Var_T * v  UNUSED_ARG )
{
    if ( ! ep385.is_needed )
        return vars_push( INT_VAR, 1L );

    In_reset = SET;

    if (    ep385.phs[ 0 ].function != NULL
         || ep385.phs[ 1 ].function != NULL )
        vars_pop( pulser_phase_reset( NULL ) );
    vars_pop( pulser_pulse_reset( NULL ) );

    In_reset = UNSET;

    return pulser_update( NULL );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_pulse_reset( Var_T * v )
{
    Pulse_T *p;


    if ( ! ep385.is_needed )
        return vars_push( INT_VAR, 1L );

    /* An empty pulse list means that we have to reset all pulses (even the
       inactive ones). Do't explicitely reset automatically created pulses
       (which all have a negative pulse number), they will be reset together
       with the pulses belong to. */

    if ( v == NULL )
    {
        for ( p = ep385.pulses; p != NULL; p = p->next )
            if ( p->num >= 0 )
                vars_pop( pulser_pulse_reset( vars_push( INT_VAR, p->num ) ) );
    }

    /* Otherwise run through the supplied pulse list */

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        p = ep385_get_pulse( get_strict_long( v, "pulse number" ) );

        /* Reset all changeable properties back to their initial values */

        if ( p->is_pos && ! p->is_old_pos )
        {
            p->old_pos = p->pos;
            p->is_old_pos = SET;
        }

        p->pos = p->initial_pos;
        p->is_pos = p->initial_is_pos;

        if ( p->is_len && ! p->is_old_len )
        {
            p->old_len = p->len;
            p->is_old_len = SET;
        }

        p->len = p->initial_len;
        p->is_len = p->initial_is_len;

        if ( p->initial_is_dpos )
            p->dpos = p->initial_dpos;
        p->is_dpos = p->initial_is_dpos;

        if ( p->initial_is_dlen )
            p->dlen = p->initial_dlen;
        p->is_dlen = p->initial_is_dlen;

        p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
        p->needs_update = NEEDS_UPDATE( p );

        /* Also reset shape and TWT pulses associated with the pulse */

        if ( p->sp != NULL )
        {
            p->sp->pos = p->pos;
            p->sp->old_pos = p->old_pos;
            p->sp->is_old_pos = p->is_old_pos;

            p->sp->len = p->len;
            p->sp->old_len = p->old_len;
            p->sp->is_old_len = p->is_old_len;

            p->sp->has_been_active |= ( p->sp->is_active = p->is_active );
            p->sp->needs_update = p->needs_update;
        }

        if ( p->tp != NULL )
        {
            p->tp->pos = p->pos;
            p->tp->old_pos = p->old_pos;
            p->tp->is_old_pos = p->is_old_pos;

            p->tp->len = p->len;
            p->tp->old_len = p->old_len;
            p->tp->is_old_len = p->is_old_len;

            p->tp->has_been_active |= ( p->tp->is_active = p->is_active );
            p->tp->needs_update = p->needs_update;
        }

        if ( p->needs_update )
            ep385.needs_update = SET;
    }

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_next_phase( Var_T * v )
{
    Function_T *f;
    long phase_number;


    if ( ! ep385.is_needed )
        return vars_push( INT_VAR, 1L );

    if ( v == NULL )
    {
        if (    ep385.phs[ 0 ].function == NULL
             && ep385.phs[ 1 ].function == NULL
             && FSC2_MODE == TEST )
        {
            print( SEVERE, "Phase cycling isn't used for any function.\n" );
            return vars_push( INT_VAR, 0L );
        }

        if ( ep385.phs[ 0 ].function != NULL )
            vars_pop( pulser_next_phase( vars_push( INT_VAR, 1L ) ) );
        if ( ep385.phs[ 1 ].function != NULL )
            vars_pop( pulser_next_phase( vars_push( INT_VAR, 2L ) ) );
        return vars_push( INT_VAR, 1L );
    }

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        phase_number = get_strict_long( v, "phase number" );

        if ( phase_number != 1 && phase_number != 2 )
        {
            print( FATAL, "Invalid phase number: %ld.\n", phase_number );
            THROW( EXCEPTION );
        }

        if ( ! ep385.phs[ phase_number - 1 ].is_defined )
        {
            print( FATAL, "PHASE_SETUP_%ld has not been defined.\n",
                   phase_number );
            THROW( EXCEPTION );
        }

        f = ep385.phs[ phase_number - 1 ].function;

        if ( ! f->is_used )
        {
            if ( FSC2_MODE == TEST )
                print( SEVERE, "Function '%s' to be phase cycled is not "
                       "used.\n", f->name );
            return vars_push( INT_VAR, 0L );
        }

        if ( ++f->next_phase >= f->pc_len )
            f->next_phase = 0;
    }

    ep385.needs_update = SET;
    pulser_update( NULL );
    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_phase_reset( Var_T * v )
{
    Function_T *f;
    long phase_number;


    if ( ! ep385.is_needed )
        return vars_push( INT_VAR, 1L );

    if ( v == NULL )
    {
        if (    ep385.phs[ 0 ].function == NULL
             && ep385.phs[ 1 ].function == NULL
             && FSC2_MODE == TEST )
        {
            print( SEVERE, "Phase cycling isn't used for any function.\n" );
            return vars_push( INT_VAR, 0L );
        }

        if ( ep385.phs[ 0 ].function != NULL )
            vars_pop( pulser_phase_reset( vars_push( INT_VAR, 1L ) ) );
        if ( ep385.phs[ 1 ].function != NULL )
            vars_pop( pulser_phase_reset( vars_push( INT_VAR, 2L ) ) );
        return vars_push( INT_VAR, 1L );
    }

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        phase_number = get_strict_long( v, "phase number" );

        if ( phase_number != 1 && phase_number != 2 )
        {
            print( FATAL, "Invalid phase number: %ld.\n", phase_number );
            THROW( EXCEPTION );
        }

        f = ep385.phs[ phase_number - 1 ].function;

        if ( ! f->is_used )
        {
            if ( FSC2_MODE == TEST )
                print( SEVERE, "Function '%s' to be phase cycled is not "
                       "used.\n", f->name );
            return vars_push( INT_VAR, 0L );
        }

        f->next_phase = 0;
    }

    ep385.needs_update = SET;
    if ( ! In_reset )
        pulser_update( NULL );
    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_lock_keyboard( Var_T * v  UNUSED_ARG )
{
    print( SEVERE, "Function can't be used for this device.\n" );
    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            ep385_command( cmd );
            T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( cmd );
            RETHROW;
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
 * This is basically a no-op, it just returns the fixed
 * pattern length of the pulser.
 *------------------------------------------------------*/

Var_T *
pulser_maximum_pattern_length( Var_T * v  UNUSED_ARG )
{
    print( WARN, "Pulser doesn't allow setting a maximum pattern length.\n" );
    ep385_timebase_check( );
    return vars_push( FLOAT_VAR, MAX_PULSER_BITS * ep385.timebase );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
