/*
  $Id$

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


#include "fsc2.h"
#include "gpib_if.h"


#define DEVICE_NAME "LAKESHORE330"    /* compare entry in /etc/gpib.conf ! */

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
	Cur_Func = "lakeshore330_init_hook";

	need_GPIB = SET;
	lakeshore330.device = -1;
	lakeshore330.lock_state = LOCK_STATE_REMOTE_LLO;
	lakeshore330.sample_channel = DEFAULT_SAMPLE_CHANNEL;
	lakeshore330.unit = DEFAULT_UNIT;

	Cur_Func = NULL;
	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int lakeshore330_exp_hook( void )
{
	Cur_Func = "lakeshore330_exp_hook";
	if ( ! lakeshore330_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Initialization of device failed.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}
	Cur_Func = NULL;
	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int lakeshore330_end_of_exp_hook( void )
{
	Cur_Func = "lakeshore330_end_of_exp_hook";
	lakeshore330_lock( 0 );
	Cur_Func = NULL;
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

	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, 123.45 );
	else
		return vars_push( FLOAT_VAR, lakeshore330_sens_data( ) );
}


/*-----------------------------------------------------------------*/
/* Sets or returns sample channel (fuction returns either 1 or 2   */
/* for channel A or B and accepts 1 or 2 or the strings "A" or "B" */
/* as input arguments).                                            */
/*-----------------------------------------------------------------*/

Var *temp_contr_sample_channel( Var *v )
{
	long channel;

	if ( v == NULL )
		return vars_push( INT_VAR, lakeshore330.sample_channel );
	else
		vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR || FLOAT_VAR ) )
	{
		if ( v->type == INT_VAR )
			channel = v->val.lval - 1;
		else
		{
			eprint( WARN, SET, "%s: Float value used as channel channel "
					"number in %s().\n", DEVICE_NAME, Cur_Func );
			channel = lround( v->val.dval ) - 1;
		}

		if ( channel != SAMPLE_CHANNEL_A && channel != SAMPLE_CHANNEL_B )
		{
			eprint( FATAL, SET, "%s: Invalid sample channel number (%ld) "
					"in %s().\n", DEVICE_NAME, channel, Cur_Func );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( ( *v->val.sptr != 'A' && *v->val.sptr != 'B' ) ||
			 strlen( v->val.sptr ) != 1 )
		{
			eprint( FATAL, SET, "%s: Invalid sample channel (\"%s\") in "
					"%s().\n", DEVICE_NAME, v->val.sptr, Cur_Func );
			THROW( EXCEPTION );
		}
		channel = ( long ) ( *v->val.sptr - 'A' );
	}

	if ( TEST_RUN )
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
	long unit;
	const char *in_units  = "KCS";
	int i;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, lakeshore330.unit );
		return vars_push( INT_VAR, lakeshore330_get_unit( ) );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->type == INT_VAR )
			unit = v->val.lval;
		else
		{
			eprint( WARN, SET, "%s: Float value used as unit number in "
					"%s().\n", DEVICE_NAME, Cur_Func );
			unit = lround( v->val.dval );
		}

		if ( unit < UNIT_KELVIN || unit > UNIT_SENSOR )
		{
			eprint( FATAL, SET, "%s: Invalid unit number (%d) in %s().\n",
					DEVICE_NAME, unit, Cur_Func );
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
			eprint( FATAL, SET, "%s: Invalid unit (\"%s\") in %s().\n",
					DEVICE_NAME, v->val.sptr, Cur_Func );
			THROW( EXCEPTION );
		}
	}


	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of %s().\n",
				Cur_Func );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	
	if ( TEST_RUN )
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
		vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

		if ( v->type == INT_VAR )
			lock = v->val.lval == 0 ?
				LOCK_STATE_REMOTE_LLO : LOCK_STATE_REMOTE;
		else if ( v->type == FLOAT_VAR )
			lock = v->val.dval == 0.0 ?
				LOCK_STATE_REMOTE_LLO : LOCK_STATE_REMOTE;
		else
		{
			if ( ! strcasecmp( v->val.sptr, "OFF" ) )
				lock = LOCK_STATE_REMOTE;
			else if ( ! strcasecmp( v->val.sptr, "ON" ) )
				lock = LOCK_STATE_REMOTE_LLO;
			else
			{
				eprint( FATAL, SET, "%s: Invalid argument in call of "
						"function %s().\n", DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION );
			}
		}
	}

	if ( ! TEST_RUN )
		lakeshore330_lock( lock - 1 );
	return vars_push( INT_VAR, lock == LOCK_STATE_REMOTE_LLO ? 1 : 0 );
}


/**********************************************************/
/*                                                        */
/*       Internal (not-exported) functions                */
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
		eprint( FATAL, SET, "%s: Error reading temperature in %s().\n",
				DEVICE_NAME, Cur_Func );
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

	eprint( FATAL, SET, "%s: Device returned invalid unit \"%s\" in "
			"%s().\n", DEVICE_NAME, *buf, Cur_Func );
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
	eprint( FATAL, UNSET, "%s: Communication with device failed.\n",
			DEVICE_NAME );
	THROW( EXCEPTION );
}
