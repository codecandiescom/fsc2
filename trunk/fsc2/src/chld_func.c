/*
  $Id$
*/


#include "fsc2.h"



/*-------------------------------------------------------*/
/* Shows a messge box with an "OK" button - use embedded */
/* newline characters to get multi-line messages.        */
/*-------------------------------------------------------*/

void show_message( const char *str )
{
	if ( I_am == PARENT )
		fl_show_messages( str );
	else
		writer( C_SHOW_MESSAGE, str );
}


/*-------------------------------------------------------*/
/* Shows an alert box with an "OK" button - use embedded */
/* newline characters to get multi-line messages.        */
/*-------------------------------------------------------*/

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


int show_choices( const char *text, int numb, const char *b1, const char *b2,
				  const char *b3, int def )
{
	int ret;

	if ( I_am == PARENT )
		return fl_show_choices( text, numb, b1, b2, b3, def );
	else
	{
		writer( C_SHOW_CHOICES, text, numb, b1, b2, b3, def );
		reader( ( void * ) &ret );
		return ret;
	}
}
