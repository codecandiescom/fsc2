/*
  $Id$
 
  Kernel (2.4 series) module for Wasco WITIO-48 DIO ISA cards

  Copyright (C) 2003 Jens Thoms Toerring

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the program; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "autoconf.h"
#include "witio_48_drv.h"


char kernel_version[ ] = UTS_RELEASE;


/* No symbols are exported by the module */

EXPORT_NO_SYMBOLS;


/* Local function declarations */

static int witio_48_get_ioport( void );
static int witio_48_open( struct inode *inode_p, struct file *file_p );
static int witio_48_release( struct inode *inode_p, struct file * file_p );
static int witio_48_ioctl( struct inode *inode_p, struct file *file_p,
			   unsigned int cmd, unsigned long arg );
static int witio_48_set_mode( WITIO_48_MODE *state );
static int witio_48_dio_out( WITIO_48_DATA *data );
static void witio_48_3x8_dio_out( WITIO_48_DATA *data );
static void witio_48_2x12_dio_out( WITIO_48_DATA *data );
static void witio_48_1x24_dio_out( WITIO_48_DATA *data );
static void witio_48_16_8_dio_out( WITIO_48_DATA *data );
static int witio_48_dio_in( WITIO_48_DATA *data );
static void witio_48_3x8_dio_in( WITIO_48_DATA *data );
static void witio_48_2x12_dio_in( WITIO_48_DATA *data );
static void witio_48_1x24_dio_in( WITIO_48_DATA *data );
static void witio_48_16_8_dio_in( WITIO_48_DATA *data );
static int witio_48_get_from_user( unsigned long arg, void *to, size_t len );
static void witio_48_set_crtl( int dio );
static void witio_48_board_out( unsigned char *addr, unsigned char byte );
static unsigned char witio_48_board_in( unsigned char *addr );


/* Global variables of the module */

static Board board;                              /* board structure */
static int major    = WITIO_48_MAJOR;            /* major device number */
static int base     = WITIO_48_BASE;             /* IO memory address */

static struct file_operations witio_48_file_ops = {
	owner:              THIS_MODULE,
	ioctl:              witio_48_ioctl,
	open:               witio_48_open,
	release:            witio_48_release,
};


/*------------------------------------*/
/* Function for module initialization */
/*------------------------------------*/

static int __init witio_48_init( void )
{
	int dynamic_major;
	int i, j;


	/* Register the board as a character device with the major address
	   either compiled into the driver or set while the module is loaded.
	   If the major number is 0 this means we must use a dynamically
	   assigned major number. */

	board.major = major;

	if ( ( dynamic_major = register_chrdev( board.major, "witio_48",
						&witio_48_file_ops ) ) < 0 ) {
		printk( KERN_ERR "witio_48: Can't register device major "
			"%d.\n", board.major );
		goto device_registration_failure;
	}

	if ( board.major == 0 ) {
		board.major = dynamic_major;
		printk( KERN_INFO "witio_48: Using dynamically assigned major "
			"device number of %d.\n", board.major );
	}

	/* Now try to get the IO-port region required for the board */

	board.base = ( unsigned char * ) base;

	if ( witio_48_get_ioport( ) )
		goto ioport_failure;

	board.in_use = 0;

	/* Deactivate all outputs by switching to mode 0 with all ports
	   being set as input ports. Also set up a default mode to 1x24
	   (two single 24-bit DIOs) and set the structure with the current
	   state accordingly. */

	for ( i = 0; i < 2; i++ )
	{
		board.states[ i ].mode = WITIO_48_MODE_1x24;
		for ( j = 0; j < 3; j++ )
		{
			board.states[ i ].state[ j ] = READ_ALL;
			board.states[ i ].out_value[ j ] = 0;
		}

		witio_48_set_crtl( i );
	}

	/* Finally tell the world about the successful installation ;-) */

	printk( KERN_INFO "witio_48: Major = %d\n", board.major );
	printk( KERN_INFO "witio_48: Base = 0x%lx\n",
		( unsigned long ) board.base );

	return 0;

 ioport_failure:
	unregister_chrdev( board.major, "witio_48" );

 device_registration_failure:
	printk( KERN_ERR "witio_48: Module installation failed.\n" );
	return -EIO;
}


/*---------------------------------------------------*/
/* Function for module cleanup when it gets unloaded */
/*---------------------------------------------------*/

static void __exit witio_48_cleanup( void )
{
	/* Release IO-port region reserved for the board */

	release_region( ( unsigned long ) board.base, 0x0BUL );

	if ( unregister_chrdev( board.major, "witio_48" ) != 0 )
		printk( KERN_ERR "witio_48: Device busy or other module "
			"error.\n" );
	else
		printk( KERN_INFO "witio_48: Module succesfully removed.\n" );
}


/*------------------------------------------------------*/
/* Function for requesting IO-port region for the board */
/* and setting up pointers to the boards registers      */
/*------------------------------------------------------*/

static int witio_48_get_ioport( void )
{
	unsigned char *base;
	int i, j;


	/* Port addresses can start at integer multiples of 16 in the range
	   between (and including) 0x200 and 0x300. Check if the requested
	   board IO-port addrress satisfies this condition. */

	for ( base = ( unsigned char * ) 0x200;
	      base <= ( unsigned char * ) 0x300; base += 0x10 )
		if ( base == board.base )
			break;

	if ( base > ( unsigned char * ) 0x300 ) {
		printk( KERN_ERR "witio_48: Can't get region at boards base "
			"address %p\n", board.base );
		return 1;
	}

	/* Try to obtain the IO-port region requested for the port, we need
	   11 bytes */

	if ( check_region( ( unsigned long ) base, 0x0BUL ) < 0 ) {
		printk( KERN_ERR "witio_48: Can't get region at boards base "
			"address %p\n", board.base );
		return 1;
	}

	/* Now lock this region - this always succeeds when the check
           succeeded */

	request_region( ( unsigned long ) base, 0x0BUL, "witio_48" );

	/* Finally set up the pointers to the registers on the board */

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 3; j++ )
			board.regs.port[ i ][ j ] = base++;
		board.regs.control[ i ] = base++;
	}

	for ( i = 0; i < 3; i++ )
		board.regs.counter[ i ] = base++;

	return 0;
}


/*----------------------------------------------------------*/
/* Function executed when device file for board gets opened */
/*----------------------------------------------------------*/

static int witio_48_open( struct inode *inode_p, struct file *file_p )
{
	file_p = file_p;
	inode_p = inode_p;

	spin_lock( &board.spinlock );

	if ( board.in_use &&
	     board.owner != current->uid &&
	     board.owner != current->euid ) {
		PDEBUG( "Board already in use by another user\n" );
		spin_unlock( &board.spinlock );
		return -EBUSY;
	}

	if ( ! board.in_use )
		board.owner = current->uid;

	board.in_use++;
	MOD_INC_USE_COUNT;

	spin_unlock( &board.spinlock );

	return 0;
}


/*----------------------------------------------------------*/
/* Function executed when device file for board gets closed */
/*----------------------------------------------------------*/

static int witio_48_release( struct inode *inode_p, struct file * file_p )
{
	file_p = file_p;
	inode_p = inode_p;

	spin_lock( &board.spinlock );

	board.in_use--;
	MOD_DEC_USE_COUNT;

	spin_unlock( &board.spinlock );

	return 0;
}


/*-------------------------------------------------------*/
/* Function for handling of ioctl() calls for the module */
/*-------------------------------------------------------*/

static int witio_48_ioctl( struct inode *inode_p, struct file *file_p,
			   unsigned int cmd, unsigned long arg )
{
	int ret_val = 0;
	WITIO_48_MODE state_struct;
	WITIO_48_DATA data_struct;


	file_p = file_p;
	inode_p = inode_p;

	if ( _IOC_TYPE( cmd ) != WITIO_48_MAGIC_IOC || 
	     _IOC_NR( cmd ) < WITIO_48_MIN_NR ||
	     _IOC_NR( cmd ) > WITIO_48_MAX_NR ) {
		PDEBUG( "Invalid ioctl() call %d\n", cmd );
		return -EINVAL;
	}

    switch ( cmd ) {
	    case WITIO_48_IOC_MODE :
		    if ( witio_48_get_from_user( arg, ( void * ) &state_struct,
						 sizeof state_struct ) ) 
			    return -EACCES;
		    ret_val = witio_48_set_mode( &state_struct );
		    break;

	    case WITIO_48_IOC_DIO_OUT :
		    if ( witio_48_get_from_user( arg, ( void * ) &data_struct,
						 sizeof data_struct ) )
			    return -EACCES;
		    ret_val = witio_48_dio_out( &data_struct );
		    break;

	    case WITIO_48_IOC_DIO_IN :
		    if ( witio_48_get_from_user( arg, ( void * ) &data_struct,
						 sizeof data_struct ) )
			    return -EACCES;
		    if ( ( ret_val = witio_48_dio_in( &data_struct ) ) != 0 )
			    break;
		    __copy_to_user( ( WITIO_48_DATA * ) arg, &data_struct,
				    sizeof( WITIO_48_DATA ) );
		    break;

	    default :                        /* we can never end up here... */
		    PDEBUG( "Invalid ioctl() call\n" );
		    ret_val = -EINVAL;
    }

    return ret_val;
}


/*-----------------------------------------------------*/
/* Function for copying data from user to kernel space */
/*-----------------------------------------------------*/

static int witio_48_get_from_user( unsigned long arg, void *to, size_t len )
{
	if ( access_ok( VERIFY_READ, ( void * ) arg, len ) )
		__copy_from_user( to, ( void * ) arg, len );
	else
		return -EACCES;

	return 0;
}


/*-----------------------------------*/
/* Function for setting the I/O-mode */
/*-----------------------------------*/

static int witio_48_set_mode( WITIO_48_MODE *state )
{
	/* Check if the DIO number is valid */

	if ( state->dio < WITIO_48_DIO_1 || state->dio > WITIO_48_DIO_2 )
		return -EINVAL;

	/* Check if the mode is valid */

	if ( state->mode < WITIO_48_MODE_3x8 ||
	     state->mode > WITIO_48_MODE_16_8 )
		return -EINVAL;

	board.states[ state->dio ].mode = state->mode;

	return 0;
}


/*------------------------------------------------------------------.*/
/* Function for outputting values, according to the current I/O-mode */
/*-------------------------------------------------------------------*/

static int witio_48_dio_out( WITIO_48_DATA *data )
{
	int dio = data->dio;
	int ch  = data->channel;


	/* Check if the DIO number is valid */

	if ( dio < WITIO_48_DIO_1 || dio > WITIO_48_DIO_2 )
		return -EINVAL;

	/* Check if the channel number is valid */

	if ( ch < WITIO_48_CHANNEL_0 || ch > WITIO_48_CHANNEL_2 )
		return -EINVAL;

	/* Check if the channel number isn't too large for the mode of
	   the selected DIO */

	if ( ( board.states[ dio ].mode == WITIO_48_MODE_3x8 &&
	       ch > WITIO_48_CHANNEL_0 ) ||
	     ( ( board.states[ dio ].mode == WITIO_48_MODE_2x12 ||
		 board.states[ dio ].mode == WITIO_48_MODE_16_8 ) &&
	       ch > WITIO_48_CHANNEL_1 ) )
		return -EINVAL;

	/* Now write to the relevant write buffer registers and, if necessary,
	   to the control registers if the direction changed (i.e. the port
	   (or parts of it) had been used for input before) */

	switch ( board.states[ dio ].mode )
	{
		case WITIO_48_MODE_3x8 :
			witio_48_3x8_dio_out( data );
			break;

		case WITIO_48_MODE_2x12 :
			witio_48_2x12_dio_out( data );
			break;

		case WITIO_48_MODE_1x24 :
			witio_48_1x24_dio_out( data );
			break;

		case WITIO_48_MODE_16_8 :
			witio_48_16_8_dio_out( data );
			break;
	}

	return 0;
}


/*-----------------------------------------------------------------*/
/* Function for writing out an 8-bit value to one of the ports. If */
/* the channel argument is 0 the value is written to port A, for   */
/* 1 to port B and when the channel argument is 2 the value is     */
/* output at port C.                                               */
/*-----------------------------------------------------------------*/

static void witio_48_3x8_dio_out( WITIO_48_DATA *data )
{
	int dio = data->dio;
	int ch  = data->channel;


	witio_48_board_out( board.regs.port[ dio ][ ch ], data->value & 0xFF );
	board.states[ dio ].out_value[ ch ] = data->value & 0xFF;

	if ( board.states[ dio ].state[ ch ] != WRITE_ALL )
	{
		board.states[ dio ].state[ ch ] = WRITE_ALL;
		witio_48_set_crtl( dio );
	}
}


/*--------------------------------------------------------------------*/
/* Function for outputting a 12-bit value. If the channel argument is */
/* 0 the lower 8 bits of the value are send to port A and its upper   */
/* 4 bits are output at the upper nibble of port C. For a channel     */
/* argument of 1 the lower 8 bits of the value appear at port B and   */
/* the remaining upper 4 bits at the lower nibble of port C.          */
/*--------------------------------------------------------------------*/

static void witio_48_2x12_dio_out( WITIO_48_DATA *data )
{
	int dio = data->dio;
	int ch  = data->channel;
	unsigned char out = 0;
	int need_crtl_update = 0;


	/* Output the lower 8 bits at the approproate register */

	witio_48_board_out( board.regs.port[ dio ][ ch ],
			    data->value & 0xFF );
	board.states[ dio ].out_value[ ch ] = data->value & 0xFF;

	if ( board.states[ dio ].state[ ch ] != READ_ALL )
	{
		need_crtl_update = 1;
		board.states[ dio ].state[ ch ] = READ_ALL;
	}

	/* Output the upper 4 bits of the value at the appropriate nibble of
	   port C, taking care not to disturb the other nibble */

	if ( ch == WITIO_48_CHANNEL_0 )
		out = ( ( data->value >> 4 ) & 0xF0 ) |
			( board.states[ dio ].out_value[ C ] & 0x0F );
	else
		out = ( ( data->value >> 8 ) & 0x0F ) |
			( board.states[ dio ].out_value[ C ] & 0xF0 );

	witio_48_board_out( board.regs.port[ dio ][ C ], out );
	board.states[ dio ].out_value[ C ] = out;

	/* If necessary set the control port to output the value */

	if ( board.states[ dio ].state[ ch ] != WRITE_ALL )
	{
		board.states[ dio ].state[ ch ] = WRITE_ALL;
		witio_48_set_crtl( dio );
	}
		
	if ( ch == WITIO_48_CHANNEL_0 &&
	     board.states[ dio ].state[ C ] & READ_UPPER )
	{
		need_crtl_update = 1;
		board.states[ dio ].state[ C ] ^= READ_UPPER;
	}
	else if ( ch == WITIO_48_CHANNEL_1 &&
		  board.states[ dio ].state[ C ] & READ_LOWER )
	{
		need_crtl_update = 1;
		board.states[ dio ].state[ C ] ^= READ_LOWER;
	}

	if ( need_crtl_update )
		witio_48_set_crtl( dio );
}


/*---------------------------------------------------------------------*/
/* Function for writing out a 24-bit value. The highest byte is output */
/* at port C, the middle byte at port B and the lowest byte at port A. */
/*---------------------------------------------------------------------*/

static void witio_48_1x24_dio_out( WITIO_48_DATA *data )
{
	int dio = data->dio;
	unsigned long val = data->value;
	int i;
	int need_crtl_update = 0;


	for ( i = 0; i < 3; val >>= 8, i++ )
	{
		witio_48_board_out( board.regs.port[ dio ][ i ], val & 0xFF );
		board.states[ dio ].out_value[ i ] = val & 0xFF;

		if ( board.states[ dio ].state[ i ] != WRITE_ALL )
		{
			need_crtl_update = 1;
			board.states[ dio ].state[ i ] = WRITE_ALL;
		}
	}

	if ( need_crtl_update )
		witio_48_set_crtl( dio );
}


/*---------------------------------------------------------------------*/
/* Function for writing out either a 16-bit or an 8-bit value. If the  */
/* channel argument is 0 a 16-bit value is output, with the upper byte */
/* appearing at port B and the lower byte at port A. For a channel     */
/* argument an 8-bit value is written to port C.                       */
/*---------------------------------------------------------------------*/

static void witio_48_16_8_dio_out( WITIO_48_DATA *data )
{
	int dio = data->dio;
	int ch  = data->channel;
	unsigned long val = data->value;
	int i;
	int need_crtl_update = 0;


	if ( ch == WITIO_48_CHANNEL_0 )
	{
		for ( i = 0; i < 2; val >>= 8, i++ )
		{
			witio_48_board_out( board.regs.port[ dio ][ i ],
					    val & 0xFF );
			board.states[ dio ].out_value[ i ] = val & 0xFF;
			if ( board.states[ dio ].state[ i ] != WRITE_ALL )
			{
				need_crtl_update = 1;
				board.states[ dio ].state[ i ] = WRITE_ALL;
			}
		}
	}
	else
	{
		witio_48_board_out( board.regs.port[ dio ][ C ], val & 0xFF );
		board.states[ dio ].out_value[ C ] = val & 0xFF;

		if ( board.states[ dio ].state[ C ] != WRITE_ALL )
		{
			need_crtl_update = 1;
			board.states[ dio ].state[ C ] = WRITE_ALL;
		}
	}

	if ( need_crtl_update )
		witio_48_set_crtl( dio );
}


/*-------------------------------------------------------------------*/
/* Function for reading in values, according to the current I/O-mode */
/*-------------------------------------------------------------------*/

static int witio_48_dio_in( WITIO_48_DATA *data )
{
	int dio = data->dio;
	int ch  = data->channel;


	/* Check if the DIO number is valid */

	if ( dio < WITIO_48_DIO_1 || dio > WITIO_48_DIO_2 )
		return -EINVAL;

	/* Check if the channel number is valid */

	if ( ch < WITIO_48_CHANNEL_0 || ch > WITIO_48_CHANNEL_2 )
		return -EINVAL;

	/* Check if the channel number isn't too large for the mode of
	   the selected DIO */

	if ( ( board.states[ dio ].mode == WITIO_48_MODE_3x8 &&
	       ch > WITIO_48_CHANNEL_0 ) ||
	     ( ( board.states[ dio ].mode == WITIO_48_MODE_2x12 ||
		 board.states[ dio ].mode == WITIO_48_MODE_16_8 ) &&
	       ch > WITIO_48_CHANNEL_1 ) )
		return -EINVAL;

	/* Now write to the relevant write buffer registers and, if necessary,
	   to the control registers if the direction changed (i.e. the port
	   (or parts of it) had been used for input before) */

	switch ( board.states[ dio ].mode )
	{
		case WITIO_48_MODE_3x8 :
			witio_48_3x8_dio_in( data );
			break;

		case WITIO_48_MODE_2x12 :
			witio_48_2x12_dio_in( data );
			break;

		case WITIO_48_MODE_1x24 :
			witio_48_1x24_dio_in( data );
			break;

		case WITIO_48_MODE_16_8 :
			witio_48_16_8_dio_in( data );
			break;
	}

	return 0;
}


/*-------------------------------------------------------------------*/
/* Function for reading in an 8-bit value from of the three channels */
/* when in 3x8 mode. If the channel argument is 0 the value comes    */
/* from port A, for 1 the value is from port B and for the channel   */
/* argument set to 2 the value is what gets read in from port C.     */
/*-------------------------------------------------------------------*/

static void witio_48_3x8_dio_in( WITIO_48_DATA *data )
{
	int dio = data->dio;
	int ch  = data->channel;


	if ( board.states[ dio ].state[ ch ] != READ_ALL )
	{
		board.states[ dio ].state[ ch ] = READ_ALL;
		witio_48_set_crtl( dio );
	}

	data->value = witio_48_board_in( board.regs.port[ dio ][ ch ] );
}


/*----------------------------------------------------------------*/
/* Function for reading in a 12-bit value. When the channel is 0  */
/* data from port A make up the lower 8 bits of the value and the */
/* upper nibble from prot C the 4 higher bits. For channel 1 the  */
/* lower 8 bits of the value come from port B while its higher 4  */
/* bits are the lower nibble of port C.                           */
/*----------------------------------------------------------------*/

static void witio_48_2x12_dio_in( WITIO_48_DATA *data )
{
	int dio = data->dio;
	int ch  = data->channel;
	int need_crtl_update = 0;


	/* If necessary set up the control port to allow reading in the
	   data */

	if ( board.states[ dio ].state[ ch ] != READ_ALL )
	{
		need_crtl_update = 1;
		board.states[ dio ].state[ ch ] = READ_ALL;
	}

	if ( ch == WITIO_48_CHANNEL_0 &&
	     ! ( board.states[ dio ].state[ C ] & READ_UPPER ) )
	{
		need_crtl_update = 1;
		board.states[ dio ].state[ C ] |= READ_UPPER;
	}
	else if ( ch == WITIO_48_CHANNEL_1 &&
		  ! ( board.states[ dio ].state[ C ] & READ_LOWER ) )
	{
		need_crtl_update = 1;
		board.states[ dio ].state[ C ] |= READ_LOWER;
	}

	if ( need_crtl_update )
		witio_48_set_crtl( dio );

	if ( ch == WITIO_48_CHANNEL_0 )
		data->value = 0x0F00 &
			( witio_48_board_in( board.regs.port[ dio ][ C ] ) 
			  << ( ch == WITIO_48_CHANNEL_0 ?  4 : 8 ) );

	data->value |= witio_48_board_in( board.regs.port[ dio ][ ch ] );
}


/*--------------------------------------------------------------*/
/* Function for reading in a 24-bit value. The higest 8 bits of */
/* the value come from port C, the middle 8 bits from port B    */
/* and the lowest 8 bits from port A.                           */
/*--------------------------------------------------------------*/

static void witio_48_1x24_dio_in( WITIO_48_DATA *data )
{
	int dio = data->dio;
	int i;
	int need_crtl_update = 0;


	for ( i = 2; i >= 0; i++, data->value <<= 8 )
		if ( board.states[ dio ].state[ i ] != READ_ALL )
		{
			need_crtl_update = 1;
			board.states[ dio ].state[ i ] = READ_ALL;
		}

	if (  need_crtl_update )
		witio_48_set_crtl( dio );

	for ( data->value = 0, i = 2; i >= 0; i++, data->value <<= 8 )
		data->value |=
			witio_48_board_in( board.regs.port[ dio ][ i ] );
}


/*------------------------------------------------------------*/
/* Function for reading in either a 16-bit or an 8-bit value. */
/* When the channel argument is 0 a 16-bit valueis read in    */
/* with the upper byte comming from port B and the lower byte */
/* from port A. When channel is 1 an 8-bit value is read in   */
/* from port C.                                               */
/*------------------------------------------------------------*/

static void witio_48_16_8_dio_in( WITIO_48_DATA *data )
{
	int dio = data->dio;
	int ch  = data->channel;
	int i;
	int need_crtl_update = 0;


	if ( ch == WITIO_48_CHANNEL_0 )
	{
		for ( i = 1; i >= 0; i++, data->value <<= 8 )
			if ( board.states[ dio ].state[ i ] != READ_ALL )
			{
				need_crtl_update = 1;
				board.states[ dio ].state[ i ] = READ_ALL;
			}

		if (  need_crtl_update )
			witio_48_set_crtl( dio );

		for ( data->value = 0, i = 1; i >= 0; i++, data->value <<= 8 )
			data->value |=
			      witio_48_board_in( board.regs.port[ dio ][ i ] );
	}
	else
	{
		if ( board.states[ dio ].state[ C ] != READ_ALL )
		{
			board.states[ dio ].state[ C ] = READ_ALL;
			witio_48_set_crtl( dio );
		}

		data->value = witio_48_board_in( board.regs.port[ dio ][ C ] );
	}
}


/*--------------------------------------------------------------*/
/* Calculates the value that needs to be written to the control */
/* register for one of the DIOs and writes it to the register.  */
/*--------------------------------------------------------------*/

#define READ_A        0x10
#define READ_B        0x02
#define READ_C_UPPER  0x04
#define READ_C_LOWER  0x01

static void witio_48_set_crtl( int dio )
{
	unsigned char crtl = 0x80;


	if ( board.states[ dio ].state[ A ] == READ_ALL )
		crtl |= READ_A;

	if ( board.states[ dio ].state[ B ] == READ_ALL )
		crtl |= READ_B;

	if ( board.states[ dio ].state[ C ] & READ_UPPER )
		crtl |= READ_C_UPPER;

	if ( board.states[ dio ].state[ C ] & READ_LOWER )
		crtl |= READ_C_LOWER;

	witio_48_board_out( board.regs.control[ dio ], crtl );
}


/*------------------------------------------------------------------*/
/* Outputs one byte to the register addressed by the first argument */
/*------------------------------------------------------------------*/

static void witio_48_board_out( unsigned char *addr, unsigned char byte )
{
    outb_p( byte, ( PORT ) addr );
}


/*-------------------------------------------------------------*/
/* Inputs one byte from the register addressed by the argument */
/*-------------------------------------------------------------*/

static unsigned char witio_48_board_in( unsigned char *addr )
{
    return inb_p( ( PORT ) addr );
}


module_init( witio_48_init );
module_exit( witio_48_cleanup );


/* Module options */

MODULE_PARM( base, "i" );
MODULE_PARM_DESC( base, "Base address" );
MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number (either 1-254 or 0 to use "
                 "dynamically assigned number)" );
MODULE_AUTHOR( "Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>" );
MODULE_DESCRIPTION( "Driver for Wasco WITIO-48 DIO card" );


/* License of module */

#if defined MODULE_LICENSE
MODULE_LICENSE( "GPL" );
#endif


/*
 * Local variables:
 * c-basic-offset: 8
 * c-indent-level: 8
 * c-brace-imaginary-offset: 0
 * c-brace-offset: 0
 * c-argdecl-indent: 4
 * c-label-ofset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: t
 * tab-width: 8
 * End:
 */
