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


/********************************************************************/
/* The basic ideas for the following code came from an article by   */
/* Peter Simons in the iX magazine No. 5, 1998, pp. 160-162. It has */
/* been changed a lot thanks to the very constructive criticism by  */
/* Chris Torek <nospam@elf.eng.bsdi.com> on comp.lang.c.            */
/*                                                                  */
/* In order to avoid overflows of the fixed size exception frame    */
/* stack (i.e. after MAX_NESTED_EXCEPTIONS successful TRY's) it is  */
/* necessary manually remove an exception frame (in contrast to C++ */
/* where this is handled automatically) by calling TRY_SUCCESS if   */
/* the code of the TRY block finished successfully. A typical TRY   */
/* block sequence is thus:                                          */
/*                                                                  */
/* TRY {                                                            */
/*     statement;                                                   */
/*     TRY_SUCCESS;                                                 */
/* }                                                                */
/* CATCH( exception ) {                                             */
/*     ...                                                          */
/* }                                                                */
/*                                                                  */
/* Don't use this exception mechanism from within signal handlers   */
/* or other code invoked asynchronously, it easily could trigger a  */
/* race condition.                                                  */
/********************************************************************/


#include <stdlib.h>
#include <syslog.h>
#include "exceptions.h"
#include <stdio.h>


#define MAX_NESTED_EXCEPTION 64

extern char *prog_name;

static struct {
	const char      *file;
	unsigned int    line;
	Exception_Types exception_type;
	unsigned char   is_thrown;
	jmp_buf         env;
} exception_stack[ MAX_NESTED_EXCEPTION ];

static int exception_stack_pos = -1;


/*--------------------------------------------------------------------------*/
/* Function sets up a new exception frame and returns a pointer to a new    */
/* jmp_buf object needed by setjmp() to store the current environment. The  */
/* function fails if already all slots in the array of Exception structures */
/* are used up (i.e. there are more than MAX_NESTED_EXCEPTION) in which     */
/* case the pogram is stopped immediatedly.                                 */
/*--------------------------------------------------------------------------*/

jmp_buf *push_exception_frame( const char *file, unsigned int line )
{
	if ( exception_stack_pos >= MAX_NESTED_EXCEPTION )
	{
	    syslog( LOG_ERR, "%s: Too many nested exceptions at %s:%u.\n",
				prog_name, file, line );
#ifdef FSC2_HEADER
		if ( Internals.I_am == CHILD )
			_exit( FAIL );
#endif
		exit( EXIT_FAILURE );
	}

	exception_stack[ ++exception_stack_pos ].file = file;
	exception_stack[ exception_stack_pos ].line = line;
	exception_stack[ exception_stack_pos ].is_thrown = 0;
	return &exception_stack[ exception_stack_pos ].env;
}


/*-------------------------------------------------------------------------*/
/* Function removes the current exception frame from the stack. It fails   */
/* (and calls exit() immediately) if the exception stack is already empty. */
/*-------------------------------------------------------------------------*/

void pop_exception_frame( const char *file, int unsigned line )
{
	if ( exception_stack_pos < 0 )
	{
		syslog( LOG_ERR, "%s: Exception stack empty at %s:%u.\n",
				prog_name, file, line );
#ifdef FSC2_HEADER
		if ( Internals.I_am == CHILD )
			_exit( FAIL );
#endif
		exit( EXIT_FAILURE );
	}

	exception_stack_pos--;
}


/*------------------------------------------------------------------------*/
/* Function gets called when an exception is to be thrown. It stores the  */
/* exception type in the current exception frame and returns the address  */
/* of the jmp_buf object that is needed by longjmp() to restore the state */
/* at the time of the corresponding call of setjmp(). If the exception    */
/* stack is already empty (i.e. the exception can't be caught because     */
/* setjmp() was never called) the program is stopped immediately.         */
/*------------------------------------------------------------------------*/

jmp_buf *throw_exception( Exception_Types exception_type )
{
	if ( exception_stack_pos < 0 )
	{
#ifdef FSC2_HEADER
		if ( Internals.I_am == CHILD )
			_exit( FAIL );
#endif
		exit( EXIT_FAILURE );
	}

#ifdef FSC2_HEADER
	lower_permissions( );
#endif

	exception_stack[ exception_stack_pos ].exception_type = exception_type;
	exception_stack[ exception_stack_pos ].is_thrown = 1;
	return &exception_stack[ exception_stack_pos-- ].env;
}


/*-------------------------------------------------------------------------*/
/* Function returns the type of the exception that has been thrown. If no  */
/* exception has been thrown the program is stopped immediately.           */
/*-------------------------------------------------------------------------*/

Exception_Types get_exception_type( const char *file, int unsigned line )
{
	if ( exception_stack_pos + 1 >= MAX_NESTED_EXCEPTION ||
		 ! exception_stack[ exception_stack_pos + 1 ].is_thrown )
	{
	    syslog( LOG_ERR, "%s: Request for type of exception that never got "
				"thrown at %s:%u.\n", prog_name, file, line );
#ifdef FSC2_HEADER
		if ( Internals.I_am == CHILD )
			_exit( FAIL );
#endif
		exit( EXIT_FAILURE );
	}

	return exception_stack[ exception_stack_pos + 1 ].exception_type;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
