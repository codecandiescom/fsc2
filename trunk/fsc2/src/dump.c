/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2004 Jens Thoms Toerring
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

int Fail_Mess_Fd = -1;

#if ! defined( NDEBUG ) && defined( ADDR2LINE ) && ! defined __STRICT_ANSI__
static void write_dump( int *pipe_fd, int *answer_fd, int k, void * addr );

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

#endif


/*-----------------------------------------------------------------------*/
/* This function is hardware depended, i.e. it will only work on i386    */
/* type processors, so it returns immediately without doing anything if  */
/* the machine is not a i386.                                            */
/* This function is called from the signal handler for 'deadly' signals, */
/* e.g. SIGSEGV etc. It tries to figure out where this signal happend,   */
/* creates a backtrace by running through the stackframes and determines */
/* from the return addresses and with the help of the GNU utility        */
/* 'addr2line' the function name and the source file and line number     */
/* (asuming the executable was compiled with the -g flag and wasn't      */
/* stripped). The result is written to the write end of a pipe that is   */
/* (mis)used as a temporary buffer, from which the results will be read  */
/* later (the read end is a global variable named 'Fail_Mess_Fd').       */
/* If the macro ADDR2LINE isn't defined the function will do nothing.    */
/* If it is defined it must be the complete path to 'addr2line'. The     */
/* best way to set it correctly is probably to have lines like           */
/* ADDR2LINE = $(shell which addr2line)                                  */
/* ifneq ($(word 1,$(ADDR2LINE)),which:)                                 */
/*     CFLAGS += -DADDR2LINE=\"$(ADDR2LINE)\"                            */
/* endif                                                                 */
/* in the Makefile.                                                      */
/*-----------------------------------------------------------------------*/

#if ! defined( NDEBUG ) && defined( ADDR2LINE ) && ! defined __STRICT_ANSI__
void DumpStack( void *crash_address )
{
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
	int answer_fd[ 2 ];
	int pipe_fd[ 4 ];
	pid_t pid;
	int i, k = 0;
	char buf[ 128 ];
	struct sigaction sact;
	Device_T *cd;
	static bool already_crashed = UNSET;


	/* Sometimes the program crashes again while trying to send out the
	   mail (remember, we're already deep in undefied behavior land when
	   we get here at all, so all bets are off). To avoid getting into an
	   infinite loop of crashes we bail out immediately when 'alread_crashed'
	   is set. */

	if ( already_crashed )
		return;
	else
		already_crashed = SET;

	/* Don't do anything on a machine with a non-Intel processor */

	if ( ! Fsc2_Internals.is_linux_i386 )
		return;

	/* Childs death signal isn't needed anymore */

	sact.sa_handler = ( void ( * )( int ) ) SIG_DFL;
	sact.sa_flags = 0;
	sigaction( SIGCHLD, &sact, NULL );

	/* Don't crash on SIGPIPE if child process fails to exec (in this case
	   the output will only contain the addresses) */

	sact.sa_handler = ( void ( * )( int ) ) SIG_IGN;
	sact.sa_flags = 0;
	sigaction( SIGPIPE, &sact, NULL );

	/* Set up the pipes, two for communication with child process and one
	   misused as a temporary buffer for the result */

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

		dup2( pipe_fd[ DUMP_CHILD_WRITE ], STDOUT_FILENO );
		dup2( pipe_fd[ DUMP_CHILD_WRITE ], STDERR_FILENO );
		dup2( pipe_fd[ DUMP_CHILD_READ  ], STDIN_FILENO );

		close( pipe_fd[ DUMP_CHILD_READ ] );
		close( pipe_fd[ DUMP_CHILD_WRITE ] );

		if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
			execl( ADDR2LINE, ADDR2LINE, "-C", "-f", "-e", srcdir "fsc2",
				   NULL );
		else
			execl( ADDR2LINE, ADDR2LINE, "-C", "-f", "-e", bindir "fsc2",
				   NULL );
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

	/* The program counter where the crash happened has been passed by the
	   signal handler from which we are called. We now feed it to ADDR2LINE
	   to get the function name, source file and line number. */

	write_dump( pipe_fd, answer_fd, k++, crash_address );

	/* Load content of ebp register into EBP - ebp always points to the stack
	   address before the return address of the current subroutine, and the
	   stack address it points to contains the value the epb register had while
	   running the next higher level subroutine */

	asm( "mov %%ebp, %0" : "=g" ( EBP ) );

	/* Loop over all stackframes, starting with the second next and working
	   up the way to the top - the topmost stackframe would be reached when
	   the content of ebp is zero, but this is already _libc_start_main(),
	   so stop one frame earlier */

	EBP = ( int * ) * ( int * ) * EBP;
	while ( * ( int * ) * EBP != 0 )
	{
		/* Get return address of current subroutine and ask the process
		   running ADDR2LINE to convert it into function name, source file
		   and line number. (This fails for programs competely stripped of
		   all debugging information.) If the address indicates that the
		   error hapened in a loaded library we try at least to figure out
		   if it comes from a device module and print the offset in the
		   library, which we then can later check with addr2line. */

		sprintf( buf, "%p\n", ( void * ) * ( EBP + 1 ) );

		write_dump( pipe_fd, answer_fd, k++, ( void * ) * ( EBP + 1 ) );

		/* Get value of ebp register for the next higher level stackframe */

		EBP = ( int * ) * EBP;
	}

	kill( pid, SIGTERM );

	close( pipe_fd[ DUMP_PARENT_READ ] );
	close( pipe_fd[ DUMP_PARENT_WRITE ] );

	if ( EDL.Device_List != NULL )
	{
		write( answer_fd[ DUMP_ANSWER_WRITE ], "\nDevice handles:\n", 17 );

		for ( cd = EDL.Device_List; cd != NULL; cd = cd->next )
		{
			if ( ! cd->is_loaded )
				continue;
			sprintf( buf, "%s.so: %p\n",
					 cd->name, ( void * ) * ( int * ) cd->driver.handle );
			write( answer_fd[ DUMP_ANSWER_WRITE ], buf, strlen( buf ) );
		}
	}

	close( answer_fd[ DUMP_ANSWER_WRITE ] );

	Fail_Mess_Fd = answer_fd[ DUMP_ANSWER_READ ];
}
#else
void DumpStack( UNUSED_ARG void *crash_address )
{
}
#endif


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

#if ! defined( NDEBUG ) && defined( ADDR2LINE ) && ! defined __STRICT_ANSI__

static void write_dump( int *pipe_fd, int *answer_fd, int k, void *addr )
{
	char buf[ 128 ];
	char c;
	Device_T *cd1, *cd2;


	/* Since addr2line is only translating addresses from fsc2 itself we
	   also check if the current address is in one of the device modules
	   (assuming that these are located at the highest addresses) and, if
	   the address seems to be from one of them we print out enough
	   infomation to make it possible to figure out the exact location of
	   the error by manually using addr2line on the library. */

	for ( cd1 = EDL.Device_List; cd1 != NULL; cd1 = cd1->next )
		if ( cd1->is_loaded )
			break;

	if ( cd1 != NULL )
	{
		for ( cd2 = cd1->next; cd2 != NULL; cd2 = cd2->next )
		{
			if ( ! cd2->is_loaded )
				continue;

			if ( ( int ) addr >= * ( int * ) cd1->driver.handle &&
				 ( int ) addr < * ( int * ) cd2->driver.handle )
				break;

			cd1 = cd2;
		}

		if ( cd1->is_loaded &&
			 ( int ) addr > * ( int * ) cd1->driver.handle )
		{
			sprintf( buf, "#%-3d %-10p  %p in %s.so\n", k, addr,
					 ( void * ) ( ( int ) addr
								  - * ( int * ) cd1->driver.handle ),
					 cd1->name );
			write( answer_fd[ DUMP_ANSWER_WRITE ], buf, strlen( buf ) );
			return;
		}
	}

	sprintf( buf, "%p\n", addr );
	write( pipe_fd[ DUMP_PARENT_WRITE ], buf, strlen( buf ) );

	sprintf( buf, "#%-3d %-10p  ", k, addr );
	write( answer_fd[ DUMP_ANSWER_WRITE ], buf, strlen( buf ) );

	/* Copy ADDR2LINE's reply to the answer pipe */

	while ( read( pipe_fd[ DUMP_PARENT_READ ], &c, 1 ) == 1 && c != '\n' )
		write( answer_fd[ DUMP_ANSWER_WRITE ], &c, 1 );

	write( answer_fd[ DUMP_ANSWER_WRITE ], "() in ", 6 );

	while ( read( pipe_fd[ DUMP_PARENT_READ ], &c, 1 ) == 1 && c != '\n' )
		write( answer_fd[ DUMP_ANSWER_WRITE ], &c, 1 );
	write( answer_fd[ DUMP_ANSWER_WRITE ], "\n", 1 );
}

#endif  /* ! NDEBUG && ADDR2LINE && !  __STRICT_ANSI__ */



#if 0

/* An alternative method to obtain a backtrace - unfortunately this won't
   show us crashes in modules... */

#include <execinfo.h>

void DumpStack( void )
{
	int size;
	void *buf[ 100 ];
	int p[ 2 ];

	if ( pipe( p ) < 0 )
		return;
	size = backtrace( buf, 100 );
	if ( size != 0 )
	{
		Fail_Mess_Fd = p[ 0 ];
		backtrace_symbols_fd( buf, size, p[ 1 ] );
		close( p[ 1 ] );
	}
	else
	{
		close( p[ 0 ] );
		close( p[ 1 ] );
	}
}

#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
