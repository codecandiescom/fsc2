/*
  $Id$
*/


#include "fsc2.h"


void show_cut( int dir, int pos )
{
	printf( "Show %c cut at %d\n", dir == X ? 'x' : 'y', pos );
}
