/*
  $Id$
*/


#include "fsc2.h"


int dg2020_init_hook( void );


int dg2020_init_hook( void )
{
	void *x;
	int ret;

	printf( "This is dg2020_init_hook()\n" );

	ret = get_lib_symbol( "User_Functions", "square", &x );
	if ( ret != LIB_NO_ERR )
		printf( "dg2020_init_hook: Error while loading symbol: %d\n", ret );
	else
		printf( "dg2020_init_hook: Address of symbol: %p\n", x );
	return( 1 );
}
