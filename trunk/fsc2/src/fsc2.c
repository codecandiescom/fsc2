/*
  $Id$
*/

#define FSC2_MAIN

#include "fsc2.h"

#if defined MDEBUG
#include <mcheck.h>
#endif


int main( int argc, char *argv[ ] )
{
#if defined MDEBUG
	if ( mcheck( NULL ) != 0 )
	{
		printf( "Can't start mcheck() !\n" );
		return EXIT_FAILURE;
	}
#endif

	if ( argc < 2 )
	{
		printf( "Missing input file.\n" );
		return EXIT_FAILURE;
	}

	clean_up( );

	split( argv[ 1 ] );
	clean_up( );

	/* Make sure the TRY/CATCH stuff worked out right */

	assert( exception_env_stack_pos == 0 );

	return EXIT_SUCCESS;
}


void clean_up( void )
{
	int i;


	/* clear up the compilation structure */

	for ( i = 0; i < 3; ++i )
		compilation.error[ 3 ] = UNSET;
	for ( i = DEVICES_SECTION; i <= EXPERIMENT_SECTION; ++i )
		compilation.sections[ i ] = UNSET;

	/* Deallocate memory used for file names */

	if ( Fname != NULL )
		T_free( Fname );
	Fname = NULL;

	/* run exit hook functiosn and unlink modules */

	delete_devices( );

	/* delete function list */

	functions_exit( );

	/* delete device list */

	delete_device_name_list( );

	/* clear up assignments structures etc. */

	assign_clear( );

	/* clear up structures for phases */

	phases_clear( );

	/* clear up copy of variables from test run */

	delete_var_list_copy( );

	/* delete variables and get rid of variable stack */

	vars_clean_up( );

	/* delete all the pulses */

	delete_pulses( );

	/* delete stored program */

	forget_prg( );
}
