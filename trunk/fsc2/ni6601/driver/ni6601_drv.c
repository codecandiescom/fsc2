/*
 *  $Id$
 * 
 *  Driver for National Instruments 6601 boards
 * 
 *  Copyright (C) 2002-2006 Jens Thoms Toerring
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
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 *  To contact the author send email to:  jt@toerring.de
 */


#include "ni6601_drv.h"


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static int ni6601_init_board( struct pci_dev *             dev,
			      const struct pci_device_id * id );
static void ni6601_release_board( struct pci_dev *dev );
#else
static __init int ni6601_init_board( struct pci_dev *dev, Board *board );
#endif


static int major = NI6601_MAJOR;
Board boards[ NI6601_MAX_BOARDS ];       /* also needed by fops.c */
int board_count = 0;                     /* also needed by fops.c */

static struct pci_device_id ni6601_pci_tbl[ ] = {
	{ PCI_VENDOR_NATINST, PCI_DEVICE_NATINST_6601,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0 }
};

MODULE_DEVICE_TABLE( pci, ni6601_pci_tbl );


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static struct pci_driver ni6601_pci_driver = {
	.name     = NI6601_NAME,
	.id_table = ni6601_pci_tbl,
	.probe    = ni6601_init_board,
	.remove   = ni6601_release_board,
};

static dev_t dev_no;
static struct cdev ch_dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 14 )
static struct class *ni6601_class;
#endif

#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
struct file_operations ni6601_file_ops = {
	owner:		    THIS_MODULE,
	ioctl:              ni6601_ioctl,
	open:               ni6601_open,
	poll:               ni6601_poll,
	read:               ni6601_read,
	release:            ni6601_release
};
#else
struct file_operations ni6601_file_ops = {
	.owner =	    THIS_MODULE,
#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
	.ioctl =            ni6601_ioctl,
#else
	.unlocked_ioctl =   ni6601_ioctl,
#endif
	.open =             ni6601_open,
	.poll =             ni6601_poll,
	.read =             ni6601_read,
	.release =          ni6601_release
};
#endif


/*-------------------------------------------------------*
 * Function that gets executed when the module is loaded
 *-------------------------------------------------------*/

static int __init ni6601_init( void )
{
	struct pci_dev *dev = NULL;
	int result;
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 14 )
	int i;
#endif

	/* Try to find all boards */

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )

	while ( ( dev = pci_get_device( PCI_VENDOR_NATINST,
					PCI_DEVICE_NATINST_6601, dev ) )
		                                                    != NULL ) {
		if ( board_count >= NI6601_MAX_BOARDS )
			break;

		boards[ board_count ].dev = dev;
		boards[ board_count ].is_init = 0;
		boards[ board_count ].is_enabled = 0;
		boards[ board_count ].has_region = 0;
		boards[ board_count ].in_use = 0;
		boards[ board_count ].buf = NULL;
		boards[ board_count ].mite = NULL;
		boards[ board_count ].addr = NULL;
		boards[ board_count++ ].irq = 0;

		pci_dev_put( dev );
	}
#else
	int i;

	while ( ( dev = pci_find_device( PCI_VENDOR_NATINST,
					 PCI_DEVICE_NATINST_6601, dev ) )
		                                                    != NULL ) {
		if ( board_count >= NI6601_MAX_BOARDS )
			break;

		if ( ni6601_init_board( dev, boards + board_count ) ) {
			for ( i = 0; i <= board_count; i++ )
				ni6601_release_resources( boards + i );
			printk( KERN_ERR NI6601_NAME ": Failed to initialize "
				"board %d\n", board_count );
			return -EIO;
		}

		board_count++;
	}
#endif

	if ( board_count == 0 ) {
		printk( KERN_WARNING NI6601_NAME ": No boards found.\n" );
		return -ENODEV;
	}

	if ( board_count >= NI6601_MAX_BOARDS && dev != NULL )
		printk( KERN_WARNING NI6601_NAME ": There are more than "
			"the currently configured maximum of %d boards!\n",
			NI6601_MAX_BOARDS );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )

	if ( major == 0 )
		result = alloc_chrdev_region( &dev_no, 0,board_count,
					      NI6601_NAME );
	else {
		dev_no = MKDEV( major, 0 );
		result = register_chrdev_region( dev_no, board_count,
						 NI6601_NAME );
	}

	if ( result < 0 ) {
                printk( KERN_ERR NI6601_NAME ": Can't register as char "
			"device.\n" );
		return -EIO;
	}

	cdev_init( &ch_dev, &ni6601_file_ops );
	ch_dev.owner = THIS_MODULE;

	if ( cdev_add( &ch_dev, dev_no, board_count ) < 0 ) {
                printk( KERN_ERR NI6601_NAME ": Can't register as char "
			"device.\n" );
		unregister_chrdev_region( dev_no, board_count );
		return -EIO;
	}

	if ( ( result = pci_register_driver( &ni6601_pci_driver ) ) < 0 ) {
		unregister_chrdev_region( dev_no, board_count );
		printk( KERN_ERR NI6601_NAME ": Registering PCI device "
			"failed.\n" );
		return result;
	}

	if ( major == 0 )
		major = MAJOR( dev_no );


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 14 )
	ni6601_class = class_create( THIS_MODULE, NI6601_NAME );
	if ( IS_ERR( ni6601_class ) ) {
		printk( KERN_ERR "ME6X00: Can't create a class for "
			"the device.\n" );
		pci_unregister_driver( &ni6601_pci_driver );
		cdev_del( &ch_dev );
		unregister_chrdev_region( dev_no, board_count );
		return -EIO;
	}

	for ( i = 0; i < board_count; i++ )
		class_device_create( ni6601_class, NULL, major, NULL,
				     NI6601_NAME "_%i", i );
#endif

#else

	if ( ( result = register_chrdev( major, NI6601_NAME,
					 &ni6601_file_ops ) ) < 0 ) {

		printk( KERN_ERR NI6601_NAME ": Can't register as char "
			"device.\n" );
		for ( i = 0; i < board_count; i++ )
			ni6601_release_resources( boards + i );
		return -ENODEV;
	}

	if ( major == 0 )
		major = result;

#endif

	printk( KERN_INFO NI6601_NAME ": Module successfully installed\n"
		NI6601_NAME ": Initialized %d board%s\n",
		board_count, board_count > 1 ? "(s)" : "" );

	return 0;
}


/*-----------------------------------------------------*
 * Function gets executed when the module gets removed
 *-----------------------------------------------------*/

static void __exit ni6601_cleanup( void )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 14 )
	int i;

	for ( i = 0; i < board_count; i++ )
		class_device_destroy( ni6601_class, major );
        class_destroy( ni6601_class );
#endif

	pci_unregister_driver( &ni6601_pci_driver );
	cdev_del( &ch_dev );
	unregister_chrdev_region( dev_no, board_count );

#else
	int i;

	for ( i = 0; i < board_count; i++ )
		ni6601_release_resources( boards + i );

	/* Unregister the character device */

	if ( unregister_chrdev( major, NI6601_NAME ) < 0 ) {
		printk( KERN_ERR NI6601_NAME ": Unable to unregister "
			"module.\n" );
		return;
	}

#endif

	printk( KERN_INFO NI6601_NAME ": Module successfully "
		"removed\n" );
}


/*----------------------------------*
 * Initialization of a single board
 *----------------------------------*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static int ni6601_init_board( struct pci_dev *             dev,
			      const struct pci_device_id * id )
{
	Board *board;
	int i;


	for ( i = 0; i < board_count; i++ )
		if ( boards[ i ].dev == dev )
			break;

	if ( i == board_count ) {
		printk( KERN_ERR NI6601_NAME ": ni6601_init_board() called "
			"with unknown pci_dev structure.\n" );
		return -1;
	}

	board = boards + i;

        if ( pci_enable_device( dev ) )
	{
		PDEBUG( "Failed to enable board %d\n", board - boards );
		goto init_failure;
	}

	board->is_enabled = 1;

	pci_set_master( dev );

	if ( pci_request_regions( dev, NI6601_NAME ) ) {
		PDEBUG( "Failed to lock memory regions for board %d\n",
			board - boards );
		goto init_failure;
	}

	board->has_region = 1;

	/* Remap the MITE memory region as well as the memory region at
	   which we're going to access the TIO */

	board->mite_phys = pci_resource_start( dev, 0 );
	board->mite_len  = pci_resource_len( dev, 0 );
	board->mite = ioremap( board->mite_phys, board->mite_len );
	if ( board->mite == NULL ) {
		PDEBUG( "Can't remap MITE memory range for board %d\n",
			board - boards );
		goto init_failure;
	}

	board->addr_phys = pci_resource_start( dev, 1 );
	board->addr_len  = pci_resource_len( dev, 1 );
	board->addr = ioremap( board->addr_phys, board->addr_len );
	if ( board->addr == NULL ) {
		PDEBUG( "Can't remap TIO memory range for board %d\n",
			board - boards );
		goto init_failure;
	}

	/* Enable access to the 2nd memory region (width 8k) where the
	   TIO is located through the MITE */

	iowrite32( ( board->addr_phys & 0xFFFFFF00L ) | 0x8C,
		   board->mite + 0xC4 );
	iowrite32( 0, board->mite + 0xF4 );

	/* Request the interrupt used by the board */

	if ( request_irq( dev->irq, ni6601_irq_handler, SA_SHIRQ,
			  NI6601_NAME, board ) ) {
		PDEBUG( "Can't obtain IRQ %d\n", dev->irq );
		dev->irq = 0;
		goto init_failure;
	}

	/* Initialize variables for interrupt handling */

	board->irq = dev->irq;
	board->expect_tc_irq = 0;
	for ( i = 0; i < 4; i++ ) {
		board->tc_irq_enabled[ i ] = 0;
		board->tc_irq_raised[ i ] = 0;
	}

	board->dma_irq_enabled = 0;
	board->buf_counter = -1;

	init_waitqueue_head( &board->tc_waitqueue );
	init_waitqueue_head( &board->dma_waitqueue );

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
        sema_init( &board->open_mutex, 1 );
        sema_init( &board->ioctl_mutex, 1 );
#else
	init_MUTEX( &board->open_mutex );
	init_MUTEX( &board->ioctl_mutex );
#endif

	/* Get addresses of all registers and reset the board */

	ni6601_register_setup( board );

	/* Initialization of DIO lines */

	ni6601_dio_init( board );

	board->is_init = 1;

	return 0;

 init_failure:
	ni6601_release_resources( board );
	return -1;
}

#else  /* version for 2.4 kernels */

static int __init ni6601_init_board( struct pci_dev * dev,
				     Board *          board )
{
	int i;


	board->dev = dev;
	boards->is_init = 0;
	board->is_enabled = 0;
	boards->has_region = 0;
	boards->in_use = 0;
	board->buf = NULL;
	board->mite = NULL;
	board->addr = NULL;
	board->irq = 0;

        if ( pci_enable_device( dev ) )
	{
		PDEBUG( "Failed to enable board %d\n", board - boards );
		return -1;
	}

	board->is_enabled = 1;

	pci_set_master( dev );

	if ( pci_request_regions( dev, NI6601_NAME ) ) {
		PDEBUG( "Failed to lock memory regions for board %d\n",
			board - boards );
		return -1;
	}

	board->has_region = 1;

	/* Remap the MITE memory region as well as the memory region at
	   which we're going to access the TIO */

	board->mite_phys = pci_resource_start( dev, 0 );
	board->mite_len  = pci_resource_len( dev, 0 );
	board->mite = ioremap( board->mite_phys, board->mite_len );
	if ( board->mite == NULL ) {
		PDEBUG( "Can't remap MITE memory range for board %d\n",
			board - boards );
		return -1;
	}

	board->addr_phys = pci_resource_start( dev, 1 );
	board->addr_len  = pci_resource_len( dev, 1 );
	board->addr = ioremap( board->addr_phys, board->addr_len );
	if ( board->addr == NULL ) {
		PDEBUG( "Can't remap TIO memory range for board %d\n",
			board - boards );
		return -1;
	}

	/* Enable access to the 2nd memory region (width 8k) where the
	   TIO is located through the MITE */

	iowrite32( ( board->addr_phys & 0xFFFFFF00L ) | 0x8C,
		   board->mite + 0xC4 );
	iowrite32( 0, board->mite + 0xF4 );

	/* Request the interrupt used by the board */

	if ( request_irq( dev->irq, ni6601_irq_handler, SA_SHIRQ,
			  NI6601_NAME, board ) ) {
		PDEBUG( "Can't obtain IRQ %d\n", dev->irq );
		dev->irq = 0;
		return -1;
	}

	/* Initialize variables for interrupt handling */

	board->irq = dev->irq;
	board->expect_tc_irq = 0;
	for ( i = 0; i < 4; i++ ) {
		board->tc_irq_enabled[ i ] = 0;
		board->tc_irq_raised[ i ] = 0;
	}

	board->dma_irq_enabled = 0;
	board->buf_counter = -1;

	init_waitqueue_head( &board->tc_waitqueue );
	init_waitqueue_head( &board->dma_waitqueue );

        sema_init( &board->open_mutex, 1 );

	/* Get addresses of all registers and reset the board */

	ni6601_register_setup( board );

	/* Initialization of DIO lines */

	ni6601_dio_init( board );

	board->is_init = 1;

	return 0;
}

#endif


/*-----------------------------------------------------------------------*
 * Called from PCI core system on unload of driver (or removal of board)
 *-----------------------------------------------------------------------*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static void ni6601_release_board( struct pci_dev * dev )
{
	int i;


	for ( i = 0; i < board_count; i++ )
		if ( boards[ i ].dev == dev )
			break;

	if ( i == board_count ) {
		printk( KERN_ERR NI6601_NAME ": ni6601_release_board() called "
			"with unknown pci_dev structure.\n" );
		return;
	}

	if ( ! boards[ i ].is_init ) {
		printk( KERN_ERR NI6601_NAME ": ni6601_release_board() called "
			"on board that never got initialized or already was "
			"released.\n" );
		return;
	}

	ni6601_release_resources( boards + i );

	boards[ i ].is_init = 0;
}
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
EXPORT_NO_SYMBOLS;
#endif


module_init( ni6601_init );
module_exit( ni6601_cleanup );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
module_param( major, int, S_IRUGO );
#else
MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number to use" );
#endif

MODULE_AUTHOR( "Jens Thoms Toerring <jt@toerring.de>" );
MODULE_DESCRIPTION( "National Instruments 6601 GPCT board driver" );


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
