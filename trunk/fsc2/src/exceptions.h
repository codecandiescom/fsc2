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
/*  `exception_env_stack_pos' after each successful TRY, as defined with   */
/*  the macro `TRY_SUCCESS'. A typical sequence is thus:                   */
/*                                                                         */
/*  TRY {                                                                  */
/*      statement;                                                         */
/*		TRY_SUCCESS;                                                       */
/*  }                                                                      */
/*  CATCH( exception ) {                                                   */
/*      ...                                                                */
/*  }                                                                      */
/*                                                                         */
/***************************************************************************/


#ifndef EXCEPTIONS_HEADER
#define EXCEPTIONS_HEADER

#include <setjmp.h>

enum {
	NO_EXCEPTION = 0,               /* must be 0 ! */
	OUT_OF_MEMORY_EXCEPTION,
	TOO_DEEPLY_NESTED_EXCEPTION,
	EOF_IN_COMMENT_EXCEPTION,
	EOF_IN_STRING_EXCEPTION,
	DANGLING_END_OF_COMMENT,
	UNDEFINED_SECTION_EXCEPTION,
	SYNTAX_ERROR_EXCEPTION,
	FLOATING_POINT_EXCEPTION,
	MISSING_SEMICOLON_EXCEPTION,
	INVALID_INPUT_EXCEPTION,
	CLEANER_EXCEPTION,
	ASSIGNMENTS_EXCEPTION,
	ACCESS_NONEXISTING_VARIABLE,
	PHASES_EXCEPTION,
	MULTIPLE_VARIABLE_DEFINITION_EXCEPTION,
	UNKNOWN_FUNCTION_EXCEPTION,
	PRINT_SYNTAX_EXCEPTION,
	VARIABLES_EXCEPTION,
	FUNCTION_EXCEPTION,
	PREPARATIONS_EXCEPTION,
	DEFAULTS_EXCEPTION,
	BLOCK_ERROR_EXCEPTION,
	EXPERIMENT_EXCEPTION,
	CONDITION_EXCEPTION,
	DEVICES_EXCEPTION,
	LIBRARY_EXCEPTION
};


#define TRY \
    exception_id = setjmp( exception_env_stack[ exception_env_stack_pos++ ] );\
    if ( exception_id == NO_EXCEPTION )

#define TRY_SUCCESS   exception_env_stack_pos--

#define CATCH( e ) \
    else if ( exception_id == ( e ) )

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

#ifndef MAX_NEST_EXCEPTION
#define MAX_NEST_EXCEPTION 256
#endif

#ifndef EXCEPTIONS_SOURCE_COMPILE

extern jmp_buf      exception_env_stack[ ];
extern unsigned int exception_env_stack_pos;
extern unsigned int exception_id;
extern void longjmperror( void );

#endif  /* EXCEPTIONS_SOURCE_COMPILE */


#endif  /* EXCEPTIONS_HEADER */
