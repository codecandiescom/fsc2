/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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
#include "gpib.h"
#include "gpibd.h"
#include <sys/un.h>


int fd = -1;
static volatile sig_atomic_t Gpibd_replied;
static char *err_msg = NULL;


static void gpib_abort( void );
static int connect_to_gpibd( void );
static int start_gpibd( void );
static void gpibd_sig_handler( int signo );


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_init( void )
{
#if defined GPIB_LIBRARY_NONE
    return FAILURE;
#else
	char line[ 20 ];
	ssize_t len;


	if ( fd > 0 )
		return SUCCESS;

	/* Try to connect to the gpibd daemon - if that fails because it's
	   not yet running try to start it and then try another connect */

	if (    ( fd = connect_to_gpibd( ) ) <= 0
		 && (    fd == -1
			|| ( fd = start_gpibd( ) ) <= 0 ) )
	{
		fd = -1;
		return FAILURE;
	}

	/* Send the magic number for gpib_init() and our PID */

	len = sprintf( line, "%d %ld\n", GPIB_INIT, ( long ) getpid( ) );
	if (    write( fd, line, len ) != len
		 || read_line( fd, line, 5 ) < 3
		 || strncmp( line, "OK\n", 3 ) )
	{
		shutdown( fd, SHUT_RDWR );
		close( fd );
		fd = -1;
		return FAILURE;
	}

	return SUCCESS;
#endif
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_shutdown( void )
{
#if defined GPIB_LIBRARY_NONE
    return SUCCESS;
#else
	char line[ 10 ];
	ssize_t len;
	int ret;


	if ( fd <= 0 )
		return FAILURE;

	len = sprintf( line, "%d\n", GPIB_SHUTDOWN );
	ret = write( fd, line, len ) == len;

	shutdown( fd, SHUT_RDWR );
	close( fd );
	fd = -1;

	return ret ? SUCCESS : FAILURE;
#endif
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_init_device( const char * name,
				  int        * dev )
{
	char *line;
	ssize_t len;
	char reply[ 20 ];
	long val;
	char *eptr;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

	/* Send a string with just the magic number for gpib_init_device(),
	   on success the device ID should be returned. */

	line = get_string( "%d %s\n", GPIB_INIT_DEVICE, name );
	len = ( ssize_t ) strlen( line );

	if (    write( fd, line, len ) != len
		 || ( len = read_line( fd, reply, sizeof reply - 1 ) ) < 2
		 || ! isdigit( *reply ) )
		gpib_abort( );

	if ( len == 5 && ! strncmp( reply, "FAIL\n", 5 ) )
		return FAILURE;

	reply[ len ] = '\0';
	val = strtol( reply, &eptr, 10 );
	if (    eptr == reply
		 || *eptr != '\n'
		 || val < 0
		 || val > INT_MAX )
		gpib_abort( );

	*dev = val;
	return SUCCESS;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_timeout( int dev,
			  int timeout )
{
	char line[ 20 ];
	ssize_t len;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

	len = sprintf( line, "%d %d %d\n", GPIB_TIMEOUT, dev, timeout );
	if (    write( fd, line, len ) != len
		 || ( len = read_line( fd, line, sizeof line ) ) < 3 )
		gpib_abort( );

	if ( len == 5 && ! strncmp( line, "FAIL\n", 5 ) )
		return FAILURE;

	if ( len != 3 || strncmp( line, "OK\n", 3 ) )
		gpib_abort( );

	return SUCCESS;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_clear_device( int dev )
{
	char line[ 20 ];
	ssize_t len;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

	len = sprintf( line, "%d %d\n", GPIB_CLEAR_DEVICE, dev );
	if (    write( fd, line, len ) != len
		 || ( len = read_line( fd, line, sizeof line ) ) < 3 )
		gpib_abort( );

	if ( len == 5 && ! strncmp( line, "FAIL\n", 5 ) )
		return FAILURE;

	if ( len != 3 || strncmp( line, "OK\n", 3 ) )
		gpib_abort( );

	return SUCCESS;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_local( int dev )
{
	char line[ 20 ];
	ssize_t len;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

	len = sprintf( line, "%d %d\n", GPIB_LOCAL, dev );
	if (    write( fd, line, len ) != len
		 || ( len = read_line( fd, line, sizeof line ) ) < 3 )
		gpib_abort( );

	if ( len == 5 && ! strncmp( line, "FAIL\n", 5 ) )
		return FAILURE;

	if ( len != 3 || strncmp( line, "OK\n", 3 ) )
		gpib_abort( );

	return SUCCESS;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_local_lockout( int dev )
{
	char line[ 20 ];
	ssize_t len;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

	len = sprintf( line, "%d %d\n", GPIB_LOCAL_LOCKOUT, dev );
	if (    write( fd, line, len ) != len
		 || ( len = read_line( fd, line, sizeof line ) ) < 3 )
		gpib_abort( );

	if ( len == 5 && ! strncmp( line, "FAIL\n", 5 ) )
		return FAILURE;

	if ( len != 3 || strncmp( line, "OK\n", 3 ) )
		gpib_abort( );

	return SUCCESS;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_trigger( int dev )
{
	char line[ 20 ];
	ssize_t len;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

	len = sprintf( line, "%d %d\n", GPIB_TRIGGER, dev );
	if (    write( fd, line, len ) != len
		 || ( len = read_line( fd, line, sizeof line ) ) < 3 )
		gpib_abort( );

	if ( len == 5 && ! strncmp( line, "FAIL\n", 5 ) )
		return FAILURE;

 	if ( len != 3 || strncmp( line, "OK\n", 3 ) )
		gpib_abort( );

	return SUCCESS;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_wait( int dev,
		   int mask,
		   int * status )
{
	char line[ 30 ];
	ssize_t len;
	long val;
	char *eptr;
		

    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

    /* Send the 'magic value' for gpib_wait(), the device ID and the mask.
       The other side should either reply by sending the status of the
       device or with "FAIL\n" */

	len = sprintf( line, "%d %d %d\n", GPIB_WAIT, dev, mask );
	if (    write( fd, line, len ) != len
		 || ( len = read_line( fd, line, sizeof line - 1 ) ) < 2
		 || ! isdigit( *line ) )
		gpib_abort( );

	if ( len == 5 && ! strncmp( line, "FAIL\n", 5 ) )
		return FAILURE;

	line[ len ] = '\0';
	val = strtol( line, &eptr, 10 );
	if (    eptr == line
		 || *eptr != '\n'
		 || val < 0
		 || val > INT_MAX )
		gpib_abort( );

	*status = val;
	return SUCCESS;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_write( int          dev,
			const char * buffer,
            long         length )
{
	char line[ 30 ];
	ssize_t len;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

    /* Send a line with the 'magic number' for gpib_write(), the device ID
       and the number of bytes to write */

	len = sprintf( line, "%d %d %ld\n", GPIB_WRITE, dev, length );
	if ( write( fd, line, len ) != len )
        gpib_abort( );

    /* The expected answer is "OK\n" or "FAIL\n" */

    if ( ( len = read_line( fd, line, 5 ) ) != 3 )
    {
        if ( len == 5 && ! strncmp( line, "FAIL\n", 5 ) )
            return FAILURE;
        gpib_abort( );
    }
    else if ( strncmp( line, "OK\n", 3 ) )
        gpib_abort( );

    /* Now send over all the data to be written to the device and expect
       a reply of "OK\n" or "FAIL\n" */

    if (    write( fd, buffer, length ) != length
		 || ( len = read_line( fd, line, sizeof line ) ) < 3 )
		gpib_abort( );

	if ( len == 5 && ! strncmp( line, "FAIL\n", 5 ) )
		return FAILURE;
 	else if ( len != 3 || strncmp( line, "OK\n", 3 ) )
		gpib_abort( );

	return SUCCESS;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_read( int    dev,
           char * buffer,
           long * length )
{
	char line[ 30 ];
	ssize_t len;
	long val;
	char *eptr;
	long bytes_to_read;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

    /* Send the 'magic value' for gpib_read(), the device ID and the
       number of bytes we want to read */

	len = sprintf( line, "%d %d %ld\n", GPIB_READ, dev, *length );
	if ( write( fd, line, len ) != len )
        gpib_abort( );

    /* The expected answer is either a line with the number of bytes
       that got read or "FAIL\n" */

    if ( ( len = read_line( fd, line, sizeof line - 1 ) ) < 2 )
		gpib_abort( );

	if ( ! isdigit( *line ) )
	{
		if ( len != 5 || strncmp( line, "FAIL\n", 5 ) )
			gpib_abort( );
		return FAILURE;
	}

	line[ len ] = '\0';
	val = strtol( line, &eptr, 10 );
	if (    eptr == line
		 || *eptr != '\n'
		 || val < 0
		 || val > *length  )
		gpib_abort( );

	bytes_to_read = val;
	*length = 0;

	/* Now should arrive the data send by the device. Keep on reading until
       we got the whole amount of data (or a serious error is detected) */

	while ( bytes_to_read )
	{
		if (    ( val = read( fd, buffer + *length, bytes_to_read ) ) < 0 
			 && errno != EINTR
			 && errno != EAGAIN )
			gpib_abort( );

		if ( val > 0 )
		{
			length += val;
			bytes_to_read -= val;
		}
	}

	return SUCCESS;
}
	

/*------------------------------------------------------*
 *------------------------------------------------------*/

int
gpib_serial_poll( int             dev,
				  unsigned char * stb )
{
	char line[ 20 ];
	ssize_t len;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return FAILURE;

	len = sprintf( line, "%d %d\n", GPIB_SERIAL_POLL, dev );
	if (    write( fd, line, len ) != len
		 || ( len = read_line( fd, line, sizeof line ) ) < 1 )
		gpib_abort( );

	if ( len == 5 && ! strncmp( line, "FAIL\n", 5 ) )
		return FAILURE;

	if ( len != 1 )
		gpib_abort( );

	*stb = *line;
	return SUCCESS;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

const char *
gpib_last_error( void )
{
	char line[ 20 ];
	ssize_t len;
	long val;
	char *eptr;
	static char no_err = '\0';
	long bytes_read = 0;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( fd <= 0 )
		return &no_err;

	if ( err_msg != NULL )
		err_msg = T_free( err_msg );

	len = sprintf( line, "%d\n", GPIB_LAST_ERROR );
	if (    write( fd, line, len ) != len
		 || ( len = read_line( fd, line, sizeof line - 1 ) ) < 2 )
		gpib_abort( );

	line[ len ] = '\0';
	val = strtol( line, &eptr, 10 );
	if (    eptr == line
		 || *eptr != '\n'
		 || val < 0
		 || ( val == LONG_MAX && errno == ERANGE ) )
		gpib_abort( );

	if ( val == 0 )
		return &no_err;

	err_msg = T_malloc( val + 1 );
	while ( val > 0 )
	{
		if ( ( len = read( fd, err_msg + bytes_read, val ) ) < 0 )
		{
			if ( errno != EINTR && errno != EAGAIN )
			{
				err_msg = T_free( err_msg );
				gpib_abort( );
			}
		}

		if ( len <= 0 )
			continue;

		val -= len;
		bytes_read += len;
	}

	err_msg[ bytes_read ] = '\0';
	return err_msg;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static void
gpib_abort( void )
{
	shutdown( fd, SHUT_RDWR );
	close( fd );
	fd = -1;
	THROW( EXCEPTION );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static int
connect_to_gpibd( void )
{
    int sock_fd;
    struct sockaddr_un serv_addr;


    /* Try to get a socket */

    if ( ( sock_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
        return -1;

    memset( &serv_addr, 0, sizeof serv_addr );
    serv_addr.sun_family = AF_UNIX;
    strcpy( serv_addr.sun_path, GPIBD_SOCK_FILE );

    /* If connect fails due to connection refused or because there's no socket
       file (or it exists but isn't a socket file) it means gpibd isn't running
       and we've got to start it */

    if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
                  sizeof serv_addr ) == -1 )
    {
        close( sock_fd );
        if ( errno == ECONNREFUSED || errno == ENOENT )
            return 0;
        return -1;
    }

    return sock_fd;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static int
start_gpibd( void )
{
	const char *a[ 2 ] = { NULL, NULL };
	pid_t pid;
    struct sigaction sact;
    struct sigaction old_sact;


    if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
        a[ 0 ] = srcdir "gpibd";
    else
        a[ 0 ] = bindir "gpibd";

	/* Install a signal handler */

	Gpibd_replied = 0;
    sact.sa_handler = gpibd_sig_handler;
    sigemptyset( &sact.sa_mask );
    sact.sa_flags = 0;
    if ( sigaction( SIGUSR2, &sact, &old_sact ) == -1 )
        return -1;

	raise_permissions( );

	if ( ( pid = fork( ) ) < 0 )
	{
		lower_permissions( );
		return -1;
	}

	if ( pid == 0 )
	{
		close( STDIN_FILENO );
		close( STDOUT_FILENO );
		close( STDERR_FILENO );

		execv( a[ 0 ], ( char ** ) a );
		_exit( EXIT_FAILURE );
	}

	lower_permissions( );

	/* Wait for the child to send a signal that tells us that it's prepared
	   to accept connections */

    while ( ! Gpibd_replied && waitpid( pid, NULL, WNOHANG ) == 0 )
        fsc2_usleep( 20000, SET );

	sigaction( SIGUSR2, &old_sact, NULL );

	if ( ! Gpibd_replied )
		return -1;

	return connect_to_gpibd( );
}


/*-----------------------------------------*
 * Signal handler for signal by the daemon
 *-----------------------------------------*/

static void
gpibd_sig_handler( int signo )
{
    if ( signo == SIGUSR2 )
        Gpibd_replied = 1;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
