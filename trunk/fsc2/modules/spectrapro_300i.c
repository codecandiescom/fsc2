/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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

#include "spectrapro_300i.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define SPECTRAPRO_300I_WAIT  100000   /* 100 ms */


int spectrapro_300i_init_hook( void );
int spectrapro_300i_exp_hook( void );
int spectrapro_300i_end_of_exp_hook( void );
void spectrapro_300i_exit_hook( void );
Var *monochromator_name( Var *v );
Var *monochromator_wavelength( Var *v );
static bool spectrapro_300i_open( void );
static bool spectrapro_300i_close( void );
static void spectrapro_300i_send( const char *buf );
static char *spectrapro_300i_talk( const char *buf, size_t len );
static bool spectrapro_300i_comm( int type, ... );
static void spectrapro_300i_comm_fail( void );


typedef struct SPECTRAPRO_300I SPECTRAPRO_300I;

struct SPECTRAPRO_300I {
	bool is_needed;         /* is the monochromator needed at all?      */
    struct termios *tio;    /* serial port terminal interface structure */
	double wavelength;      /* current wavelength setting of grating    */
};


SPECTRAPRO_300I spectrapro_300i, spectrapro_300i_stored;


enum {
	   SERIAL_INIT,
	   SERIAL_EXIT,
	   SERIAL_READ,
	   SERIAL_WRITE
};


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int spectrapro_300i_init_hook( void )
{
	/* Claim the serial port (throws exception on failure) */

	fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	spectrapro_300i.is_needed = SET;

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int spectrapro_300i_exp_hook( void )
{
	if ( ! spectrapro_300i.is_needed )
		return 1;

	if ( ! spectrapro_300i_open( ) )
		spectrapro_300i_comm_fail( );

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int spectrapro_300i_end_of_exp_hook( void )
{
	if ( ! spectrapro_300i.is_needed )
		return 1;

	spectrapro_300i_close( );

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spectrapro_300i_exit_hook( void )
{
	spectrapro_300i.is_needed = UNSET;
	spectrapro_300i_close( );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *monochromator_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *monochromator_wavelength( Var *v )
{
	const char *reply;
	double wl;
	char *buf = NULL;


	CLOBBER_PROTECT( buf );

	if ( v == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			reply = spectrapro_300i_talk( "?NM", 100 );
			spectrapro_300i.wavelength = T_atod( reply ) * 1.0e-9;
			T_free( ( char * ) reply );
		}

		return vars_push( FLOAT_VAR, spectrapro_300i.wavelength );
	}

	wl = get_double( v, "wavelength" );

	if ( wl < 0.0 )
	{
		print( FATAL, "Invalid negative wavelength.\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			buf = get_string( "%.3f GOTO", 1.0e9 * wl );
			spectrapro_300i_send( buf );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			if ( buf != NULL )
				T_free( buf );
			RETHROW( );
		}

		T_free( buf );
	}

	spectrapro_300i.wavelength = wl;

	return vars_push( FLOAT_VAR, spectrapro_300i.wavelength );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool spectrapro_300i_open( void )
{
	return spectrapro_300i_comm( SERIAL_INIT );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool spectrapro_300i_close( void )
{
	return spectrapro_300i_comm( SERIAL_EXIT );
}


/*--------------------------------------------------------*/
/* Function for commands that just get send to the device */
/* and don't expect any replies.                          */
/*--------------------------------------------------------*/

static void spectrapro_300i_send( const char *buf )
{
	char *lbuf;
	size_t len;
	char reply[ 5 ];
	int repeats = 100;


	fsc2_assert( buf != NULL && *buf != '\0' );

	lbuf = get_string( "%s\r", buf );
	len = strlen( lbuf );

	if ( ! spectrapro_300i_comm( SERIAL_WRITE, lbuf ) )
	{
		T_free( lbuf );
		spectrapro_300i_comm_fail( );
	}

	/* The device always echos the command, we got to get rid of the echo */

	if ( ! spectrapro_300i_comm( SERIAL_READ, lbuf, &len ) )
	{
		T_free( lbuf );
		spectrapro_300i_comm_fail( );
	}

	T_free( lbuf );

	/* When the command just send has been executed " ok\r\n" gets returned.
	   This may take quite some time when e.g. the grating has to be rotated,
	   so we retry quite a lot of times. */

	while ( repeats-- > 0 )
	{
		len = 5;
		if ( spectrapro_300i_comm( SERIAL_READ, reply, &len ) )
			break;
		fsc2_usleep( SPECTRAPRO_300I_WAIT, SET );
		stop_on_user_request( );
	}

	if ( len < 5 || strncmp( reply, " ok\r\n", 5 ) )
		spectrapro_300i_comm_fail( );
}


/*---------------------------------------------------------------*/
/* Function sends a command and returns a buffer (with a maximum */
/* length of *len bytes) with the reply of the device.           */
/*---------------------------------------------------------------*/

static char *spectrapro_300i_talk( const char *buf, size_t len )
{
	char *lbuf;
	size_t comm_len;


	CLOBBER_PROTECT( lbuf );

	fsc2_assert( buf != NULL && *buf != '\0' && len != 0 );

	lbuf = get_string( "%s\r", buf );
	comm_len = strlen( lbuf );

	if ( ! spectrapro_300i_comm( SERIAL_WRITE, lbuf ) )
	{
		T_free( lbuf );
		THROW( EXCEPTION );
	}

	/* The device always echos the command, we got to get rid of the echo */

	if ( ! spectrapro_300i_comm( SERIAL_READ, lbuf, &comm_len ) )
	{
		T_free( lbuf );
		spectrapro_300i_comm_fail( );
	}

	T_free( lbuf );

	/* Now we read the reply by the device, which is followed by " ok\r\n". */

	lbuf = T_malloc( len + 6 );
	len += 5;

	TRY
	{
		if ( ! spectrapro_300i_comm( SERIAL_READ, lbuf, &len ) )
			THROW( EXCEPTION );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( lbuf );
		RETHROW( );
	}

	/* Cut off the " ok\r\n" stuff and return the buffer with the reply */

	lbuf[ len ] = '\0';
	if ( strncmp( lbuf + len - 5, " ok\r\n", 5 ) )
	{
		T_free( lbuf );
		THROW( EXCEPTION );
	}

	len -= 5;
	lbuf[ len ] = '\0';

	return T_realloc( lbuf, len + 1 );
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static bool spectrapro_300i_comm( int type, ... )
{
	va_list ap;
	char *buf;
	ssize_t len;
	size_t *lptr;
	long read_retries = 10;            /* number of times we try to read */


	CLOBBER_PROTECT( buf );
	CLOBBER_PROTECT( len );
	CLOBBER_PROTECT( read_retries );

	switch ( type )
	{
		case SERIAL_INIT :               /* open and initialize serial port */
			/* We need exclussive access to the serial port and we also need
			   non-blocking mode to avoid hanging indefinitely if the other
			   side does not react. O_NOCTTY is set because the serial port
			   should not become the controlling terminal, otherwise line
			   noise read as a CTRL-C might kill the program. */

			if ( ( spectrapro_300i.tio = fsc2_serial_open( SERIAL_PORT,
					    DEVICE_NAME,
						O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
				return FAIL;

			/* Switch off parity checking (8N1) and use of 2 stop bits and
			   clear character size mask, set character size mask to CS8 and
			   the flag for ignoring modem lines, enable reading and, finally,
			   set the baud rate. */

			spectrapro_300i.tio->c_cflag &= ~ ( PARENB | CSTOPB | CSIZE );
			spectrapro_300i.tio->c_cflag |= CS8 | CLOCAL | CREAD;
			cfsetispeed( spectrapro_300i.tio, SERIAL_BAUDRATE );
			cfsetospeed( spectrapro_300i.tio, SERIAL_BAUDRATE );

			fsc2_tcflush( SERIAL_PORT, TCIFLUSH );
			fsc2_tcsetattr( SERIAL_PORT, TCSANOW, spectrapro_300i.tio );
			break;

		case SERIAL_EXIT :                    /* reset and close serial port */
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
			lptr = va_arg( ap, size_t * );
			va_end( ap );

			/* The monochromator might not be ready yet to send data, in
			   this case we retry it a few times before giving up */

			len = 1;
			do
			{
				TRY
				{
					if ( len < 0 )
						fsc2_usleep( SPECTRAPRO_300I_WAIT, SET );
					stop_on_user_request( );
					TRY_SUCCESS;
				}
				OTHERWISE
					RETHROW( );
				len = fsc2_serial_read( SERIAL_PORT, buf, *lptr );
			} while ( len < 0 && errno == EAGAIN && read_retries-- > 0 );

			if ( len < 0 )
			{
				*lptr = 0;
				return FAIL;
			}
			else
				*lptr = len;

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

static void spectrapro_300i_comm_fail( void )
{
	print( FATAL, "Can't access the monochromator.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */

