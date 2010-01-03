/*
 *  Copyright (C) 2002-2010 Jens Thoms Toerring
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 *  To contact the author send email to:  jt@toerring.de
 */


#include "ni6601_drv.h"


/*--------------------------------------------------------*
 * Function releases all resources allocated to the board
 *--------------------------------------------------------*/

void ni6601_release_resources( Board * board )
{
	if ( board->buf ) {
		vfree( board->buf );
		board->buf = NULL;
	}

	if ( board->mite ) {
		iounmap( board->mite );
		board->mite = NULL;
	}

	if ( board->addr ) {
		iounmap( board->addr );
		board->addr = NULL;
	}

	if ( board->irq ) {
		free_irq( board->irq, board );
		board->irq = 0;
	}

	if ( board->has_region ) {
		pci_release_regions( board->dev );
		board->has_region = 0;
	}

	if ( board->is_enabled ) {
		pci_disable_device( board->dev );
		board->is_enabled = 0;
	}

	board->is_init = 0;
}


/*----------------------------------------------------------*
 * Function for setting up the structure with the addresses
 * of the boards registers
 *----------------------------------------------------------*/

void ni6601_register_setup( Board * board )
{
	int i;


	board->regs.irq_ack[ 0 ]        = board->addr + 0x004;
	board->regs.irq_ack[ 1 ]        = board->addr + 0x006;
	board->regs.irq_ack[ 2 ]        = board->addr + 0x104;
	board->regs.irq_ack[ 3 ]        = board->addr + 0x106;

	board->regs.status[ 0 ]         = board->addr + 0x004;
	board->regs.status[ 1 ]         = board->addr + 0x006;
	board->regs.status[ 2 ]         = board->addr + 0x104;
	board->regs.status[ 3 ]         = board->addr + 0x106;
	
	board->regs.joint_status[ 0 ]   = board->addr + 0x008;
	board->regs.joint_status[ 1 ]   = board->addr + 0x008;
	board->regs.joint_status[ 2 ]   = board->addr + 0x108;
	board->regs.joint_status[ 3 ]   = board->addr + 0x108;
	
	board->regs.command[ 0 ]        = board->addr + 0x00C;
	board->regs.command[ 1 ]        = board->addr + 0x00E;
	board->regs.command[ 2 ]        = board->addr + 0x10C;
	board->regs.command[ 3 ]        = board->addr + 0x10E;

	board->regs.hw_save[ 0 ]        = board->addr + 0x010;
	board->regs.hw_save[ 1 ]        = board->addr + 0x014;
	board->regs.hw_save[ 2 ]        = board->addr + 0x110;
	board->regs.hw_save[ 3 ]        = board->addr + 0x114;

	board->regs.sw_save[ 0 ]        = board->addr + 0x018;
	board->regs.sw_save[ 1 ]        = board->addr + 0x01C;
	board->regs.sw_save[ 2 ]        = board->addr + 0x118;
	board->regs.sw_save[ 3 ]        = board->addr + 0x11C;

	board->regs.mode[ 0 ]           = board->addr + 0x034;
	board->regs.mode[ 1 ]           = board->addr + 0x036;
	board->regs.mode[ 2 ]           = board->addr + 0x134;
	board->regs.mode[ 3 ]           = board->addr + 0x136;

	board->regs.joint_status_1[ 0 ] = board->addr + 0x036;
	board->regs.joint_status_1[ 1 ] = board->addr + 0x036;
	board->regs.joint_status_1[ 2 ] = board->addr + 0x136;
	board->regs.joint_status_1[ 3 ] = board->addr + 0x136;

	board->regs.load_a[ 0 ]         = board->addr + 0x038;
	board->regs.load_a[ 1 ]         = board->addr + 0x040;
	board->regs.load_a[ 2 ]         = board->addr + 0x138;
	board->regs.load_a[ 3 ]         = board->addr + 0x140;

	board->regs.joint_status_2[ 0 ] = board->addr + 0x03A;
	board->regs.joint_status_2[ 1 ] = board->addr + 0x03A;
	board->regs.joint_status_2[ 2 ] = board->addr + 0x13A;
	board->regs.joint_status_2[ 3 ] = board->addr + 0x13A;

	board->regs.load_b[ 0 ]         = board->addr + 0x03C;
	board->regs.load_b[ 1 ]         = board->addr + 0x044;
	board->regs.load_b[ 2 ]         = board->addr + 0x13C;
	board->regs.load_b[ 3 ]         = board->addr + 0x144;

	board->regs.input_select[ 0 ]   = board->addr + 0x048;
	board->regs.input_select[ 1 ]   = board->addr + 0x04A;
	board->regs.input_select[ 2 ]   = board->addr + 0x148;
	board->regs.input_select[ 3 ]   = board->addr + 0x14A;

	board->regs.autoincrement[ 0 ]  = board->addr + 0x088;
	board->regs.autoincrement[ 1 ]  = board->addr + 0x08A;
	board->regs.autoincrement[ 2 ]  = board->addr + 0x188;
	board->regs.autoincrement[ 3 ]  = board->addr + 0x18A;

	board->regs.joint_reset[ 0 ]    = board->addr + 0x090;
	board->regs.joint_reset[ 1 ]    = board->addr + 0x090;
	board->regs.joint_reset[ 2 ]    = board->addr + 0x190;
	board->regs.joint_reset[ 3 ]    = board->addr + 0x190;

	board->regs.irq_enable[ 0 ]     = board->addr + 0x092;
	board->regs.irq_enable[ 1 ]     = board->addr + 0x096;
	board->regs.irq_enable[ 2 ]     = board->addr + 0x192;
	board->regs.irq_enable[ 3 ]     = board->addr + 0x196;

	board->regs.counting_mode[ 0 ]  = board->addr + 0x0B0;
	board->regs.counting_mode[ 1 ]  = board->addr + 0x0B2;
	board->regs.counting_mode[ 2 ]  = board->addr + 0x1B0;
	board->regs.counting_mode[ 3 ]  = board->addr + 0x1B2;

	board->regs.second_gate[ 0 ]    = board->addr + 0x0B4;
	board->regs.second_gate[ 1 ]    = board->addr + 0x0B6;
	board->regs.second_gate[ 2 ]    = board->addr + 0x1B4;
	board->regs.second_gate[ 3 ]    = board->addr + 0x1B6;

	board->regs.dma_config[ 0 ]     = board->addr + 0x0B8;
	board->regs.dma_config[ 1 ]     = board->addr + 0x0BA;
	board->regs.dma_config[ 2 ]     = board->addr + 0x1B8;
	board->regs.dma_config[ 3 ]     = board->addr + 0x1BA;

	board->regs.dma_status[ 0 ]     = board->addr + 0x0B8;
	board->regs.dma_status[ 1 ]     = board->addr + 0x0BA;
	board->regs.dma_status[ 2 ]     = board->addr + 0x1B8;
	board->regs.dma_status[ 3 ]     = board->addr + 0x1BA;

	board->regs.dio_parallel_in     = board->addr + 0x00E;
	board->regs.dio_output          = board->addr + 0x014;
	board->regs.dio_control         = board->addr + 0x016;
	board->regs.dio_serial_input    = board->addr + 0x038;

	for ( i = 0; i < 10; i++ )
		board->regs.io_config[ i ] = board->addr + 0x77C + 4 * i;

	board->regs.reset_control       = board->addr + 0x700;
	board->regs.global_irq_control  = board->addr + 0x770;
	board->regs.global_irq_status   = board->addr + 0x754;
	board->regs.dma_configuration   = board->addr + 0x76C;
	board->regs.clock_config        = board->addr + 0x73C;
	board->regs.chip_signature      = board->addr + 0x700;

	/* Send a reset to get the board in a clean, well-defined state */

	iowrite32( SOFT_RESET, board->regs.reset_control );
}


/*-------------------------------------------------------*
 * Make the DIO I/O pins (PFI 0-7) controllable via the
 * STC DIO Control Register and switched to input
 *-------------------------------------------------------*/

void ni6601_dio_init( Board * board )
{
	int i;


	board->dio_mask = 0;
	iowrite16( board->dio_mask, board->regs.dio_control );

	/* Switch the DIO pins to output (so that they can be controlled
	   via the DIO Control Register), all other to input */

	iowrite32( ( u32 ) 0x01010101, board->regs.io_config[ 0 ] );
	iowrite32( ( u32 ) 0x01010101, board->regs.io_config[ 1 ] );

	for ( i = 2; i < 10; i++ )
		iowrite32( ( u32 ) 0x0, board->regs.io_config[ i ] );
}


/*------------------------------------------------------------------*
 * Enables a pin of a board for output, argument is the PFI number.
 * Take care not to change any other bit of the register.
 *------------------------------------------------------------------*/

void ni6601_enable_out( Board * board,
			int     pfi )
{
	u32 val, to_set;


	val = ioread32( board->regs.io_config[ pfi >> 2 ] );
	to_set = 1 << ( 8 * ( 3 - ( pfi & 0x3 ) ) );

	if ( ! ( val & to_set ) )
	     iowrite32( val | to_set, board->regs.io_config[ pfi >> 2 ] );
}


/*---------------------------------------------------------------------*
 * Disables a pin of a board for output (i.e. switches to input only),
 * argument is the PFI number. Take care not to change any other bits
 * of the register.
 *---------------------------------------------------------------------*/

void ni6601_disable_out( Board * board,
			 int     pfi )
{
	u32 val, to_clear;


	val = ioread32( board->regs.io_config[ pfi >> 2 ] );
	to_clear = 1 << ( 8 * ( 3 - ( pfi & 0x3 ) ) );

	if ( val & to_clear )
	     iowrite32( val ^ to_clear, board->regs.io_config[ pfi >> 2 ] );
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

int ni6601_input_gate( int   gate,
		       u16 * bits )
{
	switch ( gate ) {
		case NI6601_NONE :
			*bits = 0;
			return 0;

		case NI6601_LOW :
			*bits = 0x1F << 7;
			return 1;

		case NI6601_DEFAULT_SOURCE :
			*bits = 0;
			return 1;

		case NI6601_DEFAULT_GATE :
			*bits = 1 << 7;
			return 1;

		case NI6601_GATE_0 : case NI6601_GATE_1 :
		case NI6601_GATE_2 : case NI6601_GATE_3 :
			*bits = ( 2 + gate - NI6601_GATE_0 ) << 7;
			return 1;

		case NI6601_NEXT_SOURCE :
			*bits = 0x0A << 7;
			return 1;

		case NI6601_RTSI_0 : case NI6601_RTSI_1 :
		case NI6601_RTSI_2 : case NI6601_RTSI_3 :
			*bits = ( 0x0B + gate - NI6601_RTSI_0 ) << 7;
			return 1;

		case NI6601_NEXT_OUT :
			*bits = 0x14 << 7;
			return 1;
	}

	return -EINVAL;
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

int ni6601_input_source( int   source,
			 u16 * bits )
{
	switch ( source ) {
		case NI6601_LOW :
			*bits = 0x1F << 2;
			return 1;

		case NI6601_DEFAULT_SOURCE :
			*bits = 1 << 2;
			return 1;

		case NI6601_SOURCE_0 : case NI6601_SOURCE_1 :
		case NI6601_SOURCE_2 : case NI6601_SOURCE_3 :
			*bits = ( 2 + source - NI6601_SOURCE_0 ) << 2;
			return 1;

		case NI6601_NEXT_GATE :
			*bits = 0x0A << 2;
			return 1;

		case NI6601_RTSI_0 : case NI6601_RTSI_1 :
		case NI6601_RTSI_2 : case NI6601_RTSI_3 :
			*bits = ( 0x0B + source - NI6601_RTSI_0 ) << 2;
			return 1;

		case NI6601_NEXT_TC :
			*bits = 0x13 << 2;
			return 1;

		case NI6601_TIMEBASE_1 :
			*bits = 0;
			return 1;

		case NI6601_TIMEBASE_2 :
			*bits = 0x12 << 2;
			return 1;

		case NI6601_TIMEBASE_3 :
			*bits = 0x1E << 2;
			return 1;
	}

	return -EINVAL;
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
 * indent-tabs-mode: t
 * tab-width: 8
 * End:
 */
