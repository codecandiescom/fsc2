/*
  $Id$
*/


#include "fsc2.h"



void show_message( const char *str )
{
	writer( C_SHOW_MESSAGE, str );
}

void hide_message( void )
{
	writer( C_HIDE_MESSAGE );
}
