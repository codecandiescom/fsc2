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

/***************************************************************************/
/*                                                                         */
/*      The following code is taken from an article by Peter Simons        */
/*            in the iX magazine No. 5, 1998, pp. 160-162.                 */
/*                                                                         */
/*  In order to avoid exception stack overflows (e.g. after 256 successful */
/*  calls with TRY) I found it is necessary to decrement the stack pointer */
/*  `exception_env_stack_pos' after each successful TRY, as defined by the */
/*  macro `TRY_SUCCESS'. A typical sequence is thus:                       */
/*                                                                         */
/*  TRY {                                                                  */
/*      statement;                                                         */
/*		TRY_SUCCESS;                                                       */
/*  }                                                                      */
/*  CATCH( exception ) {                                                   */
/*      ...                                                                */
/*  }                                                                      */
/*                                                                         */
/*  Further changes are checks to avoid overflow of the exception stack    */
/*  and, for added security, switching to the users privileges whenever    */
/*  an exception is thrown.                                                */
/***************************************************************************/


#ifndef EXCEPTIONS_HEADER
#define EXCEPTIONS_HEADER

#include <unistd.h>
#include <setjmp.h>
#include <syslog.h>


#ifndef MAX_NESTED_EXCEPTION
#define MAX_NESTED_EXCEPTION 256
#endif

#ifndef EXCEPTIONS_SOURCE_COMPILE

extern jmp_buf exception_env_stack[ ];
extern unsigned int exception_env_stack_pos;
extern int exception_id;
extern void longjmperror( void );

#endif  /* ! EXCEPTIONS_SOURCE_COMPILE */


enum {
	NO_EXCEPTION                = 0,               /* must be 0 ! */
	EXCEPTION                   = ( 1 <<  0 ),
	OUT_OF_MEMORY_EXCEPTION     = ( 1 <<  1 ),
	TOO_DEEPLY_NESTED_EXCEPTION = ( 1 <<  2 ),
	EOF_IN_COMMENT_EXCEPTION    = ( 1 <<  3 ),
	EOF_IN_STRING_EXCEPTION     = ( 1 <<  4 ),
	DANGLING_END_OF_COMMENT     = ( 1 <<  5 ),
	MISPLACED_SHEBANG           = ( 1 <<  6 ),
	SYNTAX_ERROR_EXCEPTION      = ( 1 <<  7 ),
	MISSING_SEMICOLON_EXCEPTION = ( 1 <<  8 ),
	INVALID_INPUT_EXCEPTION     = ( 1 <<  9 ),
	USER_BREAK_EXCEPTION        = ( 1 << 10 ),
	ABORT_EXCEPTION             = ( 1 << 11 )
};



#define TRY \
    if ( exception_env_stack_pos >= MAX_NESTED_EXCEPTION ) \
	{ \
	    syslog( LOG_ERR, "Internal error: Too many exceptions at %s:%d.\n", \
				__FILE__, __LINE__ ); \
		exit( -1 ); \
	} \
    exception_id = setjmp( exception_env_stack[ exception_env_stack_pos++ ] );\
    if ( exception_id == NO_EXCEPTION )

#define TRY_SUCCESS  \
    --exception_env_stack_pos

#define CATCH( e ) \
    else if ( exception_id & ( e ) )

#define OTHERWISE \
    else

#define PASSTHROUGH( ) \
    THROW( exception_id )

#define THROW( e ) \
    do \
    { \
		lower_permissions( ); \
        exception_id = e; \
        if ( exception_env_stack_pos == 0 ) \
		    longjmperror( ); \
        longjmp( exception_env_stack[ --exception_env_stack_pos ], e ); \
    } while ( 0 )



#endif  /* ! EXCEPTIONS_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
