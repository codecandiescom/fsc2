/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if ! defined T_HEADER
#define T_HEADER

#include "fsc2.h"


void *T_malloc( size_t /* size */ );

void *T_calloc( size_t /* nmemb */,
                size_t /* size  */  );

void *T_realloc( void * /* ptr  */,
                 size_t /* size */  );

void *T_realloc_or_free( void * /* ptr  */,
                         size_t /* size */  );

void *T_free( void * /* ptr */ );

char *T_strdup( const char * /* str */ );

long T_atol( const char * /* txt */ );

int T_atoi( const char * /* txt */ );

double T_atod( const char * /* txt */ );


#endif   /* ! T_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
