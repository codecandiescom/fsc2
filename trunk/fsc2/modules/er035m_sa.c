/*
  $Id$
*/



/* On Axel's request: Here's a driver for the Bruker ER035M gaussmeter that
   allows the gaussmeter to be used in conjunction with the Bruker BH15
   field controller. The latter is used to control the field while the NMR
   gaussmeter is only used to measure the field for calibration purposes. */


#include "fsc2.h"
#include "gpib_if.h"


#define DEVICE_NAME "ER035M"     /* name, compare /etc/gpib.conf */


/* exported functions and symbols */

int er035m_sa_init_hook( void );
int er035m_sa_test_hook( void );
int er035m_sa_exp_hook( void );
int er035m_sa_end_of_exp_hook( void );
void er035m_sa_exit_hook( void );

Var *measure_field( Var *v );


/* internally used functions */

static double er035m_sa_get_field( void );
static void er035m_sa_failure( void );


typedef struct
{
	int state;
	int device;
	bool is_needed;
	const char *name;
	double field;
	int resolution;
} NMR;


static NMR nmr;



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
	if ( exist_device( "er035m_s" ) || exist_device( "er035m" ) ||
		 exist_device( "er035m_sas" ) )
	{
		eprint( FATAL, "ER035M: A driver for the ER035 gaussmeter is already "
				"loaded.\n" );
		THROW( EXCEPTION );
	}

	need_GPIB = SET;
	nmr.is_needed = SET;
	nmr.name = DEVICE_NAME;

	nmr.state = ER035M_SA_UNKNOWN;
	nmr.device = -1;

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_sa_test_hook( void )
{
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


	if ( ! nmr.is_needed )
		return 1;

	assert( nmr.device < 0 );

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

	/* Ask gaussmeter to send status byte and test if it does */

	nmr.state = ER035M_SA_UNKNOWN;

try_again:

	if ( DO_STOP )
		THROW( USER_BREAK_EXCEPTION );

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
				if ( exist_device( "aeg_s_band" ) )
					break;
				eprint( FATAL, "%s: Wrong field probe (F0) connected to the "
						"NMR gaussmeter.\n", nmr.name );
				THROW( EXCEPTION );
				

			case '1' :      /* Probe F1 is connected -> OK for X-band*/
				if ( exist_device( "aeg_x_band" ) )
					break;
				eprint( FATAL, "%s: Wrong field probe (F1) connected to the "
						"NMR gaussmeter.\n", nmr.name );
				THROW( EXCEPTION );

			case '2' :      /* No probe connected -> error */
				eprint( FATAL, "%s: No field probe connected to the NMR "
						"gaussmeter.\n", nmr.name );
				THROW( EXCEPTION );

			case '3' :      /* Error temperature -> error */
				eprint( FATAL, "%s: Temperature error from NMR gaussmeter.\n",
						nmr.name );
				THROW( EXCEPTION );

			case '4' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				eprint( FATAL, "%s: NMR gaussmeter can't find the actual "
						"field.\n", nmr.name );
				THROW( EXCEPTION );

			case '5' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				eprint( FATAL, "%s: NMR gaussmeter can't find the actual "
						"field.\n", nmr.name );
				THROW( EXCEPTION );

			case '6' :      /* MOD OFF -> error (should never happen */
				eprint( FATAL, "%s: Modulation of NMR gaussmeter is switched "
						"off.\n", nmr.name );
				THROW( EXCEPTION );

			case '7' :      /* MOD POS -> OK (default state) */
				break;

			case '8' :      /* MOD NEG -> OK (should never happen */
				break;      /* because of initialisation) */ 

			case '9' :      /* System in lock -> very good... */
				nmr.state = ER035M_SA_LOCKED;
				break;

			case 'A' :      /* FIELD ? -> error (doesn't seem to work) */
				eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
						"problem.\n", nmr.name );
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

	/* Find out the resolution and set it to at least 2 digits */

	if ( gpib_write( nmr.device, "RS\r", 3 ) == FAILURE )
		er035m_sa_failure( );
	usleep( ER035M_SA_WAIT );

	length = 20;
	if ( gpib_read( nmr.device, buffer, &length ) == FAILURE )
		er035m_sa_failure( );

	switch ( buffer[ 2 ] )
	{
		case '1' :                    /* set resolution to 2 digits */
			if ( gpib_write( nmr.device, "RS2\r", 4 ) == FAILURE )
				er035m_sa_failure( );
			usleep( ER035M_SA_WAIT );
			/* drop through */

		case '2' :
			nmr.resolution = LOW;
			break;

		case '3' :
			nmr.resolution = HIGH;
			break;

		default :                     /* should never happen... */
		{
			eprint( FATAL, "%s: Undocumented data received from the NMR "
					"gaussmeter.\n", nmr.name );
			THROW( EXCEPTION );
		}
	}

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
	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, 0.0 );

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
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );

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
					eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
							"problem.\n", nmr.name );
					THROW( EXCEPTION );

				case 'B' :      /* SU active -> OK */
					nmr.state = ER035M_SA_SU_ACTIVE;
					break;

				case 'C' :      /* SD active */
					nmr.state = ER035M_SA_SD_ACTIVE;
					break;

				case 'D' :      /* OU active -> error (should never happen) */
					nmr.state = ER035M_SA_OU_ACTIVE;
					eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
							"problem.\n", nmr.name );
					THROW( EXCEPTION );

				case 'E' :      /* OD active -> error (should never happen) */
					nmr.state = ER035M_SA_OD_ACTIVE;
					eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
							"problem.\n", nmr.name );
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


	/* Repeat asking for field value until it's correct up to LSD -
	   it will give up after `ER035M_SA_MAX_RETRIES' retries to avoid
	   getting into an infinite loop when the field is too unstable */

	do
	{
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );

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
			eprint( FATAL, "%s: NMR gaussmeter can't lock on the current "
					"field.\n", nmr.name );
			THROW( EXCEPTION );
		}

	} while ( *state_flag != '0' && tries-- > 0 );

	/* If the maximum number of retries was exceeded we've got to give up */

	if ( tries < 0 )
	{
		eprint( FATAL, "%s: Field is too unstable to be measured with the "
				"requested resolution of %s G.\n", nmr.name,
				nmr.resolution == LOW ? "0.01" : "0.001" );
		THROW( EXCEPTION );
	}

	/* Finally interpret the field value string */

	*( state_flag - 1 ) = '\0';
	sscanf( buffer + 2, "%lf", &nmr.field );

	return nmr.field;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void er035m_sa_failure( void )
{
	eprint( FATAL, "%s: Can't access the NMR gaussmeter.\n", nmr.name );
	THROW( EXCEPTION );
}
