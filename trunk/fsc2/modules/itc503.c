/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2004 Jens Thoms Toerring
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


/* Include configuration information for the device */

#include "itc503.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define C2K_OFFSET   273.16         /* offset between Celsius and Kelvin */


#define MAX_RETRIES            10

#define SAMPLE_CHANNEL_1        1
#define SAMPLE_CHANNEL_2        2
#define SAMPLE_CHANNEL_3        3
#define DEFAULT_SAMPLE_CHANNEL  SAMPLE_CHANNEL_1

enum {
    LOCAL_AND_LOCKED,
    REMOTE_AND_LOCKED,
    LOCAL_AND_UNLOCKED,
    REMOTE_AND_UNLOCKED
};

#define UNIT_KELVIN            0
#define UNIT_CELSIUS           1
#define DEFAULT_UNIT           UNIT_KELVIN


int itc503_init_hook( void );
int itc503_exp_hook( void );
int itc503_end_of_exp_hook( void );


Var_T *temp_contr_name( Var_T *v );
Var_T *temp_contr_temperature( Var_T *v );
Var_T *temp_contr_sample_channel( Var_T *v );
Var_T *temp_contr_sensor_unit( Var_T *v );
Var_T *temp_contr_lock_keyboard( Var_T *v );
Var_T *temp_contr_command( Var_T *v );


static bool itc503_init( const char *name );
static double itc503_sens_data( void );
static void itc503_lock( int state );
static bool itc503_command( const char *cmd );
static long itc503_talk( const char *message, char *reply, long length );
static void itc503_gpib_failure( void );


static struct {
	int device;
	int lock_state;
	int sample_channel;
	int unit;
} itc503;



/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int itc503_init_hook( void )
{
	Need_GPIB = SET;
	itc503.device = -1;
	itc503.lock_state = REMOTE_AND_LOCKED;
	itc503.sample_channel = DEFAULT_SAMPLE_CHANNEL;
	itc503.unit = DEFAULT_UNIT;

	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int itc503_exp_hook( void )
{
	if ( ! itc503_init( DEVICE_NAME ) )
	{
		print( FATAL, "Initialization of device failed.\n" );
		THROW( EXCEPTION );
	}

	itc503.sample_channel = SAMPLE_CHANNEL_1;
	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int itc503_end_of_exp_hook( void )
{
	itc503_lock( LOCAL_AND_LOCKED );
	return 1;
}


/**********************************************************/
/*                                                        */
/*          EDL functions                                 */
/*                                                        */
/**********************************************************/

/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var_T *temp_contr_name( UNUSED_ARG Var_T *v )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------*/
/* Returns temperature reading from controller */
/* (in the currently selected units)           */
/*---------------------------------------------*/

Var_T *temp_contr_temperature( UNUSED_ARG Var_T *v )
{
	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR,
						  itc503.unit == UNIT_KELVIN ? 123.45 : -149.71 );

	return vars_push( FLOAT_VAR, itc503_sens_data( ) -
					  ( itc503.unit == UNIT_KELVIN ? 0.0 : C2K_OFFSET ) );
}


/*---------------------------------------------------------------------*/
/* Sets or returns the currentyl used sample channel (function returns */
/* either 1, 2 or 3 for channel A, B or C and accepts 1, 2, 3 or the   */
/* strings "A","B" or "C" as input arguments). Please note that the    */
/* number of sample channels that can be used is a compile time        */
/* constant that can be changed in the configuration file.             */
/*---------------------------------------------------------------------*/

Var_T *temp_contr_sample_channel( Var_T *v )
{
	long channel;

	if ( v == NULL )
		return vars_push( INT_VAR, ( long ) itc503.sample_channel );

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		channel = get_long( v, "channel number" );

		if ( channel < SAMPLE_CHANNEL_1 && channel > SAMPLE_CHANNEL_3 )
		{
			print( FATAL, "Invalid sample channel number (%ld).\n", channel );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( ( *v->val.sptr != 'A' && *v->val.sptr != 'B' &&
			   *v->val.sptr != 'C' ) || strlen( v->val.sptr ) != 1 )
		{
			print( FATAL, "Invalid sample channel (\"%s\").\n", v->val.sptr );
			THROW( EXCEPTION );
		}
		channel = ( long ) ( *v->val.sptr - 'A' );
	}

	if ( channel > NUM_SAMPLE_CHANNELS )
	{
		print( FATAL, "Device module is configured to use not more than %d "
			   "sample channels.\n", NUM_SAMPLE_CHANNELS );
		THROW( EXCEPTION );
	}

	itc503.sample_channel = channel;

	return vars_push( INT_VAR, itc503.sample_channel );
}


/*--------------------------------------------------------*/
/* Select or determine which unit (K or degree C) is used */
/* when reporting temperatures.                           */
/*--------------------------------------------------------*/

Var_T *temp_contr_sensor_unit( Var_T *v )
{
	long unit = 0;
	const char *in_units  = "KC";
	int i;


	if ( v == NULL )
		return vars_push( INT_VAR, itc503.unit );

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		unit = get_long( v, "unit number" );

		if ( unit < UNIT_KELVIN || unit > UNIT_CELSIUS )
		{
			print( FATAL, "Invalid unit number (%d).\n", unit );
			THROW( EXCEPTION );
		}
	}
	else
	{
		for ( i = 0; i < ( long ) strlen( in_units ); i++ )
			if ( toupper( *v->val.sptr ) == in_units[ i ] )
			{
				unit = i;
				break;
			}

		if ( strlen( v->val.sptr ) != 1 || i >= ( long ) strlen( in_units ) )
		{
			print( FATAL, "Invalid unit (\"%s\").\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}

	too_many_arguments( v );

	return vars_push( INT_VAR, itc503.unit = unit );
}


/*-----------------------------------------------------*/
/* If called with a zero argument the keyboard becomes */
/* unlocked during an experiment.                      */
/*-----------------------------------------------------*/

Var_T *temp_contr_lock_keyboard( Var_T *v )
{
	int lock;


	if ( v == NULL )
		lock = REMOTE_AND_LOCKED;
	else
	{
		lock = get_boolean( v ) ? REMOTE_AND_LOCKED : REMOTE_AND_UNLOCKED;
		too_many_arguments( v );

		if ( FSC2_MODE == EXPERIMENT )
			itc503_lock( lock );
	}

	return vars_push( INT_VAR, lock == REMOTE_AND_LOCKED ? 1L : 0L );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var_T *temp_contr_command( Var_T *v )
{
	char *cmd = NULL;


	CLOBBER_PROTECT( cmd );

	vars_check( v, STR_VAR );
	
	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
			itc503_command( cmd );
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


/**********************************************************/
/*                                                        */
/*       Internal (non-exported) functions                */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static bool itc503_init( const char *name )
{
	char buf[ 10 ];


	/* Initialize GPIB communication with the temperature controller */

	if ( gpib_init_device( name, &itc503.device ) == FAILURE )
		return FAIL;

	/* Set end of EOS character to '\n' */

	if ( gpib_write( itc503.device, "Q0\r", 3 ) == FAILURE )
		return FAIL;

	/* Switch device per default to remote state with local lockout */

	if ( itc503_talk( "C1\r", buf, 10 ) != 2 )
		return FAIL;

	return OK;
}


/*--------------------------------------------*/
/* Returns the measured temperature in Kelvin */
/*--------------------------------------------*/

static double itc503_sens_data( void )
{
	char buf[ 50 ];
	long len = 50;
	double temp;
	char cmd[ ] = "R*\r";


	cmd[ 1 ] = itc503.sample_channel + '0';
	len = itc503_talk( cmd, buf, sizeof buf );

	if ( buf[ 1 ] != '-' && buf[ 1 ] != '+' && ! isdigit( buf[ 1 ] ) )
	{
		print( FATAL, "Error reading temperature.\n" );
		THROW( EXCEPTION );
	}

	sscanf( buf + 1, "%lf", &temp );

	/* If the first character is a '+' or '-' the sensor is returning
	   temperatures om degree celsius */

	if ( ! isdigit( buf[ 1 ] ) )
		 temp += C2K_OFFSET;

	return temp;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static void itc503_lock( int state )
{
	const char *cmd = NULL;


	fsc2_assert( state >= LOCAL_AND_LOCKED && state <= REMOTE_AND_UNLOCKED );

	switch ( state )
	{
		case LOCAL_AND_LOCKED :
			cmd = "Q0\r";
			break;

		case REMOTE_AND_LOCKED :
			cmd = "Q1\r";
			break;

		case LOCAL_AND_UNLOCKED :
			cmd = "Q2\r";
			break;

		case REMOTE_AND_UNLOCKED :
			cmd = "Q3\r";
			break;
	}

	itc503_command( cmd );
	itc503.lock_state = state;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool itc503_command( const char *cmd )
{
	if ( gpib_write( itc503.device, cmd, strlen( cmd ) ) == FAILURE )
		itc503_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static long itc503_talk( const char *message, char *reply, long length )
{
	long len = length;
	int retries = MAX_RETRIES;


 start:

	if ( gpib_write( itc503.device, message, strlen( message ) ) == FAILURE )
		itc503_gpib_failure( );

 reread:

	stop_on_user_request( );

	len = length;
	if ( gpib_read( itc503.device, reply, &len ) == FAILURE )
		itc503_gpib_failure( );

	/* If device misunderstood the command send it again, repeat up to
	   MAX_RETRIES times */

	if ( reply[ 0 ] == '?' )
	{
		if ( retries-- )
			goto start;
		else
			itc503_gpib_failure( );
	}

	/* If the first character of the reply isn't equal to the first character
	   of the message we probably read the reply for a previous command and
	   try to read again... */

	if ( reply[ 0 ] != message[ 0 ] )
	{
		if ( retries-- )
			goto reread;
		else
			itc503_gpib_failure( );
	}

	return len;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static void itc503_gpib_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
