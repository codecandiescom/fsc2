/*
 *  $Id$
 * 
 *  Driver for National Instruments PCI E Series DAQ boards
 * 
 *  Copyright (C) 2003-2006 Jens Thoms Toerring
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


#include "ni_daq_board.h"
#include "board.h"
#include "board_props.h"


Board boards[ NI_DAQ_MAX_BOARDS ];
static int major = NI_DAQ_MAJOR;
int board_count = 0;

static Register_Addresses regs[ NI_DAQ_MAX_PCI_E_BOARDS ];

static Board_Functions func = {
	board_reset_all:            pci_board_reset_all,
	stc_writew:                 pci_stc_writew,
	stc_writel:                 pci_stc_writel,
	stc_readw:                  pci_stc_readw,
	stc_readl:                  pci_stc_readl,
	start_critical_section:     pci_start_critical_section,
	end_critical_section:       pci_end_critical_section,
	clear_configuration_memory: pci_clear_configuration_memory,
	clear_ADC_FIFO:             pci_clear_ADC_FIFO,
	clear_DAC_FIFO:             pci_clear_DAC_FIFO,
	configuration_high:         pci_configuration_high,
	configuration_low:          pci_configuration_low,
	ao_configuration:           pci_ao_configuration,
	dac_direct_data:            pci_dac_direct_data,
	dma_setup:                  pci_dma_setup,
	dma_buf_setup:              pci_dma_buf_setup,
	dma_buf_get:                pci_dma_buf_get,
	dma_get_available:          pci_dma_get_available,
	dma_shutdown:               pci_dma_shutdown,
	set_trigger_levels:         pci_set_trigger_levels
};


extern struct file_operations ni_daq_file_ops;   /* from daq_stc/fops.c */


static void pci_e_series_release_resources( Board * boards,
					    int     board_count );

static void pci_e_series_init_all( Board * board );
static int pci_e_series_init_board( struct pci_dev * dev,
				    Board *          board );


static unsigned int num_pci_e_series_boards = 
		sizeof pci_e_series_boards / sizeof pci_e_series_boards[ 0 ];


/*-------------------------------------------------------*
 * Function that gets executed when the module is loaded
 *-------------------------------------------------------*/

static int __init pci_e_series_init( void )
{
	struct pci_dev *dev = NULL;
	unsigned int i;


	/* Try to find all boards in the sequence they appear on the PCI bus */

	while ( board_count < NI_DAQ_MAX_PCI_E_BOARDS ) {
		if ( ( dev = pci_find_device( PCI_VENDOR_NATINST,
					      PCI_ANY_ID, dev ) ) == NULL )
			break;

		for ( i = 0; i < num_pci_e_series_boards; i++ )
			if ( dev->device == pci_e_series_boards[ i ].id )
				break;

		if ( i == num_pci_e_series_boards )
			continue;

		boards[ board_count ].type = pci_e_series_boards + i;
		boards[ board_count ].func = &func;

		if ( pci_e_series_init_board( dev, boards + board_count ) ) {
			printk( KERN_ERR BOARD_SERIES_NAME ": Failed to "
				"initialize %d. board (%s)\n",
				board_count, boards[ i ].type->name );
			pci_e_series_release_resources( boards, board_count );
			return -EIO;
		}

		board_count++;
	}

	if ( board_count == 0 ) {
		printk( KERN_WARNING BOARD_SERIES_NAME
			": No boards found.\n" );
		return -ENODEV;
	}

	if ( board_count == NI_DAQ_MAX_PCI_E_BOARDS )
		printk( KERN_WARNING BOARD_SERIES_NAME ": There could be more "
			"than the %d supported PCI-E series boards!\n",
			NI_DAQ_MAX_PCI_E_BOARDS );

#ifdef CONFIG_DEVFS_FS
	for ( i = 0; i < board_count; i++ ) {
		char dev_name[ 12 ];

		sprintf( name, BOARD_SERIES_NAME "_%d", i );
		boards[ i ].dev_handle =
			devfs_register( NULL, name, DEVFS_FL_AUTO_OWNER |
					DEVFS_FL_AUTO_DEVNUM, 0, 0,
					S_IFCHR | S_IRUGO | S_IWUGO,
					&ni_daq_file_ops, boards + i );
		if ( boards[ i ].dev_handle == NULL ) {
			printk( KERN_ERR BOARD_SERIES_NAME ": Failure to "
				"register board %d\n", i );
			while ( i > 0 )
				devfs_unregister( boards[ --i ].dev_handle );
			pci_e_series_release_resources( boards, board_count );
			return -ENODEV;
		}
	}
#else
	if ( ( major = register_chrdev( major, BOARD_SERIES_NAME,
					&ni_daq_file_ops ) ) < 0 ) {
		printk( KERN_ERR BOARD_SERIES_NAME ": Can't get assigned a "
			"major device number\n" );
		pci_e_series_release_resources( boards, board_count );
		return -ENODEV;
	}
#endif

	printk( KERN_INFO BOARD_SERIES_NAME ": Module successfully installed\n"
		BOARD_SERIES_NAME ": Major device number = %d\n"
		BOARD_SERIES_NAME ": Initialized %d board%s:\n",
		major, board_count, board_count > 1 ? "(s)" : "" );

	for ( i = 0; i < board_count; i++ )
		printk( KERN_INFO BOARD_SERIES_NAME ": %d. board is a %s\n",
			i + 1, boards[ i ].type->name );

	return 0;
}


/*---------------------------------------------------------------*
 * Initialization of a board (only use while loading the module)
 *---------------------------------------------------------------*/

static int __init pci_e_series_init_board( struct pci_dev * dev,
					   Board *          board )
{
	board->dev = dev;
	board->irq = 0;

	if ( pci_enable_device( dev ) ) {
		PDEBUG( "Failed to enable board %d\n", board - boards );
		goto init_failure;
	}

	pci_set_master( dev );

	if ( pci_request_regions( dev, BOARD_SERIES_NAME ) ) {
		PDEBUG( "Failed to lock memory regions for %d. board\n",
			board - boards + 1 );
		goto init_failure;
	}

	/* Determine the virtual address of the MITE */

	board->mite_phys = pci_resource_start( dev, 0 );
	board->mite_len  = pci_resource_len( dev, 0 );
	if ( ( board->mite = ioremap( board->mite_phys, board->mite_len ) )
	     == NULL ) {
		PDEBUG( "Can't remap MITE memory range for %d. board\n",
			board - boards + 1);
		goto init_failure;
	}

	/* Determine the virtual address of the memory region with the DAQs
	   registers and enable access to it in the MITE */

	board->daq_stc_phys = pci_resource_start( dev, 1 );
	board->daq_stc_len  = pci_resource_len( dev, 1 );
	if ( ( board->daq_stc = ioremap( board->daq_stc_phys,
					 board->daq_stc_len ) ) == NULL ) {
		PDEBUG( "Can't remap STC memory range for %d. board\n",
			board - boards + 1 );
		goto init_failure;
	}

	writel( ( board->daq_stc_phys & 0xFFFFFF00L ) | 0x80,
		board->mite + 0xC0 );

	/* Request the interrupt used by the board */

	if ( request_irq( dev->irq, pci_board_irq_handler, SA_SHIRQ,
			  BOARD_SERIES_NAME, board ) ) {
		PDEBUG( "Can't obtain IRQ %d for %d. board\n", dev->irq,
			board - boards + 1 );
		goto init_failure;
	}
	board->irq = dev->irq;

	spin_lock_init( &board->open_spinlock );

	pci_init_critical_section_handling( board );

	init_waitqueue_head( &board->AI.waitqueue );
	init_waitqueue_head( &board->AO.waitqueue );
	init_waitqueue_head( &board->GPCT0.waitqueue );
	init_waitqueue_head( &board->GPCT1.waitqueue );

	pci_e_series_init_all( board );

	board->in_use = 0;              /* board hasn't been opened yet */

	return 0;

 init_failure:
	pci_e_series_release_resources( boards, board_count );
	return -1;
}


/*------------------------------------------------------*
 * Function gets executed when the module gets unloaded
 *------------------------------------------------------*/

static void __exit pci_e_series_cleanup( void )
{
#ifdef CONFIG_DEVFS_FS
	int i;
#endif


	pci_e_series_release_resources( boards, board_count );

#ifdef CONFIG_DEVFS_FS
	for ( i = board_count - 1; i >= 0; i-- )
		devfs_unregister( boards[ i ].dev_handle );
#else
	if ( unregister_chrdev( major, BOARD_SERIES_NAME ) < 0 ) {
		printk( KERN_ERR BOARD_SERIES_NAME ": Unable to unregister "
			"module with major = %d\n", major );
		return;
	}
#endif
	printk( KERN_INFO BOARD_SERIES_NAME ": Module (controlling "
		"%d board%s) successfully removed\n", board_count,
		board_count == 1 ? "" : "s" );
}


/*---------------------------------------------------------------*
 * Function for releasing all resources requested for the boards
 *---------------------------------------------------------------*/

static void pci_e_series_release_resources( Board * boards,
					    int     board_count )
{
	int i;


	for ( i = 0; i < board_count; i++ ) {
		pci_mite_close( boards + i );

		if ( boards[ i ].irq ) {
			free_irq( boards[ i ].irq, boards + i );
			boards[ i ].irq = 0;
		}

		if ( boards[ i ].daq_stc ) {
			iounmap( boards[ i ].daq_stc );
			boards[ i ].daq_stc = NULL;
		}

		if ( boards[ i ].mite ) {
			iounmap( boards[ i ].mite );
			boards[ i ].mite = NULL;
		}

		if ( boards[ i ].dev ) {
			pci_release_regions( boards[ i ].dev );
			boards[ i ].dev = NULL;
		}
	}
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static void pci_e_series_init_all( Board * board )
{
	/* Set up the pointers to the registers, the list of windowed
	   register offsets and the structures for interrupt handling */

	boards->regs = regs + ( board - boards );
	pci_board_register_setup( board );
	pci_board_irq_handling_setup( board );

	/* Clear the copy of the contents of write-only DAQ-STC registers */

	memset( &board->stc, sizeof board->stc, 0 );

	/* Set up the MITE */

	pci_mite_init( board );

	/* Reset the board to a known, inactive state */

	pci_board_reset_all( board );
}


EXPORT_NO_SYMBOLS;


module_init( pci_e_series_init );
module_exit( pci_e_series_cleanup );

MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number to use" );
MODULE_AUTHOR( "Jens Thoms Toerring <jt@toerring.de>" );
MODULE_DESCRIPTION( "National Instruments PCI E Series DAQ board driver" );


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
