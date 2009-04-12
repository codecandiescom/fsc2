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


#include "bvt3000.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


BVT3000 bvt3000;


static double flow_rates[ ] = { 0.0, 135.0, 270.0, 400.0, 535.0, 670.0, 800.0,
                                935.0, 1070.0, 1200.0, 1335.0, 1470.0, 1600.0,
                                1735.0, 1870.0, 2000.0 };


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int
bvt3000_init_hook( void )
{
    /* Claim the serial port (throws exception on failure) */

    bvt3000.sn = fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

    bvt3000.is_open            = UNSET;
    bvt3000.setpoint           = TEST_SETPOINT;
    bvt3000.min_setpoint       = MIN_SETPOINT;
    bvt3000.max_setpoint       = MAX_SETPOINT;
    bvt3000.heater_power_limit = TEST_HEATER_POWER_LIMIT;
    bvt3000.heater_power       = TEST_HEATER_POWER;
    bvt3000.flow_rate          = TEST_FLOW_RATE;

	return 1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int
bvt3000_exp_hook( void )
{
	bvt3000_init( );
	return 1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int
bvt3000_end_of_exp_hook( void )
{
    if ( bvt3000.is_open )
    {
		fsc2_serial_close( bvt3000.sn );
        bvt3000.is_open = UNSET;
    }

	return 1;
}


/*------------------------------*
 * Just returns the device name 
 *------------------------------*/

Var_T *
temp_contr_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
temp_contr_temperature( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, TEST_TEMPERATURE );

    return vars_push( FLOAT_VAR, eurotherm902s_get_temperature( ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
temp_contr_setpoint( Var_T * v )
{
    double sp;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, bvt3000.setpoint );
        return vars_push( FLOAT_VAR, eurotherm902s_get_working_setpoint( ) );
    }

    sp = get_double( v, "setpoint" );

    too_many_arguments( v );

    if ( sp < bvt3000.min_setpoint || sp > bvt3000.max_setpoint )
    {
        print( FATAL, "Requested setpoint out of range\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        bvt3000.setpoint = sp;
        bvt3000.min_setpoint = d_min( sp, bvt3000.min_setpoint );
        bvt3000.max_setpoint = d_max( sp, bvt3000.max_setpoint );
        return vars_push( FLOAT_VAR, sp );
    }

    eurotherm902s_set_setpoint( sp );
    return vars_push( FLOAT_VAR, eurotherm902s_get_setpoint( ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
temp_contr_heater_power_limit( Var_T *v )
{
    double hp;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, bvt3000.heater_power_limit );
        return vars_push( FLOAT_VAR, eurotherm902s_get_heater_power_limit( ) );
    }

    hp = get_double( v, "heater power" );

    too_many_arguments( v );

    if ( hp < 0.0 || hp > 100.0 )
    {
        print( FATAL, "Invalid value for heater power limit, must be between "
               "0 %% and 100 %%.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        bvt3000.heater_power_limit = hp;
        return vars_push( FLOAT_VAR, hp );
    }

    eurotherm902s_set_heater_power_limit( hp );
    return vars_push( FLOAT_VAR, eurotherm902s_get_heater_power_limit( ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
temp_contr_heater_power( Var_T *v )
{
    double hp;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, bvt3000.heater_power );
        return vars_push( FLOAT_VAR, eurotherm902s_get_heater_power( ) );
    }

    hp = get_double( v, "heater power" );

    too_many_arguments( v );

    if ( hp < 0.0 || hp > 100.0 )
    {
        print( FATAL, "Invalid value for heater output, must be between "
               "0 %% and 100 %%.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        bvt3000.heater_power = hp;
        return vars_push( FLOAT_VAR, hp );
    }

    eurotherm902s_set_heater_power( hp );
    return vars_push( FLOAT_VAR, eurotherm902s_get_heater_power( ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_flow_rate( Var_T * v )
{
    size_t i;
    double flow_rate;
    unsigned int fr_index = 0x0F;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, bvt3000.flow_rate );
        return vars_push( FLOAT_VAR, bvt3000_get_flow_rate( ) );
    }

    flow_rate = get_double( v, "flow rate" );

    too_many_arguments( v );

    if ( flow_rate < 0 )
    {
        print( FATAL, "Invalid negative flow rate.\n" );
        THROW( EXCEPTION );
    }

    if ( flow_rate > 1.001 * flow_rates[ NUM_ELEMS( flow_rates ) - 1 ] )
    {
        print( FATAL, "Invalid flow rate of %.1f l/h, must be below "
               "%.1f l/h.\n", flow_rates[ NUM_ELEMS( flow_rates ) - 1 ] );
        THROW( EXCEPTION );
    }

    for ( i = 1; i < NUM_ELEMS( flow_rates ); i++ )
        if (    flow_rate >= flow_rates[ i - 1 ]
             && flow_rate <= flow_rates[ i ] )
        {
            if ( flow_rate < 0.5 * ( flow_rates[ i - 1 ] + flow_rates[ i ] ) )
                fr_index = i - 1;
            else
                fr_index = i;
            break;
        }

    if (    flow_rate != 0.0
         && fabs( ( flow_rate - flow_rates[ fr_index ] ) / flow_rate ) > 0.01 )
        print( WARN, "Flow rate had to be adjusted from %.1f l/h to "
               "%.1f l/h.\n", flow_rate, flow_rates[ fr_index ] );
               

    if ( FSC2_MODE != EXPERIMENT )
    {
        bvt3000.flow_rate = flow_rates[ fr_index ];
        return vars_push( FLOAT_VAR, flow_rates[ fr_index ] );
    }

    bvt3000_set_flow_rate( fr_index );
    return vars_push( FLOAT_VAR, flow_rates[ bvt3000_get_flow_rate( ) ] );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_lock_keyboard( Var_T * v )
{
    bool lock;


    if ( v == NULL )
        lock = SET;
    else
    {
        lock = get_boolean( v );
        too_many_arguments( v );
    }

    if ( FSC2_MODE == EXPERIMENT )
        eurotherm902s_lock_keyboard( lock );

    return vars_push( INT_VAR, lock ? 1L : 0L );
}


/*----------------------------------------------------*
 * Sends a query to the device and returns the result
 *----------------------------------------------------*/

Var_T *
temp_contr_command( Var_T * v )
{
	if ( v->type != STR_VAR )
	{
		print( FATAL, "Argument is not a string.\n" );
		THROW( EXCEPTION );
	}

	if ( strlen( v->val.sptr ) != 2 )
	{
		print( FATAL, "Invalid argument, must be a string with 2 "
			   "characters.\n" );
		THROW( EXCEPTION );
	}

	if (    islower( ( int ) v->val.sptr[ 0 ] )
		 || islower( ( int ) v->val.sptr[ 1 ] ) )
	{
		print( FATAL, "Invalid argument, all characters must be upper "
			   "case.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( STR_VAR, "No reply" );

	return vars_push( STR_VAR, bvt3000_query( v->val.sptr ) );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
