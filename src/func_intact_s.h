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
#if ! defined INTERACTIVE_SLIDERS_HEADER
#define INTERACTIVE_SLIDERS_HEADER

#include "fsc2.h"


/* exported functions */

Var_T *f_screate(  Var_T * /* v */ );
Var_T *f_sdelete(  Var_T * /* v */ );
Var_T *f_svalue(   Var_T * /* v */ );
Var_T *f_schanged( Var_T * /* v */ );


#endif   /* ! INTERACTIVE_SLIDERS_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
