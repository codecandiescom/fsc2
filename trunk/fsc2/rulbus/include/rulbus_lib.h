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


#if ! defined RULBUS_LIB_HEADER
#define RULBUS_LIB_HEADER


#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "rulbus.h"


#if ! defined RULBUS_DEFAULT_CONFIG_FILE
#define RULBUS_DEFAULT_CONFIG_FILE  "/etc/rulbus.conf"
#endif

#if ! defined RULBUS_DEFAULT_DEVICE_FILE
#define RULBUS_DEFAULT_DEVICE_FILE  "/dev/rulbus_epp"
#endif


#ifndef __cplusplus

typedef enum
{
      false = 0,
      true  = 1
} bool;

#endif

#define FAIL    false
#define UNSET   false
#if ! defined FALSE
#define FALSE   false
#endif
#define OK      true
#define SET     true
#if ! defined TRUE
#define TRUE    true
#endif


#define RULBUS_UNIPOLAR       0
#define RULBUS_BIPOLAR        1


#define RULBUS_DEF_RACK_ADDR      0x0F
#define RULBUS_INV_RACK_ADDR      0xFF
#define RULBUS_MIN_CARD_ADDR      0x01
#define RULBUS_MAX_CARD_ADDR      0xfe
#define RULBUS_INV_CARD_ADDR      0xff


#define RB_GENERIC             0       /* card of unspecified type */
#define RB8506_PIA             1       /* unsupported */
#define RB8506_SIFU            2       /* unsupported */
#define RB8506_VIA             3       /* unsupported */
#define RB8509_ADC12           4       /* 12-bit ADC card */
#define RB8510_DAC12           5       /* 12-bit DAC card */
#define RB8513_TIMEBASE        6       /* unsupported */
#define RB8514_DELAY           7       /* Delay card */
#define RB8515_CLOCK           8       /* Clock card */
#define RB8905_ADC12           9       /* unsupported */
#define RB9005_AMPLIFIER      10       /* unsupported */
#define RB9603_MONOCHROMATOR  11       /* unsupported */

#define RULBUS_RB_GENERIC_DEF_ADDR             0x00

#define RULBUS_RB8509_ADC12_DEF_ADDR           0xC0
#define RULBUS_RB8509_ADC12_WIDTH              2
#define RULBUS_RB8509_ADC12_DEF_BIPOLAR        1
#define RULBUS_RB8509_ADC12_DEF_VPB            5.0e-3
#define RULBUS_RB8509_ADC12_DEF_NUM_CHANNELS   8
#define RULBUS_RB8509_ADC12_DEF_EXT_TRIGGER    0
#define RULBUS_RB8509_ADC12_MAX_CHANNELS       8

#define RULBUS_RB8510_DAC12_DEF_ADDR           0xD0
#define RULBUS_RB8510_DAC12_WIDTH              2
#define RULBUS_RB8510_DAC12_DEF_BIPOLAR        1
#define RULBUS_RB8510_DAC12_DEF_VPB            5.0e-3

#define RULBUS_RB8514_DELAY_DEF_ADDR           0xC4
#define RULBUS_RB8514_DELAY_WIDTH              4
#define RULBUS_RB8514_DELAY_DEF_INTR_DELAY     6.0e-8

#define RULBUS_RB8515_CLOCK_DEF_ADDR           0xC8
#define RULBUS_RB8515_CLOCK_WIDTH              1


typedef struct RULBUS_CARD_LIST RULBUS_CARD_LIST;

struct RULBUS_CARD_LIST {
	char *name;
	int type;
	unsigned char rack;
	unsigned char addr;
	unsigned char width;
	struct RULBUS_CARD_HANDLER *handler;
	bool in_use;
	int num_channels;
	double vpb;
	int bipolar;
	int has_ext_trigger;
	double intr_delay;
};

extern RULBUS_CARD_LIST *rulbus_card;

int rulbus_write( int handle, unsigned char offset, unsigned char *data,
				  size_t len );
int rulbus_read( int handle, unsigned char offset, unsigned char *data,
				 size_t len );


/* Internal functions for 12-bit ADC cards (RB8509) */

int rulbus_rb8509_adc12_init( void );
void rulbus_rb8509_adc12_exit( void );
int rulbus_rb8509_adc12_card_init( int handle );
void rulbus_rb8509_adc12_card_exit( int handle );

/* Internal functions for 12-bit DAC cards (RB8510) */

int rulbus_rb8510_dac12_init( void );
void rulbus_rb8510_dac12_exit( void );
int rulbus_rb8510_dac12_card_init( int handle );
void rulbus_rb8510_dac12_card_exit( int handle );

/* Internal functions for delay cards (RB8514) */

int rulbus_rb8514_delay_init( void );
void rulbus_rb8514_delay_exit( void );
int rulbus_rb8514_delay_card_init( int handle );
void rulbus_rb8514_delay_card_exit( int handle );

/* Internal functions for clock cards (RB8515) */

int rulbus_rb8515_clock_init( void );
void rulbus_rb8515_clock_exit( void );
int rulbus_rb8515_clock_card_init( int handle );
void rulbus_rb8515_clock_card_exit( int handle );

/* Internal functions for generic cards (RB_GENERIC) */

int rulbus_generic_init( void );
void rulbus_generic_exit( void );
int rulbus_generic_card_init( int handle );
void rulbus_generic_card_exit( int handle );


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* ! RULBUS_LIB_HEADER */
