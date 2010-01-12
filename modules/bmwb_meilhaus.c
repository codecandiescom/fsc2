#include "bmwb.h"
#include <medriver/medriver.h>


/*---------------------------------------*
 * Initialized the Meilhaus ME-4670 card
 *---------------------------------------*/

int
meilhaus_init( void )
{
	raise_permissions( );

	if ( meOpen( ME_OPEN_NO_FLAGS ) != ME_ERRNO_SUCCESS )
	{
		char msg[ ME_ERROR_MSG_MAX_COUNT ];

		meErrorGetLastMessage( msg, sizeof msg );
		lower_permissions( );
		fprintf( stderr, "Failed to initialize Meilhaus driver: "
				 "%s\n", msg );
        return 1;
	}
  
	lower_permissions( );

	return 0;
}


/*------------------------------------*
 * Releases the Meilhaus ME-4670 card
 *------------------------------------*/

void
meilhaus_finish( void )
{
	int retval;

	raise_permissions( );

	if ( ( retval = meClose( ME_CLOSE_NO_FLAGS ) ) != ME_ERRNO_SUCCESS )
	{
		char msg[ ME_ERROR_MSG_MAX_COUNT ];

		meErrorGetMessage( retval, msg, sizeof msg );
		fprintf( stderr, "Failed to close Meilhaus driver: %s\n", msg );
	}

	lower_permissions( );
}
