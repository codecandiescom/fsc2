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


#include "fsc2.h"

#if defined MDEBUG
#include <mcheck.h>
#endif


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_malloc( size_t size )
{
	void *mem;
#if defined( MDEBUG )
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


#ifndef NDEBUG
	if ( size == 0 )
	{
		eprint( FATAL, UNSET,
				"Internal error detected at %s:%u (malloc with size 0).\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	mem = malloc( size );

	if ( mem == NULL )
	{
		eprint( FATAL, Internals.mode != TEST, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	if ( Internals.is_i386 )
	{
		asm( "mov %%ebp, %0" : "=g" ( EBP ) );
		fprintf( stderr, "(%d) malloc:  %p (%u) from %p\n",
				 Internals.I_am == CHILD, mem, size,
				 ( void * ) * ( EBP + 1 ) );
	}
	else
		fprintf( stderr, "(%d) malloc:  %p (%u)\n",
				 Internals.I_am == CHILD, mem, size );

	fflush( stderr );
#endif

	return mem;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_calloc( size_t nmemb, size_t size )
{
	void *mem;
#if defined( MDEBUG )
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


#ifndef NDEBUG
	if ( size == 0 )
	{
		eprint( FATAL, UNSET,
				"Internal error detected at %s:%u (calloc with size 0).\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	mem = calloc( nmemb, size );

	if ( mem == NULL )
	{
		eprint( FATAL, Internals.mode != TEST, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	if ( Internals.is_i386 )
	{
		asm( "mov %%ebp, %0" : "=g" ( EBP ) );
		fprintf( stderr, "(%d) calloc:  %p (%u) from %p\n",
				 Internals.I_am == CHILD, mem, nmemb * size,
				 ( void * ) * ( EBP + 1 ) );
	}
	else
		fprintf( stderr, "(%d) calloc:  %p (%u)\n",
				 Internals.I_am == CHILD, mem, nmemb * size );

	fflush( stderr );
#endif

	return mem;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

void *T_realloc( void *ptr, size_t size )
{
	void *new_ptr;
#if defined( MDEBUG )
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


#ifndef NDEBUG
	if ( size == 0 )
	{
		eprint( FATAL, UNSET,
				"Internal error detected at %s:%u (realloc with size 0).\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	new_ptr = realloc( ptr, size );

	if ( new_ptr == NULL )
	{
		eprint( FATAL, Internals.mode != TEST, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	if ( Internals.is_i386 )
	{
		asm( "mov %%ebp, %0" : "=g" ( EBP ) );
		fprintf( stderr, "(%d) realloc: %p -> %p (%u) from %p\n",
				 Internals.I_am == CHILD, ptr, new_ptr, size,
				 ( void * ) * ( EBP + 1 ) );
	}
	else
		fprintf( stderr, "(%d) realloc: %p -> %p (%u)\n",
				 Internals.I_am == CHILD, ptr, new_ptr, size );

	fflush( stderr );
#endif

	return new_ptr;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_free( void *ptr )
{
#if defined( MDEBUG )
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif

	if ( ptr == NULL )
		return NULL;

#if defined MDEBUG
	if ( Internals.is_i386 )
	{
		asm( "mov %%ebp, %0" : "=g" ( EBP ) );
		fprintf( stderr, "(%d) free:    %p from %p\n",
				 Internals.I_am == CHILD, ptr, ( void * ) * ( EBP + 1 ) );
	}
	else
		fprintf( stderr, "(%d) free:    %p\n", Internals.I_am == CHILD, ptr );

	fflush( stderr );
	fsc2_assert( mprobe( ptr ) == MCHECK_OK );
#endif

	free( ptr );
	return NULL;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

char *T_strdup( const char *str )
{
	char *new_str;
#if defined( MDEBUG )
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


	if ( str == NULL )
		return NULL;

	if ( ( new_str = strdup( str ) ) == NULL )
	{
		eprint( FATAL, Internals.mode != TEST, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	if ( Internals.is_i386 )
	{
		asm( "mov %%ebp, %0" : "=g" ( EBP ) );
		fprintf( stderr, "(%d) strdup:  %p (%u) from %p\n",
				 Internals.I_am == CHILD,
				 ( void * ) new_str, strlen( str ) + 1,
				 ( void * ) * ( EBP + 1 ) );
	}
	else
		fprintf( stderr, "(%d) strdup:  %p (%u)\n",
			 Internals.I_am == CHILD, ( void * ) new_str, strlen( str ) + 1 );

	fflush( stderr );
#endif

	return new_str;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

long T_atol( const char *txt )
{
	long ret;
	char *end_p;


	if ( txt == NULL || *txt == '\0' )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%u.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	ret = strtol( txt, &end_p, 10 );

	if ( errno == ERANGE )
	{
		eprint( FATAL, Internals.mode != TEST,
				"Long integer number out of range: %s.\n", txt );
		THROW( EXCEPTION );
	}

	if ( end_p == ( char * ) txt )
	{
		eprint( FATAL, Internals.mode != TEST,
				"Not an integer number: %s.\n", txt );
		THROW( EXCEPTION );
	}

	return ret;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

int T_atoi( const char *txt )
{
	long ret;
	char *end_p;


	if ( txt == NULL || *txt == '\0' )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%u.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	ret = strtol( txt, &end_p, 10 );

	if ( errno == ERANGE || ret > INT_MAX || ret < INT_MIN )
	{
		eprint( FATAL, Internals.mode != TEST,
				"Integer number out of range: %s.\n", txt );
		THROW( EXCEPTION );
	}

	if ( end_p == ( char * ) txt )
	{
		eprint( FATAL, Internals.mode != TEST,
				"Not an integer number: %s.\n", txt );
		THROW( EXCEPTION );
	}

	return ( int ) ret;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

double T_atod( const char *txt )
{
	double ret;
	char *end_p;


	if ( txt == NULL || *txt == '\0' )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%u.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	ret = strtod( txt, &end_p );

	if ( errno == ERANGE )
	{
		eprint( FATAL, Internals.mode != TEST, 
				"Floating point number out of range: %s.\n", txt );
		THROW( EXCEPTION );
	}

	if ( end_p == ( char * ) txt )
	{
		eprint( FATAL, Internals.mode != TEST,
				"Not a floating point number: %s.\n", txt );
		THROW( EXCEPTION );
	}

	return ret;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
