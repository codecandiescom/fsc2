/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#include "rb_pulser.h"


/*--------------------------------*
 * global variables of the module
 *--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


RB_PULSER rb_pulser;
PULSE *rb_pulser_Pulses;
RULBUS_CLOCK_CARD clock_card[ NUM_CLOCK_CARDS ];
RULBUS_DELAY_CARD delay_card[ NUM_DELAY_CARDS ];

static void rb_pulser_card_setup( void );


/*-----------------------------------------------------------*
 *-----------------------------------------------------------*/

int rb_pulser_init_hook( void )
{
	int i;
	FUNCTION *f;


	pulser_struct.name     = DEVICE_NAME;
	pulser_struct.has_pods = UNSET;

	/* Set global variable to indicate that Rulbus is needed */

#ifndef RB_PULSER_RULBUS_DEBUG
	need_RULBUS = SET;
#endif

	pulser_struct.set_timebase = rb_pulser_store_timebase;

	pulser_struct.set_trigger_mode = rb_pulser_set_trigger_mode;
	pulser_struct.set_repeat_time = rb_pulser_set_repeat_time;
	pulser_struct.set_trig_in_slope = rb_pulser_set_trig_in_slope;
	pulser_struct.set_function_delay = rb_pulser_set_function_delay;

	pulser_struct.new_pulse = rb_pulser_new_pulse;
	pulser_struct.set_pulse_function = rb_pulser_set_pulse_function;
	pulser_struct.set_pulse_position = rb_pulser_set_pulse_position;
	pulser_struct.set_pulse_length = rb_pulser_set_pulse_length;
	pulser_struct.set_pulse_position_change =
										   rb_pulser_set_pulse_position_change;
	pulser_struct.set_pulse_length_change = rb_pulser_set_pulse_length_change;

	pulser_struct.get_pulse_function = rb_pulser_get_pulse_function;
	pulser_struct.get_pulse_position = rb_pulser_get_pulse_position;
	pulser_struct.get_pulse_length = rb_pulser_get_pulse_length;
	pulser_struct.get_pulse_position_change =
										   rb_pulser_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = rb_pulser_get_pulse_length_change;


	pulser_struct.assign_function = NULL;
	pulser_struct.assign_channel_to_function = NULL;
	pulser_struct.invert_function = NULL;
	pulser_struct.set_max_seq_len = NULL;
	pulser_struct.set_function_high_level = NULL;
	pulser_struct.set_function_low_level = NULL;
	pulser_struct.set_trig_in_level = NULL;
	pulser_struct.set_trig_in_impedance = NULL;
	pulser_struct.set_phase_reference = NULL;
	pulser_struct.get_pulse_phase_cycle = NULL;
	pulser_struct.phase_setup_prep = NULL;
	pulser_struct.phase_setup = NULL;
	pulser_struct.set_phase_switch_delay = NULL;
	pulser_struct.set_pulse_phase_cycle = NULL;
	pulser_struct.set_grace_period = NULL;
	pulser_struct.ch_to_num = NULL;
	pulser_struct.keep_all_pulses = NULL;

	rb_pulser.is_needed = SET;

	rb_pulser.is_timebase = UNSET;
	rb_pulser.is_trig_in_mode = UNSET;
	rb_pulser.is_trig_in_slope = UNSET;
	rb_pulser.is_rep_time = UNSET;
	rb_pulser.is_neg_delay = UNSET;
	rb_pulser.neg_delay = 0;

	rb_pulser.trig_in_mode = INTERNAL;
	rb_pulser.rep_time = 0;

	rb_pulser.synth_pulse_state = NULL;
	rb_pulser.synth_pulse_width = NULL;
	rb_pulser.synth_pulse_delay = NULL;
	rb_pulser.synth_trig_slope = NULL;

	rb_pulser.dump_file = NULL;
	rb_pulser.show_file = NULL;

	rb_pulser.do_show_pulses = UNSET;
	rb_pulser.do_dump_pulses = UNSET;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
        f = rb_pulser.function + i;
        f->self = i;
        f->name = Function_Names[ i ];
        f->num_pulses = 0;
		f->num_active_pulses = 0;
        f->pulses = NULL;
		f->delay_card = NULL;
        f->delay = 0.0;
        f->is_delay = UNSET;
		if ( i != PULSER_CHANNEL_MW &&
			 i != PULSER_CHANNEL_RF &&
			 i != PULSER_CHANNEL_DET )
			f->is_used = UNSET;
		else
			f->is_used = SET;
		f->is_declared = UNSET;
	}

	rb_pulser_card_setup( );

	rb_pulser.function[ PULSER_CHANNEL_MW ].delay_card =
													   delay_card + MW_DELAY_0;
	rb_pulser.function[ PULSER_CHANNEL_RF ].delay_card = delay_card + RF_DELAY;
	rb_pulser.function[ PULSER_CHANNEL_DET ].delay_card =
														delay_card + DET_DELAY;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int rb_pulser_test_hook( void )
{
	/* Check consistency of pulse settings and do everything to setup the
	   pulser for the test run */

	TRY
	{
		if ( rb_pulser.do_show_pulses )
			rb_pulser_show_pulses( );
		if ( rb_pulser.do_dump_pulses )
			rb_pulser_dump_pulses( );
		rb_pulser_init_setup( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( rb_pulser.dump_file )
		{
			fclose( rb_pulser.dump_file );
			rb_pulser.dump_file = NULL;
		}

		if ( rb_pulser.show_file )
		{
			fclose( rb_pulser.show_file );
			rb_pulser.show_file = NULL;
		}

		RETHROW( );
	}

	/* We now need some somewhat different functions for setting of pulse
	   properties */

	pulser_struct.set_pulse_position = rb_pulser_change_pulse_position;
	pulser_struct.set_pulse_length = rb_pulser_change_pulse_length;
	pulser_struct.set_pulse_position_change =
										rb_pulser_change_pulse_position_change;
	pulser_struct.set_pulse_length_change =
										  rb_pulser_change_pulse_length_change;

	if ( rb_pulser_Pulses == NULL )
		rb_pulser.is_needed = UNSET;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int rb_pulser_end_of_test_hook( void )
{
	if ( ! rb_pulser.is_needed )
		return 1;

	if ( rb_pulser.dump_file != NULL )
	{
		fclose( rb_pulser.dump_file );
		rb_pulser.dump_file = NULL;
	}

	if ( rb_pulser.show_file != NULL )
	{
		fclose( rb_pulser.show_file );
		rb_pulser.show_file = NULL;
	}

	rb_pulser_full_reset( );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int rb_pulser_exp_hook( void )
{
	if ( ! rb_pulser.is_needed )
		return 1;

	/* Initialize the device */

	rb_pulser_init( );

	/* Now we have to tell the pulser about all the pulses */

	rb_pulser_do_update_pulses( );

	rb_pulser_run( rb_pulser.is_running );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int rb_pulser_end_of_exp_hook( void )
{
	if ( ! rb_pulser.is_needed )
		return 1;

	rb_pulser_exit( );

	/* Finally reset the internal representation back to its initial state
	   in case another experiment is started */

	rb_pulser_full_reset( );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void rb_pulser_exit_hook( void )
{
	PULSE *p, *pn;
	FUNCTION *f;
	int i;


	if ( ! rb_pulser.is_needed )
		return;

	/* Free all memory allocated within the module */

	p = rb_pulser_Pulses;
	while ( p != NULL )
	{
		pn = p->next;
		T_free( p );
		p = pn;
	}

	if ( rb_pulser.synth_pulse_state )
		rb_pulser.synth_pulse_state =
								  CHAR_P T_free( rb_pulser.synth_pulse_state );

	if ( rb_pulser.synth_pulse_width )
		rb_pulser.synth_pulse_width =
								  CHAR_P T_free( rb_pulser.synth_pulse_width );

	if ( rb_pulser.synth_pulse_delay )
		rb_pulser.synth_pulse_delay =
								  CHAR_P T_free( rb_pulser.synth_pulse_delay );

	if ( rb_pulser.synth_trig_slope )
		rb_pulser.synth_trig_slope =
								  CHAR_P T_free( rb_pulser.synth_trig_slope );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rb_pulser.function + i;
		f->pulses = PULSE_PP T_free( f->pulses );
	}
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var *pulser_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var *pulser_show_pulses( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! FSC2_IS_CHECK_RUN && ! FSC2_IS_BATCH_MODE )
		rb_pulser.do_show_pulses = SET;

	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var *pulser_dump_pulses( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! FSC2_IS_CHECK_RUN && ! FSC2_IS_BATCH_MODE )
		rb_pulser.do_dump_pulses = SET;

	return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------*
 * Switches the output of the pulser on or off
 *---------------------------------------------*/

Var *pulser_state( Var *v )
{
	bool state;


	if ( v == NULL )
		return vars_push( INT_VAR, 1L );

	state = get_boolean( v );

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( INT_VAR, ( long ) ( rb_pulser.is_running = state ) );

	rb_pulser_run( state );
	return vars_push( INT_VAR, ( long ) rb_pulser.is_running );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_update( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! rb_pulser.is_needed )
		return vars_push( INT_VAR, 1L );

	/* Send all changes to the pulser */

	rb_pulser_do_update( );

	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------------*
 * Public function to change the position of pulses. If called with no
 * argument all active pulses that have a position change time set will
 * be moved, otherwise all pulses passed as arguments to the function.
 * Take care: The changes will only be commited on the next call of the
 *            function pulser_update() !
 *----------------------------------------------------------------------*/

Var *pulser_shift( Var *v )
{
	PULSE *p;


	if ( ! rb_pulser.is_needed )
		return vars_push( INT_VAR, 1L );

	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
		for ( p = rb_pulser_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dpos )
				pulser_shift( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = rb_pulser_get_pulse( get_strict_long( v, "pulse number" ) );

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

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
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

Var *pulser_increment( Var *v )
{
	PULSE *p;


	if ( ! rb_pulser.is_needed )
		return vars_push( INT_VAR, 1L );

	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
		for ( p = rb_pulser_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dlen )
				pulser_increment( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = rb_pulser_get_pulse( get_strict_long( v, "pulse number" ) );

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

		if ( ( p->len += p->dlen ) < 0 )
		{
			print( FATAL, "Incrementing the length of pulse #%ld leads to an "
				   "invalid negative pulse length of %s.\n",
				   p->num, rb_pulser_pticks( p->len ) );
			THROW( EXCEPTION );
		}

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
	}

	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var *pulser_reset( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! rb_pulser.is_needed )
		return vars_push( INT_VAR, 1L );

	vars_pop( pulser_pulse_reset( NULL ) );

	return pulser_update( NULL );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var *pulser_pulse_reset( Var *v )
{
	PULSE *p;


	if ( ! rb_pulser.is_needed )
		return vars_push( INT_VAR, 1L );

	/* An empty pulse list means that we have to reset all pulses (even the
       inactive ones). Do't explicitely reset automatically created pulses
	   (which all have a negative pulse number), they will be reset together
	   with the pulses belong to. */

	if ( v == NULL )
	{
		for ( p = rb_pulser_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				vars_pop( pulser_pulse_reset( vars_push( INT_VAR, p->num ) ) );
	}

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		p = rb_pulser_get_pulse( get_strict_long( v, "pulse number" ) );

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


/*-----------------------------------------------------------*
 *-----------------------------------------------------------*/

static void rb_pulser_card_setup( void )
{
	int i;


	clock_card[ ERT_CLOCK ].name  = ERT_CLOCK_CARD;
	clock_card[ TB_CLOCK ].name   = TB_CLOCK_CARD;

	for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
		clock_card[ i ].handle = -1;

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
	{
		delay_card[ i ].handle = -1;
		delay_card[ i ].prev = NULL;
		delay_card[ i ].next = NULL;
		delay_card[ i ].needs_update = UNSET;
		delay_card[ i ].is_active = UNSET;
	}

	delay_card[ ERT_DELAY ]. name = ERT_DELAY_CARD;

	delay_card[ INIT_DELAY ].name = INIT_DELAY_CARD;

	delay_card[ MW_DELAY_0 ].name = MW_DELAY_CARD_0;
	delay_card[ MW_DELAY_0 ].prev = delay_card + INIT_DELAY;
	delay_card[ MW_DELAY_0 ].next = delay_card + MW_DELAY_1;

	delay_card[ MW_DELAY_1 ].name = MW_DELAY_CARD_1;
	delay_card[ MW_DELAY_1 ].prev = delay_card + MW_DELAY_0;
	delay_card[ MW_DELAY_1 ].next = delay_card + MW_DELAY_2;

	delay_card[ MW_DELAY_2 ].name = MW_DELAY_CARD_2;
	delay_card[ MW_DELAY_2 ].prev = delay_card + MW_DELAY_1;
	delay_card[ MW_DELAY_2 ].next = delay_card + MW_DELAY_3;

	delay_card[ MW_DELAY_3 ].name = MW_DELAY_CARD_4;
	delay_card[ MW_DELAY_3 ].prev = delay_card + MW_DELAY_2;
	delay_card[ MW_DELAY_3 ].next = delay_card + MW_DELAY_4;

	delay_card[ MW_DELAY_4 ].name = MW_DELAY_CARD_4;
	delay_card[ MW_DELAY_4 ].prev = delay_card + MW_DELAY_3;

	delay_card[ RF_DELAY ].name = RF_DELAY_CARD;
	delay_card[ RF_DELAY ].prev = delay_card + INIT_DELAY;

	delay_card[ DET_DELAY ].name = DET_DELAY_CARD;
	delay_card[ DET_DELAY ].prev = delay_card + INIT_DELAY;
}
