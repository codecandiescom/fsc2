/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


#if ! defined VARS_UTIL_HEADER
#define VARS_UTIL_HEADER


#include "fsc2.h"


Var *vars_add_to_int_var( Var *v1, Var *v2 );
Var *vars_add_to_float_var( Var *v1, Var *v2 );
Var *vars_add_to_int_arr( Var *v1, Var *v2 );
Var *vars_add_to_float_arr( Var *v1, Var *v2 );
Var *vars_sub_from_int_var( Var *v1, Var *v2 );
Var *vars_sub_from_float_var( Var *v1, Var *v2 );
Var *vars_sub_from_int_arr( Var *v1, Var *v2 );
Var *vars_sub_from_float_arr( Var *v1, Var *v2 );
Var *vars_mult_by_int_var( Var *v1, Var *v2 );
Var *vars_mult_by_float_var( Var *v1, Var *v2 );
Var *vars_mult_by_int_arr( Var *v1, Var *v2 );
Var *vars_mult_by_float_arr( Var *v1, Var *v2 );
Var *vars_div_of_int_var( Var *v1, Var *v2 );
Var *vars_div_of_float_var( Var *v1, Var *v2 );
Var *vars_div_of_int_arr( Var *v1, Var *v2 );
Var *vars_div_of_float_arr( Var *v1, Var *v2 );
Var *vars_mod_of_int_var( Var *v1, Var *v2 );
Var *vars_mod_of_float_var( Var *v1, Var *v2 );
Var *vars_mod_of_int_arr( Var *v1, Var *v2 );
Var *vars_mod_of_float_arr( Var *v1, Var *v2 );
Var *vars_pow_of_int_var( Var *v1, Var *v2 );
Var *vars_pow_of_float_var( Var *v1, Var *v2 );
Var *vars_pow_of_int_arr( Var *v1, Var *v2 );
Var *vars_pow_of_float_arr( Var *v1, Var *v2 );
Var *vars_array_check( Var *v1, Var *v2 );


#endif  /* ! VARS_UTIL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
