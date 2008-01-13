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

static int MSC_set_speed( Board *                  board,
			  unsigned int             divider,
			  NI_DAQ_CLOCK_SPEED_VALUE speed );

static int MSC_clock_output( Board *           board,
			     NI_DAQ_CLOCK_TYPE clock,
			     NI_DAQ_STATE      output_state );

static int  MSC_trigger_setup( Board *          board,
			       NI_DAQ_TRIG_TYPE trigger_type,
			       int              trigger_high,
			       int              trigger_low );

static int MSC_board_properties( Board *                   board,
				 NI_DAQ_BOARD_PROPERTIES * arg );


#if defined NI_DAQ_DEBUG
#define stc_writew( a, b, c ) stc_writew( __FUNCTION__, a, b, c )
#define stc_writel( a, b, c ) stc_writel( __FUNCTION__, a, b, c )
#endif


typedef struct {
	NI_DAQ_SUBSYSTEM sub_system;
	NI_DAQ_PFI_STATE state;
} PFI_States;

static PFI_States PFI_state[ 10 ];



/*-----------------------------------------------------------*
 * Function resets the MSC subsystem back into a known state
 *-----------------------------------------------------------*/

void MSC_reset_all( Board * board )
{
	int i;


	/* Disable all interrupts and clear all outstanding conditions 
	   (the interrupt polarity *must* be active high) */

	board->func->stc_writew( board, STC_Interrupt_Control,
				 Interrupt_Output_Polarity );
	board->func->stc_writew( board, STC_Interrupt_A_Enable, 0 );
	board->func->stc_writew( board, STC_Interrupt_B_Enable, 0 );
	board->func->stc_writew( board, STC_Second_Irq_A_Enable, 0 );
	board->func->stc_writew( board, STC_Second_Irq_B_Enable, 0 );
	board->func->stc_writew( board, STC_Interrupt_A_Ack, 0xFFE0 );
	board->func->stc_writew( board, STC_Interrupt_B_Ack, 0xFFFE );

	/* Switch off the frequency output before doing changes */

	board->stc.Clock_and_FOUT &= ~ FOUT_Enable;
	board->func->stc_writew( board, STC_Clock_and_FOUT,
				 board->stc.Clock_and_FOUT );

	/* Enable the slow internal time base */

	board->stc.Clock_and_FOUT |= Slow_Internal_Timebase;

	/* Set the speed for both the fast and slow clock to the highest
	   possible speed (20 MHz and 200 kHz). Take care: the IN_TIMEBASE1
	   input clock of the subsystems is always 20 MHz (if 10 MHz are
	   needed it must set by a subsystem-specific flag) and not influenced
	   by the divider for the fast clock (that only applies to the
	   frequency at the FREQ_OUT pin of the card), but IN_TIMEBASE2
	   depends on what the Slow_Internal_Time_Divide_By_2 bit has been
	   set to. */

	board->stc.Clock_and_FOUT &= ~ FOUT_Timebase_Select;
	board->stc.Clock_and_FOUT &= ~ FOUT_Divider_Field;
	board->stc.Clock_and_FOUT &= ~ Slow_Internal_Time_Divide_By_2;
	board->stc.Clock_and_FOUT |= 1;

	board->MSC.clock   = NI_DAQ_FAST_CLOCK;
	board->MSC.divider = 1;
	board->MSC.speed   = NI_DAQ_FULL_SPEED;

	board->timebase2   = 5000;

	board->func->stc_writew( board, STC_Clock_and_FOUT,
				 board->stc.Clock_and_FOUT );

	board->stc.Clock_and_FOUT |= FOUT_Enable;
	board->func->stc_writew( board, STC_Clock_and_FOUT,
				 board->stc.Clock_and_FOUT );

	/* Disable the analog trigger circuitry and make the PFI0/TRIG1
	   input a digital trigger */

	board->stc.Analog_Trigger_Etc &= ~ Analog_Trigger_Enable;
	board->stc.Analog_Trigger_Etc |= Analog_Trigger_Drive;

	board->func->stc_writew( board, STC_Analog_Trigger_Etc,
				 board->stc.Analog_Trigger_Etc );

	/* Switch all PFI pins to input (which is supposed to be less
	   dangerous according to the manual) and mark them as unused */

	for ( i = 0; i < 10; i++ ) {
		PFI_state[ i ].sub_system = NI_DAQ_NO_SUBSYSTEM;
		PFI_state[ i ].state      = NI_DAQ_PFI_UNUSED;
	}

	board->func->stc_writew( board, STC_IO_Bidirection_Pin,
				 board->stc.IO_Bidirection_Pin & ~ 0x03FF );
}


/*------------------------------------------------------------*
 * Function for enabling one of the interrupts of the DAQ-STC
 *------------------------------------------------------------*/

void daq_irq_enable( Board *  board,
		     int      irq,
		     void ( * handler )( Board * ) )
{
	int i;


	if ( irq < 0 || irq > IRQ_AO_START ) {
		PDEBUG( "Invalid IRQ number %d\n", irq );
		return;
	}

	/* Make sure the raised flag for the interrupt isn't set */

	board->irq_hand[ irq ].raised = 0;

	/* If a handler for the interrupt is set this also means that the
	   interrupt is already enabled and all that needs to be done is
	   to replace the old handler function by the new one. */

	if ( board->irq_hand[ irq ].handler ) {
		board->irq_hand[ irq ].handler = handler;
		return;
	}
		
	board->irq_hand[ irq ].handler = handler;
	board->irq_hand[ irq ].enabled = 1;

	/* After having set up the handler for the interrupt it now also
	   needs to be enabled. There are two groups of interrupts, A and
	   B, and for each group there's a "master" bit that also needs to
	   be set to enable interrupts from the group. */

	if ( irq <= IRQ_AI_START ) {      /* Interrupt group A */

		/* Acknowledge an old interrupt if there was one to avoid
		   confusion in functions that check the status register */

		board->func->stc_writew( board, STC_Interrupt_A_Ack,
				      board->irq_hand[ irq ].ack_bit_pattern );

		/* Enable the the interrupt */

		board->func->stc_writew( board, STC_Interrupt_A_Enable,
				   board->stc.Interrupt_A_Enable |
				   board->irq_hand[ irq ].enable_bit_pattern );

		/* Check if interrupts for group A are already enabled and
		   if they aren't enable them by setting the "master" bit */

		for ( i = 0; i <= IRQ_AI_START; i++ )
			if ( i != irq && board->irq_hand[ i ].handler )
				break;

		if ( i > IRQ_AI_START ) {
			board->irq_group_A_enabled = 1;
			board->func->stc_writew( board, STC_Interrupt_Control,
						 board->stc.Interrupt_Control |
						 Interrupt_A_Enable_Bit );
		}

	} else {                          /* Interrupt group B */

		board->func->stc_writew( board, STC_Interrupt_B_Ack,
				      board->irq_hand[ irq ].ack_bit_pattern );

		board->func->stc_writew( board, STC_Interrupt_B_Enable,
				   board->stc.Interrupt_B_Enable |
				   board->irq_hand[ irq ].enable_bit_pattern );

		for ( i = IRQ_AO_FIFO; i <= IRQ_AO_START; i++ )
			if ( i != irq && board->irq_hand[ i ].handler )
				break;

		if ( i > IRQ_AO_START ) {
			board->irq_group_B_enabled = 1;
			board->func->stc_writew( board, STC_Interrupt_Control,
						 board->stc.Interrupt_Control |
						 Interrupt_B_Enable_Bit );
		}
	}
}

/*-------------------------------------------------------------*
 * Function for disabling one of the interrupts of the DAQ-STC
 *-------------------------------------------------------------*/

void daq_irq_disable( Board * board,
		      int     irq )
{
	int i;


	if ( irq < 0 || irq > IRQ_AO_START ) {
		PDEBUG( "Invalid IRQ number %d\n", irq );
		return;
	}

	/* Like in the function for enabling we have to distinguish between
	   the interrupt groups A and B */

	if ( irq <= IRQ_AI_START ) {      /* Interrupt group A */

		/* Check if there's another interrupt enabled for the group,
		   if there is the "master" enable bit for the group must
		   not be cleared */

		for ( i = 0; i <= IRQ_AI_START; i++ )
			if ( i != irq && board->irq_hand[ irq ].handler )
				break;

		if ( i > IRQ_AI_START ) {
			board->irq_group_A_enabled = 0;
			board->func->stc_writew( board, STC_Interrupt_Control,
						 board->stc.Interrupt_Control &
						 ~ Interrupt_A_Enable_Bit );
		}

		/* Disable the interrupt and acknowledge an old interrupt
		   that might have been forgotten */

		board->func->stc_writew( board, STC_Interrupt_A_Ack,
				      board->irq_hand[ irq ].ack_bit_pattern );

		board->func->stc_writew( board, STC_Interrupt_A_Enable,
				 board->stc.Interrupt_A_Enable &
				 ~ board->irq_hand[ irq ].enable_bit_pattern );

	} else {                          /* Interrupt group B */

		for ( i = IRQ_AO_FIFO; i <= IRQ_AO_START; i++ )
			if ( i != irq && board->irq_hand[ irq ].handler )
				break;

		if ( i > IRQ_AO_START ) {
			board->irq_group_B_enabled = 0;
			board->func->stc_writew( board, STC_Interrupt_Control,
						 board->stc.Interrupt_Control &
						 ~ Interrupt_B_Enable_Bit );
		}

		board->func->stc_writew( board, STC_Interrupt_B_Ack,
				      board->irq_hand[ irq ].ack_bit_pattern );

		board->func->stc_writew( board, STC_Interrupt_B_Enable,
				 board->stc.Interrupt_B_Enable &
				 ~ board->irq_hand[ irq ].enable_bit_pattern );
	}

	/* Finally remove the handler for the interrupt and reset the
	   raised and enabled flag */

	board->irq_hand[ irq ].enabled = 0;
	board->irq_hand[ irq ].raised = 0;
	board->irq_hand[ irq ].handler = NULL;
}


/*-----------------------------------------------------------------*
 * Function for switching the PFIi lines to output or input. Each
 * subsystem (i.e. AI, AO, GPCT0 and GPCT1) is supposed to call
 * this function and will be allowed setting a direction of a PFI
 * line only if hasn't been already claimed by a different sub-
 * system. Thus each subsystem also has to "release" the PFI lines
 * it "allocated" when it's not using them anymore. This is done
 * by calling the function with the state argument being
 * "NI_DAQ_UNUSED". A subsystem can also "release" all of its PFI
 * lines by calling the function with the channel argument being
 * set to "NI_DAQ_ALL".
 * The function returns 0 on success and 1 on errors.
 *-----------------------------------------------------------------*/

int MSC_PFI_setup( Board *          board,
		   NI_DAQ_SUBSYSTEM sub_system,
		   NI_DAQ_INPUT     channel,
		   NI_DAQ_PFI_STATE state )
{
	int ch;
	int i;


	if ( channel == NI_DAQ_ALL ) {
		if ( state != NI_DAQ_PFI_UNUSED ) {
			PDEBUG( "All PFI channels can only be freed all at "
				"once\n" );
			return 1;
		}

		for ( i = 0; i < 10; i++ ) {
			if ( PFI_state[ i ].sub_system != sub_system )
				continue;
			board->stc.IO_Bidirection_Pin &= ~ ( 1 << i );
			PFI_state[ i ].sub_system = NI_DAQ_NO_SUBSYSTEM;
			PFI_state[ i ].state = NI_DAQ_PFI_UNUSED;
		}

		board->func->stc_writew( board, STC_IO_Bidirection_Pin,
					 board->stc.IO_Bidirection_Pin );
		return 0;
	}

	/* Nothing to be done for non-PFI channel arguments */

	if ( channel < NI_DAQ_PFI0 || channel > NI_DAQ_PFI9 )
		return 0;

	ch = channel - NI_DAQ_PFI0;

	if ( PFI_state[ ch ].sub_system != NI_DAQ_NO_SUBSYSTEM &&
	     PFI_state[ ch ].sub_system != sub_system ) {
		PDEBUG( "PFI%d already used by other subsystem\n", ch );
		return 1;
	}

	switch ( state ) {
		case NI_DAQ_PFI_UNUSED :
			PFI_state[ ch ].sub_system = NI_DAQ_NO_SUBSYSTEM;
			board->stc.IO_Bidirection_Pin &= ~ ( 1 << ch );
			break;

		case NI_DAQ_PFI_INPUT :
			PFI_state[ ch ].sub_system = sub_system;
			board->stc.IO_Bidirection_Pin &= ~ ( 1 << ch );
			break;

		case NI_DAQ_PFI_OUTPUT :
			PFI_state[ ch ].sub_system = sub_system;
			board->stc.IO_Bidirection_Pin |= 1 << ch;
			break;

		default :
			PDEBUG( "Something strange is going on\n" );
			return 1;
	}

	PFI_state[ ch ].state = state;

	board->func->stc_writew( board, STC_IO_Bidirection_Pin,
				 board->stc.IO_Bidirection_Pin );

	return 0;
}


/*-----------------------------------------------------------*
 * Function for handling ioctl() calls for the MSC subsystem
 *-----------------------------------------------------------*/

int MSC_ioctl_handler( Board *          board,
		       NI_DAQ_MSC_ARG * arg )
{
	NI_DAQ_MSC_ARG a;


	if ( copy_from_user( &a, ( const void __user * ) arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	switch ( a.cmd ) {
		case NI_DAQ_MSC_SET_CLOCK_SPEED :
			return MSC_set_speed( board, a.divider, a.speed );

		case NI_DAQ_MSC_GET_CLOCK :
			a.clock = board->MSC.clock;
			a.output_state = board->MSC.output_state;
			a.speed = board->MSC.speed;
			a.divider = board->MSC.divider;
			break;

		case NI_DAQ_MSC_CLOCK_OUTPUT :
			return MSC_clock_output( board, a.clock,
						 a.output_state );

		case NI_DAQ_MSC_TRIGGER_STATE :
			return MSC_trigger_setup( board, a.trigger_type,
						  a.trigger_high,
						  a.trigger_low );

		case NI_DAQ_MSC_BOARD_PROPERTIES :
			return MSC_board_properties( board, a.properties );

		default :
			PDEBUG( "Invalid MSC command %d\n", a.cmd );
			return -EINVAL;
	}

	if ( copy_to_user( ( void __user * ) arg, &a, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EFAULT;
	}

	return 0;
}


/*------------------------------------------------------*
 * Sets the clock speeds (both the fast and slow clock)
 *------------------------------------------------------*/

static int MSC_set_speed( Board *                  board,
			  unsigned int             divider,
			  NI_DAQ_CLOCK_SPEED_VALUE speed )
{
	u16 caf = board->stc.Clock_and_FOUT;
	int output_state = caf & FOUT_Enable;


	if ( divider < 1 || divider > 16 ) {
		PDEBUG( "Invalid clock divider %u\n", divider );
		return -EINVAL;
	}

	/* Switch off clock output while doing changes */

	if ( output_state ) {
		caf &= ~ FOUT_Enable;
		board->func->stc_writew( board, STC_Clock_and_FOUT, caf );
	}

	/* Switch clock rate division by 2 on or off */

	if ( speed == NI_DAQ_HALF_SPEED ) {
		caf |= Slow_Internal_Time_Divide_By_2;
		board->timebase2 = 10000;
	} else {
		caf &= ~ Slow_Internal_Time_Divide_By_2;
		board->timebase2 = 5000;
	}

	board->MSC.speed = speed;

	/* Set additional divider for the clock rate (for a division factor
	   of 16 all bits must be cleared) */

	caf &= ~ FOUT_Divider_Field;
	if ( divider < 16 )
		caf |= divider;

	board->MSC.divider = divider;

	board->func->stc_writew( board, STC_Clock_and_FOUT, caf );

	/* Switch clock output on again if necessary */

	if ( output_state ) {
		caf |= FOUT_Enable;
		board->func->stc_writew( board, STC_Clock_and_FOUT, caf );
	}

	return 0;
}


/*---------------------------------------------------*
 * Selects which clock to output at the FREQ_OUT pin
 * and enables or disables output.
 *---------------------------------------------------*/

static int MSC_clock_output( Board *           board,
			     NI_DAQ_CLOCK_TYPE clock,
			     NI_DAQ_STATE      output_state )
{
	u16 caf = board->stc.Clock_and_FOUT;

	if ( clock == NI_DAQ_FAST_CLOCK )
		caf &= ~ FOUT_Timebase_Select;
	else
		caf |= FOUT_Timebase_Select;

	if ( output_state )
		caf |= FOUT_Enable;
	else
		caf &= ~ FOUT_Enable;

	board->MSC.clock = clock;
	board->MSC.output_state = output_state;

	board->func->stc_writew( board, STC_Clock_and_FOUT, caf );

	return 0;
}


/*--------------------------------------------------------*
 * Selects the trigger mode (either TTL digital or analog
 * with the required level(s)).
 *--------------------------------------------------------*/

static int MSC_trigger_setup( Board *          board,
			      NI_DAQ_TRIG_TYPE trigger_type,
			      int              trigger_high,
			      int              trigger_low )
{
	u16 ate = board->stc.Analog_Trigger_Etc &
		  ~ ( Analog_Trigger_Mode_Field | Analog_Trigger_Drive |
		      Analog_Trigger_Enable );
	u16 max_trig;
	u16 th = 0;
	u16 tl = 0;


	if ( trigger_type != NI_DAQ_TRIG_TTL ) {
		if ( ! board->type->has_analog_trig ) {
			PDEBUG( "Board has no analog trigger capability\n" );
			return -EINVAL;
		}

		max_trig = 1 << board->type->atrig_bits;

		switch ( trigger_type ) {
			case NI_DAQ_TRIG_LOW_WINDOW :
				tl = max_trig / 2 - 1 - trigger_low;
				th = 0;
				trigger_high = max_trig / 2 - 1;
				break;

			case NI_DAQ_TRIG_HIGH_WINDOW :
				th = max_trig / 2 - 1 - trigger_high;
				tl = max_trig;
				trigger_low = - max_trig / 2;
				break;

			default :
				th = max_trig / 2 - 1 - trigger_high;
				tl = max_trig / 2 - trigger_low;
				break;
		}

		if ( trigger_low > trigger_high ||
		     trigger_low < - max_trig / 2 ||
		     trigger_high > max_trig / 2 - 1 ) {
			PDEBUG( "Invalid trigger level\n" );
			return -EINVAL;
		}

		ate |= ( trigger_type & Analog_Trigger_Mode_Field ) |
		       Analog_Trigger_Enable;

	} else
		ate |= Analog_Trigger_Drive;

	board->func->stc_writew( board, STC_Analog_Trigger_Etc, ate );

	if ( trigger_type != NI_DAQ_TRIG_TTL )
		board->func->set_trigger_levels( board, th, tl );

	return 0;
}


/*---------------------------------------------------*
 * Function fills in a structure with the properties
 * of a board and returns it to the caller.
 *---------------------------------------------------*/

int MSC_board_properties( Board *                   board,
			  NI_DAQ_BOARD_PROPERTIES * arg )
{
	int i;
	NI_DAQ_BOARD_PROPERTIES p;


	strncpy( p.name, board->type->name, 20 );
	p.name[ 19 ] = '\0';

	p.num_ai_channels = board->type->ai_num_channels;
	p.num_ai_bits     = board->type->ai_num_bits;

	for ( i = 0; i < 8; i++ ) {
		if ( board->type->ai_gains[ i ] == NI_DAQ_GAIN_NOT_AVAIL ) {
			p.num_ai_ranges = i;
			break;
		}

		switch ( board->type->ai_gains[ i ] ) {
			case NI_DAQ_GAIN_0_5 :
				p.ai_mV_ranges[ 0 ][ i ] = 10000;
				p.ai_mV_ranges[ 1 ][ i ] = -1;
				break;

			case NI_DAQ_GAIN_1 :
				if ( ! strcmp( board->type->name,
					       "pci-mio-16e-1" ) ||
				     ! strcmp( board->type->name,
					       "pci-mio-16e-4" ) ||
				     ! strcmp( board->type->name,
					       "pci-6071e" ) ) {
					p.ai_mV_ranges[ 0 ][ i ] = 5000;
					p.ai_mV_ranges[ 1 ][ i ] = 10000;
				} else
					p.ai_mV_ranges[ 0 ][ i ] =
					      p.ai_mV_ranges[ 1 ][ i ] = 10000;
				break;

			case NI_DAQ_GAIN_2 :
				if ( ! strcmp( board->type->name,
					       "pci-mio-16e-1" ) ||
				     ! strcmp( board->type->name,
					       "pci-mio-16e-4" ) ||
				     ! strcmp( board->type->name,
					       "pci-6071e" ) ) {
					p.ai_mV_ranges[ 0 ][ i ] = 2500;
					p.ai_mV_ranges[ 1 ][ i ] = 5000;
				} else
					p.ai_mV_ranges[ 0 ][ i ] =
					       p.ai_mV_ranges[ 1 ][ i ] = 5000;
				break;

			case NI_DAQ_GAIN_5 :
				if ( ! strcmp( board->type->name,
					       "pci-mio-16e-1" ) ||
				     ! strcmp( board->type->name,
					       "pci-mio-16e-4" ) ||
				     ! strcmp( board->type->name,
					       "pci-6071e" ) ) {
					p.ai_mV_ranges[ 0 ][ i ] = 1000;
					p.ai_mV_ranges[ 1 ][ i ] = 2000;
				} else
					p.ai_mV_ranges[ 0 ][ i ] =
					       p.ai_mV_ranges[ 1 ][ i ] = 2000;
				break;

			case NI_DAQ_GAIN_10 :
				if ( ! strcmp( board->type->name,
					       "pci-mio-16e-1" ) ||
				     ! strcmp( board->type->name,
					       "pci-mio-16e-4" ) ||
				     ! strcmp( board->type->name,
					       "pci-6071e" ) ) {
					p.ai_mV_ranges[ 0 ][ i ] = 500;
					p.ai_mV_ranges[ 1 ][ i ] = 1000;
				} else
					p.ai_mV_ranges[ 0 ][ i ] =
					       p.ai_mV_ranges[ 1 ][ i ] = 1000;
				break;

			case NI_DAQ_GAIN_20 :
				if ( ! strcmp( board->type->name,
					       "pci-mio-16e-1" ) ||
				     ! strcmp( board->type->name,
					       "pci-mio-16e-4" ) ||
				     ! strcmp( board->type->name,
					       "pci-6071e" ) ) {
					p.ai_mV_ranges[ 0 ][ i ] = 250;
					p.ai_mV_ranges[ 1 ][ i ] = 500;
				} else
					p.ai_mV_ranges[ 0 ][ i ] =
						p.ai_mV_ranges[ 1 ][ i ] = 500;
				break;

			case NI_DAQ_GAIN_50 :
				if ( ! strcmp( board->type->name,
					       "pci-mio-16e-1" ) ||
				     ! strcmp( board->type->name,
					       "pci-mio-16e-4" ) ||
				     ! strcmp( board->type->name,
					       "pci-6071e" ) ) {
					p.ai_mV_ranges[ 0 ][ i ] = 100;
					p.ai_mV_ranges[ 1 ][ i ] = 200;
				} else
					p.ai_mV_ranges[ 0 ][ i ] =
						p.ai_mV_ranges[ 1 ][ i ] = 200;
				break;

			case NI_DAQ_GAIN_100 :
				if ( ! strcmp( board->type->name,
					       "pci-mio-16e-1" ) ||
				     ! strcmp( board->type->name,
					       "pci-mio-16e-4" ) ||
				     ! strcmp( board->type->name,
					       "pci-6071e" ) ) {
					p.ai_mV_ranges[ 0 ][ i ] = 50;
					p.ai_mV_ranges[ 1 ][ i ] = 100;
				} else
					p.ai_mV_ranges[ 0 ][ i ] =
						p.ai_mV_ranges[ 1 ][ i ] = 100;
				break;

			case NI_DAQ_GAIN_NOT_AVAIL :
				break;
		}
	}

	p.ai_time_res = board->type->ai_speed;

	p.num_ao_channels  = board->type->ao_num_channels;
	p.num_ao_bits      = board->type->ao_num_bits;
	p.ao_does_unipolar = board->type->ao_unipolar;
	p.ao_has_ext_ref   = board->type->ao_has_ext_ref;
	p.has_analog_trig  = board->type->has_analog_trig;
	p.atrig_bits       = board->type->atrig_bits;

	if ( copy_to_user( ( void __user * ) arg, &p, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EFAULT;
	}

	return 0;
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
