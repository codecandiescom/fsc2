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


#if ! defined MODULE_UTIL_HEADER
#define MODULE_UTIL_HEADER


inline int get_mode( void );
inline void too_many_arguments( Var *v, const char *device_name );
inline void no_query_possible( const char device_name );


#endif  /* ! MODULE_UTIL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
