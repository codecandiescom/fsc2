#if ! defined VARIABLES_HEADER
#define VARIABLES_HEADER


#include "fsc2.h"

/* The following defines the function that decides if a variable is an integer
   or a float variable. The function gets passed the first character of the
   variable name. If it returns TRUE, the variable will be an integer variable
   (or array), otherwise a float variable or array. */

/* Variables with a name starting with a capital letter will be integers, all
   others float variables */

#define IF_FUNC( a )  ( isupper( a ) )


/* some people prefer C-style arrays, some FORTRAN- or MATLAB-style arrays -
   by setting the following constant the program can be easily adapted to
   whatever style the users like (even something rather stupid...) */

#define ARRAY_OFFSET 1      /* 0: C-like array indexing */
                            /* 1: FORTRAN-like array indexing */
                            /* or something else for really weird results :) */


/* dynamically sized arrays have (as long as their dimension is still
   undetermined) a size of 0 for the very last dimension */

#define is_variable_array( a )  ( ( a )->sizes[ ( a )->dim - 1 ] <= 0 )


typedef struct Var_
{
	char *name;                         /* name of the variable or array */
	bool new_flag;                      /* set while no data are assigned */
	int type;                           /* type of the variable - see below */
	union
	{
		long lval;                          /* for integer values */
		double dval;                        /* for float values */
		long *lpnt;                         /* for integer arrays */
		double *dpnt;                       /* for double arrays */
		struct Var_ * ( * fnct )( long, double * ); /* for functions */
	} val;
	long dim;             /* dimension of array / number of args of function */
	long *sizes;
	long len;             /* total len of array / position in function list */
	struct Var_ *next;
} Var;

typedef struct AStack_
{
	Var *var;
	long act_entry;
	long *entries;
	struct AStack_ *next;
} AStack;


enum {
	COMP_EQUAL,
	COMP_LESS,
	COMP_LESS_EQUAL
};


Var *vars_get( char *name );
Var *vars_new( char *name );
Var *vars_new_assign( Var *src, Var *dest );
void vars_default( Var *v );
Var *vars_add( Var *v1, Var *v2 );
Var *vars_sub( Var *v1, Var *v2 );
Var *vars_mult( Var *v1, Var *v2 );
Var *vars_div( Var *v1, Var *v2 );
Var *vars_mod( Var *v1, Var *v2 );
Var *vars_pow( Var *v1, Var *v2 );
Var *vars_negate( Var *v );
Var *vars_comp( int comp_type, Var *v1, Var *v2 );
Var *vars_push_simple( Var *v );
Var *vars_push( int type, void *data );
void vars_pop( Var *v );
void vars_del_stack( void );
void vars_arr_start( Var *a );
void vars_arr_extend( Var *a, Var *s );
void vars_arr_init( Var *a, Var *d );
void vars_push_astack( Var *v );
Var *vars_pop_astack( void );
void vars_arr_assign( Var *a, Var *v );
void vars_del_top_astack( void );
void vars_del_astack( void );
void vars_update_astack( Var *v );
void vars_clean_up( void );
void free_vars( void );
void vars_check( Var *v );
void vars_warn_new( Var *v );
Var *vars_assign( Var *src, Var *dest );


#endif  /* ! VARIABLES_HEADER */
