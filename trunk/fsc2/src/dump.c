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


enum {
	DUMP_PARENT_READ = 0,
	DUMP_CHILD_WRITE,
	DUMP_CHILD_READ,
	DUMP_PARENT_WRITE
};

enum {
	DUMP_ANSWER_READ = 0,
	DUMP_ANSWER_WRITE
};


/*-----------------------------------------------------------------------*/
/* This function is highly hardware depended, i.e. it will only work on  */
/* i386 type processors, so it returns immediately if the machine is not */
/* a i386.                                                               */
/* This function is called from the signal handler for 'deadly' signals, */
/* e.g. SIGSEGV etc. It tries to figure out where this signal happend    */
/* and creating a backtrace by running through the stackframes and       */
/* determining from the return addresses and with the help of the GNU    */
/* utility 'addr2line' the function name and the source file and line    */
/* number. The result is written to the write end of a pipe that is      */
/* (mis)used as a temporary buffer, from which the results can be read   */
/* later (the read end is a global variable named 'fail_mess_fd').       */
/* If the macro ADDR2LINE isn't defined the function will do nothing.    */
/* If it is defined it must be the complete path to 'addr2line'. The     */
/* best way to set it correctly is probably to have lines like           */
/* ADDR2LINE = $(shell which addr2line)                                  */
/* ifneq ($(word 1,$(ADDR2LINE)),which:)                                 */
/*     CFLAGS += -DADDR2LINE=\"$(ADDR2LINE)\"                            */
/* endif                                                                 */
/* in the Makefile.                                                      */
/*-----------------------------------------------------------------------*/

void DumpStack( void )
{
#if defined( ADDR2LINE )

	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
	int answer_fd[ 2 ];
	int pipe_fd[ 4 ];
	pid_t pid;
	int i, k = 1;
	char buf[ 32 ];
	char c;
	struct sigaction sact;


	if ( ! is_i386 )
		return;

	/* Childs death signal isn't needed */

	sact.sa_handler = SIG_DFL;
	sact.sa_flags = 0;
	sigaction( SIGCHLD, &sact, NULL );

	/* Don't crash with SIGPIPE if child process fails to exec (in this case
	   the output will only contain the addresses) */

	sact.sa_handler = SIG_IGN;
	sact.sa_flags = 0;
	sigaction( SIGPIPE, &sact, NULL );

	/* Setup all the pipes, two for communication with child process and one
	   as a temporary buffer for the result */

	if ( pipe( pipe_fd ) < 0 )
		return;
	if ( pipe( pipe_fd + 2 ) < 0 )
	{
		for ( i = 0; i < 2; i++ )
			close( pipe_fd[ i ] );
		return;
	}

	if ( pipe( answer_fd ) < 0 )
	{
		for ( i = 0; i < 4; i++ )
			close( pipe_fd[ i ] );
		return;
	}

	/* Create child running ADDR2LINE with input and output redirected */

	if ( ( pid = fork( ) ) == 0 )
	{
		close( pipe_fd[ DUMP_PARENT_READ ] );
		close( pipe_fd[ DUMP_PARENT_WRITE ] );

		close( STDOUT_FILENO );
		close( STDERR_FILENO );
		close( STDIN_FILENO );

		dup2( pipe_fd[ DUMP_CHILD_WRITE ], STDOUT_FILENO );
		dup2( pipe_fd[ DUMP_CHILD_WRITE ], STDERR_FILENO );
		dup2( pipe_fd[ DUMP_CHILD_READ  ], STDIN_FILENO );

		execl( ADDR2LINE, ADDR2LINE, "-C", "-f", "-e", bindir "fsc2", NULL );
		_exit( EXIT_FAILURE );
	}
	else if ( pid < 0 )               /* fork failed */
	{
		for ( i = 0; i < 4; i++ )
			close( pipe_fd[ i ] );
		for ( i = 0; i < 2; i++ )
			close( answer_fd[ i ] );
		return;
	}

	close( pipe_fd[ DUMP_CHILD_READ  ] );
	close( pipe_fd[ DUMP_CHILD_WRITE ] );

	/* Load content of ebp register into EBP - ebp always points to the stack
	   address before the return address of the current subroutine, and the
	   stack address it points to contains the value the epb register had while
	   running the next higher level subroutine */

	asm( "mov %%ebp, %0" : "=g" ( EBP ) );

	/* Loop over all stackframes, starting with the current one and working
	   our way all up to the top - the topmost stackframe is reached when
	   the content of ebp is zero, but this is already _libc_start_main(),
	   so stop one frame earlier */

	while ( * ( int * ) * EBP != 0 )
	{
		/* Get return address of current subroutine and ask the process
		   running ADDR2LINE to convert it into a function name and the
		   source file and line number. (This fails for programs competely
		   stripped of all debugging information as well as for library
		   functions in which case question marks get returned. But using
		   the address it's still possible to find out via the debugger
		   where the shit hit the fan.) */

		sprintf( buf, "%p\n", ( int * ) * ( EBP + 1 ) );
		write( pipe_fd[ DUMP_PARENT_WRITE ], buf, strlen( buf ) );

		sprintf( buf, "#%-3d %-10p  ", k++, ( int * ) * ( EBP + 1 ) );
		write( answer_fd[ DUMP_ANSWER_WRITE ], buf, strlen( buf ) );

		/* Copy ADDR2LINE's reply to the answer pipe */

		while ( read( pipe_fd[ DUMP_PARENT_READ ], &c, 1 ) == 1 && c != '\n' )
			write( answer_fd[ DUMP_ANSWER_WRITE ], &c, 1 );

		write( answer_fd[ DUMP_ANSWER_WRITE ], "() in ", 6 );

		while ( read( pipe_fd[ DUMP_PARENT_READ ], &c, 1 ) == 1 && c != '\n' )
			write( answer_fd[ DUMP_ANSWER_WRITE ], &c, 1 );
		write( answer_fd[ DUMP_ANSWER_WRITE ], "\n", 1 );

		/* Get value of ebp register for the next higher level stackframe */

		EBP = ( int * ) * EBP;
	}

	kill( pid, SIGTERM );

	close( pipe_fd[ DUMP_PARENT_READ ] );
	close( pipe_fd[ DUMP_PARENT_WRITE ] );
	close( answer_fd[ DUMP_ANSWER_WRITE ] );

	fail_mess_fd = answer_fd[ DUMP_ANSWER_READ ];

#endif  /* ! ADDR2LINE */
}
