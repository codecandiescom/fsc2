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
Var *monochromator_grating( Var *v );
Var *monochromator_wavelength( Var *v );
Var *monochromator_install_grating( Var *v );
static bool spectrapro_300i_open( void );
static bool spectrapro_300i_close( void );
static long spectrapro_300i_get_turret( void );
static void spectrapro_300i_set_turret( long turret );
static long spectrapro_300i_get_grating( void );
static void spectrapro_300i_set_grating( long grating );
static void spectrapro_300i_get_gratings( void );
static void spectrapro_300i_install_grating( char *part_no, long grating );
static void spectrapro_300i_send( const char *buf );
static char *spectrapro_300i_talk( const char *buf, size_t len,
								   long wait_cycles );
static bool spectrapro_300i_comm( int type, ... );
static void spectrapro_300i_comm_fail( void );


typedef struct SPECTRAPRO_300I SPECTRAPRO_300I;

struct SPECTRAPRO_300I {
	bool is_needed;         /* is the gaussmter needed at all? */
    struct termios *tio;    /* serial port terminal interface structure */
	double wavelength;
	long turret;            /* current turret, range 0-2 */
	long max_gratings;      /* current grating, range 0-2 */
	long current_grating;
	struct {
		bool is_installed;
		long grooves;       /* number of grooes per m */
		double blaze;       /* blaze wavelength (negative if not appicable) */
	} grating[ MAX_GRATINGS ];
};


SPECTRAPRO_300I spectrapro_300i;


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
	int i;


	/* Claim the serial port (throws exception on failure) */

	fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	spectrapro_300i.is_needed = SET;

	for ( i = 0; i < MAX_GRATINGS; i++ )
		spectrapro_300i.grating[ i ].is_installed = UNSET;
	spectrapro_300i.turret = 0;
	spectrapro_300i.current_grating = 0;

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

	spectrapro_300i_get_gratings( );
	spectrapro_300i.current_grating = spectrapro_300i_get_grating( ) - 1;
	spectrapro_300i.turret = spectrapro_300i_get_turret( ) - 1;

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


/*-----------------------------------------------------*/
/* Function for setting or quering the current grating */
/*-----------------------------------------------------*/

Var *monochromator_grating( Var *v )
{
	long grating;


	if ( v == 0 )
		return vars_push( INT_VAR, spectrapro_300i.current_grating + 1 );

	grating = get_strict_long( v, "grating number" );

	if ( grating < 1 || grating > MAX_GRATINGS )
	{
		if ( FSC2_MODE == TEST )
		{
			print( FATAL, "Invalid grating number, valid range is 1 to %ld.\n",
				   MAX_GRATINGS );
			THROW( EXCEPTION );
		}

		print( SEVERE,  "Invalid grating number, keeping grating #%ld\n",
			   spectrapro_300i.current_grating + 1 );
		return vars_push( INT_VAR, spectrapro_300i.current_grating + 1 );
	}

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ! spectrapro_300i.grating[ grating - 1 ].is_installed )
		{
			print( SEVERE,  "No grating #%ld is installed, keeping grating "
				   "#%ld\n", grating, spectrapro_300i.current_grating + 1 );
			return vars_push( INT_VAR, spectrapro_300i.current_grating + 1 );
		}

		if ( grating - spectrapro_300i.turret * 3 < 1 ||
			 grating - spectrapro_300i.turret * 3 > 3 )
		{
			print( FATAL, "Can't switch to grating #ld while turret #ld is "
				   "in use.\n", grating, spectrapro_300i.turret + 1 );
			THROW( EXCEPTION );
		}
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
	{
		spectrapro_300i_set_grating( grating );
		spectrapro_300i.current_grating = grating - 1;
	}

	return vars_push( INT_VAR, grating );
}


/*----------------------------------------------------*/
/* Function for setting or quering the current turret */
/*----------------------------------------------------*/

#if 0
Var *monochromator_turret( Var *v )
{
	long turret;


	if ( v == 0 )
		return vars_push( INT_VAR, spectrapro_300i.turret + 1 );

	turret = get_strict_long( v, "turret number" );

	if ( turret < 1 || grating > MAX_TURRETS )
	{
		if ( FSC2_MODE == TEST )
		{
			print( FATAL, "Invalid turret number, valid range is 1 to %ld.\n",
				   MAX_TURRETS );
			THROW( EXCEPTION );
		}

		print( SEVERE,  "Invalid turret number, keeping turret #%ld\n",
			   spectrapro_300i.turret + 1 );
		return vars_push( INT_VAR, spectrapro_300i.turret + 1 );
	}

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ! sectrapro_300i.grating[ 3 * turret + i ].is_installed )

		for ( i = 0; i < 3; i++ )
			if ( sectrapro_300i.grating[ 3 * turret + i ].is_installed )
				break;
}
#endif

/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *monochromator_wavelength( Var *v )
{
	const char *reply;
	double wl;
	char *buf;


	CLOBBER_PROTECT( buf );
	CLOBBER_PROTECT( wl );

	if ( v == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			reply = spectrapro_300i_talk( "?NM", 100, 1 );
			spectrapro_300i.wavelength = T_atod( reply ) * 1.0e-9;
			T_free( ( void * ) reply );
		}

		return vars_push( FLOAT_VAR, spectrapro_300i.wavelength );
	}

	wl = get_double( v, "wavelength" );

	if ( wl < 0.0 )
	{
		if ( FSC2_MODE == TEST )
		{
			print( FATAL, "Invalid negative wavelength.\n" );
			THROW( EXCEPTION );
		}

		print( SEVERE, "Invalid negative wavelength, using 0 nm instead.\n" );
		wl = 0.0;
	}

	if ( wl > MAX_WAVELENGTH + 1.0e-12 )
	{
		if ( FSC2_MODE == TEST )
		{
			print( FATAL, "Wavelength of %.3f nm is too large, maximum "
				   "wavelength is %.3f nm.\n",
				   wl * 1.0e9, MAX_WAVELENGTH * 1.0e9 );
			THROW( EXCEPTION );
		}

		print( SEVERE, "Wavelength of %.3f nm is too large, using %.3f nm "
			   "instead.\n", wl * 1.0e9, MAX_WAVELENGTH * 1.0e9 );
		wl = MAX_WAVELENGTH;
	}

	if ( wl > MAX_WAVELENGTH )
		wl = MAX_WAVELENGTH;

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
	{
		buf = get_string( "%.3f GOTO", 1.0e9 * wl );

		TRY
		{
			spectrapro_300i_send( buf );
			T_free( buf );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( buf );
			RETHROW( );
		}
	}

	spectrapro_300i.wavelength = wl;

	return vars_push( FLOAT_VAR, spectrapro_300i.wavelength );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *monochromator_install_grating( Var *v )
{
	long grating;


	if ( v->type != STR_VAR )
	{
		print( FATAL, "First varianbe must be a string with the part "
			   "number, e.g. 1-120-500.\n" );
		THROW( EXCEPTION );
	}

	/* Do some minimal checks on the part number */

	if ( strncmp( v->val.sptr, "1-", 2 ) ||
		 ! isdigit( v->val.sptr[ 2 ] ) ||
		 ! isdigit( v->val.sptr[ 3 ] ) ||
		 ! isdigit( v->val.sptr[ 4 ] ) ||
		 ! v->val.sptr[ 5 ] != '-' || 
		 strlen( v->val.sptr ) > 10 )
	{
		print( FATAL, "First argument doesn't look like a valid part "
			   "number.\n" );
		THROW( EXCEPTION );
	}

	grating = get_strict_long( v->next, "grating position" );

	if ( grating < 1 && grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating position, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == EXPERIMENT )
		spectrapro_300i_install_grating( v->val.sptr, grating );

	return vars_push( INT_VAR, 1 );
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
/*--------------------------------------------------------*/

static long spectrapro_300i_get_turret( void )
{
	const char *reply;
	long turret;


	reply = spectrapro_300i_talk( "?TURRET", 20, 1 );
	turret = T_atol( reply );
	T_free( ( void * ) reply );
	return turret;
}

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static void spectrapro_300i_set_turret( long turret )
{
	char *buf;


	CLOBBER_PROTECT( buf );

	fsc2_assert( turret >= 1 && turret <= MAX_TURRETS );

	buf = get_string( "%ld TURRET", turret );

	TRY
	{
		spectrapro_300i_send( buf );
		T_free( buf );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( buf );
		RETHROW( );
	}

}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static long spectrapro_300i_get_grating( void )
{
	const char *reply;
	long grating;


	reply = spectrapro_300i_talk( "?GRATING", 20, 1 );
	grating = T_atol( reply );
	T_free( ( void * ) reply );
	return grating;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static void spectrapro_300i_set_grating( long grating )
{
	char *buf;


	CLOBBER_PROTECT( buf );

	fsc2_assert( grating >= 1 && grating <= MAX_GRATINGS &&
				 grating - spectrapro_300i.turret * 3 >= 1 &&
				 grating - spectrapro_300i.turret * 3 <= 3 &&
				 spectrapro_300i.grating[ grating - 1 ].is_installed );

	buf = get_string( "%ld GRATING", grating );

	TRY
	{
		spectrapro_300i_send( buf );
		T_free( buf );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( buf );
		RETHROW( );
	}
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static void spectrapro_300i_get_gratings( void )
{
	const char *reply;
	const char *sp;
	long gn, gr, bl;
	int i;


	reply = spectrapro_300i_talk( "?GRATINGS", 80 * MAX_GRATINGS, 5 );

	for ( sp = reply, i = 0; i < MAX_GRATINGS; i++ )
	{
		while ( *sp != '\0' && ! isdigit( *sp ) )
			sp++;

		if ( *sp == '\0' )
		{
			T_free( ( void * ) reply );
			spectrapro_300i_comm_fail( );
		}

		gn = 0;
		while( *sp != '\0' && isdigit( *sp ) )
			gn = gn * 10 + ( long ) ( *sp++ - '0' );

		if ( *sp == '\0' || gn - 1 != i )
		{
			T_free( ( void * ) reply );
			spectrapro_300i_comm_fail( );
		}

		while ( *sp != '\0' && isspace( *sp ) )
			sp++;

		if ( *sp == '\0' )
		{
			T_free( ( void * ) reply );
			spectrapro_300i_comm_fail( );
		}

		if ( ! strncmp( sp, "Not Installed", 13 ) )
		{
			sp += 13;
			continue;
		}

		if ( ! isdigit( *sp ) )
		{
			T_free( ( void * ) reply );
			spectrapro_300i_comm_fail( );
		}

		gr = 0;
		while( *sp != '\0' && isdigit( *sp ) )
			gr = gr * 10 + ( long ) ( *sp++ - '0' );

		if ( *sp == '\0' )
		{
			T_free( ( void * ) reply );
			spectrapro_300i_comm_fail( );
		}

		spectrapro_300i.grating[ i ].grooves = gr * 1000;

		while ( *sp != '\0' && isspace( *sp ) )
			sp++;

		if ( *sp == '\0' )
		{
			T_free( ( void * ) reply );
			spectrapro_300i_comm_fail( );
		}

		if ( strncmp( sp, "g/mm BLZ=", 9 ) )
		{
			T_free( ( void * ) reply );
			spectrapro_300i_comm_fail( );
		}

		sp += 9;

		while ( *sp != '\0' && isspace( *sp ) )
			sp++;

		if ( *sp == '\0' )
		{
			T_free( ( void * ) reply );
			spectrapro_300i_comm_fail( );
		}

		if ( ! isdigit( *sp ) )
		{
			spectrapro_300i.grating[ i ].blaze = -1;
			while ( *sp != '\0' && isalpha( *sp ) )
				sp++;
			if ( *sp == '\0' )
			{
				T_free( ( void * ) reply );
				spectrapro_300i_comm_fail( );
			}
		}
		else
		{
			bl = 0;
			while( *sp != '\0' && isdigit( *sp ) )
				bl = bl * 10 + ( long ) ( *sp++ - '0' );

			if ( *sp == '\0' )
			{
				T_free( ( void * ) reply );
				spectrapro_300i_comm_fail( );
			}

			spectrapro_300i.grating[ i ].blaze = bl * 1.0e-9;

			while ( *sp != '\0' && isspace( *sp ) )
				sp++;

			if ( *sp == '\0' )
			{
				T_free( ( void * ) reply );
				spectrapro_300i_comm_fail( );
			}

			if ( *sp == '\0' )
			{
				T_free( ( void * ) reply );
				spectrapro_300i_comm_fail( );
			}

			if ( strncmp( sp, "nm", 2 ) )
			{
				T_free( ( void * ) reply );
				spectrapro_300i_comm_fail( );
			}

			sp += 2;
		}

		spectrapro_300i.grating[ i ].is_installed = SET;
	}

	T_free( ( void * ) reply );
}
	

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static void spectrapro_300i_install_grating( char *part_no, long grating )
{
	char *buf;


	fsc2_assert( grating >= 1 && grating <= MAX_GRATINGS );

	buf = get_string( "%s %ld INSTALL", part_no, grating );

	TRY
	{
		spectrapro_300i_send( buf );
		T_free( bif );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( buf );
		RETHROW( );
	}
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


	CLOBBER_PROTECT( repeats );

	fsc2_assert( buf != NULL && *buf != '\0' );

	lbuf = get_string( "%s\r", buf );
	len = strlen( lbuf );

	if ( ! spectrapro_300i_comm( SERIAL_WRITE, lbuf ) )
	{
		T_free( lbuf );
		spectrapro_300i_comm_fail( );
	}

	/* The device always echos the command, we got to get rid of the echo */

	TRY
	{
		if ( ! spectrapro_300i_comm( SERIAL_READ, lbuf, &len ) )
		{
			T_free( lbuf );
			spectrapro_300i_comm_fail( );
		}
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( lbuf );
		RETHROW( );
	}

	T_free( lbuf );

	/* When the command just send has been executed " ok\r\n" gets returned */

	while ( repeats-- >= 0 )
	{
		len = 5;
		if ( spectrapro_300i_comm( SERIAL_READ, reply, &len ) )
			break;
		fsc2_usleep( SPECTRAPRO_300I_WAIT, UNSET );
		stop_on_user_request( );
	}

	if ( repeats < 0 || len != 5 || strncmp( reply, " ok\r\n", 5 ) )
		spectrapro_300i_comm_fail( );
}


/*---------------------------------------------------------------*/
/* Function sends a command and returns a buffer (with a maximum */
/* length of *len bytes) with the reply of the device.           */
/*---------------------------------------------------------------*/

static char *spectrapro_300i_talk( const char *buf, size_t len,
								   long wait_cycles )
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
		spectrapro_300i_comm_fail( );
	}

	/* The device always echos the command, we got to get rid of the echo */

	TRY
	{
		if ( ! spectrapro_300i_comm( SERIAL_READ, lbuf, &comm_len ) )
		{
			T_free( lbuf );
			spectrapro_300i_comm_fail( );
		}
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( lbuf );
		RETHROW( );
	}

	T_free( lbuf );

	/* Now we read the reply by the device, which is followed by " ok\r\n". */

	fsc2_usleep( wait_cycles * SPECTRAPRO_300I_WAIT, SET );

	lbuf = T_malloc( len + 6 );
	len += 5;

	TRY
	{
		if ( ! spectrapro_300i_comm( SERIAL_READ, lbuf, &len ) )
		{
			T_free( lbuf );
			spectrapro_300i_comm_fail( );
		}
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
		spectrapro_300i_comm_fail( );
	}

	len -= 5;
	lbuf[ len ] = '\0';

	TRY
	{
		T_realloc( lbuf, len + 1 );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( lbuf );
		RETHROW( );
	}

	return lbuf;
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static bool spectrapro_300i_comm( int type, ... )
{
	va_list ap;
	char *buf;
	ssize_t len;
	size_t *lptr;
	long read_retries = 20;            /* number of times we try to read */


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
				if ( len < 0 )
				{
					fsc2_usleep( SPECTRAPRO_300I_WAIT, SET );
					stop_on_user_request( );
				}

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

