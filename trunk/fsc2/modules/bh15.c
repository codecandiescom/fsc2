/*
  $Id$
*/

#include "fsc2.h"
#include "gpib.h"


int bh15_init_hook( void );
int bh15_test_hook( void );
int bh15_exp_hook( void );
int bh15_end_of_exp_hook( void );
void bh15_exit_hook( void );

Var *find_field( Var *v );
Var *field_resolution( Var *v );
Var *field_meter_wait( Var *v );

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


int bh15_init_hook( void )
{
	/* Set global flag to tell magnet power supply driver that the
	   field controller has already been loaded */

	is_gaussmeter = SET;

	if ( exist_device( "er035m" ) )
	{
		eprint( FATAL, "bh15: Driver for Bruker ER035M gaussmeter is already "
				"loaded - there can only be one gaussmeter.\n" );
		THROW( EXCEPTION );
	}

	if ( ! exist_device( "s_band" ) && ! exist_device( "x_band" ) )
	{
		eprint( WARN, "bh15: Driver for Bruker BH15 field controller is "
				"loaded but no appropriate magnet power supply driver.\n" );
		bh15.is_needed = UNSET;
	}
	else
	{
		need_GPIB = SET;
		bh15.is_needed = SET;
		bh15.name = "BH15";
	}

	bh15.state = BH15_UNKNOWN;
	bh15.device = -1;

	return 1;
}


int bh15_test_hook( void )
{
	return 1;
}


int bh15_exp_hook( void )
{
	char buffer[ 20 ];
	long len;
	int tries = 0;


	if ( ! bh15.is_needed )
		return 1;

	assert( bh15.device < 0 );

	if ( gpib_init_device( bh15.name, &bh15.device ) == FAILURE )
	{
		bh15.device = -1;
		eprint( FATAL, "Can't access the Bruker BH15 field controller.\n" );
		THROW( EXCEPTION );
	}

	/* Send a "Selected Device Clear" - I don't know yet if this is really
	   needed */

	gpib_clear_device( bh15.device );

	/* Set Mode 5 */

	if ( gpib_write( bh15.device, "MO 5", 4 ) == FAILURE )
	{
		eprint( FATAL, "Can't access the Bruker BH15 field controller.\n" );
		THROW( EXCEPTION );
	}

	/* Set it into run mode */

	if ( gpib_write( bh15.device, "RU", 2 ) == FAILURE )
	{
		eprint( FATAL, "Can't access the Bruker BH15 field controller.\n" );
		THROW( EXCEPTION );
	}

	sleep( 5 );                /* unfortunately, it seems to need this... */

	do
	{
		usleep( 100000 );

		if ( gpib_write( bh15.device, "LE", 2 ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker BH15 field "
					"controller.\n" );
			THROW( EXCEPTION );
		}

		len = 20;
		if ( gpib_read( bh15.device, buffer, &len ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker BH15 field "
					"controller.\n" );
			THROW( EXCEPTION );
		}
	} while ( ++tries < BH15_MAX_TRIES && strchr( buffer, '1' ) != NULL );

	bh15.state = BH15_FAR_OFF;

	/* We are always going to achieve a resolution of 50 mG ... */

	bh15.resolution = 0.05;
	bh15_get_field( );

	return 1;
}


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


void bh15_exit_hook( void )
{
	bh15_end_of_exp_hook( );
}


/*****************************************************************************/
/*                                                                           */
/*              exported functions                                           */
/*                                                                           */
/*****************************************************************************/


Var *find_field( Var *v )
{
	v = v;
	return vars_push( FLOAT_VAR, bh15_get_field( ) );
}


Var *field_resolution( Var *v )
{
	v = v;
	return vars_push( FLOAT_VAR, bh15.resolution );
}


Var *field_meter_wait( Var *v )
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


static double bh15_get_field( void )
{
	char buffer[ 20 ];
	long len;
	int tries = 0;
	char *val;
	char *endptr;


	if ( TEST_RUN )
		return 0.0;

	do
	{
		usleep( 100000 );

		if ( gpib_write( bh15.device, "LE", 2 ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker BH15 field "
					"controller.\n" );
			THROW( EXCEPTION );
		}

		len = 20;
		if ( gpib_read( bh15.device, buffer, &len ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker BH15 field "
					"controller.\n" );
			THROW( EXCEPTION );
		}
	} while ( ++tries < BH15_MAX_TRIES && strchr( buffer, '1' ) != NULL );


	tries = 0;
	bh15.state = BH15_FAR_OFF;

	do
	{
		usleep( 100000 );

		if ( gpib_write( bh15.device, "FV", 2 ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker BH15 field "
					"controller.\n" );
			THROW( EXCEPTION );
		}

		len = 20;
		if ( gpib_read( bh15.device, buffer, &len ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker BH15 field "
					"controller.\n" );
			THROW( EXCEPTION );
		}

		/* Try to find the qualifier */

		val = buffer;
		while ( *val && ! isdigit( *val ) )
			val++;

		if ( *val == '\0' )    /* no qualifier found ? */
		{
			eprint( FATAL, "Invalid data returned by Bruker BH15 field "
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
				eprint( FATAL, "Bruker BH15 field controller dropped out of "
						"run mode.\n" );
				THROW( EXCEPTION );
				break;

			default :
				eprint( FATAL, "Invalid data returned by Bruker BH15 field "
						"controller.\n" );
				THROW( EXCEPTION );
		}

	} while ( bh15.state != BH15_LOCKED && ++tries < BH15_MAX_TRIES );

	if ( bh15.state != BH15_LOCKED )
	{
		eprint( FATAL, "Bruker BH15 field controller can't find the "
				"field.\n" );
		THROW( EXCEPTION );
	}

	/* Try to locate the start of the field value */

	val++;
	while ( *val && ! isdigit( *val ) && *val != '+' && *val != '-' )
		val++;

	if ( *val == '\0' )    /* no field value found ? */
	{
		eprint( FATAL, "Invalid data returned by Bruker BH15 field "
				"controller.\n" );
		THROW( EXCEPTION );
	}

	/* Convert the string and check for errors */

	bh15.field = strtod( val, &endptr );
	if ( endptr == val || errno == ERANGE )
	{
		eprint( FATAL, "Invalid data returned by Bruker BH15 field "
				"controller.\n" );
		THROW( EXCEPTION );
	}

	return bh15.field;
}
