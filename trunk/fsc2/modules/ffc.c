/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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

#include "ffc.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Exported functions */

int ffc_init_hook( void );
int ffc_exp_hook( void );
int ffc_end_of_exp_hook( void );

Var_T * freq_contr_name( Var_T * v );
Var_T * freq_contr_change_freq( Var_T * v );


/* Local functions */

static void ffc_init( void );
static void ffc_change( int dir );
static void ffc_open( void );
static void ffc_comm_fail( void );


static struct {
    int              sn;
	bool             is_open;
    struct termios * tio;         /* serial port terminal interface structure */
} ffc;

#define RESPONSE_TIME          50000    /* 50 ms (for "normal commands) */

enum {
	DOWN  = 0,
	UP
};


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
ffc_init_hook( void )
{
    /* Claim the serial port (throws exception on failure) */

    ffc.sn = fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	ffc.is_open = UNSET;

	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
ffc_exp_hook( void )
{
	ffc_init( );
	return 1;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
ffc_end_of_exp_hook( void )
{
    if ( ffc.is_open )
    {
        fsc2_serial_close( ffc.sn );
        ffc.is_open = UNSET;
    }

	return 1;
}


/*-------------------------------*
 * Returns device name as string 
 *-------------------------------*/

Var_T *
freq_contr_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

Var_T *
freq_contr_change_freq( Var_T * v )
{
	int dir;


	if ( v->type == STR_VAR )
	{
		if ( ! strcasecmp( v->val.sptr, "UP" ) )
			dir = UP;
		else if ( ! strcasecmp( v->val.sptr, "DOWN" ) )
			dir = DOWN;
		else
		{
			print( FATAL, "Invalid argument.\n" );
		THROW( EXCEPTION );
		}
	}
	else
		dir = get_boolean( v );

	if ( FSC2_MODE == EXPERIMENT )
		ffc_change( dir );


    return vars_push( INT_VAR, ( long ) dir );
}


/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/

static void
ffc_init( void )
{
	ffc_open( );
}


/*-----------------------------------------*
 *-----------------------------------------*/

static void
ffc_change( int dir )
{
	char data = dir ? 'p' : 'm';


	if (    fsc2_serial_write( ffc.sn, &data, 1, RESPONSE_TIME, UNSET ) != 1
		 || fsc2_serial_read( ffc.sn, &data, 1, NULL,
                              RESPONSE_TIME, UNSET ) != 1
		 || data != dir ? 'p' : 'm' )
		ffc_comm_fail( );
}


/*-----------------------------------------------------*
 * Function for opening the device file for the device
 * and setting up the communication parameters.
 *-----------------------------------------------------*/

static void
ffc_open( void )
{
#ifndef FFC_TEST

    /* We need exclusive access to the serial port and we also need non-
       blocking mode to avoid hanging indefinitely if the other side does not
       react. O_NOCTTY is set because the serial port should not become the
       controlling terminal, otherwise line noise read as a CTRL-C might kill
       the program. */

    if ( ( ffc.tio = fsc2_serial_open( ffc.sn,
                          O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
    {
        print( FATAL, "Can't open device file for power supply.\n" );
        THROW( EXCEPTION );
    }

    ffc.tio->c_cflag = 0;

    switch ( PARITY_TYPE )
    {
        case NO_PARITY :
            break;

        case ODD_PARITY :
            ffc.tio->c_cflag |= PARENB | PARODD;
            break;

        case EVEN_PARITY :
            ffc.tio->c_cflag |= PARENB;
            break;

        default :
            fsc2_serial_close( ffc.sn );
            print( FATAL, "Invalid setting for parity bit in "
                   "configuration file for the device.\n" );
            THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_STOP_BITS )
    {
        case 1 :
            break;

        case 2 :
            ffc.tio->c_cflag |= CSTOPB;
            break;

        default :
            fsc2_serial_close( ffc.sn );
            print( FATAL, "Invalid setting for number of stop bits in "
                   "configuration file for the device.\n" );
            THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_BITS_PER_CHARACTER )
    {
        case 5 :
            ffc.tio->c_cflag |= CS5;
            break;

        case 6 :
            ffc.tio->c_cflag |= CS6;
            break;

        case 7 :
            ffc.tio->c_cflag |= CS7;
            break;

        case 8 :
            ffc.tio->c_cflag |= CS8;
            break;

        default :
            fsc2_serial_close( ffc.sn );
            print( FATAL, "Invalid setting for number of bits per "
                   "in character configuration file for the device.\n" );
			THROW( EXCEPTION );
    }

    ffc.tio->c_cflag |= CLOCAL | CREAD;
    ffc.tio->c_iflag = IGNBRK;
    ffc.tio->c_oflag = 0;
    ffc.tio->c_lflag = 0;

    cfsetispeed( ffc.tio, SERIAL_BAUDRATE );
    cfsetospeed( ffc.tio, SERIAL_BAUDRATE );

    /* We can't use canonical mode here... */

    ffc.tio->c_lflag = 0;

    fsc2_tcflush( ffc.sn, TCIOFLUSH );
    fsc2_tcsetattr( ffc.sn, TCSANOW, ffc.tio );
#endif

    ffc.is_open = SET;
}


/*-------------------------------------------------------*
 * Function to handle situations where the communication
 * with the device failed completely.
 *-------------------------------------------------------*/

static void
ffc_comm_fail( void )
{
	print( FATAL, "Can't access the power supply.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
