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

#if ! defined UTIL_HEADER
#define UTIL_HEADER


char *get_string( const char *fmt, ... );
char *string_to_lower( char *str );
void *get_memcpy( const void *array, size_t size );
char *correct_line_breaks( char *str );
const char *strip_path( const char *path );
const char *slash( const char *path );
long get_file_length( const char *name, int *len );
void eprint( int severity, bool print_fl, const char *fmt, ... );
bool fsc2_locking( void );
int is_in( const char *supplied_in, const char **altern, int max );
void i2rgb( double h, int *rgb );
void create_colors( void );

inline void raise_permissions( void );
inline void lower_permissions( void );

inline unsigned long d2color( double a );

inline short  d2shrt( double a );
inline unsigned short d2ushrt( double a );
inline short  i2shrt( int a );

inline int    i_max( int    a, int    b );
inline int    i_min( int    a, int    b );
inline long   l_max( long   a, long   b );
inline long   l_min( long   a, long   b );
inline float  f_max( float  a, float  b );
inline float  f_min( float  a, float  b );
inline double d_max( double a, double b );
inline double d_min( double a, double b );

inline long lround( double x );


#endif  /* ! UTIL_HEADER */
