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


#define EP385_MAIN


#include "ep385.h"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/*-------------------------------------------------------------------------*/
/* This function is called directly after all modules are loaded. Its main */
/* function is to initialize all global variables that are needed in the   */
/* module.                                                                 */
/*-------------------------------------------------------------------------*/

int ep385_init_hook( void )
{
	int i, j;


	fsc2_assert( SHAPE_2_DEFENSE_DEFAULT_MIN_DISTANCE > 0 );
	fsc2_assert( DEFENSE_2_SHAPE_DEFAULT_MIN_DISTANCE > 0 );

	pulser_struct.name     = DEVICE_NAME;
	pulser_struct.has_pods = UNSET;

	/* Set global variable to indicate that GPIB bus is needed */

//	need_GPIB = SET;

	/* We have to set up the global structure for the pulser, especially the
	   pointers for the functions that will get called from pulser.c */

	ep385.needs_update = SET;
	ep385.keep_all     = UNSET;
	ep385.is_cw_mode   = UNSET;

	pulser_struct.set_timebase = ep385_store_timebase;
	pulser_struct.assign_function = NULL;
	pulser_struct.assign_channel_to_function =
											  ep385_assign_channel_to_function;
	pulser_struct.invert_function = NULL;
	pulser_struct.set_function_delay = ep385_set_function_delay;
	pulser_struct.set_function_high_level = NULL;
	pulser_struct.set_function_low_level = NULL;

	pulser_struct.set_trigger_mode = ep385_set_trigger_mode;
	pulser_struct.set_repeat_time = ep385_set_repeat_time;
	pulser_struct.set_trig_in_level = NULL;
	pulser_struct.set_trig_in_slope = NULL;
	pulser_struct.set_trig_in_impedance = NULL;
	pulser_struct.set_max_seq_len = NULL;

	pulser_struct.set_phase_reference = ep385_set_phase_reference;

	pulser_struct.new_pulse = ep385_new_pulse;
	pulser_struct.set_pulse_function = ep385_set_pulse_function;
	pulser_struct.set_pulse_position = ep385_set_pulse_position;
	pulser_struct.set_pulse_length = ep385_set_pulse_length;
	pulser_struct.set_pulse_position_change = ep385_set_pulse_position_change;
	pulser_struct.set_pulse_length_change = ep385_set_pulse_length_change;
	pulser_struct.set_pulse_phase_cycle = ep385_set_pulse_phase_cycle;
	pulser_struct.set_grace_period = NULL;

	pulser_struct.get_pulse_function = ep385_get_pulse_function;
	pulser_struct.get_pulse_position = ep385_get_pulse_position;
	pulser_struct.get_pulse_length = ep385_get_pulse_length;
	pulser_struct.get_pulse_position_change = ep385_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = ep385_get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = ep385_get_pulse_phase_cycle;

	pulser_struct.phase_setup_prep = ep385_phase_setup_prep;
	pulser_struct.phase_setup = ep385_phase_setup;

	pulser_struct.set_phase_switch_delay = NULL;

	pulser_struct.keep_all_pulses = ep385_keep_all;

	/* Finally, we initialize variables that store the state of the pulser */

	ep385.is_trig_in_mode = UNSET;
	ep385.is_repeat_time = UNSET;
	ep385.is_neg_delay = UNSET;
	ep385.neg_delay = 0;
	ep385.is_running = SET;
	ep385.has_been_running = SET;
	ep385.is_timebase = UNSET;
	ep385.timebase_mode = EXTERNAL;

	ep385.is_shape_2_defense = UNSET;
	ep385.is_defense_2_shape = UNSET;
	ep385.shape_2_defense_too_near = UNSET;
	ep385.defense_2_shape_too_near = UNSET;
	ep385.is_confirmation = UNSET;

	ep385.dump_file = NULL;

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		ep385.channel[ i ].self = i;
		ep385.channel[ i ].function = NULL;
		ep385.channel[ i ].pulse_params = NULL;
		ep385.channel[ i ].num_pulses = 0;
		ep385.channel[ i ].num_active_pulses = 0;
		ep385.channel[ i ].needs_update = SET;
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		ep385.function[ i ].self = i;
		ep385.function[ i ].is_used = UNSET;
		ep385.function[ i ].is_needed = UNSET;
		for ( j = 0; j <= MAX_CHANNELS; j++ )
			ep385.function[ i ].channel[ j ] = NULL;
		ep385.function[ i ].num_pulses = 0;
		ep385.function[ i ].pulses = NULL;
		ep385.function[ i ].pm = NULL;
		ep385.function[ i ].delay = 0;
		ep385.function[ i ].is_delay = UNSET;
		ep385.function[ i ].next_phase = 0;
	}

	ep385_is_needed = SET;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int ep385_test_hook( void )
{
	/* Make sure that a timebase is set (shouldn't really be needed) */

	if ( ep385.is_cw_mode )
		return 1;

	if ( ! ep385.is_timebase )
	{
		ep385.is_timebase = SET;
		ep385.timebase_mode = INTERNAL;
		ep385.timebase = FIXED_TIMEBASE;

		ep385.shape_2_defense = ( Ticks )
				 lrnd( SHAPE_2_DEFENSE_DEFAULT_MIN_DISTANCE / FIXED_TIMEBASE );
		ep385.defense_2_shape = ( Ticks )
				 lrnd( DEFENSE_2_SHAPE_DEFAULT_MIN_DISTANCE / FIXED_TIMEBASE );
	}

	if ( ep385_Pulses == NULL && ! ep385.is_cw_mode )
	{
		ep385_is_needed = UNSET;
		print( WARN, "Driver loaded but no pulses are defined.\n" );
		return 1;
	}

	/* Check consistency of pulse settings and do everything to setup the
	   pulser for the test run */

	ep385_init_setup( );

	/* We need some somewhat different functions (or disable some) for
	   setting the pulse properties */

	pulser_struct.set_pulse_function = NULL;

	pulser_struct.set_pulse_position = ep385_change_pulse_position;
	pulser_struct.set_pulse_length = ep385_change_pulse_length;
	pulser_struct.set_pulse_position_change =
											ep385_change_pulse_position_change;
	pulser_struct.set_pulse_length_change = ep385_change_pulse_length_change;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int ep385_end_of_test_hook( void )
{
	if ( ep385.dump_file != NULL )
	{
		fclose( ep385.dump_file );
		ep385.dump_file = NULL;
	}

	if ( ! ep385_is_needed || ep385.is_cw_mode )
		return 1;

	/* Reset the internal representation back to its initial state */

	if ( ! ep385.is_cw_mode )
		ep385_full_reset( );

	/* If in the test it was found that shape and defense pulses got too
	   near to each other bail out */

	if ( ep385.shape_2_defense_too_near )
		print( FATAL, "Distance between PULSE_SHAPE and DEFENSE pulses was "
			   "shorter than %s during the test run.\n",
			   ep385_ptime( ep385_ticks2double( ep385.shape_2_defense ) ) );

	if ( ep385.defense_2_shape_too_near )
		print( FATAL, "Distance between DEFENSE and PULSE_SHAPE pulses was "
			   "shorter than %s during the test run.\n",
			   ep385_ptime( ep385_ticks2double( ep385.defense_2_shape ) ) );

	if ( ep385.shape_2_defense_too_near || ep385.defense_2_shape_too_near )
		THROW( EXCEPTION );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int ep385_exp_hook( void )
{
	int i, j;
	FUNCTION *f;


	if ( ! ep385_is_needed )
		return 1;

	/* Extra safety net: If the minimum distances between shape and defense
	   pulses have been changed by calling the appropriate functions ask
	   the user the first time the experiment gets started if (s)he is 100%
	   sure that's what (s)he really wants. This can be switched off by
	   commenting out the definition in config/ep385.conf of
	   "ASK_FOR_SHAPE_DEFENSE_DISTANCE_CONFORMATION" */

#if defined ASK_FOR_SHAPE_DEFENSE_DISTANCE_CONFORMATION
	if ( ! ep385.is_confirmation && 
		 ( ep385.is_shape_2_defense || ep385.is_defense_2_shape ) )
	{
		char str[ 500 ];

		if ( ep385.is_shape_2_defense && ep385.is_defense_2_shape )
		{
			sprintf( str, "Minimum distance between SHAPE and DEFENSE\n"
					 "pulses has been changed to %s",
					 ep385_ptime( 
						 ep385_ticks2double( ep385.shape_2_defense ) ) );
			sprintf( str + strlen( str ), " and %s.\n"
					 "***** Is this really what you want? *****",
					 ep385_ptime(
						 ep385_ticks2double( ep385.defense_2_shape ) ) );
		}
		else if ( ep385.is_shape_2_defense )
			sprintf( str, "Minimum distance between SHAPE and DEFENSE\n"
					 "pulses has been changed to %s.\n"
					 "***** Is this really what you want? *****",
					 ep385_ptime(
						 ep385_ticks2double( ep385.shape_2_defense ) ) );
		else
			sprintf( str, "Minimum distance between DEFENSE and SHAPE\n"
					 "pulses has been changed to %s.\n"
					 "***** Is this really what you want? *****",
					 ep385_ptime(
						 ep385_ticks2double( ep385.defense_2_shape ) ) );

		if ( 2 != show_choices( str, 2, "Abort", "Yes", "", 1 ) )
			THROW( EXCEPTION );

		ep385.is_confirmation = SET;
	}
#endif

	/* Initialize the device */

	if ( ! ep385_init( DEVICE_NAME ) )
	{
		print( FATAL, "Failure to initialize the pulser: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	/* Channels associated with unused functions have been set to output no
	   pulses, now we can remove the association between the function and
	   the channel */

	if ( ! ep385.is_cw_mode )
		for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		{
			f = ep385.function + i;

			if ( f->is_used || f->num_channels == 0 )
				continue;

			for ( j = 0; j < f->num_channels; j++ )
			{
				f->channel[ j ]->function = NULL;
				f->channel[ j ] = NULL;
			}

			f->num_channels = 0;
		}


	ep385_run( SET );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int ep385_end_of_exp_hook( void )
{
	const char *cmd = "FPAN:MESS \"\"\n";


	if ( ! ep385_is_needed )
		return 1;

	gpib_write( ep385.device, cmd, strlen( cmd ) );
	ep385_run( UNSET );
	gpib_local( ep385.device );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void ep385_exit_hook( void )
{
	PULSE *p, *np;
	FUNCTION *f;
	int i, j;


	if ( ep385.dump_file != NULL )
	{
		fclose( ep385.dump_file );
		ep385.dump_file = NULL;
	}

	if ( ! ep385_is_needed )
		return;

	/* Free all memory that may have been allocated for the module */

	for ( p = ep385_Pulses; p != NULL; p = np )
	{
		np = p->next;
		T_free( p );
	}

	ep385_Pulses = NULL;

	for ( i = 0; i < MAX_CHANNELS; i++ )
		if ( ep385.channel[ i ].pulse_params != NULL )
			ep385.channel[ i ].pulse_params =
				T_free( ep385.channel[ i ].pulse_params );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &ep385.function[ i ];

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
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

	if ( ep385.dump_file != NULL )
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
			vars_push( INT_VAR, 0 );
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

		if ( ( ep385.dump_file = fopen( name, "w+" ) ) == NULL )
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
	} while ( ep385.dump_file == NULL );

	T_free( name );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_shape_to_defense_minimum_distance( Var *v )
{
	double s2d;


	if ( ep385.is_shape_2_defense )
	{
		print( FATAL, "SHAPE to DEFENSE pulse minimum distance has already "
			   "been set to %s.\n",
			   ep385_ptime( ep385_ticks2double( ep385.shape_2_defense ) ) );
		THROW( EXCEPTION );
	}

	s2d = get_double( v, "SHAPE to DEFENSE pulse minimum distance" );

	if ( s2d < 0 )
	{
		print( FATAL, "Negative SHAPE to DEFENSE pulse minimum distance.\n" );
		THROW( EXCEPTION );
	}

	ep385.shape_2_defense = ep385_double2ticks( s2d );
	ep385.is_shape_2_defense = SET;

	return vars_push( FLOAT_VAR, s2d );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_defense_to_shape_minimum_distance( Var *v )
{
	double d2s;


	if ( ep385.is_defense_2_shape )
	{
		print( FATAL, "DEFENSE to SHAPE pulse minimum distance has already "
			   "been set to %s.\n",
			   ep385_ptime( ep385_ticks2double( ep385.defense_2_shape ) ) );
		THROW( EXCEPTION );
	}

	d2s = get_double( v, "DEFENSE to SHAPE pulse minimum distance" );

	if ( d2s < 0 )
	{
		print( FATAL, "Negative DEFENSE to SHAPE pulse minimum distance.\n" );
		THROW( EXCEPTION );
	}

	ep385.defense_2_shape = ep385_double2ticks( d2s );
	ep385.is_defense_2_shape = SET;

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

	ep385_run( state );
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


	if ( ep385.is_cw_mode )
	{
		print( FATAL, "Function can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! ep385_is_needed )
		return vars_push( INT_VAR, 1 );

	/* Send all changes to the pulser */

	if ( ep385.needs_update )
		return vars_push( INT_VAR, ep385_do_update( ) ? 1 : 0 );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *pulser_cw_mode( Var *v )
{
	v = v;


	/* We can't have any pulses in cw mode */

	if ( ep385_Pulses != NULL )
	{
		print( FATAL, "CW mode and use of pulses is mutually exclusive.\n" );
		THROW( EXCEPTION );
	}

	/* We need the function MICROWAVE defined for cw mode */

	if ( ! ep385.function[ PULSER_CHANNEL_MW ].is_used )
	{
		print( FATAL, "Function 'MICROWAVE' has not been defined as needed "
			   "for CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ep385.is_trig_in_mode && ep385.trig_in_mode == EXTERNAL )
	{
		print( FATAL, "External trigger mode can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ep385.is_repeat_time )
	{
		print( FATAL, "Repeat time/frequency can't be set in CW mode.\n" );
		THROW( EXCEPTION );
	}

	ep385.is_cw_mode = SET;
	ep385.trig_in_mode = INTERNAL;
	ep385.is_trig_in_mode = SET;

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


	if ( ep385.is_cw_mode )
	{
		print( FATAL, "Pulses can't be shifted in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! ep385_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
		for ( p = ep385_Pulses; p != NULL; p = p->next )
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

		if ( p->needs_update )
			ep385.needs_update = SET;
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


	if ( ep385.is_cw_mode )
	{
		print( FATAL, "Pulses can't be changed in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! ep385_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
		for ( p = ep385_Pulses; p != NULL; p = p->next )
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

		/* If the pulse was or is active we've got to update the pulser */

		if ( p->needs_update )
			ep385.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_pulse_reset( Var *v )
{
	PULSE *p;


	if ( ep385.is_cw_mode )
	{
		print( FATAL, "Function can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! ep385_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to reset all pulses (even the
       inactive ones) */

	if ( v == NULL )
	{
		for ( p = ep385_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				pulser_pulse_reset( vars_push( INT_VAR, p->num ) );
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

		if ( p->needs_update )
			ep385.needs_update = SET;
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


	if ( ep385.is_cw_mode )
	{
		print( FATAL, "Function can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! ep385_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		long res = 1;


		if ( ep385_phs[ 0 ].function == NULL &&
			 ep385_phs[ 1 ].function == NULL &&
			 FSC2_MODE == TEST )
		{
			print( SEVERE, "Phase cycling isn't used for any function.\n" );
			return vars_push( INT_VAR, 0 );
		}

		if ( ep385_phs[ 0 ].function != NULL )
			res &= pulser_next_phase( vars_push( INT_VAR, 1 ) )->val.lval;
		if ( ep385_phs[ 1 ].function != NULL )
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

		if ( ! ep385_phs[ phase_number - 1 ].is_defined )
		{
			print( FATAL, "PHASE_SETUP_%ld has not been defined.\n",
				   phase_number );
			THROW( EXCEPTION );
		}

		f = ep385_phs[ phase_number - 1 ].function;

		if ( ! f->is_used )
		{
			if ( FSC2_MODE == TEST )
				print( SEVERE, "Function '%s' to be phase cycled is not "
					   "used.\n", Function_Names[ f->self ] );
			return vars_push( INT_VAR, 0 );
		}

		if ( ++f->next_phase >= f->pc_len )
			f->next_phase = 0;
	}

	ep385.needs_update = SET;
	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_phase_reset( Var *v )
{
	FUNCTION *f;
	long phase_number;


	if ( ep385.is_cw_mode )
	{
		print( FATAL, "Function can't be used in CW mode.\n" );
		THROW( EXCEPTION );
	}

	if ( ! ep385_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		long ret = 1;


		if ( ep385_phs[ 0 ].function == NULL &&
			 ep385_phs[ 1 ].function == NULL &&
			 FSC2_MODE == TEST )
		{
			print( SEVERE, "Phase cycling isn't used for any function.\n" );
			return vars_push( INT_VAR, 0 );
		}

		if ( ep385_phs[ 0 ].function != NULL )
			ret &= pulser_phase_reset( vars_push( INT_VAR, 1 ) )->val.lval;
		if ( ep385_phs[ 1 ].function != NULL )
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

		f = ep385_phs[ phase_number - 1 ].function;

		if ( ! f->is_used )
		{
			if ( FSC2_MODE == TEST )
				print( SEVERE, "Function '%s' to be phase cycled is not "
					   "used.\n", Function_Names[ f->self ] );
			return vars_push( INT_VAR, 0 );
		}

		f->next_phase = 0;
	}

	ep385.needs_update = SET;
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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
