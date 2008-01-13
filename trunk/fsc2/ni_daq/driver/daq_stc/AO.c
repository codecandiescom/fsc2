/*
 *  $Id$
 * 
 *  Driver for National Instruments DAQ boards based on a DAQ-STC
 * 
 *  Copyright (C) 2003-2008 Jens Thoms Toerring
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


static int AO_channel_setup( Board *                  board,
			     unsigned                 int channel,
			     NI_DAQ_AO_CHANNEL_ARGS * args );

static int AO_direct_output( Board *      board,
			     unsigned int channel,
			     int          value );


#if defined NI_DAQ_DEBUG
#define stc_writew( a, b, c ) stc_writew( __FUNCTION__, a, b, c )
#define stc_writel( a, b, c ) stc_writel( __FUNCTION__, a, b, c )
#endif


/*----------------------------------------------------------*
 * Function resets the AO subsystem back into a known state
 *----------------------------------------------------------*/

void AO_reset_all( Board * board )
{
	u16 data;


	/* Make sure nobody can get in between */

	board->func->start_critical_section( board );

	/* Set flag indicating to the card that a configuration is done */

	data = board->stc.Joint_Reset | AO_Reset;
	board->func->stc_writew( board, STC_Joint_Reset, AO_Reset );
	board->stc.Joint_Reset &= ~ AO_Reset;

	/* Set flag to indicate we're doing a configuration */

	data = board->stc.Joint_Reset | AO_Configuration_Start;
	board->func->stc_writew( board, STC_Joint_Reset, data );
	board->stc.Joint_Reset &= ~ AO_Configuration_Start;

	/* Disarm the BC, UC and UI counters */

	data = board->stc.AO_Command_1 | AO_Disarm;
	board->func->stc_writew( board, STC_AO_Command_1, data );
	board->stc.AO_Command_1 &= ~ AO_Disarm;

	/* Reset the registers responsible for the AO subsystem */

	board->func->stc_writew( board, STC_AO_Personal, 0 );
	board->func->stc_writew( board, STC_AO_Command_1, 0 );
	board->func->stc_writew( board, STC_AO_Command_2, 0 );
	board->func->stc_writew( board, STC_AO_Mode_1, 0 );
	board->func->stc_writew( board, STC_AO_Mode_2, 0 );
	board->func->stc_writew( board, STC_AO_Output_Control, 0 );
	board->func->stc_writew( board, STC_AO_Mode_3, 0 );
	board->func->stc_writew( board, STC_AO_START_Select, 0 );
	board->func->stc_writew( board, STC_AO_Trigger_Select, 0 );

	/* Disable all interrupts */

	data = board->stc.Interrupt_B_Enable &
	       ~ ( AO_FIFO_Interrupt_Enable   |
		   AO_UC_TC_Interrupt_Enable  |
		   AO_Error_Interrupt_Enable  |
		   AO_STOP_Interrupt_Enable   |
		   AO_START_Interrupt_Enable  |
		   AO_UPDATE_Interrupt_Enable |
		   AO_START1_Interrupt_Enable |
		   AO_BC_TC_Interrupt_Enable    );

	board->func->stc_writew( board, STC_Interrupt_B_Enable, data );

	/* The following bit must always be set */

	board->func->stc_writew( board, STC_AO_Personal,
				board->stc.AO_Personal | AO_BC_Source_Select );

	/* Acknowledge (and thereby clear) all interrupt conditions */

	data = board->stc.Interrupt_B_Ack      |
	       AO_Error_Interrupt_Ack          |
	       AO_STOP_Interrupt_Ack           |
	       AO_START_Interrupt_Ack          |
	       AO_UPDATE_Interrupt_Ack         |
	       AO_START1_Interrupt_Ack         |
	       AO_BC_TC_Interrupt_Ack          |
	       AO_UC_TC_Interrupt_Ack          |
	       AO_BC_TC_Trigger_Error_Confirm  |
	       AO_BC_TC_Error_Confirm          |
	       AO_BC_TC_Trigger_Error_Confirm ;

	board->func->stc_writew( board, STC_Interrupt_B_Ack, data );

	board->stc.Interrupt_B_Ack &= ~ ( AO_Error_Interrupt_Ack         |
					  AO_STOP_Interrupt_Ack          |
					  AO_START_Interrupt_Ack         |
					  AO_UPDATE_Interrupt_Ack        |
					  AO_START1_Interrupt_Ack        |
					  AO_BC_TC_Interrupt_Ack         |
					  AO_UC_TC_Interrupt_Ack         |
					  AO_BC_TC_Trigger_Error_Confirm |
					  AO_BC_TC_Error_Confirm         |
					  AO_BC_TC_Trigger_Error_Confirm   );

	/* Configuration is done, set the bit indicating this to the card */

	data = board->stc.Joint_Reset | AO_Configuration_End;
	board->func->stc_writew( board, STC_Joint_Reset, data );
	board->stc.Joint_Reset &= ~ AO_Configuration_End;

	/* Switch off transfer off data to the DAQ and free buffers */

	board->func->data_shutdown( board, NI_DAQ_AO_SUBSYSTEM );

	board->AO.is_channel_setup[ 0 ] = board->AO.is_channel_setup[ 1 ] = 0;

	board->func->end_critical_section( board );
}


/*------------------------------------------------*
 * Handler for ioctl() calls for the AO subsystem
 *------------------------------------------------*/

int AO_ioctl_handler( Board *         board,
		      NI_DAQ_AO_ARG * arg )
{
	NI_DAQ_AO_ARG a;


	if ( board->type->ao_num_channels == 0 ) {
		PDEBUG( "Board has no AO channels\n ");
		return -EINVAL;
	}

	if ( copy_from_user( &a, ( const void __user * ) arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	switch ( a.cmd ) {
		case NI_DAQ_AO_CHANNEL_SETUP :
			return AO_channel_setup( board, a.channel,
						 a.channel_args );

		case NI_DAQ_AO_DIRECT_OUTPUT :
			return AO_direct_output( board, a.channel, a.value );
	}

	PDEBUG( "Invalid AO command %d\n", a.cmd );
	return -EINVAL;
}


/*-----------------------------------*
 * Function for setting up a channel
 *-----------------------------------*/

static int AO_channel_setup( Board *                  board,
			     unsigned                 int channel,
			     NI_DAQ_AO_CHANNEL_ARGS * channel_args )
{
	NI_DAQ_AO_CHANNEL_ARGS a;


	if ( channel >= board->type->ao_num_channels ) {
		PDEBUG( "Invalid AO channel\n ");
		return -EINVAL;
	}

	if ( copy_from_user( &a, ( const void __user * ) channel_args,
			     sizeof *channel_args ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	/* Some boards don't allow an external reference */

	if ( a.external_ref != NI_DAQ_DISABLED &&
	     ! board->type->ao_has_ext_ref )
		return -EINVAL;

	/* Some boards don't allow setting the ground reference bit */

	if ( a.ground_ref == NI_DAQ_DISABLED &&
	     ( ! strcmp( board->type->name, "pci-mio-16xe-50" ) ||
	       ! strcmp( board->type->name, "pci-mio-16xe-10" ) ||
	       ! strcmp( board->type->name, "pci-6031e" ) ) )
		return -EINVAL;

	/* Some boards can only do bipolar output */

	if ( a.polarity == NI_DAQ_UNIPOLAR &&
	     ! board->type->ao_unipolar )
		return -EINVAL;

	return board->func->ao_configuration( board, channel, a.ground_ref,
					      a.external_ref,
					      a.reglitch, a.polarity );
}


/*---------------------------------------------------------------*
 * Function for immediate direct output (i.e. without use of the
 * ADC FIFO etc.). The channel must have been set up before.
 *---------------------------------------------------------------*/

static int AO_direct_output( Board *      board,
			     unsigned int channel,
			     int          value )
{
	if ( channel >= board->type->ao_num_channels ) {
		PDEBUG( "Invalid AO channel\n ");
		return -EINVAL;
	}

	return board->func->dac_direct_data( board, channel, value );
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
