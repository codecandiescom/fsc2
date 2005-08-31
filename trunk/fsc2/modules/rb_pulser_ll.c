/*
 *  $Id$
 *
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
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


#include "rb_pulser.h"


static void rb_pulser_failure( bool rb_flag, const char *mess );
static void rb_pulser_synthesizer_init( void );
static void rb_pulser_start_external_trigger( void );
static void rb_pulser_start_internal_trigger( void );


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


	/* Try to get handles for all Rulbus (clock and delay) cards used by
	   the pulser */

	for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
		if ( ( rb_pulser.clock_card[ i ].handle =
			   		 rulbus_card_open( rb_pulser.clock_card[ i ].name ) ) < 0 )
			rb_pulser_failure( SET, "Failure to initialize pulser" );

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
		if ( ( rb_pulser.delay_card[ i ].handle =
					 rulbus_card_open( rb_pulser.delay_card[ i ].name ) ) < 0 )
			rb_pulser_failure( SET, "Failure to initialize pulser" );

	/* Make sure the ERT delay card can't emit end pulses yet (which might
	   in turn trigger the following cards), stop the clock card feeding the
	   ERT delay card and set the delay for the card to 0 */

	if ( rulbus_rb8514_delay_set_output_pulse(
			 						  rb_pulser.delay_card[ ERT_DELAY ].handle,
									  RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									  RULBUS_RB8514_DELAY_PULSE_NONE )
		 														!= RULBUS_OK ||
		 rulbus_rb8515_clock_set_frequency(
			 						  rb_pulser.clock_card[ ERT_CLOCK ].handle,
									  RULBUS_RB8515_CLOCK_FREQ_OFF )
																!= RULBUS_OK ) 
		rb_pulser_failure( SET, "Failure to stop pulser" );

	rb_pulser_delay_card_delay( rb_pulser.delay_card[ ERT_DELAY ].handle, 0 );

	/* In external trigger mode set the trigger input slope of the INIT or
	   ERT delay card (depending on the external trigger going into the INIT
	   or the ERT delay card) to what the user asked for, in internal trigger
	   mode to trigger on raising edge. */

	if ( rb_pulser.trig_in_mode == EXTERNAL )
	{
#if defined EXT_TRIGGER_GOES_TO_INIT_DELAY
		if ( rulbus_rb8514_delay_set_trigger(
				 					 rb_pulser.delay_card[ INIT_DELAY ].handle,
									 ( rb_pulser.trig_in_slope == POSITIVE ) ?
									 RULBUS_RB8514_DELAY_RAISING_EDGE :
									 RULBUS_RB8514_DELAY_FALLING_EDGE )
			 													!= RULBUS_OK )
			rb_pulser_failure( SET, "Failure to initialize pulser" );

		rb_pulser_delay_card_delay( rb_pulser.delay_card[ INIT_DELAY ].handle,
									0 );
#else
		if ( rulbus_rb8514_delay_set_trigger(
				 					  rb_pulser.delay_card[ ERT_DELAY ].handle,
									  ( rb_pulser.trig_in_slope == POSITIVE ) ?
									  RULBUS_RB8514_DELAY_RAISING_EDGE :
									  RULBUS_RB8514_DELAY_FALLING_EDGE )
			 													 != RULBUS_OK )
			rb_pulser_failure( SET, "Failure to initialize pulser" );
#endif
	}
	else
	{
		if ( rulbus_rb8514_delay_set_trigger(
				 					  rb_pulser.delay_card[ ERT_DELAY ].handle,
									  RULBUS_RB8514_DELAY_FALLING_EDGE )
			 													 != RULBUS_OK )
			rb_pulser_failure( SET, "Failure to initialize pulser" );
	}

	/* Make all cards output end pulses with a positive polarity. For all
	   cards (except the card for the experiment repetition time, which
	   must remain inactive until the experiment is started, and the
	   card for the delay for the detection pulse, which is triggered on the
	   falling edge) set the input trigger slope to trigger on raising edge */

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
	{
		if ( rulbus_rb8514_delay_set_output_pulse_polarity(
			 						  rb_pulser.delay_card[ i ].handle,
									  RULBUS_RB8514_DELAY_END_PULSE,
									  RULBUS_RB8514_DELAY_POLARITY_POSITIVE )
			 													 != RULBUS_OK )
			 rb_pulser_failure( SET, "Failure to initialize pulser" );

		if ( i == ERT_DELAY || i == DET_DELAY_0 )
			continue;

#if defined EXT_TRIGGER_GOES_TO_INIT_DELAY
		/* Don't mess with the INIT delay card in external trigger mode when
		   the external trigger is fed to this card */

		if ( i == INIT_DELAY && rb_pulser.trig_in_mode == EXTERNAL )
			continue;
#endif

		if ( rulbus_rb8514_delay_set_trigger( rb_pulser.delay_card[ i ].handle,
									   		 RULBUS_RB8514_DELAY_RAISING_EDGE )
			 													 != RULBUS_OK )
			rb_pulser_failure( SET, "Failure to initialize pulser" );
	}

	/* Set the delay card for the detection delay to trigger on the falling
	   edge (it's connected to the GATE output of the INIT_DELAY card) */

	if ( rulbus_rb8514_delay_set_trigger(
			 						rb_pulser.delay_card[ DET_DELAY_0 ].handle,
									RULBUS_RB8514_DELAY_FALLING_EDGE )
																 != RULBUS_OK )
			rb_pulser_failure( SET, "Failure to initialize pulser" );

	/* Have the clock feeding the delay cards (except the ERT card) run at
	   the frequency required for the timebase */

#ifndef FIXED_TIMEBASE
	if ( rulbus_rb8515_clock_set_frequency(
			 						   rb_pulser.clock_card[ TB_CLOCK ].handle,
									   rb_pulser.clock_card[ TB_CLOCK ].freq )
																 != RULBUS_OK )
		rb_pulser_failure( SET, "Failure to start pulser" );
#endif

	/* Initialize synthesizer if it's required for RF pulses */

	rb_pulser_synthesizer_init( );
}


/*------------------------------------------------------------------------*
 * Function for switching on the synthesizer pulse modulation and setting
 * the pulse delay to the smallest possible value of 20 ns (but that's
 * only a nominal value, the real delay is quite a bit longer and has to
 * be measured carefully, the additional intrinsic delay must be defined
 * in the configuration file under the name SYNTHESIZER_INTRINSIC_DELAY)
 *------------------------------------------------------------------------*/

static void rb_pulser_synthesizer_init( void )
{
	Function_T *f = rb_pulser.function + PULSER_CHANNEL_RF;
	Var_T *func_ptr;
	int acc;


	if ( ! f->is_used )
		return;

	if ( ! rb_pulser.synth_trig_slope )
		rb_pulser_failure( UNSET, "Function for setting synthesizer pulse "
						   "trigger slope is unknown" );

	if ( ( func_ptr = func_get( rb_pulser.synth_trig_slope, &acc ) ) == NULL )
		rb_pulser_failure( UNSET, "Function for setting synthesizer pulse "
						   "trigger slope is not available" );

	vars_push( STR_VAR, "POSITIVE" );
	vars_pop( func_call( func_ptr ) );

	if ( ! rb_pulser.synth_pulse_delay )
		rb_pulser_failure( UNSET, "Function for setting synthesizer pulse "
						   "delay is unknown" );

	if ( ( func_ptr = func_get( rb_pulser.synth_pulse_delay, &acc ) ) == NULL )
		rb_pulser_failure( UNSET, "Function for setting synthesizer pulse "
						   "delay is not available" );

	vars_push( FLOAT_VAR, SYNTHESIZER_MIN_PULSE_DELAY );
	vars_pop( func_call( func_ptr ) );

	if ( ! rb_pulser.synth_pulse_state )
		rb_pulser_failure( UNSET, "Function for switching synthesizer pulse "
						   "modulation on or off is unknown" );

	if ( ( func_ptr = func_get( rb_pulser.synth_pulse_state, &acc ) ) == NULL )
		rb_pulser_failure( UNSET, "Function for switching synthesizer pulse "
						   "modulation on or off is not available" );

	vars_push( STR_VAR, f->pulses[ 0 ]->is_active ? "ON" : "OFF" );
	vars_pop( func_call( func_ptr ) );

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
		if ( rb_pulser.clock_card[ i ].handle >= 0 )
		{
#if 0
			/* Commented out according to Huibs wishes, he wants the clocks
			   to continue to run even after the end of the experiment. */

			rulbus_rb8515_clock_set_frequency(
											  rb_pulser.clock_card[ i ].handle,
											  RULBUS_RB8515_CLOCK_FREQ_OFF );
#endif

			rulbus_card_close( rb_pulser.clock_card[ i ].handle );
			rb_pulser.clock_card[ i ].handle = -1;
		}

	/* Set the delay for all open delay cards to 0 and close them */

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
		if ( rb_pulser.delay_card[ i ].handle >= 0 )
		{
			/* Commented out according to Huibs wishes, he want's the pulses
			   to stay where they were at the end of the experiment. */
#if 0
			rulbus_rb8514_delay_set_output_pulse(
											  rb_pulser.delay_card[ i ].handle,
											  RULBUS_RB8514_DELAY_OUTPUT_BOTH,
											  RULBUS_RB8514_DELAY_PULSE_NONE );
			rb_pulser_delay_card_delay( rb_pulser.delay_card[ i ].handle, 0 );
#endif

			rulbus_card_close( rb_pulser.delay_card[ i ].handle );
			rb_pulser.delay_card[ i ].handle = -1;
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
			rb_pulser_start_external_trigger( );
		else
			rb_pulser_start_internal_trigger( );
	}
	else                        /* stop the pulser */
	{
#if defined EXT_TRIGGER_GOES_TO_INIT_DELAY
			/* In external trigger mode with the external trigger getting fed
			   to the INIT_DELAY card keep the card from outputting pulses */

			if ( rulbus_rb8514_delay_set_output_pulse(
					 				 rb_pulser.delay_card[ INIT_DELAY ].handle,
									 RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									 RULBUS_RB8514_DELAY_PULSE_NONE )
				 												!= RULBUS_OK )
				rb_pulser_failure( SET, "Failure to start pulser" );
			rb_pulser_delay_card_delay(
				                rb_pulser.delay_card[ INIT_DELAY ].handle, 0 );
#else
		/* To stop the pulser keep the ERT delay card from emitting further
		   end pulses that would trigger the following cards, then stop the
		   clock card feeding the delay card and finally set its delay to 0 */

		if ( rulbus_rb8514_delay_set_output_pulse(
									  rb_pulser.delay_card[ ERT_DELAY ].handle,
									  RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									  RULBUS_RB8514_DELAY_PULSE_NONE )
			 													!= RULBUS_OK ||
			 rulbus_rb8515_clock_set_frequency(
				 					  rb_pulser.clock_card[ ERT_CLOCK ].handle,
									  RULBUS_RB8515_CLOCK_FREQ_OFF )
			 													!= RULBUS_OK )
			rb_pulser_failure( SET, "Failure to stop pulser" );
		rb_pulser_delay_card_delay(
				                 rb_pulser.delay_card[ ERT_DELAY ].handle, 0 );
#endif
	}
}


/*-------------------------------------------------------------------*
 * This function is for actually starting the pulser when running in
 * external trigger mode. Depending on the configuration, i.e. if
 * the external trigger goes to the INIT_DELAY or the ERT_DELAY card,
 * different methods have to be used.
 *-----------------------------------------------------------*/


static void rb_pulser_start_external_trigger( void )
{
#if defined EXT_TRIGGER_GOES_TO_INIT_DELAY

	/* In external trigger mode with the external trigger getting fed
	   to the INIT_DELAY card set the delay for this card and enable
	   end pulses */

	if ( rulbus_rb8514_delay_set_output_pulse(
					 				 rb_pulser.delay_card[ INIT_DELAY ].handle,
									 RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									 RULBUS_RB8514_DELAY_END_PULSE )
				 												!= RULBUS_OK )
		rb_pulser_failure( SET, "Failure to start pulser" );

	rb_pulser_delay_card_delay( rb_pulser.delay_card[ INIT_DELAY ].handle,
								rb_pulser.delay_card[ INIT_DELAY ].delay );

	rb_pulser.delay_card[ INIT_DELAY ].old_delay =
									 rb_pulser.delay_card[ INIT_DELAY ].delay;

#else

	/* In external trigger mode with the external trigger going to the
	   ERT_DELAY card set the rate of the clock feeding the ERT delay
	   card to the highest possible rate and set the delay of that card
	   to 0 to make the end pulse appear as fast as possible after the
	   input trigger from the external source and make the card output
	   end pulses on the first start/end pulse output connector. */

	if ( rulbus_rb8514_delay_set_output_pulse(
					 				  rb_pulser.delay_card[ ERT_DELAY ].handle,
									  RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									  RULBUS_RB8514_DELAY_END_PULSE )
				 												!= RULBUS_OK ||
		 rulbus_rb8515_clock_set_frequency(
									  rb_pulser.clock_card[ ERT_CLOCK ].handle,
									  RULBUS_RB8515_CLOCK_FREQ_100MHz )
				 												!= RULBUS_OK )
		rb_pulser_failure( SET, "Failure to start pulser" );

	rb_pulser_delay_card_delay( rb_pulser.delay_card[ ERT_DELAY ].handle, 0 );

#endif
}


/*-----------------------------------------------------------*
 * This function is for actually starting the pulser when running in
 * external trigger mode.
 *-----------------------------------------------------------*/

static void rb_pulser_start_internal_trigger( void )
{
	/* In internal trigger mode set the clock frequency to the value required
	   for the experiment repetition time, set the delay of the ERT card
	   accordingly, make the card output end pulses on both the first and
	   second start/end pulse connector, also have the card for the initial
	   delay output end pulses (it could have been switched off in an earlier
	   experiment with external trigger) and then start a delay via software */

	if ( rulbus_rb8515_clock_set_frequency(
									 rb_pulser.clock_card[ ERT_CLOCK ].handle,
									 rb_pulser.clock_card[ ERT_CLOCK ].freq )
				 												!= RULBUS_OK )
		 rb_pulser_failure( SET, "Failure to start pulser" );


	rb_pulser_delay_card_delay( rb_pulser.delay_card[ ERT_DELAY ].handle,
								rb_pulser.delay_card[ ERT_DELAY ].delay );

	if ( rulbus_rb8514_delay_set_output_pulse(
					 				 rb_pulser.delay_card[ ERT_DELAY ].handle,
									 RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									 RULBUS_RB8514_DELAY_END_PULSE )
				 												!= RULBUS_OK ||
		 rulbus_rb8514_delay_set_output_pulse(
					 				 rb_pulser.delay_card[ INIT_DELAY ].handle,
									 RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									 RULBUS_RB8514_DELAY_END_PULSE )
				 												!= RULBUS_OK ||
		rulbus_rb8514_software_start(
									 rb_pulser.delay_card[ ERT_DELAY ].handle )
																 != RULBUS_OK )
		rb_pulser_failure( SET, "Failure to start pulser" );
}


/*-----------------------------------------------------------*
 * Function for making a card either "active" or "inactive"
 * which just means that it either gets set up to create end
 * pulses or that the creation of end pulses is switched off
 * (and the delay is set to 0 to keep it from creating GATE
 * pulses).
 *-----------------------------------------------------------*/

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


/*---------------------------------------------------------*
 * Function gets called when communication with the device
 * fails. It stops the running experiment.
 *---------------------------------------------------------*/

static void rb_pulser_failure( bool rb_flag, const char *mess )
{
	static int calls = 0;

	if ( calls != 0 )
		return;

	calls++;
	if ( rb_flag )
		print( FATAL, "%s: %s.\n", mess, rulbus_strerror( ) );
	else
		print( FATAL, "%s.\n", mess );
	rb_pulser_exit( );
	calls--;
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
