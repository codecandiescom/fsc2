/*
  $Id$
*/


#include "fsc2.h"


Var *square( Var *v );
Var *int_slice( Var *v );
Var *float_slice( Var *v );




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


Var *int_slice( Var *v )
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


Var *float_slice( Var *v )
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
