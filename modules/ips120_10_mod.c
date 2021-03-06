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


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "ips120_10_mod.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Values used during the test run to simulate the "real" device */

#define TEST_CURRENT       63.0      /* about 9 T */
#define TEST_SWEEP_RATE    1.0e-3    /* about 1.5 G/s */


/* How many time we retry when an error happens during communication with
   the device */

#define MAX_RETRIES        10

/* Declaration of exported functions */

int ips120_10_mod_init_hook(       void );
int ips120_10_mod_test_hook(       void );
int ips120_10_mod_exp_hook(        void );
int ips120_10_mod_end_of_exp_hook( void );

Var_T * magnet_name(              Var_T * v );
Var_T * magnet_setup(             Var_T * v );
Var_T * magnet_field(             Var_T * v );
Var_T * get_field(                Var_T * v );
Var_T * set_field(                Var_T * v );
Var_T * magnet_field_step_size(   Var_T * v );
Var_T * magnet_sweep(             Var_T * v );
Var_T * magnet_sweep_rate(        Var_T * v );
Var_T * magnet_reset_field(       Var_T * v );
Var_T * reset_field(              Var_T * v );
Var_T * magnet_goto_field_on_end( Var_T * v );
Var_T * magnet_command(           Var_T * v );


static void magnet_sweep_up( void );

static void magnet_sweep_down( void );

static void magnet_stop_sweep( void );

static bool ips120_10_mod_init( const char * name );

static void ips120_10_mod_to_local( void );

static void ips120_10_mod_get_complete_status( void );

static void ips120_10_mod_sweep_up( void );

static void ips120_10_mod_sweep_down( void );

static double ips120_10_mod_current_check( double current );

static double ips120_10_mod_sweep_rate_check( double sweep_rate );

static double ips120_10_mod_get_act_current( void );

static double ips120_10_mod_set_target_current( double current );

static double ips120_10_mod_get_target_current( void );

static double ips120_10_mod_set_sweep_rate( double sweep_rate );

static double ips120_10_mod_get_sweep_rate( void );

static double ips120_10_mod_goto_current( double current );

static int ips120_10_mod_set_activity( int activity );

static long ips120_10_mod_talk( const char * message,
								char *       reply,
								long         length );

static void  ips120_10_mod_read( char * buffer,
								 long * len );

static void ips120_10_mod_comm_failure( void );


#define MESSAGE_AVAILABLE 0x10


struct IPS120_10_MOD {
	int device;

	int sweep_state;

	int activity;
	int mode;
	int state;
	int loc_rem_state;

	double act_current;
	double target_current;

	double start_current;
	bool is_start_current;

	double sweep_rate;
	bool is_sweep_rate;

	double fast_sweep_rate;

	double max_current;
	double min_current;

	double time_estimate;

	bool goto_field_on_end;
	double final_target_current;

	char *dac_func;           /* name of function to set DAC voltage */

	bool in_init_phase;
};


enum {
	STOPPED,
	SWEEPING_UP,
	SWEEPING_DOWN
};

enum {
	LOCAL_AND_LOCKED,
	REMOTE_AND_LOCKED,
	LOCAL_AND_UNLOCKED,
	REMOTE_AND_UNLOCKED
};

enum {
	HOLD,
	TO_SET_POINT,
	TO_ZERO
};

enum {
	AT_REST,
	SWEEPING,
	SWEEP_LIMITING,
	SWEEPING_AND_SWEEP_LIMITING
};


enum {
	FAST,
	SLOW
};


static struct IPS120_10_MOD ips120_10_mod, ips120_10_mod_stored;


/*-----------------------------------*
 * Init hook function for the module
 *-----------------------------------*/

int
ips120_10_mod_init_hook( void )
{
	int dev_num;
	Var_T *func_ptr;
	char *reserve_dac_func;
	int acc;
	Var_T *v;


	Need_GPIB = SET;

	ips120_10_mod.in_init_phase = UNSET;

	/* Check if the module for the DAC we need has been loaded */

	if ( ( dev_num = exists_device( DAC_NAME ) ) < 1 )
	{
		print( FATAL, "Module for DAC '%s' required by the magnet "
			   "power supply isn't loaded.\n", DAC_NAME );
		THROW( EXCEPTION );
	}

	/* Assemble the name of the function for setting the DAC port and test
	   if it exists */

	if ( dev_num == 1 )
		ips120_10_mod.dac_func = T_strdup( "daq_set_voltage" );
	else
		ips120_10_mod.dac_func = get_string( "daq_set_voltage#%d", dev_num );

	if ( ( func_ptr = func_get( ips120_10_mod.dac_func, &acc ) ) == NULL )
	{
		print( FATAL, "DAC module '%s' not supplying a function for setting "
			   "a voltage.\n", DAC_NAME );
		T_free( ips120_10_mod.dac_func );
		THROW( EXCEPTION );
	}

	vars_pop( func_ptr );

	/* Now try to resere the DAC, password is the power supplies name */

	if ( dev_num == 1 )
		reserve_dac_func = T_strdup( "daq_reserve_dac" );
	else
		reserve_dac_func = get_string( "daq_reserve_dac#%d", dev_num );

	if ( ( func_ptr = func_get( reserve_dac_func, &acc ) ) == NULL )
	{
		print( FATAL, "DAC module '%s' not supplying a function for reserving "
			   "the DAC.\n", DAC_NAME );
		THROW( EXCEPTION );
	}

	vars_push( STR_VAR, DEVICE_NAME );
	vars_push( INT_VAR, 1L );
	v = func_call( func_ptr );

	if ( v->val.lval != 1 )
	{
		print( FATAL, "Can't reserve DAC '%s'.\n" DAC_NAME );
		vars_pop( v );
		THROW( EXCEPTION );
	}

	vars_pop( v );

	ips120_10_mod.device = -1;

	ips120_10_mod.act_current = 0.0;

	ips120_10_mod.is_start_current = UNSET;
	ips120_10_mod.is_sweep_rate = UNSET;

	ips120_10_mod.fast_sweep_rate = FAST_SWEEP_RATE;

	ips120_10_mod.max_current = MAX_CURRENT;
	ips120_10_mod.min_current = MIN_CURRENT;

	ips120_10_mod.goto_field_on_end = UNSET;

	return 1;
}


/*-----------------------------------*
 * Test hook function for the module
 *-----------------------------------*/

int
ips120_10_mod_test_hook( void )
{
	ips120_10_mod_stored = ips120_10_mod;

	if ( ips120_10_mod.is_start_current )
		ips120_10_mod.act_current = ips120_10_mod.start_current;
	else
	{
		ips120_10_mod.start_current = ips120_10_mod.act_current = TEST_CURRENT;
		ips120_10_mod.is_start_current = SET;
	}

	if ( ! ips120_10_mod.is_sweep_rate )
	{
		ips120_10_mod.sweep_rate = TEST_SWEEP_RATE;
		ips120_10_mod.is_sweep_rate = SET;
	}

	ips120_10_mod.activity = HOLD;
	ips120_10_mod.sweep_state = STOPPED;

	ips120_10_mod.time_estimate = experiment_time( );

	return 1;
}


/*--------------------------------------------------*
 * Start of experiment hook function for the module
 *--------------------------------------------------*/

int
ips120_10_mod_exp_hook( void )
{
	ips120_10_mod = ips120_10_mod_stored;

	ips120_10_mod.in_init_phase = SET;

	if ( ! ips120_10_mod_init( DEVICE_NAME ) )
	{
		ips120_10_mod.in_init_phase = UNSET;
		print( FATAL, "Initialization of device failed: %s.\n",
			   gpib_last_error( ) );
		THROW( EXCEPTION );
	}

	ips120_10_mod.in_init_phase = UNSET;

	return 1;
}


/*------------------------------------------------*
 * End of experiment hook function for the module
 *------------------------------------------------*/

int
ips120_10_mod_end_of_exp_hook( void )
{
	ips120_10_mod_to_local( );
	ips120_10_mod = ips120_10_mod_stored;
	ips120_10_mod.device = -1;

	return 1;
}


/*--------------------------------------*
 * Function returns the name the device
 *--------------------------------------*/

Var_T *
magnet_name( Var_T * v  UNUSED_ARG )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*
 * Function for registering the start current and the sweep rate.
 *----------------------------------------------------------------*/

Var_T *
magnet_setup( Var_T * v )
{
	double cur;
	double sweep_rate;


	if ( v->next == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	cur = get_double( v, "field" ) / F2C_RATIO;
	ips120_10_mod.start_current = ips120_10_mod_current_check( cur );
	ips120_10_mod.is_start_current = SET;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		sweep_rate = get_double( v, "magnet sweep speed" ) / F2C_RATIO;
		if ( sweep_rate < 0.0 )
		{
			print( FATAL, "Negative sweep rates can't be used, use argument "
				   "to magnet_sweep() to set sweep direction.\n" );
			THROW( EXCEPTION );
		}

		ips120_10_mod.sweep_rate =
								  ips120_10_mod_sweep_rate_check( sweep_rate );
		ips120_10_mod.is_sweep_rate = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------------------------*
 * Function returns the momentary field. During the test run an estimate
 * of the field is returned, based on the sweep rate and a guess of the
 * time spent since the last call for determining the field.
 *-----------------------------------------------------------------------*/

Var_T *
get_field( Var_T * v  UNUSED_ARG )
{
	if ( FSC2_MODE == TEST )
	{
		/* During the test run we need to return some not completely bogus
		   value when a sweep is run. Thus an estimate for the time spend
		   until now is fetched, multiplied by the sweep rate and added to
		   the current field */

		if (    ips120_10_mod.sweep_state != STOPPED
			 && ips120_10_mod.activity == TO_SET_POINT )
		{
			ips120_10_mod.time_estimate = experiment_time( );

			ips120_10_mod.act_current =
				 1.0e-4 * lrnd( 1.0e4 * ( ips120_10_mod.act_current
								+ experiment_time( ) * ips120_10_mod.sweep_rate
								* ( ips120_10_mod.sweep_state == SWEEPING_UP ?
									1.0 : - 1.0 ) ) );

			if ( ips120_10_mod.act_current > ips120_10_mod.max_current )
				ips120_10_mod.act_current = ips120_10_mod.max_current;
			if ( ips120_10_mod.act_current < ips120_10_mod.min_current )
				ips120_10_mod.act_current = ips120_10_mod.min_current;
		}
	}
	else
		ips120_10_mod.act_current = ips120_10_mod_get_act_current( );

	/* If a sweep reached one of the current limits stop the sweep */

	if (    (    (    ips120_10_mod.sweep_state == SWEEPING_UP
				   || ips120_10_mod.activity == TO_SET_POINT )
			  && ips120_10_mod.act_current >= ips120_10_mod.max_current )
		 || (    (    ips120_10_mod.sweep_state == SWEEPING_DOWN
				   || ips120_10_mod.activity == TO_SET_POINT )
			  && ips120_10_mod.act_current <= ips120_10_mod.min_current ) )
	{
		print( WARN, "Sweep had to be stopped because current limit was "
			   "reached.\n" );

		if ( FSC2_MODE == EXPERIMENT )
			ips120_10_mod.activity = ips120_10_mod_set_activity( HOLD );
		else
			ips120_10_mod.activity = HOLD;
		ips120_10_mod.sweep_state = STOPPED;
	}

	return vars_push( FLOAT_VAR, ips120_10_mod.act_current * F2C_RATIO );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

Var_T *
magnet_field( Var_T * v )
{
	return v == NULL ? get_field( v ) : set_field( v );
}


/*------------------------------------------------------*
 * Function for setting a new field value. Please note
 * that setting a new field also stops a running sweep.
 *------------------------------------------------------*/

Var_T *
set_field( Var_T * v )
{
	double cur;


	/* Stop sweeping */

	if (    ips120_10_mod.sweep_state != STOPPED
		 || ips120_10_mod.activity != HOLD )
	{
		if ( FSC2_MODE == EXPERIMENT )
			ips120_10_mod.activity = ips120_10_mod_set_activity( HOLD );
		else
			ips120_10_mod.activity = HOLD;
		ips120_10_mod.sweep_state = STOPPED;
	}

	/* Check the current */

	cur = ips120_10_mod_current_check( get_double( v, "field" ) / F2C_RATIO );

	if ( FSC2_MODE == EXPERIMENT )
		cur = ips120_10_mod_goto_current( cur );

	ips120_10_mod.act_current = cur;

	return vars_push( FLOAT_VAR, ips120_10_mod.act_current * F2C_RATIO );
}


/*----------------------------------------------------------------*
 * Function returns the minimum field step size if called without
 * an argument and the possible field step size nearest to the
 * argument.
 *----------------------------------------------------------------*/

Var_T *
magnet_field_step_size( Var_T * v )
{
    double cur_step;
    long steps;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, DAC_CURRENT_RESOLUTION * F2C_RATIO );

    cur_step = get_double( v, "field step size" ) / F2C_RATIO;

    too_many_arguments( v );

    if ( cur_step < 0.0 )
    {
        print( FATAL, "Invalid negative field step size\n" );
        THROW( EXCEPTION );
    }

    if ( ( steps = lrnd( cur_step / DAC_CURRENT_RESOLUTION ) ) == 0 )
		steps++;

    return vars_push( FLOAT_VAR, steps * DAC_CURRENT_RESOLUTION * F2C_RATIO );
}


/*-------------------------------------------------------------*
 * This is the EDL function for starting or stopping sweeps or
 * inquiring about the current sweep state.
 *-------------------------------------------------------------*/

Var_T *
magnet_sweep( Var_T * v )
{
	long dir;
	Var_T *vc;


	if ( v == NULL || FSC2_MODE == TEST )
	{
		vc = get_field( NULL );
		ips120_10_mod.act_current = vc->val.dval / F2C_RATIO;
		vars_pop( vc );
	}

	if ( v == NULL )
		switch ( ips120_10_mod.sweep_state )
		{
			case STOPPED :
				return vars_push( INT_VAR, 0 );

			case SWEEPING_UP :
				return vars_push( INT_VAR, 1 );

			case SWEEPING_DOWN :
				return vars_push( INT_VAR, -1 );
		}

	if ( v->type != STR_VAR )
		dir = get_long( v, "sweep direction" );
	else
	{
		if ( ! strcasecmp( v->val.sptr, "UP" ) )
			dir = 1;
		else if ( ! strcasecmp( v->val.sptr, "DOWN" ) )
			dir = -1;
		else if ( ! strcasecmp( v->val.sptr, "STOP" ) )
			dir = 0;
		else
		{
			print( FATAL, "Invalid sweep direction : '%s'.\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}

	if ( dir == 0 )
		magnet_stop_sweep( );
	else if ( dir > 0 )
	{
		dir = 1;
		magnet_sweep_up( );
	}
	else
	{
		dir = -1;
		magnet_sweep_down( );
	}

	return vars_push( FLOAT_VAR, dir );
}


/*----------------------------------------------------*
 * Function for starting an upward sweep of the field
 *----------------------------------------------------*/

static void
magnet_sweep_up( void )
{
	if ( ! ips120_10_mod.is_sweep_rate )
	{
		print( FATAL, "No sweep rate has been set.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ips120_10_mod.sweep_state == SWEEPING_UP )
		{
			print( SEVERE, "Field is already sweeping up.\n" );
			return;
		}

		if ( ips120_10_mod.act_current >=
			 				   ips120_10_mod.max_current - CURRENT_RESOLUTION )
		{
			print( WARN, "Magnet is already at maximum field.\n" );
			return;
		}

		ips120_10_mod.sweep_state = SWEEPING_UP;
		ips120_10_mod.activity = TO_SET_POINT;
		ips120_10_mod.target_current = ips120_10_mod.max_current;
		return;
	}

	ips120_10_mod_sweep_up( );
}


/*-----------------------------------------------------*
 * Function for starting a downward sweep of the field
 *-----------------------------------------------------*/

static void
magnet_sweep_down( void )
{
	if ( ! ips120_10_mod.is_sweep_rate )
	{
		print( FATAL, "No sweep rate has been set.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ips120_10_mod.sweep_state == SWEEPING_DOWN )
		{
			print( SEVERE, "Field is already sweeping down.\n" );
			return;
		}

		if ( ips120_10_mod.act_current
			 				<= ips120_10_mod.min_current + CURRENT_RESOLUTION )
		{
			print( WARN, "Magnet is already at minimum field.\n" );
			return;
		}

		ips120_10_mod.sweep_state = SWEEPING_DOWN;
		ips120_10_mod.activity = TO_SET_POINT;
		ips120_10_mod.target_current = ips120_10_mod.min_current;
		return;
	}

	ips120_10_mod_sweep_down( );
}


/*----------------------------------------------------*
 * Function for stopping a running sweep of the field
 *----------------------------------------------------*/

static void
magnet_stop_sweep( void )
{
	if ( ips120_10_mod.sweep_state == STOPPED )
	{
		print( FSC2_MODE == TEST ? SEVERE : WARN,
			   "Sweep is already stopped.\n" );
		return;
	}

	if ( FSC2_MODE == TEST )
	{
		ips120_10_mod.sweep_state = STOPPED;
		ips120_10_mod.activity = HOLD;
		return;
	}

	ips120_10_mod.activity = ips120_10_mod_set_activity( HOLD );
	ips120_10_mod.sweep_state = STOPPED;
}


/*---------------------------------------------*
 * Function to query or set a field sweep rate
 *---------------------------------------------*/

Var_T *
magnet_sweep_rate( Var_T * v )
{
	double sweep_rate;


	if ( v == NULL )
		switch( FSC2_MODE )
		{
			case PREPARATION :
				if ( ! ips120_10_mod.is_sweep_rate )
					no_query_possible( );
				else
					return vars_push( FLOAT_VAR, ips120_10_mod.sweep_rate );

			case TEST :
				return vars_push( FLOAT_VAR, ips120_10_mod.is_sweep_rate ?
								  ips120_10_mod.sweep_rate : TEST_SWEEP_RATE );

			case EXPERIMENT :
				return vars_push( FLOAT_VAR, ips120_10_mod.sweep_rate );
		}

	sweep_rate = ips120_10_mod_sweep_rate_check(
								   get_double( v, "sweep rate" ) / F2C_RATIO );

	if (    ips120_10_mod.is_sweep_rate
		 && sweep_rate == ips120_10_mod.sweep_rate )
		return vars_push( FLOAT_VAR, ips120_10_mod.sweep_rate * F2C_RATIO );

	ips120_10_mod.sweep_rate = sweep_rate;
	ips120_10_mod.is_sweep_rate = SET;

	if ( FSC2_MODE == EXPERIMENT )
		ips120_10_mod_set_sweep_rate( ips120_10_mod.sweep_rate );

	return vars_push( FLOAT_VAR, ips120_10_mod.sweep_rate * F2C_RATIO );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
magnet_reset_field( Var_T * v )
{
	return reset_field( v );
}


/*--------------------------------------------------------*
 * Function to bring the field back to its initial value,
 * i.e. at the very start of the experiment
 *--------------------------------------------------------*/

Var_T *
reset_field( Var_T * v  UNUSED_ARG )
{
	return set_field( vars_push( FLOAT_VAR,
								 ips120_10_mod.start_current * F2C_RATIO) );
}


/*-----------------------------------------------------------*
 * This function was added on Martin Fuchs' request to allow
 * to keep sweeping the magnet to a predefined current after
 * the experiment has ended.
 *-----------------------------------------------------------*/

Var_T *
magnet_goto_field_on_end( Var_T * v )
{
	double cur;


	cur = get_double( v, "final target field" ) / F2C_RATIO;
	ips120_10_mod.final_target_current = ips120_10_mod_current_check( cur );
	ips120_10_mod.goto_field_on_end = SET;

	return vars_push( FLOAT_VAR,
					  ips120_10_mod.final_target_current * F2C_RATIO );
}


/*-------------------------------------------------------------*
 * Function to allow sending a GPIB command string directly to
 * the magent - only use for debugging or testing purposes!
 *-------------------------------------------------------------*/

Var_T *
magnet_command( Var_T * v )
{
	vars_check( v, STR_VAR );

	if ( FSC2_MODE == EXPERIMENT )
	{
		char * volatile cmd = NULL;

		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );

			char reply[ 100 ];
			ips120_10_mod_talk( cmd, reply, sizeof reply );

			T_free( cmd );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( cmd );
			RETHROW;
		}
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static bool
ips120_10_mod_init( const char * name )
{
	char reply[ 100 ];
	long length;
	double cur_limit;
	bool was_hold = UNSET;
	Var_T *func_ptr;
	int acc;


	if ( gpib_init_device( name, &ips120_10_mod.device ) == FAILURE )
	{
		ips120_10_mod.device = -1;
        return FAIL;
	}

	if ( gpib_clear_device( ips120_10_mod.device ) == FAILURE )
		ips120_10_mod_comm_failure( );

	fsc2_usleep( 250000, UNSET );

	/* Bring power supply in remote state */

	ips120_10_mod_talk( "C3\r", reply, sizeof reply );

	/* Set the sweep power supply to send and accept data with extended
	   resolution (this is one of the few commands that don't produce a
	   reply) */

	if ( gpib_write( ips120_10_mod.device, "Q4\r", 3 ) == FAILURE )
		ips120_10_mod_comm_failure( );

	/* Get the status of the magnet - if it's not in the LOC/REMOTE state
	   afterwards something is going wrong... */

	ips120_10_mod_get_complete_status( );
	if ( ips120_10_mod.loc_rem_state != REMOTE_AND_UNLOCKED )
	{
		print( FATAL, "Magnet did not accept command.\n" );
		THROW( EXCEPTION );
	}

	/* Get the maximum and minimum safe current limits and use these as the
	   allowed current range (unless they are larger than the ones set in the
	   configuration file). */

	length = ips120_10_mod_talk( "R21\r", reply, sizeof reply );
	reply[ length - 1 ] = '\0';
	cur_limit = T_atod( reply + 1 );

	if ( cur_limit > MIN_CURRENT )
		ips120_10_mod.min_current = cur_limit;

	length = ips120_10_mod_talk( "R22\r", reply, sizeof reply );

	reply[ length - 1 ] = '\0';
	cur_limit = T_atod( reply + 1 );

	if ( cur_limit < MAX_CURRENT )
		ips120_10_mod.max_current = cur_limit;

	/* Lets make sure the DAC is set to outpuut 0 V */

	func_ptr = func_get( ips120_10_mod.dac_func, &acc );
	vars_push( STR_VAR, DEVICE_NAME );
	vars_push( FLOAT_VAR, 0.0 );
	vars_pop( func_call( func_ptr ) );

	/* Get the actual current and, if the magnet is sweeping, the current
	   sweep direction. If the magnet is running to zero current just stop
	   it. */

	ips120_10_mod.act_current = ips120_10_mod_get_act_current( );
	ips120_10_mod.target_current = ips120_10_mod_get_target_current( );

	switch ( ips120_10_mod.activity )
	{
		case HOLD :
			ips120_10_mod.sweep_state = STOPPED;
			was_hold = SET;
			break;

		case TO_SET_POINT :
			ips120_10_mod.sweep_state =
				ips120_10_mod.act_current < ips120_10_mod.target_current ?
				SWEEPING_UP : SWEEPING_DOWN;
			break;

		case TO_ZERO:
			ips120_10_mod.activity = ips120_10_mod_set_activity( HOLD );
			ips120_10_mod.sweep_state = STOPPED;
			break;
	}

	/* Set the sweep rate if the user defined one, otherwise get the current
	   sweep rate. */

	if ( ips120_10_mod.is_sweep_rate )
		ips120_10_mod.sweep_rate =
					  ips120_10_mod_set_sweep_rate( ips120_10_mod.sweep_rate );
	else
	{
		ips120_10_mod.sweep_rate = ips120_10_mod_get_sweep_rate( );
		ips120_10_mod.is_sweep_rate = SET;
	}

	/* Finally, if a start curent has been set stop the magnet if necessary
	   and set the start current. If the magnet was in HOLD state when we
	   started and no start current had been set use the actual current as the
	   start current. */

	if ( ips120_10_mod.is_start_current )
	{
		if ( ips120_10_mod.sweep_state != STOPPED )
		{
			ips120_10_mod.activity = ips120_10_mod_set_activity( HOLD );
			ips120_10_mod.sweep_state = STOPPED;
		}

		ips120_10_mod.act_current =
					 ips120_10_mod_goto_current( ips120_10_mod.start_current );
	}
	else if ( was_hold )
	{
		ips120_10_mod.start_current = ips120_10_mod.act_current;
		ips120_10_mod.is_start_current = SET;
	}

	return OK;
}


/*-----------------------------------------------------------*
 *-----------------------------------------------------------*/

static void
ips120_10_mod_to_local( void )
{
	char reply[ 100 ];


	/* On Martin Fuchs' request there are now two alternatives: Normally
	   the magnet simply gets stopped when the experiment finishes, but if
	   the function magnet_current_field_on_end() has been called the magnet
	   instead sweeps to the current value passed to the function. */

	if ( ips120_10_mod.goto_field_on_end )
	{
		ips120_10_mod_set_target_current( ips120_10_mod.final_target_current );
		ips120_10_mod_set_sweep_rate( ips120_10_mod.fast_sweep_rate );
		ips120_10_mod_set_activity( TO_SET_POINT );
	}
	else
	{
		ips120_10_mod_set_sweep_rate( ips120_10_mod.fast_sweep_rate );
		ips120_10_mod_set_activity( HOLD );
		ips120_10_mod.sweep_state = STOPPED;
	}

	ips120_10_mod_talk( "C2\r", reply, sizeof reply );

	gpib_local( ips120_10_mod.device );
}


/*-------------------------------------------------------*
 * Function that asks the magnet about its current state
 *-------------------------------------------------------*/

static void
ips120_10_mod_get_complete_status( void )
{
	char reply[ 100 ];
	long len = 100;
	long offset = 0;
	int i, max_retries = 3;


	/* Get all information about the state of the magnet power supply and
	   analyze the reply which has the form "XmnAnCnHnMmnPmn" where m and n
	   are single decimal digits.
	   This transmission seems to have problems when the device was just
	   switched on, so we try to deal with situations gracefully where the
	   device returns data that it isn't supposed to send (at least if we
	   would still be inclined after all these years to believe in what's
	   written in manuals;-) */

	if ( gpib_write( ips120_10_mod.device, "X\r", 2 ) == FAILURE )
		ips120_10_mod_comm_failure( );

	for ( i = 0; i < max_retries; i++ )
	{
		ips120_10_mod_read( reply + offset, &len );

		if ( reply[ 0 ] != 'X' )
		{
			len = 100;
			continue;
		}

		if ( offset + len < 15 )
		{
			offset += len;
			len = 100;
			continue;
		}

		break;
	}

	/* Check system status data (following the 'X') */

	switch ( reply[ 1 ] )
	{
		case '0' :     /* normal */
			break;

		case '1' :     /* quenched */
			print( FATAL, "Magnet claims to be quenched.\n" );
			THROW( EXCEPTION );

		case '2' :     /* overheated */
			print( FATAL, "Magnet claims to be overheated.\n" );
			THROW( EXCEPTION );

		case '4' :     /* warming up */
			print( FATAL, "Magnet claims to be warming up.\n" );
			THROW( EXCEPTION );

		case '8' :     /* fault */
			print( FATAL, "Device signals fault condition.\n" );
			THROW( EXCEPTION );

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	switch ( reply[ 2 ] )
	{
		case '0' :     /* normal */
		case '1' :     /* on positive voltage limit */
		case '2' :     /* on negative voltage limit */
			break;

		case '4' :
			print( FATAL, "Magnet is outside of its positive current "
				   "limit.\n" );
			THROW( EXCEPTION );

		case '8' :
			print( FATAL, "Magnet is outside of its negative current "
				   "limit.\n" );
			THROW( EXCEPTION );

		default :
			print( FATAL, "Recived invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	/* Check activity status (following the 'A') */

	switch ( reply[ 4 ] )
	{
		case '0' :
			ips120_10_mod.activity = HOLD;
			break;

		case '1' :
			ips120_10_mod.activity = TO_SET_POINT;
			break;

		case '2' :
			ips120_10_mod.activity = TO_ZERO;
			break;

		case '4' :
			print( FATAL, "Magnet claims to be clamped.\n" );
			THROW( EXCEPTION );

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	/* Check LOC/REM status (following the 'C') */

	switch ( reply[ 6 ] )
	{
		case '0' :
			ips120_10_mod.loc_rem_state = LOCAL_AND_LOCKED;
			break;

		case '1' :
			ips120_10_mod.loc_rem_state = REMOTE_AND_LOCKED;
			break;

		case '2' :
			ips120_10_mod.loc_rem_state = LOCAL_AND_UNLOCKED;
			break;

		case '3' :
			ips120_10_mod.loc_rem_state = REMOTE_AND_UNLOCKED;
			break;

		case '4' :
		case '5' :
		case '6' :
		case '7' :
			print( FATAL, "Magnet claims to be auto-running down.\n" );
			THROW( EXCEPTION );

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	/* Check switch heater status (following the 'H') */

	switch ( reply[ 8 ] )
	{
		case '0' :
			print( FATAL, "Switch heater is off (at zero field).\n" );
			THROW( EXCEPTION );

		case '1' :     /* swich heater is on */
			break;

		case '2' :
			print( FATAL, "Switch heater is off (at non-zero field).\n" );
			THROW( EXCEPTION );

		case '5' :
			print( FATAL, "Switch heater fault condition.\n" );
			THROW( EXCEPTION );

		case '8' :     /* no switch fitted */
			break;

		default :
			/* The manual claims that the above are the only values we should
			   expect, but as usual the manual is shamelessly lying. At least
			   for the magnet at hand the character 'C' seems to be returned.
			   Because we don't have any better documentation we simply accept
			   whatever the device tells us...

			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
			*/
			break;
	}

	/* Check mode status (following the 'M') */

	switch ( reply[ 10 ] )
	{
		case '0' : case '1' : case '2' : case '3' :
			ips120_10_mod.mode = FAST;
			break;

		case '4' : case '5' : case '6' : case '7' :
			ips120_10_mod.mode = SLOW;
			break;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	switch ( reply[ 11 ] )
	{
		case '0' :
			ips120_10_mod.state = AT_REST;
			break;

		case '1' :
			ips120_10_mod.state = SWEEPING;
			break;

		case '2' :
			ips120_10_mod.state = SWEEP_LIMITING;
			break;

		case '3' :
			ips120_10_mod.state = SWEEPING_AND_SWEEP_LIMITING;
			break;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	/* We don't really care about the polarity status (follwoing the 'P') */

#if 0
	if ( reply[ 13 ] != '0' || reply[ 14 ] != '0' )
	{
		print( FATAL, "Received invalid reply from device.\n" );
		THROW( EXCEPTION );
	}
#endif
}


/*---------------------------------------------------------------*
 * Function for making the magnet sweep up (at least if it's not
 * already sweeping up or very near to the maximum current)
 *---------------------------------------------------------------*/

static void
ips120_10_mod_sweep_up( void )
{
	/* Do nothing except printing a warning when we're alredy sweeping up */

	if ( ips120_10_mod.sweep_state == SWEEPING_UP )
	{
		print( WARN, "Useless command, magnet is already sweeping up.\n" );
		return;
	}

	/* Print a severe warning when the actual current is already very near to
	   the maximum current */

	ips120_10_mod.act_current = ips120_10_mod_get_act_current( );
	if ( ips120_10_mod.act_current >=
		                       ips120_10_mod.max_current - CURRENT_RESOLUTION )
	{
		print( SEVERE, "Magnet is already at maximum field.\n" );
		return;
	}

	if ( ips120_10_mod.activity != HOLD )
		ips120_10_mod.activity = ips120_10_mod_set_activity( HOLD );

	ips120_10_mod.target_current =
				 ips120_10_mod_set_target_current( ips120_10_mod.max_current );
	ips120_10_mod.activity = ips120_10_mod_set_activity( TO_SET_POINT );
	ips120_10_mod.sweep_state = SWEEPING_UP;
}


/*-----------------------------------------------------------------*
 * Function for making the magnet sweep down (at least if it's not
 * already sweeping down or very near to the minimum current)
 *-----------------------------------------------------------------*/

static void
ips120_10_mod_sweep_down( void )
{
	/* Do nothing except printing a warning when we're alredy sweeping down */

	if ( ips120_10_mod.sweep_state == SWEEPING_DOWN )
	{
		print( WARN, "Useless command, magnet is already sweeping down.\n" );
		return;
	}

	/* Print a severe warning when the actual current is already very near to
	   the minimum current */

	ips120_10_mod.act_current = ips120_10_mod_get_act_current( );
	if ( ips120_10_mod.act_current <=
		                       ips120_10_mod.min_current + CURRENT_RESOLUTION )
	{
		print( SEVERE, "Magnet is already at minimum field.\n" );
		return;
	}

	if ( ips120_10_mod.activity != HOLD )
		ips120_10_mod.activity = ips120_10_mod_set_activity( HOLD );
	ips120_10_mod.target_current =
				 ips120_10_mod_set_target_current( ips120_10_mod.min_current );
	ips120_10_mod.activity = ips120_10_mod_set_activity( TO_SET_POINT );
	ips120_10_mod.sweep_state = SWEEPING_DOWN;
}


/*-------------------------------------------------------------*
 * Function that's always called before a new current value is
 * accepted to test if it's within the limits of the magnet.
 * It returns the current value that will be set (which might
 * differ a bit from the one the user requested due to the
 * limited resolution).
 *-------------------------------------------------------------*/

static double
ips120_10_mod_current_check( double current )
{
	double norm_current,
		   dac_current;

	if ( current > ips120_10_mod.max_current )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Field of %f G is too high, maximum field is "
				   "%f G.\n", current * F2C_RATIO,
				   ips120_10_mod.max_current * F2C_RATIO );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Field of %f G is too high, using maximum field of "
				   "%f G instead.\n", current * F2C_RATIO,
				   ips120_10_mod.max_current * F2C_RATIO );
			return MAX_CURRENT;
		}
	}

	if ( current < ips120_10_mod.min_current )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Field of %f G is too low, minimum field is %f G.\n",
				   current * F2C_RATIO,
				   ips120_10_mod.min_current * F2C_RATIO );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Field of %f G is too low, using minimum field of "
				   "%f G instead.\n", current * F2C_RATIO,
				   ips120_10_mod.min_current * F2C_RATIO );
			return ips120_10_mod.min_current;
		}
	}

	/* Maximum current resolution is 10 mA plus what we get from the 5 mV
	   voltage resolution of the DAC. Using that calculate the nearest
	   possible curent we can set and return this to the caller. */

	norm_current = lrnd( current / CURRENT_RESOLUTION ) * CURRENT_RESOLUTION;
	dac_current  = lrnd( ( current - norm_current ) / DAC_CURRENT_RESOLUTION )
		           * DAC_CURRENT_RESOLUTION;

	return norm_current + dac_current;
}


/*-----------------------------------------------------------*
 * Function that's always called before a sweep speed is set
 * to test if it's within the magnets limits.
 *-----------------------------------------------------------*/

static double
ips120_10_mod_sweep_rate_check( double sweep_rate )
{
	if (    sweep_rate > MAX_SWEEP_RATE
		 && ( sweep_rate - MAX_SWEEP_RATE ) / sweep_rate > 0.0001 )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Sweep rate of %f G/s is too high, maximum sweep "
				   "rate is %f G/s.\n",
				   sweep_rate * F2C_RATIO, MAX_SWEEP_RATE * F2C_RATIO );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Sweep rate of %f G/s is too high, using maximum "
				   "sweep rate of %f G/s instead.\n",
				   sweep_rate * F2C_RATIO, MAX_SWEEP_RATE * F2C_RATIO );
			return MAX_SWEEP_RATE;
		}
	}

	if (    sweep_rate < MIN_SWEEP_RATE
		 && ( MIN_SWEEP_RATE - sweep_rate ) / MIN_SWEEP_RATE > 0.0001 )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Sweep rate of %f mG/s is too low, minimum sweep "
				   "rate is %f mG/s.\n", sweep_rate * 1.0e3 * F2C_RATIO,
				   MIN_SWEEP_RATE * 1.0e3 * F2C_RATIO );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Sweep rate of %f mG/s is too low, using minimum "
				   "sweep rate of %f mG/s instead.\n",
					sweep_rate * 1.0e3 * F2C_RATIO,
				   MIN_SWEEP_RATE * 1.0e3 * F2C_RATIO );
			return MIN_SWEEP_RATE;
		}
	}

	/* Minimum sweep speed resolution is 10 mA/min */

	return ( lrnd( ( 60.0 * sweep_rate ) / MIN_SWEEP_RATE ) * MIN_SWEEP_RATE )
		   / 60.0;
}


/*--------------------------------------------------*
 * Function for asking the magnet about the current
 *--------------------------------------------------*/

static double
ips120_10_mod_get_act_current( void )
{
	char reply[ 100 ];
	long length;
	double norm_current,
		   dac_current;
	int acc;
	Var_T *v;


	/* Get the current the power supply is set to (which gets returned in
	   units of the current resolution) */

	length = ips120_10_mod_talk( "R0\r", reply, sizeof reply );
	reply[ length - 1 ] = '\0';
	norm_current = T_atod( reply + 1 ) * CURRENT_RESOLUTION;

	/* Get the additional current due to the setting of the DAC */

	v = func_call( func_get( ips120_10_mod.dac_func, &acc ) );
	dac_current = v->val.dval * C2V_RATIO;
	vars_pop( v );

	return norm_current + dac_current;
}


/*-----------------------------------------------------------*
 * Function for setting the target current, i.e. the current
 * the magnet is supposed to sweep to.
 *-----------------------------------------------------------*/

static double
ips120_10_mod_set_target_current( double current )
{
	char cmd[ 30 ];
	char reply[ 100 ];
	double norm_current,
		   dac_current;
	Var_T *func_ptr;
	int acc;


	current = ips120_10_mod_current_check( current );

	/* Split the current up into the part that can be set directly and the
	   additional one induced by the DAC */

	norm_current = lrnd( current / CURRENT_RESOLUTION ) * CURRENT_RESOLUTION;
	dac_current  = lrnd( ( current - norm_current ) / DAC_CURRENT_RESOLUTION )
		           * DAC_CURRENT_RESOLUTION;

	/* Set the power supplies current - please note that it expects the
	   current as an integer in units of 0.01 A (which just seems to be
	   the current resolution). */

	sprintf( cmd, "I%5ld\r", lrnd( norm_current / CURRENT_RESOLUTION ) );
	ips120_10_mod_talk( cmd, reply, sizeof reply );

	/* And set the DAC voltage */

	func_ptr = func_get( ips120_10_mod.dac_func, &acc );
	vars_push( STR_VAR, DEVICE_NAME );
	vars_push( FLOAT_VAR, dac_current / C2V_RATIO );
	vars_pop( func_call( func_ptr ) );

	return current;
}


/*---------------------------------------------------------------*
 * Function for asking the current setting of the target current
 *---------------------------------------------------------------*/

static double
ips120_10_mod_get_target_current( void )
{
	char reply[ 100 ];
	long length;
	double norm_current,
		   dac_current;
	Var_T *v;
	int acc;


	length = ips120_10_mod_talk( "R5\r", reply, sizeof reply );
	reply[ length - 1 ] = '\0';
	norm_current = T_atod( reply + 1 );

	/* Get the additional current due to the setting of the DAC */

	v = func_call( func_get( ips120_10_mod.dac_func, &acc ) );
	dac_current = v->val.dval * C2V_RATIO;
	vars_pop( v );

	return norm_current + dac_current;
}


/*---------------------------------------*
 * Function for setting a new sweep rate
 *---------------------------------------*/

static double
ips120_10_mod_set_sweep_rate( double sweep_rate )
{
	char cmd[ 30 ];
	char reply[ 100 ];


	sweep_rate = ips120_10_mod_sweep_rate_check( sweep_rate );

	/* Please note: the sweep rate must be set in units of 0.01 A/min
	   (the minimum sweep rate) while the variable 'sweep_rate' is the
	   sweep rate in A/s. */

	sprintf( cmd, "S%.5ld\r", lrnd( sweep_rate / MIN_SWEEP_RATE ) );
	ips120_10_mod_talk( cmd, reply, sizeof reply );

	return sweep_rate;
}


/*-------------------------------------------------------------*
 * Function for asking the magnet about its current sweep rate
 *-------------------------------------------------------------*/

static double
ips120_10_mod_get_sweep_rate( void )
{
	char reply[ 100 ];
	long length;


	length = ips120_10_mod_talk( "R6\r", reply, sizeof reply );
	reply[ length - 1 ] = '\0';

	/* Please note: it looks as if the the sweep rate gets returned in units
	   of 0.01 A/min (the minimum sweep rate) while the function returns the
	   sweep rate in A/s */

	return T_atol( reply + 1 ) * MIN_SWEEP_RATE;
}


/*-----------------------------------------------------------*
 * Function to move the magnet to a certain currrent setting
 * as fast as savely possible. Can be aborted in which case
 * a running sweep will be stopped by the end-of-experiment
 * handler function that's then invoked automatically.
 *-----------------------------------------------------------*/

static double
ips120_10_mod_goto_current( double current )
{
	ips120_10_mod_set_target_current( current );
	ips120_10_mod_set_sweep_rate( ips120_10_mod.fast_sweep_rate );

	if ( ips120_10_mod.activity != TO_SET_POINT )
		ips120_10_mod.activity = ips120_10_mod_set_activity( TO_SET_POINT );

	while ( lrnd( ( current -  ips120_10_mod_get_act_current( ) ) /
				  CURRENT_RESOLUTION ) != 0 )
	{
		fsc2_usleep( 50000, UNSET );
		stop_on_user_request( );
	}

	ips120_10_mod.activity = ips120_10_mod_set_activity( HOLD );
	ips120_10_mod.sweep_state = STOPPED;

	if ( ips120_10_mod.is_sweep_rate )
		ips120_10_mod_set_sweep_rate( ips120_10_mod.sweep_rate );

	return ips120_10_mod_get_act_current( );
}


/*-------------------------------------------------------------*
 * Function to either stop a sweep, make the magnet sweep to a
 * certain field value or make it sweep to zero.
 *-------------------------------------------------------------*/

static int
ips120_10_mod_set_activity( int activity )
{
	char cmd[ 20 ];
	char reply[ 100 ];
	int act;


	switch ( activity )
	{
		case HOLD :
			act = 0;
			break;

		case TO_SET_POINT :
			act = 1;
			break;

		case TO_ZERO :
			act = 2;
			break;

		default :
			print( FATAL, "Internal error detected at %s:%d,\n",
				   __FILE__, __LINE__ );
			THROW( EXCEPTION );
	}

	sprintf( cmd, "A%1d\r", act );
	ips120_10_mod_talk( cmd, reply, sizeof reply );

	return activity;
}


/*---------------------------------------------------*
 * Function for talking with the magnet via the GPIB
 *---------------------------------------------------*/

static long
ips120_10_mod_talk( const char * message,
					char *       reply,
					long         length )
{
	long len = length;
	int retries = MAX_RETRIES;


 start:

	if ( gpib_write( ips120_10_mod.device, message, strlen( message ) ) ==
		 															  FAILURE )
		ips120_10_mod_comm_failure( );

 retry_read:

	stop_on_user_request( );

	len = length;
	ips120_10_mod_read( reply, &len );

	/* If the device misunderstood the command send it again, repeat up to
	   MAX_RETRIES times */

	if ( reply[ 0 ] == '?' )
	{
		if ( retries-- )
			goto start;
		else
			ips120_10_mod_comm_failure( );
	}

	/* If the first character of the reply isn't equal to the first character
	   of the message we probably read the reply for a previous command and
	   try to read again... */

	if ( reply[ 0 ] != message[ 0 ] )
	{
		if ( retries-- )
			goto retry_read;
		else
			ips120_10_mod_comm_failure( );
	}

	return len;
}


/*-------------------------------------------------------------*
 * Function to read a message from the device single character
 * mode from the GPV24 RS323C to IEEE488 converter
 *-------------------------------------------------------------*/

static void
ips120_10_mod_read( char * buffer,
					long * len )
{
	char *bp = buffer;
	long count = 0;
	long to_read;
	long fails = 0;


	while ( count < *len )
	{
		to_read = 1;
		if ( gpib_read( ips120_10_mod.device, bp, &to_read ) == FAILURE )
			ips120_10_mod_comm_failure( );

		/* If no new character is available (i.e. the call returned a '\0'
		   character) sleep a bit until the next one should be available
		   (the baud rate of 9600 should result in about one character per
		   ms getting delivered). Make it possible to get out of here on
		   user request (i.e. when the user hits the STOP button). */

		if ( *bp == '\0' )
		{
			stop_on_user_request( );
			fsc2_usleep( 1000, SET );

			/* During initialization the user can't press the "Stop" button,
			   so we have to stop automatically if the device returns too
			   many '\0' (in that case the power supply os either off or the
			   GPV24 converter has to be powercycled) */

			if ( ips120_10_mod.in_init_phase && ++fails > 10 * MAX_RETRIES )
				ips120_10_mod_comm_failure( );

			continue;
		}

		fails = 0;

		count++;

		/* If the device sends a carriage return we got the whole message */

		if ( *bp++ == '\r' )
			break;
	}

	*len = count;
}


/*---------------------------------------------------*
 * Function called in case of communication failures
 *---------------------------------------------------*/

static void
ips120_10_mod_comm_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
