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


/********************************************************************/
/* The following functions are that short that inlining them seemed */
/* to be a good idea...                                             */
/********************************************************************/

static inline short d2shrt( double a );
static inline short i2shrt( int a );
static inline unsigned short d2ushrt( double a );
static inline unsigned short i2ushrt( int a );
static inline int i_max( int a, int b );
static inline int i_min( int a, int b );
static inline long l_max( long a, long b );
static inline long l_min( long a, long b );
static inline float f_max( float a, float b );
static inline float f_min( float  a, float  b );
static inline double d_max( double a, double b );
static inline double d_min( double a, double b );
static inline size_t s_min( size_t a, size_t b );
static inline long lrnd( double x );
static inline int irnd( double x );


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static inline short d2shrt( double a )
{
	if ( a > SHRT_MAX_HALF )
		return SHRT_MAX_HALF;
	if ( a < SHRT_MIN_HALF )
		return SHRT_MIN_HALF;
	return ( short ) floor( a + 0.5 );
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static inline short i2shrt( int a )
{
	if ( a > SHRT_MAX_HALF )
		return SHRT_MAX_HALF;
	if ( a < SHRT_MIN_HALF )
		return SHRT_MIN_HALF;
	return ( short ) a;
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static inline unsigned short d2ushrt( double a )
{
	if ( a > USHRT_MAX )
		return USHRT_MAX;
	if ( a < 0 )
		return 0;
	return ( unsigned short ) floor( a + 0.5 );
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static inline unsigned short i2ushrt( int a )
{
	if ( a > USHRT_MAX )
		return USHRT_MAX;
	if ( a < 0 )
		return 0;
	return ( unsigned short ) a;
}

/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static inline int    i_max( int    a, int    b ) { return a > b ? a : b; }
static inline int    i_min( int    a, int    b ) { return a < b ? a : b; }
static inline long   l_max( long   a, long   b ) { return a > b ? a : b; }
static inline long   l_min( long   a, long   b ) { return a < b ? a : b; }
static inline float  f_max( float  a, float  b ) { return a > b ? a : b; }
static inline float  f_min( float  a, float  b ) { return a < b ? a : b; }
static inline double d_max( double a, double b ) { return a > b ? a : b; }
static inline double d_min( double a, double b ) { return a < b ? a : b; }
static inline size_t s_min( size_t a, size_t b ) { return a < b ? a : b; }


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static inline long lrnd( double x )
{
	if ( x > LONG_MAX )
		return LONG_MAX;
	if ( x < LONG_MIN )
		return LONG_MIN;
	return ( long ) floor( x + 0.5 );
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static inline int irnd( double x )
{
	if ( x > INT_MAX )
		return INT_MAX;
	if ( x < INT_MIN )
		return INT_MIN;
	return ( int ) floor( x + 0.5 );
}
