/*
  $Id$
 
  Driver for National Instruments PCI E Series DAQ boards

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


#include "ni_daq_board.h"
#include "board.h"


typedef struct Subsystem_Data Subsystem_Data;

struct Subsystem_Data {
	int num_links;
	size_t max_size;
	size_t transfered;
};

static Subsystem_Data sys_data[ 4 ];

static void pci_dma_stop( Board *board, NI_DAQ_SUBSYSTEM sys );


/*-------------------------------------------------------------------*/
/* Function for initializing the MITE, bringing it in a well-defined */
/* quit state and the assigning the different MITE DMA channels to   */
/* different subsystems of the board.                                */
/*-------------------------------------------------------------------*/

void pci_mite_init( Board *board )
{
	int i;


	/* Reset DMA and FIFO for all channels, disable interrupts and
	   initialize the pointers to the DMA chain memory (one for each
	   logical DMA channel) */

	for ( i = 0; i < board->type->num_mite_channels; i++ ) {
		writel( CHOR_DMARESET | CHOR_FRESET, mite_CHOR( i ) );
		writel( 0, mite_CHCR( i ) );
		board->mite_chain[ i ] = NULL;
		sys_data[ i ].num_links = 0;
		sys_data[ i ].transfered = 0;
	}

	/* Logical DMA channel A is going to be used for the AI subsystem,
	   channel B for AO (if available), channel C for GPCT0 and
	   channel D for GPCT1. */

	if ( board->type->ao_num_channels > 0 )
		writeb( Output_B | Input_A, board->regs->AI_AO_Select );
	else
		writeb( Input_A, board->regs->AI_AO_Select );

	writeb( GPCT1_D | GPCT0_C, board->regs->G0_G1_Select );
}


/*------------------------------------------------------------*/
/* Function gets called when the module gets unloaded to stop */
/* all MITE activities and freeing DMA kernel buffers.        */
/*------------------------------------------------------------*/

void pci_mite_close( Board *board )
{
	int i;


	for ( i = 0; i < board->type->num_mite_channels; i++ )
		pci_dma_shutdown( board, i );

	writeb( 0, board->regs->AI_AO_Select );
	writeb( 0, board->regs->G0_G1_Select );
}


/*----------------------------------------------------------------------*/
/* Function allocates kernel buffers for DMA transfer to and from the   */
/* board. The MITE chip allows hardware scatter-gather, i.e. it accepts */
/* a linked list of buffers and switches between buffers if necessary.  */
/* The default size of the buffers can be set by the DMA_BUFFER_SIZE    */
/* macro, but this value is increased to a power of the PAGE size if    */
/* necessary. Setting this to too large a value may lead to allocation  */
/* failures because a continuous buffer may not be available, too small */
/* a value may require too many buffers.                                */
/*----------------------------------------------------------------------*/

#define DMA_BUFFER_SIZE  16384   /* 16 kByte */

int pci_dma_buf_setup( Board *board, NI_DAQ_SUBSYSTEM sys,
		       size_t num_points, int continuous )
{
	MITE_DMA_CHAIN *mdc;
	int num_links;
	unsigned long size;
	unsigned long buf_size = 1;
	unsigned long last_buf_size = 1;
	unsigned long order = 0;
	unsigned long last_order = 0;
	int i;


	/* If there's already memory allocated for the subsystem release it */

	pci_dma_buf_release( board, sys );

	/* Make sure the buffer size fits the page size */

	while ( buf_size < DMA_BUFFER_SIZE )
	{
		buf_size <<= 1;
		order++;
	}
	order = ( order - PAGE_SHIFT > 0 ) ? order - PAGE_SHIFT : 0;
	buf_size = 1UL << ( order + PAGE_SHIFT );

	/* Calculate how many links in the chain are needed and how much
	   memory (in powers of PAGE_SIZE) is needed for the last buffer */

	sys_data[ sys ].max_size = size = num_points * sizeof( u16 );

	num_links = size / buf_size;
	if ( size % buf_size > 0 )
	{
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

	board->mite_chain[ sys ] = kmalloc( ( num_links + 1 )
					    * sizeof *board->mite_chain[ sys ],
					    GFP_KERNEL );

	if ( board->mite_chain[ sys ] == NULL ) {
		PDEBUG( "Not enough memory for DMA chain links\n" );
		return -ENOMEM;
	}

	/* Allocate memory for the DMA buffer(s) and put their addresses into
	   the non-MITE-related part of the link chain structures */

	board->mite_chain[ sys ][ num_links ].buf = NULL;

	for ( mdc = board->mite_chain[ sys ], i = 0; i < num_links;
	      i++, mdc++ )
	{
		if ( size >= buf_size ) {
			mdc->buf = ( char * ) __get_free_pages( GFP_KERNEL,
								order );
			mdc->size = buf_size;
			mdc->order = order;
			size -= buf_size;
		} else {
			mdc->buf = ( char * ) __get_free_pages( GFP_KERNEL,
								last_order );
			board->mite_chain[ sys ][ i ].size = size;
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
		kfree( board->mite_chain[ sys ] );
		board->mite_chain[ sys ] = NULL;
		PDEBUG( "Not enough memory for DMA buffer\n" );
		return -ENOMEM;
	}

	/* Set up the structures needed by the MITE as link chains. For the
	   DMA memory buffers streaming mapping must be set up (except for
	   the AO buffers because data still need to be written to them before
	   they can be passed to the MITE). Take care, the link chain pointers
	   (LKAR) must be bus addresses. */

	for ( mdc = board->mite_chain[ sys ]; mdc->buf != NULL; mdc++ ) {

		mdc->lc.tcr = cpu_to_le32( mdc->size );

		if ( sys != NI_DAQ_AO_SUBSYSTEM )
		{
			mdc->lc.mar =
				cpu_to_le32( pci_map_single( board->dev,
							mdc->buf, mdc->size,
							PCI_DMA_FROMDEVICE ) );
			if ( mdc->lc.mar == 0 )
				break;

			mdc->is_mapped = 1;
		}

		mdc->lc.dar = 0;
		mdc->lc.lkar = cpu_to_le32( virt_to_bus( &( mdc + 1 )->lc ) );
	}

	/* If the streaming mapping failed all mapped buffers must be unmapped
	   and then all memory realeased */

	if ( mdc != board->mite_chain[ sys ] + num_links )
	{
		for ( mdc = board->mite_chain[ sys ]; mdc->buf != NULL;
		      mdc++ ) {
			if ( mdc->is_mapped )
				pci_unmap_single( board->dev,
						  le32_to_cpu( mdc->lc.mar ),
						  mdc->size,
						  PCI_DMA_FROMDEVICE );
			free_pages( ( unsigned long ) mdc->buf, mdc->order );
		}

		kfree( board->mite_chain[ sys ] );
		board->mite_chain[ sys ] = NULL;
		PDEBUG( "Failed to set up DMA stream mapping\n" );
		return -ENOMEM;
	}

	/* The terminator element of the MITE link chains must have all fields
	   set to 0 */

	mdc->lc.tcr = mdc->lc.mar = mdc->lc.dar = mdc->lc.lkar = 0;

	/* If continuous input or output is to be done the LKAR pointer in
	   the last link must point back to the first element */

	if ( continuous )
		board->mite_chain[ num_links - 1 ][ i ].lc.lkar =
			cpu_to_le32( virt_to_bus(
					 &board->mite_chain[ sys ][ 0 ].lc ) );

	return 0;
}


/*-------------------------------------------------------------*/
/* Gets newly acquired data from a subsystem. The function not */
/* necessarily returns as many bytes as requested by what the  */
/* 'size' argument points to but only as many as are currently */
/* available, writing them to the 'dest' buffer. If it's clear */
/* that all data from the acquisition have already been read   */
/* DMA is disabled and the DMA kernel buffers are released.    */
/* When the function returns teh 'size' points to the number   */
/* of transfered bytes. A negative return value indicates an   */
/* error, a return value of 0 means that everything went well  */
/* and there are still further data to be expected and a value */
/* of 1 also means success but that all data to be expected    */
/* now have been fetched and the DMA system has been shut down */
/* (and a further call would fail unless a new acquisition has */
/* beed started in the mean time).                             */
/* Please note: The data returned may belong to different AI   */
/*              channels. This depends on the channel setup:   */
/*              a scan returns a value for each channels in    */
/*              the channel setup (unless the type is GHOST).  */
/*              Furthermore, the data are "raw" data, i.e.     */
/*              2-byte, little-endian numbers.                 */
/*-------------------------------------------------------------*/

int pci_dma_buf_get( Board *board, NI_DAQ_SUBSYSTEM sys, void *dest,
		     size_t *size, int still_used )
{
	size_t left;
	size_t transf = 0;
	size_t total = 0;
	MITE_DMA_CHAIN *mdc;
	size_t avail;


	/* If no DMA buffers are allocated calling the function is a bad
	   mistake */

	if ( board->mite_chain[ sys ] == NULL ) {
		PDEBUG( "Can't read DMA buffers, none allocated\n" );
		return -EINVAL;
	}

	/* Figure out how many new data are available and restrict the
	   number of bytes to be returned to this value */

	if ( ( avail = pci_dma_get_available( board, sys ) ) == 0 ) {
		*size = 0;
		return 0;
	}

	if ( avail < *size )
		*size = avail;

	/* Copy as many (of the newly acquired) bytes as possible or requested
	   to the user buffer */

	for ( mdc = board->mite_chain[ sys ]; mdc->buf != NULL && *size > 0;
	      mdc++ ) {

		/* Skip the current link if it has already been used up */

		if ( ( left = mdc->size - mdc->transfered ) == 0 )
			continue;

		/* Calculate how many bytes there are left in the buffer */

		transf = left >= *size ? *size : left;

		/* Unmap the buffer (or sync it if it still might be needed) */

		if ( mdc->is_mapped ) {
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
		}

		/* Copy it's (yet unused) content to the user buffer */

		if ( copy_to_user( dest + total, mdc->buf + mdc->transfered,
				   transf ) ) {
			pci_dma_shutdown( board, sys );
			PDEBUG( "Can't write to user space\n" );
			return -EACCES;
		}

		mdc->transfered += transf;
		*size -= transf;
		total += transf;
	}

	sys_data[ sys ].transfered += total;
	*size = total;

	/* If all data points that can be expected have been fetched disable
	   DMA and release the buffers and return a value indicating this */

	if ( sys_data[ sys ].max_size <= sys_data[ sys ].transfered )
	{
		pci_dma_shutdown( board, sys );
		return 1;
	}

	return 0;
}


/*-----------------------------------------------------------------------*/
/* Function returns the number of bytes that have been written to memory */
/* or read by the board since the function was called the last time.     */
/*-----------------------------------------------------------------------*/

size_t pci_dma_get_available( Board *board, NI_DAQ_SUBSYSTEM sys )
{
	u32 avail;

	avail = le32_to_cpu( readl( mite_DAR( sys ) )
			     - ( readl( mite_FCR( sys ) ) & 0x000000FF ) )
	        - sys_data[ sys ].transfered;

	/* Only tell about AI data for completed scans */

	if ( sys == NI_DAQ_AI_SUBSYSTEM )
		avail = ( avail / ( 2 * board->AI.num_data_per_scan) )
			* 2 * board->AI.num_data_per_scan;

	return avail;
}


/*------------------------------------------------------*/
/* Function for unmapping and releasing all DMA buffers */
/* allocated for a subsystem of the board               */
/*------------------------------------------------------*/

void pci_dma_buf_release( Board *board, NI_DAQ_SUBSYSTEM sys )
{
	MITE_DMA_CHAIN *mdc;


	if ( board->mite_chain[ sys ] == NULL )
		return;

	for ( mdc = board->mite_chain[ sys ]; mdc->buf != NULL; mdc++ ) {
		if ( mdc->is_mapped )
			pci_unmap_single( board->dev,
					  le32_to_cpu( mdc->lc.mar ),
					  mdc->size,
					  sys == NI_DAQ_AI_SUBSYSTEM ?
					  PCI_DMA_FROMDEVICE :
					  PCI_DMA_TODEVICE );

		free_pages( ( unsigned long ) mdc->buf, mdc->order );
	}

	kfree( board->mite_chain[ sys ] );
	board->mite_chain[ sys ] = NULL;

	sys_data[ sys ].num_links = 0;
	sys_data[ sys ].transfered = 0;
}


/*--------------------------------------------------------------------*/
/* Function enables DMA for a subsystem. Requires that DMA memory has */
/* already been allocated.                                            */
/*--------------------------------------------------------------------*/

int pci_dma_setup( Board *board, NI_DAQ_SUBSYSTEM sys )
{
	MITE_DMA_CHAIN *mdc;
	u32 chcr;
	u32 dcr;
	u32 lkar;


	if ( board->mite_chain[ sys ] == NULL ) {
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

	writel( cpu_to_le32( chcr ), mite_CHCR( sys ) );

	board->mite_irq_enabled[ sys ] = 1;

	/* 16 bits to memory */

	writel( cpu_to_le32( CR_RL64 | CR_ASEQxP1 | CR_PSIZEHALF ),
		mite_MCR( sys ) );

	/* 16 bits from device */

	dcr = CR_RL64 |  CR_ASEQx( 1 ) | CR_PSIZEHALF |
	      CR_PORTIO | CR_AMDEVICE | CR_REQS( sys );
	writel( cpu_to_le32( dcr ), mite_DCR( sys ) );
    
	/* Reset the DAR */

	writel( 0, mite_DAR( sys ) );
    
	/* The link is 32 bits wide */

	writel( cpu_to_le32( CR_RL64 | CR_ASEQUP | CR_PSIZEWORD ),
		mite_LKCR( sys ) );

	/* Before finally passing the pointer to the link chain to the MITE
	   make sure all buffers are streaming mapped */

	for ( mdc = board->mite_chain[ sys ]; mdc->buf != NULL; mdc++ ) {

		if ( mdc->is_mapped )
			continue;

		mdc->lc.mar = cpu_to_le32( pci_map_single(
				       	       board->dev, mdc->buf, mdc->size,
					       sys == NI_DAQ_AO_SUBSYSTEM ? 
					       PCI_DMA_TODEVICE :
					       PCI_DMA_FROMDEVICE ) );
		if ( mdc->lc.mar == 0 ) {
			PDEBUG( "Failed to set up DMA stream mapping\n" );
			pci_dma_shutdown( board, sys );
			return -ENOMEM;
		}

		mdc->is_mapped = 1;
	}

	/* Set start address for link chaining */

	lkar = virt_to_bus( &board->mite_chain[ sys ][ 0 ].lc );
	writel( cpu_to_le32( lkar ), mite_LKAR( sys ) );

	/* Arm the MITE for DMA transfers */

	writel( CHOR_START, mite_CHOR( sys ) );

	return 0;
}


/*-----------------------------------------------------*/
/* Disables DMA ad releases DMA momory for a subsystem */
/*-----------------------------------------------------*/

int pci_dma_shutdown( Board *board, NI_DAQ_SUBSYSTEM sys )
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


/*-------------------------*/
/* Stops DMA for subsystem */
/*-------------------------*/

static void pci_dma_stop( Board *board, NI_DAQ_SUBSYSTEM sys )
{
	if ( board->mite_irq_enabled[ sys ] == 0 )
		return;

	/* Stop the MITE */

	writel( CHOR_DMARESET | CHOR_CLRDONE | CHOR_FRESET | CHOR_ABORT,
		mite_CHOR( sys ) );
	board->mite_irq_enabled[ sys ] = 0;
}



/*--------------------------------------------------------------------*/
/* Function for printing out all accessible registers of a channel of */
/* the MITE - only available when compiling with debuuging support.   */
/*--------------------------------------------------------------------*/

#if defined NI_DAQ_DEBUG
void pci_mite_dump( Board *board, int channel )
{
	PDEBUG( "mite_CHOR  = 0x%08X\n", readl( mite_CHOR(  channel ) ) );
	PDEBUG( "mite_CHCR  = 0x%08X\n", readl( mite_CHCR(  channel ) ) );
	PDEBUG( "mite_TCR   = 0x%08X\n", readl( mite_TCR(   channel ) ) );
	PDEBUG( "mite_CHSR  = 0x%08X\n", readl( mite_CHSR(  channel ) ) );
	PDEBUG( "mite_MCR   = 0x%08X\n", readl( mite_MCR(   channel ) ) );
	PDEBUG( "mite_MAR   = 0x%08X\n", readl( mite_MAR(   channel ) ) );
	PDEBUG( "mite_DCR   = 0x%08X\n", readl( mite_DCR(   channel ) ) );
	PDEBUG( "mite_DAR   = 0x%08X\n", readl( mite_DAR(   channel ) ) );
	PDEBUG( "mite_LKCR  = 0x%08X\n", readl( mite_LKCR(  channel ) ) );
	PDEBUG( "mite_LKAR  = 0x%08X\n", readl( mite_LKAR(  channel ) ) );
	PDEBUG( "mite_LLKAR = 0x%08X\n", readl( mite_LLKAR( channel ) ) );
	PDEBUG( "mite_BAR   = 0x%08X\n", readl( mite_BAR(   channel ) ) );
	PDEBUG( "mite_BCR   = 0x%08X\n", readl( mite_BCR(   channel ) ) );
	PDEBUG( "mite_SAR   = 0x%08X\n", readl( mite_SAR(   channel ) ) );
	PDEBUG( "mite_WSCR  = 0x%08X\n", readl( mite_WSCR(  channel ) ) );
	PDEBUG( "mite_WSER  = 0x%08X\n", readl( mite_WSER(  channel ) ) );
	PDEBUG( "mite_FCR   = 0x%08X\n", readl( mite_FCR(   channel ) ) );
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
