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
/*  calls with TRY) I found it necessary to decrement the stack pointer    */
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


#include <syslog.h>
#define EXCEPTIONS_SOURCE_COMPILE
#include "exceptions.h"


void longjmperror( void );

jmp_buf       exception_env_stack[ MAX_NESTED_EXCEPTION ];
unsigned int  exception_env_stack_pos = 0;
int           exception_id;


void longjmperror( void )
{
	syslog( LOG_ERR, "INTERNAL ERROR: Uncaught exception %d at %s:%d.\n",
			exception_id, __FILE__, __LINE__ );
	exit( - exception_id );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
