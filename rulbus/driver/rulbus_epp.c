/*
 *	Driver for the RULBUS EPP interface
 *
 *	Copyright (C) 2003-2012  Jens Thoms Toerring
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; see the file COPYING.	If not, write to
 *	the Free Software Foundation, 59 Temple Place - Suite 330,
 *	Boston, MA 02111-1307, USA.
 *
 *	To contact the author send email to jt@toerring.de
 */


#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 19 )
#include <linux/config.h>
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 33 )
#include <linux/autoconf.h>
#else
#include <generated/autoconf.h>
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )

#if defined( CONFIG_MODVERSIONS ) && ! defined( MODVERSIONS )
#define MODVERSIONS
#endif

#if defined( MODVERSIONS )
#include <linux/modversions.h>
#endif

#include <linux/module.h>

#if defined( CONFIG_SMP ) && ! defined( __SMP__ )
#define __SMP__
#endif

#else

#include <linux/module.h>

#endif


#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/parport.h>
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/delay.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 26 )
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#include <linux/slab.h>
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



#include "rulbus_epp.h"
#include "autoconf.h"


/*****************************************
 * Defines
 *****************************************/


#define RULBUS_EPP_NAME	 "rulbus_epp"


#define RULBUS_MAX_BUFFER_SIZE	10		 /* may never be smaller than 1 ! */

#define WRITE_TO_DEVICE		0
#define READ_FROM_DEVICE	1


#define STATUS_BYTE			( base + 1 )
#define CTRL_BYTE			( base + 2 )
#define ADDR_BYTE			( base + 3 )
#define DATA_BYTE			( base + 4 )


/* Bits in SPP status byte */

#define EPP_Timeout			( 1 << 0 )	 /* EPP timeout (reserved in SPP) */
#define SPP_Reserved2		( 1 << 1 )	 /* SPP reserved2 */
#define SPP_nIRQ			( 1 << 2 )	 /* SPP !IRQ */
#define SPP_nError			( 1 << 3 )	 /* SPP !error */
#define SPP_SelectIn		( 1 << 4 )	 /* SPP select in */
#define SPP_PaperOut		( 1 << 5 )	 /* SPP paper out */
#define SPP_nAck			( 1 << 6 )	 /* SPP !acknowledge */
#define SPP_nBusy			( 1 << 7 )	 /* SPP !busy */


/* Bits in SPP control byte */

#define SPP_Strobe			( 1 << 0 )	 /* SPP strobe */
#define SPP_AutoLF			( 1 << 1 )	 /* SPP auto linefeed */
#define SPP_nReset			( 1 << 2 )	 /* SPP !reset */
#define SPP_SelectPrinter	( 1 << 3 )	 /* SPP select printer */
#define SPP_EnaIRQVAL		( 1 << 4 )	 /* SPP enable IRQ */
#define SPP_EnaBiDirect		( 1 << 5 )	 /* SPP enable bidirectional */
#define SPP_Unused1			( 1 << 6 )	 /* SPP unused1 */
#define SPP_Unused2			( 1 << 7 )	 /* SPP unused2 */




/*****************************************
 * Declaration of non-inline functions
 *****************************************/

static int __init rulbus_init( void );

static void __exit rulbus_cleanup( void );

static int rulbus_open( struct inode * /* inode_p */,
						struct file *  /* filep	  */ );

static int rulbus_release( struct inode * /* inode_p */,
						   struct file *  /* filep	 */ );

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
static int rulbus_ioctl( struct inode * /* inode_p */,
						 struct file *	/* filep   */,
						 unsigned int	/* cmd	   */,
						 unsigned long	/* arg	   */ );
#else
static long rulbus_ioctl( struct file * /* filep */,
						  unsigned int	/* cmd	 */,
						  unsigned long /* arg	 */ );
#endif

static int rulbus_read( RULBUS_EPP_IOCTL_ARGS * /* rulbus_arg */ );

static int rulbus_read_range( RULBUS_EPP_IOCTL_ARGS * /* rulbus_arg */ );

static int rulbus_write( RULBUS_EPP_IOCTL_ARGS * /* rulbus_arg */ );

static int rulbus_write_range( RULBUS_EPP_IOCTL_ARGS * /* rulbus_arg */ );

static void rulbus_epp_attach( struct parport * /* port */ );

static void rulbus_epp_detach( struct parport * /* port */ );

static int rulbus_epp_init( void );

static int rulbus_epp_interface_present( void );



/*****************************************
 * Definition of global variables
 *****************************************/

static int major = RULBUS_EPP_MAJOR;
static unsigned long base = RULBUS_EPP_BASE;

static struct parport_driver rulbus_drv = { RULBUS_EPP_NAME,
											rulbus_epp_attach,
											rulbus_epp_detach };

static struct rulbus_device {
		struct parport *port;
		struct pardevice *dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
		struct cdev ch_dev;
		dev_t dev_no;
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 14 )
		struct class * rb_class;
#endif
#endif
		int in_use;					  /* set when device is opened */
		int is_claimed;				  /* set while we have exclusive access */
		struct semaphore open_mutex;  /* mutex for open call */
		struct semaphore ioctl_mutex; /* mutex for ioctl calls */
		unsigned char exclusive;	  /* for exclusive access to device file */
		unsigned char rack;			  /* currently addressed rack */
		unsigned char direction;	  /* current transfer direction */
} rulbus = { NULL,
			 NULL,
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
			 { },
			 ( dev_t ) 0,
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 13 )
			 NULL,
#endif
#endif
			 0,
			 0,
			 { },
			 { },
			 0,
			 0x0F,
			 WRITE_TO_DEVICE
		   };


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
struct file_operations rulbus_file_ops = {
		owner:			  THIS_MODULE,
		ioctl:			  rulbus_ioctl,
		open:			  rulbus_open,
		release:		  rulbus_release,
};
#else
struct file_operations rulbus_file_ops = {
		.owner =		  THIS_MODULE,
#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
		.ioctl =		  rulbus_ioctl,
#else
		.unlocked_ioctl = rulbus_ioctl,
#endif
		.open =			  rulbus_open,
		.release =		  rulbus_release,
};
#endif



/*****************************************
 * Bookkeeping for the module
 *****************************************/

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number of device file" );
MODULE_PARM( base, "i" );
MODULE_PARM_DESC( base, "Base address of parallel port the RULBUS is "
				  "connected to" );
#else
module_param( major, int, S_IRUGO );
module_param(  base, ulong, S_IRUGO );
#endif

MODULE_AUTHOR( "Jens Thoms Toerring <jt@toerring.de>" );
MODULE_DESCRIPTION( "RULBUS parallel port (EPP) driver" );
#if defined MODULE_LICENSE
MODULE_LICENSE( "GPL" );
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
EXPORT_NO_SYMBOLS;
#endif


module_init( rulbus_init );
module_exit( rulbus_cleanup );


/*****************************************
 * Definition of inlined functions
 *****************************************/

/*-----------------------------------------------------------*
 * Function tests if the last transfer resulted in a timeout
 *-----------------------------------------------------------*/

inline static
unsigned char
rulbus_epp_timeout_check( void )
{
		return inb( STATUS_BYTE ) & EPP_Timeout;
}


/*-----------------------------------------------------*
 * Function to check for and clear a timeout condition
 *-----------------------------------------------------*/

inline static
int
rulbus_clear_epp_timeout( void )
{
		unsigned char status;


		if ( ! rulbus_epp_timeout_check( ) )
				return 1;

		/* To clear timeout some chips require a double read */

		inb( STATUS_BYTE );
		status = inb( STATUS_BYTE );

		/* Some chips reset the timeout condition by setting the lowest bit,
		   others by resetting it */

		outb( status | EPP_Timeout, STATUS_BYTE );
		outb( status & ~ EPP_Timeout, STATUS_BYTE );

		return ! rulbus_epp_timeout_check( );
}


/*--------------------------------------------------------------------------*
 * Reverse the port direction by enabling or disabling the bi-direction bit
 *--------------------------------------------------------------------------*/

inline static
void
rulbus_parport_reverse( void )
{
		rulbus.direction ^= 1;
		outb( inb( CTRL_BYTE ) ^ SPP_EnaBiDirect, CTRL_BYTE );
}


/*----------------------------------------------------------*
 * Function for reading a data byte from the parallel port,
 * if necessary after switching from write to read mode
 *---------------------------------------------------------*/

inline static
unsigned char
rulbus_parport_read_data( void )
{
		if ( rulbus.direction != READ_FROM_DEVICE )
				rulbus_parport_reverse( );
		rulbus_clear_epp_timeout( );
		return inb( DATA_BYTE );
}


/*--------------------------------------------------------------*
 * Function for reading an address byte from the parallel port,
 * if necessary after switching from write to read mode
 *--------------------------------------------------------------*/

inline static
unsigned char
rulbus_parport_read_addr( void )
{
		if ( rulbus.direction != READ_FROM_DEVICE )
				rulbus_parport_reverse( );
		rulbus_clear_epp_timeout( );
		return inb( ADDR_BYTE );
}


/*--------------------------------------------------------*
 * Function for writing a data byte to the parallel port,
 * if necessary after switching from read to write mode
 * (the data byte gets written out twice with a delay of
 * 100 ns in between since otherwise the write sometimes
 * fails - even using out_p() doesn't seem to help).
*--------------------------------------------------------*/

inline static
void
rulbus_parport_write_data( unsigned char data )
{
		if ( rulbus.direction != WRITE_TO_DEVICE )
				rulbus_parport_reverse( );
		rulbus_clear_epp_timeout( );
		outb( data, DATA_BYTE );
		ndelay( 100 );
		outb( data, DATA_BYTE );
}


/*------------------------------------------------------------*
 * Function for writing an address byte to the parallel port,
 * if necessary after switching from read to write mode
 *------------------------------------------------------------*/

inline static
void
rulbus_parport_write_addr( unsigned char addr )
{
		if ( rulbus.direction != WRITE_TO_DEVICE )
				rulbus_parport_reverse( );
		rulbus_clear_epp_timeout( );
		outb( addr, ADDR_BYTE );
}


/*****************************************
 * Definition of non-inlined function
 *****************************************/

/*------------------------------------------------------*
 * Function that gets invoked when the module is loaded
 *------------------------------------------------------*/

static
int
__init rulbus_init( void )
{
		int result;


		/* All we do at the moment is registering a driver and a char device,
		   everything else is deferred until we get notified about the
		   parports of the system and when the device file gets opened. */

		if ( parport_register_driver( &rulbus_drv ) != 0 )
				return -EIO;

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
		if ( major == 0 )
				result = alloc_chrdev_region( &rulbus.dev_no, 0, 1,
											  RULBUS_EPP_NAME );
		else {
				rulbus.dev_no = MKDEV( major, 0 );
				result = register_chrdev_region( rulbus.dev_no, 1,
												 RULBUS_EPP_NAME );
		}

		if ( result < 0 ) {
				printk( KERN_ERR RULBUS_EPP_NAME ": Can't register as char "
						"device.\n" );
				parport_unregister_driver( &rulbus_drv );
				return -EIO;
		}

		cdev_init( &rulbus.ch_dev, &rulbus_file_ops );
		rulbus.ch_dev.owner = THIS_MODULE;

		if ( cdev_add( &rulbus.ch_dev, rulbus.dev_no, 1 ) < 0 ) {
				printk( KERN_ERR RULBUS_EPP_NAME ": Can't register as char "
						"device.\n" );
				unregister_chrdev_region( rulbus.dev_no, 1 );
				parport_unregister_driver( &rulbus_drv );
				return -EIO;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 13 )
		rulbus.rb_class = class_create( THIS_MODULE, RULBUS_EPP_NAME );
		if ( IS_ERR( rulbus.rb_class ) ) {
				printk( KERN_ERR RULBUS_EPP_NAME ": Can't create a class for "
						"the device.\n" );
				cdev_del( &rulbus.ch_dev );
				unregister_chrdev_region( rulbus.dev_no, 1 );
				parport_unregister_driver( &rulbus_drv );
				return -EIO;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 27 )
		device_create( rulbus.rb_class, NULL, rulbus.dev_no,
					   NULL, RULBUS_EPP_NAME );
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 26 )
		device_create( rulbus.rb_class, NULL, rulbus.dev_no, RULBUS_EPP_NAME );
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 15 )
		class_device_create( rulbus.rb_class, NULL, rulbus.dev_no,
							 NULL, RULBUS_EPP_NAME );
#else
		class_device_create( rulbus.rb_class, rulbus.dev_no,
							 NULL, RULBUS_EPP_NAME );
#endif
#endif
#endif

#endif

#else
		if ( ( result = register_chrdev( major, RULBUS_EPP_NAME,
										 &rulbus_file_ops ) ) < 0 ) {
				printk( KERN_ERR RULBUS_EPP_NAME ": Can't register as char "
						"device.\n" );
				parport_unregister_driver( &rulbus_drv );
				return -EIO;
		}

		if ( major == 0 )
				major = result;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
		sema_init( &rulbus.open_mutex, 1 );
		sema_init( &rulbus.ioctl_mutex, 1 );
#else
		init_MUTEX( &rulbus.open_mutex );
		init_MUTEX( &rulbus.ioctl_mutex );
#endif

		printk( KERN_INFO RULBUS_EPP_NAME
				": Module successfully installed\n" );

		return 0;
}


/*---------------------------------------------------*
 * Function gets invoked when the module is unloaded
 *---------------------------------------------------*/

static
void
__exit rulbus_cleanup( void )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 13 )
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 26 )
		device_destroy( rulbus.rb_class, rulbus.dev_no );
#else
		class_device_destroy( rulbus.rb_class, rulbus.dev_no );
#endif
		class_destroy( rulbus.rb_class );
#endif

		cdev_del( &rulbus.ch_dev );
		unregister_chrdev_region( rulbus.dev_no, 1 );
#else
		if ( unregister_chrdev( major, RULBUS_EPP_NAME ) < 0 )
		{
				printk( KERN_ERR RULBUS_EPP_NAME
						": Device busy or other module error.\n" );
				return;
		}
#endif

		/* Unregister the device (but only if it's registered) */

		if ( rulbus.dev )
				rulbus_epp_detach( rulbus.port );

		parport_unregister_driver( &rulbus_drv );

		printk( KERN_INFO RULBUS_EPP_NAME
				": Module successfully removed\n" );
}


/*--------------------------------------------------------------------*
 * Function that gets called automatically for each detected parport,
 * it picks the one with the right base address and tries to register
 * a driver for it.
 *--------------------------------------------------------------------*/

static
void
rulbus_epp_attach( struct parport * port )
{
		/* Check that the port has the base address we're looking for and
		   that we don't already have a device registered for this port */

		if ( port->base != base || rulbus.dev )
				return;

		/* Register a device for this parport */

		if ( ( rulbus.dev = parport_register_device( port,
													 RULBUS_EPP_NAME,
													 NULL, NULL, NULL,
													 0, NULL ) ) == NULL ) {
				printk( KERN_NOTICE "Failed to register device\n" );
				return;
		}

		rulbus.port = port;
}


/*---------------------------------------------------------------------*
 * Function that gets called either automatically in case of a parport
 * suddenly vanishing or from the cleanup function of the module. It
 * unregisters the device we might have registered for the port.
 *---------------------------------------------------------------------*/

static
void
rulbus_epp_detach( struct parport * port )
{
		if ( rulbus.dev == NULL || rulbus.port != port )
				return;

		if ( rulbus.is_claimed )
				parport_release( rulbus.dev );
		parport_unregister_device( rulbus.dev );
		rulbus.dev = NULL;
}


/*-------------------------------------------------------------------------*
 * Function that gets called when the device file for the RULBUS interface
 * gets opened. After checking that a device has been registered we must
 * claim the port - and we only will unclaim it when the device file is
 * closed, there's no daisy chaining possible with this device. Next we
 * must check that the device is present and talks EPP. Only then we can
 * be reasonably sure we got it.
 *-------------------------------------------------------------------------*/

static
int
rulbus_open( struct inode * inode_p,
			 struct file  * filep )
{
		/* All the following code can be only be accessed from a single thread
		   at a time. For callers that want to open the device not prepared
		   to wait we need to work with down_trylock(). For others we call
		   down_interruptible() in order to be able to react to signals (the
		   return value ERESTARTSYS is a "magic" value saying that it's ok to
		   restart the system call automagically after the signal has been
		   handled and gets converted to EINTR if that's not possible).
		   Please note that the mutex is also used during the close() call. */

		if ( filep->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
				if ( down_trylock( &rulbus.open_mutex ) )
						return -EAGAIN;
		} else if ( down_interruptible( &rulbus.open_mutex ) )
				return -ERESTARTSYS;

		/* Return EEXIST if the caller asks for exclusive access or the device
		   has already been opened with exclusive access by another caller -
		   this allows to grant exclusive access to the device without
		   having to use some kind of locking mechanism in userland. */

		if ( rulbus.in_use != 0 && 
			 ( filep->f_flags & O_EXCL || rulbus.exclusive != 0 ) ) {
				up( &rulbus.open_mutex );
				return -EEXIST;
		}

		/* If the device file hasn't already been opened by another caller
		   do all the required initializations. */

		if ( rulbus.in_use == 0 ) {
				/* Check that a device is registered for the port we need */

				if ( rulbus.dev == NULL ) {
						up( &rulbus.open_mutex );
						printk( KERN_NOTICE
								"No device registered for port\n" );
						return -EIO;
				}

				/* We need exclusive access to the port as long as the device
				   file is open - the device does not allow daisy chaining
				   since its using all addresses on the bus. */

				if ( parport_claim_or_block( rulbus.dev ) < 0 ) {
						up( &rulbus.open_mutex );
						printk( KERN_NOTICE
								"Failed to claim parallel port\n" );
						return -EIO;
				}

				if ( rulbus_epp_init( ) != 0  ) {
						parport_release( rulbus.dev );
						up( &rulbus.open_mutex );
						printk( KERN_NOTICE
								"Rulbus interface not present.\n" );
						return -EIO;
				}

				rulbus.is_claimed = 1;

				/* Set the currently rack address to an invalid rack address
				   so that the next the read or write results in a readdressing
				   of the rack. */

				rulbus.rack = 0x0F;

				/* Finally, if the caller asked for exclusive access to the
				   device file, set a flag that reminds us about it. */

				if ( filep->f_flags & O_EXCL )
						rulbus.exclusive = 1;
		}

		rulbus.in_use++;

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
		MOD_INC_USE_COUNT;
#endif

		up( &rulbus.open_mutex );

		return 0;
}


/*---------------------------------------------------------------------*
 * Function that gets called when the device file for the interface is
 * closed. We must unclaim the port and unregister the device for it.
 *---------------------------------------------------------------------*/

static
int
rulbus_release( struct inode * inode_p,
				struct file  * filep )
{
		if ( rulbus.in_use == 0 ) {
				printk( KERN_NOTICE "Device not open\n" );
				return -ENODEV;
		}

		if ( down_interruptible( &rulbus.open_mutex ) )
				return -ERESTARTSYS;

		if ( rulbus.in_use == 0 ) {
				up( &rulbus.open_mutex );
				return -EBADF;
		}

		/* Unclaim and unregister the parallel port */

		if ( --rulbus.in_use == 0 &&
			 rulbus.dev != NULL &&
			 rulbus.is_claimed ) {
				parport_release( rulbus.dev );
				rulbus.is_claimed = 0;
		}

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
		MOD_DEC_USE_COUNT;
#endif

		rulbus.exclusive = 0;

		up( &rulbus.open_mutex );

		return 0;
}


/*----------------------------------------------------*
 * Function dealing with ioctl() calls for the device
 *----------------------------------------------------*/

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
static
int
rulbus_ioctl( struct inode * inode_p,
			  struct file  * filep,
			  unsigned int	 cmd,
			  unsigned long	 arg )
#else
static
long
rulbus_ioctl( struct file   * filep,
			  unsigned int	  cmd,
			  unsigned long   arg )
#endif
{
		RULBUS_EPP_IOCTL_ARGS rulbus_arg;
		int ret;


		if ( rulbus.dev == NULL || ! rulbus.is_claimed ) {
				printk( KERN_NOTICE "Device is not open\n" );
				return -ENODEV;
		}

		/* The ioctl() calls can't be handled for two or more callers at once.
		   Thus callers must wait for a mutex (or, in the case where instant
		   access is requested, be prepared to get an EGAIN return value). */

		if ( filep->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
				if ( down_trylock( &rulbus.ioctl_mutex ) )
						return -EAGAIN;
		} else if ( down_interruptible( &rulbus.ioctl_mutex ) )
				return -ERESTARTSYS;

		if ( copy_from_user( &rulbus_arg, ( const void __user * ) arg,
							 sizeof rulbus_arg ) ) {
				up( &rulbus.ioctl_mutex );
				printk( KERN_NOTICE "Can't read from user space\n" );
				return -EFAULT;
		}

		switch ( cmd )
		{
				case RULBUS_EPP_IOC_READ :
						ret = rulbus_read( ( RULBUS_EPP_IOCTL_ARGS * ) arg );
						break;

				case RULBUS_EPP_IOC_READ_RANGE :
						ret =
						  rulbus_read_range( ( RULBUS_EPP_IOCTL_ARGS * ) arg );
						break;

				case RULBUS_EPP_IOC_WRITE :
						ret = rulbus_write( ( RULBUS_EPP_IOCTL_ARGS * ) arg );
						break;

				case RULBUS_EPP_IOC_WRITE_RANGE :
						ret = 
						 rulbus_write_range( ( RULBUS_EPP_IOCTL_ARGS * ) arg );
						break;

				default :
						printk( KERN_NOTICE "Invalid ioctl() call %d\n", cmd );
						ret = -EINVAL;
		}

		up( &rulbus.ioctl_mutex );

		return ret;
}


/*-----------------------------------------------------------*
 * Function for reading data from a card in one of the racks
 *-----------------------------------------------------------*/

static
int
rulbus_read( RULBUS_EPP_IOCTL_ARGS * rulbus_arg )
{
		unsigned char * data;
		unsigned char buffer[ RULBUS_MAX_BUFFER_SIZE ];
		unsigned char * bp;
		size_t i;


		/* Plausibility checks */

		if ( rulbus_arg->rack & 0xF0 ) {
				printk( KERN_NOTICE "Invalid rack number.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->offset < 1 || rulbus_arg->offset > 0xFE ) {
				printk( KERN_NOTICE "Invalid address offset.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->len < 1 ) {
				printk( KERN_NOTICE "Invalid number of bytes to read.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->data == NULL ) {
				printk( KERN_NOTICE "Invalid read data pointer.\n" );
				return -EINVAL;
		}

		/* If there are not more that RULBUS_MAX_BUFFER_SIZE to be read
		   (the typical case) use a local buffer (which should be faster),
		   otherwise allocate as much memory as needed */

		if ( rulbus_arg->len <= RULBUS_MAX_BUFFER_SIZE )
				data = buffer;
		else {
				data = kmalloc( rulbus_arg->len, GFP_KERNEL );

				if ( data == NULL ) {
						printk( KERN_NOTICE
								"Not enough memory for reading.\n" );
						return -ENOMEM;
				}
		}

		/* Select the rack (unless it's not already selected) */

		if ( rulbus.rack != rulbus_arg->rack ) {
				rulbus_parport_write_addr( 0 );
				rulbus_parport_write_data( rulbus_arg->rack );
				rulbus.rack = rulbus_arg->rack;
		}

		/* Write the rulbus address and then read as many data as required */

		rulbus_parport_write_addr( rulbus_arg->offset );
		for ( bp = data, i = rulbus_arg->len; i > 0; bp++, i-- )
				*bp = rulbus_parport_read_data( );

		/* Copy all that has just been read to the user space buffer and, if
		   necessary, deallocate the kernel buffer */

		if ( copy_to_user( ( void __user * ) rulbus_arg->data,
						   data, rulbus_arg->len ) ) {
				if ( rulbus_arg->len > RULBUS_MAX_BUFFER_SIZE )
						kfree( data );
				printk( KERN_NOTICE "Can't write to user space\n" );
				return -EFAULT;
		}

		if ( rulbus_arg->len > RULBUS_MAX_BUFFER_SIZE )
				kfree( data );

		return rulbus_arg->len;
}


/*--------------------------------------------------------------------*
 * Function for reading data from a (consecutive) range of addresses
 * of one of the racks. Since there are never more that 254 addresses
 * we can do with a local buffer instead of allocating one.
 *--------------------------------------------------------------------*/

static
int
rulbus_read_range( RULBUS_EPP_IOCTL_ARGS * rulbus_arg )
{
		unsigned char data[ 254 ];
		size_t i;


		/* If only a single byte is to be read call rulbus_read() */

		if ( rulbus_arg->len == 1 )
				return rulbus_read( rulbus_arg ); 

		/* Plausibility checks */

		if ( rulbus_arg->rack & 0xF0 ) {
				printk( KERN_NOTICE "Invalid rack number.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->offset < 1 || rulbus_arg->offset > 0xFE ) {
				printk( KERN_NOTICE "Invalid address offset.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->len < 1 ) {
				printk( KERN_NOTICE "Invalid number of bytes to read.\n" );
				return -EINVAL;
		}

		if ( ( unsigned long ) rulbus_arg->offset + rulbus_arg->len > 0xFE ) {
				printk( KERN_NOTICE "Address range too large.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->data == NULL ) {
				printk( KERN_NOTICE "Invalid read data pointer.\n" );
				return -EINVAL;
		}

		/* Select the rack (unless it's not already selected) */

		if ( rulbus.rack != rulbus_arg->rack ) {
				rulbus_parport_write_addr( 0 );
				rulbus_parport_write_data( rulbus_arg->rack );
				rulbus.rack = rulbus_arg->rack;
		}

		/* Now read as many data as required from consecutive rulbus
		   addresses*/

		for ( i = 0; i < rulbus_arg->len; i++ ) {
				rulbus_parport_write_addr( rulbus_arg->offset + i );
				data[ i ] = rulbus_parport_read_data( );
		}

		/* Copy all that has just been read to the user space buffer and
		   deallocate the kernel buffer */

		if ( copy_to_user( ( void __user * ) rulbus_arg->data,
						   data, rulbus_arg->len ) ) {
				printk( KERN_NOTICE "Can't write to user space\n" );
				return -EFAULT;
		}

		return rulbus_arg->len;
}


/*---------------------------------------------------------*
 * Function for writing data to a card in one of the racks
 *---------------------------------------------------------*/

static
int
rulbus_write( RULBUS_EPP_IOCTL_ARGS * rulbus_arg )
{
		unsigned char buffer[ RULBUS_MAX_BUFFER_SIZE ];
		unsigned char * data;
		unsigned char * bp;
		size_t i;


		/* Plausibility checks */

		if ( rulbus_arg->rack & 0xF0 ) {
				printk( KERN_NOTICE "Invalid rack number.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->offset < 1 || rulbus_arg->offset > 0xFE ) {
				printk( KERN_NOTICE "Invalid address offset.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->len < 1 ) {
				printk( KERN_NOTICE
						"Invalid number of bytes to be written.\n" );
				return -EINVAL;
		}

		/* If there's just a single byte to be written the 'byte' field of
		   the RULBUS_EPP_IOCTL_ARGS structure gets used, for not more than
		   RULBUS_MAX_BUFFER_SIZE (which is the typical case) copy everything
		   to a local buffer, and only for "huge" amounts of data allocate
		   memory and then copy the data there. */

		if ( rulbus_arg->len == 1 )
				data = &rulbus_arg->byte;
		else {
				if ( rulbus_arg->len <= RULBUS_MAX_BUFFER_SIZE )
						data = buffer;
				else {
						data = kmalloc( rulbus_arg->len, GFP_KERNEL );

						if ( data == NULL ) {
								printk( KERN_NOTICE
										"Not enough memory for reading.\n" );
								return -ENOMEM;
						}
				}

				if ( rulbus_arg->data == NULL ) {
						printk( KERN_NOTICE "Invalid write data pointer.\n" );
						return -EINVAL;
				}

				if ( copy_from_user( data,
									 ( const void __user * ) rulbus_arg->data,
									 rulbus_arg->len ) ) {
						if ( data != buffer )
								kfree( data );
						printk( KERN_NOTICE "Can't read from user space\n" );
						return -EFAULT;
				}
		}

		/* Select the rack (if necessary) */

		if ( rulbus.rack != rulbus_arg->rack ) {
				rulbus_parport_write_addr( 0 );
				rulbus_parport_write_data( rulbus_arg->rack );
				rulbus.rack = rulbus_arg->rack;
		}

		/* Now write the rulbus address and then as many data as required */

		rulbus_parport_write_addr( rulbus_arg->offset );
		for ( bp = data, i = rulbus_arg->len; i > 0; bp++, i-- )
				rulbus_parport_write_data( *bp );

		/* If we had to allocate a buffer get rid of it */

		if ( rulbus_arg->len > RULBUS_MAX_BUFFER_SIZE )
				kfree( data );

		return rulbus_arg->len;
}


/*---------------------------------------------------------------*
 * Function for writing data to a consecutive range of addresses 
 * in one of the racks. Since there are never more that 254
 * addresses we can do with a local buffer instead of allocating
 * one.
 *---------------------------------------------------------------*/

static
int
rulbus_write_range( RULBUS_EPP_IOCTL_ARGS * rulbus_arg )
{
		unsigned char data[ 254 ];
		size_t i;


		/* If only a single byte is to be written call rulbus_write() */

		if ( rulbus_arg->len == 1 )
				return rulbus_write( rulbus_arg ); 

		/* Plausibility checks */

		if ( rulbus_arg->rack & 0xF0 ) {
				printk( KERN_NOTICE "Invalid rack number.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->offset < 1 || rulbus_arg->offset > 0xFE ) {
				printk( KERN_NOTICE "Invalid address offset.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->len < 1 ) {
				printk( KERN_NOTICE
						"Invalid number of bytes to be written.\n" );
				return -EINVAL;
		}

		if ( ( unsigned long ) rulbus_arg->offset + rulbus_arg->len > 0xFE ) {
				printk( KERN_NOTICE "Address range too large.\n" );
				return -EINVAL;
		}

		if ( rulbus_arg->data == NULL ) {
				printk( KERN_NOTICE "Invalid write data pointer.\n" );
				return -EINVAL;
		}

		if ( copy_from_user( data, ( const void __user * ) rulbus_arg->data,
							 rulbus_arg->len ) ) {
				kfree( data );
				printk( KERN_NOTICE "Can't read from user space\n" );
				return -EFAULT;
		}

		/* Select the rack (if necessary) */

		if ( rulbus.rack != rulbus_arg->rack ) {
				rulbus_parport_write_addr( 0 );
				rulbus_parport_write_data( rulbus_arg->rack );
				rulbus.rack = rulbus_arg->rack;
		}

		/* Now write as many data as required to consecutive rulbus
		   addresses. */

		for ( i = 0; i < rulbus_arg->len; i++ ) {
				rulbus_parport_write_addr( rulbus_arg->offset + i );
				rulbus_parport_write_data( data[ i ] );
		}

		return rulbus_arg->len;
}


/*------------------------------------------*
 * Function to initialize the parallel port
 *------------------------------------------*/

static
int
rulbus_epp_init( void )
{
		unsigned char ctrl;


		outb( SPP_nReset, CTRL_BYTE );

		/* Reset the device by toggling the !reset bit - afterwards always
		   wait a bit for the RC-network to follow the reset line (thanks
		   to Martin Moene for sending me a patch to do that correctly). */

		ctrl = inb( CTRL_BYTE );
		outb( ctrl & ~ SPP_nReset, CTRL_BYTE );
		udelay( 1000 );
		outb( ctrl | SPP_nReset, CTRL_BYTE );
		udelay( 1000 );

		/* Finally check if the interface can be detected */

		return rulbus_epp_interface_present( );
}


/*----------------------------------------------------------------*
 * Function tries to determine if the Rulbus interface is present
 * by writing two different address patterns and then checking if
 * they can be read back.
 *----------------------------------------------------------------*/

static
int
rulbus_epp_interface_present( void )
{
		const int p1 = 0xE5;		 // write/read address bit-pattern 1
		const int p2 = 0x5E;		 // write/read address bit-pattern 2


		rulbus_parport_write_addr( p1 );

		if ( rulbus_epp_timeout_check( ) )
				return 1;

		if ( rulbus_parport_read_addr( ) != p1 )
				return 1;

		if ( rulbus_epp_timeout_check( ) )
				return 1;

		rulbus_parport_write_addr( p2 );

		if ( rulbus_epp_timeout_check( ) )
				return 1;

		if ( rulbus_parport_read_addr( ) != p2 )
				return 1;

		if ( rulbus_epp_timeout_check( ) )
				return 1;

		return 0;
}


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
 * indent-tabs-mod:
 * tab-width: 8
 * End:
 */
