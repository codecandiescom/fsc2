/*
  $Id$
*/

#include "fsc2.h"


/*-----------------------------------------------------------------------*/
/* This function is called from the signal handler for 'deadly' signals, */
/* i.e. SIGSEGV etc. It tries to figure out where this signal happend by */
/* running through the stackframes and determining from the return       */
/* addresses and by the help of the GNU utility 'addr2line' the function */
/* name and the source file and line number. The result is written to    */
/* write end of a pipe that, under the name of 'fail_mess_fd', can be    */
/* read and, for example, written into a mail.                           */
/* This function is highly hardware depended, i.e. it will probably only */
/* work with i386 type processors.                                       */
/* If the macro ADDR2LINE isn't defined the function will do nothing. If */
/* it is defined it must be the complete path to the GNU utility         */
/* "addr2line". The best way to set it correctly is probably to have     */
/* lines like                                                            */
/* ADDR2LINE = $(shell which addr2line)                                  */
/* ifneq ($(word 1,$(ADDR2LINE)),which:)                                 */
/*     CFLAGS += -DADDR2LINE=\"$(ADDR2LINE)\"                            */
/* endif                                                                 */
/* in the Makefile.                                                      */
/*-----------------------------------------------------------------------*/

void DumpStack( void )
{
#ifdef ADDR2LINE

	int *EBP;          /* assumes that sizeof( int ) equals size of pointers */
	int answer_fd[ 2 ];
	int pipe_fd[ 4 ];
	pid_t pid;
	int i, k = 1;
	struct sigaction sact;
	char buf[ 32 ];
	char c;


	/* Childs death signal isn't needed */

	sact.sa_handler = SIG_DFL;
	sact.sa_flags = 0;
	sigaction( SIGCHLD, &sact, NULL );

	/* Don't crash with SIGPIPE if child process fails to exec (in this case
	   the output will only contain the addresses) */

	sact.sa_handler = SIG_IGN;
	sact.sa_flags = 0;
	sigaction( SIGPIPE, &sact, NULL );

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
		close( pipe_fd[ 0 ] );
		close( pipe_fd[ 3 ] );

		close( STDOUT_FILENO );
		close( STDERR_FILENO );
		close( STDIN_FILENO );

		dup2( pipe_fd[ 1 ], STDOUT_FILENO );
		dup2( pipe_fd[ 1 ], STDERR_FILENO );
		dup2( pipe_fd[ 2 ], STDIN_FILENO );

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

	close( pipe_fd[ 1 ] );
	close( pipe_fd[ 2 ] );

	/* Load content of ebp register into EBP - ebp always points to the stack
	   address before the return address of the current subroutine, and the
	   stack address it points to contains the value the epb register had while
	   running the next higher level subroutine */

	asm( "mov %%ebp, %0" : "=g" ( EBP ) );

	/* Loop upwards over all stackframes - the topmost stackframe is reached
	   when the content of ebp is zero, but this is already _libc_start_main,
	   so stop one frame earlier */

	while ( * ( int * ) * EBP != 0 )
	{
		/* Get return address of current subroutine and ask the process
		   running ADDR2LINE to convert this into a function name and the
		   source file and line number. (This fails for programs competely
		   stripped of all debugging information as well as for library
		   functions - in this case question marks get returned, but using
		   the address it's still possible to find out via the debugger
		   where the shit hit the fan.) */

		sprintf( buf, "%p\n", ( int * ) * ( EBP + 1 ) );
		write( pipe_fd[ 3 ], buf, strlen( buf ) );

		sprintf( buf, "#%-3d %-10p  ", k++, ( int * ) * ( EBP + 1 ) );
		write( answer_fd[ 1 ], buf, strlen( buf ) );

		/* Copy answer to answer pipe */

		while ( read( pipe_fd[ 0 ], &c, 1 ) == 1 && c != '\n' )
			write( answer_fd[ 1 ], &c, 1 );

		write( answer_fd[ 1 ], "() in ", 6 );

		while ( read( pipe_fd[ 0 ], &c, 1 ) == 1 && c != '\n' )
			write( answer_fd[ 1 ], &c, 1 );
		write( answer_fd[ 1 ], &c, 1 );

		/* Get value of ebp register of the next higher level stackframe */

		EBP = ( int * ) * EBP;
	}

	kill( pid, SIGTERM );
	close( pipe_fd[ 0 ] );
	close( pipe_fd[ 3 ] );
	close( answer_fd[ 1 ] );
	fail_mess_fd = answer_fd[ 0 ];

#endif  /* ! ADDR2LINE */
}
