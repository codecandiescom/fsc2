/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


#include "fsc2.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "er023m.conf"

const char generic_type[ ] = DEVICE_TYPE;


/* Here values are defined that get returned by the driver in the test run
   when the lock-in can't be accessed - these values must really be
   reasonable ! */

#define ER023M_TEST_SENSITIVITY   0.5
#define ER023M_TEST_TIME_CONSTANT 0.1
#define ER023M_TEST_PHASE         0.0
#define ER023M_TEST_MOD_FREQUENCY 5.0e3
#define ER023M_TEST_MOD_LEVEL     1.0
#define ER023M_TEST_MOD_MODE      1         // this must be INTERNAL, i.e. 1

#define MAX_MOD_FREQ          100000.0
#define MIN_MOD_FREQ          0.001

#define MAX_HARMONIC          2
#define MIN_HARMONIC          1


#define RG_SETTINGS    58
#define MIN_RG         2.0e1
#define MAX_RG         1.0e7

static double rg_list[ RG_SETTINGS ];

#define TC_SETTINGS    20
#define MIN_TC         1.0e-5       /* 10 us */

static double tc_list[ TC_SETTINGS ];


#define MOD_FREQ_SETTINGS  7

static double mod_freq_list[ ] = { 1.0e5, 5.0e4, 2.5e4, 1.25e4, 6.25e3,
								   3.125e3, 1.5625e3 };


#define MAX_MOD_ATT  79
#define MIN_MOD_ATT  0
#define MOD_OFF      80


#define MIN_OF       0
#define MAX_OF       99
#define CENTER_OF    50


/* Declaration of exported functions */

int er023m_init_hook( void );
int er023m_exp_hook( void );
int er023m_end_of_exp_hook( void );
void er023m_exit_hook( void );



typedef struct {
	int device;

	int rg_index;
	bool is_rg;

	int phase;
	bool is_phase;

	int tc_index;
	bool is_tc;

	int mod_freq_index;
	bool is_mod_freq;

	int mod_level_index;
	bool is_mod_level;

	int harmonic;
	bool is_harmonic;

	int of;
	bool is_of;

	int ct;
	bool is_ct;

	int nb;               /* number of bytes send from ADC */
	                      /* recheck whenever CT changes */
} ER023M;


static ER023M er023m;
static ER023M er023m_store;


/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int er023m_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Reset several variables in the structure describing the device */

	er023m.device = -1;

	/* Set up the receiver gain list */

	rg_list[ 0 ]  = MIN_RG;
	rg_list[ RC_SETTINGS- 1 ] = MAX_RG;
	fac = pow( MAX_RG / MIN_RG, 1.0 / ( double ) ( RC_SETTINGS - 1 ) );
	for ( i = 1; i < RC_SETTINGS - 1; i++ )
		rg_list[ i ] = fac * rg_list[ i - 1 ];

	/* Set up the time constant list */

	tc_list[ 0 ] = MIN_TC;
	for ( i = 1; i < TC_SETTINGS; i++ )
		tc_list[ i ] = 2.0 * tc_list[ i - 1 ];

	sr830.rg_index = -1;         /* no sensitivity has to be set at start of */
	sr830.is_rg    = UNSET;
	sr830.rg_warn  = UNSET;      /* experiment and no warning concerning the */
                                 /* sensitivity setting has been printed yet */
	sr830.is_phase = UNSET;

	sr830.tc_index = -1;         /* no time constant has to be set at the */
	sr830.is_tc    = UNSET;
	sr830.tc_warn  = UNSET;      /* start of the experiment and no warning */
	                             /* concerning it has been printed yet */
	sr830.is_mod_freq = UNSET;   /* modulation frequency has not to be set */
	sr830.is_mod_level = UNSET;  /* modulation level has not to be set */
	sr830.is_harmonic = UNSET;   /* harmonics has not to be set */
	sr830.is_of = UNSET;
	sr830.is_ct = UNSET;

	return 1;
}
