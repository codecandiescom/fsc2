/*
  $Id$
*/

/* Return codes:

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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#define FSC2_SOCKET  "/tmp/fsc2.uds"
#define MAXLINE      4096


char fname[ ] = P_tmpdir "/fsc2.edl.XXXXXX";
volatile int sig_type;


void make_tmp_file( void );
int open_socket( void );
void start_fsc2( const char *pname );
void sig_handler( int signo );
void contact_fsc2( int sock_fd, const char *pname );
ssize_t writen( int fd, const void *vptr, size_t n );
ssize_t read_line( int fd, void *vptr, size_t max_len );
ssize_t do_read( int fd, char *ptr );


int main( int argc, char *argv[ ] )
{
	int sock_fd;


	argc = argc;


	make_tmp_file( );
	if ( ( sock_fd = open_socket( ) ) == -1 )
		start_fsc2( argv[ 0 ] );
	else
		contact_fsc2( sock_fd, argv[ 0 ] );

	return 0;
}



void make_tmp_file( void )
{
	int tmp;
	ssize_t bytes_read;
	char line[ MAXLINE ];


	/* Try to open a temporary file */

	if ( ( tmp = mkstemp( fname ) ) < 0 )
		exit( -1 );

	fchmod( tmp, S_IRUSR | S_IWUSR );

	/* Now read in from stdin and write into the emp file */

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



int open_socket( void )
{
	int sock_fd;
	struct sockaddr_un serv_addr;


	/* Create a socket */

	if ( ( sock_fd = socket( AF_LOCAL, SOCK_STREAM, 0 ) ) == -1 )
	{
		unlink( fname );
		exit( -1 );
	}

	memset( &serv_addr, 0, sizeof( serv_addr ) );
	serv_addr.sun_family = AF_LOCAL;
	strcpy( serv_addr.sun_path, FSC2_SOCKET );

	/* If connect fails due to connection refused or because there's no socket
	   file (or it it isn't a socket file at all) that means fsc2 isn't
	   running and we've got to start it */

	if ( connect( sock_fd, (struct sockaddr_un * ) &serv_addr,
				  sizeof( serv_addr ) ) == -1 )
	{
		if ( errno == ECONNREFUSED || errno == ENOTSOCK || errno == ENOENT )
		{
			close( sock_fd );
			return -1;
		}

		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	return sock_fd;
}


void start_fsc2( const char *pname )
{
	char *av[ 6 ] = { NULL, NULL, NULL, NULL, NULL, NULL };
	int ac = 0;
	char *prog_name;
	pid_t new_pid;


	if ( ( av[ 0 ] = malloc( ( bindir ? strlen( bindir ) + 1 : 0 ) + 6 ) )
		 == NULL )
	{
		unlink ( fname );
		exit( -1 );
	}

	if ( bindir )
		strcpy( av[ 0 ], bindir );
	if ( bindir != NULL && av[ 0 ][ strlen( av[ 0 ] ) - 1 ] != '/' )
		strcat( av[ 0 ], "/" );
	strcat( av[ ac++ ], "fsc2" );

	av[ ac++ ] = ( char * ) "-d";
	av[ ac++ ] = ( char * ) "-s";

	if ( ( prog_name = strrchr( pname, '/' ) ) != NULL )
		prog_name++;
	else
		prog_name = ( char * ) pname;

	if ( ! strcmp( prog_name, "fsc2_start" ) )
		av[ ac++ ] = ( char * ) "-S ";
	else if ( ! strcmp( prog_name, "fsc2_test" ) )
		av[ ac++ ] = ( char * ) "-T ";
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
		execvp( av[ 0 ], av );

	free( av[ 0 ] );

	if ( new_pid < 0 )          /* fork failed ? */
	{
		unlink( fname );
		exit( -1 ) ;
	}

	sleep( INT_MAX );           /* wait for fsc2 to send signal or fail */

	if ( sig_type == SIGCHLD )
	{
		unlink( fname );
		exit( 4 );
	}

	if ( sig_type != SIGUSR1 )
		exit( -1 );
}


void sig_handler( int signo )
{
	assert( signo == SIGUSR1 || signo == SIGCHLD );
	sig_type = signo;
}


void contact_fsc2( int sock_fd, const char *pname )
{
	char line[ MAXLINE ];
	ssize_t count;
	char *prog_name;


	/* Send UID to fsc2 as credential */

	sprintf( line, "%d\n", getuid( ) );
	if ( writen( sock_fd, line, strlen( line ) )
		 != ( ssize_t ) strlen( line ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	if ( ( count = read_line( sock_fd, line, MAXLINE ) ) <= 0 )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	if ( ! strcmp( line, "FAIL\n" ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( 1 );
	}

	if ( ! strcmp( line, "BUSY\n" ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( 2 );
	}

	if ( strcmp( line, "OK\n" ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	/* Assemble second string to send, the first character is the method ('s'
	   for start, 't' for test or 'l' for load). The method is deduced from
	   the name the program was called with - if a non-standard name was used
	   this is an error. The second character is always 'd', telling fsc2 to
	   delete the temporary files when it's done with it */

	if ( ( prog_name = strrchr( pname, '/' ) ) != NULL )
		prog_name++;
	else
		prog_name = ( char * ) pname;
	strcpy( line, " d\n" );
	if ( ! strcmp( prog_name, "fsc2_start" ) )
		line[ 0 ] = 's';
	if ( ! strcmp( prog_name, "fsc2_test" ) )
		line[ 0 ] = 't';
	if ( ! strcmp( prog_name, "fsc2_load" ) ||
		 ! strcmp( prog_name, "fsc2_connect" ) )
		line[ 0 ] = 'l';

	if ( line[ 0 ] == ' ' )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	/* Now tell fsc2 the method - it can either reply wtih "BUSY\n" or with
       "OK\n" */

	if ( writen( sock_fd, line, strlen( line ) )
		 != ( ssize_t ) strlen( line ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	if ( ( count = read_line( sock_fd, line, MAXLINE ) ) <= 0 )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	if ( ! strcmp( line, "BUSY\n" ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( 2 );
	}

	if ( strcmp( line, "OK\n" ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	/* Finally we tell fsc2 the name of the temporary file */

	strcpy( line, fname );
	strcat( line, "\n" );
	if ( writen( sock_fd, line, strlen( line ) )
		 != ( ssize_t ) strlen( line ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	if ( ( count = read_line( sock_fd, line, MAXLINE ) ) <= 0 )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	if ( ! strcmp( line, "FAIL\n" ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( 3 );
	}

	if ( ! strcmp( line, "BUSY\n" ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( 2 );
	}

	if ( strcmp( line, "OK\n" ) )
	{
		unlink( fname );
		close( sock_fd );
		exit( -1 );
	}

	close( sock_fd );
}


ssize_t read_line( int fd, void *vptr, size_t max_len )
{
	ssize_t n, rc;
	char c, *ptr;

	ptr = vptr;
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


ssize_t writen( int fd, const void *vptr, size_t n )
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;


	ptr = vptr;
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
