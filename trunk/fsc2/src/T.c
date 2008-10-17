/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2.h"

#if defined FSC2_MDEBUG
#include <mcheck.h>
#endif


/*----------------------------------------------------------------*
 * Replacement for malloc(), throws an OUT_OF_MEMORY_EXCEPTION if
 * malloc() does not succeed, checks (as long as NDEBUG isn't
 * defined) that the 'size' argument isn't zero (which would be
 * a hint that there's some bug in the calling code) and, if
 * FSC2_MDEBUG writes out information about the address of the
 * new memory segment, the amount of memory allocated and the
 * address the call came from to stderr.
 *----------------------------------------------------------------*/

void *
T_malloc( size_t size )
{
    void *mem;
#if defined FSC2_MDEBUG
    int *EBP;             /* assumes sizeof(int) equals size of pointers */
#endif


#ifndef NDEBUG
    if ( size == 0 )
    {
        eprint( FATAL, UNSET,
                "Internal error detected at %s:%d (malloc with size = 0).\n",
                __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }
#endif

    if ( ( mem = malloc( size ) ) == NULL )
    {
        print( FATAL, "Running out of memory.\n" );
        THROW( OUT_OF_MEMORY_EXCEPTION );
    }

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
    if ( Fsc2_Internals.is_linux_i386 )
    {
        asm( "mov %%ebp, %0" : "=g" ( EBP ) );
        fprintf( stderr, "(%d) malloc:  %p (%u) from %p\n",
                 Fsc2_Internals.I_am == CHILD, mem, size,
                 ( void * ) * ( EBP + 1 ) );
    }
    else
        fprintf( stderr, "(%d) malloc:  %p (%u)\n",
                 Fsc2_Internals.I_am == CHILD, mem, size );
#endif

    return mem;
}


/*----------------------------------------------------------------*
 * Replacement for calloc(), throws an OUT_OF_MEMORY_EXCEPTION if
 * calloc() does not succeed, checks (as long as NDEBUG isn't
 * defined) that both the 'nmemb' and 'size' arguments aren't
 * zero (which would be a hint that there's some bug in the
 * calling code) and, if FSC2_MDEBUG writes out information about
 * the address of the new memory segment, the amount of memory
 * allocated and the address the call came from to stderr.
 *----------------------------------------------------------------*/

void *
T_calloc( size_t nmemb,
          size_t size )
{
    void *mem;
#if defined FSC2_MDEBUG
    int *EBP;             /* assumes sizeof(int) equals size of pointers */
#endif


#ifndef NDEBUG
    if ( nmemb == 0 )
    {
        eprint( FATAL, UNSET,
                "Internal error detected at %s:%d (calloc with nmemb = 0).\n",
                __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }


    if ( size == 0 )
    {
        eprint( FATAL, UNSET,
                "Internal error detected at %s:%d (calloc with size  = 0).\n",
                __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }
#endif

    if ( ( mem = calloc( nmemb, size ) ) == NULL )
    {
        print( FATAL, "Running out of memory.\n" );
        THROW( OUT_OF_MEMORY_EXCEPTION );
    }

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
    if ( Fsc2_Internals.is_linux_i386 )
    {
        asm( "mov %%ebp, %0" : "=g" ( EBP ) );
        fprintf( stderr, "(%d) calloc:  %p (%u) from %p\n",
                 Fsc2_Internals.I_am == CHILD, mem, nmemb * size,
                 ( void * ) * ( EBP + 1 ) );
    }
    else
        fprintf( stderr, "(%d) calloc:  %p (%u)\n",
                 Fsc2_Internals.I_am == CHILD, mem, nmemb * size );
#endif

    return mem;
}


/*-----------------------------------------------------------------*
 * Replacement for realloc(), throws an OUT_OF_MEMORY_EXCEPTION if
 * realloc() does not succeed, checks (as long as NDEBUG isn't
 * defined) that the 'size' argument isn't zero (which would be a
 * hint that there's some bug in the calling code) and, if
 * FSC2_MDEBUG writes out information about the previous and the
 * new address of the memory segment, the amount of memory
 * allocated and the address the call came from to stderr.
 *-----------------------------------------------------------------*/

void *
T_realloc( void * ptr,
           size_t size )
{
    void *new_ptr;
#if defined FSC2_MDEBUG
    int *EBP;             /* assumes sizeof(int) equals size of pointers */
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

    if ( ( new_ptr = realloc( ptr, size ) ) == NULL )
    {
        print( FATAL, "Running out of memory.\n" );
        THROW( OUT_OF_MEMORY_EXCEPTION );
    }

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
    if ( Fsc2_Internals.is_linux_i386 )
    {
        asm( "mov %%ebp, %0" : "=g" ( EBP ) );
        fprintf( stderr, "(%d) realloc: %p -> %p (%u) from %p\n",
                 Fsc2_Internals.I_am == CHILD, ptr, new_ptr, size,
                 ( void * ) * ( EBP + 1 ) );
    }
    else
        fprintf( stderr, "(%d) realloc: %p -> %p (%u)\n",
                 Fsc2_Internals.I_am == CHILD, ptr, new_ptr, size );
#endif

    return new_ptr;
}


/*-----------------------------------------------------------------*
 * Replacement for realloc(), frees already allocated memory and
 * throws an OUT_OF_MEMORY_EXCEPTION if realloc() does not succeed,
 * checks (as long as NDEBUG isn't defined) that the 'size' argument
 * isn't zero (which would be a hint that there's some bug in the
 * calling code) and, if FSC2_MDEBUG writes out information about
 * the previous and the new address of the memory segment, the
 * amount of memory allocated and the address the call came from
 * to stderr.
 *-----------------------------------------------------------------*/

void *
T_realloc_or_free( void * ptr,
                   size_t size )
{
    void *new_ptr;
#if defined FSC2_MDEBUG
    int *EBP;             /* assumes sizeof(int) equals size of pointers */
#endif


#ifndef NDEBUG
    if ( size == 0 )
    {
        if ( ptr != NULL )
            T_free( ptr );
        eprint( FATAL, UNSET,
                "Internal error detected at %s:%d (realloc with size 0).\n",
                __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }
#endif

    if ( ( new_ptr = realloc( ptr, size ) ) == NULL )
    {
        if ( ptr != NULL )
            T_free( ptr );
        print( FATAL, "Running out of memory.\n" );
        THROW( OUT_OF_MEMORY_EXCEPTION );
    }

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
    if ( Fsc2_Internals.is_linux_i386 )
    {
        asm( "mov %%ebp, %0" : "=g" ( EBP ) );
        fprintf( stderr, "(%d) realloc: %p -> %p (%u) from %p\n",
                 Fsc2_Internals.I_am == CHILD, ptr, new_ptr, size,
                 ( void * ) * ( EBP + 1 ) );
    }
    else
        fprintf( stderr, "(%d) realloc: %p -> %p (%u)\n",
                 Fsc2_Internals.I_am == CHILD, ptr, new_ptr, size );
#endif

    return new_ptr;
}


/*------------------------------------------------------------------*
 * Replacement for free() with the difference that it returns NULL.
 * If FSC2_MDEBUG is defined information about the address of the
 * freed memory segment and the addres the call came from is
 * written to stderr.
 *------------------------------------------------------------------*/

void *
T_free( void * ptr )
{
#if defined FSC2_MDEBUG
    int *EBP;             /* assumes sizeof(int) equals size of pointers */
#endif

    if ( ptr == NULL )
        return NULL;

#if defined FSC2_MDEBUG && ! defined __STRICT_ANSI__
    if ( Fsc2_Internals.is_linux_i386 )
    {
        asm( "mov %%ebp, %0" : "=g" ( EBP ) );
        fprintf( stderr, "(%d) free:    %p from %p\n",
                 Fsc2_Internals.I_am == CHILD, ptr, ( void * ) * ( EBP + 1 ) );
    }
    else
        fprintf( stderr, "(%d) free:    %p\n",
                 Fsc2_Internals.I_am == CHILD, ptr );

    fsc2_assert( mprobe( ptr ) == MCHECK_OK );
#endif

    free( ptr );
    return NULL;
}


/*--------------------------------------------------------------------*
 * Replacement for strdup() (returning NULL if the argument is NULL),
 * throwing an OUT_OF_MEMORY_EXCEPTION if strdup() does not succeed.
 * If FSC2_MDEBUG writes out information about the address of the
 * new memory string, the amount of memory allocated for the string
 * and the address the call came from to stderr.
 *--------------------------------------------------------------------*/

char *
T_strdup( const char * str )
{
    char *new_str;
    size_t len;

#if defined FSC2_MDEBUG
    int *EBP;             /* assumes sizeof(int) equals size of pointers */
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
    if ( Fsc2_Internals.is_linux_i386 )
    {
        asm( "mov %%ebp, %0" : "=g" ( EBP ) );
        fprintf( stderr, "(%d) strdup:  %p (%u) from %p\n",
                 Fsc2_Internals.I_am == CHILD, ( void * ) new_str, len + 1,
                 ( void * ) * ( EBP + 1 ) );
    }
    else
        fprintf( stderr, "(%d) strdup:  %p (%u)\n",
             Fsc2_Internals.I_am == CHILD, ( void * ) new_str, len + 1 );
#endif

    return new_str;
}


/*--------------------------------------------------------------------*
 * Replacement for atol() and strtol(), throwing an EXCEPTION if the
 * string passed as argument can't be converted to a long. If NDEBUG
 * isn't defined the string pointer is also tested for being not NULL
 * or just pointing to an empty string, which indicates a serious bug
 * in the calling function.
 *--------------------------------------------------------------------*/

long
T_atol( const char * txt )
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

    if ( ( ret == LONG_MIN || ret == LONG_MAX ) && errno == ERANGE )
    {
        print( FATAL, "Long integer number out of range: %s.\n", txt );
        THROW( EXCEPTION );
    }

    if ( end_p == ( char * ) txt )
    {
        print( FATAL, "Not a long integer number: %s.\n", txt );
        THROW( EXCEPTION );
    }

    return ret;
}


/*--------------------------------------------------------------------*
 * Replacement for atoi(), throwing an EXCEPTION if the string passed
 * as argument can't be converted to an int. If NDEBUG isn't defined
 * the string pointer is also tested for being not NULL or pointing
 * to an empty string, which indicates a serious bug in the calling
 * function.
 *--------------------------------------------------------------------*/

int
T_atoi( const char * txt )
{
    long ret = T_atol( txt );

    if ( ret > INT_MAX || ret < INT_MIN )
    {
        print( FATAL, "Integer number out of range: %s.\n", txt );
        THROW( EXCEPTION );
    }

    return ( int ) ret;
}


/*--------------------------------------------------------------------*
 * Replacement for atof() and strtod(), throwing an EXCEPTION if the
 * string passed as the argument can't be converted to a double. If
 * NDEBUG isn't defined the string pointer is also tested for being
 * not NULL or pointing to an empty string, which indicates a serious
 * bug in the calling function.
 *--------------------------------------------------------------------*/

double
T_atod( const char * txt )
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
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
