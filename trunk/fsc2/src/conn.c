/*
  $Id$
*/


#include "fsc2.h"
#include <sys/socket.h>
#include <sys/un.h>


static void comm_sig_handler( int signo );

static volatile bool is_busy;


pid_t spawn_conn( bool start_state )
{
	int listen_fd, conn_fd;
	struct sockaddr_un cli_addr, serv_addr;
	mode_t old_mask;
	pid_t new_pid;
	socklen_t cli_len;


	is_busy = start_state;

	/* Create UNIX domain socket */

	listen_fd = socket( AF_LOCAL, SOCK_STREAM, 0 );

	unlink( FSC2_SOCKET );
	memset( &serv_addr, 0, sizeof( serv_addr ) );
	serv_addr.sun_family = AF_LOCAL;
	strcpy( serv_addr.sun_path, FSC2_SOCKET );

	old_mask = umask( 0 );

	if ( bind( listen_fd, ( struct sockaddr * ) &serv_addr,
			   sizeof( serv_addr ) ) == -1 )
		return -1;

	if ( listen( listen_fd, SOMAXCONN ) < 0 )
		return -1;

	/* Fork a child to handle the communication */

	if ( ( new_pid = fork( ) ) == 0 )
	{
		signal( BUSY_SIGNAL, comm_sig_handler );
		signal( UNBUSY_SIGNAL, comm_sig_handler );

		while ( 1 )
		{
			cli_len = sizeof( cli_addr );
			if ( ( conn_fd = accept( listen_fd,
									 ( struct sockaddr * ) &cli_addr,
									 &cli_len ) ) < 0 )
				continue;
		}
	}

	return new_pid;
}


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
	}

	signal( signo, comm_sig_handler );
}
