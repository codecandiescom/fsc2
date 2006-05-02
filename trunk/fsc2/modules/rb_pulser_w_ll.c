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


static void rb_pulser_w_switch_to_x_phase( void );

static void rb_pulser_w_synthesizer_init( void );

static void rb_pulser_w_start_external_trigger( void );

static void rb_pulser_w_start_internal_trigger( void );

static void rb_pulser_w_failure( bool         rb_flag,
								 const char * mess );


#if ! defined RB_PULSER_W_TEST
static int phase_settings[ 4 ][ 2 ] = { { RULBUS_RB8514_DELAY_OUTPUT_BOTH,
										  RULBUS_RB8514_DELAY_PULSE_NONE },
										{ RULBUS_RB8514_DELAY_OUTPUT_1,
										  RULBUS_RB8514_DELAY_END_PULSE },
										{ RULBUS_RB8514_DELAY_OUTPUT_2,
										  RULBUS_RB8514_DELAY_END_PULSE },
										{ RULBUS_RB8514_DELAY_OUTPUT_BOTH,
										  RULBUS_RB8514_DELAY_END_PULSE } };
#else
static const char *ps_str[ 4 ][ 2 ] = { { "RULBUS_RB8514_DELAY_OUTPUT_BOTH",
										  "RULBUS_RB8514_DELAY_PULSE_NONE" },
									    { "RULBUS_RB8514_DELAY_OUTPUT_1",
										  "RULBUS_RB8514_DELAY_END_PULSE" },
									    { "RULBUS_RB8514_DELAY_OUTPUT_2",
										  "RULBUS_RB8514_DELAY_END_PULSE" },
									    { "RULBUS_RB8514_DELAY_OUTPUT_BOTH",
										  "RULBUS_RB8514_DELAY_END_PULSE" } };
#endif


/*---------------------------------------------------------------*
 * Function called at the very start of the experiment to bring
 * the pulser into the state it's supposed to be in. This mainly
 * includes "opening" the Rulbus cards it's made of and setting
 * them up and also initializing the pulse modulation system of
 * the RF synthesizer that takes care of the single possible RF
 * pulse
 *---------------------------------------------------------------*/

void rb_pulser_w_init( void )
{
	Rulbus_Delay_Card_T *card;
	Function_T *f;
	int i;


#if ! defined RB_PULSER_W_TEST
	/* Try to get handles for all Rulbus (clock and delay) cards used by
	   the pulser */

	for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
		if ( ( rb_pulser_w.clock_card[ i ].handle =
			   	   rulbus_card_open( rb_pulser_w.clock_card[ i ].name ) ) < 0 )
			rb_pulser_w_failure( SET, "Failure to initialize pulser" );

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
		if ( ( rb_pulser_w.delay_card[ i ].handle =
				   rulbus_card_open( rb_pulser_w.delay_card[ i ].name ) ) < 0 )
			rb_pulser_w_failure( SET, "Failure to initialize pulser" );

	/* Make sure the ERT delay card can't emit end pulses yet (which in turn
	   would trigger the following cards) and stop the clock card feeding the
	   ERT delay card */

	if ( rulbus_rb8514_delay_set_output_pulse(
			 						rb_pulser_w.delay_card[ ERT_DELAY ].handle,
									RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									RULBUS_RB8514_DELAY_PULSE_NONE )
		 														!= RULBUS_OK ||
		 rulbus_rb8515_clock_set_frequency(
			 						rb_pulser_w.clock_card[ ERT_CLOCK ].handle,
									RULBUS_RB8515_CLOCK_FREQ_OFF )
																!= RULBUS_OK ) 
		rb_pulser_w_failure( SET, "Failure to stop pulser" );

	/* Initially make all delay cards relevant for phase switching output end
	   pulses for the +X phase and create a short pulse at one of the cards so
	   that we can be sure the switch is in the position for +X-phase. */

	rb_pulser_w_switch_to_x_phase( );

	/* In external trigger mode set the trigger input slope of the ERT delay
	   card to whatever the user had asked for and then set the delay for the
	   ERT card to no delay at all (take care: we need to do this without
	   waiting for the card to be finished with outputting a pulses).
	   For internal trigger mode just set the card to trigger on raising edges
	   and set the delay for the card, we're later going to "auto-start" it
	   when everything else is done. */

	if ( rb_pulser_w.trig_in_mode == EXTERNAL )
	{
		if ( rulbus_rb8514_delay_set_trigger(
				 					rb_pulser_w.delay_card[ ERT_DELAY ].handle,
									( rb_pulser_w.trig_in_slope == POSITIVE ) ?
									RULBUS_RB8514_DELAY_RAISING_EDGE :
									RULBUS_RB8514_DELAY_FALLING_EDGE )
			 													!= RULBUS_OK ||
			 rulbus_rb8514_delay_set_raw_delay(
				                    rb_pulser_w.delay_card[ ERT_DELAY ].handle,
				                    0, 1 )                    != RULBUS_OK )
			rb_pulser_w_failure( SET, "Failure to initialize pulser" );
	}
	else  /* in internal trigger mode */
	{
		if ( rulbus_rb8514_delay_set_trigger(
				 					rb_pulser_w.delay_card[ ERT_DELAY ].handle,
									RULBUS_RB8514_DELAY_FALLING_EDGE )
			 													!= RULBUS_OK ||
			 rulbus_rb8514_delay_set_raw_delay(
				                    rb_pulser_w.delay_card[ ERT_DELAY ].handle,
									rb_pulser_w.delay_card[ ERT_DELAY ].delay,
				                    1 )                         != RULBUS_OK )
			rb_pulser_w_failure( SET, "Failure to initialize pulser" );
	}

	/* Make all cards output end pulses with a positive polarity. For all
	   cards (except the card for the experiment repetition time, which must
	   remain inactive until the experiment is started) set the input trigger
	   slope to trigger on raising edge. */

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
	{
		if ( rulbus_rb8514_delay_set_output_pulse_polarity(
				                    rb_pulser_w.delay_card[ i ].handle,
									RULBUS_RB8514_DELAY_END_PULSE,
									RULBUS_RB8514_DELAY_POLARITY_POSITIVE )
			                                                     != RULBUS_OK )
			rb_pulser_w_failure( SET, "Failure to initialize pulser" );

		if ( i == ERT_DELAY )
			continue;

		if ( rulbus_rb8514_delay_set_trigger(
				                            rb_pulser_w.delay_card[ i ].handle,
									   	    RULBUS_RB8514_DELAY_RAISING_EDGE )
			 													 != RULBUS_OK )
			rb_pulser_w_failure( SET, "Failure to initialize pulser" );
	}

#ifndef FIXED_TIMEBASE
	/* Unless the pulser is wired to use a fixed frequency clock card, running
	   at 100 MHz, feeding the delay cards (except the ERT card), have the
	   clock cards run at the frequency required for the timebase */

	if ( rulbus_rb8515_clock_set_frequency(
			 					    rb_pulser_w.clock_card[ TB_CLOCK ].handle,
									rb_pulser_w.clock_card[ TB_CLOCK ].freq )
																 != RULBUS_OK )
		rb_pulser_w_failure( SET, "Failure to start pulser" );
#endif

	/* Initialize synthesizer if it's required for RF pulses */

	rb_pulser_w_synthesizer_init( );

#else     /* in test mode */
	fprintf( stderr, "rb_pulser_w_init()\n" );
	fprintf( stderr, "rulbus_rb8514_delay_set_output_puls( ERT_DELAY, "
			 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
			 "RULBUS_RB8514_DELAY_PULSE_NONE )\n" );
	fprintf( stderr, "rulbus_rb8515_clock_set_frequency( ERT_CLOCK, "
			 "RULBUS_RB8515_CLOCK_FREQ_OFF )\n" );

	rb_pulser_w_switch_to_x_phase( );

	if ( rb_pulser_w.trig_in_mode == EXTERNAL )
	{
		fprintf( stderr, "rulbus_rb8514_delay_set_trigger( ERT_DELAY, %s )\n",
				 ( ( rb_pulser_w.trig_in_slope == POSITIVE ) ?
				   "RULBUS_RB8514_DELAY_RAISING_EDGE" :
				   "RULBUS_RB8514_DELAY_FALLING_EDGE" ) );
		fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( ERT_DELAY, "
				 "0, 1 )\n" );
	}
	else
	{
		fprintf( stderr, "rulbus_rb8514_delay_set_trigger( ERT_DELAY, "
				 "RULBUS_RB8514_DELAY_FALLING_EDGE )\n" );
		fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( ERT_DELAY, "
				 "%lu, 1 )\n", rb_pulser_w.delay_card[ ERT_DELAY ].delay );
	}

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
	{
		fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse_polarity( "
				 "%s, RULBUS_RB8514_DELAY_END_PULSE, "
				 "RULBUS_RB8514_DELAY_POLARITY_POSITIVE )\n",
				 rb_pulser_w.delay_card[ i ].name );

		if ( i == ERT_DELAY )
			continue;

		fprintf(stderr, "rulbus_rb8514_delay_set_trigger( %s, "
				"RULBUS_RB8514_DELAY_RAISING_EDGE )\n",
				rb_pulser_w.delay_card[ i ].name );
	}

#ifndef FIXED_TIMEBASE
	fprintf( stderr, "rulbus_rb8515_clock_set_frequency( TB_CLOCK, %d )\n",
			 rb_pulser_w.clock_card[ TB_CLOCK ].freq );
#endif
#endif
}


/*-------------------------------------------------------------------*
 * Function hat gets called in the initialization of the pulser to
 * (at least) initially set all cards relevant for phase switching
 * to emit end pulses for a +X phase switch setting and one of the
 * card gets started to make sure the phase switch is in this state.
 * Finally the delay time for all the phase cards gets set to the
 * maximum value so that normally they don't output anything (or
 * only the end pulses for the +X phase setting).
 *-------------------------------------------------------------------*/

static void rb_pulser_w_switch_to_x_phase( void )
{
	Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + PHASE_DELAY_0;
	int is_busy;


#if ! defined RB_PULSER_W_TEST
	/* Set up all the cards for phase pulses to emit end pulses for a +X
	   phase switch setting */

	for ( ; card != NULL; card = card->next )
		if ( rulbus_rb8514_delay_set_output_pulse(
			                              card->handle,
										  phase_settings[ PHASE_PLUS_X ][ 0 ],
										  phase_settings[ PHASE_PLUS_X ][ 1 ] )
		                                                         != RULBUS_OK )
			rb_pulser_w_failure( SET, "Failure to inititialize pulser" );

	/* Set the first of them and to emit  pulse just long enough to set the
	   phase switch */

	card = rb_pulser_w.delay_card + PHASE_DELAY_0;
	rb_pulser_w_delay_card_delay( card,
								  Ticks_ceil( MINIMUM_PHASE_PULSE_LENGTH /
											  rb_pulser_w.timebase ) );

#ifndef FIXED_TIMEBASE
	/* If the card isn't connected to a fixed frequency clock card start the
	   clock to run at the desired frequency */

	if ( rulbus_rb8515_clock_set_frequency(
			 					    rb_pulser_w.clock_card[ TB_CLOCK ].handle,
									rb_pulser_w.clock_card[ TB_CLOCK ].freq )
																 != RULBUS_OK )
		rb_pulser_w_failure( SET, "Failure to inititialize pulser" );
#endif

	/* Now start the phase card by software to make it emit a singe pulse
	   that in turn sets the phase switch to +X */

	if ( rulbus_rb8514_software_start( card->handle ) != RULBUS_OK )
		rb_pulser_w_failure( SET, "Failure to inititialize pulser" );

	/* Wait for the pulse to end, then stop the clock */

	do
	{
		is_busy = rulbus_rb8514_delay_busy( card->handle );

		if ( rulbus_errno != RULBUS_OK )
			rb_pulser_w_failure( SET, "Failure to inititialize pulser" );
	} while ( is_busy );
	
#ifndef FIXED_TIMEBASE
	if ( rulbus_rb8515_clock_set_frequency(
			                         rb_pulser_w.clock_card[ TB_CLOCK ].handle,
									 RULBUS_RB8515_CLOCK_FREQ_OFF )
																 != RULBUS_OK )
		rb_pulser_w_failure( SET, "Failure to inititialize pulser" );
#endif

	/* Set all the cards for phases to produce no output pulse thus keeping
	   them from influencing the phase switch */

	for ( ; card != NULL; card = card->next )
	{
		if ( rulbus_rb8514_delay_set_raw_delay( card->handle, 0, 1 )
			                                                   != RULBUS_OK ||
			 rulbus_rb8514_delay_set_trigger(
				                             card->handle,
									   	     RULBUS_RB8514_DELAY_RAISING_EDGE )
			 													 != RULBUS_OK )
				rb_pulser_w_failure( SET, "Failure to initialize pulser" );

		card->old_delay = card->old_delay = 0;
	}

#else /* in test mode */

	fprintf( stderr, "rb_pulser_w_switch_to_x_phase()\n" );

	for ( ; card != NULL; card = card->next )
		fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, %s, "
				 "%s )\n", card->name, ps_str[ PHASE_PLUS_X ][ 0 ],
				 ps_str[ PHASE_PLUS_X ][ 1 ] );

	card = rb_pulser_w.delay_card + PHASE_DELAY_0;
	rb_pulser_w_delay_card_delay( card,
								  Ticks_ceil( MINIMUM_PHASE_PULSE_LENGTH /
											  rb_pulser_w.timebase ) );
#ifndef FIXED_TIMEBASE
	fprintf( stderr, "rulbus_rb8515_clock_set_frequency( TB_CLOCK, %d )\n",
			 rb_pulser_w.clock_card[ TB_CLOCK ].freq );
#endif
	fprintf( stderr, "rulbus_rb8514_software_start( %s )\n", card->name );
#ifndef FIXED_TIMEBASE
	fprintf( stderrm "rulbus_rb8515_clock_set_frequency( TB_CLOCK, "
			 "RULBUS_RB8515_CLOCK_FREQ_OFF )\n" );
#endif
	for ( ; card != NULL; card = card->next )
	{
		fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %s, %lu, 1 )\n",
				 card->name, card->delay );
		fprintf( stderr, "rulbus_rb8514_delay_set_trigger( %s, "
				 "RULBUS_RB8514_DELAY_RAISING_EDGE )\n", card->name );
		card->old_delay = card->old_delay = 0;
	}
#endif
}


/*------------------------------------------------------------------------*
 * Function for switching on the synthesizer pulse modulation and setting
 * the pulse delay to the smallest possible value of 20 ns (but that's
 * only a nominal value, the real delay is quite a bit longer and has to
 * be measured carefully, the additional intrinsic delay must be defined
 * in the configuration file under the name SYNTHESIZER_INTRINSIC_DELAY)
 *------------------------------------------------------------------------*/

static void rb_pulser_w_synthesizer_init( void )
{
	Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_RF;
	Var_T *func_ptr;
	int acc;


	if ( ! f->is_used )
		return;

	if ( ! rb_pulser_w.synth_trig_slope )
		rb_pulser_w_failure( UNSET, "Function for setting synthesizer pulse "
							 "trigger slope is unknown" );

	if ( ( func_ptr = func_get( rb_pulser_w.synth_trig_slope, &acc ) )
		                                                              == NULL )
		rb_pulser_w_failure( UNSET, "Function for setting synthesizer pulse "
							 "trigger slope is not available" );

	vars_push( STR_VAR, "POSITIVE" );
	vars_pop( func_call( func_ptr ) );

	if ( ! rb_pulser_w.synth_pulse_delay )
		rb_pulser_w_failure( UNSET, "Function for setting synthesizer pulse "
							 "delay is unknown" );

	if ( ( func_ptr = func_get( rb_pulser_w.synth_pulse_delay, &acc ) )
		                                                              == NULL )
		rb_pulser_w_failure( UNSET, "Function for setting synthesizer pulse "
							 "delay is not available" );

	vars_push( FLOAT_VAR, SYNTHESIZER_MIN_PULSE_DELAY );
	vars_pop( func_call( func_ptr ) );

	if ( ! rb_pulser_w.synth_pulse_state )
		rb_pulser_w_failure( UNSET, "Function for switching synthesizer pulse "
							 "modulation on or off is unknown" );

	if ( ( func_ptr = func_get( rb_pulser_w.synth_pulse_state, &acc ) )
		                                                              == NULL )
		rb_pulser_w_failure( UNSET, "Function for switching synthesizer pulse "
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

void rb_pulser_w_exit( void )
{
	int i;


#if ! defined RB_PULSER_W_TEST
	/* Stop all open clock cards and close them */

	for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
		if ( rb_pulser_w.clock_card[ i ].handle >= 0 )
		{
#if 0
			/* Commented out according to Huibs wishes, he wants the clocks
			   to continue to run even after the end of the experiment. */

			rulbus_rb8515_clock_set_frequency(
											rb_pulser_w.clock_card[ i ].handle,
											RULBUS_RB8515_CLOCK_FREQ_OFF );
#endif

			rulbus_card_close( rb_pulser_w.clock_card[ i ].handle );
			rb_pulser_w.clock_card[ i ].handle = -1;
		}

	/* Set the delay for all open delay cards to 0 and close them */

	for ( i = 0; i < NUM_DELAY_CARDS; i++ )
		if ( rb_pulser_w.delay_card[ i ].handle >= 0 )
		{
#if 0
			/* Commented out according to Huibs wishes, he want's the pulses
			   to stay where they were at the end of the experiment. */

			rulbus_rb8514_delay_set_output_pulse(
											rb_pulser_w.delay_card[ i ].handle,
											RULBUS_RB8514_DELAY_OUTPUT_BOTH,
											RULBUS_RB8514_DELAY_PULSE_NONE );
#endif

			rulbus_card_close( rb_pulser_w.delay_card[ i ].handle );
			rb_pulser_w.delay_card[ i ].handle = -1;
		}
#else
	fprintf( stderr, "rb_pulser_w_exit()\n" );
#endif
}


/*------------------------------------------*
 * Function to "start" or "stop" the pulser
 *------------------------------------------*/

void rb_pulser_w_run( bool state )
{
	Rulbus_Delay_Card_T *card;
	int is_busy;
	int i;


#if defined RB_PULSER_W_TEST
	fprintf( stderr, "rb_pulser_w_run( %s )\n",
			 state == START ? "START" : "STOP" );
#endif

	if ( state == START )
	{
		/* If phase switching is used set the delay for the stopped cards */

		if ( rb_pulser_w.trig_in_mode == EXTERNAL )
			rb_pulser_w_start_external_trigger( );
		else
			rb_pulser_w_start_internal_trigger( );
	}
	else                        /* stop the pulser */
	{
		/* Keep the ERT delay card from emitting further end pulses that
		   would trigger the following cards, then stop the clock card
		   feeding the ERT delay card */

#if ! defined RB_PULSER_W_TEST
		if ( rulbus_rb8514_delay_set_output_pulse(
									rb_pulser_w.delay_card[ ERT_DELAY ].handle,
									RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									RULBUS_RB8514_DELAY_PULSE_NONE )
			 													!= RULBUS_OK ||
			 rulbus_rb8515_clock_set_frequency(
				 					rb_pulser_w.clock_card[ ERT_CLOCK ].handle,
									RULBUS_RB8515_CLOCK_FREQ_OFF )
			 													!= RULBUS_OK )
			rb_pulser_w_failure( SET, "Failure to stop pulser" );

		/* Wait until all cards (except the ERT and the phase cards) are
		   quiet, i.e. aren't outputting pulses anymore */

		do {
			for ( is_busy = 0, i = 0; i < NUM_DELAY_CARDS && ! is_busy; i++ )
			{
				if ( i == ERT_DELAY )
					continue;

				is_busy = rulbus_rb8514_delay_busy(
										  rb_pulser_w.delay_card[ i ].handle );

				if ( rulbus_errno != RULBUS_OK )
					rb_pulser_w_failure( SET, "Failure to stop pulser" );
			}
		} while ( is_busy );

#else   /* in test mode */
		fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( ERT_DELAY "
				 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
				 "RULBUS_RB8514_DELAY_PULSE_NONE )\n" );
		fprintf( stderr, "rulbus_rb8515_clock_set_frequency( ERT_CLOCK, "
				 "RULBUS_RB8515_CLOCK_FREQ_OFF )\n" );
#endif
	}
}


/*----------------------------------------------------------------*
 * This function is for actually starting the pulser when running
 * in external trigger mode
 *----------------------------------------------------------------*/

static void rb_pulser_w_start_external_trigger( void )
{
	/* In external trigger mode with the external trigger going to the
	   ERT_DELAY card set the rate of the clock feeding the ERT delay
	   card to the highest possible rate and set the delay of that card
	   to 0 to make the end pulse appear as fast as possible after the
	   input trigger from the external source and make the card output
	   end pulses on the first start/end pulse output connector. */

#if ! defined RB_PULSER_W_TEST
	if ( rulbus_rb8514_delay_set_output_pulse(
					 				rb_pulser_w.delay_card[ ERT_DELAY ].handle,
									RULBUS_RB8514_DELAY_OUTPUT_BOTH,
									RULBUS_RB8514_DELAY_END_PULSE )
				 												!= RULBUS_OK ||
		 rulbus_rb8515_clock_set_frequency(
									rb_pulser_w.clock_card[ ERT_CLOCK ].handle,
									RULBUS_RB8515_CLOCK_FREQ_100MHz )
				 												!= RULBUS_OK )
		rb_pulser_w_failure( SET, "Failure to start pulser" );

#else   /* in test mode */
	fprintf( stderr, "rb_pulser_w_start_external_trigger( )\n" );
	fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( ERT_DELAY, "
			 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
			 "RULBUS_RB8514_DELAY_END_PULSE )\n" );
	fprintf( stderr, "rulbus_rb8515_clock_set_frequency( ERT_CLOCK, "
			 "RULBUS_RB8515_CLOCK_FREQ_100MHz )\n" );
#endif
}


/*----------------------------------------------------------*
 * This function is for starting the pulser when running in
 * external trigger mode.
 *----------------------------------------------------------*/

static void rb_pulser_w_start_internal_trigger( void )
{
	/* In internal trigger mode set the clock frequency to the value required
	   for the experiment repetition time, set the delay of the ERT card
	   accordingly, make the card output end pulses on both the first and
	   second start/end pulse connector, also have the card for the initial
	   delay output end pulses (it could have been switched off in an earlier
	   experiment with external trigger) and then start a delay via software */

#if ! defined RB_PULSER_W_TEST
	if ( rulbus_rb8515_clock_set_frequency(
								   rb_pulser_w.clock_card[ ERT_CLOCK ].handle,
								   rb_pulser_w.clock_card[ ERT_CLOCK ].freq )
				 												!= RULBUS_OK ||
		 rulbus_rb8514_delay_set_output_pulse(
					 			   rb_pulser_w.delay_card[ ERT_DELAY ].handle,
								   RULBUS_RB8514_DELAY_OUTPUT_BOTH,
								   RULBUS_RB8514_DELAY_END_PULSE )
				 												!= RULBUS_OK ||
		rulbus_rb8514_software_start(
								   rb_pulser_w.delay_card[ ERT_DELAY ].handle )
																 != RULBUS_OK )
		rb_pulser_w_failure( SET, "Failure to start pulser" );

#else   /* in test mode */
	fprintf( stderr, "rb_pulser_w_start_internal_trigger( )\n" );
	fprintf( stderr, "rulbus_rb8515_clock_set_frequency( ERT_CLOCK, %d )\n",
			 rb_pulser_w.clock_card[ ERT_CLOCK ].freq );
	fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( ERT_DELAY, "
			 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
			 "RULBUS_RB8514_DELAY_END_PULSE )\n" );
	fprintf( stderr, "rulbus_rb8514_software_start( ERT_DELAY )\n" );
#endif
}


/*-----------------------------------------------------------*
 * Function for making a card either "active" or "inactive"
 * which just means that it either gets set up to create end
 * pulses or that the creation of end pulses is switched off
 * (and the delay is set to 0 to keep it from creating GATE
 * pulses).
 *-----------------------------------------------------------*/

void rb_pulser_w_delay_card_state( Rulbus_Delay_Card_T * card,
								   bool                  state )
{
#if ! defined RB_PULSER_W_TEST
	unsigned char type = state == START ?
				RULBUS_RB8514_DELAY_END_PULSE : RULBUS_RB8514_DELAY_PULSE_NONE;


	if ( rulbus_rb8514_delay_set_output_pulse( card->handle
											   RULBUS_RB8514_DELAY_OUTPUT_BOTH,
											   type ) != RULBUS_OK )
		rb_pulser_w_failure( SET, "Failure to set card trigger out mode" );
#else
	fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, "
			 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, %s )\n", card->name,
			 state == START ? "RULBUS_RB8514_DELAY_END_PULSE" :
			                  "RULBUS_RB8514_DELAY_PULSE_NONE" );
#endif
}


/*--------------------------------------------*
 * Function to set the delay for a delay card
 *--------------------------------------------*/

void rb_pulser_w_delay_card_delay( Rulbus_Delay_Card_T * card,
								   unsigned long         delay )
{
#if ! defined RB_PULSER_W_TEST
	int ret;


	/* Set the new delay but wait until the card is finished with outputting
	   a pulse */

	while ( ( ret = rulbus_rb8514_delay_set_raw_delay( card->handle,
													   delay, 0 ) )
			                                           == RULBUS_CARD_IS_BUSY )
		/* empty */ ;

	if ( ret != RULBUS_OK )
		rb_pulser_w_failure( SET, "Failure to set card delay length" );
#else
	fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %s, %lu, 0 )\n",
			 card->name, delay );
#endif
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

void rb_pulser_w_set_phase( Rulbus_Delay_Card_T * card,
							int                   phase )
{
#if ! defined RB_PULSER_W_TEST
	if ( rulbus_rb8514_delay_set_output_pulse( card->handle,
											   phase_settings[ phase ][ 0 ],
											   phase_settings[ phase ][ 1 ] )
									                             != RULBUS_OK )
		rb_pulser_w_failure( SET, "Failure to set card trigger out mode" );
#else   /* in test mode */
	fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, %s, %s )\n",
			 card->name, ps_str[ phase ][ 0 ], ps_str[ phase ][ 1 ] );
#endif
}


/*---------------------------------------------------------*
 * Function gets called when communication with the device
 * fails. It stops the running experiment.
 *---------------------------------------------------------*/

static void rb_pulser_w_failure( bool         rb_flag,
								 const char * mess )
{
	static int calls = 0;

	if ( calls != 0 )
		return;

	calls++;
	if ( rb_flag )
		print( FATAL, "%s: %s.\n", mess, rulbus_strerror( ) );
	else
		print( FATAL, "%s.\n", mess );
	rb_pulser_w_exit( );
	calls--;
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
