/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#if ! defined RS_SPEC10_HEADER
#define RS_SPEC10_HEADER

/* Define the following for test purposes where no real communication with
   the camera is supposed to happen */

/*
#define RS_SPEC10_TEST
*/


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


#if ! defined RS_SPEC10_UPSIDE_DOWN
#define RS_SPEC10_UPSIDE_DOWN 0
#endif

#if ! defined RS_SPEC10_MIRROR
#define RS_SPEC10_MIRROR 0
#endif


struct RS_SPEC10 {
	const char *dev_file;
	int16 handle;
	bool is_open;
	bool lib_is_init;

	struct {
		uns16 max_size[ 2 ];      /* number of pixels in x- and y-direction */
		uns16 roi[ 4 ];
		bool roi_is_set;
		long bin[ 2 ];
		bool bin_is_set;
		bool bin_mode;            /* 0: hardware binning, 1: software bin. */
		bool bin_mode_is_set;
		double exp_res;
		flt64 exp_min_time;
		uns32 exp_time;           /* exposure time in multiples of 1 us */
		uns16 clear_cycles;

		double test_min_exp_time;
	} ccd;

	struct {
		uns16 acc_setpoint;       /* access flags for setpoints */
		uns16 acc_temp;           /* access flags for temperature */

		double setpoint;          /* last requested setpoint */
		bool is_setpoint;         /* has setpoint been set in PREPARATIONS ? */
	} temp;
};


extern struct RS_SPEC10 *rs_spec10,
						 rs_spec10_prep,
						 rs_spec10_test,
						 rs_spec10_exp;


#define RS_SPEC10_TEST_TEMP      180.0    /* in Kelvin */

#define HARDWARE_BINNING         0
#define SOFTWARE_BINNING         1


/* Functions from rs_spec10.c */

int rs_spec10_init_hook( void );
int rs_spec10_test_hook( void );
int rs_spec10_exp_hook( void );
void rs_spec10_child_exit_hook( void );
int rs_spec10_end_of_exp_hook( void );
void rs_spec10_exit_hook( void );

Var_T *ccd_camera_name( Var_T *v );
Var_T *ccd_camera_roi( Var_T *v );
Var_T *ccd_camera_binning( Var_T *v );
Var_T *ccd_camera_binning_method( Var_T *v );
Var_T *ccd_camera_exposure_time( Var_T *v );
Var_T *ccd_camera_clear_cycles( Var_T *v );
Var_T *ccd_camera_get_image( Var_T *v );
Var_T *ccd_camera_get_spectrum( Var_T *v );
Var_T *ccd_camera_temperature( Var_T *v );
Var_T *ccd_camera_pixel_size( Var_T *v );
Var_T *ccd_camera_pixel_area( Var_T *v );

/* Functions from rs_spec10_int.c */

void rs_spec10_init_camera( void );
void rs_spec10_clear_cycles( uns16 cycles );
uns16 *rs_spec10_get_pic( uns32 *size );
double rs_spec10_get_temperature( void );
double rs_spec10_set_temperature( double temp );
void rs_spec10_error_handling( void );

/* Functions from rs_spec10_util.c */

bool rs_spec10_read_state( void );
bool rs_spec10_store_state( void );
double rs_spec10_k2c( double tk );
double rs_spec10_c2k( double tc );
int16 rs_spec10_k2ic( double tk );
double rs_spec10_ic2k( int16 tci );
bool rs_spec10_param_access( uns32 param, uns16 *acc );
const char *rs_spec10_ptime( double p_time );
int *rs_spec10_get_fd_list( void );
void rs_spec10_close_on_exec_hack( int *fd_list );


#endif /* ! RS_SPEC10_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
