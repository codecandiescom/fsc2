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

#include "ips20_4.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Declaration of exported functions */

int ips20_4_init_hook( void );
int ips20_4_test_hook( void );
int ips20_4_exp_hook( void );
int ips20_4_end_of_exp_hook( void );
void ips20_4_exit_hook( void );

Var *magnet_name( Var *v );

static bool ips20_4_init( const char *name );

typedef struct {
	int device;

	int activity;
	int mode;
	int state;

	bool lock_state;
} IPS20_4;


IPS20_4 ips20_4;
IPS20_4 ips20_4_stored;


/*-----------------------------------*/
/* Init hook function for the module */
/*-----------------------------------*/

int ips20_4_init_hook( void )
{
	need_GPIB = SET;

	ips20_4.device = -1;
	ips20_4.lock_state = SET;
}


/*-----------------------------------*/
/* Test hook function for the module */
/*-----------------------------------*/

int ips20_4_test_hook( void )
{
	ips20_4_stored = ips20_4;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int ips20_4_exp_hook( void )
{
	ips20_4 = ips20_4_stored;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int ips20_4_end_of_exp_hook( void )
{
	ips20_4 = ips20_4_stored;
}


/*-----------------------------------------*/
/* Called before device driver is unloaded */
/*-----------------------------------------*/

void ips20_4_exit_hook( void )
{
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *magnet_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool ips20_4_init( const char *name )
{
	char cmd[ 100 ];
	char buf[ 100 ];
	long length = 100;


	/* First we have to set up the ITC503 temperature controller which
	   actually does the whole GPIB communication and passes on commands
	   to the sweep power supply and returns the replies. */

	if ( gpib_init_device( name, &ips20_4.device ) == FAILURE )
	{
		ips20_4.device = -1;
        return FAIL;
	}

	/* Bring both the GPIB master device (ITC 503) as well as the sweep power
	   supply in the remote state and set the keyboard lock to what the user
	   asked for. */

	sprintf( cmd, "@%1dC%1d\n", MASTER_ISOBUS_ADDRESS,
			 ips20_4.lock_state ? 1 : 3 );
	length = 100;
	if ( gpib_write( ips20_4.device, cmd, 5 ) == FAILURE ||
		 gpib_read( ips20_4.device, buf, &length ) == FAILURE )
		return FAIL;

	sprintf( cmd, "@%1dC%1d\n", IPS20_4_ISOBUS_ADDRESS,
			 ips20_4.lock_state ? 1 : 3 );
	length = 100;
	if ( gpib_write( ips20_4.device, cmd, 5 ) == FAILURE ||
		 gpib_read( ips20_4.device, buf, &length ) == FAILURE )
		return FAIL;

	/* Set the sweep power supply to send data with extended resolution 
	   (this is one of the few commands that don't produce a reply) */

	sprintf( cmd, "@%1dC4\n", IPS20_4_ISOBUS_ADDRESS );
	if ( gpib_write( ips20_4.device, cmd, 5 ) == FAILURE )
		return FAIL;

	/* Get all information about the state of the magnet power supply and
	   analyze the reply which has the form "XmnAnCnMmnPmn" where m and n
	   are single decimal digits. */

	sprintf( cmd, "@%1dX\n", IPS20_4_ISOBUS_ADDRESS );
	length = 100;
	if ( gpib_write( ips20_4.device, cmd, 4 ) == FAILURE ||
		 gpib_read( ips20_4.device, buf, &length ) == FAILURE )
		return FAIL;

	/* Check system status data */

	switch ( buf[ 1 ] )
	{
		case '0' :     /* normal */
			break;

		case '1' :     /* quenched */
			print( FATAL, "Magnet is quenched.\n" );
			return FAIL;

		case '2' :     /* overheated */
			print( FATAL, "Magnet is overheated.\n" );
			return FAIL;

		case '4' :     /* warming up */
			print( FATAL, "Magnet is warming up.\n" );
			return FAIL;

		case '8' :     /* fault */
			print( FATAL, "Device signals fault condition.\n" );
			return FAIL;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			return FAIL;
	}

	switch ( buf[ 2 ] )
	{
		case '0' :     /* normal */
		case '1' :     /* on positive voltage limit */
		case '2' :     /* on negative volatge limit */
			break;

		case '4' :
			print( FATAL, "Magnet is outside positive current limit.\n" );
			return FAIL;

		case '8' :
			print( FATAL, "Magnet is outside negative current limit.\n" );
			return FAIL;
			
		default :
			print( FATAL, "Recived invalid reply from device.\n" );
			return FAIL;
	}
			
	/* Check activity status */

	switch ( buf[ 4 ] )
	{
		case '0' :
			ips20_4.activity = HOLD;
			break;

		case '1' :
			ips20_4.activity = TO_SET_POINT;
			break;

		case '2' :
			ips20_4.activity = TO_ZERO;
			break;

		case '4' :
			print( FATAL, "Magnet is clamped.\n" );
			return FAIL;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			return FAIL;
	}

	/* Check LOC/REM status (must be identical to what we already set it to) */

	switch ( buf[ 6 ] )
	{
		case '0' :
			print( FATAL, "Magnet did not accept command.\n" );
			return FAIL;

		case '1' :
			if ( ! ips20_4.lock_state )
			{
				print( FATAL, "Magnet did not accept command.\n" );
				return FAIL;
			}
			break;

		case '2' :
			print( FATAL, "Magnet did not accept command.\n" );
			return FAIL;

		case '3' :
			if ( ! ips20_4.lock_state )
			{
				print( FATAL, "Magnet did not accept command.\n" );
				return FAIL;
			}
			break;

		case '4' :
		case '5' :
		case '6' :
		case '7' :
			print( FATAL, "Magnet is auto-running down.\n" );
			return FAIL;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			return FAIL;
	}

	/* Check switch heater status */

	switch ( buf[ 8 ] )
	{
		case '0' :
			print( FATAL, "Switch heater is off (at zero field).\n" );
			return FAIL;

		case '1' :     /* swich heater is on */
			break;

		case '2' :
			print( FATAL, "Switch heater is off (at non-zero field).\n" );
			return FAIL;

		case '5' :
			print( FATAL, "Switch heater fault condition.\n" );
			return FAIL;

		case '8' :     /* no switch fitted */
			break;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			return FAIL;
	}

	/* Check mode status */

	switch ( buf[ 10 ] )
	{
		case '0' :
			ips20_4.mode = FAST;
			break;

		case '4' :
			ips20_4.mode = SLOW;
			break;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			return FAIL;
	}

	switch ( buf[ 11 ] )
	{
		case '0' :
			ips20_4.state = AT_REST;
			break;

		case '1' :
			ips20_4.state = SWEEPING;
			break;

		case '2' :
			ips20_4.state = SWEEP_LIMITING;
			break;

		case '4' :
			ips20_4.state = SWEEPING_AND_SWEEP_LIMITING;
			break;

		default :
			print( FATAL, "Received invalid reply from device.\n" );
			return FAIL;
	}

	/* The polarity status bytes are always '0' according to the manual */

	if ( buf[ 13 ] != '0' || buf[ 14 ] != '0' )
	{
		print( FATAL, "Received invalid reply from device.\n" );
		return FAIL;
	}

	return OK;
}
