/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2.h"


Fsc2_Assert Assert_struct;


int fsc2_assert_print( const char *expression, const char *filename,
					   unsigned int line )
{
	fprintf( stderr, "%s:%u: failed assertion: %s\n", filename, line,
			 expression );
	fflush( stderr );
	Assert_struct.expression = expression;
	Assert_struct.line = line;
	Assert_struct.filename = filename;
	abort( );

	return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
