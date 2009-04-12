/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#include "hfs9000.h"


/*------------------------------------------------------------------*
 * Function is called via the TIMEBASE command to set the timebase
 * used with the pulser - got to be called first because all nearly
 * all other functions depend on the timebase setting !
 *------------------------------------------------------------------*/

bool
hfs9000_store_timebase( double timebase )
{
    if ( timebase < MIN_TIMEBASE || timebase > MAX_TIMEBASE )
    {
        print( FATAL, "Invalid time base of %s, valid range is "
               "%s to %s.\n", hfs9000_ptime( timebase ),
               hfs9000_ptime( MIN_TIMEBASE ), hfs9000_ptime( MAX_TIMEBASE ) );
        THROW( EXCEPTION );
    }

    hfs9000.is_timebase = SET;
    hfs9000.timebase = timebase;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
hfs9000_assign_channel_to_function( int  function,
                                    long channel )
{
    Function_T *f = hfs9000.function + function;
    Channel_T *c = hfs9000.channel + channel;


    if ( channel < HFS9000_TRIG_OUT || channel > MAX_CHANNEL )
    {
        print( FATAL, "Invalid channel, valid are %s[1-4] and TRIG_OUT.\n",
               NUM_CHANNEL_CARDS == 1 ? "A" :
               ( NUM_CHANNEL_CARDS == 2 ? "[AB]" : "[A-C]" ) );
        THROW( EXCEPTION );
    }

    if ( channel == HFS9000_TRIG_OUT )
    {
        if ( f->is_inverted )
        {
            print( FATAL, "Function '%s' has been set to use inverted logic. "
                   "This can't be done with TRIG_OUT.\n",
                   Function_Names[ function ] );
            THROW( EXCEPTION );
        }

        if ( f->is_high_level || f->is_low_level )
        {
            print( FATAL, "For function '%s', associated with TRIG_OUT, no "
                   "voltage levels can be set.\n",
                   Function_Names[ function ] );
            THROW( EXCEPTION );
        }
    }

    if ( c->function != NULL )
    {
        if ( c->function->self == function )
        {
            if ( channel == HFS9000_TRIG_OUT )
                print( SEVERE, "TRIG_OUT is assigned more than once to "
                       "function '%s'.\n",
                       Function_Names[ c->function->self ] );
            else
                print( SEVERE, "Channel %c%c is assigned more than once to "
                       "function '%s'.\n", CHANNEL_LETTER( channel ),
                       CHANNEL_NUMBER( channel ),
                       Function_Names[ c->function->self ] );
            return FAIL;
        }

        if ( channel == HFS9000_TRIG_OUT )
            print( FATAL, "TRIG_OUT is already used for function '%s'.\n",
                   Function_Names[ c->function->self ] );
        else
            print( FATAL, "Channel %c%c is already used for function '%s'.\n",
                   CHANNEL_LETTER( channel ), CHANNEL_NUMBER( channel ),
                   Function_Names[ c->function->self ] );
        THROW( EXCEPTION );
    }

    f->is_used = SET;
    f->channel = c;

    c->function = f;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
hfs9000_invert_function( int function )
{
    if (    hfs9000.function[ function ].channel != NULL
         && hfs9000.function[ function ].channel->self == HFS9000_TRIG_OUT )
    {
        print( FATAL, "Polarity of function '%s' associated with TRIG_OUT "
               "can't be inverted.\n", Function_Names[ function ] );
        THROW( EXCEPTION );
    }

    hfs9000.function[ function ].is_inverted = SET;
    hfs9000.function[ function ].is_used = SET;
    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
hfs9000_set_function_delay( int    function,
                            double delay )
{
    Ticks Delay = hfs9000_double2ticks( delay );
    int i;


    if ( hfs9000.function[ function ].is_delay )
    {
        print( FATAL, "Delay for function '%s' has already been set.\n",
               Function_Names[ function ] );
        THROW( EXCEPTION );
    }

    /* Check that the delay value is reasonable */

    if ( Delay < 0 )
    {
        if (    ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == EXTERNAL )
             || hfs9000.is_trig_in_slope
             || hfs9000.is_trig_in_level )
        {
            print( FATAL, "Negative delays are invalid in EXTERNAL trigger "
                   "mode.\n" );
            THROW( EXCEPTION );
        }

        hfs9000.is_neg_delay = SET;

        if ( Delay < - hfs9000.neg_delay )
        {
            for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
                hfs9000.function[ i ].delay -= hfs9000.neg_delay - Delay;

            hfs9000.neg_delay = - Delay;
            hfs9000.function[ function ].delay = 0;
        }
        else
            hfs9000.function[ function ].delay += Delay;
    }
    else
        hfs9000.function[ function ].delay += Delay;

    hfs9000.function[ function ].is_used = SET;
    hfs9000.function[ function ].is_delay = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
hfs9000_set_function_high_level( int    function,
                                 double voltage )
{
    Function_T *f = hfs9000.function + function;
    long v;


    if ( f->is_high_level )
    {
        print( FATAL, "High level for function '%s' has already been set.\n",
               Function_Names[ function ] );
        THROW( EXCEPTION );
    }

    if ( f->channel != NULL && f->channel->self == HFS9000_TRIG_OUT )
    {
        print( FATAL, "Function '%s' is associated with TRIG_OUT which "
               "doesn't allow setting of levels.\n",
               Function_Names[ function ] );
        THROW( EXCEPTION );
    }

    v = lrnd( voltage / VOLTAGE_RESOLUTION );

    if (    v < lrnd( MIN_POD_HIGH_VOLTAGE / VOLTAGE_RESOLUTION )
         || v > lrnd( MAX_POD_HIGH_VOLTAGE / VOLTAGE_RESOLUTION ) )
    {
        print( FATAL, "Invalid high level of %g V for function '%s', valid "
               "range is %g V to %g V.\n", voltage, Function_Names[ function ],
               MIN_POD_HIGH_VOLTAGE, MAX_POD_HIGH_VOLTAGE );
        THROW( EXCEPTION );
    }

    voltage = VOLTAGE_RESOLUTION * v;

    if ( f->is_low_level )
        hfs9000_check_pod_level_diff( voltage, f->low_level );

    f->high_level = voltage;
    f->is_high_level = SET;
    f->is_used = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
hfs9000_set_function_low_level( int    function,
                                double voltage )
{
    Function_T *f = hfs9000.function + function;
    long v;


    if ( f->is_low_level )
    {
        print( FATAL, "Low level for function '%s' has already been set.\n",
               Function_Names[ function ] );
        THROW( EXCEPTION );
    }

    if ( f->channel != NULL && f->channel->self == HFS9000_TRIG_OUT )
    {
        print( FATAL, "Function '%s' is associated with TRIG_OUT "
               "which doesn't allow setting of levels.\n",
               Function_Names[ function ] );
        THROW( EXCEPTION );
    }

    v = lrnd( voltage / VOLTAGE_RESOLUTION );

    if (    v < lrnd( MIN_POD_LOW_VOLTAGE / VOLTAGE_RESOLUTION )
         || v > lrnd( MAX_POD_LOW_VOLTAGE / VOLTAGE_RESOLUTION ) )
    {
        print( FATAL, "Invalid low level of %g V for function '%s', valid "
               "range is %g V to %g V.\n", voltage, Function_Names[ function ],
               MIN_POD_LOW_VOLTAGE, MAX_POD_LOW_VOLTAGE );
        THROW( EXCEPTION );
    }

    voltage = VOLTAGE_RESOLUTION * v;

    if ( f->is_high_level )
        hfs9000_check_pod_level_diff( f->high_level, voltage );

    f->low_level = voltage;
    f->is_low_level = SET;
    f->is_used = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
hfs9000_set_trigger_mode( int mode )
{
    fsc2_assert( mode == INTERNAL || mode == EXTERNAL );

    if ( hfs9000.is_trig_in_mode )
    {
        print( FATAL, "Trigger mode has already been set.\n" );
        THROW( EXCEPTION );
    }

    if ( mode == INTERNAL )
    {
        if ( hfs9000.is_trig_in_slope )
        {
            print( FATAL, "INTERNAL trigger mode and setting a trigger slope "
                   "isn't possible.\n" );
            THROW( EXCEPTION );
        }

        if ( hfs9000.is_trig_in_level )
        {
            print( FATAL, "INTERNAL trigger mode and setting a trigger level "
                   "isn't possible.\n" );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if ( hfs9000.is_neg_delay )
        {
            print( FATAL, "EXTERNAL trigger mode and using negative delays "
                   "for functions isn't possible.\n" );
            THROW( EXCEPTION );
        }
    }

    hfs9000.trig_in_mode = mode;
    hfs9000.is_trig_in_mode = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
hfs9000_set_trig_in_level( double voltage )
{
    long v;


    if ( hfs9000.is_trig_in_level && hfs9000.trig_in_level != voltage )
    {
        print( FATAL, "A different level of %g V for the trigger has already "
               "been set.\n", hfs9000.trig_in_level );
        THROW( EXCEPTION );
    }

    if ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL )
    {
        print( SEVERE, "Setting a trigger level is useless in INTERNAL "
               "trigger mode.\n" );
        return FAIL;
    }

    if (    hfs9000.is_neg_delay
         && ! ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL ) )
    {
        print( FATAL, "Setting a trigger level (thus implicitly selecting "
               "EXTERNAL trigger mode) and using negative delays for "
               "functions is incompatible.\n" );
        THROW( EXCEPTION );
    }

    v = lrnd( voltage / VOLTAGE_RESOLUTION );

    if (    v > lrnd( MAX_TRIG_IN_LEVEL / VOLTAGE_RESOLUTION )
         || v < lrnd( MIN_TRIG_IN_LEVEL / VOLTAGE_RESOLUTION ) )
    {
        print( FATAL, "Invalid level for trigger of %g V, valid range is %g V "
               "to %g V.\n", MIN_TRIG_IN_LEVEL, MAX_TRIG_IN_LEVEL );
        THROW( EXCEPTION );
    }

    voltage = VOLTAGE_RESOLUTION * v;

    hfs9000.trig_in_level = voltage;
    hfs9000.is_trig_in_level = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
hfs9000_set_trig_in_slope( int slope )
{
    fsc2_assert( slope == POSITIVE || slope == NEGATIVE );

    if ( hfs9000.is_trig_in_slope && hfs9000.trig_in_slope != slope )
    {
        print( FATAL, "A different trigger slope (%s) has already been set.\n",
               slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
        THROW( EXCEPTION );
    }

    if ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL )
    {
        print( SEVERE, "Setting a trigger slope is useless in INTERNAL "
               "trigger mode.\n" );
        return FAIL;
    }

    if (    hfs9000.is_neg_delay
         && ! ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL ) )
    {
        print( FATAL, "Setting a trigger slope (implicitly selecting EXTERNAL "
               "trigger mode) and using negative delays for functions is "
               "incompatible.\n" );
        THROW( EXCEPTION );
    }

    hfs9000.trig_in_slope = slope;
    hfs9000.is_trig_in_slope = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
hfs9000_set_max_seq_len( double seq_len )
{
    if (    hfs9000.is_max_seq_len
         && hfs9000.max_seq_len != hfs9000_double2ticks( seq_len ) )
    {
        print( FATAL, "A differrent minimum pattern length of %s has already "
               "been set.\n", hfs9000_pticks( hfs9000.max_seq_len ) );
        THROW( EXCEPTION );
    }

    /* Check that the value is reasonable */

    if ( seq_len <= 0 )
    {
        print( FATAL, "Zero or negative minimum pattern length.\n" );
        THROW( EXCEPTION );
    }

    hfs9000.max_seq_len = hfs9000_double2ticks( seq_len );
    hfs9000.is_max_seq_len = SET;

    return OK;
}


/*----------------------------------------------*
 * Function that gets called to tell the pulser
 * driver to keep all pulses, even unused ones.
 *----------------------------------------------*/

bool
hfs9000_keep_all( void )
{
    hfs9000.keep_all = SET;
    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
