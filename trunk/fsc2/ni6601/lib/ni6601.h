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


#include <ni6601_drv.h>
#include <math.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


typedef struct {
    int is_init;              /* has board been used so far? */
    int fd;                   /* file descriptor for board */
	int is_used[ 4 ];
	int state[ 4 ];
} NI6601_Device_Info;


enum {
	NI6601_IDLE = 0,
	NI6601_BUSY,
	NI6601_PULSER_RUNNING,
	NI6601_CONT_PULSER_RUNNING,
	NI6601_COUNTER_RUNNING,
	NI6601_CONT_COUNTER_RUNNING
};


int ni6601_close( int board );
int ni6601_start_counter( int board, int counter, int source );
int ni6601_start_gated_counter( int board, int counter, double gate_length,
								int source );
int ni6601_stop_counter( int board, int counter );
int ni6601_get_count( int board, int counter, int wait_for_end,
					  unsigned long *count, int *state );
int ni6601_generate_continuous_pulses( int board, int counter,
									   double high_phase, double low_phase );
int ni6601_stop_pulses( int board, int counter );
int ni6601_generate_single_pulse( int board, int counter, double duration );
int ni6601_dio_write( int board, unsigned char bits, unsigned char mask );
int ni6601_dio_read( int board, unsigned char *bits, unsigned char mask );
int ni6601_is_counter_armed( int board, int counter, int *state );


#define NI6601_OK        0

#define NI6601_ERR_NSB  -1
#define NI6601_ERR_NSC  -2
#define NI6601_ERR_CBS  -3
#define NI6601_ERR_IVA  -4
#define NI6601_ERR_WFC  -5
#define NI6601_ERR_BBS  -6
#define NI6601_ERR_IVS  -7
#define NI6601_ERR_BNO  -8
#define NI6601_ERR_NDV  -9
#define NI6601_ERR_NCB -10
#define NI6601_ERR_ITR -11
#define NI6601_ERR_ACS -12
#define NI6601_ERR_DFM -13
#define NI6601_ERR_DFP -14

#define NI6601_ERR_INT -15

#define NI6601_ERR_NSB_MESS  "No such board"
#define NI6601_ERR_NSC_MESS  "No such counter"
#define NI6601_ERR_CBS_MESS  "Counter is busy"
#define NI6601_ERR_IVA_MESS  "Invalid argument"
#define NI6601_ERR_WFC_MESS  "Can't wait for continuous counter to stop"
#define NI6601_ERR_BBS_MESS  "Board is busy"
#define NI6601_ERR_IVS_MESS  "Source argument invalid"
#define NI6601_ERR_BNO_MESS  "Board not open"
#define NI6601_ERR_NDV_MESS  "No driver loaded for board"
#define NI6601_ERR_NCB_MESS  "Neighbouring counter is busy"
#define NI6601_ERR_ITR_MESS  "Interrupted by signal"
#define NI6601_ERR_ACS_MESS  "No permissions to open file"
#define NI6601_ERR_DFM_MESS  "Device file does not exist"
#define NI6601_ERR_DFP_MESS  "Unspecified error when opening device file"

#define NI6601_ERR_INT_MESS  "Internal driver or library error"
