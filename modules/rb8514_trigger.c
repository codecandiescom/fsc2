/*
 *  Copyright (C) 2011 Jens Thoms Toerring
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


#include "fsc2_module.h"
#include <rulbus.h>


/* Include configuration information for the device */

#include "rb8514_trigger.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


static struct {
    int    handle;
    double length;
    double intrinsic_delay;
} rb8514_trigger, rb8514_trigger_stored;


int rb8514_trigger_init_hook(       void );
int rb8514_trigger_test_hook(       void );
int rb8514_trigger_exp_hook(        void );
int rb8514_trigger_end_of_exp_hook( void );

Var_T * trigger_name(     Var_T * v );
Var_T * trigger_duration( Var_T * v );
Var_T * trigger_raise(    Var_T * v );


/*--------------------------------------------------------------*
 * Function called immediately after the module has been loaded
 *--------------------------------------------------------------*/

int
rb8514_trigger_init_hook( void )
{
    RULBUS_CARD_INFO card_info;


    Need_RULBUS = SET;

    if ( rulbus_get_card_info( RULBUS_CARD_NAME, &card_info ) != RULBUS_OK )
    {
        print( FATAL, "Failed to get RULBUS configuration: %s.\n",
               rulbus_strerror( ) );
        THROW( EXCEPTION );
    }

    rb8514_trigger.handle = -1;
    rb8514_trigger.length = 6.0e-8;
    rb8514_trigger.intrinsic_delay = 4.0e-8;

    return 1;
}


/*----------------------------------------------*
 * Function called at the start of the test run
 *----------------------------------------------*/

int
rb8514_trigger_test_hook( void )
{
    rb8514_trigger_stored = rb8514_trigger;
    return 1;
}

/*------------------------------------------------*
 * Function called at the start of the experiment
 *------------------------------------------------*/

int rb8514_trigger_exp_hook( void )
{
    rb8514_trigger = rb8514_trigger_stored;

    raise_permissions( );
    rb8514_trigger.handle = rulbus_card_open( RULBUS_CARD_NAME );

    if (    rb8514_trigger.handle < 0
         || rulbus_rb8514_delay_set_clock_frequency( rb8514_trigger.handle,
                                                     RULBUS_DELAY_INPUT_FREQ )
                                                                          < 0
         || ( rb8514_trigger.intrinsic_delay =
              rulbus_rb8514_delay_get_intrinsic_delay( rb8514_trigger.handle ) )
                                                                         < 0.0 )
    {
        lower_permissions( );
        print( FATAL, "Initialization of card failed: %s.\n",
               rulbus_strerror( ) );
        THROW( EXCEPTION );
    }

    lower_permissions( );
    return 1;
}


/*----------------------------------------------*
 * Function called at the end of the experiment
 *----------------------------------------------*/

int
rb8514_trigger_end_of_exp_hook( void )
{
    if ( rb8514_trigger.handle >= 0 )
    {
        raise_permissions( );
        rulbus_card_close( rb8514_trigger.handle );
        lower_permissions( );

        rb8514_trigger.handle = -1;
    }

    return 1;
}


/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *
trigger_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

Var_T *
trigger_duration( Var_T * v )
{
    double length;


    if ( v == 0 )
        return vars_push( FLOAT_VAR, rb8514_trigger.length );

    length = get_double( v, "trigger duration" );

    if (    ( FSC2_MODE == TEST && length < 0.99 / RULBUS_DELAY_INPUT_FREQ )
         || (    FSC2_MODE == EXPERIMENT
              && length < 0.99 * rb8514_trigger.intrinsic_delay ) )
    {
        print( FATAL, "Requested length of trigger is too short.\n" );
        THROW( EXCEPTION );
    }

    if ( length > 16777215.0 /RULBUS_DELAY_INPUT_FREQ )
    {
        print( FATAL, "Requested length of trigger is too long.\n" );
        THROW( EXCEPTION );
    }
        
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        int ret;

        raise_permissions( );
        ret = rulbus_rb8514_delay_set_delay( rb8514_trigger.handle, length, 1 );
        lower_permissions( );

        if ( ret < 0 )
        {
            print( FATAL, "Falied to set trigger length.\n" );
            THROW( EXCEPTION );
        }
    }
            
    rb8514_trigger.length = length;

    return vars_push( FLOAT_VAR, length );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

Var_T *
trigger_raise( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == EXPERIMENT )
    {
        int ret;

        raise_permissions( );
        ret = rulbus_rb8514_software_start( rb8514_trigger.handle );
        lower_permissions( );

        if ( ret < 0 )
        {
            print( FATAL, "Failed to raise trigger.\n" );
            THROW( EXCEPTION );
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
