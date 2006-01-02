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


static int ni6601_init_board( struct pci_dev *dev, Board *board );


static int major = NI6601_MAJOR;
Board boards[ NI6601_MAX_BOARDS ];
int board_count = 0;


struct file_operations ni6601_file_ops = {
	owner:		    THIS_MODULE,
	ioctl:              ni6601_ioctl,
	open:               ni6601_open,
	poll:               ni6601_poll,
	read:               ni6601_read,
	release:            ni6601_release
};



/*-------------------------------------------------------*
 * Function that gets executed when the module is loaded
 *-------------------------------------------------------*/

static int __init ni6601_init( void )
{
	struct pci_dev *dev = NULL;
	int res_major;


	/* Try to find all boards */

	while ( board_count < NI6601_MAX_BOARDS ) {
		dev = pci_find_device( PCI_VENDOR_NATINST,
				       PCI_DEVICE_NATINST_6601, dev );

		if ( dev == NULL )
			break;

		if ( ni6601_init_board( dev, boards + board_count ) ) {
			printk( KERN_ERR NI6601_NAME ": Failed to initialize "
				"board %d\n", board_count );
			ni6601_release_resources( boards, board_count );
			return -EIO;
		}

		board_count++;
	}

	if ( board_count == 0 ) {
		printk( KERN_WARNING NI6601_NAME ": No boards found.\n" );
		return -ENODEV;
	}

	if ( board_count == NI6601_MAX_BOARDS )
		printk( KERN_WARNING NI6601_NAME ": There could be more than "
			"the %d supported boards!\n", NI6601_MAX_BOARDS );

	if ( ( res_major = register_chrdev( major, NI6601_NAME,
					    &ni6601_file_ops ) ) < 0 ) {

		printk( KERN_ERR NI6601_NAME ": Can't register as char "
			"device.\n" );
		ni6601_release_resources( boards, board_count );
		return -ENODEV;
	}

	if ( major == 0 )
		major = res_major;

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
	ni6601_release_resources( boards, board_count );

	/* Unregister the character device */

	if ( unregister_chrdev( major, NI6601_NAME ) < 0 ) {
		printk( KERN_ERR NI6601_NAME ": Unable to unregister "
			"module.\n" );
		return;
	}

	printk( KERN_INFO NI6601_NAME ": Module successfully "
		"removed\n" );
}


/*----------------------------------*
 * Initialization of a single board
 *----------------------------------*/

static int __init ni6601_init_board( struct pci_dev *dev, Board *board )
{
	int i;


	board->dev = dev;
	board->irq = 0;

        if ( pci_enable_device( dev ) )
	{
		PDEBUG( "Failed to enable board %d\n", board - boards );
		goto init_failure;
	}

	pci_set_master( dev );

	if ( pci_request_regions( dev, NI6601_NAME ) ) {
		PDEBUG( "Failed to lock memory regions for board %d\n",
			board - boards );
		goto init_failure;
	}

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

	writel( ( board->addr_phys & 0xFFFFFF00L ) | 0x8C,
		board->mite + 0xC4 );
	writel( 0, board->mite + 0xF4 );

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

	spin_lock_init( &board->spinlock );

	/* Get addresses of all registers and reset the board */

	ni6601_register_setup( board );

	/* Initialization of DIO lines */

	ni6601_dio_init( board );

	board->in_use = 0;

	return 0;

 init_failure:
	ni6601_release_resources( boards, board_count );
	return -1;
}



EXPORT_NO_SYMBOLS;


module_init( ni6601_init );
module_exit( ni6601_cleanup );

MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number to use" );
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
