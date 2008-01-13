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


/*******************************************************************
 * The original ideas for the following code came from an article  *
 * by Peter Simons in the iX magazine No. 5, 1998, pp. 160-162. It *
 * has been changed a lot thanks to very constructive criticism by *
 * Chris Torek <nospam@elf.eng.bsdi.com> on comp.lang.c (which is  *
 * meant to mean that he would be responsible for its flaws!)      *
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
 * OTHERWISE                                                       *
 *     RETHROW( );                                                 *
 *                                                                 *
 * Don't use this exception mechanism from within signal handlers  *
 * or other code invoked asynchronously, it easily could trigger a *
 * race condition.                                                 *
 *******************************************************************/


#ifndef EXCEPTIONS_HEADER
#define EXCEPTIONS_HEADER

#include <setjmp.h>


typedef enum Exception_Types Exception_Types_T;

enum Exception_Types {
    EXCEPTION,
    OUT_OF_MEMORY_EXCEPTION,
    TOO_DEEPLY_NESTED_EXCEPTION,
    EOF_IN_COMMENT_EXCEPTION,
    EOF_IN_STRING_EXCEPTION,
    DANGLING_END_OF_COMMENT,
    MISPLACED_SHEBANG,
    MISSING_SEMICOLON_EXCEPTION,
    INVALID_INPUT_EXCEPTION,
    USER_BREAK_EXCEPTION,
    ABORT_EXCEPTION
};


jmp_buf *push_exception_frame( const char * /* file */,
                               int          /* line */  );

void pop_exception_frame( const char * /* file */,
                          int          /* line */  );

jmp_buf *throw_exception( Exception_Types_T /* type */ );

Exception_Types_T get_exception_type( const char * /* file */,
                                      int          /* line */  );


#define TRY  if ( setjmp( *push_exception_frame( __FILE__, __LINE__ ) ) == 0 )

#define TRY_SUCCESS  pop_exception_frame( __FILE__, __LINE__ )

#define THROW( e )  longjmp( *throw_exception( e ), 1 )

#define RETHROW( )  THROW( get_exception_type( __FILE__, __LINE__ ) )

#define CATCH( e )  \
                  else if ( get_exception_type( __FILE__, __LINE__ ) == ( e ) )

#define OTHERWISE  else

/* Automatic variables are not necessarily restored after a longjmp() when
   they were stored in a register, so we have to keep the compiler from
   putting automatic variable that could be clobbered into a register.
   The following hack seems to do the trick for all gcc versions. */

#define CLOBBER_PROTECT( a )                                 \
     do {                                                    \
         unsigned char *xI3_2sYzii = ( unsigned char * ) &a; \
         * ( unsigned char * ) &a = *xI3_2sYzii;             \
     } while( 0 )


#endif  /* ! EXCEPTIONS_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
