/*
 *  $Id$
 * 
 *  Driver for National Instruments DAQ boards based on a DAQ-STC
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

#define GPCT_is_valid_counter( x )  ( ( x ) < 2 )


static int GPCT_clock_speed( Board *                  board,
			     NI_DAQ_CLOCK_SPEED_VALUE speed );

static int GPCT_counter_output_state( Board *      board,
				      unsigned int counter,
				      NI_DAQ_STATE output_state );

static int GPCT_counter_disarm( Board *      board,
				unsigned int counter );

static int GPCT_get_count( Board *         board,
			   unsigned int    counter,
			   int             wait_for_end,
			   unsigned long * count );

static void G0_TC_handler( Board * board );

static void G1_TC_handler( Board * board );

static int GPCT_start_pulses( Board *board,
			      unsigned int    counter,
			      NI_DAQ_INPUT    source,
			      NI_DAQ_INPUT    gate,
			      unsigned long   low_ticks,
			      unsigned long   high_ticks,
			      unsigned long   delay_ticks,
			      NI_DAQ_POLARITY output_polarity,
			      NI_DAQ_POLARITY source_polarity,
			      NI_DAQ_POLARITY gate_polarity,
			      int             continuous,
			      int             delay_start );

static int GPCT_arm( Board *      board,
		     unsigned int counter );

static int GPCT_start_counting( Board *         board,
				unsigned        counter,
				NI_DAQ_INPUT    source,
				NI_DAQ_INPUT    gate,
				NI_DAQ_POLARITY source_polarity,
				NI_DAQ_POLARITY gate_polarity );

static int GPCT_is_busy( Board *      board,
			 unsigned int counter,
			 int *        is_armed );

static int GPCT_input_gate( int   gate,
			    u16 * bits );

static int GPCT_input_source( int   source,
			      u16 * bits );


#if defined NI_DAQ_DEBUG
#define stc_writew( a, b, c ) stc_writew( __FUNCTION__, a, b, c )
#define stc_writel( a, b, c ) stc_writel( __FUNCTION__, a, b, c )
#endif


/*------------------------------------------------------------------*
 * Function resets the whole GCPT subsystem back into a known state
 *------------------------------------------------------------------*/

void GPCT_reset_all( Board * board )
{
	int i;
	u16 data;


	board->func->stc_writew( board, STC_Joint_Reset,
				 board->stc.Joint_Reset |
				 Gi_Reset( 0 ) | Gi_Reset( 1 ) );
	board->stc.Joint_Reset &= ~ ( Gi_Reset( 0 ) | Gi_Reset( 1 ) );

	for ( i = 0; i < 2; i++ ) {
		board->func->stc_writew( board, STC_Gi_Mode( i ), 0 );
		board->func->stc_writew( board, STC_Gi_Command( i ), 0 );
		board->func->stc_writew( board, STC_Gi_Input_Select( i ), 0 );
	}

	/* Disable interrupts */

	board->func->stc_writew( board, STC_Interrupt_A_Enable,
				 board->stc.Interrupt_A_Enable &
				 ~ ( G0_Gate_Interrupt_Enable |
				     G0_TC_Interrupt_Enable ) );

	board->func->stc_writew( board, STC_Interrupt_B_Enable,
				 board->stc.Interrupt_B_Enable &
				 ~ ( G1_Gate_Interrupt_Enable |
				     G1_TC_Interrupt_Enable ) );

	for ( i = 0; i < 2; i++ )
		board->func->stc_writew( board, STC_Gi_Command ( i ),
					 board->stc.Gi_Command[ i ] |
					 Gi_Synchronized_Gate );

	/* Acknowledge (and thereby clear) all interrupt conditions */

	data = board->stc.Interrupt_A_Ack |
	       G0_Gate_Interrupt_Ack      |
	       G0_TC_Interrupt_Ack        |
	       G0_TC_Error_Confirm        |
	       G0_Gate_Error_Confirm;

	board->func->stc_writew( board, STC_Interrupt_A_Ack, data );

	board->stc.Interrupt_A_Ack &= ~ ( G0_Gate_Interrupt_Ack |
					  G0_TC_Interrupt_Ack   |
					  G0_TC_Error_Confirm   |
					  G0_Gate_Error_Confirm   );

	data = board->stc.Interrupt_B_Ack |
	       G1_Gate_Interrupt_Ack      |
	       G1_TC_Interrupt_Ack        |
	       G1_TC_Error_Confirm        |
	       G1_Gate_Error_Confirm;

	board->func->stc_writew( board, STC_Interrupt_B_Ack, data );

	board->stc.Interrupt_B_Ack &= ~ ( G1_Gate_Interrupt_Ack |
					  G1_TC_Interrupt_Ack   |
					  G1_TC_Error_Confirm   |
					  G1_Gate_Error_Confirm   );

	/* Disable output of both counters and select the G_OUT signal as
	   the default output for both counters */

	  board->stc.Analog_Trigger_Etc &=
		       ~ ( GPFO_0_Output_Enable | GPFO_1_Output_Enable |
		           GPFO_0_Output_Select_Field | GPFO_1_Output_Select );

	  board->func->stc_writew( board, STC_Analog_Trigger_Etc,
				   board->stc.Analog_Trigger_Etc );

	  /* Switch to full clock speed */

	  board->stc.Clock_and_FOUT &= ~ G_Source_Divide_By_2;
	  board->GPCT0.timebase1 = board->GPCT0.timebase1 = 50;

	  board->func->stc_writew( board, STC_Clock_and_FOUT,
				   board->stc.Clock_and_FOUT );

	/* Switch off DMA and free DMA buffers */

	board->func->dma_shutdown( board, NI_DAQ_GPCT0_SUBSYSTEM );
	board->func->dma_shutdown( board, NI_DAQ_GPCT1_SUBSYSTEM );

	/* Zero out the auto-increment values */

	for ( i = 0; i < 2; i++ )
		board->func->stc_writew( board, STC_Gi_Autoincrement( i ), 0 );
}


/*--------------------------------------------------*
 * Handler for ioctl() calls for the GPCT subsystem
 *--------------------------------------------------*/

int GPCT_ioctl_handler( Board *           board,
			NI_DAQ_GPCT_ARG * arg )
{
	NI_DAQ_GPCT_ARG a;
	int ret = 0;


	if ( copy_from_user( &a, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	switch ( a.cmd ) {
		case NI_DAQ_GPCT_SET_CLOCK_SPEED :
			return GPCT_clock_speed( board, a.speed );

		case NI_DAQ_GPCT_GET_CLOCK_SPEED :
			a.speed = ( board->stc.Clock_and_FOUT &
				    G_Source_Divide_By_2 ) ?
				   NI_DAQ_HALF_SPEED : NI_DAQ_FULL_SPEED;
			break;

		case NI_DAQ_GPCT_COUNTER_OUTPUT_STATE :
			return GPCT_counter_output_state( board, a.counter,
							  a.output_state );

		case NI_DAQ_GPCT_DISARM_COUNTER :
			return GPCT_counter_disarm( board, a.counter );

		case NI_DAQ_GPCT_START_COUNTER :
			return GPCT_start_counting( board, a.counter,
						    a.source, a.gate,
						    a.source_polarity,
						    a.gate_polarity );

		case NI_DAQ_GPCT_START_PULSER :
			return GPCT_start_pulses( board, a.counter,
						  a.source, a.gate,
						  a.low_ticks, a.high_ticks,
						  a.delay_ticks,
						  a.output_polarity,
						  a.source_polarity,
						  a.gate_polarity,
						  a.continuous,
						  a.delay_start );

		case NI_DAQ_GPCT_ARM :
			return GPCT_arm( board, a.counter );

		case NI_DAQ_GPCT_GET_COUNT :
			ret = GPCT_get_count( board, a.counter,
					      a.wait_for_end, &a.count );
			break;

		case NI_DAQ_GPCT_IS_BUSY :
			ret = GPCT_is_busy( board, a.counter, &a.is_armed );
			break;

		default :
			PDEBUG( "Invalid GPCT command %d\n", a.cmd );
			return -EINVAL;
	}

	if ( ret == 0 && copy_to_user( arg, &a, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EACCES;
	}

	return ret;
}


/*----------------------------------------------------------------------*
 * Function for setting the G_IN_TIMEBASE1 - it can be switched between
 * 20 MHz and 10 MHz (please note that the time base for IN_TIMEBASE2
 * is controlled by the general setting of the MSC subsystem!).
 *----------------------------------------------------------------------*/

static int GPCT_clock_speed( Board *                  board,
			     NI_DAQ_CLOCK_SPEED_VALUE speed )
{
	if ( speed == NI_DAQ_FULL_SPEED ) {
		board->stc.Clock_and_FOUT &= ~ G_Source_Divide_By_2;
		board->GPCT0.timebase1 = board->GPCT1.timebase1 = 50;
	} else {
		board->stc.Clock_and_FOUT |= G_Source_Divide_By_2;
		board->GPCT0.timebase1 = board->GPCT1.timebase1 = 100;
	}

	board->func->stc_writew( board, STC_Clock_and_FOUT,
				 board->stc.Clock_and_FOUT );

	return 0;
}


/*------------------------------------------------------------*
 * Function for enabling or disabling the output of a counter
 *------------------------------------------------------------*/

static int GPCT_counter_output_state( Board *      board,
				      unsigned int counter,
				      NI_DAQ_STATE output_state )
{
	if ( ! GPCT_is_valid_counter( counter ) ) {
		PDEBUG( "Invalid counter number %d\n", counter );
		return -EINVAL;
	}

	if ( output_state == NI_DAQ_ENABLED )
		board->stc.Analog_Trigger_Etc |=
			GPFO_i_Output_Enable( counter );
	else
		board->stc.Analog_Trigger_Etc &=
			~ GPFO_i_Output_Enable( counter );
		
	board->func->stc_writew( board, STC_Analog_Trigger_Etc,
				 board->stc.Analog_Trigger_Etc );

	return 0;
}


/*---------------------------------------------------------------------*
 * Function for stopping the counter(s) of a board, i.e. disarming it.
 *---------------------------------------------------------------------*/

static int GPCT_counter_disarm( Board *      board,
				unsigned int counter )
{
	switch ( counter ) {
		case 0 : case 1 :
			board->stc.Joint_Reset |= Gi_Reset( counter );
			board->func->stc_writew( board, STC_Joint_Reset,
						 board->stc.Joint_Reset );
			board->stc.Joint_Reset &= ~ Gi_Reset( counter );
			return 0;

		case 2 :
			board->stc.Joint_Reset |=
						 Gi_Reset( 0 ) | Gi_Reset( 1 );
			board->func->stc_writew( board, STC_Joint_Reset,
						 board->stc.Joint_Reset );
			board->stc.Joint_Reset &=
					   ~ ( Gi_Reset( 0 ) | Gi_Reset( 1 ) );
			return 0;
	}

	PDEBUG( "Invalid counter number %d\n", counter );
	return -EINVAL;
}


/*--------------------------------------------------------------------------*
 * Function for reading the value of a counter (via the save registers).
 * When called with the 'wait_on_end' member of the NI_DAQ_COUNTER_VAL
 * structure passed to the function being set it's assumed that the counter
 * is doing a gated measurement with the neighboring counter as the source
 * of the (single) gate pulse. The function will then wait for the neigh-
 * boring counter to stop counting before disarming the counter and reading
 * the counters value.
 *--------------------------------------------------------------------------*/

static int GPCT_get_count( Board *         board,
			   unsigned int    counter,
			   int             wait_for_end,
			   unsigned long * count )
{
	u16 cmd;
	u32 next_val;


	if ( ! GPCT_is_valid_counter( counter ) ) {
		PDEBUG( "Invalid counter number %d\n", counter );
		return -EINVAL;
	}

	/* If required wait for counting to stop (by waiting for the
	   neighboring counter creating the gate pulse to stop) and then
	   reset both counters. */

	if ( wait_for_end ) {
		if ( counter == 0 ) {
			daq_irq_enable( board, IRQ_G1_TC, G1_TC_handler );
			wait_event_interruptible( board->GPCT1.waitqueue,
					 board->irq_hand[ IRQ_G1_TC ].raised );
			daq_irq_disable( board, IRQ_G1_TC );
		} else {
			daq_irq_enable( board, IRQ_G1_TC, G0_TC_handler );
			wait_event_interruptible( board->GPCT0.waitqueue,
					 board->irq_hand[ IRQ_G0_TC ].raised );
			daq_irq_disable( board, IRQ_G0_TC );
		}

		if ( signal_pending( current ) ) {
			PDEBUG( "Aborted by signal\n" );
			return -EINTR;
		}

		*count = board->func->stc_readl( board,
						 STC_Gi_HW_Save( counter ) );

		board->func->stc_writew( board, STC_Joint_Reset,
					 board->stc.Joint_Reset | Gi_Disarm );
		board->stc.Joint_Reset &= ~ Gi_Disarm;
	} else {
		/* Otherwise enable tracing, read the save register until
		   there's a consistent value and then again disable tracing */

		cmd = board->stc.Gi_Command[ counter ];

		cmd &= ~ ( Gi_Save_Trace_Copy | Gi_Save_Trace );
		board->func->stc_writew( board, STC_Gi_Command( counter ),
					 cmd );

		cmd |= Gi_Save_Trace;
		board->func->stc_writew( board, STC_Gi_Command( counter ),
					 cmd );

		*count = board->func->stc_readl( board,
						 STC_Gi_Save( counter ) );
		next_val = board->func->stc_readl( board,
						   STC_Gi_Save( counter ) );

		if ( *count != next_val )
			*count = board->func->stc_readl( board,
						      STC_Gi_Save( counter ) );

		cmd &= ~ Gi_Save_Trace;
		board->func->stc_writew( board, STC_Gi_Command( counter ),
					 cmd );
	}

	return 0;
}

/*----------------------------------------------------*
 *----------------------------------------------------*/

static void G0_TC_handler( Board * board )
{
	wake_up_interruptible( &board->GPCT0.waitqueue );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static void G1_TC_handler( Board * board )
{
	wake_up_interruptible( &board->GPCT1.waitqueue );
}


/*---------------------------------------------------*
 * Function to start output of pulses from a counter
 *---------------------------------------------------*/

static int GPCT_start_pulses( Board *         board,
			      unsigned int    counter,
			      NI_DAQ_INPUT    source,
			      NI_DAQ_INPUT    gate,
			      unsigned long   low_ticks,
			      unsigned long   high_ticks,
			      unsigned long   delay_ticks,
			      NI_DAQ_POLARITY output_polarity,
			      NI_DAQ_POLARITY source_polarity,
			      NI_DAQ_POLARITY gate_polarity,
			      int             continuous,
			      int             delay_start )
{
	int use_gate;
	u16 input = 0;
	u16 mode = 0;
	u16 cmd = delay_start ? 0 : Gi_Arm;


	/* Check that the specified counter is valid */

	if ( ! GPCT_is_valid_counter( counter ) ) {
		PDEBUG( "Invalid counter number %d\n", counter );
		return -EINVAL;
	}

	/* Find out if and what to use as the gate */

	if ( ( use_gate = GPCT_input_gate( gate, &input ) ) < 0 ) {
		PDEBUG( "Invalid gate setting %d\n", gate );
		return -EINVAL;
	}

	/* Find out what to use as the source of the counter (usually this
	   will be the internal timer) */

	if ( GPCT_input_source( source, &input ) <= 0 ) {
		PDEBUG( "Invalid source setting %d\n", source );
		return -EINVAL;
	}

	/* Check the times for the 'low' and 'high' period as well as the
	   initial delay of the pulses */

	if ( low_ticks < 2 || high_ticks < 2 ) {
		PDEBUG( "Invalid low or high ticks\n" );
		return -EINVAL;
	}

	if ( delay_ticks < 2 )
		delay_ticks = 2;

	if ( low_ticks > 0xFFFFFF ) {
		PDEBUG( "Low period too long\n" );
		return -EINVAL;
	}

	if ( high_ticks > 0xFFFFFF ) {
		PDEBUG( "High period too long\n" );
		return -EINVAL;
	}

	if ( delay_ticks > 0xFFFFFF ) {
		PDEBUG( "Delay period too long\n" );
		return -EINVAL;
	}

	/* Set the polarity of the output */

	if ( source_polarity == NI_DAQ_INVERTED )
		input |= Gi_Source_Polarity;

	if ( output_polarity != NI_DAQ_NORMAL )
		input |= Gi_Output_Polarity;

	board->func->stc_writew( board, STC_Gi_Input_Select( counter ),
				 input );

	/* Load the delay into the counter so that the first pulse starts
	   after the first TC event that happens when the delay is over */

	board->func->stc_writel( board, STC_Gi_Load_A( counter ),
				 delay_ticks - 1 );
	board->func->stc_writew( board, STC_Gi_Command( counter ),
				 board->stc.Gi_Command[ counter ] | Gi_Load );
	board->stc.Gi_Command[ counter ] &= ~ Gi_Load;

	/* Set the count for the duration of the high and the low voltage
	   phase of the pulse (both reduced by 1) */

	board->func->stc_writel( board, STC_Gi_Load_A( counter ),
				 high_ticks - 1 );
	board->func->stc_writel( board, STC_Gi_Load_B( counter ),
				 low_ticks - 1 );

	/* Assemble value to be written into the mode register: load on
	   counter TC, toggling between load registers A and B, toggle
	   output on TC condition and set register A as the first register
	   to load from. */

	mode |= Gi_Reload_Source_Switching | Gi_Loading_On_TC |
		( 2 << Gi_Output_Mode_Shift );

	/* In non-continuous mode (i.e. when creating just a single pulse)
	   stop on second TC condition */

	if ( ! continuous )
		mode |= 2 << Gi_Stop_Mode_Shift;

	/* If a gate is to be used set level gating and synchronize between
	   gate and counter */

	if ( use_gate ) {
		mode |= 1 | ( 3 << Gi_Trigger_Mode_For_Edge_Gate_Shift );
		if ( gate_polarity == NI_DAQ_INVERTED )
			mode |= Gi_Gate_Polarity;
		cmd |= Gi_Synchronized_Gate;
	}

	/* Set the mode */

	board->func->stc_writew( board, STC_Gi_Mode( counter ), mode );

	/* Finally set counter to count downward and, if necessary, arm it */

	board->func->stc_writew( board, STC_Gi_Command( counter ), cmd );

	if ( ! delay_start )
		board->stc.Gi_Command[ counter ] &= ~ Gi_Arm;

	return 0;
}


/*-------------------------------------------------------------*
 * Function to start output of pulses from one or both counter
 * (it or they must have been set up by an ioctl() call with
 * NI_DAQ_GPCT_START_PULSER in delayed start mode)
 *-------------------------------------------------------------*/

static int GPCT_arm( Board *      board,
		     unsigned int counter )
{
	u16 cmd = Gi_Arm;


	switch ( counter ) {
		case 0 : case 1 :
			cmd = board->stc.Gi_Command[ counter ] | Gi_Arm;
			board->func->stc_writew( board,
						 STC_Gi_Command( counter ),
						 cmd );
			board->stc.Gi_Command[ counter ] &= ~ Gi_Arm;
			return 0;

		case 2 :                      /* both counters at once! */
			cmd = board->stc.Gi_Command[ 0 ] |
			      Gi_Arm | Gi_Arm_Copy;
			board->func->stc_writew( board, STC_Gi_Command( 0 ),
						 cmd );
			board->stc.Gi_Command[ 0 ] &=
						    ~ ( Gi_Arm | Gi_Arm_Copy );
			return 0;
	}

	PDEBUG( "Invalid counter number %d\n", counter );
	return -EINVAL;
}



/*----------------------------------*
 * Function to start event counting
 *----------------------------------*/

static int GPCT_start_counting( Board *         board,
				unsigned int    counter,
				NI_DAQ_INPUT    source,
				NI_DAQ_INPUT    gate,
				NI_DAQ_POLARITY source_polarity,
				NI_DAQ_POLARITY gate_polarity )
{
	int use_gate;
	u16 input;
	u16 mode;
	u16 cmd;


	if ( ! GPCT_is_valid_counter( counter ) ) {
		PDEBUG( "Invalid counter number %d\n", counter );
		return -EINVAL;
	}

	if ( ( use_gate = GPCT_input_gate( gate, &input ) ) < 0 ) {
		PDEBUG( "Invalid gate setting %d\n", gate );
		return -EINVAL;
	}

	if ( GPCT_input_source( source, &input ) <= 0 ) {
		PDEBUG( "Invalid source setting %d\n", source );
		return -EINVAL;
	}

	if ( source_polarity == NI_DAQ_INVERTED )
		input |= Gi_Source_Polarity;

	board->func->stc_writew( board, STC_Gi_Input_Select( counter ),
				 input );

	/* initialize the counter with 0 */

	board->func->stc_writel( board, STC_Gi_Load_A( counter ), 0 );
	board->func->stc_writew( board, STC_Gi_Command( counter ), Gi_Load );
	board->stc.Gi_Command[ counter ] &= ~ Gi_Load;

	/* Output is usually not relevant for a counter but, since something
	   must be specified, default to something nearly useless, e.g. the TC
	   signal which only happens when the counter overflows. */

	mode = 1 << Gi_Output_Mode_Shift;

	/* Set the counter to count up and also set the bits for loading
	   from load register A and for arming */

	cmd = ( 1 << Gi_Up_Down_Shift ) | Gi_Load | Gi_Arm;

	/* If gating is to be used use level gating and synchronize between
	   counter and gate */

	if ( use_gate ) {
		mode |= 1;
		if ( gate_polarity == NI_DAQ_INVERTED )
			mode |= Gi_Gate_Polarity;
		cmd  |= Gi_Synchronized_Gate;
	}

	board->func->stc_writew( board, STC_Gi_Mode( counter ), mode );

	/* Finally start the counter by arming it */

	board->func->stc_writew( board, STC_Gi_Command( counter ), cmd );
	board->stc.Gi_Command[ counter ] &= ~ Gi_Arm;

	return 0;
}


/*---------------------------------------------------------------*
 * Function checks if a counter of a board is currently counting
 * (i.e. if it's armed).
 *---------------------------------------------------------------*/

static int GPCT_is_busy( Board *  board,
			 unsigned counter,
			 int *    is_armed )
{
	if ( ! GPCT_is_valid_counter( counter ) ) {
		PDEBUG( "Invalid counter number %d\n", counter );
		return -EINVAL;
	}

	/* Test if the counter is armed */

	*is_armed =  ( board->func->stc_readw( board, STC_G_Status ) &
		       Gi_Armed_St( counter ) ) ? 1 : 0;
	
	return 0;
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static int GPCT_input_gate( int   gate,
			    u16 * bits )
{
	if ( gate == NI_DAQ_NONE ) {
		*bits = 0;
		return 0;
	}

	if ( gate != NI_DAQ_G_IN_TIMEBASE1 ) {
		*bits |= gate << Gi_Gate_Select_Shift;
		return 1;
	}

	return -EINVAL;
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static int GPCT_input_source( int   source,
			      u16 * bits )
{
	if ( source == NI_DAQ_NONE ) {
		return 0;
	}

	if ( source > NI_DAQ_G_TC_OTHER && source != NI_DAQ_LOW &&
	     source != NI_DAQ_IN_TIMEBASE2 )
		return -EINVAL;

	if ( source == NI_DAQ_IN_TIMEBASE2 )
		*bits |= 18 << Gi_Source_Select_Shift;
	else
		*bits |= source << Gi_Source_Select_Shift;

	return 1;
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
