/*
  $Id$
*/


#include "fsc2.h"

#if defined MDEBUG
#include <mcheck.h>
#endif


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_malloc( size_t size )
{
	void *mem;

	mem = malloc( size );

	if ( mem == NULL )
	{
		eprint( FATAL, Fname != NULL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	fprintf( stderr, "(%d) malloc:  %p (%u)\n",
			 I_am == CHILD, mem, size );
	fflush( stderr );
#endif

	return mem;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_calloc( size_t nmemb, size_t size )
{
	void *mem;

	mem = calloc( nmemb, size );

	if ( mem == NULL )
	{
		eprint( FATAL, Fname != NULL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	fprintf( stderr, "(%d) calloc:  %p (%u)\n",
			 I_am == CHILD, mem, nmemb * size );
	fflush( stderr );
#endif
	return mem;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

void *T_realloc( void *ptr, size_t size )
{
	void *new_ptr;

	new_ptr = realloc( ptr, size );

	if ( new_ptr == NULL )
	{
		eprint( FATAL, Fname != NULL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	fprintf( stderr, "(%d) realloc: %p -> %p (%u)\n",
			 I_am == CHILD, ptr, new_ptr, size );
	fflush( stderr );
#endif

	return new_ptr;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_free( void *ptr )
{
	if ( ptr == NULL )
		return NULL;

#if defined MDEBUG
	fprintf( stderr, "(%d) free:    %p\n", I_am == CHILD, ptr );
	fflush( stderr );
	assert( mprobe( ptr ) == MCHECK_OK );
#endif

	free( ptr );
	return NULL;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

char *T_strdup( const char *str )
{
	char *new_str;


	if ( str == NULL )
		return NULL;

	if ( ( new_str = strdup( str ) ) == NULL )
	{
		eprint( FATAL, Fname != NULL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	fprintf( stderr, "(%d) strdup:  %p (%u)\n",
			 I_am == CHILD, new_str, strlen( str ) + 1 );
	fflush( stderr );
#endif

	return new_str;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

long T_atol( const char *txt )
{
	long ret;


	ret = strtol( txt, NULL, 10 );
	if ( errno == ERANGE )
	{
		eprint( FATAL, SET, "Integer number out of range: %s.\n", txt );
		THROW( EXCEPTION );
	}

	return ret;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

double T_atof( const char *txt )
{
	double ret;


	ret = strtod( txt, NULL );
	if ( errno == ERANGE )
	{
		eprint( FATAL, SET, "Floating point number out of range: %s.\n", txt );
		THROW( EXCEPTION );
	}

	return ret;
}
