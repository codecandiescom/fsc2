/*
  $Id$
*/

#define FSC2_MAIN

#include "fsc2.h"

#include <mcheck.h>


int main( int argc, char *argv[ ] )
{

#if defined DEBUG
	if ( mcheck( NULL ) != 0 )
		printf( "--> mcheck did not start ! <--\n" );
#endif


	if ( argc < 2 )
	{
		printf( "Missing input file.\n" );
		return EXIT_FAILURE;
	}

	clear_up( );

	while ( 1 )
	{
		split( argv[ 1 ] );
		clear_up( );
	}

	return EXIT_SUCCESS;
}


void clear_up( void )
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

	/* delete variables and get rid of variable stack */

	vars_clean_up( );

	/* delete all the pulses */

	delete_pulses( );

	/* delete stored program */

	forget_prg( );
}
