/*
  $Id$

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

#if ! defined VARIABLES_HEADER
#define VARIABLES_HEADER


#include "fsc2.h"

/* The following defines the function that decides if a variable is an integer
   or a float variable. The function gets passed the first character of the
   variable name. If it returns TRUE, the variable will be an integer variable
   (or array), otherwise a float variable or array. */

/* Variables with a name starting with a capital letter will be integers, all
   others float variables */

#define IF_FUNC( a )  ( isupper( a[ 0 ] ) )


/* some people prefer C-style arrays, some FORTRAN- or MATLAB-style arrays -
   by setting the following constant the program can be easily adapted to
   whatever style the users like (even something rather stupid...) */

#define ARRAY_OFFSET 1      /* 0: C-like array indexing */
                            /* 1: FORTRAN-like array indexing */
                            /* or something else for really weird results :) */


/* dynamically sized arrays have (as long as their dimension is still
   undetermined) a size of 0 for the very last dimension */


#define need_alloc( a )  ( ( a )->sizes[ ( a )->dim - 1 ] == 0 )


typedef struct Var_
{
	char *name;                         /* name of the variable */
	int  type;                          /* type of the variable */
	union
	{
		long   lval;                                /* for integer values */
		double dval;                                /* for float values */
		long   *lpnt;                               /* for integer arrays */
		double *dpnt;                               /* for double arrays */
		char   *sptr;                               /* for strings */
		struct Var_ * ( * fnct )( struct Var_ * );  /* for functions */
		struct Var_ *vptr;                          /* for array references */
		void   *gptr;                               /* generic pointer */
	} val;
	int    dim;              /* dimension of array */
	int    *sizes;           /* array of sizes of dimensions */
	long   len;              /* total len of array */
	long   flags;
	struct Var_ *from;
	struct Var_ *next;       /* next variable in list or stack */
	struct Var_ *prev;       /* previous variable in list or stack */
} Var;


/* Define the different types of variables */

enum {
	UNDEF_VAR       = 0,               /*    0 */
	INT_VAR         = ( 1 << 0 ),      /*    1 */
	FLOAT_VAR       = ( 1 << 1 ),      /*    2 */
	STR_VAR         = ( 1 << 2 ),      /*    4 */
	INT_ARR         = ( 1 << 3 ),      /*    8 */
	FLOAT_ARR       = ( 1 << 4 ),      /*   16 */
	FUNC            = ( 1 << 5 ),      /*   32 */
	ARR_PTR         = ( 1 << 6 ),      /*   64 */
	INT_TRANS_ARR   = ( 1 << 7 ),      /*  128 */
	FLOAT_TRANS_ARR = ( 1 << 8 ),      /*  256 */
	ARR_REF         = ( 1 << 9 ),      /*  512 */
};


enum {
	NEW_VARIABLE       = ( 1 << 0 ),   /*    1 */
	VARIABLE_SIZED     = ( 1 << 1 ),   /*    2 */
	NEED_SLICE         = ( 1 << 2 ),   /*    4 */
	NEED_INIT          = ( 1 << 3 ),   /*    8 */
	NEED_ALLOC         = ( 1 << 4 ),   /*   16 */
	IS_DYNAMIC         = ( 1 << 5 ),   /*   32 */
};


enum {
	COMP_EQUAL,
	COMP_UNEQUAL,
	COMP_LESS,
	COMP_LESS_EQUAL,
	COMP_AND,
	COMP_OR,
	COMP_XOR
};


Var *vars_get( char *name );
Var *vars_new( char *name );
Var *vars_add( Var *v1, Var *v2 );
Var *vars_sub( Var *v1, Var *v2 );
Var *vars_mult( Var *v1, Var *v2 );
Var *vars_div( Var *v1, Var *v2 );
Var *vars_mod( Var *v1, Var *v2 );
Var *vars_pow( Var *v1, Var *v2 );
Var *vars_negate( Var *v );
Var *vars_comp( int comp_type, Var *v1, Var *v2 );
Var *vars_lnegate( Var *v );
Var *vars_push( int type, ... );
Var *vars_pop( Var *v );
void vars_del_stack( void );
void vars_clean_up( void );
void free_vars( void );
void vars_check( Var *v, int type );
void vars_warn_new( Var *v );
bool vars_exist( Var *v );
Var *vars_arr_start( Var *v );
Var *vars_arr_lhs( Var *v );
Var *vars_get_lhs_pointer( Var *v, int dim );
long vars_calc_index( Var *a, Var *v );
Var *vars_setup_new_array( Var *v, int dim );
Var *vars_arr_rhs( Var *v );
void vars_assign( Var *src, Var *dest );
void vars_ass_from_var( Var *src, Var *dest );
void vars_ass_from_ptr( Var *src, Var *dest );
void vars_ass_from_trans_ptr( Var *src, Var *dest );
void vars_arr_init( Var *dest );
Var * apply_unit( Var *var, Var *unit );
Var *vars_val( Var *v );


#endif  /* ! VARIABLES_HEADER */
