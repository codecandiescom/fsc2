/*
  $Id$

  Copyright (C) 2003-2004 Jens Thoms Toerring

  Library for Rulbus (Rijksuniversiteit Leiden BUS)

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  The library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the package; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.

  To contact the author send email to
  Jens.Toerring@physik.fu-berlin.de
*/


#if ! defined RULBUS_HEADER
#define RULBUS_HEADER


#ifdef __cplusplus
extern "C" {
#endif


/* Functions and variables for the whole library */

typedef struct RULBUS_CARD_INFO RULBUS_CARD_INFO;

int rulbus_open( void );
void rulbus_close( void );
int rulbus_perror( const char *s );
const char *rulbus_strerror( void );
int rulbus_card_open( const char *name );
int rulbus_card_close( int handle );
int rulbus_get_card_info( const char *name, RULBUS_CARD_INFO *card_info );

extern int rulbus_errno;

struct RULBUS_CARD_INFO {
	int type;
	int num_channels;
	int ext_trigger;
	int bipolar;
	double volt_per_bit;
	double intr_delay;
};


/* Functions and definitions for the 12-bit ADC card module (RB8509) */

int rulbus_adc12_num_channels( int handle );
int rulbus_adc12_set_channel( int handle, int channel );
int rulbus_adc12_set_gain( int handle, int gain );
int rulbus_adc12_has_external_trigger( int handle );
int rulbus_adc12_set_trigger_mode( int handle, int mode );
int rulbus_adc12_properties( int handle, double *Vmax, double *Vmin,
							 double *dV );
int rulbus_adc12_check_convert( int handle, double *volts );
int rulbus_adc12_convert( int handle, double *volts );

#define RULBUS_ADC12_GAIN_1         1
#define RULBUS_ADC12_GAIN_2         2
#define RULBUS_ADC12_GAIN_4         4
#define RULBUS_ADC12_GAIN_8         8

#define RULBUS_ADC12_INT_TRIG       0
#define RULBUS_ADC12_EXT_TRIG       1


/* Functions and definitions for the 12-bit DAC card module (RB8510) */

int rulbus_dac12_set_voltage( int handle, double volts );
int rulbus_dac12_properties( int handle, double *Vmax, double *Vmin,
							 double *dV );


/* Functions and definitions for the delay card module (RB8514) */

int rulbus_delay_set_clock_frequency( int handle, double freq );
int rulbus_delay_set_delay( int handle, double delay, int force );
int rulbus_delay_set_raw_delay( int handle, unsigned long delay, int force );
int rulbus_delay_set_trigger( int handle, int edge );
int rulbus_delay_set_output_pulse( int handle, int output, int type );
int rulbus_delay_set_output_pulse_polarity( int handle, int type, int pol );
int rulbus_delay_busy( int handle );
int rulbus_delay_software_start( int handle );
double rulbus_delay_get_intrinsic_delay( int handle );


#define RULBUS_DELAY_FALLING_EDGE      1
#define RULBUS_DELAY_RAISING_EDGE      0

#define RULBUS_DELAY_CARD_MAX          ( ( 1 << 24 ) - 1 )

#define RULBUS_DELAY_OUTPUT_1          1
#define RULBUS_DELAY_OUTPUT_2          2
#define RULBUS_DELAY_OUTPUT_BOTH       3

#define RULBUS_DELAY_PULSE_NONE        0
#define RULBUS_DELAY_START_PULSE       1
#define RULBUS_DELAY_END_PULSE         2
#define RULBUS_DELAY_PULSE_BOTH        3

#define RULBUS_DELAY_POLARITY_NEGATIVE 0
#define RULBUS_DELAY_POLARITY_POSITIVE 1


/* Functions and definitions for the clock card module (RB8515) */

int rulbus_clock_set_frequency( int handle, int freq );

#define RULBUS_CLOCK_FREQ_OFF       0
#define RULBUS_CLOCK_FREQ_100Hz     1
#define RULBUS_CLOCK_FREQ_1kHz      2
#define RULBUS_CLOCK_FREQ_10kHz     3
#define RULBUS_CLOCK_FREQ_100kHz    4
#define RULBUS_CLOCK_FREQ_1MHz      5
#define RULBUS_CLOCK_FREQ_10MHz     6
#define RULBUS_CLOCK_FREQ_100MHz    7


/* Functions for the generic card module (RB_GENERIC) */

int rulbus_generic_write( int handle, unsigned char address,
						  unsigned char *data, size_t len );
int rulbus_generic_read( int handle, unsigned char address,
						 unsigned char *data, size_t len );


/* Error codes of the library */

#define RULBUS_OK                             0
#define RULBUS_NO_MEMORY 					 -1
#define RULBUS_CONF_FILE_ACCESS 			 -2
#define RULBUS_CONF_FILE_NAME_INVALID 		 -3
#define RULBUS_CONF_FILE_OPEN_FAIL 			 -4
#define RULBUS_CF_SYNTAX_ERROR 				 -5
#define RULBUS_CF_EOF_IN_COMMENT 			 -6
#define RULBUS_CF_DEV_FILE_DUPLICATE 		 -7
#define RULBUS_CF_RACK_ADDR_DUPLICATE 		 -8
#define RULBUS_CF_RACK_ADDR_INVALID 		 -9
#define RULBUS_CF_RACK_ADDR_CONFLICT 		-10
#define RULBUS_CF_RACK_ADDR_DEF_DUPLICATE 	-11
#define RULBUS_CF_UNSUPPORTED_CARD_TYPE 	-12
#define RULBUS_CF_CARD_NAME_CONFLICT 		-13
#define RULBUS_CF_CARD_ADDR_INVALID 		-14
#define RULBUS_CF_CARD_ADDR_CONFLICT  	    -15
#define RULBUS_CF_CARD_ADDR_DUPLICATE       -16
#define RULBUS_CF_CARD_ADDR_DEF_CONFLICT    -17
#define RULBUS_CF_CARD_ADDR_OVERLAP 		-18
#define RULBUS_CF_CARD_ADDR_GENERIC 		-19
#define RULBUS_CF_CARD_PROPERTY_INVALID 	-20
#define RULBUS_CF_DUPLICATE_NUM_CHANNELS 	-21
#define RULBUS_CF_INVALID_NUM_CHANNELS 		-22
#define RULBUS_CF_VPB_DUPLICATE 			-23
#define RULBUS_CF_INVALID_VPB 				-24
#define RULBUS_CF_BIPLOAR_DUPLICATE 		-25
#define RULBUS_CF_EXT_TRIGGER_DUPLICATE     -26
#define RULBUS_CF_INTR_DELAY_DUPLICATE 		-27
#define RULBUS_CF_INTR_DELAY_INVALID 		-28
#define RULBUS_DEV_FILE_ACCESS 				-29
#define RULBUS_DEV_FILE_NAME_INVALID 		-30
#define RULBUS_DEV_FILE_OPEN_FAIL 			-31
#define RULBUS_DEV_NO_DEVICE                -32
#define RULBUS_NO_INITIALIZATION 			-33
#define RULBUS_INVALID_ARGUMENT 			-34
#define RULBUS_INVALID_CARD_NAME 			-35
#define RULBUS_INVALID_CARD_HANDLE 			-36
#define RULBUS_CARD_NOT_OPEN 				-37
#define RULBUS_INVALID_CARD_OFFSET 			-38
#define RULBUS_WRITE_ERROR 					-39
#define RULBUS_READ_ERROR  					-40
#define RULBUS_CARD_IS_BUSY 				-41
#define RULBUS_INVALID_VOLTAGE 				-42
#define RULBUS_TIME_OUT                     -43
#define RULBUS_NO_CLOCK_FREQ                -44
#define RULBUS_NO_EXT_TRIGGER               -45


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* ! RULBUS_HEADER */
