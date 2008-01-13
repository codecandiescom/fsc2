/*
 *  $Id
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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2_module.h"
#include "serial.h"


/* Include configuration information for the device */

#include "fsps25.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Exported functions */

int fsps25_init_hook( void );
int fsps25_test_hook( void );
int fsps25_exp_hook( void );
int fsps25_end_of_exp_hook( void );

Var_T * magnet_name( Var_T * v  UNUSED_ARG );
Var_T * magnet_field( Var_T * v );
Var_T * magnet_sweep_rate( Var_T * v );
Var_T * magnet_max_current( Var_T * v );
Var_T * magnet_max_sweep_rate( Var_T *v );


/* Local functions */

static void fsps25_init( void );
static void fsps25_on_off( bool state );
static void fsps25_on( void );
static void fsps25_off( void );
static void fsps25_get_state( void );
static void fsps25_read_state( void );
static bool fsps25_get_heater_state( void );
static void fsps25_set_heater_state( bool state );
static bool fsps25_abort_sweep( void );
static long fsps25_get_act_current( void );
static long fsps25_set_act_current( long current );
static long fsps25_get_super_current( void );
static long fsps25_get_max_current( void );
static long fsps25_set_max_current( long current );
static long fsps25_get_sweep_speed( void );
static long fsps25_set_sweep_speed( long speed );
static long fsps25_get_max_sweep_speed( void );
static long fsps25_set_max_sweep_speed( long max_speed );
static void fsps25_open( void );
static void fsps25_get_command_reply( void );
static void fsps25_comm_fail( void );
static void fsps25_wrong_data( void );
static void fsps25_pfail( void );


typedef struct {
	bool is_open;
	bool is_expert_mode;
	int state;
	bool sweep_state;
	bool heater_state;
	bool is_matched;
	long act_current;
	bool act_current_is_set;
	long super_current;
	long max_current;       /* maximum current (in mA) */
	bool max_current_is_set;
	long max_speed;         /* maximum sweep speed (in mA/min) */
	bool max_speed_is_set;
	long act_speed;         /* actual speed (in mA/min) */
	bool act_speed_is_set;
    struct termios *tio;    /* serial port terminal interface structure */
} FSPS25;

FSPS25 fsps25, fsps25_stored;

#define RESPONSE_TIME         50        /* 50 us (for "normal commands) */

#define HEATER_OFF_DELAY       10000000  /* 10 s  */
#define HEATER_ON_DELAY        1500000   /* 1.5 s */

/* Defines for the parity used by the device */

#define NO_PARITY         0
#define ODD_PARITY        1
#define EVEN_PARITY       2


/* The different states the device can be in */

#define STATE_OFF         0
#define STATE_ON          1
#define STATE_PFAIL       2


/* The different states the heater can be in */

#define HEATER_OFF        0
#define HEATER_ON         1


/* The sweep states */

#define STOPPED           0
#define SWEEPING          1


/* The sweep rate with the heater being off */

#define  MAX_HEATER_OFF_SPEED  6000   /* 6000 mA/min */


/****************************************************/
/*                                                  */
/*                  hook functions                  */
/*                                                  */
/****************************************************/


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
fsps25_init_hook( void )
{
	fsps25.is_open = UNSET;
	fsps25.state = STATE_OFF;
	fsps25.sweep_state = STOPPED;
	fsps25.heater_state = HEATER_OFF;
	fsps25.is_matched = SET;
	fsps25.act_current = fsps25.super_current = 0;
	fsps25.max_current = MAX_ALLOWED_CURRENT;
	fsps25.max_speed = fsps25.act_speed = MAX_SPEED;
	fsps25.is_expert_mode = UNSET;

	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
fsps25_test_hook( void )
{
	fsps25_stored = fsps25;
	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
fsps25_exp_hook( void )
{
	fsps25 = fsps25_stored;
	fsps25_init( );

	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
fsps25_end_of_exp_hook( void )
{
	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
magnet_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/

Var_T *
magnet_field( Var_T * v )
{
	double current;
	long raw_current;


	if ( v == NULL )
		vars_push( FLOAT_VAR,
				   1.0e-3 * 
				   ( FSC2_MODE == EXPERIMENT ?
					 fsps25_get_super_current( ) : fsps25.super_current ) );

	current = get_double( v, "current" );
	raw_current = lrnd( 1.0e3 * current );

	too_many_arguments( v );

	if ( labs( raw_current ) > MAX_CURRENT )
	{
		print( FATAL, "Requested current of %.3f A is not within the possible "
			   "range.\n", current );
		THROW( EXCEPTION );
	}

	if ( labs( raw_current ) > MAX_ALLOWED_CURRENT )
	{
		print( FATAL, "Requested current of %.3f A is not within the allowed "
			   "range.\n", current );
		THROW( EXCEPTION );
	}

	if ( labs( raw_current ) > fsps25.max_current )
	{
		print( FATAL, "Requested current of %.3f A is not within the currently "
			   "set limits.\n", current );
		THROW( EXCEPTION );
	}

	if (    fsps25.act_current == raw_current
		 || FSC2_MODE != EXPERIMENT )
	{
		fsps25.act_current = raw_current;
		fsps25.act_current_is_set = SET;
		return vars_push( FLOAT_VAR, 1.0e-3 * raw_current );
	}

	return vars_push( FLOAT_VAR,
					  1.0e-3 * fsps25_set_act_current( raw_current ) );
}


/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/

Var_T *
magnet_sweep_rate( Var_T * v )
{
	long raw_speed;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, fsps25.act_speed / 6.0e4 );


	raw_speed = lrnd( 6.0e4 * get_double( v, "sweep rate" ) );

	too_many_arguments( v );

	if ( raw_speed < 0 )
	{
		print( FATAL, "Invalid negative sweep speed argument.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_speed < MIN_SPEED )
	{
		print( FATAL, "Sweep speed argument is too small.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_speed > fsps25.max_speed )
	{
		print( FATAL, "Sweep speed argument is larger than the currently set "
			   "upper limit of %.3f mA/s.\n", fsps25.max_speed / 6.0e4 );
		THROW( EXCEPTION );
	}

	if (    fsps25.act_speed == raw_speed
		 || FSC2_MODE != EXPERIMENT )
	{
		fsps25.act_speed = raw_speed;
		fsps25.act_speed_is_set = SET;
		return vars_push( FLOAT_VAR, raw_speed / 6.0e4 );
	}

	return vars_push( FLOAT_VAR, fsps25_set_sweep_speed( raw_speed ) / 6.0e4 );
}


/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/

Var_T *
magnet_max_current( Var_T * v )
{
	double max_current;
	long raw_max_current;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, 1.0e-3 * fsps25.max_current );


	max_current = get_double( v, "maximum current" );
	raw_max_current = lrnd( 1.0e3 * max_current );

	too_many_arguments( v );

	if ( raw_max_current < 0 )
	{
		print( FATAL, "Invalid negativ value for maximum current.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_max_current > MAX_CURRENT )
	{
		print( FATAL, "Requested maximum current is larger than maximum "
			   "possible current.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_max_current > MAX_ALLOWED_CURRENT )
	{
		print( FATAL, "Requested maximum current is larger than maximum "
			   "allowed current.\n" );
		THROW( EXCEPTION );
	}

	/* We need the to have the newest value of the actual current (we might
	   be in a sweep at this moment) and refuse to set a maximum current that's
	   lower than this actual current */

	if ( FSC2_MODE == EXPERIMENT )
		fsps25_get_act_current( );

	if ( raw_max_current > labs( fsps25.act_current ) )
	{
		print( FATAL, "Can't set maximum current to a value lower than the "
			   "actual current.\n" );
		THROW( EXCEPTION );
	}

	if (    raw_max_current == fsps25.max_current
		 || FSC2_MODE != EXPERIMENT )
	{
		fsps25.max_current = raw_max_current;
		fsps25.max_current = SET;
		return vars_push( FLOAT_VAR, 1.0e-3 * raw_max_current );
	}

	return vars_push( FLOAT_VAR,
					  1.0e-3 * fsps25_set_max_current( raw_max_current ) );
}


/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/

Var_T *
magnet_max_sweep_rate( Var_T *v )
{
	double max_sweep_rate;
	long raw_max_sweep_rate;

	if ( v == NULL )
		return vars_push( FLOAT_VAR, fsps25.max_speed / 6.0e4 );

	max_sweep_rate = get_double( v, "maximum sweep rate" );
	raw_max_sweep_rate = lrnd( 6.0e4 * max_sweep_rate );

	if ( raw_max_sweep_rate < 0 )
	{
		print( FATAL, "Invalid negative maximum sweep speed argument.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_max_sweep_rate < MIN_SPEED )
	{
		print( FATAL, "Maximum sweep speed argument is to small.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_max_sweep_rate > MAX_SPEED )
	{
		print( FATAL, "Maximum sweep speed argument is larger than maximum "
			   "possible value.\n" );
		THROW( EXCEPTION );
	}

	if (    raw_max_sweep_rate == fsps25.max_speed
		 || FSC2_MODE != EXPERIMENT )
	{
		fsps25.max_speed = raw_max_sweep_rate;
		fsps25.max_speed_is_set = SET;
		if ( raw_max_sweep_rate < fsps25.act_speed  )
			fsps25.act_speed = raw_max_sweep_rate;
		return vars_push( FLOAT_VAR, raw_max_sweep_rate / 6.0e4 );
	}

	return vars_push( FLOAT_VAR,
					  fsps25_set_max_sweep_speed( raw_max_sweep_rate )
					  / 6.0e4 );
}


/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/

static void
fsps25_init( void )
{
	FSPS25 target_state = fsps25;


	fsps25_open( );
	fsps25_get_state( );

	if ( fsps25.state == STATE_PFAIL )
		return;

	/* Switch to ON state if necessary */

	if ( fsps25.state == STATE_OFF )
		fsps25_on_off( STATE_ON );

	/* Set or get the values for the maximun current, the sweep speed and
	   the maximum sweep speed */

	if ( fsps25.max_current_is_set )
		fsps25_get_max_current( );
	else
		fsps25_set_max_current( fsps25.max_current );

	if ( fsps25.act_speed_is_set )
		fsps25_set_sweep_speed( fsps25.act_speed );
	else
		fsps25_get_sweep_speed( );

	if ( fsps25.max_speed_is_set )
		fsps25_set_max_sweep_speed( fsps25.max_speed );
	else
		fsps25_get_max_sweep_speed( );

	/* We now need to switch on the heater */

	if ( ! fsps25.heater_state )
	{
		fsps25_get_super_current( );
		if ( ! fsps25.is_matched )
			fsps25_set_act_current( fsps25.super_current );

		fsps25_set_heater_state( HEATER_ON );
	}

	/* If a start current had been given set in now */

	if ( target_state.act_current_is_set )
		fsps25_set_act_current( target_state.act_current );
}


/*------------------------------------------------------------------------*
 * Function for bringing the device into remote and back into local state
 *------------------------------------------------------------------------*/

static void
fsps25_on_off( bool state )
{
	if (    ( ! state && fsps25.state == STATE_OFF )
		 || (   state && fsps25.state != STATE_ON  ) )
		return;

	if ( state == 1 )
		fsps25_on( );
	else
		fsps25_off( );
}


/*-----------------------------------------*
 * Function for bringing the device online
 *-----------------------------------------*/

static void
fsps25_on( void )
{
	if ( FSC2_MODE != EXPERIMENT )
	{
		fsps25.state = STATE_ON;
		fsps25.heater_state = HEATER_OFF;
		fsps25.sweep_state = STOPPED;
		fsps25.is_matched = UNSET;
		return;
	}

	if ( fsc2_serial_write( SERIAL_PORT, "On!\r", 4,
							RESPONSE_TIME, UNSET ) != 4 )
		fsps25_comm_fail( );

	fsps25_read_state( );

	/* Now the device should be in ON (or, in the worst case. in PFAIL) state
	   with the heater off and the sweep stopped */

	if ( fsps25.state == STATE_OFF )
	{
		print( FATAL, "Device doesn't react properly to being brought "
			   "online.\n" );
		THROW( EXCEPTION );
	}

	if ( fsps25.heater_state == HEATER_ON )
	{
		print( FATAL, "Heater is on immediately without switching it on!\n" );
		THROW( EXCEPTION );
	}

	if( fsps25.sweep_state == SWEEPING )
	{
		print( FATAL, "Current is sweeping without a command having been "
			   "sent.\n" );
		THROW( EXCEPTION );
	}
}


/*------------------------------------------*
 * Function for bringing the device offline
 *------------------------------------------*/

static void
fsps25_off( void )
{
	char buf[ 5 ];
	int retries = 30;


	if ( FSC2_MODE != EXPERIMENT )
	{
		fsps25.state = STATE_OFF;
		return;
	}

	/* Make sure the device isn't sweeping anymore */

	if ( fsps25.sweep_state == SWEEPING )
		fsps25_abort_sweep( );

	/* Send it the command to shut down */

	if (    fsc2_serial_write( SERIAL_PORT, "DSD!\r", 5,
							   RESPONSE_TIME, UNSET ) != 5
		 || fsc2_serial_read( SERIAL_PORT, buf,  5,
							  RESPONSE_TIME, UNSET ) != 5 )
		fsps25_comm_fail( );

	if ( strncmp( buf, "DSD;\r", 5 ) )
		fsps25_wrong_data( );

	/* If the heater was on wait enough time for the heater to get
	   switched off */

	if ( fsps25.heater_state == HEATER_ON )
		fsc2_usleep( HEATER_OFF_DELAY, UNSET );

	/* If the current wasn't zero wait long enough for the device to sweep
	   it down to zero */

	if ( fsps25.act_current != 0 )
		fsc2_usleep( lrnd( ( 6.0e7 * fabs( fsps25.act_current ) )
						   / HEATER_OFF_SPEED ), UNSET );

	/* Check the state and give it up to 3 seconds of extra time if it
	   isn't finished yet */

	while ( retries-- > 0 )
	{
		fsps25_get_state( );
		if ( fsps25.state == STATE_OFF )
			return;
		fsc2_usleep( 100000, UNSET );
	}

    print( FATAL, "Device doesn't react properly to being shutdown.\n" );
    THROW( EXCEPTION );
}


/*-----------------------------------------------*
 * Function for asking the device for its status
 *-----------------------------------------------*/

static void
fsps25_get_state( void )
{
	char buf[ 5 ] = "GCS?\r";


	if ( fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5 )
		fsps25_comm_fail( );
	fsps25_read_state( );
}



/*-------------------------------------------------------*
 * Function to be called after a "GCS?" or "On!" command
 * to fetch the reply from the device
 *-------------------------------------------------------*/

static void
fsps25_read_state( void )
{
	char buf[ 27 ];
	ssize_t count;


	if ( ( count = fsc2_serial_read( SERIAL_PORT, buf, 27, 
									 RESPONSE_TIME, UNSET ) ) <= 0 )
		 fsps25_comm_fail( );

	/* The shortest possible response is "CS OFF;\r", it always must start
	   with "CS " and end in ";\r" */

	if (    count < 8 
		 || strncmp( buf, "CS ", 3 )
		 || buf[ count - 2 ] != ';'
		 || buf[ count - 1 ] != '\r' )
		fsps25_wrong_data( );

	buf[ count - 2 ] = '\0';

	if ( ! strcmp( buf + 3, "OFF" ) )
	{
		fsps25.state = STATE_OFF;
		fsps25.sweep_state = STOPPED;
		fsps25.heater_state = HEATER_OFF;
		fsps25.is_matched = UNSET;
	}
	else if ( ! strcmp( buf + 3, "PFAIL" ) )
	{
		fsps25.state = STATE_PFAIL;
		fsps25_pfail( );
		fsps25.sweep_state = STOPPED;
		fsps25.heater_state = HEATER_OFF;
		fsps25.is_matched = UNSET;
	}
	else if ( ! strcmp( buf + 3, "Stopped HOff Matched" ) )
	{
		fsps25.state = STATE_ON;
		fsps25.sweep_state = STOPPED;
		fsps25.heater_state = HEATER_OFF;
		fsps25.is_matched = SET;
	}
	else if ( ! strcmp( buf + 3, "Stopped HOff UnMatched" ) )
	{
		fsps25.state = STATE_ON;
		fsps25.sweep_state = STOPPED;
		fsps25.heater_state = HEATER_OFF;
		fsps25.is_matched = UNSET;
	}
	else if ( ! strcmp( buf + 3, "Stopped HOn" ) )
	{
		fsps25.state = STATE_ON;
		fsps25.sweep_state = STOPPED;
		fsps25.heater_state = HEATER_ON;
		fsps25.is_matched = SET;
	}
	else if ( ! strcmp( buf + 3, "Sweeping HOff" ) )
	{
		fsps25.state = STATE_ON;
		fsps25.sweep_state = SWEEPING;
		fsps25.heater_state = HEATER_OFF;
		fsps25.is_matched = UNSET;
	}
	else if ( ! strcmp( buf + 3, "Sweeping HOn" ) )
	{
		fsps25.state = STATE_ON;
		fsps25.sweep_state = SWEEPING;
		fsps25.heater_state = HEATER_ON;
		fsps25.is_matched = SET;
	}
	else
		fsps25_wrong_data( );
}


/*-----------------------------------------------------*
 * Function for asking the device for the heater state
 *-----------------------------------------------------*/

static bool
fsps25_get_heater_state( void )
{
	char buf[ 18 ] = "GHS?\r";
	ssize_t count = 2;


	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.heater_state;

	fsc2_assert( fsps25.state != STATE_OFF );

	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
		 || ( count = fsc2_serial_read( SERIAL_PORT, buf, 18,
										RESPONSE_TIME, UNSET ) ) <= 0 )
		fsps25_comm_fail( );

	if (    strncmp( buf, "HS ", 3 )
		 || ( buf[ 3 ] != '0' && buf[ 3 ] != '1' )
		 || buf[ 4 ] != ' '
         || buf[ count - 2 ] != ';'
		 || buf[ count - 1 ] != '\r' )
		fsps25_wrong_data( );

	buf[ count - 2 ] = '\0';

	/* There might be trouble with the heater (it may appear to be unconnected
	   or to be shorted) which we should check for here */

	if ( ! strcmp( buf + 5, "Unconnected" ) )
	{
		print( FATAL, "Heater seems not to be properly connected.\n" );
		THROW( EXCEPTION );
	}
	else if ( ! strcmp( buf + 5, "Shorted" ) )
	{
		print( FATAL, "Short circuit of heater detected.\n" );
		THROW( EXCEPTION );
	}
	else if ( strcmp( buf + 5, "OK" ) )
		fsps25_wrong_data( );

	return fsps25.heater_state = buf[ 3 ] == '1';
}


/*---------------------------------------*
 * Function for setting the heater state
 *---------------------------------------*/

static void
fsps25_set_heater_state( bool state )
{
	char buf[ 8 ];
	int retries = 30;


	fsc2_assert( fsps25.state != STATE_OFF );
	fsc2_assert( fsps25.sweep_state != SWEEPING );

	if (    state == fsps25.heater_state
		 || FSC2_MODE != EXPERIMENT )
	{
		fsps25.heater_state = state;
		return;
	}

	/* If we're going to switch the heater on the current through the
	   coil and the current output by the power supply must be matched.
	   The only exception is the case that the power supply got switched
	   off in an uncontrolled way (e.g. because of a power failure) and
	   we now want to get it back to normal, in which case a mismatch
	   might be acceptable. */

	if ( state )
	{
		if ( ! fsps25.is_matched )
		{
			print( FATAL, "Can't switch on heater while power supply current "
				   "doesn't match that through the sweep coil.\n" );
			THROW( EXCEPTION );
		}

		if ( fsps25.state == STATE_PFAIL )
		{
			char *warn;
			long coil_current = lrnd( 1.0e3 * fsps25_get_super_current( ) );
			long ps_current   = lrnd( 1.0e3 * fsps25_get_act_current( ) );

			if ( coil_current == ps_current )
				warn = get_string( "Power supply status is in PFail mode.\n"
								   "Really switch heater on at a current\n"
								   "of %.3f A?", 1.0e-3 * coil_current );
			else
				warn = get_string( "Power supply status is in PFail mode.\n"
								   "You're about to switch on the heater\n"
								   "at a current of %.3f A and a coil\n"
								   "current of %.3f A. Really continue?",
								   1.0e-3 * ps_current, 1.0e-3 * coil_current );

			if ( 2 != show_choices( warn, 2, "Abort", "Yes", NULL, 1 ) )
			{
				T_free( warn );
				THROW( EXCEPTION );
			}

			T_free( warn );
		}
	}

	sprintf( buf, "AHS %c;\r", state ?  '1' : '0' );
	if ( fsc2_serial_write( SERIAL_PORT, buf, 7, RESPONSE_TIME, UNSET ) != 7 )
		fsps25_comm_fail( );

	fsps25_get_command_reply( );

	/* Switching the heater on or off takes some time to take effect */

	fsc2_usleep( state ? HEATER_OFF_DELAY : HEATER_OFF_DELAY, UNSET );

	/* Give the heater some more time to get set */

	while ( retries-- > 0 )
	{
		if ( fsps25_get_heater_state( ) == state )
		{
			fsps25_get_super_current( );
			return;
		}
		fsc2_usleep( 100000, UNSET );
	}

	print( FATAL, "Failed to switch heater %s.\n", state ? "on" : "off" );
	THROW( EXCEPTION );
}


/*------------------------------------------------------*
 * Function for aborting a sweep. Returns OK on success 
 * and FAIL on failure to stop the sweep.
 *------------------------------------------------------*/

static bool
fsps25_abort_sweep( void )
{
	char buf[ 5 ] = "DCS!\r";


	fsc2_assert( fsps25.state != STATE_OFF );

	if ( fsps25.sweep_state != STOPPED )
		return OK;

	if ( FSC2_MODE != EXPERIMENT )
	{
		fsps25.sweep_state = STOPPED;
		return OK;
	}

	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
		  || fsc2_serial_read( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) < 4 )
		fsps25_comm_fail( );

	if ( strncmp( buf, "SC;\r", 4 ) && strncmp( buf, "CCS;\r", 5 ) )
		fsps25_wrong_data( );

	if ( ! strncmp( buf, "CCS;\r", 5 ) )
		return FAIL;

	fsps25_get_state( );
	fsps25_get_act_current( );

	return OK;
}


/*--------------------------------------------------------------------*
 * Function for querying the actual current output by the power suply
 *--------------------------------------------------------------------*/

static long
fsps25_get_act_current( void )
{
	char buf[ 11 ] = "GAC?\r";
	long current;


	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.act_current;

	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
	     || fsc2_serial_read( SERIAL_PORT, buf, 11, RESPONSE_TIME, UNSET )
			                                                            != 11 )
		fsps25_comm_fail( );

	if (    strncmp( buf, "AC ", 3 )
		 || ( buf[ 3 ] != '+' && buf[ 3 ] != '-' )
		 || buf[ 9 ] != ';' 
		 || buf[ 10 ] != '\r' )
		fsps25_wrong_data( );

	buf[ 9 ] = '\0';
	current = T_atol( buf + 3 );

	if ( labs( current ) > MAX_CURRENT )
		fsps25_wrong_data( );

	if ( labs( current ) > MAX_ALLOWED_CURRENT )
	{
		print( FATAL, "Reported current of %.3f A is not within the allowed "
			   "range.\n", 1.0e-3 * current );
		THROW( EXCEPTION );
	}

	if ( labs( current ) > fsps25.max_current )
	{
		print( FATAL, "Reported current of %.3f A is not within the currently "
			   "set limits.\n", 1.0e-3 * current );
		THROW( EXCEPTION );
	}

	fsps25.act_current = current;
	if ( fsps25.heater_state == HEATER_ON )
		fsps25.super_current = current;

	return current;
}


/*------------------------------------------------------------*
 * Function for setting the current output by the power suply
 *------------------------------------------------------------*/

static long
fsps25_set_act_current( long current )
{
	char buf[ 13 ];
	unsigned long delay;
	unsigned long short_delay;
	int retries = 30;


	fsc2_assert( fsps25.state != STATE_OFF );

	sprintf( buf, "GTC %+06ld;\r", current );
	if ( fsc2_serial_write( SERIAL_PORT, buf, 12, RESPONSE_TIME, UNSET ) != 12 )
		fsps25_comm_fail( );
		
	fsps25_get_command_reply( );

	/* Calculate how long the sweep is going to take (in us) and wait for
	   this time (but give the user a chance to abort the sweep in between) */

	delay = ulrnd( ( 6.0e7 * labs( current - fsps25.act_current ) ) /
				   ( fsps25.heater_state ?
					 fsps25.act_speed : HEATER_OFF_SPEED ) );

	short_delay = delay > 100000 ? 100000 : delay;

	while ( 1 )
	{
		fsc2_usleep( short_delay, UNSET );
		if ( check_user_request( ) )
		{
			fsps25_abort_sweep( );
			return fsps25_get_act_current( );
		}

		if ( delay < 100000 )
			break;

		delay -= 100000;
		if ( delay < 100000 )
			short_delay = delay;
	}

	/* Check if the sweep has stopped, if not give it a bit more time (3s) */

	while ( retries-- > 0 )
	{
		fsps25_get_state( );
		if ( fsps25.sweep_state == STOPPED )
			break;
		
		fsc2_usleep( 100000, UNSET );

		if ( check_user_request( ) )
		{
			fsps25_abort_sweep( );
			return 1.0e-3 * fsps25.act_current;
		}
	}

	/* Now make sure the sweep has stopped and the target current has been
	   reached */

	if ( fsps25.sweep_state == SWEEPING )
	{
		fsps25_abort_sweep( );
		print( FATAL, "Failed to reach target current in time.\n" );
		THROW( EXCEPTION );
	}

	fsps25_get_act_current( );
	if ( current != fsps25.act_current )
	{
		print( FATAL, "Failed to reach target current.\n" );
		THROW( EXCEPTION );
	}

	return current;
}


/*----------------------------------------------------------------------*
 * Function for querying the current through the coil (which can differ
 * from the result of the "GAC?" command if the heater is off)
 *----------------------------------------------------------------------*/

static long
fsps25_get_super_current( void )
{
	char buf[ 11 ] = "GSC?\r";
	long current;


	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
	     || fsc2_serial_read( SERIAL_PORT, buf, 11, RESPONSE_TIME, UNSET )
			                                                            != 11 )
		fsps25_comm_fail( );

	if (    strncmp( buf, "AC ", 3 )
		 || ( buf[ 3 ] != '+' && buf[ 3 ] != '-' )
		 || buf[ 9 ] != ';'
		 || buf[ 10 ] != '\r' )
		fsps25_wrong_data( );

	buf[ 9 ] = '\0';
	current = T_atol( buf + 3 );

	if ( labs( current ) > MAX_CURRENT )
		fsps25_wrong_data( );

	if ( labs( current ) > MAX_ALLOWED_CURRENT )
	{
		print( FATAL, "Sweep coil current isn't within the allowed range.\n" );
		THROW( EXCEPTION );
	}

	if ( labs( current ) > fsps25.max_current )
	{
		print( FATAL, "Reported coil current of %.3f A is not within the "
			   "currently set limits.\n", 1.0e-3 * current );
		THROW( EXCEPTION );
	}

	return fsps25.super_current = current;
}


/*---------------------------------------------------------*
 * Function for querying the maximum current (if necessary
 * reducing the value to the maximum allowed current)
 *---------------------------------------------------------*/

static long
fsps25_get_max_current( void )
{
	char buf[ 10 ] = "GMC?\r";
	long current;


	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
	     || fsc2_serial_read( SERIAL_PORT, buf, 10, RESPONSE_TIME, UNSET )
			                                                            != 10 )
		fsps25_comm_fail( );

	if ( strncmp( buf, "MC ", 3 ) || buf[ 8 ] != ';' || buf[ 9 ] != '\r' )
		fsps25_wrong_data( );

	buf[ 8 ] = '\0';
	current = T_atol( buf + 3 );

	if ( current < 0 || current > MAX_CURRENT )
		fsps25_wrong_data( );

	if ( current > MAX_ALLOWED_CURRENT )
	{
		print( SEVERE, "Reported maximum current value is not within the "
			   "allowed range, reducing setting this limit.\n" );
		return fsps25_set_max_current( MAX_ALLOWED_CURRENT );
	}

	return fsps25.max_current = current;
}


/*------------------------------------------*
 * Function for setting the maximum current
 *------------------------------------------*/

static long
fsps25_set_max_current( long current )
{
	char buf[ 12 ];


	fsc2_assert( fsps25.state != STATE_OFF );

	if ( current < 0 )
	{
		print( FATAL, "Invalid negativ value for maximum current.\n" );
		THROW( EXCEPTION );
	}

	if ( current > MAX_CURRENT )
	{
		print( FATAL, "Requested maximum current is larger than maximum "
			   "possible current.\n" );
		THROW( EXCEPTION );
	}

	if ( current > MAX_ALLOWED_CURRENT )
	{
		print( FATAL, "Requested maximum current is larger than maximum "
			   "allowed current.\n" );
		THROW( EXCEPTION );
	}

	/* We need the to have the newest value of the actual current (we might
	   be in a sweep at this moment) and refuse to set a maximum current that's
	   lower than this actual current */

	fsps25_get_act_current( );
	if ( current > labs( fsps25.act_current ) )
	{
		print( FATAL, "Can't set maximum current to a value lower than the "
			   "actual current.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == EXPERIMENT )
	{
		sprintf( buf, "SMC %05ld;\r", current );
		if ( fsc2_serial_write( SERIAL_PORT, buf, 11,
								RESPONSE_TIME, UNSET ) != 11 )
			fsps25_comm_fail( );

		fsps25_get_command_reply( );
	}

	return fsps25.max_current = current;
}


/*-----------------------------------------------------------*
 * Function for querying the sweep speed of the power supply
 *-----------------------------------------------------------*/

static long
fsps25_get_sweep_speed( void )
{
	char buf[ 9 ] = "GAS?\r";
	long raw_speed;


	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
	     || fsc2_serial_read( SERIAL_PORT, buf, 9, RESPONSE_TIME, UNSET ) != 9 )
		fsps25_comm_fail( );

	if ( strncmp( buf, "AS ", 3 ) || buf[ 7 ] != ';' || buf[ 8 ] != '\r' )
		fsps25_wrong_data( );

	buf[ 7 ] = '\0';
	raw_speed = T_atol( buf + 3 );

	if ( raw_speed < MIN_SPEED )
		fsps25_wrong_data( );

	if ( raw_speed > fsps25.max_speed )
		print( SEVERE, "Maximum sweep speed is larger than currently set limit "
			   "of %.3f mA/s.\n", fsps25.max_speed / 60.0 );

	return fsps25.act_speed = raw_speed;
}


/*----------------------------------------------------------------------*
 * Function for setting the sweep speed (in mA/min) of the power supply
 *----------------------------------------------------------------------*/

static long
fsps25_set_sweep_speed( long speed )
{
	char buf[ 11 ];


	fsc2_assert( fsps25.state != STATE_OFF );

	sprintf( buf, "SAS %04ld;\r", speed );
	if ( fsc2_serial_write( SERIAL_PORT, buf, 10, RESPONSE_TIME, UNSET ) != 10 )
		fsps25_comm_fail( );

	fsps25_get_command_reply( );

	fsps25.max_speed = speed;
	return speed;
}


/*---------------------------------------------------------------*
 * Function for querying the maximum sweep speed that can be set
 *---------------------------------------------------------------*/

static long
fsps25_get_max_sweep_speed( void )
{
	char buf[ 9 ] = "GMS?\r";
	long raw_speed;


	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
	     || fsc2_serial_read( SERIAL_PORT, buf, 9, RESPONSE_TIME, UNSET ) != 9 )
		fsps25_comm_fail( );

	if ( strncmp( buf, "MS ", 3 ) || buf[ 7 ] != ';' || buf[ 8 ] != '\r' )
		fsps25_wrong_data( );

	buf[ 7 ] = '\0';
	raw_speed = T_atol( buf + 3 );

	if ( raw_speed < MIN_SPEED )
		fsps25_wrong_data( );

	if ( raw_speed > MAX_SPEED )
		print( SEVERE, "Maximum sweep speed is larger than upper limit of "
			   "%.3f mA/s.\n", MAX_SPEED / 6.0e4 );

	return fsps25.max_speed = raw_speed;
}


/*--------------------------------------------------------------*
 * Function for setting the maximum sweep speed that can be set
 *--------------------------------------------------------------*/

static long
fsps25_set_max_sweep_speed( long max_speed )
{
	char buf[ 11 ];


	sprintf( buf, "SMS %4ld;\r", max_speed );
	if ( fsc2_serial_write( SERIAL_PORT, buf, 10, RESPONSE_TIME, UNSET ) != 10 )
		fsps25_comm_fail( );

	fsps25_get_command_reply( );

	/* Please note: setting a new maximum to a value lower than the currently
	   set sweep speed automatically reduces the sweep sweep */

	if ( max_speed < fsps25.act_speed  )
		fsps25.act_speed = max_speed;

	return fsps25.max_speed = max_speed;
}


/*-----------------------------------------------------*
 * Function for opening the device file for the device
 * and setting up the communication parameters.
 *-----------------------------------------------------*/

static void
fsps25_open( void )
{
#ifndef FSPS25_TEST

    /* We need exclusive access to the serial port and we also need non-
       blocking mode to avoid hanging indefinitely if the other side does not
       react. O_NOCTTY is set because the serial port should not become the
       controlling terminal, otherwise line noise read as a CTRL-C might kill
       the program. */

    if ( ( fsps25.tio = fsc2_serial_open( SERIAL_PORT, DEVICE_NAME,
                          O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
    {
        print( FATAL, "Can't open device file for monochromator.\n" );
        THROW( EXCEPTION );
    }

    fsps25.tio->c_cflag = 0;

    switch ( PARITY_TYPE )
    {
        case NO_PARITY :
            break;

        case ODD_PARITY :
            fsps25.tio->c_cflag |= PARENB | PARODD;
            break;

        case EVEN_PARITY :
            fsps25.tio->c_cflag |= PARENB;
            break;

        default :
            fsc2_serial_close( SERIAL_PORT );
            print( FATAL, "Invalid setting for parity bit in "
                   "configuration file for the device.\n" );
            THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_STOP_BITS )
    {
        case 1 :
            break;

        case 2 :
            fsps25.tio->c_cflag |= CSTOPB;
            break;

        default :
            fsc2_serial_close( SERIAL_PORT );
            print( FATAL, "Invalid setting for number of stop bits in "
                   "configuration file for the device.\n" );
            THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_BITS_PER_CHARACTER )
    {
        case 5 :
            fsps25.tio->c_cflag |= CS5;
            break;

        case 6 :
            fsps25.tio->c_cflag |= CS6;
            break;

        case 7 :
            fsps25.tio->c_cflag |= CS7;
            break;

        case 8 :
            fsps25.tio->c_cflag |= CS8;
            break;

        default :
            fsc2_serial_close( SERIAL_PORT );
            print( FATAL, "Invalid setting for number of bits per "
                   "in character configuration file for the device.\n" );
			THROW( EXCEPTION );
    }

    fsps25.tio->c_cflag |= CLOCAL | CREAD;
    fsps25.tio->c_iflag = IGNBRK;
    fsps25.tio->c_oflag = 0;
    fsps25.tio->c_lflag = 0;

    cfsetispeed( fsps25.tio, SERIAL_BAUDRATE );
    cfsetospeed( fsps25.tio, SERIAL_BAUDRATE );

    /* We can't use canonical mode here... */

    fsps25.tio->c_lflag = 0;

    fsc2_tcflush( SERIAL_PORT, TCIOFLUSH );
    fsc2_tcsetattr( SERIAL_PORT, TCSANOW, fsps25.tio );

#endif

    fsps25.is_open = SET;
}


/*--------------------------------------------------*
 * Function for receiving the response to a command
 *--------------------------------------------------*/

static void
fsps25_get_command_reply( void )
{
	char reply[ 2 ];


	if ( fsc2_serial_read( SERIAL_PORT, reply, 2, RESPONSE_TIME, UNSET ) != 2 )
		fsps25_comm_fail( );

	if ( strncmp( reply, "#\r", 2 ) )
		fsps25_wrong_data( );
}


/*-------------------------------------------------------*
 * Function to handle situations where the communication
 * with the device failed completely.
 *-------------------------------------------------------*/

static void
fsps25_comm_fail( void )
{
	print( FATAL, "Can't access the monochromator.\n" );
    THROW( EXCEPTION );
}


/*--------------------------------------------------*
 * Function for situations where the device reacted
 * but send a message it wasn't supposed to send.
 *--------------------------------------------------*/

static void
fsps25_wrong_data( void )
{
    print( FATAL, "Device send unexpected data.\n" );
    THROW( EXCEPTION );
}


/*------------------------------------------------------------*
 * Function for situations where the device is in PFAIL state
 *------------------------------------------------------------*/

static void
fsps25_pfail( void )
{
	/* Unless we're in the "expert mode" a PFAIL situation leads to an
	   immediate shutdown of the program */

	if ( ! fsps25.is_expert_mode )
	{
		print( FATAL, "Device is in PFAIL state!\n" );
		THROW( EXCEPTION );
	}
	else
		print( SEVERE, "Device is in PFAIL state!\n" );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
