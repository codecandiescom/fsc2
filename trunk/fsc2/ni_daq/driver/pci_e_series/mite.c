/*
 *  $Id$
 * 
 *  Driver for National Instruments PCI E Series DAQ boards
 * 
 *  Copyright (C) 2003-2007 Jens Thoms Toerring
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


typedef struct Subsystem_Data Subsystem_Data;

struct Subsystem_Data {
	int num_links;
	size_t max_size;
	size_t transfered;
};

static Subsystem_Data sys_data[ 4 ];

static void pci_dma_stop( Board *          board,
			  NI_DAQ_SUBSYSTEM sys );


/*---------------------------------------------------------------------*
 * Function for initializing the MITE, bringing it into a well-defined
 * quiet state and the assigning the different MITE DMA channels to
 * different subsystems of the board.
 *---------------------------------------------------------------------*/

void pci_mite_init( Board * board )
{
	int i;


	/* Reset DMA and FIFO for all channels, disable interrupts and
	   initialize the pointers to the DMA chain memory (one for each
	   logical DMA channel) */

	for ( i = 0; i < board->type->num_data_channels; i++ ) {
		iowrite32( CHOR_DMARESET | CHOR_FRESET, mite_CHOR( i ) );
		iowrite32( 0, mite_CHCR( i ) );
		board->data_buffer[ i ] = NULL;
		sys_data[ i ].num_links = 0;
		sys_data[ i ].transfered = 0;
	}

	/* Logical DMA channel A is going to be used for the AI subsystem,
	   channel B for AO (if available), channel C for GPCT0 and
	   channel D for GPCT1. */

	if ( board->type->ao_num_channels > 0 )
		iowrite8( Output_B | Input_A, board->regs->AI_AO_Select );
	else
		iowrite8( Input_A, board->regs->AI_AO_Select );

	iowrite8( GPCT1_D | GPCT0_C, board->regs->G0_G1_Select );
}


/*----------------------------------------------------------*
 * Function gets called when the module is unloaded to stop
 * all MITE activities and freeing DMA kernel buffers.
 *----------------------------------------------------------*/

void pci_mite_close( Board * board )
{
	int i;


	for ( i = 0; i < board->type->num_data_channels; i++ )
		pci_dma_shutdown( board, i );

	iowrite8( 0, board->regs->AI_AO_Select );
	iowrite8( 0, board->regs->G0_G1_Select );
}


/*----------------------------------------------------------------------*
 * Function allocates kernel buffers for DMA transfer to and from the
 * board. The MITE chip allows hardware scatter-gather, i.e. it accepts
 * a linked list of buffers and switches between buffers if necessary.
 * The default size of the buffers can be set by the DMA_BUFFER_SIZE
 * macro, but this value is increased to a power of the PAGE size if
 * necessary. Setting this to too large a value may lead to allocation
 * failures because a buffer of the requested size may not be available,
 * while too small a buffer size may require too many buffers.
 *----------------------------------------------------------------------*/

#define DMA_BUFFER_SIZE  16384   /* 16 kByte */

int pci_dma_buf_setup( Board *          board,
		       NI_DAQ_SUBSYSTEM sys,
		       size_t           num_points,
		       int              continuous )
{
	MITE_DMA_Chain_t *mdc;
	int num_links;
	unsigned long size;
	unsigned long buf_size = 1;
	unsigned long last_buf_size = 1;
	unsigned long order = 0;
	unsigned long last_order = 0;
	int i;


	/* Make sure memory already allocated for the subsystem is released
	   before we continue */

	pci_dma_buf_release( board, sys );

	/* Make the buffers size fit the page size */

	while ( buf_size < DMA_BUFFER_SIZE ) {
		buf_size <<= 1;
		order++;
	}
	order = ( order - PAGE_SHIFT > 0 ) ? order - PAGE_SHIFT : 0;
	buf_size = 1UL << ( order + PAGE_SHIFT );

	/* Calculate how many links in the chain are needed and how much
	   memory (in powers of PAGE_SIZE) is needed for the last buffer */

	sys_data[ sys ].max_size = size = num_points * sizeof( u16 );

	num_links = size / buf_size;

	if ( size % buf_size > 0 ) {
		num_links++;

		while ( last_buf_size < size % buf_size ) {
			last_buf_size <<= 1;
			last_order++;
		}

		last_order = ( last_order >= PAGE_SHIFT ) ?
			     last_order - PAGE_SHIFT : 0;
	}

	sys_data[ sys ].num_links = num_links;
	sys_data[ sys ].transfered = 0;

	/* Get memory for the link chain (plus an additional one as the
	   terminator) */

	board->data_buffer[ sys ] = kmalloc( ( num_links + 1 )
					     * sizeof **board->data_buffer,
					     GFP_KERNEL );

	if ( board->data_buffer[ sys ] == NULL ) {
		PDEBUG( "Not enough memory for DMA chain links\n" );
		return -ENOMEM;
	}

	/* Allocate memory for the DMA buffer(s) and put their addresses into
	   the non-MITE-related part of the link chain structures */

	board->data_buffer[ sys ][ num_links ].buf = NULL;

	for ( mdc = board->data_buffer[ sys ], i = 0; i < num_links;
	      i++, mdc++ ) {
		if ( size >= buf_size ) {
			mdc->buf = ( char * ) __get_free_pages( GFP_KERNEL,
								order );
			mdc->size = buf_size;
			mdc->order = order;
			size -= buf_size;
		} else {
			mdc->buf = ( char * ) __get_free_pages( GFP_KERNEL,
								last_order );
			board->data_buffer[ sys ][ i ].size = size;
			mdc->order = last_order;
		}

		if ( mdc->buf == NULL )
			break;

		mdc->transfered = 0;
		mdc->is_mapped = 0;
	}

	/* If memory allocation failed deallocate everything */

	if ( i < num_links ) {
		for ( i--, mdc--; i >= 0; i--, mdc-- )
			free_pages( ( unsigned long ) mdc->buf, mdc->order );
		kfree( board->data_buffer[ sys ] );
		board->data_buffer[ sys ] = NULL;
		PDEBUG( "Not enough memory for DMA buffer\n" );
		return -ENOMEM;
	}

	/* Set up the structures needed by the MITE as link chains. For the
	   DMA memory buffers streaming mapping must be set up (except for
	   the AO buffers because data still need to be written to them before
	   they can be passed to the MITE). Take care, the link chain pointers
	   (LKAR) must be bus addresses. */

	for ( mdc = board->data_buffer[ sys ]; mdc->buf != NULL; mdc++ ) {

		mdc->lc.tcr = cpu_to_le32( mdc->size );

		if ( sys != NI_DAQ_AO_SUBSYSTEM ) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
			mdc->lc.mar = cpu_to_le32( dma_map_single(
							   &board->dev->dev,
							   mdc->buf, mdc->size,
							   DMA_FROM_DEVICE ) );
#else
			mdc->lc.mar = cpu_to_le32( pci_map_single( board->dev,
							mdc->buf, mdc->size,
							PCI_DMA_FROMDEVICE ) );
#endif
			if ( mdc->lc.mar == 0 )
				break;
			mdc->is_mapped = 1;
		}

		mdc->lc.dar = 0;
		mdc->lc.lkar = cpu_to_le32( virt_to_bus( &( mdc + 1 )->lc ) );
	}

	/* If the streaming mapping failed all mapped buffers must be unmapped
	   and then all memory released */

	if ( mdc != board->data_buffer[ sys ] + num_links ) {
		for ( mdc = board->data_buffer[ sys ]; mdc->buf != NULL;
		      mdc++ ) {
			if ( mdc->is_mapped )
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
				dma_unmap_single( &board->dev->dev,
						  le32_to_cpu( mdc->lc.mar ),
						  mdc->size,
						  DMA_FROM_DEVICE );
#else
				pci_unmap_single( board->dev,
						  le32_to_cpu( mdc->lc.mar ),
						  mdc->size,
						  PCI_DMA_FROMDEVICE );
#endif
			free_pages( ( unsigned long ) mdc->buf, mdc->order );
		}

		kfree( board->data_buffer[ sys ] );
		board->data_buffer[ sys ] = NULL;
		PDEBUG( "Failed to set up DMA stream mapping\n" );
		return -ENOMEM;
	}

	/* The terminator element of the MITE link chains must have all fields
	   set to 0 */

	mdc->lc.tcr = mdc->lc.mar = mdc->lc.dar = mdc->lc.lkar = 0;

	/* If continuous input or output (not yet implemented!) is to be done
	   the LKAR pointer in the last link must point back to the first
	   element */

	if ( continuous )
		board->data_buffer[ sys ][ num_links - 1 ].lc.lkar =
			cpu_to_le32( virt_to_bus(
					&board->data_buffer[ sys ][ 0 ].lc ) );

	return 0;
}


/*-------------------------------------------------------------*
 * Gets newly acquired data from a subsystem. The function not
 * necessarily returns as many bytes as requested by what the
 * 'size' argument points to but only as many as are currently
 * available, writing them to the 'dest' buffer. If it's clear
 * that all data from the acquisition have already been read
 * DMA is disabled and the DMA kernel buffers are released.
 * When the function returns 'size' points to the number of
 * transfered bytes. A negative return value indicates an
 * error, a return value of 0 means that everything went well
 * and there are still more data to be expected, while a value
 * of 1 also means success but that all data to be expected
 * now have been fetched and the DMA system has been shut down
 * (and a further call would fail unless a new acquisition has
 * been started in the mean time).
 * Please note: The data returned may belong to different AI
 *              channels. This depends on the channel setup:
 *              a scan returns a value for each channels in
 *              the channel setup (unless the type is GHOST).
 *              Furthermore, the data are "raw" data, i.e.
 *              2-byte, little-endian numbers.
 *-------------------------------------------------------------*/

int pci_dma_buf_get( Board *          board,
		     NI_DAQ_SUBSYSTEM sys,
		     void __user *    dest,
		     size_t *         size,
		     int              still_used )
{
	size_t left;
	size_t transf = 0;
	size_t total = 0;
	MITE_DMA_Chain_t *mdc;
	size_t avail;


	/* When no DMA buffers are allocated calling the function is a bad
	   mistake */

	if ( board->data_buffer[ sys ] == NULL ) {
		PDEBUG( "Can't read DMA buffers, none allocated\n" );
		return -EINVAL;
	}

	/* Figure out how many new data are available and restrict the
	   number of bytes to be returned to this value */

	if ( ( avail = pci_dma_get_available( board, sys ) ) == 0 ) {
		*size = 0;
		return 0;
	}

	if ( sys == NI_DAQ_AI_SUBSYSTEM )
		avail *= 2 * board->AI.num_data_per_scan;

	if ( avail < *size )
		*size = avail;

	/* Copy as many of the newly acquired bytes as requested or possible
	   to the user supplied buffer */

	for ( mdc = board->data_buffer[ sys ]; *size > 0 && mdc->buf != NULL;
	      mdc++ ) {

		/* Skip the current link if it has already been used up, i.e.
		   all the data it contains are already send to the user*/

		if ( ( left = mdc->size - mdc->transfered ) == 0 )
			continue;

		/* Calculate how many bytes there are left in the link */

		transf = left >= *size ? *size : left;

		/* Unmap the buffer (or sync it if it still might be needed) */

		if ( mdc->is_mapped ) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
			if ( still_used ||
			     mdc->transfered + transf < mdc->size )
				dma_sync_single_for_cpu( &board->dev->dev,
						     le32_to_cpu( mdc->lc.mar )
						     + mdc->transfered,
						     transf, DMA_FROM_DEVICE );
			else {
				dma_unmap_single( &board->dev->dev,
						  le32_to_cpu( mdc->lc.mar )
						  + mdc->transfered,
						  transf, DMA_FROM_DEVICE );
				mdc->is_mapped = 0;
			}
#else
			if ( still_used ||
			     mdc->transfered + transf < mdc->size )
				pci_dma_sync_single( board->dev,
						  le32_to_cpu( mdc->lc.mar )
						  + mdc->transfered,
						  transf, PCI_DMA_FROMDEVICE );
			else {
				pci_unmap_single( board->dev,
						  le32_to_cpu( mdc->lc.mar )
						  + mdc->transfered,
						  transf, PCI_DMA_FROMDEVICE );
				mdc->is_mapped = 0;
			}
#endif
		}

		/* Copy its (yet unused) content to the user buffer */

		if ( copy_to_user( dest + total, mdc->buf + mdc->transfered,
				   transf ) ) {
			pci_dma_shutdown( board, sys );
			PDEBUG( "Can't write to user space\n" );
			return -EFAULT;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
		if ( mdc->is_mapped )
			dma_sync_single_for_device( &board->dev->dev,
						    le32_to_cpu( mdc->lc.mar )
						    + mdc->transfered,
						    transf, DMA_FROM_DEVICE );
#endif

		mdc->transfered += transf;
		*size -= transf;
		total += transf;
	}

	sys_data[ sys ].transfered += total;
	*size = total;

	/* If all data points that can be expected have been fetched disable
	   DMA, release the buffers and return 1 to indicate this */

	if ( sys_data[ sys ].max_size <= sys_data[ sys ].transfered ) {
		pci_dma_shutdown( board, sys );
		return 1;
	}

	return 0;
}


/*-------------------------------------------------------------------------*
 * For the AI subsystem the function returns the number of scans for which
 * data have been written to memory. For all other subsystems (where the
 * function is not used yet) it returns the number of bytes that have been
 * written to memory or read by the board since the function was called
 * the last time.
 *-------------------------------------------------------------------------*/

size_t pci_dma_get_available( Board *          board,
			      NI_DAQ_SUBSYSTEM sys )
{
	/* It seems to be important to read from the MITE DAR register first
	   and only then from the FCR register - otherwise sometimes the very
	   last data point read later on was garbage if during the read of
	   the data there was heavy disk I/O (also using DMA) going on. That's
	   also why the the next two lines aren't written as a single line,
	   when doing that the compiler rearranged the code to read the FCR
	   first and the DAR only afterwards. */

	u32 avail = le32_to_cpu( ioread32( mite_DAR( sys ) ) );

	avail -= ( le32_to_cpu( ioread32( mite_FCR( sys ) ) ) & 0x000000FF )
	         + sys_data[ sys ].transfered;

	/* Only tell about AI data for completed scans */

	if ( sys == NI_DAQ_AI_SUBSYSTEM )
		avail = avail / ( 2 * board->AI.num_data_per_scan );

	return avail;
}


/*------------------------------------------------------*
 * Function for unmapping and releasing all DMA buffers
 * allocated for a subsystem of the board
 *------------------------------------------------------*/

void pci_dma_buf_release( Board *          board,
			  NI_DAQ_SUBSYSTEM sys )
{
	MITE_DMA_Chain_t *mdc;


	if ( board->data_buffer[ sys ] == NULL )
		return;

	for ( mdc = board->data_buffer[ sys ]; mdc->buf != NULL; mdc++ ) {
		if ( mdc->is_mapped )
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
			dma_unmap_single( &board->dev->dev,
					  le32_to_cpu( mdc->lc.mar ),
					  mdc->size,
					  sys == NI_DAQ_AI_SUBSYSTEM ?
					  DMA_FROM_DEVICE : DMA_TO_DEVICE );
#else
			pci_unmap_single( board->dev,
					  le32_to_cpu( mdc->lc.mar ),
					  mdc->size,
					  sys == NI_DAQ_AI_SUBSYSTEM ?
					  PCI_DMA_FROMDEVICE :
					  PCI_DMA_TODEVICE );
#endif

		free_pages( ( unsigned long ) mdc->buf, mdc->order );
	}

	kfree( board->data_buffer[ sys ] );
	board->data_buffer[ sys ] = NULL;

	sys_data[ sys ].num_links = 0;
	sys_data[ sys ].transfered = 0;
}


/*------------------------------------------------*
 * Function enables DMA for a subsystem. Requires
 * that DMA memory has already been allocated.
 *------------------------------------------------*/

int pci_dma_setup( Board *          board,
		   NI_DAQ_SUBSYSTEM sys )
{
	MITE_DMA_Chain_t *mdc;
	u32 chcr;
	u32 dcr;
	u32 lkar;


	if ( board->data_buffer[ sys ] == NULL ) {
		PDEBUG( "No DMA buffers allocated\n" );
		return -EINVAL;
	}

	/* Make sure the MITE isn't already DMA-enabled */

	pci_dma_stop( board, sys );

	/* Run MITE DMA channel in short link mode and set the transfer
	   direction */

	chcr = CHCR_LINKSHORT;

	if ( sys != NI_DAQ_AO_SUBSYSTEM )
		chcr |= CHCR_DEV_TO_MEM;

	iowrite32( cpu_to_le32( chcr ), mite_CHCR( sys ) );

	board->mite_irq_enabled[ sys ] = 1;

	/* 16 bits to memory */

	iowrite32( cpu_to_le32( CR_RL64 | CR_ASEQxP1 | CR_PSIZEHALF ),
		   mite_MCR( sys ) );

	/* 16 bits from device */

	dcr = CR_RL64 |  CR_ASEQx( 1 ) | CR_PSIZEHALF |
	      CR_PORTIO | CR_AMDEVICE | CR_REQS( sys );
	iowrite32( cpu_to_le32( dcr ), mite_DCR( sys ) );
    
	/* Reset the DAR */

	iowrite32( 0, mite_DAR( sys ) );
    
	/* The link is 32 bits wide */

	iowrite32( cpu_to_le32( CR_RL64 | CR_ASEQUP | CR_PSIZEWORD ),
		   mite_LKCR( sys ) );

	/* Before finally passing the pointer to the link chain to the MITE
	   make sure all buffers are streaming mapped */

	for ( mdc = board->data_buffer[ sys ]; mdc->buf != NULL; mdc++ ) {

		if ( mdc->is_mapped )
			continue;

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
		mdc->lc.mar = cpu_to_le32( dma_map_single(
					   &board->dev->dev,
					   mdc->buf, mdc->size,
					   sys == NI_DAQ_AO_SUBSYSTEM ? 
					   DMA_TO_DEVICE : DMA_FROM_DEVICE ) );
#else
		mdc->lc.mar = cpu_to_le32( pci_map_single(
				       	       board->dev, mdc->buf, mdc->size,
					       sys == NI_DAQ_AO_SUBSYSTEM ? 
					       PCI_DMA_TODEVICE :
					       PCI_DMA_FROMDEVICE ) );
#endif
		if ( mdc->lc.mar == 0 ) {
			PDEBUG( "Failed to set up DMA stream mapping\n" );
			pci_dma_shutdown( board, sys );
			return -ENOMEM;
		}

		mdc->is_mapped = 1;
	}

	/* Set start address for link chaining */

	lkar = virt_to_bus( &board->data_buffer[ sys ][ 0 ].lc );
	iowrite32( cpu_to_le32( lkar ), mite_LKAR( sys ) );

	/* Arm the MITE for DMA transfers */

	iowrite32( CHOR_START, mite_CHOR( sys ) );

	return 0;
}


/*------------------------------------------------------*
 * Disables DMA and releases DMA memory for a subsystem
 *------------------------------------------------------*/

int pci_dma_shutdown( Board *          board,
		      NI_DAQ_SUBSYSTEM sys )
{
	if ( board->mite_irq_enabled[ sys ] == 0 )
		return 0;

	/* Stop the MITE */

	pci_dma_stop( board, sys );

	/* Release DMA memory and clear the ADC FIFO */

	pci_dma_buf_release( board, sys );
	pci_clear_ADC_FIFO( board );

	return 0;
}


/*-------------------------*
 * Stops DMA for subsystem
 *-------------------------*/

static void pci_dma_stop( Board *          board,
			  NI_DAQ_SUBSYSTEM sys )
{
	if ( board->mite_irq_enabled[ sys ] == 0 )
		return;

	/* Stop the MITE */

	iowrite32( CHOR_DMARESET | CHOR_CLRDONE | CHOR_FRESET | CHOR_ABORT,
		   mite_CHOR( sys ) );
	board->mite_irq_enabled[ sys ] = 0;
}



/*--------------------------------------------------------------------*
 * Function for printing out all accessible registers of a channel of
 * the MITE - only available when compiling with debugging support.
 *--------------------------------------------------------------------*/

#if defined NI_DAQ_DEBUG
void pci_mite_dump( Board * board,
		    int     channel )
{	PDEBUG( "mite_CHOR  = 0x%08X\n", ioread32( mite_CHOR(  channel ) ) );
	PDEBUG( "mite_CHCR  = 0x%08X\n", ioread32( mite_CHCR(  channel ) ) );
	PDEBUG( "mite_TCR   = 0x%08X\n", ioread32( mite_TCR(   channel ) ) );
	PDEBUG( "mite_CHSR  = 0x%08X\n", ioread32( mite_CHSR(  channel ) ) );
	PDEBUG( "mite_MCR   = 0x%08X\n", ioread32( mite_MCR(   channel ) ) );
	PDEBUG( "mite_MAR   = 0x%08X\n", ioread32( mite_MAR(   channel ) ) );
	PDEBUG( "mite_DCR   = 0x%08X\n", ioread32( mite_DCR(   channel ) ) );
	PDEBUG( "mite_DAR   = 0x%08X\n", ioread32( mite_DAR(   channel ) ) );
	PDEBUG( "mite_LKCR  = 0x%08X\n", ioread32( mite_LKCR(  channel ) ) );
	PDEBUG( "mite_LKAR  = 0x%08X\n", ioread32( mite_LKAR(  channel ) ) );
	PDEBUG( "mite_LLKAR = 0x%08X\n", ioread32( mite_LLKAR( channel ) ) );
	PDEBUG( "mite_BAR   = 0x%08X\n", ioread32( mite_BAR(   channel ) ) );
	PDEBUG( "mite_BCR   = 0x%08X\n", ioread32( mite_BCR(   channel ) ) );
	PDEBUG( "mite_SAR   = 0x%08X\n", ioread32( mite_SAR(   channel ) ) );
	PDEBUG( "mite_WSCR  = 0x%08X\n", ioread32( mite_WSCR(  channel ) ) );
	PDEBUG( "mite_WSER  = 0x%08X\n", ioread32( mite_WSER(  channel ) ) );
	PDEBUG( "mite_FCR   = 0x%08X\n", ioread32( mite_FCR(   channel ) ) );
}
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
