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


#include "spectrapro_300i.h"


#define SPECTRAPRO_300I_WAIT  100000   /* 100 ms */


enum {
	   SERIAL_INIT,
	   SERIAL_EXIT,
	   SERIAL_READ,
	   SERIAL_WRITE
};


static bool spectrapro_300i_comm( int type, ... );
static void spectrapro_300i_comm_fail( void );


/*-----------------------------------------------------------------------*/
/* If the function succeeds it returns a file pointer to the calibration */
/* file. Otherwise an exception is thrown.                               */
/*-----------------------------------------------------------------------*/

FILE *spectrapro_300i_find_calib( char *name )
{
	FILE *cfp = NULL;
	char *new_name = NULL;


	CLOBBER_PROTECT( new_name );

	/* Expand a leading tilde to the users home directory */

	if ( name[ 0 ] == '~' )
		new_name = get_string( "%s%s%s", getenv( "HOME" ),
							   name[ 1 ] != '/' ? "/" : "", name + 1 );

	/* Now try to open the file and return the file pointer */

	TRY
	{
		cfp = spectrapro_300i_open_calib( new_name != NULL ?
										  new_name : name );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( new_name != NULL )
			T_free( new_name );
		RETHROW( );
	}

	if ( new_name != NULL )
		T_free( new_name );

	if ( cfp )
		return cfp;

	/* If the file name contains a slash we give up after freeing memory */

	if ( strchr( name, '/' ) != NULL )
	{
		print( FATAL, "Calibration file '%s' not found.\n", new_name );
		THROW( EXCEPTION );
	}

	/* Last chance: The calibration file is in the library directory... */

	new_name = get_string( "%s%s%s", libdir, slash( libdir ), name );

	TRY
	{
		cfp = spectrapro_300i_open_calib( new_name );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( new_name );
		RETHROW( );
	}

	if ( cfp == NULL )
	{
		print( FATAL, "Calibration file '%s' not found in either the current "
			   "directory or in '%s'.\n", strip_path( new_name ), libdir );
		T_free( new_name );
		THROW( EXCEPTION );
	}

	T_free( new_name );

	return cfp;
}


/*------------------------------------------------------------------*/
/* Tries to open the file with the given name and returns the file  */
/* pointer or NULL if file does not exist returns, or throws an     */
/* exception if the file can't be read (either because of problems  */
/*  with the permissions or other, unknown reasons).                */
/*------------------------------------------------------------------*/

FILE *spectrapro_300i_open_calib( char *name )
{
	FILE *cfp;


	if ( access( name, R_OK ) == -1 )
	{
		if ( errno == ENOENT )       /* file not found */
			return NULL;

		print( FATAL, "No read permission for calibration file '%s'.\n",
			   name );
		T_free( name );
		THROW( EXCEPTION );
	}

	if ( ( cfp = fopen( name, "r" ) ) == NULL )
	{
		print( FATAL, "Can't open calibration file '%s'.\n", name );
		T_free( name );
		THROW( EXCEPTION );
	}

	return cfp;
}


/*-----------------------------------------------------------------------*/
/* Function for calculation of the sum of the squares of the differences */
/* between the measured offsets of the lines from he center (in pixels)  */
/* and the calculated offsets. The formulas used are (in TeX notation):  */
/* $n$: deviation in numbers of pixels                                   */
/* $x$: width of a single pixel                                          */
/* $m$: diffraction order                                                */
/* $\psi$: grating angle, rotation angle of the grating                  */
/* $\gamma$: inclusion angle, angle between incoming and outgoing beam   */
/* $f$: focal length of monochromator                                    */
/* $\delta$: detector angle, deviation of angle between center beam and  */ 
/* perpendicular on the CCD plane                                        */
/* $\theta$: incident angle of beam on detector                          */ 
/* $\lambda$: wavelength of measured line                                */
/* $\lambda_C$: wavelength at the center of the detector                 */ 
/* \begin{eqnarray*}                                                     */
/* \tan \theta & = & \frac{ nx \, \cos \delta}{f + nx \, \sin \delta} \\ */
/* \sin \psi & = & \frac{m \lambda_C}{2 d \, \cos \gamma / 2}         \\ */
/* \lambda & = & \frac{d}{m} \, \{ \sin ( \psi - \frac{\gamma}{2} )      */	
/*               + \sin ( \psi - \frac{\gamma}{2} + \theta ) \}          */
/* \end{eqnarray*}                                                       */
/*-----------------------------------------------------------------------*/

double spectrapro_300i_min( double *x, void *par )
{
	double inclusion_angle_2;                 /* half of inclusion angle */
	double focal_length;
	double detector_angle;
	CALIB_PARAMS *c;
	double grating_angle;
	double sum = 0.0;
	double a, b;
	size_t i;


	c = ( CALIB_PARAMS * ) par;

	if ( c->opt == 0 )
	{
		inclusion_angle_2 = 0.5 * x[ 0 ];
		if ( inclusion_angle_2 <= 0.0 )
			return HUGE_VAL;

		focal_length = c->focal_length;
		detector_angle = c->detector_angle;
	}
	else if ( c->opt == 1 )
	{
		focal_length = x[ 0 ];
		if ( focal_length <= 0.0 )
			return HUGE_VAL;

		inclusion_angle_2 = 0.5 * c->inclusion_angle;
		detector_angle    = c->detector_angle;
	}
	else if ( c->opt == 2 )
	{
		detector_angle    = x[ 0 ];

		inclusion_angle_2 = 0.5 * c->inclusion_angle;
		focal_length      = c->focal_length;
	}
	else
		THROW( EXCEPTION );

	/* For each measured value (that's the deviation of the line position
	   from the center, expressed in pixels) calculate the theoretical value
	   with the values for the inclusion angle, the focal length and the
	   detector angle we got from the simplex routine. Then sum up the squares
	   of the deviations from the measured values, which is what we want to
	   minimize. */

	for ( i = 0; i < c->num_values; i++ )
	{
		a = c->m[ i ] * c->center_wavelengths[ i ]
			/ ( 2.0 * c->d * cos( inclusion_angle_2 ) );

		if ( fabs( a ) > 1.0 )
		{
			sum = HUGE_VAL;
			break;
		}

		grating_angle = asin( a );

		a = c->m[ i ] * c->wavelengths[ i ] / c->d
			- sin( grating_angle - inclusion_angle_2 );

		if ( fabs( a ) > 1.0 )
		{
			sum = HUGE_VAL;
			break;
		}

		a = tan( asin( a ) - grating_angle - inclusion_angle_2 );

		b = cos( detector_angle ) - a * sin( detector_angle );

		if ( b == 0.0 )
		{
			sum = HUGE_VAL;
			break;
		}

		b = c->n_exp[ i ] - focal_length * a / ( b * c->pixel_width );

		sum += b * b;
	}

	return sum;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spectrapro_300i_open( void )
{
	if ( ! spectrapro_300i_comm( SERIAL_INIT ) )
	{
		print( FATAL, "Can't contact the monochromator.\n" );
		THROW( EXCEPTION );
	}

	spectrapro_300i.is_open = SET;
	spectrapro_300i_get_gratings( );
	spectrapro_300i.current_grating = spectrapro_300i_get_grating( ) - 1;
	spectrapro_300i.turret = spectrapro_300i_get_turret( ) - 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spectrapro_300i_close( void )
{
	spectrapro_300i_comm( SERIAL_EXIT );
	spectrapro_300i.is_open = UNSET;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

long spectrapro_300i_get_turret( void )
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

void spectrapro_300i_set_turret( long turret )
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

long spectrapro_300i_get_grating( void )
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

void spectrapro_300i_set_grating( long grating )
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

void spectrapro_300i_get_gratings( void )
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

void spectrapro_300i_install_grating( char *part_no, long grating )
{
	char *buf;


	fsc2_assert( grating >= 1 && grating <= MAX_GRATINGS );

	buf = get_string( "%s %ld INSTALL", part_no, grating );

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

void spectrapro_300i_uninstall_grating( long grating )
{
	char *buf;


	fsc2_assert( grating >= 1 && grating <= MAX_GRATINGS );

	buf = get_string( "%ld UNINSTALL", grating );

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
/* Function for commands that just get send to the device */
/* and don't expect any replies.                          */
/*--------------------------------------------------------*/

void spectrapro_300i_send( const char *buf )
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
			spectrapro_300i_comm_fail( );
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

char *spectrapro_300i_talk( const char *buf, size_t len, long wait_cycles )
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
			spectrapro_300i_comm_fail( );
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
			spectrapro_300i_comm_fail( );
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
			   noise read as CTRL-C might kill the program. */

			if ( ( spectrapro_300i.tio = fsc2_serial_open( SERIAL_PORT,
					    DEVICE_NAME,
						O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
				return FAIL;

			/* Set transfer mode to 8 bit, no parity and 1 stop bit (8N1)
			   and ignore control lines, don't use flow control. */

			spectrapro_300i.tio->c_cflag  = 0;
			spectrapro_300i.tio->c_cflag  = CLOCAL | CREAD | CS8;
			spectrapro_300i.tio->c_iflags = IGNBRK;
			spectrapro_300i.tio->c_oflags = 0;
			spectrapro_300i.tio->c_lflags = 0;
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
