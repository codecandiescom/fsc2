/*
  $Id$
*/


#include "fsc2.h"

int User_Functions_init_hook( void );
void User_Functions_exit_hook( void );


Var *square( Var *var );
Var *int_slice( Var *var );
Var *float_slice( Var *var );



/* Here examples for init and exit hok functions - the init hook function
   will be called directly after all libraries are loaded while the exit
   hook function is called immediately before the library is unloaded.
*/

int User_Functions_init_hook( void )
{
	printf( "This is User_Functions_init_hook()\n" );
	return( 1 );
}
void User_Functions_exit_hook( void )
{
	printf( "This is User_Functions_exit_hook()\n" );
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
	free( array );

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
	free( array );

	return ret;
}
