/*
  $Id$
*/

#include "fsc2.h"

/* Enter the declarations of all exported functions below this line! */

static Var *incr( Var *v );
static Var *square( Var *v );

static Function_List FL[ ] =
{
	{ "incr", incr },
	{ "square", square },
	{ NULL,   NULL }
};


void load_user_functions( Func *fncts, int num_def_func, int num_func )
{
	Function_List *cfl = FL;
	int num;

	while ( cfl->name != NULL )
	{
		for ( num = num_def_func; num < num_func; num++ )
			if ( ! strcmp( fncts[ num ].name, cfl->name ) )
			{
				 fncts[ num ].fnct = cfl->fnct;
				 break;
			}

		cfl++;
	}
}


/****************************************************************/
/* Enter the definition of all nedded functions below this line */
/****************************************************************/

				 
Var *incr( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval + 1 );
	else
		return vars_push( FLOAT_VAR, v->val.dval + 1.0 );
}


Var *square( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval * v->val.lval );
	else
		return vars_push( FLOAT_VAR, v->val.dval * v->val.dval );
}
