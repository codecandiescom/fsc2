/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#if ! defined MODULE_UTIL_HEADER
#define MODULE_UTIL_HEADER


int get_mode( void );
int get_check_state( void );
void stop_on_user_request( void );
void too_many_arguments( Var *v );
void no_query_possible( void );
long get_long( Var *v, const char *snippet );
double get_double( Var *v, const char *snippet );
long get_strict_long( Var *v, const char *snippet );
bool get_boolean( Var *v );
double is_mult_ns( double val, const char * text );
char *translate_escape_sequences( char *str );
double experiment_time( void );

#define MODULE_CALL_ESTIMATE   0.02   /* 20 ms per module function call -
										 estimate for calculation of time in
										 test run via experiment_time() */


#endif  /* ! MODULE_UTIL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
