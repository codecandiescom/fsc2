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


#if ! defined FUNC_BASIC_HEADER
#define FUNC_BASIC_HEADER


#include "fsc2.h"


Var *f_abort(   Var *v );
Var *f_stopsim( Var *v );
Var *f_int(     Var *v );
Var *f_float(   Var *v );
Var *f_round(   Var *v );
Var *f_floor(   Var *v );
Var *f_ceil(    Var *v );
Var *f_abs(     Var *v );
Var *f_sin(     Var *v );
Var *f_cos(     Var *v );
Var *f_tan(     Var *v );
Var *f_asin(    Var *v );
Var *f_acos(    Var *v );
Var *f_atan(    Var *v );
Var *f_sinh(    Var *v );
Var *f_cosh(    Var *v );
Var *f_tanh(    Var *v );
Var *f_exp(     Var *v );
Var *f_ln(      Var *v );
Var *f_log(     Var *v );
Var *f_sqrt(    Var *v );
Var *f_random(  Var *v );
Var *f_grand(   Var *v );
Var *f_setseed( Var *v );
Var *f_time(    Var *v );
Var *f_dtime(   Var *v );
Var *f_date(    Var *v );
Var *f_dim(     Var *v );
Var *f_size(    Var *v );
Var *f_sizes(   Var *v );
Var *f_mean(    Var *v );
Var *f_rms(     Var *v );
Var *f_slice(   Var *v );
Var *f_square(  Var *v );
Var *f_islice(  Var *v );
Var *f_fslice(  Var *v );


#endif  /* ! FUNC_BASIC_HEADER */
