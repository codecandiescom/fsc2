/* Device driver for Meilhaus me6x00 boards
 * ========================================
 *
 *   Copyright (C) 2001 Meilhaus Electronic GmbH (support@meilhaus.de)
 *
 *   This file is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Source File : me6x00_drv.h
 * Destination : me6x00.o
 * Author      : GG (Guenter Gebhardt)
 *
 * File History: Version   Date       Editor   Action
 *---------------------------------------------------------------------
 *               1.00.00   02.01.21   GG       first release
 *               1.01.00   02.10.17   JTT      clean up and bug fixes
 *---------------------------------------------------------------------
 *
 * Description:
 *
 *   Header file for Meilhaus ME-6000 and ME-61000 driver module
 *   to be shared between kernel module and userland library (and
 *   programs using that library).
 *
 */



#if ! defined ME6X00_DRV_HEADER
#define ME6X00_DRV_HEADER


#ifdef __cplusplus
extern "C" {
#endif


#include <linux/ioctl.h>


/* Maximum count of ME-6x00 devices */

#define ME6X00_MAX_BOARDS 4


/* Fifo size in values of shorts (i.e. 2-byte values) */

#define ME6X00_FIFO_SIZE 0x2000


/* Minimum Timer Divisor */

#define ME6X00_MIN_TICKS 66


typedef enum {
	ME6X00_SINGLE,
	ME6X00_WRAPAROUND,
	ME6X00_CONTINUOUS
} me6x00_mode_et;


typedef enum {
	ME6X00_START,
	ME6X00_STOP
} me6x00_stasto_et;


typedef enum {
	ME6X00_ENABLE,
	ME6X00_DISABLE
} me6x00_endis_et;


typedef enum {
	ME6X00_RISING,
	ME6X00_FALLING
} me6x00_rifa_et;


typedef enum {
	ME6X00_DONT_KEEP,
	ME6X00_KEEP
} me6x00_keep_et;

typedef enum { 
	ME6X00_DAC00,
	ME6X00_DAC01,
	ME6X00_DAC02,
	ME6X00_DAC03,
	ME6X00_DAC04,
	ME6X00_DAC05,
	ME6X00_DAC06,
	ME6X00_DAC07,
	ME6X00_DAC08,
	ME6X00_DAC09,
	ME6X00_DAC10,
	ME6X00_DAC11,
	ME6X00_DAC12,
	ME6X00_DAC13,
	ME6X00_DAC14,
	ME6X00_DAC15
} me6x00_dac_et;


typedef struct {
	me6x00_dac_et  dac;
	me6x00_mode_et mode;
} me6x00_mode_st;


typedef struct {
	me6x00_dac_et    dac;
	me6x00_stasto_et stasto;
} me6x00_stasto_st;


typedef struct {
	me6x00_dac_et   dac;
	me6x00_endis_et endis;
} me6x00_endis_st;


typedef struct {
	me6x00_dac_et   dac;
	me6x00_rifa_et  rifa;
} me6x00_rifa_st;


typedef struct {
	me6x00_dac_et   dac;
	unsigned int    divisor;
} me6x00_timer_st;


typedef struct {
	me6x00_dac_et   dac;
	unsigned short  value;
} me6x00_single_st;


typedef struct {
	me6x00_dac_et   dac;
	me6x00_keep_et  value;
} me6x00_keep_st;


typedef struct {
	unsigned int vendor_ID;
	unsigned int device_ID;
	unsigned int serial_no;
	unsigned int hw_revision;
	int bus_no;
	int dev_no;
	int func_no;
} me6x00_dev_info;


typedef struct {
	me6x00_dac_et   dac;    /* Selects the DAC                         */
	unsigned short  *buf;   /* Pointer to user data                    */
	int             count;  /* Count of short values                   */
	int             ret;    /* Returns number of values really written */
} me6x00_write_st;


/* IOCTL's */

#define ME6X00_MAGIC 'r'
#define ME6X00_SET_MODE           _IOW  ( ME6X00_MAGIC, 32, me6x00_mode_st   )
#define ME6X00_START_STOP_CONV    _IOW  ( ME6X00_MAGIC, 33, me6x00_stasto_st )
#define ME6X00_CLEAR_ENABLE_FIFO  _IOW  ( ME6X00_MAGIC, 34, me6x00_endis_st  )
#define ME6X00_ENDIS_EXTRIG       _IOW  ( ME6X00_MAGIC, 35, me6x00_endis_st  )
#define ME6X00_RIFA_EXTRIG        _IOW  ( ME6X00_MAGIC, 36, me6x00_rifa_st   )
#define ME6X00_SET_TIMER          _IOW  ( ME6X00_MAGIC, 37, me6x00_timer_st  )
#define ME6X00_WRITE_SINGLE       _IOW  ( ME6X00_MAGIC, 38, me6x00_single_st )
#define ME6X00_WRITE_CONTINUOUS   _IOWR ( ME6X00_MAGIC, 39, me6x00_write_st  )
#define ME6X00_WRITE_WRAPAROUND   _IOWR ( ME6X00_MAGIC, 40, me6x00_write_st  )
#define ME6X00_RESET_BOARD        _IO   ( ME6X00_MAGIC, 41                   )
#define ME6X00_BOARD_INFO         _IOR  ( ME6X00_MAGIC, 42, me6x00_dev_info  )
#define ME6X00_KEEP_VOLTAGE       _IOW  ( ME6X00_MAGIC, 43, me6x00_keep_st   )



#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* ! ME6X00_DRV_HEADER */


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
