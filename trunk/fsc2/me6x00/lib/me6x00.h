/*
  $Id$

  Copyright (C) 2002-2004 Jens Thoms Toerring
 
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


#ifdef __cplusplus
extern "C" {
#endif


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
int me6x00_board_info( int board, me6x00_dev_info **info );
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
int me6x00_perror( const char *s );
const char *me6x00_strerror( void );


extern const char *me6x00_errlist[ ];
extern const int me6x00_nerr;


#define ME6X00_TRIGGER_TIMER     0x00
#define ME6X00_TRIGGER_EXT_LOW   0x10
#define ME6X00_TRIGGER_EXT_HIGH  0x30


#define ME6X00_OK        0
#define ME6X00_ERR_IBN  -1
#define ME6X00_ERR_IFR  -2
#define ME6X00_ERR_BSY  -3
#define ME6X00_ERR_NDF  -4
#define ME6X00_ERR_ACS  -5
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
#define ME6X00_ERR_DFP -16
#define ME6X00_ERR_NDV -17
#define ME6X00_ERR_INT -18


#ifdef __cplusplus
} /* extern "C" */
#endif
