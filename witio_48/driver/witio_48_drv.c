/*
 *  Kernel module for Wasco WITIO-48 DIO ISA cards
 * 
 *  Copyright (C) 2003-2014  Jens Thoms Toerring
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with the program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 *  To contact the author send email to:  jt@toerring.de
 */


#include "autoconf.h"
#include "witio_48_drv.h"

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 19 )
#include <linux/config.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )

#if defined( CONFIG_MODVERSIONS ) && ! defined( MODVERSIONS )
#define MODVERSIONS
#endif


#if defined( MODVERSIONS )
#include <linux/modversions.h>
#endif

#define __NO_VERSION__
#include <linux/module.h>

#else

#include <linux/module.h>

#endif


#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/device.h>
#else
#define __user
#endif


/* The following two macros are removed since kernel 2.6.37, but we use them */

#ifndef init_MUTEX
# define init_MUTEX( sem ) sema_init( sem, 1 )
#endif
#ifndef init_MUTEX_LOCKED
# define init_MUTEX_LOCKED( sem ) sema_init( sem, 0 )
#endif


#define PORT unsigned long


#define A 0
#define B 1
#define C 2


#define WRITE_ALL   0
#define READ_LOWER  1
#define READ_UPPER  2
#define READ_ALL    3


typedef struct Board Board;
typedef struct Registers Registers;
typedef struct State State;

struct Registers {
	unsigned long port[ 2 ][ 3 ];
	unsigned long control[ 2 ];
	unsigned long counter[ 3 ];
};


struct State {
	WITIO_48_MODE mode;
	unsigned char state[ 3 ];
	unsigned char out_value[ 3 ];
};


struct Board {
	int major;
	unsigned long base;
	int in_use;
	uid_t owner;
	struct semaphore open_mutex;
	struct semaphore use_mutex;
	Registers regs;
	State states[ 2 ];
};


#if defined WITIO_48_DEBUG
#define PDEBUG( fmt, args... )                           \
	do {                                             \
        	printk( KERN_DEBUG " WITIO_48: %s(): "   \
			fmt, __FUNCTION__ , ## args );   \
	} while( 0 )
#else
#define PDEBUG( ftm, args... )
#endif


/* Local function declarations */

static int witio_48_get_ioport( void );

static int witio_48_open( struct inode * inode_p,
			  struct file *  file_p );

static int witio_48_release( struct inode * inode_p,
			     struct file *  file_p );

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
static int witio_48_ioctl( struct inode * inode_p,
			   struct file *  file_p,
			   unsigned int   cmd,
			   unsigned long  arg );
#else
static long witio_48_ioctl( struct file *  file_p,
			    unsigned int   cmd,
			    unsigned long  arg );
#endif

static int witio_48_set_mode( WITIO_48_DIO_MODE * state );

static int witio_48_get_mode( WITIO_48_DIO_MODE * state );

static int witio_48_dio_out( WITIO_48_DATA * data );

static void witio_48_3x8_dio_out( WITIO_48_DATA * data );

static void witio_48_2x12_dio_out( WITIO_48_DATA * data );

static void witio_48_1x24_dio_out( WITIO_48_DATA * data );

static void witio_48_16_8_dio_out( WITIO_48_DATA * data );

static int witio_48_dio_in( WITIO_48_DATA * data );

static void witio_48_3x8_dio_in( WITIO_48_DATA * data );

static void witio_48_2x12_dio_in( WITIO_48_DATA * data );

static void witio_48_1x24_dio_in( WITIO_48_DATA * data );

static void witio_48_16_8_dio_in( WITIO_48_DATA * data );

static void witio_48_set_crtl( int dio );

static void witio_48_board_out( unsigned long addr,
				unsigned char byte );

static unsigned char witio_48_board_in( unsigned long addr );


/* Global variables of the module */

static Board board;                              /* board structure */
static int major    = WITIO_48_MAJOR;            /* major device number */
static int base     = WITIO_48_BASE;             /* IO memory address */

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static struct file_operations witio_48_file_ops = {
	.owner =            THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 11 )
	.unlocked_ioctl =   witio_48_ioctl,
#else
	.ioctl =            witio_48_ioctl,
#endif
	.open =             witio_48_open,
	.release =          witio_48_release
};


static dev_t dev_no;
struct cdev ch_dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 13 )
static struct class *witio_48_class;
#endif

#else
static struct file_operations witio_48_file_ops = {
	owner:              THIS_MODULE,
	ioctl:              witio_48_ioctl,
	open:               witio_48_open,
	release:            witio_48_release
};
#endif


/*------------------------------------*
 * Function for module initialization
 *------------------------------------*/

static int __init witio_48_init( void )
{
	int result;
	int i, j;


	/* Register the board as a character device with the major address
	   either compiled into the driver or set while the module is loaded
	   (or a dynamically assigned one if 'major' is 0). */

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
	if ( major == 0 )
		result = alloc_chrdev_region( &dev_no, 0, 1, "witio_48" );
	else {
		dev_no = MKDEV( major, 0 );
		result = register_chrdev_region( dev_no, 1, "witio_48" );
	}

	if ( result < 0 ) {
                printk( KERN_ERR "witio_48: Can't register as char "
			"device.\n" );
		return -EIO;
	}

	if ( major != 0 )
		board.major = MAJOR( dev_no );

	cdev_init( &ch_dev, &witio_48_file_ops );
	ch_dev.owner = THIS_MODULE;

	if ( cdev_add( &ch_dev, dev_no, 1 ) < 0 ) {
                printk( KERN_ERR "witio_48: Can't register as char "
                        "device.\n" );
		goto device_registration_failure;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 13 )
	witio_48_class = class_create( THIS_MODULE, "witio_48" );

	if ( IS_ERR( witio_48_class ) ) {
		printk( KERN_ERR "witio_48: Can't create a class for "
			"the device.\n" );
		cdev_del( &ch_dev );
		unregister_chrdev_region( dev_no, 1 );
		return -EIO;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 27 )
	device_create( witio_48_class, NULL, major, NULL, "witio_48" );
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 26 )
	device_create( witio_48_class, NULL, major, "witio_48" );
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 15 )
	class_device_create( witio_48_class, NULL, major, NULL, "witio_48" );
#else
	class_device_create( witio_48_class, major, NULL, "witio_48" );
#endif
#endif
#endif

#endif

#else
	if ( ( result = register_chrdev( major, "witio_48",
						&witio_48_file_ops ) ) < 0 ) {
		printk( KERN_ERR "witio_48: Can't register device major "
			"%d.\n", board.major );
		goto device_registration_failure;
	}

	if ( major != 0 )
		board.major = result;
#endif

	/* Now try to get the IO-port region required for the board */

	board.base = base;

	if ( witio_48_get_ioport( ) )
		goto ioport_failure;

	board.in_use = 0;

	/* Deactivate all outputs by switching to mode 0 with all ports
	   being set as input ports. Also set up the default mode to 1x24
	   (two single 24-bit DIOs) and set the structure with the current
	   state accordingly. */

	for ( i = 0; i < 2; i++ ) {
		board.states[ i ].mode = WITIO_48_MODE_1x24;
		for ( j = 0; j < 3; j++ ) {
			board.states[ i ].state[ j ] = READ_ALL;
			board.states[ i ].out_value[ j ] = 0;
		}

		witio_48_set_crtl( i );
	}

	/* Finally initialize the mutexes */

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
        sema_init( &board.open_mutex, 1 );
        sema_init( &board.use_mutex, 1 );
#else
	init_MUTEX( &board.open_mutex );
	init_MUTEX( &board.use_mutex );
#endif

	/* Finally tell the world about the successful installation ;-) */

	printk( KERN_INFO "witio_48: Module succesfully installed.\n" );
	printk( KERN_INFO "witio_48: Major = %d\n", board.major );
	printk( KERN_INFO "witio_48: Base = 0x%lx\n",
		board.base );

	return 0;

 ioport_failure:
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
	unregister_chrdev_region( dev_no, 1 );
#else
	unregister_chrdev( board.major, "witio_48" );
#endif

 device_registration_failure:
	printk( KERN_ERR "witio_48: Module installation failed.\n" );
	return -EIO;
}


/*---------------------------------------------------*
 * Function for module cleanup when it gets unloaded
 *---------------------------------------------------*/

static void __exit witio_48_cleanup( void )
{
	/* Release IO-port region reserved for the board */

	release_region( board.base, 0x0BUL );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 13 )
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 26 )
	device_destroy( witio_48_class, dev_no );
#else
	class_device_destroy( witio_48_class, dev_no );
#endif
        class_destroy( witio_48_class );
#endif

	cdev_del( &ch_dev );
	unregister_chrdev_region( dev_no, 1 );

	printk( KERN_INFO "witio_48: Module succesfully removed.\n" );
#else
	if ( unregister_chrdev( board.major, "witio_48" ) != 0 )
		printk( KERN_ERR "witio_48: Device busy or other module "
			"error.\n" );
	else
		printk( KERN_INFO "witio_48: Module succesfully removed.\n" );
#endif
}


/*------------------------------------------------------*
 * Function for requesting IO-port region for the board
 * and setting up pointers to the boards registers
 *------------------------------------------------------*/

static int witio_48_get_ioport( void )
{
	unsigned long base;
	int i, j;


	/* Port addresses can start at integer multiples of 16 in the range
	   between (and including) 0x200 and 0x300. Check if the requested
	   board IO-port addrress satisfies this condition. */

	for ( base = 0x200;
	      base <= 0x300; base += 0x10 )
		if ( base == board.base )
			break;

	if ( base > 0x300 ) {
		printk( KERN_ERR "witio_48: Invalid board base address: "
			"0x%lx\n", board.base );
		return 1;
	}

	/* Try to obtain the IO-port region requested for the port, we need
	   11 bytes */

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
	if ( request_region( base, 0x0BUL, "witio_48" ) == NULL ) {
		printk( KERN_ERR "witio_48: Can't obtain region at boards "
			"base address: 0x%lx\n", board.base );
		return 1;
	}
#else
	if ( check_region( base, 0x0BUL ) < 0 ) {
		printk( KERN_ERR "witio_48: Can't obtain region at boards "
			"base address: 0x%lx\n", board.base );
		return 1;
	}

	/* Now lock this region - this always succeeds when the check
           succeeded */

	request_region( base, 0x0BUL, "witio_48" );
#endif

	/* Finally set up the pointers to the registers on the board */

	for ( i = 0; i < 2; i++ ) {
		for ( j = 0; j < 3; j++ )
			board.regs.port[ i ][ j ] = base++;
		board.regs.control[ i ] = base++;
	}

	for ( i = 0; i < 3; i++ )
		board.regs.counter[ i ] = base++;

	return 0;
}


/*----------------------------------------------------------*
 * Function executed when device file for board gets opened
 *----------------------------------------------------------*/

static int witio_48_open( struct inode * inode_p,
			  struct file *  file_p )
{
	inode_p = inode_p;

	if ( file_p->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board.open_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board.open_mutex ) )
		return -ERESTARTSYS;

	if ( board.in_use ) {
		up( &board.open_mutex );
		return -EBUSY;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
	MOD_INC_USE_COUNT;
#endif

	board.in_use = 1;
	up( &board.open_mutex );

	return 0;
}


/*----------------------------------------------------------*
 * Function executed when device file for board gets closed
 *----------------------------------------------------------*/

static int witio_48_release( struct inode * inode_p,
			     struct file *  file_p )
{
	inode_p = inode_p;

	if ( down_interruptible( &board.open_mutex ) )
		return -ERESTARTSYS;

	if ( board.in_use == 0 ) {
		up( &board.open_mutex );
		return 0;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
	MOD_DEC_USE_COUNT;
#endif
	board.in_use = 0;
	up( &board.open_mutex );

	return 0;
}


/*-------------------------------------------------------*
 * Function for handling of ioctl() calls for the module
 *-------------------------------------------------------*/

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
static int witio_48_ioctl( struct inode * inode_p,
			   struct file *  file_p,
			   unsigned int   cmd,
			   unsigned long  arg )
#else
static long witio_48_ioctl( struct file *  file_p,
			    unsigned int   cmd,
			    unsigned long  arg )
#endif
{
	int ret = 0;
	WITIO_48_DIO_MODE state_struct;
	WITIO_48_DATA data_struct;


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
	inode_p = inode_p;
#endif

	if ( _IOC_TYPE( cmd ) != WITIO_48_MAGIC_IOC ) {
		up( &board.use_mutex );
		PDEBUG( "Invalid ioctl() call.\n" );
		return -EINVAL;
	}

	/* Serialize access to board */

	if ( file_p->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board.use_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board.use_mutex ) )
		return -ERESTARTSYS;

	if ( cmd == WITIO_48_IOC_SET_MODE )
	{
		if ( copy_from_user( &state_struct,
				     ( const void __user * ) arg,
				     sizeof state_struct ) ) {
			up( &board.use_mutex );
			return -EFAULT;
		}
	} else if ( copy_from_user( &data_struct, ( const void __user * ) arg,
				    sizeof data_struct ) ) {
		up( &board.use_mutex );
		return -EFAULT;
	}

	switch ( cmd ) {
		case WITIO_48_IOC_SET_MODE :
			ret = witio_48_set_mode( &state_struct );
			break;

		case WITIO_48_IOC_GET_MODE :
			ret = witio_48_get_mode( &state_struct );
			if ( ret == 0 &&
			     copy_to_user( ( void __user * ) arg,
					   &state_struct,
					   sizeof state_struct ) )
			     ret = -EFAULT;
			break;

		case WITIO_48_IOC_DIO_OUT :
			ret = witio_48_dio_out( &data_struct );
			break;

		case WITIO_48_IOC_DIO_IN :
			ret = witio_48_dio_in( &data_struct );
			if ( ret == 0 &&
			     copy_to_user( ( void __user * ) arg,
					   &data_struct, sizeof data_struct ) )
			     ret = -EFAULT;
			break;

		default :
			PDEBUG( "Invalid ioctl() call.\n" );
			ret = -EINVAL;
	}

	up( &board.use_mutex );
	return ret;
}


/*-----------------------------------*
 * Function for setting the I/O-mode
 *-----------------------------------*/

static int witio_48_set_mode( WITIO_48_DIO_MODE * state )
{
	/* Check if the DIO number is valid */

	if ( state->dio < WITIO_48_DIO_1 || state->dio > WITIO_48_DIO_2 ) {
		PDEBUG( "Invalid DIO number.\n" );
		return -EINVAL;
	}

	/* Check if the mode is valid */

	if ( state->mode < WITIO_48_MODE_3x8 ||
	     state->mode > WITIO_48_MODE_16_8 ) {
		PDEBUG( "Invalid channel number.\n" );
		return -EINVAL;
	}

	board.states[ state->dio ].mode = state->mode;

	return 0;
}


/*-------------------------------------*
 * Function for returning the I/O-mode
 *-------------------------------------*/

static int witio_48_get_mode( WITIO_48_DIO_MODE * state )
{
	/* Check if the DIO number is valid */

	if ( state->dio < WITIO_48_DIO_1 || state->dio > WITIO_48_DIO_2 ) {
		PDEBUG( "Invalid DIO number.\n" );
		return -EINVAL;
	}

	state->mode = board.states[ state->dio ].mode;

	return 0;
}


/*-------------------------------------------------------------------*
 * Function for outputting values, according to the current I/O-mode
 *-------------------------------------------------------------------*/

static int witio_48_dio_out( WITIO_48_DATA * data )
{
	int dio = data->dio;
	int ch  = data->channel;


	/* Check if the DIO number is valid */

	if ( dio < WITIO_48_DIO_1 || dio > WITIO_48_DIO_2 ) {
		PDEBUG( "Invalid DIO number.\n" );
		return -EINVAL;
	}

	/* Check if the channel number is valid */

	if ( ch < WITIO_48_CHANNEL_0 || ch > WITIO_48_CHANNEL_2 ) {
		PDEBUG( "Invalid channel number.\n" );
		return -EINVAL;
	}

	/* Check if the channel number isn't too large for the mode of
	   the selected DIO */

	if ( ( board.states[ dio ].mode == WITIO_48_MODE_1x24 &&
	       ch > WITIO_48_CHANNEL_0 ) ||
	     ( ( board.states[ dio ].mode == WITIO_48_MODE_2x12 ||
		 board.states[ dio ].mode == WITIO_48_MODE_16_8 ) &&
	       ch > WITIO_48_CHANNEL_1 ) ) {
		PDEBUG( "Invalid channel number for current mode.\n" );
		return -EINVAL;
	}

	/* Now write to the relevant write buffer registers and, if necessary,
	   to the control registers if the direction changed (i.e. the port
	   (or parts of it) had been used for input before) */

	switch ( board.states[ dio ].mode ) {
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


/*-----------------------------------------------------------------*
 * Function for writing out an 8-bit value to one of the ports. If
 * the channel argument is 0 the value is written to port A, for
 * 1 to port B and when the channel argument is 2 the value is
 * output at port C.
 *-----------------------------------------------------------------*/

static void witio_48_3x8_dio_out( WITIO_48_DATA * data )
{
	int dio = data->dio;
	int ch  = data->channel;


	witio_48_board_out( board.regs.port[ dio ][ ch ], data->value & 0xFF );
	board.states[ dio ].out_value[ ch ] = data->value & 0xFF;

	if ( board.states[ dio ].state[ ch ] != WRITE_ALL ) {
		board.states[ dio ].state[ ch ] = WRITE_ALL;
		witio_48_set_crtl( dio );
	}
}


/*--------------------------------------------------------------------*
 * Function for outputting a 12-bit value. If the channel argument is
 * 0 the lower 8 bits of the value are send to port A and its upper
 * 4 bits are output at the upper nibble of port C. For a channel
 * argument of 1 the lower 8 bits of the value appear at port B and
 * the remaining upper 4 bits at the lower nibble of port C.
 *--------------------------------------------------------------------*/

static void witio_48_2x12_dio_out( WITIO_48_DATA * data )
{
	int dio = data->dio;
	int ch  = data->channel;
	unsigned char out = 0;
	int need_crtl_update = 0;


	/* Output the lower 8 bits at the approproate register */

	witio_48_board_out( board.regs.port[ dio ][ ch ],
			    data->value & 0xFF );
	board.states[ dio ].out_value[ ch ] = data->value & 0xFF;

	if ( board.states[ dio ].state[ ch ] != READ_ALL ) {
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

	if ( board.states[ dio ].state[ ch ] != WRITE_ALL ) {
		board.states[ dio ].state[ ch ] = WRITE_ALL;
		witio_48_set_crtl( dio );
	}
		
	if ( ch == WITIO_48_CHANNEL_0 &&
	     board.states[ dio ].state[ C ] & READ_UPPER ) {
		need_crtl_update = 1;
		board.states[ dio ].state[ C ] ^= READ_UPPER;
	} else if ( ch == WITIO_48_CHANNEL_1 &&
		    board.states[ dio ].state[ C ] & READ_LOWER ) {
		need_crtl_update = 1;
		board.states[ dio ].state[ C ] ^= READ_LOWER;
	}

	if ( need_crtl_update )
		witio_48_set_crtl( dio );
}


/*---------------------------------------------------------------------*
 * Function for writing out a 24-bit value. The highest byte is output
 * at port C, the middle byte at port B and the lowest byte at port A.
 *---------------------------------------------------------------------*/

static void witio_48_1x24_dio_out( WITIO_48_DATA * data )
{
	int dio = data->dio;
	unsigned long val = data->value;
	int i;
	int need_crtl_update = 0;


	for ( i = 0; i < 3; val >>= 8, i++ ) {
		witio_48_board_out( board.regs.port[ dio ][ i ], val & 0xFF );
		board.states[ dio ].out_value[ i ] = val & 0xFF;

		if ( board.states[ dio ].state[ i ] != WRITE_ALL ) {
			need_crtl_update = 1;
			board.states[ dio ].state[ i ] = WRITE_ALL;
		}
	}

	if ( need_crtl_update )
		witio_48_set_crtl( dio );
}


/*---------------------------------------------------------------------*
 * Function for writing out either a 16-bit or an 8-bit value. If the
 * channel argument is 0 a 16-bit value is output, with the upper byte
 * appearing at port B and the lower byte at port A. For a channel
 * argument an 8-bit value is written to port C.
 *---------------------------------------------------------------------*/

static void witio_48_16_8_dio_out( WITIO_48_DATA * data )
{
	int dio = data->dio;
	int ch  = data->channel;
	unsigned long val = data->value;
	int i;
	int need_crtl_update = 0;


	if ( ch == WITIO_48_CHANNEL_0 ) {
		for ( i = 0; i < 2; val >>= 8, i++ )
		{
			witio_48_board_out( board.regs.port[ dio ][ i ],
					    val & 0xFF );
			board.states[ dio ].out_value[ i ] = val & 0xFF;
			if ( board.states[ dio ].state[ i ] != WRITE_ALL ) {
				need_crtl_update = 1;
				board.states[ dio ].state[ i ] = WRITE_ALL;
			}
		}
	} else {
		witio_48_board_out( board.regs.port[ dio ][ C ], val & 0xFF );
		board.states[ dio ].out_value[ C ] = val & 0xFF;

		if ( board.states[ dio ].state[ C ] != WRITE_ALL ) {
			need_crtl_update = 1;
			board.states[ dio ].state[ C ] = WRITE_ALL;
		}
	}

	if ( need_crtl_update )
		witio_48_set_crtl( dio );
}


/*-------------------------------------------------------------------*
 * Function for reading in values, according to the current I/O-mode
 *-------------------------------------------------------------------*/

static int witio_48_dio_in( WITIO_48_DATA * data )
{
	int dio = data->dio;
	int ch  = data->channel;


	/* Check if the DIO number is valid */

	if ( dio < WITIO_48_DIO_1 || dio > WITIO_48_DIO_2 ) {
		PDEBUG( "Invalid DIO number.\n" );
		return -EINVAL;
	}

	/* Check if the channel number is valid */

	if ( ch < WITIO_48_CHANNEL_0 || ch > WITIO_48_CHANNEL_2 ) {
		PDEBUG( "Invalid channel number.\n" );
		return -EINVAL;
	}

	/* Check if the channel number isn't too large for the mode of
	   the selected DIO */

	if ( ( board.states[ dio ].mode == WITIO_48_MODE_1x24 &&
	       ch > WITIO_48_CHANNEL_0 ) ||
	     ( ( board.states[ dio ].mode == WITIO_48_MODE_2x12 ||
		 board.states[ dio ].mode == WITIO_48_MODE_16_8 ) &&
	       ch > WITIO_48_CHANNEL_1 ) ) {
		PDEBUG( "Invalid channel number for current mode.\n" );
		return -EINVAL;
	}

	/* Now write to the relevant write buffer registers and, if necessary,
	   to the control registers if the direction changed (i.e. the port
	   (or parts of it) had been used for output before) */

	switch ( board.states[ dio ].mode ) {
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


/*-------------------------------------------------------------------*
 * Function for reading in an 8-bit value from of the three channels
 * when in 3x8 mode. If the channel argument is 0 the value comes
 * from port A, for 1 the value is from port B and for the channel
 * argument set to 2 the value is what gets read in from port C.
 *-------------------------------------------------------------------*/

static void witio_48_3x8_dio_in( WITIO_48_DATA * data )
{
	int dio = data->dio;
	int ch  = data->channel;


	if ( board.states[ dio ].state[ ch ] != READ_ALL ) {
		board.states[ dio ].state[ ch ] = READ_ALL;
		witio_48_set_crtl( dio );
	}

	data->value = witio_48_board_in( board.regs.port[ dio ][ ch ] );
}


/*----------------------------------------------------------------*
 * Function for reading in a 12-bit value. When the channel is 0
 * data from port A make up the lower 8 bits of the value and the
 * upper nibble from prot C the 4 higher bits. For channel 1 the
 * lower 8 bits of the value come from port B while its higher 4
 * bits are the lower nibble of port C.
 *----------------------------------------------------------------*/

static void witio_48_2x12_dio_in( WITIO_48_DATA * data )
{
	int dio = data->dio;
	int ch  = data->channel;
	int need_crtl_update = 0;


	/* If necessary set up the control port to allow reading in the
	   data */

	if ( board.states[ dio ].state[ ch ] != READ_ALL ) {
		need_crtl_update = 1;
		board.states[ dio ].state[ ch ] = READ_ALL;
	}

	if ( ch == WITIO_48_CHANNEL_0 &&
	     ! ( board.states[ dio ].state[ C ] & READ_UPPER ) ) {
		need_crtl_update = 1;
		board.states[ dio ].state[ C ] |= READ_UPPER;
	} else if ( ch == WITIO_48_CHANNEL_1 &&
		    ! ( board.states[ dio ].state[ C ] & READ_LOWER ) ) {
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


/*------------------------------------------------------------*
 * Function for reading in a 24-bit value. The higest byte of
 * the value comes from port C, the middle byte from port B
 * and the lowest byte from port A.
 *------------------------------------------------------------*/

static void witio_48_1x24_dio_in( WITIO_48_DATA * data )
{
	int dio = data->dio;
	int i;
	int need_crtl_update = 0;


	for ( i = 0; i <= 2; i++ )
		if ( board.states[ dio ].state[ i ] != READ_ALL ) {
			need_crtl_update = 1;
			board.states[ dio ].state[ i ] = READ_ALL;
		}

	if (  need_crtl_update )
		witio_48_set_crtl( dio );

	for ( data->value = 0, i = 2; i >= 0; i-- )
	{
		data->value <<= 8;
		data->value |=
			      witio_48_board_in( board.regs.port[ dio ][ i ] );
	}
}


/*------------------------------------------------------------*
 * Function for reading in either a 16-bit or an 8-bit value.
 * When the channel argument is 0 a 16-bit value is read in
 * with the upper byte comming from port B and the lower byte
 * from port A. When channel is 1 an 8-bit value is read in
 * from port C.
 *------------------------------------------------------------*/

static void witio_48_16_8_dio_in( WITIO_48_DATA * data )
{
	int dio = data->dio;
	int ch  = data->channel;
	int i;
	int need_crtl_update = 0;


	if ( ch == WITIO_48_CHANNEL_0 ) {
		for ( i = 1; i >= 0; i++, data->value <<= 8 )
			if ( board.states[ dio ].state[ i ] != READ_ALL ) {
				need_crtl_update = 1;
				board.states[ dio ].state[ i ] = READ_ALL;
			}

		if (  need_crtl_update )
			witio_48_set_crtl( dio );

		for ( data->value = 0, i = 1; i >= 0; i++, data->value <<= 8 )
			data->value |=
			      witio_48_board_in( board.regs.port[ dio ][ i ] );
	} else {
		if ( board.states[ dio ].state[ C ] != READ_ALL ) {
			board.states[ dio ].state[ C ] = READ_ALL;
			witio_48_set_crtl( dio );
		}

		data->value = witio_48_board_in( board.regs.port[ dio ][ C ] );
	}
}


/*--------------------------------------------------------------*
 * Calculates the value that needs to be written to the control
 * register for one of the DIOs and writes it to the register
 * (always with mode ("Betriebsart") 0).
 *--------------------------------------------------------------*/

#define READ_A        0x10
#define READ_B        0x02
#define READ_C_UPPER  0x08
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


/*------------------------------------------------------------------*
 * Outputs one byte to the register addressed by the first argument
 *------------------------------------------------------------------*/

static void witio_48_board_out( unsigned long addr,
				unsigned char byte )
{
	outb_p( byte, addr );
}


/*-------------------------------------------------------------*
 * Inputs one byte from the register addressed by the argument
 *-------------------------------------------------------------*/

static unsigned char witio_48_board_in( unsigned long addr )
{
	return inb_p( addr );
}



/* No symbols are exported from the module */

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
EXPORT_NO_SYMBOLS;
#endif

/* Mark the init and cleanup routines */

module_init( witio_48_init );
module_exit( witio_48_cleanup );


/* Module options */

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
module_param( base, int, S_IRUGO );
module_param( major, int, S_IRUGO );
#else
MODULE_PARM( base, "i" );
MODULE_PARM_DESC( base, "Base address" );
MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number (either 1-254 or 0 to use "
                 "dynamically assigned number)" );
#endif

MODULE_AUTHOR( "Jens Thoms Toerring <jt@toerring.de>" );
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
