/*
  $Id$
*/

/***************************************************************************/
/*                                                                         */
/*      The following code is taken from an article by Peter Simons        */
/*            in the iX magazine No. 5, 1998, pp. 160-162.                 */
/*                                                                         */
/*  In order to avoid exception stack overflows (e.g. after 256 successful */
/*  calls with TRY) I found it necessary to decrement the stack pointer    */
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


#include <syslog.h>
#define EXCEPTIONS_SOURCE_COMPILE
#include "exceptions.h"


void longjmperror( void );

jmp_buf       exception_env_stack[ MAX_NEST_EXCEPTION ];
unsigned int  exception_env_stack_pos;
unsigned int  exception_id;


void longjmperror( void )
{
	syslog( LOG_ERR, "FATAL ERROR: Uncaught exception %d\n", exception_id );
	exit( exception_id );
}
