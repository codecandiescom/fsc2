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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "bvt3000.h"


extern BVT3000 bvt3000;


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
bvt3000_init( void )
{
    double sp;


	/* Try to open the device file */

    if ( ( bvt3000.tio = fsc2_serial_open( bvt3000.sn,
						   O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
    {
        print( FATAL, "Can't open device file for device.\n" );
        THROW( EXCEPTION );
    }

	/* Set up communication parameters */

    bvt3000.tio->c_cflag = 0;

    switch ( PARITY_TYPE )
    {
        case NO_PARITY :
            break;

        case ODD_PARITY :
            bvt3000.tio->c_cflag |= PARENB | PARODD;
            break;

        case EVEN_PARITY :
            bvt3000.tio->c_cflag |= PARENB;
            break;

        default :
            fsc2_serial_close( bvt3000.sn );
            print( FATAL, "Invalid setting for parity bit in "
                   "configuration file for the device.\n" );
            THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_STOP_BITS )
    {
        case 1 :
            break;

        case 2 :
            bvt3000.tio->c_cflag |= CSTOPB;
            break;

        default :
            fsc2_serial_close( bvt3000.sn );
            print( FATAL, "Invalid setting for number of stop bits in "
                   "configuration file for the device.\n" );
            THROW( EXCEPTION );
    }

    switch ( NUMBER_OF_BITS_PER_CHARACTER )
    {
        case 5 :
            bvt3000.tio->c_cflag |= CS5;
            break;

        case 6 :
            bvt3000.tio->c_cflag |= CS6;
            break;

        case 7 :
            bvt3000.tio->c_cflag |= CS7;
            break;

        case 8 :
            bvt3000.tio->c_cflag |= CS8;
            break;

        default :
            fsc2_serial_close( bvt3000.sn );
            print( FATAL, "Invalid setting for number of bits per character "
                   "in configuration file for the device.\n" );
			THROW( EXCEPTION );
    }

    bvt3000.tio->c_cflag |= CLOCAL | CREAD;
    bvt3000.tio->c_iflag = IGNBRK;
    bvt3000.tio->c_oflag = 0;
    bvt3000.tio->c_lflag = 0;

    cfsetispeed( bvt3000.tio, SERIAL_BAUDRATE );
    cfsetospeed( bvt3000.tio, SERIAL_BAUDRATE );

    /* We can't use canonical mode here... */

    bvt3000.tio->c_lflag = 0;

    fsc2_tcflush( bvt3000.sn, TCIOFLUSH );
    fsc2_tcsetattr( bvt3000.sn, TCSANOW, bvt3000.tio );

    bvt3000.is_open = SET;

    /* Check that during the test run no out of range setpoint was requested */

    sp = eurotherm902s_get_min_setpoint( );
    if ( bvt3000.min_setpoint < sp )
    {
        print( FATAL, "During test run a setpoint of %f K was requested "
               "which is below the lowest configured setpoint of %f K.\n",
               bvt3000.min_setpoint, sp );
        THROW( EXCEPTION );
    }
    bvt3000.min_setpoint = sp;

    sp = eurotherm902s_get_max_setpoint( );
    if ( bvt3000.max_setpoint > sp )
    {
        print( FATAL, "During test run a setpoint of %f K was requested "
               "which is above the highest configured setpoint of %f K.\n",
               bvt3000.max_setpoint, sp );
        THROW( EXCEPTION );
    }
    bvt3000.max_setpoint = sp;

    /* Check if the Eurotherm 902S detected a sensor break */

    if ( eurotherm902s_check_sensor_break( ) )
    {
        print( FATAL, "No temperature sensor connected or sensor broken.\n" );
        THROW( EXCEPTION );
    }
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
bvt3000_set_flow_rate( unsigned int fr )
{
    char buf[ 20 ];


    fsc2_assert( fr <= 0x0F );

    sprintf( buf, "AF>%c%c%c%c", fr & 0x8 ? '1' : '0',
                                 fr & 0x4 ? '1' : '0',
                                 fr & 0x2 ? '1' : '0',
                                 fr & 0x1 ? '1' : '0' );
    bvt3000_send_command( buf );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

unsigned int
bvt3000_get_flow_rate( void )
{
    unsigned int fr = 0;
    char *reply = bvt3000_query( "AF" );
    unsigned int i;


    if ( *reply++ != '>' )
        bvt3000_comm_fail( );

    for ( i = 1; i < 5; reply++, i++ )
    {
        if ( *reply != '1' && *reply != '0' )
            bvt3000_comm_fail( );

        if ( *reply == '1' )
            fr |= 1 << ( 4 - i );
    }

    return fr;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void
bvt3000_set_heater_state( bool state )
{
    char buf[ 4 ] = "HP1";


    if ( state == UNSET )
        buf[ 2 ] = '0';
    bvt3000_send_command( buf );
}
    

/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

bool
bvt3000_get_heater_state( void )
{
    char *reply = bvt3000_query( "HP" );


    if ( reply[ 0 ] != '1' || reply[ 0 ] != '0' )
        bvt3000_comm_fail( );
    return reply[ 0 ] != '1' ? SET : UNSET;
}
   

/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

unsigned char
bvt300_get_port( int port )
{
    char buf[ ] = "P0";
    unsigned int x;
    char *reply;


    fsc2_assert( port >= 1 && port <= 4 );
    buf[ 1 ] += port;
    reply = bvt3000_query( buf );
    if (    reply[ 0 ] != '>'
         || sscanf( reply + 1, "%x", &x ) != 1
         || x > 0x0F )
        bvt3000_comm_fail( );
    return ( unsigned char ) x;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

unsigned int
bvt3000_get_interface_status( void )
{
    unsigned int is;
    char *reply = bvt3000_query( "IS" );


    if (    reply[ 0 ] != '>'
         || sscanf( reply + 1, "%x", &is ) != 1
         || is > 0x07FF
         || is & 0x0002
         || ! ( is & 0x0200 ) )
        bvt3000_comm_fail( );
    return is;
}


/*---------------------------------------------------------------*
 * Sends an query to the device. The command must consist of a
 * 2 character string and the function returns the data returned
 * by te device as a '\0'-terminated string.
 *---------------------------------------------------------------*/

char *
bvt3000_query( const char * cmd )
{
	static char buf[ 100 ];
	unsigned char bc;
	ssize_t len;


	fsc2_assert( cmd[ 2 ] == '\0' );

	/* Assemble string to be send */

	len = sprintf( buf, "%c%02d%02d%s%c", EOT, GROUP_ID, DEVICE_ID, cmd, ENQ );

	/* Send string and read and analyze response. The response must start
	   with STX, followed by the 2-char command, then data and finally an
	   ETX and the BCC (block check character) gets send. */

    if (    fsc2_serial_write( bvt3000.sn, buf, len,
						      SERIAL_WAIT, UNSET ) != len
         || ( len = fsc2_serial_read( bvt3000.sn, buf, sizeof buf - 1, NULL,
									  SERIAL_WAIT, UNSET ) ) < 5
		 || len == sizeof buf - 1                        /* reply too long */
		 || buf[ 0 ] != STX                              /* missing STX */
         || buf[ len - 2 ] != ETX                        /* missing ETX */
         || strncmp( buf + 1, cmd, 2 ) )                 /* wrong command */
        bvt3000_comm_fail( );

    bc = buf[ len - 1 ];
    buf[ len - 1 ] = '\0';

    if ( ! bvt3000_check_bcc( ( unsigned char * ) ( buf + 1 ), bc ) )
        bvt3000_comm_fail( );

	/* Return just the data as a '\0'-terminated string */

    buf[ len - 2 ] = '\0';
	return buf + 3;
}


/*-------------------------------*
 * Sends a command to the device
 *-------------------------------*/

void
bvt3000_send_command( const char * cmd )
{
	char buf[ 100 ];
	ssize_t len;


	/* Assemble string to be send */

	sprintf( buf, "%c%02d%02d%c%s%c",
             EOT, GROUP_ID, DEVICE_ID, STX, cmd, ETX );
	len = bvt3000_add_bcc( ( unsigned char * ) buf + 6 ) + 6;

	/* Send string and check for ACK */

	if (    fsc2_serial_write( bvt3000.sn, buf, len,
							   SERIAL_WAIT, UNSET ) != len
		 || ! bvt3000_check_ack( ) )
		bvt3000_comm_fail( );
}	 


/*-------------------------------------------------*
 * Tries to read a single ACK char from the device
 *-------------------------------------------------*/

bool
bvt3000_check_ack( void )
{
	unsigned char r;

	return    fsc2_serial_read( bvt3000.sn, &r, 1, NULL, ACK_WAIT, UNSET ) == 1
		   && r == ACK;
}


/*-------------------------------------------------------------*
 * Adds BCC (block check character) to the end of the command.
 * The BCC is calculated by doing a XOR on all bytes. The
 * function returns the length.
 *-------------------------------------------------------------*/

size_t
bvt3000_add_bcc( unsigned char * data )
{
	size_t len = 2;
	unsigned char bcc = *data++;


    while ( *data )
	{
		bcc ^= *data++;
		len++;
	}

	/* Append the resulting BCC and return the total length */

	*data = bcc;
	return len;
}


/*------------------------------------------------------------------*
 * Tests if the BCC (block check character) for a string is correct
 *------------------------------------------------------------------*/

bool
bvt3000_check_bcc( unsigned char * data,
						 unsigned char   bcc )
{
	while ( *data != ETX )
		bcc ^= *data++;

	return bcc == ETX;
}


/*------------------------------------------*
 *------------------------------------------*/

void
bvt3000_comm_fail( void )
{
	print( FATAL, "Communication failure.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
