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


static void spex_cd2a_scan_end( void );
static void spex_cd2a_print( char *mess, int digits, double val );
static size_t spex_cd2a_write( int type, const char *mess );
static void spex_cd2a_read_ack( void );
static void spex_cd2a_read_cmd_ack( const char *cmd );
static void spex_cd2a_read_set_pos_ack( void );
static void spex_cd2a_read_start_scan_ack( void );
static void spex_cd2a_read_scan_ack( void );
static void spex_cd2a_comm_fail( void );
static void spex_cd2a_wrong_data( void );
static ssize_t spex_cd2a_pos_mess_len( void );
static void spex_cd2a_pos_mess_check( const char *bp );


static ssize_t pos_mess_len;
static bool do_print_message = UNSET;


/*--------------------------------------------------------------*/
/* Function for initializing the monochromator to bring it into */
/* a well-defined state.                                        */
/*--------------------------------------------------------------*/

void spex_cd2a_init( void )
{
	/* First calculate the length of position length messages, so we don't
	   have to recalculate it all of the time. */

	pos_mess_len = spex_cd2a_pos_mess_len( );

	/* Now check if the device is prepared to talk with us by sending it the
	   command to use burst scans - that's the only type of scans supported
	   anyway (with bursts induced by software triggers). */

	do_print_message = UNSET;

	while ( 1 )
	{
		TRY
		{
			fsc2_tcflush( SERIAL_PORT, TCIOFLUSH );

			spex_cd2a_write( PARAMETER, "TYB" );
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

	/* Set the laser line position if the monochromator is wave-number driven
	   (if not set the laser line gets switched off automatically to make
	   the device accept wave-numbers in absolute units). */

	if ( spex_cd2a.mode & WN_MODES )
		spex_cd2a_set_laser_line( );

	/* Set the end of scans to the upper wavelength or lower wavenumber limit
	   and the number of scan repetitions to 1. */

	spex_cd2a_scan_end( );
	spex_cd2a_write( PARAMETER, "NS1" );

	/* If a wavelength was set in the PREPARATIONS section set it now. */

	if ( spex_cd2a.is_wavelength )
		spex_cd2a_set_wavelength( );

	/* If there was a scan setup send the values to the device now and, if
	   no wave-length or -number was set, set it to the value of the start
	   position of the scan. */

	if ( spex_cd2a.scan_is_init )
	{
		spex_cd2a_scan_start( );
		spex_cd2a_scan_step( );

		if ( spex_cd2a.is_wavelength )
		{
			spex_cd2a.wavelength = spex_cd2a.scan_start;
			spex_cd2a_set_wavelength( );
			spex_cd2a.is_wavelength = SET;
		}
	}
}


/*---------------------------------------------------*/
/* Function for setting a new wave-length or -number */
/*---------------------------------------------------*/

void spex_cd2a_set_wavelength( void )
{
	char mess[ 11 ] = "SE";


	/* If we're in scan mode we need to stop the currently running scan
	   before we can set a new position */

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

	/* ...and ask the monochromator to move to it */

	spex_cd2a_write( COMMAND, "P" );
}


/*----------------------------*/
/* Function for ending a scan */
/*----------------------------*/

void spex_cd2a_halt( void )
{
	if ( FSC2_MODE == EXPERIMENT )
		spex_cd2a_write( COMMAND, "H" );
	spex_cd2a.in_scan = UNSET;
}


/*-----------------------------------------------*/
/* Function for starting a (triggered burs) scan */
/*-----------------------------------------------*/

void spex_cd2a_start_scan( void )
{
	fsc2_assert( spex_cd2a.scan_is_init );

	/* If we're already scanning stop the current scan */

	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );

	if ( FSC2_MODE == EXPERIMENT )
		spex_cd2a_write( COMMAND, "T" );

	spex_cd2a.in_scan = SET;
	spex_cd2a.wavelength = spex_cd2a.scan_start;
}


/*--------------------------------------------------------*/
/* Function sends a trigger (only possible during a scan) */
/*--------------------------------------------------------*/

void spex_cd2a_trigger( void )
{
	fsc2_assert( spex_cd2a.in_scan );

	if ( FSC2_MODE == EXPERIMENT )
		spex_cd2a_write( COMMAND, "E" );
}


/*-------------------------------------------------------------------*/
/* Function for setting the position of the laser line. This is only */
/* possible when the monochromator is wave-number driven. It also    */
/* switches the way positions are treated by the device: if no laser */
/* line position is set positions are handled as absolute wave-      */
/* numbers. But when the laser line is set, positions are treated as */
/* relative to the laser line (i.e. position of the laser line minus */
/* the absolute position). The only execption is when setting the    */
/* laser line position itself: the value passed to the device is     */
/* always treated as a position in  absolute wave-numbers.           */
/* The laser line position can also be switched off (thus reverting  */
/* to absolute positions) by setting the laser line position to 0.   */
/*-------------------------------------------------------------------*/

void spex_cd2a_set_laser_line( void )
{
	char mess[ 11 ] = "LL0";


	if ( FSC2_MODE != EXPERIMENT )
		return;

	if ( spex_cd2a.mode == WN )
		spex_cd2a_write( PARAMETER, mess );
	else
	{
		spex_cd2a_print( mess + 2, 8, spex_cd2a.laser_wavenumber );
		spex_cd2a_write( PARAMETER, mess );
	}
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spex_cd2a_set_shutter_limits( void )
{
	char mess[ 11 ] = "SL";


	fsc2_assert( spex_cd2a.has_shutter );

	/* Halt a running scan */

	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );

	if ( FSC2_MODE != EXPERIMENT )
		return;

	/* Set the lower shutter limit */

	spex_cd2a_print( mess + 2, 8, spex_cd2a.mode & WN_MODES ?
					 spex_cd2a_wl2mwn( spex_cd2a.lower_limit ) :
					 spex_cd2a.lower_limit );
	spex_cd2a_write( PARAMETER, mess );

	/* Set the upper shutter limit */

	strcpy( mess, "SH" );
	spex_cd2a_print( mess + 2, 8, spex_cd2a.mode & WN_MODES ?
					 spex_cd2a_wl2mwn( spex_cd2a.upper_limit ) :
					 spex_cd2a.lower_limit );
	spex_cd2a_write( PARAMETER, mess );
}


/*--------------------------------------------------*/
/* Function for setting the start position of scans */
/*--------------------------------------------------*/

void spex_cd2a_scan_start( void )
{
	char mess[ 11 ] = "ST";


	/* If a scan is already running halt it first */

	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );

	if ( FSC2_MODE != EXPERIMENT )
		return;

	if ( spex_cd2a.mode & WN_MODES )
		spex_cd2a_print( mess + 2, 8,
						 spex_cd2a_wl2mwn( spex_cd2a.scan_start ) );
	else
	{
		if ( UNITS == NANOMETER )
			spex_cd2a_print( mess + 2, 8, 10e9 * spex_cd2a.scan_start );
		else
			spex_cd2a_print( mess + 2, 8, 10e10 * spex_cd2a.scan_start );
	}

	spex_cd2a_write( PARAMETER, mess );
}


/*---------------------------------------------------------------------*/
/* Function for sending the end point of scans to the upper wavelength */
/* and lower wavenumber limit.                                         */
/*---------------------------------------------------------------------*/

static void spex_cd2a_scan_end( void )
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
}


/*---------------------------------------------*/
/* Function for setting the step size in scans */
/*---------------------------------------------*/

void spex_cd2a_scan_step( void )
{
	char mess[ 9 ] = "BI";


	if ( FSC2_MODE != EXPERIMENT )
		return;

	spex_cd2a_print( mess + 2, 6, fabs( spex_cd2a.scan_step ) );
	spex_cd2a_write( PARAMETER, mess );
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

	/* The device don't seem to accept commands unless there's a short
	   delay since the last transmission... */

	if ( type == COMMAND )
		fsc2_usleep( 10000, UNSET );

	if ( ( written = fsc2_serial_write( SERIAL_PORT, tm, len, 0, UNSET ) )
		 <= 0 )
	{
		T_free( tm );
		spex_cd2a_comm_fail( );
	}

	T_free( tm );

	if ( type == PARAMETER )
		spex_cd2a_read_ack( );
	else
		spex_cd2a_read_cmd_ack( mess );

	return ( size_t ) written;
}


/*----------------------------------------------------------------------*/
/* Function for reading the acknowledgement send by the device when it  */
/* received a parameter or a command.                                   */ 
/*----------------------------------------------------------------------*/

static void spex_cd2a_read_ack( void )
{
	char buf[ 7 ];
	ssize_t r = 0;


	while ( r < 2 )
	{
		if ( ( r += fsc2_serial_read( SERIAL_PORT, buf + r,
									  7 - r, 1000000, SET ) ) <= 0 )
			spex_cd2a_comm_fail( );

		/* A message starting with a NAK means that there are some
		   communication problems */

		if ( r > 0 && *buf == NAK )
		{
			print( FATAL, "Communication problem with device.\n" );
			THROW( EXCEPTION );
		}
	}

	/* Device send an ACK and a CAN so everything is fine */

	if ( buf[ 0 ] == ACK && buf[ 1 ] == CAN )
		return;

	/* An ACK followed by BEL means something went wrong and we get an
	   error code in the next to bytes. */

	if ( buf[ 0 ] == ACK || buf[ 1 ] == '\a' )
	{
		buf[ 4 ] = '\0';
		print( FATAL, "Failure to execute command, error code: %s.\n",
			   buf + 2 );
		THROW( EXCEPTION );
	}

	/* If none of the above was received from the device things went really
	   wrong... */

	spex_cd2a_wrong_data( );
}


/*---------------------------------------------------------------------*/
/* Most commands not only send a ACK, CAN sequence but also make the   */
/* device transmit position information until the command is executed. */
/*---------------------------------------------------------------------*/

static void spex_cd2a_read_cmd_ack( const char *cmd )
{
	switch ( *cmd )
	{
		case 'P' :
			spex_cd2a_read_set_pos_ack( );
			break;

		case 'T' :
			spex_cd2a_read_start_scan_ack( );
			break;

		case 'E' :
			spex_cd2a_read_scan_ack( );
			break;

		case 'H' :
			spex_cd2a_read_ack( );
			break;

		default :
			fsc2_assert( 1 == 0 );
	}
}


/*---------------------------------------------------------------------*/
/* Function for handling of messages received after a position command */
/* ("P") has been send to the device.                                  */
/*---------------------------------------------------------------------*/

static void spex_cd2a_read_set_pos_ack( void )
{
	char buf[ 20 ];
	ssize_t to_be_read = 5;
	ssize_t already_read = 0;
	char *bp;
	bool done = UNSET;

	
	while ( already_read < to_be_read )
	{
		if ( ( already_read +=
			   fsc2_serial_read( SERIAL_PORT, buf + already_read,
								 to_be_read - already_read, 1000000,
								 SET ) ) < 0 )
			spex_cd2a_comm_fail( );
		stop_on_user_request( );
	}

	/* According to the manual the device is supposed to send an ACK and
	   then a CAN, but tests did show that it instead sends a CAN, an ACK,
	   and then a CAN for three times before it starts reporting the
	   current position until the final position is reached. */

	if ( buf[ 0 ] != CAN || buf[ 1 ] != ACK || buf[ 2 ] != CAN ||
		 buf[ 3 ] != CAN || buf[ 4 ] != CAN )
		spex_cd2a_wrong_data( );

	/* Keep reading until the final position is reached, each time carefully
	   testing what the device is sending. */

	to_be_read = pos_mess_len;

	while ( ! done )
	{
		already_read = 0;

		while ( already_read < to_be_read )
		{
			if ( ( already_read +=
				   fsc2_serial_read( SERIAL_PORT, buf + already_read,
									 to_be_read - already_read, 1000000,
									 SET ) ) < 0 )
				spex_cd2a_comm_fail( );
			stop_on_user_request( );
		}

		bp = buf;

		/* In STANDARD data format the first byte has to be a STX */

		if ( spex_cd2a.data_format == STANDARD && *bp++ != STX )
			spex_cd2a_wrong_data( );

		/* Check that the reported status is reasonable */

		switch ( *bp++ )
		{
			case '*' :          /* final position reached ? */
				done = SET;
				break;

			case 'P' :          /* still moving to final position ? */
				break;

			default :
				spex_cd2a_wrong_data( );
		}

		/* Check the rest of the message */

		spex_cd2a_pos_mess_check( bp );
	}
}
		

/*-----------------------------------------------------------------------*/
/* Function for handling of messages received after a start trigger scan */
/* command ("T") has been send to the device.                            */
/*-----------------------------------------------------------------------*/

static void spex_cd2a_read_start_scan_ack( void )
{
	char buf[ 20 ];
	ssize_t to_be_read = pos_mess_len;
	ssize_t already_read = 0;
	char *bp;
	bool done = UNSET;

	
	/* Read the ACK and CAN that get send at first (or an error indication) */

	spex_cd2a_read_ack( );

	/* Now repeatedly read in the current position until the start position
	   for the scan is reached. */

	while ( ! done )
	{
		already_read = 0;

		while ( already_read < to_be_read )
		{
			if ( ( already_read +=
				   fsc2_serial_read( SERIAL_PORT, buf + already_read,
									 to_be_read - already_read, 1000000,
									 SET ) ) < 0 )
				spex_cd2a_comm_fail( );
			stop_on_user_request( );
		}

		bp = buf;

		if ( spex_cd2a.data_format == STANDARD && *bp++ != STX )
			spex_cd2a_wrong_data( );

		/* Check that the reported status is reasonable */

		switch ( *bp++ )
		{
			case 'S' :          /* final position reached ? */
				done = SET;
				break;

			case 'P' :          /* still moving to final position ? */
				break;

			default :
				spex_cd2a_wrong_data( );
		}

		/* Check the rest of the message */

		spex_cd2a_pos_mess_check( bp );
	}
}


/*--------------------------------------------------------------------*/
/* Function for handling of messages received after a trigger command */
/* ("E") during a burst scan has been send to the device.             */
/*--------------------------------------------------------------------*/

static void spex_cd2a_read_scan_ack( void )
{
	char buf[ 20 ];
	ssize_t to_be_read = pos_mess_len;
	ssize_t already_read = 0;
	char *bp;
	bool done = UNSET;

	
	/* Read the ACK and CAN that get send at first (or an error indication) */

	spex_cd2a_read_ack( );

	/* Now repeatedly read in the position until the burst movement is
	   complete */

	while ( ! done )
	{
		already_read = 0;

		while ( already_read < to_be_read )
		{
			if ( ( already_read +=
				   fsc2_serial_read( SERIAL_PORT, buf + already_read,
									 to_be_read - already_read, 1000000,
									 SET ) ) < 0 )
				spex_cd2a_comm_fail( );
			stop_on_user_request( );
		}

		bp = buf;

		if ( spex_cd2a.data_format == STANDARD && *bp++ != STX )
			spex_cd2a_wrong_data( );

		/* Check that the reported status is reasonable */

		switch ( *bp++ )
		{
			case 'B' :          /* final position reached ? */
				done = SET;
				break;

			case 'P' :          /* still moving to final position ? */
				break;

			default :
				spex_cd2a_wrong_data( );
		}

		/* Check the rest of the message */

		spex_cd2a_pos_mess_check( bp );
	}
}


/*-------------------------------------------------------------------*/
/* Function that gets called to close the device file for the device */
/*-------------------------------------------------------------------*/

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


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void spex_cd2a_wrong_data( void )
{
	print( FATAL, "Device send unexpected data.\n" );
	THROW( EXCEPTION );
}


/*-------------------------------------------------------------------------*/
/* Function calculates the length of position messages send by the device. */
/* The message consists at least of a status char, a char indicating the   */
/* unit, an eigth byte long floating point number in ASCII format and a    */
/* carriage return. When STANDARD data format is used the message starts   */
/* with a STX char and an ETX char is send directly after tne number. If   */
/* checksums are transmitted two additional bytes are send directly before */
/* carriage return. And if the device is set up to send a linefeed this is */
/* send at the end of the string.                                          */
/*-------------------------------------------------------------------------*/

static ssize_t spex_cd2a_pos_mess_len( void )
{
	ssize_t len = 11;


	if ( spex_cd2a.data_format == STANDARD )
		len += 2;
	if ( spex_cd2a.use_checksum )
		len += 2;
	if ( spex_cd2a.sends_lf )
		len++;

	return len;
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static void spex_cd2a_pos_mess_check( const char *bp )
{
	char *ep;


	/* Check that the reported unit is reasonable */

	if ( ( spex_cd2a.mode == WN && *bp == 'W' ) ||
		 ( spex_cd2a.mode == WND && *bp == 'D' ) ||
		 ( spex_cd2a.mode == WL &&
		   ( ( spex_cd2a.units == NANOMETER && *bp == 'N' ) ||
			 ( spex_cd2a.units == ANGSTROEM && *bp == 'A' ) ) ) )
		bp++;
	else
		spex_cd2a_wrong_data( );

	/* Check that the reported wave-length or -number is reasonable, i.e.
	   is a number consisting of 8 bytes. */

	errno = 0;
	strtod( bp, &ep );
	if ( errno || ep != bp + 8 )
		spex_cd2a_wrong_data( );

	bp += 8;

	/* In STANDARD data format the next byte has to be an ETX */

	if ( spex_cd2a.data_format == STANDARD && *bp++ != ETX )
		spex_cd2a_wrong_data( );

	/* Skip the checksum if the device is configured to send one */

	if ( spex_cd2a.use_checksum )
		bp += 2;

	/* Next byte must be a carriage return */

	if ( *bp++ != '\r' )
		spex_cd2a_wrong_data( );

	/* Finally, there might be a linefeed if the device is configured to
	   do send one */

	if ( spex_cd2a.sends_lf && *bp != '\n' )
		spex_cd2a_wrong_data( );
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
