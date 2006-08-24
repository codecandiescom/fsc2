/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "hjs_sfc.conf"


struct HJS_SFC {
    double B0V;             /* Field for DAC voltage of 0 V */
    double slope;           /* field step for 1 V DAC voltage increment */

    bool B0V_has_been_used;
    bool slope_has_been_used;

    double field;           /* the start field given by the user */
    double field_step;      /* the field steps to be used */

    bool is_field;          /* flag, set if start field is defined */
    bool is_field_step;     /* flag, set if field step size is defined */

    char *dac_func;
    double dac_range;

    double act_field;       /* used internally */
    bool is_act_field;

    char *calib_file;
};


extern struct HJS_SFC hjs_sfc;


/* Exported functions */

int hjs_sfc_init_hook(       void );
int hjs_sfc_test_hook(       void );
int hjs_sfc_exp_hook(        void );
int hjs_sfc_end_of_exp_hook( void );
void hjs_sfc_exit_hook(      void );

Var_T *magnet_name(             Var_T * /* v */ );
Var_T *magnet_setup(            Var_T * /* v */ );
Var_T *magnet_field(            Var_T * /* v */ );
Var_T *set_field(               Var_T * /* v */ );
Var_T *get_field(               Var_T * /* v */ );
Var_T *sweep_up(                Var_T * /* v */ );
Var_T *sweep_down(              Var_T * /* v */ );
Var_T *magnet_sweep_up(         Var_T * /* v */ );
Var_T *magnet_sweep_down(       Var_T * /* v */ );
Var_T *reset_field(             Var_T * /* v */ );
Var_T *magnet_reset_field(      Var_T * /* v */ );
Var_T *magnet_B0(               Var_T * /* v */ );
Var_T *magnet_slope(            Var_T * /* v */ );
Var_T *magnet_calibration_file( Var_T * /* v */ );


void hjs_sfc_read_calibration( void );


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 * tab-width: 4
 * indent-tabs-mode: nil
 */

