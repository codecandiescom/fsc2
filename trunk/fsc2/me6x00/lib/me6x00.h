/*
  $Id$

  Copyright (C) 2002 Jens Thoms Toerring
 
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


#include <me6x00_drv.h>


typedef struct {
	me6x00_dev_info info;     /* structure with informations about board */
	int is_init;              /* has board been used so far? */
	int fd;                   /* file descriptor for board */
	int num_dacs;             /* number of DACs on the board */
} ME6X00_Device_Info;


unsigned int me6x00_frequency_to_timer( double freq );
int me6x00_board_type( int board, unsigned int *type );
int me6x00_num_dacs( int board, unsigned int *num_dacs );
int me6x00_serial_number( int board, unsigned int *serial_no );
const me6x00_dev_info *me6x00_board_info( int board );
const char *me6x00_error_message( void );
int me6x00_close( int board );
int me6x00_voltage( int board, int dac, double volts );
int me6x00_keep_voltage( int board, int dac, int state );

int me6x00_continuous( int board, int dac, int size, unsigned short *buf );
int me6x00_continuous_ex( int board, int dac, int size, unsigned short *buf );
int me6x00_reset( int board, int dac );
int me6x00_reset_all( int board );
int me6x00_set_timer( int board, int dac, unsigned int ticks );
int me6x00_set_trigger( int board, int dac, int mode );
int me6x00_single( int board, int dac, unsigned short val );
int me6x00_start( int board, int dac );
int me6x00_stop( int board, int dac );
int me6x00_stop_ex( int board, int dac );
int me6x00_wraparound( int board, int dac, int size, unsigned short *buf );


#define ME6X00_TRIGGER_TIMER     0x00
#define ME6X00_TRIGGER_EXT_LOW   0x10
#define ME6X00_TRIGGER_EXT_HIGH  0x30


#define ME6X00_ERR_IBN  -1
#define ME6X00_ERR_NSB  -2
#define ME6X00_ERR_IFR  -3
#define ME6X00_ERR_BSY  -4
#define ME6X00_ERR_NDF  -5
#define ME6X00_ERR_NAP  -6
#define ME6X00_ERR_DAC  -7
#define ME6X00_ERR_TDC  -8
#define ME6X00_ERR_TCK  -9
#define ME6X00_ERR_TRG -10
#define ME6X00_ERR_VLT -11
#define ME6X00_ERR_IBA -12
#define ME6X00_ERR_IBS -13
#define ME6X00_ERR_BNO -14
#define ME6X00_ERR_ABS -15

#define ME6X00_ERR_INT -17

#define ME6X00_ERR_IBN_MESS "Invalid board number"
#define ME6X00_ERR_NSB_MESS "No such board"
#define ME6X00_ERR_IFR_MESS "Invalid frequency"
#define ME6X00_ERR_BSY_MESS "Board is busy"
#define ME6X00_ERR_NDF_MESS "No device file"
#define ME6X00_ERR_NAP_MESS "Command can't be used with ME6X00 boards"
#define ME6X00_ERR_DAC_MESS "Invalid DAC channel number"
#define ME6X00_ERR_TDC_MESS "Invalid DAC channel for requested operation"
#define ME6X00_ERR_TCK_MESS "Ticks too low (frequency too high)"
#define ME6X00_ERR_TRG_MESS "Invalid trigger mode parameter"
#define ME6X00_ERR_VLT_MESS "Invalid voltage"
#define ME6X00_ERR_IBA_MESS "Invalid buffer address"
#define ME6X00_ERR_IBS_MESS "Invalid buffer size"
#define ME6X00_ERR_BNO_MESS "Board not open"
#define ME6X00_ERR_ABS_MESS "Operation aborted due to signal"


#define ME6X00_ERR_INT_MESS "Internal library error"
