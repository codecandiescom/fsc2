#define FSC2_MAIN

#include "fsc2.h"




int main( int argc, char *argv[ ] )
{
	if ( argc < 2 )
	{
		printf( "Missing input file.\n" );
		return( EXIT_FAILURE );
	}

	return split( argv[ 1 ] );
}
