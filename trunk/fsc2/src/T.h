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

#if ! defined T_HEADER
#define T_HEADER

#include "fsc2.h"


void *T_malloc( size_t size );
void *T_calloc( size_t nmemb, size_t size );
void *T_realloc( void *ptr, size_t size );
void *T_free( void *ptr );
char *T_strdup( const char *str );
long T_atol( const char *txt );
long T_atoi( const char *txt );
double T_atof( const char *txt );


#endif   /* ! T_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
