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

#include "er035m_sas.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define ER035M_TEST_FIELD 2000.0    /* returned as current field in test run */
#define ER035M_TEST_RES   0.001

#define PROBE_ORIENTATION_PLUS       0
#define PROBE_ORIENTATION_MINUS      1
#define PROBE_ORIENTATION_UNDEFINED -1
#define UNDEF_RESOLUTION            -1


/* exported functions and symbols */

int er035m_sas_init_hook( void );
int er035m_sas_test_hook( void );
int er035m_sas_exp_hook( void );
int er035m_sas_end_of_exp_hook( void );
void er035m_sas_end_hook( void );

Var *gaussmeter_name( Var *v );
Var *gaussmeter_resolution( Var *v );
Var *gaussmeter_probe_orientation( Var *v );
Var *measure_field( Var *v );
Var *gaussmeter_command( Var *v );


/* internally used functions */

static double er035m_sas_get_field( void );
static int er035m_sas_get_resolution( void );
static void er035m_sas_set_resolution( int res_index );
static bool er035m_sas_open( void );
static bool er035m_sas_close( void );
static bool er035m_sas_write( const char *buf );
static bool er035m_sas_read( char *buf, long *len );
static bool er035m_sas_comm( int type, ... );
static void er035m_sas_comm_fail( void );



typedef struct
{
	bool is_needed;         /* is the gaussmter needed at all? */
	int state;              /* current state of the gaussmeter */
	double field;           /* last measured field */
	int resolution;         /* set to either RES_VERY_LOW = 0.1 G,
							   RES_LOW = 0.01 G or RES_HIGH = 0.001 G */
	struct termios *tio;    /* serial port terminal interface structure */
	char prompt;            /* prompt character send on each reply */
	int probe_orientation;
} NMR;

static NMR nmr, nmr_stored;
static const char *er035m_sas_eol = "\r\n";
static double res_list[ 3 ] = { 0.1, 0.01, 0.001 };

enum {
	UNDEF_RES = -1,
	LOW_RES,
	MEDIUM_RES,
	HIGH_RES
};


/* The gaussmeter seems to be more cooperative if we wait for some time
   (i.e. 200 ms) after each write operation... */

#define E2_US           100000    /* 100 ms, used in calls of usleep() */
#define ER035M_SAS_WAIT 200000    /* this means 200 ms for usleep() */


/* If the field is too unstable the gausmeter might never get to the state
   where the field value is valid with the requested resolution eventhough
   the look state is achieved. 'ER035M_SAS_MAX_RETRIES' determines the
   maximum number of retries before we give up. With a value of 100 and the
   current setting for 'ER035M_SAS_WAIT' of 200 ms it will take at least
   20 s before this will happen.

   Take care: This does not mean that we stop trying to get the field while
   the gaussmeter is still actively searching but only in the case that a
   lock is already achieved but the field value is too unstable! */

#define ER035M_SAS_MAX_RETRIES 100
#define FAIL_RETRIES           5


enum {
	   ER035M_SAS_UNKNOWN = -1,
	   ER035M_SAS_LOCKED = 0,
	   ER035M_SAS_SU_ACTIVE,
	   ER035M_SAS_SD_ACTIVE,
	   ER035M_SAS_OU_ACTIVE,
	   ER035M_SAS_OD_ACTIVE,
	   ER035M_SAS_SEARCH_AT_LIMIT
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

int er035m_sas_init_hook( void )
{
	/* Claim the serial port (throws exception on failure) */

	fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	nmr.is_needed = SET;
	nmr.state = ER035M_SAS_UNKNOWN;
	nmr.resolution = UNDEF_RES;
	nmr.prompt = '\0';

	return 1;
}

/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_sas_test_hook( void )
{
	nmr_stored = nmr;
	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_sas_exp_hook( void )
{
	char buffer[ 21 ], *bp;
	long length = 20;
	Var *v;
	long retries;
	int cur_res;


	nmr = nmr_stored;

	if ( ! nmr.is_needed )
		return 1;

	if ( ! er035m_sas_open( ) )
		er035m_sas_comm_fail( );
	fsc2_usleep( ER035M_SAS_WAIT, UNSET );

	if ( ! er035m_sas_write( "REM" ) )
		er035m_sas_comm_fail( );
	fsc2_usleep( ER035M_SAS_WAIT, UNSET );

	/* Switch the display on */

	if ( er035m_sas_write( "ED" ) == FAIL )
		er035m_sas_comm_fail( );
	fsc2_usleep( ER035M_SAS_WAIT, UNSET );

	/* Find out the curent resolution, and if necessary, change it to the
	   value requested by the user */

	cur_res = er035m_sas_get_resolution( );

	if ( nmr.resolution == UNDEF_RES )
		nmr.resolution = cur_res;
	else if ( nmr.resolution != cur_res )
		er035m_sas_set_resolution( nmr.resolution );

	/* Ask gaussmeter to send status byte and test if it does - sometimes the
	   fucking thing does not answer (i.e. it just seems to send the prompt
	   character and nothing else) so in this case we give it another chance
	   (or even two, see FAIL_RETRIES above) */

	nmr.state = ER035M_SAS_UNKNOWN;

try_again:

	for ( retries = FAIL_RETRIES; ; retries-- )
	{
		stop_on_user_request( );

		if ( er035m_sas_write( "PS" ) == FAIL )
			er035m_sas_comm_fail( );
		fsc2_usleep( ER035M_SAS_WAIT, UNSET );

		length = 20;
		if ( er035m_sas_read( buffer, &length ) == OK )
			break;

		if ( retries <= 0 )
			er035m_sas_comm_fail( );
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
			case '0' : case '1' :     /* Probe F0 or F1 is connected */
				break;

			case '2' :      /* No probe connected -> error */
				print( FATAL, "No field probe connected.\n" );
				THROW( EXCEPTION );

			case '3' :      /* Error temperature -> error */
				print( FATAL, "Temperature error.\n" );
				THROW( EXCEPTION );

			case '4' :      /* TRANS L-H -> test again */
				fsc2_usleep( 500000, SET );
				goto try_again;

			case '5' :      /* TRANS L-H -> test again */
				fsc2_usleep( 500000, SET );
				goto try_again;

			case '6' :      /* MOD OFF -> error (should never happen) */
				print( FATAL, "Modulation is switched off.\n" );
				THROW( EXCEPTION );

			case '7' :      /* MOD POS -> OK */
				nmr.probe_orientation = PROBE_ORIENTATION_PLUS;
				break;

			case '8' :      /* MOD NEG -> OK */
				nmr.probe_orientation = PROBE_ORIENTATION_MINUS;
				break;

			case '9' :      /* System in lock -> very good... */
				nmr.state = ER035M_SAS_LOCKED;
				break;

			case 'A' :      /* FIELD ? -> error (doesn't seem to work) */
				print( FATAL, "Unidentifiable device problem.\n" );
				THROW( EXCEPTION );

			case 'B' :      /* SU active -> OK */
				nmr.state = ER035M_SAS_SU_ACTIVE;
				break;

			case 'C' :      /* SD active -> OK */
				nmr.state = ER035M_SAS_SD_ACTIVE;
				break;

			case 'D' :      /* OU active -> OK */
				nmr.state = ER035M_SAS_OU_ACTIVE;
				break;

			case 'E' :      /* OD active -> OK */
				nmr.state = ER035M_SAS_OD_ACTIVE;
				break;

			case 'F' :      /* Search active but just at search limit -> OK */
				nmr.state = ER035M_SAS_SEARCH_AT_LIMIT;
				break;

			default :
				print( FATAL, "Undocumented data received.\n" );
				THROW( EXCEPTION );
		}
	} while ( *++bp );

	/* If the gaussmeter is already locked just get the field value, other-
	   wise try to achieve locked state */

	if ( nmr.state != ER035M_SAS_LOCKED )
	{
		v = measure_field( NULL );
		nmr.field = v->val.dval;
		vars_pop( v );
	}
	else
		nmr.field = er035m_sas_get_field( );

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int er035m_sas_end_of_exp_hook( void )
{
	if ( ! nmr.is_needed )
		return 1;

	er035m_sas_close( );

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void er035m_sas_end_hook( void )
{
	nmr.is_needed = UNSET;
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
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*/
/* find_field() tries to get the gaussmeter into the locked state */
/* and returns the current field value in a variable.             */
/*----------------------------------------------------------------*/

Var *measure_field( Var *v )
{
	char buffer[ 21 ];
	char *bp;
	long length;
	long retries;


	UNUSED_ARGUMENT( v );

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, ER035M_TEST_FIELD );

	/* If gaussmeter is in oscillator up/down state or the state is unknown
	   (i.e. it's standing somewhere and not searching at all) search for the
	   field. Starting with searching down is just as probable the wrong
	   decision as searching up... */

	if ( ( nmr.state == ER035M_SAS_OU_ACTIVE ||
		   nmr.state == ER035M_SAS_OD_ACTIVE ||
		   nmr.state == ER035M_SAS_UNKNOWN ) &&
		 er035m_sas_write( "SD" ) == FAIL )
		er035m_sas_comm_fail( );
	fsc2_usleep( ER035M_SAS_WAIT, UNSET );

	/* Wait for gaussmeter to go into lock state (or FAIL) */

	while ( nmr.state != ER035M_SAS_LOCKED )
	{
		/* Get status byte and check if lock was achieved - sometimes the
		   fucking thing does not answer (i.e. it just seems to send the
		   prompt character and nothing else) so in this case we give it
		   another chance (or even two, see FAIL_RETRIES above) */

		for ( retries = FAIL_RETRIES; ; retries-- )
		{
			stop_on_user_request( );

			if ( er035m_sas_write( "PS" ) == FAIL )
				er035m_sas_comm_fail( );
			fsc2_usleep( ER035M_SAS_WAIT, UNSET );

			length = 20;
			if ( er035m_sas_read( buffer, &length ) == FAIL )
				break;

			if ( retries <= 0 )
				er035m_sas_comm_fail( );
		}

		bp = buffer;

		do     /* loop through the chars in the status byte */
		{
			switch ( *bp )
			{
				case '0' : case '1' :  /* probe indicator data -> OK */
					break;

				case '2' :             /* no probe -> error */
					print( FATAL, "No field probe connected to the NMR "
						   "gaussmeter.\n" );
					THROW( EXCEPTION );

				case '4' : case '5' :  /* TRANS L-H or H-L -> test again */
					break;

				case '7' : case '8' :  /* MOD POS or NEG -> just go on */
					break;

				case '9' :      /* System finally in lock -> very good... */
					nmr.state = ER035M_SAS_LOCKED;
					break;

				case 'A' :      /* FIELD ? -> error */
					print( FATAL, "NMR gaussmeter has an unidentifiable "
						   "problem.\n" );
					THROW( EXCEPTION );

				case 'B' :      /* SU active -> OK */
					nmr.state = ER035M_SAS_SU_ACTIVE;
					break;

				case 'C' :      /* SD active */
					nmr.state = ER035M_SAS_SD_ACTIVE;
					break;

				case 'D' :      /* OU active -> error (should never happen) */
					nmr.state = ER035M_SAS_OU_ACTIVE;
					print( FATAL, "NMR gaussmeter has an unidentifiable "
						   "problem.\n" );
					THROW( EXCEPTION );

				case 'E' :      /* OD active -> error (should never happen) */
					nmr.state = ER035M_SAS_OD_ACTIVE;
					print( FATAL, "NMR gaussmeter has an unidentifiable "
						   "problem.\n" );
					THROW( EXCEPTION );

				case 'F' :      /* Search active but at a search limit -> OK */
					nmr.state = ER035M_SAS_SEARCH_AT_LIMIT;
					break;

				default :
					print( FATAL, "Undocumented data received from the NMR "
						   "gaussmeter.\n" );
					THROW( EXCEPTION );
			}
		} while ( *++bp );
	}

	/* Finally  get current field value */

	return vars_push( FLOAT_VAR, er035m_sas_get_field( ) );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

Var *gaussmeter_resolution( Var *v )
{
	double res;
	int i;
	int res_index = UNDEF_RES;


	if ( v == NULL )
	{
		if ( FSC2_MODE == PREPARATION && nmr.resolution == UNDEF_RES )
		{
			no_query_possible( );
			THROW( EXCEPTION );
		}

		if ( FSC2_MODE == TEST && nmr.resolution == UNDEF_RES )
			return vars_push( FLOAT_VAR, ER035M_TEST_RES );

		return vars_push( FLOAT_VAR, res_list[ nmr.resolution ] );
	}

	res = get_double( v, "resolution" );

	too_many_arguments( v );

	if ( res <= 0 )
	{
		print( FATAL, "Invalid resolution of %f G.\n", res );
		THROW( EXCEPTION );
	}

	/* Try to match the requested resolution, if necessary use the nearest
	   possible value */

	for ( i = 0; i < 2; i++ )
		if ( res <= res_list[ i ] && res >= res_list[ i + 1 ] )
		{
			res_index = i +
				 ( ( res / res_list[ i ] > res_list[ i + 1 ] / res ) ? 0 : 1 );
			break;
		}

	if ( res_index == UNDEF_RES )
	{
		if ( res >= res_list[ LOW_RES ] )
			res_index = LOW_RES;
		if ( res <= res_list[ HIGH_RES ] )
			res_index = HIGH_RES;
	}

	if ( fabs( res_list[ res_index ] - res ) > 1.0e-2 * res_list[ res_index ] )
		print( WARN, "Can't set resolution to %.3f G, using %.3f G instead.\n",
			   res, res_list[ res_index ] );

	fsc2_assert( res_index >= LOW_RES && res_index <= HIGH_RES );

	nmr.resolution = res_index;

	if ( FSC2_MODE == EXPERIMENT )
		er035m_sas_set_resolution( nmr.resolution );

	return vars_push( FLOAT_VAR, res_list[ nmr.resolution ] );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *gaussmeter_probe_orientation( Var *v )
{
	if ( v == NULL )
	{
		if ( FSC2_MODE == PREPARATION )
		{
			no_query_possible( );
			THROW( EXCEPTION );
		}

		if ( FSC2_MODE == TEST )
			return vars_push( INT_VAR, 1 );

		return vars_push( INT_VAR, ( long ) nmr.probe_orientation );
	}

	print( FATAL, "Device does not allow setting of probe orientation.\n" );
	THROW( EXCEPTION );

	return NULL;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *gaussmeter_command( Var *v )
{
	static char *cmd;


	cmd = NULL;
	vars_check( v, STR_VAR );
	
	if ( FSC2_MODE == EXPERIMENT )
	{
		cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
		if ( er035m_sas_write( cmd ) == FAIL )
		{
			T_free( cmd );
			er035m_sas_comm_fail( );
		}

		fsc2_usleep( ER035M_SAS_WAIT, UNSET );
		T_free( cmd );
	}

	return vars_push( INT_VAR, 1 );
}



/*****************************************************************************/
/*                                                                           */
/*            internally used functions                                      */
/*                                                                           */
/*****************************************************************************/


/*-----------------------------------------------------------------------*/
/* er035m_sas_get_field() returns the field value from the gaussmeter - it */
/* will return the settled value but will report a failure if gaussmeter */
/* isn't in lock state. Another reason for a failure is a field that is  */
/* too unstable to achieve the requested resolution eventhough the       */
/* gaussmeter is already in lock state.                                  */
/* Take care: If the gaussmeter isn't already in the lock state call     */
/*            the function find_field() instead.                         */
/*-----------------------------------------------------------------------*/

static double er035m_sas_get_field( void )
{
	char buffer[ 21 ];
	char *vs;
	char *state_flag;
	long length;
	long tries = ER035M_SAS_MAX_RETRIES;
	long retries;
	const char *res[ ] = { "0.1", "0.01", "0.001" };


	/* Repeat asking for field value until it's correct up to the LSD -
	   it will give up after 'ER035M_SAS_MAX_RETRIES' retries to avoid
	   getting into an infinite loop when the field is too unstable */

	do
	{
		/* Ask gaussmeter to send the current field and read result -
		 sometimes the fucking thing does not answer (i.e. it just seems to
		 send the prompt character and nothing else) so in this case we give
		 it another chance (or even two, see FAIL_RETRIES above) */

		for ( retries = FAIL_RETRIES; ; retries-- )
		{
			stop_on_user_request( );

			if ( er035m_sas_write( "PF" ) == FAIL )
				er035m_sas_comm_fail( );
			fsc2_usleep( ER035M_SAS_WAIT, UNSET );

			length = 20;
			if ( er035m_sas_read( buffer, &length ) == OK )
				break;

			if ( retries <= 0 )
				er035m_sas_comm_fail( );
		}

		/* Disassemble field value and flag showing the state */

		state_flag = strrchr( buffer, ',' ) + 1;

		/* Report error if gaussmeter isn't in lock state */

		if ( *state_flag >= '3' )
		{
			print( FATAL, "NMR gaussmeter can't get lock onto the current "
				   "field.\n" );
			THROW( EXCEPTION );
		}

	} while ( *state_flag != '0' && tries-- > 0 );

	/* If the maximum number of retries was exceeded we've got to give up */

	if ( tries < 0 )
	{
		print( FATAL, "Field is too unstable to be measured with the current "
			   "resolution of %s G.\n", res[ nmr.resolution ] );
		THROW( EXCEPTION );
	}

	/* Finally interpret the field value string - be careful because there can
	   be spaces between the sign and the number, something that sscanf does
	   not like. We also don't care about the sign of the field, so we just
	   drop it... */

	*( state_flag - 1 ) = '\0';
	for ( vs = buffer; ! isdigit( *vs ); vs++ )
		/* empty */ ;
	sscanf( vs, "%lf", &nmr.field );

	return nmr.field;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static int er035m_sas_get_resolution( void )
{
	int retries;
	char buffer[ 20 ];
	long length = 20;


	for ( retries = FAIL_RETRIES; ; retries-- )
	{
		stop_on_user_request( );

		if ( er035m_sas_write( "RS" ) == FAIL )
			er035m_sas_comm_fail( );

		fsc2_usleep( ER035M_SAS_WAIT, UNSET );

		length = 20;
		if ( er035m_sas_read( buffer, &length ) == OK )
			break;

		if ( retries <= 0 )
			er035m_sas_comm_fail( );
	}

	switch ( buffer[ 2 ] )
	{
		case '1' :
			return LOW_RES;

		case '2' :
			return MEDIUM_RES;

		case '3' :
			return HIGH_RES;
	}

	print( FATAL, "Undocumented data received.\n" );
	THROW( EXCEPTION );

	return UNDEF_RES;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void er035m_sas_set_resolution( int res_index )
{
	char buf[ 4 ];


	sprintf( buf, "RS%1d", res_index + 1 );
	if ( er035m_sas_write( buf ) == FAIL )
		er035m_sas_comm_fail( );

	fsc2_usleep( ER035M_SAS_WAIT, UNSET );

}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_sas_open( void )
{
	return er035m_sas_comm( SERIAL_INIT );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_sas_close( void )
{
	return er035m_sas_comm( SERIAL_EXIT );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_sas_write( const char *buf )
{
	static char *wrbuf = NULL;
	static long wrlen = 0;
	bool res;


	if ( buf == NULL || *buf == '\0' )
		return OK;
	wrlen = strlen( buf );

	if ( er035m_sas_eol != NULL && strlen( er035m_sas_eol ) > 0 )
	{
		wrlen += strlen( er035m_sas_eol );

		TRY
		{
			wrbuf = CHAR_P T_malloc( wrlen + 1 );
			TRY_SUCCESS;
		}

		strcpy( wrbuf, buf );
		strcat( wrbuf, er035m_sas_eol );

		res = er035m_sas_comm( SERIAL_WRITE, wrbuf );
		T_free( wrbuf );
	}
	else
		res = er035m_sas_comm( SERIAL_WRITE, buf );

	return res;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_sas_read( char *buf, long *len )
{
	char *ptr;


	if ( buf == NULL || *len == 0 )
		return OK;

	if ( ! er035m_sas_comm( SERIAL_READ, buf, len ) )
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
		/* empty */ ;
	*len -= ( long ) ( ptr - buf );

	if ( *len == 0 )          /* if nothing (except the prompt) was received */
		return FAIL;

	memmove( buf, ptr, *len + 1 );

	return OK;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool er035m_sas_comm( int type, ... )
{
	va_list ap;
	char *buf;
	long len;
	long *lptr;
	long read_retries = 10;            /* number of times we try to read */


	switch ( type )
	{
		case SERIAL_INIT :
			/* We need exclussive access to the serial port and we also need
			   non-blocking mode to avoid hanging indefinitely if the other
			   side does not react. O_NOCTTY is set because the serial port
			   should not become the controlling terminal, otherwise line
			   noise read as a CTRL-C might kill the program. */

			if ( ( nmr.tio = fsc2_serial_open( SERIAL_PORT, DEVICE_NAME,
						  O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
				return FAIL;

			/* Switch off parity checking (8N1) and use of 2 stop bits and
			   clear character size mask, set character size mask to CS8 and
			   the flag for ignoring modem lines, enable reading and, finally,
			   set the baud rate. */

			nmr.tio->c_cflag &= ~ ( PARENB | CSTOPB | CSIZE );

			nmr.tio->c_cflag |= CS8 | CLOCAL | CREAD;
			cfsetispeed( nmr.tio, SERIAL_BAUDRATE );
			cfsetospeed( nmr.tio, SERIAL_BAUDRATE );

			nmr.tio->c_iflag = IGNBRK;
			nmr.tio->c_oflag = 0;
			nmr.tio->c_lflag = 0;
			fsc2_tcflush( SERIAL_PORT, TCIOFLUSH );
			fsc2_tcsetattr( SERIAL_PORT, TCSANOW, nmr.tio );
			break;

		case SERIAL_EXIT :
			er035m_sas_write( "LOC" );
			fsc2_serial_close( SERIAL_PORT );
			break;

		case SERIAL_WRITE :
			va_start( ap, type );
			buf = va_arg( ap, char * );
			va_end( ap );

			len = strlen( buf );
			if ( fsc2_serial_write( SERIAL_PORT, buf, len ) != len )
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
					fsc2_usleep( ER035M_SAS_WAIT, UNSET );
				len = fsc2_serial_read( SERIAL_PORT, buf, *lptr );
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
			print( FATAL, "INTERNAL ERROR detected at %s:%d.\n",
				   __FILE__, __LINE__ );
			THROW( EXCEPTION );
	}

	return OK;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void er035m_sas_comm_fail( void )
{
	print( FATAL, "Can't access the NMR gaussmeter.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */

