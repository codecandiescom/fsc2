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


#if ! defined INLINE_HEADER
#define INLINE_HEADER

#include "fsc2.h"

#if defined __STRICT_ANSI__
#define inline
#endif


inline short d2shrt( double a );
inline short i2shrt( int a );
inline unsigned short d2ushrt( double a );
inline unsigned short i2ushrt( int a );
inline int i_max( int a, int b );
inline int i_min( int a, int b );
inline long l_max( long a, long b );
inline long l_min( long a, long b );
inline float f_max( float a, float b );
inline float f_min( float  a, float  b );
inline double d_max( double a, double b );
inline double d_min( double a, double b );
inline size_t s_min( size_t a, size_t b );
inline long lrnd( double x );
inline int irnd( double x );


#endif  /* ! INLINE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
