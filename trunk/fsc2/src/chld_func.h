/*
  $Id$
*/

#if ! defined CHLD_FUNC_HEADER
#define CHLD_FUNC_HEADER



#include "fsc2.h"



void show_message( const char *str );
void show_alert( const char *str );
int show_choices( const char *text, int numb, const char *b1, const char *b2,
				  const char *b3, int def );
const char *show_fselector( const char *message, const char *directory,
							const char *pattern, const char *def );
const char *show_input( const char *content, const char *label );


#endif  /* ! CHLD_FUNC_HEADER */
