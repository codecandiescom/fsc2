/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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

#include "lakeshore330.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define SAMPLE_CHANNEL_A       0
#define SAMPLE_CHANNEL_B       1
#define DEFAULT_SAMPLE_CHANNEL SAMPLE_CHANNEL_B

#define LOCK_STATE_LOCAL       0
#define LOCK_STATE_REMOTE      1
#define LOCK_STATE_REMOTE_LLO  2

#define UNIT_KELVIN            0
#define UNIT_CELSIUS           1
#define UNIT_SENSOR            2

#define UNIT_VOLTS             2
#define UNIT_OHMS              3
#define UNITS_MILLIVOLTS       4

#define DEFAULT_UNIT           UNIT_KELVIN


int lakeshore330_init_hook( void );
int lakeshore330_exp_hook( void );
int lakeshore330_end_of_exp_hook( void );
void lakeshore330_exit_hook( void );


Var *temp_contr_name( Var *v );
Var *temp_contr_temperature( Var *v );
Var *temp_contr_sample_channel( Var *v );
Var *temp_contr_sensor_unit( Var *v );
Var *temp_contr_lock_keyboard( Var *v );


static bool lakeshore330_init( const char *name );
static double lakeshore330_sens_data( void );
static long lakeshore330_sample_channel( long channel );
static void lakeshore330_set_unit( long unit );
static long lakeshore330_get_unit( void );
static void lakeshore330_lock( int state );
static void lakeshore330_gpib_failure( void );


typedef struct {
	int device;
	int lock_state;
	int sample_channel;
	long unit;
} LAKESHORE330;


static LAKESHORE330 lakeshore330;


/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int lakeshore330_init_hook( void )
{
	need_GPIB = SET;
	lakeshore330.device = -1;
	lakeshore330.lock_state = LOCK_STATE_REMOTE_LLO;
	lakeshore330.sample_channel = DEFAULT_SAMPLE_CHANNEL;
	lakeshore330.unit = DEFAULT_UNIT;

	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int lakeshore330_exp_hook( void )
{
	if ( ! lakeshore330_init( DEVICE_NAME ) )
	{
		print( FATAL, "Initialization of device failed.\n" );
		THROW( EXCEPTION );
	}
	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int lakeshore330_end_of_exp_hook( void )
{
	lakeshore330_lock( 0 );
	return 1;
}


/**********************************************************/
/*                                                        */
/*          EDL functions                                 */
/*                                                        */
/**********************************************************/

/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *temp_contr_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------*/
/* Returns temperature reading from controller */
/*---------------------------------------------*/

Var *temp_contr_temperature( Var *v )
{
	v = v;

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, 123.45 );

	return vars_push( FLOAT_VAR, lakeshore330_sens_data( ) );
}


/*-----------------------------------------------------------------*/
/* Sets or returns sample channel (function returns either 1 or 2  */
/* for channel A or B and accepts 1 or 2 or the strings "A" or "B" */
/* as input arguments).                                            */
/*-----------------------------------------------------------------*/

Var *temp_contr_sample_channel( Var *v )
{
	long channel;

	if ( v == NULL )
		return vars_push( INT_VAR, lakeshore330.sample_channel );

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR || FLOAT_VAR ) )
	{
		channel = get_long( v, "channel number" ) - 1;

		if ( channel != SAMPLE_CHANNEL_A && channel != SAMPLE_CHANNEL_B )
		{
			print( FATAL, "Invalid sample channel number (%ld).\n", channel );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( ( *v->val.sptr != 'A' && *v->val.sptr != 'B' ) ||
			 strlen( v->val.sptr ) != 1 )
		{
			print( FATAL, "Invalid sample channel (\"%s\").\n", v->val.sptr );
			THROW( EXCEPTION );
		}
		channel = ( long ) ( *v->val.sptr - 'A' );
	}

	if ( FSC2_MODE == TEST )
	{
		lakeshore330.sample_channel = channel;
		return vars_push( INT_VAR, channel + 1 );
	}

	return vars_push( INT_VAR, lakeshore330_sample_channel( channel ) + 1 );
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

Var *temp_contr_sensor_unit( Var *v )
{
	long unit = 0;
	const char *in_units  = "KCS";
	int i;


	if ( v == NULL )
	{
		if ( FSC2_MODE == TEST )
			return vars_push( INT_VAR, lakeshore330.unit );

		return vars_push( INT_VAR, lakeshore330_get_unit( ) );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		unit = get_long( v, "unit number" );

		if ( unit < UNIT_KELVIN || unit > UNIT_SENSOR )
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

		if ( strlen( v->val.sptr ) != 1 || i > UNIT_SENSOR )
		{
			print( FATAL, "Invalid unit (\"%s\").\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}


	if ( ( v = vars_pop( v ) ) != NULL )
	{
		print( WARN, "Too many arguments, discarding superfluous "
			   "arguments.\n" );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	
	if ( FSC2_MODE == TEST )
		return vars_push( INT_VAR, lakeshore330.unit = unit );

	lakeshore330_set_unit( unit );
	return vars_push( INT_VAR, lakeshore330_get_unit( ) );
}


/*---------------------------------------------------------*/
/* If called with a non-zero argument the keyboard becomes */
/* unlocked during an experiment.                          */
/*---------------------------------------------------------*/

Var *temp_contr_lock_keyboard( Var *v )
{
	int lock;


	if ( v == NULL )
		lock = LOCK_STATE_REMOTE_LLO;
	else
	{
		lock = get_boolean( v ) ? LOCK_STATE_REMOTE_LLO : LOCK_STATE_REMOTE;
		too_many_arguments( v );
	}

	if ( FSC2_MODE == EXPERIMENT )
		lakeshore330_lock( lock - 1 );

	return vars_push( INT_VAR, lock == LOCK_STATE_REMOTE_LLO ? 1 : 0 );
}


/**********************************************************/
/*                                                        */
/*       Internal (non-exported) functions                */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static bool lakeshore330_init( const char *name )
{
	char buf[ 20 ];
	long len = 20;
	const char *in_units  = "KCS";


	/* Initialize GPIB communication with the temperature controller */

	if ( gpib_init_device( name, &lakeshore330.device ) == FAILURE )
		return FAIL;

	/* Set end of EOS character to '\n' */

	if ( gpib_write( lakeshore330.device, "TERM 2\r\n", 8 ) == FAILURE ||
		 gpib_write( lakeshore330.device, "END 0\n", 6 ) == FAILURE )
		return FAIL;

	if ( gpib_write( lakeshore330.device, "*STB?\n", 6 ) == FAILURE ||
		 gpib_read( lakeshore330.device, buf, &len ) == FAILURE )
		return FAIL;

	sprintf( buf, "SUNI %c\n", in_units[ lakeshore330.unit ] );
	if ( gpib_write( lakeshore330.device, buf, strlen( buf ) ) == FAILURE )
		return FAIL;

	/* Set default sample channel */

	sprintf(buf, "SCHN %c\n", ( char ) ( lakeshore330.sample_channel + 'A' ) );
	if ( gpib_write( lakeshore330.device, buf, strlen( buf ) ) == FAILURE )
		return FAIL;
	usleep( 500000 );

	/* Switch device to remote state with local lockout */

	if ( gpib_write( lakeshore330.device, "MODE 2\n", 7 ) == FAILURE )
		return FAIL;

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static double lakeshore330_sens_data( void )
{
	char buf[ 50 ];
	long len = 50;
	double temp;


	if ( gpib_write( lakeshore330.device, "SDAT?\n", 6 ) == FAILURE ||
		 gpib_read( lakeshore330.device, buf, &len ) == FAILURE )
		lakeshore330_gpib_failure( );

	if ( *buf != '-' && *buf != '+' && ! isdigit( *buf ) )
	{
		print( FATAL, "Error reading temperature.\n" );
		THROW( EXCEPTION );
	}

	sscanf( buf, "%lf", &temp );
	return temp;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static void lakeshore330_set_unit( long unit )
{
	const char *in_units  = "KCS";
	char buf[ 30 ];


	fsc2_assert( unit >= UNIT_KELVIN && unit <= UNIT_SENSOR );

	sprintf( buf, "SUNI %c\n", in_units[ unit ] );
	if ( gpib_write( lakeshore330.device, buf, strlen( buf ) ) == FAILURE )
		lakeshore330_gpib_failure( );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static long lakeshore330_get_unit( void )
{
	const char *out_units = "KCVRM";
	char buf[ 30 ];
	long len = 30;
	long i;


	if ( gpib_write( lakeshore330.device, "SUNI?\n", 6 ) == FAILURE ||
		 gpib_read( lakeshore330.device, buf, &len ) == FAILURE )
		lakeshore330_gpib_failure( );

	for ( i = 0; i <= UNITS_MILLIVOLTS; i++ )
		if ( *buf == out_units[ i ] )
			return lakeshore330.unit = i;

	print( FATAL, "Device returned invalid unit \"%s\".\n", *buf );
	THROW( EXCEPTION );

	return -1;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static long lakeshore330_sample_channel( long channel )
{
	char buf[ 20 ];


	fsc2_assert( channel == SAMPLE_CHANNEL_A || channel == SAMPLE_CHANNEL_B );

	sprintf( buf, "SCHN %c\n", ( char ) ( channel + 'A' ) );
	if ( gpib_write( lakeshore330.device, buf, strlen( buf ) ) == FAILURE )
		lakeshore330_gpib_failure( );

	usleep( 500000);
	return lakeshore330.sample_channel = channel;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static void lakeshore330_lock( int state )
{
	char cmd[ 20 ];


	fsc2_assert( state >= LOCK_STATE_LOCAL && state <= LOCK_STATE_REMOTE_LLO );

	sprintf( cmd, "MODE %d\n", state );
	if ( gpib_write( lakeshore330.device, cmd, strlen( cmd ) ) == FAILURE )
		lakeshore330_gpib_failure( );

	lakeshore330.lock_state = state;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static void lakeshore330_gpib_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
