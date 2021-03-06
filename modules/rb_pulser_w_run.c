/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


#include "rb_pulser_w.h"


static void rb_pulser_w_function_init( void );

static void rb_pulser_w_delay_card_setup( void );

static void rb_pulser_w_mw_channel_setup( void );

static void rb_pulser_w_phase_channel_setup( void );

static void rb_pulser_w_rf_channel_setup( void );

static void rb_pulser_w_laser_channel_setup( void );

static void rb_pulser_w_detection_channel_setup( void );

static void rb_pulser_w_defense_channel_setup( void );

static void rb_pulser_w_auto_defense_channel_setup( void );

static void rb_pulser_w_seq_length_check( void );

static void rb_pulser_w_commit( bool test_only );

static void rb_pulser_w_set_phases( void );

static void rb_pulser_w_rf_pulse( void );


/*--------------------------------------------------------------------*
 * Function is called either in the experiment after pulses have been
 * changed to update the pulser accordingly or as the last step in
 * the inititialization of the pulser.
 *--------------------------------------------------------------------*/

void
rb_pulser_w_do_update( void )
{
    bool restart = UNSET;


    /* Stop the pulser while the update is done */

    if ( rb_pulser_w.is_running )
    {
        restart = SET;
        if ( FSC2_MODE == EXPERIMENT )
            rb_pulser_w_run( STOP );
    }

    /* Recount and resort the pulses according to their positions, check
       that the new settings are reasonable, set the defense pulse if
       necessary and then commit all changes */

    rb_pulser_w_update_pulses( FSC2_MODE == TEST );

    /* Restart the pulser if it was running when we started */

    if ( restart && FSC2_MODE == EXPERIMENT )
        rb_pulser_w_run( START );
}


/*---------------------------------------------------------------------*
 * Recounts and resort the pulses according to their positions, checks
 * that the new settings are reasonable and then commits all changes.
 *---------------------------------------------------------------------*/

void
rb_pulser_w_update_pulses( bool test_only )
{
    rb_pulser_w_function_init( );
    rb_pulser_w_delay_card_setup( );
    rb_pulser_w_rf_pulse( );

    /* Now calculate the pulse sequence length and then commit changes */

    rb_pulser_w_seq_length_check( );

    rb_pulser_w_commit( test_only );
}


/*-----------------------------------------------------------------*
 * Before the pulses can be updated we must count how many pulses
 * there are per function after the update and sort them according
 * to their positions (at least if there's more than one active
 * pulse).
 *-----------------------------------------------------------------*/

static void
rb_pulser_w_function_init( void )
{
    int i, j;
    Function_T *f;


    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_w.function + i;

        if (    ! f->is_used
             || (    i == PULSER_CHANNEL_DEFENSE
                  && rb_pulser_w.defense_pulse_mode ) )
            continue;

        f->old_num_active_pulses = f->num_active_pulses;

        for ( f->num_active_pulses = 0, j = 0; j < f->num_pulses; j++ )
            if ( f->pulses[ j ]->is_active )
                f->num_active_pulses++;

        if ( f->num_pulses > 1 )
            qsort( f->pulses, f->num_pulses, sizeof *f->pulses,
                   rb_pulser_w_start_compare );
    }
}


/*-----------------------------------------------------------------------*
 * Function for setting all the delay cards, i.e. both the cards for the
 * lengths of the pulses and the cards creating the delays between the
 * pulses. This also involves checking that the pulses don't overlap or
 * can't be set for other reasons.
 *-----------------------------------------------------------------------*/

static void
rb_pulser_w_delay_card_setup( void )
{
    rb_pulser_w_mw_channel_setup( );
    rb_pulser_w_rf_channel_setup( );
    rb_pulser_w_laser_channel_setup( );
    rb_pulser_w_detection_channel_setup( );
    if ( rb_pulser_w.defense_pulse_mode )
        rb_pulser_w_auto_defense_channel_setup( );
    else
        rb_pulser_w_defense_channel_setup( );
}


/*--------------------------------------------------------------------*
 * Function for setting up the cards that create the microwave pulses
 * and, if necessary, the phase pulse cards.
 *--------------------------------------------------------------------*/

static void
rb_pulser_w_mw_channel_setup( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_MW;
    Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + MW_DELAY_0;
    Rulbus_Delay_Card_T *cur_card;
    Pulse_T **pulses = f->pulses;
    double start, delta, shift;
    Ticks dT;
    int i;


    if ( ! f->is_used )
        return;

    for ( cur_card = card; cur_card != NULL; cur_card = cur_card->next )
    {
        cur_card->was_active = cur_card->is_active;
        cur_card->is_active = UNSET;
    }

    if ( ! IS_ACTIVE( *pulses ) )
    {
        if ( rb_pulser_w.needs_phases )
            rb_pulser_w_phase_channel_setup( );
        return;
    }

    /* Now loop over the microwave pulses, setting up the cards and
       checking that the pulses don't get too near to each other */

    cur_card = card;
    start = rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay + f->delay;

    for ( i = 0; i < f->num_pulses && IS_ACTIVE( pulses[ i ] ); i++ )
    {
        fsc2_assert( cur_card != NULL && cur_card->next != NULL );

        /* Figure out where the microwave pulse starts and test that it
           neither starts too early nor "collides" with its predecessor */

        start += cur_card->intr_delay + cur_card->next->intr_delay;

        if ( i == 0 && pulses[ 0 ]->pos <= start )
        {
            print( FATAL, "Microwave pulse #%ld starts too early.\n",
                   pulses[ 0 ]->num );
            THROW( EXCEPTION );
        }

        if (    i != 0
             && (    pulses[ i ]->pos <=
                           start - cur_card->prev->delay * rb_pulser_w.timebase
                  || pulses[ i ]->pos <=
                                pulses[ i - 1 ]->pos
                              + cur_card->prev->delay * rb_pulser_w.timebase ) )
        {
            print( FATAL, "Microwave pulse #%ld not far enough away from its "
                   "predecessor, pulse #%ld.\n", pulses[ i ]->num,
                   pulses[ i - 1 ]->num );
            THROW( EXCEPTION );
        }

        delta = pulses[ i ]->pos - start;
        dT = Ticks_rnd( delta / rb_pulser_w.timebase );
        shift = dT * rb_pulser_w.timebase - delta;
        if ( i > 0 )
            dT += cur_card->prev->delay;

        /* Make sure the delay settings for both cards aren't too long */

        if ( dT > MAX_TICKS )
        {
            if ( i == 0 )
                print( FATAL, "Microwave pulse #%ld starts too late.\n",
                       pulses[ 0 ]->num );
            else
                print( FATAL, "Microwave pulse #%ld is too far away from its "
                       "predecessor, pulse #%ld.\n", pulses[ i ]->num,
                       pulses[ i - 1 ]->num );
            THROW( EXCEPTION );
        }

        if ( pulses[ i ]->len > MAX_TICKS )
        {
            print( FATAL, "Microwave pulse #%ld is too long.\n" );
            THROW( EXCEPTION );
        }

        if ( fabs( shift ) > PRECISION * rb_pulser_w.timebase )
            print( SEVERE, "Position of microwave pulse #%ld not possible, "
                   "must shift it by %s.\n", pulses[ i ]->num,
                   rb_pulser_w_ptime( shift ) );

        cur_card->delay = dT;
        if ( i > 0 )
            dT -= cur_card->prev->delay;

        cur_card->is_active = SET;
        cur_card = cur_card->next;

        cur_card->delay = pulses[ i ]->len;
        cur_card->is_active = SET;
        cur_card = cur_card->next;

        start += ( dT + pulses[ i ]->len ) * rb_pulser_w.timebase;
    }

    /* Now in case that phase pulses are to be set we have to also set up
       the cards for the controlling the phase switch and again check the
       timings for the microwave pulses */

    if ( rb_pulser_w.needs_phases )
        rb_pulser_w_phase_channel_setup( );
}


/*------------------------------------------------------------------*
 * Function for setting up the cards that control the phase switch.
 * Since the settings depend on the positions and lengths of the
 * microwave pulses the cards for the microwave pulses must have
 * been set up before.
 *------------------------------------------------------------------*/

static void
rb_pulser_w_phase_channel_setup( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_MW;
    Rulbus_Delay_Card_T *mw_card = rb_pulser_w.delay_card + MW_DELAY_0;
    Rulbus_Delay_Card_T *p_card = rb_pulser_w.delay_card + PHASE_DELAY_0;
    Rulbus_Delay_Card_T *cur_card;
    Pulse_T **pulses = f->pulses;
    double mw_start,
           mw_end;
    Ticks dT;
    int i;


    for ( cur_card = p_card; cur_card != NULL; cur_card = cur_card->next )
    {
        cur_card->was_active = cur_card->is_active;
        cur_card->is_active = UNSET;
    }

    /* The cards for setting phases aren't needed if there's no active
       microwave pulse */

    if ( ! IS_ACTIVE( *pulses ) )
        return;

    /* Again we loop over the microwave pulses, setting the phase card for
       each of them */

    cur_card = p_card;
    mw_start = f->delay;
    mw_end = 0.0;

    for ( i = 0; i < f->num_pulses && IS_ACTIVE( pulses[ i ] ); i++ )
    {
        fsc2_assert(    cur_card != NULL
                     && mw_card != NULL
                     && mw_card->next != NULL );

        /* Calculate where the microwave pulse really starts and from this
           how long the phase cards delay must be */

        mw_start +=   mw_card->intr_delay + mw_card->next->intr_delay
                    + mw_card->delay * rb_pulser_w.timebase;

        dT = Ticks_rnd(   ( mw_start - cur_card->intr_delay - rb_pulser_w.psd )
                        / rb_pulser_w.timebase );

        /* The phase pulse must have a minimum length so that Leendert's
           "MoNos W-band magic box" can react to it */

        if ( dT * rb_pulser_w.timebase < MINIMUM_PHASE_PULSE_LENGTH )
        {
            print( FATAL, "Microwave pulse #%ld starts too early for phase "
                   "switching.\n", pulses[ i ]->num );
            THROW( EXCEPTION );
        }

        /* And, of course, we have to make sure that a phase pulse doesn't
           are to be longer than we can produce with a single delay card */

        if ( dT > MAX_TICKS )
        {
            print( FATAL, "Microwave pulse #%ld comes too late for phase "
                   "switching.\n", pulses[ i ]->num );
            THROW( EXCEPTION );
        }

        /* There also needs to be some amount of time following the previous
           microwave pulse before the phase can be switched */

        if (    i != 0
             && dT * rb_pulser_w.timebase + cur_card->intr_delay <
                                            mw_end + rb_pulser_w.grace_period )
        {
            print( FATAL, "Microwave pulse #%ld too near to its "
                   "predecessor #%ld to allow phase switching.\n",
                   pulses[ i ]->num, pulses[ i - 1 ]->num );
            THROW( EXCEPTION );
        }

        cur_card->delay = dT;
        cur_card->is_active = SET;
        cur_card = cur_card->next;

        mw_end = mw_start + mw_card->next->delay * rb_pulser_w.timebase;
        mw_start += mw_card->next->delay * rb_pulser_w.timebase;

        mw_card = mw_card->next->next;
    }

}


/*---------------------------------------------------------*
 * Function for setting up the card that creates the delay
 * before the RF pulse starts
 *---------------------------------------------------------*/

static void
rb_pulser_w_rf_channel_setup( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_RF;
    Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + RF_DELAY;
    Pulse_T *p = NULL,
            *p2 = NULL;
    Ticks dT;
    double start = 0.0,
           delta,
           shift;


    if ( ! f->is_used )
        return;

    card->was_active = card->is_active;
    card->is_active = UNSET;

    if ( f->num_active_pulses == 0 )
        return;

    if ( f->num_active_pulses == 1 )
    {
        int i;

        for ( i = 0; i < f->num_active_pulses; i ++ )
            if ( IS_ACTIVE( f->pulses[ i ] ) )
            {
                p = f->pulses[ i ];
                break;
            }

        start = SYNTHESIZER_MIN_PULSE_DELAY;
    }
    else
    {
        p  = f->pulses[ 0 ];
        p2 = f->pulses[ 1 ];

        if ( p->len != p2->len )
        {
            print( SEVERE, "Lengths of RF pulses #%ld and #%ld differ which "
                   "isn't possible, adjusting length of pulse #%ld to %s.\n",
                   p->num, p2->num, p2->num,
                   rb_pulser_w_ptime( p2->len * rb_pulser_w.timebase ) );
            p2->len = p->len;
        }
    }

    start +=   rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay
             + SYNTHESIZER_INTRINSIC_DELAY + f->delay;

    if ( p->pos <= start + 0.5 * rb_pulser_w.timebase )
    {
        print( FATAL, "RF pulse #%ld starts too early.\n", p->num );
        THROW( EXCEPTION );
    }

    delta = p->pos - start;
    dT = Ticks_rnd( delta / rb_pulser_w.timebase );
    shift = dT * rb_pulser_w.timebase - delta;

    if ( dT > MAX_TICKS )
    {
        print( FATAL, "RF pulse #%ld starts too late.\n", p->num );
        THROW( EXCEPTION );
    }

    if ( fabs( shift ) > PRECISION * rb_pulser_w.timebase )
        print( SEVERE, "Position of RF pulse #%ld not possible, must "
               "shift it by %s.\n", p->num, rb_pulser_w_ptime( shift ) );

    card->delay = dT;
    card->is_active = SET;

    /* Store the length of the pulse itself separately. there's no card for
       this pulse, it gets set by the synthesizer directly */


    f->last_pulse_len = p->len * rb_pulser_w.timebase;

    /* If there are two RF pulses check that the separation is large enough */

    if (    f->num_active_pulses > 1
         && p2->pos <   p->pos + p->len * rb_pulser_w.timebase
                      + SYNTHESIZER_MIN_PULSE_SEPARATION )
    {
        print( FATAL, "Position of RF pulse #%ld too near to pulse #%ld, "
               "minimum separation between RF pulses is %s.\n", p2->num, p->num,
               rb_pulser_w_ptime( SYNTHESIZER_MIN_PULSE_SEPARATION ) );
        THROW( EXCEPTION );
    }
}


/*---------------------------------------------------------------*
 * Function for setting up the cards that create the laser pulse
 *---------------------------------------------------------------*/

static void
rb_pulser_w_laser_channel_setup( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_LASER;
    Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + LASER_DELAY_0;
    Pulse_T *p;
    Ticks dT;
    double start, delta, shift;


    if ( ! f->is_used )
        return;

    card->was_active = card->is_active;
    card->is_active = UNSET;

    card->next->was_active = card->is_active;
    card->next->is_active = UNSET;

    p = *f->pulses;

    if ( ! IS_ACTIVE( p ) )
        return;

    /* Make sure the pulse doesn't start earlier than possible (due to the
       intrinsic delays of the cards) */

    start =   rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay
            + card->intr_delay + card->next->intr_delay + f->delay;

    if ( p->pos <= start )
    {
        print( FATAL, "Laser pulse #%ld starts too early.\n", p->num );
        THROW( EXCEPTION );
    }

    /* Calculate the delay for the first card and make sure it's a multiple
       of the pulsers timebase */

    delta = p->pos - start;
    dT = Ticks_rnd( delta / rb_pulser_w.timebase );
    shift = dT * rb_pulser_w.timebase - delta;

    if ( dT > MAX_TICKS )
    {
        print( FATAL, "Laser pulse #%ld starts too late.\n", p->num );
        THROW( EXCEPTION );
    }

    if ( p->len > MAX_TICKS )
    {
        print( FATAL, "Laser pulse #%ld is too long.\n", p->num );
        THROW( EXCEPTION );
    }

    if ( fabs( shift ) > PRECISION * rb_pulser_w.timebase )
        print( SEVERE, "Position of laser pulse #%ld not possible, must shift "
               "it by %s.\n", rb_pulser_w_ptime( shift ) );

    card->delay = dT;
    card->is_active = SET;

    card->next->delay = p->len;
    card->next->is_active = SET;
}


/*---------------------------------------------------------------------*
 * Function for setting up the card creating the detection pulse (it's
 * just a single card and the end of the pulse it emits is used as the
 * detection pulse).
 *---------------------------------------------------------------------*/

static void
rb_pulser_w_detection_channel_setup( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_DET;
    Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + DET_DELAY;
    Pulse_T *p;
    Ticks dT;
    double start, delta, shift;


    if ( ! f->is_used )
        return;

    card->was_active = card->is_active;
    card->is_active = UNSET;

    p = *f->pulses;

    if ( ! IS_ACTIVE( p ) )
        return;

    start =   rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay + card->intr_delay
            + f->delay;

    if ( p->pos <= start )
    {
        print( FATAL, "Detection pulse #%ld starts too early.\n", p->num );
        THROW( EXCEPTION );
    }

    delta = p->pos - start;
    dT = Ticks_rnd( delta / rb_pulser_w.timebase );
    shift = dT * rb_pulser_w.timebase - delta;

    if ( dT > MAX_TICKS )
    {
        print( FATAL, "Detection pulse #%ld starts too late.\n", p->num );
        THROW( EXCEPTION );
    }

    if ( fabs( shift ) > PRECISION * rb_pulser_w.timebase )
        print( SEVERE, "Position of detection pulse #%ld not possible, must "
               "shift it by %s.\n", p->num, rb_pulser_w_ptime( shift ) );

    card->delay = dT;
    card->is_active = SET;

    f->last_pulse_len = p->len * rb_pulser_w.timebase;
}


/*------------------------------------------------------------------*
 * Function for setting up the card creating the defense pulse when
 * the defense pulse is set manually, i.e. by the user. We only need
 * to care about its length since it always starts as early as
 * possible.
 *------------------------------------------------------------------*/

static void
rb_pulser_w_defense_channel_setup( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_DEFENSE;
    Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + DEFENSE_DELAY;
    Pulse_T *p;


    if ( ! f->is_used )
        return;

    card->was_active = card->is_active;
    card->is_active = UNSET;

    p = *f->pulses;

    if ( ! IS_ACTIVE( p ) )
        return;

    if ( p->len > MAX_TICKS )
    {
        print( FATAL, "Defense pulse #%ld is too long.\n", p->num );
        THROW( EXCEPTION );
    }

    card->delay = p->len;
    card->is_active = SET;

    f->last_pulse_len = p->len * rb_pulser_w.timebase;
}


/*------------------------------------------------------------------*
 * Function for setting up the card creating the defense pulse when
 * it get created automatically. The defense pulse must start at
 * least as early as the first microwave pulse and must last some
 * time longer than the end of the last microwave pulse. To be able
 * to work with the correct data this function must only be called
 * after the microwave pulses have been set up.
 *------------------------------------------------------------------*/

static void
rb_pulser_w_auto_defense_channel_setup( void )
{
    Function_T *mw = rb_pulser_w.function + PULSER_CHANNEL_MW;
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_DEFENSE;
    Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + DEFENSE_DELAY;
    Rulbus_Delay_Card_T *mw_card = rb_pulser_w.delay_card + MW_DELAY_0;
    double mw_start, mw_end, start, end;
    Ticks dT;
    int i;


    card->was_active = card->is_active;
    card->is_active = UNSET;

    /* No defense pulse needs to be set if there are no microwave pulses */

    if ( ! mw->is_used || mw->num_active_pulses == 0 )
        return;

    /* Determine the position of the first microwave pulse and check if the
       defense pulse can at least start at the same time or earlier */

    start    =   rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay
               + card->intr_delay;

    mw_start =   rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay
               + mw_card->intr_delay
               + mw_card->delay * rb_pulser_w.timebase
               + mw_card->next->intr_delay;

    if ( start > mw_start )
    {
        print( FATAL, "First microwave pulse #%ld starts that early that the "
               "automatically generated defense pulse can't start at least "
               "at the same time.\n", mw->pulses[ 0 ]->num );
        THROW( EXCEPTION );
    }

    /* The position of the defense pulse is actually never used fo setting
       the pulse, it's only used for visualisation of the pulses via a
       dump (to file or the fsc2_pulse utility) */

    f->pulses[ 0 ]->pos = start;

    /* Now figure out where the end of the last microwave pulse is and from
       that at what time the defense pulse can end */

    mw_end = rb_pulser_w.delay_card[ ERT_DELAY ].intr_delay;

    for ( i = 0; i < mw->num_active_pulses; i++ )
    {
        /* Please note: the length of the inter-pulse delay already
           contains the length of the previous microwave pulse. */

        mw_end +=   mw_card->intr_delay
                  + mw_card->delay * rb_pulser_w.timebase;
        mw_card = mw_card->next;

        mw_end += mw_card->intr_delay;
        if ( i == mw->num_active_pulses - 1 )
            mw_end += mw_card->delay * rb_pulser_w.timebase;
        mw_card = mw_card->next;
    }

    end = mw_end + rb_pulser_w.pulse_2_defense;

    dT = Ticks_ceil( 0.1 *
                     Ticks_rnd( 10 * ( end - start ) / rb_pulser_w.timebase ) );

    if ( dT > MAX_TICKS )
    {
        print( FATAL, "Automatically generated defense pulse becomes too "
               "long.\n" );
        THROW( EXCEPTION );
    }

    card->delay = dT;
    card->is_active = SET;

    f->last_pulse_len = dT * rb_pulser_w.timebase;
}


/*-------------------------------------------------------------------------*
 * Function is called in the initialization of the pulser to reset all the
 * variables describing the state of the pulser to their initial values.
 *-------------------------------------------------------------------------*/

void
rb_pulser_w_full_reset( void )
{
    Pulse_T *p = rb_pulser_w.pulses;
    Function_T *f;
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

    /* Make sure all cards associated with functions are assumed to be
       inactive */

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_w.function + i;

        if ( ! f->is_used )
            continue;

        for ( card = f->delay_card; card != NULL; card = card->next )
        {
            card->is_active = card->was_active = UNSET;
            card->delay = card->old_delay = 0;
        }
    }

    /* Reset to the first phase */

    rb_pulser_w.cur_phase = 0;
}


/*------------------------------------------------------------------*
 * Functions determines the length of the pulse sequence and throws
 * an exception if that's longer than possible.
 *------------------------------------------------------------------*/

static void
rb_pulser_w_seq_length_check( void )
{
    int i;
    Function_T *f;
    Rulbus_Delay_Card_T *card;
    double max_seq_len = 0.0, seq_len;


    if ( rb_pulser_w.trig_in_mode == EXTERNAL )
        return;

    /* Determine length of sequence lengths of the individual functions */

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_w.function + i;

        /* Nothing to be done for unused functions */

        if ( ! f->is_used )
            continue;

        seq_len = 0.0;
        for ( card = f->delay_card; card != NULL && card->is_active;
              card = card->next )
            seq_len += card->delay * rb_pulser_w.timebase + card->intr_delay;

        max_seq_len = d_max( max_seq_len, seq_len );
    }

    /* Don't forget the cards used for phase switching */

    if ( rb_pulser_w.needs_phases )
        for ( card = rb_pulser_w.delay_card + PHASE_DELAY_0;
              card != NULL && card->is_active; card = card->next )
            max_seq_len =
                d_max( max_seq_len,
                       card->delay * rb_pulser_w.timebase + card->intr_delay );

    if ( max_seq_len > rb_pulser_w.rep_time )
    {
        print( FATAL, "Pulse sequence is longer than the experiment "
               "repetition time.\n" );
        THROW( EXCEPTION );
    }
}


/*-------------------------------------------------------------------*
 * Changes the pulse pattern in all channels so that the data in the
 * pulser get in sync with the the internal representation.
 * 'test_only' is set during the test run.
 *-------------------------------------------------------------------*/

static void
rb_pulser_w_commit( bool test_only )
{
    Rulbus_Delay_Card_T *card;
    Function_T *f;
    size_t i;


    /* During the test run only write out information about the pulse
       settings (if the user asked for it) */

    if ( test_only )
    {
        if ( rb_pulser_w.dump_file != NULL )
            rb_pulser_w_write_pulses( rb_pulser_w.dump_file );
        if ( rb_pulser_w.show_file != NULL )
            rb_pulser_w_write_pulses( rb_pulser_w.show_file );

        return;
    }

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_w.function + i;

        if ( ! f->is_used )
            continue;

        for ( card = f->delay_card; card != NULL; card = card->next )
        {
            if ( card->was_active && ! card->is_active )
            {
                rb_pulser_w_delay_card_state( card, STOP );
                card->delay = card->old_delay = 0;
                continue;
            }

            /* If the card as just become active set it to active state
               (thereby also setting the delay), otherwise just set the
               new delay if that changed. */

            if ( ! card->was_active && card->is_active )
                rb_pulser_w_delay_card_state( card, START );
            else if ( card->old_delay != card->delay )
                rb_pulser_w_delay_card_delay( card, card->delay, UNSET );

            card->old_delay = card->delay;
        }
    }

    if ( rb_pulser_w.needs_phases )
        rb_pulser_w_set_phases( );
}


/*-----------------------------------------------------------------*
 * Function to set up the cards responsible for the phase settings
 *-----------------------------------------------------------------*/

static void
rb_pulser_w_set_phases( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_MW;
    Rulbus_Delay_Card_T *card = rb_pulser_w.delay_card + PHASE_DELAY_0;
    Pulse_T **pulses = f->pulses;
    int i;


    if ( ! rb_pulser_w.needs_phases )
        return;

    for ( i = 0; i < f->num_pulses; card = card->next, i++ )
    {
        /* If the card isn't needed switch set it for end pulses for the +X
           phase and keep it from emitting a pulse that would trigger
           Leendert's "magic box" */

        if ( card->was_active && ! card->is_active )
        {
            rb_pulser_w_set_phase( card, PHASE_PLUS_X );
            rb_pulser_w_delay_card_delay( card, 0, UNSET );
            card->delay = card->old_delay = 0;
            continue;
        }

        /* Set the card up to emit the correct set of end pulses for the phase
           needed for the pulse */

        if ( pulses[ i ]->pc == NULL )
            rb_pulser_w_set_phase( card, PHASE_PLUS_X );
        else
            rb_pulser_w_set_phase( card,
                          pulses[ i ]->pc->sequence[ rb_pulser_w.cur_phase ] );

        /* If the pulse length changed tell the card about it */

        if ( card->delay != card->old_delay )
        {
            rb_pulser_w_delay_card_delay( card, card->delay, UNSET );
            card->old_delay = card->delay;
        }
    }
}


/*-------------------------------------------------------------*
 * Function for setting the RF pulse length at the synthesizer
 *-------------------------------------------------------------*/

static void
rb_pulser_w_rf_pulse( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_RF;
    Pulse_T *p  = NULL,
            *p2 = NULL;
    Var_T *func_ptr;
    int acc;


    if ( ! f->is_used )
        return;

    if ( f->num_active_pulses == 1 )
    {
        int i;

        for ( i = 0; i < f->num_active_pulses; i ++ )
            if ( IS_ACTIVE( f->pulses[ i ] ) )
            {
                p = f->pulses[ i ];
                break;
            }
    }
    else
    {
        p  = f->pulses[ 0 ];
        p2 = f->pulses[ 1 ];
    }

    /* Set the length of the first RF pulse (all others have the same
       length) */

    if ( p->is_active )
    {
#if ! defined RB_PULSER_W_TEST
        if ( ( func_ptr = func_get( rb_pulser_w.synth_pulse_width, &acc ) )
                                                                      == NULL )
        {
            print( FATAL, "Function for setting synthesizer pulse length is "
                   "not available.\n" );
            THROW( EXCEPTION );
        }

        vars_push( FLOAT_VAR, f->last_pulse_len );
        vars_pop( func_call( func_ptr ) );

#else   /* in test mode */
        if ( FSC2_MODE == EXPERIMENT )
            fprintf( stderr, "synthesizer_pulse_width( %lf ns )\n",
                     1.0e9 * f->last_pulse_len );
#endif
    }

    /* Switch synthesizer output on if there were no active pulses before but
       now are and off if there were active pulse but now there are none */

    if (    ( f->old_num_active_pulses <= 0 && f->num_active_pulses > 0 )
         || ( f->old_num_active_pulses > 0 && f->num_active_pulses == 0 ) )
    {
#if ! defined RB_PULSER_W_TEST
        if ( ( func_ptr = func_get( rb_pulser_w.synth_state, &acc ) ) == NULL )
        {
            print( FATAL, "Function for setting synthesizer output state is "
                   "not available.\n" );
            THROW( EXCEPTION );
        }

        if ( f->num_active_pulses > 1 )
            vars_push( STR_VAR, "ON" );
        else
            vars_push( STR_VAR, "OFF" );

        vars_pop( func_call( func_ptr ) );

#else   /* in test mode */
        fprintf( stderr, "synthesizer_pulse_state( \"%s\" )\n",
                 f->num_active_pulses > 1 ? "ON" : "OFF" );
#endif

        p->was_active = p->is_active;
    }

    /* Switch between double and single mode if the number o active pulses
       changed */

    if ( f->old_num_active_pulses != f->num_active_pulses )
    {
#if ! defined RB_PULSER_W_TEST
        if ( ( func_ptr = func_get( rb_pulser_w.synth_double_mode, &acc ) )
                                                                       == NULL )
        {
            print( FATAL, "Synthesizer function for switching between single "
                   "and double pulse mode is not available.\n" );
            THROW( EXCEPTION );
        }

        if ( f->num_active_pulses > 1 )
            vars_push( STR_VAR, "ON" );
        else
            vars_push( STR_VAR, "OFF" );

        vars_pop( func_call( func_ptr ) );
#else
        fprintf( stderr, "synthesizer_double_pulse_mode( \"%s\" )\n",
                 f->num_active_pulses > 1 ? "ON" : "OFF" );
#endif
    }

    /* If there are two RF pulses set the delay between them */

    if (    f->num_active_pulses > 1
		 && ( f->old_delay < 0.0
			  || lrnd( 1.0e9 * f->old_delay ) !=
                        lrnd( 1.0e9 * (   p2->pos - p->pos
										- p->len * rb_pulser_w.timebase ) ) ) )
    {
#if ! defined RB_PULSER_W_TEST
        if ( ( func_ptr = func_get( rb_pulser_w.synth_double_delay, &acc ) )
                                                                      == NULL )
        {
            print( FATAL, "Synthesizer function for setting delay between two "
                   "pulses is not available.\n" );
            THROW( EXCEPTION );
        }

        f->old_delay = p2->pos - p->pos - p->len * rb_pulser_w.timebase;
        vars_push( FLOAT_VAR, f->old_delay );
        vars_pop( func_call( func_ptr ) );
#else
        fprintf( stderr, "synthesizer_double_pulse_delay( \"%s\" )\n",
                 rb_pulser_w_ptime(   p2->pos - p->pos
                                    - p->len * rb_pulser_w.timebase ) );
#endif
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
