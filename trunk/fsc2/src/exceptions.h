/*
  $Id$
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
/*  Further changes are checks to avoid overflow of the exception stack.   */
/***************************************************************************/


#ifndef EXCEPTIONS_HEADER
#define EXCEPTIONS_HEADER

#include <setjmp.h>

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
	ABORT_EXCEPTION             = ( 1 << 11 ),
};


#ifndef MAX_NESTED_EXCEPTION
#define MAX_NESTED_EXCEPTION 256
#endif


#define TRY \
    if ( exception_env_stack_pos >= MAX_NESTED_EXCEPTION - 1 ) \
	{ \
	    syslog( LOG_ERR, "INTERNAL ERROR: Too many exceptions at %s:%d.\n", \
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

#define PASSTHROU( ) \
    THROW( exception_id )

#define THROW( e ) \
    { \
        exception_id = e; \
        if ( exception_env_stack_pos == 0 ) \
		    longjmperror( ); \
        longjmp( exception_env_stack[ --exception_env_stack_pos ], e ); \
    }

#ifndef EXCEPTIONS_SOURCE_COMPILE

extern jmp_buf exception_env_stack[ ];
extern unsigned int exception_env_stack_pos;
extern unsigned int exception_id;
extern void longjmperror( void );

#endif  /* EXCEPTIONS_SOURCE_COMPILE */


#endif  /* EXCEPTIONS_HEADER */
