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


static void pci_irq_A_handler( Board * board,
			       u16     status );

static void pci_irq_B_handler( Board * board,
			       u16     status );


#if defined NI_DAQ_DEBUG
#define pci_stc_writew( a, b, c ) pci_stc_writew( __FUNCTION__, a, b, c	)
#define pci_stc_writel( a, b, c ) pci_stc_writel( __FUNCTION__, a, b, c	)
#endif


/*----------------------------------------------------------------*
 * Function called at initialization time to set up the structure
 * with board register addresses
 *----------------------------------------------------------------*/

void pci_board_register_setup( Board * board )
{
	/* Addresses of the registers of the board */

	/* Misc Register Group */

	board->regs->Serial_Command           = board->daq_stc + 0x0D; /* wo */
	board->regs->Misc_Command             = board->daq_stc + 0x0F; /* wo */
	board->regs->Status                   = board->daq_stc + 0x01; /* ro */

	/* Analog Input Register Group */

	board->regs->ADC_FIFO_Data_Register   =
				  ( u16 * ) ( board->daq_stc + 0x1C ); /* ro */
	board->regs->Configuration_Memory_Low =
		                  ( u16 * ) ( board->daq_stc + 0x10 ); /* wo */
	board->regs->Configuration_Memory_High =
		                  ( u16 * ) ( board->daq_stc + 0x12 ); /* wo */

	/* Analog Output Register Group */

	board->regs->AO_Configuration         =
		                  ( u16 * ) ( board->daq_stc + 0x16 ); /* wo */
	board->regs->DAC_FIFO_Data            =
		                  ( u16 * ) ( board->daq_stc + 0x1E ); /* wo */
	board->regs->DAC0_Direct_data         =
		                  ( u16 * ) ( board->daq_stc + 0x18 ); /* wo */
	board->regs->DAC1_Direct_data         =
		                  ( u16 * ) ( board->daq_stc + 0x1A ); /* wo */

	/* DMA Control Register Group */

	board->regs->AI_AO_Select             = board->daq_stc + 0x09; /* wo */
	board->regs->G0_G1_Select             = board->daq_stc + 0x0B; /* wo */

	/* DAQ-STC Register Group */

	board->regs->Window_Address           =
		                  ( u16 * ) ( board->daq_stc + 0x00 ); /* rw */
	board->regs->Window_Data              =
		                  ( u16 * ) ( board->daq_stc + 0x02 ); /* rw */
	board->regs->Interrupt_A_Acknowledge  =
		                  ( u16 * ) ( board->daq_stc + 0x04 ); /* wo */
	board->regs->Interrupt_B_Acknowledge  =
		                  ( u16 * ) ( board->daq_stc + 0x06 ); /* wo */
	board->regs->AI_Command_2             =
		                  ( u16 * ) ( board->daq_stc + 0x08 ); /* wo */
	board->regs->AO_Command_2             =
		                  ( u16 * ) ( board->daq_stc + 0x0A ); /* wo */
	board->regs->Gi_Command[ 0 ]          =
		                  ( u16 * ) ( board->daq_stc + 0x0C ); /* wo */
	board->regs->Gi_Command[ 1 ]          =
		                  ( u16 * ) ( board->daq_stc + 0x0E ); /* wo */
	board->regs->AI_Status_1              =
		                  ( u16 * ) ( board->daq_stc + 0x04 ); /* ro */
	board->regs->AO_Status_1              =
		                  ( u16 * ) ( board->daq_stc + 0x06 ); /* ro */
	board->regs->G_status                 =
		                  ( u16 * ) ( board->daq_stc + 0x08 ); /* ro */
	board->regs->AI_Status_2              =
		                  ( u16 * ) ( board->daq_stc + 0x0A ); /* ro */
	board->regs->AO_Status_2              =
		                  ( u16 * ) ( board->daq_stc + 0x0C ); /* ro */
	board->regs->DIO_Parallel_Input       =
		                  ( u16 * ) ( board->daq_stc + 0x0E ); /* ro */
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

void pci_board_irq_handling_setup( Board * board )
{
	int i;


	board->irq_group_A_enabled = board->irq_group_B_enabled = 0;

	for ( i = 0; i < 22; i++ ) {
		board->irq_hand[ i ].enabled = 0;
		board->irq_hand[ i ].handler = NULL;
		board->irq_hand[ i ].raised = 0;
	}

	/* Interrupt group A */ 

	board->irq_hand[ IRQ_AI_FIFO ].enable_bit_pattern =
						      AI_FIFO_Interrupt_Enable;
	board->irq_hand[ IRQ_AI_FIFO ].status_bit_pattern = AI_FIFO_Request_St;
	board->irq_hand[ IRQ_AI_FIFO ].ack_bit_pattern = 0;

	board->irq_hand[ IRQ_Pass_Thru_0 ].enable_bit_pattern =
						  Pass_Thru_0_Interrupt_Enable;
	board->irq_hand[ IRQ_Pass_Thru_0 ].status_bit_pattern =
						      Pass_Thru_0_Interrupt_St;
	board->irq_hand[ IRQ_Pass_Thru_0 ].ack_bit_pattern = 0;

	board->irq_hand[ IRQ_G0_TC ].enable_bit_pattern =
							G0_TC_Interrupt_Enable;
	board->irq_hand[ IRQ_G0_TC ].status_bit_pattern = G0_TC_St;
	board->irq_hand[ IRQ_G0_TC ].ack_bit_pattern = G0_TC_Interrupt_Ack;

	board->irq_hand[ IRQ_G0_Gate ].enable_bit_pattern =
						      G0_Gate_Interrupt_Enable;
	board->irq_hand[ IRQ_G0_Gate ].status_bit_pattern =
							  G0_Gate_Interrupt_St;
	board->irq_hand[ IRQ_G0_Gate ].ack_bit_pattern = G0_Gate_Interrupt_Ack;

	board->irq_hand[ IRQ_AI_SC_TC ].enable_bit_pattern =
						     AI_SC_TC_Interrupt_Enable;
	board->irq_hand[ IRQ_AI_SC_TC ].status_bit_pattern = AI_SC_TC_St;
	board->irq_hand[ IRQ_AI_SC_TC ].ack_bit_pattern =
							AI_SC_TC_Interrupt_Ack;

	board->irq_hand[ IRQ_AI_START1 ].enable_bit_pattern =
						    AI_START1_Interrupt_Enable;
	board->irq_hand[ IRQ_AI_START1 ].status_bit_pattern = AI_START1_St;
	board->irq_hand[ IRQ_AI_START1 ].ack_bit_pattern =
						       AI_START1_Interrupt_Ack;

	board->irq_hand[ IRQ_AI_START2 ].enable_bit_pattern =
						    AI_START2_Interrupt_Enable;
	board->irq_hand[ IRQ_AI_START2 ].status_bit_pattern = AI_START2_St;
	board->irq_hand[ IRQ_AI_START2 ].ack_bit_pattern =
						       AI_START2_Interrupt_Ack;

	board->irq_hand[ IRQ_AI_Error ].enable_bit_pattern =
						     AI_Error_Interrupt_Enable;
	board->irq_hand[ IRQ_AI_Error ].status_bit_pattern =
						AI_Overrun_St | AI_Overflow_St;
	board->irq_hand[ IRQ_AI_Error ].ack_bit_pattern =
							AI_Error_Interrupt_Ack;

	board->irq_hand[ IRQ_AI_STOP	].enable_bit_pattern =
						      AI_STOP_Interrupt_Enable;
	board->irq_hand[ IRQ_AI_STOP	].status_bit_pattern = AI_STOP_St;
	board->irq_hand[ IRQ_AI_STOP	].ack_bit_pattern =
							 AI_STOP_Interrupt_Ack;

	board->irq_hand[ IRQ_AI_START ].enable_bit_pattern =
						     AI_START_Interrupt_Enable;
	board->irq_hand[ IRQ_AI_START ].status_bit_pattern = AI_START_St;
	board->irq_hand[ IRQ_AI_START ].ack_bit_pattern =
							AI_START_Interrupt_Ack;

	/* Interrupt group B */ 

	board->irq_hand[ IRQ_AO_FIFO ].enable_bit_pattern =
						      AO_FIFO_Interrupt_Enable;
	board->irq_hand[ IRQ_AO_FIFO ].status_bit_pattern = AO_FIFO_Request_St;
	board->irq_hand[ IRQ_AO_FIFO ].ack_bit_pattern = 0;

	board->irq_hand[ IRQ_Pass_Thru_1 ].enable_bit_pattern =
						  Pass_Thru_1_Interrupt_Enable;
	board->irq_hand[ IRQ_Pass_Thru_1 ].status_bit_pattern =
						      Pass_Thru_1_Interrupt_St;
	board->irq_hand[ IRQ_Pass_Thru_1 ].ack_bit_pattern = 0;

	board->irq_hand[ IRQ_AO_UI2_TC ].enable_bit_pattern =
						    AO_UI2_TC_Interrupt_Enable;
	board->irq_hand[ IRQ_AO_UI2_TC ].status_bit_pattern = AO_UI2_TC_St;
	board->irq_hand[ IRQ_AO_UI2_TC ].ack_bit_pattern =
						       AO_UI2_TC_Interrupt_Ack;

	board->irq_hand[ IRQ_G1_TC ].enable_bit_pattern =
							G1_TC_Interrupt_Enable;
	board->irq_hand[ IRQ_G1_TC ].status_bit_pattern = G1_TC_St;
	board->irq_hand[ IRQ_G1_TC ].ack_bit_pattern = G1_TC_Interrupt_Ack;

	board->irq_hand[ IRQ_G1_Gate ].enable_bit_pattern =
						      G1_Gate_Interrupt_Enable;
	board->irq_hand[ IRQ_G1_Gate ].status_bit_pattern =
							  G1_Gate_Interrupt_St;
	board->irq_hand[ IRQ_G1_Gate ].ack_bit_pattern = G1_Gate_Interrupt_Ack;

	board->irq_hand[ IRQ_AO_BC_TC ].enable_bit_pattern =
						     AO_BC_TC_Interrupt_Enable;
	board->irq_hand[ IRQ_AO_BC_TC ].status_bit_pattern = AO_BC_TC_St;
	board->irq_hand[ IRQ_AO_BC_TC ].ack_bit_pattern =
							AO_BC_TC_Interrupt_Ack;

	board->irq_hand[ IRQ_AO_UC_TC ].enable_bit_pattern =
						    AO_UC_TC_Interrupt_Enable;
	board->irq_hand[ IRQ_AO_UC_TC ].status_bit_pattern = AO_UC_TC_St;
	board->irq_hand[ IRQ_AO_UC_TC ].ack_bit_pattern =
							AO_UC_TC_Interrupt_Ack;

	board->irq_hand[ IRQ_AO_START1 ].enable_bit_pattern =
						   AO_START1_Interrupt_Enable;
	board->irq_hand[ IRQ_AO_START1 ].status_bit_pattern = AO_START1_St;
	board->irq_hand[ IRQ_AO_START1 ].ack_bit_pattern =
						       AO_START1_Interrupt_Ack;

	board->irq_hand[ IRQ_AO_UPDATE ].enable_bit_pattern =
						    AO_UPDATE_Interrupt_Enable;
	board->irq_hand[ IRQ_AO_UPDATE ].status_bit_pattern = AO_UPDATE_St;
	board->irq_hand[ IRQ_AO_UPDATE ].ack_bit_pattern =
						       AO_UPDATE_Interrupt_Ack;

	board->irq_hand[ IRQ_AO_Error ].enable_bit_pattern =
						     AO_Error_Interrupt_Enable;
	board->irq_hand[ IRQ_AO_Error ].status_bit_pattern = AO_Overrun_St;
	board->irq_hand[ IRQ_AO_Error ].ack_bit_pattern =
							AO_Error_Interrupt_Ack;

	board->irq_hand[ IRQ_AO_STOP	].enable_bit_pattern =
						      AO_STOP_Interrupt_Enable;
	board->irq_hand[ IRQ_AO_STOP	].status_bit_pattern = AO_STOP_St;
	board->irq_hand[ IRQ_AO_STOP	].ack_bit_pattern =
							 AO_STOP_Interrupt_Ack;

	board->irq_hand[ IRQ_AO_START ].enable_bit_pattern =
						     AO_START_Interrupt_Enable;
	board->irq_hand[ IRQ_AO_START ].status_bit_pattern = AO_START_St;
	board->irq_hand[ IRQ_AO_START ].ack_bit_pattern =
							AO_START_Interrupt_Ack;
}


/*-------------------------------------------------------*
 * Function to bring the board into a known, quiet state
 *-------------------------------------------------------*/

void pci_board_reset_all( Board * board )
{
	/* Set up the DACs of the board with the values stored in the EEPROM */

	caldac_calibrate( board );

	/* Select PFI0/TRIG1 input as trigger source */

	writeb( 0x00, board->regs->Misc_Command );

	/* Reset all subsystems */

	MSC_reset_all( board );        /* should always come first ! */
	AI_reset_all( board );
	AO_reset_all( board );
	GPCT_reset_all( board );
	DIO_reset_all( board );

	/* Clear the configuration memory and both FIFOs */

	pci_clear_configuration_memory( board );
	pci_clear_ADC_FIFO( board );
	pci_clear_DAC_FIFO( board );
}


/*---------------------------------------------*
 * Function clears the configuration memory by
 * writing to the Write_Strobe_0 register.
 *---------------------------------------------*/

void pci_clear_configuration_memory( Board * board )
{
	pci_stc_writew( board, STC_Write_Strobe_0, Write_Strobe_0_Bit );
}


/*-------------------------------------------------------------------------*
 * Function clears the ADC FIFO by writing to the Write_Strobe_1 register.
 *-------------------------------------------------------------------------*/

void pci_clear_ADC_FIFO( Board * board )
{
	pci_stc_writew( board, STC_Write_Strobe_1, Write_Strobe_1_Bit );
}


/*-------------------------------------------------------------------------*
 * Function clears the DAC FIFO by writing to the Write_Strobe_1 register.
 *-------------------------------------------------------------------------*/

void pci_clear_DAC_FIFO( Board * board )
{
	pci_stc_writew( board, STC_Write_Strobe_2, Write_Strobe_2_Bit );
}


/*----------------------------------------------------------------*
 * Function for setting up the Configuration Memory High Register
 *----------------------------------------------------------------*/

int pci_configuration_high( Board *        board,
			    unsigned int   channel, 
			    NI_DAQ_AI_TYPE channel_type )
{
	u16 val;
	unsigned int bank = 0;

	if ( channel > board->type->ai_num_channels) {
		PDEBUG( "Invalid AI channel %u\n", channel );
		return -EINVAL;
	}

	bank = channel / 16;
	channel %= 16;

	if ( ( channel_type == NI_DAQ_AI_TYPE_Calibration &&
	       ( channel > 8 ||
		 ( ( ! strcmp( board->type->name, "pci-6032e" ) ||
		     ! strcmp( board->type->name, "pci-6032e" ) ) &&
		     channel > 7 ) ) ) ||
	     ( channel_type == NI_DAQ_AI_TYPE_Differential &&
	       channel > 7 ) ||
	     ( channel_type == NI_DAQ_AI_TYPE_Aux && channel != 4 ) ) {
		PDEBUG( "Invalid AI channel/type combination\n" );
		return -EINVAL;
	}

	val = ( channel_type << 12 ) | ( bank << 4 ) | channel;

	PDEBUG( "Configuration_Memory_High: 0x%04x\n", val );

	writew( val, board->regs->Configuration_Memory_High );

	return channel_type == NI_DAQ_AI_TYPE_Ghost ? 0 : 1;
}


/*---------------------------------------------------------------*
 * Function for setting up the Configuration Memory Low Register
 *---------------------------------------------------------------*/

int pci_configuration_low( Board *              board,
			   unsigned int         last_channel,
			   NI_DAQ_STATE         generate_trigger,
			   NI_DAQ_STATE         dither_enable,
			   NI_DAQ_BU_POLARITY   polarity,
			   NI_DAQ_AI_GAIN_TYPES gain )
{
	u16 val = 0;
	unsigned int i;


	if ( last_channel )
		val |= LastChan;

	if ( generate_trigger == NI_DAQ_ENABLED )
		val |= GenTrig;

	if ( dither_enable == NI_DAQ_ENABLED || board->type->ai_always_dither )
		val |= DitherEn;

	if ( polarity == NI_DAQ_UNIPOLAR )
		val |= Unip_Bip;

	for ( i = 0; i < 8; i++ ) {
		if ( board->type->ai_gains[ i ] == gain )
			break;
		if ( board->type->ai_gains[ i ] == NI_DAQ_GAIN_NOT_AVAIL ) {
			PDEBUG( "Board does not support requested gain\n ");
			return -EINVAL;
		}
	}

	if ( i >= 8 ) {
		PDEBUG( "Invalid gain setting\n " );
		return -EINVAL;
	}

	val |= gain;

	PDEBUG( "Configuration_Memory_Low: 0x%04x\n", val );

	writew( val, board->regs->Configuration_Memory_Low );

	return 0;
}


/*-------------------------------------------------------*
 * Function for setting up the AO Configuration Register
 *-------------------------------------------------------*/

int pci_ao_configuration( Board *            board,
			  unsigned int       channel,
			  NI_DAQ_STATE       ground_ref,
			  NI_DAQ_STATE       external_ref,
			  NI_DAQ_STATE       reglitch,
			  NI_DAQ_BU_POLARITY polarity )
{
	u16 val = 0;


	if ( channel )
		val |= DACSel;

	if ( ground_ref )
		val |= GroundRef;

	if ( external_ref )
		val |= ExtRef;

	if ( reglitch )
		val |= ReGlitch;

	if ( polarity == NI_DAQ_BIPOLAR || ! board->type->ao_unipolar ) {
		val |= BipDac;
		board->AO.polarity = NI_DAQ_BIPOLAR;
	}
	else
		board->AO.polarity = NI_DAQ_UNIPOLAR;

	PDEBUG( "AO_Configuration: %04x\n", val );

	writew( val, board->regs->AO_Configuration );

	board->AO.is_channel_setup[ channel ] = 1;

	return 0;
}

/*----------------------------------------------------------------------*
 * Function for writing directly to the DAC via the DAC0 or DAC1 Direct
 * Data Register. Depending on how the channel is set up the value is
 * taken to be either in straight binary form (for unipolar output) or
 * in two's complement form (for bipolar output).
 *----------------------------------------------------------------------*/

int pci_dac_direct_data( Board *      board,
			 unsigned int channel,
			 int          value )
{
	u16 *reg = channel ? board->regs->DAC1_Direct_data :
			     board->regs->DAC0_Direct_data;


	if ( ! board->AO.is_channel_setup[ channel ] ) {
		PDEBUG( "AO channel not configured\n" );
		return -EINVAL;
	}

	writew( ( u16 ) ( value & 0xFFFF ), reg );

	return 0;
}


/*---------------------------------------------*
 * Handler for interrupts raised by the boards
 *---------------------------------------------*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
irqreturn_t pci_board_irq_handler( int              irq,
				   void *           data,
				   struct pt_regs * dummy )
#else
void pci_board_irq_handler( int              irq,
			    void *           data,
			    struct pt_regs * dummy )
#endif
{
	u16 status;
	Board *board = ( Board * ) data;


	if ( board->irq_group_A_enabled &&
	     ( status = pci_stc_readw( board, STC_AI_Status_1 ) ) &
	     Interrupt_A_St )
		pci_irq_A_handler( board, status );

	if ( board->irq_group_B_enabled &&
	     ( status = pci_stc_readw( board, STC_AO_Status_1 ) ) &
	     Interrupt_B_St )
		pci_irq_B_handler( board, status );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
	return IRQ_HANDLED;
#endif
}


/*---------------------------------------------------------------*
 * Function for handling and acknowledging interrupts of group A
 * (i.e. from the AI subsystem and general purpose counter 0).
 *---------------------------------------------------------------*/

static void pci_irq_A_handler( Board * board,
			       u16     status )
{
	int i;


	for ( i = IRQ_AI_FIFO; i <= IRQ_AI_START; i++ ) {
		if ( ! ( board->irq_hand[ i ].enabled &&
			 status & board->irq_hand[ i ].status_bit_pattern ) )
			continue;

		/* Increment the variable that indicates that the interrupt
		   has been raised and execute the handler function associated
		   with the interrupt (if there's one) */

		if ( board->irq_hand[ i ].handler != NULL ) {
			board->irq_hand[ i ].raised++;
			board->irq_hand[ i ].handler( board );

			/* Acknowledge the interrupt (but note: AI_FIFO and
			   Pass_Thru_0 interrupts can't, the corresponding
			   bits in the status word get reset automatically
			   when proper actions have been taken) */

			if ( i >= IRQ_G0_TC )
				pci_stc_writew( board, STC_Interrupt_A_Ack,
					board->irq_hand[ i ].ack_bit_pattern );
		}
	}
}


/*---------------------------------------------------------------*
 * Function for handling and acknowledging interrupts of group B
 * (i.e. from the AO subsystem and general purpose counter 1).
 *---------------------------------------------------------------*/

static void pci_irq_B_handler( Board * board,
			       u16     status )
{
	int i;


	for ( i = IRQ_AO_FIFO; i <= IRQ_AO_START; i++ ) {
		if ( ! ( board->irq_hand[ i ].enabled &&
			 status & board->irq_hand[ i ].status_bit_pattern ) )
			continue;

		if ( board->irq_hand[ i ].handler != NULL ) {
			board->irq_hand[ i ].raised++;
			board->irq_hand[ i ].handler( board );

			/* Acknowledge the interrupt (but note: AO_FIFO and
			   Pass_Thru_1 interrupts can't, the corresponding
			   bits in the status word get reset automatically
			   when proper actions have been taken) */

			if ( i >= IRQ_AO_UI2_TC )
				pci_stc_writew( board, STC_Interrupt_B_Ack,
					board->irq_hand[ i ].ack_bit_pattern );
		}
	}
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
