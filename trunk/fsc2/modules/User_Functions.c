/*
  $Id$
*/


#include "fsc2.h"

int User_Functions_init_hook( void );
int User_Functions_test_hook( void );
int User_Functions_exp_hook( void );
void User_Functions_exit_hook( void );


Var *square( Var *var );
Var *int_slice( Var *var );
Var *float_slice( Var *var );
Var *filename( Var *var );



/* Here examples for init and exit hook functions - the init hook function
   will be called directly after all libraries are loaded while the exit
   hook function is called immediately before the library is unloaded.
*/

int User_Functions_init_hook( void )
{
	eprint( NO_ERROR, "This is User_Functions_init_hook()\n" );
	return 1;
}

int User_Functions_test_hook( void )
{
	eprint( NO_ERROR, "This is User_Functions_test_hook()\n" );
	return 1;
}

int User_Functions_exp_hook( void )
{
	eprint( NO_ERROR, "This is User_Functions_exp_hook()\n" );
	return 1;
}

void User_Functions_exit_hook( void )
{
	eprint( NO_ERROR, "This is User_Functions_exit_hook()\n" );
}



/****************************************************************/
/* Enter the definition of all needed functions below this line */
/****************************************************************/


				 
Var *square( Var *var )
{
	vars_check( var, INT_VAR | FLOAT_VAR );

	if ( var->type == INT_VAR )
		return vars_push( INT_VAR, var->INT * var->INT );
	else
		return vars_push( FLOAT_VAR, var->FLOAT * var->FLOAT );
}


Var *int_slice( Var *var )
{
	long *array;
	long size;
	Var *ret;


	vars_check( var, INT_VAR | FLOAT_VAR );

	if ( var->type == INT_VAR )
		size = var->INT;
	else
		size = ( long ) var->FLOAT;

	array = T_calloc( size, sizeof( long ) );
	ret = vars_push( INT_TRANS_ARR, array, size );
	T_free( array );

	return ret;
}


Var *float_slice( Var *var )
{
	long *array;
	long size;
	Var *ret;


	vars_check( var, INT_VAR | FLOAT_VAR );

	if ( var->type == INT_VAR )
		size = var->INT;
	else
		size = ( long ) var->FLOAT;

	array = T_calloc( size, sizeof( double ) );
	ret = vars_push( FLOAT_TRANS_ARR, array, size );
	T_free( array );

	return ret;
}


Var *filename( Var *var )
{
	Var *cur;
	int i, j;
	char *s[ 4 ];
	const char *r;


	/* While test is running just rerturn an empty string */

	if ( TEST_RUN )
		return vars_push( STR_VAR, "" );

	for ( i = 0, cur = var; cur != NULL; i++, cur = cur->next )
	{
		vars_check( cur, STR_VAR );
		s[ i ] = cur->val.sptr;
	}

	for ( j = i; j < 4; j++ )
		s[ j ] = get_string_copy( "" );

	r = show_fselector( s[ 0 ], s[ 1 ], s[ 2 ], s[ 3 ] );
	if ( r != NULL )
		cur = vars_push( STR_VAR, r );
	else
		cur = vars_push( STR_VAR, get_string_copy( "" ) );

	for ( j = i; j < 4; j++ )
		T_free( s[ j ] );

	return cur;
}
