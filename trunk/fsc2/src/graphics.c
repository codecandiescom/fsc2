/*
  $Id$
*/


#include "fsc2.h"


static GRAPHICS graph = { UNSET, 0, NULL, NULL };



void graphics_init( long dim, char *x_label, char *y_label )
{
	/* The parent process does all the graphics stuff... */

	if ( I_am == CHILD )
	{
		writer( C_INIT_GRAPHICS, dim, x_label, y_label );
		return;
	}

	if ( dim < 2 || dim > 3 )
	{
		eprint( FATAL, "%s:%ld: Invalid display dimension (%ld), valid "
				"values are 2 or 3.\n", Fname, Lc, dim );
		THROW( EXCEPTION );
	}

	graph.dim = dim;

	if ( x_label != NULL )
	{
		if ( graph.x_label != NULL )
			T_free( graph.x_label );
		graph.x_label = get_string_copy( x_label );
	}

	if ( dim == 3 && y_label != NULL )
	{
		if ( graph.y_label != NULL )
			T_free( graph.y_label );
		graph.y_label = get_string_copy( y_label );
	}

	graph.is_init = SET;
}


