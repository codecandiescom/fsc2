/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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


#if ! defined INTERACTIVE_OBJECTS_HEADER
#define INTERACTIVE_OBJECTS_HEADER

#include "fsc2.h"


/* exported functions */

Var_T *f_ocreate(  Var_T * /* v */ );
Var_T *f_odelete(  Var_T * /* v */ );
Var_T *f_ovalue(   Var_T * /* v */ );
Var_T *f_ochanged( Var_T * /* v */ );


#endif   /* ! INTERACTIVE_OBJECTS_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
