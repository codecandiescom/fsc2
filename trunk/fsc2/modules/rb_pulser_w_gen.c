/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


/* Array of timebases the adjustable clock (RB8515) can be set to and
   of values to be send to the clocks driver to set the timebase */

static double timebases[ ] = { 1.0e-8, 1.0e-7, 1.0e-6, 1.0e-5,
                               1.0e-4, 1.0e-3, 1.0e-2 };
static int num_timebases = NUM_ELEMS( timebases );

static int tb_index[ ] = { RULBUS_RB8515_CLOCK_FREQ_100MHz,
                           RULBUS_RB8515_CLOCK_FREQ_10MHz,
                           RULBUS_RB8515_CLOCK_FREQ_1MHz,
                           RULBUS_RB8515_CLOCK_FREQ_100kHz,
                           RULBUS_RB8515_CLOCK_FREQ_10kHz,
                           RULBUS_RB8515_CLOCK_FREQ_1kHz,
                           RULBUS_RB8515_CLOCK_FREQ_100Hz };


/*-----------------------------------------------------------------*
 * Function is called via the TIMEBASE command to set the timebase
 * used with the pulser - got to be called first because nearly
 * all other functions depend on the timebase setting (unless the
 * module is compiled with FIXED_TIMEBASE being defined in which
 * case this function never will be called).
 *-----------------------------------------------------------------*/

bool
rb_pulser_w_store_timebase( double timebase )
{
#ifndef FIXED_TIMEBASE
    int i;


    if ( rb_pulser_w.is_timebase )
    {
        print( FATAL, "Time base has already been set to %s.\n",
               rb_pulser_w_ptime( rb_pulser_w.timebase ) );
        THROW( EXCEPTION );
    }

    if ( timebase <= 0.0 )
    {
        print( FATAL, "Invalid zero or negative timebase.\n" );
        THROW( EXCEPTION );
    }

    for ( i = 0; i < num_timebases; i++ )
        if ( fabs( timebase - timebases[ i ] ) <= PRECISION * timebase )
            break;

    if ( i == num_timebases )
    {
        print( FATAL, "Timebase of %s can't be realized with this pulser.\n",
               rb_pulser_w_ptime( timebase ) );
        THROW( EXCEPTION );
    }

    rb_pulser_w.timebase = timebases[ i ];
    rb_pulser_w.tb_index = tb_index[ i ];
    rb_pulser_w.is_timebase = SET;

    return OK;
#else
    if ( fabs( timebase - rb_pulser_w.timebase ) >
                                             PRECISION * rb_pulser_w.timebase )
    {
        print( FATAL, "Timebase is in the current configuration fixed to "
               "%s.\n", rb_pulser_w_ptime( rb_pulser_w.timebase ) );
        THROW( EXCEPTION );
    }

    return OK;
#endif
}


/*---------------------------------------------------------------*
 * Function for setting a delay for a function - negative delays
 * are only possible for INTERNAL trigger mode!
 *---------------------------------------------------------------*/

bool
rb_pulser_w_set_function_delay( int    function,
                                double delay )
{
    Function_T *f = rb_pulser_w.function + function;
    int i;


    if ( f->is_delay )
    {
        print( FATAL, "Delay for function '%s' has already been set.\n",
               f->name );
        THROW( EXCEPTION );
    }

    if ( delay == 0.0 )
        return OK;

    /* Check that the delay value is reasonable */

    if ( delay < 0.0 )
    {
        if (    rb_pulser_w.is_trig_in_mode
             && rb_pulser_w.trig_in_mode == EXTERNAL )
        {
            print( FATAL, "Negative delays are not possible in EXTERNAL "
                   "trigger mode.\n" );
            THROW( EXCEPTION );
        }

        rb_pulser_w.is_neg_delay = SET;

        if ( delay < rb_pulser_w.neg_delay )
        {
            for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
                if ( i != function )
                    rb_pulser_w.function[ i ].delay -=
                                                 rb_pulser_w.neg_delay + delay;

            rb_pulser_w.neg_delay = - delay;
            f->delay = 0;
        }
        else
            f->delay += rb_pulser_w.neg_delay + delay;
    }
    else
        f->delay += delay;

    f->is_delay = SET;
    f->is_declared = SET;

    return OK;
}


/*--------------------------------------------------------------------*
 * Function for setting the trigger mode, either INTERNAL or EXTERNAL
 *--------------------------------------------------------------------*/

bool
rb_pulser_w_set_trigger_mode( int mode )
{
    fsc2_assert( mode == INTERNAL || mode == EXTERNAL );

    if ( rb_pulser_w.is_trig_in_mode && rb_pulser_w.trig_in_mode != mode )
    {
        print( FATAL, "Trigger mode has already been set to %sTERNAL.\n",
               rb_pulser_w.trig_in_mode == INTERNAL ? "IN" : "EX" );
        THROW( EXCEPTION );
    }

    if ( mode == EXTERNAL && rb_pulser_w.is_neg_delay )
    {
        print( FATAL, "EXTERNAL trigger mode and using negative delays "
               "for functions isn't possible.\n" );
        THROW( EXCEPTION );
    }

    rb_pulser_w.trig_in_mode = mode;
    rb_pulser_w.is_trig_in_mode = SET;

    return OK;
}


/*-------------------------------------------------------*
 * Function for setting the trigger in slope when pulser
 * runs in external trigger mode
 *-----------------------------------.-------------------*/

bool
rb_pulser_w_set_trig_in_slope( int slope )
{
    fsc2_assert( slope == POSITIVE || slope == NEGATIVE );

    if ( rb_pulser_w.is_trig_in_slope && rb_pulser_w.trig_in_slope != slope )
    {
        print( FATAL, "A different trigger slope (%s) has already been set.\n",
               slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
        THROW( EXCEPTION );
    }

    if ( rb_pulser_w.is_trig_in_mode && rb_pulser_w.trig_in_mode == INTERNAL )
    {
        print( SEVERE, "Setting a trigger slope is useless in INTERNAL "
               "trigger mode.\n" );
        return FAIL;
    }

    if (    rb_pulser_w.is_neg_delay
         && ! (    rb_pulser_w.is_trig_in_mode
                && rb_pulser_w.trig_in_mode == INTERNAL ) )
    {
        print( FATAL, "Setting a trigger slope (requiring EXTERNAL "
               "trigger mode) and using negative delays for functions is "
               "not possible.\n" );
        THROW( EXCEPTION );
    }

    rb_pulser_w.trig_in_mode = EXTERNAL;
    rb_pulser_w.trig_in_slope = slope;
    rb_pulser_w.is_trig_in_slope = SET;

    return OK;
}


/*------------------------------------------------------------------*
 * Function for setting the repetition time for the pulse sequences
 * when pulser runs in internal trigger mode
 *------------------------------------------------------------------*/

bool
rb_pulser_w_set_repeat_time( double rep_time )
{
    int i;


    if ( rb_pulser_w.is_trig_in_mode && rb_pulser_w.trig_in_mode != INTERNAL )
    {
        print( FATAL, "Setting repeat time/frequency is useless for EXTERNAL "
               "trigger mode.\n" );
        return FAIL;
    }

    /* Complain if a different repetition time has already been set */

    if ( rb_pulser_w.is_rep_time )
    {
        print( FATAL, "Repeat time/frequency has already been set.\n" );
        THROW( EXCEPTION );
    }

    if ( rep_time <= 0.0 )
    {
        print( FATAL, "Invalid zero or negative repeat time/frequency.\n" );
        THROW( EXCEPTION );
    }

    /* Check how we can realize the repeat time with the fastest possible
       clock speed */

    for ( i = 0; i < num_timebases; i++ )
        if ( rep_time <= RULBUS_RB8514_DELAY_CARD_MAX * timebases[ i ] )
            break;

    if ( i == num_timebases )
    {
        print( FATAL, "Repeat time/frequency is too long.\n" );
        THROW( EXCEPTION );
    }

    rb_pulser_w.rep_time_index = tb_index[ i ];
    rb_pulser_w.rep_time_ticks = Ticks_rnd( rep_time / timebases[ i ] );
    rb_pulser_w.rep_time = rb_pulser_w.rep_time_ticks * timebases[ i ];
    rb_pulser_w.is_rep_time = SET;

    return OK;
}


/*------------------------------------------------------------------*
 * Function for setting the minimum delay between switching a phase
 * and the start of a pulse.
 *------------------------------------------------------------------*/

bool
rb_pulser_w_set_phase_switch_delay( int    dummy  UNUSED_ARG,
                                    double del_time )
{
    if ( del_time < 0.0 )
    {
        print( FATAL, "Invalid negative value for phase switch delay: "
               "%s.\n", rb_pulser_w_ptime( del_time ) );
        THROW( EXCEPTION );
    }

    if ( rb_pulser_w.is_psd )
    {
        print( FATAL, "Phase switch delay for has already been set.\n" );
        THROW( EXCEPTION );
    }

    rb_pulser_w.is_psd = SET;
    rb_pulser_w.psd = del_time;

    return OK;
}


/*---------------------------------------------------*
 * Function for setting the minimum time a phase has
 * to be kept after te end of a pulse
 *---------------------------------------------------*/

bool
rb_pulser_w_set_grace_period( double gp_time )
{
    if ( gp_time < 0 )
    {
        print( FATAL, "Invalid negative value for grace period: %s.\n",
               rb_pulser_w_ptime( gp_time ) );
        THROW( EXCEPTION );
    }

    if ( rb_pulser_w.is_grace_period )
    {
        print( FATAL, "Grace period has already been set.\n" );
        THROW( EXCEPTION );
    }

    rb_pulser_w.is_grace_period = SET;
    rb_pulser_w.grace_period = gp_time;

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
