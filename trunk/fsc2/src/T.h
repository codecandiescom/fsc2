/*
  $Id$
*/

#if ! defined T_HEADER
#define T_HEADER

#include "fsc2.h"


void *T_malloc( size_t size );
void *T_calloc( size_t nmemb, size_t size );
void *T_realloc( void *ptr, size_t size );
void T_free( void *ptr );

#endif   /* ! T_HEADER */
