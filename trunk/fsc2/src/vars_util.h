/*
  $Id$
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


#endif  /* ! VARS_UTIL_HEADER */
