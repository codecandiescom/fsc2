/*
  $Id$
*/


#include "fsc2.h"

#include <mcheck.h>


void *T_malloc( size_t size )
{
	void *mem;

	mem = malloc( size );

	if ( mem == NULL )
	{
		eprint( FATAL, "%s:%ld: Running out of memory.\n", Fname, Lc );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

	return mem;
}


void *T_calloc( size_t nmemb, size_t size )
{
	void *mem;

	mem = calloc( nmemb, size );

	if ( mem == NULL )
	{
		eprint( FATAL, "%s:%ld: Running out of memory.\n", Fname, Lc );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

	return mem;
}


void *T_realloc( void *ptr, size_t size )
{
	void *new_ptr;

	new_ptr = realloc( ptr, size );

	if ( new_ptr == NULL )
	{
		eprint( FATAL, "%s:%ld: Running out of memory.\n", Fname, Lc );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

	return new_ptr;
}


void T_free( void *ptr )
{
#if defined DEBUG
	assert( mprobe( ptr ) == MCHECK_OK );
#endif

	free( ptr );
}
