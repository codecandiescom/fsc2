/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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


static void rb_pulser_w_phase_init( void );

static void rb_pulser_w_synthesizer_init( void );

static void rb_pulser_w_start_external_trigger( void );

static void rb_pulser_w_start_internal_trigger( void );

static void rb_pulser_w_failure( bool         rb_flag,
                                 const char * mess );


/* The following array determines the setting of the end pulse patterns of
 * the delay cards responsible for the phases. They must reflect what
 * Leendert's "magic box" expects as inputs to achieve a certain phase
 * setting. */

#if ! defined RB_PULSER_W_TEST
static int phase_settings[ 4 ][ 2 ][ 2 ] = {
              /* PHASE_PLUS_X  */     { { RULBUS_RB8514_DELAY_OUTPUT_1,
                                          RULBUS_RB8514_DELAY_START_PULSE },
                                        { RULBUS_RB8514_DELAY_OUTPUT_2,
                                          RULBUS_RB8514_DELAY_START_PULSE } },
              /* PHASE_MINUS_X */     { { RULBUS_RB8514_DELAY_OUTPUT_1,
                                          RULBUS_RB8514_DELAY_START_PULSE },
                                        { RULBUS_RB8514_DELAY_OUTPUT_2,
                                          RULBUS_RB8514_DELAY_END_PULSE } },
              /* PHASE_PLUS_Y  */     { { RULBUS_RB8514_DELAY_OUTPUT_1,
                                          RULBUS_RB8514_DELAY_END_PULSE },
                                        { RULBUS_RB8514_DELAY_OUTPUT_2,
                                          RULBUS_RB8514_DELAY_START_PULSE } },
              /* PHASE_MINUS_Y */     { { RULBUS_RB8514_DELAY_OUTPUT_1,
                                          RULBUS_RB8514_DELAY_END_PULSE },
                                        { RULBUS_RB8514_DELAY_OUTPUT_2,
                                          RULBUS_RB8514_DELAY_END_PULSE } } };

#else /* for test mode */

static const char *ps_str[ 4 ][ 2 ] = {
              /* PHASE_PLUS_X  */       { "RULBUS_RB8514_DELAY_OUTPUT_BOTH",
                                          "RULBUS_RB8514_DELAY_PULSE_NONE" },
              /* PHASE_MINUS_X */       { "RULBUS_RB8514_DELAY_OUTPUT_1",
                                          "RULBUS_RB8514_DELAY_END_PULSE" },
              /* PHASE_PLUS_Y  */       { "RULBUS_RB8514_DELAY_OUTPUT_2",
                                          "RULBUS_RB8514_DELAY_END_PULSE" },
              /* PHASE_MINUS_Y */       { "RULBUS_RB8514_DELAY_OUTPUT_BOTH",
                                          "RULBUS_RB8514_DELAY_END_PULSE" } };
#endif


/*----------------------------------------------------------------*
 * Function called at the very start of the experiment to bring
 * the pulser into the state it's supposed to be in. This mostly
 * includes "opening" the Rulbus cards it's made of, setting them
 * up and also initializing the pulse modulation system of the RF
 * synthesizer that takes care of the single possible RF pulse.
 *----------------------------------------------------------------*/

void
rb_pulser_w_init( void )
{
#if ! defined RB_PULSER_W_TEST
    int i;


    raise_permissions( );

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
       would trigger the following cards) */

    if ( rulbus_rb8514_delay_set_output_pulse(
                                    rb_pulser_w.delay_card[ ERT_DELAY ].handle,
                                    RULBUS_RB8514_DELAY_OUTPUT_BOTH,
                                    RULBUS_RB8514_DELAY_PULSE_NONE )
                                                                 != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to stop pulser" );

#ifndef FIXED_TIMEBASE
    /* If the cards are connected to an adjustable frequency clock card start
       the clock to run at the desired frequency */

    if (    rulbus_rb8515_clock_set_frequency(
                                 rb_pulser_w.clock_card[ TB_CLOCK_1 ].handle,
                                 rb_pulser_w.clock_card[ TB_CLOCK_1 ].freq )
         || rulbus_rb8515_clock_set_frequency(
                                 rb_pulser_w.clock_card[ TB_CLOCK_2 ].handle,
                                 rb_pulser_w.clock_card[ TB_CLOCK_2 ].freq )
                                                                 != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to inititialize pulser" );
#endif

    /* In external trigger mode set the trigger input slope of the ERT delay
       card to whatever the user had asked for, make the card emit end pulses
       as fast as possible by setting the delay to 0 and finally set the clock
       for the card to the highest possible frequency. For internal trigger
       mode set the card to trigger on raising edges (it then gets fed its
       own pulse as it's trigger input and must react to the end of the pulse)
       and set the clock card to whatever we calculated is needed for the
       requested repetition time - setting the delay must be kept for later
       when we start the whole pulser. */

    if ( rb_pulser_w.trig_in_mode == EXTERNAL )
    {
        if (    rulbus_rb8514_delay_set_trigger(
                                   rb_pulser_w.delay_card[ ERT_DELAY ].handle,
                                   ( rb_pulser_w.trig_in_slope == POSITIVE ) ?
                                   RULBUS_RB8514_DELAY_RAISING_EDGE :
                                   RULBUS_RB8514_DELAY_FALLING_EDGE )
                                                                != RULBUS_OK
             || rulbus_rb8514_delay_set_raw_delay(
                                   rb_pulser_w.delay_card[ ERT_DELAY ].handle,
                                   1, 1 )                       != RULBUS_OK
             || rulbus_rb8515_clock_set_frequency(
                                   rb_pulser_w.clock_card[ ERT_CLOCK ].handle,
                                   RULBUS_RB8515_CLOCK_FREQ_100MHz )
                                                                != RULBUS_OK )
            rb_pulser_w_failure( SET, "Failure to initialize pulser" );
    }
    else  /* in internal trigger mode */
    {
        if (    rulbus_rb8514_delay_set_trigger(
                                    rb_pulser_w.delay_card[ ERT_DELAY ].handle,
                                    RULBUS_RB8514_DELAY_FALLING_EDGE )
                                                                != RULBUS_OK
             || rulbus_rb8515_clock_set_frequency(
                                   rb_pulser_w.clock_card[ ERT_CLOCK ].handle,
                                   rb_pulser_w.clock_card[ ERT_CLOCK ].freq )
                                                                != RULBUS_OK )
            rb_pulser_w_failure( SET, "Failure to initialize pulser" );
    }

    /* Make all cards output end pulses with a positive polarity. For all cards
       (except the card for the experiment repetition time) set the input
       trigger slope to trigger on raising edge and keep the card from out-
       putting start/end pulses. */

    for ( i = 0; i < NUM_DELAY_CARDS; i++ )
    {
        if ( rulbus_rb8514_delay_set_output_pulse_polarity(
                                    rb_pulser_w.delay_card[ i ].handle,
                                    RULBUS_RB8514_DELAY_PULSE_BOTH,
                                    RULBUS_RB8514_DELAY_POLARITY_POSITIVE )
                                                                != RULBUS_OK )
            rb_pulser_w_failure( SET, "Failure to initialize pulser" );

        if ( i == ERT_DELAY )
            continue;

        if (    rulbus_rb8514_delay_set_trigger(
                                            rb_pulser_w.delay_card[ i ].handle,
                                            RULBUS_RB8514_DELAY_RAISING_EDGE )
                                                                != RULBUS_OK
             || rulbus_rb8514_delay_set_output_pulse(
                                            rb_pulser_w.delay_card[ i ].handle,
                                            RULBUS_RB8514_DELAY_OUTPUT_BOTH,
                                            RULBUS_RB8514_DELAY_PULSE_NONE )
                                                                != RULBUS_OK )
            rb_pulser_w_failure( SET, "Failure to initialize pulser" );
    }

    /* Initially make all delay cards relevant for phase switching output end
       pulses for the +X phase and create a short pulse at one of the cards so
       that we can be sure the switch is in the position for the +X-phase. */

    rb_pulser_w_phase_init( );

    /* Initialize synthesizer if it's required for RF pulses */

    rb_pulser_w_synthesizer_init( );

    /* Set the length of the defense pulse to zero if the user switched off
       automatic generation of the defense pulse but didn't set her/his own
       defense pulse */

    if (    ! rb_pulser_w.function[ PULSER_CHANNEL_DEFENSE ].is_used
         && rulbus_rb8514_delay_set_raw_delay(
                         rb_pulser_w.delay_card[ DEFENSE_DELAY ].handle, 0, 1 )
                                                                 != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to initialize pulser" );

    lower_permissions( );

#else     /* in test mode */
    int i;

    fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, %s, %s )\n",
             rb_pulser_w.delay_card[ ERT_DELAY ].name,
             "RULBUS_RB8514_DELAY_OUTPUT_BOTH",
             "RULBUS_RB8514_DELAY_PULSE_NONE" );
#ifndef FIXED_TIMEBASE
    fprintf( stderr, "rulbus_rb8515_clock_set_frequency( %s, %d )\n",
             rb_pulser_w.clock_card[ TB_CLOCK_1 ].name,
             rb_pulser_w.clock_card[ TB_CLOCK_1 ].freq );
    fprintf( stderr, "rulbus_rb8515_clock_set_frequency( %s, %d )\n",
             rb_pulser_w.clock_card[ TB_CLOCK_2 ].name,
             rb_pulser_w.clock_card[ TB_CLOCK_2 ].freq );
#endif
    if ( rb_pulser_w.trig_in_mode == EXTERNAL )
    {
        fprintf( stderr, "rulbus_rb8514_delay_set_trigger( %s, %s )\n",
                 rb_pulser_w.delay_card[ ERT_DELAY ].name,
                 ( rb_pulser_w.trig_in_slope == POSITIVE ) ?
                 "RULBUS_RB8514_DELAY_RAISING_EDGE" :
                 "RULBUS_RB8514_DELAY_FALLING_EDGE" );
        fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %s, 0, 1 )\n",
                 rb_pulser_w.delay_card[ ERT_DELAY ].name );
        fprintf( stderr, "rulbus_rb8515_clock_set_frequency( %s, %s)\n",
                 rb_pulser_w.clock_card[ ERT_CLOCK ].name,
                 "RULBUS_RB8515_CLOCK_FREQ_100MHz" );
    }
    else  /* in internal trigger mode */
    {
        fprintf( stderr, "rulbus_rb8514_delay_set_trigger( %s, %s )\n",
                 rb_pulser_w.delay_card[ ERT_DELAY ].name,
                 "RULBUS_RB8514_DELAY_FALLING_EDGE" );
        fprintf( stderr, "rulbus_rb8515_clock_set_frequency( %s, %d )\n",
                 rb_pulser_w.clock_card[ ERT_CLOCK ].name,
                 rb_pulser_w.clock_card[ ERT_CLOCK ].freq );
    }

    for ( i = 0; i < NUM_DELAY_CARDS; i++ )
    {
        fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse_polarity( "
                 "%s, %s, %s )\n", rb_pulser_w.delay_card[ i ].name,
                 "RULBUS_RB8514_DELAY_PULSE_BOTH",
                 "RULBUS_RB8514_DELAY_POLARITY_POSITIVE" );
        if ( i == ERT_DELAY )
            continue;
        fprintf( stderr, "rulbus_rb8514_delay_set_trigger( %s, %s )\n",
                 rb_pulser_w.delay_card[ i ].name,
                 "RULBUS_RB8514_DELAY_RAISING_EDGE" );
        fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, "
                 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
                 "RULBUS_RB8514_DELAY_PULSE_NONE )\n",
                 rb_pulser_w.delay_card[ i ].name );
    }

    rb_pulser_w_phase_init( );

    if ( ! rb_pulser_w.function[ PULSER_CHANNEL_DEFENSE ].is_used )
        fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %s, 0, 1 )\n",
                 rb_pulser_w.delay_card[ DEFENSE_DELAY ].name );
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

static void
rb_pulser_w_phase_init( void )
{
    Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + PHASE_DELAY_0;
#if ! defined RB_PULSER_W_TEST
    int is_busy;


    /* Set up all the cards for phase pulses to emit end pulses for a +X
       phase switch setting. Also set them up tp emit negative polarity
       end pulses, that's what the "magic box"expects. */

    for ( ; card != NULL; card = card->next )
        if (    rulbus_rb8514_delay_set_output_pulse_polarity(
                                    card->handle,
                                    RULBUS_RB8514_DELAY_PULSE_BOTH,
                                    RULBUS_RB8514_DELAY_POLARITY_NEGATIVE )
                                                                != RULBUS_OK
             || rulbus_rb8514_delay_set_output_pulse(
                                    card->handle,
                                    phase_settings[ PHASE_PLUS_X ][ 0 ][ 0 ],
                                    phase_settings[ PHASE_PLUS_X ][ 0 ][ 1 ] )
                                                                != RULBUS_OK
             || rulbus_rb8514_delay_set_output_pulse(
                                    card->handle,
                                    phase_settings[ PHASE_PLUS_X ][ 1 ][ 0 ],
                                    phase_settings[ PHASE_PLUS_X ][ 1 ][ 1 ] )
                                                                != RULBUS_OK )
            rb_pulser_w_failure( SET, "Failure to inititialize pulser" );

    /* Set the first of the phase cards to emit a pulse just long enough to
       set the phase switch */

    card = rb_pulser_w.delay_card + PHASE_DELAY_0;
    rb_pulser_w_delay_card_delay( card,
                                  Ticks_ceil( MINIMUM_PHASE_PULSE_LENGTH /
                                              rb_pulser_w.timebase ) );
    raise_permissions( );

    /* Now start this card to make it emit a singe pulse that in turn sets
       the phase switch to +X */

    if ( rulbus_rb8514_software_start( card->handle ) != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to inititialize pulser" );

    /* Wait for the pulse to end */

    do
    {
        is_busy = rulbus_rb8514_delay_busy( card->handle );

        if ( rulbus_errno != RULBUS_OK )
            rb_pulser_w_failure( SET, "Failure to inititialize pulser" );
    } while ( is_busy );

    /* Set all the phase cards to produce no output pulse, thus keeping them
       from influencing the phase switch (in case they are needed they will
       become set up when they do) */

    for ( ; card != NULL; card = card->next )
    {
        if ( rulbus_rb8514_delay_set_raw_delay( card->handle, 0, 0 )
                                                                 != RULBUS_OK )
                rb_pulser_w_failure( SET, "Failure to initialize pulser" );

        card->old_delay = card->old_delay = 0;
    }

#else /* in test mode */
    for ( ; card != NULL; card = card->next )
        fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, %s, "
                 "%s )\n", card->name, ps_str[ PHASE_PLUS_X ][ 0 ],
                 ps_str[ PHASE_PLUS_X ][ 1 ] );
    card = rb_pulser_w.delay_card + PHASE_DELAY_0;
    rb_pulser_w_delay_card_delay( card,
                                  Ticks_ceil( MINIMUM_PHASE_PULSE_LENGTH /
                                              rb_pulser_w.timebase ) );
    fprintf( stderr, "rulbus_rb8514_software_start( %s )\n", card->name );
    for ( ; card != NULL; card = card->next )
    {
        fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %s, 0, 0 )\n",
                 card->name );
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

static void
rb_pulser_w_synthesizer_init( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_RF;
    Var_T *func_ptr;
    int acc;


    if ( ! f->is_used )
        return;

    /* Set the trigger slope to positive */

    if ( ! rb_pulser_w.synth_trig_slope )
        rb_pulser_w_failure( UNSET, "Function for setting synthesizer pulse "
                             "trigger slope is unknown" );

    if ( ( func_ptr = func_get( rb_pulser_w.synth_trig_slope, &acc ) )
                                                                      == NULL )
        rb_pulser_w_failure( UNSET, "Function for setting synthesizer pulse "
                             "trigger slope is not available" );

    vars_push( STR_VAR, "POSITIVE" );
    vars_pop( func_call( func_ptr ) );

    /* Set the minimum pulse delay (only has an effect in single pulse mode) */

    if ( ! rb_pulser_w.synth_pulse_delay )
        rb_pulser_w_failure( UNSET, "Function for setting synthesizer pulse "
                             "delay is unknown" );

    if ( ( func_ptr = func_get( rb_pulser_w.synth_pulse_delay, &acc ) )
                                                                      == NULL )
        rb_pulser_w_failure( UNSET, "Function for setting synthesizer pulse "
                             "delay is not available" );

    vars_push( FLOAT_VAR, SYNTHESIZER_MIN_PULSE_DELAY );
    vars_pop( func_call( func_ptr ) );

#if defined SYNTHESIZER_MAX_PULSES
    /* If there can be more than a single pulse switch to double pulse
       mode if necessary, else make sure pulser is in singe pulse mode */

    if ( ! rb_pulser_w.synth_double_mode )
        rb_pulser_w_failure( UNSET, "Function for switching synthesizer "
                             "between single and double pulse mode is "
                             "unknown" );

    if ( ( func_ptr = func_get( rb_pulser_w.synth_double_mode, &acc ) )
                                                                      == NULL )
        rb_pulser_w_failure( UNSET, "Function for switching synthesizer "
                             "between single and double pulse mode is not "
                             "available" );

    if (    f->num_pulses > 1
         && f->pulses[ 0 ]->is_active
         && f->pulses[ 1 ]->is_active )
        vars_push( STR_VAR, "ON" );
    else
        vars_push( STR_VAR, "OFF" );

    vars_pop( func_call( func_ptr ) );
#endif

    /* Switch pulser on if there are any active pulses */

    if ( ! rb_pulser_w.synth_pulse_state )
        rb_pulser_w_failure( UNSET, "Function for switching synthesizer pulse "
                             "modulation on or off is unknown" );

    if ( ( func_ptr = func_get( rb_pulser_w.synth_pulse_state, &acc ) )
                                                                      == NULL )
        rb_pulser_w_failure( UNSET, "Function for switching synthesizer pulse "
                             "modulation on or off is not available" );

    if (    ( f->num_pulses > 1
              && ( f->pulses[ 0 ]->is_active || f->pulses[ 1 ]->is_active ) )
         || ( f->num_pulses == 1 && f->pulses[ 0 ]->is_active ) )
        vars_push( STR_VAR, "ON" );
    else
        vars_push( STR_VAR, "OFF" );

    vars_pop( func_call( func_ptr ) );

    f->pulses[ 0 ]->was_active = f->pulses[ 0 ]->is_active;
    f->old_num_active_pulses = f->pulses[ 0 ]->is_active ? 1 : 0;
    if ( f->num_pulses > 1 )
    {
        f->pulses[ 1 ]->was_active = f->pulses[ 1 ]->is_active;
        f->old_num_active_pulses += f->pulses[ 1 ]->is_active ? 1 : 0;
    }
}


/*-----------------------------------------------------------*
 * Function called at the end of the experiment to bring the
 * pulser back into a quite state and "close" the Rulbus
 * cards the pulser is mode of
 *-----------------------------------------------------------*/

void
rb_pulser_w_exit( void )
{
    int i;


#if ! defined RB_PULSER_W_TEST

    raise_permissions( );

    /* Stop all open clock cards and close them */

    for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
    {
#if 0
        /* Commented out according to Huibs wishes, he wants the clocks
           to continue to run even after the end of the experiment to
           keep the pulser running as before. */

        rulbus_rb8515_clock_set_frequency( rb_pulser_w.clock_card[ i ].handle,
                                           RULBUS_RB8515_CLOCK_FREQ_OFF );
#endif

        rulbus_card_close( rb_pulser_w.clock_card[ i ].handle );
        rb_pulser_w.clock_card[ i ].handle = -1;
    }

    /* Set the delay for all open delay cards to 0 and close them */

    for ( i = 0; i < NUM_DELAY_CARDS; i++ )
    {
#if 0
        /* Commented out according to Huibs wishes, he wants the pulses
           to stay where they were at the end of the experiment to keep
           the pulser running as before. */

        rulbus_rb8514_delay_set_output_pulse(
                                            rb_pulser_w.delay_card[ i ].handle,
                                            RULBUS_RB8514_DELAY_OUTPUT_BOTH,
                                            RULBUS_RB8514_DELAY_PULSE_NONE );
#endif

        rulbus_card_close( rb_pulser_w.delay_card[ i ].handle );
        rb_pulser_w.delay_card[ i ].handle = -1;
    }

    lower_permissions( );

#else /* in test mode */

    for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
    {
#if 0
        fprintf( stderr, "rulbus_rb8515_clock_set_frequency( %s, "
                 "RULBUS_RB8515_CLOCK_FREQ_OFF )\n",
                 rb_pulser_w.clock_card[ i ].name );
#endif
    }
    for ( i = 0; i < NUM_DELAY_CARDS; i++ )
    {
#if 0
        fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, "
                 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
                 "RULBUS_RB8514_DELAY_PULSE_NONE )*\n",
                 rb_pulser_w.delay_card[ i ].name );
#endif
    }
#endif
}


/*------------------------------------------*
 * Function to "start" or "stop" the pulser
 *------------------------------------------*/

void
rb_pulser_w_run( bool state )
{
#if ! defined RB_PULSER_W_TEST
    Rulbus_Delay_Card_T *card;
    Function_T *f;
    int i;
    Ticks ticks;
    Ticks max_ticks = 0;


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
        /* Keep the ERT delay card from emitting end pulses that would trigger
           the following cards. */

#if ! defined RB_PULSER_W_TEST
        raise_permissions( );

        if ( rulbus_rb8514_delay_set_output_pulse(
                                    rb_pulser_w.delay_card[ ERT_DELAY ].handle,
                                    RULBUS_RB8514_DELAY_OUTPUT_BOTH,
                                    RULBUS_RB8514_DELAY_PULSE_NONE )
                                                                != RULBUS_OK )
            rb_pulser_w_failure( SET, "Failure to stop pulser" );

        lower_permissions( );

        /* Wait until all cards are quiet, i.e. aren't outputting pulses
           anymore (we don't test the cards directly since that takes
           about 8 ms per card but rely on the pulser being quit after
           the longest function) */

        for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
        {
            f = rb_pulser_w.function + i;

            if ( ! f->is_used )
                continue;

            for ( ticks = 0, card = f->delay_card;
                  card != NULL && card->is_active;
                  card = card->next )
                ticks += card->delay;

            max_ticks = Ticks_max( ticks, max_ticks );
        }

        fsc2_usleep( lrnd( ceil( max_ticks *
                                 rb_pulser_w.timebase * 1.0e6 ) ), UNSET );
#else   /* in test mode */
        fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( ERT_DELAY "
                 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
                 "RULBUS_RB8514_DELAY_PULSE_NONE )\n" );
#endif
    }
}


/*----------------------------------------------------------------*
 * This function is for actually starting the pulser when running
 * in external trigger mode
 *----------------------------------------------------------------*/

static void
rb_pulser_w_start_external_trigger( void )
{
    /* In external trigger mode with the external trigger going to the
       ERT_DELAY card just have to make the card output end pulses on both
       the start/end pulse output connector. */

#if ! defined RB_PULSER_W_TEST
    raise_permissions( );

    if ( rulbus_rb8514_delay_set_output_pulse(
                                    rb_pulser_w.delay_card[ ERT_DELAY ].handle,
                                    RULBUS_RB8514_DELAY_OUTPUT_BOTH,
                                    RULBUS_RB8514_DELAY_END_PULSE )
                                                                 != RULBUS_OK )
    	rb_pulser_w_failure( SET, "Failure to start pulser" );

    lower_permissions( );
#else   /* in test mode */
    fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, %s, %s )\n",
             rb_pulser_w.delay_card[ ERT_DELAY ].name,
             "RULBUS_RB8514_DELAY_OUTPUT_BOTH",
             "RULBUS_RB8514_DELAY_END_PULSE" );
#endif
}


/*----------------------------------------------------------*
 * This function is for starting the pulser when running in
 * external trigger mode.
 *----------------------------------------------------------*/

static void
rb_pulser_w_start_internal_trigger( void )
{
    /* In internal trigger mode we have to set the delay required for the
       repetition time (without waiting for the card to finish a pulse already
       being created, make the card output end pulsed on both the start/end
       pulse output connectors and then start it via software */

#if ! defined RB_PULSER_W_TEST
    raise_permissions( );

    if (    rulbus_rb8514_delay_set_raw_delay(
                                   rb_pulser_w.delay_card[ ERT_DELAY ].handle,
                                   rb_pulser_w.delay_card[ ERT_DELAY ].delay,
                                   1 )                          != RULBUS_OK
         || rulbus_rb8514_delay_set_output_pulse(
                                   rb_pulser_w.delay_card[ ERT_DELAY ].handle,
                                   RULBUS_RB8514_DELAY_OUTPUT_BOTH,
                                   RULBUS_RB8514_DELAY_END_PULSE )
                                                                != RULBUS_OK
         || rulbus_rb8514_software_start(
                                   rb_pulser_w.delay_card[ ERT_DELAY ].handle )
                                                                 != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to start pulser" );

    lower_permissions( );
#else   /* in test mode */
    fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %s, %lu, 1 )\n",
             rb_pulser_w.delay_card[ ERT_DELAY ].name,
             rb_pulser_w.delay_card[ ERT_DELAY ].delay );
    fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, %s %s )\n",
             rb_pulser_w.delay_card[ ERT_DELAY ].name,
             "RULBUS_RB8514_DELAY_OUTPUT_BOTH",
             "RULBUS_RB8514_DELAY_END_PULSE" );
    fprintf( stderr, "rulbus_rb8514_software_start( %s )\n",
             rb_pulser_w.delay_card[ ERT_DELAY ].name );
#endif
}


/*-----------------------------------------------------------*
 * Function for making a card either "active" or "inactive".
 * This involves the end pulses of the predecessor (as far
 * as it exits) to be switched on or off, so that the card
 * gets triggered or not, and, in the case of deactivation,
 * the delay getting set to 0, so that no pulse can be
 * created.
 *-----------------------------------------------------------*/

void
rb_pulser_w_delay_card_state( Rulbus_Delay_Card_T * card,
                              bool                  state )
{
#if ! defined RB_PULSER_W_TEST
    unsigned char type;


    /* Most cards are supposed to output an end pulse when activated, only
       the predecessors of the delay cards for creating the delays in
       between microwave pulses (i.e. the ones between the cards that create
       the first and second microwave pulse) should emit start pulses to
       allow shorter pulse spacings. Deactivating is done by making cards
       not output start or end pulses anymore. */

    if ( state == START )
    {
        if (    card == rb_pulser_w.delay_card + MW_DELAY_2
             || card == rb_pulser_w.delay_card + MW_DELAY_4 )
            type = RULBUS_RB8514_DELAY_START_PULSE;
        else
            type = RULBUS_RB8514_DELAY_END_PULSE;
    }
    else
        type = RULBUS_RB8514_DELAY_PULSE_NONE;

    raise_permissions( );

    /* If a card has a predecessor we must make the predecessor create end
       pulses to make the card active, or the predecessor must be kept from
       outputting end pulses in order to deactivate the card */

    if (    card->prev != NULL
         && rulbus_rb8514_delay_set_output_pulse( card->prev->handle,
                                               RULBUS_RB8514_DELAY_OUTPUT_BOTH,
                                               type )            != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to set card state" );

    /* Special handling for the cards of the DETECTION and RF function: here
       the card itself must output end pulses to be active */

    if (    card->prev == NULL && card->next == NULL
         && rulbus_rb8514_delay_set_output_pulse( card->handle,
                                               RULBUS_RB8514_DELAY_OUTPUT_BOTH,
                                               type )            != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to set card state" );

    /* For deactivating the card also keep it from outputting a pulse by
       setting its delay to 0 */

    if (    state == STOP
         && rulbus_rb8514_delay_set_raw_delay( card->handle, 0, 1 )
                                                                 != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to set card state" );

    /* For activating the card also set the delay to the required value */

    if (    state == START
         && rulbus_rb8514_delay_set_raw_delay( card->handle, card->delay, 1 )
                                                                 != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to set card state" );

    lower_permissions( );

#else
    const char *type;

    if ( state == START )
    {
        if (    card == rb_pulser_w.delay_card + MW_DELAY_2
             || card == rb_pulser_w.delay_card + MW_DELAY_4 )
            type = "RULBUS_RB8514_DELAY_START_PULSE";
        else
            type = "RULBUS_RB8514_DELAY_END_PULSE";
    }
    else
        type = "RULBUS_RB8514_DELAY_PULSE_NONE";

    if ( card->prev != NULL )
        fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, "
                 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, %s )\n",
                 card->prev->name, type );
    if ( card->prev == NULL && card->next == NULL )
        fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, "
                 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, %s )\n", card->name, type );
    if ( state == STOP)
        fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %s, 0, 1 )\n",
                 card->name );
    if ( state == START )
        fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %s, %lu, 1 )\n",
                 card->name, card->delay );
#endif
}


/*--------------------------------------------*
 * Function to set the delay for a delay card
 *--------------------------------------------*/

void
rb_pulser_w_delay_card_delay( Rulbus_Delay_Card_T * card,
                              unsigned long         delay )
{
#if ! defined RB_PULSER_W_TEST
    int ret;


    raise_permissions( );

    /* Set the new delay but wait until the card is finished with outputting
       a pulse */

    while ( ( ret = rulbus_rb8514_delay_set_raw_delay( card->handle,
                                                       delay, 0 ) )
                                                       == RULBUS_CARD_IS_BUSY )
        /* empty */ ;

    if ( ret != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to set card delay length" );

    lower_permissions( );
#else
    fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %s, %lu, 0 )\n",
             card->name, delay );
#endif
}


/*------------------------------------------------------------*
 * Function for setting up then end pulses a card responsible
 * for the phase setting emits.
 *------------------------------------------------------------*/

void
rb_pulser_w_set_phase( Rulbus_Delay_Card_T * card,
                       int                   phase )
{
#if ! defined RB_PULSER_W_TEST
    raise_permissions( );

    if (    rulbus_rb8514_delay_set_output_pulse( card->handle,
                                          phase_settings[ phase ][ 0 ][ 0 ],
                                          phase_settings[ phase ][ 0 ][ 1 ] )
                                                                != RULBUS_OK
         || rulbus_rb8514_delay_set_output_pulse( card->handle,
                                          phase_settings[ phase ][ 1 ][ 0 ],
                                          phase_settings[ phase ][ 1 ][ 1 ] )
                                                                != RULBUS_OK )
        rb_pulser_w_failure( SET, "Failure to set card trigger out mode" );

    lower_permissions( );
#else   /* in test mode */
    fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %s, %s, %s )\n",
             card->name, ps_str[ phase ][ 0 ], ps_str[ phase ][ 1 ] );
#endif
}


/*---------------------------------------------------------*
 * Function gets called when communication with the device
 * fails. It stops the running experiment.
 *---------------------------------------------------------*/

static void
rb_pulser_w_failure( bool         rb_flag,
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
    lower_permissions( );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
