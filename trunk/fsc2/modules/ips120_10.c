/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "ips120_10.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define TEST_CURRENT       63.0      /* about 9 T */
#define TEST_SWEEP_RATE    1.0e-3    /* about 1.5 G/s */


/* How many time we retry when an error happens during communication with
   the device */

#define MAX_RETRIES        10

/* Declaration of exported functions */

int ips120_20_init_hook( void );
int ips120_20_test_hook( void );
int ips120_20_exp_hook( void );
int ips120_20_end_of_exp_hook( void );
void ips120_20_exit_hook( void );

Var *magnet_name( Var *v );
Var *magnet_setup( Var *v );
Var *get_field( Var *v );
Var *set_field( Var *v );
Var *set_sweep_rate( Var *v );
Var *magnet_sweep( Var *v );
Var *magnet_sweep_rate( Var *v );
Var *reset_field( Var *v );
Var *magnet_goto_field_on_end( Var *v );
Var *magnet_command( Var *v );


static void magnet_sweep_up( void );
static void magnet_sweep_down( void );
static void magnet_stop_sweep( void );

static bool ips120_20_init( const char *name );
static void ips120_20_to_local( void );
static void ips120_20_get_complete_status( void );
static void ips120_20_sweep_up( void );
static void ips120_20_sweep_down( void );
static double ips120_20_current_check( double current );
static double ips120_20_sweep_rate_check( double sweep_rate );
static double ips120_20_get_act_current( void );
static double ips120_20_current_check( double current );
static double ips120_20_sweep_rate_check( double sweep_rate );
static double ips120_20_set_target_current( double current );
static double ips120_20_get_target_current( void );
static double ips120_20_set_sweep_rate( double sweep_rate );
static double ips120_20_get_sweep_rate( void );
static double ips120_20_goto_current( double current );
static int ips120_20_set_activity( int activity );
static long ips120_20_talk( const char *message, char *reply, long length );
static void ips120_20_comm_failure( void );


#define MESSAGE_AVAILABLE 0x10


struct IPS120_20 {
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


static struct IPS120_20 ips120_20, ips120_20_stored;


/*-----------------------------------*/
/* Init hook function for the module */
/*-----------------------------------*/

int ips120_20_init_hook( void )
{
	need_GPIB = SET;

	ips120_20.device = -1;

	ips120_20.act_current = 0.0;

	ips120_20.is_start_current = UNSET;
	ips120_20.is_sweep_rate = UNSET;

	ips120_20.fast_sweep_rate = FAST_SWEEP_RATE;

	ips120_20.max_current = MAX_CURRENT;
	ips120_20.min_current = MIN_CURRENT;

	ips120_20.goto_field_on_end = UNSET;
	return 1;
}


/*-----------------------------------*/
/* Test hook function for the module */
/*-----------------------------------*/

int ips120_20_test_hook( void )
{
	ips120_20_stored = ips120_20;

	if ( ips120_20.is_start_current )
		ips120_20.act_current = ips120_20.start_current;
	else
	{
		ips120_20.start_current = ips120_20.act_current = TEST_CURRENT;
		ips120_20.is_start_current = SET;
	}

	if ( ! ips120_20.is_sweep_rate )
	{
		ips120_20.sweep_rate = TEST_SWEEP_RATE;
		ips120_20.is_sweep_rate = SET;
	}

	ips120_20.activity = HOLD;
	ips120_20.sweep_state = STOPPED;

	ips120_20.time_estimate = experiment_time( );

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int ips120_20_exp_hook( void )
{
	ips120_20 = ips120_20_stored;

	if ( ! ips120_20_init( DEVICE_NAME ) )
	{
		print( FATAL, "Initialization of device failed: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int ips120_20_end_of_exp_hook( void )
{
	ips120_20_to_local( );
	ips120_20 = ips120_20_stored;
	ips120_20.device = -1;

	return 1;
}


/*-----------------------------------------*/
/* Called before device driver is unloaded */
/*-----------------------------------------*/

void ips120_20_exit_hook( void )
{
	if ( ips120_20.device >= 0 )
		ips120_20_end_of_exp_hook( );
}


/*---------------------------------------------------*/
/* Function returns the name the device is known as. */
/*---------------------------------------------------*/

Var *magnet_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*/
/* Function for registering the start current and the sweep rate. */
/*----------------------------------------------------------------*/

Var *magnet_setup( Var *v )
{
	double cur;
	double sweep_rate;


	if ( v->next == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	cur = get_double( v, "field" ) / F2C_RATIO;
	ips120_20.start_current = ips120_20_current_check( cur );
	ips120_20.is_start_current = SET;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		sweep_rate = get_double( v, "magnet sweep speed" ) / F2C_RATIO;
		if ( sweep_rate < 0 )
		{
			print( FATAL, "Negative sweep rates can't be used, use argument "
				   "to magnet_sweep() to set sweep direction.\n" );
			THROW( EXCEPTION );
		}

		ips120_20.sweep_rate = ips120_20_sweep_rate_check( sweep_rate );
		ips120_20.is_sweep_rate = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------------------------*/
/* Function returns the momentary field. During the test run an estimate */
/* of the field is returned, based on the sweep rate and a guess of the  */
/* time spent since the last call for determining the field.             */
/*-----------------------------------------------------------------------*/

Var *get_field( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( FSC2_MODE == TEST )
	{
		/* During the test run we need to return some not completely bogus
		   value when a sweep is run. Thus an estimate for the time spend
		   until now is fetched, multiplied by the sweep rate and added to
		   the current field */

		if ( ips120_20.sweep_state != STOPPED &&
			 ips120_20.activity == TO_SET_POINT )
		{
			double cur_time, dtime;

			cur_time = experiment_time( );
			dtime = cur_time - ips120_20.time_estimate;
			ips120_20.time_estimate = cur_time;

			ips120_20.act_current = 1.0e-4 *
				( double ) lrnd( 1.0e4 * ( ips120_20.act_current
								 + experiment_time( ) * ips120_20.sweep_rate
								 * ( ips120_20.sweep_state == SWEEPING_UP ?
									 1.0 : - 1.0 ) ) );

			if ( ips120_20.act_current > ips120_20.max_current )
				ips120_20.act_current = ips120_20.max_current;
			if ( ips120_20.act_current < ips120_20.min_current )
				ips120_20.act_current = ips120_20.min_current;
		}
	}
	else
		ips120_20.act_current = ips120_20_get_act_current( );

	/* If a sweep reached one of the current limits stop the sweep */

	if ( ( ( ips120_20.sweep_state == SWEEPING_UP ||
			 ips120_20.activity == TO_SET_POINT ) &&
		   ips120_20.act_current >= ips120_20.max_current ) ||
		 ( ( ips120_20.sweep_state == SWEEPING_DOWN ||
			 ips120_20.activity == TO_SET_POINT ) &&
		   ips120_20.act_current <= ips120_20.min_current ) )
	{
		print( WARN, "Sweep had to be stopped because current limit was "
			   "reached.\n" );

		if ( FSC2_MODE == EXPERIMENT )
			ips120_20.activity = ips120_20_set_activity( HOLD );
		else
			ips120_20.activity = HOLD;
		ips120_20.sweep_state = STOPPED;
	}

	return vars_push( FLOAT_VAR, ips120_20.act_current * F2C_RATIO );
}


/*------------------------------------------------------*/
/* Function for setting a new field value. Please note  */
/* that setting a new field also stops a running sweep. */
/*------------------------------------------------------*/

Var *set_field( Var *v )
{
	double cur;


	/* Stop sweeping */

	if ( ips120_20.sweep_state != STOPPED ||
		 ips120_20.activity != HOLD )
	{
		if ( FSC2_MODE == EXPERIMENT )
			ips120_20.activity = ips120_20_set_activity( HOLD );
		else
			ips120_20.activity = HOLD;
		ips120_20.sweep_state = STOPPED;
	}

	/* Check the current */

	cur = ips120_20_current_check( get_double( v, "field" ) / F2C_RATIO );

	if ( FSC2_MODE == EXPERIMENT )
		cur = ips120_20_goto_current( cur );

	ips120_20.act_current = cur;

	return vars_push( FLOAT_VAR, ips120_20.act_current * F2C_RATIO );
}


/*-------------------------------------------------------------*/
/* This is the EDL function for starting or stopping sweeps or */
/* inquiring about the current sweep state.                    */
/*-------------------------------------------------------------*/

Var *magnet_sweep( Var *v )
{
	long dir;
	Var *vc;


	if ( v == NULL || FSC2_MODE == TEST )
	{
		vc = get_field( NULL );
		ips120_20.act_current = vc->val.dval / F2C_RATIO;
		vars_pop( vc );
	}

	if ( v == NULL )
		switch ( ips120_20.sweep_state )
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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void magnet_sweep_up( void )
{
	if ( ! ips120_20.is_sweep_rate )
	{
		print( FATAL, "No sweep rate has been set.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ips120_20.sweep_state == SWEEPING_UP )
		{
			print( SEVERE, "Field is already sweeping up.\n" );
			return;
		}

		if ( ips120_20.act_current >=
			 					   ips120_20.max_current - CURRENT_RESOLUTION )
		{
			print( WARN, "Magnet is already at maximum field.\n" );
			return;
		}

		ips120_20.sweep_state = SWEEPING_UP;
		ips120_20.activity = TO_SET_POINT;
		ips120_20.target_current = ips120_20.max_current;
		return;
	}

	ips120_20_sweep_up( );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void magnet_sweep_down( void )
{
	if ( ! ips120_20.is_sweep_rate )
	{
		print( FATAL, "No sweep rate has been set.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ips120_20.sweep_state == SWEEPING_DOWN )
		{
			print( SEVERE, "Field is already sweeping down.\n" );
			return;
		}

		if ( ips120_20.act_current
			 					<= ips120_20.min_current + CURRENT_RESOLUTION )
		{
			print( WARN, "Magnet is already at minimum field.\n" );
			return;
		}

		ips120_20.sweep_state = SWEEPING_DOWN;
		ips120_20.activity = TO_SET_POINT;
		ips120_20.target_current = ips120_20.min_current;
		return;
	}

	ips120_20_sweep_down( );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void magnet_stop_sweep( void )
{
	if ( ips120_20.sweep_state == STOPPED )
	{
		print( FSC2_MODE == TEST ? SEVERE : WARN,
			   "Sweep is already stopped.\n" );
		return;
	}

	if ( FSC2_MODE == TEST )
	{
		ips120_20.sweep_state = STOPPED;
		ips120_20.activity = HOLD;
		return;
	}

	ips120_20.activity = ips120_20_set_activity( HOLD );
	ips120_20.sweep_state = STOPPED;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *magnet_sweep_rate( Var *v )
{
	double sweep_rate;


	if ( v == NULL )
		switch( FSC2_MODE )
		{
			case PREPARATION :
				if ( ! ips120_20.is_sweep_rate )
					no_query_possible( );
				else
					return vars_push( FLOAT_VAR, ips120_20.sweep_rate );

			case TEST :
				return vars_push( FLOAT_VAR, ips120_20.is_sweep_rate ?
								  ips120_20.sweep_rate : TEST_SWEEP_RATE );

			case EXPERIMENT :
				return vars_push( FLOAT_VAR, ips120_20.sweep_rate );
		}

	sweep_rate = ips120_20_sweep_rate_check(
								   get_double( v, "sweep rate" ) / F2C_RATIO );
	if ( ips120_20.is_sweep_rate && sweep_rate == ips120_20.sweep_rate )
		return vars_push( FLOAT_VAR, ips120_20.sweep_rate * F2C_RATIO );

	ips120_20.sweep_rate = sweep_rate;
	ips120_20.is_sweep_rate = SET;

	if ( FSC2_MODE == EXPERIMENT )
		ips120_20_set_sweep_rate( ips120_20.sweep_rate );

	return vars_push( FLOAT_VAR, ips120_20.sweep_rate * F2C_RATIO );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *reset_field( Var *v )
{
	UNUSED_ARGUMENT( v );
	return set_field( vars_push( FLOAT_VAR,
								 ips120_20.start_current * F2C_RATIO) );
}


/*-----------------------------------------------------------*/
/* This function was added on Martin Fuchs' request to allow */
/* to keep sweeping the magnet to a predefined current after */
/* the experiment has ended.                                 */
/*-----------------------------------------------------------*/

Var *magnet_goto_field_on_end( Var *v )
{
	double cur;


	cur = get_double( v, "final target field" ) / F2C_RATIO;
	ips120_20.final_target_current = ips120_20_current_check( cur );
	ips120_20.goto_field_on_end = SET;

	return vars_push( FLOAT_VAR, ips120_20.final_target_current * F2C_RATIO );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *magnet_command( Var *v )
{
	char *cmd = NULL;
	char reply[ 100 ];


	CLOBBER_PROTECT( cmd );

	vars_check( v, STR_VAR );
	
	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
			ips120_20_talk( cmd, reply, 100 );
			T_free( cmd );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( cmd );
			RETHROW( );
		}
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool ips120_20_init( const char *name )
{
	char reply[ 100 ];
	long length;
	double cur_limit;
	bool was_hold = UNSET;


	if ( gpib_init_device( name, &ips120_20.device ) == FAILURE )
	{
		ips120_20.device = -1;
        return FAIL;
	}

	if ( gpib_clear_device( ips120_20.device ) == FAILURE )
		ips120_20_comm_failure( );

	fsc2_usleep( 250000, UNSET );

	/* Bring power supply in remote state */

	ips120_20_talk( "C3\r", reply, 100 );

	/* Set the sweep power supply to send and accept data with extended
	   resolution (this is one of the few commands that don't produce a
	   reply) */

	if ( gpib_write( ips120_20.device, "Q4\r", 3 ) == FAILURE )
		ips120_20_comm_failure( );

	/* Get the status of the magnet - if it's not in the LOC/REMOTE state we
	   set it to something is going wrong... */

	ips120_20_get_complete_status( );
	if ( ips120_20.loc_rem_state != REMOTE_AND_UNLOCKED )
	{
		print( FATAL, "Magnet did not accept command.\n" );
		THROW( EXCEPTION );
	}

	/* Get the maximum and minimum safe current limits and use these as the
	   allowed current range (unless they are larger than the ones set in the
	   configuration file). */

	length = ips120_20_talk( "R21\r", reply, 100 );
	reply[ length - 1 ] = '\0';
	cur_limit = T_atod( reply + 1 );

	if ( cur_limit > MIN_CURRENT )
		ips120_20.min_current = cur_limit;

	length = ips120_20_talk( "R22\r", reply, 100 );

	reply[ length - 1 ] = '\0';
	cur_limit = T_atod( reply + 1 );

	if ( cur_limit < MAX_CURRENT )
		ips120_20.max_current = cur_limit;

	/* Get the actual current, and if the magnet is sweeping, the current
	   sweep direction. If the magnet is running to zero current just stop
	   it. */

	ips120_20.act_current = ips120_20_get_act_current( );
	ips120_20.target_current = ips120_20_get_target_current( );

	switch ( ips120_20.activity )
	{
		case HOLD :
			ips120_20.sweep_state = STOPPED;
			was_hold = SET;
			break;

		case TO_SET_POINT :
			ips120_20.sweep_state =
				ips120_20.act_current < ips120_20.target_current ?
				SWEEPING_UP : SWEEPING_DOWN;
			break;

		case TO_ZERO:
			ips120_20.activity = ips120_20_set_activity( HOLD );
			ips120_20.sweep_state = STOPPED;
			break;
	}

	/* Set the sweep rate if the user defined one, otherwise get the current
	   sweep rate. */

	if ( ips120_20.is_sweep_rate )
		ips120_20.sweep_rate =
							  ips120_20_set_sweep_rate( ips120_20.sweep_rate );
	else
	{
		ips120_20.sweep_rate = ips120_20_get_sweep_rate( );
		ips120_20.is_sweep_rate = SET;
	}

	/* Finally, if a start curent has been set stop the magnet if necessary
	   and set the start current. If the magnet was in HOLD state when we
	   started and no start current had been set use the actual current as the
	   start current. */

	if ( ips120_20.is_start_current )
	{
		if ( ips120_20.sweep_state != STOPPED )
		{
			ips120_20.activity = ips120_20_set_activity( HOLD );
			ips120_20.sweep_state = STOPPED;
		}

		ips120_20.act_current =
							 ips120_20_goto_current( ips120_20.start_current );
	}
	else if ( was_hold )
	{
		ips120_20.start_current = ips120_20.act_current;
		ips120_20.is_start_current = SET;
	}

	return OK;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ips120_20_to_local( void )
{
	char reply[ 100 ];


	/* On Martin Fuchs' request there are now two alternatives: Normally
	   the magnet simply gets stopped when the experiment finishes, but if
	   the function magnet_current_field_on_end() has been called the magnet
	   instead sweeps to the current value passed to the function. */

	if ( ips120_20.goto_field_on_end )
	{
		ips120_20_set_target_current( ips120_20.final_target_current );
		ips120_20_set_sweep_rate( ips120_20.fast_sweep_rate );
		ips120_20_set_activity( TO_SET_POINT );
	}
	else
	{	
		ips120_20_set_activity( HOLD );
		ips120_20.sweep_state = STOPPED;
	}

	ips120_20_talk( "C0\r", reply, 100 );

	gpib_local( ips120_20.device );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ips120_20_get_complete_status( void )
{
	char reply[ 100 ];
	long len = 100;
	long offset = 0;
	int i, max_retries = 3;


	/* Get all information about the state of the magnet power supply and
	   analyze the reply which has the form "XmnAnCnMmnPmn" where m and n
	   are single decimal digits.
	   This transmission seems to have problems when the device was just
	   switched on, so we try to deal with situations gracefully where the
	   device returns data that it isn't supposed to send (at least if we
	   would still be inclined after all these years to believe in what's
	   written in manuals ;-) */

	if ( gpib_write( ips120_20.device, "X\r", 2 ) == FAILURE )
		ips120_20_comm_failure( );

	for ( i = 0; i < max_retries; i++ )
	{
		if ( gpib_read( ips120_20.device, reply + offset, &len ) == FAILURE )
			ips120_20_comm_failure( );

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
	
	/* Check system status data */

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
			print( FATAL, "Magnet is outside positive current limit.\n" );
			THROW( EXCEPTION );

		case '8' :
			print( FATAL, "Magnet is outside negative current limit.\n" );
			THROW( EXCEPTION );

		default :
			print( FATAL, "Recived invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	/* Check activity status */

	switch ( reply[ 4 ] )
	{
		case '0' :
			ips120_20.activity = HOLD;
			break;

		case '1' :
			ips120_20.activity = TO_SET_POINT;
			break;

		case '2' :
			ips120_20.activity = TO_ZERO;
			break;

		case '4' :
			print( FATAL, "Magnet claims to be clamped.\n" );
			THROW( EXCEPTION );

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	/* Check LOC/REM status */

	switch ( reply[ 6 ] )
	{
		case '0' :
			ips120_20.loc_rem_state = LOCAL_AND_LOCKED;
			break;

		case '1' :
			ips120_20.loc_rem_state = REMOTE_AND_LOCKED;
			break;

		case '2' :
			ips120_20.loc_rem_state = LOCAL_AND_UNLOCKED;
			break;

		case '3' :
			ips120_20.loc_rem_state = REMOTE_AND_UNLOCKED;
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

	/* Check switch heater status */

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
			   for the current magnet the character 'C' seems to be returned.
			   Because we don't have any better documentation we simply accept
			   whatever the device tells us...

			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
			*/
			break;
	}

	/* Check mode status */

	switch ( reply[ 10 ] )
	{
		case '0' :
			ips120_20.mode = FAST;
			break;

		case '4' :
			ips120_20.mode = SLOW;
			break;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	switch ( reply[ 11 ] )
	{
		case '0' :
			ips120_20.state = AT_REST;
			break;

		case '1' :
			ips120_20.state = SWEEPING;
			break;

		case '2' :
			ips120_20.state = SWEEP_LIMITING;
			break;

		case '4' :
			ips120_20.state = SWEEPING_AND_SWEEP_LIMITING;
			break;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	/* The polarity status bytes are always '0' according to the manual,
	 but as usual the manual is lying, the device sends '7' or '2' and '0'.
	 Due to lack of better documentation we simply ignore the P field...

	if ( reply[ 13 ] != '0' || reply[ 14 ] != '0' )
	{
		print( FATAL, "Received invalid reply from device.\n" );
		THROW( EXCEPTION );
	}

	*/
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ips120_20_sweep_up( void )
{
	/* Do nothing except printing a warning when we're alredy sweeping up */

	if ( ips120_20.sweep_state == SWEEPING_UP )
	{
		print( WARN, "Useless command, magnet is already sweeping up.\n" );
		return;
	}

	/* Print a severe warning when the actual current is already very near to
	   the maximum current */

	ips120_20.act_current = ips120_20_get_act_current( );
	if ( ips120_20.act_current >= ips120_20.max_current - CURRENT_RESOLUTION )
	{
		print( SEVERE, "Magnet is already at maximum field.\n" );
		return;
	}

	if ( ips120_20.activity != HOLD )
		ips120_20.activity = ips120_20_set_activity( HOLD );

	ips120_20.target_current =
						 ips120_20_set_target_current( ips120_20.max_current );
	ips120_20.activity = ips120_20_set_activity( TO_SET_POINT );
	ips120_20.sweep_state = SWEEPING_UP;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ips120_20_sweep_down( void )
{
	/* Do nothing except printing a warning when we're alredy sweeping down */

	if ( ips120_20.sweep_state == SWEEPING_DOWN )
	{
		print( WARN, "Useless command, magnet is already sweeping down.\n" );
		return;
	}

	/* Print a severe warning when the actual current is already very near to
	   the minimum current */

	ips120_20.act_current = ips120_20_get_act_current( );
	if ( ips120_20.act_current <= ips120_20.min_current + CURRENT_RESOLUTION )
	{
		print( SEVERE, "Magnet is already at minimum field.\n" );
		return;
	}

	if ( ips120_20.activity != HOLD )
		ips120_20.activity = ips120_20_set_activity( HOLD );
	ips120_20.target_current =
						 ips120_20_set_target_current( ips120_20.min_current );
	ips120_20.activity = ips120_20_set_activity( TO_SET_POINT );
	ips120_20.sweep_state = SWEEPING_DOWN;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips120_20_current_check( double current )
{
	if ( current > ips120_20.max_current )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Field of %f G is too high, maximum field is "
				   "%f G.\n",
				   current * F2C_RATIO, ips120_20.max_current * F2C_RATIO );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Field of %f G is too high, using maximum field of "
				   "%f G instead.\n",
				   current * F2C_RATIO, ips120_20.max_current * F2C_RATIO );
			return MAX_CURRENT;
		}
	}

	if ( current < ips120_20.min_current )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Field of %f G is too low, minimum field is %f G.\n",
				   current * F2C_RATIO, ips120_20.min_current * F2C_RATIO );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Field of %f G is too low, using minimum field of "
				   "%f G instead.\n",
				   current * F2C_RATIO, ips120_20.min_current * F2C_RATIO );
			return ips120_20.min_current;
		}
	}

	/* Maximum current resolution is 0.1 mA */

	return lrnd( current / CURRENT_RESOLUTION ) * CURRENT_RESOLUTION;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips120_20_sweep_rate_check( double sweep_rate )
{
	if ( sweep_rate > MAX_SWEEP_RATE )
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

	if ( sweep_rate < MIN_SWEEP_RATE )
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

	/* Minimum sweep speed resolution is 10 mA/s */

	return lrnd( sweep_rate / MIN_SWEEP_RATE ) * MIN_SWEEP_RATE;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips120_20_get_act_current( void )
{
	char reply[ 100 ];
	long length;


	length = ips120_20_talk( "R0\r", reply, 100 );

	reply[ length - 1 ] = '\0';
	return T_atod( reply + 1 );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips120_20_set_target_current( double current )
{
	char cmd[ 30 ];
	char reply[ 100 ];


	current = ips120_20_current_check( current );
	sprintf( cmd, "I%.4f\r", current );
	ips120_20_talk( cmd, reply, 100 );

	return current;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips120_20_get_target_current( void )
{
	char reply[ 100 ];
	long length;


	length = ips120_20_talk( "R5\r", reply, 100 );
	reply[ length - 1 ] = '\0';
	return T_atod( reply + 1 );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips120_20_set_sweep_rate( double sweep_rate )
{
	char cmd[ 30 ];
	char reply[ 100 ];


	sweep_rate = ips120_20_sweep_rate_check( sweep_rate );
	sprintf( cmd, "S%.3f\r", sweep_rate * 60.0 );
	ips120_20_talk( cmd, reply, 100 );

	return sweep_rate;
}

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips120_20_get_sweep_rate( void )
{
	char reply[ 100 ];
	long length;


	length = ips120_20_talk( "R6\r", reply, 100 );
	reply[ length - 1 ] = '\0';
	return T_atod( reply + 1 ) / 60.0;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips120_20_goto_current( double current )
{
	ips120_20_set_target_current( current );
	ips120_20_set_sweep_rate( ips120_20.fast_sweep_rate );

	if ( ips120_20.activity != TO_SET_POINT )
		ips120_20.activity = ips120_20_set_activity( TO_SET_POINT );

	while ( lrnd( ( current -  ips120_20_get_act_current( ) ) /
				  CURRENT_RESOLUTION ) != 0 )
	{
		fsc2_usleep( 50000, UNSET );
		stop_on_user_request( );
	}

	ips120_20.activity = ips120_20_set_activity( HOLD );
	ips120_20.sweep_state = STOPPED;

	if ( ips120_20.is_sweep_rate )
		ips120_20_set_sweep_rate( ips120_20.sweep_rate );

	return ips120_20_get_act_current( );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static int ips120_20_set_activity( int activity )
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
	ips120_20_talk( cmd, reply, 100 );

	return activity;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static long ips120_20_talk( const char *message, char *reply, long length )
{
	long len = length;
	int retries = MAX_RETRIES;


 start:

	if ( gpib_write( ips120_20.device, message, strlen( message ) ) ==
		 															  FAILURE )
		ips120_20_comm_failure( );

	/* Re-enable the following if you want to be extremely careful (that's
	   what the manual recommends), but even the LabVIEW driver written by
	   Oxford doesn't use it... */

#if 0
	do {
		unsigned char stb;

		stop_on_user_request( );

		fsc2_usleep( 500, UNSET );

		if ( gpib_serial_poll( ips120_20.device, &stb ) == FAILURE )
			ips120_20_comm_failure( );
	} while ( ! ( stb & MESSAGE_AVAILABLE ) );
#endif

 reread:

	stop_on_user_request( );

	len = length;
	if ( gpib_read( ips120_20.device, reply, &len ) == FAILURE )
		ips120_20_comm_failure( );

	/* If device mis-understood the command send it again */

	if ( reply[ 0 ] == '?' )
	{
		if ( retries-- )
			goto start;
		else
			ips120_20_comm_failure( );
	}

	/* If the first character of the reply isn't equal to the third character
	   of the message we probably read the reply for a previous command and
	   try to read again... */

	if ( reply[ 0 ] != message[ 2 ] )
	{
		if ( retries-- )
			goto reread;
		else
			ips120_20_comm_failure( );
	}

	return len;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ips120_20_comm_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}