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



/* On Axel's request: Here's a driver for the Bruker ER035M gaussmeter that
   allows the gaussmeter to be used in conjunction with the Bruker BH15
   field controller. The latter is used to control the field while the NMR
   gaussmeter is only used to measure the field for calibration purposes. */


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "er035m_sa.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define ER035M_TEST_FIELD 2000.0   /* returned as current fireld in test run */
#define ER035M_TEST_RES   0.001

#define PROBE_ORIENTATION_PLUS       0
#define PROBE_ORIENTATION_MINUS      1
#define PROBE_ORIENTATION_UNDEFINED -1
#define UNDEF_RESOLUTION            -1


/* exported functions and symbols */

int er035m_sa_init_hook( void );
int er035m_sa_test_hook( void );
int er035m_sa_exp_hook( void );
int er035m_sa_end_of_exp_hook( void );
void er035m_sa_exit_hook( void );

Var *gaussmeter_name( Var *v );
Var *measure_field( Var *v );
Var *gaussmeter_resolution( Var *v );
Var *gaussmeter_probe_orientation( Var *v );


/* internally used functions */

static double er035m_sa_get_field( void );
static int er035m_sa_get_resolution( void );
static void er035m_sa_set_resolution( int res_index );
static void er035m_sa_failure( void );


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

enum {
	UNDEF_RES = -1,
	LOW_RES,
	MEDIUM_RES,
	HIGH_RES
};


/* The gaussmeter seems to be more cooperative if we wait for some time
   (i.e. 200 ms) after each write operation... */

#define E2_US          100000    /* 100 ms, used in calls of usleep() */
#define ER035M_SA_WAIT 200000    /* this means 200 ms for usleep() */

/* If the field is unstable the gausmeter might never get to the state where
   the field value is valid with the requested resolution eventhough the look
   state is achieved. `ER035M_SA_MAX_RETRIES' tells how many times we retry in
   this case. With a value of 100 and the current setting of `ER035M_SA_WAIT'
   of 200 ms it will take at least 20 s before this will happen.

   Take care: This does not mean that we stop trying to get the field while
   the gaussmeter is still actively searching but only in the case that a
   lock is already achieved but the field value is too unstable! */

#define ER035M_SA_MAX_RETRIES 100


enum {
	   ER035M_SA_UNKNOWN = -1,
	   ER035M_SA_LOCKED = 0,
	   ER035M_SA_SU_ACTIVE,
	   ER035M_SA_SD_ACTIVE,
	   ER035M_SA_OU_ACTIVE,
	   ER035M_SA_OD_ACTIVE,
	   ER035M_SA_SEARCH_AT_LIMIT
};



/*****************************************************************************/
/*                                                                           */
/*                  hook functions                                           */
/*                                                                           */
/*****************************************************************************/


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_sa_init_hook( void )
{
	need_GPIB = SET;
	nmr.is_needed = SET;
	nmr.name = DEVICE_NAME;

	nmr.state = ER035M_SA_UNKNOWN;
	nmr.resolution = UNDEF_RES;
	nmr.device = -1;

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_sa_test_hook( void )
{
	nmr_stored = nmr;
	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_sa_exp_hook( void )
{
	char buffer[ 21 ], *bp;
	long length = 20;
	int try_count = 0;
	Var *v;
	int cur_res;


	nmr = nmr_stored;

	if ( ! nmr.is_needed )
		return 1;

	fsc2_assert( nmr.device < 0 );

	if ( gpib_init_device( nmr.name, &nmr.device ) == FAILURE )
	{
		nmr.device = -1;
		er035m_sa_failure( );
	}
	usleep( ER035M_SA_WAIT );

	/* Send a "Selected Device Clear" - otherwise the gaussmeter does not
	   work ... */

	gpib_clear_device( nmr.device );
	usleep( ER035M_SA_WAIT );

	/* Find out the curent resolution, and if necessary, change it to the
	   value requested by the user */

	cur_res = er035m_sa_get_resolution( );

	if ( nmr.resolution == UNDEF_RES )
		nmr.resolution = cur_res;
	else if ( nmr.resolution != cur_res )
		er035m_sa_set_resolution( res_list[ nmr.resolution ] );

	/* Ask gaussmeter to send status byte and test if it does */

	nmr.state = ER035M_SA_UNKNOWN;

try_again:

	stop_on_user_request( );

	if ( gpib_write( nmr.device, "PS\r", 3 ) == FAILURE )
		er035m_sa_failure( );
	usleep( ER035M_SA_WAIT );

	length = 20;
	if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
		er035m_sa_failure( );

	/* Now look if the status byte says that device is OK where OK means that
	   for the X-Band magnet the F0-probe is connected, modulation is on and
	   the gaussmeter is either in locked state or is actively searching to
	   achieve the lock (if it's just in TRANS L-H or H-L state check again) */

	bp = buffer + 2;     /* skip first two chars of status byte */

	do     /* loop through remaining chars in status byte */
	{
		switch ( *bp )
		{
			case '0' :      /* Probe F0 is connected -> OK for S-band */
				if ( exists_device( "aeg_s_band" ) )
					break;
				print( FATAL, "Wrong field probe (F0) connected to the NMR "
					   "gaussmeter.\n" );
				THROW( EXCEPTION );
				

			case '1' :      /* Probe F1 is connected -> OK for X-band*/
				if ( exists_device( "aeg_x_band" ) )
					break;
				print( FATAL, "Wrong field probe (F1) connected to the NMR "
					   "gaussmeter.\n" );
				THROW( EXCEPTION );

			case '2' :      /* No probe connected -> error */
				print( FATAL, "No field probe connected to the NMR "
					   "gaussmeter.\n" );
				THROW( EXCEPTION );

			case '3' :      /* Error temperature -> error */
				print( FATAL, "Temperature error from NMR gaussmeter.\n" );
				THROW( EXCEPTION );

			case '4' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				print( FATAL, "NMR gaussmeter can't find the actual "
					   "field.\n" );
				THROW( EXCEPTION );

			case '5' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				print( FATAL, "NMR gaussmeter can't find the actual "
					   "field.\n" );
				THROW( EXCEPTION );

			case '6' :      /* MOD OFF -> error (should never happen */
				print( FATAL, "Modulation of NMR gaussmeter is switched "
					   "off.\n" );
				THROW( EXCEPTION );

			case '7' :      /* MOD POS -> OK */
				nmr.probe_orientation = PROBE_ORIENTATION_PLUS;
				break;

			case '8' :      /* MOD NEG -> OK */
				nmr.probe_orientation = PROBE_ORIENTATION_MINUS;
				break;

			case '9' :      /* System in lock -> very good... */
				nmr.state = ER035M_SA_LOCKED;
				break;

			case 'A' :      /* FIELD ? -> error (doesn't seem to work) */
				print( FATAL, "NMR gaussmeter has an unidentifiable "
					   "problem.\n" );
				THROW( EXCEPTION );

			case 'B' :      /* SU active -> OK */
				nmr.state = ER035M_SA_SU_ACTIVE;
				break;

			case 'C' :      /* SD active -> OK */
				nmr.state = ER035M_SA_SD_ACTIVE;
				break;

			case 'D' :      /* OU active -> OK */
				nmr.state = ER035M_SA_OU_ACTIVE;
				break;

			case 'E' :      /* OD active -> OK */
				nmr.state = ER035M_SA_OD_ACTIVE;
				break;

			case 'F' :      /* Search active but just at search limit -> OK */
				nmr.state = ER035M_SA_SEARCH_AT_LIMIT;
				break;
		}

	} while ( *bp++ != '\r' ); 


	/* Switch the display on */

	if ( gpib_write( nmr.device, "ED\r", 3 ) == FAILURE )
		er035m_sa_failure( );
	usleep( ER035M_SA_WAIT );

	/* If the gaussmeter is already locked just get the field value, other-
	   wise try to achieve locked state */

	if ( nmr.state != ER035M_SA_LOCKED )
	{
		v = measure_field( NULL );
		nmr.field = v->val.dval;
		vars_pop( v );
	}
	else
		nmr.field = er035m_sa_get_field( );

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_sa_end_of_exp_hook( void )
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

void er035m_sa_exit_hook( void )
{
	er035m_sa_end_of_exp_hook( );
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

Var *measure_field( Var *v )
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

	if ( ( nmr.state == ER035M_SA_OU_ACTIVE ||
		   nmr.state == ER035M_SA_OD_ACTIVE ||
		   nmr.state == ER035M_SA_UNKNOWN ) &&
		 gpib_write( nmr.device, "SD\r", 3 ) == FAILURE )
		er035m_sa_failure( );
	usleep( ER035M_SA_WAIT );

	/* wait for gaussmeter to go into lock state (or FAILURE) */

	while ( nmr.state != ER035M_SA_LOCKED )
	{
		stop_on_user_request( );

		/* Get status byte and check if lock was achieved */

		if ( gpib_write( nmr.device, "PS\r", 3 ) == FAILURE )
			er035m_sa_failure( );
		usleep( ER035M_SA_WAIT );

		length = 20;
		if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
			er035m_sa_failure( );

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
					nmr.state = ER035M_SA_LOCKED;
					break;

				case 'A' :      /* FIELD ? -> error */
					print( FATAL, "NMR gaussmeter has an unidentifiable "
						   "problem.\n" );
					THROW( EXCEPTION );

				case 'B' :      /* SU active -> OK */
					nmr.state = ER035M_SA_SU_ACTIVE;
					break;

				case 'C' :      /* SD active */
					nmr.state = ER035M_SA_SD_ACTIVE;
					break;

				case 'D' :      /* OU active -> error (should never happen) */
					nmr.state = ER035M_SA_OU_ACTIVE;
					print( FATAL, "NMR gaussmeter has an unidentifiable "
						   "problem.\n" );
					THROW( EXCEPTION );

				case 'E' :      /* OD active -> error (should never happen) */
					nmr.state = ER035M_SA_OD_ACTIVE;
					print( FATAL, "NMR gaussmeter has an unidentifiable "
						   "problem.\n" );
					THROW( EXCEPTION );

				case 'F' :      /* Search active but at a search limit -> OK*/
					nmr.state = ER035M_SA_SEARCH_AT_LIMIT;
					break;
			}

		} while ( *bp++ != '\r' );
	};

	/* Finally  get current field value */

	return vars_push( FLOAT_VAR, er035m_sa_get_field( ) );
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
		er035m_sa_set_resolution( nmr.resolution );

	return vars_push( FLOAT_VAR, res_list[ nmr.resolution ] );
}


/*****************************************************************************/
/*                                                                           */
/*            internally used functions                                      */
/*                                                                           */
/*****************************************************************************/


/*------------------------------------------------------------------------*/
/* er035m_sa_get_field() returns the field value from the gaussmeter - it */
/* will return the settled value but will report a failure if gaussmeter  */
/* isn't in lock state. Another reason for a failure is a field that is   */
/* too unstable to achieve the requested resolution eventhough the        */
/* gaussmeter is already in lock state.                                   */
/* Take care: If the gaussmeter isn't already in the lock state call      */
/*            the function find_field() instead.                          */
/*------------------------------------------------------------------------*/

double er035m_sa_get_field( void )
{
	char buffer[ 21 ];
	char *state_flag;
	long length;
	long tries = ER035M_SA_MAX_RETRIES;
	const char *res[ ] = { "0.1", "0.01", "0.001" };


	/* Repeat asking for field value until it's correct up to LSD -
	   it will give up after `ER035M_SA_MAX_RETRIES' retries to avoid
	   getting into an infinite loop when the field is too unstable */

	do
	{
		stop_on_user_request( );

		/* Ask gaussmeter to send the current field and read result */

		if ( gpib_write( nmr.device, "PF\r", 3 ) == FAILURE )
			er035m_sa_failure( );
		usleep( ER035M_SA_WAIT );

		length = 20;
		if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
			er035m_sa_failure( );

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
			   "requested resolution of %s G.\n", res[ nmr.resolution ] );
		THROW( EXCEPTION );
	}

	/* Finally interpret the field value string */

	*( state_flag - 1 ) = '\0';
	sscanf( buffer + 2, "%lf", &nmr.field );

	return nmr.field;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static int er035m_sa_get_resolution( void )
{
	char buffer[ 20 ];
	long length = 20;


	if ( gpib_write( nmr.device, "RS\r", 3 ) == FAILURE )
		er035m_sa_failure( );

	usleep( ER035M_SA_WAIT );

	if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
		er035m_sa_failure( );

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

static void er035m_sa_set_resolution( int res_index )
{
	char buf[ 5 ];


	sprintf( buf, "RS%1d\r", res_index + 1 );
	if ( gpib_write( nmr.device, buf, 4 ) == FAILURE )
		er035m_sa_failure( );

	usleep( ER035M_SA_WAIT );

}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void er035m_sa_failure( void )
{
	print( FATAL, "Can't access the NMR gaussmeter.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
