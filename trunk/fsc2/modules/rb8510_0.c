/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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


#include "fsc2_module.h"
#include <rulbus.h>


/* Include configuration information for the device */

#include "rb8510_0.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define TEST_VOLTS 3.578


static struct {
    int handle;
    double volts;
    bool volts_is_set;
    double Vmax;
    double Vmin;
    double dV;
    char *reserved_by;
} rb8510, rb8510_stored;


int  rb8510_0_init_hook(       void );
int  rb8510_0_test_hook(       void );
int  rb8510_0_exp_hook(        void );
int  rb8510_0_end_of_exp_hook( void );
void rb8510_0_exit_hook(       void );

Var_T *daq_name(        Var_T * v );
Var_T *daq_reserve_dac( Var_T * v );
Var_T *daq_set_voltage( Var_T * v );


/*--------------------------------------------------------------*
 * Function called immediately after the module has been loaded
 *--------------------------------------------------------------*/

int rb8510_0_init_hook( void )
{
    Need_RULBUS = SET;

    rb8510.handle = -1;
    rb8510.volts_is_set = UNSET;
    rb8510.reserved_by = NULL;

    return 1;
}


/*----------------------------------------------*
 * Function called at the start of the test run
 *----------------------------------------------*/

int rb8510_0_test_hook( void )
{
    rb8510_stored = rb8510;

    if ( rb8510.volts_is_set )
        rb8510.Vmax = rb8510.Vmin = rb8510.volts;
    else
    {
        rb8510.Vmax = - HUGE_VAL;
        rb8510.Vmin = HUGE_VAL;
    }

    if ( rb8510.reserved_by )
        rb8510_stored.reserved_by = T_strdup( rb8510.reserved_by );

    return 1;
}

/*------------------------------------------------*
 * Function called at the start of the experiment
 *------------------------------------------------*/

int rb8510_0_exp_hook( void )
{
    int ret;


    rb8510 = rb8510_stored;

    /* Open the card */

    raise_permissions( );
    rb8510.handle = rulbus_card_open( RULBUS_CARD_NAME );
    lower_permissions( );

    if ( rb8510.handle < 0 )
    {
        print( FATAL, "Initialization of card failed: %s.\n",
               rulbus_strerror( ) );
        THROW( EXCEPTION );
    }

    /* Find out about minimum and maximum output voltage */

    raise_permissions( );
    ret = rulbus_rb8510_dac12_properties( rb8510.handle, &rb8510.Vmax,
                                          &rb8510.Vmin, &rb8510.dV );
    lower_permissions( );

    if ( ret != RULBUS_OK )
    {
        print( FATAL, "Initialization of card failed: %s.\n",
               rulbus_strerror( ) );
        THROW( EXCEPTION );
    }

    /* Check if during the test run an invalid voltage was requested */

    if ( rb8510_stored.Vmax >= rb8510.Vmax + 0.5 * rb8510.dV )
    {
        print( FATAL, "Voltage requested in preparation or test run was "
               "higher than maximum output voltage of card.\n" );
        THROW( EXCEPTION );
    }

    if ( rb8510_stored.Vmin <= rb8510.Vmin - 0.5 * rb8510.dV )
    {
        print( FATAL, "Voltage requested in preparation or test run was lower "
               "than minimum output voltage of card.\n" );
        THROW( EXCEPTION );
    }

    if ( rb8510.volts_is_set )
    {
        raise_permissions( );
        ret = rulbus_rb8510_dac12_set_voltage( rb8510.handle, rb8510.volts );
        lower_permissions( );

        if ( ret != RULBUS_OK )
        {
            print( FATAL, "Initialization of card failed: %s.\n",
                   rulbus_strerror( ) );
            THROW( EXCEPTION );
        }
    }

    return 1;
}


/*----------------------------------------------*
 * Function called at the end of the experiment
 *----------------------------------------------*/

int rb8510_0_end_of_exp_hook( void )
{
    if ( rb8510.handle >= 0 )
    {
        raise_permissions( );
        rulbus_card_close( rb8510.handle );
        lower_permissions( );

        rb8510.handle = -1;

        if ( rb8510.reserved_by &&
             rb8510.reserved_by != rb8510_stored.reserved_by )
            rb8510.reserved_by = CHAR_P T_free( rb8510.reserved_by );
    }

    return 1;
}


/*------------------------------------------------------*
 * Function called just before the module gets unloaded
 *------------------------------------------------------*/

void rb8510_0_exit_hook( void )
{
    if ( rb8510_stored.reserved_by )
        T_free( rb8510_stored.reserved_by );
}


/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *daq_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------------------*
 * Functions allows to reserve (or un-reserve) the DAC so that in the
 * following setting the DAC requires a pass-phrase as the very first
 * argument to the function daq_set_voltage().
 *-------------------------------------------------------------------*/

Var_T *daq_reserve_dac( Var_T * v )
{
    bool lock_state = SET;


    if ( v == NULL )
        return vars_push( INT_VAR, rb8510.reserved_by ? 1L : 0L );

    if ( v->type != STR_VAR )
    {
        print( FATAL, "First argument isn't a string.\n" );
        THROW( EXCEPTION );
    }

    if ( v->next != NULL )
    {
        lock_state = get_boolean( v->next );
        too_many_arguments( v->next );
    }

    if ( rb8510.reserved_by )
    {
        if ( lock_state == SET )
        {
            if ( ! strcmp( rb8510.reserved_by, v->val.sptr ) )
                return vars_push( INT_VAR, 1L );
            else
                return vars_push( INT_VAR, 0L );
        }
        else
        {
            if ( ! strcmp( rb8510.reserved_by, v->val.sptr ) )
            {
                rb8510.reserved_by = CHAR_P T_free( rb8510.reserved_by );
                return vars_push( INT_VAR, 1L );
            }
            else
                return vars_push( INT_VAR, 0L );
        }
    }

    if ( ! lock_state )
        return vars_push( INT_VAR, 1L );

    rb8510.reserved_by = T_strdup( v->val.sptr );
    return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------*
 * Function sets or returns the output voltage
 *--------------------------------------------*/

Var_T *daq_set_voltage( Var_T * v )
{
    double volts;
    char *pass = NULL;
    int ret;


    if ( v != NULL && v->type == STR_VAR )
    {
        pass = v->val.sptr;
        v = v->next;
    }

    if ( v == NULL )
    {
        if ( ! rb8510.volts_is_set )
        {
            print( FATAL, "Can't determine output voltage because it never "
                   "has been set.\n" );
            THROW( EXCEPTION );
        }

        return vars_push( FLOAT_VAR, rb8510.volts );
    }

    if ( rb8510.reserved_by )
    {
        if ( pass == NULL )
        {
            print( FATAL, "DAQ is reserved, phase-phrase required.\n" );
            THROW( EXCEPTION );
        }
        else if ( strcmp( rb8510.reserved_by, pass ) )
        {
            print( FATAL, "DAQ is reserved, wrong phase-phrase.\n" );
            THROW( EXCEPTION );
        }
    }       

    volts = get_double( v, "DAC output voltage" );
    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION )
    {
        if ( rb8510.volts_is_set )
        {
            print( SEVERE, "Output voltage has already been set, keeping "
                   "old value of %.2f V.\n", rb8510.volts );
            return vars_push( FLOAT_VAR, rb8510.volts );
        }

        rb8510.volts_is_set = SET;
        rb8510.volts = volts;
        return vars_push( FLOAT_VAR, rb8510.volts );
    }

    if ( FSC2_MODE == TEST )
    {
        if ( volts > rb8510.Vmax )
            rb8510.Vmax = volts;
        else if ( volts < rb8510.Vmin )
            rb8510.Vmin = volts;
        rb8510.volts = volts;

        return vars_push( FLOAT_VAR, rb8510.volts );
    }

    if ( volts >= rb8510.Vmax + 0.5 * rb8510.dV )
    {
        print( FATAL, "Requested output voltage is higher than maximum output "
               "voltage of %.2f V\n", rb8510.Vmax );
        THROW( EXCEPTION );
    }

    if ( volts <= rb8510.Vmin - 0.5 * rb8510.dV )
    {
        print( FATAL, "Requested output voltage is lower than minimum output "
               "voltage of %.2f V\n", rb8510.Vmin );
        THROW( EXCEPTION );
    }

    raise_permissions( );
    ret = rulbus_rb8510_dac12_set_voltage( rb8510.handle, volts ); 
    lower_permissions( );

    if ( ret != RULBUS_OK )
    {
        print( FATAL, "Setting output voltage failed: %s.\n",
               rulbus_strerror( ) );
        THROW( EXCEPTION );
    }

    rb8510.volts = volts;
    rb8510.volts_is_set = SET;

    return vars_push( FLOAT_VAR, volts );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

