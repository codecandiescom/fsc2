/*
  $Id$
*/


#include "fsc2.h"


/* exported functions */

int er035m_init_hook( void );
int er035m_test_hook( void );
int er035m_exp_hook( void );
void er035m_exit_hook( void );

Var *find_field( void );
void field_meter_wait( void );



typedef struct
{
	int state;
	int device;
	char *name;
	double field;
	int resolution;
} NMR;


static er035m_get_field( void );

static bool er035m_is_needed;
static NMR mmr;



/* The gaussmeter seems to be more cooperative if we wait for some time
   (i.e. 200 ms) after each write operation... */

#define E2_US       100000    /* 100 ms, used in calls of usleep() */
#define ER036M_WAIT 200000    /* this means 200 ms for usleep() */

enum {
	   ER035M_UNKNOWN = -1,
	   ER035M_LOCKED = 0,
	   ER035M_SU_ACTIVE,
	   ER035M_SD_ACTIVE,
	   ER035M_OU_ACTIVE,
	   ER035M_OD_ACTIVE,
	   ER035M_SEARCH_AT_LIMIT
};

enum {
	   LOW = 0,
	   HIGH
};




/*****************************************************************************/
/*                                                                           */
/*                  hook functions                                           */
/*                                                                           */
/*****************************************************************************/


int er035m_init_hook( void )
{
	if ( ! exist_device( "s_band" ) && ! exist_device( "x_band" ) )
	{
		eprint( WARN, "Driver for Bruker ER035M gaussmeter is loaded but no "
				"magnet driver.\n" );
		er035m_is_needed = UNSET;
		nmr.device = -1;
	}
	else
	{
		er035m_is_needed = SET;
		nmr.name = "ER035M";
		nmr.state = UNKNOWN;
	}

	return 1;
}


int er035m_test_hook( void )
{
	return 1;
}


int er035m_exp_hook( void )
{
	char buffer[ 21 ], *bp;
	long length = 20;
	int try_count = 0;
	Var *v


	if ( ! er035m_is_needed )
		return 1;


	if ( gpib_init_device( nmr.name, &nmr.device ) == FAILURE )
	{
		eprint( FATAL, "Can't access the Bruker ER035M NMR gaussmeter.\n" );
		THROW( EXCEPTION );
	}
	usleep( ER036M_WAIT );

	/* Send a "Selected Device Clear" - otherwise the gaussmeter does not
	   work ... */

	gpib_clear_device( nmr.device );
	usleep( ER036M_WAIT );

	/* Ask gaussmeter to send status byte and test if it does */

	nmr.state = ER036M_UNKNOWN;

try_again:

	if ( gpib_write( nmr.device, "PS", 2 ) == FAILURE )
	{
		eprint( FATAL, "Can't access the Bruker ER035M NMR gaussmeter.\n" );
		THROW( EXCEPTION );
	}
	usleep( ER036M_WAIT );

	length = 20;
	if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "Can't access the Bruker ER035M NMR gaussmeter.\n" );
		THROW( EXCEPTION );
	}

	/* Now look if the status byte says that device is ok where ok means that
	   for the X-Band magnet the F0-probe is connected, modulation is on and
	   the gaussmeter is either in locked state or is actively searching to
	   achieve the lock (if it's just in TRANS L-H or H-L state check again) */

	bp = buffer + 2;     /* skip first two chars of status byte */

	do     /* loop through remaining chars in status byte */
	{
		switch ( *bp )
		{
			case '0' :      /* Probe F0 is connected -> ok for S-band */
				if ( exist_device( "s_band" ) )
					break;
				eprint( FATAL, "Wrong field probe (F0) connected to the "
						"Bruker ER035M NMR gaussmeter.\n" );
				THROW( EXCEPTION );
				

			case '1' :      /* Probe F1 is connected -> ok for X-band*/
				if ( exist_device( "x_band" ) )
					break;
				eprint( FATAL, "Wrong field probe (F1) connected to the "
						"Bruker ER035M NMR gaussmeter.\n" );
				THROW( EXCEPTION );

			case '2' :      /* No probe connected -> error */
				eprint( FATAL, "No field probe connected to the Bruker ER035M "
						"NMR gaussmeter.\n" );
				THROW( EXCEPTION );

			case '3' :      /* Error temperature -> error */
				eprint( FATAL, "Temperature error for Bruker ER035M NMR "
						"gaussmeter.\n" );
				THROW( EXCEPTION );

			case '4' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				eprint( FATAL, "Bruker ER035M NMR gaussmeter can't find the "
						"actual field.\n" );
				THROW( EXCEPTION );

			case '5' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				eprint( FATAL, "Bruker ER035M NMR gaussmeter can't find the "
						"actual field.\n" );
				THROW( EXCEPTION );

			case '6' :      /* MOD OFF -> error (should never happen */
				eprint( FATAL, "Modulation of Bruker ER035M NMR gaussmeter "
						"is switched off.\n" );
				THROW( EXCEPTION );

			case '7' :      /* MOD POS -> ok (default state) */
				break;

			case '8' :      /* MOD NEG -> ok (should never happen */
				break;                     /* because of intialization) */ 

			case '9' :      /* System in lock -> very good... */
				nmr.state = ER036M_LOCKED;
				break;

			case 'A' :      /* FIELD ? -> error (doesn't seem to work) */
				eprint( FATAL, "Bruker ER035M NMR gaussmeter has "
						"unidentifiable problem.\n" );
				THROW( EXCEPTION );

			case 'B' :      /* SU active -> ok */
				nmr.state = ER036M_SU_ACTIVE;
				break;

			case 'C' :      /* SD active -> ok */
				nmr.state = ER036M_SD_ACTIVE;
				break;

			case 'D' :      /* OU active -> ok */
				nmr.state = ER036M_OU_ACTIVE;
				break;

			case 'E' :      /* OD active -> ok */
				nmr.state = ER036M_OD_ACTIVE;
				break;

			case 'F' :      /* Search active but just at search limit -> ok */
				nmr.state = ER036M_SEARCH_AT_LIMIT;
				break;
		}

	} while ( *bp++ != '\r' ); 


	/* Switch the display on */

	if ( gpib_write( nmr.device, "ED", 2 ) == FAILURE )
	{
		eprint( FATAL, "Can't access the Bruker ER035M NMR gaussmeter.\n" );
		THROW( EXCEPTION );
	}
	usleep( ER036M_WAIT );

	/* Find out the resolution and set it to at least 2 digits */

	if ( gpib_write( nmr.device, "RS", 2 ) == FAILURE )
	{
		eprint( FATAL, "Can't access the Bruker ER035M NMR gaussmeter.\n" );
		THROW( EXCEPTION );
	}
	usleep( ER036M_WAIT );

	length = 20;
	if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "Can't access the Bruker ER035M NMR gaussmeter.\n" );
		THROW( EXCEPTION );
	}

	switch ( buffer[ 2 ] )
	{
		case '1' :                    /* set resolution to 2 digits */
			if ( gpib_write( nmr.device, "RS2", 3 ) == FAILURE )
			{
				eprint( FATAL, "Can't access the Bruker ER035M NMR "
						"gaussmeter.\n" );
				THROW( EXCEPTION );
			}
			usleep( ER036M_WAIT );

		case '2' :
			nmr.resolution = LOW;
			break;

		case '3' :
			nmr.resolution = HIGH;
			break;

		default :                     /* should never happen... */
		{
			eprint( FATAL, "Undocumented data received from Bruker ER035M NMR "
					"gaussmeter.\n" );
			THROW( EXCEPTION );
		}
	}

	/* If the gaussmeter is already locked just get the field value, other-
	   wise try to achieve locked state */

	if ( nmr.state != ER036M_LOCKED )
	{
		v = find_field( );
		nmr.field = v->val.dval;
		vars_pop( v );
	}
	else
		nmr.field = er035m_get_field( );

	return 1;
}


void er035m_exit_hook( void )
{
	if ( ! er035m_is_needed )
		return;

	if ( nmr.device != -1 )
		gpib_local( nmr.device );
}


/*****************************************************************************/
/*                                                                           */
/*              exported functions                                           */
/*                                                                           */
/*****************************************************************************/


/*----------------------------------------------------------------*/
/* find_field() tries to get the gaussmeter into the locked state */
/* and returns the current field alue in a variable.              */
/*----------------------------------------------------------------*/

Var *find_field( void )
{
	char buffer[ 21 ];
	char *bp;
	long length;


	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, 0.0 );


	/* If gaussmeter is in oszillator up/down state or the state is unknown
	   (i.e. it's standing somewhere but not searching) search for field.
	   Starting with searching down is just as probable the wrong decision
	   as searching up... */

	if ( ( nmr.state == NMR_OU_ACTIVE || nmr.state == NMR_OD_ACTIVE ||
		   nmr.state == NMR_UNKNOWN ) &&
		 gpib_write( nmr.device, "SD", 2 ) == FAILURE )
	{
		eprint( FATAL, "Can't access the Bruker ER035M NMR gaussmeter.\n" );
		THROW( EXCEPTION );
	}
	usleep( NMR_WAIT );

	/* wait for gaussmeter to go into lock state (or FAILURE) */

	while ( nmr.state != NMR_LOCKED )
	{
		/* Get status byte and check if lock was achieved */

		if ( gpib_write( nmr.device, "PS", 2 ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker ER035M NMR "
					"gaussmeter.\n" );
			THROW( EXCEPTION );
		}
		usleep( NMR_WAIT );

		length = 20;
		if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker ER035M NMR "
					"gaussmeter.\n" );
			THROW( EXCEPTION );
		}

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
					nmr.state = NMR_LOCKED;
					break;

				case 'A' :      /* FIELD ? -> error */
					eprint( FATAL, "Bruker ER035M NMR gaussmeter has "
							"unidentifiable problem.\n" );
					THROW( EXCEPTION );

				case 'B' :      /* SU active -> ok */
					nmr.state = NMR_SU_ACTIVE;
					break;

				case 'C' :      /* SD active */
					nmr.state = NMR_SD_ACTIVE;
					break;

				case 'D' :      /* OU active -> error (should never happen) */
					nmr.state = NMR_OU_ACTIVE;
					eprint( FATAL, "Bruker ER035M NMR gaussmeter has "
							"unidentifiable problem.\n" );
					THROW( EXCEPTION );

				case 'E' :      /* OD active -> error (should never happen) */
					nmr.state = NMR_OD_ACTIVE;
					eprint( FATAL, "Bruker ER035M NMR gaussmeter has "
							"unidentifiable problem.\n" );
					THROW( EXCEPTION );

				case 'F' :      /* Search active but at a search limit -> ok*/
					nmr.state = NMR_SEARCH_AT_LIMIT;
					break;
			}

		} while ( *bp++ != '\r' );
	};

	/* Finally  get current field value */

	return vars_push( FLOAT_VAR, er035m_get_field( ) );
}


void field_meter_wait( void )
{
	if ( er035m_is_needed )
		usleep( ( nmr.resolution == LOW ? 10 : 20 ) * E2_US );
}



/*****************************************************************************/
/*                                                                           */
/*            internally used functions                                      */
/*                                                                           */
/*****************************************************************************/


/*-----------------------------------------------------------------------*/
/* er035m_get_field() returns the field value from the gaussmeter - it   */
/* will return the settled value but will report a failure if gaussmeter */
/* isn't in lock state. Thus, if the gaussmeter isn't already in locked  */
/* state call find_field() instead.                                      */
/*-----------------------------------------------------------------------*/

double er035m_get_field( void )
{
	char buffer[ 21 ];
	char *state_flag;
	long length;


	/* Repeat asking for field value until it's correct up to LSD */

	do
	{
		/* ask gaussmeter to send the current field and read result */

		if ( gpib_write( nmr.device, "PF", 2 ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker ER035M NMR "
					"gaussmeter.\n" );
			THROW( EXCEPTION );
		}
		usleep( NMR_WAIT );

		length = 20;
		if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
		{
			eprint( FATAL, "Can't access the Bruker ER035M NMR "
					"gaussmeter.\n" );
			THROW( EXCEPTION );
		}

		/* dissasemble field value and flag showing the state */

		state_flag = strrchr( buffer, ',' ) + 1;

		/* report error if gaussmeter isn't in lock state */

		if ( *state_flag >= '3' )
		{
			eprint( FATAL, "Bruker ER035M NMR gaussmeter can't get lock on "
					"current field.\n" );
			THROW( EXCEPTION );
		}

	} while ( *state_flag != '0' );

	/* Finally interpret the field value string */

	*( state_flag - 1 ) = '\0';
	sscanf( buffer + 2, "%lf", &nmr.field );

	return nmr.field;
}
