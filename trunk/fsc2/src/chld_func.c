/*
  $Id$
*/


#include "fsc2.h"



void show_message( const char *str )
{
	if ( I_am == PARENT )
		fl_show_messages( str );
	else
		writer( C_SHOW_MESSAGE, str );
}


void show_alert( const char *str )
{
	char *strc, *strs[ 3 ];
	

	if ( I_am == PARENT )
	{
		strc = get_string_copy( str );
		strs[ 0 ] = strc;
		if ( ( strs[ 1 ] = strchr( strs[ 0 ], '\n' ) ) != NULL )
		{
			*strs[ 1 ]++ = '\0';
			if ( ( strs[ 2 ] = strchr( strs[ 1 ], '\n' ) ) != NULL )
				*strs[ 2 ]++ = '\0';
			else
				strs[ 2 ] = NULL;
		}
		else
			strs[ 1 ] = strs[ 2 ] = NULL;

		fl_show_alert( strs[ 0 ], strs[ 1 ], strs[ 2 ], 1 );
		T_free( strc );
	}
	else
		writer( C_SHOW_ALERT, str );
}
