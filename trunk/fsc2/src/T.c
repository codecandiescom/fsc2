/*
  $Id$
*/


#include "fsc2.h"

#if defined MDEBUG
#include <mcheck.h>
#endif


void *T_malloc( size_t size )
{
	void *mem;

	mem = malloc( size );

	if ( mem == NULL )
	{
		eprint( FATAL, "%s:%ld: Running out of memory.\n", Fname, Lc );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	fprintf( stderr, "malloc:  %p (%u)\n", mem, size );
#endif

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

#if defined MDEBUG
	fprintf( stderr, "calloc:  %p (%u)\n", mem, nmemb * size );
#endif
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

#if defined MDEBUG
	fprintf( stderr, "realloc: %p -> %p (%u)\n", ptr, new_ptr, size );
#endif

	return new_ptr;
}


void T_free( void *ptr )
{
#if defined MDEBUG
	fprintf( stderr, "free:    %p\n", ptr );
	fflush( stderr );
	assert( mprobe( ptr ) == MCHECK_OK );
#endif

	free( ptr );
}


long T_atol( const char *txt )
{
	long ret;


	ret = strtol( txt, NULL, 10 );
	if ( errno == ERANGE )
	{
		eprint( FATAL, "%s:%ld: Integer number out of range: %s.\n",
				Fname, Lc, txt );
		THROW( EXCEPTION );
	}

	return ret;
}


double T_atof( const char *txt )
{
	double ret;


	ret = strtod( txt, NULL );
	if ( errno == ERANGE )
	{
		eprint( FATAL, "%s:%ld: Floating point number out of range: %s.\n",
				Fname, Lc, txt );
		THROW( EXCEPTION );
	}

	return ret;
}
