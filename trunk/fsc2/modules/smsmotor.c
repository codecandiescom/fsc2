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

#include "smsmotor.conf"


/* Set name and type of device */

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Exported functions */

int smsmotor_init_hook( void );
int smsmotor_exp_hook( void );
int smsmotor_end_of_exp_hook( void );


Var_T * motor_name(          Var_T * v );
Var_T * motor_position(      Var_T * v );
Var_T * motor_move_relative( Var_T * v );
Var_T * motor_limits(        Var_T * v );
Var_T * motor_speed(         Var_T * v );
Var_T * motor_acceleration(  Var_T * v );


/* Local functions */

static void smsmotor_init( void );
static void smsmotor_open( void );
static double smsmotor_goto( long   dev,
							 double pos );
static void smsmotor_fail( void );
static void smsmotor_set_speed( long   dev,
                                double speed );
static double smsmotor_get_speed( long  dev );
static void smsmotor_set_acceleration( long   dev,
                                       double acceleration );
static double smsmotor_get_acceleration( long  dev );


static struct {
    int              sn;
	bool             is_open;
	double           position[ DEVICE_COUNT ];
	double           lower_limit[ DEVICE_COUNT ];
	double           upper_limit[ DEVICE_COUNT ];
	double           abs_min_position[ DEVICE_COUNT ];
	double           abs_max_position[ DEVICE_COUNT ];
	double           rel_min_position[ DEVICE_COUNT ];
	double           rel_max_position[ DEVICE_COUNT ];
    bool             is_rel_mode[ DEVICE_COUNT ];
    double           init_speed[ DEVICE_COUNT ];
    double           speed[ DEVICE_COUNT ];
    bool             speed_is_set[ DEVICE_COUNT ];
    double           init_acceleration[ DEVICE_COUNT ];
    double           acceleration[ DEVICE_COUNT ];
    bool             acceleration_is_set[ DEVICE_COUNT ];
    struct termios * tio;
} smsmotor;


/* Maximum time (in micro-seconds) to wait for a reply from the device */


#define WAIT_TIME 100000


/* Values for speed and acceleration reported during test run */

#define TEST_SPEED           26.0
#define TEST_ACCELERATION   200.0


/*---------------------------------------------*
 * Init hook, called when the module is loaded
 *---------------------------------------------*/

int
smsmotor_init_hook( void )
{
	long i;


    /* Request the seral port for the module */

    smsmotor.sn = fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );
	smsmotor.is_open = UNSET;

    /* Initialize the device structure */

	for ( i = 0; i < DEVICE_COUNT; i++ )
	{
		smsmotor.position[ i ]= 0.0;
		smsmotor.abs_min_position[ i ]    =   0.5 * HUGE_VAL;
		smsmotor.abs_max_position[ i ]    = - 0.5 * HUGE_VAL;
		smsmotor.rel_min_position[ i ]    =   0.5 * HUGE_VAL;
		smsmotor.rel_max_position[ i ]    = - 0.5 * HUGE_VAL;
        smsmotor.is_rel_mode[ i ]         = SET;
        smsmotor.lower_limit[ i ]         = default_limits[ i ][ 0 ];
        smsmotor.upper_limit[ i ]         = default_limits[ i ][ 1 ];
        smsmotor.speed[ i ]               = TEST_SPEED;
        smsmotor.speed_is_set[ i ]        = UNSET;
        smsmotor.acceleration_is_set[ i ] = UNSET;
        smsmotor.acceleration[ i ]        = TEST_ACCELERATION;
	}

	return 1;
}


/*---------------------------------------------------------*
 * Exp hook, called when an experiment is started for real
 *---------------------------------------------------------*/

int
smsmotor_exp_hook( void )
{
	smsmotor_init( );
	return 1;
}


/*-----------------------------------------------------*
 * End of exp hook, called at the end of an experiment
 *-----------------------------------------------------*/

int
smsmotor_end_of_exp_hook( void )
{
    if ( smsmotor.is_open )
	{
#ifndef SMSMOTOR_TEST
        fsc2_serial_close( smsmotor.sn );
#endif
        smsmotor.is_open = UNSET;
    }

	return 1;
}


/*------------------------------*
 * Just returns the device name 
 *------------------------------*/

Var_T *
motor_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------------------*
 * If called with only a single argument returns the current
 * position of the addressed axis, if called with two arguments
 * moves the addressed axis to a new position and returns the
 * position reported by the device.
 *--------------------------------------------------------------*/

Var_T *
motor_position( Var_T * v )
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
		return vars_push( FLOAT_VAR, smsmotor.position[ dev ] );

	position = get_double( v, "position" );

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
	{
        smsmotor.is_rel_mode[ dev ] = UNSET;
		smsmotor.abs_min_position[ dev ] =
                            d_min( smsmotor.abs_min_position[ dev ], position );
		smsmotor.abs_max_position[ dev ] =
                            d_max( smsmotor.abs_max_position[ dev ], position );

		return vars_push( FLOAT_VAR, smsmotor.position[ dev ] = position );
	}

	if ( position > smsmotor.upper_limit[ dev ] )
	{
		print( FATAL, "Requested position of %f is larger then upper limit of "
			   "%f for device #%ld\n", position, smsmotor.upper_limit[ dev ],
			   dev + 1 );
		THROW( EXCEPTION );
	}

	if ( position < smsmotor.lower_limit[ dev ] )
	{
		print( FATAL, "Requested position of %f is lower then lower limit of "
			   "%f for device #%ld\n", position, smsmotor.lower_limit[ dev ],
			   dev + 1 );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR,
					  smsmotor.position[ dev ] = smsmotor_goto( dev,
																position ) );
}


/*------------------------------------------------------------------*
 * Moves an axis to a new position relative to the current position
 *------------------------------------------------------------------*/

Var_T *
motor_move_relative( Var_T * v )
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
		print( FATAL, "Missing amount position change.\n" );
		THROW( EXCEPTION );
	}

	step = get_double( v, "amount position change" );

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
	{
        if ( smsmotor.is_rel_mode[ dev ] ) {
            smsmotor.rel_min_position[ dev ] =
                                      d_min( smsmotor.rel_min_position[ dev ], 
                                             smsmotor.position[ dev ] + step );
            smsmotor.rel_max_position[ dev ] =
                                      d_max( smsmotor.rel_max_position[ dev ],
                                             smsmotor.position[ dev ] + step );
        }
        else
        {
            smsmotor.abs_min_position[ dev ] =
                                      d_min( smsmotor.abs_min_position[ dev ],
                                             smsmotor.position[ dev ] + step );
            smsmotor.abs_max_position[ dev ] =
                                      d_max( smsmotor.abs_max_position[ dev ],
                                             smsmotor.position[ dev ] + step );
        }
		return vars_push( FLOAT_VAR, smsmotor.position[ dev ] += step );
	}

	if ( smsmotor.position[ dev ] + step > smsmotor.upper_limit[ dev ] )
	{
		print( FATAL, "Resulting position of %f is larger then upper limit of "
			   "%f for device #%ld\n", smsmotor.position[ dev ] + step,
			   smsmotor.upper_limit[ dev ], dev + 1 );
		THROW( EXCEPTION );
	}

	if ( smsmotor.position[ dev ] + step < smsmotor.lower_limit[ dev ] )
	{
		print( FATAL, "Resulting position of %f is lower then lower limit of "
			   "%f for device #%ld\n", smsmotor.position[ dev ] + step,
			   smsmotor.lower_limit[ dev ], dev + 1 );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, smsmotor.position[ dev ] =
					    smsmotor_goto( dev, smsmotor.position[ dev ] + step ) );
}


/*------------------------------------------------------------*
 * If called with a single argument returns an array with two
 * elements, the first one being the lower and the second the
 * upper limit of the addressed axis. If called with either
 * two more arguments of integer or floating point type or
 * a single arrary with two elements during the PREPARATIONS
 * phase the limits are set to these values.
 *------------------------------------------------------------*/

Var_T *
motor_limits( Var_T * v )
{
    long dev;
    double limits[ 2 ];


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
        limits[ 0 ] =  smsmotor.lower_limit[ dev ];
        limits[ 1 ] =  smsmotor.upper_limit[ dev ];
        return vars_push( FLOAT_ARR, limits, 2 );
    }

    if ( v->type & ( INT_ARR || FLOAT_ARR ) )
    {
        if ( v->dim != 1 )
        {
            print( FATAL, "Multidimensional array received as second "
                   "argument.\n" );
            THROW( EXCEPTION );
        }

        if ( v->len != 2 )
        {
            print( FATAL, "Array received as second argument does not have "
                   "tow elements\n" );
            THROW( EXCEPTION );
        }

        if ( v->type == INT_ARR )
        {
            print( WARN, "Integer data used for limits.\n" );

            limits[ 0 ] = v->val.lpnt[ 0 ];
            limits[ 1 ] = v->val.lpnt[ 1 ];
        }
        else
            memcpy( limits, v->val.dpnt, sizeof limits );
    }
    else
    {
        limits[ 0 ] = get_double( v, "lower limit" );

        if ( ( v = vars_pop( v ) ) == NULL )
        {
            print( FATAL, "Missing upper limit.\n" );
            THROW( EXCEPTION );
        }

        limits[ 1 ] = get_double( v, "upper limit" );
    }

    too_many_arguments( v );

    if ( FSC2_MODE != PREPARATION )
    {
        print( FATAL, "Limits can only be set during the PREPARATIONS "
               "section.\n" );
        THROW( EXCEPTION );
    }

    smsmotor.lower_limit[ dev ] = limits[ 0 ];
    smsmotor.upper_limit[ dev ] = limits[ 1 ];
    return vars_push( FLOAT_ARR, limits, 2 );
}


/*----------------------------------------------------------------*
 * If called with a single argument the maximum speed setting for
 * the addressed axis is returned. If there's a second argument
 * the maximum speed is set to this value.
 *----------------------------------------------------------------*/

Var_T *
motor_speed( Var_T * v )
{
    long dev;
    double speed;


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
        return vars_push( FLOAT_VAR, smsmotor.speed[ dev ] );

    speed = get_double( v, "speed" );

    if ( speed <= 0.0 )
    {
        print( FATAL, "Invalid negative or zero speed.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION )
    {
        smsmotor.speed[ dev ] = smsmotor.init_speed[ dev ] = speed;
        smsmotor.speed_is_set[ dev ] = SET;
    }
    else if ( FSC2_MODE == TEST )
        smsmotor.speed[ dev ] = speed;
    else
    {
            smsmotor_set_speed( dev, speed );
            smsmotor.speed[ dev ] = smsmotor_get_speed( dev );
    }

    return vars_push( FLOAT_VAR, smsmotor.speed[ dev ] );
}


/*-------------------------------------------------------------------*
 * If called with a single argument the maximum acceleration setting
 * for the addressed axis is returned. If there's a second argument
 * the maximum accelertion is set to this value.
 *-------------------------------------------------------------------*/

Var_T *
motor_acceleration( Var_T * v )
{
    long dev;
    double acc;


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
        return vars_push( FLOAT_VAR, smsmotor.acceleration[ dev ] );

    acc = get_double( v, "maximum acceleration" );

    if ( acc <= 0.0 )
    {
        print( FATAL, "Invalid negative or zero acceleration.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION )
    {
        smsmotor.acceleration[ dev ] = smsmotor.init_acceleration[ dev ] = acc;
        smsmotor.acceleration_is_set[ dev ] = SET;
    }
    else if ( FSC2_MODE == TEST )
        smsmotor.acceleration[ dev ] = acc;
    else
    {
            smsmotor_set_acceleration( dev, acc );
            smsmotor.acceleration[ dev ] = smsmotor_get_acceleration( dev );
    }

    return vars_push( FLOAT_VAR, smsmotor.acceleration[ dev ] );
}


/*--------------------------------------------------------------*
 * Internal function for intializing the serial prot connection
 * and setting the linmits for all axis as well, if requested,
 * the speed and acceleration. Also, the positions set during
 * the test run are checked for staying within the limits.
 *--------------------------------------------------------------*/

static void
smsmotor_init( void )
{
	char buf[ 40 ];
	char reply[ 40 ];
	long i;
	long len = 0;


	/* Open connection to device */

	smsmotor_open( );

#ifndef SMSMOTOR_TEST

	for ( i = 0; i < DEVICE_COUNT; i++ )
	{
        /* Ask for the current position */

		sprintf( buf, "%ld npos ", i + 1 );
		if (    fsc2_serial_write( smsmotor.sn, buf, strlen( buf ),
								   WAIT_TIME, UNSET ) <= 0
			 || ( len = fsc2_serial_read( smsmotor.sn, reply, sizeof reply,
										  " \r\n", WAIT_TIME, UNSET ) ) <= 3
             || strncmp( reply + len - 3, " \r\n", 3 ) )
			smsmotor_fail( );

		reply[ len - 3 ] = '\0';
		smsmotor.position[ i ] = T_atod( reply );

        /* Check that the reported position is within the limits we're going
           to set */

        if ( smsmotor.position[ i ] < smsmotor.lower_limit[ i ] )
        {
            print( FATAL, "The position of device #%d reported by the device "
                   "as %f is below the lower limit of %f to be set.\n",
                   i + 1, smsmotor.position[ i ], smsmotor.lower_limit[ i ] );
            THROW( EXCEPTION );
        }

        if ( smsmotor.position[ i ] > smsmotor.upper_limit[ i ] )
        {
            print( FATAL, "The position of device #%d reported by the device "
                   "as %f is above the upper limit of %f to be set.\n",
                   i + 1, smsmotor.position[ i ], smsmotor.upper_limit[ i ] );
            THROW( EXCEPTION );
        }

        /* Check that all positions set during the test run we're within
           the limits we're going to set */

		if (    smsmotor.abs_min_position[ i ] < smsmotor.lower_limit[ i ]
             || smsmotor.rel_min_position[ i ] + smsmotor.position[ i ]
                                                  < smsmotor.lower_limit[ i ] )
		{
			print( FATAL, "During the test run a position was requested for "
                   "device #%ld which is lower that the lower limit of %f.\n",
				   i + 1, smsmotor.lower_limit[ i ] );
			THROW( EXCEPTION );
		}

		if (    smsmotor.abs_max_position[ i ] > smsmotor.upper_limit[ i ]
             || smsmotor.rel_max_position[ i ] + smsmotor.position[ i ]
                                                  > smsmotor.upper_limit[ i ] )
		{
			print( FATAL, "During the test run a position was requested for "
                   "device #%ld which is larger that the upper limit of %f.\n",
				   i + 1, smsmotor.upper_limit[ i ] );
			THROW( EXCEPTION );
		}

        /* Set the limits */

		sprintf( buf, "%f %f %ld setnlimit ", smsmotor.lower_limit[ i ],
                 smsmotor.upper_limit[ i ], i + 1 );
		if ( fsc2_serial_write( smsmotor.sn, buf, strlen( buf ),
                                WAIT_TIME, UNSET ) <= 0 )
			smsmotor_fail( );

        /* Set a maximum speed if asked to during the PREPARATIONS section,
           otherwise just fetch the current setting */

        if ( smsmotor.speed_is_set[ i ] )
            smsmotor_set_speed( i, smsmotor.init_speed[ i ] );
        smsmotor.speed[ i ] = smsmotor_get_speed( i );

        /* Set a maximum acceleration if asked to during the PREPARATIONS
           section, otherwise just fetch the current setting */

        if ( smsmotor.acceleration_is_set[ i ] )
            smsmotor_set_acceleration( i, smsmotor.init_acceleration[ i ] );
        smsmotor.acceleration[ i ] = smsmotor_get_acceleration( i );
	}
#endif
}


/*-----------------------------------------------------*
 * Function for opening the device file for the device
 * and setting up the communication parameters.
 *-----------------------------------------------------*/

static void
smsmotor_open( void )
{
#ifndef SMSMOTOR_TEST

    /* We need exclusive access to the serial port and we also need non-
       blocking mode to avoid hanging indefinitely if the other side does not
       react. O_NOCTTY is set because the serial port should not become the
       controlling terminal, otherwise line noise read as a CTRL-C might kill
       the program. */

    if ( ( smsmotor.tio = fsc2_serial_open( smsmotor.sn,
                          O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
    {
        print( FATAL, "Can't open device file for power supply.\n" );
        THROW( EXCEPTION );
    }

    smsmotor.tio->c_cflag = 0;

    switch ( PARITY_TYPE )
    {
        case NO_PARITY :
            break;

        case ODD_PARITY :
            smsmotor.tio->c_cflag |= PARENB | PARODD;
            break;

        case EVEN_PARITY :
            smsmotor.tio->c_cflag |= PARENB;
            break;

        default :
            fsc2_serial_close( smsmotor.sn );
            print( FATAL, "Invalid setting for parity bit in "
                   "configuration file for the device.\n" );
            THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_STOP_BITS )
    {
        case 1 :
            break;

        case 2 :
            smsmotor.tio->c_cflag |= CSTOPB;
            break;

        default :
            fsc2_serial_close( smsmotor.sn );
            print( FATAL, "Invalid setting for number of stop bits in "
                   "configuration file for the device.\n" );
            THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_BITS_PER_CHARACTER )
    {
        case 5 :
            smsmotor.tio->c_cflag |= CS5;
            break;

        case 6 :
            smsmotor.tio->c_cflag |= CS6;
            break;

        case 7 :
            smsmotor.tio->c_cflag |= CS7;
            break;

        case 8 :
            smsmotor.tio->c_cflag |= CS8;
            break;

        default :
            fsc2_serial_close( smsmotor.sn );
            print( FATAL, "Invalid setting for number of bits per character "
                   "in configuration file for the device.\n" );
			THROW( EXCEPTION );
    }

    smsmotor.tio->c_cflag |= CLOCAL | CREAD;
    smsmotor.tio->c_iflag = IGNBRK;
    smsmotor.tio->c_oflag = 0;
    smsmotor.tio->c_lflag = 0;

    cfsetispeed( smsmotor.tio, SERIAL_BAUDRATE );
    cfsetospeed( smsmotor.tio, SERIAL_BAUDRATE );

    /* We can't use canonical mode here... */

    smsmotor.tio->c_lflag = 0;

    fsc2_tcflush( smsmotor.sn, TCIOFLUSH );
    fsc2_tcsetattr( smsmotor.sn, TCSANOW, smsmotor.tio );

#endif

    smsmotor.is_open = SET;
}


/*-------------------------------------------------------*
 * Function for moving one of the axis to a new position
 *-------------------------------------------------------*/

static double
smsmotor_goto( long   dev,
			   double pos )
{
#ifndef SMSMOTOR_TEST
	char buf[ 40 ];
	char reply[ 30 ];
	long len;


	/* Send the command to move to the new position */

	sprintf( buf, "%.6f %ld nm ", pos, dev + 1 );
	len = strlen( buf );

	if ( fsc2_serial_write( smsmotor.sn, buf, len, WAIT_TIME, UNSET ) != len )
		smsmotor_fail( );

	/* Now wait until the device reports that the final position has been
	   reached, give the user a chance to interrupt */

	sprintf( buf, "%ld nst ", dev + 1 );
	len = strlen( buf );

	while ( 1 )
	{
		if (    fsc2_serial_write( smsmotor.sn, buf, len,
								   WAIT_TIME, UNSET ) != len
			 || fsc2_serial_read( smsmotor.sn, reply, 4, " \r\n",
								  WAIT_TIME, UNSET ) != 4 )
			smsmotor_fail( );

		if ( reply[ 0 ] == '0' )
			break;

		if ( check_user_request ( ) )
		{
			sprintf( buf, "%ld nabort ", dev + 1 );
			fsc2_serial_write( smsmotor.sn, buf, strlen( buf ),
							   WAIT_TIME, UNSET );
			THROW( USER_BREAK_EXCEPTION );
		}
	}

	/* Get the new position value from the device */

	sprintf( buf, "%ld npos ", dev + 1 );
	if (    fsc2_serial_write( smsmotor.sn, buf, strlen( buf ),
							   WAIT_TIME, UNSET ) <= 0
		 || ( len = fsc2_serial_read( smsmotor.sn, reply, sizeof reply,
                                      " \r\n", WAIT_TIME, UNSET ) ) < 2
		 || strncmp( reply + len - 3, " \r\n", 3 ) )
			smsmotor_fail( );

	reply[ len - 3 ] = '\0';
	return T_atod( reply );
#else
	return pos;
#endif
}


/*----------------------------------------------------*
 * Function for setting the maximum speed for an axis
 *----------------------------------------------------*/

static void
smsmotor_set_speed( long   dev,
                       double speed )
{
#ifndef SMSMOTOR_TEST
    char buf[ 30 ];


    sprintf( buf, "%f %ld snv ", speed, dev + 1 );
    if ( fsc2_serial_write( smsmotor.sn, buf, strlen( buf ),
                                    WAIT_TIME, UNSET ) <= 0 )
        smsmotor_fail( );
#endif
}


/*--------------------------------------------------------*
 * Function for determining the maximum speed for an axis
 *--------------------------------------------------------*/

static double
smsmotor_get_speed( long  dev )
{
#ifndef SMSMOTOR_TEST
    char buf[ 10 ],
         reply[ 40 ];
    long len = 0;


    sprintf( buf, "%ld gnv ", dev + 1 );
    if (    fsc2_serial_write( smsmotor.sn, buf, strlen( buf ),
                               WAIT_TIME, UNSET ) <= 0
         || ( len = fsc2_serial_read( smsmotor.sn, reply, sizeof reply,
                                      " \r\n", WAIT_TIME, UNSET ) ) <= 3
         || strncmp( reply + len - 3, " \r\n", 3 ) )
        smsmotor_fail( );

    reply[ len - 3 ] = '\0';
    return T_atod( reply );
#else
    return TEST_SPEED;
#endif
}


/*-----------------------------------------------------------*
 * Function for setting the maximum acceleration for an axis
 *-----------------------------------------------------------*/

static void
smsmotor_set_acceleration( long   dev,
                           double acceleration )
{
#ifndef SMSMOTOR_TEST
    char buf[ 30 ];


    sprintf( buf, "%f %ld sna ", acceleration, dev + 1 );
    if ( fsc2_serial_write( smsmotor.sn, buf, strlen( buf ),
                            WAIT_TIME, UNSET ) <= 0 )
        smsmotor_fail( );
#endif
}


/*---------------------------------------------------------------*
 * Function for determining the maximum acceleration for an axis
 *---------------------------------------------------------------*/

static double
smsmotor_get_acceleration( long  dev )
{
#ifndef SMSMOTOR_TEST
    char buf[ 10 ],
         reply[ 40 ];
    long len = 0;


    sprintf( buf, "%ld gna ", dev + 1 );
    if (    fsc2_serial_write( smsmotor.sn, buf, strlen( buf ),
                               WAIT_TIME, UNSET ) <= 0
         || ( len = fsc2_serial_read( smsmotor.sn, reply, sizeof reply,
                                      " \r\n", WAIT_TIME, UNSET ) ) <= 3
         || strncmp( reply + len - 3, " \r\n", 3 ) )
        smsmotor_fail( );

    reply[ len - 3 ] = '\0';
    return T_atod( reply );
#else
    return TEST_ACCELERATION;
#endif
}


/*-------------------------------------------*
 * Function called on communication failures 
 *-------------------------------------------*/

static void
smsmotor_fail( void )
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
