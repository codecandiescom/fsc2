/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


  This program expects an EDL program on standard input. It will then read it
  in, write it to a temporary file and try to connect to fsc2 via a socket on
  which fsc2 is supposed to listen. If this succeeds it sends fsc2 the
  invoking users UID, followed by a letter that depends on the name the
  program was invocated with - if it was started as 'fsc2_start' it sends an
  'S', if as 'fsc2_test' it sends 'T' and if invocated as either 'fsc2_load'
  or 'fsc2_connect' it sends an 'L', thus indicating what fsc2 is supposed to
  do, i.e. either start the EDL program immediately, to test it or only to
  load it. It also sends a second letter, 'd' to tell fsc2 to delete the
  temporary file when it's done with it. Finally it sends the file name.
  fsc2 can react in different ways: It can indicate that it is either run by
  a user with a different UID, that it is busy, or that it couldn't read the
  messages, which in turn is returned by this program via its return value
  (see list below).

  If, on the other hand, the program finds that fsc2 isn't running (because
  it can't connect to fsc2) it will try to start fsc2 with the '--delete'
  flag (to make fsc2 delete its input file when done with it) and, depending
  on the name this program was invocated with, with the '-S' or '-T' flag or
  no flag and the name of the temporary file. If fsc2 can't be started
  properly this program will return a return value to indicate it.

  The best way to use this program is to create the EDL program to send to
  fsc2 and send it to this program using a popen() call. The success can be
  obtained by examining the return status of the following call of pclose().

  Return codes:

   0: Everything ok
  -1: Internal error
   1: Different user is running fsc2
   2: fsc2 is busy
   3: Internal error in fsc2
   4: fsc2 could not be started
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "fsc2_config.h"
#include "fsc2_types.h"


/* Stuff needed if we're still running an old libc */

#if defined IS_STILL_LIBC1
typedef unsigned int socklen_t;
#endif


/* Functions and defines we can't do without but which aren't available
   when compiling with -ansi on Linux */

#if defined __STRICT_ANSI__
#define P_tmpdir   "/tmp"
extern int mkstemp( char *template );
extern int fchmod( int fildes, mode_t mode );
extern int snprintf( char *str, size_t size, const  char *format, ... );
#endif

/* FSC2_SOCKET must be identical to the definition in fsc2.h ! */

#define FSC2_SOCKET  "/tmp/fsc2.uds"

#define MAXLINE      4096


void make_tmp_file( char *fname );
int open_fsc2_socket( const char *fname );
void start_fsc2( char *pname, char *fname );
void sig_handler( int signo );
void contact_fsc2( int sock_fd, char *pname, char *fname );
void clean_up( const char *fname, int sock_fd, int ret_val );
ssize_t writen( int fd, const void *vptr, size_t n );
ssize_t read_line( int fd, void *vptr, size_t max_len );
ssize_t do_read( int fd, char *ptr );


static volatile sig_atomic_t sig_type = 0;

#define UNUSED_ARGUMENT( a )   ( void ) ( a )

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

int main( int argc, char *argv[ ] )
{
	char fname[ ] = P_tmpdir "/fsc2.edl.XXXXXX";
	int sock_fd;


	UNUSED_ARGUMENT( argc );

	make_tmp_file( fname );
	if ( ( sock_fd = open_fsc2_socket( fname ) ) == -1 )
		start_fsc2( argv[ 0 ], fname );
	else
		contact_fsc2( sock_fd, argv[ 0 ], fname );

	return 0;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

void make_tmp_file( char *fname )
{
	int tmp;
	ssize_t bytes_read;
	char line[ MAXLINE ];


	/* Try to open a temporary file */

	if ( ( tmp = mkstemp( fname ) ) < 0 )
		exit( -1 );

	fchmod( tmp, S_IRUSR | S_IWUSR );

	/* Now read in from stdin and write into the temporary file */

	while ( ( bytes_read = read( 0, line, MAXLINE ) ) != 0 )
	{
		if ( bytes_read == -1 )
		{
			if ( errno == EINTR )
				continue;
			else
			{
				close( tmp );
				unlink( fname );
				exit( -1 );
			}
		}

		if ( write( tmp, line, ( size_t ) bytes_read ) != bytes_read )
		{
			close( tmp );
			unlink( fname );
			exit( -1 );
		}
	}

	close( tmp );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

int open_fsc2_socket( const char *fname )
{
	int sock_fd;
	struct sockaddr_un serv_addr;


	/* Out of paranoia make sure the name of the socket file isn't
	   larger than the char array we're going to copy it to... */

	if ( strlen( FSC2_SOCKET ) >= sizeof serv_addr.sun_path )
	{
		unlink( fname );
		exit( -1 );
	}

	/* Try to get a socket */

	if ( ( sock_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
	{
		unlink( fname );
		exit( -1 );
	}

	memset( &serv_addr, 0, sizeof( serv_addr ) );
	serv_addr.sun_family = AF_UNIX;
	strcpy( serv_addr.sun_path, FSC2_SOCKET );

	/* If connect fails due to connection refused or because there's no socket
	   file (or it exists but isn't a socket file) it means fsc2 isn't running
	   and we've got to start it */

	if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
				  sizeof( serv_addr ) ) == -1 )
	{
		if ( errno == ECONNREFUSED || errno == ENOENT || errno == ENOTSOCK )
		{
			close( sock_fd );
			return -1;
		}

		/* Bomb out on other errors */

		clean_up( fname, sock_fd, -1 );
	}

	return sock_fd;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

void start_fsc2( char *pname, char *fname )
{
	char *av[ 6 ] = { NULL, NULL, NULL, NULL, NULL, NULL };
	int ac = 0;
	char *prog_name;
	pid_t new_pid;
	char flags[ 4 ][ 9 ] = { "--delete", "-s", "-S", "-T" };


	if ( NULL == ( av[ 0 ] =
				 CHAR_P malloc( ( bindir ? strlen( bindir ) + 1 : 0 ) + 6 ) ) )
	{
		unlink ( fname );
		exit( -1 );
	}

	if ( bindir )
		strcpy( av[ 0 ], bindir );
	if ( bindir != NULL && av[ 0 ][ strlen( av[ 0 ] ) - 1 ] != '/' )
		strcat( av[ 0 ], "/" );
	strcat( av[ ac++ ], "fsc2" );

	av[ ac++ ] = flags[ 0 ];
	av[ ac++ ] = flags[ 1 ];

	if ( ( prog_name = strrchr( pname, '/' ) ) != NULL )
		prog_name++;
	else
		prog_name = pname;

	if ( ! strcmp( prog_name, "fsc2_start" ) )
		av[ ac++ ] = flags[ 2 ];
	else if ( ! strcmp( prog_name, "fsc2_test" ) )
		av[ ac++ ] = flags[ 3 ];
	else if ( strcmp( prog_name, "fsc2_load" ) &&
			  strcmp( prog_name, "fsc2_connect" ) )
	{
		free( av[ 0 ] );
		unlink( fname );
		exit( -1 );
	}

	av[ ac++ ] = fname;

	signal( SIGUSR1, sig_handler );
	signal( SIGCHLD, sig_handler );

	if ( ( new_pid = fork( ) ) == 0 )
	{
		execvp( av[ 0 ], av );
		_exit( -1 );                 /* kill child on failed execvp() */
	}

	free( av[ 0 ] );

	if ( new_pid < 0 )               /* fork failed ? */
	{
		unlink( fname );
		exit( -1 ) ;
	}

	while ( ! sig_type )
	{
		pause( );                    /* wait for fsc2 to send signal or fail */

		if ( sig_type == SIGCHLD )   /* fsc2 failed to start */
		{
			unlink( fname );
			exit( 4 );
		}
	}
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

void sig_handler( int signo )
{
	/* Bomb out on unexpected signals */

	if ( signo != SIGCHLD && signo != SIGUSR1 )
		exit( -1 );

	sig_type = signo;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

void contact_fsc2( int sock_fd, char *pname, char *fname )
{
	char line[ MAXLINE ];
	char *prog_name;


	/* Send UID to fsc2 as credential */

	sprintf( line, "%d\n", getuid( ) );
	if ( writen( sock_fd, line, strlen( line ) )
		 != ( ssize_t ) strlen( line ) )
		clean_up( fname, sock_fd, -1 );

	if ( read_line( sock_fd, line, MAXLINE ) <= 0 )
		clean_up( fname, sock_fd, -1 );

	if ( ! strcmp( line, "FAIL\n" ) )
		clean_up( fname, sock_fd, 1 );

	if ( ! strcmp( line, "BUSY\n" ) )
		clean_up( fname, sock_fd, 2 );

	if ( strcmp( line, "OK\n" ) )                /* unexpected reply */
		clean_up( fname, sock_fd, -1 );

	/* Assemble second string to send, the first character is the method ('S'
	   for start, 'T' for test or 'L' for load). The method is deduced from
	   the name the program was called with - if a non-standard name was used
	   this is an error. The second character is always 'd', telling fsc2 to
	   delete the temporary files when it's done with it */

	if ( ( prog_name = strrchr( pname, '/' ) ) != NULL )
		prog_name++;
	else
		prog_name = pname;
	strcpy( line, " d\n" );

	if ( ! strcmp( prog_name, "fsc2_start" ) )
		line[ 0 ] = 'S';
	if ( ! strcmp( prog_name, "fsc2_test" ) )
		line[ 0 ] = 'T';
	if ( ! strcmp( prog_name, "fsc2_load" ) ||
		 ! strcmp( prog_name, "fsc2_connect" ) )
		line[ 0 ] = 'L';

	if ( line[ 0 ] == ' ' )
		clean_up( fname, sock_fd, -1 );

	/* Now tell fsc2 the method - it can either reply with "BUSY\n" or with
       "OK\n" */

	if ( writen( sock_fd, line, strlen( line ) )
		 != ( ssize_t ) strlen( line ) )                 /* can't write */
		clean_up( fname, sock_fd, -1 );

	if ( read_line( sock_fd, line, MAXLINE ) <= 0 )      /* unexpected reply */
		clean_up( fname, sock_fd, -1 );

	if ( ! strcmp( line, "BUSY\n" ) )
		clean_up( fname, sock_fd, 2 );

	if ( strcmp( line, "OK\n" ) )                        /* unexpected reply */
		clean_up( fname, sock_fd, -1 );

	/* Finally tell fsc2 the name of the temporary file */

	snprintf( line, MAXLINE - 2, "%s\n", fname );
	if ( writen( sock_fd, line, strlen( line ) )
		 != ( ssize_t ) strlen( line ) )                 /* can't write */
		clean_up( fname, sock_fd, -1 );

	if ( read_line( sock_fd, line, MAXLINE - 2 ) <= 0 )  /* unexpected reply */
		clean_up( fname, sock_fd, -1 );

	if ( ! strcmp( line, "FAIL\n" ) )
		clean_up( fname, sock_fd, 3 );

	if ( ! strcmp( line, "BUSY\n" ) )
		clean_up( fname, sock_fd, 2 );

	if ( strcmp( line, "OK\n" ) )                        /* unexpected reply */
		clean_up( fname, sock_fd, -1 );

	close( sock_fd );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

void clean_up( const char *fname, int sock_fd, int ret_val )
{
	unlink( fname );
	close( sock_fd );
	exit( ret_val );
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

ssize_t read_line( int fd, void *vptr, size_t max_len )
{
	ssize_t n, rc;
	char c, *ptr;


	ptr = CHAR_P vptr;
	for ( n = 1; n < ( ssize_t ) max_len; n++ )
	{
		if ( ( rc = do_read( fd, &c ) ) == 1 )
		{
			*ptr++ = c;
			if ( c == '\n' )
				break;
		}
		else if ( rc == 0 )
		{
			if ( n == 1 )
				return 0;
			else
				break;
		}
		else
			return -1;
	}

	*ptr = '\0';
	return n;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

ssize_t do_read( int fd, char *ptr )
{
	static int read_cnt;
	static char *read_ptr;
	static char read_buf[ MAXLINE ];


	if ( read_cnt <= 0 )
	{
	  again:
		if ( ( read_cnt = read( fd, read_buf, sizeof( read_buf ) ) ) < 0 )
		{
			if ( errno == EINTR )
				goto again;
			return -1;
		}
		else if ( read_cnt == 0 )
			return 0;

		read_ptr = read_buf;
	}

	read_cnt--;
	*ptr = *read_ptr++;
	return 1;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

ssize_t writen( int fd, const void *vptr, size_t n )
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;


	ptr = CHAR_P vptr;
	nleft = n;
	while ( nleft > 0 )
	{
		if ( ( nwritten = write( fd, ptr, nleft ) ) <= 0 )
		{
			if ( errno == EINTR )
				nwritten = 0;
			else
				return -1;
		}

		nleft -= nwritten;
		ptr += nwritten;
	}

	return n;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
