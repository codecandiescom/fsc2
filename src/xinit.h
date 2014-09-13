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


#if ! defined XINIT_HEADER
#define XINIT_HEADER


typedef struct G_Funcs G_Funcs_T;

struct G_Funcs {
    bool               size;
    FD_fsc2 *          ( * create_form_fsc2       ) ( void );
    FD_run_1d *        ( * create_form_run_1d     ) ( void );
    FD_run_2d *        ( * create_form_run_2d     ) ( void );
    FD_input_form *    ( * create_form_input_form ) ( void );
    FD_print *         ( * create_form_print      ) ( void );
    FD_cut *           ( * create_form_cut        ) ( void );
    FD_print_comment * ( * create_pc_form         ) ( void );
};

bool xforms_init( int *  /* argc */,
                  char * /* argv */ [ ] );

void xforms_close( void );

int is_iconic( Display * /* d */,
               Window    /* w */  );


#endif   /* ! XINIT__HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
