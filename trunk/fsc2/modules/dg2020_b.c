/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#define DG2020_B_MAIN


#include "dg2020_b.h"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/*---------------------------------------------------------------------------
  This function is called directly after all modules are loaded. Its function
  is to initialize all global variables that are needed in the module.
---------------------------------------------------------------------------*/

int dg2020_b_init_hook( void )
{
	int i, j;
	FUNCTION *f;


	pulser_struct.name     = DEVICE_NAME;
	pulser_struct.has_pods = SET;

	/* Set global variable to indicate that GPIB bus is needed */

#ifndef DG2020_B_GPIB_DEBUG
	need_GPIB = SET;
#endif

	/* We have to set up the global structure for the pulser, especially the
	   pointers for the functions that will get called from pulser.c */

	dg2020.needs_update = UNSET;
	dg2020.is_running   = SET;
	dg2020.keep_all     = UNSET;

	pulser_struct.set_timebase = dg2020_store_timebase;

	pulser_struct.assign_function = dg2020_assign_function;
	pulser_struct.assign_channel_to_function =
											 dg2020_assign_channel_to_function;
	pulser_struct.invert_function = dg2020_invert_function;
	pulser_struct.set_function_delay = dg2020_set_function_delay;
	pulser_struct.set_function_high_level = dg2020_set_function_high_level;
	pulser_struct.set_function_low_level = dg2020_set_function_low_level;

	pulser_struct.set_trigger_mode = dg2020_set_trigger_mode;
	pulser_struct.set_repeat_time = dg2020_set_repeat_time;
	pulser_struct.set_trig_in_level = dg2020_set_trig_in_level;
	pulser_struct.set_trig_in_slope = dg2020_set_trig_in_slope;
	pulser_struct.set_trig_in_impedance = dg2020_set_trig_in_impedance;
	pulser_struct.set_max_seq_len = dg2020_set_max_seq_len;

	pulser_struct.set_phase_reference = dg2020_set_phase_reference;

	pulser_struct.new_pulse = dg2020_new_pulse;
	pulser_struct.set_pulse_function = dg2020_set_pulse_function;
	pulser_struct.set_pulse_position = dg2020_set_pulse_position;
	pulser_struct.set_pulse_length = dg2020_set_pulse_length;
	pulser_struct.set_pulse_position_change = dg2020_set_pulse_position_change;
	pulser_struct.set_pulse_length_change = dg2020_set_pulse_length_change;
	pulser_struct.set_pulse_phase_cycle = dg2020_set_pulse_phase_cycle;
	pulser_struct.set_grace_period = NULL;

	pulser_struct.get_pulse_function = dg2020_get_pulse_function;
	pulser_struct.get_pulse_position = dg2020_get_pulse_position;
	pulser_struct.get_pulse_length = dg2020_get_pulse_length;
	pulser_struct.get_pulse_position_change = dg2020_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = dg2020_get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = dg2020_get_pulse_phase_cycle;

	pulser_struct.phase_setup_prep = dg2020_phase_setup_prep;
	pulser_struct.phase_setup = dg2020_phase_setup;

	pulser_struct.set_phase_switch_delay = NULL;

	pulser_struct.keep_all_pulses = dg2020_keep_all;

	/* Finally, we initialize variables that store the state of the pulser */

	dg2020.is_timebase = UNSET;
	dg2020.is_trig_in_mode = UNSET;
	dg2020.is_trig_in_slope = UNSET;
	dg2020.is_trig_in_level = UNSET;
	dg2020.is_repeat_time = UNSET;
	dg2020.is_neg_delay = UNSET;
	dg2020.neg_delay = 0;
	dg2020.is_max_seq_len = UNSET;

	dg2020.trig_in_mode = INTERNAL;
	dg2020.repeat_time = 0;

	dg2020.dump_file = NULL;
	dg2020.show_file = NULL;

	dg2020.is_shape_2_defense = UNSET;
	dg2020.is_defense_2_shape = UNSET;
	dg2020.shape_2_defense_too_near = 0;
	dg2020.defense_2_shape_too_near = 0;
	dg2020.is_confirmation = UNSET;

	dg2020.auto_shape_pulses = UNSET;
	dg2020.left_shape_warning = dg2020.right_shape_warning = 0;

	dg2020.auto_twt_pulses = UNSET;
	dg2020.left_twt_warning = dg2020.right_twt_warning = 0;

	dg2020.block[ 0 ].is_used = dg2020.block[ 1 ].is_used = UNSET;

	dg2020.twt_distance_warning = 0;

	for ( i = 0; i < MAX_PODS; i++ )
	{
		dg2020.pod[ i ].self = i;
		dg2020.pod[ i ].function = NULL;
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = dg2020.function + i;
		f->self = i;
		f->name = Function_Names[ i ];
		f->is_used = UNSET;
		f->is_needed = UNSET;
		for ( j = 0; j < MAX_PODS_PER_FUNC; j++ )
			f->pod[ j ] = NULL;
		f->num_pods = 0;
		f->num_channels = 0;
		f->num_pulses = 0;
		f->pulses = NULL;
		f->phase_setup = NULL;
		f->next_phase = 0;
		f->is_inverted = UNSET;
		f->delay = 0;
		f->is_delay = UNSET;
		f->is_high_level = UNSET;
		f->is_low_level = UNSET;
		f->pm = NULL;
		f->pcm = NULL;
		f->pc_len = 1;
		f->num_active_pulses = 0;
		f->pulse_params = NULL;
		f->old_pulse_params = NULL;
		f->uses_auto_shape_pulses = UNSET;
		f->uses_auto_twt_pulses = UNSET;
		f->max_duty_warning = 0;
	}

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		dg2020.channel[ i ].self = i;
		dg2020.channel[ i ].function = NULL;
		dg2020.channel[ i ].needs_update = UNSET;
        dg2020.channel[ i ].old_d = dg2020.channel[ i ].new_d = NULL;
	}

	for ( i = 0; i < 2; i++ )
	{
		dg2020_phs[ i ].is_defined = UNSET;
		dg2020_phs[ i ].function = NULL;
		for ( j = 0; j < NUM_PHASE_TYPES; j++ )
		{
			dg2020_phs[ i ].is_set[ j ] = UNSET;
			dg2020_phs[ i ].is_needed[ j ] = UNSET;
			dg2020_phs[ i ].pod[ j ] = NULL;
		}
	}

	dg2020_is_needed = SET;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_b_test_hook( void )
{
	if ( dg2020_Pulses == NULL )
	{
		dg2020_is_needed = UNSET;
		print( WARN, "Driver loaded but no pulses defined.\n" );
		return 1;
	}

	/* Check consistency of pulse settings and do everything to setup the
	   pulser for the test run */

	TRY {
		dg2020_IN_SETUP = SET;
		dg2020_init_setup( );
		dg2020_IN_SETUP = UNSET;
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		dg2020_IN_SETUP = UNSET;
		if ( dg2020.dump_file )
		{
			fclose( dg2020.dump_file );
			dg2020.dump_file = NULL;
		}

		if ( dg2020.show_file )
		{
			fclose( dg2020.show_file );
			dg2020.show_file = NULL;
		}

		RETHROW( );
	}

	/* We need some somewhat different functions (or disable some of them) for
	   setting of pulse properties */

	pulser_struct.set_pulse_function = NULL;
	pulser_struct.set_pulse_phase_cycle = NULL;

	pulser_struct.set_pulse_position = dg2020_change_pulse_position;
	pulser_struct.set_pulse_length = dg2020_change_pulse_length;
	pulser_struct.set_pulse_position_change =
		dg2020_change_pulse_position_change;
	pulser_struct.set_pulse_length_change = dg2020_change_pulse_length_change;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_b_end_of_test_hook( void )
{
	static char *min = NULL;
	static int i;
	static FUNCTION *f;


	if ( ! dg2020_is_needed )
		return 1;

	if ( dg2020.dump_file != NULL )
	{
		fclose( dg2020.dump_file );
		dg2020.dump_file = NULL;
	}

	if ( dg2020.show_file != NULL )
	{
		fclose( dg2020.show_file );
		dg2020.show_file = NULL;
	}

	/* Check that TWT duty cycle isn't exceeded due to an excessive length of
	   TWT or TWT_GATE pulses */

	if ( dg2020.is_repeat_time )
	{
		f = dg2020.function + PULSER_CHANNEL_TWT;
		if ( f->max_duty_warning != 0 )
			print( SEVERE, "Duty cycle of TWT %ld time%s exceeded due to "
				   "length of %s pulses.\n", f->max_duty_warning,
				   f->max_duty_warning == 1 ? "" : "s", f->name );

		f = dg2020.function + PULSER_CHANNEL_TWT_GATE;
		if ( f->max_duty_warning != 0 )
			print( SEVERE, "Duty cycle of TWT %ld time%s exceeded due to "
				   "length of %s pulses.\n", f->max_duty_warning,
				   f->max_duty_warning == 1 ? "" : "s", f->name );
	}

	/* If in the test it was found that shape and defense pulses got too
	   near to each other bail out */

	if ( dg2020.shape_2_defense_too_near != 0 )
		print( FATAL, "Distance between PULSE_SHAPE and DEFENSE pulses was "
			   "%ld time%s shorter than %s during the test run.\n",
			   dg2020.shape_2_defense_too_near,
			   dg2020.shape_2_defense_too_near == 1 ? "" : "s",
			   dg2020_pticks( dg2020.shape_2_defense ) );

	if ( dg2020.defense_2_shape_too_near != 0 )
		print( FATAL, "Distance between DEFENSE and PULSE_SHAPE pulses was "
			   "%ld time%s shorter than %s during the test run.\n",
			   dg2020.defense_2_shape_too_near,
			   dg2020.defense_2_shape_too_near == 1 ? "" : "s",
			   dg2020_pticks( dg2020.defense_2_shape ) );

	if ( dg2020.shape_2_defense_too_near != 0 ||
		 dg2020.defense_2_shape_too_near != 0 )
		THROW( EXCEPTION );

	/* Tell the user if there had been problems with shape pulses */

	if ( dg2020.left_shape_warning != 0 )
	{
		print( SEVERE, "For %ld time%s left padding for a pulse with "
			   "automatic shape pulse couldn't be set.\n",
			   dg2020.left_shape_warning,
			   dg2020.left_shape_warning == 1 ? "" : "s" );

		for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		{
			f = dg2020.function + i;

			if ( ! f->uses_auto_shape_pulses ||
				 f->left_shape_padding <= f->min_left_shape_padding )
				continue;

			TRY
			{
				min = T_strdup( dg2020_pticks( f->min_left_shape_padding ) );
				print( SEVERE, "Minimum left padding for function '%s' was %s "
					   "instead of requested %s.\n", f->name,
					   min, dg2020_pticks( f->left_shape_padding ) );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				min = CHAR_P T_free( min );
				RETHROW( );
			}

			min = CHAR_P T_free( min );
		}
	}

	if ( dg2020.right_shape_warning != 0 )
	{
		print( SEVERE, "For %ld time%s right padding for a pulse with "
			   "automatic shape pulse couldn't be set.\n",
			   dg2020.left_shape_warning,
			   dg2020.left_shape_warning == 1 ? "" : "s" );

		for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		{
			f = dg2020.function + i;

			if ( ! f->uses_auto_shape_pulses ||
				 f->right_shape_padding <= f->min_right_shape_padding )
				continue;

			TRY
			{
				min = T_strdup( dg2020_pticks( f->min_right_shape_padding ) );
				print( SEVERE, "Minimum right padding for function '%s' was "
					   "%s instead of requested %s.\n", f->name,
					   min, dg2020_pticks( f->right_shape_padding ) );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				min = CHAR_P T_free( min );
				RETHROW( );
			}

			min = CHAR_P T_free( min );
		}
	}

	/* Tell the user if there had been problems with TWT pulses */

	if ( dg2020.left_twt_warning != 0 )
		print( SEVERE, "For %ld time%s a pulse was too early to allow correct "
			   "setting of its TWT pulse.\n", dg2020.left_twt_warning,
			   dg2020.left_twt_warning == 1 ? "" : "s" );

	if ( dg2020.right_twt_warning != 0 )
		print( SEVERE, "For %ld time%s a pulse was too late or too long to "
			   "allow correct setting of its TWT pulse.\n",
			   dg2020.right_twt_warning,
			   dg2020.right_twt_warning == 1 ? "" : "s" );

	if ( dg2020.twt_distance_warning != 0 )
		print( SEVERE, "Distance between TWT pulses was %ld time%s shorter "
			   "than %s.\n", dg2020.twt_distance_warning,
			   dg2020.twt_distance_warning == 1 ? "" : "s",
			   dg2020_pticks( dg2020.minimum_twt_pulse_distance ) );

	/* Now we have to reset the internal representation back to its initial
	   state */

	dg2020_full_reset( );

	/* Now we've got to find out about the maximum sequence length and set
	   up padding to achieve the requested repeat time */

	dg2020.max_seq_len = dg2020_get_max_seq_len( );
	dg2020_calc_padding( );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_b_exp_hook( void )
{
	int i;


	if ( ! dg2020_is_needed )
		return 1;

	/* Extra safety net: If the minimum distances between shape and defense
	   pulses have been changed by calling the appropriate functions ask
	   the user the first time the experiment gets started if (s)he is 100%
	   sure that's what (s)he really wants. This can be switched off by
	   commenting out the definition in config/dg2020.conf of
	   "ASK_FOR_SHAPE_DEFENSE_DISTANCE_CONFORMATION" */

#if defined ASK_FOR_SHAPE_DEFENSE_DISTANCE_CONFORMATION
	if ( ! dg2020.is_confirmation && 
		 ( dg2020.is_shape_2_defense || dg2020.is_defense_2_shape ) )
	{
		char str[ 500 ];

		if ( dg2020.is_shape_2_defense && dg2020.is_defense_2_shape )
		{
			sprintf( str, "Minimum distance between SHAPE and DEFENSE\n"
					 "pulses has been changed to %s",
					 dg2020_ptime( 
						 dg2020_ticks2double( dg2020.shape_2_defense ) ) );
			sprintf( str + strlen( str ), " and %s.\n"
					 "***** Is this really what you want? *****",
					 dg2020_ptime(
						 dg2020_ticks2double( dg2020.defense_2_shape ) ) );
		}
		else if ( dg2020.is_shape_2_defense )
			sprintf( str, "Minimum distance between SHAPE and DEFENSE\n"
					 "pulses has been changed to %s.\n"
					 "***** Is this really what you want? *****",
					 dg2020_ptime(
						 dg2020_ticks2double( dg2020.shape_2_defense ) ) );
		else
			sprintf( str, "Minimum distance between DEFENSE and SHAPE\n"
					 "pulses has been changed to %s.\n"
					 "***** Is this really what you want? *****",
					 dg2020_ptime(
						 dg2020_ticks2double( dg2020.defense_2_shape ) ) );

		if ( 2 != show_choices( str, 2, "Abort", "Yes", "", 1 ) )
			THROW( EXCEPTION );

		dg2020.is_confirmation = SET;
	}
#endif

	/* Initialize the device */

	if ( ! dg2020_init( DEVICE_NAME ) )
	{
		print( FATAL, "Failure to initialize the pulser: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	/* Now we have to tell the pulser about all the pulses */

	dg2020_IN_SETUP = SET;
	if ( ! dg2020_reorganize_pulses( SET ) )
	{
		dg2020_IN_SETUP = UNSET;
		THROW( EXCEPTION );
	}
	dg2020_IN_SETUP = UNSET;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		if ( dg2020.function[ i ].is_used )
			dg2020_set_pulses( dg2020.function + i );

	/* Finally tell the pulser to update (we're always running in manual
	   update mode) and than switch the pulser into run mode */

	dg2020_update_data( );

	dg2020_run( dg2020.is_running );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_b_end_of_exp_hook( void )
{
	FUNCTION *f;
	int i;


	if ( ! dg2020_is_needed )
		return 1;

	dg2020_run( STOP );
	gpib_local( dg2020.device );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = dg2020.function + i;
		f->pulse_params = PULSE_PARAMS_P T_free( f->pulse_params );
		f->old_pulse_params = PULSE_PARAMS_P T_free( f->old_pulse_params );
	}

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void dg2020_b_exit_hook( void )
{
	PULSE *p;
	FUNCTION *f;
	int i;


	if ( ! dg2020_is_needed )
		return;

	/* Free all memory allocated within the module */

	p = dg2020_Pulses;
	while ( p != NULL )
		p = dg2020_delete_pulse( p, UNSET );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = dg2020.function + i;
		f->pcm = CHANNEL_PP T_free( f->pcm );
		f->pm = BOOL_P T_free( f->pm );
		f->pulses = PULSE_PP T_free( f->pulses );
	}
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_automatic_shape_pulses( Var *v )
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

	if ( func == PULSER_CHANNEL_PULSE_SHAPE ||
		 func == PULSER_CHANNEL_TWT ||
		 func == PULSER_CHANNEL_TWT_GATE ||
		 func == PULSER_CHANNEL_PHASE_1 ||
		 func == PULSER_CHANNEL_PHASE_2 )
	{
		print( FATAL, "Shape pulses can't be set for function '%s'.\n",
			   Function_Names[ func ] );
		THROW( EXCEPTION );
	}

	/* Check that a pod has been set for shape pulses */

	if ( dg2020.function[ PULSER_CHANNEL_PULSE_SHAPE ].num_pods == 0 )
	{
		print( FATAL, "No channel has been set for function '%s' needed for "
			   "creating shape pulses.\n",
			   Function_Names[ PULSER_CHANNEL_PULSE_SHAPE ] );
		THROW( EXCEPTION );
	}

	/* Complain if automatic shape pulses have already been switched on for
	   the function */

	if ( dg2020.function[ func ].uses_auto_shape_pulses )
	{
		print( FATAL, "Use of automatic shape pulses for function '%s' has "
			   "already been switched on.\n", Function_Names[ func ] );
		THROW( EXCEPTION );
	}

	dg2020.auto_shape_pulses = SET;
	dg2020.function[ func ].uses_auto_shape_pulses = SET;

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
		dg2020.function[ func ].left_shape_padding = dg2020_double2ticks( dl );

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			dr = get_double( v, "right side shape pulse padding" );
			if ( dr < 0 )
			{
				print( FATAL, "Can't use negative right side shape pulse "
					   "padding.\n" );
				THROW( EXCEPTION );
			}
			dg2020.function[ func ].right_shape_padding =
													 dg2020_double2ticks( dr );
		}
		else
		{
			tmp = AUTO_SHAPE_RIGHT_PADDING / dg2020.timebase;
			tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
			dg2020.function[ func ].right_shape_padding = Ticksrnd( tmp );
		}
	}
	else
	{
		tmp = AUTO_SHAPE_LEFT_PADDING / dg2020.timebase;
		tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
		dg2020.function[ func ].left_shape_padding = Ticksrnd( tmp );

		tmp = AUTO_SHAPE_RIGHT_PADDING / dg2020.timebase;
		tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
		dg2020.function[ func ].right_shape_padding = Ticksrnd( tmp );
	}

	too_many_arguments( v );

	dg2020.function[ func ].min_left_shape_padding = 
									dg2020.function[ func ].left_shape_padding;
	dg2020.function[ func ].min_right_shape_padding =
								   dg2020.function[ func ].right_shape_padding;

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_automatic_twt_pulses( Var *v )
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

	if ( func == PULSER_CHANNEL_TWT ||
		 func == PULSER_CHANNEL_TWT_GATE ||
		 func == PULSER_CHANNEL_PULSE_SHAPE ||
		 func == PULSER_CHANNEL_PHASE_1 ||
		 func == PULSER_CHANNEL_PHASE_2 )
	{
		print( FATAL, "TWT pulses can't be set for function '%s'.\n",
			   Function_Names[ func ] );
		THROW( EXCEPTION );
	}

	/* Check that a pod has been set for TWT pulses */

	if ( dg2020.function[ PULSER_CHANNEL_TWT ].num_pods == 0 )
	{
		print( FATAL, "No channel has been set for function '%s' needed for "
			   "creating TWT pulses.\n",
			   Function_Names[ PULSER_CHANNEL_TWT ] );
		THROW( EXCEPTION );
	}

	/* Complain if automatic TWT pulses have already been switched on for
	   the function */

	if ( dg2020.function[ func ].uses_auto_twt_pulses )
	{
		print( FATAL, "Use of automatic TWT pulses for function '%s' has "
			   "already been switched on.\n", Function_Names[ func ] );
		THROW( EXCEPTION );
	}

	dg2020.auto_twt_pulses = SET;
	dg2020.function[ func ].uses_auto_twt_pulses = SET;

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
		dg2020.function[ func ].left_twt_padding = dg2020_double2ticks( dl );

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			dr = get_double( v, "right side TWT pulse padding" );
			if ( dr < 0 )
			{
				print( FATAL, "Can't use negative right side TWT pulse "
					   "padding.\n" );
				THROW( EXCEPTION );
			}
			dg2020.function[ func ].right_twt_padding =
													 dg2020_double2ticks( dr );
		}
		else
		{
			tmp = AUTO_TWT_RIGHT_PADDING / dg2020.timebase;
			tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
			dg2020.function[ func ].right_twt_padding = Ticksrnd( tmp );
		}
	}
	else
	{
		tmp = AUTO_TWT_LEFT_PADDING / dg2020.timebase;
		tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
		dg2020.function[ func ].left_twt_padding = Ticksrnd( tmp );

		tmp = AUTO_TWT_RIGHT_PADDING / dg2020.timebase;
		tmp = tmp - floor( tmp ) > 0.01 ? ceil( tmp ) : floor( tmp );
		dg2020.function[ func ].right_twt_padding = Ticksrnd( tmp );
	}

	too_many_arguments( v );

	dg2020.function[ func ].min_left_twt_padding = 
									 dg2020.function[ func ].left_twt_padding;
	dg2020.function[ func ].min_right_twt_padding =
									 dg2020.function[ func ].right_twt_padding;

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_show_pulses( Var *v )
{
	int pd[ 2 ];
	pid_t pid;


	v = v;

	if ( FSC2_IS_CHECK_RUN )
		return vars_push( INT_VAR, 1 );

	if ( dg2020.show_file != NULL )
		return vars_push( INT_VAR, 1 );

	if ( pipe( pd ) == -1 )
	{
		if ( errno == EMFILE || errno == ENFILE )
			print( FATAL, "Failure, running out of system resources.\n" );
		return vars_push( INT_VAR, 0 );
	}

	if ( ( pid =  fork( ) ) < 0 )
	{
		if ( errno == ENOMEM || errno == EAGAIN )
			print( FATAL, "Failure, running out of system resources.\n" );
		return vars_push( INT_VAR, 0 );
	}

	/* Here's the childs code */

	if ( pid == 0 )
	{
		static char *cmd = NULL;


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
	dg2020.show_file = fdopen( pd[ 1 ], "w" );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_dump_pulses( Var *v )
{
	char *name;
	char *m;
	struct stat stat_buf;


	v = v;

	if ( FSC2_IS_CHECK_RUN )
		return vars_push( INT_VAR, 1 );

	if ( dg2020.dump_file != NULL )
	{
		print( WARN, "Pulse dumping is already switched on.\n" );
		return vars_push( INT_VAR, 1 );
	}

	do
	{
		name = T_strdup( fl_show_fselector( "File for dumping pulses:", "./",
											"*.pls", NULL ) );
		if ( name == NULL || *name == '\0' )
		{
			T_free( name );
			return vars_push( INT_VAR, 0 );
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

		if ( ( dg2020.dump_file = fopen( name, "w+" ) ) == NULL )
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
	} while ( dg2020.dump_file == NULL );

	T_free( name );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_shape_to_defense_minimum_distance( Var *v )
{
	double s2d;


	if ( ! dg2020.is_timebase )
	{
		print( FATAL, "Can't set a time because no pulser time base has been "
			   "set.\n" );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_shape_2_defense )
	{
		print( FATAL, "SHAPE to DEFENSE pulse minimum distance has already "
			   "been set to %s.\n",
			   dg2020_ptime( dg2020_ticks2double( dg2020.shape_2_defense ) ) );
		THROW( EXCEPTION );
	}

	s2d = get_double( v, "SHAPE to DEFENSE pulse minimum distance" );

	if ( s2d < 0 )
	{
		print( FATAL, "Negative SHAPE to DEFENSE pulse minimum distance.\n" );
		THROW( EXCEPTION );
	}

	dg2020.shape_2_defense = dg2020_double2ticks( s2d );
	dg2020.is_shape_2_defense = SET;

	return vars_push( FLOAT_VAR, s2d );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_defense_to_shape_minimum_distance( Var *v )
{
	double d2s;


	if ( ! dg2020.is_timebase )
	{
		print( FATAL, "Can't set a time because no pulser time base has been "
			   "set.\n" );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_defense_2_shape )
	{
		print( FATAL, "DEFENSE to SHAPE pulse minimum distance has already "
			   "been set to %s.\n",
			   dg2020_ptime( dg2020_ticks2double( dg2020.defense_2_shape ) ) );
		THROW( EXCEPTION );
	}

	d2s = get_double( v, "DEFENSE to SHAPE pulse minimum distance" );

	if ( d2s < 0 )
	{
		print( FATAL, "Negative DEFENSE to SHAPE pulse minimum distance.\n" );
		THROW( EXCEPTION );
	}

	dg2020.defense_2_shape = dg2020_double2ticks( d2s );
	dg2020.is_defense_2_shape = SET;

	return vars_push( FLOAT_VAR, d2s );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_state( Var *v )
{
	bool state;


	if ( v == NULL )
		return vars_push( INT_VAR, ( long ) dg2020.is_running );

	state = get_boolean( v );

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( INT_VAR, ( long ) ( dg2020.is_running = state ) );

	dg2020_run( state );
	return vars_push( INT_VAR, ( long ) state );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_channel_state( Var *v )
{
	v = v;
	print( SEVERE, "Individual pod channels can't be switched on or off with "
		   "this device.\n" );
	return vars_push( INT_VAR, 0 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_update( Var *v )
{
	bool state = OK;


	v = v;

	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* Send all changes to the pulser */

	if ( dg2020.needs_update )
		state = dg2020_do_update( );

	/* If we're doing a real experiment also tell the pulser to start */

	if ( FSC2_MODE == EXPERIMENT )
		dg2020_run( dg2020.is_running );

	return vars_push( INT_VAR, state ? 1 : 0 );
}


/*----------------------------------------------------------------------*/
/* Public function to change the position of pulses. If called with no  */
/* argument all active pulses that have a position change time set will */
/* be moved, otherwise all pulses passed as arguments to the function.  */
/* Take care: The changes will only commited on the next call of the    */
/*            function pulser_update() !                                */
/*----------------------------------------------------------------------*/

Var *pulser_shift( Var *v )
{
	PULSE *p;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
	{
		long ret = 1;

		for ( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dpos )
				ret &= pulser_shift( vars_push( INT_VAR, p->num ) )->val.lval;
		return vars_push( INT_VAR, ret );
	}

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = dg2020_get_pulse( get_strict_long( v, "pulse number" ) );

		if ( ! p->is_pos )
		{
			print( FATAL, "Pulse #%ld has no position set, so shifting it is "
				   "impossible.\n", p->num );
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
				   p->num, dg2020_pticks( p->pos ) );
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
			dg2020.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------------*/
/* Public function for incrementing the length of pulses. If called with   */
/* no argument all active pulses that have a length change defined are     */
/* incremented, oltherwise all pulses passed as arguments to the function. */
/* Take care: The changes will only commited on the next call of the       */
/*            function pulser_update() !                                   */
/*-------------------------------------------------------------------------*/

Var *pulser_increment( Var *v )
{
	PULSE *p;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
	{
		long ret = 1;

		for ( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dlen )
				ret &=
					pulser_increment( vars_push( INT_VAR, p->num ) )->val.lval;
		return vars_push( INT_VAR, ret );
	}

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = dg2020_get_pulse( get_strict_long( v, "pulse number" ) );

		if ( ! p->is_len )
		{
			print( FATAL, "Pulse #%ld has no length set, so incrementing it "
				   "is impossibe.\n", p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->is_dlen )
		{
			print( FATAL, "No length change time has been defined for pulse "
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
				   p->num, dg2020_pticks( p->len ) );
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
			dg2020.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_next_phase( Var *v )
{
	int j;
	FUNCTION *f;
	long phase_number;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		long res = 1;


		if ( dg2020_phs[ 0 ].function == NULL &&
			 dg2020_phs[ 1 ].function == NULL &&
			 FSC2_MODE == TEST )
		{
			print( SEVERE, "Phase cycling isn't used for any function.\n" );
			return vars_push( INT_VAR, 0 );
		}

		if ( dg2020_phs[ 0 ].function != NULL )
			res &= pulser_next_phase( vars_push( INT_VAR, 1 ) )->val.lval;
		if ( dg2020_phs[ 1 ].function != NULL )
			res &= pulser_next_phase( vars_push( INT_VAR, 2 ) )->val.lval;
		return vars_push( INT_VAR, res );
	}

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		phase_number = get_strict_long( v, "phase number" );

		if ( phase_number != 1 && phase_number != 2 )
		{
			print( FATAL, "Invalid phase number: %ld.\n", phase_number );
			THROW( EXCEPTION );
		}

		if ( ! dg2020_phs[ phase_number - 1 ].is_defined )
		{
			print( FATAL, "PHASE_SETUP_%ld has not been defined.\n",
				   phase_number );
			THROW( EXCEPTION );
		}

		f = dg2020_phs[ phase_number - 1 ].function;

		if ( ! f->is_used )
		{
			if ( FSC2_MODE == TEST )
				print( SEVERE, "Function '%s' to be phase cycled is not "
					   "used.\n", Function_Names[ f->self ] );
			return vars_push( INT_VAR, 0 );
		}

		if ( ++f->next_phase >= f->pc_len )
			f->next_phase = 0;

		if ( FSC2_MODE == EXPERIMENT )
			for ( j = 0; j < NUM_PHASE_TYPES; j++ )
				if ( f->phase_setup->is_set[ j ] &&
					 ! dg2020_channel_assign(
						 f->pcm[ j * f->pc_len + f->next_phase ]->self,
						 f->phase_setup->pod[ j ]->self ) )
					return vars_push( INT_VAR, 0 );
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_reset( Var *v )
{
	v = v;

	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	vars_pop( pulser_pulse_reset( NULL ) );
	if ( dg2020_phs[ 0 ].function != NULL ||
		 dg2020_phs[ 1 ].function != NULL )
		vars_pop( pulser_pulse_reset( NULL ) );

	return pulser_update( NULL );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_phase_reset( Var *v )
{
	int j;
	FUNCTION *f;
	long phase_number;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		if ( dg2020_phs[ 0 ].function == NULL &&
			 dg2020_phs[ 1 ].function == NULL &&
			 FSC2_MODE == TEST )
		{
			print( SEVERE, "Phase cycling isn't used for any function.\n" );
			return vars_push( INT_VAR, 0 );
		}

		if ( dg2020_phs[ 0 ].function != NULL )
			vars_pop( pulser_phase_reset( vars_push( INT_VAR, 1 ) ) );
		if ( dg2020_phs[ 1 ].function != NULL )
			vars_pop( pulser_phase_reset( vars_push( INT_VAR, 2 ) ) );
		return vars_push( INT_VAR, 1 );
	}

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		phase_number = get_strict_long( v, "phase number" );

		if ( phase_number != 1 && phase_number != 2 )
		{
			print( FATAL, "Invalid phase number: %ld.\n", phase_number );
			THROW( EXCEPTION );
		}

		f = dg2020_phs[ phase_number - 1 ].function;

		if ( ! f->is_used )
		{
			if ( FSC2_MODE == TEST )
				print( SEVERE, "Function '%s' to be phase cycled is not "
					   "used.\n", Function_Names[ f->self ] );
			return vars_push( INT_VAR, 0 );
		}

		f->next_phase = 0;

		if ( FSC2_MODE == EXPERIMENT )
			for ( j = 0; j < NUM_PHASE_TYPES; j++ )
				if ( f->phase_setup->is_set[ j ] &&
					 ! dg2020_channel_assign(
						 					f->pcm[ j * f->pc_len + 0 ]->self,
											f->phase_setup->pod[ j ]->self ) )
					return vars_push( INT_VAR, 0 );
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_pulse_reset( Var *v )
{
	PULSE *p;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to reset all pulses (even the
       inactive ones) */

	if ( v == NULL )
	{
		if ( dg2020_phs[ 0 ].function != NULL ||
			 dg2020_phs[ 1 ].function != NULL )
			vars_pop( pulser_phase_reset( NULL ) );

		for ( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				vars_pop( pulser_pulse_reset( vars_push( INT_VAR, p->num ) ) );

		return vars_push( INT_VAR, 1 );
	}

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = dg2020_get_pulse( get_strict_long( v, "pulse number" ) );

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
			dg2020.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_lock_keyboard( Var *v )
{
	bool lock;


	if ( v == NULL )
		lock = SET;
	else
	{
		lock = get_boolean( v );
		too_many_arguments( v );
	}

	if ( FSC2_MODE == EXPERIMENT )
		dg2020_lock_state( lock );

	return vars_push( INT_VAR, lock ? 1 : 0 );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
