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

#include "er035m.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define ER035M_TEST_FIELD 2000.0    /* returned as current field in test run */
#define ER035M_TEST_RES   0.001

#define LOW_RES     0               /* 0.1 G */
#define MEDIUM_RES  1               /* 0.01 G */
#define HIGH_RES    2               /* 0.001 G */
#define UNDEF_RES  -1

#define PROBE_ORIENTATION_PLUS       0
#define PROBE_ORIENTATION_MINUS      1
#define PROBE_ORIENTATION_UNDEFINED -1
#define UNDEF_RESOLUTION            -1


/* exported functions and symbols */

int er035m_init_hook( void );
int er035m_test_hook( void );
int er035m_exp_hook( void );
int er035m_end_of_exp_hook( void );
void er035m_exit_hook( void );

Var *gaussmeter_name( Var *v );
Var *find_field( Var *v );
Var *gaussmeter_resolution( Var *v );
Var *gaussmeter_probe_orientation( Var *v );
Var *gaussmeter_command( Var *v );
Var *gaussmeter_wait( Var *v );

/* internally used functions */

static double er035m_get_field( void );
static int er035m_get_resolution( void );
static void er035m_set_resolution( int res_index );
static bool er035_command( const char *cmd );
static void er035m_failure( void );



typedef struct
{
	int state;
	int device;
	bool is_needed;
	const char *name;
	double field;
	int resolution;
	int probe_orientation;
} NMR;


static NMR nmr, nmr_stored;
static double res_list[ 3 ] = { 0.1, 0.01, 0.001 };
bool is_gaussmeter = UNSET;         /* tested by magnet power supply driver */


/* The gaussmeter seems to be more cooperative if we wait for some time
   (i.e. 200 ms) after each write operation... */

#define E2_US       100000    /* 100 ms, used in calls of usleep() */
#define ER035M_WAIT 200000    /* this means 200 ms for usleep() */

/* If the field is unstable the gausmeter might never get to the state where
   the field value is valid with the requested resolution eventhough the look
   state is achieved. 'ER035M_S_MAX_RETRIES' tells how many times we retry in
   this case. With a value of 100 and the current setting of 'ER035M_S_WAIT'
   of 200 ms it will take at least 20 s before this will happen.

   Take care: This does not mean that we stop trying to get the field while
   the gaussmeter is still actively searching but only in the case that a
   lock is already achieved but the field value is too unstable! */

#define ER035M_MAX_RETRIES 100


enum {
	   ER035M_UNKNOWN = -1,
	   ER035M_LOCKED = 0,
	   ER035M_SU_ACTIVE,
	   ER035M_SD_ACTIVE,
	   ER035M_OU_ACTIVE,
	   ER035M_OD_ACTIVE,
	   ER035M_SEARCH_AT_LIMIT
};



/*****************************************************************************/
/*                                                                           */
/*                  hook functions                                           */
/*                                                                           */
/*****************************************************************************/


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_init_hook( void )
{
	/* Set global flag to tell magnet power supply driver that a
	   gaussmeter has been loaded */

	is_gaussmeter = SET;

	if ( exists_device( "bh15" ) )
	{
		print( FATAL, "Driver for BH15 field controller is already loaded - "
			   "only one field control gaussmeter can be used.\n" );
		THROW( EXCEPTION );
	}

	if ( exists_device( "er035m_s" ) )
	{
		print( FATAL, "Driver for ER035M gaussmeter (serial port version) is "
			   "already loaded - only one field controlling gaussmeter can be "
			   "used.\n" );
		THROW( EXCEPTION );
	}

	if ( exists_device( "er035m_sa" ) || exists_device( "er035m_sas" ) )
	{
		print( FATAL, "Driver for ER035M field controlling gaussmeter must "
			   "be first gaussmeter in DEVICES list.\n" );
		THROW( EXCEPTION );
	}

	if ( ! exists_device( "aeg_s_band" ) && ! exists_device( "aeg_x_band" ) )
		print( WARN, "Driver for NMR gaussmeter is loaded but no appropriate "
			   "magnet power supply driver.\n" );

	need_GPIB = SET;
	nmr.is_needed = SET;
	nmr.name = DEVICE_NAME;
	nmr.state = ER035M_UNKNOWN;
	nmr.resolution = UNDEF_RES;
	nmr.probe_orientation = PROBE_ORIENTATION_UNDEFINED;
	nmr.device = -1;

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_test_hook( void )
{
	nmr_stored = nmr;
	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_exp_hook( void )
{
	char buffer[ 21 ], *bp;
	long length = 20;
	int try_count = 0;
	int cur_res;
	Var *v;


	nmr = nmr_stored;

	if ( ! nmr.is_needed )
		return 1;

	fsc2_assert( nmr.device < 0 );

	if ( gpib_init_device( nmr.name, &nmr.device ) == FAILURE )
	{
		nmr.device = -1;
		print( FATAL, "Initialization of device failed: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}
	fsc2_usleep( ER035M_WAIT, UNSET );

	/* Send a "Selected Device Clear" - otherwise the gaussmeter does not
	   work ... */

	gpib_clear_device( nmr.device );
	fsc2_usleep( ER035M_WAIT, UNSET );

	/* Find out the curent resolution, and if necessary, change it to the
	   value requested by the user. */

	cur_res = er035m_get_resolution( );

	if ( nmr.resolution == UNDEF_RES )
		nmr.resolution = cur_res;
	else if ( nmr.resolution != cur_res )
		er035m_set_resolution( nmr.resolution );

	/* Ask gaussmeter to send status byte and test if it does */

	nmr.state = ER035M_UNKNOWN;

try_again:

	stop_on_user_request( );

	if ( gpib_write( nmr.device, "PS\r", 3 ) == FAILURE )
		er035m_failure( );

	fsc2_usleep( ER035M_WAIT, UNSET );

	length = 20;
	if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
		er035m_failure( );

	/* Now look if the status byte says that device is OK where OK means that
	   for the X-band magnet the F1 probe and for the S-band the F0 probe is
	   connected, modulation is on and the gaussmeter is either in locked
	   state or is actively searching to achieve the lock (if it's just in
	   TRANS L-H or H-L state check again) */

	bp = buffer + 2;     /* skip first two chars of status byte */

	do     /* loop through remaining chars in status byte */
	{
		switch ( *bp )
		{
			case '0' :      /* Probe F0 is connected -> OK for S-band */
				if ( exists_device( "aeg_s_band" ) )
					break;
				print( FATAL, "Wrong field probe (F0) connected.\n" );
				THROW( EXCEPTION );

			case '1' :      /* Probe F1 is connected -> OK for X-band*/
				if ( exists_device( "aeg_x_band" ) )
					break;
				print( FATAL, "Wrong field probe (F1) connected.\n" );
				THROW( EXCEPTION );

			case '2' :      /* No probe connected -> error */
				print( FATAL, "No field probe connected.\n" );
				THROW( EXCEPTION );

			case '3' :      /* Error temperature -> error */
				print( FATAL, "Temperature error.\n" );
				THROW( EXCEPTION );

			case '4' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				print( FATAL, "Can't find the field.\n" );
				THROW( EXCEPTION );

			case '5' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				print( FATAL, "Can't find the field.\n" );
				THROW( EXCEPTION );

			case '6' :      /* MOD OFF -> error (should never happen */
				print( FATAL, "Modulation is switched off.\n" );
				THROW( EXCEPTION );

			case '7' :      /* MOD POS -> OK */
				nmr.probe_orientation = PROBE_ORIENTATION_PLUS;
				break;

			case '8' :      /* MOD NEG -> OK*/
				nmr.probe_orientation = PROBE_ORIENTATION_MINUS;
				break;

			case '9' :      /* System in lock -> very good... */
				nmr.state = ER035M_LOCKED;
				break;

			case 'A' :      /* FIELD ? -> error (doesn't seem to work) */
				print( FATAL, "Unidentifiable device problem.\n" );
				THROW( EXCEPTION );

			case 'B' :      /* SU active -> OK */
				nmr.state = ER035M_SU_ACTIVE;
				break;

			case 'C' :      /* SD active -> OK */
				nmr.state = ER035M_SD_ACTIVE;
				break;

			case 'D' :      /* OU active -> OK */
				nmr.state = ER035M_OU_ACTIVE;
				break;

			case 'E' :      /* OD active -> OK */
				nmr.state = ER035M_OD_ACTIVE;
				break;

			case 'F' :      /* Search active but just at search limit -> OK */
				nmr.state = ER035M_SEARCH_AT_LIMIT;
				break;
		}

	} while ( *bp++ != '\r' );

	/* Switch the display on */

	if ( gpib_write( nmr.device, "ED\r", 3 ) == FAILURE )
		er035m_failure( );
	fsc2_usleep( ER035M_WAIT, UNSET );

	/* If the gaussmeter is already locked just get the field value, other-
	   wise try to achieve locked state */

	if ( nmr.state != ER035M_LOCKED )
	{
		v = find_field( NULL );
		nmr.field = v->val.dval;
		vars_pop( v );
	}
	else
		nmr.field = er035m_get_field( );

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_end_of_exp_hook( void )
{
	if ( ! nmr.is_needed )
		return 1;

	if ( nmr.device >= 0 )
		gpib_local( nmr.device );

	nmr.device = -1;

	return 1;
}

/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void er035m_exit_hook( void )
{
	er035m_end_of_exp_hook( );
}


/*****************************************************************************/
/*                                                                           */
/*              exported functions                                           */
/*                                                                           */
/*****************************************************************************/


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *gaussmeter_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*/
/* find_field() tries to get the gaussmeter into the locked state */
/* and returns the current field value in a variable.             */
/*----------------------------------------------------------------*/

Var *find_field( Var *v )
{
	char buffer[ 21 ];
	char *bp;
	long length;


	v = v;

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, ER035M_TEST_FIELD );

	/* If gaussmeter is in oscillator up/down state or the state is unknown
	   (i.e. it's standing somewhere but not searching) search for field.
	   Starting with searching down is just as probable the wrong decision
	   as searching up... */

	if ( ( nmr.state == ER035M_OU_ACTIVE || nmr.state == ER035M_OD_ACTIVE ||
		   nmr.state == ER035M_UNKNOWN ) &&
		 gpib_write( nmr.device, "SD\r", 3 ) == FAILURE )
		er035m_failure( );
	fsc2_usleep( ER035M_WAIT, UNSET );

	/* Wait for gaussmeter to go into lock state (or FAILURE) */

	while ( nmr.state != ER035M_LOCKED )
	{
		stop_on_user_request( );

		/* Get status byte and check if lock was achieved */

		if ( gpib_write( nmr.device, "PS\r", 3 ) == FAILURE )
			er035m_failure( );
		fsc2_usleep( ER035M_WAIT, UNSET );

		length = 20;
		if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
		er035m_failure( );

		bp = buffer + 2;   /* skip first two chars of status byte */

		do     /* loop through remaining chars in status byte */
		{
			switch ( *bp )
			{
				case '4' : case '5' :  /* TRANS L-H or H-L -> test again */
					break;

				case '7' : case '8' :  /* MOD POS or NEG -> just go on */
					break;

				case '9' :      /* System in lock -> very good... */
					nmr.state = ER035M_LOCKED;
					break;

				case 'A' :      /* FIELD ? -> error */
					print( FATAL, "NMR gaussmeter has an unidentifiable "
						   "problem.\n" );
					THROW( EXCEPTION );

				case 'B' :      /* SU active -> OK */
					nmr.state = ER035M_SU_ACTIVE;
					break;

				case 'C' :      /* SD active */
					nmr.state = ER035M_SD_ACTIVE;
					break;

				case 'D' :      /* OU active -> error (should never happen) */
					nmr.state = ER035M_OU_ACTIVE;
					print( FATAL, "Unidentifiable device problem.\n" );
					THROW( EXCEPTION );

				case 'E' :      /* OD active -> error (should never happen) */
					nmr.state = ER035M_OD_ACTIVE;
					print( FATAL, "Unidentifiable device problem.\n" );
					THROW( EXCEPTION );

				case 'F' :      /* Search active but at a search limit -> OK*/
					nmr.state = ER035M_SEARCH_AT_LIMIT;
					break;
			}

		} while ( *bp++ != '\r' );
	};

	/* Finally  get current field value */

	return vars_push( FLOAT_VAR, er035m_get_field( ) );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

Var *gaussmeter_resolution( Var *v )
{
	double res;
	int i;
	int res_index = UNDEF_RES;


	if ( v == NULL )
	{
		if ( FSC2_MODE == PREPARATION && nmr.resolution == UNDEF_RES )
		{
			no_query_possible( );
			THROW( EXCEPTION );
		}

		if ( FSC2_MODE == TEST && nmr.resolution == UNDEF_RES )
			return vars_push( FLOAT_VAR, ER035M_TEST_RES );

		return vars_push( FLOAT_VAR, res_list[ nmr.resolution ] );
	}

	res = get_double( v, "resolution" );

	too_many_arguments( v );

	if ( res <= 0 )
	{
		print( FATAL, "Invalid resolution of %f G.\n", res );
		THROW( EXCEPTION );
	}

	/* Try to match the requested resolution, if necessary use the nearest
	   possible value */

	for ( i = 0; i < 2; i++ )
		if ( res <= res_list[ i ] && res >= res_list[ i + 1 ] )
		{
			res_index = i +
				 ( ( res / res_list[ i ] > res_list[ i + 1 ] / res ) ? 0 : 1 );
			break;
		}

	if ( res_index == UNDEF_RES )
	{
		if ( res >= res_list[ LOW_RES ] )
			res_index = LOW_RES;
		if ( res <= res_list[ HIGH_RES ] )
			res_index = HIGH_RES;
	}

	if ( fabs( res_list[ res_index ] - res ) > 1.0e-2 * res_list[ res_index ] )
		print( WARN, "Can't set resolution to %.3f G, using %.3f G instead.\n",
			   res, res_list[ res_index ] );

	fsc2_assert( res_index >= LOW_RES && res_index <= HIGH_RES );

	nmr.resolution = res_index;

	if ( FSC2_MODE == EXPERIMENT )
		er035m_set_resolution( nmr.resolution );

	return vars_push( FLOAT_VAR, res_list[ nmr.resolution ] );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *gaussmeter_probe_orientation( Var *v )
{
	if ( v == NULL )
	{
		if ( FSC2_MODE == PREPARATION )
		{
			no_query_possible( );
			THROW( EXCEPTION );
		}

		if ( FSC2_MODE == TEST )
			return vars_push( INT_VAR, 1 );

		return vars_push( INT_VAR, ( long ) nmr.probe_orientation );
	}

	print( FATAL, "Device does not allow setting of probe orientation.\n" );
	THROW( EXCEPTION );

	return NULL;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *gaussmeter_command( Var *v )
{
	static char *cmd;


	cmd = NULL;
	vars_check( v, STR_VAR );
	
	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
			er035_command( cmd );
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


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

Var *gaussmeter_wait( Var *v )
{
	v = v;

	if ( FSC2_MODE == EXPERIMENT && nmr.is_needed )
		fsc2_usleep( ( nmr.resolution * 10 ) * E2_US, UNSET );
	return vars_push( INT_VAR, 1 );
}



/*****************************************************************************/
/*                                                                           */
/*            internally used functions                                      */
/*                                                                           */
/*****************************************************************************/


/*-----------------------------------------------------------------------*/
/* er035m_get_field() returns the field value from the gaussmeter - it   */
/* will return the settled value but will report a failure if gaussmeter */
/* isn't in lock state. Another reason for a failure is a field that is  */
/* too unstable to achieve the requested resolution eventhough the       */
/* gaussmeter is already in lock state.                                  */
/* Take care: If the gaussmeter isn't already in the lock state call     */
/*            the function find_field() instead.                         */
/*-----------------------------------------------------------------------*/

double er035m_get_field( void )
{
	char buffer[ 21 ];
	char *state_flag;
	long length;
	long tries = ER035M_MAX_RETRIES;


	/* Repeat asking for field value until it's correct up to LSD -
	   it will give up after 'ER035M_S_MAX_RETRIES' retries to avoid
	   getting into an infinite loop when the field is too unstable */

	do
	{
		stop_on_user_request( );

		/* Ask gaussmeter to send the current field and read result */

		if ( gpib_write( nmr.device, "PF\r", 3 ) == FAILURE )
		er035m_failure( );
		fsc2_usleep( ER035M_WAIT, UNSET );

		length = 20;
		if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
			er035m_failure( );

		/* Disassemble field value and flag showing the state */

		state_flag = strrchr( buffer, ',' ) + 1;

		/* Report error if gaussmeter isn't in lock state */

		if ( *state_flag >= '3' )
		{
			print( FATAL, "NMR gaussmeter can't lock on the current "
				   "field.\n" );
			THROW( EXCEPTION );
		}

	} while ( *state_flag != '0' && tries-- > 0 );

	/* If the maximum number of retries was exceeded we've got to give up */

	if ( tries < 0 )
	{
		print( FATAL, "Field is too unstable to be measured with the "
			   "current resolution of %.3f G.\n", res_list[ nmr.resolution ] );
		THROW( EXCEPTION );
	}

	/* Finally interpret the field value string */

	*( state_flag - 1 ) = '\0';
	sscanf( buffer + 2, "%lf", &nmr.field );

	return nmr.field;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static int er035m_get_resolution( void )
{
	char buffer[ 20 ];
	long length = 20;


	if ( gpib_write( nmr.device, "RS\r", 3 ) == FAILURE )
		er035m_failure( );

	fsc2_usleep( ER035M_WAIT, UNSET );

	if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
		er035m_failure( );

	switch ( buffer[ 2 ] )
	{
		case '1' :
			return LOW_RES;

		case '2' :
			return MEDIUM_RES;

		case '3' :
			return HIGH_RES;
	}

	print( FATAL, "Undocumented data received.\n" );
	THROW( EXCEPTION );

	return UNDEF_RES;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void er035m_set_resolution( int res_index )
{
	char buf[ 5 ];


	sprintf( buf, "RS%1d\r", res_index + 1 );
	if ( gpib_write( nmr.device, buf, 4 ) == FAILURE )
		er035m_failure( );

	fsc2_usleep( ER035M_WAIT, UNSET );

}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool er035_command( const char *cmd )
{
	if ( gpib_write( er035.device, cmd, strlen( cmd ) ) == FAILURE )
		er035m_failure( );

	return OK;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void er035m_failure( void )
{
	print( FATAL, "Can't access the NMR gaussmeter.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
