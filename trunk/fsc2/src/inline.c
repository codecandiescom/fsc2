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


#include "fsc2.h"


/********************************************************************/
/* The following functions are that short that inlining them seemed */
/* to be a good idea...                                             */
/********************************************************************/


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

inline short d2shrt( double a )
{
	if ( a > SHRT_MAX_HALF )
		return SHRT_MAX_HALF;
	if ( a < SHRT_MIN_HALF )
		return SHRT_MIN_HALF;

	return ( short ) ( a < 0.0 ? ceil( a - 0.5 ) : floor( a + 0.5 ) );
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

inline short i2shrt( int a )
{
	if ( a > SHRT_MAX_HALF )
		return SHRT_MAX_HALF;
	if ( a < SHRT_MIN_HALF )
		return SHRT_MIN_HALF;

	return ( short ) a;
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

inline unsigned short d2ushrt( double a )
{
	if ( a > USHRT_MAX )
		return USHRT_MAX;
	if ( a < 0 )
		return 0;

	return ( unsigned short ) floor( a + 0.5 );
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

inline unsigned short i2ushrt( int a )
{
	if ( a > USHRT_MAX )
		return USHRT_MAX;
	if ( a < 0 )
		return 0;

	return ( unsigned short ) a;
}

/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

inline int    i_max( int    a, int    b ) { return a > b ? a : b; }
inline int    i_min( int    a, int    b ) { return a < b ? a : b; }
inline long   l_max( long   a, long   b ) { return a > b ? a : b; }
inline long   l_min( long   a, long   b ) { return a < b ? a : b; }
inline float  f_max( float  a, float  b ) { return a > b ? a : b; }
inline float  f_min( float  a, float  b ) { return a < b ? a : b; }
inline double d_max( double a, double b ) { return a > b ? a : b; }
inline double d_min( double a, double b ) { return a < b ? a : b; }
inline size_t s_min( size_t a, size_t b ) { return a < b ? a : b; }


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

inline long lrnd( double x )
{
	if ( x > LONG_MAX )
		return LONG_MAX;
	if ( x < LONG_MIN )
		return LONG_MIN;

	return ( long ) ( x < 0.0 ? ceil( x - 0.5 ) : floor( x + 0.5 ) );
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

inline int irnd( double x )
{
	if ( x > INT_MAX )
		return INT_MAX;
	if ( x < INT_MIN )
		return INT_MIN;

	return ( int ) ( x < 0.0 ? ceil( x - 0.5 ) : floor( x + 0.5 ) );
}
