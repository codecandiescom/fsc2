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


#include "fsc2.h"
#include "gpib_if.h"


#define DEVICE_NAME "PT2025"
#define MAX_TRIES   100


int  pt2025_init_hook( void );
int  pt2025_exp_hook( void );
int  pt2025_end_of_exp_hook( void );


Var *gaussmeter_name( Var *v );
Var *measure_field( Var *v );


static bool pt2025_init( const char *name );
static double pt2025_get_field( void );


typedef struct
{
	int device;
} PT2025;

static PT2025 pt2025;



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

	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int pt2025_exp_hook( void )
{
	if ( ! pt2025_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Failed to initialize device.\n",
				DEVICE_NAME );
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
	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, 34089.3 );
	return vars_push( FLOAT_VAR, pt2025_get_field );
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
	char buf[ 50 ];
	long len = 50;


	/* Initialize GPIB communication with the temperature controller */

	if ( gpib_init_device( name, &pt2025.device ) == FAILURE )
		return FAIL;

	/* Primary settings:
	   1. Display field values (in T)
	   2. Use multiplexer channel A
	   3. Normal reading display rate
	   4. medium fast search time (15 s over probes field range)
	   5. Switch to search mode
	*/

	if ( gpib_write( pt2025.device, "D1\r\n", 4 ) == FAILURE ||
		 gpib_write( pt2025.device, "PA\r\n", 4 ) == FAILURE ||
		 gpib_write( pt2025.device, "F0\r\n", 4 ) == FAILURE ||
		 gpib_write( pt2025.device, "O3\r\n", 4 ) == FAILURE ||
		 gpib_write( pt2025.device, "H\r\n", 3 ) == FAILURE  )
		return FAIL;

	/* Get some random data to check if the device responds */

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
		len = 50;
		if ( gpib_read( pt2025.device, buf, &len ) == FAILURE )
		{
			eprint( FATAL, UNSET, "%s: Can't access the NMR gaussmeter.\n",
					DEVICE_NAME );
			THROW( EXCEPTION );
		}

		if ( toupper( *buf ) == 'L' )
			break;

		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );

		if ( count > 1 )
			usleep( 250000 );

		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );

	} while ( --count > 0 );

	if ( count == 0 )
	{
		eprint( FATAL, SET, "%s: NMR gaussmeter can't lock on the current "
				"field in %s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION );
	}

	sscanf( buf + 1, "%lf", &field );
	return field * 1.0e4;
}
