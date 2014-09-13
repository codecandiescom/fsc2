/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "hjs_fc.conf"


/* Maximum mumber of times we retry to get a consistent reading (i.e. the
   same value for MIN_NUM_IDENTICAL_READINGS times) from the gaussmeter */

#define MAX_GET_FIELD_RETRIES          500


/* Number of identical readings from the gaussmeter that we take to indicate
   that the the field is stable */

#define MIN_NUM_IDENTICAL_READINGS      3


/* Time (in microseconds) that we'll wait between fetching new values
   from the gaussmeter when trying to get a consistent reading */

#define WAIT_LENGTH         100000UL    /* 100 ms */


/* Field value that will be returned during a test run */

#define HJS_FC_TEST_FIELD      3300.0      /* in Gauss */

#define HJS_FC_TEST_B0V        2900.0      /* in Gauss */
#define HJS_FC_TEST_SLOPE        50.0      /* in Gauss/V */


struct HJS_FC {
    double B0V;             /* Field for DAC voltage of 0 V */
    double slope;           /* field step for 1 V DAC voltage increment */

    double min_test_field;  /* minimum and maximum field during test run */
    double max_test_field;

    double B_max;           /* maximum and minimum field the magnet can */
    double B_min;           /* produce */

    double field;           /* the start field given by the user */
    double field_step;      /* the field steps to be used for sweeps */

    bool is_field;          /* flag, set if start field is defined */
    bool is_field_step;     /* flag, set if field step size is defined */

    bool use_calib_file;    /* set when calibration file is to be used */
    char *calib_file;       /* name of (optional) calibration file */

    char *dac_func;
    char *gm_gf_func;
    char *gm_res_func;

    double dac_min_volts;
    double dac_max_volts;
    double dac_resolution;

    double cur_volts;       /* current voltage at the DAC output */
    double act_field;       /* used internally */
    double target_field;
};


extern struct HJS_FC hjs_fc;


/* Exported functions */

int hjs_fc_init_hook(        void );
int hjs_fc_test_hook(        void );
int hjs_fc_exp_hook(         void );
void hjs_fc_exit_hook(       void );
void hjs_fc_child_exit_hook( void );


Var_T * magnet_calibration_file( Var_T * /* v */ );
Var_T * magnet_name(             Var_T * /* v */ );
Var_T * magnet_setup(            Var_T * /* v */ );
Var_T * magnet_field(            Var_T * /* v */ );
Var_T * set_field(               Var_T * /* v */ );
Var_T * get_field(               Var_T * /* v */ );
Var_T * sweep_up(                Var_T * /* v */ );
Var_T * sweep_down(              Var_T * /* v */ );
Var_T * magnet_sweep_up(         Var_T * /* v */ );
Var_T * magnet_sweep_down(       Var_T * /* v */ );
Var_T * reset_field(             Var_T * /* v */ );
Var_T * magnet_reset_field(      Var_T * /* v */ );
Var_T * magnet_B0(               Var_T * /* v */ );
Var_T * magnet_slope(            Var_T * /* v */ );


void hjs_fc_read_calibration( void );


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 * tab-width: 4
 * indent-tabs-mode: nil
 */

