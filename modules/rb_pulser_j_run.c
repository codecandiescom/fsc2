/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "rb_pulser_j.h"


static void rb_pulser_j_function_init( void );

static void rb_pulser_j_init_delay( void );

static void rb_pulser_j_delay_card_setup( void );

static void rb_pulser_j_commit( bool flag );

static void rb_pulser_j_rf_pulse( void );

static void rb_pulser_j_seq_length_check( void );


/*---------------------------------------------------------------------*
 * Function is called in the experiment after pulses have been changed
 * to update the pulser accordingly.
 *---------------------------------------------------------------------*/

void
rb_pulser_j_do_update( void )
{
    bool restart = UNSET;


    /* Stop the pulser while the update is done */

    if ( rb_pulser_j.is_running )
    {
        restart = SET;
        if ( FSC2_MODE == EXPERIMENT )
            rb_pulser_j_run( STOP );
    }

    /* Recount and resort the pulses according to their positions, check
       that the new settings are reasonable and then commit all changes */

    rb_pulser_j_update_pulses( FSC2_MODE == TEST );

    /* Restart the pulser if necessary */

    if ( restart && FSC2_MODE == EXPERIMENT )
        rb_pulser_j_run( START );
}


/*---------------------------------------------------------------------*
 * Recounts and resort the pulses according to their positions, checks
 * that the new settings are reasonable and then commit all changes.
 *---------------------------------------------------------------------*/

void
rb_pulser_j_update_pulses( bool flag )
{
    rb_pulser_j_function_init( );
    rb_pulser_j_init_delay( );
    rb_pulser_j_delay_card_setup( );
    rb_pulser_j_rf_pulse( );

    /* Now calculate the pulse sequence length and then commit changes */

    rb_pulser_j_seq_length_check( );

    rb_pulser_j_commit( flag );
}


/*-----------------------------------------------------------------*
 * Before the pulses can be updated we must count how many pulses
 * there are per function after the update and sort them according
 * to their positions.
 *-----------------------------------------------------------------*/

static void
rb_pulser_j_function_init( void )
{
    int i, j;
    Function_T *f;


    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_j.function + i;

        if ( ! f->is_used )
            continue;

        for ( f->num_active_pulses = 0, j = 0; j < f->num_pulses; j++ )
            if ( f->pulses[ j ]->is_active )
                f->num_active_pulses++;

        if ( f->num_pulses > 1 )
            qsort( f->pulses, f->num_pulses, sizeof *f->pulses,
                   rb_pulser_j_start_compare );
    }
}


/*----------------------------------------------------------------------*
 * Function sets the delay for the very first delay card, i.e. the card
 * controlling the delay of the first microwave pulse (and all other
 * channels). If there's no MW pulse the delay for this card is set to
 * the shortest possible time. Since the GATE output from this card also
 * triggers the detection delay card the length of this delay can never
 * be zero but must be at least INIT_DELAY_MINIMUM_DELAY_TICKS long!
 *----------------------------------------------------------------------*/

static void
rb_pulser_j_init_delay( void )
{
    Function_T *f = rb_pulser_j.function + PULSER_CHANNEL_MW;
    Pulse_T *p;
    double pos, shift;
    Rulbus_Delay_Card_T *card = rb_pulser_j.delay_card + INIT_DELAY;


    card->was_active = card->is_active;
    card->is_active = SET;

    /* If there's no active MW pulse the initial delay card always gets
       set to the shortest possible delay at which there's still a pulse
       output (needed for triggering the detection delay card), otherwise
       to the value required for delaying the first MW pulse correctly (but
       not shorter than the minimum length). */

    if ( f->num_active_pulses == 0 )
    {
        card->delay = INIT_DELAY_MINIMUM_DELAY_TICKS;
        return;
    }

    p = f->pulses[ 0 ];

    pos =   p->pos + p->function->delay * rb_pulser_j.timebase
          - rb_pulser_j.delay_card[ ERT_DELAY  ].intr_delay
          - rb_pulser_j.delay_card[ INIT_DELAY ].intr_delay
          - rb_pulser_j.delay_card[ MW_DELAY_0 ].intr_delay;

    if ( pos / rb_pulser_j.timebase - INIT_DELAY_MINIMUM_DELAY_TICKS <
                                                                 - PRECISION )
    {
        print( FATAL, "First MW pulse starts too early.\n" );
        THROW( EXCEPTION );
    }

    if ( pos / rb_pulser_j.timebase - INIT_DELAY_MINIMUM_DELAY_TICKS < 0.0 )
        pos = INIT_DELAY_MINIMUM_DELAY_TICKS * rb_pulser_j.timebase;

    shift =   Ticks_rnd( pos / rb_pulser_j.timebase )
            * rb_pulser_j.timebase - pos;

    if ( fabs( shift ) > PRECISION * rb_pulser_j.timebase )
    {
        print( SEVERE, "Position of first MW pulse not possible, must shift "
               "it by %s.\n", rb_pulser_j_ptime( shift ) );
        pos += shift;
    }

    card->delay = Ticks_rnd( pos / rb_pulser_j.timebase );
}


/*------------------------------------------------------------------------*
 * Function for setting all the other delay cards, both the cards for the
 * lengths of the pulses as well as the cards creating the delays between
 * the pulses.
 *------------------------------------------------------------------------*/

static void
rb_pulser_j_delay_card_setup( void )
{
    Rulbus_Delay_Card_T *card;
    Function_T *f;
    Pulse_T *p;
    volatile double start, delta, shift;
    Ticks dT;
    int i, j;


    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_j.function + i;

        if ( ! f->is_used )
            continue;

        /* Declare all delay cards associated with the function as inactive */

        for ( card = f->delay_card; card != NULL; card = card->next )
        {
            card->was_active = card->is_active;
            card->is_active = UNSET;
        }

        /* Loop over all active pulses of the function (which are already
           ordered by start position */

        start =   rb_pulser_j.delay_card[ ERT_DELAY  ].intr_delay
                + rb_pulser_j.delay_card[ INIT_DELAY ].intr_delay
                + rb_pulser_j.delay_card[ INIT_DELAY ].delay
                * rb_pulser_j.timebase;

        for ( card = f->delay_card, j = 0; j < f->num_active_pulses; j++ )
        {
            p = f->pulses[ j ];

            /* If no cards are left for the function we have too many pulses */

            if ( card == NULL )
            {
                print( FATAL, "Too many active pulses for function '%s'.\n",
                       f->name );
                THROW( EXCEPTION );
            }

            /* All except the first MW pulse have a previous delay card which
               is not the delay card for the initial delay and which must be
               set to get the pulse to start at the correct moment */

            if (    f->self != PULSER_CHANNEL_MW
                 || ( f->self == PULSER_CHANNEL_MW && j != 0 ) )
            {
                /* Find out the first possible moment the following pulse
                   could start at - if there's no following delay card
                   creating the pulse we must not include the delay for this
                   non-existent card. But, on the other hand, for the RF
                   channel we have to include the intrinsic delay of the
                   synthesizer creating the pulse... */

                start += card->intr_delay;
                if ( card->next != NULL )
                    start += card->next->intr_delay;
                else if ( f->self == PULSER_CHANNEL_RF )
                    start += SYNTHESIZER_INTRINSIC_DELAY;

                /* Check that the pulse doesn't start earlier than that */

                delta = p->pos + f->delay - start;

                if ( delta < - PRECISION * rb_pulser_j.timebase )
                {
                    if ( j == 0 )
                        print( FATAL, "Pulse #%ld of function '%s' starts too "
                               "early.\n", p->num, f->name );
                    else
                        print( FATAL, "Pulse #%ld of function '%s' gets too "
                               "near to pulse #%ld.\n", p->num, f->name,
                               f->pulses[ j - 1 ]->num );
                    THROW( EXCEPTION );
                }

                if ( delta < 0.0 )
                    delta = 0.0;

                dT = Ticks_rnd( delta / rb_pulser_j.timebase );
                shift = dT * rb_pulser_j.timebase - delta;

                if ( fabs( shift ) > PRECISION * rb_pulser_j.timebase )
                    print( SEVERE, "Position of pulse #%ld of function '%s' "
                           "not possible, must shift it by %s.\n", p->num,
                           f->name, rb_pulser_j_ptime( shift ) );

                card->delay = dT;
                card->is_active = SET;

                start += card->delay * rb_pulser_j.timebase;

                /* Some channels have no card for the pulse itself, here we
                   just store the length and deal with this case later */

                if ( ( card = card->next ) == NULL )
                {
                    f->last_pulse_len = p->len * rb_pulser_j.timebase;
                    continue;
                }
            }
            else
                start += card->intr_delay;

            card->delay = p->len;
            start += card->delay * rb_pulser_j.timebase;
            card->is_active = card->delay != 0;
            card = card->next;
        }
    }
}


/*------------------------------------------------------------------------
 * Function is called after the test run and experiments to reset all the
 * variables describing the state of the pulser to their initial values.
 *------------------------------------------------------------------------*/

void
rb_pulser_j_full_reset( void )
{
    Pulse_T *p = rb_pulser_j.pulses;
    Rulbus_Delay_Card_T *card;
    size_t i;


    while ( p != NULL )
    {
        p->pos     = p->initial_pos;
        p->is_pos  = p->initial_is_pos;
        p->len     = p->initial_len;
        p->is_len  = p->initial_is_len;
        p->dpos    = p->initial_dpos;
        p->is_dpos = p->initial_is_dpos;
        p->dlen    = p->initial_dlen;
        p->is_dlen = p->initial_is_dlen;

        p->is_active = IS_ACTIVE( p );

        p = p->next;
    }

    /* Make sure all cards (except the ERT card) are inactive (i.e. have a
       delay of 0 and don't output trigger pulses) */

    for ( i = INIT_DELAY, card = rb_pulser_j.delay_card + i;
          i < NUM_DELAY_CARDS; card++, i++ )
    {
        card->is_active =
        card->was_active = UNSET;
        card->delay = card->old_delay = 0;

#if ! defined RB_PULSER_J_TEST
        rb_pulser_j_delay_card_state( card->handle, STOP );

#else   /* in test mode */
        fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %d, "
                 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
                 "RULBUS_RB8514_DELAY_PULSE_NONE )\n", i );
#endif
    }

    /* The card for the initial delay needs some special handling, otherwise
       its delay won't get set even though it should (thanks to Huib for
       pointing out that problem) */

    rb_pulser_j.delay_card[ INIT_DELAY ].old_delay = -1;
}


/*------------------------------------------------------------------*
 * Functions determines the length of the pulse sequence and throws
 * an exception if that's longer than possible with the pulser.
 *------------------------------------------------------------------*/

static void
rb_pulser_j_seq_length_check( void )
{
    int i;
    Function_T *f;
    Rulbus_Delay_Card_T *card;
    double max_seq_len = 0.0, seq_len;


    if ( rb_pulser_j.trig_in_mode == EXTERNAL )
        return;

    /* Determine length of sequence lengths of the individual functions */

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_j.function + i;

        /* Nothing to be done for unused functions and the phase functions */

        if ( ! f->is_used )
            continue;

        seq_len = 0.0;
        for ( card = f->delay_card; card != NULL && card->is_active;
              card = card->next )
            seq_len += card->delay * rb_pulser_j.timebase + card->intr_delay;

        max_seq_len = d_max( max_seq_len, seq_len );
    }

    max_seq_len +=   rb_pulser_j.delay_card[ INIT_DELAY ].delay
                   * rb_pulser_j.timebase
                   + rb_pulser_j.delay_card[ INIT_DELAY ].intr_delay;

    if ( max_seq_len > rb_pulser_j.rep_time )
    {
        print( FATAL, "Pulse sequence is longer than the experiment "
               "repetition time.\n" );
        THROW( EXCEPTION );
    }
}


/*-------------------------------------------------------------------*
 * Changes the pulse pattern in all channels so that the data in the
 * pulser get in sync with the its internal representation.
 * 'flag' is set during the test run.
 *-------------------------------------------------------------------*/

static void
rb_pulser_j_commit( bool flag )
{
    Rulbus_Delay_Card_T *card;
    size_t i;


    if ( flag )
    {
        if ( rb_pulser_j.dump_file != NULL )
            rb_pulser_j_write_pulses( rb_pulser_j.dump_file );
        if ( rb_pulser_j.show_file != NULL )
            rb_pulser_j_write_pulses( rb_pulser_j.show_file );

        return;
    }

    for ( i = 0, card = rb_pulser_j.delay_card;
          i < NUM_DELAY_CARDS; card++, i++ )
    {
        /* Don't mess with the ERT_DELAY card, it only gets set by starting
           or stopping the pulser (if at all) */

        if ( i == ERT_DELAY )
            continue;

        if ( card->was_active && ! card->is_active )
        {
#if ! defined RB_PULSER_J_TEST
            rb_pulser_j_delay_card_state( card->handle, STOP );

#else   /* in test mode */
            fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %d, "
                     "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
                     "RULBUS_RB8514_DELAY_PULSE_NONE )\n", i );
#endif
            card->delay = card->old_delay = 0;
            continue;
        }

        if ( ! card->was_active && card->is_active )
#if ! defined RB_PULSER_J_TEST
            rb_pulser_j_delay_card_state( card->handle, START );

#else   /* in test mode */
        fprintf( stderr, "rulbus_rb8514_delay_set_output_pulse( %d, "
                 "RULBUS_RB8514_DELAY_OUTPUT_BOTH, "
                 "RULBUS_RB8514_DELAY_END_PULSE )\n", i );
#endif

        if ( card->old_delay != card->delay )
        {
#if ! defined RB_PULSER_J_TEST
            rb_pulser_j_delay_card_delay( card->handle, card->delay );

#else   /* in test mode */
            fprintf( stderr, "rulbus_rb8514_delay_set_raw_delay( %d, "
                     "%lu, 0 )\n", i, card->delay );
#endif
            card->old_delay = card->delay;
        }
    }
}


/*----------------------------------------------------------------*
 * Function for telling the synthesizer about the RF pulse length
 *----------------------------------------------------------------*/

static void
rb_pulser_j_rf_pulse( void )
{
    Function_T *f = rb_pulser_j.function + PULSER_CHANNEL_RF;
    Pulse_T *p;
    Var_T *func_ptr;
    int acc;


    if ( ! f->is_used )
        return;

    p = *f->pulses;

    if ( p->is_active )
    {
#if ! defined RB_PULSER_J_TEST
        if ( ( func_ptr = func_get( rb_pulser_j.synth_pulse_width, &acc ) )
                                                                      == NULL )
        {
            print( FATAL, "Function for setting synthesizer pulse length is "
                   "not available.\n" );
            THROW( EXCEPTION );
        }

        vars_push( FLOAT_VAR, f->last_pulse_len );
        vars_pop( func_call( func_ptr ) );

#else   /* in test mode */
        fprintf( stderr, "synthesizer_pulse_width( %lf )\n",
                 f->last_pulse_len );
#endif
    }

    /* Switch synthesizer output on or off if the pulse just became active or
       inactive */

    if (    ( p->was_active && ! p->is_active )
         || ( ! p->was_active && p->is_active ) )
    {
#if ! defined RB_PULSER_J_TEST
        if ( ( func_ptr = func_get( rb_pulser_j.synth_state, &acc ) ) == NULL )
        {
            print( FATAL, "Function for setting synthesizer output state is "
                   "not available.\n" );
            THROW( EXCEPTION );
        }

        if ( p->was_active && ! p->is_active )
            vars_push( STR_VAR, "OFF" );
        else
            vars_push( STR_VAR, "ON" );

        vars_pop( func_call( func_ptr ) );

#else   /* in test mode */
        fprintf( stderr, "synthesizer_pulse_state( \"%s\" )\n",
                 ( p->was_active && ! p->is_active ) ? "OFF" : "ON" );
#endif

        p->was_active = p->is_active;
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
