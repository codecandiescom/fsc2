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

#include "bh15.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define BH15_TEST_FIELD   0.0


int bh15_init_hook( void );
int bh15_test_hook( void );
int bh15_exp_hook( void );
int bh15_end_of_exp_hook( void );
void bh15_exit_hook( void );

Var *gaussmeter_name( Var *v );
Var *find_field( Var *v );
Var *gaussmeter_resolution( Var *v );
Var *gaussmeter_wait( Var *v );

bool is_gaussmeter = UNSET;         /* tested by magnet power supply driver */


static double bh15_get_field( void );


typedef struct
{
	int state;
	int device;
	bool is_needed;
	const char *name;
	double field;
	double resolution;
} BH15;

static BH15 bh15;


enum {
	BH15_UNKNOWN = 0,
	BH15_FAR_OFF,
	BH15_STILL_OFF,
	BH15_LOCKED
};

#define BH15_MAX_TRIES   10


/*****************************************************************************/
/*                                                                           */
/*                  hook functions                                           */
/*                                                                           */
/*****************************************************************************/


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int bh15_init_hook( void )
{
	/* Set global flag to tell magnet power supply driver that the
	   field controller has already been loaded */

	is_gaussmeter = SET;

	if ( exists_device( "er035m" ) || exists_device( "er035m_s" ) )
	{
		print( FATAL, "Driver for Bruker ER035M gaussmeter is already loaded "
			   "- there can only be one gaussmeter.\n" );
		THROW( EXCEPTION );
	}

	if ( ! exists_device( "aeg_s_band" ) && ! exists_device( "aeg_x_band" ) )
	{
		print( WARN, "Driver for Bruker BH15 field controller is loaded but "
			   "no appropriate magnet power supply driver.\n" );
		bh15.is_needed = UNSET;
	}
	else
	{
		need_GPIB = SET;
		bh15.is_needed = SET;
		bh15.name = DEVICE_NAME;
	}

	bh15.state = BH15_UNKNOWN;
	bh15.device = -1;

	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int bh15_test_hook( void )
{
	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int bh15_exp_hook( void )
{
	char buffer[ 20 ];
	long len;
	int tries = 0;


	if ( ! bh15.is_needed )
		return 1;

	fsc2_assert( bh15.device < 0 );

	if ( gpib_init_device( bh15.name, &bh15.device ) == FAILURE )
	{
		bh15.device = -1;
		print( FATAL, " Can't initialize device: %s\n", gpib_error_msg );
		THROW( EXCEPTION );
	}

	/* Send a "Selected Device Clear" - I don't know yet if this is really
	   needed */

	gpib_clear_device( bh15.device );

	/* Set Mode 5 */

	if ( gpib_write( bh15.device, "MO 5\r", 5 ) == FAILURE )
	{
		print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
		THROW( EXCEPTION );
	}

	/* Set it into run mode */

	if ( gpib_write( bh15.device, "RU\r", 3 ) == FAILURE )
	{
		print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
		THROW( EXCEPTION );
	}

	sleep( 5 );                /* unfortunately, it seems to need this... */

	do
	{
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );

		usleep( 100000 );

		if ( gpib_write( bh15.device, "LE\r", 3 ) == FAILURE )
		{
			print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
			THROW( EXCEPTION );
		}

		len = 20;
		if ( gpib_read( bh15.device, buffer, &len ) == FAILURE )
		{
			print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
			THROW( EXCEPTION );
		}
	} while ( ++tries < BH15_MAX_TRIES && strchr( buffer, '1' ) != NULL );

	bh15.state = BH15_FAR_OFF;

	/* We are always going to achieve a resolution of 50 mG ... */

	bh15.resolution = 0.05;
	bh15_get_field( );

	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int bh15_end_of_exp_hook( void )
{
	if ( ! bh15.is_needed )
		return 1;

	if ( bh15.device >= 0 )
	{
		bh15_get_field( );
		gpib_local( bh15.device );
	}

	bh15.device = -1;

	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

void bh15_exit_hook( void )
{
	bh15_end_of_exp_hook( );
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


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *find_field( Var *v )
{
	v = v;
	return vars_push( FLOAT_VAR, bh15_get_field( ) );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *gaussmeter_resolution( Var *v )
{
	v = v;
	return vars_push( FLOAT_VAR, bh15.resolution );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *gaussmeter_wait( Var *v )
{
	v = v;

	/* do this thing needs a time out ? */

	usleep( 100000 );

	return vars_push( INT_VAR, 1 );
}


/*****************************************************************************/
/*                                                                           */
/*            internally used functions                                      */
/*                                                                           */
/*****************************************************************************/


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static double bh15_get_field( void )
{
	char buffer[ 20 ];
	long len;
	int tries = 0;
	char *val;
	char *endptr;


	if ( FSC2_MODE == TEST )
		return BH15_TEST_FIELD;

	do
	{
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );

		usleep( 100000 );

		if ( gpib_write( bh15.device, "LE\r", 3 ) == FAILURE )
		{
			print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
			THROW( EXCEPTION );
		}

		len = 20;
		if ( gpib_read( bh15.device, buffer, &len ) == FAILURE )
		{
			print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
			THROW( EXCEPTION );
		}
	} while ( ++tries < BH15_MAX_TRIES && strchr( buffer, '1' ) != NULL );


	tries = 0;
	bh15.state = BH15_FAR_OFF;

	do
	{
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );

		usleep( 100000 );

		if ( gpib_write( bh15.device, "FV\r", 3 ) == FAILURE )
		{
			print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
			THROW( EXCEPTION );
		}

		len = 20;
		if ( gpib_read( bh15.device, buffer, &len ) == FAILURE )
		{
			print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
			THROW( EXCEPTION );
		}

		/* Try to find the qualifier */

		val = buffer;
		while ( *val && ! isdigit( *val ) )
			val++;

		if ( *val == '\0' )    /* no qualifier found ? */
		{
			print( FATAL, "Invalid data returned by Bruker BH15 field "
				   "controller.\n" );
			THROW( EXCEPTION );
		}

		switch ( *val )
		{
			case '0' :                             /* correct within 50 mG */
				bh15.state = BH15_LOCKED;
				break;

			case '1' :                             /* correct within 1 G */
				if ( bh15.state == BH15_FAR_OFF )
				{
					bh15.state = BH15_STILL_OFF;
					tries = 0;
				}
				break;

			case '2' :                             /* error larger than 1 G */
				bh15.state = BH15_FAR_OFF;
				break;

			case '3' :                             /* BH15 not in RUN mode */
				print( FATAL, "Bruker BH15 field controller dropped out of "
					   "run mode.\n" );
				THROW( EXCEPTION );
				break;

			default :
				print( FATAL, "Invalid data returned by Bruker BH15 field "
					   "controller.\n" );
				THROW( EXCEPTION );
		}

	} while ( bh15.state != BH15_LOCKED && ++tries < BH15_MAX_TRIES );

	if ( bh15.state != BH15_LOCKED )
	{
		print( FATAL, "Bruker BH15 field controller can't find the field.\n" );
		THROW( EXCEPTION );
	}

	/* Try to locate the start of the field value */

	val++;
	while ( *val && ! isdigit( *val ) && *val != '+' && *val != '-' )
		val++;

	if ( *val == '\0' )    /* no field value found ? */
	{
		print( FATAL, "Invalid data returned by Bruker BH15 field "
			   "controller.\n" );
		THROW( EXCEPTION );
	}

	/* Convert the string and check for errors */

	bh15.field = strtod( val, &endptr );
	if ( endptr == val || errno == ERANGE )
	{
		print( FATAL, "Invalid data returned by Bruker BH15 field "
			   "controller.\n" );
		THROW( EXCEPTION );
	}

	return bh15.field;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
