/*
  $Id$
*/


#include "fsc2.h"
#include <sys/socket.h>
#include <sys/un.h>



static void connect_handler( int listen_fd );
static ssize_t read_line( int fd, char *ptr, ssize_t max_len );
static ssize_t do_read( int fd, char *ptr );
static ssize_t writen( int fd, const char *ptr, ssize_t n );
static void comm_sig_handler( int signo );

static volatile bool is_busy;


/*------------------------------------------------------*/
/*------------------------------------------------------*/

pid_t spawn_conn( bool start_state )
{
	int listen_fd;
	struct sockaddr_un serv_addr;
	mode_t old_mask;
	pid_t new_pid;


	is_busy = start_state;

	/* Open pipe for communication with child that's going to be spawned */

	if ( pipe( conn_pd ) < 0 )
		return -1;

	/* Create UNIX domain socket */

	listen_fd = socket( AF_LOCAL, SOCK_STREAM, 0 );

	unlink( FSC2_SOCKET );
	memset( &serv_addr, 0, sizeof( serv_addr ) );
	serv_addr.sun_family = AF_LOCAL;
	strcpy( serv_addr.sun_path, FSC2_SOCKET );

	old_mask = umask( 0 );

	if ( bind( listen_fd, ( struct sockaddr * ) &serv_addr,
			   sizeof( serv_addr ) ) == -1 )
	{
		umask( old_mask );
		return -1;
	}

    umask( old_mask );

	if ( listen( listen_fd, SOMAXCONN ) < 0 )
		return -1;

	/* Fork a child to handle the communication */

	if ( ( new_pid = fork( ) ) == 0 )
		connect_handler( listen_fd );

	/* Wait for child to signal that it got its signal handlers installed */

	if ( new_pid >= 0 )
	{
		close( conn_pd[ WRITE ] );
		while ( ! conn_child_replied )
			pause( );
		conn_child_replied = UNSET;
	}

	return new_pid;
}


/*------------------------------------------------------*/
/*------------------------------------------------------*/

static void connect_handler( int listen_fd )
{
	int conn_fd;
	socklen_t cli_len;
	struct sockaddr_un cli_addr;
	char line[ MAXLINE ];
	ssize_t count;
	int extern_UID;
	ssize_t i;


	signal( BUSY_SIGNAL, comm_sig_handler );
	signal( UNBUSY_SIGNAL, comm_sig_handler );

	close( conn_pd[ READ ] );

	/* Signal parent that child is up and running and can accept signals */

	kill( getppid( ), SIGUSR2 );

	while ( 1 )
	{
		cli_len = sizeof( cli_addr );
		if ( ( conn_fd = accept( listen_fd, ( struct sockaddr * ) &cli_addr,
								 &cli_len ) ) < 0 )
			continue;

		/* Read first line with UID of connecting program */

		if ( ( count = read_line( conn_fd, line, MAXLINE ) ) <= 0 )
		{
			close( conn_fd );
			continue;
		}

		/* String got by client has to consist of digits and end with a
		   newline character and must represent an integer that we now try to
		   figure out */

		if ( line[ count - 1 ] == '\n' )
			line[ count - 1 ] = '\0';
		else
		{
			close( conn_fd );
			continue;
		}

		for ( extern_UID = 0, i = 0; line[ i ] != '\0'; i++ )
			if ( ! isdigit( line[ i ] ) )
			{
				close( conn_fd );
				continue;
			}
			else
			{
				extern_UID = 10 * extern_UID + ( int ) ( line[ i ] - '0' );

				if ( extern_UID < 0 )           /* overflow ? */
				{
					close( conn_fd );
					continue;
				}
			}

		/* If parent is busy return "BUSY\n", if UID that client send us
		   differs from fsc2s effective UID send "FAIL\n" and otherwise
		   send "OK\n". On write failure close the connection */

		if ( is_busy )
			strcpy( line, "BUSY\n" );
		else
			strcpy( line, ( unsigned int ) extern_UID == getuid( ) ?
					"OK\n" : "FAIL\n" );

		if ( writen( conn_fd, line, strlen( line ) )
			 != ( ssize_t ) strlen( line ) ||
			 is_busy || ( unsigned int ) extern_UID != getuid( ) )
		{
			close( conn_fd );
			continue;
		}

		/* Read the method to use and store it in the first byte of `line',
		   close connection if method doesn't start with either 's' (start),
		   't' (test) or 'l' (load). The second character can be 'd' (delete)
		   to tell fsc2 to delete the file after it has been used. If there
		   is no 'd' replace the newline with a space */

		if ( ( count = read_line( conn_fd, line, MAXLINE ) ) <= 0 ||
			 ( line[ 0 ] != 's' && line[ 0 ] != 'l' && line[ 0 ] != 't' ) ||
			 ( line[ 1 ] != '\n' && line[ 1 ] != 'd' ) )
		{
			close( conn_fd );
			continue;
		}

		if ( line[ 1 ] == '\n' )
			line[ 1 ] = ' ';

		/* Return "OK\n" unless parent has become busy in the mean time */

		strcpy( line + 2, is_busy ? "BUSY\n" : "OK\n" );
		if ( writen( conn_fd, line + 2, strlen( line + 2 ) )
			 != ( ssize_t ) strlen( line + 2 ) || is_busy )
		{
			close( conn_fd );
			continue;
		}

		/* Now read the file name */

		if ( ( count = read_line( conn_fd, line + 2, MAXLINE - 2 ) ) <= 0 )
		{
			close( conn_fd );
			continue;
		}

		/* Check that file exists and we have permission to read it,
		   if not close the connection */

		line[ count + 1 ] = '\0';

		if ( access( line + 2, R_OK ) == -1 )
		{
			close( conn_fd );
			continue;
		}

		line[ count + 1 ] = '\n';
		line[ count + 2 ] = '\0';

		/* Now inform parent about it (but check that it hasn't become busy
		   in the mean time) and also send reply to client */

		if ( ! is_busy )
		{
			kill( getppid( ), SIGUSR1 );
			if ( write( conn_pd[ WRITE ], line, strlen( line ) )
				 != ( ssize_t ) strlen( line ) )
				writen( conn_fd, "FAIL\n", 5 );
			else
				writen( conn_fd, "OK\n", 3 );
		}
		else
			writen( conn_fd, "BUSY\n", 5 );

		close( conn_fd );
	}

	_exit( 0 );                          /* we'll never end here */ 
}


/*----------------------------------------------------------------*/
/* Reads a line of text of max_len characters ending in '\n' from */
/* the socket. This is directly copied from W. R. Stevens, UNP1.2 */
/*----------------------------------------------------------------*/

static ssize_t read_line( int fd, char *ptr, ssize_t max_len )
{
    ssize_t n, rc;
    char c;


    for ( n = 1; n < max_len; n++ )
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


/*----------------------------------------------------*/
/* This is directly copied from W. R. Stevens, UNP1.2 */
/*----------------------------------------------------*/

static ssize_t do_read( int fd, char *ptr )
{
    static int read_cnt = 0;
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


/*----------------------------------------------------*/
/* Write a line of a n characters of text to socket   */
/* This is directly copied from W. R. Stevens, UNP1.2 */
/*----------------------------------------------------*/

static ssize_t writen( int fd, const char *ptr, ssize_t n )
{
    size_t nleft;
    ssize_t nwritten;


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


/*-----------------------------------------------------*/
/* Set variable 'is_busy' depenming on type of signal. */
/*-----------------------------------------------------*/

static void comm_sig_handler( int signo )
{
	switch( signo )
	{
		case BUSY_SIGNAL :
			is_busy = SET;
			break;

		case UNBUSY_SIGNAL :
			is_busy = UNSET;
			break;

		default :
			assert( 1 == 0 );
	}
	signal( signo, comm_sig_handler );
	kill( getppid( ), SIGUSR2 );
}
