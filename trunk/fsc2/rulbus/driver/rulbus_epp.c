/*
  $Id$
 
  Driver for the RULBUS EPP interface

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
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.

  To contact the author send email to
  Jens.Toerring@physik.fu-berlin.de
*/


#include <linux/config.h>

#if defined( CONFIG_MODVERSIONS ) && ! defined( MODVERSIONS )
#define MODVERSIONS
#endif

#if defined( MODVERSIONS )
#include <linux/modversions.h>
#endif

#include <linux/module.h>
#include <linux/version.h>

#if ! defined( KERNEL_VERSION )
#define KERNEL_VERSION( a, b, c ) ( ( ( a ) << 16 ) | ( ( b ) << 8 ) | ( c ) )
#endif

#if defined( CONFIG_SMP ) && ! defined( __SMP__ )
#define __SMP__
#endif


#include <linux/kernel.h>
#include <linux/parport.h>
#include <linux/stddef.h>             /* for definition of NULL              */
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <asm/uaccess.h>


#if defined ( CONFIG_PROC_FS )
#include <linux/proc_fs.h>
#endif


#define RULBUS_EPP_NAME  "rulbus_epp"


#if defined RULBUS_EPP_DEBUG
#define PDEBUG( fmt, args... )                          \
    do {                                                \
            printk( KERN_DEBUG " "RULBUS_EPP_NAME ": "  \
            __FUNCTION__ "(): " fmt, ## args );         \
    } while( 0 )
#else
#define PDEBUG( fmt, args... )
#endif


#include "rulbus_epp.h"
#include "autoconf.h"


static int __init rulbus_init( void );
static void __exit rulbus_cleanup( void );
static int rulbus_open( struct inode *inode_p, struct file *file_p );
static int rulbus_release( struct inode *inode_p, struct file *file_p );
static int rulbus_ioctl( struct inode *inode_p, struct file *file_p,
			 unsigned int cmd, unsigned long arg );
static int rulbus_read( RULBUS_EPP_IOCTL_ARGS *rulbus_arg );
static int rulbus_write( RULBUS_EPP_IOCTL_ARGS *rulbus_arg );
static void rulbus_epp_attach( struct parport *port );
static void rulbus_epp_detach( struct parport *port );


struct parport_driver rulbus_drv = { RULBUS_EPP_NAME,
				     rulbus_epp_attach,
				     rulbus_epp_detach };

static struct rulbus_device {
	struct parport *port;
	struct pardevice *dev;
	unsigned char rack;
	int in_use;                 /* set when device is opened */
	int is_claimed;
	uid_t owner;                /* current owner of the device */
	spinlock_t spinlock;
} rulbus = { NULL, NULL, 0xf0, 0, 0 };


struct file_operations rulbus_file_ops = {
	owner:		THIS_MODULE,
	ioctl:          rulbus_ioctl,
	open:           rulbus_open,
	release:        rulbus_release,
};


static int major = RULBUS_EPP_MAJOR;
static unsigned long base = RULBUS_EPP_BASE;


/*-------------------------------------------------------*
 * Function that gets executed when the module is loaded
 *-------------------------------------------------------*/

static int __init rulbus_init( void )
{
	int res_major;


	/* All we do at the moment is registering a driver and a char device,
	   everything else is deferred until we get notified about the
	   parports of the system and when the device file gets opened. */

	if ( parport_register_driver( &rulbus_drv ) != 0 )
		return -EIO;

#ifdef CONFIG_DEVFS_FS
	if ( ( res_major = devfs_register_chrdev( major, RULBUS_EPP_NAME,
						  &rulbus_file_ops ) ) < 0 ) {
#else
	if ( ( res_major = register_chrdev( major, RULBUS_EPP_NAME,
					    &rulbus_file_ops ) ) < 0 ) {
#endif
		printk( KERN_ERR RULBUS_EPP_NAME ": Can't register as char "
			"device.\n" );
		parport_put_port( rulbus.port );
		return -EIO;
	}

	if ( major == 0 )
		major = res_major;

	spin_lock_init( &rulbus->spinlock );

	printk( KERN_INFO RULBUS_EPP_NAME
		": Module successfully installed\n" );

	return 0;
}


/*------------------------------------------------------*
 * Function gets executed when the module gets unloaded
 *------------------------------------------------------*/

static void __exit rulbus_cleanup( void )
{
#ifdef CONFIG_DEVFS_FS
	if ( major && devfs_unregister_chrdev( major, RULBUS_EPP_NAME ) < 0 )
#else
	if ( unregister_chrdev( major, RULBUS_EPP_NAME ) < 0 )
#endif
		printk( KERN_ERR RULBUS_EPP_NAME
			": Device busy or other module error.\n" );

	/* Unregister the device (but only if it's registered) */

	if ( rulbus.dev )
		rulbus_epp_detach( rulbus.port );

	parport_unregister_driver( &rulbus_drv );
}


/*---------------------------------------------------------------------*
 * Function that gets called automatically for each detected parport -
 * we pick the one with the right base address and try to register a
 * driver for it.
 *---------------------------------------------------------------------*/

static void rulbus_epp_attach( struct parport *port )
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

static void rulbus_epp_detach( struct parport *port )
{
	if ( rulbus.dev == NULL || rulbus.port != port )
		return;

	if ( rulbus.is_claimed )
		parport_release( rulbus.dev );
	parport_unregister_device( rulbus.dev );
	rulbus.dev = NULL;
}


/*---------------------------------------------------------------------------*
 * Function that gets called when the device file for the RULBUS interface
 * gets opened. First, we must make sure there's only a single user of the
 * device (except that root can still fiddle with it). Then, after checking
 * that a device has been registered we must claim the port - and we only
 * will unclaim it when the device file is closed, there's no daisy chaining
 * possible with this device. Next we must check that the device is present
 * and talks EPP. Only then we can be rather sure we got it.
 *---------------------------------------------------------------------------*/

static int rulbus_open( struct inode *inode_p, struct file *file_p )
{
	spin_lock( &rulbus.spinlock );

	if ( rulbus.in_use > 0 ) {
		if ( rulbus.owner != current->uid &&
		     rulbus.owner != current->euid &&
		     ! capable( CAP_DAC_OVERRIDE ) ) {
			PDEBUG( "Device already in use by another user\n" );
			spin_unlock( &rulbus.spinlock );
			return -EBUSY;
		}

		if ( rulbus.dev != NULL )
		{
			rulbus.in_use++;
			MOD_INC_USE_COUNT;
			spin_unlock( &rulbus.spinlock );
			return 0;
		}
	}

	/* Check that a device is registered for the port we need */

	if ( rulbus.dev == NULL )
	{
		spin_unlock( &rulbus.spinlock );
		PDEBUG( "No device registered for port\n" );
		return -EIO;
	}

	/* We need exclusive access to the port as long as the device file
	   is open - the device does not allow daisy chaining since its
	   using all addresses on the bus. */

	if ( parport_claim_or_block( rulbus.dev ) < 0 ) {
		spin_unlock( &rulbus.spinlock );
		PDEBUG( "Failed to claim parallel port\n" );
		return -EIO;
	}

	rulbus.is_claimed = 1;

	/* Now lets check if the peripherial speeks EPP */

	switch ( parport_negotiate( rulbus.port, PARPORT_MODE_EPP ) )
	{
		case -1 :
			parport_release( rulbus.dev );
			rulbus.is_claimed = 0;
			spin_unlock( &rulbus.spinlock );
			printk( KERN_NOTICE
				"Device not connected or non-compliant\n" );
			return -EIO;

		case 1 :
			parport_release( rulbus.dev );
			rulbus.is_claimed = 0;
			spin_unlock( &rulbus.spinlock );
			printk( KERN_NOTICE "Device is connected but can't "
				"use EPP mode\n" );
			return -EIO;
	}

	rulbus.owner = current->uid;

	rulbus.in_use++;

	MOD_INC_USE_COUNT;

	spin_unlock( &rulbus.spinlock );

	return 0;
}


/*---------------------------------------------------------------------*
 * Function that gets called when the device file for the interface is
 * closed. We must unclaim the port and unregister the device for it.
 *---------------------------------------------------------------------*/

static int rulbus_release( struct inode *inode_p, struct file *file_p )
{
	if ( rulbus.in_use == 0 ) {
		PDEBUG( "Device not open\n" );
		return -ENODEV;
	}

	if ( rulbus.owner != current->uid &&
	     rulbus.owner != current->euid &&
	     ! capable( CAP_DAC_OVERRIDE ) ) {
		PDEBUG( "No permission to release device\n" );
		return -EPERM;
	}

	/* Unclaim and unregister the parallel port */

	if ( --rulbus.in_use == 0 && rulbus.dev != NULL ) {
		if ( rulbus.is_claimed ) {
			parport_release( rulbus.dev );
			rulbus.is_claimed = 0;
		}
		parport_unregister_device( rulbus.dev );
		rulbus.dev = NULL;
	}

	MOD_DEC_USE_COUNT;

	return 0;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static int rulbus_ioctl( struct inode *inode_p, struct file *file_p,
			 unsigned int cmd, unsigned long arg )
{
	RULBUS_EPP_IOCTL_ARGS rulbus_arg;
	int ret;


	if ( rulbus.dev == NULL || ! rulbus.is_claimed ) {
		PDEBUG( "Device has been closed\n" );
		return -ENODEV;
	}

	if ( copy_from_user( &rulbus_arg, ( RULBUS_EPP_IOCTL_ARGS * ) arg,
			     sizeof rulbus_arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	switch ( cmd )
	{
		case RULBUS_EPP_IOC_READ :
			ret = rulbus_read( ( RULBUS_EPP_IOCTL_ARGS * ) arg );
			break;

		case RULBUS_EPP_IOC_WRITE :
			ret = rulbus_write( ( RULBUS_EPP_IOCTL_ARGS * ) arg );
			break;

		default :
			PDEBUG( "Invalid ioctl() call %d\n", cmd );
			return -EINVAL;
	}

	if ( ret < 0 )
		return ret;

	if ( copy_to_user( ( RULBUS_EPP_IOCTL_ARGS * ) arg, &rulbus_arg,
			   sizeof rulbus_arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EACCES;
	}

	return 0;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static int rulbus_read( RULBUS_EPP_IOCTL_ARGS *rulbus_arg )
{
	unsigned char null = 0;
	unsigned char *data;


	if ( rulbus_arg->rack & 0xF0 ) {
		PDEBUG( "Invalid rack number.\n" );
		return -EINVAL;
	}

	if ( rulbus_arg->offset < 1 || rulbus_arg->offset > 0xFE ) {
		PDEBUG( "Invalid address offset.\n" );
		return -EINVAL;
	}

	if ( rulbus_arg->len < 1 ) {
		PDEBUG( "Invalid number of bytes to be read.\n" );
		return -EINVAL;
	}

	/* If there's just a single byte to be read use the 'byte' field of the
	   RULBUS_EPP_IOCTL_ARGS structure as the buffer, otherwise we need to
	   allocate a buffer and copy everything there from user-space */

	if ( rulbus_arg->len == 1 )
		data = &rulbus_arg->byte;
	else {
		if ( rulbus_arg->data == NULL ) {
			PDEBUG( "Invalid write data pointer.\n" );
			return -EINVAL;
		}

		data = kmalloc( rulbus_arg->len, GFP_KERNEL );

		if ( data == NULL )
		{
			PDEBUG( "Not enough memory for reading.\n" );
			return -ENOMEM;
		}
	}

	/* Select the rack (if necessary) and then write the data */

	if ( rulbus.rack != rulbus_arg->rack ) {
		if ( rulbus.port->ops->epp_write_addr( rulbus.port,
						       &rulbus_arg->rack,
						       1, 0 ) != 1 ||
		     rulbus.port->ops->epp_write_data( rulbus.port, &null,
						       1, 0  ) != 1 ) {
			if ( rulbus_arg->len > 1 )
				kfree( data );
			PDEBUG( "Selecting rack failed\n" );
			return -EIO;
		}

		rulbus.rack = rulbus_arg->rack;
	}
		
	if ( rulbus.port->ops->epp_write_addr( rulbus.port,
					       &rulbus_arg->offset,
					       1, 0 ) != 1 ||
	     rulbus.port->ops->epp_read_data( rulbus.port, data,
					      rulbus_arg->len, 0  )
	     						 != rulbus_arg->len ) {
			if ( rulbus_arg->len > 1 )
				kfree( data );
		PDEBUG( "Reading data failed\n" );
		return -EIO;
	}

	/* If more than a single byte was read copy what we just read to the
	   user-space buffer and deallocate the kernel buffer */

	if ( rulbus_arg->len > 1 )
	{
		if ( copy_to_user( rulbus_arg->data, data,
				   rulbus_arg->len ) ) {
			kfree( data );
			PDEBUG( "Can't write to user space\n" );
			return -EACCES;
		}

		kfree( data );
	}

	return 0;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static int rulbus_write( RULBUS_EPP_IOCTL_ARGS *rulbus_arg )
{
	unsigned char null = 0;
	unsigned char *data;


	if ( rulbus_arg->rack & 0xF0 ) {
		PDEBUG( "Invalid rack number.\n" );
		return -EINVAL;
	}

	if ( rulbus_arg->offset < 1 || rulbus_arg->offset > 0xFE ) {
		PDEBUG( "Invalid address offset.\n" );
		return -EINVAL;
	}

	if ( rulbus_arg->len < 1 ) {
		PDEBUG( "Invalid number of bytes to be written.\n" );
		return -EINVAL;
	}

	/* If there's just a single byte to be written use the 'byte' field of
	   the RULBUS_EPP_IOCTL_ARGS structure, otherwise we need to allocate
	   a buffer and copy everything there from user-space */

	if ( rulbus_arg->len == 1 )
		data = &rulbus_arg->byte;
	else {
		if ( rulbus_arg->data == NULL ) {
			PDEBUG( "Invalid write data pointer.\n" );
			return -EINVAL;
		}

		data = kmalloc( rulbus_arg->len, GFP_KERNEL );

		if ( data == NULL )
		{
			PDEBUG( "Not enough memory for reading.\n" );
			return -ENOMEM;
		}

		if ( copy_from_user( data, rulbus_arg->data,
				     rulbus_arg->len ) ) {
			kfree( data );
			PDEBUG( "Can't read from user space\n" );
			return -EACCES;
		}
	}

	/* Select the rack (if necessary) and then read the data */

	if ( rulbus.rack != rulbus_arg->rack ) {
		if ( rulbus.port->ops->epp_write_addr( rulbus.port,
						       &rulbus_arg->rack,
						       1, 0 ) != 1 ||
		     rulbus.port->ops->epp_write_data( rulbus.port, &null,
						       1, 0  ) != 1 ) {
			if ( rulbus_arg->len > 1 )
				kfree( data );
			PDEBUG( "Selecting rack failed\n" );
			return -EIO;
		}

		rulbus.rack = rulbus_arg->rack;
	}

	if ( rulbus.port->ops->epp_write_addr( rulbus.port,
					       &rulbus_arg->offset,
					       1, 0 ) != 1 ||
	     rulbus.port->ops->epp_write_data( rulbus.port, data,
					       rulbus_arg->len, 0  )
	     						 != rulbus_arg->len ) {
		if ( rulbus_arg->len > 1 )
			kfree( data );
		PDEBUG( "Writing data failed\n" );
		return -EIO;
	}

	if ( rulbus_arg->len > 1 )
		kfree( data );

	return 0;
}



EXPORT_NO_SYMBOLS;


module_init( rulbus_init );
module_exit( rulbus_cleanup );


MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number of device file" );
MODULE_PARM( base, "i" );
MODULE_PARM_DESC( base, "Base address of parallel port the RULBUS is "
			"connected to" );
MODULE_AUTHOR( "Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>" );
MODULE_DESCRIPTION( "RULBUS parallel port (EPP) driver" );


/* MODULE_LICENSE should be defined since at least 2.4.10 */

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

