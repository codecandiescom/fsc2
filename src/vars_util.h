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


#pragma once
#if ! defined VARS_UTIL_HEADER
#define VARS_UTIL_HEADER


#include "fsc2.h"


Var_T *vars_negate( Var_T * /* v */ );

Var_T * vars_comp( int     /* comp_type */,
                   Var_T * /* v1        */,
                   Var_T * /* v2        */  );

Var_T * vars_lnegate( Var_T * /* v */ );

void vars_arith_len_check( Var_T *      /* v1 */,
                           Var_T *      /* v2 */,
                           const char * /* op */  );


#endif  /* ! VARS_UTIL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
