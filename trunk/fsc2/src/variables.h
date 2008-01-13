/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


#if ! defined VARIABLES_HEADER
#define VARIABLES_HEADER


#include "fsc2.h"


/* The following macro defines how it is decided if a variable is an integer
   or a float variable - variables with a name starting with a capital letter
   will be integers (or arrays of integers etc.), all others floats. */

#define VAR_TYPE( a )  ( isupper( ( a )->name[ 0 ] ) ? INT_VAR : FLOAT_VAR )


/* Some people prefer C-style arrays, some FORTRAN- or MATLAB-style arrays -
   by setting the following constant the program can be easily adapted to
   whatever style the users like (even something rather weird...) */

#define ARRAY_OFFSET 1      /* 0: C-like array indexing */
                            /* 1: FORTRAN-like array indexing */
                            /* or something else for really weird results :) */


/* Different types of variables */

typedef enum Var_Type Var_Type_T;

enum Var_Type {
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
    SUB_REF_PTR     = ( 1 <<  9 ),      /*   512 */
    REF_PTR         = ( 1 << 10 ),      /*  1024 */
    FUNC            = ( 1 << 11 )       /*  2048 */
};


typedef struct Var Var_T;

struct Var {
    char       * name;             /* name of the variable */
    Var_Type_T   type;             /* type of the variable */

    union
    {
        long           lval;       /* for integer values */
        double         dval;       /* for floating point values */
        long        *  lpnt;       /* for integer arrays */
        double      *  dpnt;       /* for floating point arrays */
        char        *  sptr;       /* for strings */
        Var_T       ** vptr;       /* for array references */
        struct Func *  fnct;       /* for functions */
        ssize_t     *  index;      /* for indices of LHS sub-array */
    } val;

    int dim;                       /* dimension of array */
    ssize_t len;                   /* total len of array */
    unsigned long flags;

    Var_T * from;                  /* used in pointer variables */
    Var_T * next;                  /* next variable in list or stack */
    Var_T * prev;                  /* previous variable in list or stack */

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


Var_T * vars_get( const char * /* name */ );

Var_T * vars_new( const char * /* name */ );

void vars_arr_create( Var_T * /* a       */,
                      Var_T * /* v       */,
                      int     /* dim     */,
                      bool    /* is_temp */  );

Var_T * vars_push_copy( Var_T * /* v */ );

Var_T * vars_push_matrix( Var_Type_T /* type */,
                          int        /* dim  */,
                          ...                    );

Var_T * vars_push( Var_Type_T /* type */,
                  ... );

Var_T * vars_pop( Var_T * /* v */ );

Var_T * vars_make( Var_Type_T /* type */,
                   Var_T *    /* src  */  );

void vars_del_stack( void );

void vars_clean_up( void );

void vars_check( Var_T * /* v    */,
                 int     /* type */  );

bool vars_exist( Var_T * /* v */ );

Var_T * vars_arr_start( Var_T * /* v */ );

void * vars_iter( Var_T * /* v */ );

void vars_save_restore( bool /* flag */ );

Var_T * vars_free( Var_T * /* v             */,
                   bool    /* also_nameless */  );


#endif  /* ! VARIABLES_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
