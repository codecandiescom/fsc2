/*
 *  $Id$
 */

/* Device driver for Meilhaus ME-6000 and ME-6100 boards
 * =====================================================
 *
 *   Copyright (C) 2001 Meilhaus Electronic GmbH (support@meilhaus.de)
 *
 *   This file is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *
 * Source File   : me6x00.c
 * Destination   : me6x00.o
 * Author        : GG (Guenter Gebhardt)
 * Modifications : JTT (Jens Thoms Toerring)
 *
 * File History: Version  Date      Editor  Action
 *---------------------------------------------------------------------
 *               1.00.00  02.01.21  GG      first release
 *               1.01.00  02.10.19  JTT     fixes, extensions
 *---------------------------------------------------------------------
 *
 * Description:
 *
 *   Contains the entire kernel code for controlling Meilhaus ME-6000 and
 *   ME-6100 cards. Definitions and type declarations are kept in the
 *   header file me6x00_drv.h.
 *
 *   Responsible for this version:
 *   Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>
 */


#include <linux/config.h>

#if defined( CONFIG_MODVERSIONS ) && ! defined( MODVERSIONS )
#define MODVERSIONS
#endif

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/module.h>
#include <linux/version.h>

#if defined( CONFIG_SMP ) && ! defined( __SMP__ )
#define __SMP__
#endif

/*
 * Needed for the registration of I/O and MEMORY regions.
 * (request_region, ...)
 */

#include <linux/ioport.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/unistd.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 2, 0 )
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#else
#include <linux/malloc.h>

#endif

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

/* Compatibility file for kernels from 2.0 up to 2.4 */

#include "sysdep.h"


/* Include-File for the Meilhaus ME6000 and ME6100 I/O boards */

#include "autoconf.h"
#include "me6x00_drv.h"


/* Include-File for the Xilinx Firmware */

#include "xilinx_firm.h"


/* Board specific data are kept global */

static me6x00_info_st info_vec[ ME6X00_MAX_BOARDS ];


/* Number of boards, detected from the BIOS */

static int me6x00_board_count;


/* Major Device Number, 0 means to get it automatically from the System */

static int major = ME6X00_MAJOR;


/* Structure holding information about the different board types */

static struct pci_device_id me6x00_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6004,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6008,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME600F,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6014,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6018,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME601F,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6034,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6038,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME603F,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6104,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6108,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME610F,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6114,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6118,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME611F,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6134,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME6138,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_MEILHAUS, PCI_DEVICE_ID_MEILHAUS_ME613F,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0 }
};

MODULE_DEVICE_TABLE( pci, me6x00_pci_tbl );


/* Prototypes */

static int me6x00_open( struct inode *, struct file * );
static int me6x00_release( struct inode *, struct file * );
static int me6x00_ioctl( struct inode *, struct file *,
			 unsigned int , unsigned long );
static void me6x00_isr( int, void *, struct pt_regs * );

static int me6x00_find_boards( void );
static int me6x00_init_board( int, struct pci_dev * );
static int me6x00_xilinx_download( me6x00_info_st *info );
static int me6x00_reset_board( int, int );

static int me6x00_board_info( me6x00_dev_info *arg, int minor );
static int me6x00_board_keep_volts( me6x00_keep_st *arg, int minor );
static int me6x00_set_mode( me6x00_mode_st *, int );
static int me6x00_start_stop_conv( me6x00_stasto_st *, int );
static int me6x00_clear_enable_fifo( me6x00_endis_st *, int );
static int me6x00_endis_extrig( me6x00_endis_st *, int );
static int me6x00_rifa_extrig( me6x00_rifa_st *, int );
static int me6x00_set_timer( me6x00_timer_st *arg, int minor );
static int me6x00_write_single( me6x00_single_st *arg, int minor );
static int me6x00_write_continuous( me6x00_write_st *arg, int minor );
static int me6x00_write_wraparound( me6x00_write_st *arg, int minor );

static int me6x00_buf_count( me6x00_circ_buf_st * );
static int me6x00_space_to_end( me6x00_circ_buf_st * );
static int me6x00_values_to_end( me6x00_circ_buf_st * );

static void me6x00_outb( unsigned char value, unsigned int port );
static void me6x00_outw( unsigned short value, unsigned int port );
static void me6x00_outl( unsigned int value, unsigned int port );
static unsigned char  me6x00_inb( unsigned int port );
static unsigned short me6x00_inw( unsigned int port );
static unsigned int   me6x00_inl( unsigned int port );


/* File operations provided by the driver */

static struct file_operations me6x00_file_operations = {
#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 2, 0 )
    NULL,               /* lseek              */
    NULL                /* read               */
    NULL,               /* write              */
    NULL,               /* readdir            */
    NULL,               /* select             */
    me6x00_ioctl,       /* ioctl              */
    NULL,               /* mmap               */
    me6x00_open,        /* open               */
    me6x00_release,     /* release            */
    NULL,               /* fsync              */
    NULL,               /* fasync             */
    NULL,               /* check_media_change */
    NULL                /* revalidate         */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 2, 0 ) && \
    LINUX_VERSION_CODE < KERNEL_VERSION( 2, 4, 0 )
    NULL,               /* llseek             */
    NULL,               /* read               */
    NULL,               /* write              */
    NULL,               /* readdir            */
    NULL,               /* poll               */
    me6x00_ioctl,       /* ioctl              */
    NULL,               /* mmap               */
    me6x00_open,        /* open               */
    NULL,               /* flush              */
    me6x00_release,     /* release            */
    NULL,               /* fsync              */
    NULL,               /* fasync             */
    NULL,               /* check_media_change */
    NULL,               /* revalidate         */
    NULL                /* lock               */
#elif LINUX_VERSION_CODE > KERNEL_VERSION( 2, 4, 0 )
    owner:      THIS_MODULE,
    ioctl:      me6x00_ioctl,
    open:       me6x00_open,
    release:    me6x00_release
#endif
};


/* Array of DAC register offsets */

static Registers regs[ ] = {
	{ ME6X00_CTRLREG_DAC00, ME6X00_STATREG_DAC00,
	  ME6X00_FIFOREG_DAC00, ME6X00_TIMEREG_DAC00,
	  ME6X00_IRQRREG_DAC00 },
	{ ME6X00_CTRLREG_DAC01, ME6X00_STATREG_DAC01,
	  ME6X00_FIFOREG_DAC01, ME6X00_TIMEREG_DAC01,
	  ME6X00_IRQRREG_DAC01 },
	{ ME6X00_CTRLREG_DAC02, ME6X00_STATREG_DAC02,
	  ME6X00_FIFOREG_DAC02, ME6X00_TIMEREG_DAC02,
	  ME6X00_IRQRREG_DAC02 },
	{ ME6X00_CTRLREG_DAC03, ME6X00_STATREG_DAC03,
	  ME6X00_FIFOREG_DAC03, ME6X00_TIMEREG_DAC03,
	  ME6X00_IRQRREG_DAC03 }
};


static unsigned int sval_regs[ ] = {
	ME6X00_SVALREG_DAC00,
	ME6X00_SVALREG_DAC01,
	ME6X00_SVALREG_DAC02,
	ME6X00_SVALREG_DAC03,
	ME6X00_SVALREG_DAC04,
	ME6X00_SVALREG_DAC05,
	ME6X00_SVALREG_DAC06,
	ME6X00_SVALREG_DAC07,
	ME6X00_SVALREG_DAC08,
	ME6X00_SVALREG_DAC09,
	ME6X00_SVALREG_DAC10,
	ME6X00_SVALREG_DAC11,
	ME6X00_SVALREG_DAC12,
	ME6X00_SVALREG_DAC13,
	ME6X00_SVALREG_DAC14,
	ME6X00_SVALREG_DAC15
};



/*
 * Routine:
 *   me6x00_init / init_module
 *
 * Description:
 *   This function is called by the kernel when the module is loaded. It
 *   scans the PCI-Bus for ME6x00 and ME6100 boards.
 *   Actions:
 *   - A maximum of ME6X00_MAX_BOARDS are initialized by the
 *     function me6x00_find_boards()
 *   - It stores the number of detected boards in the global
 *     variable me6x00_board_count.
 *   - It registers the abilities of the driver in the system and
 *     creates an entry in /proc/drivers (register_chrdev).
 *
 * Parameter list:
 *   Name           Type          Access             Description
 *--------------------------------------------------------------------------
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 4, 0 )
static int __init me6x00_init( void )
#else
int init_module( void )
#endif
{
#ifdef CONFIG_PCI
	int res_major;
	int i;


	CALL_PDEBUG( "init_module() is executed\n" );

	if ( ! pci_present( ) ) {
		printk( KERN_ERR "ME6X00: init_module(): "
			"No PCI-BIOS present!\n");
		return -ENODEV;
	}

	if ( ( me6x00_board_count = me6x00_find_boards( ) ) == 0 ) {
		printk( KERN_ERR "ME6x00: init_module(): "
			"No ME6x00 bords found\n" );
		return -ENODEV;
	}

	/* A ngative result means we couldn't initialize one of the boards */

	if ( me6x00_board_count < 0 )
		return -EIO;

	PDEBUG( "init_module(): %d board(s) found\n", me6x00_board_count );

#ifdef CONFIG_DEVFS_FS
	if ( ( res_major = devfs_register_chrdev( major, ME6X00_NAME,
					    &me6x00_file_operations ) ) < 0 ) {
#else
	if ( ( res_major = register_chrdev( major, ME6X00_NAME,
					    &me6x00_file_operations ) ) < 0 ) {
#endif
		printk( KERN_ERR "ME6X00: init_module(): "
			"Can't get assigned a major number\n" );
		return -ENODEV;
	}

	if ( major == 0 )
		major = res_major;

	printk( KERN_INFO "ME6X00: driver succesfully installed.\n" );

	for ( i = 0; i < me6x00_board_count; i++ )
		printk( KERN_INFO "ME6X00: Board %d: type is %s\n",
			i, info_vec[ i ].name );

	return 0;

#else
	printk( KERN_ERR "ME6X00: init_module(): "
		"Missing PCI support in kernel.\n" );
	return -ENODEV
#endif
}


/*
 * Routine:
 *   me6x00_exit / cleanup_module
 *
 * Description:
 *   This routine is called when the module is removed from the kernel.
 *   It unregisters the module from the system.
 *
 * Parameter list:
 *   Name       Type              Access    Description
 *--------------------------------------------------------------------------
 *
 * Result:
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 4, 0 )
static void __exit me6x00_exit( vod )
#else
void cleanup_module( void )
#endif
{
	int i;


	for ( i = 0; i < me6x00_board_count; i++ ) {
		me6x00_reset_board( i, FROM_CLEANUP );
		pci_release_regions( info_vec[ i ].dev );
	}

#ifdef CONFIG_DEVFS_FS
	if ( major && devfs_unregister_chrdev( major, ME6x00_NAME ) < 0 )
#else
	if ( unregister_chrdev( major, ME6X00_NAME ) )
#endif
		printk( KERN_ERR "ME6X00: cleanup_module(): "
			"can't unregister module.\n" );
	else
		printk( KERN_INFO "ME6X00: driver de-installed.\n" );
}


/*
 * Routine:
 *   me6x00_find_boards
 *
 * Description:
 *   This function searches for and initializes all ME6x00 boards
 *   Actions:
 *   - If a board is found it's initialized by the
 *     subroutine me6x00_init_board.
 *
 * Result:
 *   On success the return value is the number of found boards,
 *   a negative value indicates failure to initialize one of the board
 *--------------------------------------------------------------------------
 * Author: JTT
 * Modification:
 */

static int me6x00_find_boards( void )
{
	int board_count = 0;
	unsigned int max_types;
	unsigned int i;
	static struct pci_dev *dev = NULL;


	max_types = sizeof me6x00_pci_tbl / sizeof me6x00_pci_tbl[ 0 ];

	while ( board_count < ME6X00_MAX_BOARDS &&
		NULL != ( dev = pci_find_device( PCI_VENDOR_ID_MEILHAUS,
						 PCI_ANY_ID, dev ) ) ) {
		/* Check that board we found is a ME6X00 or ME6100 card */

		for ( i = 0; i < max_types; i++ )
			if ( dev->device == me6x00_pci_tbl[ i ].device )
				break;

		if ( i == max_types )
			continue;

		/* Initialize the board */

		if ( me6x00_init_board( board_count, dev ) ) {
			printk( KERN_ERR "ME6X00: init_module(): "
				"Can't initialize board\n" );
			return -1;
		}

		board_count++;
	}

	if ( board_count == ME6X00_MAX_BOARDS )
		printk( KERN_WARNING "ME6X00: init_module(): Possibly more "
			"than %d boards!\n", ME6X00_MAX_BOARDS );

	return board_count;
}


/*
 * Routine:
 *   me6x00_init_board
 *
 * Description:
 *   This function initializes the global vector info_vec for each board.
 *   Actions:
 *   - It gets the base addresses of the ME6000 and saves them in
 *     the board context.
 *   - It downloads the firmware to the board.
 *   - It executes the subroutine me6x00_reset_board to bring the
 *     board into a known state.
 *
 * Parameter list:
 *   Name           Type          Access    Description
 *--------------------------------------------------------------------------
 *   board_count    int           read      Index of the detected board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_init_board( int board_count, struct pci_dev *dev )
{
	unsigned int i;
	me6x00_info_st *info;


	CALL_PDEBUG( "me6x00_init_board() is executed\n" );

        if ( pci_enable_device( dev ) )
		return -1;

	info = info_vec + board_count;
	info->dev = dev;

	if ( pci_request_regions( dev, ME6X00_NAME ) ) {
		printk( KERN_ERR "ME6X00: me6x00_init_board(): "
			"Failed to obtain required I/O regions\n" );
		return -1;
	}

	/*------------------------ plx regbase ------------------------------*/

	info->plx_regbase = pci_resource_start( dev, 1 );
	info->plx_regbase_size = pci_resource_len( dev, 1 );

	if ( ! ( pci_resource_flags( dev, 1 ) & IORESOURCE_IO ) ) {
		printk( KERN_ERR "ME6X00: me6x00_init_board(): "
			"PLX regbase is not I/O\n" );
		goto init_failure;
	}

	PDEBUG( "me6x00_init_board(): PLX at 0x%08X[ 0x%08X ]\n",
		info->plx_regbase, info->plx_regbase_size );

	/*------------------------ me6x00 regbase ---------------------------*/

	info->me6x00_regbase = pci_resource_start( dev, 2 );
	info->me6x00_regbase_size = pci_resource_len( dev, 2 );

	if ( ! ( pci_resource_flags( dev, 2 ) & IORESOURCE_IO ) ) {
		printk( KERN_ERR "ME6X00: me6x00_init_board(): "
			"ME6X00 regbase is not I/O\n" );
		goto init_failure;
	}

	PDEBUG( "me6x00_init_board(): me6x00 at 0x%08X[ 0x%08X ]\n",
		info->me6x00_regbase, info->me6x00_regbase_size );

	/*------------------------ xilinx regbase ---------------------------*/

	info->xilinx_regbase = pci_resource_start( dev, 3 );
	info->xilinx_regbase_size = pci_resource_len( dev, 3 );

	if ( ! ( pci_resource_flags( dev, 3 ) & IORESOURCE_IO ) ) {
		printk( KERN_ERR "ME6X00: me6x00_init_board(): "
			"Xilinx regbase is not I/O\n" );
		goto init_failure;
	}

	PDEBUG( "me6x00_init_board(): xilinx at 0x%08X[ 0x%08X ]\n",
		info->xilinx_regbase, info->xilinx_regbase_size );

	/*---------------------- Init the board info ------------------------*/

	info->pci_dev_no = PCI_FUNC( dev->devfn );
	PDEBUG( "me6x00_init_board(): pci_func_no = 0x%x\n",
		info->pci_func_no );

	info->pci_dev_no = PCI_SLOT( dev->devfn );
	PDEBUG( "me6x00_init_board(): pci_dev_no = 0x%x\n", info->pci_dev_no );

	info->pci_bus_no = dev->bus->number;
	PDEBUG( "me6x00_init_board(): pci_bus_no = 0x%x\n", info->pci_bus_no );

	info->vendor_id = dev->vendor;
	PDEBUG( "me6x00_init_board(): vendor_id = 0x%x\n", info->vendor_id );

	info->device_id = dev->device;
	sprintf( info->name, "ME%X", info->device_id );
	PDEBUG( "me6x00_init_board(): device_id = 0x%x\n", info->device_id );

	if ( PCIBIOS_SUCCESSFUL != pci_read_config_byte( dev, 0x08,
							 &info->hw_revision ) )
	{
		printk( KERN_WARNING "ME6X00: me6x00_init_module: "
			"Can't get hw_revision\n" );
		goto init_failure;
	}

	PDEBUG( "me6x00_init_board(): hw_revision = 0x%x\n",
		info->hw_revision );

	info->serial_no = ( ( unsigned int ) dev->subsystem_device ) << 16 |
			  dev->subsystem_vendor;

	PDEBUG( "me6x00_init_board(): serial_no = 0x%x\n", info->serial_no );

	info->irq = dev->irq;
	PDEBUG( "me6x00_init_board(): irq = %d\n", info->irq );

	info->board_in_use = 0;

	for ( i = 0; i < 4; i++ ) {
		info->buf[ i ].buf = NULL;
		info->buf[ i ].head = info->buf[ i ].tail = 0;
		init_waitqueue_head( info->wait_queues + i );
	}

	/* Initialize the spinlock that's going to be used to avoid two
	   processes opening the same board at the same time */

	spin_lock_init( &info->use_lock );

	/* Download the Xilinx firmware, but only if the the reading the
	   first byte of the I/O range for the Xilinx FPGA does not return
	   0x74 - as far a I found out in this case the chip has already
	   been initialized and trying to do so a second time will fail. */

	if ( inb( info->xilinx_regbase ) != 0x74 &&
	     me6x00_xilinx_download( info ) ) {
		printk( KERN_ERR "ME6X00: me6x00_init_board: "
			"Can't download firmware\n" );
		goto init_failure;
	}

	switch ( info->device_id & 0xF ) {
		case 0x04 :
			info->num_dacs = 4;
			break;

		case 0x08 :
			info->num_dacs = 8;
			break;

		case 0x0F :
			info->num_dacs = 16;
	}

	for ( i = 0; i < info->num_dacs; i++ )
		info->keep_voltage_on_close[ i ] = 0;

	/* Now reset the board to bring it into a known state */

	if ( me6x00_reset_board( board_count, FROM_INIT ) ) {
		printk( KERN_ERR "ME6X00: me6x00_init_board: "
			"Can't reset board\n");
		goto init_failure;
	}

	return 0;

 init_failure:

	pci_release_regions( dev );
	return -1;
}


/*
 * Routine
 *   me6x00_xilinx_download
 *
 * Description:
 *   This function downloads the firmware to the xilinx.
 *
 * Parameter list:
 *   Name       Type               Access    Description
 *--------------------------------------------------------------------------
 *   info       me6x00_info_st *   read      pointer to structure with
 *                                           information about board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */


#define XILINX_INIT_COUNT 100    /* all of these are quite large, in tests */
#define XILINX_BUSY_COUNT 100    /* I found that none of the loops where   */
#define XILINX_DONE_COUNT 100    /* run more than once                     */

static int me6x00_xilinx_download( me6x00_info_st *info )
{
	int size = 0;
	int value = 0;
	int idx = 0;
	int i;


	PDEBUG( "me6x00_xilinx_download() is executed\n" );

	/* Set PLX local interrupt 2 polarity to high (necessary) */

	me6x00_outb( 0x10, info->plx_regbase + PLX_INTCSR );

	/* Set CSWrite bit of Xilinx FPGA */

	value = me6x00_inw( info->plx_regbase + PLX_ICR ) | 0x0100;
	me6x00_outw( value, info->plx_regbase + PLX_ICR );

	/* Init Xilinx with CS1 */

	value = me6x00_inb( info->xilinx_regbase + 0x200 );

	/* Wait for init bit to become set */

	for ( i = 0; i < XILINX_INIT_COUNT; i++ )
		if ( me6x00_inb( info->plx_regbase + PLX_INTCSR ) & 0x20 )
			break;

	if ( i == XILINX_INIT_COUNT ) {
		printk( KERN_ERR "ME6X00: me6x00_xilinx_download(): "
			"maximum wait for init bit reached!\n" );
		return -EIO;
	}

	/* Reset CSWrite of Xilinx in order to reset Xilinx */

	value = me6x00_inw( info->plx_regbase + PLX_ICR ) & 0xFEFF;
	me6x00_outw( value, info->plx_regbase + PLX_ICR );

	/* Download Xilinx firmware */

	size =   ( xilinx_firm[ 0 ] << 24 ) + ( xilinx_firm[ 1 ] << 16 )
	       + ( xilinx_firm[ 2 ] <<  8 ) +   xilinx_firm[ 3 ];

	for ( idx = 0; idx < size; idx++ ) {
		outb( xilinx_firm[ 16 + idx ], info->me6x00_regbase );

		/* Wait for busy flag to go low */

		for ( i = 0; i < XILINX_BUSY_COUNT; i++ ) {
			value = me6x00_inw( info->plx_regbase + PLX_ICR );
			if ( ! ( value & 0x0800 ) )   /* busy flag */
				break;
		}

		if ( i == XILINX_BUSY_COUNT ) {
			udelay( 1000 );
			value = me6x00_inw( info->plx_regbase + PLX_ICR );
			if ( ! ( value & 0x0800 ) )   /* busy flag */
				break;

			printk( KERN_ERR "ME6X00: me6x00_xilinx_down_load(): "
				"Maximum wait for !busy reached!\n" );
			return -EIO;
		}

		if ( value & 0x04 )    /* check also the done flag */
			break;
	}

	PDEBUG( "me6x00_download_xilinx(): %d bytes written\n", idx );

	/* Wait for done flag to become set */

	for ( i = 0; i < XILINX_DONE_COUNT; i++ ) {
		if ( me6x00_inw( info->plx_regbase + PLX_ICR ) & 0x04 )
			break;
	        udelay( 1000 );
	}

	if ( i == XILINX_DONE_COUNT ) {
		PDEBUG( "me6x00_download_xilinx(): Done flag not set\n" );
		printk( KERN_ERR "ME6X00: me6x00_download_xilinx(): "
			"Download failed\n" );
		return -EIO;
	}

	PDEBUG( "me6x00_download_xilinx(): Done flag is set\n"
		"me6x00_download_xilinx(): Download successful\n" );

	/* Write User DIO (CSWrite, reset Xilinx) */

	value = me6x00_inw( info->plx_regbase + PLX_ICR ) | 0x0100;
	me6x00_outw( value,info->plx_regbase + PLX_ICR );

	return 0;
}


/*
 * Routine
 *   me6x00_reset_board
 *
 * Description:
 *   This function resets the board indexed by board_count. It sets the
 *   mode of all DACs to single mode, sets the analog value to 0x7FFF (but
 *   not always, see below) and resets interrupts that may still be pending.
 *   At last it enables the interrupt logic of the PLX.
 *
 * Parameter list:
 *   Name           Type          Access    Description
 *--------------------------------------------------------------------------
 *   board_count    int           r         Index of the detected board
 *   from           int           r         shows from where we were called
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_reset_board( int board_count, int from )
{
	unsigned int i;
	unsigned int base;
	unsigned int icr;


	CALL_PDEBUG( "me6x00_reset_board() is executed\n" );

	/* I have no idea yet what the following write to the PLX register
	   is supposed to do. But I found that it won't let me keep the
	   current voltage of the DACs. On the other hand, at least for
	   ME6000 boards irt doesn't seem to lead to any problems when these
	   lines are left out.                       JTT
	*/

	if ( from != FROM_CLOSE ) {
		base = info_vec[ board_count ].plx_regbase;
		icr = inl( base + PLX_ICR );
		icr |= 0x40000000;
		PDEBUG( "me6x00_reset_board: write 0x%08X to plx with offset "
			"= 0x%02X\n", icr, PLX_ICR );
		outl( icr, base + PLX_ICR );

		icr &= ~0x40000000;
		PDEBUG( "me1600_reset_board: write 0x%08X to plx with offset "
			"= 0x%02X\n", icr, PLX_ICR );
		outl( icr, base + PLX_ICR );
	}

	base = info_vec[ board_count ].me6x00_regbase;

	/* Clear the control registers of all DACs */

	for ( i = 0; i < 4; i++ )
		me6x00_outl( 0x0, base + regs[ i ].ctrl );

	/* Sending 0x7FFF to the DACs means an output voltage of 0V */
	/*                                                          */
	/* There's a problem here: I don't want the voltage to be   */
	/* set to 0 V when closing the boards file descriptor. To   */
	/* switch to this behaviour one has to set the "keep        */
	/* voltage on close" property for each DAC of the board.    */

	for ( i = 0; i < info_vec[ board_count ].num_dacs; i++ )
		if ( from == FROM_CLOSE &&
		     ! info_vec[ board_count ].keep_voltage_on_close[ i ] )
		{
			PDEBUG( "Resetting DAC%02d\n", i );
			me6x00_outl( 0x7FFF, base + sval_regs[ i ] );
		}

	/* Reset the interrupts */

	me6x00_inl( base + regs[ i ].irqr );

	/* Enable interrupts on the PLX */

	if ( from != FROM_CLOSE )
		me6x00_outb( 0x43, info_vec[ board_count ].plx_regbase 
			     + PLX_INTCSR );

	return 0;
}


/*
 * Routine:
 *   me6x00_open
 *
 * Description:
 *   Function which is executed when a user program calls open() on a board
 *   device file. It first checks that the board to open is present and not
 *   already used by another program. Then it requests the interrupt line for
 *   the board from the system.  It allocates the buffers for the ME6100
 *   versions. Finally, it marks the board as used and increments the systems
 *   counter for the module.
 *
 * Parameter list:
 *   Name       Type            Access    Description
 *--------------------------------------------------------------------------
 *   inode_ptr  struct inode *  read      pointer to the inode structure of
 *                                        the system
 *   file_p     struct file *   read      pointer to the file structure of
 *                                        the system
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_open( struct inode *inode_p, struct file *file_p )
{
	int minor = 0;
	int i;
	me6x00_info_st *info;


	file_p = file_p;
	CALL_PDEBUG( "me6x00_open() is executed\n" );

	minor = MINOR( inode_p->i_rdev );

	if ( minor >= me6x00_board_count ) {
		printk( KERN_ERR "ME6X00: me6x00_open(): Board %d does not "
			"exist\n", minor );
		return -ENODEV;
	}

	info = info_vec + minor;

	/* Use a spinlock to avoid a race condition between two processes
	   trying to open the same board at the same time on SMP machines */

	spin_lock( &info->use_lock );

	if ( info->board_in_use ) {
		printk( KERN_ERR "ME6X00: me6x00_open(): Board %d already "
			"in use\n", minor );
		spin_unlock( &info->use_lock );
		return -EBUSY;
	}

	info->board_in_use = 1;
	spin_unlock( &info->use_lock );

	if ( IS_ME6100( minor ) ) {
		CALL_PDEBUG( "Got a ME6100 board\n" );

		if ( request_irq( info->irq, me6x00_isr,
				  SA_INTERRUPT | SA_SHIRQ,
				  ME6X00_NAME, info ) ) {
			printk( KERN_WARNING "ME6X00: me6x00_open(): "
				"Can't get interrupt line" );
			return -ENODEV;
		}

		for ( i = 0; i < 4; i++ ) {
			if ( ! ( info->buf[ i ].buf =
				 kmalloc( ME6X00_BUFFER_SIZE, GFP_KERNEL ) ) )
				break;
			memset( info_vec[ minor ].buf[ i ].buf, 0,
				ME6X00_BUFFER_SIZE );
		}

		if ( i < 4 ) {
			printk( KERN_ERR "ME6X00: me6x00_open(): "
				"Can't get memory\n" );
			for ( --i; i >= 0; --i )
				kfree( info->buf[ i ].buf );
			free_irq( info->irq, info );
			info->board_in_use = 0;
			return -ENODEV;
		}

	}

	MOD_INC_USE_COUNT;
	return 0;
}


/*
 * Routine:
 *   me6x00_release
 *
 * Description:
 *   Function is executed when the user program calls close() on the file
 *   descriptor associated with a board. First it resets the board and marks
 *   the board as unused in the global array info_vec. Then it frees the
 *   interrupt and memory requested in me6x00_open(). At last the use count
 *   of the module is decremented.
 *
 * Parameter list:
 *   Name       Type            Access    Description
 *--------------------------------------------------------------------------
 *   inode_p    struct inode *  read      pointer to the inode structure of
 *                                        the system
 *   file_p     struct file *   read      pointer to the file structure of
 *                                        the system
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_release( struct inode *inode_p, struct file *file_p )
{
	unsigned int i;
	int minor = 0;
	me6x00_info_st *info;


	file_p = file_p;
	CALL_PDEBUG( "me6x00_release() is executed\n" );

	minor = MINOR( inode_p->i_rdev );
	if ( me6x00_reset_board( minor, FROM_CLOSE ) ) {
		printk( KERN_ERR "ME6X00: me6x00_release(): Can't reset "
			"board %d\n", minor );
		return 1;
	}

	info = info_vec + minor;

	for ( i = 0; i < info->num_dacs; i++ )
		info->keep_voltage_on_close[ i ] = 0;

	info->board_in_use = 0;

	if ( IS_ME6100( minor ) ) {
		free_irq( info->irq, info );

		for ( i = 0; i < 4; i++ ) {
			kfree( info->buf[ i ].buf );
			info->buf[ i ].head = 0;
			info->buf[ i ].tail = 0;
		}
	}

	MOD_DEC_USE_COUNT;
	return 0;
}


/*
 * Routine:
 *   me6x00_ioctl
 *
 * Description:
 *   Function which provides the functionality of the ME6X00 board. This
 *   function is executed when a user program executes the system call
 *   ioctl(). It checks if the service requested is valid and calls the
 *   appropriate routine to handle the request.
 *
 * Parameter list:
 *   Name       Type            Access    Description
 *--------------------------------------------------------------------------
 *   inode_p    struct inode *  read      pointer to the inode structure of
 *                                        the system
 *   file_p     struct file *   read      pointer to the file structure of
 *                                        the system
 *   service    unsigned int    read      requested service
 *   arg        unsigned int    r/w       address of the structure with
 *                                        user data and parameters
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_ioctl( struct inode * inode_p, struct file *file_p,
			 unsigned int service, unsigned long arg )
{
	int minor = 0;


	file_p = file_p;
	CALL_PDEBUG( "me6x00_ioctl() is executed\n" );

	minor = MINOR( inode_p->i_rdev );

	if ( minor >= me6x00_board_count ) {
		printk( KERN_ERR "ME6X00: me6x00_open(): Board %d does not "
			"exist\n", minor );
		return -ENODEV;
	}

	if ( _IOC_TYPE( service ) != ME6X00_MAGIC ) {
		PDEBUG( "me6x00_ioctl(): Wrong magic number\n" );
		return -EINVAL;
	}

	if ( _IOC_NR( service ) < ME6X00_IOCTL_MINNR ) {
		PDEBUG( "me6x00_ioctl(): IOCTL number low low\n" );
		return -EINVAL;
	}

	if ( _IOC_NR( service ) > ME6X00_IOCTL_MAXNR ) {
		PDEBUG( "me6x00_ioctl(): Service number too high\n" );
		return -EINVAL;
	}

	switch ( service ) {
		case ME6X00_SET_MODE :
			return me6x00_set_mode( ( me6x00_mode_st * ) arg,
						minor );

		case ME6X00_START_STOP_CONV :
			return me6x00_start_stop_conv(
					   ( me6x00_stasto_st * ) arg, minor );

		case ME6X00_CLEAR_ENABLE_FIFO :
			return me6x00_clear_enable_fifo(
					    ( me6x00_endis_st * ) arg, minor );

		case ME6X00_ENDIS_EXTRIG :
			return me6x00_endis_extrig(
					    ( me6x00_endis_st * ) arg, minor );

		case ME6X00_RIFA_EXTRIG :
			return me6x00_rifa_extrig(
					     ( me6x00_rifa_st * ) arg, minor );

		case ME6X00_SET_TIMER :
			return me6x00_set_timer(
					    ( me6x00_timer_st * ) arg, minor );

		case ME6X00_WRITE_SINGLE :
			return me6x00_write_single(
					   ( me6x00_single_st * ) arg, minor );

		case ME6X00_WRITE_CONTINUOUS :
			return me6x00_write_continuous(
					    ( me6x00_write_st * ) arg, minor );

		case ME6X00_WRITE_WRAPAROUND :
			return me6x00_write_wraparound(
					    ( me6x00_write_st * ) arg, minor );

		case ME6X00_RESET_BOARD :
			return me6x00_reset_board( minor, FROM_IOCTL );

		case ME6X00_BOARD_INFO :
			return me6x00_board_info(
					    ( me6x00_dev_info * ) arg, minor );

		case ME6X00_KEEP_VOLTAGE :
			return me6x00_board_keep_volts(
					     ( me6x00_keep_st * ) arg, minor );

		default :
			return -EINVAL;
	}

	return 0;
}


/*
 * Routine:
 *   me6x00_set_mode
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to set up a structure
 *   with informations about a board to be returned to the user.
 *
 *
 * Parameter list:
 *   Name    Type             Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_dev_info*   w    structure for returning device information
 *   minor   int                r    specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: JTT
 * Modification:
 */

static int me6x00_board_info( me6x00_dev_info *arg, int minor )
{
	me6x00_dev_info dev_info;
	me6x00_info_st *info;


	CALL_PDEBUG( "me6x00_board_info() is executed\n" );

	info = info_vec + minor;

	dev_info.vendor_ID   = info->vendor_id;
	dev_info.device_ID   = info->device_id;
	dev_info.serial_no   = info->serial_no;
	dev_info.hw_revision = info->hw_revision;
	dev_info.bus_no      = info->pci_bus_no;
	dev_info.dev_no      = info->pci_dev_no;
	dev_info.func_no     = info->pci_func_no;

	if ( copy_to_user( arg, &dev_info, sizeof *arg ) )
		return -EFAULT;

	return 0;
}


/*
 * Routine:
 *   me6x00_board_keep_volts
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to set the behaviour
 *   of one of the DACs of the board after the device file is closed. It can
 *   be set to keep outputing the last set voltage or to switch back to 0 V.
 *
 *
 * Parameter list:
 *   Name    Type             Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_keep_st *   r    specifies DAC and mode
 *   minor   int                r    specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: JTT
 * Modification:
 */

static int me6x00_board_keep_volts( me6x00_keep_st *arg, int minor )
{
	me6x00_keep_st keep;
	

	CALL_PDEBUG( "me6x00_board_keep() is executed\n" );

	if ( copy_from_user( &keep, arg, sizeof *arg ) )
		return -EFAULT;

	if ( keep.dac >= info_vec[ minor ].num_dacs ) {
		printk( KERN_ERR "ME6X00: me6x00_board_keep(): No such "
			"DAC for this board\n" );
		return -EINVAL;
	}

	info_vec[ minor ].keep_voltage_on_close[ keep.dac ] = keep.value;

	PDEBUG( "Setting keep = %d for DAC %d\n", keep.value, keep.dac );

	return 0;
}


/*
 * Routine:
 *   me6x00_set_mode
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to set the mode of
 *   a DAC to single, continuous or wrap around mode.
 *
 * Parameter list:
 *   Name    Type                      Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_mode_st*           r       Specifies the DAC and mode
 *   minor   int                       r       specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_set_mode( me6x00_mode_st *arg, int minor )
{
	me6x00_mode_st dacmode;
	unsigned int reg = 0;
	unsigned int port = 0;


	CALL_PDEBUG( "me6x00_set_mode() is executed\n" );

	if ( ! IS_ME6100( minor ) ) {
		printk( KERN_ERR "ME6X00: me6x00_set_mode(): Not a ME6100\n" );
		return -ENODEV;
	}

	if ( copy_from_user( &dacmode, arg, sizeof *arg ) )
		return -EFAULT;

	if ( dacmode.dac > ME6X00_DAC03 )
	{
		printk( KERN_ERR "ME6X00: me6x00_set_mode(): No such "
			"function for DAC %d\n", dacmode.dac );
		return -EINVAL;
	}

	port = info_vec[ minor ].me6x00_regbase + regs[ dacmode.dac ].ctrl;
	reg = me6x00_inl( port );

	/*
	 * Conversion must be stopped before reconfiguring the DAC!
	*/

	reg |= 0x4;
	me6x00_outl( reg, port );

	reg &= 0xFFFFFFFC;
	reg |= ( unsigned int ) dacmode.mode & 0x3;
	me6x00_outl( reg, port );
	info_vec[ minor ].buf[ dacmode.dac ].head = 0;
	info_vec[ minor ].buf[ dacmode.dac ].tail = 0;

	return 0;
}


/*
 * Routine:
 *   me6x00_start_stop_conv
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to start or stop
 *   the conversion of the values stored in the FIFO.
 * Parameter list:
 *   Name    Type                      Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_stasto_st*         r       specifies the DAC and
 *                                             start/stop
 *   minor   int                       r       specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_start_stop_conv( me6x00_stasto_st *arg, int minor )
{
	me6x00_stasto_st conv;
	me6x00_info_st *info;
	unsigned int ctrl_reg;
	unsigned int ctrl_port;
	unsigned int stat_port;
	wait_queue_head_t wait;


	init_waitqueue_head( &wait );

	CALL_PDEBUG( "me6x00_start_stop_conv() is executed\n" );

	if ( ! IS_ME6100( minor ) ) {
		printk( KERN_ERR "ME6X00: me6x00_start_stop_conv(): "
			"Not a ME6100\n" );
		return -ENODEV;
	}

	if ( copy_from_user( &conv, arg, sizeof *arg ) )
		return -EFAULT;

	if ( conv.dac > ME6X00_DAC03 ) {
		printk( KERN_ERR "ME6X00: me6x00_start_stop_conv(): "
			"No such function for DAC %d\n", conv.dac );
		return -EINVAL;
	}

	info = info_vec + minor;

	ctrl_port = info->me6x00_regbase + regs[ conv.dac ].ctrl;
	stat_port = info->me6x00_regbase + regs[ conv.dac ].stat;

	ctrl_reg = me6x00_inl( ctrl_port );

	/* Starting is simple... */

	if ( conv.stasto == ME6X00_START ) {
		ctrl_reg &= ~0x4;
		me6x00_outl( ctrl_reg, ctrl_port );

		/* If external trigger is disabled start conversion by a
		   dummy write of 0 V */

		if ( ! ( 0x10 & me6x00_inl( ctrl_port ) ) )
			me6x00_outl( 0x7FFF, info->me6x00_regbase
				             + sval_regs[ conv.dac  ] );
		return 0;
	}

	/* Already stopped ? */

	if ( ctrl_reg & 0x4 )
		return 0;

	ctrl_reg |= 0x4;
	me6x00_outl( ctrl_reg, ctrl_port );

	/* In wrap-around mode we have to wait until the FIFO is empty */

	if ( ( ctrl_reg & 0x3 ) == 0x1 ) {
		while ( me6x00_inl( stat_port ) & 0x8 ) {
			PDEBUG( "me6x00_start_stop_conv(): Sending "
				"process to sleep\n" );

			interruptible_sleep_on_timeout( &wait, 10 );

			/*
			 * There are two reasons, why a wake up may occur:
			 * 1) the timeout is reached.
			 * 2) a signal was sent to the process (CTRL+C, ...)
			 */

			if ( signal_pending( current ) ) {
				printk( KERN_WARNING
					"ME6X00: me6x00_start_stop_conv(): "
					"aborted by signal\n" );
				return -EINTR;
			}
		}
	}

	return 0;
}


/*
 * Routine:
 *   me6x00_clear_enable_fifo
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to clear the contents
 *   of the DAC FIFOs or to enable them.
 *
 * Parameter list:
 *   Name    Type                      Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_fifo_st*           r       specifies the DAC and cl/en
 *   minor   int                       r       specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_clear_enable_fifo( me6x00_endis_st *arg, int minor )
{
	me6x00_endis_st fifo;
	unsigned int reg;
	unsigned int port;


	CALL_PDEBUG( "me6x00_clear_enable_fifo() is executed\n" );

	if ( ! IS_ME6100( minor ) ) {
		printk( KERN_ERR "ME6X00: me6x00_clear_enable_fifo(): "
			"Not a ME6100\n" );
		return -ENODEV;
	}

	if ( copy_from_user( &fifo, arg, sizeof *arg ) )
		return -EFAULT;

	if ( fifo.dac > ME6X00_DAC03 ) {
		printk( KERN_ERR "ME6X00: me6x00_clear_enable_fifo(): "
			"No such function for DAC %d\n", fifo.dac );
		return -EINVAL;
	}

	port = info_vec[ minor ].me6x00_regbase + regs[ fifo.dac ].ctrl;
	reg = me6x00_inl( port );

	/*
	 * Conversion must be stopped before reconfiguring the DAC!
	 */

	reg |= 0x4;
	me6x00_outl( reg, port );

	if ( fifo.endis == ME6X00_ENABLE ) {
		reg |= 0x00000008;
		me6x00_outl( reg, port );
	} else {
		reg &= 0xFFFFFFF7;
		me6x00_outl( reg, port );
	}

	return 0;
}


/*
 * Routine:
 *   me6x00_endis_extrig
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to enable or to
 *   disable the external trigger.
 *
 * Parameter list:
 *   Name    Type                      Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_en_dis_st*         r       specifies the DAC and en/dis
 *   minor   int                       r       specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_endis_extrig( me6x00_endis_st *arg, int minor )
{
	me6x00_endis_st trig;
	unsigned int reg;
	unsigned int port;


	CALL_PDEBUG("me6x00_endis_extrig() is executed\n");

	if ( ! IS_ME6100( minor ) ) {
		printk( KERN_ERR "ME6X00: me6x00_endis_extrig(): "
			"Not a ME6100\n" );
		return -ENODEV;
	}

	if ( copy_from_user( &trig, arg, sizeof *arg ) )
		return -EFAULT;

	if ( trig.dac > ME6X00_DAC03 ) {
		printk( KERN_ERR "ME6X00: me6x00_endis_extrig(): "
			"No such function for DAC %d\n", trig.dac );
		return -EINVAL;
	}

	port = info_vec[ minor ].me6x00_regbase + regs[ trig.dac ].ctrl;
	reg = me6x00_inl( port );

	/*
	 * Conversion must be stopped before reconfiguring the DAC!
	 */

	reg |= 0x4;
	me6x00_outl( reg, port );

	if ( trig.endis == ME6X00_ENABLE ) {
		reg |= 0x00000010;
		me6x00_outl( reg, port );
	} else {
		reg &= 0xFFFFFFEF;
		me6x00_outl( reg, port );
	}

	return 0;
}


/*
 * Routine:
 *   me6x00_rifa_extrig
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to adjust the
 *   sensitivity of the external trigger to rising or falling edge.
 *
 * Parameter list:
 *   Name    Type                      Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_en_dis_st*         r       specifies the DAC and enable
 *                                             or disable
 *   minor   int                       r       specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_rifa_extrig( me6x00_rifa_st *arg, int minor )
{
	me6x00_rifa_st edge;
	unsigned int reg;
	unsigned int port;


	CALL_PDEBUG( "me6x00_rifa_extrig() is executed\n" );

	if ( ! IS_ME6100( minor ) ) {
		printk( KERN_ERR "ME6X00: me6x00_rifa_extrig(): "
			"Not a ME6100\n" );
		return -ENODEV;
	}

	if ( copy_from_user( &edge, arg, sizeof *arg ) )
		return -EFAULT;

	if ( edge.dac > ME6X00_DAC03 ) {
		printk( KERN_ERR "ME6X00: me6x00_rifa_extrig(): "
			"No such function for DAC %d\n", edge.dac );
		return -EINVAL;
	}

	port = info_vec[minor].me6x00_regbase + regs[ edge.dac ].ctrl;
	reg = me6x00_inl( port );

	/*
	 * Conversion must be stopped before reconfiguring the DAC!
	 */

	reg |= 0x4;
	me6x00_outl( reg, port );

	if ( edge.rifa == ME6X00_RISING ) {
		reg &= 0xFFFFFFDF;
		me6x00_outl( reg, port );
	} else {
		reg |= 0x00000020;
		me6x00_outl( reg, port );
	}

	return 0;
}


/*
 * Routine:
 *   me6x00_set_timer
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to set the ticks
 *   (30.30ns) per value in the FIFO. The output time per value is
 *   calculated as follows:
 *   time/value = ticks * 30.30ns
 *
 * Parameter list:
 *   Name    Type                      Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_timer_st*          r       carries the value from user
 *   minor   int                       r       specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_set_timer( me6x00_timer_st *arg, int minor )
{
	me6x00_timer_st timer;
	unsigned int port;


	CALL_PDEBUG( "me6x00_set_timer() is executed\n" );

	if ( ! IS_ME6100( minor ) ) {
		printk( KERN_ERR "ME6X00: me6x00_set_timer(): "
			"Not a ME6100\n" );
		return -ENODEV;
	}

	if ( copy_from_user( &timer, arg, sizeof *arg ) )
		return -EFAULT;

	/* Check if the divisor is right. ME6X00_MIN_TICKS is the lowest */

	if ( timer.divisor < ME6X00_MIN_TICKS ) {
		printk( KERN_ERR "ME6X00: me6x00_set_timer(): "
			"Divisor too low\n" );
		return -EINVAL;
	}

	/* Fix bug in Firmware */

	timer.divisor -= 2;

	PDEBUG( "me6x00_set_timer(): Divisor = %d\n", timer.divisor );

	if ( timer.dac > ME6X00_DAC03 ) {
		printk( KERN_ERR "ME6X00: me6x00_set_timer:"
			"No such function for DAC %d\n", timer.dac );
		return -EINVAL;
	}

	port = info_vec[ minor ].me6x00_regbase + regs[ timer.dac ].timer;
	me6x00_outl( timer.divisor, port );

	return 0;
}


/*
 * Routine:
 *   me6x00_write_single
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to write a single
 *   value to a DAC. For the first four DAC, it checks if in single mode.
 *
 * Parameter list:
 *   Name    Type                      Access  Description
 *--------------------------------------------------------------------------
 *   *arg    me6x00_single_st          r       specifies the DAC and
 *                                             carries the value from user
 *   minor   int                       r       specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_write_single( me6x00_single_st *arg, int minor )
{
	wait_queue_head_t wait;
	unsigned int port;
	me6x00_single_st single;
	unsigned int which_bit;
	int i = 0;
	unsigned int x;


	CALL_PDEBUG( "me6x00_write_single() is executed\n" );

	init_waitqueue_head( &wait );

	if ( copy_from_user( &single, arg, sizeof *arg ) )
		return -EFAULT;

	if ( single.dac >= info_vec[ minor ].num_dacs ) {
		printk( KERN_ERR "ME6X00: me6x00_write_single(): No such "
			"DAC for this board\n" );
		return -EINVAL;
	}

	if ( single.dac <= ME6X00_DAC03 &&
	     ( 3 & me6x00_inl( info_vec[ minor ].me6x00_regbase
			       + regs[ single.dac ].ctrl ) ) ) {
		printk( KERN_ERR "ME6X00: me6x00_write_single(): "
			"Not in single mode!\n");
		return -EIO;
	}

	port = info_vec[ minor ].me6x00_regbase 
	       + ( single.dac <= ME6X00_DAC03 ?
		   regs[ single.dac ].stat : ME6X00_STATREG_DACXX );

	which_bit = 1;
	if ( single.dac > ME6X00_DAC04 )
		which_bit <<= ( single.dac - ME6X00_DAC04 );

	/* Wait while board is still busy trying to output the last value */

	PDEBUG( "which_bit = 0x%04X\n", which_bit );

	while ( i++ < 10 && ( which_bit & ( x = me6x00_inl( port ) ) ) ) {
		PDEBUG( "0x%04X\n", x );
		PDEBUG( "me6x00_write_single(): Sending process to sleep\n" );
		interruptible_sleep_on_timeout( &wait, 1 );
		if ( signal_pending( current ) ) {
			PDEBUG( "ME6X00: me6x00_single(): "
				"aborted by signal\n" );
			return -EINTR;
		}
	}

	if ( i >= 10 )
		return -EIO;

	port = info_vec[ minor ].me6x00_regbase + sval_regs[ single.dac ];
	me6x00_outl( ( unsigned int ) single.value, port );

	return 0;
}


/*
 * Routine:
 *   me6x00_write_continuous
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to write values
 *   contiously to the DACs. The driver provides a temporary buffer for
 *   each FIFO. The data to be written to the FIFOs are copied from user
 *   to kernel space. If the buffer gets full the application in user mode
 *   is sent to sleep on a wait queue. There is a wait queue for each FIFO.
 *   Writing to the FIFO is done in the interrupt routine (ISR). The ISR also
 *   wakes up the sleeping process when some values to the FIFOs were written
 *   and buffer space thus becomes available.
 *   Actions:
 *     - Check if board is a ME6100 type.
 *     - Enable FIFO.
 *     - Clear Stop bit.
 *     - Write values to circular buffer.
 *     - Enable interrupts.
 *
 * Parameter list:
 *   Name    Type                      Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_write_st*          r       specifies the DAC and
 *                                             carries the value from user
 *   minor   int                       r       specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_write_continuous( me6x00_write_st *arg, int minor )
{
	me6x00_info_st *info;
	me6x00_write_st write;
	unsigned int flags;
	int free_space = 0;
	int k = 0;
	int ret = 0;
	int count = 0;
	unsigned short *user_buf = NULL;
	unsigned int reg;
	unsigned int port;


	CALL_PDEBUG( "me6x00_write_continuous() is executed\n" );

	if ( ! IS_ME6100( minor ) ) {
		printk( KERN_ERR "ME6X00: me6x00_write_continuous(): "
			"Not a ME6100\n" );
		return -ENODEV;
	}

	if ( copy_from_user( &write, arg, sizeof *arg ) )
		return -EFAULT;

	if ( write.dac > ME6X00_DAC03 ) {
		printk( KERN_ERR "ME6X00: me6x00_write_continuous(): "
			"No such function for DAC %d\n", write.dac );
		return -EINVAL;
	}

	info = info_vec + minor;
	count = write.count;
	user_buf = write.buf;

	/* Check that continuous mode has been set */

	port = info->me6x00_regbase + regs[ write.dac ].ctrl;
	reg = me6x00_inl( port );

	if ( ( reg & 0x3 ) != 0x2 ) {
		printk( KERN_ERR "ME6X00: me6x00_write_continuous():"
			"DAC %d: Not in continuous mode\n", write.dac );
		return -EIO;
	}

	/* Enable the FIFO */

	reg |= 0x08;
	me6x00_outl( reg, port );

	/* Start conversion */

	reg &= ~0x4;
	me6x00_outl( reg, port );

	/*
	 * Now do the following:
	 * 1. If buffer is full wait for an interrupt and then try again
	 * 2. Fill buffer as far as possible
	 * 3. Enable interrupt
	 * Repeat this until all data have been written to the buffer
	 */

	while ( 1 ) {

		/* Get free buffer */

		save_flags( flags );
		cli( );

		free_space = me6x00_space_to_end( info->buf + write.dac );
		PDEBUG( "me6x00_write_continuous(): Buffer %d: Head = "
			"0x%04X\n", write.dac,
			info_vec[ minor ].buf[ write.dac ].head );
		PDEBUG( "me6x00_write_continuous(): Buffer %d: Tail = "
			"0x%04X\n", write.dac,
			info_vec[ minor ].buf[ write.dac ].tail );
		PDEBUG( "me6x00_write_continuous(): Buffer %d: Space to end = "
			"0x%04X\n", write.dac, free_space );
		PDEBUG( "me6x00_write_continuous(): Buffer %d: Udata size = "
			"0x%04X\n", write.dac, count );

		restore_flags( flags );

		/* If there's no space left in the buffer wait to be woken on
		   the next interrupt */

		if ( free_space == 0 ) {

			PDEBUG( "me6x00_write_continuous(): Sending process "
				"to sleep\n" );
			interruptible_sleep_on( info->wait_queues
						+ write.dac );

			/*
			 * There are two reasons why a wake up may occur:
			 * 1) an interrupt wakes up the write routine.
			 * 2) a signal was sent to the process (CTRL+C, ...)
			 */

			if ( signal_pending( current ) ) {
				printk( KERN_WARNING
					"ME6X00: me6x00_write_continuous() "
					"aborted by signal\n" );
				return -EINTR;
			}

			continue;
		}

		/* Only able to write size of free buffer or size of count */

		if ( count < free_space )
			free_space = count;

		/* If count is zero return to user */

		if ( free_space <= 0 ) {
			PDEBUG( "me6x00_write_continuous(): Buffer %d: Count "
				"reached 0!\n", write.dac );
			break;
		}

		PDEBUG( "me6x00_write_continuous(): Buffer %d: Copy 0x%04X "
			"values\n", write.dac, free_space );

		/* Copy as many data as we need from the user supplied buffer,
		   bail out if this fails (in this case k will be 0) */

		k = 2 * free_space;
		k -= copy_from_user( info->buf[ write.dac ].buf
				     + info->buf[ write.dac ].head,
				     user_buf, k );
		if ( k == 0 )
			return -EFAULT;

		free_space = k / 2;
		info->buf[ write.dac ].head =
				( info->buf[ write.dac ].head + free_space ) &
				( ME6X00_BUFFER_COUNT - 1 );

		user_buf += free_space;
		count -= free_space;
		ret += free_space;

		save_flags( flags );
		cli( );

		if ( me6x00_buf_count( info->buf + write.dac ) ) {
			/* enable interrupts */

			PDEBUG( "me6x00_write_continuous(): FIFO %d: Enable "
				"interrupt\n", write.dac );
			reg |= 0x40;
			me6x00_outl( reg, port );
		}

		restore_flags( flags );
	}

	write.ret = ret;
	if ( copy_to_user( arg, &write, sizeof *arg ) )
		return -EFAULT;

	return 0;
}


/*
 * Routine:
 *   me6x00_write_wraparound
 *
 * Description:
 *   This function is called by me6x00_ioctl() in order to write values
 *   to the FIFOs for wraparound mode.
 *   Actions:
 *     - Check, if board is a ME6100.
 *     - Check, if mode is wraparound.
 *     - Check, if DAC is stopped, if not stop it.
 *     - Clears the FIFO.
 *     - If count of values is greater than ME6X00_FIFO_SIZE only
 *       ME6X00_FIFO_SIZE is loaded to the FIFOs.
 *   The user has to start the conversion manually.
 *
 * Parameter list:
 *   Name    Type                      Access  Description
 *--------------------------------------------------------------------------
 *   arg     me6x00_write_st*          r       specifies the DAC and
 *                                             carries the values from user
 *   minor   int                       r       specifies the board
 *
 * Result:
 *   On success the return value is 0, everything else means failure.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_write_wraparound( me6x00_write_st *arg, int minor )
{
	me6x00_info_st *info;
	me6x00_write_st write;
	unsigned int tmp;
	int i = 0;
	int k = 0;
	int to_send = 0;
	unsigned int ctrl_port;
	unsigned int stat_port;
	unsigned int fifo_port;
	unsigned int ctrl_reg;
	wait_queue_head_t wait;


	CALL_PDEBUG( "me6x00_write_wraparound() is executed\n" );

	init_waitqueue_head( &wait );

	if ( ! IS_ME6100( minor ) ) {
		printk( KERN_ERR "ME6X00: me6x00_write_wraparound(): "
			"Not a ME6100\n" );
		return -ENODEV;
	}

	if ( copy_from_user( &write, arg, sizeof *arg ) )
		return -EFAULT;

	if ( write.dac > ME6X00_DAC03 ) {
		printk( KERN_ERR "ME6X00: me6x00_write_wraparound(): "
			"No such function for DAC %d\n", write.dac );
		return -EINVAL;
	}

	info = info_vec + minor;

	ctrl_port = info->me6x00_regbase + regs[ write.dac ].ctrl;
	stat_port = info->me6x00_regbase + regs[ write.dac ].stat;

	/* Check that wrap-around mode has been set */

	ctrl_reg = me6x00_inl( ctrl_port );
	if ( ( ctrl_reg & 0x3 ) != 0x1 ) {
		printk( KERN_ERR "ME6X00: me6x00_write_wraparound(): "
			"Not in wraparound mode\n" );
		return -EIO;
	}

	/*
	 * Check if the DAC is already stopped, if not stop it and wait 
	 * until the FIFO has become empty.
	 */

	if ( ! ( ctrl_reg & 0x4 ) ) {

		ctrl_reg |= 0x4;
		me6x00_outl( ctrl_reg, ctrl_port );

		while ( me6x00_inl( stat_port ) & 0x8 ) {
			PDEBUG( "me6x00_write_wraparound(): Sending process "
				"to sleep\n" );
			interruptible_sleep_on_timeout( &wait, 10 );

			/*
			 * There are two reasons why a wake up may occur:
			 * 1) the timeout is reached
			 * 2) a signal was sent to the process (CTRL+C, ...)
			 */

			if ( signal_pending( current ) ) {
				printk( KERN_WARNING "ME6X00: "
					"me6x00_write_wraparound() aborted by "
					"signal\n" );
				return -EINTR;
			}
		}
	}

	/* Reset the FIFO */

	ctrl_reg &= 0xF7;
	me6x00_outl( ctrl_reg, ctrl_port );

	ctrl_reg |= 0x08;
	me6x00_outl( ctrl_reg, ctrl_port );

	/* Now write as many values to the FIFO as we have, but not more than
	   the FIFO size */

	to_send = write.count;
	if ( to_send > ME6X00_FIFO_SIZE )
		to_send = ME6X00_FIFO_SIZE;
	k = to_send * 2;
	k -= copy_from_user( info->buf[ write.dac ].buf, write.buf, k );

	if ( k == 0 )
		return -EFAULT;

	to_send = k / 2;

	PDEBUG( "me6x00_write_wraparound(): read %d shorts from user space\n",
		to_send );

	fifo_port = info->me6x00_regbase + regs[ write.dac ].fifo;
	for ( i = 0; i < to_send; i++ ) {
		tmp = ( unsigned int ) * ( info->buf[ write.dac ].buf + i );
		if ( write.dac & 1 )
			tmp <<= 16;
		outl( tmp, fifo_port );
		PDEBUG( "me6x00_write_wraparound():%d = %X\n", i, tmp );
	}

	write.ret = to_send;

	if ( copy_to_user( arg, &write, sizeof *arg ) )
		return -EFAULT;

	return 0;
}


/*
 * Routine:
 *   me6x00_isr
 *
 * Description:
 *   This function is the interrupt service routine for the board. It is
 *   called when the interrupt logic of the plx and the board is enabled
 *   and an FIFO generates an interrupt in order to signal that data can
 *   be written into the FIFOs. It checks which board raised the interrupt
 *   and the reason. Then it loads values into the FIFOs if the buffer is
 *   not empty. If the buffer is empty it stops download and disables the
 *   interrupt for this FIFO.
 *
 * Parameter list:
 *   Name       Type              Access    Description
 *--------------------------------------------------------------------------
 *   irq        int               read      number of interrupt occured
 *   dev_id     void*             read      pointer to board specific
 *                                          informations
 *   dummy      struct pt_regs *  read      pointer to cpu register
 *
 * Result:
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static void me6x00_isr( int irq, void *dev_id, struct pt_regs *dummy )
{
	me6x00_info_st *info;
	unsigned int reg1;
	unsigned int reg2;
	unsigned int tmp;
	int dac;
	int to_send = 0;
	int available = 0;
	int i;


	dummy = dummy;
	ISR_PDEBUG( "me6x00_isr() is executed\n" );

	info = dev_id;

	/* Check if irq number is right */

	if ( irq != info->irq ) {
		ISR_PDEBUG( "me6x00_isr(): incorrect interrupt num: %d\n",
			    irq );
		return;
	}

	/* Check if this board raised an interrupt */

	if ( ! ( reg1 = 0xF & ( me6x00_inl( info->me6x00_regbase
					    + ME6X00_IRQSREG_DACXX ) ) ) ) {
		ISR_PDEBUG( "me6x00_isr(): not this board\n" );
		return;
	}

	/* Figure for which DAC the interrupt was send */

	dac = 0;
	while ( ! ( reg1 & 1 ) && dac <= 3 ) {
		reg1 >>= 1;
		dac++;
	}

	/* Something strange must be going on here... */

	if ( dac > ME6X00_DAC03 )
		return;

	/* Increment interrupt count for the board */

	ISR_PDEBUG( "me6x00_isr(): Interrupt from DAC %d\n", dac );

	/* Read status register to find out what happened */

	reg2 = me6x00_inl( info->me6x00_regbase + regs[ dac ].stat );

	/* Figure out how many (short) values we can send */

	if ( ( reg2 & 0xE ) == 0x6 ) {
		ISR_PDEBUG("me6x00_isr(): FIFO %d: Fifo empty\n", dac );
		to_send = ME6X00_FIFO_SIZE;
	} else if ( ( reg2 & 0xE ) == 0xE ) {
		ISR_PDEBUG( "me6x00_isr(): FIFO %d: Fifo less than half "
			    "full\n", dac );
		to_send = ME6X00_FIFO_SIZE / 2;
	} else {
		to_send = 0;
		ISR_PDEBUG( "me6x00_isr(): FIFO %d: Fifo full\n", dac );
	}

	ISR_PDEBUG( "me6x00_isr(): FIFO %d: Trying to write 0x%04X values\n",
		    dac, to_send );

	while ( 1 ) {
		ISR_PDEBUG( "me6x00_isr(): Buffer %d: Head = 0x%04X\n",
			    dac, info->buf[ dac ].head );
		ISR_PDEBUG( "me6x00_isr(): Buffer %d: Tail = 0x%04X\n",
			    dac, info->buf[ dac ].tail );

		available = me6x00_values_to_end( info->buf + dac );

		ISR_PDEBUG( "me6x00_isr(): Buffer %d: Values to end = "
			    "0x%04X\n", dac, available );

		if ( available > to_send )
			available = to_send;

		if ( available <= 0 ) {
			ISR_PDEBUG("me6x00_isr(): Buffer %d: Complete or "
				   "buffer empty!\n", dac );
			break;
		}

		/* Distinguishing between the DACs with odd and even numbers is
		   necessary because for DAC01 and DAC03 the two upper bytes
		   of the 4-byte FIFO register have to be written to while for
		   DAC00 and DAC02 the lower two bytes are to be used. */

		if ( dac & 1 ) {
			for ( i = 0; i < available; i++ ) {
				tmp = ( ( unsigned int )
					( * ( info->buf[ dac ].buf +
					      info->buf[ dac ].tail + i ) ) )
						      << 16;
				outl( tmp, info->me6x00_regbase +
				      regs[ dac ].fifo );
			}
		} else
			outsw( info->me6x00_regbase + regs[ dac ].fifo,
			       info->buf[ dac ].buf + info->buf[ dac ].tail,
			       available );

		info->buf[ dac ].tail = ( info->buf[ dac ].tail + available ) &
					( ME6X00_BUFFER_COUNT - 1 );
		ISR_PDEBUG( "me6x00_isr(): Buffer %d: 0x%04X values written "
			    "to port 0x%04X\n", dac, available,
			    info->me6x00_regbase + regs[ dac ].fifo );
		to_send -= available;
	}

	/* Buffer space is now available, so wake up waiting process */

	ISR_PDEBUG( "me6x00_isr(): Buffer %d: Wake up waiting process\n",
		    dac );

	wake_up_interruptible( info->wait_queues + dac );

	/* Start conversion by dummy write, only when ext trigger is
	   disabled */

	if ( ! ( 0x10 & me6x00_inl( info->me6x00_regbase
				    + regs[ dac ].ctrl ) ) )
		me6x00_outl( 0x7FFF, info->me6x00_regbase + sval_regs[ dac ] );

	/* If there are no values left in the buffer disable interrupts */

	if ( ! me6x00_buf_count( info->buf + dac ) ) {
		ISR_PDEBUG( "me6x00_isr(): FIFO %d: Disable Interrupt\n",
			    dac );
		reg2 = me6x00_inl( info->me6x00_regbase + regs[ dac ].ctrl ) &
			0xBF;
		me6x00_outl( reg2, info->me6x00_regbase + regs[ dac ].ctrl );
	}

	/* Work is done, so reset the interrupt */

	ISR_PDEBUG( "me6x00_isr(): FIFO %d: reset interrupt\n", dac );
	reg2 = me6x00_inl( info->me6x00_regbase + regs[ dac ].irqr );
}


/*
 * Routine:
 *   me6x00_buf_count
 *
 * Description:
 *   Calculates the number of available values in the buffer.
 *
 * Parameter list:
 *   Name       Type                 Access    Description
 *--------------------------------------------------------------------------
 *   buf        me6x00_circ_buf_st*  r         Head and Tail.
 *
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_buf_count( me6x00_circ_buf_st *buf ) {
	return ( buf->head - buf->tail ) & ( ME6X00_BUFFER_COUNT - 1 );
}


/*
 * Routine:
 *   me6x00_values_to_end
 *
 * Description:
 *   Calculates the available values to the end of the buffer or to head,
 *   whicheve comes first.
 *
 * Parameter list:
 *   Name       Type                 Access    Description
 *--------------------------------------------------------------------------
 *   buf        me6x00_circ_buf_st*  r         Head and Tail.
 *
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_values_to_end( me6x00_circ_buf_st *buf )
{
	int end;
	int n;
	end = ME6X00_BUFFER_COUNT - buf->tail;


	n = ( buf->head + end ) & ( ME6X00_BUFFER_COUNT - 1 );

	if ( n < end )
		return n ;
	return end;
}


/*
 * Routine:
 *   me6x00_space_to_end
 *
 * Description:
 *   Calculates the available free space to the end of the buffer or to tail,
 *   whichever comes first.
 *
 * Parameter list:
 *   Name       Type                 Access    Description
 *--------------------------------------------------------------------------
 *   buf        me6x00_circ_buf_st*  r         Head and Tail.
 *
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification: JTT
 */

static int me6x00_space_to_end( me6x00_circ_buf_st *buf )
{
	int end;
	int n;

	end = ME6X00_BUFFER_COUNT - 1 - buf->head;
	n = ( end + buf->tail ) & ( ME6X00_BUFFER_COUNT - 1 );

	if ( n <= end )
		return n;
	return end + 1;
}


/*
 * Routine:
 *   me6x00_outb
 *
 * Parameter list:
 *   Name       Type               Access    Description
 *--------------------------------------------------------------------------
 *   port       unsigned int       r         Port address.
 *   value      unsigned char      r         Byte you want to write.
 *
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification:
 */

static void me6x00_outb( unsigned char value, unsigned int port )
{
	PORT_PDEBUG( "--> 0x%02X port 0x%04X\n", value, port );
	outb( value, port );
}


/*
 * Routine:
 *   me6x00_outw
 *
 * Parameter list:
 *   Name       Type               Access    Description
 *--------------------------------------------------------------------------
 *   port       unsigned int       r         Port address.
 *   value      unsigned short     r         Word you want to write.
 *
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification:
 */

static void me6x00_outw( unsigned short value, unsigned int port )
{
	PORT_PDEBUG( "--> 0x%04X port 0x%04X\n", value, port );
	outw( value, port );
}


/*
 * Routine:
 *   me6x00_outl
 *
 * Parameter list:
 *   Name       Type               Access    Description
 *--------------------------------------------------------------------------
 *   port       unsigned int       r         Port address.
 *   value      unsigned int       r         Integer you want to write.
 *
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification:
 */

static void me6x00_outl( unsigned int value, unsigned int port )
{
	PORT_PDEBUG( "--> 0x%08X port 0x%04X\n", value, port );
	outl( value, port );
}


/*
 * Routine:
 *   me6x00_inb
 *
 * Parameter list:
 *   Name       Type               Access    Description
 *--------------------------------------------------------------------------
 *   port       unsigned int       r         Port address.
 *
 * Result:
 *  Value from the port.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification:
 */

static unsigned char me6x00_inb( unsigned int port )
{
	unsigned char value;

	value = inb( port );
	PORT_PDEBUG( "<-- 0x%02X port 0x%04X\n", value, port );
	return value;
}


/*
 * Routine:
 *   me6x00_inw
 *
 * Parameter list:
 *   Name       Type               Access    Description
 *--------------------------------------------------------------------------
 *   port       unsigned int       r         Port address.
 *
 * Result:
 *  Value from the port.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification:
 */

static unsigned short me6x00_inw( unsigned int port )
{
	unsigned short value;

	value = inw( port );
	PORT_PDEBUG( "<-- 0x%04X port 0x%04X\n", value, port );
	return value;
}


/*
 * Routine:
 *   me6x00_inl
 *
 * Parameter list:
 *   Name       Type               Access    Description
 *--------------------------------------------------------------------------
 *   port       unsigned int       r         Port address.
 *
 * Result:
 *  Value from the port.
 *--------------------------------------------------------------------------
 * Author: GG
 * Modification:
 */

static unsigned int me6x00_inl( unsigned int port )
{
	unsigned int value;
	value = inl( port );
	PORT_PDEBUG( "<-- 0x%08X port 0x%04X\n", value, port );
	return value;
}




#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 2, 0 )
MODULE_DESCRIPTION( "Preliminary driver for ME6x00 D/A cards "
		    "(Meilhaus Electronic GmbH)" );
MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number to use" );
#endif


/* MODULE_LICENSE should be defined since at least 2.4.10 */

#if defined MODULE_LICENSE
MODULE_LICENSE( "GPL" );
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 4, 0 )
module_init( me6x00_init );
module_exit( me6x00_exit );
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
