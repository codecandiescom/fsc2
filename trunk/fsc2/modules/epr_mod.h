/*
 *  $Id
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


#if ! defined EPR_MOD_HEADER
#define EPR_MOD_HEADER


#include "fsc2_module.h"
#include "epr_mod.conf"


typedef struct {
	double freq;
	double ratio;
	bool   is_phase;
	double phase;
} FREQ_ENTRY_T;


typedef struct Calibration {
	char         * name;
	size_t         count;               /* number of frequency entries */
	FREQ_ENTRY_T * fe;
    bool           interpolate;
    bool           extrapolate;
	double         r2;
	double         slope;
	double         offset;
} Calibration_T;


typedef struct EPR_MOD {
	char          * state_file;
	Calibration_T * calibrations;
	size_t          count;
} EPR_MOD;


extern EPR_MOD epr_mod,
               epr_mod_stored;


int epr_mod_init_hook( void );
int epr_mod_test_hook( void );
int epr_mod_exp_hook( void );
int epr_mod_exit_hook( void );

Var_T * epr_modulation_name(                        Var_T * /* v */ );
Var_T * epr_modulation_ratio(                       Var_T * /* v */ );
Var_T * epr_modulation_phase(                       Var_T * /* v */ );
Var_T * epr_modulation_has_phase(                   Var_T * /* v */ );
Var_T * epr_modulation_add_calibration(             Var_T * /* v */ );
Var_T * epr_modulation_delete_calibration(          Var_T * /* v */ );
Var_T * epr_modulation_calibration_count(           Var_T * /* v */ );
Var_T * epr_modulation_calibration_name(            Var_T * /* v */ );
Var_T * epr_modulation_calibration_interpolate(     Var_T * /* v */ );
Var_T * epr_modulation_calibration_can_interpolate( Var_T * /* v */ );
Var_T * epr_modulation_calibration_extrapolate(     Var_T * /* v */ );
Var_T * epr_modulation_calibration_can_extrapolate( Var_T * /* v */ );
Var_T * epr_modulation_calibration_frequencies(     Var_T * /* v */ );
Var_T * epr_modulation_store(                       Var_T * /* v */ );


void epr_mod_read_state( void );

Calibration_T * epr_mod_find( Var_T * /* v */ );
FREQ_ENTRY_T * epr_mod_find_fe( Calibration_T * /* res */,
                                double        /* freq */ );
void epr_mod_save( void );
void epr_mod_clear( EPR_MOD * /* em */ );
void epr_mod_copy_state( EPR_MOD * /* to */,
                         EPR_MOD * /* from */ );
void epr_mod_recalc( Calibration_T * /* res */ );
int epr_mod_comp( const void * /* a */,
                  const void * /* b */ );


#endif /* ! EPR_MOD_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
