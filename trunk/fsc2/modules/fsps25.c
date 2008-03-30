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

Var_T * magnet_name( Var_T * v );
Var_T * magnet_field( Var_T * v );
Var_T * magnet_sweep_rate( Var_T * v );
Var_T * magnet_max_current( Var_T * v );
Var_T * magnet_max_sweep_rate( Var_T *v );
Var_T * magnet_request_expert_mode( Var_T * v );
Var_T * magnet_coil_current( Var_T * v );
Var_T * magnet_shutdown( Var_T * v  );
Var_T * magnet_heater_state( Var_T * v );


/* Local functions */

static void fsps25_init( void );
static void fsps25_on( bool shutdown_on_fail );
static void fsps25_off( void );
static void fsps25_get_state( bool shutdown_on_fail );
static int  fsps25_get_heater_state( void );
static void fsps25_set_heater_state( int state );
static void fsps25_set_expert_mode( bool state );
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
static bool fsps25_get_command_reply( void );
static void fsps25_comm_fail( void );
static void fsps25_fail_handler( void );
static void fsps25_wrong_data( void );


typedef struct {
	bool is_open;
	bool is_expert_mode;
	int state;
	bool sweep_state;
    int  heater_state;
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

#define RESPONSE_TIME          50000    /* 50 ms (for "normal commands) */

#define HEATER_OFF_DELAY       10000000  /* 10 s  */
#define HEATER_ON_DELAY        1500000   /* 1.5 s */

#define MAX_CURRENT_DIFF       10         /* 10 mA */

/* Defines for the parity used by the device */

#define NO_PARITY         0
#define ODD_PARITY        1
#define EVEN_PARITY       2


/* The different states the device can be in */

#define STATE_OFF         0
#define STATE_ON          1
#define STATE_PFAIL      -1


/* The different states the heater can be in */

#define HEATER_OFF        0
#define HEATER_ON         1
#define HEATER_FAIL      -1

/* The sweep states */

#define STOPPED           0
#define SWEEPING          1

/* The match states */

#define UNMATCHED         0
#define MATCHED           1


/* The sweep rate with the heater being off */

#define  MAX_HEATER_OFF_SPEED  6000   /* 6000 mA/min */


#define FSPS25_TEST_CURRENT  0


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
    /* Claim the serial port (throws exception on failure) */

    fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	fsps25.is_open = UNSET;
	fsps25.state = STATE_OFF;
	fsps25.sweep_state  = STOPPED;
	fsps25.heater_state = HEATER_OFF;
	fsps25.is_matched = SET;
	fsps25.act_current = fsps25.super_current = 0;
	fsps25.act_current_is_set = UNSET;
	fsps25.max_current = MAX_ALLOWED_CURRENT;
	fsps25.max_current_is_set = UNSET;
	fsps25.max_speed = fsps25.act_speed = MAX_SPEED;
    fsps25.max_speed_is_set = fsps25.act_speed_is_set = UNSET;
	fsps25.is_expert_mode = UNSET;

	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
fsps25_test_hook( void )
{
	fsps25_stored = fsps25;

    fsps25.state = STATE_ON;
    fsps25.heater_state = HEATER_ON;
    if ( ! fsps25.act_current_is_set )
        fsps25.act_current = fsps25.super_current = FSPS25_TEST_CURRENT;
    fsps25.is_matched = SET;

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
    if ( fsps25.is_open )
    {
        fsc2_serial_close( SERIAL_PORT );
        fsps25.is_open = UNSET;
    }

	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
magnet_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
magnet_request_expert_mode( Var_T * v  UNUSED_ARG )
{
    return vars_push( INT_VAR, ( long ) ( fsps25.is_expert_mode = SET ) );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
magnet_coil_current( Var_T * v  UNUSED_ARG )
{
    return vars_push( FLOAT_VAR, 1.0e-3 * fsps25_get_super_current( ) );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
magnet_shutdown( Var_T * v  UNUSED_ARG )
{
    fsps25_off( );
    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/

Var_T *
magnet_field( Var_T * v )
{
	double current;
	long raw_current;


	if ( v == NULL )
		return vars_push( FLOAT_VAR,
                          1.0e-3 * fsps25_get_super_current( ) );

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

    /* Can't set a current through the coil if heater is off */

    if ( fsps25.heater_state != HEATER_ON )
    {
        print( FATAL, "Can't set field while heater is off.\n" );
        THROW( EXCEPTION );
    }

    if ( fsps25.heater_state == HEATER_FAIL )
    {
        print( FATAL, "Heater is in failure state, can't set field, "
               "shutting device down.\n" );
        THROW( EXCEPTION );
    }

	if (    fsps25.act_current == raw_current
		 || FSC2_MODE != EXPERIMENT )
		fsps25.act_current_is_set = SET;
    else
        fsps25_set_act_current( raw_current );

	return vars_push( FLOAT_VAR,
					  1.0e-3 * fsps25_get_super_current( ) );
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
		print( FATAL, "Invalid negative sweep rate argument.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_speed < MIN_SPEED )
	{
		print( FATAL, "Sweep rate argument is too small.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_speed > fsps25.max_speed )
	{
		print( FATAL, "Sweep rate argument is larger than the currently set "
			   "upper limit of %.1f mA/s.\n", fsps25.max_speed / 60.0 );
		THROW( EXCEPTION );
	}

	if (    fsps25.act_speed == raw_speed
		 || FSC2_MODE != EXPERIMENT )
	{
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

	/* We need to have the newest value of the actual current (we might be in
       a sweep at this moment) and refuse to set a maximum current that's
       lower than this actual current */

    if ( FSC2_MODE == EXPERIMENT )
    {
        fsps25_get_act_current( );

        if ( raw_max_current < labs( fsps25.act_current ) - MAX_CURRENT_DIFF )
        {
            print( FATAL, "Can't set maximum current to a value lower than "
                   "the actual current.\n" );
            print( FATAL, "%ld < %ld\n", raw_max_current, fsps25.act_current );
            THROW( EXCEPTION );
        }
    }

	if (    raw_max_current == fsps25.max_current
		 || FSC2_MODE != EXPERIMENT )
	{
		fsps25.max_current_is_set = SET;
		fsps25.max_current = raw_max_current;
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
		print( FATAL, "Invalid negative maximum sweep rate argument.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_max_sweep_rate < MIN_SPEED )
	{
		print( FATAL, "Maximum sweep rate argument is to small.\n" );
		THROW( EXCEPTION );
	}

	if ( raw_max_sweep_rate > MAX_SPEED )
	{
		print( FATAL, "Maximum sweep rate argument is larger than maximum "
			   "possible value.\n" );
		THROW( EXCEPTION );
	}

	if (    fsps25.max_speed == raw_max_sweep_rate
		 || FSC2_MODE != EXPERIMENT )
	{
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

Var_T *
magnet_heater_state( Var_T * v )
{
    bool state;

    if ( v == NULL )
        return vars_push( INT_VAR, ( long ) fsps25_get_heater_state( ) );

    if ( ! fsps25.is_expert_mode )
    {
        print( FATAL, "This function can only be used whe expert mode "
               "has been requested.\n" );
        THROW( EXCEPTION );
    }

    state = get_boolean( v );
    too_many_arguments( v );
    fsps25_set_heater_state( state );

    return vars_push( INT_VAR, ( long ) fsps25_get_heater_state( ) );
}


/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/

static void
fsps25_init( void )
{
	FSPS25 target_state = fsps25;


	fsps25_open( );

	if ( fsps25.state == STATE_PFAIL && ! fsps25.is_expert_mode )
    {
        print( FATAL,
               "Device is in PFail state. Need expert mode to contuinue.\n" );
        print( WARN, "Last reported super current: %f A.\n",
               1.0e-3 * fsps25_get_super_current( ) );
        THROW( EXCEPTION );
    }

    /* There's no reason to keep the expert mode if the device isn't in
       PFail state */

    if ( fsps25.state == STATE_PFAIL )
        fsps25.is_expert_mode = UNSET;

	/* Switch to ON state if necessary */

	if ( fsps25.state == STATE_OFF )
		fsps25_on( UNSET );

    /* We can't continue if the heater is in failure state */

    if ( fsps25.heater_state == HEATER_FAIL )
        fsps25_fail_handler( );

    /* Determine the actual current output by the powetr supply */

    fsps25_get_act_current( );

	/* Set or get the values for the maximun current, the sweep rate and
	   the maximum sweep rate */

	if ( fsps25.max_current_is_set )
    {
        if (    labs( fsps25.act_current ) - MAX_CURRENT_DIFF >
                                                             fsps25.max_current
             || target_state.act_current > fsps25.max_current )
        {
            print( FATAL, "Can't set requested maximum current, it's lower "
                   "than either the actual current or the requested "
                   "current.\n" );
            THROW( EXCEPTION );
        }

		fsps25_set_max_current( fsps25.max_current );
    }
    else
		fsps25_get_max_current( );

	if ( fsps25.act_speed_is_set )
		fsps25_set_sweep_speed( fsps25.act_speed );
	else
		fsps25_get_sweep_speed( );

	if ( fsps25.max_speed_is_set )
		fsps25_set_max_sweep_speed( fsps25.max_speed );
	else
		fsps25_get_max_sweep_speed( );

    /* If we're in PFail state and expert mode is on we have to stop right
       here - switching on the heater must be done only on user request */

	if ( fsps25.state == STATE_PFAIL )
        return;

	/* We now need to switch on the heater */

	if ( ! fsps25.heater_state )
	{
		if ( ! fsps25.is_matched )
        {
            fsps25_get_super_current( );
			fsps25_set_act_current( fsps25.super_current );
        }

		if ( fsps25.is_matched )
            fsps25_set_heater_state( HEATER_ON );
        else
        {
            print( FATAL, "Current can't be matched to current through "
                   "sweep coil.\n" );
            THROW( EXCEPTION );
        }
	}
    else

	/* If a start current had been given set in now */

	if (    target_state.act_current_is_set
         && target_state.act_current != fsps25.act_current )
		fsps25_set_act_current( target_state.act_current );
}


/*-----------------------------------------*
 * Function for bringing the device online
 *-----------------------------------------*/

static void
fsps25_on( bool shutdown_on_fail )
{
	fsps25_get_state( shutdown_on_fail );

    if ( fsps25.state == STATE_ON || fsps25.state == STATE_PFAIL )
        return;

	if ( fsc2_serial_write( SERIAL_PORT, "ON!\r", 4,
							RESPONSE_TIME, UNSET ) != 4 )
		fsps25_comm_fail( );

	fsps25_get_state( shutdown_on_fail );

	/* Now the device should be in ON (or, in the worst case, in PFail) state
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

    if ( fsps25.heater_state == HEATER_FAIL )
    {
        print( FATAL, "Heater is in failure state.\n" );
        THROW( EXCEPTION );
    }

	if( fsps25.sweep_state == SWEEPING )
	{
		print( FATAL, "Current is sweeping without a command having been "
			   "issued.\n" );
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
		fsps25.heater_state = HEATER_OFF;
		fsps25.sweep_state = STOPPED;
		fsps25.is_matched = UNSET;
		return;
	}

	fsps25_get_state( UNSET );

	/* Make sure the device isn't sweeping anymore */

	if ( fsps25.sweep_state == SWEEPING )
		fsps25_abort_sweep( );

	/* Send it the command to shut down */

	if (    fsc2_serial_write( SERIAL_PORT, "DSD!\r", 5,
							   RESPONSE_TIME, UNSET ) != 5
		 || fsc2_serial_read( SERIAL_PORT, buf, 5,
							  RESPONSE_TIME, UNSET ) != 5 )
		fsps25_comm_fail( );

	if ( strncmp( buf, "DSD;\r", 5 ) )
		fsps25_wrong_data( );

	/* If the heater was on (or in fail state) wait enough time for the
       heater to get switched off */

	if ( fsps25.heater_state != HEATER_OFF )
		fsc2_usleep( HEATER_OFF_DELAY, UNSET );

	/* If the current wasn't zero wait long enough for the device to sweep
	   it down to zero */

	if ( fabs( fsps25.act_current ) > MAX_CURRENT_DIFF )
		fsc2_usleep( lrnd( ( 6.0e7 * fabs( fsps25.act_current ) )
						   / HEATER_OFF_SPEED ), UNSET );

	/* Check the state and give the device up to 3 seconds of extra time if
       it's not finished yet */

	while ( retries-- > 0 )
	{
		fsps25_get_state( UNSET );
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

#define MAX_RESPONSE_LEN 30 /* 28 */

static void
fsps25_get_state( bool shutdown_on_fail )
{
	char buf[ MAX_RESPONSE_LEN ] = "GCS?\r";
	ssize_t i;
    ssize_t count = 0;
    struct {
        const char *response;
        int state;
        bool sweep_state;
        bool heater_state;
        bool is_matched;
    } states[ ] = {
        { "OFF;",
          STATE_OFF, STOPPED, HEATER_OFF, UNMATCHED },
        { "Stopped HOff UnMatched;",
          STATE_ON, STOPPED, HEATER_OFF, UNMATCHED },
        { "Stopped HOff Matched;",
          STATE_ON, STOPPED, HEATER_OFF, MATCHED },
        { "Sweeping HOff;",
          STATE_ON, SWEEPING, HEATER_ON, UNMATCHED },
        { "Stopped HOn;",
          STATE_ON, STOPPED, HEATER_ON, MATCHED },
        { "Sweeping HOn;",
          STATE_ON, SWEEPING, HEATER_ON, MATCHED },
        { "Stopped HFail UnMatched;",
          STATE_ON, STOPPED, HEATER_FAIL, UNMATCHED },
        { "Stopped HFail Matched;",
          STATE_ON, STOPPED, HEATER_FAIL, MATCHED },
        { "PFail Stopped;",
          STATE_PFAIL, STOPPED, HEATER_OFF, UNMATCHED },
        { "PFail Sweeping;",
          STATE_PFAIL, SWEEPING, HEATER_OFF, UNMATCHED },
        { "PFail HFail;",
          STATE_PFAIL, STOPPED, HEATER_FAIL, UNMATCHED },
    };


	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
         || ( count = fsc2_serial_read( SERIAL_PORT, buf, MAX_RESPONSE_LEN, 
                                        RESPONSE_TIME, UNSET ) ) <= 0 )
        fsps25_comm_fail( );

    /* Find '\r' in the response and replace it by '\0' */

    for ( i = 0; i < count; i++ )
        if ( buf[ i ] == '\r' )
        {
            buf[ i ] = '\0';
            break;
        }

    if ( i >= count || strncmp( buf, "CS ", 3 ) )
        fsps25_wrong_data( );

    for ( i = 0; i < ( ssize_t ) NUM_ELEMS( states ); i++ )
        if ( ! strcmp( buf + 3, states[ i ].response ) )
        {
            fsps25.state = states[ i ].state;
            fsps25.sweep_state = states[ i ].sweep_state;
            fsps25.heater_state = states[ i ].heater_state;
            fsps25.is_matched = states[ i ].is_matched;

            if ( ! shutdown_on_fail )
                return;

            if (    fsps25.state == STATE_PFAIL
                 || fsps25.heater_state == HEATER_FAIL )
                fsps25_fail_handler( );
        }

    fsps25_wrong_data( );
}


/*-----------------------------------------------------*
 * Function for asking the device for the heater state
 *-----------------------------------------------------*/

static int
fsps25_get_heater_state( void )
{
	char buf[ 6 ] = "GHS?\r";


	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.heater_state;

	fsc2_assert( fsps25.state != STATE_OFF );

	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
		 || fsc2_serial_read( SERIAL_PORT, buf, 6,
                              RESPONSE_TIME, UNSET ) <= 0 )
		fsps25_comm_fail( );

    if ( ! strncasecmp( buf, "HS 1;\r", 6 ) )
        fsps25.heater_state = HEATER_ON;
    else if ( ! strncasecmp( buf, "HS 0;\r", 6 ) )
        fsps25.heater_state = HEATER_OFF;
    else
		fsps25_wrong_data( );
         
	return fsps25.heater_state;
}


/*---------------------------------------*
 * Function for setting the heater state
 *---------------------------------------*/

static void
fsps25_set_heater_state( int state )
{
	int retries = 30;


	fsc2_assert( fsps25.state != STATE_OFF );
	fsc2_assert( fsps25.sweep_state != SWEEPING );
	fsc2_assert( fsps25.heater_state != HEATER_FAIL );
    fsc2_assert( state == HEATER_ON || state == HEATER_OFF );

	if ( state == fsps25.heater_state )
        return;

    if ( FSC2_MODE != EXPERIMENT )
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

	if ( state && ! fsps25.is_matched )
	{
		if ( ! fsps25.is_expert_mode )
		{
			print( FATAL, "Can't switch on heater while power supply current "
				   "doesn't match that through the sweep coil.\n" );
			THROW( EXCEPTION );
		}
        else
		{
			char *warn;
			long coil_current = fsps25_get_super_current( );
			long ps_current   = fsps25_get_act_current( );

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
                return;
			}

			T_free( warn );

            fsps25_set_expert_mode( SET );
		}
	}

	if ( fsc2_serial_write( SERIAL_PORT, state ? "AHS 1;\r" : "AHS 0;\r", 7,
                            RESPONSE_TIME, UNSET ) != 7 )
		fsps25_comm_fail( );

	if ( ! fsps25_get_command_reply( ) )
    {
        print( FATAL, "Device did not accept AHS command.\n" );
        THROW( EXCEPTION );
    }

	/* Switching the heater on or off takes some time to take effect */

	fsc2_usleep( state ? HEATER_ON_DELAY : HEATER_OFF_DELAY, UNSET );

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
 *------------------------------------------------------*/

static void
fsps25_set_expert_mode( bool state )
{
	if ( fsc2_serial_write( SERIAL_PORT, state ? "SEM 1;\r" : "SEM 0\r", 7,
                            RESPONSE_TIME, UNSET ) != 7 )
		fsps25_comm_fail( );

	if ( ! fsps25_get_command_reply( ) )
    {
        print( FATAL, "Device did not accept SEM command.\n" );
        THROW( EXCEPTION );
    }
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

	if ( fsps25.sweep_state == STOPPED )
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
    {
        print( SEVERE, "Failed to stop running sweep.\n" );
        return FAIL;
    }

	fsps25_get_state( SET );

    if ( fsps25.sweep_state == SWEEPING )
    {
        print( FATAL, "Failed to stop running sweep.\n" );
        THROW( EXCEPTION );
    }

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

	if ( labs( current ) - MAX_CURRENT_DIFF > MAX_CURRENT )
		fsps25_wrong_data( );

	if ( labs( current ) - MAX_CURRENT_DIFF > MAX_ALLOWED_CURRENT )
	{
		print( FATAL, "Reported current of %.3f A is not within the allowed "
			   "range.\n", 1.0e-3 * current );
		THROW( EXCEPTION );
	}

	if ( labs( current ) - MAX_CURRENT_DIFF > fsps25.max_current )
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
	fsc2_assert( fsps25.heater_state != HEATER_FAIL );

	if ( FSC2_MODE != EXPERIMENT )
    {
        if ( fsps25.heater_state == HEATER_ON )
            fsps25.super_current = current;
		return fsps25.act_current = current;
    }

	sprintf( buf, "GTC %+06ld;\r", current );
	if ( fsc2_serial_write( SERIAL_PORT, buf, 12, RESPONSE_TIME, UNSET ) != 12 )
		fsps25_comm_fail( );
		
	if ( ! fsps25_get_command_reply( ) )
    {
        print( FATAL, "Device did not accept GTC command.\n" );
        THROW( EXCEPTION );
    }

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
            THROW( USER_BREAK_EXCEPTION );
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
		fsps25_get_state( UNSET );
        if (    ( fsps25.state == STATE_PFAIL && ! fsps25.is_expert_mode )
             || fsps25.heater_state == HEATER_FAIL )
            fsps25_fail_handler( );

		if ( fsps25.sweep_state == STOPPED )
			break;
		
		fsc2_usleep( 100000, UNSET );

		if ( check_user_request( ) )
		{
			fsps25_abort_sweep( );
            THROW( USER_BREAK_EXCEPTION );
		}
	}

	/* Now make sure the sweep has really stopped and the target current has
       been reached */

	if ( fsps25.sweep_state == SWEEPING )
	{
		fsps25_abort_sweep( );
		print( FATAL, "Failed to reach requested current in time.\n" );
		THROW( EXCEPTION );
	}

	fsps25_get_act_current( );
	if ( labs( current - fsps25.act_current ) > MAX_CURRENT_DIFF )
	{
		print( FATAL, "Failed to reach requested current.\n" );
		THROW( EXCEPTION );
	}

	return fsps25.act_current;
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


	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.super_current;

	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
	     || fsc2_serial_read( SERIAL_PORT, buf, 11, RESPONSE_TIME, UNSET )
			                                                            != 11 )
		fsps25_comm_fail( );

	if (    strncmp( buf, "SC ", 3 )
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

	if ( labs( current ) - MAX_CURRENT_DIFF > fsps25.max_current )
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


	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.max_current;

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

	if ( current - MAX_CURRENT_DIFF > MAX_ALLOWED_CURRENT )
	{
		print( SEVERE, "Reported maximum current value is not within the "
			   "allowed range, reducing maximum current this limit.\n" );
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

	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.max_current = current;

	/* We need the to have the newest value of the actual current (we might
	   be in a sweep at this moment) and refuse to set a maximum current that's
	   lower than this actual current */

	fsps25_get_act_current( );
	if ( current < labs( fsps25.act_current ) - MAX_CURRENT_DIFF )
	{
		print( FATAL, "Can't set maximum current to a value lower than the "
			   "actual current.\n" );
		THROW( EXCEPTION );
	}

    sprintf( buf, "SMC %05ld;\r", current );

    if ( fsc2_serial_write( SERIAL_PORT, buf, 11,
                            RESPONSE_TIME, UNSET ) != 11 )
        fsps25_comm_fail( );

    if ( ! fsps25_get_command_reply( ) )
    {
        print( FATAL, "Device did not accept SMC command.\n" );
        THROW( EXCEPTION );
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


	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.act_speed;

	if (    fsc2_serial_write( SERIAL_PORT, buf, 5, RESPONSE_TIME, UNSET ) != 5
	     || fsc2_serial_read( SERIAL_PORT, buf, 9, RESPONSE_TIME, UNSET )
                                                                         != 9 )
		fsps25_comm_fail( );

	if ( strncmp( buf, "AS ", 3 ) || buf[ 7 ] != ';' || buf[ 8 ] != '\r' )
		fsps25_wrong_data( );

	buf[ 7 ] = '\0';
	raw_speed = T_atol( buf + 3 );

	if ( raw_speed < MIN_SPEED )
		fsps25_wrong_data( );

	if ( raw_speed > fsps25.max_speed )
		print( SEVERE, "Maximum sweep rate is larger than currently set limit "
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

	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.act_speed = speed;

	sprintf( buf, "SAS %04ld;\r", speed );
	if ( fsc2_serial_write( SERIAL_PORT, buf, 10, RESPONSE_TIME, UNSET ) != 10 )
		fsps25_comm_fail( );

	if ( ! fsps25_get_command_reply( ) )
    {
        print( FATAL, "Device did not accept SAS command.\n" );
        THROW( EXCEPTION );
    }

	return fsps25.max_speed = speed;
}


/*---------------------------------------------------------------*
 * Function for querying the maximum sweep speed that can be set
 *---------------------------------------------------------------*/

static long
fsps25_get_max_sweep_speed( void )
{
	char buf[ 9 ] = "GMS?\r";
    long raw_speed;

	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.max_speed;

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
		print( SEVERE, "Maximum sweep rate is larger than upper limit of "
			   "%.1f mA/s.\n", MAX_SPEED / 60.0 );

	return fsps25.max_speed = raw_speed;
}


/*--------------------------------------------------------------*
 * Function for setting the maximum sweep speed that can be set
 *--------------------------------------------------------------*/

static long
fsps25_set_max_sweep_speed( long max_speed )
{
	char buf[ 11 ];


	if ( FSC2_MODE != EXPERIMENT )
		return fsps25.max_speed = max_speed;

	sprintf( buf, "SMS %4ld;\r", max_speed );
	if ( fsc2_serial_write( SERIAL_PORT, buf, 10, RESPONSE_TIME, UNSET ) != 10 )
		fsps25_comm_fail( );

	if ( ! fsps25_get_command_reply( ) )
    {
        print( FATAL, "Device did not accept SMS command.\n" );
        THROW( EXCEPTION );
    }

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
        print( FATAL, "Can't open device file for power supply.\n" );
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

    fsps25_get_state( UNSET );
    fsps25.is_open = SET;
}


/*--------------------------------------------------*
 * Function for receiving the response to a command
 *--------------------------------------------------*/

static bool
fsps25_get_command_reply( void )
{
	char reply[ 2 ];

	if ( fsc2_serial_read( SERIAL_PORT, reply, 2, RESPONSE_TIME, UNSET ) != 2 )
		fsps25_comm_fail( );

	if ( ( reply[ 0 ] != '#' && reply[ 0 ] != '?' ) || reply[ 1 ] != '\r' )
		fsps25_wrong_data( );

    return reply[ 0 ] == '#';
}


/*-------------------------------------------------------*
 * Function to handle situations where the communication
 * with the device failed completely.
 *-------------------------------------------------------*/

static void
fsps25_comm_fail( void )
{
	print( FATAL, "Can't access the power supply.\n" );
    THROW( EXCEPTION );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void
fsps25_fail_handler( void )
{
    if ( fsps25.state == STATE_PFAIL && fsps25.heater_state == HEATER_FAIL )
        print( FATAL, "Device is in power failure state and heater is "
               "also in failure mode, shutting device down.\n" );
    else if ( fsps25.state == STATE_PFAIL )
        print( FATAL, "Device is in power failure state, shutting it down.\n" );
    else if ( fsps25.heater_state == HEATER_FAIL )
        print( FATAL, "Heater failure, shutting device down.\n" );

    fsps25_off( );

    if ( fsps25.state == STATE_PFAIL )
        print( WARN, "Last reported super current: %f A.\n",
               1.0e-3 * fsps25_get_super_current( ) );

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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
