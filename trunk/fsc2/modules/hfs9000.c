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


#define HFS9000_MAIN

#include "hfs9000.h"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;



/*----------------------------------------------------------------------
  This function is called directly after all modules are loaded. It
  initializes all global variables that are needed in the module.
----------------------------------------------------------------------*/

int hfs9000_init_hook( void )
{
	int i;


	pulser_struct.name     = DEVICE_NAME;
	pulser_struct.has_pods = UNSET;

	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* We have to set up the global structure for the pulser, especially the
	   pointers for the functions that will get called from pulser.c */

	hfs9000.needs_update   = UNSET;
	hfs9000.is_running     = SET;
	hfs9000.keep_all       = UNSET;
	hfs9000.stop_on_update = SET;

	pulser_struct.set_timebase = hfs9000_store_timebase;

	pulser_struct.assign_function = NULL;
	pulser_struct.assign_channel_to_function
		= hfs9000_assign_channel_to_function;
	pulser_struct.invert_function = hfs9000_invert_function;
	pulser_struct.set_function_delay = hfs9000_set_function_delay;
	pulser_struct.set_function_high_level = hfs9000_set_function_high_level;
	pulser_struct.set_function_low_level = hfs9000_set_function_low_level;

	pulser_struct.set_trigger_mode = hfs9000_set_trigger_mode;
	pulser_struct.set_repeat_time = NULL;
	pulser_struct.set_trig_in_level = hfs9000_set_trig_in_level;
	pulser_struct.set_trig_in_slope = hfs9000_set_trig_in_slope;
	pulser_struct.set_trig_in_impedance = NULL;
	pulser_struct.set_max_seq_len = hfs9000_set_max_seq_len;

	pulser_struct.set_phase_reference = NULL;

	pulser_struct.new_pulse = hfs9000_new_pulse;
	pulser_struct.set_pulse_function = hfs9000_set_pulse_function;
	pulser_struct.set_pulse_position = hfs9000_set_pulse_position;
	pulser_struct.set_pulse_length = hfs9000_set_pulse_length;
	pulser_struct.set_pulse_position_change =
		hfs9000_set_pulse_position_change;
	pulser_struct.set_pulse_length_change = hfs9000_set_pulse_length_change;
	pulser_struct.set_pulse_phase_cycle = NULL;
	pulser_struct.set_grace_period = NULL;

	pulser_struct.get_pulse_function = hfs9000_get_pulse_function;
	pulser_struct.get_pulse_position = hfs9000_get_pulse_position;
	pulser_struct.get_pulse_length = hfs9000_get_pulse_length;
	pulser_struct.get_pulse_position_change =
		hfs9000_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = hfs9000_get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = NULL;

	pulser_struct.phase_setup_prep = NULL;
	pulser_struct.phase_setup = NULL;

	pulser_struct.set_phase_switch_delay = NULL;

	pulser_struct.keep_all_pulses = hfs9000_keep_all;

	/* Finally, we initialize variables that store the state of the pulser */

	hfs9000.trig_in_mode = EXTERNAL;

	hfs9000.is_timebase = UNSET;

	hfs9000.dump_file = NULL;
	hfs9000.show_file = NULL;

	hfs9000.is_trig_in_mode = UNSET;
	hfs9000.is_trig_in_slope = UNSET;
	hfs9000.is_trig_in_level = UNSET;
	hfs9000.is_neg_delay = UNSET;
	hfs9000.neg_delay = 0;
	hfs9000.max_seq_len = 0;
	hfs9000.is_max_seq_len = UNSET;

	for ( i = 0; i <= MAX_CHANNEL; i++ )
	{
		hfs9000.channel[ i ].self = i;
		hfs9000.channel[ i ].function = NULL;
		hfs9000.channel[ i ].state = SET;
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		hfs9000.function[ i ].self = i;
		hfs9000.function[ i ].name = Function_Names[ i ];
		hfs9000.function[ i ].is_used = UNSET;
		hfs9000.function[ i ].is_needed = UNSET;
		hfs9000.function[ i ].channel = NULL;
		hfs9000.function[ i ].num_pulses = 0;
		hfs9000.function[ i ].num_active_pulses = 0;
		hfs9000.function[ i ].pulses = NULL;
		hfs9000.function[ i ].max_seq_len = 0;
		hfs9000.function[ i ].is_inverted = UNSET;
		hfs9000.function[ i ].delay = 0;
		hfs9000.function[ i ].is_delay = UNSET;
		hfs9000.function[ i ].is_high_level = UNSET;
		hfs9000.function[ i ].is_low_level = UNSET;
	}

	hfs9000_is_needed = SET;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int hfs9000_test_hook( void )
{
	if ( hfs9000_Pulses == NULL )
	{
		hfs9000_is_needed = UNSET;
		print( WARN, "Driver loaded but no pulses are defined.\n" );
		return 1;
	}

	/* Check consistency of pulse settings and do everything to setup the
	   pulser for the test run */

	TRY
	{
		hfs9000_IN_SETUP = SET;
		hfs9000_init_setup( );
		hfs9000_IN_SETUP = UNSET;
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( hfs9000.dump_file )
		{
			fclose( hfs9000.dump_file );
			hfs9000.dump_file = NULL;
		}

		if ( hfs9000.show_file )
		{
			fclose( hfs9000.show_file );
			hfs9000.show_file = NULL;
		}

		hfs9000_IN_SETUP = UNSET;
		RETHROW( );
	}


	/* We need some somewhat different functions (or disable some) for
	   setting the pulse properties */

	pulser_struct.set_pulse_function = NULL;

	pulser_struct.set_pulse_position = hfs9000_change_pulse_position;
	pulser_struct.set_pulse_length = hfs9000_change_pulse_length;
	pulser_struct.set_pulse_position_change =
		hfs9000_change_pulse_position_change;
	pulser_struct.set_pulse_length_change = hfs9000_change_pulse_length_change;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int hfs9000_end_of_test_hook( void )
{
	if ( hfs9000.dump_file != NULL )
	{
		fclose( hfs9000.dump_file );
		hfs9000.dump_file = NULL;
	}

	if ( hfs9000.show_file != NULL )
	{
		fclose( hfs9000.show_file );
		hfs9000.show_file = NULL;
	}

	if ( ! hfs9000_is_needed )
		return 1;

	/* First we have to reset the internal representation back to its initial
	   state */

	hfs9000_full_reset( );

	/* Now we've got to find out about the maximum sequence length */

	hfs9000.max_seq_len = hfs9000_get_max_seq_len( );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int hfs9000_exp_hook( void )
{
	int i;


	if ( ! hfs9000_is_needed )
		return 1;

	/* Initialize the device */

	if ( ! hfs9000_init( DEVICE_NAME ) )
	{
		print( FATAL, "Failure to initialize the pulser: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	/* Now we have to tell the pulser about all the pulses and switch on
	   all needed channels */

	for ( i = 0; i <= MAX_CHANNEL; i++ )
		if ( hfs9000.channel[ i ].function != NULL &&
			 hfs9000.channel[ i ].function->is_used )
		{
			hfs9000_set_pulses( hfs9000.channel[ i ].function );
			if ( i != HFS9000_TRIG_OUT )
				hfs9000_set_channel_state( i, hfs9000.channel[ i ].state );
		}
		else
		{
			hfs9000.channel[ i ].state = UNSET;
			if ( i != HFS9000_TRIG_OUT )
				hfs9000_set_channel_state( i, hfs9000.channel[ i ].state );
		}


	hfs9000_run( SET );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int hfs9000_end_of_exp_hook( void )
{
	const char *cmd = "FPAN:MESS \"\"\n";


	if ( ! hfs9000_is_needed )
		return 1;

	gpib_write( hfs9000.device, cmd, strlen( cmd ) );
	hfs9000_run( hfs9000.has_been_running );
	gpib_local( hfs9000.device );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void hfs9000_exit_hook( void )
{
	PULSE *p, *np;
	int i;


	if ( ! hfs9000_is_needed )
		return;

	/* Free all the memory allocated within the module */

	for ( p = hfs9000_Pulses; p != NULL; p = np )
	{
		np = p->next;
		T_free( p );
	}

	hfs9000_Pulses = NULL;

	for ( i = 0; i <= MAX_CHANNEL; i++ )
		if ( hfs9000.channel[ i ].function != NULL &&
			 hfs9000.channel[ i ].function->pulses != NULL )
			T_free( hfs9000.channel[ i ].function->pulses );
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

Var *pulser_show_pulses( Var *v )
{
	int pd[ 2 ];
	pid_t pid;


	v = v;

	if ( FSC2_IS_CHECK_RUN )
		return vars_push( INT_VAR, 1 );

	if ( hfs9000.show_file != NULL )
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
	hfs9000.show_file = fdopen( pd[ 1 ], "w" );

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

	if ( hfs9000.dump_file != NULL )
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

		if ( ( hfs9000.dump_file = fopen( name, "w+" ) ) == NULL )
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
	} while ( hfs9000.dump_file == NULL );

	T_free( name );

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------*/
/* Switches the output of the pulser on or off */
/*---------------------------------------------*/

Var *pulser_state( Var *v )
{
	bool state;


	if ( v == NULL )
		return vars_push( INT_VAR, ( long ) hfs9000.is_running );

	state = get_boolean( v );

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( INT_VAR, ( long ) ( hfs9000.is_running = state ) );

	hfs9000_run( state );
	return vars_push( INT_VAR, ( long ) hfs9000.is_running );
}


/*------------------------------------------------------------*/
/* Switches an individual channel of the pulser on or off or, */
/* if called with just one argument, returns the state.       */
/*------------------------------------------------------------*/

Var *pulser_channel_state( Var *v )
{
	int channel;
	bool state;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	channel = get_strict_long( v, "pulser channel" );

	if ( channel < MIN_CHANNEL || channel > MAX_CHANNEL )
	{
		print( FATAL, "Invalid channel parameter.\n" );
		THROW( EXCEPTION );
	}

	v = vars_pop( v );
	if ( v == NULL )
		return vars_push( INT_VAR,
						  ( long ) ( hfs9000.channel[ channel ].state ?
									 1 : 0 ) );

	if ( v->type == INT_VAR )
		state = v->val.lval != 0 ? SET : UNSET;
	else if ( v->type == FLOAT_VAR )
		state = v->val.dval != 0.0 ? SET : UNSET;
	else
	{
		if ( ! strcasecmp( v->val.sptr, "OFF" ) )
			 state = UNSET;
		else if ( ! strcasecmp( v->val.sptr, "ON" ) )
			state = SET;
		else
		{
			print( FATAL, "Invalid argument.\n" );
			THROW( EXCEPTION );
		}
	}

	hfs9000.channel[ channel].state = state;

	if ( FSC2_MODE == EXPERIMENT )
		hfs9000_set_channel_state( channel, state );

	return vars_push( INT_VAR, ( long ) state );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_update( Var *v )
{
	v = v;


	if ( ! hfs9000_is_needed )
		return vars_push( INT_VAR, 1 );

	/* Send all changes to the pulser */

	if ( hfs9000.needs_update )
		return vars_push( INT_VAR, hfs9000_do_update( ) ? 1 : 0 );

	return vars_push( INT_VAR, 1 );
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


	if ( ! hfs9000_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
		for ( p = hfs9000_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dpos )
				pulser_shift( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = hfs9000_get_pulse( get_strict_long( v, "pulse number" ) );

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
				   p->num, hfs9000_pticks( p->pos ) );
			THROW( EXCEPTION );
		}

		if ( p->pos == p->old_pos )       /* nothing really changed ? */
			p->is_old_pos = UNSET;

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
		p->needs_update = NEEDS_UPDATE( p );

		if ( p->needs_update )
			hfs9000.needs_update = SET;
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


	if ( ! hfs9000_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
		for ( p = hfs9000_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dlen )
				pulser_increment( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = hfs9000_get_pulse( get_strict_long( v, "pulse number" ) );

		if ( ! p->is_len )
		{
			print( FATAL, "Pulse #%ld has no length set, so incrementing its "
				   "length is impossibe.\n", p->num );
			THROW( EXCEPTION );
		}

		if ( p->channel->self == HFS9000_TRIG_OUT )
		{
			print( FATAL, "Length of Trigger Out pulse #%ld can't be "
				   "changed.\n", p->num );
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
				   p->num, hfs9000_pticks( p->len ) );
			THROW( EXCEPTION );
		}

		if ( p->old_len == p->len )
			p->is_old_len = UNSET;

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
		p->needs_update = NEEDS_UPDATE( p );

		/* If the pulse was or is active we've got to update the pulser */

		if ( p->needs_update )
			hfs9000.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_reset( Var *v )
{
	v = v;

	vars_pop( pulser_pulse_reset( NULL ) );
	return pulser_update( NULL );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_pulse_reset( Var *v )
{
	PULSE *p;


	if ( ! hfs9000_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to reset all pulses (even the
       inactive ones) */

	if ( v == NULL )
	{
		for ( p = hfs9000_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				pulser_pulse_reset( vars_push( INT_VAR, p->num ) );
	}

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = hfs9000_get_pulse( get_strict_long( v, "pulse number" ) );

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

		if ( p->needs_update )
			hfs9000.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_lock_keyboard( Var *v )
{
	v = v;

	print( SEVERE, "Function can't be used for this device.\n" );
	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_stop_on_update( Var *v )
{
	hfs9000.stop_on_update = get_strict_long( v, "boolean value" ) == 0 ?
		                     UNSET : SET;

	return vars_push( INT_VAR, ( long ) hfs9000.stop_on_update );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_command( Var *v )
{
	static char *cmd;


	cmd = NULL;
	vars_check( v, STR_VAR );
	
	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
			hfs9000_command( cmd );
			T_free( cmd );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( cmd );
			RETHROW( );
		}
	}

	return vars_push( 1 );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
