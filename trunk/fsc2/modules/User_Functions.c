/*
  $Id$
*/


#include "fsc2.h"



/* 
   To define a new function do:

   1. Append the name of the function (as will be called from the EDL file!),
      the number of arguments and the accessibility flag to the list in the
	  file `Functions' (also see comment for syntax)
   2. Enter the declaration of the function - all functions have to be of typ

            static Var *function_name( Var *variable_name )

	  i.e. each function must return a variable on the variable stack and each
	  functions has a variable pointer as its argument - even functions that
	  don't need an argument have to be defined this way, but they only will
	  get passed a NULL pointer, so there is nothing to worry about.
   3. Append the name of the function by which it will be called in the EDL
      file as well as the address of the function to the function list
	  `Function_List'.
   4. Apppend the definition of the function to this file.

*/


static Var *square( Var *v );
static Var *islice( Var *v );
static Var *fslice( Var *v );
static Var *f_xxx( Var *v );


static Function_List FL[ ] =
{
	{ "square", square },
	{ "int_slice", islice },
	{ "float_slice", fslice },
	{ "int", f_xxx },
	{ NULL,   NULL }
};



void load_user_functions( Func *fncts, int num_def_func, int num_func )
{
	Function_List *cfl = FL;
	int num;

	eprint( NO_ERROR, "Loading functions from file `%s'.\n", __FILE__ );

	/* Run trough all the functions in the function list and if they need
	   loading (i.e. they are listed in `Functions') store the address of 
	   the function - check that the function has not already been loaded
	   (but overloading built-in functions is acceptable). */

	while ( cfl->name != NULL )
	{
		for ( num = 0; num < num_func; num++ )
			if ( ! strcmp( fncts[ num ].name, cfl->name ) )
			{
				if ( num >= num_def_func && fncts[ num ].fnct != NULL )
				{
					eprint( FATAL, "  Redefinition of function `%s()'.\n",
							fncts[ num ].name );
					THROW( FUNCTION_EXCEPTION );
				}
				else
				{
					eprint( NO_ERROR, "  Loading function `%s()' from file "
							"`%s'.\n", fncts[ num ].name, __FILE__ );
					fncts[ num ].fnct = cfl->fnct;
				}
				break;
			}

		cfl++;
	}
}


/****************************************************************/
/* Enter the definition of all nedded functions below this line */
/****************************************************************/

				 
Var *square( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval * v->val.lval );
	else
		return vars_push( FLOAT_VAR, v->val.dval * v->val.dval );
}


Var *islice( Var *v )
{
	long *x;
	long size;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		size = v->val.lval;
	else
		size = ( long ) v->val.dval;

	x = T_calloc( size, sizeof( long ) );
	return vars_push( INT_TRANS_ARR, x, size );
}


Var *fslice( Var *v )
{
	long *x;
	long size;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		size = v->val.lval;
	else
		size = ( long ) v->val.dval;

	x = T_calloc( size, sizeof( double ) );
	return vars_push( FLOAT_TRANS_ARR, x, size );
}


Var *f_xxx( Var *v )
{
	double arg;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		arg = ( double ) v->val.lval;
	else
		arg = v->val.dval;

	if ( arg < 0.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `sqrt' is negative.\n", 
				Fname, Lc );
		THROW( FUNCTION_EXCEPTION );
	}
	return vars_push( FLOAT_VAR, sqrt( arg ) );
}
