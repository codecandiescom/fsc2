/*
  $Id$

  Copyright (C) 2003 Jens Thoms Toerring

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


/* Functions and variables for the whole library */

int rulbus_open( void );
void rulbus_close( void );
int rulbus_perror( const char *s );
const char *rulbus_strerror( void );
int rulbus_card_open( const char *name );
int rulbus_card_close( int handle );

extern int rulbus_errno;


/* Functions and definitions for the delay card module (RB8514) */

int rulbus_delay_set_delay( int handle, unsigned long delay );
int rulbus_delay_set_trigger( int handle, int edge );
int rulbus_delay_set_output_pulse( int handle, int output, int type );
int rulbus_delay_set_output_pulse_polarity( int handle, int type, int pol );
int rulbus_delay_software_start( int handle );

#define RULBUS_DELAY_FALLING_EDGE      1
#define RULBUS_DELAY_RAISING_EDGE      0

#define RULBUS_DELAY_CARD_MAX          ( ( 1 << 24 ) - 1 )

#define RULBUS_DELAY_OUTPUT_1          1
#define RULBUS_DELAY_OUTPUT_2          2
#define RULBUS_DELAY_OUTPUT_BOTH       3

#define RULBUS_DELAY_START_PULSE       1
#define RULBUS_DELAY_END_PULSE         2
#define RULBUS_DELAY_PULSE_BOTH        3

#define RULBUS_DELAY_POLARITY_NEGATIVE 0
#define RULBUS_DELAY_POLARITY_POSITIVE 1



/* Functions and definitions for the card card module (RB8515) */

int rulbus_clock_set_frequency( int handle, int freq );

#define RULBUS_CLOCK_FREQ_OFF       0
#define RULBUS_CLOCK_FREQ_100Hz     1
#define RULBUS_CLOCK_FREQ_1kHz      2
#define RULBUS_CLOCK_FREQ_10kHz     3
#define RULBUS_CLOCK_FREQ_100kHz    4
#define RULBUS_CLOCK_FREQ_1MHz      5
#define RULBUS_CLOCK_FREQ_10MHz     6
#define RULBUS_CLOCK_FREQ_100MHz    7


/* Error codes of the library */

#define RULBUS_OK         0
#define RULBUS_OPN_CFG   -1
#define RULBUS_OPN_DVF   -2
#define RULBUS_STX_ERR   -3
#define RULBUS_NO_MEM    -4
#define RULBUS_RCK_CNT   -5
#define RULBUS_RCK_DUP   -6
#define RULBUS_NO_RCK    -7
#define RULBUS_INV_RCK   -8
#define RULBUS_DVF_CNT   -9
#define RULBUS_MAX_CRD  -10
#define RULBUS_ADD_DUP  -11
#define RULBUS_TYP_DUP  -12
#define RULBUS_CRD_ADD  -13
#define RULBUS_NAM_DUP  -14
#define RULBUS_CRD_CNT  -15
#define RULBUS_NO_CRD   -16
#define RULBUS_CRD_NAM  -17
#define RULBUS_INV_TYP  -18
#define RULBUS_ADD_CFL  -19
#define RULBUS_TYP_HND  -20
#define RULBUS_NO_INIT  -21
#define RULBUS_INV_ARG  -22
#define RULBUS_INV_CRD  -23
#define RULBUS_INV_HND  -24
#define RULBUS_CRD_NOP  -25
#define RULBUS_INV_OFF  -26
#define RULBUS_WRT_ERR  -27
#define RULBUS_RD_ERR   -28
#define RULBUS_CRD_BSY  -29
#define RULBUS_INV_DEL  -30
