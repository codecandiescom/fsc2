/*
  $Id$
 
  Driver for National Instruments DAQ boards

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


#if ! defined NI_DAQ_HEADER
#define NI_DAQ_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/ioctl.h>


/* Define the maximum number of boards that can be used at the same time
   (a simple change of this number allows to recompile the module for a
   different number of boards) */

#define NI_DAQ_MAX_BOARDS         8
#define NI_DAQ_MAX_PCI_E_BOARDS   4
#define NI_DAQ_MAX_AT_MIO_BOARDS  4


typedef enum {
	NI_DAQ_NO_SUBSYSTEM    = -1,
	NI_DAQ_AI_SUBSYSTEM    =  0,
	NI_DAQ_AO_SUBSYSTEM    =  1,
	NI_DAQ_GPCT0_SUBSYSTEM =  2,
	NI_DAQ_GPCT1_SUBSYSTEM =  3
} NI_DAQ_SUBSYSTEM;


typedef enum {
	NI_DAQ_PFI_UNUSED = -1,
	NI_DAQ_PFI_INPUT  =  0,
	NI_DAQ_PFI_OUTPUT =  1
} NI_DAQ_PFI_STATE;


typedef enum {
	NI_DAQ_IN_TIMEBASE1    = 0,
	NI_DAQ_AI_START1_Pulse = 0,
	NI_DAQ_SI_TC           = 0,
	NI_DAQ_SI2_TC          = 0,
	NI_DAQ_SAME_AS_SI      = 0,
	NI_DAQ_DIV_TC          = 0,
	NI_DAQ_AI_IN_TIMEBASE1 = 0,
	NI_DAQ_AO_IN_TIMEBASE1 = 0,
	NI_DAQ_G_IN_TIMEBASE1  = 0,
	NI_DAQ_PFI0            = 1,
	NI_DAQ_TRIG1           = 1,
	NI_DAQ_PFI1            = 2,
	NI_DAQ_TRIG2           = 2,
	NI_DAQ_PFI2            = 3,
	NI_DAQ_CONVERT         = 3,
	NI_DAQ_PFI3            = 4,
	NI_DAQ_GCTR1_SOURCE    = 4,
	NI_DAQ_PFI4            = 5,
	NI_DAQ_GPCTR1_GATE     = 5,
	NI_DAQ_PFI5            = 6,
	NI_DAQ_UPDATE          = 6,
	NI_DAQ_PFI6            = 7,
	NI_DAQ_WFTTRIG         = 7,
	NI_DAQ_PFI7            = 8,
	NI_DAQ_STARTSCAN       = 8,
	NI_DAQ_PFI8            = 9,
	NI_DAQ_GPCTR0_SOURCE   = 9,
	NI_DAQ_PFI9            = 10,
	NI_DAQ_GPCTR0_GATE     = 10,
	NI_DAQ_RTSI_0          = 11,
	NI_DAQ_RTSI_1          = 12,
	NI_DAQ_RTSI_2          = 13,
	NI_DAQ_RTSI_3          = 14,
	NI_DAQ_RTSI_4          = 15,
	NI_DAQ_RTSI_5          = 16,
	NI_DAQ_RTSI_6          = 17,
	NI_DAQ_START2          = 18,
	NI_DAQ_AI_START_Pulse  = 18,
	NI_DAQ_SOURCE_TC_OTHER = 19,
	NI_DAQ_UI2_TC          = 19,
	NI_DAQ_GOUT_0          = 19,
	NI_DAQ_G_TC_OTHER      = 19,
	NI_DAQ_START1          = 21,
	NI_DAQ_LOW             = 31,
	NI_DAQ_IN_TIMEBASE2    = 44,
	NI_DAQ_NONE            = 55,
	NI_DAQ_ALL             = 66,
	NI_DAQ_NOW             = 77,
	NI_DAQ_INTERNAL        = 88
} NI_DAQ_INPUT;


typedef enum {
	NI_DAQ_NORMAL,
	NI_DAQ_INVERTED
} NI_DAQ_POLARITY;


typedef enum {
	NI_DAQ_LEVEL,
	NI_DAQ_EDGE
} NI_DAQ_TRIGGER_MODE;


typedef enum {
	NI_DAQ_DISABLED,
	NI_DAQ_ENABLED
} NI_DAQ_STATE;


typedef enum {
	NI_DAQ_FULL_SPEED,
	NI_DAQ_HALF_SPEED
} NI_DAQ_CLOCK_SPEED_VALUE;


typedef enum {
	NI_DAQ_FAST_CLOCK,
	NI_DAQ_SLOW_CLOCK
} NI_DAQ_CLOCK_TYPE;


typedef enum {
	NI_DAQ_DIO_INPUT,
	NI_DAQ_DIO_OUTPUT
} NI_DAQ_DIO_CMD;


typedef struct {
	NI_DAQ_DIO_CMD cmd;
	unsigned char value;
	unsigned char mask;
} NI_DAQ_DIO_ARG;


typedef enum {
	NI_DAQ_GPCT_SET_CLOCK_SPEED,
	NI_DAQ_GPCT_GET_CLOCK_SPEED,
	NI_DAQ_GPCT_COUNTER_OUTPUT_STATE,
	NI_DAQ_GPCT_START_COUNTER,
	NI_DAQ_GPCT_START_PULSER,
	NI_DAQ_GPCT_GET_COUNT,
	NI_DAQ_GPCT_DISARM_COUNTER,
	NI_DAQ_GPCT_IS_BUSY
} NI_DAQ_GPCT_CMD;


typedef struct {
	NI_DAQ_GPCT_CMD cmd;

	unsigned int counter;
	NI_DAQ_STATE output_state;

	unsigned long low_ticks;
	unsigned long high_ticks;

	NI_DAQ_INPUT gate;
	NI_DAQ_INPUT source;

	NI_DAQ_POLARITY output_polarity;
	NI_DAQ_POLARITY source_polarity;
	NI_DAQ_POLARITY gate_polarity;

	int continuous;
	int wait_for_end;
	unsigned long count;

	NI_DAQ_CLOCK_SPEED_VALUE speed;

	int is_armed;
} NI_DAQ_GPCT_ARG;


typedef enum {
	NI_DAQ_MSC_BOARD_PROPERTIES,
	NI_DAQ_MSC_SET_CLOCK_SPEED,
	NI_DAQ_MSC_CLOCK_OUTPUT,
	NI_DAQ_MSC_GET_CLOCK,
	NI_DAQ_MSC_TRIGGER_STATE
} NI_DAQ_MSC_CMD;


typedef struct {
	char name[ 20 ];

	int num_ai_channels;
	int num_ai_bits;
	int num_ai_ranges;
	double ai_ranges[ 2 ][ 8 ];
	unsigned long ai_time_res;

	int num_ao_channels;
	int num_ao_bits;
	int has_unipolar_ao;
	int ao_has_ext_ref;
} NI_DAQ_BOARD_PROPERTIES;


typedef enum {
	NI_DAQ_MSC_TRIG_LOW_WINDOW = 0,
	NI_DAQ_MSC_TRIG_HIGH_WINDOW = 1,
	NI_DAQ_MSC_TRIG_MIDDLE_WINDOW = 2,
	NI_DAQ_MSC_TRIG_HIGH_HYSTERESIS = 4,
	NI_DAQ_MSC_TRIG_LOW_HYSTERESIS = 6
} NI_DAQ_MSC_TRIG_MODE;


typedef struct {
	NI_DAQ_MSC_CMD cmd;
	NI_DAQ_CLOCK_TYPE clock;
	NI_DAQ_STATE output_state;
	NI_DAQ_CLOCK_SPEED_VALUE speed;
	unsigned int divider;
	NI_DAQ_MSC_TRIG_MODE trigger_mode;
	NI_DAQ_STATE trigger_state;
	NI_DAQ_STATE trigger_output_state;
	NI_DAQ_BOARD_PROPERTIES *properties;
} NI_DAQ_MSC_ARG;


typedef enum {
	NI_DAQ_AI_CHANNEL_SETUP,
	NI_DAQ_AI_SET_CLOCK_SPEED,
	NI_DAQ_AI_GET_CLOCK_SPEED,
	NI_DAQ_AI_ACQ_SETUP,
	NI_DAQ_AI_ACQ_START,
	NI_DAQ_AI_ACQ_WAIT,
	NI_DAQ_AI_ACQ_STOP
} NI_DAQ_AI_CMD;


typedef enum {
	NI_DAQ_AI_TYPE_UNASSIGNED = -1,
	NI_DAQ_AI_TYPE_Calibration = 0,
	NI_DAQ_AI_TYPE_Differential = 1,
	NI_DAQ_AI_TYPE_NRSE = 2,
	NI_DAQ_AI_TYPE_RSE = 3,
	NI_DAQ_AI_TYPE_Aux = 5,
	NI_DAQ_AI_TYPE_Ghost = 7
} NI_DAQ_AI_TYPE;


typedef enum {
	NI_DAQ_GAIN_NOT_AVAIL = -1,
	NI_DAQ_GAIN_0_5 = 0,              /* divide by 2 */          
	NI_DAQ_GAIN_1   = 1,		  /* no amplification */          
	NI_DAQ_GAIN_2   = 2,		  /* amplify by factor of 2 */  
	NI_DAQ_GAIN_5   = 3,		  /* amplify by factor of 5 */  
	NI_DAQ_GAIN_10  = 4,		  /* amplify by factor of 10 */ 
	NI_DAQ_GAIN_20  = 5,		  /* amplify by factor of 20 */ 
	NI_DAQ_GAIN_50  = 6,		  /* amplify by factor of 50 */ 
	NI_DAQ_GAIN_100 = 7,		  /* amplify by factor of 100 */
} NI_DAQ_AI_GAIN_TYPES;


typedef enum {
	NI_DAQ_BIPOLAR,
	NI_DAQ_UNIPOLAR
} NI_DAQ_BU_POLARITY;


typedef struct {
	unsigned int channel;
	NI_DAQ_AI_TYPE channel_type;
	NI_DAQ_STATE generate_trigger;
	NI_DAQ_STATE dither_enable;
	NI_DAQ_BU_POLARITY polarity;
	NI_DAQ_AI_GAIN_TYPES gain;
} NI_DAQ_AI_CHANNEL_ARGS;
		

typedef struct {
	NI_DAQ_INPUT START_source;
	NI_DAQ_INPUT START1_source;
	NI_DAQ_INPUT CONVERT_source;
	NI_DAQ_INPUT SI_source;
	NI_DAQ_INPUT SI2_source;
	
	NI_DAQ_POLARITY START_polarity;
	NI_DAQ_POLARITY START1_polarity;
	NI_DAQ_POLARITY CONVERT_polarity;
	NI_DAQ_POLARITY SI_polarity;

	unsigned long SI_start_delay;
	unsigned long SI_stepping;

	unsigned int SI2_start_delay;
	unsigned int SI2_stepping;

	unsigned long num_scans;
} NI_DAQ_ACQ_SETUP;


typedef struct {
	NI_DAQ_AI_CMD cmd;
	NI_DAQ_CLOCK_SPEED_VALUE speed;
	unsigned int num_channels;
	NI_DAQ_AI_CHANNEL_ARGS *channel_args;
	NI_DAQ_ACQ_SETUP *acq_args;
	size_t num_points;
	void *data;
} NI_DAQ_AI_ARG;


typedef struct {
	NI_DAQ_STATE ground_ref;
	NI_DAQ_STATE external_ref;
	NI_DAQ_STATE reglitch;
	NI_DAQ_BU_POLARITY polarity;
} NI_DAQ_AO_CHANNEL_ARGS;


typedef enum {
	NI_DAQ_AO_CHANNEL_SETUP,
	NI_DAQ_AO_DIRECT_OUTPUT,
} NI_DAQ_AO_CMD;


typedef struct {
	NI_DAQ_AO_CMD cmd;
	unsigned int channel;
	NI_DAQ_AO_CHANNEL_ARGS *channel_args;
	int value;
} NI_DAQ_AO_ARG;


/* Definition of the numbers of all the different ioctl() commands - the
   character 'j' as the 'magic' type seems to be not in use for command
   numbers above 0x3F (see linux/Documentation/ioctl-number.txt). All
   commands need write access to the structure passed to ioctl() because
   status and error states are returned via this structure. */

#define NI_DAQ_MAGIC_IOC    'j'

#define NI_DAQ_IOC_DIO    _IOWR( NI_DAQ_MAGIC_IOC, 0x90, NI_DAQ_DIO_ARG  )
#define NI_DAQ_IOC_GPCT   _IOWR( NI_DAQ_MAGIC_IOC, 0x91, NI_DAQ_GPCT_ARG )
#define NI_DAQ_IOC_MSC    _IOWR( NI_DAQ_MAGIC_IOC, 0x92, NI_DAQ_MSC_ARG  )
#define NI_DAQ_IOC_AI     _IOWR( NI_DAQ_MAGIC_IOC, 0x93, NI_DAQ_AI_ARG   )
#define NI_DAQ_IOC_AO     _IOWR( NI_DAQ_MAGIC_IOC, 0x94, NI_DAQ_AO_ARG   )

#define NI_DAQ_MIN_NR     _IOC_NR( NI_DAQ_IOC_DIO )
#define NI_DAQ_MAX_NR     _IOC_NR( NI_DAQ_IOC_AO  )


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* ! NI_DAQ_HEADER */


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
