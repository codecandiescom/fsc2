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


#if ! defined RS_SPEC10__HEADER
#define RS_SPEC10_HEADER


/* The following sequence of includes and undefining BIG_ENDIAN is
   required to avoid some compiler warnings. The include files coming
   with the PVCAM library must go first - make sure the include path
   has been set up correctly (per default installation the header
   files go into the rather unusual location /usr/local/pvcam/examples).
   This must be done by setting the variable 'rs_spec10_incl' in the
   Makefile to the path where the include files reside. */

#include <master.h>
#include <pvcam.h>

#ifdef BIG_ENDIAN
#undef BIG_ENDIAN
#endif

#include "fsc2_module.h"
#include "rs_spec10.conf"


typedef struct
{
	const char *dev_file;
	int16 handle;
	bool is_open;
	bool lib_is_init;

	struct {
		uns16 max_size[ 2 ];      /* number of pixels in x- and y-direction */
		uns16 roi[ 4 ];
		long bin[ 2 ];
		bool bin_mode;            /* 0: hardware binning, 1: software bin. */
		uns32 exp_time;           /* exposure time in multiples of 1 us */
	} ccd;

	struct {
		uns16 acc_setpoint;       /* access flags for setpoints */
		uns16 acc_temp;           /* access flags for temperature */

		double max;               /* maximum setpoint temperature */
		double min;               /* minimum setpoint temperature */

		double cur;               /* current temperature */

		double setpoint;          /* last requested setpoint */
		bool is_setpoint;         /* has setpoint been set in PREPARATIONS ? */

		double test_min_setpoint; /* maximum setpoint in TEST run */
		double test_max_setpoint; /* minimum setpoint in TEST run */
	} temp;
} RS_SPEC10;



#if defined RS_SPEC10_MAIN

RS_SPEC10 *rs_spec10, rs_spec10_prep, rs_spec10_test, rs_spec10_exp;

#else

extern RS_SPEC10 *rs_spec10, rs_spec10_prep, rs_spec10_test, rs_spec10_exp;

#endif


#define RS_SPEC10_TEST_TEMP      120.0

#define HARDWARE_BINNING         0
#define SOFTWARE_BINNING         1

#define CCD_EXPOSURE_RESOLUTION   1.0e-6       /*   1 us */


/* Functions from rs_spec10.c */

int rs_spec10_init_hook( void );
int rs_spec10_test_hook( void );
int rs_spec10_exp_hook( void );
int rs_spec10_end_of_exp_hook( void );
void rs_spec10_exit_hook( void );

Var *ccd_camera_name( Var *v );
Var *ccd_camera_roi( Var *v );
Var *ccd_camera_binning( Var *v );
Var *ccd_camera_binning_method( Var *v );
Var *ccd_camera_exposure_time( Var *v );
Var *ccd_camera_get_pic( Var *v );
Var *ccd_camera_temperature( Var *v );

/* Functions from rs_spec10_int.c */

void rs_spec10_init_camera( void );
uns16 *rs_spec10_get_pic( void );
double rs_spec10_get_temperature( void );
double rs_spec10_set_temperature( double temp );
void rs_spec10_error_handling( void );

/* Functions from rs_spec10_util.c */

double rs_spec10_k2c( double tk );
double rs_spec10_c2k( double tc );
int16 rs_spec10_k2ic( double tk );
double rs_spec10_ic2k( int16 tci );
bool rs_spec10_param_access( uns32 param, uns16 *access );


#endif /* ! RS_SPEC10_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
