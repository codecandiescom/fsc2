/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/

#if ! defined FUNC_UTIL_HEADER
#define FUNC_UTIL_HEADER


#include "fsc2.h"


Var *f_print( Var *v );
Var *f_showm( Var *v );
Var *f_wait( Var *v );
Var *f_init_1d( Var *v );
Var *f_init_2d( Var *v );
Var *f_dmode( Var *v );
Var *f_cscale( Var *v );
Var *f_cscale_1d( Var *v );
Var *f_cscale_2d( Var *v );
Var *f_clabel( Var *v );
Var *f_clabel_1d( Var *v );
Var *f_clabel_2d( Var *v );
Var *f_rescale( Var *v );
Var *f_rescale_1d( Var *v );
Var *f_rescale_2d( Var *v );
Var *f_display( Var *v );
Var *f_display_1d( Var *v );
Var *f_display_2d( Var *v );
Var *f_clearcv( Var *v );
Var *f_clearcv_1d( Var *v );
Var *f_clearcv_2d( Var *v );
Var *f_setmark( Var *v );
Var *f_setmark_1d( Var *v );
Var *f_setmark_2d( Var *v );
Var *f_clearmark( Var *v );
Var *f_clearmark_1d( Var *v );
Var *f_clearmark_2d( Var *v );


#endif  /* ! FUNC_UTIL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
