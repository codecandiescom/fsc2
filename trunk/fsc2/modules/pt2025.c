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

#include "pt2025.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define MAX_TRIES   150


int  pt2025_init_hook( void );
int  pt2025_test_hook( void );
int  pt2025_exp_hook( void );
int  pt2025_end_of_exp_hook( void );


Var *gaussmeter_name( Var *v );
Var *measure_field( Var *v );
Var *gaussmeter_resolution( Var *v );
Var *gaussmeter_probe_orientation( Var *v );


static bool pt2025_init( const char *name );
static double pt2025_get_field( void );
static void pt2025_set_resolution( int res );


#define PROBE_ORIENTATION_PLUS       0
#define PROBE_ORIENTATION_MINUS      1
#define PROBE_ORIENTATION_UNDEFINED -1
#define UNDEF_RESOLUTION            -1


typedef struct
{
	int device;
	int probe_orientation;
	int resolution;
} PT2025;

static PT2025 pt2025, pt2025_stored;



/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int pt2025_init_hook( void )
{
	need_GPIB = SET;

	pt2025.device = -1;
	pt2025.probe_orientation = PROBE_ORIENTATION_UNDEFINED;
	pt2025.resolution = UNDEF_RESOLUTION;

	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int pt2025_test_hook( void )
{
	pt2025_stored = pt2025;
	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int pt2025_exp_hook( void )
{
	pt2025 = pt2025_stored;

	if ( ! pt2025_init( DEVICE_NAME ) )
	{
		print( FATAL, "Failed to initialize device.\n" );
		THROW( EXCEPTION );
	}

	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int pt2025_end_of_exp_hook( void )
{
	pt2025.device = -1;

	return 1;
}



/**********************************************************/
/*                                                        */
/*              Exported functions                        */
/*                                                        */
/**********************************************************/


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *gaussmeter_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *measure_field( Var *v )
{
	v = v;

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, 34089.3 );

	return vars_push( FLOAT_VAR, pt2025_get_field( ) );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *gaussmeter_resolution( Var *v )
{
	double res;


	if ( v == NULL )
	{
		if ( FSC2_MODE == PREPARATION &&
			 pt2025.resolution == UNDEF_RESOLUTION )
		{
			no_query_possible( );
			THROW( EXCEPTION );
		}

		if ( FSC2_MODE == TEST && pt2025.resolution == UNDEF_RESOLUTION )
			return vars_push( FLOAT_VAR, 0.001 );
		return vars_push( FLOAT_VAR, pt2025.resolution == LOW ? 0.01 : 0.001 );
	}

	res = get_double( v, "resolution" );

	if ( res <= 0.0 )
	{
		print( FATAL, "Invalid resolution of %f G.\n", res );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	pt2025.resolution = res < sqrt( 1.0e-5 ) ? HIGH : LOW;

	if ( res > 0.0101 || res < 0.00099 )
		print( WARN, "Can't set resolution to %.3f G, using %.3f G instead.\n",
			   res, pt2025.resolution == LOW ? 0.01 : 0.001 );

	if ( FSC2_MODE == EXPERIMENT )
		pt2025_set_resolution( pt2025.resolution );

	return vars_push( FLOAT_VAR, pt2025.resolution == LOW ? 0.01 : 0.001 );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *gaussmeter_probe_orientation( Var *v )
{
	long orientation;


	if ( v == NULL )
	{
		if ( FSC2_MODE == PREPARATION )
		{
			no_query_possible( );
			THROW( EXCEPTION );
		}

		if ( FSC2_MODE == TEST )
			return vars_push( INT_VAR, 1 );

		return vars_push( INT_VAR, ( long ) pt2025.probe_orientation );
	}

	if ( FSC2_MODE != PREPARATION )
	{
		print( FATAL, "Probe orientation can only be set during the "
			   "PREPARATIONS section.\n" );
		THROW( EXPERIMENT );
	}

	if ( v->type != STR_VAR )
		orientation = get_long( v, "probe orientation" );
	else
	{
		if ( strcmp( v->val.sptr, "+" ) && strcmp( v->val.sptr, "-" ) )
		{
			print( FATAL, "Invalid argument: %s\n", v->val.sptr );
			THROW( EXCEPTION );
		}

		orientation = v->val.sptr[ 0 ] == '+' ? 1 : 0;
	}

	pt2025.probe_orientation = orientation ?
							  PROBE_ORIENTATION_PLUS : PROBE_ORIENTATION_MINUS;

	too_many_arguments( v );

	return vars_push( INT_VAR, ( long ) pt2025.probe_orientation );
}


/**********************************************************/
/*                                                        */
/*              Internal functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static bool pt2025_init( const char *name )
{
	unsigned char buf[ 50 ];
	long len = 50;
	long status;


	/* Initialize GPIB communication with the temperature controller */

	if ( gpib_init_device( name, &pt2025.device ) == FAILURE )
		return FAIL;

	/* Get status byte #3 - unfortunately, we often get not the status byte
	   but the current field value, so try again unless the reply starts
	   with an 'S' an is exactly 3 bytes long. */

	do
	{
		len = 50;
		if ( gpib_write( pt2025.device, "S3\r\n", 4 ) == FAILURE ||
			 gpib_read( pt2025.device, buf, &len ) == FAILURE )
			return FAIL;
	} while ( buf[ 0 ] != 'S' || len != 3 );

	buf[ 3 ] = '\0';
	status = strtol( buf + 1, NULL, 16 );

	/* If necessary switch to field display mode (i.e. send data in Tesla) */

	if ( ! ( status & 1 ) &&
		 gpib_write( pt2025.device, "D1\r\n", 4 ) == FAILURE )
		return FAIL;

	/* If necessary switch to multiplexer A */

	if ( status & 0x70 &&
		 gpib_write( pt2025.device, "PA\r\n", 4 ) == FAILURE )
		return FAIL;

	/* If necessary switch to field AUTO mode */

	if ( ! ( status & 2 ) &&
		 gpib_write( pt2025.device, "A1\r\n", 4 ) == FAILURE )
		return FAIL;

	/* If the probe orientation isn't the way the user asked for set it,
	   otherwise keep the current setting */

	if ( pt2025.probe_orientation != PROBE_ORIENTATION_UNDEFINED &&
		 ( ( status & 4 &&
			 pt2025.probe_orientation == PROBE_ORIENTATION_MINUS ) || 
		   ( ! ( status & 4 ) &&
			 pt2025.probe_orientation == PROBE_ORIENTATION_PLUS ) ) &&
		 gpib_write( pt2025.device,
					 pt2025.probe_orientation == PROBE_ORIENTATION_PLUS ?
					 "F1\r\n" : "F0\r\n", 4 ) == FAILURE ) 
		 return FAIL;
	else
		pt2025.probe_orientation = status & 4;

	/* If necessary set the resolution */

	if ( pt2025.resolution == UNDEF_RESOLUTION )
		pt2025.resolution = status & 0x80 ? LOW : HIGH;
	else if ( ( ( pt2025.resolution == LOW && ! ( status & 0x80 ) ) || 
				( pt2025.resolution == HIGH && status & 0x80 ) ) &&
			  gpib_write( pt2025.device,
						  pt2025.resolution == LOW ? "V1\r\n" : "V0\r\n", 4 )
			  == FAILURE )
		return FAIL;

	/* Activate the search, starting at about 3.15 T */

	if ( gpib_write( pt2025.device, "H3500\r\n", 7 ) == FAILURE  )
		return FAIL;

	/* Get first field value */

	len = 50;
	if ( gpib_read( pt2025.device, buf, &len ) == FAILURE )
		return FAIL;

	return OK;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static double pt2025_get_field( void )
{
	char buf[ 50 ];
	long len;
	long count = MAX_TRIES;
	double field;


	do
	{
		stop_on_user_request( );

		len = 50;
		if ( gpib_read( pt2025.device, buf, &len ) == FAILURE )
		{
			print( FATAL, "Can't access the NMR gaussmeter.\n" );
			THROW( EXCEPTION );
		}

		if ( toupper( *buf ) == 'L' )
			break;

		stop_on_user_request( );

		if ( count > 1 )
			usleep( 250000 );
	} while ( --count > 0 );

	if ( count == 0 )
	{
		print( FATAL, "NMR gaussmeter can't lock on the current field.\n" );
		THROW( EXCEPTION );
	}

	sscanf( buf + 1, "%lf", &field );
	return field * 1.0e4;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static void pt2025_set_resolution( int res )
{
	if ( gpib_write( pt2025.device, res == LOW ? "V1\r\n" : "V0\r\n", 4 )
		 == FAILURE )
	{
		print( FATAL, "Can't access the NMR gaussmeter.\n" );
		THROW( EXCEPTION );
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
