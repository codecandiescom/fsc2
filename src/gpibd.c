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


/*
  This file, together with the file for interfacing to the GPIB library
  used, makes up the "GPIB daemon". Having a kind of daemon for the GPIB
  is necessary when there are several instances of fsc2, all trying to
  access the GPIB bus at the same time. In that case it must be avoided
  that these instances try to access the single bus asynchronously or
  that more than one instance tries to talk to the same device.

  Thus each instance must connect to the "GPIB daemon" (and start it if
  it's not yet running) and then route all requests through the daemon.
  The daemon, exclusively talking to the devices on the GPIB, can now
  enforce that requests to the devices happen in an ordered fashion and
  also ensure that only a single process can talk to a device (until
  that process "releases" the device).

  The "GPIB daemon" creates a UNIX domain socket it's listening on and
  accepts connections from each instance of fsc2, starting a new thread
  for each connection.

  An instance of fsc2 makes requests by sending a line starting with a
  unique number for the kind of request (possibly followed by some more,
  request-dependend data). Before handling the request the thread handing
  requests by this instance of fsc2 locks a mutex that gives it exclusive
  access to the GPIB bus, thereby avoiding intermixing data on the bus
  from different requests. Only when the request is satisfied the mutex
  is unlocked again and another instance of fsc2 and the corresponding
  thread can get access to the GPIB.

  At the same time there's a list of all device claimed by the different
  instances of fsc2. Once a device is succesfully claimed by one instance
  of fsc2 requests for that device by another instance of fsc2 are denied,
  thus avoiding that different instances send unrelated requests to the
  same device.

  Since fsc2 supports diferrent libraries for GPIB cards the daemons gets
  built with an interface suitable for the GPIB library being used. These
  "interfaces" are the files named "gpib_if_*.[ch]x" (and in some cases
  an additional flex and bison file).
*/


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "fsc2_config.h"
#include "gpibd.h"
#include "gpib_if.h"


/* The maximum number of devices that can be connected to the GPIB bus
   at once is 15, one of them being the controller */

#define MAX_DEVICES   14


/* Since there can't be more than 15 GPIB devices on the bus, including the
   controller which is handled by the main thread, there never can be more
   than 14 threads */

#define MAX_THREADS MAX_DEVICES


/* Typedef for a structure with thread specific data */

typedef struct {
    pthread_t tid;                  /* thread ID */
    pid_t     pid;                  /* PID of process the thread is serving */
    int       fd;                   /* socket file descriptor */
    char      err_msg[ GPIB_ERROR_BUFFER_LENGTH ];
} thread_data_T;


/* Typedef for a structure with device specific data */

typedef struct
{
    char      * name;               /* symbolic name of device */
    int         dev_id;             /* GPIB library ID of device */
    pthread_t   tid;                /* thread handling the device */
} device_T;


static thread_data_T thread_data[ MAX_THREADS ];
static device_T devices[ MAX_DEVICES ];


/* Mutex for protecting accssesto the GPIB bus */

static pthread_mutex_t gpib_mutex   = PTHREAD_MUTEX_INITIALIZER;

static size_t thread_count = 0;
static size_t device_count = 0;

char *gpib_error_msg;


static void new_client( int fd );
static int test_connect( void );
static size_t check_connections( void );
static void cleanup_devices( pthread_t tid );
static void * gpib_handler( void * null );
static int gpibd_init( int    fd,
                       char * line );
static int gpibd_shutdown( int    fd,
                           char * line );
static int gpibd_init_device( int    fd,
                              char * line );
static int gpibd_timeout( int    fd,
                          char * line );
static int gpibd_clear_device( int    fd,
                               char * line );
static int gpibd_local( int    fd,
                        char * line );
static int gpibd_local_lockout( int    fd,
                                char * line );
static int gpibd_trigger( int    fd,
                          char * line );
static int gpibd_wait( int    fd,
                       char * line );
static int gpibd_write( int    fd,
                        char * line );
static int gpibd_read( int    fd,
                       char * line );
static int gpibd_serial_poll( int    fd,
                              char * line );
static int gpibd_last_error( int    fd,
                             char * line );
static ssize_t swrite( int          d,
                       const char * buf,
                       ssize_t      len );
static ssize_t sread( int       d,
                      char    * buf,
                      ssize_t   len );
static ssize_t readline( int       d,
                         char    * vptr,
                         ssize_t   max_len );
static thread_data_T * find_thread_data( pthread_t tid );
static int extract_long( char ** line,
                         char    ec,
                         long  * val );
static int extract_int( char ** line,
                        char    ec,
                        int   * val );
static int create_socket( void );
static int send_nak( int fd );
static int send_ack( int fd );
static void set_gpibd_signals( void );



/*--------------------------------------------*
 *--------------------------------------------*/

int
main( void )
{
    struct stat sbuf;
    int fd;
    fd_set fds;
    struct timeval timeout;
    char err_msg[ GPIB_ERROR_BUFFER_LENGTH ];


    /* Igore all signals */

	set_gpibd_signals( );

    /* Check that the socket file does not exist, otherwise assume that
       another instance of gpibd is already running */

    if ( stat( GPIBD_SOCK_FILE, &sbuf ) != -1 )
    {
        if (    S_ISSOCK( sbuf.st_mode )
             && test_connect( ) == -1 )
            return EXIT_FAILURE;
        unlink( GPIBD_SOCK_FILE );
    }

    /* Create a UNIX domain socket we're going to listen on for
       connections */

    if ( ( fd = create_socket( ) ) == -1 )
        return EXIT_FAILURE;

    /* Initialize the GPIB library */

    gpib_error_msg = err_msg;
    if ( gpib_init( GPIB_LOG_FILE, GPIB_LOG_LEVEL ) != SUCCESS )
    {
        shutdown( fd, SHUT_RDWR );
        close( fd );
        unlink( GPIBD_SOCK_FILE );
        return EXIT_FAILURE;
    }

    /* Send parent a signal to tell it we're listening n the socket */

    kill( getppid( ), SIGUSR2 );

    /* Wait for connections and quit when no clients exist anymore (the
       first client will connect more or less immediately since it's our
       parent) */

    do
    {
        FD_ZERO( &fds );
        FD_SET( fd, &fds );

        timeout.tv_sec  = 5;
        timeout.tv_usec = 0; 

        /* Wait for a new clients trying to connect and regularly check if
           the processes for which threads where started are still alive */

        if ( select( fd + 1, &fds, NULL, NULL, &timeout ) == 1 )
        {
            pthread_mutex_lock( &gpib_mutex );
            new_client( fd );
        }
        else
            pthread_mutex_lock( &gpib_mutex );

        check_connections( );
        pthread_mutex_unlock( &gpib_mutex );
   } while ( thread_count > 0 );

    shutdown( fd, SHUT_RDWR );
    close( fd );
    unlink( GPIBD_SOCK_FILE );

    gpib_shutdown( );

    return 0;
}


/*---------------------------------------------------*
 * Called whenever there's a new connection. Creates
 * a new thread for dealing with the client.
 *---------------------------------------------------*/

static void
new_client( int fd )
{
    struct sockaddr_un cli_addr;
    socklen_t cli_len = sizeof cli_addr;
    int cli_fd;
    pthread_attr_t attr;
    char c;


    /* Accept the connection by the new client */

    if ( ( cli_fd = accept( fd, ( struct sockaddr * ) &cli_addr,
                            &cli_len ) ) < 0 )
        return;

    /* Check if the client sends an ACK character */

    if ( sread( cli_fd, &c, 1 ) != 1
            || c != ACK )
    {
        shutdown( cli_fd, SHUT_RDWR );
        close( cli_fd );
        return;
    }

    /* If all the threads we're prepared to run are used up check if there
       are dead clients and close connections for those, and if then there
       still are no free threads just send a single NAK character and close
       the socket. */

    if (    thread_count >= MAX_THREADS
         && check_connections( ) >= MAX_THREADS )
    {
        swrite( cli_fd, STR_NAK, 1 );
        shutdown( cli_fd, SHUT_RDWR );
        close( cli_fd );
        return;
    }

    /* Send a single ACK character, if that fails close down connection */

    if ( swrite( cli_fd, STR_ACK, 1 ) != 1 )
    {
        shutdown( cli_fd, SHUT_RDWR );
        close( cli_fd );
        return;
    }

    /* Store the socket descriptor in an empty slot and initialize the
       string for error messages */

    thread_data[ thread_count ].fd  = cli_fd;
    thread_data[ thread_count ].pid = -1;
    *thread_data[ thread_count ].err_msg = '\0';

    /* Create a new thread for dealing with the client */

    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

    if ( pthread_create( &thread_data[ thread_count ].tid,
                         &attr,gpib_handler, NULL ) )
    {
        shutdown( cli_fd, SHUT_RDWR );
        close( cli_fd );
        pthread_attr_destroy( &attr );
        return;
    }

    pthread_attr_destroy( &attr );
    thread_count++;
}


/*--------------------------------------------------------*
 * Function tests if another instance of gpibd is already
 * running by trying to connect to the socket file. If
 * this is successful or fails due to other reasons than
 * that there's nothing accepting connections return 0,
 * otherwise -1.
 *--------------------------------------------------------*/

static int
test_connect( void )
{
    int sock_fd;
    struct sockaddr_un serv_addr;
    int ret = 0;


    /* Try to get a socket */

    if ( ( sock_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
        return -1;

    memset( &serv_addr, 0, sizeof serv_addr );
    serv_addr.sun_family = AF_UNIX;
    strcpy( serv_addr.sun_path, GPIBD_SOCK_FILE );

    /* If connect fails due to connection refused or because there's no socket
       file (or it exists but isn't a socket file) it means gpibd isn't running
       and we've got to start it */

    if (    connect( sock_fd, ( struct sockaddr * ) &serv_addr,
                     sizeof serv_addr ) != -1 )
    {
        swrite( sock_fd, STR_NAK, 1 );
        ret = -1;
    }
    else if ( errno != ECONNREFUSED && errno != ENOENT )
        ret = -1;

    shutdown( sock_fd, SHUT_RDWR );
    close( sock_fd );

    return ret;
}


/*----------------------------------------------------*
 * Function checks if clients still exist and removes
 * the threads handling those that have vanished
 *----------------------------------------------------*/

static size_t
check_connections( void )
{
    size_t i;


    for ( i = 0; i < thread_count; i++ )
        if (    thread_data[ i ].pid != -1
             && kill( thread_data[ i ].pid, 0 ) == -1
             && errno == ESRCH )
        {
            pthread_cancel( thread_data[ i ].tid );
            shutdown( thread_data[ i ].fd, SHUT_RDWR );
            close( thread_data[ i ].fd );
            cleanup_devices( thread_data[ i ].tid );
            gpib_log_message( "Connection closed for dead process, "
                              "PID = %ld\n", thread_data[ i ].pid );
            memmove( thread_data + i, thread_data + i + 1,
                     ( --thread_count - i ) * sizeof *thread_data );
            i--;
        }

   return thread_count;
}


/*--------------------------------------------------------*
 * Main function of each thread for dealing with a client
 *--------------------------------------------------------*/

static void *
gpib_handler( void * null  UNUSED_ARG )
{
    int fd = -1;
    size_t slot;
    size_t i;
	ssize_t len;
    char buf[ 1024 ];
    long cmd;
    char *eptr;
    int ret;
    int ( * gpib_func[ ] )( int, char * ) = { gpibd_init,
                                              gpibd_shutdown,
                                              gpibd_init_device,
                                              gpibd_timeout,
                                              gpibd_clear_device,
                                              gpibd_local,
                                              gpibd_local_lockout,
                                              gpibd_trigger,
                                              gpibd_wait,
                                              gpibd_write,
                                              gpibd_read,
                                              gpibd_serial_poll,
                                              gpibd_last_error };


    /* Wait for our thread ID to become available in the list of threads
       and the copy the socket file descriptor we're going to communicate
       on to a local variable. */

    while ( fd == -1 )
    {
        pthread_mutex_lock( &gpib_mutex );

        for ( slot = 0; slot < thread_count; slot++ )
            if ( pthread_equal( thread_data[ slot ].tid, pthread_self( ) ) )
            {
                fd = thread_data[ slot ].fd;
                pthread_mutex_unlock( &gpib_mutex );
                break;
            }

        pthread_mutex_unlock( &gpib_mutex );
    }

    /* Now keep on waiting for requests from the client until the client
       either tells us it's finished or a severe error happens */

    while ( 1 )
    {
        /* Get a line-feed terminated line from the client */

        if (    ( len = readline( fd, buf, sizeof buf - 1 ) ) < 2
             || buf[ len - 1 ] != '\n' )
            continue;

        buf[ len ] = '\0';
        cmd = strtol( buf, &eptr, 10 );

        /* Check for obviously wrong data sent by the client, where "wrong data"
           means not a number (or an out of range number) or the number not
           followed by a space (or a line-feed for the last error request),
           or a request other than GPIB_INIT while it hasn't called before. */

        if (    eptr == buf
             || cmd < 0
             || cmd > GPIB_LAST_ERROR
             || (    ( cmd == GPIB_LAST_ERROR || cmd == GPIB_SHUTDOWN )
                  && *eptr != '\n' )
             || (    ! ( cmd == GPIB_LAST_ERROR || cmd == GPIB_SHUTDOWN )
                  && *eptr != ' ' ) )
            continue;

        /* Set the 'gpib_error_msg' pointer to the threads string for error
           messages and call the appropriate GPIB function. During its
           execution no other thread can access the GPIB bus */

        pthread_mutex_lock( &gpib_mutex );
        gpib_error_msg = find_thread_data( pthread_self( ) )->err_msg;
        ret = gpib_func[ cmd ]( fd, eptr + 1 );
        pthread_mutex_unlock( &gpib_mutex );

        /* Quit on failed gpib_init() and always on gpib_shutdown() command */

        if ( ( cmd == GPIB_INIT && ret == -1 ) || cmd == GPIB_SHUTDOWN )
            break;
    }

    /* Close the socket and remove ourself from the list of threads */

    pthread_mutex_lock( &gpib_mutex );

    shutdown( fd, SHUT_RDWR );
    close( fd );

    for ( i = 0; i < thread_count; i++ )
        if ( pthread_equal( thread_data[ i ].tid, pthread_self( ) ) )
        {
             memmove( thread_data + i, thread_data + i + 1,
                      ( --thread_count - i ) * sizeof *thread_data );
             break;
        }

    pthread_mutex_unlock( &gpib_mutex );

    pthread_exit( NULL );
}


/*-----------------------------------------*
 * Removes all devices handled by a thread
 * from the list of devices
 *-----------------------------------------*/

static void
cleanup_devices( pthread_t tid )
{
	size_t i;


    for ( i = 0; i < device_count; i++ )
        if ( pthread_equal( devices[ i ].tid, tid ) )
        {
            gpib_remove_device( devices[ i ].dev_id );
            memmove( devices + i, devices + i + 1,
                     ( --device_count - i ) * sizeof *devices );
            i--;
        }
}


/*--------------------------------------------------*
 * Function called for a gpib_init() client request
 *--------------------------------------------------*/

static int
gpibd_init( int    fd,
            char * line )
{
    long pid;


    /* Client should have sent its PID */

    if ( extract_long( &line, '\n', &pid ) || pid < 0 )
    {
        send_nak( fd );
        return -1;
    }

    find_thread_data( pthread_self( ) )->pid = pid;
    gpib_log_message( "New connection, PID = %ld\n", pid );

    send_ack( fd );
    return 0;
}


/*---------------------------------------------------------*
 * Function called when the client wants to shutdown all
 * its devices and end the connection.
 *---------------------------------------------------------*/

static int
gpibd_shutdown( int    fd    UNUSED_ARG,
                char * line  UNUSED_ARG )
{
    cleanup_devices( find_thread_data( pthread_self( ) )->pid );
    gpib_log_message( "Connection closed, PID = %ld\n",
                      find_thread_data( pthread_self( ) )->pid );
    return 0;
}


/*---------------------------------------------------------*
 * Function called for a gpib_init_device() client request
 *---------------------------------------------------------*/

static int
gpibd_init_device( int    fd,
                   char * line )
{
    int dev_id;
    int len;
	size_t i;


    /* Check if we already have as many devices open as can be connected */

    if ( device_count >= MAX_DEVICES )
    {
        sprintf( gpib_error_msg, "Too many devices used concurrently" );
        return send_nak( fd );
    }

    /* The client should have sent the device name, check that it's not
       already in use. */

    line[ strlen( line ) - 1 ] = '\0';

    for ( i = 0; i < device_count; i++ )
        if ( ! strcmp( line, devices[ i ].name ) )
        {
            if ( pthread_equal( devices[ i ].tid, pthread_self( ) ) )
                sprintf( gpib_error_msg, "Device %s is already "
                         "initialized", devices[ i ].name );
            else
                sprintf( gpib_error_msg, "Device %s is already in use by "
                         "another process, PID = %ld", devices[ i ].name,
                         ( long ) find_thread_data( devices[ i ].tid )->pid );

            return send_nak( fd );
        }

    /* Initialze the device */

    if ( gpib_init_device( line, &dev_id ) != SUCCESS )
        return send_nak( fd );

    /* Set up the device array element for the device */

    devices[ device_count ].dev_id = dev_id;
    devices[ device_count ].tid    = pthread_self( );

    if ( ! ( devices[ device_count ].name = strdup( line ) ) )
    {
        gpib_remove_device( dev_id );
        return send_nak( fd );
    }

    device_count++;

    len = sprintf( line, "%d\n", dev_id );
    return swrite( fd, line, len ) == len ? 0 : -1;
}


/*-----------------------------------------------------*
 * Function called for a gpib_timeout() client request
 *-----------------------------------------------------*/

static int
gpibd_timeout( int    fd,
               char * line )
{
    int dev_id;
    int timeout;
    int ret;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = extract_int( &line, ' ', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_timeout() with invalid "
                 "device handle" );
        return send_nak( fd );
    }

    if ( ( ret = extract_int( &line, ' ', &timeout ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_timeout() with invalid "
                 "timeout value" );
        return send_nak( fd );
    }

    if ( gpib_timeout( dev_id, timeout ) != SUCCESS )
        return send_nak( fd );

    return send_ack( fd );
}


/*----------------------------------------------------------*
 * Function called for a gpib_clear_device() client request
 *----------------------------------------------------------*/

static int
gpibd_clear_device( int    fd,
                    char * line )
{
    int dev_id;
    int ret;


    /* Client should have sent device ID */

    if ( ( ret = extract_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_clear_device() with invalid "
                 "device handle" );
        return send_nak( fd );
    }

    if ( gpib_clear_device( dev_id ) != SUCCESS )
        return send_nak( fd );

    return send_ack( fd );
}


/*---------------------------------------------------*
 * Function called for a gpib_local() client request
 *---------------------------------------------------*/

static int
gpibd_local( int    fd,
             char * line )
{
    int dev_id;
    int ret;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = extract_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_local() with invalid device "
                 "handle" );
        return send_nak( fd );
    }

    if ( gpib_local( dev_id ) != SUCCESS )
        return send_nak( fd );

    return send_ack( fd );
}


/*-----------------------------------------------------------*
 * Function called for a gpib_local_lockout() client request
 *-----------------------------------------------------------*/

static int
gpibd_local_lockout( int    fd,
                     char * line )
{
    int dev_id;
    int ret;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = extract_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_local_lockout() with invalid "
                 "device handle" );
        return send_nak( fd );
    }

    if ( gpib_local_lockout( dev_id ) != SUCCESS )
        return send_nak( fd );

    return send_ack( fd );
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_trigger( int    fd,
               char * line )
{
    int dev_id;
    int ret;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = extract_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_trigger() with invalid device "
                 "handle" );
        return send_nak( fd );
    }

    if ( gpib_trigger( dev_id ) != SUCCESS )
        return send_nak( fd );

    return send_ack( fd );
}


/*--------------------------------------------------*
 * Function called for a gpib_wait() client request
 *--------------------------------------------------*/

static int
gpibd_wait( int    fd,
            char * line )
{
    int dev_id;
    int mask;
    int status;
    int len;
    int ret;


    /* Client should have sent device ID and mask value */

    if ( ( ret = extract_int( &line, ' ', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_wait() with invalid device "
                 "handle" );
        return send_nak( fd );
    }
        
    if ( ( ret = extract_int( &line, '\n', &mask ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_wait() with invalid mask "
                 "value" );
        return send_nak( fd );
    }

    if ( gpib_wait( dev_id, mask, &status ) != SUCCESS )
        return send_nak( fd );

    len = sprintf( line, "%d\n", status );
    return swrite( fd, line, len ) == len ? 0 : -1;
}


/*---------------------------------------------------*
 * Function called for a gpib_write() client request
 *---------------------------------------------------*/

static int
gpibd_write( int    fd,
             char * line )
{
    int ret;
    int dev_id;
    long len;
    char *buf;


    /* Client should have sent device ID and the number of bytes to write */

    if ( ( ret = extract_int( &line, ' ', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_write() with invalid device "
                 "handle" );
        return send_nak( fd );
    }

    if ( ( ret = extract_long( &line, '\n', &len ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_write() with invalid buffer "
                 "length value" );
        return send_nak( fd );
    }

    /* Get a large enough buffer */

    if ( ( buf = malloc( len ) ) == NULL )
    {
        sprintf( gpib_error_msg, "Running out of memory in gpib_write()" );
        return send_nak( fd );
    }

    /* Send an ACK as acknowledgement to client to make it send the data... */

    if ( send_ack( fd ) == -1 )
    {
        free( buf );
        return -1;
    }

    /* ...read the complete data from the client... */

    if ( len != sread( fd, buf, len ) )
    {
        free( buf );
        return -1;
    }

    /* ...and pass them on to the device */

    if ( gpib_write( dev_id, buf, len ) != SUCCESS )
    {
        free( buf );
        return send_nak( fd );
    }

    free( buf );

    /* Signal back success */

    return send_ack( fd );
}


/*--------------------------------------------------*
 * Function called for a gpib_read() client request
 *--------------------------------------------------*/

static int
gpibd_read( int    fd,
            char * line )
{
    int dev_id;
    long len;
    char *buf;
    long mlen;
	int ret;


    /* Client should have sent device ID and the number of bytes to read */

    if ( ( ret = extract_int( &line, ' ', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_read() with invalid device "
                 "handle" );
        return send_nak( fd );
    }

    if ( ( ret = extract_long( &line, '\n', &len ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_read() with invalid length "
                 "value" );
        return send_nak( fd );
    }

    /* Get a large enough buffer */

    if ( ( buf = malloc( len ) ) == NULL )
    {
        sprintf( gpib_error_msg, "Running out of memory in gpib_read()" );
        return send_nak( fd );
    }

    /* Now read from the device */

    if ( gpib_read( dev_id, buf, &len ) != SUCCESS )
    {
        free( buf );
        return send_nak( fd );
    }

    /* First send the length of the buffer as a line-feed terminated line,
       wait for an ACK character as acknowledgement and then send the data */

    mlen = sprintf( line, "%lu\n", len );
    if (    swrite( fd, line, mlen ) != mlen
         || sread( fd, line, 1 ) != 1
         || *line != ACK
         || swrite( fd, buf, len ) != len )
    {
        free( buf );
        return -1;
    }

    free( buf );
    return 0;
}


/*---------------------------------------------------------*
 * Function called for a gpib_serial_poll() client request
 *---------------------------------------------------------*/

static int
gpibd_serial_poll( int    fd,
                   char * line )
{
    int dev_id;
    int ret;
    unsigned char reply;
    ssize_t len;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = extract_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( gpib_error_msg, "Call of gpib_serial_poll() with invalid "
                 "device handle" );
        return send_nak( fd );
    }

    if ( gpib_serial_poll( dev_id, &reply ) != SUCCESS )
        return send_nak( fd );

    len = sprintf( line, "%u\n", ( unsigned int ) reply );
    return swrite( fd, line, len ) == len ? 0 : -1;
}


/*--------------------------------------------------------*
 * Function called for a gpib_last_error() client request
 *--------------------------------------------------------*/

static int
gpibd_last_error( int    fd,
                  char * line )
{
    long len = strlen( gpib_error_msg ) + 1;
	long mlen = sprintf( line, "%ld\n", len );


    /* Send a line with the length of the error message string and then,
       if its not the empty string wait for a single ACK character before
       sending the error message */

    if (    swrite( fd, line, mlen ) != mlen
         || ( len > 0
              && (    sread( fd, line, 1 ) != 1
                   || *line != ACK
                   || swrite( fd, gpib_error_msg, len ) != len ) ) )
        return -1;

    return 0;
}


/*-----------------------------------------------------------*
 * Writes as many bytes as was asked for to file descriptor,
 * returns the number of bytes on success and -1 otherwise.
 *-----------------------------------------------------------*/

static ssize_t
swrite( int          d,
        const char * buf,
        ssize_t      len )
{
    ssize_t n = len,
            ret;


    if ( len == 0 )
        return 0;

    do
    {
        if ( ( ret = write( d, buf, n ) ) < 1 )
        {
            if ( ret == -1 && errno != EINTR )
                return -1;
            continue;
        }
        buf += ret;
    } while ( ( n -= ret ) > 0 );

    return len;
}

/*----------------------------------------------------------------*
 * Reads as many bytes as was asked for from file descriptor,
 * returns the number of bytes on success and -1 otherwise.
 *----------------------------------------------------------------*/

static ssize_t
sread( int       d,
       char    * buf,
       ssize_t   len )
{
    ssize_t n = len,
            ret;


    if ( len == 0 )
        return 0;

    do
    {
        if ( ( ret = read( d, buf, n ) ) < 1 )
        {
            if ( ret == 0 || errno != EINTR )
                return -1;
            continue;
        }
        buf += ret;
    } while ( ( n -= ret ) > 0 );

    return len;
}
               

/*------------------------------------------------------------*
 * Reads a line-feed terminated (but only up to a maximum
 * number of bytes) from a file descriptor, returns the
 * number of bytes read on success and -1 on failure
 *------------------------------------------------------------*/

static ssize_t
readline( int       d,
          char    * buf,
          ssize_t   max_len )
{
    ssize_t n = 0;


    if ( max_len == 0 )
        return 0;

    do
        if ( sread( d, buf, 1 ) != 1 )
            return -1;
    while ( ++n < max_len && *buf++ != '\n' );

    return n;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static thread_data_T *
find_thread_data( pthread_t tid )
{
    size_t i;


    for ( i = 0; i < thread_count; i++ )
        if ( pthread_equal( thread_data[ i ].tid, tid ) )
            return thread_data + i;

    return NULL;
}


/*--------------------------------------------*
 * Tries to extract a long from a line, which
 * has to be follwed directly by 'ec'. The
 * result is stored in 'val'.
 *--------------------------------------------*/

static int
extract_long( char ** line,
              char    ec,
              long  * val )
{
    char *eptr;


    *val = strtol( *line, &eptr, 10 );

    if (    eptr == *line
         || *eptr != ec )
        return -1;

    if ( ( *val == LONG_MAX || *val == LONG_MIN ) && errno == ERANGE )
        return 1;

    *line = eptr + 1;
    return 0;
}


/*--------------------------------------------*
 * Tries to extract an int from a line, which
 * has to be follwed directly by 'ec' The
 * result is stored in 'val'.
 *--------------------------------------------*/

static int
extract_int( char ** line,
             char    ec,
             int   * val )
{
    long lval;
    char *eptr;

    lval = strtol( *line, &eptr, 10 );

    if (    eptr == *line
         || *eptr != ec )
        return -1;

    if ( lval > INT_MAX || lval < INT_MIN )
        return 1;

    *val = lval;
    *line = eptr + 1;
    return 0;
}


/*------------------------------------------------------*
 * Creates a UNIX domain socket the daemon is listening
 * on for clients trying to connect. Returns the file
 * descriptor on success or -1 on failure.
 *------------------------------------------------------*/

static int
create_socket( void )
{
    int listen_fd;
    struct sockaddr_un serv_addr;
    mode_t old_mask;


    /* Create a UNIX domain socket */

    if ( ( listen_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
        return -1;

    /* Bind it to the socket file */

    memset( &serv_addr, 0, sizeof serv_addr );
    serv_addr.sun_family = AF_UNIX;
    strcpy( serv_addr.sun_path, GPIBD_SOCK_FILE );

    old_mask = umask( 0 );

    if ( bind( listen_fd, ( struct sockaddr * ) &serv_addr,
               sizeof serv_addr ) == -1 )
    {
        umask( old_mask );
        shutdown( listen_fd, SHUT_RDWR );
        close( listen_fd );
        unlink( GPIBD_SOCK_FILE );
        return -1;
    }

    umask( old_mask );

    /* Start to listen on the socket for connections, allow not more than
       14 concurrent connections since there can't be more devices than
       that on the GPIB bus (the 15th is the controller) */

    if ( listen( listen_fd, MAX_THREADS ) == -1 )
    {
        shutdown( listen_fd, SHUT_RDWR );
        close( listen_fd );
        unlink( GPIBD_SOCK_FILE );
        return -1;
    }

    return listen_fd;
}


/*-----------------------------------------*
 * Sends a single NAK character to client,
 * returns 0 on success and -1 in failure
 *-----------------------------------------*/

static int
send_nak( int fd )
{
    if ( swrite( fd, STR_NAK, 1 ) != 1 )
        return -1;
    return 0;
}


/*-----------------------------------------*
 * Sends a single ACK character to client,
 * returns 0 on success and -1 in failure
 *-----------------------------------------*/

static int
send_ack( int fd )
{
    if ( swrite( fd, STR_ACK, 1 ) != 1 )
        return -1;
    return 0;
}


/*-----------------------------------------*
 * Signal handler: all signals are ignored
 *-----------------------------------------*/

static void
set_gpibd_signals( void )
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
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
