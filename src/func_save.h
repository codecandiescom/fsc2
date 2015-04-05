/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


#if ! defined FUNC_SAVE_HEADER
#define FUNC_SAVE_HEADER


#include "fsc2.h"
#include <zlib.h>


typedef struct File_List File_List_T;

struct File_List {
    FILE   * fp;
    gzFile   gp;
    char   * name;
    bool     gzip;
};


Var_T * f_openf(     Var_T * /* v */ );
Var_T * f_opengzf(   Var_T * /* v */ );
Var_T * f_getf(      Var_T * /* v */ );
Var_T * f_getgzf(    Var_T * /* v */ );
Var_T * f_clonef(    Var_T * /* v */ );
Var_T * f_clonegzf(  Var_T * /* v */ );
Var_T * f_resetf(    Var_T * /* v */ );
Var_T * f_save(      Var_T * /* v */ );
Var_T * f_fsave(     Var_T * /* v */ );
Var_T * f_ffsave(    Var_T * /* v */ );
Var_T * f_save_p(    Var_T * /* v */ );
Var_T * f_save_o(    Var_T * /* v */ );
Var_T * f_save_c(    Var_T * /* v */ );
Var_T * f_is_file(   Var_T * /* v */ );
Var_T * f_file_name( Var_T * /* v */ );
Var_T * f_path_name( Var_T * /* v */ );
Var_T * f_delf(      Var_T * /* v */ );


#endif  /* ! FUNC_SAVE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
