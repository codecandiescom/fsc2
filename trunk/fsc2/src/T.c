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


#include "fsc2.h"

#if defined MDEBUG
#include <mcheck.h>
#endif


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_malloc( size_t size )
{
	void *mem;
#if defined MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif

	mem = malloc( size );

	if ( mem == NULL )
	{
		eprint( FATAL, Fname != NULL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	asm( "mov %%ebp, %0" : "=g" ( EBP ) );
	fprintf( stderr, "(%d) malloc:  %p (%u) from %p\n",
			 I_am == CHILD, mem, size, ( int * ) * ( EBP + 1 ) );
	fflush( stderr );
#endif

	return mem;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_calloc( size_t nmemb, size_t size )
{
	void *mem;
#if defined MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


	mem = calloc( nmemb, size );

	if ( mem == NULL )
	{
		eprint( FATAL, Fname != NULL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	asm( "mov %%ebp, %0" : "=g" ( EBP ) );
	fprintf( stderr, "(%d) calloc:  %p (%u) from %p\n",
			 I_am == CHILD, mem, nmemb * size, ( int * ) * ( EBP + 1 ) );
	fflush( stderr );
#endif
	return mem;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

void *T_realloc( void *ptr, size_t size )
{
	void *new_ptr;
#if defined MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif

	new_ptr = realloc( ptr, size );

	if ( new_ptr == NULL )
	{
		eprint( FATAL, Fname != NULL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	asm( "mov %%ebp, %0" : "=g" ( EBP ) );
	fprintf( stderr, "(%d) realloc: %p -> %p (%u) from %p\n",
			 I_am == CHILD, ptr, new_ptr, size, ( int * ) * ( EBP + 1 ) );
	fflush( stderr );
#endif

	return new_ptr;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void *T_free( void *ptr )
{
#if defined MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif

	if ( ptr == NULL )
		return NULL;

#if defined MDEBUG
	asm( "mov %%ebp, %0" : "=g" ( EBP ) );
	fprintf( stderr, "(%d) free:    %p from %p\n",
		     I_am == CHILD, ptr, ( int * ) * ( EBP + 1 ) );
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
#if defined MDEBUG
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


	if ( str == NULL )
		return NULL;

	if ( ( new_str = strdup( str ) ) == NULL )
	{
		eprint( FATAL, Fname != NULL, "Running out of memory.\n" );
		THROW( OUT_OF_MEMORY_EXCEPTION );
	}

#if defined MDEBUG
	asm( "mov %%ebp, %0" : "=g" ( EBP ) );
	fprintf( stderr, "(%d) strdup:  %p (%u) from %p\n",
			 I_am == CHILD, new_str, strlen( str ) + 1,
			 ( int * ) * ( EBP + 1 ) );
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
