/*
  $Id$

  Copyright (C) 2002 Anton Savitsky

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

#include "thurlby330.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Declaration of exported functions */

int thurlby330_init_hook( void );
int thurlby330_exp_hook( void );
int thurlby330_end_of_exp_hook( void );

Var *powersupply_name( Var *v );
Var *powersupply_damping( Var *v );
Var *powersupply_channel_state( Var *v );
Var *powersupply_set_voltage( Var *v );
Var *powersupply_get_voltage( Var *v );
Var *powersupply_set_current( Var *v );
Var *powersupply_get_current( Var *v );
Var *powersupply_command( Var *v );


/* Locally used functions */

static bool thurlby330_init( const char *name );
static bool thurlby330_command( const char *cmd );
static bool thurlby330_talk( const char *cmd, char *reply, long *length );
static void thurlby330_failure( void );
static long thurlby330_get_channel( Var *v );


#define TEST_VOLTAGE        12.12   /* in V */
#define TEST_CURRENT        1.001   /* in A */

#define MAX_VOLTAGE         32.0    /* in V */
#define MIN_VOLTAGE         0.0     /* in V */
#define VOLTAGE_RESOLUTION  1.0e-2  /* in V */
#define MAX_CURRENT         3.0     /* in A */
#define MIN_CURRENT         0.0     /* in A */
#define CURRENT_RESOLUTION  1.0e-3  /* in A */


typedef struct THURLBY330 THURLBY330;

struct THURLBY330 {
	int device;
};

static THURLBY330 thurlby330;


/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int thurlby330_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Reset several variables in the structure describing the device */

	thurlby330.device = -1;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int thurlby330_exp_hook( void )
{
	/* Initialize the power supply*/

	if ( ! thurlby330_init( DEVICE_NAME ) )
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

int thurlby330_end_of_exp_hook( void )
{
	/* Switch power supply back to local mode */

	if ( thurlby330.device >= 0 )
		gpib_local( thurlby330.device );

	thurlby330.device = -1;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *powersupply_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------*/
/* Swicth on/off damping of a power supply channel */
/*-------------------------------------------------*/

Var *powersupply_damping( Var *v )
{   
	long channel;
	long status;
	char buffer[ 100 ];
	char reply[ 100 ];
	long length = 100;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* First argument must be the channel number (1 or 2) */

	channel = thurlby330_get_channel( v );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing damping status argument.\n" );
		THROW( EXCEPTION );
	}
		
	/* Second argument must be 0 or "OFF" for OFF or not 0 or "ON" for ON */

	status = get_boolean( v );

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, status );

	sprintf( buffer, "DAMPING%ld %c\n", channel, status ? '1' : '0' );
	thurlby330_command( buffer );

	thurlby330_talk( "*OPC?\n", reply, &length );
	reply[ length - 1 ] = '\0';
	print( NO_ERROR, "OP Reply is : \"%s\"\n", reply );

	return vars_push( INT_VAR, status );
}


/*--------------------------------------*/
/* Switch on/off a power supply channel */
/*--------------------------------------*/

Var *powersupply_channel_state( Var *v )
{   
	long channel;
	long status;
	char buffer[ 100 ];
	char reply[ 100 ];
	long length = 100;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* First argument must be the channel number (1 or 2) */

	channel = thurlby330_get_channel( v );
    
	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing channel status argument.\n" );
		THROW( EXCEPTION );
	}

	/* Second argument must be 0 or "OFF" for OFF and not 0 or "ON" for ON */

	status = get_boolean( v );

	too_many_arguments( v );

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, status );

	sprintf( buffer, "OP%ld %c\n", channel, status ? '1' : '0' );
	thurlby330_command( buffer );

	thurlby330_talk( "*OPC?\n", reply, &length );
	reply[ length - 1 ] = '\0';
	print( NO_ERROR, "OP Reply is : \"%s\"\n", reply );

	return vars_push( INT_VAR, status );
}


/*---------------------------------------------------------------*/
/* Function sets or returns the voltage at one of the 2 outputs. */
/* The argument must be the output number, either 1 or 2, the    */
/* second the voltage in the range between 0 V and 32 V (in 10mV */
/* steps). If there isn't a second argument the set output       */
/* voltage (which may differ from the real output voltage in     */
/* case the required current would be to high ) is returned      */
/* (which is initially set to 0 V).                              */
/*---------------------------------------------------------------*/

Var *powersupply_set_voltage( Var *v )
{
	long channel;
	double voltage = 0.0;
	char buffer[ 100 ];
	char reply[ 100 ];
	long length = 100;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* First argument must be the channel number (1 or 2) */

	channel = thurlby330_get_channel( v );

	/* If no second argument is specified return the actual setting */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == TEST )
			return vars_push( FLOAT_VAR, TEST_VOLTAGE );

		sprintf( buffer, "V%ld?\n", channel );
		thurlby330_talk( buffer, reply, &length);

		reply[ length - 2 ] = '\0';
		voltage = T_atod( reply + 3 );
		print( NO_ERROR, "Reply is : \"%f\"\n", voltage );
		return vars_push( FLOAT_VAR, voltage );
	}

	/* Second argument must be a voltage between 0 V and 32 V */

	voltage = get_double( v, "voltage" );

	if (  voltage < MIN_VOLTAGE ||
		  voltage >= MAX_VOLTAGE + 0.5 * VOLTAGE_RESOLUTION )
	{
		print( FATAL, "Voltage of %.1f V is out of valid range "
			   "(%.1f to %.1f V).\n", voltage, MIN_VOLTAGE, MAX_VOLTAGE );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, voltage );

	sprintf( buffer, "V%ld V %.2f\n", channel, voltage );
	thurlby330_command( buffer );

	return vars_push( FLOAT_VAR, voltage );
}


/*--------------------------------------------------------------*/
/* Function returns the actual voltage at one of the 2 outputs. */
/* The argument must be the output number, either 1 or 2.       */
/*--------------------------------------------------------------*/

Var *powersupply_get_voltage( Var *v )
{
	long channel;
	double voltage;
	char buffer[ 100 ];
	char reply[ 100 ];
	long length = 100;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* First argument must be the channel number (1 or 2) */

	channel = thurlby330_get_channel( v );

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, TEST_VOLTAGE );

	sprintf( buffer, "V%ldO?\n", channel );
	thurlby330_talk( buffer, reply, &length);

	reply[ length - 2 ] = '\0';
	voltage = T_atod( reply );
	print( NO_ERROR, "Reply is : \"%f\"\n", voltage );
	return vars_push( FLOAT_VAR, voltage );
}


/*-------------------------------------------------------------*/
/* Function sets current limit or returns the setted current   */
/* at one of the 2 outputs. The first argument must be the     */
/* output number, either 1 or 2, the second the current in the */ 
/* range between 0 A and 3 A (in 1mA steps). If there isn't a  */
/* second argument the setted output voltage is returned       */
/* (which is initially set to 0 V).                            */
/*-------------------------------------------------------------*/

Var *powersupply_set_current( Var *v )
{
	long channel;
	double current;
	char buffer[ 100 ];
	char reply[ 100 ];
	long length = 100;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* First argument must be the channel number (1 or 2) */

	channel = thurlby330_get_channel( v );

	/* If no second argument is specified return the actual setting */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == TEST )
			return vars_push( FLOAT_VAR, TEST_CURRENT );

		sprintf( buffer, "I%ld?\n", channel );
		thurlby330_talk( buffer, reply, &length);

		reply[ length - 2 ] = '\0';
		current = T_atod( reply + 3 );

		print( NO_ERROR, "Current is : %f\n", current );
		return vars_push( FLOAT_VAR, current );
	}

	/* Otherwise the second argument must be a current between 0 A and 3 A */

	current = get_double( v, "current" );

	if ( current < MIN_CURRENT ||
		 current >= MAX_CURRENT + 0.5 * CURRENT_RESOLUTION )
	{
		print( FATAL, "Current of %f A is out of valid range "
			   "(%.1f to %.1f A).\n", current, MIN_CURRENT, MAX_CURRENT );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, current );

	sprintf( buffer, "I%ld %.3f\n", channel, current );
	thurlby330_command( buffer );

	return vars_push( FLOAT_VAR, current );
}


/*---------------------------------------------------------*/
/* Function returns the actual voltage at one of the 2     */
/* outputs. The argument must be the output number, either */
/* 1 or 2                                                  */
/*---------------------------------------------------------*/

Var *powersupply_get_current( Var *v )
{
	long channel;
	double current;
	char buffer[ 100 ];
	char reply[ 100 ];
	long length = 100;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* First argument must be the channel number (1 or 2) */

	channel = thurlby330_get_channel( v );

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, TEST_CURRENT );

	sprintf( buffer, "I%ldO?\n", channel );
	thurlby330_talk( buffer, reply, &length);

	reply[ length - 2 ] = '\0';
	current = T_atod( reply );
	print( NO_ERROR, "Reply is : \"%f\"\n", current );
	return vars_push( FLOAT_VAR, current );
}


/*--------------------------------------------------------------*/
/* Function for sending a GPIB command directly to power supply */
/*--------------------------------------------------------------*/

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
			thurlby330_command( cmd );
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


/*--------------------------------------------------------*/
/* Internal functions for initialization of power suppply */ 
/*--------------------------------------------------------*/

static bool thurlby330_init( const char *name )
{
	if ( gpib_init_device( name, &thurlby330.device ) == FAILURE )
        return FAIL;

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool thurlby330_command( const char *cmd )
{
	if ( gpib_write( thurlby330.device, cmd, strlen( cmd ) ) == FAILURE )
		thurlby330_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool thurlby330_talk( const char *cmd, char *reply, long *length )
{
	if ( gpib_write( thurlby330.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( thurlby330.device, reply, length ) == FAILURE )
		thurlby330_failure( );
	return OK;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

static void thurlby330_failure( void )
{
	print( FATAL, "Communication with power supply failed.\n" );
	THROW( EXCEPTION );
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

static long thurlby330_get_channel( Var *v )
{
	long channel;


	channel = get_long( v, "channel number" );

	if ( channel < 1 || channel > 2 )
	{
		print( FATAL, "Invalid power supply channel number %ld, valid "
			   "channels are 1 and 2.\n", channel );
		THROW( EXCEPTION );
	}

	return channel;
}

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
