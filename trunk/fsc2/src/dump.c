/*
  $Id$
*/

#include "fsc2.h"


void DumpStack( void )
{
#ifdef ADDR2LINE

	int *EBP;
	int f[ 2 ];
	int p[ 4 ];
	pid_t pid;
	int i, k = 0;
	struct sigaction sact;
	char buf[ 100 ];
	char c;


	/* Childs death signal isn't needed */

	sact.sa_handler = SIG_DFL;
	sact.sa_flags = 0;
	sigaction( SIGCHLD, &sact, NULL );

	/* Don't crash with SIGPIPE if child process fails to exec ADDR2LINE */

	sact.sa_handler = SIG_IGN;
	sact.sa_flags = 0;
	sigaction( SIGPIPE, &sact, NULL );

	if ( pipe( p ) < 0 )
		return;
	if ( pipe( p + 2 ) < 0 )
	{
		for ( i = 0; i < 2; i++ )
			close( p[ i ] );
		return;
	}

	if ( pipe( f ) < 0 )
	{
		for ( i = 0; i < 4; i++ )
			close( p[ i ] );
		return;
	}

	/* Create child running addr2line with input and output redirected */

	if ( ( pid = fork( ) ) == 0 )
	{
		close( p[ 0 ] );
		close( p[ 3 ] );

		close( STDOUT_FILENO );
		close( STDERR_FILENO );
		close( STDIN_FILENO );

		dup2( p[ 1 ], STDOUT_FILENO );
		dup2( p[ 1 ], STDERR_FILENO );
		dup2( p[ 2 ], STDIN_FILENO );

		execl( ADDR2LINE, ADDR2LINE, "-C", "-f", "-e", bindir "fsc2", NULL );
		_exit( EXIT_FAILURE );
	}
	else if ( pid < 0 )               /* fork failed */
	{
		for ( i = 0; i < 4; i++ )
			close( p[ i ] );
		for ( i = 0; i < 2; i++ )
			close( f[ i ] );
		return;
	}

	close( p[ 1 ] );
	close( p[ 2 ] );

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
		   source file and line number (this fails for programs competely
		   striiped of all debugging information as well as for library
		   functions - in this case question marks get returned, but using
		   the address it's still possible to find out via the debugger
		   where the shit hit the fan). */

		sprintf( buf, "%p\n", ( int * ) * ( EBP + 1 ) );
		write( p[ 3 ], buf, strlen( buf ) );

		sprintf( buf, "#%-3d %-10p ", k++, ( int * ) * ( EBP + 1 ) );
		write( f[ 1 ], buf, strlen( buf ) );

		while ( read( p[ 0 ], &c, 1 ) == 1 && c != '\n' )
			write( f[ 1 ], &c, 1 );

		write( f[ 1 ], "() in ", 6 );

		while ( read( p[ 0 ], &c, 1 ) == 1 )
		{
			write( f[ 1 ], &c, 1 );
			if ( c == '\n' )
				break;
		}

		/* Get content of ebp for the next higher stackframe */

		EBP = ( int * ) * EBP;
	}

	kill( pid, SIGTERM );
	close( p[ 0 ] );
	close( p[ 3 ] );
	close( f[ 1 ] );
	fail_mess_fd = f[ 0 ];

#endif  /* ! ADDR2LINE */
}
