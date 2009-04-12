/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "spex232.h"


static bool spex232_comm_init( void );

static unsigned char spex232_autobaud( void );

static void spex232_reboot( void );

static void spex232_discard_response( void );

static unsigned char spex232_switch_to_int_mode( void );

static bool spex232_check_confirmation( void );

static void spex232_motor_init( void );

static void spex232_set_motor_position( long int /* pos */ );

#if 0
static long int spex232_get_motor_position( void );
#endif

static void spex232_move_relative( long int /* steps */ );

static bool spex232_motor_is_busy( void );

static void spex232_comm_fail( void );


#define MAX_INIT_RETRIES       3
#define MAX_AUTOBAUD_REPEATS   3
#define SERIAL_WAIT            30000     /* 30 ms */


/*--------------------------------------*
 * Function for initializing the device
 *--------------------------------------*/

bool
spex232_init( void )
{
	int init_retries = 0;


#ifndef SPEX232_TEST
	/* Try to initialize communication with the device, bringing it into
	   "intellegent communication mode" with the MAIN program running. If
	   this fails even after a few retries give up. */

	while ( ! spex232_comm_init( ) && ++init_retries <= MAX_INIT_RETRIES )
		/* empty */ ;

	if ( init_retries >= MAX_INIT_RETRIES )
		return FAIL;

	/* Initialize the motor */

	spex232_motor_init( );
#endif

	/* Set the current position to what we read from the state file (unless
	   an autocalibration has been done) */

	/* If the user asked for the monochromator for a specific wavelength
	   set it now */

	if ( spex232.is_wavelength )
		spex232.wavelength = spex232_set_wavelength( spex232.wavelength );
    else
        spex232.wavelength = spex232_p2wl( spex232.motor_position );

	spex232.is_wavelength = SET;

	return OK;
}


/*--------------------------------------------------*
 * Initialization of communication with the  device
 *--------------------------------------------------*/

static bool
spex232_comm_init( void )
{
	unsigned char buf[ ] = "O2000";


	/* Start with trying to "autobaud". This may either result in total
	   failure to communicate with the device (in this case an exception
	   is thrown), a potentially recoverable failure (in which case we
	   get a NUL character) or the the device being in "intelligent
	   communication mode". In the later case it may be running the MAIN
	   program (that's what we need) or still the BOOT program. */

	switch ( spex232_autobaud( ) )
	{
		case '\0' :
			return FAIL;

		case 'F' :               /* MAIN program running */
			return OK;

		case 'B' :               /* BOOT program running */
			break;

		default :
			fsc2_impossible( );
	}

	/* If we arrive here we're now talking to the BOOT program. Try to switch
	   to the MAIN program (the trailing '\0' at the end of the string must
	   also be send!). */

	if ( fsc2_serial_write( spex232.sn, buf, sizeof buf, SERIAL_WAIT, SET )
		                                                        != sizeof buf )
		spex232_comm_fail( );

	fsc2_usleep( 500000, UNSET );

	/* The device now should react by sending '*' */

	if (    fsc2_serial_read( spex232.sn, buf, 1, NULL,
                              SERIAL_WAIT, SET )  != 1
         || buf[ 0 ] != '*' )
		spex232_comm_fail( );


	/* Test if the device is really running the MAIN program */

	buf[ 0 ] = ' ';
	if (    fsc2_serial_write( spex232.sn, buf, 1, SERIAL_WAIT, SET ) != 1
         || fsc2_serial_read( spex232.sn, buf, 1, NULL,
                              SERIAL_WAIT, SET ) != 1 )
		spex232_comm_fail( );

	switch ( buf[ 0 ] )
	{
		case 'F' :
			return OK;

		case 0x1b :
			if ( spex232_switch_to_int_mode( ) == 'F' )
				return OK;
	}

	return FAIL;
}


/*-----------------------------------------------------------------------*
 * Sends the "WHERE AM I" command - if the device has just been powered
 * this "autoauds" it, otherwise it should tell us in which mode it's
 * in. If it already is in "intelligent communication mode" we return
 * immediately the character that indicates if we're talking to the
 * BOOT or the MAIN program. If it's in "terminal communication" mode
 * or has been freshly autobauded  we try to switch to "intelligent
 * communication mode" and return the mode it's then reporting. On
 * complete failure we return '\0'.
 *-----------------------------------------------------------------------*/

static unsigned char
spex232_autobaud( void )
{
	unsigned int autobaud_repeats = 0;
	unsigned char buf;


	/* Try  few times to "autobaud" (if the device is already "autobauded"
	   then the command elicits a response indicating the mode the device
	   is in) */

	while ( 1 ) {
		buf = ' ';

		/* Send autobaud command */

		if ( fsc2_serial_write( spex232.sn, &buf, 1, SERIAL_WAIT, SET ) != 1 )
			spex232_comm_fail( );

		/* Try to read reply and if there's one of the expected characters
		   break from the loop */

		fsc2_usleep( 500000, UNSET );

		if ( fsc2_serial_read( spex232.sn, &buf, 1, NULL,
                               SERIAL_WAIT, SET ) == 1 )
		{
			/* If the device is already in "intelligent communication mode" it
			   either returns 'B' or 'F', telling if it's running the BOOT or
			   the MAIN program */

			if ( buf == 'B' || buf == 'F' )
				return buf;

			/* If it has successfully "autobauded" '*' is returned, and if it
			   was already "autobauded" but in terminal communication mode
			   (i.e. communication had been previously established with a
			   hand-held controller) an <ESC> character is returned. */

			if ( buf == '*' || buf == 0x1b )
				break;
		}

		/* All other replies mean failure - retry unless we have already
		   tried several times in which case we try a reboot, perhaps the
		   device is just hanging */

		if ( ++autobaud_repeats >= MAX_AUTOBAUD_REPEATS )
		{
			spex232_reboot( );
			return '\0';
		}
	}

	/* There may now be a response for the handheld controller waiting to be
	   delivered and that needs discarding */

	spex232_discard_response( );

	/* If the device had previously established communication with a hand-held
	   controller as indeicated by having received an <ESC> character we need
	   to try to switch to "intelligent communication mode" to make it talk to
	   the computer */

	if ( buf == 0x1b )
		return spex232_switch_to_int_mode( );

	/* Final alternative: we've got a '*', so the device has been freshly
	   powered on or rebooted and must now be switched to "intelligent
	   communication mode". The device is supposed to react by sending '='. */

	buf = 247;
	if (    fsc2_serial_write( spex232.sn, &buf, 1, SERIAL_WAIT, SET ) != 1
         || fsc2_serial_read( spex232.sn, &buf, 1, NULL,
                              SERIAL_WAIT, SET )  != 1
         || buf != '=' )
		spex232_comm_fail( );

	buf = ' ';
	if (    fsc2_serial_write( spex232.sn, &buf, 1, SERIAL_WAIT, SET ) != 1
         || fsc2_serial_read( spex232.sn, &buf, 1, NULL,
                              SERIAL_WAIT, SET )  != 1 )
		spex232_comm_fail( );

	if ( buf == 'B' )
		return 'B';

	spex232_reboot( );

	return '\0';
}


/*------------------------------------------------------------*
 * Reboots the device (after trying to switch to "intelligent
 * communication mode")
 *------------------------------------------------------------*/

static void
spex232_reboot( void )
{
	unsigned char buf = 248;


	/* Switch to "inteligent communication mode", then wait for 200 ms*/

	if ( fsc2_serial_write( spex232.sn, &buf, 1, SERIAL_WAIT, SET ) != 1 )
		spex232_comm_fail( );

	fsc2_usleep( 200000, UNSET );

	/* Send the reboot command */

	buf = 222;
	if ( fsc2_serial_write( spex232.sn, &buf, 1, SERIAL_WAIT, SET ) != 1 )
		spex232_comm_fail( );
}


/*---------------------------------------------------------------------*
 * In some situation the device may send data destined for a hand-held
 * controller. This function tries to read them in and discards them.
 *---------------------------------------------------------------------*/

static void
spex232_discard_response( void )
{
	unsigned char buf;


	while ( fsc2_serial_read( spex232.sn, &buf, 1, NULL,
                              SERIAL_WAIT, SET ) == 1 )
		/* empty */ ;
}


/*-------------------------------------------------------------------*
 * Switches the device from terminal mode for talking to a hand-held
 * controller to "intelligent communication mode". Returns either 'B'
 * or 'F', indicating which program we're talking to on success or
 * '\0' on failure.
 *----------------------------------------------------------*/

static unsigned char
spex232_switch_to_int_mode( void )
{
	unsigned char buf;


	/* Send command to switch the device to "intelligent communication mode" */

	buf = 248;
	if ( fsc2_serial_write( spex232.sn, &buf, 1, SERIAL_WAIT, SET ) != 1 )
		spex232_comm_fail( );

	fsc2_usleep( 200000, UNSET );

	/* Check if this worked and which program we're talking to */

	buf = ' ';
	if (    fsc2_serial_write( spex232.sn, &buf, 1, SERIAL_WAIT, SET ) != 1
         || fsc2_serial_read( spex232.sn, &buf, 1, NULL,
                              SERIAL_WAIT, SET )  != 1 )
		spex232_comm_fail( );

	return ( buf == 'F' || buf == 'B' ) ? buf : '\0';
}


/*--------------------------------------------------------------------*
 * Several commands send a confirmation character, either 'o' or 'b',
 * indicating success or failure. This function reads this character
 * and returns boolean true for success and false for failure.
 *--------------------------------------------------------------------*/

static bool
spex232_check_confirmation( void )
{
	unsigned char buf;

	if (    fsc2_serial_read( spex232.sn, &buf, 1, NULL,
                              SERIAL_WAIT, SET )  != 1
         || ( buf != 'o' && buf != 'b' ) )
		spex232_comm_fail( );

	return buf == 'o';
}


/*------------------------------------------------------------------*
 * Function for "initializing" the monochromators motor (including
 * autocalibration if the user asked for it and the device has this
 * capability) by setting the divers limits (maximum and minimum
 * speed and "ramp time", i.e. the time needed to go from the
 * slowest ramp speed to the fastest.
 * -----------------------------------------------------------------*/


#define MOTOR_INIT_BUF_SIZE  30

static void
spex232_motor_init( void )
{
	unsigned char buf[ MOTOR_INIT_BUF_SIZE ] = "A";
	int len;


	/* Send the "MOTOR INIT" command if the user asked for it - this takes
	   extremely long (about 100 s) and is only useful for autocalibrating
	   monochromators. Moreover, it can only be done if the macro variable
	   'AUTOCALIBRATION_POSITION' has been set, otherwise we have no idea
	   what exactly the position of the monochromator after the auto-
	   calibration is. If there's no autocalibration capability (or no auto-
	   calibration is asked for) use the wavelength from the state file. */

#if defined AUTOCALIBRATION_POSITION
	if ( spex232.need_motor_init )
	{
		if (    fsc2_serial_write( spex232.sn, buf, 1, 100000000, SET ) != 1
             || ! spex232_check_confirmation( ) )
			spex232_comm_fail( );
		if ( spex232.mode = WL )
			spex232.motor_position = spex_wl2p( AUTOCALIBRATION_POSITION );
		else
			spex232.motor_position =
				        spex_wl2p( spex232_wl2wn( AUTOCALIBRATION_POSITION ) );
	}
	else
    {
        if ( ! spex232.do_enforce_position )
            spex232_set_motor_position( spex232.motor_position );
        else
            spex232_set_motor_position( spex232.enforced_position );
    }
#else
    if ( ! spex232.do_enforce_position )
        spex232_set_motor_position( spex232.motor_position );
    else
        spex232_set_motor_position( spex232.enforced_position );
#endif

	/* Set the motor minimum and maximum speed and the ramp time */

	len = snprintf( ( char * ) buf, MOTOR_INIT_BUF_SIZE, "B0,%d,%d,%d\r",
					MIN_STEPS_PER_SECOND, MAX_STEPS_PER_SECOND, RAMP_TIME );
	SPEX232_ASSERT( len < MOTOR_INIT_BUF_SIZE );

	if (    fsc2_serial_write( spex232.sn, buf, len, SERIAL_WAIT, SET ) != len
         || ! spex232_check_confirmation( ) )
		spex232_comm_fail( );
}


/*------------------------------------------------------------------*
 * Converts an (uncorrected abolute) wavelength to a motor position
 *------------------------------------------------------------------*/

long int
spex232_wl2p( double wl )
{
    SPEX232_ASSERT(    wl - spex232.abs_lower_limit >= 0.0
                    && spex232.upper_limit - wl >= 0.0 );

	if ( spex232.mode == WL )
		return lrnd( ( wl - spex232.abs_lower_limit ) / spex232.mini_step );
	else
		return lrnd( ( spex232_wl2wn( spex232.abs_lower_limit )
					   - spex232_wl2wn( wl ) ) / spex232.mini_step );
}
				
		
/*-------------------------------------------------------------------*
 * Converts a motor position to an (uncorrected absolute) wavelength
 *-------------------------------------------------------------------*/

double
spex232_p2wl( long int pos )
{
	if ( spex232.mode == WL )
		return pos * spex232.mini_step + spex232.abs_lower_limit;
	else
		return spex232_wn2wl( spex232_wl2wn( spex232.abs_lower_limit )
                              - pos * spex232.mini_step );
}


/*-------------------------------*
 * Function for starting  a scan 
 *-------------------------------*/

void
spex232_scan_start( void )
{
    spex232.in_scan = SET;
	spex232.wavelength = spex232_set_wavelength( spex232.scan_start );
}


/*--------------------------------*
 * Function for doing a scan step 
 *--------------------------------*/

void
spex232_scan_step( void )
{
    if ( spex232.mode == WL )
        spex232.wavelength =
		      spex232_set_wavelength( spex232.wavelength + spex232.scan_step );
    else
        spex232.wavelength =
            spex232_set_wavelength( spex232_wn2wl( 
                                            spex232_wl2wn( spex232.wavelength )
                                            - spex232.scan_step ) );
}


/*---------------------------------------*
 * Function for setting a new wavelength
 *---------------------------------------*/

double
spex232_set_wavelength( double wl )
{
	long int new_pos;


	new_pos = spex232_wl2p( wl );

	SPEX232_ASSERT( new_pos >= 0 && new_pos <= spex232.max_motor_position );
	SPEX232_ASSERT(    new_pos > spex232.motor_position
                    || new_pos - spex232.backslash_steps >= 0 );

#ifndef SPEX232_TEST
    if ( FSC2_MODE == EXPERIMENT )
	{
		/* If we're driving to lower wavelenths (or higher wavenumbers) we have
		   to first add backslash and then drive up again, otherwise we can go
		   straight to the new position */

		if ( new_pos < spex232.motor_position )
		{
			SPEX232_ASSERT( new_pos - spex232.backslash_steps >= 0 );
			spex232_move_relative( new_pos - spex232.motor_position
								   - spex232.backslash_steps );
			spex232_move_relative( spex232.backslash_steps );
		} else
			spex232_move_relative( new_pos - spex232.motor_position );
	}
#endif

	spex232.motor_position = new_pos;
	return spex232_p2wl( new_pos );
}


/*-------------------------------------------------------------------*
 * Function for telling the device the current position of the motor
 *-------------------------------------------------------------------*/

static void
spex232_set_motor_position( long int pos )
{
    char buf[ 30 ];


	sprintf( buf, "G0,%ld\r", pos );
	if (    fsc2_serial_write( spex232.sn, buf, strlen( buf ),
						   SERIAL_WAIT, UNSET ) != ( ssize_t ) strlen( buf )
         || ! spex232_check_confirmation( ) )
		spex232_comm_fail( );

	spex232.motor_position = pos;
}


/*---------------------------------------------------------------------*
 * Function for querying the device the current position of the motor.
 *---------------------------------------------------------------------*/

#if 0
static long int
spex232_get_motor_position( void )
{
	char buf[ 30 ] = "H0\r";
	ssize_t len = 0;


	if (    fsc2_serial_write( spex232.sn, buf, strlen( buf ),
                               SERIAL_WAIT, UNSET ) != ( ssize_t )strlen( buf )
         || ! spex232_check_confirmation( )
         || ( len = fsc2_serial_read( spex232.sn, buf, sizeof buf, NULL,
                                      SERIAL_WAIT, UNSET ) ) < 2 )
		spex232_comm_fail( );

	buf[ len - 1 ] = '\0';
	return T_atol( buf );
}
#endif


/*------------------------------------------------------------*
 * Function for moving the motor by a certain number of steps
 *------------------------------------------------------------*/

static void
spex232_move_relative( long int steps )
{
	char buf[ 30 ];


    /* Ask the monochromator to move. Wait 200 ms before asking it if it's
       finished (that's not required in the manual but if the query gets
       send earlier the interface doesn't answer and we get a timeout) */

	sprintf( buf, "F0,%ld\r", steps );
	if (    fsc2_serial_write( spex232.sn, buf, strlen( buf ),
                               SERIAL_WAIT, UNSET ) != ( ssize_t ) strlen( buf )
         || ! spex232_check_confirmation( ) )
		spex232_comm_fail( );

    fsc2_usleep( 200000, UNSET );

	while ( spex232_motor_is_busy( ) )
        fsc2_usleep( 100000, UNSET );
}
	

/*------------------------------------------------------------*
 * Function tests if the motor is still busy and returns true
 * if it is, otherwise false
 *------------------------------------------------------------*/
 
static bool
spex232_motor_is_busy( void )
{
	char cmd = 'E';

	if (    fsc2_serial_write( spex232.sn, &cmd, 1, SERIAL_WAIT, UNSET ) != 1
         || ! spex232_check_confirmation( )
         || fsc2_serial_read( spex232.sn, &cmd, 1, NULL,
                              SERIAL_WAIT, UNSET ) != 1
         || ( cmd != 'q' && cmd != 'z' ) )
		spex232_comm_fail( );

	return cmd == 'q';
}

	
/*-----------------------------------------------------*
 * Function for opening the device file for the device
 * and setting up the communication parameters.
 *-----------------------------------------------------*/

void
spex232_open( void )
{
#ifndef SPEX232_TEST

    /* We need exclusive access to the serial port and we also need non-
       blocking mode to avoid hanging indefinitely if the other side does not
       react. O_NOCTTY is set because the serial port should not become the
       controlling terminal, otherwise line noise read as a CTRL-C might kill
       the program. */

    if ( ( spex232.tio = fsc2_serial_open( spex232.sn,
                          O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
    {
        print( FATAL, "Can't open device file for monochromator.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    spex232.tio->c_cflag = 0;

    switch ( PARITY_TYPE )
    {
        case NO_PARITY :
            break;

        case ODD_PARITY :
            spex232.tio->c_cflag |= PARENB | PARODD;
            break;

        case EVEN_PARITY :
            spex232.tio->c_cflag |= PARENB;
            break;

        default :
            fsc2_serial_close( spex232.sn );
            print( FATAL, "Invalid setting for parity bit in "
                   "configuration file for the device.\n" );
            SPEX232_THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_STOP_BITS )
    {
        case 1 :
            break;

        case 2 :
            spex232.tio->c_cflag |= CSTOPB;
            break;

        default :
            fsc2_serial_close( spex232.sn );
            print( FATAL, "Invalid setting for number of stop bits in "
                   "configuration file for the device.\n" );
            SPEX232_THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_BITS_PER_CHARACTER )
    {
        case 5 :
            spex232.tio->c_cflag |= CS5;
            break;

        case 6 :
            spex232.tio->c_cflag |= CS6;
            break;

        case 7 :
            spex232.tio->c_cflag |= CS7;
            break;

        case 8 :
            spex232.tio->c_cflag |= CS8;
            break;

        default :
            fsc2_serial_close( spex232.sn );
            print( FATAL, "Invalid setting for number of bits per "
                   "in character configuration file for the device.\n" );
            SPEX232_THROW( EXCEPTION );
    }

    spex232.tio->c_cflag |= CLOCAL | CREAD;
    spex232.tio->c_iflag = IGNBRK;
    spex232.tio->c_oflag = 0;
    spex232.tio->c_lflag = 0;

    cfsetispeed( spex232.tio, SERIAL_BAUDRATE );
    cfsetospeed( spex232.tio, SERIAL_BAUDRATE );

    /* We can't use canonical mode here... */

    spex232.tio->c_lflag = 0;

    fsc2_tcflush( spex232.sn, TCIOFLUSH );
    fsc2_tcsetattr( spex232.sn, TCSANOW, spex232.tio );
#endif

    spex232.is_open = SET;
}


/*-------------------------------------------------------------------*
 * Function that gets called to close the device file for the device
 *-------------------------------------------------------------------*/

void
spex232_close( void )
{
#ifndef SPEX232_TEST
    if ( spex232.is_open )
        fsc2_serial_close( spex232.sn );
#endif
    spex232.is_open = UNSET;
}


/*-------------------------------------------------------*
 * Function to handle situations where the communication
 * with the device failed completely.
 *-------------------------------------------------------*/

static void
spex232_comm_fail( void )
{
	print( FATAL, "Can't access the monochromator.\n" );
    SPEX232_THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
