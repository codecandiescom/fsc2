/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2.h"
#include "gpib.h"
#include "gpibd.h"
#include <sys/un.h>


int GPIB_fd = -1;
static char err_msg[ GPIB_ERROR_BUFFER_LENGTH + 1 ];


static int simple_gpib_call( int dev,
                             int func_id );
#ifndef GPIB_LIBRARY_NONE
static int connect_to_gpibd( void );
static int start_gpibd( void );
#endif
static ssize_t swrite( int          fd,
                       const char * buf,
                       ssize_t      len );
static ssize_t sread( int       fd,
                      char    * buf,
                      ssize_t   len );
static ssize_t readline( int       fd,
                         char    * buf,
                         ssize_t   max_len );
static int extract_long( char * line,
                         char   ec,
                         long * val );
static int extract_int( char * line,
                        char   ec,
                        int  * val );
static void block_signals( sigset_t * old_mask );


/*--------------------------------------------------*
 * If not yet running start the "GPIB daemon", then
 * connect to it and inform it about our PID.
 *--------------------------------------------------*/

int
gpib_init( void )
{
#if defined GPIB_LIBRARY_NONE
    strcpy( err_msg, "fsc2 wasn't compiled with GPIB support" );
    return FAILURE;
#else
	char line[ 20 ];
	ssize_t len;
    sigset_t old_mask;


    /* Make sure only the parent can initialize the GPIB */

    fsc2_assert( Fsc2_Internals.I_am == PARENT );

    /* If the file descriptor already has a non-negative value the GPIB
       is already up */

	if ( GPIB_fd >= 0 )
		return SUCCESS;

	/* Try to connect to the gpibd daemon - if that fails because it's
	   not yet running (when the call returned -1) try to start it and
       then again try to connect */

	if (    ( GPIB_fd = connect_to_gpibd( ) ) < 0
		 && (    GPIB_fd == -2
              || ( GPIB_fd = start_gpibd( ) ) < 0 ) )
	{
		GPIB_fd = -1;
        strcpy( err_msg, "Failed to connect to GPIB daemon, see '" );
        strcat( err_msg, GPIB_LOG_FILE );
        strcat( err_msg, "' for possible reasons." );
		return FAILURE;
	}

    block_signals( &old_mask );

	/* Send the magic number for gpib_init() and our PID. The expected
       reply is either a NAK or ACK character */

	len = sprintf( line, "%d %ld\n", GPIB_INIT, ( long ) getpid( ) );
	if (    swrite( GPIB_fd, line, len ) != len
		 || sread( GPIB_fd, line, 1 ) != 1
		 || *line != ACK )
	{
		shutdown( GPIB_fd, SHUT_RDWR );
		close( GPIB_fd );
		GPIB_fd = -1;

        strcpy( err_msg, "GPIB daemon doesn't react as required" );
        sigprocmask( SIG_SETMASK, &old_mask, NULL );	

        return FAILURE;
	}

    sigprocmask( SIG_SETMASK, &old_mask, NULL );	

	return SUCCESS;
#endif
}


/*---------------------------------------------------------------* 
 * Shutdown the connection to the daemon, no reply is expected.
 * If there are no other instances of fsc2 running an experiment
 * using the GPIB this will result in it getting shutdown an
 * the daemon exiting.
 *--------------------------------------------------------------*/

int
gpib_shutdown( void )
{
	char line[ 10 ];
	ssize_t len;
    sigset_t old_mask;


    /* Make sure only the parent can shutdown the GPIB */

    fsc2_assert( Fsc2_Internals.I_am == PARENT );

    /* If the file descriptor doesn't have a reasonable value GPIB
       must already be shutdown */

    if ( GPIB_fd < 0 )
        return SUCCESS;

    block_signals( &old_mask );

	len = sprintf( line, "%d\n", GPIB_SHUTDOWN );
	swrite( GPIB_fd, line, len );

	shutdown( GPIB_fd, SHUT_RDWR );
	close( GPIB_fd );
	GPIB_fd = -1;

    strcpy( err_msg, "Connection to GPIB daemon is closed" );
    sigprocmask( SIG_SETMASK, &old_mask, NULL );

	return SUCCESS;
}


/*----------------------------------------------------------*
 * Function to initialize a device on the GPIB bus. Expects
 * the symbolic name of the device (as given in gpib.conf)
 * and a pointer to an int for the device handle to be used
 * in all following calls concerning this device.
 *----------------------------------------------------------*/

int
gpib_init_device( const char * name,
				  int        * dev )
{
	char *line;
	ssize_t len;
	char reply[ 20 ];
    sigset_t old_mask;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( GPIB_fd < 0 )
        return FAILURE;

	/* Send a line with the 'magic number' for gpib_init_device(), followed
       by the symbolic name of the device (as given in /etc/gpib.conf). On
       success the device ID should be returned, otherwise a single NAK
       character. */

	line = get_string( "%d %s\n", GPIB_INIT_DEVICE, name );
	len = strlen( line );

    block_signals( &old_mask );

	if (    swrite( GPIB_fd, line, len ) != len
         || sread( GPIB_fd, reply, 1 ) != 1 
         || *reply == NAK
         || ( len = readline( GPIB_fd, reply + 1, sizeof reply - 2 ) ) < 1 )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        return FAILURE;
    }

    len++;

    sigprocmask( SIG_SETMASK, &old_mask, NULL );

	reply[ len ] = '\0';
    if ( extract_int( reply, '\n', dev ) || *dev < 0 )
        return FAILURE;

	return SUCCESS;
}


/*---------------------------------------------.----------*
 * Sets a new timeout for the device. Expects the device
 * number and another number indicating the length of the
 * timeout (see gpib.h for possible values).
 *--------------------------------------------------------*/

int
gpib_timeout( int dev,
			  int timeout )
{
	char line[ 20 ];
	ssize_t len;
    sigset_t old_mask;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( GPIB_fd < 0 )
        return FAILURE;

    block_signals( &old_mask );

    /* Send line with the 'magic value' for gpib_timeout(), followed by the
       device number and the timeout value. The expected reply is either a
       ACK or NAK character */

	len = sprintf( line, "%d %d %d\n", GPIB_TIMEOUT, dev, timeout );
	if (    swrite( GPIB_fd, line, len ) != len
		 || sread( GPIB_fd, line, 1 ) != 1
         || *line != ACK )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
		return FAILURE;
    }

    sigprocmask( SIG_SETMASK, &old_mask, NULL );

	return SUCCESS;
}


/*-----------------------------------------------------*
 * Function to send a "clear device" GPIB command to a
 * device, Expects just the device number as the only
 * argument.
 *-----------------------------------------------------*/

int
gpib_clear_device( int dev )
{
    return simple_gpib_call( dev, GPIB_CLEAR_DEVICE );
}


/*------------------------------------------------------*
 * Function to bring a device into local mode. Expects
 * just the device number as an argument.
 *------------------------------------------------------*/

int
gpib_local( int dev )
{
    return simple_gpib_call( dev, GPIB_LOCAL );
}


/*-----------------------------------------------------*
 * Funcion to bring a device into local lockout state.
 * Expects just the device number as an argument.
 *-----------------------------------------------------*/

int
gpib_local_lockout( int dev )
{
    return simple_gpib_call( dev, GPIB_LOCAL_LOCKOUT );
}


/*--------------------------------------------------------*
 * Function to send a "device trigger" GPIB command to a
 * device. Expects just the device number as an argument.
 *--------------------------------------------------------*/

int
gpib_trigger( int dev )
{
    return simple_gpib_call( dev, GPIB_TRIGGER );
}


/*---------------------------------------------------------*
 * Function to wait for a device event. Expects the device
 * number and a mask for the events to be reported as well
 * as a pointer to an int for returning the device status.
 *---------------------------------------------------------*/

int
gpib_wait( int   dev,
		   int   mask,
		   int * status )
{
	char line[ 30 ];
	ssize_t len;
    sigset_t old_mask;
		

    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( GPIB_fd < 0 )
        return FAILURE;

    block_signals( &old_mask );

    /* Send the 'magic value' for gpib_wait(), the device ID and the mask.
       The other side should either reply by sending the status of the
       device (as an integer) or with a single NAK character. */

	len = sprintf( line, "%d %d %d\n", GPIB_WAIT, dev, mask );
	if (    swrite( GPIB_fd, line, len ) != len
		 || ( len = readline( GPIB_fd, line, sizeof line - 1 ) ) < 1
         || ( len == 1 && *line == NAK ) )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
		return FAILURE;
    }

    sigprocmask( SIG_SETMASK, &old_mask, NULL );

	line[ len ] = '\0';
    if ( extract_int( line, '\n', status ) )
		return FAILURE;

	return SUCCESS;
}


/*-----------------------------------------------------------*
 * Function for sending data to a device. Expects the device
 * number, a pointer to the buffer with the data to be sent
 * and the number of bytes in the buffer.
 *-----------------------------------------------------------*/

int
gpib_write( int          dev,
			const char * buffer,
            long         length )
{
	char line[ 30 ];
	ssize_t len;
    sigset_t old_mask;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( GPIB_fd < 0 )
        return FAILURE;

    block_signals( &old_mask );

    /* Send a line with the 'magic number' for gpib_write(), the device ID
       and the number of bytes to be written. The expected answer is a ACK
       or NAK character */

	len = sprintf( line, "%d %d %ld\n", GPIB_WRITE, dev, length );
	if (    swrite( GPIB_fd, line, len ) != len
         || sread( GPIB_fd, line, 1 ) != 1
         || *line != ACK )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        return FAILURE;
    }

    /* Now send over all the data to be written to the device and expect
       a reply of ACK or NAK */

    if (    swrite( GPIB_fd, buffer, length ) != length
		 || sread( GPIB_fd, line, 1 ) < 1
         || *line != ACK )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
		return FAILURE;
    }

    sigprocmask( SIG_SETMASK, &old_mask, NULL );

	return SUCCESS;
}


/*--------------------------------------------------------------*
 * Function for reading data from a device. Exoects the device
 * number, a pointer to memory for storing the data sent by the
 * device and a pointer to a long that on enry contains the
 * maximum number of bytes to be read and, on exit, the number
 * of bytes that were actually sent by the device.
 *--------------------------------------------------------------*/

int
gpib_read( int    dev,
           char * buffer,
           long * length )
{
	char line[ 30 ];
	ssize_t len;
	long val;
    sigset_t old_mask;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( GPIB_fd < 0 )
        return FAILURE;

    block_signals( &old_mask );

    /* Send the 'magic value' for gpib_read(), the device ID and the
       maximum number of bytes to be read. */

	len = sprintf( line, "%d %d %ld\n", GPIB_READ, dev, *length );
	if ( swrite( GPIB_fd, line, len ) != len )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        return FAILURE;
    }

    /* The expected answer is either a line with the number of bytes
       that got read (which can't be larger then the number of bytes
       we asked for but smaller) or a single NAK character (either
       because memory allocation for a large enough buffer or the
       actual call of gpib_read() failed) */

    if (    sread( GPIB_fd, line, 1 ) != 1
         || *line == NAK
         || ( len = readline( GPIB_fd, line + 1, sizeof line - 2 ) ) < 1 )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        return FAILURE;
    }

	line[ ++len ] = '\0';
    if ( extract_long( line, '\n', &val ) || val < 0 || val > *length )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        return FAILURE;
    }

    /* Send a single ACK char as acknowledgment and then read the data
       obtained from the device */

    if (    swrite( GPIB_fd, STR_ACK, 1 ) != 1
         || sread( GPIB_fd, buffer, val ) != val )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        return FAILURE;
    }

    sigprocmask( SIG_SETMASK, &old_mask, NULL );
    *length = val;

	return SUCCESS;
}
	

/*---------------------------------------------------------*
 * Function for doing a serial poll of the device. Expects
 * the device number and a pointer to an (unsiigned) char
 * for returning the status.
 *---------------------------------------------------------*/

int
gpib_serial_poll( int             dev,
				  unsigned char * stb )
{
	char line[ 20 ];
	ssize_t len;
    int val;
    sigset_t old_mask;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( GPIB_fd < 0 )
        return FAILURE;

    block_signals( &old_mask );

    /* Send a line with the 'magic value' for gpib_serial_poll() and the
       device number. Expect either either the status byte as a number
       (must be positive an not larger the 255) or a single NAK character */

	len = sprintf( line, "%d %d\n", GPIB_SERIAL_POLL, dev );
	if (    swrite( GPIB_fd, line, len ) != len
		 || ( len = readline( GPIB_fd, line, sizeof line ) ) < 1
         || ( len == 1 && *line == NAK ) )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
		return FAILURE;
    }

    sigprocmask( SIG_SETMASK, &old_mask, NULL );

	line[ len ] = '\0';
    if (    extract_int( line, '\n', &val )
         || val < 0
         || val > 255 )
		return FAILURE;

	*stb = val;
	return SUCCESS;
}


/*-----------------------------------------------------*
 * Function returns a string with a description of the
 * last error encountered for one of our devices. The
 * returned string may not be used by the caller
 * (and must not be free()'ed).
 *-----------------------------------------------------*/

const char *
gpib_last_error( void )
{
	char line[ 20 ];
	ssize_t len;
	long val;
    sigset_t old_mask;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( GPIB_fd < 0 )
		return err_msg;

    *err_msg = '\0';

    block_signals( &old_mask );

	len = sprintf( line, "%d\n", GPIB_LAST_ERROR );
	if (    swrite( GPIB_fd, line, len ) != len
		 || ( len = readline( GPIB_fd, line, sizeof line - 1 ) ) < 2 )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        return "Communication failure with GPIB daemon";
    }

	line[ len ] = '\0';
    if ( extract_long( line, '\n', &val ) || val < 0 )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        return "Communication failure with GPIB daemon";
    }

	if ( val == 0 )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
		return strcpy( err_msg, "No errror message available" );
    }

    if (    swrite( GPIB_fd, STR_ACK, 1 ) != 1
         || sread( GPIB_fd, err_msg, val ) != val )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        return "Communication failure with GPIB daemon";
    }

    sigprocmask( SIG_SETMASK, &old_mask, NULL );

	err_msg[ val ] = '\0';
	return err_msg;
}


/*----------------------------------------------------------*
 * Function for dealing with simple commands that just need
 * to pass the device ID and wait for a reply consisting of
 * either a ACK or NAK character.
 *----------------------------------------------------------*/

static
int
simple_gpib_call( int dev,
                  int func_id )
{
	char line[ 20 ];
	ssize_t len;
    sigset_t old_mask;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( GPIB_fd < 0 )
        return FAILURE;

    block_signals( &old_mask );

    /* Send line with 'magic value' for requested function and the device
       number. The expected reply is either a ACK or NAK character. */

	len = sprintf( line, "%d %d\n", func_id, dev );
	if (    swrite( GPIB_fd, line, len ) != len
		 || sread( GPIB_fd, line, 1 ) != 1
         || *line != ACK )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
		return FAILURE;
    }

    sigprocmask( SIG_SETMASK, &old_mask, NULL );
	return SUCCESS;
}


/*--------------------------------------------------------*
 * Tries to open a connection to the GPIB daemon. Returns
 * a file descriptor for a socket connected to the "GPIB
 * daemon" on success, -1 if the daemon isn't running and
 * -2 on failure.
 *--------------------------------------------------------*/

#ifndef GPIB_LIBRARY_NONE
static
int
connect_to_gpibd( void )
{
    int sock_fd;
    struct sockaddr_un serv_addr;
    char reply = ACK;


    /* Try to get a socket */

    if ( ( sock_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
        return -2;

    memset( &serv_addr, 0, sizeof serv_addr );
    serv_addr.sun_family = AF_UNIX;
    strcpy( serv_addr.sun_path, GPIBD_SOCK_FILE );

    /* If connect fails due to connection refused or because there's no
       socket file (or it exists but isn't a socket file) it means gpibd
       isn't running and got to be started */

    if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
                  sizeof serv_addr ) == -1 )
    {
        int saved_errno = errno;

        shutdown( sock_fd, SHUT_RDWR );
        close( sock_fd );
        if ( saved_errno == ECONNREFUSED || saved_errno == ENOENT )
            return -1;
        return -2;
    }

    if ( swrite( sock_fd, STR_ACK, 1 ) != 1 )
    {
        shutdown( sock_fd, SHUT_RDWR );
        close( sock_fd );
        print( FATAL, "Connection to GPIB daemon failed.\n" );
        return -2;
    }

    /* Try to read a single character that tells if the daemon is able to
       deal with our connection. NAK means there are already too many
       connections. */

    if (    read( sock_fd, &reply, 1 ) != 1
         || reply != ACK )
    {
        shutdown( sock_fd, SHUT_RDWR );
        close( sock_fd );
        if ( reply == NAK )
            print( FATAL, "Too many concurrent users of GPIB.\n" );
        return -2;
    }
        
    return sock_fd;
}
#endif


/*-----------------------------------------------------------*
 * Starts the GPIB daemon and then opens a connection to it.
 * Returns a file descriptor for a socket on success or -2
 * or -1 on failure.
 *-----------------------------------------------------------*/

#ifndef GPIB_LIBRARY_NONE
static
int
start_gpibd( void )
{
	const char *a[ 2 ] = { NULL, NULL };
	pid_t pid;
    char c;
    int ret;
    int p[ 2 ];


    if ( pipe( p ) == -1 )
        return -2;

    if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
        a[ 0 ] = srcdir "gpibd";
    else
        a[ 0 ] = bindir "gpibd";

    /* Fork of the daemon as a new child process */

	if ( ( pid = fork( ) ) < 0 )
		return -2;

	if ( pid == 0 )
	{
        /* The daemon doesn't stdin and stdout and stderr only to be able
           to send back 1a single character (wich we're going to try to
           read from the pipe) when it's ready to accept connections. */

		close( STDIN_FILENO );
		close( STDOUT_FILENO );

        if ( dup2( p[ 1 ], STDERR_FILENO ) == -1 )
            _exit( EXIT_FAILURE );

		execv( a[ 0 ], ( char ** ) a );
		_exit( EXIT_FAILURE );
	}

	/* Wait for the daemon to send a single character (voa the pipe) that tells
       us that it's prepared to accept connections. Also check for the case
       that it exited. */

    while ( ( ret = read( p[ 0 ], &c, 1 ) ) != 1 )
    {
        if ( pid == waitpid( pid, NULL, WNOHANG ) )
            return -2;

        if ( ret == -1 && errno == EINTR )
            continue;

        return -2;
    }

    /* Otherwise return the result of an attempt to connect to it */

	return connect_to_gpibd( );
}
#endif


/*----------------------------------------------------------------*
 * Writes as many bytes as was asked for to file descriptor,
 * returns the number of bytes on success and -1 otherwise.
 *----------------------------------------------------------------*/

static
ssize_t
swrite( int          fd,
        const char * buf,
        ssize_t      len )
{
    ssize_t n = len;


    while ( n )
    {
        int ret;

        if ( ( ret = write( fd, buf, n ) ) == -1 )
        {
            if  ( errno == EINTR )
                continue;
            return -1;
        }

        buf += ret;
        n   -= ret;
    }

    return len;
}


/*------------------------------------------------------------*
 * Reads as many bytes as was asked for from file descriptor,
 * returns the number of bytes on success and -1 otherwise.
 *------------------------------------------------------------*/

static
ssize_t
sread( int       fd,
       char    * buf,
       ssize_t   len )
{
    ssize_t n = len;


    while ( n )
    {
        int ret;

        switch ( ret = read( fd, buf, n ) )
        {
            case -1 :
                if ( errno == EINTR )
                    break;
                return -1;

            case 0 :
                return -1;

            default :
                buf += ret;
                n   -= ret;
        }
    }

    return len;
}
               

/*------------------------------------------------------------*
 * Reads a line-feed terminated (but only up to a maximum of
 * bytes) from a file descriptor, returns the number of bytes
 * read on success and -1 on failure
 *------------------------------------------------------------*/

static
ssize_t
readline( int       fd,
          char    * buf,
          ssize_t   max_len )
{
    ssize_t n = 0;


    if ( max_len == 0 )
        return 0;

    do
        if ( sread( fd, buf, 1 ) != 1 )
            return -1;
    while ( ++n < max_len && *buf++ != '\n' );

    return n;
}


/*--------------------------------------------*
 * Tries to extract a long from a line, which
 * has to be follwed directly by 'ec'. The
 * result is stored in 'val'.
 *--------------------------------------------*/

static
int
extract_long( char * line,
             char   ec,
             long * val )
{
    char *eptr;


    *val = strtol( line, &eptr, 10 );

    if (    eptr == line
         || *eptr != ec )
        return -1;

    if ( ( *val == LONG_MAX || *val == LONG_MIN ) && errno == ERANGE )
        return 1;

    return 0;
}


/*--------------------------------------------*
 * Tries to extract an int from a line, which
 * has to be follwed directly by 'ec' The
 * result is stored in 'val'.
 *--------------------------------------------*/

static
int
extract_int( char * line,
            char   ec,
            int  * val )
{
    long lval;
    char *eptr;

    lval = strtol( line, &eptr, 10 );

    if (    eptr == line
         || *eptr != ec )
        return -1;

    if ( lval > INT_MAX || lval < INT_MIN )
        return 1;

    *val = lval;
    return 0;
}


/*---------------------------------------------------*
 * Blocks "DO_QUIT" and SIGALRM to make sure we don't get
 * interrupted while talking with the GPIB daemon.
 *---------------------------------------------------*/

static
void
block_signals( sigset_t * old_mask )
{
    sigset_t new_mask;


    sigemptyset( &new_mask );
    sigaddset( &new_mask, DO_QUIT );
    sigaddset( &new_mask, SIGALRM );
    sigprocmask( SIG_BLOCK, &new_mask, old_mask );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
