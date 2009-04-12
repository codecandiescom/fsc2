/*
 *  Driver for National Instruments 6601 boards
 * 
 *  Copyright (C) 2002-2009 Jens Thoms Toerring
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

static int ni6601_tc_irq_handler( Board *board );
static int ni6601_dma_irq_handler( Board *board );


/*-----------------------------------------------------------*
 * Function reads an 8-bit value from the DIO pins (PFI 0-7)
 * but only from pins for which bits are set in the mask
 * (this allows having some of the bits used for input while
 * the others are used for output at the same time).
 *-----------------------------------------------------------*/

int ni6601_dio_in( Board *           board,
		   NI6601_DIO_VALUE * arg )
{
	NI6601_DIO_VALUE dio;


	if ( copy_from_user( &dio, ( const void __user * ) arg,
			     sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	/* If necessary switch selected pins to input mode */

	if ( ( ( u8 ) ( board->dio_mask & 0xFF ) ^ dio.mask ) != 0xFF ) {
		board->dio_mask &= ~ ( u16 ) dio.mask;
		iowrite16( board->dio_mask, board->regs.dio_control );
	}

	/* Read in data value from DIO parallel input register */

	dio.value = ( unsigned char )
		    ( ioread16( board->regs.dio_parallel_in ) & dio.mask );

	if ( copy_to_user( ( void __user * ) arg, &dio, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EFAULT;
	}

	return 0;
}


/*-----------------------------------------------------------*
 * Function outputs an 8-bit value at the DIO pins (PFI 0-7)
 * but only at the pins for which bits are set in the mask
 * (this allows having some of the bits used for output
 * while the others are used for input at the same time).
 *-----------------------------------------------------------*/

int ni6601_dio_out( Board *            board,
		    NI6601_DIO_VALUE * arg )
{
	NI6601_DIO_VALUE dio;


	if ( copy_from_user( &dio, ( const void __user * ) arg,
			     sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	/* Write data value to DIO parallel output register */

	iowrite16( ( u16 ) dio.value, board->regs.dio_output );

	/* Now if necessary switch selected pins to output mode */

	if ( ( board->dio_mask & 0xFF ) != dio.mask ) {
		board->dio_mask |= dio.mask;
		iowrite16( board->dio_mask, board->regs.dio_control );
	}

	return 0;
}


/*------------------------------------------------------------------*
 * Function for stopping the counter of a board, i.e. disarming it.
 *------------------------------------------------------------------*/

int ni6601_disarm( Board *         board,
		   NI6601_DISARM * arg )
{
	NI6601_DISARM d;


	if ( copy_from_user( &d, ( const void __user * ) arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	if ( d.counter < NI6601_COUNTER_0 || d.counter > NI6601_COUNTER_3 ) {
		PDEBUG( "Invalid counter number %d\n", d.counter );
		return -EINVAL;
	}

	iowrite16( DISARM, board->regs.command[ d.counter ] );

	return 0;
}


/*---------------------------------------------------------------------------*
 * Function for reading the value of a counter (from the SW save register).
 * When called with the 'wait_on_end' member of the NI6601_COUNTER_VAL
 * structure passed to the function being set it's assumed that the counter
 * is doing a gated measurement with the neighbouring counter as the source
 * of the (single) gate pulse. The function will then wait for the neigh-
 * bouring counter to stop counting before disarming the counter and reading
 * the counter value.
 *---------------------------------------------------------------------------*/

int ni6601_read_count( Board *              board,
		       NI6601_COUNTER_VAL * arg )
{
	NI6601_COUNTER_VAL cs;
	u32 next_val;


	if ( copy_from_user( &cs, ( const void __user * ) arg,
			     sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	if ( cs.counter < NI6601_COUNTER_0 || cs.counter > NI6601_COUNTER_3 ) {
		PDEBUG( "Invalid counter number %d\n", cs.counter );
		return -EINVAL;
	}

	/* If required wait for counting to stop (by waiting for the
	   neighboring counter creating the gate pulse to stop) and then
	   reset both counters. We can wait either for an interrupt,
	   putting the process to sleep in the meantime, or poll the
	   status register in a tight loop. What is done depends on
	   the 'do_poll' member of the argument structure - polling may
	   get used when the calling process knows exactly that we're nearly
	   done with counting and don't want to lose time by giving up it's
	   time slice, while otherwise waiting for the interrupt is much more
	   preferable since it won't tie up the CPU. */

	if ( cs.wait_for_end ) {

		int oc = cs.counter + ( cs.counter & 1 ? -1 : 1 );

		if ( cs.do_poll ) {
			while ( ioread16( board->regs.joint_status[ oc ] ) &
				Gi_COUNTING( oc ) &&
				! signal_pending( current ) )
				/* empty */ ;
		} else {
			ni6601_tc_irq_enable( board, oc );
			if ( ioread16( board->regs.joint_status[ oc ] ) &
			     Gi_COUNTING( oc ) )
				wait_event_interruptible( board->tc_waitqueue,
						  board->tc_irq_raised[ oc ] );
			ni6601_tc_irq_disable( board, oc );
		}

		iowrite16( G1_RESET | G0_RESET,
			   board->regs.joint_reset[ cs.counter ] );

		if ( signal_pending( current ) ) {
			PDEBUG( "Aborted by signal\n" );
			return -EINTR;
		}
	}

	/* Read the SW save register to get the current count value */

	cs.count = ioread32( board->regs.sw_save[ cs.counter ] );

	/* If the counter is armed, re-read the SW save register until two
	   readings are identical */

	if ( ioread16( board->regs.joint_status[ cs.counter ] ) &
	     Gi_ARMED( cs.counter ) )
		while ( cs.count != ( next_val =
			      ioread32( board->regs.sw_save[ cs.counter ] ) ) )
			cs.count = next_val;
	
	if ( copy_to_user( ( void __user * ) arg, &cs, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EFAULT;
	}

	return 0;
}


/*----------------------------------------------------*
 * Function to start outputting pulses from a counter
 *----------------------------------------------------*/

int ni6601_start_pulses( Board *         board,
			 NI6601_PULSES * arg )
{
	NI6601_PULSES p;
	u16 gate_bits;
	u16 source_bits;
	u16 mode_bits = 0;
	int use_gate;
	u16 cmd_bits = ALWAYS_DOWN;


	if ( copy_from_user( &p, ( const void __user * ) arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	if ( p.counter < NI6601_COUNTER_0 || p.counter > NI6601_COUNTER_3 ) {
		PDEBUG( "Invalid counter number %d\n", p.counter );
		return -EINVAL;
	}

	if ( ( use_gate = ni6601_input_gate( p.gate, &gate_bits ) ) < 0 ) {
		PDEBUG( "Invalid gate setting %d\n", p.gate );
		return -EINVAL;
	}

	if ( ni6601_input_source( p.source, &source_bits ) <= 0 ) {
		PDEBUG( "Invalid source setting %d\n", p.source );
		return -EINVAL;
	}

	if ( p.low_ticks < 2 || p.high_ticks < 2 ) {
		PDEBUG( "Invalid low or high ticks\n" );
		return -EINVAL;
	}

	/* Set the input select register */

	source_bits |= gate_bits;
	if ( p.output_polarity != NI6601_NORMAL )
		source_bits |= INVERTED_OUTPUT_POLARITY;

	iowrite16( source_bits, board->regs.input_select[ p.counter ] );

	/* Load counter with 1 so we're able to start the first pulse as
	   fast as possible, i.e. 100 ns after arming the counter */

	iowrite32( 1UL, board->regs.load_a[ p.counter ] );
	iowrite16( LOAD, board->regs.command[ p.counter ] );

	/* Use normal counting and no second gating */

	iowrite16( 0, board->regs.counting_mode[ p.counter ] );
	iowrite16( 0, board->regs.second_gate[ p.counter ] );

	if ( ! p.disable_output )
		ni6601_enable_out( board, NI6601_OUT( p.counter ) );
	else
		ni6601_disable_out( board, NI6601_OUT( p.counter ) );

	/* Set the count for the duration of the high and the low voltage
	   phase of the pulse (after subtracting 1) */

	iowrite32( p.high_ticks - 1, board->regs.load_a[ p.counter ] );
	iowrite32( p.low_ticks  - 1, board->regs.load_b[ p.counter ] );

	/* Assemble value to be written into the mode register */

	mode_bits = RELOAD_SOURCE_SWITCHING | LOADING_ON_TC |
		    OUTPUT_MODE_TOGGLE_ON_TC | LOAD_SOURCE_SELECT_A;

	if ( ! p.continuous )
		mode_bits |= STOP_MODE_ON_GATE_OR_2ND_TC;

	if ( use_gate ) {
		mode_bits |= GATING_MODE_LEVEL;
		cmd_bits  |= SYNCHRONIZE_GATE;
	}

	iowrite16( mode_bits, board->regs.mode[ p.counter ] );

	/* Finally start outputting the pulses */

	iowrite16( cmd_bits, board->regs.command[ p.counter ] );
	cmd_bits &= ~ LOAD;
	iowrite16( cmd_bits | ARM, board->regs.command[ p.counter ] );

	return 0;
}


/*---------------------------------------*
 * Function for starting event counting.
 *---------------------------------------*/

int ni6601_start_counting( Board *          board,
			   NI6601_COUNTER * arg )
{
	NI6601_COUNTER c;
	u16 gate_bits;
	u16 source_bits;
	u16 mode_bits = OUTPUT_MODE_TC;
	int use_gate;
	u16 cmd_bits = ALWAYS_UP | LOAD;


	if ( copy_from_user( &c, ( const void __user * ) arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	if ( c.counter < NI6601_COUNTER_0 || c.counter > NI6601_COUNTER_3 ) {
		PDEBUG( "Invalid counter number %d\n", c.counter );
		return -EINVAL;
	}

	if ( ( use_gate = ni6601_input_gate( c.gate, &gate_bits ) ) < 0 ) {
		PDEBUG( "Invalid gate setting %d\n", c.gate );
		return -EINVAL;
	}

	if ( ni6601_input_source( c.source, &source_bits ) <= 0 ) {
		PDEBUG( "Invalid source setting %d\n", c.source );
		return -EINVAL;
	}

	/* Set input source and gate as well as source input polarity */

	source_bits |= gate_bits;
	if ( c.source_polarity != NI6601_NORMAL )
		source_bits |= INVERTED_SOURCE_POLARITY;

	iowrite16( source_bits, board->regs.input_select[ c.counter ] );

	if ( ! c.disable_output )
		ni6601_enable_out( board, NI6601_OUT( c.counter ) );
	else
		ni6601_disable_out( board, NI6601_OUT( c.counter ) );

	/* Use normal counting and no second gating */

	iowrite16( 0, board->regs.counting_mode[ c.counter ] );
	iowrite16( 0, board->regs.second_gate[ c.counter ] );

	if ( use_gate ) {
		mode_bits |= GATING_MODE_LEVEL;
		cmd_bits  |= SYNCHRONIZE_GATE;
	}

	iowrite16( mode_bits, board->regs.mode[ c.counter ] );

	/* Put start value into Load Register A, set counting direction
	   etc., then arm (arming while loading does not seems to work
	   for some of the counters...) */

	iowrite32( 0UL, board->regs.load_a[ c.counter ] );
	iowrite16( cmd_bits, board->regs.command[ c.counter ] );
	cmd_bits &= ~ LOAD;
	cmd_bits |= ARM;
	iowrite16( cmd_bits, board->regs.command[ c.counter ] );

	return 0;
}


/*-------------------------------------------------------------------*
 * Function for starting event counting with DMA mode PIO transfers.
 *-------------------------------------------------------------------*/

int ni6601_start_buf_counting( Board *              board,
			       NI6601_BUF_COUNTER * arg )
{
	NI6601_BUF_COUNTER c;
	u16 gate_bits;
	u16 source_bits;
	u16 mode_bits = LOADING_ON_GATE | TRIGGER_MODE_NO_GATE |
		        GATING_MODE_RISING_EDGE;
	int use_gate;
	u16 cmd_bits = LOAD | ALWAYS_UP;


	if ( copy_from_user( &c, ( const void __user * ) arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	if ( board->buf_counter >= 0 ) {
		PDEBUG( "Buffered counting already in progress\n" );
		return -EINVAL;
	}

	if ( c.counter < NI6601_COUNTER_0 || c.counter > NI6601_COUNTER_3 ) {
		PDEBUG( "Invalid counter number %d\n", c.counter );
		return -EINVAL;
	}

	if ( ( use_gate = ni6601_input_gate( c.gate, &gate_bits ) ) < 0 ) {
		PDEBUG( "Invalid gate setting %d\n", c.gate );
		return -EINVAL;
	}

	if ( ni6601_input_source( c.source, &source_bits ) <= 0 ) {
		PDEBUG( "Invalid source setting %d\n", c.source );
		return -EINVAL;
	}

	if ( c.continuous && c.num_points == 0 ) {
		PDEBUG( "Zero number of points\n" );
		return -EINVAL;
	}

	/* Set input source and gate as well as source input polarity */

	source_bits |= gate_bits;
	if ( c.source_polarity != NI6601_NORMAL )
		source_bits |= INVERTED_SOURCE_POLARITY;

	iowrite16( source_bits, board->regs.input_select[ c.counter ] );

	if ( ! c.disable_output )
		ni6601_enable_out( board, NI6601_OUT( c.counter ) );
	else
		ni6601_disable_out( board, NI6601_OUT( c.counter ) );

	/* Use normal counting and no second gating */

	iowrite16( 0, board->regs.counting_mode[ c.counter ] );
	iowrite16( 0, board->regs.second_gate[ c.counter ] );

	if ( use_gate )
		cmd_bits  |= SYNCHRONIZE_GATE;

	iowrite16( mode_bits, board->regs.mode[ c.counter ] );

	if ( ( board->buf = vmalloc( c.num_points * sizeof *board->buf ) )
	     == NULL ) {
		PDEBUG( "Not enough memory for buffer\n" );
		return -ENOMEM;
	}

	board->low_ptr = board->high_ptr = board->buf;
	board->buf_top = board->buf + c.num_points;
	board->buf_counter = c.counter;
	board->buf_continuous = c.continuous;
	board->is_just_started = 1;
	board->buf_overflow = 0;
	board->too_fast = 0;
	board->buf_waiting = 0;

	ni6601_dma_irq_enable( board, c.counter );

	/* Put start value into Load Register A, set counting direction etc.
	   and arm  */

	iowrite32( 0, board->regs.load_a[ c.counter ] );
	iowrite16( cmd_bits, board->regs.command[ c.counter ] );
	cmd_bits &= ~ LOAD;
	cmd_bits |= ARM;
	iowrite16( cmd_bits, board->regs.command[ c.counter ] );

	return 0;
}


/*-------------------------------------------------------------*
 * Function for determining how many data can be returned from
 * a buffered counter.
 *-------------------------------------------------------------*/

int ni6601_get_buf_avail( Board *            board,
			  NI6601_BUF_AVAIL * arg )
{
	if ( board->buf_counter < 0 )
		return -EINVAL;

	if ( ! board->buf_continuous )
		arg->count = ( board->high_ptr - board->low_ptr )
			     * sizeof *board->buf;
	else {
		u32 *high_ptr = high_ptr = board->high_ptr;

		if ( board->low_ptr > high_ptr )
			arg->count = sizeof *board->buf
			 	     * (   board->buf_top - board->low_ptr
					 + high_ptr - board->buf );
		else
			arg->count = ( high_ptr - board->low_ptr )
				     * sizeof *board->buf;
	}

	return 0;
}


/*-----------------------------------------------------------*
 * Internally used function for stopping a buffered counter.
 *-----------------------------------------------------------*/

int ni6601_stop_buf_counting( Board * board )
{
	if ( board->buf_counter >= 0 ) {
		ni6601_dma_irq_disable( board, board->buf_counter );
		board->buf_counter = -1;
		if ( board->buf ) {
			vfree( board->buf );
			board->buf = NULL;
		}
	}

	return 0;
}


/*---------------------------------------------------------------*
 * Function checks if a counter of a board is currently counting
 * (i.e. if it's armed).
 *---------------------------------------------------------------*/

int ni6601_is_busy( Board *           board,
		    NI6601_IS_ARMED * arg )
{
	NI6601_IS_ARMED a;


	if ( copy_from_user( &a, ( const void __user * ) arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EFAULT;
	}

	/* Test if the counter is armed */

	a.state =  ( ioread16( board->regs.joint_status[ a.counter ] ) &
		     Gi_ARMED( a.counter ) ) ? 1 : 0;
	
	if ( copy_to_user( ( void __user * ) arg, &a, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EFAULT;
	}

	return 0;
}


/*-----------------------------------------------------------*
 * Function to enable TC interrupts from one of the counters
 *-----------------------------------------------------------*/

void ni6601_tc_irq_enable( Board * board,
			   int     counter )
{
	board->tc_irq_raised[ counter ] = 0;
	if ( ! board->tc_irq_enabled[ counter ] )
	{
		PDEBUG( "Enabling TC IRQ for counter %d\n", counter );
		iowrite16( Gi_TC_INTERRUPT_ACK,
			   board->regs.irq_ack[ counter ] );
		iowrite16( Gi_TC_INTERRUPT_ENABLE( counter ),
			   board->regs.irq_enable[ counter ] );
		board->tc_irq_enabled[ counter ] = 1;
		board->expect_tc_irq = 1;
	}
}


/*------------------------------------------------------------*
 * Function to disable TC interrupts from one of the counters
 *------------------------------------------------------------*/

void ni6601_tc_irq_disable( Board * board,
			    int     counter )
{
	int i;
	int tc_irq_count = 0;


	if ( board->tc_irq_enabled[ counter ] )
	{
		PDEBUG( "Disabling TC IRQ for counter %d\n", counter );
		iowrite16( 0, board->regs.irq_enable[ counter ] );
		iowrite16( Gi_TC_INTERRUPT_ACK,
			   board->regs.irq_ack[ counter ] );
		board->tc_irq_enabled[ counter ] = 0;
		for ( tc_irq_count = 0, i = 0; i < 4; i++ )
			tc_irq_count += board->tc_irq_enabled[ i ];
		if ( tc_irq_count == 0 )
			board->expect_tc_irq = 0;
	}
}


/*------------------------------------------------------------*
 * Function to enable DMA interrupts from one of the counters
 *------------------------------------------------------------*/

void ni6601_dma_irq_enable( Board * board,
			    int     counter )
{
	if ( ! board->dma_irq_enabled )
	{
		PDEBUG( "Enabling DMA IRQ for counter %d\n", counter );
		iowrite16( DMA_INT | DMA_ENABLE,
			   board->regs.dma_config[ counter ] );
		board->dma_irq_enabled = 1;
	}
}


/*-------------------------------------------------------------*
 * Function to disable DMA interrupts from one of the counters
 *-------------------------------------------------------------*/

void ni6601_dma_irq_disable( Board * board,
			     int     counter )
{
	if ( board->dma_irq_enabled )
	{
		PDEBUG( "Disabling DMA IRQ for counter %d\n", counter );
		iowrite16( DISARM, board->regs.command[ counter ] );
		iowrite16( 0, board->regs.dma_config[ counter ] );
		board->dma_irq_enabled = 0;
	}
}


/*----------------------------------*
 * Interrupt handler for all boards
 *----------------------------------*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 19 )
irqreturn_t ni6601_irq_handler( int              irq,
				void *           data )
#elif LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
irqreturn_t ni6601_irq_handler( int              irq,
				void *           data,
				struct pt_regs * dummy )
#else

#define IRQ_HANDLED
#define IRQ_NONE

void ni6601_irq_handler( int              irq,
			 void *           data,
			 struct pt_regs * dummy )
#endif
{
	Board *board = data;


	if ( irq != board->irq ) {
		PDEBUG( "Invalid interrupt number: %d\n", irq );
		return IRQ_NONE;
	}

	/* With 2.6 kernels we must determine if the interrupt was meant
	   for us (it could also have been for a different device sharing
	   the interrupt line) and return a value accordingly. */

	if ( ( board->expect_tc_irq && ni6601_tc_irq_handler( board ) ) ||
	     ( board->dma_irq_enabled && ni6601_dma_irq_handler( board ) ) )
		return IRQ_HANDLED;

	return IRQ_NONE;
}


/*--------------------------------------------------------------------*
 * Handler for TC interrupts - returns 1 if the interrupt got handled
 * and 0 if the interrupt wasn't due to a TC interrupt (probably it
 * then was meant for a different device sharing the interrupt line)
 *--------------------------------------------------------------------*/

static int ni6601_tc_irq_handler( Board * board )
{
	int i;
	u16 state = Gi_INTERRUPT | Gi_TC_STATUS;
	int ret = 0;


	for ( i = 0; i < 4; i++ )
		if ( board->tc_irq_enabled[ i ] &&
		     ioread16( board->regs.status[ i ] ) & state ) {
			PDEBUG( "TC Interrupt for counter %d\n", i );
			ret |= 1;
			iowrite16( Gi_TC_INTERRUPT_ACK,
				   board->regs.irq_ack[ i ] );
			board->tc_irq_raised[ i ]++;
			wake_up_interruptible( &board->tc_waitqueue );
		}

	return ret;
}


/*---------------------------------------------------------------------*
 * Handler for DMA interrupts - returns 1 if the interrupt got handled
 * and 0 if the interrupt wasn't due to a DMS interrupt (probably it
 * then was meant for a different device sharing the interrupt line)
 *---------------------------------------------------------------------*/

static int ni6601_dma_irq_handler( Board * board )
{
	int counter = board->buf_counter;
	u16 state = ioread16( board->regs.dma_status[ counter ] );


	if ( ! ( state & DRQ_STATUS ) )
		return 0;

	/* Stop immediately if data got acquired too fast and thus could
	   not be stored correctly in the SW and HW save register */

	if ( state & DRQ_ERROR ) {
		ni6601_dma_irq_disable( board, counter );
		board->too_fast = 1;
		PDEBUG( "DMA FIFO overflow\n" );
		goto DMA_IRQ_DONE;
	}

	/* Throw away the very first data point - it's always junk since the
	   first counting interval isn't correlated with the gate */

	if ( board->is_just_started ) {
		if ( state & DRQ_READBACK )
			ioread32( board->regs.sw_save[ counter ] );
		else
			ioread32( board->regs.hw_save[ counter ] );
		board->is_just_started = 0;
		return 1;
	}
	
	/* If in continuous mode writing the next data point into the
	   buffer would lead to the high pointer becoming identical to
	   the low pointer the buffer would overflow (or, to precise, we
	   couldn't distinguish between the buffer empty and the buffer
	   being completely filled up) and we need an  emergency stop */

	if ( board->buf_continuous ) {
		if ( ( board->high_ptr + 1 == board->buf_top &&
		       board->low_ptr == board->buf ) ||
		     board->high_ptr + 1 == board->low_ptr ) {
			ni6601_dma_irq_disable( board, counter );
			board->buf_overflow = 1;
			PDEBUG( "Buffer overflow\n" );
			goto DMA_IRQ_DONE;
		}
	}

	if ( state & DRQ_READBACK )
		*( board->high_ptr++ ) =
				    ioread32( board->regs.sw_save[ counter ] );
	else
		*( board->high_ptr++ ) =
				    ioread32( board->regs.hw_save[ counter ] );

	/* If the high pointer reaches the top of the buffer in non-continuous
	   mode all data to be expected have been acquired and acquisition
	   must be stopped. In continuous mode the high pointer must wrap
	   around back to the start of the buffer */

	if ( board->high_ptr == board->buf_top ) {
		if ( ! board->buf_continuous ) {
			ni6601_dma_irq_disable( board, counter );
		} else
			board->high_ptr = board->buf;
	}

 DMA_IRQ_DONE:
	if ( board->buf_waiting )
		wake_up_interruptible( &board->dma_waitqueue );

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
