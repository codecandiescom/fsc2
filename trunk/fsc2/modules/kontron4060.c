/*
  $Id$

  Copyright (c) 2001 Jens Thoms Toerring

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


/* Name of device as given in GPIB configuration file /etc/gpib.conf */

#define DEVICE_NAME "KONTRON4060"


/* Declaration of exported functions */

int kontron4060_init_hook( void );
int kontron4060_exp_hook( void );
int kontron4060_end_of_exp_hook( void );

Var *voltmeter_name( Var *v );
Var *voltmeter_get_data( Var *v );
Var *voltmeter_ac_measurement( Var *v );
Var *voltmeter_dc_measurement( Var *v );


/* Locally used functions */

static bool kontron4060_init( const char *name );
static void kontron4060_failure( void );


#define MEAS_TYPE_AC 0
#define MEAS_TYPE_DC 0


typedef struct
{
	int device;
	int meas_type;
} KONTRON4060;


static KONTRON4060 kontron4060;


/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int kontron4060_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Reset several variables in the structure describing the device */

	kontron4060.device = -1;
	kontron4060.meas_type = MEAS_TYPE_DC;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int kontron4060_exp_hook( void )
{
	/* Nothing to be done yet in a test run */

	if ( TEST_RUN )
		return 1;

	/* Initialize the voltmeter */

	if ( ! kontron4060_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Initialization of device failed: %s\n",
				DEVICE_NAME, gpib_error_msg );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int kontron4060_end_of_exp_hook( void )
{
	/* Switch lock-in back to local mode */

	if ( kontron4060.device >= 0 )
		gpib_local( kontron4060.device );

	kontron4060.device = -1;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *voltmeter_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------*/
/* Switches the voltmeter to AC measurement mode */
/*--------------------------------------------------*/

Var *voltmeter_ac_measurement( Var *v )
{
	v = v;

	if ( ! TEST_RUN )
		if ( gpib_write( kontron4060.device, "M1\n", 3 ) == FAILURE )
			kontron4060_failure( );

	kontron4060.meas_type = MEAS_TYPE_AC;
	return vars_push( INT_VAR, 1  );
}


/*-----------------------------------------------*/
/* Switches the voltmeter to DC measurement mode */
/*-----------------------------------------------*/

Var *voltmeter_dc_measurement( Var *v )
{
	v = v;

	if ( ! TEST_RUN )
		if ( gpib_write( kontron4060.device, "M0\n", 3 ) == FAILURE )
			kontron4060_failure( );

	kontron4060.meas_type = MEAS_TYPE_DC;
	return vars_push( INT_VAR, 1  );
}


/*------------------------------------------------*/
/* Returns the current voltage from the voltmeter */
/*------------------------------------------------*/

Var *voltmeter_get_data( Var *v )
{
	char reply[ 100 ];
	long length = 100;
	char buffer[ 100 ];
	double val;


	v = v;

	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, -0.123 );

	if ( gpib_write( kontron4060.device, "G\n", 2 ) == FAILURE ||
		 gpib_read( kontron4060.device, reply, &length ) == FAILURE )
		kontron4060_failure( );

	sscanf( reply, "%s %lf", buffer, &val );
	return vars_push( FLOAT_VAR, val );
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

static bool kontron4060_init( const char *name )
{
	fsc2_assert( kontron4060.device < 0 );

	if ( gpib_init_device( name, &kontron4060.device ) == FAILURE )
        return FAIL;

	/* Initialize voltmeter to
	    1. send data in 7-bit ASCII (B0)
	    2. use 6 1/2 digits (D3)
	    3. switch filter off (F0)
	    4. send data whenever they're ready (H0)
	    5. send alphanumeric data (N0)
	    6. raise SRQ only on errors (Q0)
	    7. automatic scaling (R0)
	    8. sample measurement mode (T0)
	    9. send only EOI as end of message indicator (U3)
	   10. automatic calibration (Y0)
	   11. Null function off (Z0)
	*/

	if ( gpib_write( kontron4060.device, "B0D3F0H0N0Q0R0T0U3Y0Z0\n", 23 )
		 == FAILURE )
		kontron4060_failure( );

	if ( kontron4060.meas_type == MEAS_TYPE_AC )
		vars_pop( voltmeter_ac_measurement( NULL ) );
	else
		vars_pop( voltmeter_dc_measurement( NULL ) );

	/* Get one value - the first one always seems to be bogus */

	vars_pop( voltmeter_get_data( NULL ) );

	return OK;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

static void kontron4060_failure( void )
{
	eprint( FATAL, UNSET, "%s: Communication with voltmeter failed.\n",
			DEVICE_NAME );
	THROW( EXCEPTION );
}
