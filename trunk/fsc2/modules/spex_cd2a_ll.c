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


#include "spex_cd2a.h"


#define PARAMETER  0
#define COMMAND    1

#define STX   '\x02'
#define ETX   '\x03'
#define EOT   '\x04'
#define ACK   '\x06'
#define NAK   '\x15'
#define CAN   '\x18'


static void spex_cd2a_scan_type( void );
static void spex_cd2a_sweep_start( void );
static void spex_cd2a_sweep_start( void );
static void spex_cd2a_sweep_end( void );
static void spex_cd2a_sweep_step( void );
static void spex_cd2a_print( char *mess, int digits, double val );
static size_t spex_cd2a_write( int type, const char *mess );
static void spex_cd2a_read( void );
static void spex_cd2a_comm_fail( void );


static bool do_print_message = UNSET;

/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spex_cd2a_init( void )
{
	/* First check if the device is prepared to talk with us by sending a
	   command that's not doing anything relevant */

	do_print_message = UNSET;

	while ( 1 )
	{
		TRY
		{
			fsc2_tcflush( SERIAL_PORT, TCIOFLUSH );

			spex_cd2a_write( PARAMETER, "BI1.0" );
			spex_cd2a_read( );
			TRY_SUCCESS;
		}
		CATCH( EXCEPTION )
		{
			if ( 1 != show_choices( "Please press the \"REMOTE\" button of\n"
									"the console to allow computer control\n"
									"of the monochromator.",
									2, "Abort", "Done", NULL, 1 ) )
				continue;
			THROW( EXCEPTION);
			
		}
		OTHERWISE
			RETHROW( );

		break;
	}

	do_print_message = SET;

	if ( spex_cd2a.mode & WN_MODES )
		spex_cd2a_set_laser_line( );

	if ( spex_cd2a.is_init )
	{
		spex_cd2a_scan_type( );
		spex_cd2a_sweep_start( );
		spex_cd2a_sweep_end( );
		spex_cd2a_sweep_step( );
		spex_cd2a_write( PARAMETER, "NS1" );
		spex_cd2a_read( );

		spex_cd2a.is_wavelength = SET;
		spex_cd2a.wavelength = spex_cd2a.start;
		spex_cd2a_set_wavelength( );
	}
	else if ( spex_cd2a.is_wavelength )
		spex_cd2a_set_wavelength( );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spex_cd2a_set_wavelength( void )
{
	char mess[ 11 ] = "SE";


	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );

	if ( FSC2_MODE != EXPERIMENT )
		return;

	/* Set a position... */

	if ( spex_cd2a.mode & WN_MODES )
		spex_cd2a_print( mess + 2, 8,
						 spex_cd2a_wl2mwn( spex_cd2a.wavelength ) );
	else
	{
		if ( UNITS == NANOMETER )
			spex_cd2a_print( mess + 2, 8, 10e9 * spex_cd2a.wavelength );
		else
			spex_cd2a_print( mess + 2, 8, 10e10 * spex_cd2a.wavelength );
	}

	spex_cd2a_write( PARAMETER, mess );
	spex_cd2a_read( );

	/* ...and ask the monochromator to move to it */

	spex_cd2a_write( COMMAND, "P" );
	spex_cd2a_read( );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spex_cd2a_halt( void )
{
	if ( FSC2_MODE == EXPERIMENT )
	{
		spex_cd2a_write( COMMAND, "H" );
		spex_cd2a_read( );
	}

	spex_cd2a.in_scan = UNSET;
}


/*------------------------------------------*/
/* Function for starting a (triggered) scan */
/*------------------------------------------*/

void spex_cd2a_start_scan( void )
{
	fsc2_assert( spex_cd2a.is_init );

	/* If we're already scanning stop the current scan */

	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );

	if ( FSC2_MODE == EXPERIMENT )
	{
		spex_cd2a_write( COMMAND, "E" );
		spex_cd2a_read( );
	}

	spex_cd2a.in_scan = SET;
	spex_cd2a.wavelength = spex_cd2a.start;
}


/*--------------------------------------------------------*/
/* Function sends a trigger (only possible during a scan) */
/*--------------------------------------------------------*/

void spex_cd2a_trigger( void )
{
	fsc2_assert( spex_cd2a.in_scan );

	if ( FSC2_MODE == EXPERIMENT )
	{
		spex_cd2a_write( COMMAND, "T" );
		spex_cd2a_read( );
	}
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spex_cd2a_set_laser_line( void )
{
	char mess[ 11 ] = "LL0";


	if ( FSC2_MODE != EXPERIMENT )
		return;

	spex_cd2a_write( PARAMETER, mess );
	spex_cd2a_read( );

	if ( spex_cd2a.mode == WND )
	{
		spex_cd2a_print( mess + 2, 8, spex_cd2a.laser_wavenumber );
		spex_cd2a_write( PARAMETER, mess );
		spex_cd2a_read( );
	}
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spex_cd2a_set_shutter_limits( void )
{
	char mess[ 11 ] = "SL";


	if ( FSC2_MODE != EXPERIMENT )
		return;

	/* Set the lower shutter limit */

	spex_cd2a_print( mess + 2, 8, spex_cd2a.mode & WN_MODES ?
					 spex_cd2a_wl2mwn( spex_cd2a.lower_limit ) :
					 spex_cd2a.lower_limit );
	spex_cd2a_write( PARAMETER, mess );
	spex_cd2a_read( );

	/* Set the upper shutter limit */

	strcpy( mess, "SH" );
	spex_cd2a_print( mess + 2, 8, spex_cd2a.mode & WN_MODES ?
					 spex_cd2a_wl2mwn( spex_cd2a.upper_limit ) :
					 spex_cd2a.lower_limit );
	spex_cd2a_write( PARAMETER, mess );
	spex_cd2a_read( );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void spex_cd2a_scan_type( void )
{
	spex_cd2a_write( PARAMETER, "TYC" );
	spex_cd2a_read( );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void spex_cd2a_sweep_start( void )
{
	char mess[ 11 ] = "ST";


	if ( spex_cd2a.mode & WN_MODES )
		spex_cd2a_print( mess + 2, 8,
						 spex_cd2a_wl2mwn( spex_cd2a.start ) );
	else
	{
		if ( UNITS == NANOMETER )
			spex_cd2a_print( mess + 2, 8, 10e9 * spex_cd2a.start );
		else
			spex_cd2a_print( mess + 2, 8, 10e10 * spex_cd2a.start );
	}

	spex_cd2a_write( PARAMETER, mess );
	spex_cd2a_read( );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void spex_cd2a_sweep_end( void )
{
	char mess[ 11 ] = "EN";

	if ( spex_cd2a.mode & WN_MODES )
		spex_cd2a_print( mess + 2, 8,
						 spex_cd2a_wl2mwn( spex_cd2a.upper_limit ) );
	else
	{
		if ( UNITS == NANOMETER )
			spex_cd2a_print( mess + 2, 8, 10e9 * spex_cd2a.upper_limit );
		else
			spex_cd2a_print( mess + 2, 8, 10e10 * spex_cd2a.upper_limit );
	}

	spex_cd2a_write( PARAMETER, mess );
	spex_cd2a_read( );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void spex_cd2a_sweep_step( void )
{
	char mess[ 9 ] = "BI";


	spex_cd2a_print( mess + 2, 6, spex_cd2a.step );
	spex_cd2a_write( PARAMETER, mess );
	spex_cd2a_read( );
}	


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void spex_cd2a_print( char *mess, int digits, double val )
{
	int pre_digits, after_digits;


	fsc2_assert( digits > 0 );

	if ( log10( abs( fabs( val  ) ) ) < 0 )
	{
		pre_digits = 1;
		if ( val < 0.0 )
			pre_digits += 1;
		after_digits = digits - pre_digits - 1;
	}
	else
	{
		pre_digits = ( int ) floor( log10( fabs( val ) ) ) + 1;
		if ( pre_digits == 0 )
			pre_digits++;
		if ( val < 0.0 )
			pre_digits++;
		if ( pre_digits == digits )
			after_digits = 0;
		else
			after_digits = digits - pre_digits - 1;
		fsc2_assert( after_digits >= 0 );
	}

	if ( after_digits == 0 )
		sprintf( mess, "%*ld", digits, lrnd( val ) );
	else
	{
		val = lrnd( val * pow( 10, after_digits ) ) / pow( 10, after_digits );
		sprintf( mess, "%*.*f", digits, after_digits, val );
	}
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spex_cd2a_open( void )
{
	/* We need exclussive access to the serial port and we also need non-
	   blocking mode to avoid hanging indefinitely if the other side does not
	   react. O_NOCTTY is set because the serial port should not become the
	   controlling terminal, otherwise line noise read as a CTRL-C might kill
	   the program. */

	if ( ( spex_cd2a.tio = fsc2_serial_open( SERIAL_PORT, DEVICE_NAME,
						  O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
	{
		print( FATAL, "Can't open device file for monochromator.\n" );
		THROW( EXCEPTION );
	}

	spex_cd2a.tio->c_cflag = 0;

	switch ( PARITY_TYPE )
	{
		case NO_PARITY :
			break;

		case ODD_PARITY :
			spex_cd2a.tio->c_cflag |= PARENB | PARODD;
			break;

		case EVEN_PARITY :
			spex_cd2a.tio->c_cflag |= PARENB;
			break;

		default :
			fsc2_serial_close( SERIAL_PORT );
			print( FATAL, "Invalid setting for parity bit in "
				   "configuration file for the device.\n" );
			THROW( EXCEPTION );
	}

	switch ( NUMBER_OF_STOP_BITS )
	{
		case 1 :
			break;

		case 2 :
			spex_cd2a.tio->c_cflag |= CSTOPB;
			break;

		default :
			fsc2_serial_close( SERIAL_PORT );
			print( FATAL, "Invalid setting for number of stop bits in "
				   "configuration file for the device.\n" );
			THROW( EXCEPTION );
	}

	switch ( NUMBER_OF_BITS_PER_CHARACTER )
	{
		case 5 :
			spex_cd2a.tio->c_cflag |= CS5;
			break;

		case 6 :
			spex_cd2a.tio->c_cflag |= CS6;
			break;

		case 7 :
			spex_cd2a.tio->c_cflag |= CS7;
			break;

		case 8 :
			spex_cd2a.tio->c_cflag |= CS8;
			break;

		default :
			fsc2_serial_close( SERIAL_PORT );
			print( FATAL, "Invalid setting for number of bits per "
				   "in character configuration file for the device.\n" );
			THROW( EXCEPTION );
	}

	spex_cd2a.tio->c_cflag |= CLOCAL | CREAD;
	spex_cd2a.tio->c_iflag = IGNBRK;
	spex_cd2a.tio->c_oflag = 0;
	spex_cd2a.tio->c_lflag = 0;

	cfsetispeed( spex_cd2a.tio, SERIAL_BAUDRATE );
	cfsetospeed( spex_cd2a.tio, SERIAL_BAUDRATE );

	/* We can't use canonical mode here... */

	spex_cd2a.tio->c_lflag = 0;

	fsc2_tcflush( SERIAL_PORT, TCIOFLUSH );
	fsc2_tcsetattr( SERIAL_PORT, TCSANOW, spex_cd2a.tio );

	spex_cd2a.is_open = SET;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static size_t spex_cd2a_write( int type, const char *mess )
{
	unsigned char *tm;
	unsigned long cs = 0;
	size_t len;
	size_t i;
	ssize_t written;


	printf( "%s\n", mess );

	len = strlen( mess );
	tm = T_malloc( len + 6 );

	tm[ 0 ] = type == PARAMETER ? STX : CAN;
	len++;

	strcpy( tm + 1, mess );
	tm[ len++ ] = ETX;

	if ( spex_cd2a.use_checksum )
	{
		for ( i = 0; i < len; i++ )
			cs += tm[ i ];
		sprintf( tm + len, "%02lx", cs & 0xFF );
		len += 2;
	}

	tm[ len++ ] = '\r';
	tm[ len++ ] = '\0';

	if ( ( written = fsc2_serial_write( SERIAL_PORT, tm, len, 0, UNSET ) )
		 <= 0 )
	{
		T_free( tm );
		spex_cd2a_comm_fail( );
	}

	T_free( tm );

	return ( size_t ) written;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void spex_cd2a_read( void )
{
	char buf[ 7 ];
	ssize_t r = 0;
	ssize_t i;


	while ( r < 2 )
		if ( ( r += fsc2_serial_read( SERIAL_PORT, buf + r,
									  7 - r, 500000, SET ) ) <= 0 )
			spex_cd2a_comm_fail( );

	printf( "%ld: ", ( long ) r );
	for ( i = 0; i < r; i++ )
		printf( " 0x%x", buf[ i ] );
	printf( "\n" );

	if ( buf[ 0 ] == ACK && buf[ 1 ] == CAN )
		return;

	if ( buf[ 0 ] == NAK )
	{
		print( FATAL, "Communication problem with device.\n" );
		THROW( EXCEPTION );
	}

	if ( buf[ 0 ] == '\a' || buf[ 1 ] == '\a' )
	{
		if ( buf[ 0 ] == '\a' )
			buf[ 3 ] = '\0';
		else
			buf[ 4 ] = '\0';
		print( FATAL, "Failure to execute command, error code: %s.\n",
			   buf[ 0 ] == '\a' ? buf + 1 : buf + 2 );
		THROW( EXCEPTION );
	}

	print( FATAL, "Device send unexpected data.\n" );
	THROW( EXCEPTION );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spex_cd2a_close( void )
{
	if ( spex_cd2a.is_open )
		fsc2_serial_close( SERIAL_PORT );
	spex_cd2a.is_open = UNSET;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void spex_cd2a_comm_fail( void )
{
	if ( do_print_message )
		print( FATAL, "Can't access the monochromator.\n" );
	THROW( EXCEPTION );
}


/*-----------------------------------------*/
/* Converts a wavenumber into a wavelength */
/*-----------------------------------------*/

double spex_cd2a_wn2wl( double wn )
{
	fsc2_assert( wn > 0 );
	return 0.01 / wn;
}


/*-----------------------------------------*/
/* Converts a wavelength into a wavenumber */
/*-----------------------------------------*/

double spex_cd2a_wl2wn( double wl )
{
	fsc2_assert( wl > 0 );
	return 0.01 / wl;
}


/*-----------------------------------------------------------*/
/* Converts a wavelength into either an absolute or relative */
/* wavenumber, depending on a laser line having been set.    */
/*-----------------------------------------------------------*/

double spex_cd2a_wl2mwn( double wl )
{
	double wn;


	fsc2_assert( wl > 0 );
	wn = 0.01 / wl;
	if ( spex_cd2a.mode == WND )
		wn = spex_cd2a.laser_wavenumber - wn;
	return wn;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
