/*
  $Id$
*/


#include "fsc2.h"


#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


/* definitions for serial port access */

#define SERIAL_BAUDRATE B9600        /* baud rate of field controller */
#define SERIAL_PORT     1            /* serial port number (i.e. COM2) */


#define DEVICE_NAME "ER035M_S"       /* name of device */


/* exported functions and symbols */

int er035m_s_init_hook( void );
int er035m_s_test_hook( void );
int er035m_s_exp_hook( void );
int er035m_s_end_of_exp_hook( void );
void er035m_s_end_hook( void );

Var *find_field( Var *v );
Var *field_resolution( Var *v );
Var *field_meter_wait( Var *v );

bool is_gaussmeter = UNSET;         /* tested by magnet power supply driver */


/* internally used functions */

static double er035m_s_get_field( void );
static bool er035m_s_open( void );
static bool er035m_s_close( void );
static bool er035m_s_write( const char *buf, long len );
static bool er035m_s_read( char *buf, long *len );
static bool er035m_s_comm( int type, ... );



typedef struct
{
	int state;
	bool is_needed;
	const char *name;
	double field;
	int resolution;

    int fd;                 /* file descriptor for serial port */
    struct termios old_tio, /* serial port terminal interface structures */
                   new_tio;
} NMR;


static NMR nmr;
static char serial_port[ ] = "/dev/ttyS*";
static char er035m_s_eol[ ] = "\r\n";



/* The gaussmeter seems to be more cooperative if we wait for some time
   (i.e. 200 ms) after each write operation... */

#define E2_US         100000    /* 100 ms, used in calls of usleep() */
#define ER035M_S_WAIT 200000    /* this means 200 ms for usleep() */


/* If the field is unstable the gausmeter might never get to the state where
   the field value is valid with the requested resolution eventhough the look
   state is achieved. `ER035M_S_MAX_RETRIES' tells how many times we retry in
   this case. With a value of 100 and the current setting of `ER035M_S_WAIT'
   of 200 ms it will take at least 20 s before this will happen.

   Take care: This does not mean that we stop trying to get the field while
   the gaussmeter is still actively searching but only in the case that a
   lock is already achieved but the field value is too unstable! */

#define ER035M_S_MAX_RETRIES 100


enum {
	   ER035M_S_UNKNOWN = -1,
	   ER035M_S_LOCKED = 0,
	   ER035M_S_SU_ACTIVE,
	   ER035M_S_SD_ACTIVE,
	   ER035M_S_OU_ACTIVE,
	   ER035M_S_OD_ACTIVE,
	   ER035M_S_SEARCH_AT_LIMIT
};


enum {
	   SERIAL_INIT,
	   SERIAL_READ,
	   SERIAL_WRITE,
	   SERIAL_EXIT
};


/*****************************************************************************/
/*                                                                           */
/*                  hook functions                                           */
/*                                                                           */
/*****************************************************************************/


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_s_init_hook( void )
{
	/* Set global flag to tell magnet power supply driver that the
	   gaussmeter has already been loaded */

	is_gaussmeter = SET;

	if ( exist_device( "bh15" ) )
	{
		eprint( FATAL, "ER035M_S: Driver for Bruker BH15 field controller is "
				"already loaded - there can only be one gaussmeter." );
		THROW( EXCEPTION );
	}

	if ( exist_device( "er035m" ) )
	{
		eprint( FATAL, "ER035M_S: Driver for ER035 gaussmeter (connected to "
				"IEEE bus) is already loaded - there can only be one "
				"gaussmeter." );
		THROW( EXCEPTION );
	}

	if ( ! exist_device( "s_band" ) && ! exist_device( "aeg_x_band" ) )
	{	
		eprint( WARN, "ER035M_S: Driver for NMR gaussmeter is loaded "
				"but no appropriate magnet power supply driver." );
		nmr.is_needed = UNSET;
		return 0;
	}

	/* Claim the serial port */

	if ( SERIAL_PORT >= NUM_SERIAL_PORTS || SERIAL_PORT < 0 )
	{
		eprint( FATAL, "ER035M_S: Serial port number %d out of valid range "
				"(0-%d).", SERIAL_PORT, NUM_SERIAL_PORTS - 1 );
		THROW( EXCEPTION );
	}

	if ( need_Serial_Port[ SERIAL_PORT ] )
	{
		eprint( FATAL, "ER035M_S: Serial port %d (i.e. /dev/ttyS%d or COM%d) "
				"is already in use by another device.",
				SERIAL_PORT, SERIAL_PORT, SERIAL_PORT + 1 );
		THROW( EXCEPTION );
	}

	need_Serial_Port[ SERIAL_PORT ] = SET;
	*strrchr( serial_port, '*' ) = ( char ) ( SERIAL_PORT + '0' );

	nmr.is_needed = SET;
	nmr.name = DEVICE_NAME;
	nmr.state = ER035M_S_UNKNOWN;

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_s_test_hook( void )
{
	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_s_exp_hook( void )
{
	char buffer[ 21 ], *bp;
	long length = 20;
	int try_count = 0;
	Var *v;


	if ( ! nmr.is_needed )
		return 1;

	if ( ! er035m_s_open( ) )
	{
		eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
		THROW( EXCEPTION );
	}
	usleep( ER035M_S_WAIT );

	if ( ! er035m_s_write( "REM", 3 ) )
	{
		eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
		THROW( EXCEPTION );
	}

	/* Ask gaussmeter to send status byte and test if it does */

	nmr.state = ER035M_S_UNKNOWN;

try_again:

	if ( er035m_s_write( "PS", 2 ) == FAIL )
	{
		eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
		THROW( EXCEPTION );
	}
	usleep( ER035M_S_WAIT );

	length = 20;
	if ( er035m_s_read( buffer, &length ) == FAIL )
	{
		eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
		THROW( EXCEPTION );
	}

	/* Now look if the status byte says that device is OK where OK means that
	   for the X-Band magnet the F0-probe and for the S-band the F1-probe is
	   connected, modulation is on and the gaussmeter is either in locked
	   state or is actively searching to achieve the lock (if it's just in
	   TRANS L-H or H-L state check again) */

	bp = buffer;

	do     /* loop through remaining chars in status byte */
	{
		switch ( *bp )
		{
			case '0' :      /* Probe F0 is connected -> OK for S-band */
				if ( exist_device( "s_band" ) )
					break;
				er035m_s_close( );
				eprint( FATAL, "%s: Wrong field probe (F0) connected to the "
						"NMR gaussmeter.", nmr.name );
				THROW( EXCEPTION );
				

			case '1' :      /* Probe F1 is connected -> OK for X-band*/
				if ( exist_device( "aeg_x_band" ) )
					break;
				er035m_s_close( );
				eprint( FATAL, "%s: Wrong field probe (F1) connected to the "
						"NMR gaussmeter.", nmr.name );
				THROW( EXCEPTION );

			case '2' :      /* No probe connected -> error */
				er035m_s_close( );
				eprint( FATAL, "%s: No field probe connected to the NMR "
						"gaussmeter.", nmr.name );
				THROW( EXCEPTION );

			case '3' :      /* Error temperature -> error */
				er035m_s_close( );
				eprint( FATAL, "%s: Temperature error from NMR gaussmeter.",
						nmr.name );
				THROW( EXCEPTION );

			case '4' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				er035m_s_close( );
				eprint( FATAL, "%s: NMR gaussmeter can't find the actual "
						"field.", nmr.name );
				THROW( EXCEPTION );

			case '5' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				er035m_s_close( );
				eprint( FATAL, "%s: NMR gaussmeter can't find the actual "
						"field.", nmr.name );
				THROW( EXCEPTION );

			case '6' :      /* MOD OFF -> error (should never happen) */
				er035m_s_close( );
				eprint( FATAL, "%s: Modulation of NMR gaussmeter is switched "
						"off.", nmr.name );
				THROW( EXCEPTION );

			case '7' :      /* MOD POS -> OK (default state) */
				break;

			case '8' :      /* MOD NEG -> OK */
				break;

			case '9' :      /* System in lock -> very good... */
				nmr.state = ER035M_S_LOCKED;
				break;

			case 'A' :      /* FIELD ? -> error (doesn't seem to work) */
				er035m_s_close( );
				eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
						"problem.", nmr.name );
				THROW( EXCEPTION );

			case 'B' :      /* SU active -> OK */
				nmr.state = ER035M_S_SU_ACTIVE;
				break;

			case 'C' :      /* SD active -> OK */
				nmr.state = ER035M_S_SD_ACTIVE;
				break;

			case 'D' :      /* OU active -> OK */
				nmr.state = ER035M_S_OU_ACTIVE;
				break;

			case 'E' :      /* OD active -> OK */
				nmr.state = ER035M_S_OD_ACTIVE;
				break;

			case 'F' :      /* Search active but just at search limit -> OK */
				nmr.state = ER035M_S_SEARCH_AT_LIMIT;
				break;
		}

	} while ( *bp++ ); 

	/* Switch the display on */

	if ( er035m_s_write( "ED", 2 ) == FAIL )
	{
		eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
		THROW( EXCEPTION );
	}
	usleep( ER035M_S_WAIT );

	/* Find out the resolution and set it to at least 2 digits */

	if ( er035m_s_write( "RS", 2 ) == FAIL )
	{
		eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
		THROW( EXCEPTION );
	}
	usleep( ER035M_S_WAIT );

	length = 20;
	if ( er035m_s_read( buffer, &length ) == FAIL )
	{
		eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
		THROW( EXCEPTION );
	}

	switch ( buffer[ 0 ] )
	{
		case '1' :                    /* set resolution to 2 digits */
			if ( er035m_s_write( "RS2", 3 ) == FAIL )
			{
				eprint( FATAL, "%s: Can't access the NMR gaussmeter.",
						nmr.name );
				THROW( EXCEPTION );
			}
			usleep( ER035M_S_WAIT );
			/* drop through */

		case '2' :
			nmr.resolution = LOW;
			break;

		case '3' :
			nmr.resolution = HIGH;
			break;

		default :                     /* should never happen... */
		{
			er035m_s_close( );
			eprint( FATAL, "%s: Undocumented data received from the NMR "
					"gaussmeter.", nmr.name );
			THROW( EXCEPTION );
		}
	}

	/* If the gaussmeter is already locked just get the field value, other-
	   wise try to achieve locked state */

	if ( nmr.state != ER035M_S_LOCKED )
	{
		v = find_field( NULL );
		nmr.field = v->val.dval;
		vars_pop( v );
	}
	else
		nmr.field = er035m_s_get_field( );

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_s_end_of_exp_hook( void )
{
	if ( ! nmr.is_needed )
		return 1;

	er035m_s_close( );

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void er035m_s_end_hook( void )
{
	nmr.is_needed = UNSET;
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

Var *find_field( Var *v )
{
	char buffer[ 21 ];
	char *bp;
	long length;


	v = v;
	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, 0.0 );


	/* If gaussmeter is in oscillator up/down state or the state is unknown
	   (i.e. it's standing somewhere and not searching at all) search for the
	   field. Starting with searching down is just as probable the wrong
	   decision as searching up... */

	if ( ( nmr.state == ER035M_S_OU_ACTIVE ||
		   nmr.state == ER035M_S_OD_ACTIVE ||
		   nmr.state == ER035M_S_UNKNOWN ) &&
		 er035m_s_write( "SD", 2 ) == FAIL )
	{
		eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
		THROW( EXCEPTION );
	}
	usleep( ER035M_S_WAIT );

	/* Wait for gaussmeter to go into lock state (or FAIL) */

	while ( nmr.state != ER035M_S_LOCKED )
	{
		/* Get status byte and check if lock was achieved */

		if ( er035m_s_write( "PS", 2 ) == FAIL )
		{
			eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
			THROW( EXCEPTION );
		}
		usleep( ER035M_S_WAIT );

		length = 20;
		if ( er035m_s_read( buffer, &length ) == FAIL )
		{
			eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
			THROW( EXCEPTION );
		}

		bp = buffer;

		do     /* loop through remaining chars in status byte */
		{
			switch ( *bp )
			{
				case '4' : case '5' :  /* TRANS L-H or H-L -> test again */
					break;

				case '7' : case '8' :  /* MOD POS or NEG -> just go on */
					break;

				case '9' :      /* System finally in lock -> very good... */
					nmr.state = ER035M_S_LOCKED;
					break;

				case 'A' :      /* FIELD ? -> error */
					er035m_s_close( );
					eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
							"problem.", nmr.name );
					THROW( EXCEPTION );

				case 'B' :      /* SU active -> OK */
					nmr.state = ER035M_S_SU_ACTIVE;
					break;

				case 'C' :      /* SD active */
					nmr.state = ER035M_S_SD_ACTIVE;
					break;

				case 'D' :      /* OU active -> error (should never happen) */
					nmr.state = ER035M_S_OU_ACTIVE;
					er035m_s_close( );
					eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
							"problem.", nmr.name );
					THROW( EXCEPTION );

				case 'E' :      /* OD active -> error (should never happen) */
					nmr.state = ER035M_S_OD_ACTIVE;
					er035m_s_close( );
					eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
							"problem.", nmr.name );
					THROW( EXCEPTION );

				case 'F' :      /* Search active but at a search limit -> OK*/
					nmr.state = ER035M_S_SEARCH_AT_LIMIT;
					break;
			}

		} while ( *bp++ );
	}

	/* Finally  get current field value */

	return vars_push( FLOAT_VAR, er035m_s_get_field( ) );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

Var *field_resolution( Var *v )
{
	v = v;
	if ( ! TEST_RUN )
		return vars_push( FLOAT_VAR, nmr.resolution == LOW ? 0.01 : 0.001 );
	else
		return vars_push( FLOAT_VAR, 0.001 );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

Var *field_meter_wait( Var *v )
{
	v = v;

	if ( ! TEST_RUN && nmr.is_needed )
		usleep( ( nmr.resolution == LOW ? 10 : 20 ) * E2_US );
	return vars_push( INT_VAR, 1 );
}



/*****************************************************************************/
/*                                                                           */
/*            internally used functions                                      */
/*                                                                           */
/*****************************************************************************/


/*-----------------------------------------------------------------------*/
/* er035m_s_get_field() returns the field value from the gaussmeter - it */
/* will return the settled value but will report a failure if gaussmeter */
/* isn't in lock state. Another reason for a failure is a field that is  */
/* too unstable to achieve the requested resolution eventhough the       */
/* gaussmeter is already in lock state.                                  */
/* Take care: If the gaussmeter isn't already in the lock state call     */
/*            the function find_field() instead.                         */
/*-----------------------------------------------------------------------*/

static double er035m_s_get_field( void )
{
	char buffer[ 21 ];
	char *vs;
	char *state_flag;
	long length;
	long tries = ER035M_S_MAX_RETRIES;


	/* Repeat asking for field value until it's correct up to the LSD -
	   it will give up after `ER035M_S_MAX_RETRIES' retries to avoid
	   getting into an infinite loop when the field is too unstable */

	do
	{
		/* Ask gaussmeter to send the current field and read result */

		if ( er035m_s_write( "PF", 2 ) == FAIL )
		{
			eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
			THROW( EXCEPTION );
		}
		usleep( ER035M_S_WAIT );

		length = 20;
		if ( er035m_s_read( buffer, &length ) == FAIL )
		{
			eprint( FATAL, "%s: Can't access the NMR gaussmeter.", nmr.name );
			THROW( EXCEPTION );
		}

		/* Disassemble field value and flag showing the state */

		state_flag = strrchr( buffer, ',' ) + 1;

		/* Report error if gaussmeter isn't in lock state */

		if ( *state_flag >= '3' )
		{
			er035m_s_close( );
			eprint( FATAL, "%s: NMR gaussmeter can't get lock onto the "
					"current field.", nmr.name );
			THROW( EXCEPTION );
		}

	} while ( *state_flag != '0' && tries-- > 0 );

	/* If the maximum number of retries was exceeded we've got to give up */

	if ( tries < 0 )
	{
		er035m_s_close( );
		eprint( FATAL, "%s: Field is too unstable to be measured with the "
				"requested resolution of %s G.", nmr.name,
				nmr.resolution == LOW ? "0.01" : "0.001" );
		THROW( EXCEPTION );
	}

	/* Finally interpret the field value string - be careful because there can
	   be spaces between the sign and the number, something that sscanf does
	   not like. We also don't care about the sign of the field, so we just
	   drop it... */

	*( state_flag - 1 ) = '\0';
	for ( vs = buffer; ! isdigit( *vs ); vs++ )
		;
	sscanf( vs, "%lf", &nmr.field );

	return nmr.field;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_s_open( void )
{
	return er035m_s_comm( SERIAL_INIT );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_s_close( void )
{
	return er035m_s_comm( SERIAL_EXIT );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_s_write( const char *buf, long len )
{
	char *wbuf;
	long wlen;
	bool res;


	if ( buf == NULL || len == 0 )
		return OK;

	if ( er035m_s_eol != NULL && strlen( er035m_s_eol ) > 0 )
	{
		wlen = len + strlen( er035m_s_eol );
		wbuf = T_malloc( ( wlen + 1 ) * sizeof( char ) );
		strncpy( wbuf, buf, len );
		strcpy( wbuf + len, er035m_s_eol );

		res = er035m_s_comm( SERIAL_WRITE, wbuf, wlen );
		T_free( wbuf );
	}
	else
		res = er035m_s_comm( SERIAL_WRITE, buf, len );

	if ( ! res )
		er035m_s_close( );
	return res;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_s_read( char *buf, long *len )
{
	char *ptr;


	if ( buf == NULL || *len == 0 )
		return OK;

	if ( ! er035m_s_comm( SERIAL_READ, buf, len ) )
	{
		er035m_s_close( );
		return FAIL;
	}

	/* Make buffer end with zero and remove trailing `*'s (they're always
	   send at the start of a reply) */

	*( strchr( buf, '\r' ) ) = '\0';
	*len = strlen( buf );
	for ( ptr = buf; *ptr == '*'; ptr++ )
		;
	*len -= ( long ) ( ptr - buf );
	memmove( buf, ptr, *len + 1 );

	return OK;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_s_comm( int type, ... )
{
	va_list ap;
	char *buf;
	long len;
	long *lptr;


	va_start( ap, type );

	switch ( type )
	{
		case SERIAL_INIT :
			if ( ( nmr.fd =
				  open( serial_port, O_RDWR | O_NOCTTY | O_NONBLOCK ) ) < 0 )
				return FAIL;

			tcgetattr( nmr.fd, &nmr.old_tio );
			memcpy( &nmr.new_tio, &nmr.old_tio,
					sizeof( struct termios ) );
			nmr.new_tio.c_cflag = SERIAL_BAUDRATE | CS8;
			tcflush( nmr.fd, TCIOFLUSH );
			tcsetattr( nmr.fd, TCSANOW, &nmr.new_tio );
			break;

		case SERIAL_EXIT :
			er035m_s_write( "LOC", 3 );
			tcflush( nmr.fd, TCIOFLUSH );
			tcsetattr( nmr.fd, TCSANOW, &nmr.old_tio );
			close( nmr.fd );
			break;

		case SERIAL_WRITE :
			buf = va_arg( ap, char * );
			len = va_arg( ap, long );
			if ( write( nmr.fd, buf, len ) != len )
			{
				va_end( ap );
				return FAIL;
			}
			break;

		case SERIAL_READ :
			buf = va_arg( ap, char * );
			lptr = va_arg( ap, long * );
			*lptr = read( nmr.fd, buf, *lptr );
			if ( *lptr < 0 )
			{
				*lptr = 0;
				va_end( ap );
				return FAIL;
			}

			/* The most significant bit of the bytes send by the gaussmeter is
			   more or less random, so let's get rid of it... */

			for ( len = 0; len < *lptr; len++ )
				buf[ len ] &= 0x7f;

			break;

		default :
			va_end( ap );
			eprint( FATAL, "%s: INTERNAL ERROR detected at %s:%d.", nmr.name,
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
	}

	va_end( ap );
	return OK;
}
