/*
  $Id$
 
  Library for National Instruments DAQ boards based on a DAQ-STC

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


#include "ni_daq_lib.h"


static int ni_daq_gpct_check_source( int source );
static int ni_daq_gpct_state( int board, int counter, int *state );
static int ni_daq_gpct_is_armed( int board, int counter, int *state );
static int ni_daq_gpct_time_to_ticks( int board, int counter,
									  double gate_length, unsigned long *ticks,
									  int *source );



/*--------------------------------------------------------------------*/
/* Function for switching the G_IN_TIMEBASE1 (i.e. the fast timebase) */
/* between 20 MHz and 10 MHz                                          */
/*--------------------------------------------------------------------*/

int ni_daq_gpct_set_speed( int board, NI_DAQ_CLOCK_SPEED_VALUE speed )
{
	NI_DAQ_GPCT_ARG a;
	int ret;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( speed != NI_DAQ_FULL_SPEED && speed != NI_DAQ_HALF_SPEED )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	if ( ni_daq_dev[ board ].gpct_state.speed == speed )
		return ni_daq_errno = NI_DAQ_OK;

	a.cmd = NI_DAQ_GPCT_SET_CLOCK_SPEED;
	a.speed = speed;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;
	
	ni_daq_dev[ board ].gpct_state.speed = speed;

	return ni_daq_errno = NI_DAQ_OK;
}


/*------------------------------------------------------------------*/
/* Function to determine if G_IN_TIMEBASE1 (i.e. the fast timebase) */
/* is running at 20 MHz or at 10 MHz                                */
/*------------------------------------------------------------------*/

int ni_daq_gpct_get_speed( int board, NI_DAQ_CLOCK_SPEED_VALUE *speed )
{
	int ret;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( speed == NULL )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	*speed = ni_daq_dev[ board ].gpct_state.speed;

	return ni_daq_errno = NI_DAQ_OK;
}


/*-----------------------------------------------------------------*/
/* Function starts a counter that will run until it is explicitely */
/* stopped by a call of ni_daq_gpct_stop_counter().                */
/*-----------------------------------------------------------------*/

int ni_daq_gpct_start_counter( int board, int counter, int source )
{
	int ret;
	int state;
	NI_DAQ_GPCT_ARG c;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( counter < 0 || counter > 1 )
		return ni_daq_errno = NI_DAQ_ERR_NSC;

	if ( ( ret = ni_daq_gpct_check_source( source ) ) < 0 )
		return ret;

	if ( ( ret = ni_daq_gpct_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI_DAQ_BUSY )
		return ni_daq_errno = NI_DAQ_ERR_CBS;

	c.cmd = NI_DAQ_GPCT_START_COUNTER;
	c.counter = counter;
	c.source_polarity = NI_DAQ_NORMAL;
	c.gate = NI_DAQ_NONE;
	c.source = source;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &c ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;

	ni_daq_dev[ board ].gpct_state.state[ counter ] =
												   NI_DAQ_CONT_COUNTER_RUNNING;

	return ni_daq_errno = NI_DAQ_OK;
}


/*----------------------------------------------------------------------*/
/* Function starts a counter that will be gated by its adjacent counter */
/* for 'gate_length'. It stops automatically at the end of the gate.    */
/*----------------------------------------------------------------------*/

int ni_daq_gpct_start_gated_counter( int board, int counter,
									 double gate_length, int source )
{
	int ret;
	int pulser;
	int state;
	unsigned long len;
	int gate_source = -1;
	NI_DAQ_GPCT_ARG a;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	/* Check if the counter number is reasonable */

	if ( counter < 0 || counter > 1 )
		return ni_daq_errno = NI_DAQ_ERR_NSC;

	pulser = counter ^ 1;

	/* Check that the gate length is reasonable and convert it to ticks */

	if ( ni_daq_gpct_time_to_ticks( board, counter, gate_length, &len,
									&gate_source ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	/* Check that the counter source is valid */

	if ( ( ret = ni_daq_gpct_check_source( source ) ) < 0 )
		return ret;

	/* Check that none of the counters is busy */

	if ( ( ret = ni_daq_gpct_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI_DAQ_BUSY )
		return ni_daq_errno = NI_DAQ_ERR_CBS;

	if ( ( ret = ni_daq_gpct_state( board, pulser, &state ) ) < 0 )
		return ret;

	if ( state == NI_DAQ_BUSY )
		return ni_daq_errno = NI_DAQ_ERR_NCB;

	/* Start the counter */

	a.cmd = NI_DAQ_GPCT_START_COUNTER;
	a.counter = counter;
	a.source = source;
	a.source_polarity = NI_DAQ_NORMAL;
	a.gate = NI_DAQ_G_TC_OTHER;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;

	/* Start the other counter producing the gate */

	a.cmd = NI_DAQ_GPCT_START_PULSER;
	a.counter = pulser;
	a.continuous = 0;
	a.low_ticks = 1;
	a.high_ticks = len;
	a.source = gate_source;
	a.gate = NI_DAQ_NONE;
	a.output_polarity = NI_DAQ_NORMAL;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
	{
		ni_daq_gpct_stop_counter( board, counter );
		return ni_daq_errno = NI_DAQ_ERR_INT;
	}

	ni_daq_dev[ board ].gpct_state.state[ counter ] = NI_DAQ_COUNTER_RUNNING;
	ni_daq_dev[ board ].gpct_state.state[ pulser ]  = NI_DAQ_PULSER_RUNNING;

	return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni_daq_gpct_stop_counter( int board, int counter )
{
	int ret;
	int state;
	int pulser;
	NI_DAQ_GPCT_ARG a;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( counter < 0 || counter > 1 )
		return ni_daq_errno = NI_DAQ_ERR_NSC;

	if ( ( ret = ni_daq_gpct_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI_DAQ_IDLE )
		return ni_daq_errno = NI_DAQ_OK;

	a.cmd = NI_DAQ_GPCT_DISARM_COUNTER;
	a.counter = counter;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;

	if ( ni_daq_dev[ board ].gpct_state.state[ counter ] ==
		 											   NI_DAQ_COUNTER_RUNNING )
	{
		pulser = counter ^ 1;
		if ( ni_daq_dev[ board ].gpct_state.state[ pulser ] ==
			 											NI_DAQ_PULSER_RUNNING )
			ni_daq_gpct_stop_pulses( board, pulser );
	}

	ni_daq_dev[ board ].gpct_state.state[ counter ] = NI_DAQ_IDLE;

	return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni_daq_gpct_get_count( int board, int counter, int wait_for_end,
						   unsigned long *count, int *state )
{
	int ret;
	NI_DAQ_GPCT_ARG a;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( counter < 0 || counter > 1 )
		return ni_daq_errno = NI_DAQ_ERR_NSC;

	if ( count == NULL )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	if ( wait_for_end &&
		 ( ni_daq_dev[ board ].gpct_state.state[ counter ] ==
		   										  NI_DAQ_CONT_PULSER_RUNNING ||
		   ni_daq_dev[ board ].gpct_state.state[ counter ] ==
		   										  NI_DAQ_CONT_COUNTER_RUNNING )
		)
		return ni_daq_errno = NI_DAQ_ERR_WFC;

	if ( state != NULL &&
		 ( ret = ni_daq_gpct_state( board, counter, state ) ) < 0 )
		return ret;

	a.cmd = NI_DAQ_GPCT_GET_COUNT;
	a.counter = counter;
	a.wait_for_end = wait_for_end ? 1 : 0;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
		return ni_daq_errno =
						  ( errno == EINTR ) ? NI_DAQ_ERR_ITR : NI_DAQ_ERR_INT;

	*count = a.count;

	if ( wait_for_end )
	{
		ni_daq_dev[ board ].gpct_state.state[ counter ] = NI_DAQ_IDLE;
		ni_daq_dev[ board ].gpct_state.state[ counter ^ 1 ] = NI_DAQ_IDLE;
	}

	return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni_daq_gpct_single_pulse( int board, int counter, double duration )
{
	int ret;
	NI_DAQ_GPCT_ARG a;
	unsigned long len;
	int state;
	int source = -1;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( counter < 0 || counter > 1 )
		return ni_daq_errno = NI_DAQ_ERR_NSC;

	if ( ni_daq_dev[ board ].gpct_state.state[ counter ] != NI_DAQ_IDLE )
		return ni_daq_errno = NI_DAQ_ERR_CBS;

	if ( ni_daq_gpct_time_to_ticks( board, counter, duration,
									&len, &source ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_NPT;

	if ( ( ret = ni_daq_gpct_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI_DAQ_BUSY )
		return ni_daq_errno = NI_DAQ_ERR_CBS;

    a.cmd = NI_DAQ_GPCT_COUNTER_OUTPUT_STATE;
    a.counter = counter;
    a.output_state = NI_DAQ_ENABLED;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;

	a.cmd = NI_DAQ_GPCT_START_PULSER;
	a.counter = counter;
	a.continuous = 0;
	a.low_ticks = 1;
	a.high_ticks = len;
	a.source = source;
	a.gate = NI_DAQ_NONE;
	a.output_polarity = NI_DAQ_NORMAL;
	a.output_state = NI_DAQ_ENABLED;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;

	ni_daq_dev[ board ].gpct_state.state[ counter ] = NI_DAQ_PULSER_RUNNING;

	return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni_daq_gpct_continuous_pulses( int board, int counter,
								   double high_phase, double low_phase )
{
	int ret;
	int state;
	NI_DAQ_GPCT_ARG a;
	unsigned long ht, lt;
	int source = -1;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( counter < 0 || counter > 1 )
		return ni_daq_errno = NI_DAQ_ERR_NSC;

	if ( ( ni_daq_gpct_time_to_ticks( board, counter, high_phase,
									  &ht, &source ) < 0 ||
		   ni_daq_gpct_time_to_ticks( board, counter, low_phase,
									  &lt, &source ) < 0 ) &&
		 ( source = -1,
		   ni_daq_gpct_time_to_ticks( board, counter, low_phase,
									  &lt, &source ) < 0 ||
		   ni_daq_gpct_time_to_ticks( board, counter, high_phase,
									  &ht, &source ) < 0 ) )
		return ni_daq_errno = NI_DAQ_ERR_NPT;

	if ( ( ret = ni_daq_gpct_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI_DAQ_BUSY )
		return ni_daq_errno = NI_DAQ_ERR_CBS;

	/* Enable output */

    a.cmd = NI_DAQ_GPCT_COUNTER_OUTPUT_STATE;
    a.counter = counter;
    a.output_state = NI_DAQ_ENABLED;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;

	a.cmd = NI_DAQ_GPCT_START_PULSER;
	a.counter = counter;
	a.continuous = 1;
	a.low_ticks = lt;
	a.high_ticks = ht;
	a.source = source;
	a.gate = NI_DAQ_NONE;
	a.output_polarity = NI_DAQ_NORMAL;
	a.output_state = NI_DAQ_ENABLED;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;

	ni_daq_dev[ board ].gpct_state.state[ counter ] =
													NI_DAQ_CONT_PULSER_RUNNING;

	return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni_daq_gpct_stop_pulses( int board, int counter )
{
	int ret;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	return ni_daq_gpct_stop_counter( board, counter );
}


/*------------------------------------------------------*/
/* Function checks if a counter source value is correct */
/*------------------------------------------------------*/

static int ni_daq_gpct_check_source( int source )
{
    if ( source < 0 ||
		 ( source > NI_DAQ_RTSI_6 && source != NI_DAQ_IN_TIMEBASE2 &&
		   source != NI_DAQ_G_TC_OTHER && source != NI_DAQ_LOW ) )
		return ni_daq_errno = NI_DAQ_ERR_IVS;

	return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

static int ni_daq_gpct_state( int board, int counter, int *state )
{
	if ( state == NULL )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

    if ( ni_daq_dev[ board ].gpct_state.state[ counter ] == NI_DAQ_IDLE )
    {
        *state = NI_DAQ_IDLE;
        return ni_daq_errno = NI_DAQ_OK;
    }

    if ( ni_daq_dev[ board ].gpct_state.state[ counter ] ==
		 										 NI_DAQ_CONT_COUNTER_RUNNING ||
         ni_daq_dev[ board ].gpct_state.state[ counter ] ==
		 										   NI_DAQ_CONT_PULSER_RUNNING )
    {
        *state = NI_DAQ_BUSY;
        return ni_daq_errno = NI_DAQ_OK;
    }

    return ni_daq_gpct_is_armed( board, counter, state );
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

static int ni_daq_gpct_is_armed( int board, int counter, int *state )
{
    NI_DAQ_GPCT_ARG a;


	if ( state == NULL )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	a.cmd = NI_DAQ_GPCT_IS_BUSY;
    a.counter = counter;

    if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
        return ni_daq_errno = NI_DAQ_ERR_INT;

    *state = a.is_armed ? NI_DAQ_BUSY : NI_DAQ_IDLE;

    return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*/
/* Function is used only internally: when the board is opened it gets */
/* called to find out the current GPCT settings of the board          */
/*--------------------------------------------------------------------*/

int ni_daq_gpct_init( int board )
{
	NI_DAQ_GPCT_ARG a;


	a.cmd = NI_DAQ_GPCT_GET_CLOCK_SPEED;
	
	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_GPCT, &a ) < 0 )
		return 1;
	
	ni_daq_dev[ board ].gpct_state.speed = a.speed;

	ni_daq_dev[ board ].gpct_state.state[ 0 ] =
		ni_daq_dev[ board ].gpct_state.state[ 1 ] = NI_DAQ_IDLE;

	return 0;
}


/*--------------------------------------------------------------------*/
/* This function tries to figure out the timing (in units of ticks of */
/* the GPCT clock) for a given time (in seconds). If necessary (and   */
/* possible) the GPCT clock setting is changed to make it possible to */
/* achieve to requested timing.                                       */
/*--------------------------------------------------------------------*/

static int ni_daq_gpct_time_to_ticks( int board, int counter,
									  double duration, unsigned long *ticks,
									  int *source )
{
	int state;
	unsigned int poss_clock = 0;
	int i;
	double d[ 3 ] = { 5.0e-8, 1.0e-7, 5.0e-6 };
	unsigned int cfc;              /* 0 if fast clock is 20 MHz, otherwise 1 */
	int ret;


	if ( ticks == NULL || source == NULL )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	/* Time must be between 100 ns and 2^24 * 10 us (since the clock can be
	   either 20 MHz, 10 MHz, 200 kHz or 100 kHz and the counters are 24-bit
	   wide), also take into account small rounding errors of 0.01% */

	if ( 1.0001 * duration < 1.0e-7 || duration / 1.0001 > 0x1000000 * 1.0e-5 )
		return -1;

	/* Figure out how the time can be realized with one of the possible
	   clock settings of 20 MHz, 10 Mhz, 200 kHz or 100 kHz (but check
	   which one is currently switched on) and with the allowed range of
	   2 to 2^24 for the counter value. If the speed setting for the slow
	   clock has been reduced (globally) by a factor of 2 we have to take
	   this into account. */

	if ( ni_daq_dev[ board ].msc_state.speed == NI_DAQ_HALF_SPEED )
		d[ 2 ] *= 2.0;

	for ( i = 0; i < 3; i++ )
		if ( 1.0001 * duration >= 2 * d[ i ] && 
			 duration / 1.0001 <= 0x1000000 * d[ i ] &&
			 fabs( duration - floor( duration / d[ i ] + 0.5 ) * d[ i ] )
			 											   <= 0.0001 * d[ i ] )
			poss_clock |= 1 << i;

	if ( poss_clock == 0 )        /* impossible to set length */
		return -1;

	/* We must realize the duration with the setting for source if it's set to
	   something non-negative */

	cfc = ni_daq_dev[ board ].gpct_state.speed == NI_DAQ_FULL_SPEED ? 0 : 1;

	if ( *source >= 0 )
	{
		if ( ( *source == NI_DAQ_G_IN_TIMEBASE1 && ! ( poss_clock & 3 ) ) ||
			 ( *source == NI_DAQ_IN_TIMEBASE2   && ! ( poss_clock & 4 ) ) )
		return -1;

		if ( *source == NI_DAQ_IN_TIMEBASE2 )
		{
			*ticks = ( unsigned long ) floor( duration / d[ 2 ] + 0.5 );
			return 0;
		}
		else if ( *source == NI_DAQ_G_IN_TIMEBASE1 )
		{
			*ticks = ( unsigned long ) floor( duration / d[ cfc ] + 0.5 );
			return 0;
		}
	}

	/* Lets first check if we can realize the duration with the slow clock */

	if ( poss_clock & 4 )
	{
		*ticks = ( unsigned long ) floor( duration / d[ 2 ] + 0.5 );
		*source = NI_DAQ_IN_TIMEBASE2;
		return 0;
	}

	/* Now lets check if the duration can be produced with the current setting
	   of the fast clock */

	if ( ( ! cfc && poss_clock & 1 ) || ( cfc && poss_clock & 2 ) )
	{
		*ticks = ( long ) floor( duration / d[ cfc ] + 0.5 );
		*source = NI_DAQ_G_IN_TIMEBASE1;
		return 0;
	}

	/* Next thing is to try if we can do after changing the speed setting for
	   the fast clock. But this is not allowed if the other counter is already
	   creating pulses or is still creating a single pulse */

	if ( ! ( ! cfc && poss_clock & 2 ) || ! ( cfc && poss_clock & 1 ) ||
		 ni_daq_dev[ board ].gpct_state.state[ counter ^ 1 ] ==
												  NI_DAQ_CONT_PULSER_RUNNING ||
		 ni_daq_gpct_is_armed( board, counter ^ 1, &state ) < 0 ||
		 ( ni_daq_dev[ board ].gpct_state.state[ counter ^ 1 ] ==
		 											   NI_DAQ_PULSER_RUNNING &&
		   state == NI_DAQ_BUSY ) )
		return -1;

	/* Finally check if we can realize it after switching the fast clock */

	if ( ! cfc && poss_clock & 2 )
	{
		if ( ( ret = ni_daq_gpct_set_speed( board, NI_DAQ_HALF_SPEED ) ) < 0 )
			return ret;
		*ticks = ( long ) floor( duration / d[ 1 ] + 0.5 );
		*source = NI_DAQ_G_IN_TIMEBASE1;
		return 0;
	}

	if ( cfc && poss_clock & 1 )
	{
		if ( ( ret = ni_daq_gpct_set_speed( board, NI_DAQ_FULL_SPEED ) ) < 0 )
			return ret;
		*ticks = ( long ) floor( duration / d[ 0 ] + 0.5 );
		*source = NI_DAQ_G_IN_TIMEBASE1;
		return 0;
	}

	/* No alternatives left, report failure */

	return -1;
}
