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


#define RS690_MAIN


#include "rs690.h"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/*-------------------------------------------------------------------------*/
/* This function is called directly after all modules are loaded. Its main */
/* function is to initialize all global variables that are needed in the   */
/* module.                                                                 */
/*-------------------------------------------------------------------------*/

int rs690_init_hook( void )
{
	int i, j;
	FUNCTION *f;
	CHANNEL *ch;


	fsc2_assert( SHAPE_2_DEFENSE_DEFAULT_MIN_DISTANCE > 0 );
	fsc2_assert( DEFENSE_2_SHAPE_DEFAULT_MIN_DISTANCE > 0 );

	pulser_struct.name     = DEVICE_NAME;
	pulser_struct.has_pods = UNSET;

	/* Set global variable to indicate that GPIB bus is needed */
/*
	need_GPIB = SET;
*/
	/* We have to set up the global structure for the pulser, especially the
	   pointers for the functions that will get called from pulser.c */

	rs690.needs_update = SET;
	rs690.keep_all     = UNSET;
	rs690.is_cw_mode   = UNSET;

	pulser_struct.set_timebase = rs690_store_timebase;
	pulser_struct.assign_function = NULL;
	pulser_struct.assign_channel_to_function =
											  rs690_assign_channel_to_function;
	pulser_struct.invert_function = rs690_invert_function;
	pulser_struct.set_function_delay = rs690_set_function_delay;
	pulser_struct.set_function_high_level = NULL;
	pulser_struct.set_function_low_level = NULL;

	pulser_struct.set_trigger_mode = rs690_set_trigger_mode;
	pulser_struct.set_repeat_time = rs690_set_repeat_time;
	pulser_struct.set_trig_in_level = NULL;
	pulser_struct.set_trig_in_slope = rs690_set_trig_in_slope;
	pulser_struct.set_trig_in_impedance = NULL;
	pulser_struct.set_max_seq_len = NULL;

	pulser_struct.set_phase_reference = rs690_set_phase_reference;

	pulser_struct.new_pulse = rs690_new_pulse;
	pulser_struct.set_pulse_function = rs690_set_pulse_function;
	pulser_struct.set_pulse_position = rs690_set_pulse_position;
	pulser_struct.set_pulse_length = rs690_set_pulse_length;
	pulser_struct.set_pulse_position_change = rs690_set_pulse_position_change;
	pulser_struct.set_pulse_length_change = rs690_set_pulse_length_change;
	pulser_struct.set_pulse_phase_cycle = rs690_set_pulse_phase_cycle;
	pulser_struct.set_grace_period = NULL;

	pulser_struct.get_pulse_function = rs690_get_pulse_function;
	pulser_struct.get_pulse_position = rs690_get_pulse_position;
	pulser_struct.get_pulse_length = rs690_get_pulse_length;
	pulser_struct.get_pulse_position_change = rs690_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = rs690_get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = rs690_get_pulse_phase_cycle;

	pulser_struct.phase_setup_prep = rs690_phase_setup_prep;
	pulser_struct.phase_setup = rs690_phase_setup;

	pulser_struct.set_phase_switch_delay = NULL;

	pulser_struct.keep_all_pulses = rs690_keep_all;

	/* Finally, we initialize variables that store the state of the pulser */

	rs690.is_trig_in_mode = UNSET;
	rs690.is_trig_in_slope = UNSET;
	rs690.is_repeat_time = UNSET;
	rs690.is_neg_delay = UNSET;
	rs690.neg_delay = 0;
	rs690.is_running = SET;
	rs690.has_been_running = SET;
	rs690.is_timebase = UNSET;
	rs690.timebase_mode = EXTERNAL;

	rs690.trig_in_mode = INTERNAL;

	rs690.num_pulses = 0;

	rs690.dump_file = NULL;
	rs690.show_file = NULL;

	rs690.is_shape_2_defense = UNSET;
	rs690.is_defense_2_shape = UNSET;
	rs690.shape_2_defense_too_near = 0;
	rs690.defense_2_shape_too_near = 0;
	rs690.is_confirmation = UNSET;

	rs690.auto_shape_pulses = UNSET;
	rs690.left_shape_warning = 0;

	rs690.auto_twt_pulses = UNSET;
	rs690.left_twt_warning = 0;

	rs690.new_fs = rs690.old_fs = NULL;
	rs690.new_fs_count = rs690.old_fs_count = 0;

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		ch = rs690.channel + i;
		ch->self = i;
		ch->function = NULL;
		ch->pulse_params = NULL;
		ch->num_pulses = 0;
		ch->num_active_pulses = 0;
		ch->needs_update = SET;
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rs690.function + i;
		f->self = i;
		f->name = Function_Names[ i ];
		f->is_used = UNSET;
		f->is_needed = UNSET;
		f->is_inverted = UNSET;
		f->num_pulses = 0;
		f->pulses = NULL;
		f->num_channels = 0;
		f->pm = NULL;
		f->delay = 0;
		f->is_delay = UNSET;
		f->next_phase = 0;
		f->uses_auto_shape_pulses = UNSET;
		f->uses_auto_twt_pulses = UNSET;
		f->has_auto_twt_pulses = UNSET;
		for ( j = 0; j <= MAX_CHANNELS; j++ )
			f->channel[ j ] = NULL;
		f->max_len = 0;
	}

	for ( i = 0; i < 4 * NUM_HSM_CARDS; i++ )
		rs690_default_fields[ i ] = 0;

	rs690_is_needed = SET;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int rs690_test_hook( void )
{
	if ( rs690.is_cw_mode )
		return 1;

	if ( rs690_Pulses == NULL )
	{
		rs690_is_needed = UNSET;
		print( WARN, "Driver loaded but no pulses are defined.\n" );
		return 1;
	}

	/* Check consistency of pulse settings and do everything to setup the
	   pulser for the test run */

	TRY
	{
		rs690_init_setup( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( rs690.dump_file )
		{
			fclose( rs690.dump_file );
			rs690.dump_file = NULL;
		}

		if ( rs690.show_file )
		{
			fclose( rs690.show_file );
			rs690.show_file = NULL;
		}

		RETHROW( );
	}

	/* We need some somewhat different functions (or disable some) for
	   setting the pulse properties */

	pulser_struct.set_pulse_function = NULL;

	pulser_struct.set_pulse_position = rs690_change_pulse_position;
	pulser_struct.set_pulse_length = rs690_change_pulse_length;
	pulser_struct.set_pulse_position_change =
											rs690_change_pulse_position_change;
	pulser_struct.set_pulse_length_change = rs690_change_pulse_length_change;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int rs690_end_of_test_hook( void )
{
	int i;
	FUNCTION *f;
	char *min;


	if ( rs690.dump_file != NULL )
	{
		fclose( rs690.dump_file );
		rs690.dump_file = NULL;
	}

	if ( rs690.show_file != NULL )
	{
		fclose( rs690.show_file );
		rs690.show_file = NULL;
	}

	if ( ! rs690_is_needed || rs690.is_cw_mode )
		return 1;

	/* Reset the internal representation back to its initial state */

	if ( ! rs690.is_cw_mode )
		rs690_full_reset( );

	/* Check that TWT duty cycle isn't exceeded due to excessive length of
	   TWT and TWT_GATE pulses */

	if ( rs690.trig_in_mode == INTERNAL && rs690.is_repeat_time )
	{
		f = rs690.function + PULSER_CHANNEL_TWT;
		if ( f->is_used && f->num_channels > 0 &&
			 f->max_len > MAX_TWT_DUTY_CYCLE * rs690.repeat_time )
			print( SEVERE, "Duty cycle of TWT exceeded due to length of %s "
				   "pulses.\n", f->name );

		f = rs690.function + PULSER_CHANNEL_TWT_GATE;
		if ( f->is_used && f->num_channels > 0 &&
			 f->max_len > MAX_TWT_DUTY_CYCLE * rs690.repeat_time )
			print( SEVERE, "Duty cycle of TWT exceeded due to length of %s "
				   "pulses.\n", f->name );
	}

	/* If in the test it was found that shape and defense pulses got too
	   near to each other bail out */

	if ( rs690.shape_2_defense_too_near != 0 )
		print( FATAL, "Distance between PULSE_SHAPE and DEFENSE pulses was "
			   "%ld times shorter than %s during the test run.\n",
			   rs690.shape_2_defense_too_near,
			   rs690_pticks( rs690.shape_2_defense ) );

	if ( rs690.defense_2_shape_too_near != 0 )
		print( FATAL, "Distance between DEFENSE and PULSE_SHAPE pulses was "
			   "%ld times shorter than %s during the test run.\n",
			   rs690.defense_2_shape_too_near,
			   rs690_pticks( rs690.defense_2_shape ) );

	if ( rs690.shape_2_defense_too_near != 0 ||
		 rs690.defense_2_shape_too_near != 0 )
		THROW( EXCEPTION );

	if ( rs690.left_shape_warning != 0 )
	{
		print( SEVERE, "For %ld times left padding for a pulse with "
			   "automatic shape pulse couldn't be set.\n",
			   rs690.left_shape_warning );

		for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		{
			f = rs690.function + i;

			if ( ! f->uses_auto_shape_pulses ||
				 f->left_shape_padding <= f->min_left_shape_padding )
				continue;

			min = T_strdup( rs690_pticks( f->min_left_shape_padding ) );
			print( SEVERE, "Minimum left padding for function '%s' was %s "
				   "instead of requested %s.\n", f->name,
				   min, rs690_pticks( f->left_shape_padding ) );
			T_free( min );
		}
	}

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int rs690_exp_hook( void )
{
	int i, j;
	FUNCTION *f;


	if ( ! rs690_is_needed )
		return 1;

	/* Extra safety net: If the minimum distances between shape and defense
	   pulses have been changed by calling the appropriate functions ask
	   the user the first time the experiment gets started if (s)he is 100%
	   sure that's what (s)he really wants. This can be switched off by
	   commenting out the definition in config/rs690.conf of
	   "ASK_FOR_SHAPE_DEFENSE_DISTANCE_CONFORMATION" */

#if defined ASK_FOR_SHAPE_DEFENSE_DISTANCE_CONFORMATION
	if ( ! rs690.is_confirmation && 
		 ( rs690.is_shape_2_defense || rs690.is_defense_2_shape ) )
	{
		char str[ 500 ];

		if ( rs690.is_shape_2_defense && rs690.is_defense_2_shape )
		{
			sprintf( str, "Minimum distance between SHAPE and DEFENSE\n"
					 "pulses has been changed to %s",
					 rs690_pticks( rs690.shape_2_defense ) );
			sprintf( str + strlen( str ), " and %s.\n"
					 "***** Is this really what you want? *****",
					 rs690_pticks( rs690.defense_2_shape ) );
		}
		else if ( rs690.is_shape_2_defense )
			sprintf( str, "Minimum distance between SHAPE and DEFENSE\n"
					 "pulses has been changed to %s.\n"
					 "***** Is this really what you want? *****",
					 rs690_pticks( rs690.shape_2_defense ) );
		else
			sprintf( str, "Minimum distance between DEFENSE and SHAPE\n"
					 "pulses has been changed to %s.\n"
					 "***** Is this really what you want? *****",
					 rs690_pticks( rs690.defense_2_shape ) );

		if ( 2 != show_choices( str, 2, "Abort", "Yes", "", 1 ) )
			THROW( EXCEPTION );

		rs690.is_confirmation = SET;
	}
#endif

	/* Initialize the device */

	if ( ! rs690_init( DEVICE_NAME ) )
	{
		print( FATAL, "Failure to initialize the pulser: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	/* Channels associated with unused functions have been set to output no
	   pulses, now we can remove the association between the function and
	   the channel */

	if ( ! rs690.is_cw_mode )
		for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		{
			f = rs690.function + i;

			if ( f->is_used || f->num_channels == 0 )
				continue;

			for ( j = 0; j < f->num_channels; j++ )
			{
				f->channel[ j ]->function = NULL;
				f->channel[ j ] = NULL;
			}

			f->num_channels = 0;
		}


	rs690_run( SET );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int rs690_end_of_exp_hook( void )
{
	if ( ! rs690_is_needed )
		return 1;

	rs690_run( UNSET );
	gpib_local( rs690.device );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void rs690_exit_hook( void )
{
	PULSE *p;
	FUNCTION *f;
	int i, j;


	if ( rs690.dump_file != NULL )
	{
		fclose( rs690.dump_file );
		rs690.dump_file = NULL;
	}

	if ( rs690.show_file != NULL )
	{
		fclose( rs690.show_file );
		rs690.show_file = NULL;
	}

	if ( ! rs690_is_needed )
		return;

	/* Free all memory that may have been allocated for the module */

	for ( p = rs690_Pulses; p != NULL;  )
		p= rs690_delete_pulse( p, UNSET );

	rs690_Pulses = NULL;

	for ( i = 0; i < MAX_CHANNELS; i++ )
		rs690.channel[ i ].pulse_params =
					PULSE_PARAMS_P T_free( rs690.channel[ i ].pulse_params );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rs690.function + i;

		if ( f->pulses != NULL )
			f->pulses = PULSE_PP T_free( f->pulses );

		if ( f->pm != NULL )
		{
			for ( j = 0; j < f->pc_len * f->num_channels; j++ )
				if ( f->pm[ j ] != NULL )
					T_free( f->pm[ j ] );
			f->pm = PULSE_PPP T_free( f->pm );
		}
	}
}


/*----------------------------------------------------*/
/* Here come the EDL functions exported by the driver */
/*----------------------------------------------------*/


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
	double dl, dr;


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

	/* Check that a channel has been set for shape pulses */

	if ( rs690.function[ PULSER_CHANNEL_PULSE_SHAPE ].num_channels == 0 )
	{
		print( FATAL, "No channel has been set for function '%s' needed for "
			   "creating shape pulses.\n",
			   Function_Names[ PULSER_CHANNEL_PULSE_SHAPE ] );
		THROW( EXCEPTION );
	}

	/* Complain if automatic shape pulses have already been switched on for
	   the function */

	if ( rs690.function[ func ].uses_auto_shape_pulses )
	{
		print( FATAL, "Use of automatic shape pulses for function '%s' has "
			   "already been switched on.\n", Function_Names[ func ] );
		THROW( EXCEPTION );
	}

	rs690.auto_shape_pulses = SET;
	rs690.function[ func ].uses_auto_shape_pulses = SET;

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
		rs690.function[ func ].left_shape_padding = rs690_double2ticks( dl );

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			dr = get_double( v, "right side shape pulse padding" );
			if ( dr < 0 )
			{
				print( FATAL, "Can't use negative right side shape pulse "
					   "padding.\n" );
				THROW( EXCEPTION );
			}
			rs690.function[ func ].right_shape_padding =
													  rs690_double2ticks( dr );
		}
		else
			rs690.function[ func ].right_shape_padding =
				 Ticksrnd( ceil( AUTO_SHAPE_RIGHT_PADDING / rs690.timebase ) );
	}
	else
	{
		rs690.function[ func ].left_shape_padding =
				  Ticksrnd( ceil( AUTO_SHAPE_LEFT_PADDING / rs690.timebase ) );
		rs690.function[ func ].right_shape_padding =
				 Ticksrnd( ceil( AUTO_SHAPE_RIGHT_PADDING / rs690.timebase ) );
	}

	too_many_arguments( v );

	rs690.function[ func ].min_left_shape_padding = 
									 rs690.function[ func ].left_shape_padding;
	rs690.function[ func ].min_right_shape_padding =
									rs690.function[ func ].right_shape_padding;

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_automatic_twt_pulses( Var *v )
{
	long func;
	double dl, dr;


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

	/* Check that a channel has been set for TWT pulses */

	if ( rs690.function[ PULSER_CHANNEL_TWT ].num_channels == 0 )
	{
		print( FATAL, "No channel has been set for function '%s' needed for "
			   "creating TWT pulses.\n",
			   Function_Names[ PULSER_CHANNEL_TWT ] );
		THROW( EXCEPTION );
	}

	/* Complain if automatic TWT pulses have already been switched on for
	   the function */

	if ( rs690.function[ func ].uses_auto_twt_pulses )
	{
		print( FATAL, "Use of automatic TWT pulses for function '%s' has "
			   "already been switched on.\n", Function_Names[ func ] );
		THROW( EXCEPTION );
	}

	rs690.auto_twt_pulses = SET;
	rs690.function[ func ].uses_auto_twt_pulses = SET;

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
		rs690.function[ func ].left_twt_padding = rs690_double2ticks( dl );

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			dr = get_double( v, "right side TWT pulse padding" );
			if ( dr < 0 )
			{
				print( FATAL, "Can't use negative right side TWT pulse "
					   "padding.\n" );
				THROW( EXCEPTION );
			}
			rs690.function[ func ].right_twt_padding =
													  rs690_double2ticks( dr );
		}
		else
			rs690.function[ func ].right_twt_padding =
				   Ticksrnd( ceil( AUTO_TWT_RIGHT_PADDING / rs690.timebase ) );
	}
	else
	{
		rs690.function[ func ].left_twt_padding =
					Ticksrnd( ceil( AUTO_TWT_LEFT_PADDING / rs690.timebase ) );
		rs690.function[ func ].right_twt_padding =
				   Ticksrnd( ceil( AUTO_TWT_RIGHT_PADDING / rs690.timebase ) );
	}

	too_many_arguments( v );

	rs690.function[ func ].min_left_twt_padding = 
									   rs690.function[ func ].left_twt_padding;
	rs690.function[ func ].min_right_twt_padding =
									  rs690.function[ func ].right_twt_padding;

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

	if ( rs690.show_file != NULL )
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
	rs690.show_file = fdopen( pd[ 1 ], "w" );

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

	if ( rs690.dump_file != NULL )
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

		if ( ( rs690.dump_file = fopen( name, "w+" ) ) == NULL )
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
	} while ( rs690.dump_file == NULL );

	T_free( name );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_shape_to_defense_minimum_distance( Var *v )
{
	double s2d;


	if ( rs690.is_shape_2_defense )
	{
		print( FATAL, "SHAPE to DEFENSE pulse minimum distance has already "
			   "been set to %s.\n",
			   rs690_pticks( rs690.shape_2_defense ) );
		THROW( EXCEPTION );
	}

	s2d = get_double( v, "SHAPE to DEFENSE pulse minimum distance" );

	if ( s2d < 0 )
	{
		print( FATAL, "Negative SHAPE to DEFENSE pulse minimum distance.\n" );
		THROW( EXCEPTION );
	}

	rs690.shape_2_defense = rs690_double2ticks( s2d );
	rs690.is_shape_2_defense = SET;

	return vars_push( FLOAT_VAR, s2d );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_defense_to_shape_minimum_distance( Var *v )
{
	double d2s;


	if ( rs690.is_defense_2_shape )
	{
		print( FATAL, "DEFENSE to SHAPE pulse minimum distance has already "
			   "been set to %s.\n", rs690_pticks( rs690.defense_2_shape ) );
		THROW( EXCEPTION );
	}

	d2s = get_double( v, "DEFENSE to SHAPE pulse minimum distance" );

	if ( d2s < 0 )
	{
		print( FATAL, "Negative DEFENSE to SHAPE pulse minimum distance.\n" );
		THROW( EXCEPTION );
	}

	rs690.defense_2_shape = rs690_double2ticks( d2s );
	rs690.is_defense_2_shape = SET;

	return vars_push( FLOAT_VAR, d2s );
}


/*---------------------------------------------*/
/* Switches the output of the pulser on or off */
/*---------------------------------------------*/

Var *pulser_state( Var *v )
{
	bool state;


	if ( v == NULL )
		return vars_push( INT_VAR, 1 );

	state = get_boolean( v );

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( INT_VAR, 1 );

	rs690_run( state );
	return vars_push( INT_VAR, state  );
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var *pulser_channel_state( Var *v )
{
	v = v;
	print( SEVERE, "Individual channels can't be switched on or off for "
		   "this device.\n" );
	return vars_push( INT_VAR, 0 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_update( Var *v )
{
	v = v;


	if ( rs690.is_cw_mode )
	{
		print( FATAL, "Function can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs690_is_needed )
		return vars_push( INT_VAR, 1 );

	/* Send all changes to the pulser */

	if ( rs690.needs_update )
		return vars_push( INT_VAR, rs690_do_update( ) ? 1 : 0 );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *pulser_cw_mode( Var *v )
{
	v = v;


	/* We can't have any pulses in cw mode */

	if ( rs690_Pulses != NULL )
	{
		print( FATAL, "CW mode and use of pulses is mutually exclusive.\n" );
		THROW( EXCEPTION );
	}

	/* We need the function MICROWAVE defined for cw mode */

	if ( ! rs690.function[ PULSER_CHANNEL_MW ].is_used )
	{
		print( FATAL, "Function 'MICROWAVE' has not been defined as needed "
			   "for CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( rs690.is_trig_in_mode && rs690.trig_in_mode == EXTERNAL )
	{
		print( FATAL, "External trigger mode can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( rs690.is_repeat_time )
	{
		print( FATAL, "Repeat time/frequency can't be set in CW mode.\n" );
		THROW( EXCEPTION );
	}

	rs690.is_cw_mode = SET;
	rs690.trig_in_mode = INTERNAL;
	rs690.is_trig_in_mode = SET;

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------------------*/
/* Public function to change the position of pulses. If called with no  */
/* argument all active pulses that have a position change time set will */
/* be moved, otherwise all pulses passed as arguments to the function.  */
/* Take care: The changes will only be commited on the next call of the */
/*            function pulser_update() !                                */
/*----------------------------------------------------------------------*/

Var *pulser_shift( Var *v )
{
	PULSE *p;


	if ( rs690.is_cw_mode )
	{
		print( FATAL, "Pulses can't be shifted in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs690_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
		for ( p = rs690_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dpos )
				pulser_shift( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = rs690_get_pulse( get_strict_long( v, "pulse number" ) );

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
				   p->num, rs690_pticks( p->pos ) );
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
			rs690.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------------*/
/* Public function for incrementing the length of pulses. If called with   */
/* no argument all active pulses that have a length change defined are     */
/* incremented, oltherwise all pulses passed as arguments to the function. */
/* Take care: The changes will only be commited on the next call of the    */
/*            function pulser_update() !                                   */
/*-------------------------------------------------------------------------*/

Var *pulser_increment( Var *v )
{
	PULSE *p;


	if ( rs690.is_cw_mode )
	{
		print( FATAL, "Pulses can't be changed in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs690_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
		for ( p = rs690_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dlen )
				pulser_increment( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = rs690_get_pulse( get_strict_long( v, "pulse number" ) );

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
				   p->num, rs690_pticks( p->len ) );
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
			rs690.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_pulse_reset( Var *v )
{
	PULSE *p;


	if ( rs690.is_cw_mode )
	{
		print( FATAL, "Function can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs690_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to reset all pulses (even the
       inactive ones). Do't explicitely reset automatically created pulses
	   (which all have a negative pulse number), they will be reset together
	   with the pulses belong to. */

	if ( v == NULL )
	{
		for ( p = rs690_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				pulser_pulse_reset( vars_push( INT_VAR, p->num ) );
	}

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = rs690_get_pulse( get_strict_long( v, "pulse number" ) );

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
			rs690.needs_update = SET;
	}

	pulser_update( NULL );
	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_next_phase( Var *v )
{
	FUNCTION *f;
	long phase_number;


	if ( rs690.is_cw_mode )
	{
		print( FATAL, "Function can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs690_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		long res = 1;


		if ( rs690_phs[ 0 ].function == NULL &&
			 rs690_phs[ 1 ].function == NULL &&
			 FSC2_MODE == TEST )
		{
			print( SEVERE, "Phase cycling isn't used for any function.\n" );
			return vars_push( INT_VAR, 0 );
		}

		if ( rs690_phs[ 0 ].function != NULL )
			res &= pulser_next_phase( vars_push( INT_VAR, 1 ) )->val.lval;
		if ( rs690_phs[ 1 ].function != NULL )
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

		if ( ! rs690_phs[ phase_number - 1 ].is_defined )
		{
			print( FATAL, "PHASE_SETUP_%ld has not been defined.\n",
				   phase_number );
			THROW( EXCEPTION );
		}

		f = rs690_phs[ phase_number - 1 ].function;

		if ( ! f->is_used )
		{
			if ( FSC2_MODE == TEST )
				print( SEVERE, "Function '%s' to be phase cycled is not "
					   "used.\n", f->name );
			return vars_push( INT_VAR, 0 );
		}

		if ( ++f->next_phase >= f->pc_len )
			f->next_phase = 0;
	}

	rs690.needs_update = SET;
	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_phase_reset( Var *v )
{
	FUNCTION *f;
	long phase_number;


	if ( rs690.is_cw_mode )
	{
		print( FATAL, "Function can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! rs690_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		long ret = 1;


		if ( rs690_phs[ 0 ].function == NULL &&
			 rs690_phs[ 1 ].function == NULL &&
			 FSC2_MODE == TEST )
		{
			print( SEVERE, "Phase cycling isn't used for any function.\n" );
			return vars_push( INT_VAR, 0 );
		}

		if ( rs690_phs[ 0 ].function != NULL )
			ret &= pulser_phase_reset( vars_push( INT_VAR, 1 ) )->val.lval;
		if ( rs690_phs[ 1 ].function != NULL )
			ret &= pulser_phase_reset( vars_push( INT_VAR, 2 ) )->val.lval;
		return vars_push( INT_VAR, ret );
	}

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		phase_number = get_strict_long( v, "phase number" );

		if ( phase_number != 1 && phase_number != 2 )
		{
			print( FATAL, "Invalid phase number: %ld.\n", phase_number );
			THROW( EXCEPTION );
		}

		f = rs690_phs[ phase_number - 1 ].function;

		if ( ! f->is_used )
		{
			if ( FSC2_MODE == TEST )
				print( SEVERE, "Function '%s' to be phase cycled is not "
					   "used.\n", f->name );
			return vars_push( INT_VAR, 0 );
		}

		f->next_phase = 0;
	}

	rs690.needs_update = SET;
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
		rs690_lock_state( lock );

	return vars_push( INT_VAR, lock ? 1 : 0 );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
