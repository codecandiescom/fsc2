/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#include "rs_spec10.h"


#define C2K_OFFSET   273.16         /* offset between Celsius and Kelvin */


/*--------------------------------------------------*/
/*--------------------------------------------------*/

double rs_spec10_k2c( double tk )
{
	return tk - C2K_OFFSET;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

double rs_spec10_c2k( double tc )
{
	return tc + C2K_OFFSET;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

int16 rs_spec10_k2ic( double tk )
{
	return ( int16 ) ( 100.0 * ( tk - C2K_OFFSET ) );
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

double rs_spec10_ic2k( int16 tci )
{
	return 0.01 * tci + C2K_OFFSET;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

bool rs_spec10_param_access( uns32 param, uns16 *access )
{
	boolean avail;

	if ( ! pl_get_param( rs_spec10->handle, param, ATTR_AVAIL,
						 ( void_ptr ) &avail ) )
		rs_spec10_error_handling( );

	if ( ! avail )
		return FAIL;

	if ( ! pl_get_param( rs_spec10->handle, param, ATTR_ACCESS,
						 ( void_ptr ) access ) )
		rs_spec10_error_handling( );

	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
