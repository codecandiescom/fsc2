/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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

#define VAR_TYPE( a )  ( isupper( ( a )->name[ 0 ] ) ? INT_VAR : FLOAT_VAR )


/* Some people prefer C-style arrays, some FORTRAN- or MATLAB-style arrays -
   by setting the following constant the program can be easily adapted to
   whatever style the users like (even something rather weird...) */

#define ARRAY_OFFSET 1      /* 0: C-like array indexing */
                            /* 1: FORTRAN-like array indexing */
                            /* or something else for really weird results :) */


/* Different types of variables */

enum {
	UNDEF_VAR       = 0,                /*     0 */
	STR_VAR         = ( 1 <<  0 ),      /*     1 */
	INT_VAR         = ( 1 <<  1 ),      /*     2 */
	FLOAT_VAR       = ( 1 <<  2 ),      /*     4 */
	INT_ARR         = ( 1 <<  3 ),      /*     8 */
	FLOAT_ARR       = ( 1 <<  4 ),      /*    16 */
	INT_REF         = ( 1 <<  5 ),      /*    32 */
    FLOAT_REF       = ( 1 <<  6 ),      /*    64 */
	INT_PTR         = ( 1 <<  7 ),      /*   128 */
	FLOAT_PTR       = ( 1 <<  8 ),      /*   256 */
	REF_PTR         = ( 1 <<  9 ),      /*   512 */
	FUNC            = ( 1 << 10 ),      /*  1024 */
};


typedef struct Var Var;

struct Var {
	char *name;                    /* name of the variable */
	int  type;                     /* type of the variable */

	union
	{
		long        lval;          /* for integer values */
		double      dval;          /* for float values */
		long        *lpnt;         /* for integer arrays */
		double      *dpnt;         /* for double arrays */
		char        *sptr;         /* for strings */
		Var         **vptr;        /* for array references */
		struct Func *fnct;         /* for functions */
	} val;

	int dim;                       /* dimension of array */
	ssize_t len;                   /* total len of array */
	unsigned long flags;

	Var *from;                     /* used in pointer variables */
	Var *next;                     /* next variable in list or stack */
	Var *prev;                     /* previous variable in list or stack */

};



#define INT_TYPE( a ) \
	( ( a )->type & ( INT_VAR | INT_ARR | INT_REF | INT_PTR ) )


#define RHS_TYPES \
	( INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF )


enum {
	NEW_VARIABLE       = ( 1 << 0 ),       /*    1 */
	IS_DYNAMIC         = ( 1 << 1 ),       /*    2 */
	ON_STACK           = ( 1 << 2 ),       /*    4 */
	IS_TEMP            = ( 1 << 3 ),       /*    8 */
	EXISTS_BEFORE_TEST = ( 1 << 4 ),       /*   16 */
	DONT_RECURSE       = ( 1 << 5 ),       /*   32 */
	INIT_ONLY          = ( 1 << 6 )        /*   64 */
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
void vars_arr_create( Var *a, Var *v, int dim, bool is_temp );
Var *vars_push_copy( Var *v );
Var *vars_push_matrix( int type, int dim, ... );
Var *vars_push( int type, ... );
Var *vars_pop( Var *v );
Var *vars_make( int type, Var *src );
void vars_del_stack( void );
void vars_clean_up( void );
void vars_check( Var *v, int type );
bool vars_exist( Var *v );
Var *vars_arr_start( Var *v );
Var *vars_arr_lhs( Var *v );
Var *vars_arr_rhs( Var *v );
void vars_assign( Var *src, Var *dest );
Var *vars_init_list( Var *v, ssize_t level );
void vars_arr_init( Var *dest );
void *vars_iter( Var *v );
void vars_save_restore( bool flag );
Var *vars_free( Var *v, bool also_nameless );


#endif  /* ! VARIABLES_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
