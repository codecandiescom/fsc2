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

#if ! defined UTIL_HEADER
#define UTIL_HEADER


char *get_string( const char *fmt, ... );
char *string_to_lower( char *str );
void *get_memcpy( const void *array, size_t size );
char *correct_line_breaks( char *str );
const char *strip_path( const char *path );
const char *slash( const char *path );
long get_file_length( FILE *fp, int *len );
void eprint( int severity, bool print_fl, const char *fmt, ... );
void print( int severity, const char *fmt, ... );
void raise_permissions( void );
void lower_permissions( void );
FILE *filter_edl( const char *name, FILE *fp );
bool fsc2_locking( void );
int fsc2_usleep( unsigned long us_dur, bool quit_on_signal );
int is_in( const char *supplied_in, const char **alternatives, int max );
void i2rgb( double h, int *rgb );
void create_colors( void );
Var *get_digitizer_channel_number( const char *channel_name );


#endif  /* ! UTIL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
