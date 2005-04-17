/*
 *  $Id$
 * 
 *  Driver for National Instruments DAQ boards based on a DAQ-STC
 * 
 *  Copyright (C) 2003-2005 Jens Thoms Toerring
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
 *  To contact the author send email to
 *  Jens.Toerring@physik.fu-berlin.de
 */


#include "ni_daq_board.h"


static void AI_daq_reset( Board *board );
static int AI_channel_setup( Board *board, unsigned int num_channels,
			     NI_DAQ_AI_CHANNEL_ARGS *channel_args );
static int AI_clock_setup( Board *board, NI_DAQ_CLOCK_SPEED_VALUE speed );
static int AI_acq_setup( Board *board, NI_DAQ_ACQ_SETUP *acq_args );
static int AI_START_STOP_setup( Board *board, NI_DAQ_ACQ_SETUP *a );
static int AI_SI_setup( Board *board, NI_DAQ_ACQ_SETUP *a );
static int AI_START1_setup( Board *board, NI_DAQ_ACQ_SETUP *a );
static int AI_CONVERT_setup( Board *board, NI_DAQ_ACQ_SETUP *a );
static int AI_SI2_setup( Board *board, NI_DAQ_ACQ_SETUP *a );
static int AI_SC_setup( Board *board, NI_DAQ_ACQ_SETUP *a );
static void AI_acq_register_setup( Board *board );
static int AI_acq_wait( Board *board );
static int AI_acq_stop( Board *board );


static struct {
	u16 start_stop; 
	u16 mode_1;
	u16 mode_2;
	u16 mode_3;
	u16 trig;
	int need_si_load;
	u32 si_load_a;
	u32 si_load_b;
	int need_si2_load;
	u16 si2_load_a;
	u16 si2_load_b;
} acq_setup;


#if defined NI_DAQ_DEBUG
#define stc_writew( a, b, c ) stc_writew( __FUNCTION__, a, b, c )
#define stc_writel( a, b, c ) stc_writel( __FUNCTION__, a, b, c )
#endif


/*----------------------------------------------------------*
 * Function resets the AI subsystem back into a known state
 *----------------------------------------------------------*/

void AI_reset_all( Board *board )
{
	u16 data;


	/* Make sure nothing can get in between */

	board->func->start_critical_section( board );

	/* Set flag indicating to the card that a configuration is done */

	data = board->stc.Joint_Reset | AI_Configuration_Start;
	board->func->stc_writew( board, STC_Joint_Reset, data );
	board->stc.Joint_Reset &= ~ AI_Configuration_Start;

	/* Do a reset */

	AI_daq_reset( board );

	/* Configuration is done, set the bit indicating this to the card */

	data = board->stc.Joint_Reset | AI_Configuration_End;
	board->func->stc_writew( board, STC_Joint_Reset, data );
	board->stc.Joint_Reset &= ~ AI_Configuration_End;

	/* Switch off DMA and free DMA buffers */

	board->func->dma_shutdown( board, NI_DAQ_AI_SUBSYSTEM );

	/* Reset internal variables */

	board->AI.num_channels = 0;
	board->AI.num_data_per_scan = 0;
	board->AI.is_acq_setup = 0;
	board->AI.num_scans = 0;
	board->AI.is_running = 0;

	board->AI.START_source = NI_DAQ_NONE;
	board->AI.START1_source = NI_DAQ_NONE;
	board->AI.CONVERT_source = NI_DAQ_NONE;
	board->AI.SI_source = NI_DAQ_NONE;

	/* Finally set the IN_TIMEBASE1 to the maximum speed */

	board->stc.Clock_and_FOUT &= ~ AI_Source_Divide_By_2;
	board->AI.timebase1 = 50;

	board->func->stc_writew( board, STC_Clock_and_FOUT,
				 board->stc.Clock_and_FOUT );

	board->func->end_critical_section( board );

	/* Make sure the interrupts that get raised at the end of a
	   conversion and a whole acquisition are disabled */

	daq_irq_disable( board, IRQ_AI_SC_TC );
	daq_irq_disable( board, IRQ_AI_STOP );

	board->AI.num_channels = 0;
	board->AI.num_data_per_scan = 0;
	board->AI.is_acq_setup = 0;
}


/*------------------------------------*
 * Function for resetting the STC-DAQ
 *------------------------------------*/

static void AI_daq_reset( Board *board )
{
	u16 data;


	data = board->stc.Joint_Reset | AI_Reset;

	board->func->stc_writew( board, STC_Joint_Reset, AI_Reset );
	board->stc.Joint_Reset &= ~ AI_Reset;
	/* Disable interrupts by the AI subsystem */

	data = board->stc.Interrupt_A_Enable &
	       ~ ( AI_FIFO_Interrupt_Enable   |
		   AI_Error_Interrupt_Enable  |
		   AI_STOP_Interrupt_Enable   |
		   AI_START_Interrupt_Enable  |
		   AI_START1_Interrupt_Enable |
		   AI_START2_Interrupt_Enable |
		   AI_SC_TC_Interrupt_Enable );

	board->func->stc_writew( board, STC_Interrupt_A_Enable, data );

	data = board->stc.Second_Irq_A_Enable &
	       ~ ( AI_FIFO_Second_Irq_Enable   |
		   AI_Error_Second_Irq_Enable  |
		   AI_STOP_Second_Irq_Enable   |
		   AI_START_Second_Irq_Enable  |
		   AI_START1_Second_Irq_Enable |
		   AI_START2_Second_Irq_Enable |
		   AI_SC_TC_Second_Irq_Enable );

	board->func->stc_writew( board, STC_Second_Irq_A_Enable, data );

	/* Set all interrupt acknowledge flags (thereby clearing the
	   condition) */

	data = board->stc.Interrupt_A_Ack |
	       AI_Error_Interrupt_Ack     |
	       AI_STOP_Interrupt_Ack      |
	       AI_START_Interrupt_Ack     |
	       AI_START1_Interrupt_Ack    |
	       AI_START2_Interrupt_Ack    |
	       AI_SC_TC_Interrupt_Ack     |
	       AI_SC_TC_Error_Confirm;

	board->func->stc_writew( board, STC_Interrupt_A_Ack, data );

	board->stc.Interrupt_A_Ack &= ~ ( AI_Error_Interrupt_Ack  |
					  AI_STOP_Interrupt_Ack   |
					  AI_START_Interrupt_Ack  |
					  AI_START1_Interrupt_Ack |
					  AI_START2_Interrupt_Ack |
					  AI_SC_TC_Interrupt_Ack  |
					  AI_SC_TC_Error_Confirm    );

	/* Clear all private copies of registers that have been reset by
	   the above actions */

	board->stc.AI_Command_1         = 0;
	board->stc.AI_Command_2         = 0;
	board->stc.AI_Mode_1            = 0;
	board->stc.AI_Mode_2            = 0;
	board->stc.AI_Mode_3            = 0;
	board->stc.AI_Output_Control    = 0;
	board->stc.AI_Personal          = 0;
	board->stc.AI_START_STOP_Select = 0;
	board->stc.AI_Trigger_Select    = 0;

	/* The AI_Start_Stop and Reserved_One bits in AI_Mode_1_Register
	   should normally be set and also the AI_Trigger_Once (unless
	   continuous acquisition is used) */

	data = AI_Start_Stop | Reserved_One | AI_Trigger_Once;
	board->func->stc_writew( board, STC_AI_Mode_1, data );

	board->func->stc_writew( board, STC_AI_Mode_2, 0 );

	/* Set up the Personal and Output Control Register */

	board->func->stc_writew( board, STC_AI_Personal,
				 AI_SHIFTIN_Pulse_Width | AI_SOC_Polarity |
				 AI_CONVERT_Pulse_Width | AI_Overrun_Mode |
				 AI_LOCALMUX_CLK_Pulse_Width );

	board->func->stc_writew( board, STC_AI_Output_Control,
				 ( 3 << AI_SCAN_IN_PROG_Output_Select_Shift ) |
				 ( 2 << AI_LOCALMUX_CLK_Output_Select_Shift ) |
				 ( 3 << AI_SC_TC_Output_Select_Shift ) |
				 ( 2 << AI_CONVERT_Output_Select_Shift ) );
}


/*----------------------------------------------------------*
 * Function for handling ioctl() calls for the AI subsystem
 *----------------------------------------------------------*/

int AI_ioctl_handler( Board *board, NI_DAQ_AI_ARG *arg )
{
	NI_DAQ_AI_ARG a;
	int ret = 0;


	if ( copy_from_user( &a, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	switch ( a.cmd ) {
		case NI_DAQ_AI_SET_CLOCK_SPEED :
			ret = AI_clock_setup( board, a.speed );
			break;

		case NI_DAQ_AI_GET_CLOCK_SPEED :
			a.speed = ( board->stc.Clock_and_FOUT &
				    AI_Source_Divide_By_2 ) ?
				   NI_DAQ_HALF_SPEED : NI_DAQ_FULL_SPEED;
			break;

		case NI_DAQ_AI_CHANNEL_SETUP :
			ret = AI_channel_setup( board, a.num_channels,
						a.channel_args );
			if ( ret == 0 )
				ret = board->AI.num_data_per_scan;
			break;

		case NI_DAQ_AI_ACQ_SETUP :
			ret = AI_acq_setup( board, a.acq_args );
			a.num_points =   board->AI.num_data_per_scan
				       * board->AI.num_scans;
			break;

		case NI_DAQ_AI_ACQ_START :
			ret = AI_start_acq( board );
			break;

		case NI_DAQ_AI_ACQ_WAIT :
			ret = AI_acq_wait( board );
			break;

		case NI_DAQ_AI_ACQ_STOP :
			ret = AI_acq_stop( board );
			break;

		default :
			PDEBUG( "Invalid AI command %d\n", a.cmd );
			return -EINVAL;
	}

	if ( ret >= 0 && copy_to_user( arg, &a, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EACCES;
	}

	return ret;
}


/*---------------------------------------------------------------*
 * Function for setting the AI_IN_TIMEBASE1 - it can be switched
 * between 20 MHz and 10 MHz (please note that the time base for
 * IN_TIMEBASE2 is controlled by the general setting of the MSC
 * subsystem!).
 *---------------------------------------------------------------*/

static int AI_clock_setup( Board *board, NI_DAQ_CLOCK_SPEED_VALUE speed )
{
	if ( board->AI.is_running )
		return -EBUSY;

	if ( speed == NI_DAQ_FULL_SPEED ) {
		board->AI.timebase1 = 50;
		board->stc.Clock_and_FOUT &= ~ AI_Source_Divide_By_2;
	} else {
		board->AI.timebase1 = 100;
		board->stc.Clock_and_FOUT |= AI_Source_Divide_By_2;
	}

	board->func->stc_writew( board, STC_Clock_and_FOUT,
				 board->stc.Clock_and_FOUT );

	return 0;
}


/*-----------------------------------------*
 * Function sets up the Configuration FIFO
 *-----------------------------------------*/

static int AI_channel_setup( Board *board, unsigned int num_channels,
			     NI_DAQ_AI_CHANNEL_ARGS *channel_args )
{
	int ret;
	unsigned int i;
	NI_DAQ_AI_CHANNEL_ARGS a;
	unsigned int num_data = 0;
	NI_DAQ_AI_TYPE ch_types[ board->type->ai_num_channels ];


	if ( board->AI.is_running )
		return -EBUSY;

	board->AI.is_acq_setup = 0;

	if ( num_channels == 0 ||
	     num_channels > board->type->ai_num_channels ) {
		PDEBUG( "Invalid number of channels\n" );
		return -EINVAL;
	}

	for ( i = 0; i < board->type->ai_num_channels; i++ )
		ch_types[ i ] = NI_DAQ_AI_TYPE_UNASSIGNED;

	/* Clear the Configuration memory */

	board->func->clear_configuration_memory( board );

	/* Set up the high and low part of the configuration register with the
	   channel parameter. A non-negative return value from the function
	   for setting the Configuration Memory High Register tells if a data
	   point is going to be written to the FIFO for the channel during a
	   scan (not all channels necessarily deliver a data point). */

	board->AI.num_channels = 0;
	board->AI.num_data_per_scan = 0;

	for ( i = 0; i < num_channels; i++ ) {
		if ( copy_from_user( &a, channel_args + i,
				     sizeof *channel_args ) ) {
			PDEBUG( "Can't read from user space\n" );
			board->func->clear_configuration_memory( board );
			return -EACCES;
		}

		if ( channel_args->channel >= board->type->ai_num_channels ) {
			PDEBUG( "Invalid channel number\n" );
			board->func->clear_configuration_memory( board );
			return -EINVAL;
		}

		/* Remember the type of coupling used for the channel */

		ch_types[ channel_args->channel ] = channel_args->channel_type;

		if ( ( ret = board->func->configuration_high( board, a.channel,
						     a.channel_type ) ) < 0 ) {
			board->func->clear_configuration_memory( board );
			return ret;
		}
		num_data += ret;

		if ( ( ret = board->func->configuration_low( board,
						 i == num_channels - 1,
						 a.generate_trigger,
						 a.dither_enable,
						 a.polarity, a.gain ) ) < 0 ) {
			board->func->clear_configuration_memory( board );
			return ret;
		}
	}

	/* Check that the channel associated with a channel with differential
	   coupling is not used - channels with differential coupling can only
	   be in the ranges i = 0..7,16..23,32..39,48..55 and the channel
	   associated to it has to have the number (i + 8) */

	for ( i = 0; i < board->type->ai_num_channels; i++ ) {
		if ( ch_types[ i ] == NI_DAQ_AI_TYPE_UNASSIGNED )
			continue;

		if ( i % 16 < 8 &&
		     ch_types[ i ] == NI_DAQ_AI_TYPE_Differential &&
		     ch_types[ i  + 8 ] != NI_DAQ_AI_TYPE_UNASSIGNED ) {
			PDEBUG( "Channel belonging to channel %u with "
				"differential coupling isn't unused\n", i );
			board->func->clear_configuration_memory( board );
			return -EINVAL;
		}

		if ( i % 16 >= 8 &&
		     ch_types[ i ] == NI_DAQ_AI_TYPE_Differential ) {
			PDEBUG( "Channel %u can't be of type "
				"\"Differential\"\n", i );
			board->func->clear_configuration_memory( board );
			return -EINVAL;
		}
	}

	board->func->stc_writew( board, STC_AI_Command_1, AI_CONVERT_Pulse );
	board->stc.AI_Command_1 &= ~ AI_CONVERT_Pulse;
	board->func->clear_ADC_FIFO( board );

	/* Store how many channels there are in a scan and how many data
	   points are to be expected from a scan */

	board->AI.num_channels = num_channels;
	board->AI.num_data_per_scan = num_data;

	return 0;
}


/*---------------------------------------------------------------------*
 * Function for setting up all triggers and timings for an acquisition
 *---------------------------------------------------------------------*/

static int AI_acq_setup( Board *board, NI_DAQ_ACQ_SETUP *acq_args )
{
	NI_DAQ_ACQ_SETUP a;
	unsigned int i;
	int ret = 0;
	int ( * ts[ ] ) ( Board *, NI_DAQ_ACQ_SETUP * ) =
			{ AI_SC_setup, AI_START_STOP_setup, AI_SI_setup,
			  AI_START1_setup, AI_CONVERT_setup, AI_SI2_setup };


	if ( board->AI.is_running )
		return -EBUSY;

	if ( board->AI.num_channels == 0 ) {
		PDEBUG( "Missing channel setup\n" );
		return -EINVAL;
	}

	if ( copy_from_user( &a, acq_args, sizeof *acq_args ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	acq_setup.start_stop    = AI_START_Sync | AI_START_Edge;
	acq_setup.mode_1        = 0;
	acq_setup.mode_2        = 0;
	acq_setup.mode_3        = 0;
	acq_setup.trig          = AI_START1_Sync | AI_START1_Edge;
	acq_setup.need_si_load  = 0;
	acq_setup.si_load_a     = 0;
	acq_setup.si_load_b     = 0;
	acq_setup.need_si2_load = 0;
	acq_setup.si2_load_a    = 0;
	acq_setup.si2_load_b    = 0;

	/* Call all the different setup functions */

	for ( i = 0; i < sizeof ts / sizeof ts[ 0 ]; i++ )
		if ( ( ret = ts[ i ]( board, &a ) ) < 0 )
			break;

	if ( ret >= 0 )
		board->AI.is_acq_setup = 1;

	return ret;
}


/*------------------------------------------------*
 * Function for setting the number of scans to be
 * done during an acquisition.
 *------------------------------------------------*/

static int AI_SC_setup( Board *board, NI_DAQ_ACQ_SETUP *a )
{
	if ( a->num_scans == 0 ) {
		PDEBUG( "Number of scans too small\n" );
		return -EINVAL;
	}

	if ( a->num_scans > 0x1000000 ) {
		PDEBUG( "Number of scans too large\n" );
		return -EINVAL;
	}

	/* Setting up the SC counter is only done when the acquisition is
	   started, here only store the number of required acquisition */

	board->AI.num_scans = a->num_scans;

	return 0;
}


/*------------------------------------------------------------*
 * Function for mainly setting up what is going to raise the
 * START signal (the STOP signal is always going to come from
 * the Configuration FIFO).
 *------------------------------------------------------------*/

static int AI_START_STOP_setup( Board *board, NI_DAQ_ACQ_SETUP *a )
{
	/* Check the START source setting isn't bogus */

	if ( a->START_source > NI_DAQ_GOUT_0 &&
	     a->START_source != NI_DAQ_LOW ) {
		PDEBUG( "Invalid START source\n" );
		return -EINVAL;
	}

	/* Set the START source */

	board->AI.START_source = a->START_source;
	acq_setup.start_stop |= a->START_source << AI_START_Select_Shift;

	/* Set the START polarity (unless it's the internal SI_TC source
	   or a software strobe) */

	if ( a->START_polarity == NI_DAQ_INVERTED &&
	     a->START_source != NI_DAQ_SI_TC &&
	     a->START_source != NI_DAQ_AI_START_Pulse )
		acq_setup.start_stop |= AI_START_Polarity;

	/* The STOP pulse is always coming from the Configuration FIFO for
	   the entry that has the LastChan bit set */

	acq_setup.start_stop |= 19 << AI_STOP_Select_Shift;

	return 0;
}


/*-----------------------------------------------------------------*
 * Function for setting up the SI counter when the START signal is
 * supposed to come from the TC event of the SI counter.
 *-----------------------------------------------------------------*/

static int AI_SI_setup( Board *board, NI_DAQ_ACQ_SETUP *a )
{
	unsigned long len = 0;


	/* When the START trigger comes from an external signal (or is a
	   software strobe) the SI counter is not needed */

	if ( a->START_source != NI_DAQ_SI_TC )
		return 0;

	acq_setup.need_si_load = 1;

	/* Check that what the SI counter is going to count is reasonable */

	if ( a->SI_source > NI_DAQ_RTSI_6 &&
	     a->SI_source != NI_DAQ_IN_TIMEBASE2 &&
	     a->SI_source != NI_DAQ_LOW ) {
		PDEBUG( "Invalid SI source\n" );
		return -EINVAL;
	}

	/* Check the counter start values, they must be between 2 and
	   0x1000000 (SC is a 24 bit counter) */

	if ( a->SI_start_delay < 2 ) {
		PDEBUG( "SI start delay too short\n" );
		return -EINVAL;
	}

	if ( a->SI_start_delay > 0x1000000 ) {
		PDEBUG( "SI start delay too large\n" );
		return -EINVAL;
	}

	if ( a->SI_stepping < 2 ) {
		PDEBUG( "SI stepping too short\n" );
		return -EINVAL;
	}

	if ( a->SI_stepping > 0x1000000 ) {
		PDEBUG( "SI stepping too large\n" );
		return -EINVAL;
	}

	/* Check that when the SI counter is fed from one of the internal
	   time bases the spacing between the resulting START pulses isn't
	   too fast for the board */

	if ( a->SI_source == NI_DAQ_AI_IN_TIMEBASE1 ||
	     a->SI_source == NI_DAQ_IN_TIMEBASE2 ) {
		len =   ( a->SI_source == NI_DAQ_AI_IN_TIMEBASE1 ?
			  board->AI.timebase1 : board->timebase2 )
		      * ( a->SI_start_delay
			  + board->AI.num_channels * a->SI_stepping );

		if ( len < board->type->ai_speed ) {
			PDEBUG( "Scan repetition rate too high\n" );
			return -EINVAL;
		}
	}

	/* Set the SI source and polarity (unless the source is internal).
	   Take care, there's some irregularity here, IN_TIMEBASE2 has to
	   be treated separately because the value representing IN_TIMEBASE2
	   is different when used for different purposes */

	board->AI.SI_source = a->SI_source;

	if ( a->SI_source == NI_DAQ_IN_TIMEBASE2 )
		acq_setup.mode_1 |= 19 << AI_SI_Source_Select_Shift;
	else
		acq_setup.mode_1 |= a->SI_source << AI_SI_Source_Select_Shift;

	/* If what the counter is going to count is some external signal
	   set the polarity according to what the user told us */

	if ( a->SI_polarity == NI_DAQ_INVERTED &&
	     a->SI_source != NI_DAQ_AI_IN_TIMEBASE1 &&
	     a->SI_source != NI_DAQ_IN_TIMEBASE2 )
		acq_setup.mode_1 |= AI_SI_Source_Polarity;

	/* The start delay (i.e. the number of counts before the first SI TC
	   event is reached, which raises the first START signal) gets loaded
	   into the SI Load A register and the count for delays between scans
	   into SI Load B Register */

	acq_setup.si_load_a = a->SI_start_delay - 1;
	acq_setup.si_load_b = a->SI_stepping - 1;

	/* Make all loads except the very first one come from the A register,
	   i.e. the one with the time delay between START signals */

	acq_setup.mode_2 |= 6 << AI_SI_Reload_Mode_Shift;

	return 0;
}


/*------------------------------------------------------------*
 * Function for setting the source of the START1 signal which
 * starts an acquisition.
 *------------------------------------------------------------*/

static int AI_START1_setup( Board *board, NI_DAQ_ACQ_SETUP *a )
{
	/* Check that the source for START1 is reasonable */

	if (  a->START1_source > NI_DAQ_RTSI_6 &&
	      a->START1_source != NI_DAQ_GOUT_0 &&
	      a->START1_source != NI_DAQ_LOW ) {
		PDEBUG( "Invalid START1 source\n ");
		return -EINVAL;
	}

	/* Everywhere else NI_DAQ_GOUT_0 is 19, but for AI_START1_Select
	   it's 18... */

	if ( a->START1_source == NI_DAQ_GOUT_0 )
		a->START1_source -= 1;

	board->AI.START1_source = a->START1_source;

	acq_setup.trig |= a->START1_source << AI_START1_Select_Shift;

	/* If the START1 trigger isn't a software strobe the user specified
	   polarity has to be used */

	if ( a->START1_polarity == NI_DAQ_INVERTED &&
	     a->START1_source != NI_DAQ_AI_START1_Pulse )
		acq_setup.trig |= AI_START1_Polarity;

	return 0;
}


/*--------------------------------------------------------------*
 * Function for setting what's going to creates CONVERT signals
 *--------------------------------------------------------------*/

static int AI_CONVERT_setup( Board *board, NI_DAQ_ACQ_SETUP *a )
{
	/* Check that the source of CONVERT signals is reasonable */

	if ( a->CONVERT_source > NI_DAQ_RTSI_6 &&
	     a->CONVERT_source != NI_DAQ_GOUT_0 &&
	     a->CONVERT_source != NI_DAQ_LOW ) {
		PDEBUG( "Invalid CONVERT source\n" );
		return -EINVAL;
	}

	/* When the DAQ-STC is in internal CONVERT mode (it's source are TC
	   events of the SI2 counter) start/stop gates must be disabled,
	   otherwise enabled */

	if ( a->CONVERT_source != NI_DAQ_SI2_TC )
		acq_setup.mode_2 |= AI_Start_Stop_Gate_Enable;

	/* If not in internal CONVERT mode set the the polarity of the
	   signal to what the user told us. Then also set the source. */

	if ( a->CONVERT_source != NI_DAQ_SI2_TC &&
	     a->CONVERT_polarity == NI_DAQ_INVERTED )
		acq_setup.mode_1 |= AI_CONVERT_Source_Polarity;

	board->AI.CONVERT_source = a->CONVERT_source;

	acq_setup.mode_1 |=
			   a->CONVERT_source << AI_CONVERT_Source_Select_Shift;

	return 0;
}


/*-----------------------------------------------------------------*
 * Function for setting up the SI2 counter when the CONVERT signal
 * is supposed to come from the TC event of the SI2 counter.
 *-----------------------------------------------------------------*/

static int AI_SI2_setup( Board *board, NI_DAQ_ACQ_SETUP *a )
{
	/* If the CONVERT trigger does not come from the SI2_TC event the
	   SI2 counter does not need to be set up */

	if ( a->CONVERT_source != NI_DAQ_SI2_TC )
		return 0;

	acq_setup.need_si2_load = 1;

	/* Check that the setting for the delay between the START and the
	   first CONVERT signal as well as the spacing between further
	   CONVERT signals in a scan are reasonable. In principle, the
	   values can be between 2 and 0x10000 (the SI2 counter is a
	   16-bit counter), but the time difference between CONVERT pulses
	   may never be lower than the fastest rate of the card (actually,
	   the card accepts lower values but then hell breaks loose...) */

	if ( a->SI2_start_delay < 2 ) {
		PDEBUG( "CONVERT start too short\n" );
		return -EINVAL;
	}

	if ( a->SI2_stepping * 50 < board->type->ai_speed ) {
		PDEBUG( "CONVERT stepping too fast\n" );
		return -EINVAL;
	}

	if ( a->SI2_start_delay > 0x10000 ) {
		PDEBUG( "CONVERT start too long\n" );
		return -EINVAL;
	}

	if ( a->SI2_stepping > 0x10000 ) {
		PDEBUG( "CONVERT stepping too slow\n" );
		return -EINVAL;
	}

	/* What the SI2 counter counts is either the same as the SI
	   counter or the AI subsystems (fast) time base */

	if ( a->SI2_source != NI_DAQ_SAME_AS_SI )
		acq_setup.mode_3 |= AI_SI2_Source_Select;

	/* The start delay gets loaded into the SI2_Load_A Register and the
	   spacing between CONVERT signals into SI2_Load_B (as usual after
	   subtracting 1) */

	acq_setup.si2_load_a = a->SI2_start_delay - 1;
	acq_setup.si2_load_b = a->SI2_stepping - 1;

	return 0;
}


/*----------------------------------------------------------------------*
 * Function for setting up all registers according to what had been set
 * during the acquisition setup just before an acquisition is started
 *----------------------------------------------------------------------*/

static void AI_acq_register_setup( Board *board )
{
	AI_daq_reset( board );

	acq_setup.start_stop |= board->stc.AI_START_STOP_Select;
	acq_setup.mode_1     |= board->stc.AI_Mode_1;
	acq_setup.mode_2     |= board->stc.AI_Mode_2;
	acq_setup.mode_3     |= board->stc.AI_Mode_3;
	acq_setup.trig       |= board->stc.AI_Trigger_Select;

	/* First of all disable the START1 signal for the time being - it
	   will be enabled again when an acquisition is to be started */

	board->func->stc_writew( board, STC_AI_Command_2,
				 board->stc.AI_Command_2 | AI_START1_Disable );

	board->func->stc_writew( board, STC_AI_START_STOP_Select,
				 acq_setup.start_stop );
	board->func->stc_writew( board, STC_AI_Trigger_Select,
				 acq_setup.trig );

	board->func->stc_writew( board, STC_AI_Mode_1, acq_setup.mode_1 );
	board->func->stc_writew( board, STC_AI_Mode_2, acq_setup.mode_2 );
	board->func->stc_writew( board, STC_AI_Mode_3, acq_setup.mode_3 );

	if ( acq_setup.need_si_load ) {

		/* The AI_SI_Load_A register is set to the time difference
		   between scans, AI_SI_Load_B contains the time between the
		   start of the acquisition and the first scan */

		board->func->stc_writel( board, STC_AI_SI_Load_A,
					 acq_setup.si_load_a );
		board->func->stc_writel( board, STC_AI_SI_Load_B,
					 acq_setup.si_load_b );

		board->func->stc_writew( board, STC_AI_Mode_2,
					 board->stc.AI_Mode_2 &
					 ~ AI_SI_Initial_Load_Source );

		board->func->stc_writew( board, STC_AI_Command_1,
					 board->stc.AI_Command_1 |
					 AI_SI_Load );
		board->stc.AI_Command_1 &= ~ AI_SI_Load;

		board->func->stc_writew( board, STC_AI_Mode_2,
					 board->stc.AI_Mode_2 |
					 AI_SI_Initial_Load_Source );
	}

	if ( acq_setup.need_si2_load ) {

		/* The AI_SI2_Load_A register contains the time difference
		   between conversions, AI_SI2_Load_B the time between the
		   start of the scan and the first conversions */

		board->func->stc_writew( board, STC_AI_SI2_Load_A,
					 acq_setup.si2_load_a );
		board->func->stc_writew( board, STC_AI_SI2_Load_B,
					 acq_setup.si2_load_b );

		board->func->stc_writew( board, STC_AI_Mode_2,
					 ( board->stc.AI_Mode_2 |
					   AI_SI2_Reload_Mode ) &
					 ~ AI_SI2_Initial_Load_Source );

		board->func->stc_writew( board, STC_AI_Command_1,
					 board->stc.AI_Command_1 |
					 AI_SI2_Load );
		board->stc.AI_Command_1 &= ~ AI_SI2_Load;

		board->func->stc_writew( board, STC_AI_Mode_2,
					 board->stc.AI_Mode_2 |
					 AI_SI2_Reload_Mode |
					 AI_SI2_Initial_Load_Source );
	}
}


/*---------------------------------------------------------------*
 * Function to start an acquisition after the channels have been
 * configured and the triggers and timings etc. have been set up
 * - it arms all counters needed according to the acquisition
 * setup and, if the acquisition isn't to be initiated by an
 * external trigger, raises the START1 signal. It also allocates
 * memory for the data, initializes the DMA hardware and enables
 * the SC TC interrupt, indicating the end of the acquisition.
 *---------------------------------------------------------------*/

int AI_start_acq( Board *board )
{
	int ret;
	u16 data;
	u16 cmd_1;
	u16 cmd_2;


	if ( ! board->AI.is_acq_setup ) {
		PDEBUG( "Missing acquisition setup\n" );
		return -EINVAL;
	}

	/* Don't try to restart an already running acquisition */

	if ( board->AI.is_running )
		return 0;

	/* Switch PFI pins that are required for external signals into
	   input mode */

	if ( MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM, board->AI.START_source,
			    NI_DAQ_PFI_INPUT ) ) {
		MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM, NI_DAQ_ALL,
			       NI_DAQ_PFI_UNUSED );
		PDEBUG( "Can't get START signal from PFI%d pin\n",
			board->AI.START_source - NI_DAQ_PFI0 );
		return -EINVAL;
	}

	if ( MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM,
			    board->AI.START1_source, NI_DAQ_PFI_INPUT ) ) {
		MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM, NI_DAQ_ALL,
			       NI_DAQ_PFI_UNUSED );
		PDEBUG( "Can't get START1 signal from PFI%d pin\n",
			board->AI.START1_source - NI_DAQ_PFI0 );
		return -EINVAL;
	}

	if ( MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM,
			    board->AI.CONVERT_source, NI_DAQ_PFI_INPUT ) ) {
		MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM, NI_DAQ_ALL,
			       NI_DAQ_PFI_UNUSED );
		PDEBUG( "Can't get CONVERT signal from PFI%d pin\n",
			board->AI.CONVERT_source - NI_DAQ_PFI0 );
		return -EINVAL;
	}

	if ( MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM,
			    board->AI.SI_source, NI_DAQ_PFI_INPUT ) ) {
		MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM, NI_DAQ_ALL,
			       NI_DAQ_PFI_UNUSED );
		PDEBUG( "Can't get SI input from PFI%d pin\n",
			board->AI.SI_source - NI_DAQ_PFI0 );
		return -EINVAL;
	}

	/* Get a DMA buffer big enough to hold the data to be expected */

	if ( ( ret = board->func->dma_buf_setup( board, NI_DAQ_AI_SUBSYSTEM,
					     board->AI.num_data_per_scan
					     * board->AI.num_scans, 0 ) ) < 0 )
		return ret;

	/* Enable DMA */

	if ( ( ret = board->func->dma_setup( board,
					     NI_DAQ_AI_SUBSYSTEM ) ) < 0 )
		return ret;
		       
	/* Set up all registers, then arm the required counters */

	board->func->start_critical_section( board );
	data = board->stc.Joint_Reset | AI_Configuration_Start;
	board->func->stc_writew( board, STC_Joint_Reset, data );
	board->stc.Joint_Reset &= ~ AI_Configuration_Start;

	AI_acq_register_setup( board );

	cmd_1 = ( board->stc.AI_Command_1 | AI_SC_Arm ) & ~ AI_DIV_Arm;
	cmd_2 = board->stc.AI_Command_2 &
		~ ( AI_START1_Disable | AI_End_On_End_Of_Scan );

	/* Set the number of scans - that's done here and not in AI_SC_setup()
	   because when an acquisition is restarted without another setup the
	   SC counter would still hold the end value from the previous run */

	board->func->stc_writew( board, STC_AI_Mode_2,
				 board->stc.AI_Mode_2 &
				 ~ ( AI_SC_Initial_Load_Source |
				     AI_SC_Reload_Mode  |
				     AI_Pre_Trigger ) );

	/* Write the number of scans (minus 1) to the the Load A register
	   and then push this value into the SC counter */

	board->func->stc_writel( board, STC_AI_SC_Load_A,
				 ( u32 ) ( board->AI.num_scans - 1 ) );

	board->func->stc_writew( board, STC_AI_Command_1,
				 board->stc.AI_Command_1 | AI_SC_Load );
	board->stc.AI_Command_1 &= ~ AI_SC_Load;

	/* Enable interrupt on the SC TC, i.e. after the last scan. This
	   needs to be done before the counters are armed */

	daq_irq_enable( board, IRQ_AI_SC_TC, AI_SC_irq_handler );

	/* If internal START mode is used arm the SI counter */

	if ( board->AI.START_source == NI_DAQ_SI_TC )
		cmd_1 |= AI_SI_Arm;

	/* If internal CONVERT mode is used also arm the SI2 counter */

	if ( board->AI.CONVERT_source == NI_DAQ_SI2_TC )
		cmd_1 |= AI_SI2_Arm;

	board->func->stc_writew( board, STC_AI_Command_1, cmd_1 );

	board->stc.AI_Command_1 &= ~ ( AI_SC_Arm | AI_SI_Arm | AI_SI2_Arm );

	/* Now set the condition when to stop */

	cmd_2 |= AI_End_On_SC_TC;
	board->func->stc_writew( board, STC_AI_Command_2, cmd_2 );
	board->stc.AI_Command_2 &= ~ AI_End_On_SC_TC;

	data = board->stc.Joint_Reset | AI_Configuration_End;
	board->func->stc_writew( board, STC_Joint_Reset, data );
	board->stc.Joint_Reset &= ~ AI_Configuration_End;
	board->func->end_critical_section( board );

	/* Raise the START1 signal if acquisition is to be started by an
	   software strobe */

	board->AI.is_running = 1;

	if ( board->AI.START1_source == NI_DAQ_AI_START1_Pulse ) {
		board->func->stc_writew( board, STC_AI_Command_2,
					 board->stc.AI_Command_2 |
					 AI_START1_Pulse );
		board->stc.AI_Command_2 &= ~ AI_START1_Pulse;
	}

	return 0;
}


/*---------------------------------------------------------*
 * Function to wait for the end of an acquisition. Returns
 * -EIO immediately if no acquisition is running, -EINTR
 * if a signal is received and 0 otherwise.
 *---------------------------------------------------------*/

static int AI_acq_wait( Board *board )
{
	if ( board->mite_chain[ NI_DAQ_AI_SUBSYSTEM ] == NULL )
		return -EIO;

	/* Wait for the SC TC interrupt, indicating end of acquisition */

	if ( board->AI.is_running &&
	     wait_event_interruptible( board->AI.waitqueue,
				   board->irq_hand[ IRQ_AI_SC_TC ].raised ) ) {
		PDEBUG( "Aborted by signal\n" );
		return -EINTR;
	}

	/* Disable the interrupt and release possibly used trigger input
	   lines */

	daq_irq_disable( board, IRQ_AI_SC_TC );
	MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM, NI_DAQ_ALL,
		       NI_DAQ_PFI_UNUSED );

	return 0;
}


/*---------------------------------------*
 * Function for stopping an acquisition.
 *---------------------------------------*/

static int AI_acq_stop( Board *board )
{
	u16 data;


	if ( board->mite_chain[ NI_DAQ_AI_SUBSYSTEM ] == NULL )
		return 0;

	if ( board->AI.is_running ) {
		board->func->start_critical_section( board );
		data = board->stc.Joint_Reset | AI_Configuration_Start;
		board->func->stc_writew( board, STC_Joint_Reset, data );
		board->stc.Joint_Reset &= ~ AI_Configuration_Start;

		AI_daq_reset( board );

		data = board->stc.Joint_Reset | AI_Configuration_End;
		board->func->stc_writew( board, STC_Joint_Reset, data );
		board->stc.Joint_Reset &= ~ AI_Configuration_End;

		board->AI.is_running = 0;
	}

	daq_irq_disable( board, IRQ_AI_SC_TC );
	daq_irq_disable( board, IRQ_AI_STOP );
	MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM, NI_DAQ_ALL,
		       NI_DAQ_PFI_UNUSED );

	board->func->dma_shutdown( board, NI_DAQ_AI_SUBSYSTEM );

	return 0;
}


/*-------------------------------------------------------------*
 * Handler for interrupts raised by the AI subsystem - it just
 * wakes up processes sleeping on the associated wait queue.
 *-------------------------------------------------------------*/

void AI_irq_handler( Board *board )
{
	PDEBUG( "AI_interrupt\n" );
	wake_up_interruptible( &board->AI.waitqueue );
}


/*--------------------------------------------------------------------*
 * Handler for the SC TC event that gets raised on end of acquisition
 * Processes sleeping on the associated wait queue are woken.
 *--------------------------------------------------------------------*/

void AI_SC_irq_handler( Board *board )
{
	PDEBUG( "AI SC TC interrupt\n" );
	board->AI.is_running = 0;
	wake_up_interruptible( &board->AI.waitqueue );
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
