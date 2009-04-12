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


#include "tegam2714a_p.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
tegam2714a_p_store_timebase( double timebase )
{
    if ( timebase < MIN_TIMEBASE || timebase > MAX_TIMEBASE )
    {
        print( FATAL, "Invalid time base of %s, valid range is %s to %s.\n",
               tegam2714a_p_ptime( timebase ),
               tegam2714a_p_ptime( MIN_TIMEBASE ),
               tegam2714a_p_ptime( MAX_TIMEBASE ));
        THROW( EXCEPTION );
    }

    tegam2714a_p.is_timebase = SET;
    tegam2714a_p.timebase = timebase;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
tegam2714a_p_assign_channel_to_function( int  function,
                                         long channel )
{
    if ( channel < 0 || channel >= MAX_CHANNELS )
    {
        print( FATAL, "Invalid channel number: %ld, valid range is 0-%d.\n",
               channel, MAX_CHANNELS - 1 );
        THROW( EXCEPTION );
    }

    if ( channel == EMPTY_WAVEFORM_NUMBER )
    {
        print( FATAL, "The selected waveform is already in use for internal "
               "purposes.\n" );
        THROW( EXCEPTION );
    }

    if ( tegam2714a_p.function.self != PULSER_CHANNEL_NO_TYPE )
    {
        print( FATAL, "A function has already been assigned to the only "
               "channel this device has.\n" );
        THROW( EXCEPTION );
    }

    if (    function == PULSER_CHANNEL_PHASE_1
         || function == PULSER_CHANNEL_PHASE_2 )
    {
        print( FATAL, "PHASE functions can't be used with this pulser.\n" );
        THROW( EXCEPTION );
    }

    tegam2714a_p.function.channel = channel;
    tegam2714a_p.function.self = function;
    tegam2714a_p.function.name = Function_Names[ function ];

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
tegam2714a_p_invert_function( int function UNUSED_ARG )
{
    tegam2714a_p.function.is_inverted = SET;
    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
tegam2714a_p_set_function_delay( int    function,
                                 double delay )
{
    if ( tegam2714a_p.function.self == PULSER_CHANNEL_NO_TYPE )
        tegam2714a_p.function.self = function;
    else if ( tegam2714a_p.function.self != function )
    {
        print( FATAL, "Can't set delay for function '%s', (only) function of "
               "device has already been set to '%s'.\n",
               Function_Names[ function ],
               Function_Names[ tegam2714a_p.function.self ] );
        THROW( EXCEPTION );
    }

    if ( tegam2714a_p.function.is_delay )
    {
        print( FATAL, "Delay for function '%s' has already been set.\n",
               Function_Names[ function ] );
        THROW( EXCEPTION );
    }

    /* Check that the delay value is reasonable */

    if ( delay < 0.0 )
    {
        print( FATAL, "Negative delays are not possible with this "
               "device.\n" );
        THROW( EXCEPTION );
    }

    tegam2714a_p.function.delay = tegam2714a_p_double2ticks( delay );
    tegam2714a_p.function.is_delay = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
tegam2714a_p_set_function_high_level( int    function,
                                      double voltage )
{
    if ( tegam2714a_p.function.self == PULSER_CHANNEL_NO_TYPE )
        tegam2714a_p.function.self = function;
    else if ( tegam2714a_p.function.self != function )
    {
        print( FATAL, "Can't set high level for function '%s', (only) "
               "function of device has already been set to '%s'.\n",
               Function_Names[ function ],
               Function_Names[ tegam2714a_p.function.self ] );
        THROW( EXCEPTION );
    }

    if ( voltage > MAX_AMPLITUDE )
    {
        print( FATAL, "High voltage of %g V to high, maximum is %g V.\n",
               voltage, MAX_AMPLITUDE );
        THROW( EXCEPTION );
    }

    tegam2714a_p.function.high_level = voltage;
    tegam2714a_p.function.is_high_level = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
tegam2714a_p_set_function_low_level( int    function,
                                     double voltage )
{
    if ( tegam2714a_p.function.self == PULSER_CHANNEL_NO_TYPE )
        tegam2714a_p.function.self = function;
    else if ( tegam2714a_p.function.self != function )
    {
        print( FATAL, "Can't set low level for function '%s', (only) "
               "function of device has already been set to '%s'.\n",
               Function_Names[ function ],
               Function_Names[ tegam2714a_p.function.self ] );
        THROW( EXCEPTION );
    }

    if ( voltage < - MAX_AMPLITUDE )
    {
        print( FATAL, "Low voltage of %g V too low, manimum is -%g V.\n",
               voltage, MAX_AMPLITUDE );
        THROW( EXCEPTION );
    }

    tegam2714a_p.function.low_level = voltage;
    tegam2714a_p.function.is_low_level = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
tegam2714a_p_set_trigger_mode( int mode )
{
    fsc2_assert( mode == INTERNAL || mode == EXTERNAL );

    if ( mode == INTERNAL )
    {
        print( FATAL, "INTERNAL trigger mode is not possible with this "
               "device.\n" );
        THROW( EXCEPTION );
    }

    return OK;
}


/*--------------------------------------------------*
 * Function for setting the maximum sequence length
 *--------------------------------------------------*/

bool
tegam2714a_p_set_max_seq_len( double seq_len )
{
    if (    tegam2714a_p.is_max_seq_len
         && tegam2714a_p.max_seq_len != tegam2714a_p_double2ticks( seq_len ) )
    {
        print( FATAL, "A different minimum pattern length of %s has already "
               "been set.\n",
               tegam2714a_p_pticks( tegam2714a_p.max_seq_len ) );
        THROW( EXCEPTION );
    }

    /* Check that the value is reasonable */

    if ( seq_len <= 0 )
    {
        print( FATAL, "Zero or negative minimum pattern length.\n" );
        THROW( EXCEPTION );
    }

    /* Convert and store the value */

    tegam2714a_p.max_seq_len = tegam2714a_p_double2ticks( seq_len );

    if ( tegam2714a_p.max_seq_len < MIN_PULSER_BITS )
    {
        print( SEVERE, "Minimum pattern length is %s, had to increase the "
               "requested value accordingly.\n",
               tegam2714a_p_pticks( MIN_PULSER_BITS ) );
        tegam2714a_p.max_seq_len = MIN_PULSER_BITS;
    }
    else

    tegam2714a_p.is_max_seq_len = SET;

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
