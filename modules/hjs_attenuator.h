/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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

#include "hjs_attenuator.conf"


typedef struct HJS_Attenuator HJS_Attenuator_T;
typedef struct Att_Table_Entry Att_Table_Entry_T;


struct Att_Table_Entry {
    double att;
    double step;
};

struct HJS_Attenuator {
    int                 sn;            /* serial port index */
    bool                is_open;       /* set if serial port has been opened */
    struct termios    * tio;           /* serial port interface structure */
    double              att;           /* current attenuation */
    long                step;          /* current stepper motor position */
    bool                is_step;       /* set when initial position of stepper
                                          motor has been set */
    char              * calib_file;    /* name of att. calibration file */
    Att_Table_Entry_T * att_table;     /* (sorted) array of attenuation/
                                          position of motor settings */
    size_t              att_table_len; /* length of this array */
    double              min_table_att; /* lowest attenuation in array */
    double              max_table_att; /* largest attenuation in array */
};


extern HJS_Attenuator_T hjs_attenuator;


/* Exported functions */

int hjs_attenuator_init_hook(        void );
int hjs_attenuator_test_hook(        void );
int hjs_attenuator_exp_hook(         void );
int hjs_attenuator_end_of_exp_hook(  void );
void hjs_attenuator_exit_hook(       void );
void hjs_attenuator_child_exit_hook( void );


Var_T * mw_attenuator_name(                Var_T * /* v */ );
Var_T * mw_attenuator_load_calibration(    Var_T * /* v */ );
Var_T * mw_attenuator_initial_attenuation( Var_T * /* v */ );
Var_T * mw_attenuator_attenuation(         Var_T * /* v */ );


/* Functions from hjs_attenuator_lexer.l */

void hjs_attenuator_read_calibration( FILE * /* fp */ );


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
