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

#define SERIAL_PORT     1            /* serial port number (i.e. COM2) */
#define SERIAL_BAUDRATE B9600        /* baud rate of field controller */
#define SERIAL_FLAGS    CS8


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
static bool er035m_s_write( const char *buf );
static bool er035m_s_read( char *buf, long *len );
static bool er035m_s_comm( int type, ... );
static void er035_s_comm_fail( void );



typedef struct
{
	bool is_needed;         /* is the gaussmter needed at all? */
	int state;              /* current state of the gaussmeter */
	double field;           /* last measured field */
	int resolution;         /* LOW = 0.01 G, HIGH = 0.001 G */
    int fd;                 /* file descriptor for serial port */
    struct termios old_tio, /* serial port terminal interface structures */
		           new_tio; /* (stored old and current one) */
	char prompt;            /* prompt character send on each reply */
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
#define FAIL_RETRIES         5


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
	nmr.state = ER035M_S_UNKNOWN;
	nmr.prompt = '\0';

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
	long retries;


	if ( ! nmr.is_needed )
		return 1;

	if ( ! er035m_s_open( ) )
		er035_s_comm_fail( );
	usleep( ER035M_S_WAIT );

	if ( ! er035m_s_write( "REM" ) )
		er035_s_comm_fail( );
	usleep( ER035M_S_WAIT );

	/* Switch the display on */

	if ( er035m_s_write( "ED" ) == FAIL )
		er035_s_comm_fail( );
	usleep( ER035M_S_WAIT );

	/* Ask gaussmeter to send status byte and test if it does - sometimes the
	   fucking thing does not answer (i.e. it just seems to send the prompt
	   character and nothing else) so in this case we give it another chance
	   (or even two, see FAIL_RETRIES above) */

	nmr.state = ER035M_S_UNKNOWN;

try_again:

	for ( retries = FAIL_RETRIES; ; retries-- )
	{
		if ( er035m_s_write( "PS" ) == FAIL )
			er035_s_comm_fail( );
		usleep( ER035M_S_WAIT );

		length = 20;
		if ( er035m_s_read( buffer, &length ) == OK )
			break;

		if ( retries <= 0 )
			er035_s_comm_fail( );
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
				eprint( FATAL, "%s: Wrong field probe (F0) connected to the "
						"NMR gaussmeter.", DEVICE_NAME );
				THROW( EXCEPTION );
				
			case '1' :      /* Probe F1 is connected -> OK for X-band */
				if ( exist_device( "aeg_x_band" ) )
					break;
				eprint( FATAL, "%s: Wrong field probe (F1) connected to the "
						"NMR gaussmeter.", DEVICE_NAME );
				THROW( EXCEPTION );

			case '2' :      /* No probe connected -> error */
				eprint( FATAL, "%s: No field probe connected to the NMR "
						"gaussmeter.", DEVICE_NAME );
				THROW( EXCEPTION );

			case '3' :      /* Error temperature -> error */
				eprint( FATAL, "%s: Temperature error from NMR gaussmeter.",
						DEVICE_NAME );
				THROW( EXCEPTION );

			case '4' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				eprint( FATAL, "%s: NMR gaussmeter can't find the actual "
						"field.", DEVICE_NAME );
				THROW( EXCEPTION );

			case '5' :      /* TRANS L-H -> test again */
				if ( try_count++ < 10 )
					goto try_again;
				eprint( FATAL, "%s: NMR gaussmeter can't find the actual "
						"field.", DEVICE_NAME );
				THROW( EXCEPTION );

			case '6' :      /* MOD OFF -> error (should never happen) */
				eprint( FATAL, "%s: Modulation of NMR gaussmeter is switched "
						"off.", DEVICE_NAME );
				THROW( EXCEPTION );

			case '7' :      /* MOD POS -> OK (default state) */
				break;

			case '8' :      /* MOD NEG -> OK */
				break;

			case '9' :      /* System in lock -> very good... */
				nmr.state = ER035M_S_LOCKED;
				break;

			case 'A' :      /* FIELD ? -> error (doesn't seem to work) */
				eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
						"problem.", DEVICE_NAME );
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

			default :
				eprint( FATAL, "%s: Undocumented data received from the NMR "
						"gaussmeter.", DEVICE_NAME );
				THROW( EXCEPTION );
		}
	} while ( *++bp ); 

	/* Find out the resolution and set it to at least 2 digits */

	if ( er035m_s_write( "RS" ) == FAIL )
		er035_s_comm_fail( );
	usleep( ER035M_S_WAIT );

	length = 20;
	if ( er035m_s_read( buffer, &length ) == FAIL )
		er035_s_comm_fail( );

	switch ( buffer[ 0 ] )
	{
		case '1' :                    /* set resolution to 2 digits */
			if ( er035m_s_write( "RS2" ) == FAIL )
				er035_s_comm_fail( );
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
			eprint( FATAL, "%s: Undocumented data received from the NMR "
					"gaussmeter.", DEVICE_NAME );
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
	er035m_s_close( );
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
	long retries;


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
		 er035m_s_write( "SD" ) == FAIL )
	{
		eprint( FATAL, "%s: Can't access the NMR gaussmeter.", DEVICE_NAME );
		THROW( EXCEPTION );
	}
	usleep( ER035M_S_WAIT );

	/* Wait for gaussmeter to go into lock state (or FAIL) */

	while ( nmr.state != ER035M_S_LOCKED )
	{
		/* Get status byte and check if lock was achieved - sometimes the
		   fucking thing does not answer (i.e. it just seems to send the
		   prompt character and nothing else) so in this case we give it
		   another chance (or even two, see FAIL_RETRIES above) */

		for ( retries = FAIL_RETRIES; ; retries-- )
		{
			if ( er035m_s_write( "PS" ) == FAIL )
				er035_s_comm_fail( );
			usleep( ER035M_S_WAIT );

			length = 20;
			if ( er035m_s_read( buffer, &length ) == FAIL )
				break;

			if ( retries <= 0 )
				er035_s_comm_fail( );
		}

		bp = buffer;

		do     /* loop through the chars in the status byte */
		{
			switch ( *bp )
			{
				case '0' : case '1' :  /* probe indicator data -> OK */
					break;

				case '2' :             /* no probe -> error */
					eprint( FATAL, "%s: No field probe connected to the NMR "
							"gaussmeter.", DEVICE_NAME );
					THROW( EXCEPTION );

				case '4' : case '5' :  /* TRANS L-H or H-L -> test again */
					break;

				case '7' : case '8' :  /* MOD POS or NEG -> just go on */
					break;

				case '9' :      /* System finally in lock -> very good... */
					nmr.state = ER035M_S_LOCKED;
					break;

				case 'A' :      /* FIELD ? -> error */
					eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
							"problem.", DEVICE_NAME );
					THROW( EXCEPTION );

				case 'B' :      /* SU active -> OK */
					nmr.state = ER035M_S_SU_ACTIVE;
					break;

				case 'C' :      /* SD active */
					nmr.state = ER035M_S_SD_ACTIVE;
					break;

				case 'D' :      /* OU active -> error (should never happen) */
					nmr.state = ER035M_S_OU_ACTIVE;
					eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
							"problem.", DEVICE_NAME );
					THROW( EXCEPTION );

				case 'E' :      /* OD active -> error (should never happen) */
					nmr.state = ER035M_S_OD_ACTIVE;
					eprint( FATAL, "%s: NMR gaussmeter has an unidentifiable "
							"problem.", DEVICE_NAME );
					THROW( EXCEPTION );

				case 'F' :      /* Search active but at a search limit -> OK */
					nmr.state = ER035M_S_SEARCH_AT_LIMIT;
					break;

				default :
					eprint( FATAL, "%s: Undocumented data received from the "
							"NMR gaussmeter.", DEVICE_NAME );
					THROW( EXCEPTION );
			}
		} while ( *++bp );
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
	long retries;


	/* Repeat asking for field value until it's correct up to the LSD -
	   it will give up after `ER035M_S_MAX_RETRIES' retries to avoid
	   getting into an infinite loop when the field is too unstable */

	do
	{
		/* Ask gaussmeter to send the current field and read result -
		 sometimes the fucking thing does not answer (i.e. it just seems to
		 send the prompt character and nothing else) so in this case we give
		 it another chance (or even two, see FAIL_RETRIES above) */

		for ( retries = FAIL_RETRIES; ; retries-- )
		{
			if ( er035m_s_write( "PF" ) == FAIL )
				er035_s_comm_fail( );
			usleep( ER035M_S_WAIT );

			length = 20;
			if ( er035m_s_read( buffer, &length ) == OK )
				break;

			if ( retries <= 0 )
				er035_s_comm_fail( );
		}

		/* Disassemble field value and flag showing the state */

		state_flag = strrchr( buffer, ',' ) + 1;

		/* Report error if gaussmeter isn't in lock state */

		if ( *state_flag >= '3' )
		{
			eprint( FATAL, "%s: NMR gaussmeter can't get lock onto the "
					"current field.", DEVICE_NAME );
			THROW( EXCEPTION );
		}

	} while ( *state_flag != '0' && tries-- > 0 );

	/* If the maximum number of retries was exceeded we've got to give up */

	if ( tries < 0 )
	{
		eprint( FATAL, "%s: Field is too unstable to be measured with the "
				"requested resolution of %s G.", DEVICE_NAME,
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

static bool er035m_s_write( const char *buf )
{
	char *wbuf;
	long wlen;
	bool res;


	if ( buf == NULL || *buf == '\0' )
		return OK;
	wlen = strlen( buf );

	if ( er035m_s_eol != NULL && strlen( er035m_s_eol ) > 0 )
	{
		wlen += strlen( er035m_s_eol );
		wbuf = get_string( wlen );
		strcpy( wbuf, buf );
		strcat( wbuf, er035m_s_eol );

		res = er035m_s_comm( SERIAL_WRITE, wbuf );
		T_free( wbuf );
	}
	else
		res = er035m_s_comm( SERIAL_WRITE, buf );

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
		return FAIL;

	/* If the prompt character send by the device with each reply isn't known
	   yet take it to be the very first byte read (default is '*' but who
	   knows if this got changed by some unlucky coincidence...) */

	if ( nmr.prompt == '\0' )
		nmr.prompt = buf[ 0 ];

	/* Make buffer end with '\0' (but take into account that the device
	   sometimes may send complete BS) */

	buf[ *len ] = '\0';         /* make sure there's an end of string marker */

	if ( ( ptr = strchr( buf, '\r' ) ) ||
		 ( ptr = strchr( buf, '\n' ) )    )
	{
		*ptr = '\0';
		*len = ptr - buf;
	}

	/* Remove leading prompt characters if there are any */

	for ( ptr = buf; *ptr == nmr.prompt; ptr++ )
		;
	*len -= ( long ) ( ptr - buf );

	if ( *len == 0 )          /* if nothing (except the prompt) was received */
		return FAIL;

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
	long read_retries = 10;            /* number of times we try to read */


	switch ( type )
	{
		case SERIAL_INIT :
			if ( ( nmr.fd =
				   open( serial_port, O_RDWR | O_NOCTTY | O_NONBLOCK ) ) < 0 )
				return FAIL;

			tcgetattr( nmr.fd, &nmr.old_tio );
			memcpy( &nmr.new_tio, &nmr.old_tio,
					sizeof( struct termios ) );
			nmr.new_tio.c_cflag = SERIAL_BAUDRATE | SERIAL_FLAGS;
			tcflush( nmr.fd, TCIOFLUSH );
			tcsetattr( nmr.fd, TCSANOW, &nmr.new_tio );
			break;

		case SERIAL_EXIT :
			er035m_s_write( "LOC" );
			tcflush( nmr.fd, TCIOFLUSH );
			tcsetattr( nmr.fd, TCSANOW, &nmr.old_tio );
			close( nmr.fd );
			break;

		case SERIAL_WRITE :
			va_start( ap, type );
			buf = va_arg( ap, char * );
			va_end( ap );

			len = strlen( buf );
			if ( write( nmr.fd, buf, len ) != len )
				return FAIL;
			break;

		case SERIAL_READ :
			va_start( ap, type );
			buf = va_arg( ap, char * );
			lptr = va_arg( ap, long * );
			va_end( ap );

			/* The gaussmeter might not be ready yet to send data, in this
			   case we retry it a few times before giving up */

			len = 1;
			do
			{
				if ( len < 0 )
					usleep( ER035M_S_WAIT );
				len = read( nmr.fd, buf, *lptr );
			}
			while ( len < 0 && errno == EAGAIN && read_retries-- > 0 );
				
			if ( len < 0 )
			{
				*lptr = 0;
				return FAIL;
			}

			/* The two most significant bits of each byte the gaussmeter
			   sends seem to be invalid, so let's get rid of them... */

			*lptr = len;
			for ( len = 0; len < *lptr; len++ )
				buf[ len ] &= 0x3f;
			break;

		default :
			eprint( FATAL, "%s: INTERNAL ERROR detected at %s:%d.",
					DEVICE_NAME, __FILE__, __LINE__ );
			THROW( EXCEPTION );
	}

	return OK;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void er035_s_comm_fail( void )
{
	eprint( FATAL, "%s: Can't access the NMR gaussmeter.", DEVICE_NAME );
	THROW( EXCEPTION );
}
