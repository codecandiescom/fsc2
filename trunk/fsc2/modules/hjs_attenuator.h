/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "hjs_attenuator.conf"


typedef struct HJS_ATTENUATOR HJS_ATTENUATOR;
typedef struct HJS_ATT_TABLE_ENTRY HJS_ATT_TABLE_ENTRY;


#define HJS_ATTENTUATOR_WAIT_PER_STEP   2000   /* 2 ms */

struct HJS_ATT_TABLE_ENTRY {
	double att;
	double step;
};

struct HJS_ATTENUATOR {
	bool is_open;
    struct termios *tio;    /* serial port terminal interface structure */
	double att;
	long step;
	bool is_step;

	char *table_file;               /* name of attenuation table file */
	HJS_ATT_TABLE_ENTRY *att_table;
	size_t att_table_len;
	double min_table_att;
	double max_table_att;
};


extern HJS_ATTENUATOR hjs_attenuator;



/* Exported functions */

int hjs_attenuator_init_hook( void );
int hjs_attenuator_test_hook( void );
int hjs_attenuator_exp_hook( void );
int hjs_attenuator_end_of_exp_hook( void );
void hjs_attenuator_exit_hook( void );


Var *mw_attenuator_name( Var *v );
Var *mw_attenuator_use_table( Var *v );
Var *mw_attenuator_initial_attenuation( Var *v );
Var *mw_attenuator_attenuation( Var *v );

void hjs_attenuator_read_table( FILE *fp );
