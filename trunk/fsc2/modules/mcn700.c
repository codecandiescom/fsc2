/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2004 Anton Savitsky
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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "mcn700.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Declaration of exported functions */

int mcn700_init_hook( void );
int mcn700_exp_hook( void );
int mcn700_end_of_exp_hook( void );
 
Var *powersupply_name( Var *v );
Var *powersupply_voltage( Var *v );
Var *powersupply_current( Var *v );
Var *powersupply_command( Var *v ); 


/* Locally used functions */

static bool mcn700_init( const char *name );
static double mcn700_set_voltage( double voltage );
static double mcn700_get_voltage( void );
static double mcn700_set_current( double current );
static double mcn700_get_current( void );
static void mcn700_setvoltage_completed( void );
static bool mcn700_command( const char *cmd );
static bool mcn700_talk( const char *cmd, char *reply, long *length );
static void mcn700_failure( void );


#define TEST_VOLTAGE		20.0   /* in V */
#define TEST_CURRENT		0.001	/* in A */

#define MAX_VOLTAGE			2000.0	/* in V */
#define MIN_VOLTAGE			0.0		/* in V */
#define VOLTAGE_RESOLUTION	1.0		/* in V */
#define MAX_CURRENT			0.300	/* in A */
#define MIN_CURRENT			0.000	/* in A */
#define CURRENT_RESOLUTION	1.0e-3	/* in A */


static struct {
	int device;
} mcn700;


/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int mcn700_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Reset variables in the structure describing the state of the device */

	mcn700.device = -1;

	return 1;
}


/*---------------------------------------------------*
 * Start of experiment hook function for the module.
 *---------------------------------------------------*/

int mcn700_exp_hook( void )
{
	/* Initialize the power supply*/

	if ( ! mcn700_init( DEVICE_NAME ) )
	{
		print( FATAL, "Initialization of device failed: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	return 1;
}


/*-------------------------------------------------*
 * End of experiment hook function for the module.
 *-------------------------------------------------*/

int mcn700_end_of_exp_hook( void )
{
	/* Switch power supply back to local mode */

	if ( mcn700.device >= 0 ) {
		mcn700_command( "F0\n" );
		gpib_local( mcn700.device );
	}

	mcn700.device = -1;

	return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var *powersupply_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------------------------------------------*
 * Function sets or returns the voltage.
 * If called with a non-NULL argument it must be the voltage in the
 * range between 0 V and 2000 V (in 1V steps). If there isn't an
 * argument the voltage at the output gets returned (which may be
 * smaller than voltage that had been set due to the current limit).
 * NB: when setting the voltage the program waits until the 
 * "Spannungsregelung" is activated (fixed voltage mode)
 *------------------------------------------------------------------*/

Var *powersupply_voltage( Var *v )
{
	double voltage;


	/* If no argument is specified return the actual setting */

	if ( v == NULL )
	{
		if ( FSC2_MODE == TEST )
			return vars_push( FLOAT_VAR, TEST_VOLTAGE );
		return vars_push( FLOAT_VAR, mcn700_get_voltage( ) );
	}

	/* Otherwise the argument must be a voltage between 0 V and 2000 V */

	voltage = get_double( v, "voltage" );

	if ( voltage < MIN_VOLTAGE || voltage >= MAX_VOLTAGE )
	{
		print( FATAL, "Voltage of %.1f V is out of valid range "
			   "(%.1f to %.1f V).\n", voltage, MIN_VOLTAGE, MAX_VOLTAGE );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, voltage );

	return vars_push( FLOAT_VAR, mcn700_set_voltage( voltage ) );
}


/*---------------------------------------------------------------*
 * Function sets or returns the current.
 * The argument must be the current in the range between 0mA and
 * 300mA (in 1mA steps). If there isn't a argument the
 * current at the output is returned (which may be smaller than
 * current that had been set due to the voltage limit).
 *---------------------------------------------------------------*/

Var *powersupply_current( Var *v )
{
	double current;


	/* If no argument is specified return the actual setting */

	if ( v == NULL )
	{
		if ( FSC2_MODE == TEST )
			return vars_push( FLOAT_VAR, TEST_VOLTAGE );
		return vars_push( FLOAT_VAR, mcn700_get_current(  ) );
	}

	/* Otherwise the argument must be a voltage between 0mA and 300mA */

	current = get_double( v, "current" );

	if ( current < MIN_CURRENT || current >= MAX_CURRENT )
	{
		print( FATAL, "CURRENT of %.3f A is out of valid range "
			   "(%.3f to %.3f A).\n", current, MIN_CURRENT, MAX_CURRENT );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, current );

	return vars_push( FLOAT_VAR, mcn700_set_current( current ) );
}


/*---------------------------------------------------------------*
 * Function for sending a GPIB command directly to power supply.
 *---------------------------------------------------------------*/

Var *powersupply_command( Var *v )
{
	char *cmd = NULL;


	CLOBBER_PROTECT( cmd );

	vars_check( v, STR_VAR );
	
	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
			mcn700_command( cmd );
			T_free( cmd );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( cmd );
			RETHROW( );
		}
	}

	return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------*
 * Internal functions for initialization of power suppply.
 *---------------------------------------------------------*/

static bool mcn700_init( const char *name )
{
	if ( gpib_init_device( name, &mcn700.device ) == FAILURE )
		return FAIL;

	/* Set maximum integration time for measurements, command terminator to
	   none and have the device raise EOI, and switch power supply on */

	mcn700_command( "S7\n" ); 
	mcn700_command( "Y2\n" );
	mcn700_command( "F1\n" );

	return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double mcn700_set_voltage( double voltage )
{
	char buffer[ 100 ];

	fsc2_assert( voltage >= MIN_VOLTAGE &&
				 voltage <= MAX_VOLTAGE );

	sprintf( buffer, "U%.2f\n", voltage );
	mcn700_command( buffer );
	mcn700_setvoltage_completed( );

	return voltage;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double mcn700_get_voltage( void )
{
	char buffer[ 100 ];
	char reply[ 100 ];
	long length = 100;


	sprintf( buffer, "N0\n" );
	mcn700_talk( buffer, reply, &length );
	reply[ length - 2 ] = '\0';

	return T_atod( reply );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double mcn700_set_current( double current )
{
	char buffer[ 100 ];


	fsc2_assert( current >= MIN_CURRENT &&
				 current <= MAX_CURRENT);

	sprintf( buffer, "I%.3f\n", current );
	mcn700_command( buffer );

	return current;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double mcn700_get_current( void )
{
	char buffer[ 100 ];
	char reply[ 100 ];
	long length = 100;


	sprintf( buffer, "N1\n" );
	mcn700_talk( buffer, reply, &length );
	reply[ length - 2 ] = '\0';

	return T_atod( reply );
}


/*---------------------------------------------------------*
 * Function returns when the "Spannungsregelung" is active
 *---------------------------------------------------------*/

void mcn700_setvoltage_completed( void )
{
	unsigned char stb = 0; 

	while ( ! ( stb & 0x04 ) )
	{
		fsc2_usleep( 10000, UNSET );

		if ( gpib_serial_poll( mcn700.device, &stb ) == FAILURE )
			mcn700_failure( );

		stop_on_user_request( );
	}
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool mcn700_command( const char *cmd )
{
	if ( gpib_write( mcn700.device, cmd, strlen( cmd ) ) == FAILURE )
		mcn700_failure( );

	fsc2_usleep( 20000, UNSET );

	return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool mcn700_talk( const char *cmd, char *reply, long *length )
{
	if ( gpib_write( mcn700.device, cmd, strlen( cmd ) ) == FAILURE )
		mcn700_failure( );

	fsc2_usleep( 20000, UNSET );

	if ( gpib_read( mcn700.device, reply, length ) == FAILURE )
		mcn700_failure( );

	fsc2_usleep( 20000, UNSET );

	return OK;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void mcn700_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
