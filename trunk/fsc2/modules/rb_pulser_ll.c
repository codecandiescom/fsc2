/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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


static void rb_pulser_failure( bool rb_flag, const char *mess );
static void rb_pulser_synthesizer_init( void );


/*---------------------------------------------------------------*
 * Function called at the very start of the experiment to bring
 * the pulser into the state it's supposed to be in. This mainly
 * includes "opening" the Rulbus cards it's made of and setting
 * them up and also initializing the pulse modulation system of
 * the RF synthesizer that takes care of the single possible RF
 * pulse
 *---------------------------------------------------------------*/

void rb_pulser_init( void )
{
	int i;


	/* Try to open all Rulbus cards used by the pulser */

	for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
		if ( ( clock_card[ i ].handle =
			   				   rulbus_card_open( clock_card[ i ].name ) ) < 0 )
			rb_pulser_failure( SET, "Failure to initialize pulser" );

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
		if ( ( delay_card[ i ].handle =
							   rulbus_card_open( delay_card[ i ].name ) ) < 0 )
			rb_pulser_failure( SET, "Failure to initialize pulser" );

	/* Make sure the ERT delay card can't emit end pulses yet (which could
	   in turn trigger the following cards), then stop the clock card feeding
	   the ERT delay card and finally set the delay for the card to 0 */

	if ( rulbus_rb8514_delay_set_output_pulse( delay_card[ ERT_DELAY ].handle,
										RULBUS_RB8514_DELAY_OUTPUT_BOTH,
										RULBUS_RB8514_DELAY_PULSE_NONE )
		 														!= RULBUS_OK ||
		 rulbus_rb8515_clock_set_frequency( clock_card[ ERT_CLOCK ].handle,
											RULBUS_RB8515_CLOCK_FREQ_OFF )
																!= RULBUS_OK ||
		 rulbus_rb8514_delay_set_raw_delay( delay_card[ ERT_DELAY ].handle,
									 0, 1 ) != RULBUS_OK )
		rb_pulser_failure( SET, "Failure to stop pulser" );

	/* In external trigger mode set the trigger input slope of the ERT delay
	   card to what the user asked for, in internal trigger mode to trigger
	   on raising edge. Set the polarity of the first and the second output
	   to positive in both cases. */

	if ( rb_pulser.trig_in_mode == EXTERNAL )
	{
		if ( rulbus_rb8514_delay_set_trigger( delay_card[ ERT_DELAY ].handle,
									   ( rb_pulser.trig_in_slope 
										 						== POSITIVE ) ?
									   RULBUS_RB8514_DELAY_RAISING_EDGE :
									   RULBUS_RB8514_DELAY_FALLING_EDGE )
			 													 != RULBUS_OK )
			rb_pulser_failure( SET, "Failure to initialize pulser" );
	}
	else
	{
		if ( rulbus_rb8514_delay_set_trigger( delay_card[ ERT_DELAY ].handle,
									   RULBUS_RB8514_DELAY_FALLING_EDGE )
			 													 != RULBUS_OK )
			rb_pulser_failure( SET, "Failure to initialize pulser" );
	}

	if ( rulbus_rb8514_delay_set_output_pulse_polarity(
			 								delay_card[ ERT_DELAY ].handle,
											RULBUS_RB8514_DELAY_END_PULSE,
											RULBUS_RB8514_DELAY_RAISING_EDGE )
																 != RULBUS_OK )
		rb_pulser_failure( SET, "Failure to initialize pulser" );

	/* Set for all cards (except the card for experiment repetition time,
	   which must remain inactive until the experiment is started, and the
	   card for the delay for the detection pulse, which is triggered on the
	   falling edge) the input trigger slope to trigger on raising edge */

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
	{
		if ( i == ERT_DELAY || i == DET_DELAY_0 )
			continue;

		if ( rulbus_rb8514_delay_set_trigger( delay_card[ i ].handle,
									   		 RULBUS_RB8514_DELAY_RAISING_EDGE )
			 													 != RULBUS_OK )
			rb_pulser_failure( SET, "Failure to initialize pulser" );
	}

	/* Set the delay card for the detection delay to trigger on the falling
	   edge (it's connected to the GATE output of the INIT_DELAY card) */

	if ( rulbus_rb8514_delay_set_trigger( delay_card[ DET_DELAY_0 ].handle,
										  RULBUS_RB8514_DELAY_FALLING_EDGE )
																 != RULBUS_OK )
			rb_pulser_failure( SET, "Failure to initialize pulser" );

	/* Have the clock feeding the delay cards (except the ERT card run at
	   the frequency required for the timebase */

#ifndef FIXED_TIMEBASE
	if ( rulbus_rb8515_clock_set_frequency( clock_card[ TB_CLOCK ].handle,
									 clock_card[ TB_CLOCK ].freq )
																 != RULBUS_OK )
		rb_pulser_failure( SET, "Failure to start pulser" );
#endif

	/* Initialize synthesizer if it's required for RF pulses */

	rb_pulser_synthesizer_init( );
}


/*------------------------------------------------------------------------*
 * If RF pulses might be generated switch on synthesizer pulse modulation
 * and set the pulse delay to the smallest possible value of 20 ns
 *------------------------------------------------------------------------*/

static void rb_pulser_synthesizer_init( void )
{
	FUNCTION *f = rb_pulser.function + PULSER_CHANNEL_RF;
	Var *Func_ptr;
	int acc;


	if ( ! f->is_used )
		return;

	if ( ! rb_pulser.synth_trig_slope )
		rb_pulser_failure( UNSET, "Function for setting synthesizer pulse "
						   "trigger slope is unknown" );

	if ( ( Func_ptr = func_get( rb_pulser.synth_trig_slope, &acc ) ) == NULL )
		rb_pulser_failure( UNSET, "Function for setting synthesizer pulse "
						   "trigger slope is not available" );
		
	vars_push( STR_VAR, "POSITIVE" );
	vars_pop( func_call( Func_ptr ) );

	if ( ! rb_pulser.synth_pulse_delay )
		rb_pulser_failure( UNSET, "Function for setting synthesizer pulse "
						   "delay is unknown" );

	if ( ( Func_ptr = func_get( rb_pulser.synth_pulse_delay, &acc ) ) == NULL )
		rb_pulser_failure( UNSET, "Function for setting synthesizer pulse "
						   "delay is not available" );

	vars_push( FLOAT_VAR, 2.0e-8 );
	vars_pop( func_call( Func_ptr ) );

	if ( ! rb_pulser.synth_pulse_state )
		rb_pulser_failure( UNSET, "Function for switching synthesizer pulse "
						   "modulation on or off is unknown" );

	if ( ( Func_ptr = func_get( rb_pulser.synth_pulse_state, &acc ) ) == NULL )
		rb_pulser_failure( UNSET, "Function for switching synthesizer pulse "
						   "modulation on or off is not available" );
		
	vars_push( STR_VAR, f->pulses[ 0 ]->is_active ? "ON" : "OFF" );
	vars_pop( func_call( Func_ptr ) );

	f->pulses[ 0 ]->was_active = f->pulses[ 0 ]->is_active;
}


/*-----------------------------------------------------------*
 * Function called at the end of the experiment to bring the
 * pulser back into a quite state and "close" the Rulbus
 * cards the pulser is mode of
 *-----------------------------------------------------------*/

void rb_pulser_exit( void )
{
	int i;


	/* Stop all open clock cards and close them */

	for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
		if ( clock_card[ i ].handle >= 0 )
		{
			/* Commented out according to Huibs wishes, he wants the clocks
			   to continue to run even after the end of the experiment.

			rulbus_rb8515_clock_set_frequency( clock_card[ i ].handle,
										RULBUS_RB8515_CLOCK_FREQ_OFF );
			*/

			rulbus_card_close( clock_card[ i ].handle );
			clock_card[ i ].handle = -1;
		}

	/* Set the delay for all open delay cards to 0 and close them */

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
		if ( delay_card[ i ].handle >= 0 )
		{
			/* Commented out according to Huibs wishes, he ant's the pulses
			   to stay where they were at the end of the experiment.

			rulbus_rb8514_delay_set_output_pulse( delay_card[ i ].handle,
										   RULBUS_RB8514_DELAY_OUTPUT_BOTH,
										   RULBUS_RB8514_DELAY_PULSE_NONE );
			rulbus_rb8514_delay_set_raw_delay( delay_card[ i ].handle, 0, 1 );

			*/

			rulbus_card_close( delay_card[ i ].handle );
			delay_card[ i ].handle = -1;
		}
}


/*------------------------------------------*
 * Function to "start" or "stop" the pulser
 *------------------------------------------*/

void rb_pulser_run( bool state )
{
	if ( state == START )
	{
		if ( rb_pulser.trig_in_mode == EXTERNAL )
		{
			/* In external trigger mode set the rate of the clock feeding the
			   ERT delay card to the highest possible rate, set the delay
			   of thar card to 0 (so the end pulse comes as fast as possible
			   after the input trigger) and make the card output end pulses
			   on the first start/end pulse output connector */

			if ( rulbus_rb8515_clock_set_frequency(
											 clock_card[ ERT_CLOCK ].handle,
											 RULBUS_RB8515_CLOCK_FREQ_100MHz )
				 												!= RULBUS_OK ||
				 rulbus_rb8514_delay_set_raw_delay(
					 							delay_card[ ERT_DELAY ].handle,
											 	0, 1 ) != RULBUS_OK ||
				 rulbus_rb8514_delay_set_output_pulse(
					 							delay_card[ ERT_DELAY ].handle,
												RULBUS_RB8514_DELAY_OUTPUT_1,
												RULBUS_RB8514_DELAY_END_PULSE )
				 												 != RULBUS_OK )
				rb_pulser_failure( SET, "Failure to start pulser" );
		}
		else
		{
			/* In internal trigger mode set the clock frequency to the value
			   required for the experiment repetition time, set the delay of
			   the card accordingly and the make the card output end pulses
			   on both the first and second start/end pulse connector */

			if ( rulbus_rb8515_clock_set_frequency(
											 clock_card[ ERT_CLOCK ].handle,
											 clock_card[ ERT_CLOCK ].freq )
				 												!= RULBUS_OK ||
				 rulbus_rb8514_delay_set_raw_delay(
					 							delay_card[ ERT_DELAY ].handle,
												delay_card[ ERT_DELAY ].delay,
												1 ) != RULBUS_OK ||
				 rulbus_rb8514_delay_set_output_pulse(
					 						   delay_card[ ERT_DELAY ].handle,
											   RULBUS_RB8514_DELAY_OUTPUT_BOTH,
											   RULBUS_RB8514_DELAY_END_PULSE )
				 												 != RULBUS_OK )
				rb_pulser_failure( SET, "Failure to start pulser" );

			/* now get the ERT card to run by first setting the trigger slope
			   to falling, then to raising edge, afterwards it feeds itself
			   it's own end pulse as trigger input */

			if ( rulbus_rb8514_delay_set_trigger(
											delay_card[ ERT_DELAY ].handle,
										    RULBUS_RB8514_DELAY_FALLING_EDGE )
			 												    != RULBUS_OK ||
				 rulbus_rb8514_delay_set_trigger(
					 					   delay_card[ ERT_DELAY ].handle,
										   RULBUS_RB8514_DELAY_RAISING_EDGE )
																!= RULBUS_OK )
				rb_pulser_failure( SET, "Failure to start pulser" );
		}
	}
	else                        /* stop the pulser */
	{
		/* To stop the pulser keep the ERT delay card from emitting end pulses
		   that would trigger the following cards, then stop the clock card
		   feeding the ERT delay card and finally set the delay for the card
		   to 0 */

		if ( rulbus_rb8514_delay_set_output_pulse(
											delay_card[ ERT_DELAY ].handle,
											RULBUS_RB8514_DELAY_OUTPUT_BOTH,
											RULBUS_RB8514_DELAY_PULSE_NONE )
			 													!= RULBUS_OK ||
			 rulbus_rb8515_clock_set_frequency( clock_card[ ERT_CLOCK ].handle,
												RULBUS_RB8515_CLOCK_FREQ_OFF )
			 													!= RULBUS_OK ||
			 rulbus_rb8514_delay_set_raw_delay( delay_card[ ERT_DELAY ].handle,
										 0, 1 ) != RULBUS_OK )
			rb_pulser_failure( SET, "Failure to stop pulser" );
	}
}


/*----------------------------------------------------------*
 * Function for making a card either "active" or "inactive"
 * which just means that it get set up to create end pulses
 * or creation of end pulses is switched off (and the delay
 * is set to 0 to keep it from creating GATE pulses)
 *----------------------------------------------------------*/

void rb_pulser_delay_card_state( int handle, bool state )
{
	unsigned char type = state == START ?
				RULBUS_RB8514_DELAY_END_PULSE : RULBUS_RB8514_DELAY_PULSE_NONE;


	if ( rulbus_rb8514_delay_set_output_pulse(
									   handle, RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									   type ) != RULBUS_OK )
		rb_pulser_failure( SET, "Failure to set card trigger out mode" );

	if ( state == STOP )
		rb_pulser_delay_card_delay( handle, 0 );
}


/*--------------------------------------------*
 * Function to set the delay for a delay card
 *--------------------------------------------*/

void rb_pulser_delay_card_delay( int handle, unsigned long delay )
{
	if ( rulbus_rb8514_delay_set_raw_delay( handle, delay, 1 ) != RULBUS_OK )
		rb_pulser_failure( SET, "Failure to set card delay length" );
}


/*-----------------------------------------------------------*
 *-----------------------------------------------------------*/

static void rb_pulser_failure( bool rb_flag, const char *mess )
{
	if ( rb_flag )
		print( FATAL, "%s: %s.\n", mess, rulbus_strerror( ) );
	else
		print( FATAL, "%s.\n", mess );
	rb_pulser_exit( );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
