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



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#if ! defined RULBUS_DEFAULT_CONFIG_FILE
#define RULBUS_DEFAULT_CONFIG_FILE  "/etc/rulbus.conf"
#endif

#if ! defined RULBUS_DEFAULT_DEVICE_FILE
#define RULBUS_DEFAULT_DEVICE_FILE  "/dev/rulbus"
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



#define RULBUS_MAX_CARDS         256     /* maximum total number of cards */
#define RULBUS_MAX_RACK_NO      0x0f     /* maximum number of racks */
#define RULBUS_MAX_CARDS_IN_RACK  16     /* maximum number of cards in rack */
#define RULBUS_MIN_CARD_ADDR    0x01
#define RULBUS_MAX_CARD_ADDR    0xfe
#define RULBUS_INVALID_ADDR     0xff



enum {
	RB8509 = 8509,      /* 12 bit ADC */
	RB8510 = 8510,      /* 12 bit DAC */
    RB8514 = 8514,      /* Delay card */
	RB8515 = 8515,      /* Clock card */
};

#define RB8509_WIDTH 2
#define RB8510_WIDTH 4
#define RB8514_WIDTH 4
#define RB8515_WIDTH 1


typedef struct RULBUS_CARD_LIST RULBUS_CARD_LIST;

struct RULBUS_CARD_LIST {
	char *name;
	int type;
	unsigned char rack;
	unsigned char addr;
	unsigned char width;
	bool in_use;
};


extern int rulbus_errno;

int rulbus_open( void );
int rulbus_close( void );
int rulbus_perror( const char *s );
const char *rulbus_strerror( void );
int rulbus_card_open( const char *name );
int rulbus_card_close( int handle );
int rulbus_write( int handle, unsigned char offset, unsigned char data );
int rulbus_read( int handle, unsigned char offset, unsigned char *data );


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
#define RULBUS_CRD_CNT  -14
#define RULBUS_NO_CRD   -15
#define RULBUS_CRD_NAM  -16
#define RULBUS_INV_TYP  -17
#define RULBUS_ADD_CFL  -18
#define RULBUS_NO_INIT  -19
#define RULBUS_INV_ARG  -20
#define RULBUS_INV_CRD  -21
#define RULBUS_INV_HND  -22
#define RULBUS_CRD_NOP  -23
#define RULBUS_INV_OFF  -24
#define RULBUS_WRT_ERR  -25
#define RULBUS_RD_ERR   -26
