/*
 *  $Id$
 * 
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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2_module.h"
#include "serial.h"


/* Include configuration information for the device */

#include "smsmover.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Exported functions */

int smsmover_init_hook( void );
int smsmover_test_hook( void );
int smsmover_exp_hook( void );
int smsmover_end_of_exp_hook( void );

Var_T * mover_name( Var_T * v );
Var_T * mover_position( Var_T * v );
Var_T * mover_move_relative( Var_T * v );


/* Local functions */

static void smsmover_init( void );
static void smsmover_open( void );
static double smsmover_goto( long   dev,
							 double pos );
static void smsmover_fail( void );


static struct {
	bool is_open;
	double position[ DEVICE_COUNT ];
	double lower_limit[ DEVICE_COUNT ];
	double upper_limit[ DEVICE_COUNT ];
	double min_position[ DEVICE_COUNT ];
	double max_position[ DEVICE_COUNT ];
    struct termios *tio;
} smsmover;

#define WAIT_TIME 100000

/* Defines for the parity used by the device */

#define NO_PARITY         0
#define ODD_PARITY        1
#define EVEN_PARITY       2


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
smsmover_init_hook( void )
{
	long i;


    fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	smsmover.is_open = UNSET;

	for ( i = 0; i < DEVICE_COUNT; i++ )
	{
		smsmover.position[ i ]= 0.0;
		smsmover.min_position[ i ] = HUGE_VAL;
		smsmover.max_position[ i ] = - HUGE_VAL;
	}

	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
smsmover_exp_hook( void )
{
	smsmover_init( );
	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
smsmover_end_of_exp_hook( void )
{
    if ( smsmover.is_open )
	{
#ifndef SMSMOVER_TEST
        fsc2_serial_close( SERIAL_PORT );
#endif
        smsmover.is_open = UNSET;
    }

	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
mover_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
mover_position( Var_T * v )
{
	long dev;
	double position;


	if ( v == NULL )
	{
		print( FATAL, "Missing device number argument.\n" );
		THROW( EXCEPTION );
	}

	dev = get_strict_long( v, "device number" ) - 1;

	if ( dev < 0 || dev >= DEVICE_COUNT )
	{
		print( FATAL, "Invalid device number of %ld, must be between 1 and "
			   "%d.\n", dev + 1, DEVICE_COUNT );
		THROW( EXCEPTION );
	}

	v = vars_pop( v );

	if ( v == NULL )
		return vars_push( FLOAT_VAR, smsmover.position );

	position = get_double( v, "position" );

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
	{
		smsmover.min_position[ dev ] = d_min( smsmover.min_position[ dev ],
											  position );
		smsmover.max_position[ dev ] = d_max( smsmover.max_position[ dev ],
											  position );
		return vars_push( FLOAT_VAR, smsmover.position[ dev ] = position );
	}

	if ( position > smsmover.upper_limit[ dev ] )
	{
		print( FATAL, "Requested position of %f is larger then upper limit of "
			   "%f for device #%ld\n", position, smsmover.upper_limit[ dev ],
			   dev + 1 );
		THROW( EXCEPTION );
	}

	if ( position < smsmover.lower_limit[ dev ] )
	{
		print( FATAL, "Requested position of %f is lower then lower limit of "
			   "%f for device #%ld\n", position, smsmover.lower_limit[ dev ],
			   dev + 1 );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR,
					  smsmover.position[ dev ] = smsmover_goto( dev,
																position ) );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
mover_move_relative( Var_T * v )
{
	long dev;
	double step;


	if ( v == NULL )
	{
		print( FATAL, "Missing device number argument.\n" );
		THROW( EXCEPTION );
	}

	dev = get_strict_long( v, "device number" ) - 1;

	if ( dev < 0 || dev >= DEVICE_COUNT )
	{
		print( FATAL, "Invalid device number of %ld, must be between 1 and "
			   "%d.\n", dev + 1, DEVICE_COUNT );
		THROW( EXCEPTION );
	}

	v = vars_pop( v );

	if ( v == NULL )
	{
		print( FATAL, "Missing step size.\n" );
		THROW( EXCEPTION );
	}

	step = get_double( v, "step size" );

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
	{
		smsmover.min_position[ dev ] = d_min( smsmover.min_position[ dev ],
											  smsmover.position[ dev ] + step );
		smsmover.max_position[ dev ] = d_max( smsmover.max_position[ dev ],
											  smsmover.position[ dev ] + step );
		return vars_push( FLOAT_VAR, smsmover.position[ dev ] += step );
	}

	if ( smsmover.position[ dev ] + step > smsmover.upper_limit[ dev ] )
	{
		print( FATAL, "Resulting position of %f is larger then upper limit of "
			   "%f for device #%ld\n", smsmover.position[ dev ] + step,
			   smsmover.upper_limit[ dev ], dev + 1 );
		THROW( EXCEPTION );
	}

	if ( smsmover.position[ dev ] + step < smsmover.lower_limit[ dev ] )
	{
		print( FATAL, "Resulting position of %f is lower then lower limit of "
			   "%f for device #%ld\n", smsmover.position[ dev ] + step,
			   smsmover.lower_limit[ dev ], dev + 1 );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, smsmover.position[ dev ] =
					    smsmover_goto( dev, smsmover.position[ dev ] + step ) );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

static void
smsmover_init( void )
{
	char buf[ 40 ];
	char reply[ 40 ];
	char *p;
	long i;
	long len = 0;


	/* Open connection to device */

	smsmover_open( );

#ifndef SMSMOVER_TEST

	/* Ask the device for the current position and lower and upper limits for
	   all axis and check if the limits have not been exceeded during the test
	   run */

	for ( i = 0; i < DEVICE_COUNT; i++ )
	{
		sprintf( buf, "%ld npos ", i + 1 );
		if (    fsc2_serial_write( SERIAL_PORT, buf, strlen( buf ),
								   WAIT_TIME, UNSET ) <= 0
			 || ( len = fsc2_serial_read( SERIAL_PORT, reply, sizeof reply,
										  "\r\n", WAIT_TIME, UNSET ) ) < 2
			 || strncmp( buf + len - 2, "\r\n", 2 ) )
			smsmover_fail( );

		reply[ len - 2 ] = '\0';
		smsmover.position[ i ] = T_atod( reply );

		sprintf( buf, "%ld getnlimit ", i + 1 );
		if (    fsc2_serial_write( SERIAL_PORT, buf, strlen( buf ),
								   WAIT_TIME, UNSET ) <= 0
			 || ( len = fsc2_serial_read( SERIAL_PORT, reply, sizeof reply,
										  "\r\n", WAIT_TIME, UNSET ) ) < 2
			 || strncmp( buf + len - 2, "\r\n", 2 ) )
			smsmover_fail( );

		reply[ len - 2 ] = '\0';
		if ( ! ( p = strchr( reply, ' ' ) ) )
			smsmover_fail( );

		*p++ = '\0';

		smsmover.lower_limit[ i ] = T_atod( reply );
		smsmover.upper_limit[ i ] = T_atod( p );

		if ( smsmover.min_position[ i ] <
			              smsmover.lower_limit[ i ] - smsmover.posistion[ i ] )
		{
			print( FATAL, "During the test run a position of %f was requested "
				   "for device #%ld which is lower that the lower limit of "
				   "%f.\n",
				   smsmover.min_position[ i ] + smsmover.posistion[ i ], i + 1,
				   smsmover.lower_limit[ i ] );
			THROW( EXCEPTION );
		}

		if ( smsmover.max_position[ i ] >
			              smsmover.upper_limit[ i ] - smsmover.posistion[ i ] )
		{
			print( FATAL, "During the test run a position of %f was requested "
				   "for device #%ld which is larger that the upper limit of "
				   "%f.\n",
				   smsmover.max_position[ i ] + smsmover.posistion[ i ], i + 1,
				   smsmover.upper_limit[ i ] );
			THROW( EXCEPTION );
		}
	}

#endif
}


/*-----------------------------------------------------*
 * Function for opening the device file for the device
 * and setting up the communication parameters.
 *-----------------------------------------------------*/

static void
smsmover_open( void )
{
#ifndef SMSMOVER_TEST

    /* We need exclusive access to the serial port and we also need non-
       blocking mode to avoid hanging indefinitely if the other side does not
       react. O_NOCTTY is set because the serial port should not become the
       controlling terminal, otherwise line noise read as a CTRL-C might kill
       the program. */

    if ( ( smsmover.tio = fsc2_serial_open( SERIAL_PORT, DEVICE_NAME,
                          O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
    {
        print( FATAL, "Can't open device file for power supply.\n" );
        THROW( EXCEPTION );
    }

    smsmover.tio->c_cflag = 0;

    switch ( PARITY_TYPE )
    {
        case NO_PARITY :
            break;

        case ODD_PARITY :
            smsmover.tio->c_cflag |= PARENB | PARODD;
            break;

        case EVEN_PARITY :
            smsmover.tio->c_cflag |= PARENB;
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
            smsmover.tio->c_cflag |= CSTOPB;
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
            smsmover.tio->c_cflag |= CS5;
            break;

        case 6 :
            smsmover.tio->c_cflag |= CS6;
            break;

        case 7 :
            smsmover.tio->c_cflag |= CS7;
            break;

        case 8 :
            smsmover.tio->c_cflag |= CS8;
            break;

        default :
            fsc2_serial_close( SERIAL_PORT );
            print( FATAL, "Invalid setting for number of bits per "
                   "in character configuration file for the device.\n" );
			THROW( EXCEPTION );
    }

    smsmover.tio->c_cflag |= CLOCAL | CREAD;
    smsmover.tio->c_iflag = IGNBRK;
    smsmover.tio->c_oflag = 0;
    smsmover.tio->c_lflag = 0;

    cfsetispeed( smsmover.tio, SERIAL_BAUDRATE );
    cfsetospeed( smsmover.tio, SERIAL_BAUDRATE );

    /* We can't use canonical mode here... */

    smsmover.tio->c_lflag = 0;

    fsc2_tcflush( SERIAL_PORT, TCIOFLUSH );
    fsc2_tcsetattr( SERIAL_PORT, TCSANOW, smsmover.tio );

#endif

    smsmover.is_open = SET;
}


/*-------------------------------------------------------*
 * Function for moving one of the axis to a new position
 *-------------------------------------------------------*/

static double
smsmover_goto( long   dev,
			   double pos )
{
#ifndef SMSMOVER_TEST
	char buf[ 40 ];
	char reply[ 30 ];
	long len;


	/* Send the command to move to the new position */

	sprintf( buf, "%.6f %ld nm ", pos, dev + 1 );
	len = strlen( buf );

	if ( fsc2_serial_write( SERIAL_PORT, buf, len, WAIT_TIME, UNSET ) != len )
		smsmover_fail( );

	/* Now wait until the device reports that the final position has been
	   reached, give the user a chance to interrupt */

	sprintf( buf, "%ld nst ", dev + 1 );
	len = strlen( buf );

	while ( 1 )
	{
		if (    fsc2_serial_write( SERIAL_PORT, buf, len,
								   WAIT_TIME, UNSET ) != len
			 || fsc2_serial_read( SERIAL_PORT, reply, 3, "\r\n",
								  WAIT_TIME, UNSET ) != 3 )
			smsmover_fail( );

		if ( buf[ 0 ] == '0' )
			break;

		if ( check_user_request ( ) )
		{
			sprintf( buf, "%ld nabort ", dev + 1 );
			fsc2_serial_write( SERIAL_PORT, buf, strlen( buf ),
							   WAIT_TIME, UNSET );
			THROW( USER_BREAK_EXCEPTION );
		}
	}

	/* Get the new position value from the device */

	sprintf( buf, "%ld npos ", dev + 1 );
	if (    fsc2_serial_write( SERIAL_PORT, buf, strlen( buf ),
							   WAIT_TIME, UNSET ) <= 0
		 || ( len = fsc2_serial_read( SERIAL_PORT, reply, sizeof reply, "\r\n",
									  WAIT_TIME, UNSET ) ) < 2
		 || strncmp( buf + len - 2, "\r\n", 2 ) )
			smsmover_fail( );

	reply[ len - 2 ] = '\0';
	return T_atod( reply );
#else
	return pos;
#endif
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

static void
smsmover_fail( void )
{
	print( FATAL, "Can't access the device.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
