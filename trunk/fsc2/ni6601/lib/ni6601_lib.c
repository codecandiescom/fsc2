/*
  $Id$

  Copyright (C) 2002-2003 Jens Thoms Toerring
 
  This library should simplify accessing NI6601 GBCT boards by National
  Instruments by avoiding to be forced to make ioctl() calls and use
  higher level functions instead.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
  To contact the author send email to:
  Jens.Toerring@physik.fu-berlin.de
*/


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "ni6601.h"

#define NI6601_DEVICE_NAME "ni6601_"


static int ni6601_is_armed( int board, int counter, int *state );
static int ni6601_state( int board, int counter, int *state );
static int check_board( int board );
static int check_source( int source );
static int ni6601_time_to_ticks( double time, unsigned long *ticks );

static NI6601_Device_Info dev_info[ NI6601_MAX_BOARDS ];
static const char *error_message = "";


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

int ni6601_close( int board )
{
	error_message = "";

	if ( board < 0 || board >= NI6601_MAX_BOARDS )
	{
		error_message = NI6601_ERR_NSB_MESS;
		return NI6601_ERR_NSB;
	}

	if ( ! dev_info[ board ].is_init )
	{
		error_message = NI6601_ERR_BNO_MESS;
		return NI6601_ERR_BNO;
	}

    if ( dev_info[ board ].fd >= 0 )
        close( dev_info[ board ].fd );

	dev_info[ board ].is_init = 0;

	return NI6601_OK;
}


/*-----------------------------------------------------------------*/
/* Function starts a counter that will run until it is explicitely */
/* stopped by a call to ni6601_stop_counter().                     */
/*-----------------------------------------------------------------*/

int ni6601_start_counter( int board, int counter, int source )
{
	int ret;
	int state;
	NI6601_COUNTER c;


	error_message = "";

	if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
	{
		error_message = NI6601_ERR_NSC_MESS;
		return NI6601_ERR_NSC;
	}

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_source( source ) ) < 0 )
		return ret;

	if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI6601_BUSY )
	{
		error_message = NI6601_ERR_CBS_MESS;
		return NI6601_ERR_CBS;
	}

	c.counter = counter;
	c.source_polarity = NI6601_NORMAL;
	c.gate = NI6601_NONE;
	c.source = source;

	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_COUNTER, &c ) < 0 )
	{
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	dev_info[ board ].state[ counter ] = NI6601_CONT_COUNTER_RUNNING;

	return NI6601_OK;
}


/*----------------------------------------------------------------------*/
/* Function starts a counter that will be gated by its adjacent counter */
/* for 'gate_length'. It stops automatically at the end of the gate.    */
/*----------------------------------------------------------------------*/

int ni6601_start_gated_counter( int board, int counter, double gate_length,
								int source )
{
	int ret;
	int pulser;
	int state;
	unsigned long len;
	NI6601_PULSES p;
	NI6601_COUNTER c;


	error_message = "";

	/* Check if the counter number is reasonable and determine number of
	   adjacent counter to be used for gating */

	if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
	{
		error_message = NI6601_ERR_NSC_MESS;
		return NI6601_ERR_NSC;
	}

	pulser = counter & 1 ? counter - 1 : counter + 1;

	/* Check that the gate length is reasonable and convert it to ticks */

	if ( ni6601_time_to_ticks( gate_length, &len ) < 0 )
	{
		error_message = NI6601_ERR_IVA_MESS;
		return NI6601_ERR_IVA;
	}

	/* Open the board if necessary */

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	/* Check that the counters source is valid */

	if ( ( ret = check_source( source ) ) < 0 )
		return ret;

	/* Check that neither the counter nor the adjacent counter is busy */

	if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI6601_BUSY )
	{
		error_message = NI6601_ERR_CBS_MESS;
		return NI6601_ERR_CBS;
	}

	if ( ( ret = ni6601_state( board, pulser, &state ) ) < 0 )
		return ret;

	if ( state == NI6601_BUSY )
	{
		error_message = NI6601_ERR_NCB_MESS;
		return NI6601_ERR_NCB;
	}

	/* Start the counter */

	c.counter = counter;
	c.source_polarity = NI6601_NORMAL;
	c.gate = NI6601_NEXT_OUT;
	c.source = source;

	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_COUNTER, &c ) < 0 )
	{
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	/* Start the adjacent counter producing the gate */

	p.counter = pulser;
	p.continuous = 0;
	p.low_ticks = 1;
	p.high_ticks = len;
	p.source = NI6601_TIMEBASE_1;
	p.gate = NI6601_NONE;
	p.disable_output = 0;
	p.output_polarity = NI6601_NORMAL;

	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_PULSER, &p ) < 0 )
	{
		ni6601_stop_counter( board, counter );
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	dev_info[ board ].state[ counter ] = NI6601_COUNTER_RUNNING;
	dev_info[ board ].state[ pulser ]  = NI6601_PULSER_RUNNING;

	return NI6601_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni6601_stop_counter( int board, int counter )
{
	int ret;
	int state;
	NI6601_DISARM d;
	int pulser;


	error_message = "";

	if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
	{
		error_message = NI6601_ERR_NSC_MESS;
		return NI6601_ERR_NSC;
	}

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI6601_IDLE )
		return NI6601_OK;

	d.counter = counter;

	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_DISARM, &d ) < 0 )
	{
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	if ( dev_info[ board ].state[ counter ] == NI6601_COUNTER_RUNNING )
	{
		pulser = counter & 1 ? counter - 1 : counter + 1;
		if ( dev_info[ board ].state[ pulser ] == NI6601_PULSER_RUNNING )
			ni6601_stop_pulses( board, pulser );
	}

	dev_info[ board ].state[ counter ] = NI6601_IDLE;

	return NI6601_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni6601_get_count( int board, int counter, int wait_for_end,
					  unsigned long *count, int *state )
{
	int ret;
	NI6601_COUNTER_VAL v;


	error_message = "";

	if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
	{
		error_message = NI6601_ERR_NSC_MESS;
		return NI6601_ERR_NSC;
	}

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( count == NULL )
	{
		error_message = NI6601_ERR_IVA_MESS;
		return NI6601_ERR_IVA;
	}

	if ( wait_for_end &&
		 ( dev_info[ board ].state[ counter ] == NI6601_CONT_PULSER_RUNNING ||
		   dev_info[ board ].state[ counter ] == NI6601_CONT_COUNTER_RUNNING )
		)
	{
		error_message = NI6601_ERR_WFC_MESS;
		return NI6601_ERR_WFC;
	}

	if ( state != NULL &&
		 ( ret = ni6601_state( board, counter, state ) ) < 0 )
		return ret;

	v.counter = counter;
	v.wait_for_end = wait_for_end ? 1 : 0;

	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_COUNT, &v ) < 0 )
	{
		if ( errno == EINTR )
		{
			error_message = NI6601_ERR_ITR_MESS;
			return NI6601_ERR_ITR;
		}
			
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	*count = v.count;

	if ( wait_for_end )
	{
		dev_info[ board ].state[ counter ] = NI6601_IDLE;		
		dev_info[ board ].state[ counter & 1 ? counter - 1 : counter + 1 ]
			= NI6601_IDLE;
	}

	return NI6601_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni6601_generate_single_pulse( int board, int counter, double duration )
{
	int ret;
	NI6601_PULSES p;
	unsigned long len;
	int state;


	error_message = "";

	if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
	{
		error_message = NI6601_ERR_NSC_MESS;
		return NI6601_ERR_NSC;
	}

	if ( dev_info[ board ].state[ counter ] != NI6601_IDLE )
	{
		error_message = NI6601_ERR_CBS_MESS;
		return NI6601_ERR_CBS;
	}

	if ( ni6601_time_to_ticks( duration, &len ) < 0 )
	{
		error_message = NI6601_ERR_IVA_MESS;
		return NI6601_ERR_IVA;
	}

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;	

	if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI6601_BUSY )
	{
		error_message = NI6601_ERR_CBS_MESS;
		return NI6601_ERR_CBS;
	}

	p.counter = counter;
	p.continuous = 1;
	p.low_ticks = 1;
	p.high_ticks = len;
	p.source = NI6601_TIMEBASE_1;
	p.gate = NI6601_NONE;
	p.disable_output = 0;
	p.output_polarity = NI6601_NORMAL;

	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_PULSER, &p ) < 0 )
	{
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	dev_info[ board ].state[ counter ] = NI6601_PULSER_RUNNING;

	return NI6601_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni6601_generate_continuous_pulses( int board, int counter,
									   double high_phase, double low_phase )
{
	int ret;
	int state;
	NI6601_PULSES p;
	unsigned long ht, lt;


	error_message = "";

	if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
	{
		error_message = NI6601_ERR_NSC_MESS;
		return NI6601_ERR_NSC;
	}

	if ( ni6601_time_to_ticks( high_phase, &ht ) < 0 ||
		 ni6601_time_to_ticks( low_phase, &lt ) < 0 )
	{
		error_message = NI6601_ERR_IVA_MESS;
		return NI6601_ERR_IVA;
	}

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;	

	if ( ( ret = ni6601_state( board, counter, &state ) ) < 0 )
		return ret;

	if ( state == NI6601_BUSY )
	{
		error_message = NI6601_ERR_CBS_MESS;
		return NI6601_ERR_CBS;
	}

	p.counter = counter;
	p.continuous = 1;
	p.low_ticks = lt;
	p.high_ticks = ht;
	p.source = NI6601_TIMEBASE_1;
	p.gate = NI6601_NONE;
	p.disable_output = 0;
	p.output_polarity = NI6601_NORMAL;

	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_PULSER, &p ) < 0 )
	{
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	dev_info[ board ].state[ counter ] = NI6601_CONT_PULSER_RUNNING;

	return NI6601_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni6601_stop_pulses( int board, int counter )
{
	return ni6601_stop_counter( board, counter );
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni6601_dio_write( int board, unsigned char bits, unsigned char mask )
{
	int ret;
	NI6601_DIO_VALUE dio;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	dio.value = bits;
	dio.mask = mask;

	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_DIO_OUT, &dio ) < 0 )
	{
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	return NI6601_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni6601_dio_read( int board, unsigned char *bits, unsigned char mask )
{
	int ret;
	NI6601_DIO_VALUE dio;


	error_message = "";

	if ( bits == NULL )
	{
		error_message = NI6601_ERR_IVA_MESS;
		return NI6601_ERR_IVA;
	}

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;	

	dio.mask = mask;
	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_DIO_IN, &dio ) < 0 )
	{
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	*bits = dio.value;
	return NI6601_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni6601_is_counter_armed( int board, int counter, int *state )
{
	int ret;


	error_message = "";

	if ( state == NULL )
	{
		error_message = NI6601_ERR_IVA_MESS;
		return NI6601_ERR_IVA;
	}

	if ( counter < NI6601_COUNTER_0 || counter > NI6601_COUNTER_3 )
	{
		error_message = NI6601_ERR_NSC_MESS;
		return NI6601_ERR_NSC;
	}

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;	

	return ni6601_is_armed( board, counter, state );
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

static int ni6601_is_armed( int board, int counter, int *state )
{
	NI6601_IS_ARMED a;


	a.counter = counter;

	if ( ioctl( dev_info[ board ].fd, NI6601_IOC_IS_BUSY, &a ) < 0 )
	{
		error_message = NI6601_ERR_INT_MESS;
		return NI6601_ERR_INT;
	}

	*state = a.state ? NI6601_BUSY : NI6601_IDLE;

	return NI6601_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

static int ni6601_state( int board, int counter, int *state )
{
	if ( dev_info[ board ].state[ counter ] == NI6601_IDLE )
	{
		*state = NI6601_IDLE;
		return NI6601_OK;
	}

	if ( dev_info[ board ].state[ counter ] == NI6601_CONT_COUNTER_RUNNING ||
		 dev_info[ board ].state[ counter ] == NI6601_CONT_PULSER_RUNNING )
	{
		*state = NI6601_BUSY;
		return NI6601_OK;
	}

	return ni6601_is_armed( board, counter, state );
}


/*--------------------------------------------------------------------*/
/* This function gets called always before a board is really accessed */
/* to determine if the board number is valid. If the board has never  */
/* been used before the function will try to open() the device file   */
/* for the board.                                                     */
/* If opening the device file fails this indicates that there's no    */
/* board with the number passed to the function or that the board is  */
/* already in use by some other process and the function returns the  */
/* appropriate error number.                                          */
/* On success the boards file descriptor is opened and 0 is returned. */
/*--------------------------------------------------------------------*/

static int check_board( int board )
{
	char name[ 20 ] = "/dev/" NI6601_DEVICE_NAME;
	int i;
	struct stat buf;


	if ( board < 0 || board >= NI6601_MAX_BOARDS )
	{
		error_message = NI6601_ERR_NSB_MESS;
		return NI6601_ERR_NSB;
	}

	if ( ! dev_info[ board ].is_init )
	{
		/* Cobble together the device file name */

		snprintf( name + strlen( name ), 20 - strlen( name ), "%d", board );

		/* Check if the device file exists and can be accessed */

		if ( stat( name, &buf ) < 0 )
			switch ( errno )
			{
				case ENOENT :
					error_message = NI6601_ERR_DFM_MESS;
					return NI6601_ERR_DFM;

				case EACCES :
					error_message = NI6601_ERR_ACS_MESS;
					return NI6601_ERR_ACS;

				default :
					error_message = NI6601_ERR_DFP_MESS;
					return NI6601_ERR_DFP;
			}

		/* Try to open it in non-blocking mode */

		if ( ( dev_info[ board ].fd = open( name, O_RDWR | O_NONBLOCK ) )< 0 )
			switch ( errno )
			{
				case ENODEV : case ENXIO :
					error_message = NI6601_ERR_NDV_MESS;
					return NI6601_ERR_NDV;
			
				case EACCES :
					error_message = NI6601_ERR_ACS_MESS;
					return NI6601_ERR_ACS;

				case EBUSY :
					error_message = NI6601_ERR_BBS_MESS;
					return NI6601_ERR_BBS;

				default :
					error_message = NI6601_ERR_DFP_MESS;
					return NI6601_ERR_DFP;
			}

		/* Set the FD_CLOEXEC bit for the device file - exec()'ed application
		   should have no interest at all in the board (and even don't know
		   that they have the file open) but could interfere seriously with
		   normal operation of the program that did open the device file. */

		fcntl( dev_info[ board ].fd, F_SETFD, FD_CLOEXEC );

		dev_info[ board ].is_init = 1;

		for ( i = 0; i < 4; i++ )
			dev_info[ board ].state[ i ] = NI6601_IDLE;
	}

	return NI6601_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

static int check_source( int source )
{
	if ( source == NI6601_LOW || 
		 source == NI6601_DEFAULT_SOURCE ||
		 source == NI6601_SOURCE_0 ||
		 source == NI6601_SOURCE_1 ||
		 source == NI6601_SOURCE_2 ||
		 source == NI6601_SOURCE_3 ||
		 source == NI6601_NEXT_GATE ||
		 source == NI6601_TIMEBASE_1 ||
		 source == NI6601_TIMEBASE_2 ||
		 source == NI6601_TIMEBASE_3 )
		return NI6601_OK;

	error_message = NI6601_ERR_IVS_MESS;
	return NI6601_ERR_IVS;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

static int ni6601_time_to_ticks( double time, unsigned long *ticks )
{
	unsigned long t;


	error_message = "";

	if ( time <= 0.0 )
	{
		error_message = NI6601_ERR_IVA_MESS;
		return NI6601_ERR_IVA;
	}

	if ( time / NI6601_TIME_RESOLUTION - 1 > ULONG_MAX )
	{
		error_message = NI6601_ERR_IVA_MESS;
		return NI6601_ERR_IVA;
	}

	if ( ( t = ( unsigned long )
						  floor( time / NI6601_TIME_RESOLUTION - 0.5 ) ) == 0 )
	{
		error_message = NI6601_ERR_IVA_MESS;
		return NI6601_ERR_IVA;
	}
	
	*ticks = t;

	return NI6601_OK;
}


/*---------------------------------------------------------------*/
/* Returns a string with a short descriptive text of the error   */
/* encountered in the last invocation of one of the ni6601_xxx() */
/* functions. If there were no error, i.e. the function returned */
/* successfully, an empty string is returned.                    */
/*---------------------------------------------------------------*/

const char *ni6601_error_message( void )
{
	return error_message;
}
