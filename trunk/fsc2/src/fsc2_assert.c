/*
  $Id$
*/


#include "fsc2.h"


void fsc2_assert_print( const char *expression, const char *filename,
						unsigned int line )
{
	fprintf( stderr, "%s:%u: failed assertion: %s\n", filename, line,
			 expression );
	fflush( stderr );
	Assert_struct.expression = expression;
	Assert_struct.line = line;
	Assert_struct.filename = filename;
	abort( );
}
