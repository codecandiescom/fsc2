/*
  $Id$

  Copyright (C) 2002-2003 Jens Thoms Toerring
 
  This library should simplify accessing ME6000 and ME6100 DAC boards
  by Meilhaus Electronic GmbH by avoiding to be forced to make ioctl()
  calls and use some higher level functions instead. Most functions are
  based on the specifications for similar functions for the Windows API
  supplied by Meilhaus Electronic GmbH, but there are several differences
  that make the functions more UNIX-like and, unfortunately, require some
  changes to your code should you try to port an application from a Windows.

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
#include <fcntl.h>
#include <sys/ioctl.h>

#include "me6x00.h"


static int check_board( int board );
static int check_dac( int board, int dac, int is_me6100_special );


#define ME6X00_DEVICE_NAME "me6x00_"

/* Clock frequency of board, 33 MHz */

#define ME6X00_CLOCK_FREQ  3.3e7

#define ME6100_MAX_BUFFER_SIZE  0x10000     /* 64 kB */

#define IS_ME6100( board ) ( dev_info[ board ].info.device_ID & 0x100 )

/* The possible output modes */

#define ME6000_AND_ME6100  0
#define ME6100_SPECIFIC    1


static const char *error_message = "";
static ME6X00_Device_Info dev_info[ ME6X00_MAX_BOARDS ];
static me6x00_dev_info ret_info;


/*----------------------------------------------------------------*/
/* Utility function to convert a frequency into the corresponding */
/* number of timer clicks.                                        */
/*----------------------------------------------------------------*/

unsigned int me6x00_frequency_to_timer( double freq )
{
	unsigned int ticks;


	error_message = "";

	if ( freq <= 0 || freq / ME6X00_CLOCK_FREQ > UINT_MAX )
	{
		error_message = ME6X00_ERR_IFR_MESS;
		return ME6X00_ERR_IFR;
	}

	ticks = ME6X00_CLOCK_FREQ / freq;
	if ( ticks < ME6X00_MIN_TICKS )
		ticks = ME6X00_MIN_TICKS;

	return ticks;
}


/*------------------------------------------*/
/* Returns the type of the addressed board. */
/*------------------------------------------*/

int me6x00_board_type( int board, unsigned int *type )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	*type = dev_info[ board ].info.device_ID;
	return ME6X00_OK;
}


/*------------------------------------------*/
/* Returns the type of the addressed board. */
/*------------------------------------------*/

int me6x00_num_dacs( int board, unsigned int *num_dacs )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	*num_dacs = dev_info[ board ].num_dacs;
	return ME6X00_OK;
}



/*--------------------------------------------------*/
/* Returns the serial number of the addresses board */
/*--------------------------------------------------*/

int me6x00_serial_number( int board, unsigned int *serial_no )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	*serial_no = dev_info[ board ].info.serial_no;
	return ME6X00_OK;
}
		

/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int me6x00_board_info( int board, me6x00_dev_info **info )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	/* Pass back to the user a copy of the requested info so (s)he can't
	   mess around with the original */

	ret_info = dev_info[ board ].info;
	*info = &ret_info;

	return ME6X00_OK;
}


/*----------------------------------------------------------------*/
/* Returns a string with a short descriptive text of the error    */
/* encountered in the last invocation of one of the me6x000_xxx() */
/* functions. If there were no error, i.e. the function returned  */
/* successfully, an empty string is returned.                     */
/*----------------------------------------------------------------*/

const char *me6x00_error_message( void )
{
	return error_message;
}


/*--------------------------------------------------------------------*/
/* Function to be called to close a board when it's not longer needed */
/*--------------------------------------------------------------------*/

int me6x00_close( int board )
{
	if ( board < 0 || board >= ME6X00_MAX_BOARDS )
	{
		error_message = ME6X00_ERR_IBN_MESS;
		return ME6X00_ERR_IBN;
	}

	if ( ! dev_info[ board ].is_init )
	{
		error_message = ME6X00_ERR_BNO_MESS;
		return ME6X00_ERR_BNO;
	}

	if ( dev_info[ board ].fd >= 0 )
		close( dev_info[ board ].fd );

	dev_info[ board ].is_init = 0;

	return ME6X00_OK;
}


/*-----------------------------------------------------------------*/
/* Calling this function for one of the DACs of a board, one can   */
/* toggle between having the board switch back to a voltage of 0 V */
/* when its file descriptor is closed (i.e. the program using the  */
/* board ending) or having the board continue to output the last   */
/* voltage that had been set.                                      */
/* The default behaviour of a board is to revert back to 0 V on    */
/* close. To make a DAC of a board keep its last voltage this      */
/* function has to be called anew each time the board is opened!   */
/* Arguments:                                                      */
/*  1. number of board                                             */
/*  2. number of DAC                                               */
/*  3. state: 1: keep last voltage, 0: output 0 V on close         */
/*-----------------------------------------------------------------*/

int me6x00_keep_voltage( int board, int dac, int state )
{
	int ret;
	me6x00_keep_st keep;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6000_AND_ME6100 ) ) < 0 )
		return ret;

	keep.dac = dac;
	keep.value = state ? ME6X00_KEEP : ME6X00_DONT_KEEP;

	if ( ioctl( dev_info[ board ].fd, ME6X00_KEEP_VOLTAGE, &keep ) < 0 )
	{
		close( dev_info[ board ].fd );
		dev_info[ board ].fd = -1;
		error_message = ME6X00_ERR_INT_MESS;
		return ME6X00_ERR_INT;
	}

	return ME6X00_OK;
}


/*------------------------------------------------------------------*/
/* This is a wrapper function for me6x00_single() that allows users */
/* to specify a voltage directly to output on a certain DAC.        */
/*------------------------------------------------------------------*/

int me6x00_voltage( int board, int dac, double volts )
{
	unsigned short val;


	error_message = "";

	if ( volts < -10.0 || volts > 10.0 )
	{
		error_message = ME6X00_ERR_VLT_MESS;
		return ME6X00_ERR_VLT;
	}

	val = ( volts + 10.0 ) / 20.0 * 0xFFFF;
	return me6x00_single( board, dac, val );
}


/*--------------------------------------------------------------------*/
/* INCOMPLETE */
/*--------------------------------------------------------------------*/

int me6x00_continuous( int board, int dac, int size, unsigned short *buf )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6100_SPECIFIC ) ) < 0 )
		return ret;

	if ( ! buf )
	{
		error_message = ME6X00_ERR_IBA_MESS;
		return ME6X00_ERR_IBA;
	}

	if ( size <= 0 || size > ME6100_MAX_BUFFER_SIZE )
	{
		error_message = ME6X00_ERR_IBS_MESS;
		return ME6X00_ERR_IBS;
	}

	/*******/

	return ME6X00_OK;
}


/*--------------------------------------------------------------------*/
/* INCOMPLETE */
/*--------------------------------------------------------------------*/

int me6x00_continuous_ex( int board, int dac, int size, unsigned short *buf )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6100_SPECIFIC ) ) < 0 )
		return ret;

	if ( ! buf )
	{
		error_message = ME6X00_ERR_IBA_MESS;
		return ME6X00_ERR_IBA;
	}

	if ( size <= 0 || size > ME6100_MAX_BUFFER_SIZE )
	{
		error_message = ME6X00_ERR_IBS_MESS;
		return ME6X00_ERR_IBS;
	}

	/*******/

	return ME6X00_OK;
}


/*---------------------------------------------------------------------*/
/* This function resets the DAC of a board. This includes stopping any */
/* output in continuous or wrap-around mode currently in progress and  */
/* setting the output voltage of the DAC to 0 V.                       */
/* Arguments:                   									   */
/*  1. number of board          									   */
/*  2. number of DAC            									   */
/*---------------------------------------------------------------------*/

int me6x00_reset( int board, int dac )
{
	int ret;
	me6x00_stasto_st conv;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6000_AND_ME6100 ) ) < 0 )
		return ret;

	/* Stopping continuous or wraparound mode only needs to be done for
	   the four lowest-numbered channels of a ME6100 board */

	if ( IS_ME6100( board ) && dac <= ME6X00_DAC03 )
	{
		conv.dac = dac;
		conv.stasto = ME6X00_STOP;

		if ( ioctl( dev_info[ board ].fd, ME6X00_START_STOP_CONV, &conv )
			 < 0 )
		{
			close( dev_info[ board ].fd );
			dev_info[ board ].fd = -1;
			error_message = ME6X00_ERR_INT_MESS;
			return ME6X00_ERR_INT;
		}
	}

	/* Set DAC to output 0 V */

	return me6x00_single( board, dac, 0x7FFF );
}


/*--------------------------------------------------------------------*/
/* This function resets all DACs of a board. This includes stopping   */
/* any output in continuous or wrap-around mode currently in progress */
/* and  setting the output voltage of all DACs of the board to 0 V.   */
/* Arguments:                                                         */
/*  1. number of board                                                */
/*--------------------------------------------------------------------*/

int me6x00_reset_all( int board )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ioctl( dev_info[ board ].fd, ME6X00_RESET_BOARD, NULL ) < 0 )
	{
		close( dev_info[ board ].fd );
		dev_info[ board ].fd = -1;
		error_message = ME6X00_ERR_INT_MESS;
		return ME6X00_ERR_INT;
	}

	return ME6X00_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int me6x00_set_timer( int board, int dac, unsigned int ticks )
{	
	int ret;
	me6x00_timer_st timer;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6100_SPECIFIC ) ) < 0 )
		return ret;

	if ( ticks < ME6X00_MIN_TICKS )
	{
		error_message = ME6X00_ERR_TCK_MESS;
		return ME6X00_ERR_TCK;
	}

	timer.dac     = dac;
	timer.divisor = ticks;

	if ( ioctl( dev_info[ board ].fd, ME6X00_SET_TIMER, &timer ) < 0 )
	{
		close( dev_info[ board ].fd );
		dev_info[ board ].fd = -1;
		error_message = ME6X00_ERR_INT_MESS;
		return ME6X00_ERR_INT;
	}

	return ME6X00_OK;
}


/*--------------------------------------------------------------------*/
/* INCOMPLETE */
/*--------------------------------------------------------------------*/

int me6x00_set_trigger( int board, int dac, int mode )
{	
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6100_SPECIFIC ) ) < 0 )
		return ret;

	if ( mode != ME6X00_TRIGGER_TIMER &&
		 mode != ME6X00_TRIGGER_EXT_LOW &&
		 mode != ME6X00_TRIGGER_EXT_HIGH )
	{
		error_message = ME6X00_ERR_TRG_MESS;
		return ME6X00_ERR_TRG;
	}

	/*******/

	return ME6X00_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int me6x00_single( int board, int dac, unsigned short val )
{
	int ret;
	me6x00_mode_st mode;
	me6x00_single_st single;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;
	
	if ( ( ret = check_dac( board, dac, ME6000_AND_ME6100 ) ) < 0 )
		return ret;

	/* FOR ME6100 boards make sure the DAC is stopped and in single mode
	   (only necessary for the lowest four DACs, the others only can be used
	   in SINGLE mode). */

	if ( IS_ME6100( board ) && dac <= ME6X00_DAC03 )
	{
		mode.dac = dac;
		mode.mode = ME6X00_SINGLE;

		if ( ioctl( dev_info[ board ].fd, ME6X00_SET_MODE, &mode ) < 0 )
		{
			close( dev_info[ board ].fd );
			dev_info[ board ].fd = -1;
			error_message = ME6X00_ERR_INT_MESS;
			return ME6X00_ERR_INT;
		}
	}

	single.dac   = dac;
	single.value = val;

	while ( 1 )
	{
		if ( ioctl( dev_info[ board ].fd, ME6X00_WRITE_SINGLE, &single ) == 0 )
			break;

		if ( errno == -EINTR )
			continue;

		close( dev_info[ board ].fd );
		dev_info[ board ].fd = -1;
		error_message = ME6X00_ERR_INT_MESS;
		return ME6X00_ERR_INT;
	}

	return ME6X00_OK;
}


/*--------------------------------------------------------------------*/
/* INCOMPLETE */
/*--------------------------------------------------------------------*/

int me6x00_start( int board, int dac )
{
	int ret;
	me6x00_stasto_st start;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6100_SPECIFIC ) ) < 0 )
		return ret;

	start.dac = dac;
	start.stasto = ME6X00_START;

	if ( ioctl( dev_info[ board ].fd, ME6X00_START_STOP_CONV, &start ) < 0 )
	{
		close( dev_info[ board ].fd );
		dev_info[ board ].fd = -1;
		error_message = ME6X00_ERR_INT_MESS;
		return ME6X00_ERR_INT;
	}

	return ME6X00_OK;
}


/*--------------------------------------------------------------------*/
/* INCOMPLETE */
/*--------------------------------------------------------------------*/

int me6x00_stop( int board, int dac )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6100_SPECIFIC ) ) < 0 )
		return ret;

	/*******/

	return ME6X00_OK;
}


/*--------------------------------------------------------------------*/
/* INCOMPLETE */
/*--------------------------------------------------------------------*/

int me6x00_stop_ex( int board, int dac )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6100_SPECIFIC ) ) < 0 )
		return ret;

	/*******/

	return ME6X00_OK;
}


/*--------------------------------------------------------------------*/
/* INCOMPLETE */
/*--------------------------------------------------------------------*/

int me6x00_wraparound( int board, int dac, int size, unsigned short *buf )
{
	int ret;


	error_message = "";

	if ( ( ret = check_board( board ) ) < 0 )
		return ret;

	if ( ( ret = check_dac( board, dac, ME6100_SPECIFIC ) ) < 0 )
		return ret;

	if ( ! buf )
	{
		error_message = ME6X00_ERR_IBA_MESS;
		return ME6X00_ERR_IBA;
	}

	if ( size <= 0 || size > ME6X00_FIFO_SIZE )
	{
		error_message = ME6X00_ERR_IBS_MESS;
		return ME6X00_ERR_IBS;
	}

	/*******/

	return ME6X00_OK;
}


/*--------------------------------------------------------------------*/
/* This function gets called always before a board is really accessed */
/* to determine if the board number is valid. If the board has not    */
/* been used before the function will try to open() the device file   */
/* for the board and then ask the module for informations about the   */
/* board, i.e. vendor ID, type of board, serial number etc., and      */
/* store it for later use.                                            */
/* If opening the device file fails this indicates that there's no    */
/* board with the number passed to the function or that the board is  */
/* already in use by some other process and the function returns the  */
/* appropriate error number.                                          */
/* Another possibility for failure is an internal error (which, of    */
/* course, should never happen ;-).                                   */
/* On success the boards file descriptor is opened and 0 is returned. */
/*--------------------------------------------------------------------*/

static int check_board( int board )
{
	char name[ 20 ] = "/dev/" ME6X00_DEVICE_NAME;
	struct stat buf;


	if ( board < 0 || board >= ME6X00_MAX_BOARDS )
	{
		error_message = ME6X00_ERR_IBN_MESS;
		return ME6X00_ERR_IBN;
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
				{
					error_message = ME6X00_ERR_NDF_MESS;
					return ME6X00_ERR_NDF;
				}

				case EACCES :
				{
					error_message = ME6X00_ERR_ACS_MESS;
					return ME6X00_ERR_ACS;
				}

				default :
				{
					error_message = ME6X00_ERR_DFP_MESS;
					return ME6X00_ERR_DFP;
				}
			}

		/* Try to open it in non-blocking mode */

		dev_info[ board ].fd = open( name, O_RDWR | O_NONBLOCK );

		if ( dev_info[ board ].fd < 0 )
			switch( errno )
			{
				case ENODEV : case ENXIO :
					error_message = ME6X00_ERR_NDV_MESS;
					return ME6X00_ERR_NDV;

				case EACCES :
					error_message = ME6X00_ERR_ACS_MESS;
					return ME6X00_ERR_ACS;

				case EBUSY :
					error_message = ME6X00_ERR_BSY_MESS;
					return ME6X00_ERR_BSY;

				default :
					error_message = ME6X00_ERR_DFP_MESS;
					return ME6X00_ERR_DFP;
			}

		/* This should never happen and we give up on the board... */

		if ( ioctl( dev_info[ board ].fd, ME6X00_BOARD_INFO,
					&dev_info[ board ].info ) < 0 )
		{
			close( dev_info[ board ].fd );
			error_message = ME6X00_ERR_INT_MESS;
			return ME6X00_ERR_INT;
		}

		/* Set the FD_CLOEXEC bit for the device file - exec()'ed application
		   should have no interest at all in the board (and even don't know
		   that they have the file open) but could interfere seriously with
		   normal operation of the program that did open the device file. */

		fcntl( dev_info[ board ].fd, F_SETFD, FD_CLOEXEC );

		switch ( dev_info[ board ].info.device_ID & 0xF ) 
		{
			case 0x04 :
				dev_info[ board ].num_dacs = 4;
				break;

			case 0x08 :
				dev_info[ board ].num_dacs = 8;
				break;

			case 0x0F :
				dev_info[ board ].num_dacs = 16;
				break;
				break;

			default :
				close( dev_info[ board ].fd );
				dev_info[ board ].fd = -1;
				error_message = ME6X00_ERR_INT_MESS;
				return ME6X00_ERR_INT;
		}

		dev_info[ board ].is_init = 1;
	}

	return ME6X00_OK;
}


/*------------------------------------------------------------------*/
/* Function for checking a DAC parameter passed to one of the other */
/* functions. It relies on check_board() already having been called */
/* before to insure that the board exists. It used the board type   */
/* information to determine if the DAC exists for the board. For    */
/* functions only to be used with the four lowest numbered DACs     */
/* of a ME6100 the argument 'is_me6100_specific' must be set, in    */
/* which case it is also checked that the board is a ME6100. Other- */
/* wise 'is_me6100_specific' must be 0.                             */
/*------------------------------------------------------------------*/

static int check_dac( int board, int dac, int is_me6100_specific )
{
	/* DACs must be in range between DAC00 and DAC15 */

	if ( dac < ME6X00_DAC00 || dac > ME6X00_DAC15 )
	{
		error_message = ME6X00_ERR_DAC_MESS;
		return ME6X00_ERR_DAC;
	}

	if ( is_me6100_specific && ! IS_ME6100( board ) )
	{
		error_message = ME6X00_ERR_NAP_MESS;
		return ME6X00_ERR_NAP;
	}

	/* ME6100 special commands can only be used with the lowest 4 DACs */

	if ( is_me6100_specific && dac > ME6X00_DAC03 )
	{
		error_message = ME6X00_ERR_TDC_MESS;
		return ME6X00_ERR_TDC;
	}

	/* Check that the board has the requested DAC */

	switch ( dev_info[ board ].info.device_ID & 0xF ) 
	{
		case 0x04 :
			if ( dac > ME6X00_DAC03 )
			{
				error_message = ME6X00_ERR_DAC_MESS;
				return ME6X00_ERR_DAC;
			}
			break;

		case 0x08 :
			if ( dac > ME6X00_DAC07 )
			{
				error_message = ME6X00_ERR_DAC_MESS;
				return ME6X00_ERR_DAC;
			}
			break;

		case 0x0F :
			if ( dac > ME6X00_DAC15 )
			{
				error_message = ME6X00_ERR_DAC_MESS;
				return ME6X00_ERR_DAC;
			}
			break;

		default :
			close( dev_info[ board ].fd );
			dev_info[ board ].fd = -1;
			error_message = ME6X00_ERR_INT_MESS;
			return ME6X00_ERR_INT;
	}

	return ME6X00_OK;
}
