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

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static Board_Functions func = {
	.board_reset_all =            pci_board_reset_all,
	.stc_writew =                 pci_stc_writew,
	.stc_writel =                 pci_stc_writel,
	.stc_readw =                  pci_stc_readw,
	.stc_readl =                  pci_stc_readl,
	.start_critical_section =     pci_start_critical_section,
	.end_critical_section =       pci_end_critical_section,
	.clear_configuration_memory = pci_clear_configuration_memory,
	.clear_ADC_FIFO =             pci_clear_ADC_FIFO,
	.clear_DAC_FIFO =             pci_clear_DAC_FIFO,
	.configuration_high =         pci_configuration_high,
	.configuration_low =          pci_configuration_low,
	.ao_configuration =           pci_ao_configuration,
	.dac_direct_data =            pci_dac_direct_data,
	.dma_setup =                  pci_dma_setup,
	.dma_buf_setup =              pci_dma_buf_setup,
	.dma_buf_get =                pci_dma_buf_get,
	.dma_get_available =          pci_dma_get_available,
	.dma_shutdown =               pci_dma_shutdown,
	.set_trigger_levels =         pci_set_trigger_levels
};
#else
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
#endif

extern struct file_operations ni_daq_file_ops;   /* from daq_stc/fops.c */


static void pci_e_series_release_resources( Board * board );

static void pci_e_series_init_all( Board * board );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static int pci_e_series_init_board( struct pci_dev * dev,
				    const struct pci_device_id * id );
static void pci_e_series_release_board( struct pci_dev * dev );
#else
static int pci_e_series_init_board( struct pci_dev * dev,
				    Board *          board );
#endif

static unsigned int num_pci_e_series_boards = 
		sizeof pci_e_series_boards / sizeof pci_e_series_boards[ 0 ];


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static struct pci_driver pci_e_series_pci_driver = {
	.name     = BOARD_SERIES_NAME,
	.id_table = pci_e_series_pci_tbl,
	.probe    = pci_e_series_init_board,
	.remove   = pci_e_series_release_board,
};

static dev_t dev_no;
static struct cdev ch_dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 14 )
static struct class *pci_e_series_class;
#endif

#endif


/*-------------------------------------------------------*
 * Function that gets executed when the module is loaded
 *-------------------------------------------------------*/

static int __init pci_e_series_init( void )
{
	struct pci_dev *dev = NULL;
	unsigned int i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
	int result;
#endif


	/* Try to find all boards in the sequence they appear on the PCI bus */

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )

	while ( ( dev = pci_get_device( PCI_VENDOR_ID_NATINST,
					PCI_ANY_ID, dev ) ) != NULL ) {
		for ( i = 0; i < num_pci_e_series_boards; i++ )
			if ( dev->device == pci_e_series_boards[ i ].id )
				break;

		if ( i == num_pci_e_series_boards )
			continue;

		if ( board_count >= NI_DAQ_MAX_PCI_E_BOARDS )
			break;

		boards[ board_count ].dev = dev;
		boards[ board_count ].is_init = 0;
		boards[ board_count ].is_enabled = 0;
		boards[ board_count ].in_use = 0;
		boards[ board_count ].has_region = 0;
		boards[ board_count ].mite = NULL;
		boards[ board_count ].daq_stc = NULL;
		boards[ board_count ].type = pci_e_series_boards + i;
		boards[ board_count ].func = &func;
		boards[ board_count++ ].irq = 0;

		pci_dev_put( dev );
	}

#else

	while ( ( dev = pci_find_device( PCI_VENDOR_ID_NATINST,
					 PCI_ANY_ID, dev ) ) != NULL ) {
		for ( i = 0; i < num_pci_e_series_boards; i++ )
			if ( dev->device == pci_e_series_boards[ i ].id )
				break;

		if ( i == num_pci_e_series_boards )
			continue;

		if ( board_count >= NI_DAQ_MAX_PCI_E_BOARDS )
			break;

		boards[ board_count ].type = pci_e_series_boards + i;
		boards[ board_count ].func = &func;

		if ( pci_e_series_init_board( dev, boards + board_count ) ) {
			printk( KERN_ERR BOARD_SERIES_NAME ": Failed to "
				"initialize %d. board (%s)\n",
				board_count, boards[ i ].type->name );
			for ( i = 0; i <= board_count; i++ )
				pci_e_series_release_resources( boards + i );
			return -EIO;
		}

		board_count++;
	}
#endif

	if ( board_count == 0 ) {
		printk( KERN_WARNING BOARD_SERIES_NAME
			": No boards found.\n" );
		return -ENODEV;
	}

	if ( board_count >= NI_DAQ_MAX_PCI_E_BOARDS && dev != NULL )
		printk( KERN_WARNING BOARD_SERIES_NAME ": There are more than "
			"the currently supported maximum number of %d PCI-E "
			"series boards!\n", NI_DAQ_MAX_PCI_E_BOARDS );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )

	if ( major == 0 )
		result = alloc_chrdev_region( &dev_no, 0,board_count,
					      BOARD_SERIES_NAME );
	else {
		dev_no = MKDEV( major, 0 );
		result = register_chrdev_region( dev_no, board_count,
						 BOARD_SERIES_NAME );
	}

	if ( result < 0 ) {
                printk( KERN_ERR BOARD_SERIES_NAME ": Can't register as char "
			"device.\n" );
		return -EIO;
	}

	if ( major == 0 )
		major = MAJOR( dev_no );

	cdev_init( &ch_dev, &ni_daq_file_ops );
	ch_dev.owner = THIS_MODULE;

	if ( cdev_add( &ch_dev, dev_no, board_count ) < 0 ) {
                printk( KERN_ERR BOARD_SERIES_NAME ": Can't register as char "
			"device.\n" );
		unregister_chrdev_region( dev_no, board_count );
		return -EIO;
	}

	if ( ( result = pci_register_driver( &pci_e_series_pci_driver ) )
	                                                                < 0 ) {
		unregister_chrdev_region( dev_no, board_count );
		printk( KERN_ERR BOARD_SERIES_NAME ": Registering PCI device "
			"failed.\n" );
		return result;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 14 )
	pci_e_series_class = class_create( THIS_MODULE, BOARD_SERIES_NAME );

	if ( IS_ERR( pci_e_series_class ) ) {
		printk( KERN_ERR BOARD_SERIES_NAME ": Can't create a class "
			"for the device.\n" );
		pci_unregister_driver( &pci_e_series_pci_driver );
		cdev_del( &ch_dev );
		unregister_chrdev_region( dev_no, board_count );
		return -EIO;
	}

	for ( i = 0; i < board_count; i++ )
		class_device_create( pci_e_series_class, NULL, major, NULL,
				     BOARD_SERIES_NAME "_%i", i );
#endif

	printk( KERN_INFO BOARD_SERIES_NAME ": Module successfully installed\n"
		BOARD_SERIES_NAME ": Major device number = %d\n"
		BOARD_SERIES_NAME ": Found %d board%s:\n",
		major, board_count, board_count > 1 ? "(s)" : "" );

#else

	if ( ( major = register_chrdev( major, BOARD_SERIES_NAME,
					&ni_daq_file_ops ) ) < 0 ) {
		printk( KERN_ERR BOARD_SERIES_NAME ": Can't get assigned a "
			"major device number\n" );
		for ( i = 0; i < board_count; i++ )
			pci_e_series_release_resources( boards + i );
		return -ENODEV;
	}

	printk( KERN_INFO BOARD_SERIES_NAME ": Module successfully installed\n"
		BOARD_SERIES_NAME ": Major device number = %d\n"
		BOARD_SERIES_NAME ": Initialized %d board%s:\n",
		major, board_count, board_count > 1 ? "(s)" : "" );

#endif

	for ( i = 0; i < board_count; i++ )
		printk( KERN_INFO BOARD_SERIES_NAME ": %d. board is a %s\n",
			i + 1, boards[ i ].type->name );

	return 0;
}


/*---------------------------------------------------------------*
 * Initialization of a board (only use while loading the module)
 *---------------------------------------------------------------*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static int pci_e_series_init_board( struct pci_dev * dev,
				    const struct pci_device_id * id )
{
	Board *board;
	int i;


	for ( i = 0; i < board_count; i++ )
		if ( boards[ i ].dev == dev )
			break;

	if ( i == NI_DAQ_MAX_PCI_E_BOARDS ) {
		printk( KERN_ERR BOARD_SERIES_NAME
			": pci_e_series_init_board() called with unknown "
			"pci_dev structure.\n" );
		return -1;
	}

	board = boards + i;

	if ( pci_enable_device( dev ) ) {
		PDEBUG( "Failed to enable board %d\n", board - boards );
		return -1;
	}

	board->is_enabled = 1;

	pci_set_master( dev );

	if ( pci_request_regions( dev, BOARD_SERIES_NAME ) ) {
		pci_e_series_release_resources( board );
		PDEBUG( "Failed to lock memory regions for %d. board\n",
			board - boards + 1 );
		return -1;
	}

	board->has_region = 1;

	/* Determine the virtual address of the MITE */

	board->mite_phys = pci_resource_start( dev, 0 );
	board->mite_len  = pci_resource_len( dev, 0 );
	if ( ( board->mite = ioremap( board->mite_phys, board->mite_len ) )
	     == NULL ) {
		pci_e_series_release_resources( board );
		PDEBUG( "Can't remap MITE memory range for %d. board\n",
			board - boards + 1);
		return -1;
	}

	/* Determine the virtual address of the memory region with the DAQs
	   registers and enable access to it in the MITE */

	board->daq_stc_phys = pci_resource_start( dev, 1 );
	board->daq_stc_len  = pci_resource_len( dev, 1 );
	if ( ( board->daq_stc = ioremap( board->daq_stc_phys,
					 board->daq_stc_len ) ) == NULL ) {
		pci_e_series_release_resources( board );
		PDEBUG( "Can't remap STC memory range for %d. board\n",
			board - boards + 1 );
		return -1;
	}

	iowrite32( ( board->daq_stc_phys & 0xFFFFFF00L ) | 0x80,
		   board->mite + 0xC0 );

	/* Request the interrupt used by the board */

	if ( request_irq( dev->irq, pci_board_irq_handler, SA_SHIRQ,
			  BOARD_SERIES_NAME, board ) ) {
		pci_e_series_release_resources( board );
		PDEBUG( "Can't obtain IRQ %d for %d. board\n", dev->irq,
			board - boards + 1 );
		return -1;
	}
	board->irq = dev->irq;

	init_MUTEX( &board->open_mutex );
	init_MUTEX( &board->use_mutex );

	pci_init_critical_section_handling( board );

	init_waitqueue_head( &board->AI.waitqueue );
	init_waitqueue_head( &board->AO.waitqueue );
	init_waitqueue_head( &board->GPCT0.waitqueue );
	init_waitqueue_head( &board->GPCT1.waitqueue );

	pci_e_series_init_all( board );

	board->in_use = 0;              /* board hasn't been opened yet */

	board->is_init = 1;

	return 0;
}

#else

static int __init pci_e_series_init_board( struct pci_dev * dev,
					   Board *          board )
{
	board->dev = dev;
	board->is_init = 0;
	board->in_use = 0;
	board->is_enabled = 0;
	board->has_region = 0;
	board->mite = NULL;
	board->daq_stc = NULL;
	board->irq = 0;

	if ( pci_enable_device( dev ) ) {
		PDEBUG( "Failed to enable board %d\n", board - boards );
		return -1;
	}

	board->is_enabled = 1;

	pci_set_master( dev );

	if ( pci_request_regions( dev, BOARD_SERIES_NAME ) ) {
		PDEBUG( "Failed to lock memory regions for %d. board\n",
			board - boards + 1 );
		return -1;
	}

	board->has_region = 1;

	/* Determine the virtual address of the MITE */

	board->mite_phys = pci_resource_start( dev, 0 );
	board->mite_len  = pci_resource_len( dev, 0 );
	if ( ( board->mite = ioremap( board->mite_phys, board->mite_len ) )
	     == NULL ) {
		PDEBUG( "Can't remap MITE memory range for %d. board\n",
			board - boards + 1);
		return -1;
	}

	/* Determine the virtual address of the memory region with the DAQs
	   registers and enable access to it in the MITE */

	board->daq_stc_phys = pci_resource_start( dev, 1 );
	board->daq_stc_len  = pci_resource_len( dev, 1 );
	if ( ( board->daq_stc = ioremap( board->daq_stc_phys,
					 board->daq_stc_len ) ) == NULL ) {
		PDEBUG( "Can't remap STC memory range for %d. board\n",
			board - boards + 1 );
		return -1;
	}

	iowrite32( ( board->daq_stc_phys & 0xFFFFFF00L ) | 0x80,
		   board->mite + 0xC0 );

	/* Request the interrupt used by the board */

	if ( request_irq( dev->irq, pci_board_irq_handler, SA_SHIRQ,
			  BOARD_SERIES_NAME, board ) ) {
		PDEBUG( "Can't obtain IRQ %d for %d. board\n", dev->irq,
			board - boards + 1 );
		return -1;
	}
	board->irq = dev->irq;

	sema_init( &board->open_mutex, 1 );
	sema_init( &board->use_mutex, 1 );

	pci_init_critical_section_handling( board );

	init_waitqueue_head( &board->AI.waitqueue );
	init_waitqueue_head( &board->AO.waitqueue );
	init_waitqueue_head( &board->GPCT0.waitqueue );
	init_waitqueue_head( &board->GPCT1.waitqueue );

	pci_e_series_init_all( board );

	board->in_use = 0;              /* board hasn't been opened yet */

	board->is_init = 1;

	return 0;
}

#endif


/*------------------------------------------------------*
 * Function gets executed when the module gets unloaded
 *------------------------------------------------------*/

static void __exit pci_e_series_cleanup( void )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 14 )
	int i;

	for ( i = 0; i <board_count; i++ )
		class_device_destroy( pci_e_series_class, major );
        class_destroy( pci_e_series_class );
#endif

	pci_unregister_driver( &pci_e_series_pci_driver );
	cdev_del( &ch_dev );
	unregister_chrdev_region( dev_no, board_count );

#else
	int i;


	for ( i = 0; i < board_count; i++ )
		pci_e_series_release_resources( boards + i );

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


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static void pci_e_series_release_board( struct pci_dev * dev )
{
	int i;


	for ( i = 0; i < board_count; i++ )
		if ( boards[ i ].dev == dev )
			break;

	if ( i == board_count ) {
		printk( KERN_ERR BOARD_SERIES_NAME
			": pci_e_series_release_board() called "
			"with unknown pci_dev structure.\n" );
		return;
	}

	if ( ! boards[ i ].is_init ) {
		printk( KERN_ERR BOARD_SERIES_NAME
			": pci_e_series_release_board() called on board that "
			"never got initialized or already was released.\n" );
		return;
	}

	pci_e_series_release_resources( boards + i );

	boards[ i ].is_init = 0;
}
#endif


/*------------------------------------------------------------*
 * Function for releasing all resources requested for a board
 *------------------------------------------------------------*/

static void pci_e_series_release_resources( Board * board )
{
	if ( board->irq ) {
		free_irq( boards->irq, board );
		board->irq = 0;
	}

	if ( board->daq_stc ) {
		iounmap( board->daq_stc );
		board->daq_stc = NULL;
	}

	if ( board->mite ) {
		pci_mite_close( board );
		iounmap( board->mite );
		board->mite = NULL;
	}

	if ( board->has_region ) {
		pci_release_regions( board->dev );
		board->has_region = 1;
	}

	if ( board->is_enabled ) {
		pci_disable_device( board->dev );
		board->is_enabled = 0;
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


module_init( pci_e_series_init );
module_exit( pci_e_series_cleanup );

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
EXPORT_NO_SYMBOLS;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
module_param( major, int, S_IRUGO );
#else
MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number to use" );
#endif

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
