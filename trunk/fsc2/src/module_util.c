/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


/*----------------------------------------------------------------------*/
/* This function is called by modules to determine the current state of */
/* the global variable FSC2_MODE without them biing able to change it.  */
/*----------------------------------------------------------------------*/

inline int get_mode( void )
{
	return FSC2_MODE;
}


/*--------------------------------------------------------------*/
/* This function might be called to check if there are any more */
/* variables on the variable stack, representing superfluous    */
/* arguments to a function.                                     */
/*--------------------------------------------------------------*/

inline void too_many_arguments( Var *v, const char *device_name )
{
	if ( v == NULL || ( v = vars_pop( v ) ) == NULL )
		return;

	eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
			"%s().\n", device_name, v->next != NULL ? "s" : "", Cur_Func );
	while ( ( v = vars_pop( v ) ) != NULL )
		;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

inline void no_query_possible( const char device_name )
{
	eprint( FATAL, SET, "%s: Function %s() with no argument can only be used "
			"in the EXPERIMENT section.\n", device_name, Cur_Func );
	THROW( EXCEPTION )
}
