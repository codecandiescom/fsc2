/*
  $Id$
 
  Driver for the RULBUS EPP interface

  Copyright (C) 2003-2004 Jens Thoms Toerring

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
#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#define RULBUS_EPP_NAME  "rulbus_epp"


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
static int rulbus_epp_init( void );
static inline int rulbus_clear_epp_timeout( void );
static inline unsigned char rulbus_epp_timout_check( void );
static inline unsigned char rulbus_parport_read_data( void );
static inline unsigned char rulbus_parport_read_addr( void );
static inline void rulbus_parport_write_data( unsigned char data );
static inline void rulbus_parport_write_addr( unsigned char addr );
static inline void rulbus_parport_reverse( void );
static int rulbus_epp_interface_present( void );


#define FORWARD     0
#define REVERSE     1


#define STATUS_BYTE         ( base + 1 )
#define CTRL_BYTE           ( base + 2 )
#define ADDR_BYTE           ( base + 3 )
#define DATA_BYTE           ( base + 4 )


/* Bits in SPP status byte */

#define EPP_Timeout         ( 1 << 0 )   /* EPP timeout (reserved in SPP) */
#define SPP_Reserved2       ( 1 << 1 )   /* SPP reserved2 */
#define SPP_nIRQ            ( 1 << 2 )   /* SPP !IRQ */
#define SPP_nError          ( 1 << 3 )   /* SPP !error */
#define SPP_SelectIn        ( 1 << 4 )   /* SPP select in */
#define SPP_PaperOut        ( 1 << 5 )   /* SPP paper out */
#define SPP_nAck            ( 1 << 6 )   /* SPP !acknowledge */
#define SPP_nBusy           ( 1 << 7 )   /* SPP !busy */


/* Bits in SPP control byte */

#define SPP_Strobe          ( 1 << 0 )   /* SPP strobe */
#define SPP_AutoLF          ( 1 << 1 )   /* SPP auto linefeed */
#define SPP_nReset          ( 1 << 2 )   /* SPP !reset */
#define SPP_SelectPrinter   ( 1 << 3 )   /* SPP select printer */
#define SPP_EnaIRQVAL       ( 1 << 4 )   /* SPP enable IRQ */
#define SPP_EnaBiDirect     ( 1 << 5 )   /* SPP enable bidirectional */
#define SPP_Unused1         ( 1 << 6 )   /* SPP unused1 */
#define SPP_Unused2	        ( 1 << 7 )   /* SPP unused2 */


struct parport_driver rulbus_drv = { RULBUS_EPP_NAME,
                                     rulbus_epp_attach,
                                     rulbus_epp_detach };

static struct rulbus_device {
        struct parport *port;
        struct pardevice *dev;
        int in_use;                 /* set when device is opened */
        int is_claimed;             /* set while we have exclusive access */
        uid_t owner;                /* current owner of the device */
        spinlock_t spinlock;
        unsigned char rack;         /* currently addressed rack */
        unsigned char direction;
} rulbus = { NULL, NULL, 0, 0, 0, { }, 0xf0, FORWARD };


struct file_operations rulbus_file_ops = {
        owner:          THIS_MODULE,
        ioctl:          rulbus_ioctl,
        open:           rulbus_open,
        release:        rulbus_release,
};


static int major = RULBUS_EPP_MAJOR;
static unsigned long base = RULBUS_EPP_BASE;



/*------------------------------------------------------*
 * Function that gets invoked when the module is loaded
 *------------------------------------------------------*/

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

        spin_lock_init( &rulbus.spinlock );

        printk( KERN_INFO RULBUS_EPP_NAME
                ": Module successfully installed\n" );

        return 0;
}


/*---------------------------------------------------*
 * Function gets invoked when the module is unloaded
 *---------------------------------------------------*/

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


/*--------------------------------------------------------------------*
 * Function that gets called automatically for each detected parport,
 * it picks the one with the right base address and tries to register
 * a driver for it.
 *--------------------------------------------------------------------*/

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
                        printk( KERN_NOTICE
                                "Device already in use by another user\n" );
                        spin_unlock( &rulbus.spinlock );
                        return -EBUSY;
                }

                if ( rulbus.dev != NULL ) {
                        rulbus.in_use++;
                        MOD_INC_USE_COUNT;
                        spin_unlock( &rulbus.spinlock );
                        return 0;
                }
        }

        /* Check that a device is registered for the port we need */

        if ( rulbus.dev == NULL ) {
                spin_unlock( &rulbus.spinlock );
                printk( KERN_NOTICE "No device registered for port\n" );
                return -EIO;
        }

        /* We need exclusive access to the port as long as the device file
           is open - the device does not allow daisy chaining since its
           using all addresses on the bus. */

        if ( parport_claim_or_block( rulbus.dev ) < 0 ) {
                spin_unlock( &rulbus.spinlock );
                printk( KERN_NOTICE "Failed to claim parallel port\n" );
                return -EIO;
        }

        if ( rulbus_epp_init( ) != 0  ) {
                printk( KERN_NOTICE "Rulbus interface not present.\n" );
                parport_release( rulbus.dev );
                spin_unlock( &rulbus.spinlock );
                return -EIO;
        }

        rulbus.is_claimed = 1;

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
                printk( KERN_NOTICE "Device not open\n" );
                return -ENODEV;
        }

        if ( rulbus.owner != current->uid &&
             rulbus.owner != current->euid &&
             ! capable( CAP_DAC_OVERRIDE ) ) {
                printk( KERN_NOTICE "No permission to release device\n" );
                return -EPERM;
        }

        /* Unclaim and unregister the parallel port */

        if ( --rulbus.in_use == 0 &&
             rulbus.dev != NULL &&
             rulbus.is_claimed ) {
                parport_release( rulbus.dev );
                rulbus.is_claimed = 0;
        }

        MOD_DEC_USE_COUNT;

        return 0;
}


/*----------------------------------------------------*
 * Function dealing with ioctl() calls for the device
 *----------------------------------------------------*/

static int rulbus_ioctl( struct inode *inode_p, struct file *file_p,
						 unsigned int cmd, unsigned long arg )
{
        RULBUS_EPP_IOCTL_ARGS rulbus_arg;
        int ret;


        if ( rulbus.dev == NULL || ! rulbus.is_claimed ) {
                printk( KERN_NOTICE "Device has been closed\n" );
                return -ENODEV;
        }

        if ( copy_from_user( &rulbus_arg, ( RULBUS_EPP_IOCTL_ARGS * ) arg,
                             sizeof rulbus_arg ) ) {
                printk( KERN_NOTICE "Can't read from user space\n" );
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
                        printk( KERN_NOTICE "Invalid ioctl() call %d\n", cmd );
                        return -EINVAL;
        }

        return ret;
}


/*-----------------------------------------------------------*
 * Function for reading data from a card in one of the racks
 *-----------------------------------------------------------*/

static int rulbus_read( RULBUS_EPP_IOCTL_ARGS *rulbus_arg )
{
        unsigned char *data;
        unsigned char *bp;
        int i;


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

        /* Allocate a buffer for reading */

		if ( rulbus_arg->data == NULL ) {
				printk( KERN_NOTICE "Invalid write data pointer.\n" );
				return -EINVAL;
		}

		data = kmalloc( rulbus_arg->len, GFP_KERNEL );

		if ( data == NULL ) {
				printk( KERN_NOTICE
						"Not enough memory for reading.\n" );
				return -ENOMEM;
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

        /* Copy all that has just been read to the user-space buffer and
		   deallocate the kernel buffer */

		if ( copy_to_user( rulbus_arg->data, data, rulbus_arg->len ) ) {
				kfree( data );
				printk( KERN_NOTICE "Can't write to user space\n" );
				return -EACCES;
		}

		kfree( data );

        return rulbus_arg->len;
}


/*---------------------------------------------------------*
 * Function for writing data to a card in one of the racks
 *---------------------------------------------------------*/

static int rulbus_write( RULBUS_EPP_IOCTL_ARGS *rulbus_arg )
{
        unsigned char *data;
        unsigned char *bp;
        int i;


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
           the RULBUS_EPP_IOCTL_ARGS structure is used, otherwise a buffer
		   needs to be allocated and everything gets copied there from user-
		   space */

        if ( rulbus_arg->len == 1 )
                data = &rulbus_arg->byte;
        else {
                if ( rulbus_arg->data == NULL ) {
                        printk( KERN_NOTICE "Invalid write data pointer.\n" );
                        return -EINVAL;
                }

                data = kmalloc( rulbus_arg->len, GFP_KERNEL );

                if ( data == NULL ) {
                        printk( KERN_NOTICE
                                "Not enough memory for reading.\n" );
                        return -ENOMEM;
                }

                if ( copy_from_user( data, rulbus_arg->data,
                                     rulbus_arg->len ) ) {
                        kfree( data );
                        printk( KERN_NOTICE "Can't read from user space\n" );
                        return -EACCES;
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

        if ( rulbus_arg->len > 1 )
                kfree( data );

        return rulbus_arg->len;
}


/*------------------------------------------*
 * Function to initialize the parallel port
 *------------------------------------------*/

static int rulbus_epp_init( void )
{
        unsigned char ctrl;


        outb( SPP_nReset, CTRL_BYTE );

        /* Reset the device by toggling the !reset bit */

        ctrl = inb( CTRL_BYTE );
        outb( ctrl & ~ SPP_nReset, CTRL_BYTE );
		udelay( 1000 );
        outb( ctrl | SPP_nReset, CTRL_BYTE );

		/* Finally check if the interface can be detected */

		return rulbus_epp_interface_present( );
}


/*-----------------------------------------------------*
 * Function to check for and clear a timeout condition
 *-----------------------------------------------------*/

static inline int rulbus_clear_epp_timeout( void )
{
        unsigned char status;


        if ( ! rulbus_epp_timout_check( ) )
                return 1;

        printk( KERN_NOTICE "EPP Timeout\n" );

        /* To clear timeout some chips require double read */

        inb( STATUS_BYTE );
        status = inb( STATUS_BYTE );

		/* Some chips reset the timeout condition by setting the lowest bit,
		   others by resetting it */

        outb( status | EPP_Timeout, STATUS_BYTE );
        outb( status & ~ EPP_Timeout, STATUS_BYTE );

        return ! rulbus_epp_timout_check( );
}


/*-------------------------------------------------------*
 * Function tests if last transfer resulted in a timeout
 *-------------------------------------------------------*/

static inline unsigned char rulbus_epp_timout_check( void )
{
        return inb( STATUS_BYTE ) & EPP_Timeout;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static inline unsigned char rulbus_parport_read_data( void )
{
        if ( rulbus.direction != REVERSE )
                rulbus_parport_reverse( );
        rulbus_clear_epp_timeout( );
        return inb( DATA_BYTE );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static inline unsigned char rulbus_parport_read_addr( void )
{
        if ( rulbus.direction != REVERSE )
                rulbus_parport_reverse( );
        rulbus_clear_epp_timeout( );
        return inb( ADDR_BYTE );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static inline void rulbus_parport_write_data( unsigned char data )
{
        if ( rulbus.direction != FORWARD )
                rulbus_parport_reverse( );
        rulbus_clear_epp_timeout( );
        outb( data, DATA_BYTE );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static inline void rulbus_parport_write_addr( unsigned char addr )
{
        if ( rulbus.direction != FORWARD )
                rulbus_parport_reverse( );
        rulbus_clear_epp_timeout( );
        outb( addr, ADDR_BYTE );
}


/*--------------------------------------------------------------------------*
 * Reverse the port direction by enabling or disabling the bi-direction bit
 *--------------------------------------------------------------------------*/

static inline void rulbus_parport_reverse( void )
{
        rulbus.direction ^= 1;
        outb( inb( CTRL_BYTE ) ^ SPP_EnaBiDirect, CTRL_BYTE );
}


/*----------------------------------------------------------------*
 * Function tries to determine if the Rulbus interface is present
 * by writing to different address patterns and then checking if
 * they can be read back.
 *----------------------------------------------------------------*/

static int rulbus_epp_interface_present( void )
{
        const int p1 = 0xE5;         // write/read address bit-pattern 1
        const int p2 = 0x5E;         // write/read address bit-pattern 2


        rulbus_parport_write_addr( p1 );

        if ( rulbus_epp_timout_check( ) )
                return 1;

        if ( rulbus_parport_read_addr( ) != p1 )
                return 1;

        if ( rulbus_epp_timout_check( ) )
                return 1;

        rulbus_parport_write_addr( p2 );

        if ( rulbus_epp_timout_check( ) )
                return 1;

        if ( rulbus_parport_read_addr( ) != p2 )
                return 1;

        if ( rulbus_epp_timout_check( ) )
                return 1;

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
 * indent-tabs-mod
 * tab-width: 8
 * End:
 */
