/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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

#if defined FSC2_MDEBUG
#include <mcheck.h>
#endif


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_malloc( size_t size )
{
	void *mem;
#if defined FSC2_MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


#ifndef NDEBUG
	if ( size == 0 )
	{
		eprint( FATAL, UNSET,
				"Internal error detected at %s:%d (malloc with size 0).\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	mem = malloc( size );

	if ( mem == NULL )
	{
		print( FATAL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
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
#endif

	return mem;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_calloc( size_t nmemb, size_t size )
{
	void *mem;
#if defined FSC2_MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


#ifndef NDEBUG
	if ( size == 0 )
	{
		eprint( FATAL, UNSET,
				"Internal error detected at %s:%d (calloc with size 0).\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	mem = calloc( nmemb, size );

	if ( mem == NULL )
	{
		print( FATAL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
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
#endif

	return mem;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

void *T_realloc( void *ptr, size_t size )
{
	void *new_ptr;
#if defined FSC2_MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


#ifndef NDEBUG
	if ( size == 0 )
	{
		eprint( FATAL, UNSET,
				"Internal error detected at %s:%d (realloc with size 0).\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	new_ptr = realloc( ptr, size );

	if ( new_ptr == NULL )
	{
		print( FATAL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
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
#endif

	return new_ptr;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_free( void *ptr )
{
#if defined FSC2_MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif

	if ( ptr == NULL )
		return NULL;

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
	if ( Internals.is_i386 )
	{
		asm( "mov %%ebp, %0" : "=g" ( EBP ) );
		fprintf( stderr, "(%d) free:    %p from %p\n",
				 Internals.I_am == CHILD, ptr, ( void * ) * ( EBP + 1 ) );
	}
	else
		fprintf( stderr, "(%d) free:    %p\n", Internals.I_am == CHILD, ptr );

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
	size_t len;

#if defined FSC2_MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


	if ( str == NULL )
		return NULL;

	len = strlen( str );

	if ( ( new_str = malloc( len + 1 ) ) == NULL )
	{
		print( FATAL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

	strcpy( new_str, str );

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
	if ( Internals.is_i386 )
	{
		asm( "mov %%ebp, %0" : "=g" ( EBP ) );
		fprintf( stderr, "(%d) strdup:  %p (%u) from %p\n",
				 Internals.I_am == CHILD, ( void * ) new_str, len + 1,
				 ( void * ) * ( EBP + 1 ) );
	}
	else
		fprintf( stderr, "(%d) strdup:  %p (%u)\n",
			 Internals.I_am == CHILD, ( void * ) new_str, len + 1 );
#endif

	return new_str;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

long T_atol( const char *txt )
{
	long ret;
	char *end_p;


#ifndef NDEBUG
	if ( txt == NULL || *txt == '\0' )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	ret = strtol( txt, &end_p, 10 );

	if ( errno == ERANGE )
	{
		print( FATAL, "Long integer number out of range: %s.\n", txt );
		THROW( EXCEPTION );
	}

	if ( end_p == ( char * ) txt )
	{
		print( FATAL, "Not an integer number: %s.\n", txt );
		THROW( EXCEPTION );
	}

	return ret;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

long T_htol( const char *txt )
{
	long ret;
	char *end_p;


#ifndef NDEBUG
	if ( txt == NULL || *txt == '\0' ||
		 ( *txt != '0' && tolower( txt[ 1 ] ) != 'x' ) )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	ret = strtol( txt + 2, &end_p, 16 );

	if ( errno == ERANGE )
	{
		print( FATAL, "Long integer number out of range: %s.\n", txt );
		THROW( EXCEPTION );
	}

	if ( end_p == ( char * ) ( txt + 2 ) )
	{
		print( FATAL, "Not an integer number: %s.\n", txt );
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


#ifndef NDEBUG
	if ( txt == NULL || *txt == '\0' )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	ret = strtol( txt, &end_p, 10 );

	if ( errno == ERANGE || ret > INT_MAX || ret < INT_MIN )
	{
		print( FATAL, "Integer number out of range: %s.\n", txt );
		THROW( EXCEPTION );
	}

	if ( end_p == ( char * ) txt )
	{
		print( FATAL, "Not an integer number: %s.\n", txt );
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


#ifndef NDEBUG
	if ( txt == NULL || *txt == '\0' )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	ret = strtod( txt, &end_p );

	if ( errno == ERANGE )
	{
		print( FATAL, "Floating point number out of range: %s.\n", txt );
		THROW( EXCEPTION );
	}

	if ( end_p == ( char * ) txt )
	{
		print( FATAL, "Not a floating point number: %s.\n", txt );
		THROW( EXCEPTION );
	}

	return ret;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
