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


#include "rb_pulser_w.h"


/*--------------------------------*
 * global variables of the module *
 *--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


RB_Pulser_W_T rb_pulser_w;
Rulbus_Clock_Card_T clock_card[ NUM_CLOCK_CARDS ];
Rulbus_Delay_Card_T delay_card[ NUM_DELAY_CARDS ];


/*-------------------------------*
 * local functions and variables *
 *-------------------------------*/

static void rb_pulser_w_card_setup( void );

static bool Is_running_at_start;


/*------------------------------------------------------------------------*
 * Function that gets called immediately after the module has been loaded
 *------------------------------------------------------------------------*/

int rb_pulser_w_init_hook( void )
{
	size_t i;
	Function_T *f;


	Pulser_Struct.name     = DEVICE_NAME;
	Pulser_Struct.has_pods = UNSET;

	/* Set global variable to indicate that Rulbus is needed */

#if ! defined RB_PULSER_W_TEST
	Need_RULBUS = SET;

	rb_pulser_w.exists_synthesizer = exists_device( SYNTHESIZER_MODULE );

#else   /* in test mode */
	rb_pulser_w.exists_synthesizer = SET;
#endif

#ifndef FIXED_TIMEBASE
	Pulser_Struct.set_timebase = rb_pulser_w_store_timebase;
#else
	Pulser_Struct.set_timebase = NULL;
#endif

	Pulser_Struct.set_trigger_mode = rb_pulser_w_set_trigger_mode;
	Pulser_Struct.set_repeat_time = rb_pulser_w_set_repeat_time;
	Pulser_Struct.set_trig_in_slope = rb_pulser_w_set_trig_in_slope;
	Pulser_Struct.set_function_delay = rb_pulser_w_set_function_delay;

	Pulser_Struct.new_pulse = rb_pulser_w_new_pulse;
	Pulser_Struct.set_pulse_function = rb_pulser_w_set_pulse_function;
	Pulser_Struct.set_pulse_position = rb_pulser_w_set_pulse_position;
	Pulser_Struct.set_pulse_length = rb_pulser_w_set_pulse_length;
	Pulser_Struct.set_pulse_position_change =
										 rb_pulser_w_set_pulse_position_change;
	Pulser_Struct.set_pulse_length_change =
		                                   rb_pulser_w_set_pulse_length_change;
	Pulser_Struct.set_pulse_phase_cycle = rb_pulser_w_set_pulse_phase_cycle;

	Pulser_Struct.get_pulse_function = rb_pulser_w_get_pulse_function;
	Pulser_Struct.get_pulse_position = rb_pulser_w_get_pulse_position;
	Pulser_Struct.get_pulse_length = rb_pulser_w_get_pulse_length;
	Pulser_Struct.get_pulse_position_change =
										 rb_pulser_w_get_pulse_position_change;
	Pulser_Struct.get_pulse_length_change =
		                                   rb_pulser_w_get_pulse_length_change;
	Pulser_Struct.get_pulse_phase_cycle = rb_pulser_w_get_pulse_phase_cycle;

	Pulser_Struct.set_phase_switch_delay = rb_pulser_w_set_phase_switch_delay;
	Pulser_Struct.set_grace_period = rb_pulser_w_set_grace_period;

	Pulser_Struct.assign_function = NULL;
	Pulser_Struct.assign_channel_to_function = NULL;
	Pulser_Struct.invert_function = NULL;
	Pulser_Struct.set_max_seq_len = NULL;
	Pulser_Struct.set_function_high_level = NULL;
	Pulser_Struct.set_function_low_level = NULL;
	Pulser_Struct.set_trig_in_level = NULL;
	Pulser_Struct.set_trig_in_impedance = NULL;
	Pulser_Struct.set_phase_reference = NULL;
	Pulser_Struct.phase_setup_prep = NULL;
	Pulser_Struct.phase_setup = NULL;
	Pulser_Struct.set_grace_period = NULL;
	Pulser_Struct.ch_to_num = NULL;
	Pulser_Struct.keep_all_pulses = NULL;

	rb_pulser_w.is_needed = SET;

#ifndef FIXED_TIMEBASE
	rb_pulser_w.is_timebase = UNSET;
#else
	rb_pulser_w.timebase = 1.0e-8;         /* fixed 100 MHz clock is used */
	rb_pulser_w.is_timebase = SET;
#endif

	rb_pulser_w.is_running = UNSET;

	if ( RB_PULSER_W_CONFIG_FILE[ 0 ] ==  '/' )
		rb_pulser_w.config_file = T_strdup( RB_PULSER_W_CONFIG_FILE );
	else
		rb_pulser_w.config_file = get_string( "%s%s%s", libdir,
											  slash( libdir ),
											  RB_PULSER_W_CONFIG_FILE );

	rb_pulser_w.is_trig_in_mode = UNSET;
	rb_pulser_w.is_trig_in_slope = UNSET;
	rb_pulser_w.is_rep_time = UNSET;
	rb_pulser_w.is_neg_delay = UNSET;
	rb_pulser_w.neg_delay = 0;

	rb_pulser_w.trig_in_mode = INTERNAL;
	rb_pulser_w.rep_time = 0;

	rb_pulser_w.synth_state = NULL;
	rb_pulser_w.synth_pulse_state = NULL;
	rb_pulser_w.synth_pulse_width = NULL;
	rb_pulser_w.synth_pulse_delay = NULL;
	rb_pulser_w.synth_trig_slope = NULL;

	rb_pulser_w.dump_file = NULL;
	rb_pulser_w.show_file = NULL;

	rb_pulser_w.do_show_pulses = UNSET;
	rb_pulser_w.do_dump_pulses = UNSET;

	rb_pulser_w.needs_phases = UNSET;
	rb_pulser_w.next_phase = 0;

	rb_pulser_w.is_psd = UNSET;
	rb_pulser_w.is_grace_period = UNSET;
	rb_pulser_w.is_pulse_2_defense = UNSET;
	rb_pulser_w.no_defense_pulse = UNSET;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
        f = rb_pulser_w.function + i;
        f->self = i;
        f->name = Function_Names[ i ];
        f->num_pulses = 0;
		f->num_active_pulses = 0;
        f->pulses = NULL;
		f->delay_card = NULL;
        f->delay = 0.0;
        f->is_delay = UNSET;

		if ( i != PULSER_CHANNEL_MW &&
			 i != PULSER_CHANNEL_PHASE_1 &&
			 i != PULSER_CHANNEL_RF &&
			 i != PULSER_CHANNEL_DEFENSE &&
			 i != PULSER_CHANNEL_DET &&
			 i != PULSER_CHANNEL_LASER )
			f->is_used = UNSET;
		else
			f->is_used = SET;
		f->is_declared = UNSET;
	}

	rb_pulser_w_card_setup( );

	rb_pulser_w.function[ PULSER_CHANNEL_MW ].delay_card =
										   rb_pulser_w.delay_card + MW_DELAY_0;
	rb_pulser_w.function[ PULSER_CHANNEL_PHASE_1 ].delay_card =
										rb_pulser_w.delay_card + PHASE_DELAY_0;
	rb_pulser_w.function[ PULSER_CHANNEL_RF ].delay_card =
										     rb_pulser_w.delay_card + RF_DELAY;
	rb_pulser_w.function[ PULSER_CHANNEL_DEFENSE ].delay_card =
									    rb_pulser_w.delay_card + DEFENSE_DELAY;
	rb_pulser_w.function[ PULSER_CHANNEL_DET ].delay_card =
										    rb_pulser_w.delay_card + DET_DELAY;
	rb_pulser_w.function[ PULSER_CHANNEL_LASER ].delay_card =
										rb_pulser_w.delay_card + LASER_DELAY_0;

	return 1;
}


/*----------------------------------------------------------*
 * Function gets called just before the test run is started
 *----------------------------------------------------------*/

int rb_pulser_w_test_hook( void )
{
	Pulse_T *p;


	/* If not set by the user take the default time distance between
	   activating the phase switch and the start of a microwave pulse */

	if ( ! rb_pulser_w.is_psd )
		rb_pulser_w.psd = DEFAULT_PHASE_SWITCH_DELAY;

	/* Also set the default time a phase has to be kept after the end of a
	   pulse unless the user already set it (please note that this can end
	   up as a negative time if the grace period is shorter than the time
	   Leendert's magic box needs to pass a signal through). */

	if ( ! rb_pulser_w.is_grace_period )
		rb_pulser_w.grace_period = DEFAULT_GRACE_PERIOD;

	/* If the user didn't set a minimum time between the end of the last
	   microwave pulse and the end of the defense pulse use the default
	   value */

	if ( ! rb_pulser_w.is_pulse_2_defense )
		rb_pulser_w.pulse_2_defense = PULSE_2_DEFENSE_DEFAULT_MIN_DISTANCE;

	/* If a RF pulse exists make sure the synthesizer module got loaded
	   before the pulser module */

	for ( p = rb_pulser_w.pulses; p != NULL; p = p->next )
		if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_RF )
		{
			if ( ! rb_pulser_w.exists_synthesizer )
			{
				print( FATAL, "Module \"" SYNTHESIZER_MODULE "\" pulser must "
					   "be listed before \"rb_pulser_w\" module in DEVICES "
					   "section when RF pulses are to be used.\n" );
				THROW( EXCEPTION );
			}

			break;
		}

	/* Check consistency of pulse settings and do everything to setup the
	   pulser for the test run */

	TRY
	{
		if ( rb_pulser_w.trig_in_mode == INTERNAL &&
			 ! rb_pulser_w.is_rep_time )
		{
			print( FATAL, "No experiment repetition time/frequency has been "
				   "set.\n" );
			THROW( EXCEPTION );
		}

		Is_running_at_start = rb_pulser_w.is_running;
		if ( rb_pulser_w.do_show_pulses )
			rb_pulser_w_show_pulses( );
		if ( rb_pulser_w.do_dump_pulses )
			rb_pulser_w_dump_pulses( );
		rb_pulser_w_init_setup( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( rb_pulser_w.dump_file )
		{
			fclose( rb_pulser_w.dump_file );
			rb_pulser_w.dump_file = NULL;
		}

		if ( rb_pulser_w.show_file )
		{
			fclose( rb_pulser_w.show_file );
			rb_pulser_w.show_file = NULL;
		}

		RETHROW( );
	}

	/* If a repetition time has been set set up the corresponding value in
	   th structure for the card creating the repetition time */

	if ( rb_pulser_w.trig_in_mode == INTERNAL ) {
		rb_pulser_w.clock_card[ ERT_CLOCK ].freq = rb_pulser_w.rep_time_index;
		rb_pulser_w.delay_card[ ERT_DELAY ].delay = rb_pulser_w.rep_time_ticks;
	}

	/* After the initial setup is done we need some different functions for
	   setting of pulse properties */

	Pulser_Struct.set_pulse_position = rb_pulser_w_change_pulse_position;
	Pulser_Struct.set_pulse_length = rb_pulser_w_change_pulse_length;
	Pulser_Struct.set_pulse_position_change =
									  rb_pulser_w_change_pulse_position_change;
	Pulser_Struct.set_pulse_length_change =
										rb_pulser_w_change_pulse_length_change;

	if ( rb_pulser_w.pulses == NULL )
		rb_pulser_w.is_needed = UNSET;

	return 1;
}


/*----------------------------------------------------------------------*
 * Function called after the end of the test run, mostly used to finish
 * output for the functions for displaying and/or logging what pulses
 * have been set to during the experiment
 *----------------------------------------------------------------------*/

int rb_pulser_w_end_of_test_hook( void )
{
	if ( ! rb_pulser_w.is_needed )
		return 1;

	if ( rb_pulser_w.dump_file != NULL )
	{
		fclose( rb_pulser_w.dump_file );
		rb_pulser_w.dump_file = NULL;
	}

	if ( rb_pulser_w.show_file != NULL )
	{
		fclose( rb_pulser_w.show_file );
		rb_pulser_w.show_file = NULL;
	}

	return 1;
}


/*---------------------------------------------------------------*
 * Function gets called at the very start of the experiment - it
 * got to initialize the pulser and, if required, start it
 *---------------------------------------------------------------*/

int rb_pulser_w_exp_hook( void )
{
	if ( ! rb_pulser_w.is_needed )
		return 1;

	/* If the user asked to have no defense pulse ask if she really meant it */

	if ( rb_pulser_w.no_defense_pulse )
	{
		const char *warn = "You wanr the defense pulse to be switched off.\n"
			               "This can destroy the microwave amplifier.\n"
			               "*****^ Is this really what you want? *****";
		if ( 2 != show_choices( warn, 2, "Abort", "Yes", NULL, 1 ) )
			THROW( EXCEPTION );
	}

#if defined ASK_FOR_PULSE_2_DEFENSE_DISTANCE_CONFORMATION
	/* If the user changed the minimum distance between the end of the
	   last microwave pulse and the end of the defense pulse also ask
	   if she's serious about that */

	if ( rb_pulser_w.is_pulse_2_defense )
	{
		char warn[ 500 ];
		sprintf( warn, "The minimum distance between the end of the last\n"
				       "microwave pulse and the end of defense pulse has\n"
				       "been changed to %s.\n"
				       "***** Is this really what you want? *****",
				 rb_pulser_w_ptime( rb_pulser_w_ticks2double(
										     rb_pulser_w.pulse_2_defense ) ) );
		if ( 2 != show_choices( warn, 2, "Abort", "Yes", NULL, 1 ) )
			THROW( EXCEPTION );
	}
#endif

	rb_pulser_w.is_running = Is_running_at_start;

	/* Initialize the device */

	rb_pulser_w_init( );
	rb_pulser_w_full_reset( );
	rb_pulser_w_do_update( );
	rb_pulser_w_run( rb_pulser_w.is_running );

	return 1;
}


/*----------------------------------------------*
 * Function called at the end of the experiment
 *----------------------------------------------*/

int rb_pulser_w_end_of_exp_hook( void )
{
	if ( rb_pulser_w.is_needed )
		rb_pulser_w_exit( );

	return 1;
}


/*---------------------------------------------------------------*
 * Function called just before the modules gets unloaded, mainly
 * used to get rid of memory allocated for the module
 *---------------------------------------------------------------*/

void rb_pulser_w_exit_hook( void )
{
	Pulse_T *p, *pn;
	Function_T *f;
	int i;


	rb_pulser_w_cleanup( );

	if ( ! rb_pulser_w.is_needed )
		return;

	/* Free all memory allocated within the module */

	for ( p = rb_pulser_w.pulses; p != NULL; p = pn )
	{
		pn = p->next;
		T_free( p );
	}

	if ( rb_pulser_w.synth_state )
		rb_pulser_w.synth_pulse_state =
			                          CHAR_P T_free( rb_pulser_w.synth_state );

	if ( rb_pulser_w.synth_pulse_state )
		rb_pulser_w.synth_pulse_state =
								CHAR_P T_free( rb_pulser_w.synth_pulse_state );

	if ( rb_pulser_w.synth_pulse_width )
		rb_pulser_w.synth_pulse_width =
								CHAR_P T_free( rb_pulser_w.synth_pulse_width );

	if ( rb_pulser_w.synth_pulse_delay )
		rb_pulser_w.synth_pulse_delay =
								CHAR_P T_free( rb_pulser_w.synth_pulse_delay );

	if ( rb_pulser_w.synth_trig_slope )
		rb_pulser_w.synth_trig_slope =
								 CHAR_P T_free( rb_pulser_w.synth_trig_slope );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rb_pulser_w.function + i;
		f->pulses = PULSE_PP T_free( f->pulses );
	}
}


/*-------------------------------------------*
 * EDL function that returns the device name
 *-------------------------------------------*/

Var_T *pulser_name( Var_T * v UNUSED_ARG )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*
 * EDL function for getting the positions and lengths
 * of all pulses used during the experiment displayed
 *----------------------------------------------------*/

Var_T *pulser_show_pulses( Var_T * v UNUSED_ARG )
{
	if ( ! FSC2_IS_CHECK_RUN && ! FSC2_IS_BATCH_MODE )
		rb_pulser_w.do_show_pulses = SET;

	return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------------*
 * Similar to the previous functions, but instead the states
 * of all pulses during the experiment gets written to a file
 *------------------------------------------------------------*/

Var_T *pulser_dump_pulses( Var_T * v UNUSED_ARG )
{
	if ( ! FSC2_IS_CHECK_RUN && ! FSC2_IS_BATCH_MODE )
		rb_pulser_w.do_dump_pulses = SET;

	return vars_push( INT_VAR, 1L );
}


/*-----------------------------------------------------------------*
 * Function for setting the time the phase switch must be switched
 * before a microwave pulse with the new can start. The function
 * automatically includes the time it takes Leenderts "magic box"
 * to react to the phase pulse.
 *-----------------------------------------------------------------*/

Var_T *pulser_phase_switch_delay( Var_T * v )
{
	if ( v == NULL )
	{
		print( FATAL, "Missing argument.\n" );
		THROW( EXCEPTION );
	}

	rb_pulser_w_set_phase_switch_delay( 0,
									   get_double( v, "phase switch delay" ) );
	return vars_push( FLOAT_VAR, rb_pulser_w.psd * rb_pulser_w.timebase );
}


/*----------------------------------------------------------------------*
 * Function for setting the minimum time a phase must be kept after the
 * end of a pulse.
 *----------------------------------------------------------------------*/

Var_T *pulser_grace_period( Var_T * v )
{
	rb_pulser_w_set_grace_period( get_double( v, "grace period" ) );
	return vars_push( FLOAT_VAR,
					  rb_pulser_w.grace_period * rb_pulser_w.timebase );
}


/*------------------------------------------------------------*
 * Function for setting the minimum time between the end of a
 * microwave pulse and the end of the defense pulse. This
 * should always be large enough so that the receivers never
 * see anything of the pulses.
 *------------------------------------------------------------*/

Var_T *pulser_minimum_defense_distance( Var_T * v )
{
	double s2d;


	if ( rb_pulser_w.is_pulse_2_defense )
	{
		print( FATAL, "End of last microwave pulse to end of defense pulse "
			   "minimum distance has already been set to %s.\n",
			   rb_pulser_w_ptime(
				   rb_pulser_w_ticks2double( rb_pulser_w.pulse_2_defense ) ) );
		THROW( EXCEPTION );
	}

	s2d = get_double( v, "end of last microwave pulse to end of defense "
					  "pulse minimum distance" );

	if ( s2d < 0 )
	{
		print( FATAL, "Negative minimum distance between end of last "
			   "microwave pulse end end of defense pulse.\n" );
		THROW( EXCEPTION );
	}

	rb_pulser_w.is_pulse_2_defense = SET;
	rb_pulser_w.pulse_2_defense = s2d;

	return vars_push( FLOAT_VAR, s2d );
}


/*---------------------------------------------------------------*
 * Function for setting the pulser not to create a defense pulse
 *---------------------------------------------------------------*/

Var_T *pulser_disable_defense_pulse( Var_T * v  UNUSED_ARG )
{
	rb_pulser_w.no_defense_pulse = SET;

	return vars_push( INT_VAR, 1L );
}



/*---------------------------------------------*
 * Switches the output of the pulser on or off
 *---------------------------------------------*/

Var_T *pulser_state( Var_T * v )
{
	bool state;


	if ( v == NULL )
		return vars_push( INT_VAR, ( long ) rb_pulser_w.is_running );

	state = get_boolean( v );

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( INT_VAR,
						  ( long ) ( rb_pulser_w.is_running = state ) );

	rb_pulser_w_run( state );

	return vars_push( INT_VAR, ( long ) rb_pulser_w.is_running );
}


/*---------------------------------------------------------*
 * Function that must be called to commit all changes made
 * to the pulses - without calling the function only the
 * internal representation of the pulser is changed but
 * not the state of the "real" pulser
 *---------------------------------------------------------*/

Var_T *pulser_update( Var_T * v UNUSED_ARG )
{
	if ( ! rb_pulser_w.is_needed )
		return vars_push( INT_VAR, 1L );

	/* Send all changes to the pulser */

	rb_pulser_w_do_update( );

	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------------*
 * Public function to change the position of pulses. If called with no
 * argument all active pulses that have a position change time set will
 * be moved, otherwise all pulses passed as arguments to the function.
 * Take care: The changes will only be committed on the next call of
 *            the function pulser_update() !
 *----------------------------------------------------------------------*/

Var_T *pulser_shift( Var_T * v )
{
	Pulse_T *p;


	if ( ! rb_pulser_w.is_needed )
		return vars_push( INT_VAR, 1L );

	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
		for ( p = rb_pulser_w.pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dpos )
				vars_pop( pulser_shift( vars_push( INT_VAR, p->num ) ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = rb_pulser_w_get_pulse( get_strict_long( v, "pulse number" ) );

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

		/* Make sure we always end up with an integer multiple of the
		   timebase */

		p->pos =
			    rb_pulser_w_ticks2double( rb_pulser_w_double2ticks( p->pos ) );

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
	}

	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------------*
 * Public function for incrementing the length of pulses. If called with
 * no argument all active pulses that have a length change defined are
 * incremented, otherwise all pulses passed as arguments to the function.
 * Take care: The changes will only be committed on the next call of the
 *            function pulser_update() !
 *-------------------------------------------------------------------------*/

Var_T *pulser_increment( Var_T * v )
{
	Pulse_T *p;


	if ( ! rb_pulser_w.is_needed )
		return vars_push( INT_VAR, 1L );

	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
		for ( p = rb_pulser_w.pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dlen )
				vars_pop( pulser_increment( vars_push( INT_VAR, p->num ) ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = rb_pulser_w_get_pulse( get_strict_long( v, "pulse number" ) );

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
				   p->num, rb_pulser_w_pticks( p->len ) );
			THROW( EXCEPTION );
		}

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
	}

	return vars_push( INT_VAR, 1L );
}


/*-----------------------------------------------------*
 * Function for switching the pulser to the next phase
 * settings of the phase cycle sequence
 *-----------------------------------------------------*/

Var_T *pulser_next_phase( Var_T * v )
{
	if ( ! rb_pulser_w.is_needed )
		return vars_push( INT_VAR, 1L );

	too_many_arguments( v );

	if ( ! rb_pulser_w.needs_phases )
	{
		print( SEVERE, "No phase cycling is used.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ++rb_pulser_w.next_phase >= rb_pulser_w.pc_len )
		rb_pulser_w.next_phase = 0;

	pulser_update( NULL );

	return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
 * Function for switching the pulser to the first phase
 * settings of the phase cycle sequence
 *------------------------------------------------------*/

Var_T *pulser_phase_reset( Var_T * v )
{
	if ( ! rb_pulser_w.is_needed )
		return vars_push( INT_VAR, 1L );

	too_many_arguments( v );

	if ( ! rb_pulser_w.needs_phases )
	{
		print( SEVERE, "No phase cycling is used.\n" );
		return vars_push( INT_VAR, 0L );
	}

	rb_pulser_w.next_phase = 0;

	pulser_update( NULL );

	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------*
 * Function for resetting the pulser back to its initial state
 *-------------------------------------------------------------*/

Var_T *pulser_reset( Var_T * v UNUSED_ARG )
{
	if ( ! rb_pulser_w.is_needed )
		return vars_push( INT_VAR, 1L );

	rb_pulser_w.next_phase = 0;
	vars_pop( pulser_pulse_reset( NULL ) );

	return pulser_update( NULL );
}


/*---------------------------------------------------------------------*
 * Function for resetting one or more pulses back to the initial state
 *---------------------------------------------------------------------*/

Var_T *pulser_pulse_reset( Var_T * v )
{
	Pulse_T *p;


	if ( ! rb_pulser_w.is_needed )
		return vars_push( INT_VAR, 1L );

	/* An empty pulse list means that we have to reset all pulses (even the
       inactive ones). Don't explicitely reset automatically created pulses
	   (which all have a negative pulse number), they will be reset together
	   with the pulses belong to. */

	if ( v == NULL )
	{
		for ( p = rb_pulser_w.pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				vars_pop( pulser_pulse_reset( vars_push( INT_VAR, p->num ) ) );
	}

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = rb_pulser_w_get_pulse( get_strict_long( v, "pulse number" ) );

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


/*---------------------------------------------------------------------*
 * This is a special function for the Rulbus pulser only which returns
 * some minimum values for a pulse: for each first pulse of a function
 * the earliest possible position for the pulse, for all others the
 * minimum delay between the end of its predecessor and its start.
 *---------------------------------------------------------------------*/

Var_T *pulser_pulse_minimum_specs( Var_T * v )
{
	Pulse_T *p = rb_pulser_w_get_pulse( get_strict_long( v, "pulse number" ) );
	double t = 0.0;


	if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_MW )
		t = rb_pulser_mw_min_specs( p );
	else if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_RF )
		t = rb_pulser_rf_min_specs( p );
	else if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_LASER )
		t = rb_pulser_laser_min_specs( p );
	else if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_DET )
		t = rb_pulser_det_min_specs( p );
	else
		fsc2_assert( 1 == 0 );

	return vars_push( FLOAT_VAR, t );
}


/*------------------------------------------------------------*
 * Function for initialization of the structures representing
 * the Rulbus cards the pulser is made of
 *------------------------------------------------------------*/

static void rb_pulser_w_card_setup( void )
{
	size_t i;
	RULBUS_CARD_INFO card_info;


	for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
	{
		rb_pulser_w.clock_card[ i ].handle = -1;
		rb_pulser_w.clock_card[ i ].name = NULL;
	}

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
		rb_pulser_w.delay_card[ i ].name = NULL;

	/* Get the names of all cards from the configuration file */

	rb_pulser_w_read_configuration( );

	rb_pulser_w.delay_card[ ERT_DELAY ].self = ERT_DELAY;
	rb_pulser_w.delay_card[ ERT_DELAY ].prev = NULL;
	rb_pulser_w.delay_card[ ERT_DELAY ].next = NULL;

	rb_pulser_w.delay_card[ MW_DELAY_0 ].self = MW_DELAY_0;
	rb_pulser_w.delay_card[ MW_DELAY_0 ].prev = NULL;
	rb_pulser_w.delay_card[ MW_DELAY_0 ].next =
										   rb_pulser_w.delay_card + MW_DELAY_1;

	rb_pulser_w.delay_card[ MW_DELAY_1 ].self = MW_DELAY_1;
	rb_pulser_w.delay_card[ MW_DELAY_1 ].prev =
										   rb_pulser_w.delay_card + MW_DELAY_0;
	rb_pulser_w.delay_card[ MW_DELAY_1 ].next =
										   rb_pulser_w.delay_card + MW_DELAY_2;

	rb_pulser_w.delay_card[ MW_DELAY_2 ].self = MW_DELAY_2;
	rb_pulser_w.delay_card[ MW_DELAY_2 ].prev =
										   rb_pulser_w.delay_card + MW_DELAY_1;
	rb_pulser_w.delay_card[ MW_DELAY_2 ].next =
										   rb_pulser_w.delay_card + MW_DELAY_3;

	rb_pulser_w.delay_card[ MW_DELAY_3 ].self = MW_DELAY_3;
	rb_pulser_w.delay_card[ MW_DELAY_3 ].prev =
										   rb_pulser_w.delay_card + MW_DELAY_2;
	rb_pulser_w.delay_card[ MW_DELAY_3 ].next =
										   rb_pulser_w.delay_card + MW_DELAY_4;

	rb_pulser_w.delay_card[ MW_DELAY_4 ].self = MW_DELAY_4;
	rb_pulser_w.delay_card[ MW_DELAY_4 ].prev =
										   rb_pulser_w.delay_card + MW_DELAY_3;
	rb_pulser_w.delay_card[ MW_DELAY_4 ].next =
										   rb_pulser_w.delay_card + MW_DELAY_5;

	rb_pulser_w.delay_card[ MW_DELAY_5 ].self = MW_DELAY_5;
	rb_pulser_w.delay_card[ MW_DELAY_5 ].prev =
										   rb_pulser_w.delay_card + MW_DELAY_4;
	rb_pulser_w.delay_card[ MW_DELAY_5 ].next = NULL;

	rb_pulser_w.delay_card[ PHASE_DELAY_0 ].self = PHASE_DELAY_0;
	rb_pulser_w.delay_card[ PHASE_DELAY_0 ].prev = NULL;
	rb_pulser_w.delay_card[ PHASE_DELAY_0 ].next =
									    rb_pulser_w.delay_card + PHASE_DELAY_1;

	rb_pulser_w.delay_card[ PHASE_DELAY_1 ].self = PHASE_DELAY_1;
	rb_pulser_w.delay_card[ PHASE_DELAY_1 ].prev =
									    rb_pulser_w.delay_card + PHASE_DELAY_0;
	rb_pulser_w.delay_card[ PHASE_DELAY_1 ].next =
									    rb_pulser_w.delay_card + PHASE_DELAY_2;

	rb_pulser_w.delay_card[ PHASE_DELAY_2 ].self = PHASE_DELAY_2;
	rb_pulser_w.delay_card[ PHASE_DELAY_2 ].prev =
									    rb_pulser_w.delay_card + PHASE_DELAY_1;
	rb_pulser_w.delay_card[ PHASE_DELAY_1 ].next = NULL;

	rb_pulser_w.delay_card[ DEFENSE_DELAY ].self = DEFENSE_DELAY;
	rb_pulser_w.delay_card[ DEFENSE_DELAY ].prev = NULL;
	rb_pulser_w.delay_card[ DEFENSE_DELAY ].next = NULL;

	rb_pulser_w.delay_card[ RF_DELAY ].self = RF_DELAY;
	rb_pulser_w.delay_card[ RF_DELAY ].prev = NULL;
	rb_pulser_w.delay_card[ RF_DELAY ].next = NULL;

	rb_pulser_w.delay_card[ LASER_DELAY_0 ].self = LASER_DELAY_0;
	rb_pulser_w.delay_card[ LASER_DELAY_0 ].prev = NULL;
	rb_pulser_w.delay_card[ LASER_DELAY_0 ].next =
										rb_pulser_w.delay_card + LASER_DELAY_1;

	rb_pulser_w.delay_card[ LASER_DELAY_1 ].self = LASER_DELAY_1;
	rb_pulser_w.delay_card[ LASER_DELAY_1 ].prev =
										rb_pulser_w.delay_card + LASER_DELAY_0;
	rb_pulser_w.delay_card[ LASER_DELAY_1 ].next = NULL;

	rb_pulser_w.delay_card[ DET_DELAY ].self = DET_DELAY;
	rb_pulser_w.delay_card[ DET_DELAY ].prev = NULL;
	rb_pulser_w.delay_card[ DET_DELAY ].next = NULL;

	/* Get handles for all the delay cards and determine their intrinsic
	   delays */

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
	{
		rb_pulser_w.delay_card[ i ].handle = -1;
		rb_pulser_w.delay_card[ i ].is_active = UNSET;
		rb_pulser_w.delay_card[ i ].delay =
		rb_pulser_w.delay_card[ i ].old_delay = 0;

		if ( rulbus_get_card_info( rb_pulser_w.delay_card[ i ].name,
								   &card_info ) != RULBUS_OK )
		{
			print( FATAL, "Failed to obtain RULBUS configuration "
				   "information: %s.\n", rulbus_strerror( ) );
			THROW( EXCEPTION );
		}

		rb_pulser_w.delay_card[ i ].intr_delay = card_info.intr_delay;
	}
}


/*----------------------------------------------------------------*
 * Function for free()ing memory that is used for the name of the
 * configuration file and storing the names of the cards
 *---------------------------------------------------------------*/

void rb_pulser_w_cleanup( void )
{
	size_t i;


	if ( rb_pulser_w.config_file != NULL )
		rb_pulser_w.config_file = CHAR_P T_free( rb_pulser_w.config_file );

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
		if ( rb_pulser_w.delay_card[ i ].name != NULL )
			rb_pulser_w.delay_card[ i ].name =
							 CHAR_P T_free( rb_pulser_w.delay_card[ i ].name );

	for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
		if ( rb_pulser_w.clock_card[ i ].name != NULL )
			rb_pulser_w.clock_card[ i ].name =
							CHAR_P T_free( rb_pulser_w.clock_card[ i ].name );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
