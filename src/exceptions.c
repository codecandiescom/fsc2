/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


/*******************************************************************
 * The original ideas for the following code came from an article  *
 * by Peter Simons in the iX magazine No. 5, 1998, pp. 160-162. It *
 * has been changed a lot thanks to very constructive criticism by *
 * Chris Torek <nospam@elf.eng.bsdi.com> on comp.lang.c (which     *
 * doesn't meant that he would be responsible for the flaws!)      *
 *                                                                 *
 * In order to avoid overflows of the fixed size exception frame   *
 * stack (i.e. after MAX_NESTED_EXCEPTIONS successful TRY's) it is *
 * necessary to manually remove an exception frame (in contrast to *
 * C++ where this is handled automatically) by calling TRY_SUCCESS *
 * if the code of the TRY block finished successfully. A typical   *
 * TRY block sequence is thus:                                     *
 *                                                                 *
 * TRY {                                                           *
 *     statement;                                                  *
 *     TRY_SUCCESS;                                                *
 * }                                                               *
 * CATCH( exception ) {                                            *
 *     ...                                                         *
 * }                                                               *
 *                                                                 *
 * Don't use this exception mechanism from within signal handlers  *
 * or other code invoked asynchronously, it easily could trigger a *
 * race condition.                                                 *
 *******************************************************************/


#include <stdlib.h>
#include <syslog.h>
#include "exceptions.h"
#include <stdio.h>


#define MAX_NESTED_EXCEPTION 64

extern char *Prog_Name;        /* defined in global.c */

struct Exception_Struct {
    const char        * file;
    unsigned int        line;
    Exception_Types_T   type;
    unsigned char       is_thrown;
    jmp_buf             env;
};

static struct Exception_Struct Exception_stack[ MAX_NESTED_EXCEPTION ],
                               Stored_exceptions[ MAX_NESTED_EXCEPTION ];
static int Exception_stack_pos = -1;


/*--------------------------------------------------------------------------*
 * Function sets up a new exception frame and returns a pointer to a new
 * jmp_buf object needed by setjmp() to store the current environment. The
 * function fails if already all slots in the array of Exception structures
 * are used up (i.e. there are more than MAX_NESTED_EXCEPTION) in which
 * case the pogram throwing the exception is stopped immediatedly.
 * Actually, there are two arrays of exceptions. The second array is
 * necessary for being able to rethrow a caught exception even when in the
 * code dealing with the exception another TRY block is used. This new TRY
 * block would overwrite the array element for the thrown exception and
 * thus would make it impossible to rethrow it. So whenever a TRY block is
 * started (by calling this function) the state of the array element to be
 * overwritten is saved in the second array (and restored when this TRY
 * block succeded and thus the corresponding element gets removed from the
 * array), so that we're back to the state we were in before the TRY block
 * within the exception handler was started.
 *--------------------------------------------------------------------------*/

jmp_buf *
push_exception_frame( const char * file,
                      int          line )
{
    if ( Exception_stack_pos + 1 >= MAX_NESTED_EXCEPTION )
    {
        fprintf( stderr, "%s: Too many nested exceptions at %s:%d.\n",
                 Prog_Name, file, line );
        syslog( LOG_ERR, "%s: Too many nested exceptions at %s:%d.\n",
                Prog_Name, file, line );
#ifdef FSC2_HEADER
        if ( Fsc2_Internals.I_am == CHILD )
            _exit( FAIL );
#endif
        exit( EXIT_FAILURE );
    }

    ++Exception_stack_pos;
    Stored_exceptions[ Exception_stack_pos ] =
                                        Exception_stack[ Exception_stack_pos ];
    Exception_stack[ Exception_stack_pos ].file = file;
    Exception_stack[ Exception_stack_pos ].line = line;
    Exception_stack[ Exception_stack_pos ].is_thrown = 0;
    return &Exception_stack[ Exception_stack_pos ].env;
}


/*-------------------------------------------------------------------------*
 * Function removes the current exception frame from the stack. It fails
 * (and calls exit() immediately) if the exception stack is already empty.
 *-------------------------------------------------------------------------*/

void
pop_exception_frame( const char * file,
                     int          line )
{
    if ( Exception_stack_pos < 0 )
    {
        fprintf( stderr, "%s: Exception stack empty at %s:%d.\n",
                 Prog_Name, file, line );
        syslog( LOG_ERR, "%s: Exception stack empty at %s:%d.\n",
                Prog_Name, file, line );
#ifdef FSC2_HEADER
        if ( Fsc2_Internals.I_am == CHILD )
            _exit( FAIL );
#endif
        exit( EXIT_FAILURE );
    }

    Exception_stack[ Exception_stack_pos ] =
                                      Stored_exceptions[ Exception_stack_pos ];
    Exception_stack_pos--;
}


/*------------------------------------------------------------------------*
 * This function gets called when an exception is to be thrown. It stores
 * the exception type in the current exception frame and returns the
 * address of the jmp_buf object that is needed by longjmp() to restore
 * the state at the time of the corresponding call of setjmp(). If the
 * exception stack is already empty (i.e. the exception can't be caught
 * because setjmp() was never called) the program is stopped immediately.
 *------------------------------------------------------------------------*/

jmp_buf *
throw_exception( Exception_Types_T type )
{
    if ( Exception_stack_pos < 0 )
    {
#ifdef FSC2_HEADER
        if ( Fsc2_Internals.I_am == CHILD )
            _exit( FAIL );
#endif
        exit( EXIT_FAILURE );
    }

#ifdef FSC2_HEADER
    /* Make sure that carelessly written code where the exception is thrown
       while the program has higher permissions than the user opens up a
       security hole */

    lower_permissions( );
#endif

    Exception_stack[ Exception_stack_pos ].type = type;
    Exception_stack[ Exception_stack_pos ].is_thrown = 1;
    return &Exception_stack[ Exception_stack_pos-- ].env;
}


/*-------------------------------------------------------------------------*
 * Function returns the type of the exception that has been thrown. If no
 * exception has been thrown the program is stopped immediately.
 *-------------------------------------------------------------------------*/

Exception_Types_T
get_exception_type( const char * file,
                    int          line )
{
    if (    Exception_stack_pos + 1 >= MAX_NESTED_EXCEPTION
         || ! Exception_stack[ Exception_stack_pos + 1 ].is_thrown )
    {
        fprintf( stderr, "%s: Request for type of an exception that never had "
                 "been thrown at %s:%d.\n", Prog_Name, file, line );
        syslog( LOG_ERR, "%s: Request for type of an exception that never had "
                "been thrown at %s:%d.\n", Prog_Name, file, line );
#ifdef FSC2_HEADER
        if ( Fsc2_Internals.I_am == CHILD )
            _exit( FAIL );
#endif
        exit( EXIT_FAILURE );
    }

    if ( Exception_stack_pos < -1 )
    {
        fprintf( stderr, "%s: Exception stack is empty at %s:%d.\n",
                 Prog_Name, file, line );
        syslog( LOG_ERR, "%s: Exception stack is empty at %s:%d.\n",
                Prog_Name, file, line );
#ifdef FSC2_HEADER
        if ( Fsc2_Internals.I_am == CHILD )
            _exit( FAIL );
#endif
        exit( EXIT_FAILURE );
    }

    return Exception_stack[ Exception_stack_pos + 1 ].type;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
