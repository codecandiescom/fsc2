/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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
#include <dirent.h>
#include <sys/un.h>


/* Stuff needed if we're still running an old libc */

#if defined IS_STILL_LIBC1
typedef unsigned int socklen_t;
#endif


/* Try to figure out the maximum length of a user name */

#if defined _SC_LOGIN_NAME_MAX 
#define MAX_LOGIN_NAME    _SC_LOGIN_NAME_MAX
#else
#define MAX_LOGIN_NAME    64
#endif


/* Number of seconds after which the "daemon" checks if some or all
   instances of fsc2 have exited */

#define FSC2D_CHECK_TIME  5


typedef struct {
	unsigned long pid;
	char user_name[ MAX_LOGIN_NAME + 1 ];
	bool exclusive;
	bool with_conn;
} FSC2_INSTANCE;


static volatile sig_atomic_t fsc2d_replied;
static int cwd_ok;

static int connect_to_fsc2d( void );
static int start_fsc2d( bool exclusive, FILE *in_file_fp );
static void fsc2d_sig_handler( int signo );
static void fsc2d( int fd, bool exclusive, struct passwd *ui );
static int check_instances( FSC2_INSTANCE *instances, int num_instances,
							int is_new_connect );
static int new_client( int fd, FSC2_INSTANCE *instances, int num_instances );
static void set_fs2d_signals( void );


/*---------------------------------------------------------------------*/
/* Function talks to the daemon process to figure out if the current   */
/* instance of fsc2 is allowed to run. If such a daemon does not exist */
/* yet it gets created. The function returns -1 when the current       */
/* instance is not allowed to run, 0 when it's allowed to run but may  */
/* not start a child listening for external connections and 1 when it  */
/* may run and must start a child process for external connections.    */
/*---------------------------------------------------------------------*/

int check_spawn_fsc2d( bool exclusive, FILE *in_file_fp )
{
	int sock_fd;
	char line[ MAX_LINE_LENGTH ];
	ssize_t len;


	/* If there's no fsc2 "daemon" process running yet start one. */

	if ( ( sock_fd = connect_to_fsc2d( ) ) == 0 )
		return start_fsc2d( exclusive, in_file_fp );

	if ( sock_fd == -1 )
		return -1;

	/* Send the PID, the daemon must reply with "OK\n" */

	snprintf( line, sizeof( line ), "%ld\n", ( long ) getpid( ) );
	if ( writen( sock_fd, line, strlen( line ) ) !=
										 	( ssize_t ) strlen( line ) ||
		 read_line( sock_fd, line, sizeof( line ) ) != 3 ||
		 strncmp( line, "OK\n", 3 ) )
	{
		fprintf( stderr, "Can't start fsc2 for unknown reasons.\n" );
		close( sock_fd );
		return -1;
	}

	/* Send the user name, the daemon must reply with "OK\n" */

	snprintf( line, sizeof( line ), "%s\n", getpwuid( getuid( ) )->pw_name );
	if ( writen( sock_fd, line, strlen( line ) ) !=
											( ssize_t ) strlen( line ) ||
		 read_line( sock_fd, line, sizeof( line ) ) != 3 ||
		 strncmp( line, "OK\n", 3 ) )
	{
		fprintf( stderr, "Can't start fsc2 for unknown reasons.\n" );
		close( sock_fd );
		return -1;
	}

	/* Send "1" if the current instance was started in exclusive mode,
	   otherwise send "0" */

	snprintf( line, sizeof( line ), "%d\n", exclusive ? 1 : 0 );
	if ( writen( sock_fd, line, strlen( line ) ) !=
											( ssize_t ) strlen( line ) ||
		 ( len = read_line( sock_fd, line, sizeof( line ) ) ) < 5 )
	{
		fprintf( stderr, "Can't start fsc2 for unknown reasons.\n" );
		close( sock_fd );
		return -1;
	}

	close( sock_fd );

	/* If the "daemon" sends a string starting with "EXCL" another instance
	   of fsc2 is already running that keeps us from running */

	line[ len ] = '\0';
	if ( ! strncmp( line, "EXCL ", 5 ) )
	{
		if ( exclusive )
			fprintf( stderr, "Other instances of fsc2 are already "
					 "running" );
		else
			fprintf( stderr, "Another instance of fsc2 is already "
					 "running\nin exclusive mode" );

		if ( len < 6 )
		{
			fprintf( stderr, ".\n" );
			return -1;
		}

		if ( exclusive )
			fprintf( stderr, ",\nsee PID and user: %s", line + 5 );
		else
			fprintf( stderr, ", its PID and user are: %s", line + 5 );

		return -1;
	}

	if ( strncmp( line, "OK N\n", 5 ) && strncmp( line, "OK C\n", 5 ) &&
		 strncmp( line, "OK M\n", 5 ) )
	{
		fprintf( stderr, "Can't start fsc2 for unknown reasons.\n" );
		return -1;
	}

	/* If the maximum number of instances of fsc2 is already reached the
	   "daemon" replies with "OK M\n". */

	if ( line[ 3 ] == 'M' )
	{
		fprintf( stderr, "Maximum number of %d of fsc2 instances already "
				 "running.\n", FSC2_MAX_INSTANCES );
		return -1;
	}

	return line[ 3 ] == 'C' ? 1 : 0;
}


/*-----------------------------------------------------------------*/
/* Try to get a connection to the socket the "daemon" is listening */
/* on. If this fails we must conclude that there's no "daemon" yet */
/* and we have to create it.                                       */
/*-----------------------------------------------------------------*/

static int connect_to_fsc2d( void )
{
	int sock_fd;
	struct sockaddr_un serv_addr;


	/* Try to get a socket */

	if ( ( sock_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
		return -1;

	memset( &serv_addr, 0, sizeof( serv_addr ) );
	serv_addr.sun_family = AF_UNIX;
	strcpy( serv_addr.sun_path, FSC2D_SOCKET );

	/* If connect fails due to connection refused or because there's no socket
	   file (or it exists but isn't a socket file) it means fsc2d isn't running
	   and we've got to start it */

	if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
				  sizeof( serv_addr ) ) == -1 )
	{
		close( sock_fd );
		if ( errno == ECONNREFUSED || errno == ENOENT )
			return 0;
		return -1;
	}

	return sock_fd;
}


/*----------------------------------------------------------*/
/* Try to create the "daemon" process that new instances of */
/* fsc2 must get permission from before they may start.     */
/*----------------------------------------------------------*/

static int start_fsc2d( bool exclusive, FILE *in_file_fp )
{
	pid_t pid;
	int listen_fd;
	struct sockaddr_un serv_addr;
	mode_t old_mask;
	struct sigaction sact;
	struct passwd *ui = getpwuid( getuid( ) );


	raise_permissions( );

	/* Create UNIX domain socket */

	if ( ( listen_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == 1 )
		return -1;

	unlink( FSC2_SOCKET );
	unlink( FSC2D_SOCKET );
	memset( &serv_addr, 0, sizeof( serv_addr ) );
	serv_addr.sun_family = AF_UNIX;
	strcpy( serv_addr.sun_path, FSC2D_SOCKET );

	old_mask = umask( 0 );

	if ( bind( listen_fd, ( struct sockaddr * ) &serv_addr,
			   sizeof( serv_addr ) ) == -1 )
	{
		umask( old_mask );
		close( listen_fd );
		unlink( FSC2D_SOCKET );
		lower_permissions( );
		return -1;
	}

    umask( old_mask );

	if ( listen( listen_fd, SOMAXCONN ) < 0 )
	{
		close( listen_fd );
		unlink( FSC2D_SOCKET );
		lower_permissions( );
		return -1;
	}

	/* Set handler for a signal from the "daemon" that tells us when the
	   "daemon" has finished its initialization */

	sact.sa_handler = fsc2d_sig_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	if ( sigaction( SIGUSR2, &sact, NULL ) < 0 )
	{
		unlink( FSC2D_SOCKET );
		lower_permissions( );
		return -1;
	}

	/* Fork the fsc2 "daemon" */

	if ( ( pid = fork( ) ) == 0 )
	{
		if ( in_file_fp )
			fclose( in_file_fp );
		close( STDIN_FILENO );
		close( STDOUT_FILENO );
		close( STDERR_FILENO );
		fsc2d( listen_fd, exclusive, ui );
		_exit( 0 );
	}

	close( listen_fd );

	if ( pid < 0 )
	{
		unlink( FSC2D_SOCKET );
		return -1;
	}

	lower_permissions( );

	while ( ! fsc2d_replied )
		fsc2_usleep( 20000, SET );

	return 1;
}


/*--------------------------------------------------------------*/
/* Signal handler for the main program for signal by the daemon */
/*--------------------------------------------------------------*/

static void fsc2d_sig_handler( int signo )
{
	if ( signo == SIGUSR2 )
		fsc2d_replied = 1;
}


/*-------------------------------------------------------------------------*/
/* The following is the code run by the "daemon". It waits for incoming    */
/* connections by new instances of fsc2 and tells them if they are allowed */
/* to run and if they may start a child process for external connections.  */
/* The "daemon" also reguarly checks for instances to have exited and, if  */
/* the last instance has died deletes possibly left-over shared memory     */
/* segments and also quits.                                                */
/*-------------------------------------------------------------------------*/

static void fsc2d( int fd, bool exclusive, struct passwd *ui )
{
	FSC2_INSTANCE instances[ FSC2_MAX_INSTANCES + 1 ];
	fd_set fds;
	int num_instances = 1;
	struct timeval timeout;
	int sr;


	/* Ignore all signals */

	set_fs2d_signals( );

	/* Set up the first entry in the list of instances */

	fprintf( stderr, "Starting setup\n" );

	instances[ 0 ].pid = ( unsigned long ) getppid( );
	strncpy( instances[ 0 ].user_name, ui->pw_name, MAX_LOGIN_NAME );
	instances[ 0 ].user_name[ MAX_LOGIN_NAME ] = '\0';
	instances[ 0 ].exclusive = exclusive;
	instances[ 0 ].with_conn = SET;

	/* We're running with the default temporary directory as our current
	   working directory */

	cwd_ok = chdir( P_tmpdir );

	/* We're done with initialization, send signal to parent */

	kill( getppid( ), SIGUSR2 );

	/* Now listen on the socket for new instances to connect. Repeatedly
	   (every FSC2D_CHECK_TIME seconds) check which of the previously
	   registered instances are still alive and remove the dead ones
	   from our list). */

	while ( 1 )
	{
		FD_ZERO( &fds );
		FD_SET( fd, &fds );
		timeout.tv_sec = FSC2D_CHECK_TIME;
		timeout.tv_usec = 0;

		sr = select( fd + 1, &fds, NULL, NULL, &timeout );

		num_instances = check_instances( instances, num_instances, sr );

		if ( sr == 1 )
			num_instances = new_client( fd, instances, num_instances );
	}
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static int check_instances( FSC2_INSTANCE *instances, int num_instances,
							int is_new_connect )
{
	FSC2_INSTANCE *ip = instances;
	int i = 0;
	DIR *dir;
	struct dirent *ent;


	/* Check if the instances of fsc2 are still alive */

	while ( i < num_instances )
	{
		if ( kill( ( pid_t ) ip->pid, 0 ) == -1 )
		{
			if ( i < num_instances - 1 )
				memcpy( ip, ip + 1, ( num_instances - i ) * sizeof *ip );
			num_instances--;
			continue;
		}

		ip++;
		i++;
	}

	/* If there are no instances left remove shared memory segments they
	   might have leaked as well as the socket file for external connections
	   to fsc2 */

	if ( num_instances == 0 )
	{
		delete_stale_shms( );
		unlink( FSC2_SOCKET );
	}

	/* We're done when the last instance of fsc2 is dead and there's not
	   already a new instance trying to talk to us. In this case the socket
	   file is deleted as well as left-over temporary EDL scripts */

	if ( num_instances == 0 && ! is_new_connect )
	{
		unlink( FSC2D_SOCKET );

		if ( cwd_ok == 0 && ( dir = opendir( "." ) ) != NULL )
		{
			while ( ( ent = readdir( dir ) ) != NULL )
				if ( strlen( ent->d_name ) == 15 &&
					 ! strncmp( ent->d_name, "fsc2.edl.", 9 ) )
					unlink( ent->d_name );

			closedir( dir );
		}

		exit( EXIT_SUCCESS );
	}

	return num_instances;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static int new_client( int fd, FSC2_INSTANCE *instances, int num_instances )
{
	socklen_t cli_len;
	struct sockaddr_un cli_addr;
	int cli_fd;
	char line[ MAX_LINE_LENGTH ];
	FSC2_INSTANCE *cur_inst = instances + num_instances;
	FSC2_INSTANCE *ip;
	ssize_t len;
	int i;


	cli_len = sizeof( cli_addr );
	if ( ( cli_fd = accept( fd, ( struct sockaddr * ) &cli_addr,
							&cli_len ) ) < 0 )
		return num_instances;

	/* Read first line with UID of connecting program */

	if ( ( len = read_line( cli_fd, line, MAX_LINE_LENGTH ) ) <= 0 )
	{
		close( cli_fd );
		return num_instances;
	}

	/* The string we just got from the client has to consist of digits and end
	   with a newline character and must represent an integer that we now try
	   to figure out */

	if ( line[ len - 1 ] == '\n' )
		line[ len - 1 ] = '\0';
	else
	{
		writen( cli_fd, "FAIL\n", 5 );
		close( cli_fd );
		return num_instances;
	}

	/* Get the pid from what the client send us */

	cur_inst->pid = strtoul( line, NULL, 10 );
	if ( cur_inst->pid == ULONG_MAX && errno == ERANGE )
	{
		writen( cli_fd, "FAIL\n", 5 );
		close( cli_fd );
		return num_instances;
	}
	else if ( writen( cli_fd, "OK\n", 3 ) != 3 )
	{
		close( cli_fd );
		return num_instances;
	}

	/* Now read the user name */

	if ( ( len = read_line( cli_fd, line, MAX_LINE_LENGTH ) ) <= 0 )
	{
		close( cli_fd );
		return num_instances;
	}

	if ( line[ len - 1 ] == '\n' )
		line[ len - 1 ] = '\0';
	else
	{
		writen( cli_fd, "FAIL\n", 5 );
		close( cli_fd );
		return num_instances;
	}

	strncpy( cur_inst->user_name, line, MAX_LOGIN_NAME );
	cur_inst->user_name[ MAX_LOGIN_NAME ] = '\0';

	if ( writen( cli_fd, "OK\n", 3 ) != 3 )
	{
		close( cli_fd );
		return num_instances;
	}

	/* Finally read if the new child wants to run in exclusive mode */

	if ( ( len = read_line( cli_fd, line, MAX_LINE_LENGTH ) ) <= 0 )
	{
		close( cli_fd );
		return num_instances;
	}
	
	if ( line[ 1 ] != '\n' || ( *line != '0' && *line != '1' ) )
	{
		writen( cli_fd, "FAIL\n", 5 );
		close( cli_fd );
		return num_instances;
	}

	/* If we have already as many instances as can be run send tell the
	   client now */

	if ( num_instances == FSC2_MAX_INSTANCES )
	{
		writen( cli_fd, "OK M\n", 5 );
		close( cli_fd );
		return num_instances;
	}

	cur_inst->exclusive = *line == '0' ? 0 : 1;

	if ( cur_inst->exclusive && num_instances > 0 )
	{
		sprintf( line, "EXCL %lu (%s)", instances->pid,
				 instances->user_name );
		for ( ip = instances + 1, i = 1; i < num_instances; i++, ip++ )
			sprintf( line + strlen( line ), ", %lu (%s)", ip->pid,
					 ip->user_name );
		strcat( line, "\n" );
		writen( cli_fd, line, strlen( line ) );
		close( cli_fd );
		return num_instances;
	}

	for ( ip = instances; ip != cur_inst; ip++ )
		if ( ip->with_conn )
			break;

	if ( ip == cur_inst )
	{
		ip->with_conn = SET;
		writen( cli_fd, "OK C\n", 5 );
	}
	else
		writen( cli_fd, "OK N\n", 5 );

	close( cli_fd );
	return num_instances + 1;
}


/*------------------------------------------------------------------*/
/* Signal handler for the "demon" process: all signals are ignored. */
/*------------------------------------------------------------------*/

static void set_fs2d_signals( void )
{
	struct sigaction sact;
	int sig_list[ ] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE,
						SIGSEGV, SIGPIPE, SIGALRM, SIGTERM, SIGUSR1, SIGUSR2,
						SIGCHLD, SIGCONT, SIGTTIN, SIGTTOU, SIGBUS,
						SIGVTALRM, 0 };
	int i;


	for ( i = 0; sig_list[ i ] != 0; i++ )
	{
		sact.sa_handler = SIG_IGN;
		sigemptyset( &sact.sa_mask );
		sact.sa_flags = 0;
		if ( sigaction( sig_list[ i ], &sact, NULL ) < 0 )
			_exit( -1 );
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
