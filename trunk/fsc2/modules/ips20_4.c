/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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

#include "ips20_4.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define TEST_CURRENT       2.315
#define TEST_SWEEP_RATE    ( 0.02 / 60.0 )


/* How many time we retry when an error happens during communication with
   the device */

#define MAX_RETRIES        10

/* Declaration of exported functions */

int ips20_4_init_hook( void );
int ips20_4_test_hook( void );
int ips20_4_exp_hook( void );
int ips20_4_end_of_exp_hook( void );
void ips20_4_exit_hook( void );

Var *magnet_name( Var *v );
Var *magnet_setup( Var *v );
Var *get_field( Var *v );
Var *set_field( Var *v );
Var *set_sweep_rate( Var *v );
Var *magnet_sweep( Var *v );
Var *magnet_sweep_rate( Var *v );
Var *reset_field( Var *v );
Var *magnet_goto_current_on_end( Var *v );


static void magnet_sweep_up( void );
static void magnet_sweep_down( void );
static void magnet_stop_sweep( void );

static bool ips20_4_init( const char *name );
static void ips20_4_to_local( void );
static void ips20_4_get_complete_status( void );
static void ips20_4_sweep_up( void );
static void ips20_4_sweep_down( void );
static double ips20_4_current_check( double current );
static double ips20_4_sweep_rate_check( double sweep_rate );
static double ips20_4_get_act_current( void );
static double ips20_4_current_check( double current );
static double ips20_4_sweep_rate_check( double sweep_rate );
static double ips20_4_set_target_current( double current );
static double ips20_4_get_target_current( void );
static double ips20_4_set_sweep_rate( double sweep_rate );
static double ips20_4_get_sweep_rate( void );
static double ips20_4_goto_current( double current );
static int ips20_4_set_activity( int activity );
static long ips20_4_talk( const char *message, char *reply, long length );
static void ips20_4_comm_failure( void );


#define MESSAGE_AVAILABLE 0x10


typedef struct {
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

} IPS20_4;


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


IPS20_4 ips20_4;
IPS20_4 ips20_4_stored;


/*-----------------------------------*/
/* Init hook function for the module */
/*-----------------------------------*/

int ips20_4_init_hook( void )
{
	need_GPIB = SET;

	ips20_4.device = -1;

	ips20_4.act_current = 0.0;

	ips20_4.is_start_current = UNSET;
	ips20_4.is_sweep_rate = UNSET;

	ips20_4.fast_sweep_rate = FAST_SWEEP_RATE;

	ips20_4.max_current = MAX_CURRENT;
	ips20_4.min_current = MIN_CURRENT;

	ips20_4.goto_field_on_end = UNSET;
	return 1;
}


/*-----------------------------------*/
/* Test hook function for the module */
/*-----------------------------------*/

int ips20_4_test_hook( void )
{
	ips20_4_stored = ips20_4;

	if ( ips20_4.is_start_current )
		ips20_4.act_current = ips20_4.start_current;
	else
	{
		ips20_4.start_current = ips20_4.act_current = TEST_CURRENT;
		ips20_4.is_start_current = SET;
	}

	if ( ! ips20_4.is_sweep_rate )
	{
		ips20_4.sweep_rate = TEST_SWEEP_RATE;
		ips20_4.is_sweep_rate = SET;
	}

	ips20_4.activity = HOLD;
	ips20_4.sweep_state = STOPPED;

	ips20_4.time_estimate = experiment_time( );

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int ips20_4_exp_hook( void )
{
	ips20_4 = ips20_4_stored;

	if ( ! ips20_4_init( DEVICE_NAME ) )
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

int ips20_4_end_of_exp_hook( void )
{
	ips20_4_to_local( );
	ips20_4 = ips20_4_stored;
	ips20_4.device = -1;

	return 1;
}


/*-----------------------------------------*/
/* Called before device driver is unloaded */
/*-----------------------------------------*/

void ips20_4_exit_hook( void )
{
	if ( ips20_4.device >= 0 )
		ips20_4_end_of_exp_hook( );
}


/*---------------------------------------------------*/
/* Function returns the name the device is known as. */
/*---------------------------------------------------*/

Var *magnet_name( Var *v )
{
	v = v;
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

	cur = get_double( v, "magnet current" );
	ips20_4.start_current = ips20_4_current_check( cur );
	ips20_4.is_start_current = SET;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		sweep_rate = get_double( v, "magnet sweep speed" );
		if ( sweep_rate < 0 )
		{
			print( FATAL, "Negative sweep rates can't be used, use argument "
				   "to magnet_sweep() to set sweep direction.\n" );
			THROW( EXCEPTION );
		}

		ips20_4.sweep_rate = ips20_4_sweep_rate_check( sweep_rate );
		ips20_4.is_sweep_rate = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------------*/
/* Function returns the momentary current. During the test run an estimate */
/* of the current is returned, based on the sweep rate and a guess of the  */
/* time spent since the last call for determining the current.             */
/*-------------------------------------------------------------------------*/

Var *get_field( Var *v )
{
	v = v;


	if ( FSC2_MODE == TEST )
	{
		/* During the test run we need to return some not completely bogus
		   value when a sweep is run. Thus an estimate for the time spend
		   until now is fetched, multiplied by the sweep rate and added to
		   the current field */

		if ( ips20_4.sweep_state != STOPPED &&
			 ips20_4.activity == TO_SET_POINT )
		{
			double cur_time, dtime;

			cur_time = experiment_time( );
			dtime = cur_time - ips20_4.time_estimate;
			ips20_4.time_estimate = cur_time;

			ips20_4.act_current = 1.0e-4 *
				( double ) lrnd( 1.0e4 * ( ips20_4.act_current
								 + experiment_time( ) * ips20_4.sweep_rate
								 * ( ips20_4.sweep_state == SWEEPING_UP ?
									 1.0 : - 1.0 ) ) );

			if ( ips20_4.act_current > ips20_4.max_current )
				ips20_4.act_current = ips20_4.max_current;
			if ( ips20_4.act_current < ips20_4.min_current )
				ips20_4.act_current = ips20_4.min_current;
		}
	}
	else
		ips20_4.act_current = ips20_4_get_act_current( );

	/* If a sweep reached one of the current limits stop the sweep */

	if ( ( ( ips20_4.sweep_state == SWEEPING_UP ||
			 ips20_4.activity == TO_SET_POINT ) &&
		   ips20_4.act_current >= ips20_4.max_current ) ||
		 ( ( ips20_4.sweep_state == SWEEPING_DOWN ||
			 ips20_4.activity == TO_SET_POINT ) &&
		   ips20_4.act_current <= ips20_4.min_current ) )
	{
		print( WARN, "Sweep had to be stopped because current limit was "
			   "reached.\n" );

		if ( FSC2_MODE == EXPERIMENT )
			ips20_4.activity = ips20_4_set_activity( HOLD );
		else
			ips20_4.activity = HOLD;
		ips20_4.sweep_state = STOPPED;
	}

	return vars_push( FLOAT_VAR, ips20_4.act_current );
}



/*--------------------------------------------------------*/
/* Function for setting a new current value. Please note  */
/* that setting a new current also stops a running sweep. */
/*--------------------------------------------------------*/

Var *set_field( Var *v )
{
	double cur;


	/* Stop sweeping */

	if ( ips20_4.sweep_state != STOPPED ||
		 ips20_4.activity != HOLD )
	{
		if ( FSC2_MODE == EXPERIMENT )
			ips20_4.activity = ips20_4_set_activity( HOLD );
		else
			ips20_4.activity = HOLD;
		ips20_4.sweep_state = STOPPED;
	}

	/* Check the current */

	cur = ips20_4_current_check( get_double( v, "field current" ) );

	if ( FSC2_MODE == EXPERIMENT )
		cur = ips20_4_goto_current( cur );

	return vars_push( FLOAT_VAR, ips20_4.act_current = cur );
}


/*----------------------------------------------------*/
/* This is the EDL function for starting or stopping sweeps or */
/* inquiring about the current sweep state. */
/*----------------------------------------------------*/

Var *magnet_sweep( Var *v )
{
	long dir;
	Var *vc;


	if ( v == NULL || FSC2_MODE == TEST )
	{
		vc = get_field( NULL );
		ips20_4.act_current = vc->val.dval;
		vars_pop( vc );
	}

	if ( v == NULL )
		switch ( ips20_4.sweep_state )
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
	if ( ! ips20_4.is_sweep_rate )
	{
		print( FATAL, "No sweep rate has been set.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ips20_4.sweep_state == SWEEPING_UP )
		{
			print( SEVERE, "Field is already sweeping up.\n" );
			return;
		}

		if ( ips20_4.act_current >= ips20_4.max_current - CURRENT_RESOLUTION )
		{
			print( WARN, "Magnet is already at maximum current.\n" );
			return;
		}

		ips20_4.sweep_state = SWEEPING_UP;
		ips20_4.activity = TO_SET_POINT;
		ips20_4.target_current = ips20_4.max_current;
		return;
	}

	ips20_4_sweep_up( );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void magnet_sweep_down( void )
{
	if ( ! ips20_4.is_sweep_rate )
	{
		print( FATAL, "No sweep rate has been set.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ips20_4.sweep_state == SWEEPING_DOWN )
		{
			print( SEVERE, "Field is already sweeping down.\n" );
			return;
		}

		if ( ips20_4.act_current <= ips20_4.min_current + CURRENT_RESOLUTION )
		{
			print( WARN, "Magnet is already at minimum current.\n" );
			return;
		}

		ips20_4.sweep_state = SWEEPING_DOWN;
		ips20_4.activity = TO_SET_POINT;
		ips20_4.target_current = ips20_4.min_current;
		return;
	}

	ips20_4_sweep_down( );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void magnet_stop_sweep( void )
{
	if ( ips20_4.sweep_state == STOPPED )
	{
		print( FSC2_MODE == TEST ? SEVERE : WARN,
			   "Sweep is already stopped.\n" );
		return;
	}

	if ( FSC2_MODE == TEST )
	{
		ips20_4.sweep_state = STOPPED;
		ips20_4.activity = HOLD;
		return;
	}

	ips20_4.activity = ips20_4_set_activity( HOLD );
	ips20_4.sweep_state = STOPPED;
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
				if ( ! ips20_4.is_sweep_rate )
					no_query_possible( );
				else
					return vars_push( FLOAT_VAR, ips20_4.sweep_rate );

			case TEST :
				return vars_push( FLOAT_VAR, ips20_4.is_sweep_rate ?
								  ips20_4.sweep_rate : TEST_SWEEP_RATE );

			case EXPERIMENT :
				return vars_push( FLOAT_VAR, ips20_4.sweep_rate );
		}

	sweep_rate = ips20_4_sweep_rate_check( get_double( v, "sweep rate" ) );
	if ( ips20_4.is_sweep_rate && sweep_rate == ips20_4.sweep_rate )
		return vars_push( FLOAT_VAR, ips20_4.sweep_rate );

	ips20_4.sweep_rate = sweep_rate;
	ips20_4.is_sweep_rate = SET;

	if ( FSC2_MODE == EXPERIMENT )
		ips20_4_set_sweep_rate( ips20_4.sweep_rate );

	return vars_push( FLOAT_VAR, ips20_4.sweep_rate );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *reset_field( Var *v )
{
	v = v;
	return set_field( vars_push( FLOAT_VAR, ips20_4.start_current ) );
}


/*-----------------------------------------------------------*/
/* This function was added on Martin Fuchs' request to allow */
/* to keep sweeping the magnet to a predefined current after */
/* the experiment has ended.                                 */
/*-----------------------------------------------------------*/

Var *magnet_goto_current_on_end( Var *v )
{
	double cur;


	cur = get_double( v, "final target current" );
	ips20_4.final_target_current = ips20_4_current_check( cur );
	ips20_4.goto_field_on_end = SET;

	return vars_push( FLOAT_VAR, ips20_4.final_target_current );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool ips20_4_init( const char *name )
{
	char cmd[ 100 ];
	char reply[ 100 ];
	long length;
	double cur_limit;
	bool was_hold = UNSET;


	/* First we have to set up the ITC503 temperature controller which
	   actually does the whole GPIB communication and passes on commands
	   to the sweep power supply and returns its replies via the ISOBUS. */

	if ( gpib_init_device( name, &ips20_4.device ) == FAILURE )
	{
		ips20_4.device = -1;
        return FAIL;
	}

	if ( gpib_clear_device( ips20_4.device ) == FAILURE )
		ips20_4_comm_failure( );

	usleep( 250000 );

	/* Bring both the GPIB master device (ITC 503) as well as the sweep power
	   supply in remote state */

	sprintf( cmd, "@%1dC3\r", MASTER_ISOBUS_ADDRESS );
	ips20_4_talk( cmd, reply, 100 );

	sprintf( cmd, "@%1dQ0\r", MASTER_ISOBUS_ADDRESS );
	if ( gpib_write( ips20_4.device, cmd, 5 ) == FAILURE )
		ips20_4_comm_failure( );

	sprintf( cmd, "@%1dC3\r", IPS20_4_ISOBUS_ADDRESS );
	ips20_4_talk( cmd, reply, 100 );

	/* Set the sweep power supply to send and accept data with extended
	   resolution (this is one of the few commands that don't produce a
	   reply) */

	sprintf( cmd, "@%1dQ4\r", IPS20_4_ISOBUS_ADDRESS );
	if ( gpib_write( ips20_4.device, cmd, 5 ) == FAILURE )
		ips20_4_comm_failure( );

	/* Get the status of the magnet - if it's not in the LOC/REMOTE state we
	   set it to something is going wrong... */

	ips20_4_get_complete_status( );
	if ( ips20_4.loc_rem_state != REMOTE_AND_UNLOCKED )
	{
		print( FATAL, "Magnet did not accept command.\n" );
		THROW( EXCEPTION );
	}

	/* Get the maximum and minimum safe current limits and use these as the
	   allowed current range (unless they are larger than the ones set in the
	   configuration file). */

	sprintf( cmd, "@%1dR21\r", IPS20_4_ISOBUS_ADDRESS );
	length = ips20_4_talk( cmd, reply, 100 );
	reply[ length - 1 ] = '\0';
	cur_limit = T_atod( reply + 1 );

	if ( cur_limit > MIN_CURRENT )
		ips20_4.min_current = cur_limit;

	sprintf( cmd, "@%1dR22\r", IPS20_4_ISOBUS_ADDRESS );
	length = ips20_4_talk( cmd, reply, 100 );

	reply[ length - 1 ] = '\0';
	cur_limit = T_atod( reply + 1 );

	if ( cur_limit < MAX_CURRENT )
		ips20_4.max_current = cur_limit;

	/* Get the actual current, and if the magnet is sweeping, the current
	   sweep direction. If the magnet is running to zero current just stop
	   it. */

	ips20_4.act_current = ips20_4_get_act_current( );
	ips20_4.target_current = ips20_4_get_target_current( );

	switch ( ips20_4.activity )
	{
		case HOLD :
			ips20_4.sweep_state = STOPPED;
			was_hold = SET;
			break;

		case TO_SET_POINT :
			ips20_4.sweep_state =
				ips20_4.act_current < ips20_4.target_current ?
				SWEEPING_UP : SWEEPING_DOWN;
			break;

		case TO_ZERO:
			ips20_4.activity = ips20_4_set_activity( HOLD );
			ips20_4.sweep_state = STOPPED;
			break;
	}

	/* Set the sweep rate if the user defined one, otherwise get the current
	   sweep rate. */

	if ( ips20_4.is_sweep_rate )
		ips20_4.sweep_rate = ips20_4_set_sweep_rate( ips20_4.sweep_rate );
	else
	{
		ips20_4.sweep_rate = ips20_4_get_sweep_rate( );
		ips20_4.is_sweep_rate = SET;
	}

	/* Finally, if a start curent has been set stop the magnet if necessary
	   and set the start current. If the magnet was in HOLD state when we
	   started and no start current had been set use the actual current as the
	   start current. */

	if ( ips20_4.is_start_current )
	{
		if ( ips20_4.sweep_state != STOPPED )
		{
			ips20_4.activity = ips20_4_set_activity( HOLD );
			ips20_4.sweep_state = STOPPED;
		}

		ips20_4.act_current = ips20_4_goto_current( ips20_4.start_current );
	}
	else if ( was_hold )
	{
		ips20_4.start_current = ips20_4.act_current;
		ips20_4.is_start_current = SET;
	}

	return OK;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ips20_4_to_local( void )
{
	char cmd[ 20 ];
	char reply[ 100 ];


	/* On Martin Fuchs' request there are now two alternatives: Normally
	   the magnet simply gets stopped when the experiment finishes, but if
	   the function magnet_current_field_on_end() has been called the magnet
	   instead sweeps to the current value passed to the function. */

	if ( ips20_4.goto_field_on_end )
	{
		ips20_4_set_target_current( ips20_4.final_target_current );
		ips20_4_set_sweep_rate( ips20_4.fast_sweep_rate );
		ips20_4_set_activity( TO_SET_POINT );
	}
	else
	{	
		ips20_4_set_activity( HOLD );
		ips20_4.sweep_state = STOPPED;
	}

	sprintf( cmd, "@%1dC0\r", IPS20_4_ISOBUS_ADDRESS );
	ips20_4_talk( cmd, reply, 100 );

	sprintf( cmd, "@%1dC0\r", MASTER_ISOBUS_ADDRESS );
	ips20_4_talk( cmd, reply, 100 );

	gpib_local( ips20_4.device );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ips20_4_get_complete_status( void )
{
	char cmd[ 100 ];
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

	sprintf( cmd, "@%1dX\r", IPS20_4_ISOBUS_ADDRESS );
	if ( gpib_write( ips20_4.device, cmd, strlen( cmd ) ) == FAILURE )
		ips20_4_comm_failure( );

	for ( i = 0; i < max_retries; i++ )
	{
		if ( gpib_read( ips20_4.device, reply + offset, &len ) == FAILURE )
			ips20_4_comm_failure( );

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
			ips20_4.activity = HOLD;
			break;

		case '1' :
			ips20_4.activity = TO_SET_POINT;
			break;

		case '2' :
			ips20_4.activity = TO_ZERO;
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
			ips20_4.loc_rem_state = LOCAL_AND_LOCKED;
			break;

		case '1' :
			ips20_4.loc_rem_state = REMOTE_AND_LOCKED;
			break;

		case '2' :
			ips20_4.loc_rem_state = LOCAL_AND_UNLOCKED;
			break;

		case '3' :
			ips20_4.loc_rem_state = REMOTE_AND_UNLOCKED;
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
			ips20_4.mode = FAST;
			break;

		case '4' :
			ips20_4.mode = SLOW;
			break;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			THROW( EXCEPTION );
	}

	switch ( reply[ 11 ] )
	{
		case '0' :
			ips20_4.state = AT_REST;
			break;

		case '1' :
			ips20_4.state = SWEEPING;
			break;

		case '2' :
			ips20_4.state = SWEEP_LIMITING;
			break;

		case '4' :
			ips20_4.state = SWEEPING_AND_SWEEP_LIMITING;
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

static void ips20_4_sweep_up( void )
{
	/* Do nothing except printing a warning when we're alredy sweeping up */

	if ( ips20_4.sweep_state == SWEEPING_UP )
	{
		print( WARN, "Useless command, magnet is already sweeping up.\n" );
		return;
	}

	/* Print a severe warning when the actual current is already very near to
	   the maximum current */

	ips20_4.act_current = ips20_4_get_act_current( );
	if ( ips20_4.act_current >= ips20_4.max_current - CURRENT_RESOLUTION )
	{
		print( SEVERE, "Magnet is already at maximum current.\n" );
		return;
	}

	if ( ips20_4.activity != HOLD )
		ips20_4.activity = ips20_4_set_activity( HOLD );

	ips20_4.target_current = ips20_4_set_target_current( ips20_4.max_current );
	ips20_4.activity = ips20_4_set_activity( TO_SET_POINT );
	ips20_4.sweep_state = SWEEPING_UP;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ips20_4_sweep_down( void )
{
	/* Do nothing except printing a warning when we're alredy sweeping down */

	if ( ips20_4.sweep_state == SWEEPING_DOWN )
	{
		print( WARN, "Useless command, magnet is already sweeping down.\n" );
		return;
	}

	/* Print a severe warning when the actual current is already very near to
	   the minimum current */

	ips20_4.act_current = ips20_4_get_act_current( );
	if ( ips20_4.act_current <= ips20_4.min_current + CURRENT_RESOLUTION )
	{
		print( SEVERE, "Magnet is already at minimum current.\n" );
		return;
	}

	if ( ips20_4.activity != HOLD )
		ips20_4.activity = ips20_4_set_activity( HOLD );
	ips20_4.target_current = ips20_4_set_target_current( ips20_4.min_current );
	ips20_4.activity = ips20_4_set_activity( TO_SET_POINT );
	ips20_4.sweep_state = SWEEPING_DOWN;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips20_4_current_check( double current )
{
	if ( current > ips20_4.max_current )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Current of %f A is too high, maximum current is "
				   "%f A.\n", current, ips20_4.max_current );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Current of %f A is too high, using maximum "
				   "current of %f A instead.\n",
				   current, ips20_4.max_current );
			return MAX_CURRENT;
		}
	}

	if ( current < ips20_4.min_current )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Current of %f A is too low, minimum current is "
				   "%f A.\n", current, ips20_4.min_current );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Current of %f A is too low, using minimum "
				   "current of %f A instead.\n",
				   current, ips20_4.min_current );
			return ips20_4.min_current;
		}
	}

	/* Maximum current resolution is 0.1 mA */

	return ( double ) lrnd( current * 1.0e4 ) * 1.0e-4;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips20_4_sweep_rate_check( double sweep_rate )
{
	if ( sweep_rate > MAX_SWEEP_RATE )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Sweep rate of %f A/s is too high, maximum sweep "
				   "rate is %f A/s.\n", sweep_rate, MAX_SWEEP_RATE );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Sweep rate of %f A/s is too high, using maximum "
				   "sweep rate of %f A/s instead.\n",
				   sweep_rate, MAX_SWEEP_RATE );
			return MAX_SWEEP_RATE;
		}
	}

	if ( sweep_rate < MIN_SWEEP_RATE )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Sweep rate of %f mA/s is too low, minimum sweep "
				   "rate is %f mA/s.\n",
				   sweep_rate * 1.0e3, MIN_SWEEP_RATE * 1.0e3 );
			THROW( EXCEPTION );
		}
		else
		{
			print( SEVERE, "Sweep rate of %f mA/s is too low, using minimum "
				   "sweep rate of %f mA/s instead.\n",
					sweep_rate * 1.0e3, MIN_SWEEP_RATE * 1.0e3 );
			return MIN_SWEEP_RATE;
		}
	}

	/* Minimum sweep speed resolution is 1 mA/s */

	return ( double ) lrnd( sweep_rate * 60000.0 ) / 60000.0;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips20_4_get_act_current( void )
{
	char cmd[ 20 ];
	char reply[ 100 ];
	long length;


	sprintf( cmd, "@%1dR0\r", IPS20_4_ISOBUS_ADDRESS );
	length = ips20_4_talk( cmd, reply, 100 );

	reply[ length - 1 ] = '\0';
	return T_atod( reply + 1 );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips20_4_set_target_current( double current )
{
	char cmd[ 30 ];
	char reply[ 100 ];


	current = ips20_4_current_check( current );
	sprintf( cmd, "@%1dI%.4f\r", IPS20_4_ISOBUS_ADDRESS, current );
	ips20_4_talk( cmd, reply, 100 );

	return current;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips20_4_get_target_current( void )
{
	char cmd[ 30 ];
	char reply[ 100 ];
	long length;


	sprintf( cmd, "@%1dR5\r", IPS20_4_ISOBUS_ADDRESS );
	length = ips20_4_talk( cmd, reply, 100 );

	reply[ length - 1 ] = '\0';
	return T_atod( reply + 1 );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips20_4_set_sweep_rate( double sweep_rate )
{
	char cmd[ 30 ];
	char reply[ 100 ];


	sweep_rate = ips20_4_sweep_rate_check( sweep_rate );

	sprintf( cmd, "@%1dS%.3f\r", IPS20_4_ISOBUS_ADDRESS, sweep_rate * 60.0 );
	ips20_4_talk( cmd, reply, 100 );

	return sweep_rate;
}

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips20_4_get_sweep_rate( void )
{
	char cmd[ 30 ];
	char reply[ 100 ];
	long length;


	sprintf( cmd, "@%1dR6\r", IPS20_4_ISOBUS_ADDRESS );
	length = ips20_4_talk( cmd, reply, 100 );

	reply[ length - 1 ] = '\0';
	return T_atod( reply + 1 ) / 60.0;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static double ips20_4_goto_current( double current )
{
	ips20_4_set_target_current( current );
	ips20_4_set_sweep_rate( ips20_4.fast_sweep_rate );

	if ( ips20_4.activity != TO_SET_POINT )
		ips20_4.activity = ips20_4_set_activity( TO_SET_POINT );

	while ( lrnd( ( current -  ips20_4_get_act_current( ) ) /
				  CURRENT_RESOLUTION ) != 0 )
	{
		usleep( 50000 );
		stop_on_user_request( );
	}

	ips20_4.activity = ips20_4_set_activity( HOLD );
	ips20_4.sweep_state = STOPPED;

	if ( ips20_4.is_sweep_rate )
		ips20_4_set_sweep_rate( ips20_4.sweep_rate );

	return ips20_4_get_act_current( );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static int ips20_4_set_activity( int activity )
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

	sprintf( cmd, "@%1dA%1d\r", IPS20_4_ISOBUS_ADDRESS, act );
	ips20_4_talk( cmd, reply, 100 );

	return activity;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static long ips20_4_talk( const char *message, char *reply, long length )
{
	long len = length;
	int retries = MAX_RETRIES;


 start:

	if ( gpib_write( ips20_4.device, message, strlen( message ) ) == FAILURE )
		ips20_4_comm_failure( );

	/* Re-enable the following if you're extremely careful, but even the
	   LabVIEW driver by Oxford doesn't use it... */

#if 0
	do {
		unsigned char stb;

		stop_on_user_request( );

		usleep( 500 );

		if ( gpib_serial_poll( ips20_4.device, &stb ) == FAILURE )
			ips20_4_comm_failure( );
	} while ( ! ( stb & MESSAGE_AVAILABLE ) );
#endif

 reread:

	stop_on_user_request( );

	len = length;
	if ( gpib_read( ips20_4.device, reply, &len ) == FAILURE )
		ips20_4_comm_failure( );

	/* If device mis-understood the command send it again */

	if ( reply[ 0 ] == '?' )
	{
		if ( retries-- )
			goto start;
		else
			THROW( EXCEPTION );
	}

	/* If the first character of the reply isn't equal to the third character
	   of the message we probably read the reply for a previous command and
	   try to read again... */

	if ( reply[ 0 ] != message[ 2 ] )
	{
		if ( retries-- )
			goto reread;
		else
			THROW( EXCEPTION );
	}

	return len;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ips20_4_comm_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}
