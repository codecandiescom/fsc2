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


#define DG2020_F_MAIN


#include "dg2020_f.h"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/*---------------------------------------------------------------------------
  This function is called directly after all modules are loaded. Its main
  function is to initialize all global variables that are needed in the
  module.
---------------------------------------------------------------------------*/

int dg2020_f_init_hook( void )
{
	int i, j, k;


	pulser_struct.name     = DEVICE_NAME;
	pulser_struct.has_pods = SET;

	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Than we have to set up the global structure for the pulser, especially
	   we have to set the pointers for the functions that will get called from
	   pulser.c */

	dg2020.needs_update = UNSET;
	dg2020.is_running   = SET;
	dg2020.keep_all     = UNSET;

	pulser_struct.needs_phase_pulses = SET;

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
	pulser_struct.set_grace_period = dg2020_set_grace_period;

	pulser_struct.get_pulse_function = dg2020_get_pulse_function;
	pulser_struct.get_pulse_position = dg2020_get_pulse_position;
	pulser_struct.get_pulse_length = dg2020_get_pulse_length;
	pulser_struct.get_pulse_position_change = dg2020_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = dg2020_get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = dg2020_get_pulse_phase_cycle;

	pulser_struct.phase_setup_prep = dg2020_phase_setup_prep;
	pulser_struct.phase_setup = dg2020_phase_setup;

	pulser_struct.set_phase_switch_delay = dg2020_set_phase_switch_delay;

	pulser_struct.keep_all_pulses = dg2020_keep_all;

	/* Finally, we initialize variables that store the state of the pulser */

	dg2020.is_timebase = UNSET;
	dg2020.is_trig_in_mode = UNSET;
	dg2020.is_trig_in_slope = UNSET;
	dg2020.is_trig_in_level = UNSET;
	dg2020.is_repeat_time = UNSET;
	dg2020.is_neg_delay = UNSET;
	dg2020.neg_delay = 0;
	dg2020.is_grace_period = UNSET;
	dg2020.is_max_seq_len = UNSET;

	dg2020.dump_file = NULL;
	dg2020.show_file = NULL;

	dg2020.block[ 0 ].is_used = dg2020.block[ 1 ].is_used = UNSET;

	for ( i = 0; i < MAX_PODS; i++ )
	{
		dg2020.pod[ i ].self = i;
		dg2020.pod[ i ].function = NULL;
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		dg2020.function[ i ].self = i;
		dg2020.function[ i ].name = Function_Names[ i ];
		dg2020.function[ i ].is_used = UNSET;
		dg2020.function[ i ].is_needed = UNSET;
		dg2020.function[ i ].pod = NULL;
		dg2020.function[ i ].is_phs = UNSET;
		dg2020.function[ i ].is_psd = UNSET;
		dg2020.function[ i ].num_channels = 0;
		dg2020.function[ i ].num_pulses = 0;
		dg2020.function[ i ].pulses = NULL;
		dg2020.function[ i ].next_phase = 0;
		dg2020.function[ i ].phase_func = NULL;
		dg2020.function[ i ].is_inverted = UNSET;
		dg2020.function[ i ].delay = 0;
		dg2020.function[ i ].is_delay = UNSET;
		dg2020.function[ i ].is_high_level = UNSET;
		dg2020.function[ i ].is_low_level = UNSET;
	}

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		dg2020.channel[ i ].self = i;
		dg2020.channel[ i ].function = NULL;
	}

	for ( i = 0; i < 3; i++ )
		for ( j = 0; j < 4; j++ )
			for ( k = 0; k < 2; k++ )
				phs[ i ].is_var[ j ][ k ] = UNSET;

	dg2020_is_needed = SET;

	phase_numbers[ 0 ] = phase_numbers[ 2 ] = -1;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_f_test_hook( void )
{
	if ( dg2020_Pulses == NULL )
	{
		dg2020_is_needed = UNSET;
		print( WARN, "Driver loaded but no pulses are defined.\n" );
		return 1;
	}

	/* Check consistency of pulser settings and do everything to setup the
	   pulser for the test run */

	TRY
	{
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


	/* We need some somewhat different functions (or disable some) for setting
	   of pulse properties */

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

int dg2020_f_end_of_test_hook( void )
{
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

	if ( ! dg2020_is_needed )
		return 1;

	/* First we have to reset the internal representation back to its initial
	   state */

	dg2020_full_reset( );

	/* Now we've got to find out about the maximum sequence length, set up
	   padding to achieve the repeat time and set the lengths of the last
	   phase pulses in the channels needing phase cycling */

	dg2020.max_seq_len = dg2020_get_max_seq_len( );
	dg2020_calc_padding( );
	dg2020_finalize_phase_pulses( PULSER_CHANNEL_PHASE_1 );
	dg2020_finalize_phase_pulses( PULSER_CHANNEL_PHASE_2 );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_f_exp_hook( void )
{
	int i;


	if ( ! dg2020_is_needed )
		return 1;

	/* Initialize the device */

	if ( ! dg2020_init( DEVICE_NAME ) )
	{
		print( FATAL, "Failure to initialize the pulser: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	/* Now we have to tell the pulser about all the pulses */

	dg2020_IN_SETUP = SET;
	if ( ! dg2020_reorganize_pulses( UNSET ) )
	{
		dg2020_IN_SETUP = UNSET;
		THROW( EXCEPTION );
	}
	dg2020_IN_SETUP = UNSET;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		if ( ! dg2020.function[ i ].is_used )
			continue;
		dg2020_set_pulses( dg2020.function + i );
		dg2020_clear_padding_block( dg2020.function + i );
	}

	/* Finally tell the pulser to update (we're always running in manual
	   update mode) and than switch the pulser into run mode */

	dg2020_update_data( );
	dg2020_run( dg2020.is_running );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_f_end_of_exp_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

	dg2020_run( STOP );
    gpib_local( dg2020.device );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void dg2020_f_exit_hook( void )
{
	PULSE *p, *np;
	int i;


	if ( ! dg2020_is_needed )
		return;

	/* free all the memory allocated within the module */

	for ( p = dg2020_Pulses; p != NULL; np = p->next, T_free( p ), p = np )
		/* empty */ ;
	dg2020_Pulses = NULL;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		if ( dg2020.function[ i ].pulses != NULL )
			T_free( dg2020.function[ i ].pulses );
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
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

Var *pulser_state( Var *v )
{
	bool state;


	if ( v == NULL )
		return vars_push( INT_VAR, ( long ) dg2020.is_running );

	state = get_boolean( v );

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( INT_VAR, ( long ) ( dg2020.is_running = state ) );

	dg2020_run( state );
	return vars_push( INT_VAR, ( long ) dg2020.is_running );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_channel_state( Var *v )
{
	v = v;
	print( SEVERE, "Individual pod channels can't be switched on or off for "
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

	if ( FSC2_MODE == EXPERIMENT && ! dg2020_run( START ) )
	{
		print( FATAL, "Communication with pulser failed.\n" );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, state ? 1 : 0 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_shift( Var *v )
{
	PULSE *p;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
		for( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dpos )
				pulser_shift( vars_push( INT_VAR, p->num ) );

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
			print( FATAL, "Time for position change hasn't been defined for "
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

		if ( p->needs_update )
			dg2020.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_increment( Var *v )
{
	PULSE *p;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
		for( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dlen )
				pulser_increment( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = dg2020_get_pulse( get_strict_long( v, "pulse number" ) );

		if ( ! p->is_len )
		{
			print( FATAL, "Pulse #%ld has no length set, so imcrementing it "
				   "is impossibe.\n", p->num );
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
				   p->num, dg2020_pticks( p->len ) );
			THROW( EXCEPTION );
		}

		if ( p->old_len == p->len )
			p->is_old_len = UNSET;

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
		p->needs_update = NEEDS_UPDATE( p );

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
	FUNCTION *f;
	long phase_number;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		if ( ! dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used &&
			 ! dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used &&
			 FSC2_MODE == TEST )
		{
			print( SEVERE, "No phase functions are in use.\n" );
			return vars_push( INT_VAR, 0 );
		}

		if ( dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used )
			pulser_next_phase( vars_push( INT_VAR, 1 ) );
		if ( dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used )
			pulser_next_phase( vars_push( INT_VAR, 2 ) );
	}

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		phase_number = get_strict_long( v, "phase number" );

		if ( phase_number != 1 && phase_number != 2 )
		{
			print( FATAL, "Invalid phase number: %ld.\n", phase_number );
			THROW( EXCEPTION );
		}

		f = dg2020.function + ( phase_number == 1 ? PULSER_CHANNEL_PHASE_1 :
								PULSER_CHANNEL_PHASE_2 );

		if ( ! f->is_used && FSC2_MODE == TEST )
		{
			print( SEVERE, "Phase function '%s' is not used.\n", f->name );
			return vars_push( INT_VAR, 0 );
		}

		if ( f->next_phase >= f->num_channels )
			f->next_phase = 0;

		if ( FSC2_MODE == EXPERIMENT )
		{
			if ( ! dg2020_channel_assign( f->channel[ f->next_phase++ ]->self,
										  f->pod->self ) ||
				 ! dg2020_channel_assign( f->channel[ f->next_phase++ ]->self,
										  f->pod2->self ) ||
				 ! dg2020_update_data( ) )
				return vars_push( INT_VAR, 0 );
		}
		else
		{
			f->next_phase += 2;
			if ( dg2020.dump_file != NULL )
				dg2020_dump_channels( dg2020.dump_file );
			if ( dg2020.show_file != NULL )
				dg2020_dump_channels( dg2020.show_file );
		}
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_phase_reset( Var *v )
{
	FUNCTION *f;
	long phase_number;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		if ( ! dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used &&
			 ! dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used &&
			 FSC2_MODE == TEST )
		{
			print( SEVERE, "No phase functions are in use.\n" );
			return vars_push( INT_VAR, 0 );
		}

		if ( dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used )
			pulser_phase_reset( vars_push( INT_VAR, 1 ) );
		if ( dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used )
			pulser_phase_reset( vars_push( INT_VAR, 2 ) );
	}

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		phase_number = get_strict_long( v, "phase number" );

		if ( phase_number != 1 && phase_number != 2 )
		{
			print( FATAL, "Invalid phase number: %ld.\n", phase_number );
			THROW( EXCEPTION );
		}

		f = dg2020.function + (  phase_number == 1 ? PULSER_CHANNEL_PHASE_1 :
								 PULSER_CHANNEL_PHASE_2 );

		if ( ! f->is_used && FSC2_MODE == TEST )
		{
			print( SEVERE, "Phase function '%s' is not used.\n", f->name );
			return vars_push( INT_VAR, 0 );
		}

		if ( FSC2_MODE == EXPERIMENT )
		{
			if ( ! dg2020_channel_assign( f->channel[ 0 ]->self,
										  f->pod->self ) ||
				 ! dg2020_channel_assign( f->channel[ 1 ]->self,
										  f->pod2->self ) ||
				 ! dg2020_update_data( ) )
				return vars_push( INT_VAR, 0 );
		}
		else
		{
			if ( dg2020.dump_file != NULL )
				dg2020_dump_channels( dg2020.dump_file );
			if ( dg2020.show_file != NULL )
				dg2020_dump_channels( dg2020.show_file );
		}

		f->next_phase = 2;
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
		for( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				pulser_pulse_reset( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = dg2020_get_pulse( get_strict_long( v, "pulse number" ) );

		/* Reset all changeable properties back to their initial values */

		if ( p->is_pos )
		{
			p->old_pos = p->pos;
			p->is_old_pos = SET;
		}
		p->pos = p->initial_pos;
		p->is_pos = p->initial_is_pos;

		if ( p->is_len )
		{
			p->old_len = p->len;
			p->is_old_len = SET;
		}

		p->len = p->initial_len;
		if ( p->len != 0.0 )
			p->is_len = p->initial_is_len;
		else
			p->is_len = UNSET;

		p->dpos = p->initial_dpos;
		p->is_dpos = p->initial_is_dpos;

		p->dlen = p->initial_dlen;
		p->is_dlen = p->initial_is_dlen;

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
		p->needs_update = NEEDS_UPDATE( p );

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
