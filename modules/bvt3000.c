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


static double flow_rates[ ] = {    0.0,    /* in l/h */
                                 135.0,
                                 270.0,
                                 400.0,
                                 535.0,
                                 670.0,
                                 800.0,
                                 935.0,
                                1070.0,
                                1200.0,
                                1335.0,
                                1470.0,
                                1600.0,
                                1735.0,
                                1870.0,
                                2000.0 };


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int
bvt3000_init_hook( void )
{
    /* Claim the serial port (throws exception on failure) */

    bvt3000.sn = fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

    bvt3000.is_open            = UNSET;
    bvt3000.state              = TEST_STATE;
    bvt3000.tune_state         = TEST_TUNE_STATE;
    bvt3000.setpoint           = TEST_SETPOINT;
    bvt3000.min_setpoint       = MIN_SETPOINT;
    bvt3000.max_setpoint       = MAX_SETPOINT;
    bvt3000.heater_state       = SET;
    bvt3000.heater_power_limit = TEST_HEATER_POWER_LIMIT;
    bvt3000.heater_power       = TEST_HEATER_POWER;
    bvt3000.flow_rate          = TEST_FLOW_RATE;
    bvt3000.cb[ 0 ]            = TEST_CUTBACK_LOW;
    bvt3000.cb[ 1 ]            = TEST_CUTBACK_HIGH;
    bvt3000.max_cb[ 0 ]        = 0.0;
    bvt3000.max_cb[ 1 ]        = 0.0;

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

    eurotherm902s_set_setpoint( SP1, sp );
    return vars_push( FLOAT_VAR, eurotherm902s_get_setpoint( SP1 ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
temp_contr_heater_state( Var_T * v )
{
    bool state;

    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( INT_VAR, ( long ) bvt3000.heater_state );
        return vars_push( FLOAT_VAR, ( long ) bvt3000_get_heater_state( ) );
    }

    state = get_boolean( v );

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
    {
        bvt3000.heater_state = state;
        return vars_push( INT_VAR, ( long ) state );
    }

    bvt3000_set_heater_state( state );
    return vars_push( INT_VAR,
                      ( long ) ( bvt3000.heater_state =
                                               bvt3000_get_heater_state( ) ) );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *
temp_contr_heater_power_limit( Var_T * v )
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

    if ( bvt3000.state != MANUAL_MODE )
    {
        print( FATAL, "Heater power can only be set while in MANUAL state.\n" );
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
temp_contr_gas_flow( Var_T * v )
{
    size_t i;
    double flow_rate;
    unsigned int fr_index = 0x0F;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( FLOAT_VAR, bvt3000.flow_rate );
        return vars_push( FLOAT_VAR, flow_rates[ bvt3000_get_flow_rate( ) ] );
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

    /* Take care: when heater is off flow rate gets autoatically changed
       to the default rate and changes aren't possible */

    if ( ! bvt3000.heater_state )
    {
        print( FATAL, "Can't set flow rate while heater is off.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
    {
        bvt3000.flow_rate = flow_rates[ fr_index ];
        return vars_push( FLOAT_VAR, flow_rates[ fr_index ] );
    }

    bvt3000_set_flow_rate( fr_index );
    return vars_push( FLOAT_VAR, flow_rates[ bvt3000_get_flow_rate( ) ] );
}


/*-----------------------------------------------------------------*
 * Determines or sets if the device is in manual or automatic mode
 *-----------------------------------------------------------------*/

Var_T *
temp_contr_state( Var_T * v )
{
    long state;


    if ( v == NULL )
        return vars_push( INT_VAR, FSC2_MODE != EXPERIMENT ?
                          ( long ) bvt3000.state :
                          ( long ) eurotherm902s_get_mode( ) );

    
    if ( v->type == STR_VAR )
    {
        if ( ! strcmp( v->val.sptr, "MANUAL" ) )
            state = MANUAL_MODE;
        else if ( ! strcmp( v->val.sptr, "AUTO" ) )
            state = AUTOMATIC_MODE;
        else
        {
            print( FATAL, "Invalid state specified as argument, only \"AUTO\" "
                   "and \"MANUAL\" can be used.\n" );
            THROW( EXCEPTION );
        }
    }
    else
    {
        state = get_long( v, "temperature controller state" );

        if ( state != AUTOMATIC_MODE && state != MANUAL_MODE )
        {
            print( FATAL, "Invalid state specified as argument, only \"AUTO\" "
                   "(3) and \"MANUAL\" (0) can be used.\n" );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
        bvt3000.state = state;
    else
    {
        eurotherm902s_set_mode( state );
        bvt3000.state = eurotherm902s_get_mode( );
    }

    return vars_push( INT_VAR, ( long ) bvt3000.state );
}
 

/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_tune_state( Var_T * v )
{
    int state;

    if ( v == NULL )
    {
        if ( FSC2_MODE != EXPERIMENT )
            return vars_push( INT_VAR, ( long ) bvt3000.tune_state );

        state =
              ( eurotherm902s_get_adaptive_tune_state( ) ? ADAPTIVE_TUNE : 0 )
            | ( eurotherm902s_get_self_tune_state( )     ? SELF_TUNE     : 0 );
        return vars_push( INT_VAR, ( long ) state );
    }

    if ( v->type == STR_VAR )
    {
        if ( ! strcmp( v->val.sptr, "OFF" ) )
            state = TUNE_OFF;
        else if ( ! strcmp( v->val.sptr, "ADAPTIVE" ) )
            state = ADAPTIVE_TUNE;
        else if ( ! strcmp( v->val.sptr, "SELF" ) )
            state = SELF_TUNE;
        else if (    ! strcmp( v->val.sptr, "ADAPTIVE+SELF" )
                  || ! strcmp( v->val.sptr, "SELF+ADAPTIVE" ) )
            state = ADAPTIVE_TUNE | SELF_TUNE;
        else
        {
            print( FATAL, "Invalid tune mode argument.\n" );
            THROW( EXCEPTION );
        }
    }
    else
    {
        state = get_long( v, "tune mode" );

        if ( state & ~ ( ADAPTIVE_TUNE | SELF_TUNE ) )
        {
            print( FATAL, "Invalid tune mode argument.\n" );
            THROW( EXCEPTION );
        }
    }            
        
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        eurotherm902s_set_adaptive_tune_state( state & ADAPTIVE_TUNE ?
                                               SET : UNSET );
        eurotherm902s_set_self_tune_state( state & SELF_TUNE ?
                                           SET : UNSET );

        state =
              ( eurotherm902s_get_adaptive_tune_state( ) ? ADAPTIVE_TUNE : 0 )
            | ( eurotherm902s_get_self_tune_state( )     ? SELF_TUNE     : 0 );
    }

    return vars_push( INT_VAR, ( long ) ( bvt3000.tune_state = state ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_proportional_band( Var_T * v )
{
    double pb;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, FSC2_MODE == EXPERIMENT ?
                          eurotherm902s_get_proportional_band( ) :
                          TEST_PROPORTIONAL_BAND );

    pb = get_double( v, "proportional band" );

    if ( pb < 0.0 )
    {
        print( FATAL, "Invalid negative value for proportional band.\n" );
        THROW( EXCEPTION );
    }

    pb = 0.1 * floor( 10.0 * pb );

    if ( pb > MAX_PROPORTIONAL_BAND )
    {
        print( FATAL, "Value for proportional band too large, maximum is "
               "%.1f K.\n", MAX_PROPORTIONAL_BAND );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( FLOAT_VAR, pb );

    eurotherm902s_set_proportional_band( pb );
 
   return vars_push( FLOAT_VAR, eurotherm902s_get_proportional_band( ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_integral_time( Var_T * v )
{
    double it;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, FSC2_MODE == EXPERIMENT ?
                          eurotherm902s_get_integral_time( ) :
                          TEST_INTEGRAL_TIME );

    it = get_double( v, "integral time" );

    if ( it < 0.0 )
    {
        print( FATAL, "Invalid negative value for integral time.\n" );
        THROW( EXCEPTION );
    }

    if ( it > MAX_INTEGRAL_TIME )
    {
        print( FATAL, "Value for integral time is too large, maximum is "
               "%.0f s\n", MAX_INTEGRAL_TIME );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( FLOAT_VAR, it );

    eurotherm902s_set_integral_time( it );
 
   return vars_push( FLOAT_VAR, eurotherm902s_get_integral_time( ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_derivative_time( Var_T * v )
{
    double dt;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, FSC2_MODE == EXPERIMENT ?
                          eurotherm902s_get_derivative_time( ) :
                          TEST_DERIVATIVE_TIME );

    dt = get_double( v, "derivative time" );

    if ( dt < 0.0 )
    {
        print( FATAL, "Invalid negative value for derivative time.\n" );
        THROW( EXCEPTION );
    }

    dt = 0.1 * floor( 10.0 * dt );

    if ( dt > MAX_DERIVATIVE_TIME )
    {
        print( FATAL, "Value for derivative time is too large, maximum is "
               "%.1f s\n", MAX_DERIVATIVE_TIME );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( FLOAT_VAR, dt );

    eurotherm902s_set_derivative_time( dt );
 
   return vars_push( FLOAT_VAR, eurotherm902s_get_derivative_time( ) );
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
 *----------------------------------------------------*/

Var_T *
temp_contr_cutbacks( Var_T * v )
{
    double cb[ 2 ];


    if ( v == NULL )
    {
        if ( FSC2_MODE == EXPERIMENT )
        {
            bvt3000.cb[ 0 ] = eurotherm902s_get_cutback_low( );
            bvt3000.cb[ 1 ] = eurotherm902s_get_cutback_high( );
        }

        return vars_push( FLOAT_ARR, bvt3000.cb, 2 );
    }

    /* Accept an array with two values or two separate values */

    if ( v->type == INT_ARR && v->len == 2 )
    {
        cb[ 0 ] = v->val.lpnt[ 0 ];
        cb[ 1 ] = v->val.lpnt[ 1 ];
    }
    else if ( v->type == FLOAT_ARR && v->len == 2 )
        memcpy( cb, v->val.dpnt, sizeof cb );
    else
    {
        cb[ 0 ] = get_double( v, "low cutback" );

        if ( ( v = vars_pop( v ) ) == NULL )
        {
            print( FATAL, "Missing argument for high cutback.\n" );
            THROW( EXCEPTION );
        }

        cb[ 1 ] = get_double( v, "high cutback" );
    }

    if ( cb[ 0 ] < 0.0 )
    {
        print( FATAL, "Invalid negative value for low cutback.\n" );
        THROW( EXCEPTION );
    }

    if ( cb[ 0 ] > bvt3000.max_setpoint - bvt3000.min_setpoint )
    {
        print( FATAL, "Value for low cutback too high, can't be larger than "
               "difference between minimum and maximum setpoint value.\n" );
        THROW( EXCEPTION );
    }

    if ( cb[ 1 ] < 0.0 )
    {
        print( FATAL, "Invalid negative value for high cutback.\n" );
        THROW( EXCEPTION );
    }

    if ( cb[ 1 ] > bvt3000.max_setpoint - bvt3000.min_setpoint )
    {
        print( FATAL, "Value for high cutback too high, can't be larger than "
               "difference between minimum and maximum setpoint value.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        eurotherm902s_set_cutback_low( cb[ 0 ] );
        eurotherm902s_set_cutback_high( cb[ 1 ] );

        bvt3000.cb[ 0 ] = eurotherm902s_get_cutback_low( );
        bvt3000.cb[ 1 ] = eurotherm902s_get_cutback_high( );
    }
    else
    {
        bvt3000.max_cb[ 0 ] = d_max( cb[ 0 ], bvt3000.cb[ 0 ] );
        bvt3000.max_cb[ 1 ] = d_max( cb[ 1 ], bvt3000.cb[ 1 ] );
        memcpy( bvt3000.cb, cb, sizeof bvt3000.cb );
    }
    
    return vars_push( FLOAT_ARR, bvt3000.cb, 2 );
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
