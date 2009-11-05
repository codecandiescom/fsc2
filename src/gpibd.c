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


typedef struct {
    pthread_t tid;
    pid_t     pid;
    int       fd;
    char      err_msg[ 1024 ];
} thread_data_T;

typedef struct
{
    char      * name;               /* symbolic name of device */
    int         dev_id;             /* GPIB library ID of device */
    pthread_t   tid;                /* ID of thread responsible for device */
} device_T;

thread_data_T thread_data[ MAX_THREADS ];
device_T devices[ MAX_DEVICES ];

pthread_mutex_t gpib_mutex   = PTHREAD_MUTEX_INITIALIZER;

size_t thread_count = 0;
size_t device_count = 0;


static void new_client( int fd );
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
static int gpibd_triggger( int    fd,
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
static ssize_t read_line( int    fd,
                          void * vptr,
                          size_t max_len );
static ssize_t do_read( int    fd,
						char * ptr );
static thread_data_T * find_thread_data( pthread_t tid );
static int get_long( char ** line,
					 char    ec,
					 long  * val );
static int get_ulong( char          ** line,
					  char             ec,
					  unsigned long  * val );
static int get_int( char ** line,
					char    ec,
					int   * val );
static int create_socket( void );
static void set_gpibd_signals( void );


/*--------------------------------------------*
 *--------------------------------------------*/

int
main( void )
{
    int fd;
    fd_set fds;
    struct timeval timeout;


	set_gpibd_signals( );

    /* Create a UNIX domain socket we're going to listen on for
       connections */

    if ( ( fd = create_socket( ) ) == -1 )
        return EXIT_FAILURE;

    /* Initialize the GPIB library */

    if ( gpib_init( GPIB_LOG_FILE, GPIB_LOG_LEVEL ) != SUCCESS )
    {
        shutdown( fd, SHUT_RDWR );
        close( fd );
        return EXIT_FAILURE;
    }

    /* Send parent a signal to tell it we're done with initialization */

    kill( getppid( ), SIGUSR2 );

    /* Wait for connections and quit when no clients exist anymore */

    do
    {
        FD_ZERO( &fds );
        FD_SET( fd, &fds );

        timeout.tv_sec  = 1;
        timeout.tv_usec = 0; 

        if ( select( fd + 1, &fds, NULL, NULL, &timeout ) == 1 )
        {
            pthread_mutex_lock( &gpib_mutex );
            new_client( fd );
            pthread_mutex_unlock( &gpib_mutex );
        }
   } while ( thread_count > 0 );

    shutdown( fd, SHUT_RDWR );
    close( fd );
    unlink( GPIBD_SOCK_FILE );

    gpib_shutdown( );

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static void
new_client( int fd )
{
    struct sockaddr_un cli_addr;
    socklen_t cli_len = sizeof cli_addr;
    int cli_fd;
    pthread_attr_t attr;


    /* Accept the connection by the new client */

    if ( ( cli_fd = accept( fd, ( struct sockaddr * ) &cli_addr,
                            &cli_len ) ) < 0 )
        return;

    /* Store the socket descriptor in an empty slot */

    thread_data[ thread_count ].fd = cli_fd;

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


/*--------------------------------------------*
 *--------------------------------------------*/

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
    int ( * f[ ] )( int, char * ) = { gpibd_init,
									  gpibd_shutdown,
									  gpibd_init_device,
									  gpibd_timeout,
									  gpibd_clear_device,
									  gpibd_local,
									  gpibd_local_lockout,
									  gpibd_triggger,
									  gpibd_wait,
									  gpibd_write,
									  gpibd_read,
									  gpibd_serial_poll,
									  gpibd_last_error };


    /* Wait for the file descriptor we're going to listen on being set and
       store it in a local variable */

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
        usleep( 1000 );
    }

    /* Now keep on waiting for requests from the client until the client
       either tells us it's finished or a severe error happens */

    while ( 1 )
    {
        int ret;

        /* Get a line-feed terminated line from the client */

        if (    ( len = read_line( fd, buf, sizeof buf - 1 ) ) < 2
             || buf[ len - 1 ] != '\n' )
            break;

        cmd = strtol( buf, &eptr, 10 );

        /* Give up on obviously wrong data sent by the client */

        if (    eptr == buf
             || cmd < 0
             || cmd > GPIB_LAST_ERROR
             || (    ( cmd == GPIB_SHUTDOWN || cmd == GPIB_LAST_ERROR )
                  && *eptr != '\n' )
             || *eptr != ' ' )
            break;

        /* Call the appropriate function, during that time no other thread
           may access the GPIB bus */

        pthread_mutex_lock( &gpib_mutex );
        ret = f[ cmd ]( fd, eptr + 1 );
        pthread_mutex_unlock( &gpib_mutex );

        /* Leave on error or shutdown request */

        if ( ret == -1 || cmd == GPIB_SHUTDOWN )
            break;
    }

    /* When we're done remove all devices we may have been using, close
       the socket and remove ourself from the list of threads */

    pthread_mutex_lock( &gpib_mutex );

    cleanup_devices( pthread_self( ) );

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


/*--------------------------------------------*
 *--------------------------------------------*/

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


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_init( int    fd,
            char * line )
{
    long pid;


    /* Client should have sent its PID */

    if ( get_long( &line, '\n', &pid ) || pid < 0 )
    {
		pid = write( fd, "FAIL\n", 5 );
        return -1;
    }

    gpib_log_message( "New connection, PID = %ld\n", pid );

    find_thread_data( pthread_self( ) )->pid = pid;

    if ( write( fd, "OK\n", 3 ) != 3 )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_shutdown( int    fd    UNUSED_ARG,
                char * line  UNUSED_ARG )
{
    gpib_log_message( "Connection closed, PID = %ld\n",
                      ( long ) find_thread_data( pthread_self( ) )->pid );
    return 0;
}

/*--------------------------------------------*
 *--------------------------------------------*/

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
        sprintf( find_thread_data( pthread_self( ) )->err_msg, "Too many "
                 "devices used concurrently" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
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
                sprintf( gpib_error_msg, "Device %s is already in use by a "
                         "different process, PID = %ld", devices[ i ].name,
                         ( long ) find_thread_data( devices[ i ].tid )->pid );

            if ( write( fd, "FAIL\n", 5 ) != 5 )
                return -1;
        }

    /* Initialze the device */

    if ( gpib_init_device( line, &dev_id ) != SUCCESS )
    {
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    /* Mark the device as in use */

    devices[ device_count ].dev_id = dev_id;
    devices[ device_count ].tid    = pthread_self( );

    if ( ! ( devices[ device_count ].name = strdup( line ) ) )
    {
        gpib_remove_device( dev_id );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    device_count++;

    len = sprintf( line, "%d\n", dev_id );

    if ( write( fd, line, len ) != len )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_timeout( int    fd,
               char * line )
{
    int dev_id;
    int timeout;
    int ret;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = get_int( &line, ' ', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_timeout() with invalid device handle" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( ( ret = get_int( &line, ' ', &timeout ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_timeout() with invalid timeout value" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( gpib_timeout( dev_id, timeout ) != SUCCESS )
    {
        strcpy( find_thread_data( pthread_self( ) )->err_msg, gpib_error_msg );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( write( fd, "OK\n", 3 ) != 3 )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_clear_device( int    fd,
                    char * line )
{
    int dev_id;
    int ret;


    /* Client should have sent device ID */

    if ( ( ret = get_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_clear_device() with invalid device handle" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( gpib_clear_device( dev_id ) != SUCCESS )
    {
        strcpy( find_thread_data( pthread_self( ) )->err_msg, gpib_error_msg );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( write( fd, "OK\n", 3 ) != 3 )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_local( int    fd,
             char * line )
{
    int dev_id;
    int ret;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = get_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_local() with invalid device handle" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( gpib_local( dev_id ) != SUCCESS )
    {
        strcpy( find_thread_data( pthread_self( ) )->err_msg, gpib_error_msg );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( write( fd, "OK\n", 3 ) != 3 )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_local_lockout( int    fd,
                     char * line )
{
    int dev_id;
    int ret;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = get_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_local_lockout() with invalid device handle" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( gpib_local_lockout( dev_id ) != SUCCESS )
    {
        strcpy( find_thread_data( pthread_self( ) )->err_msg, gpib_error_msg );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( write( fd, "OK\n", 3 ) != 3 )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_triggger( int    fd,
                char * line )
{
    int dev_id;
    int ret;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = get_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_trigger() with invalid device handle" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( gpib_trigger( dev_id ) != SUCCESS )
    {
        strcpy( find_thread_data( pthread_self( ) )->err_msg, gpib_error_msg );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( write( fd, "OK\n", 3 ) != 3 )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

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

    if ( ( ret = get_int( &line, ' ', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_wait() with invalid device handle" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }
        
    if ( ( ret = get_int( &line, '\n', &mask ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_wait() with invalid mask value" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( gpib_wait( dev_id, mask, &status ) != SUCCESS )
    {
        strcpy( find_thread_data( pthread_self( ) )->err_msg, gpib_error_msg );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    len = sprintf( line, "%d\n", status );

    if ( write( fd, line, len ) != len )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_write( int    fd,
             char * line )
{
    int ret;
    int dev_id;
    unsigned long len;
    char *buf;
    long bytes_to_read;


    /* Client should have sent device ID and the number of bytes to write */

    if ( ( ret = get_int( &line, ' ', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_write() with invalid device handle" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( ( ret = get_ulong( &line, '\n', &len ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_write() with invalid buffer length value" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    /* Get a large enough buffer */

    if ( ( buf = malloc( len ) ) == NULL )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Running out of memory in gpib_write()" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( write( fd, "OK\n", 3 ) != 3 )
        return -1;

    /* Read the complete data from the client, avoid getting thrown off by
       read() not returning as many bytes as expected... */

    bytes_to_read = len;
	len = 0;

    while ( bytes_to_read )
    {
		ssize_t bytes_read;

        if ( ( bytes_read = read( fd, buf + len, bytes_to_read ) ) < 0 )
        {
            if ( bytes_read != EAGAIN && bytes_read != EINTR )
                return -1;
            continue;
        }
        bytes_to_read -= bytes_read;
		len += bytes_read;
    }

    /* ...and pass them on to the device */

    if ( gpib_write( dev_id, buf, len ) != SUCCESS )
    {
        strcpy( find_thread_data( pthread_self( ) )->err_msg, gpib_error_msg );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    /* Signal back success */

    if ( write( fd, "OK\n", 3 ) != 3 )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

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

    if ( ( ret = get_int( &line, ' ', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_read() with invalid device handle" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( ( ret = get_long( &line, '\n', &len ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_read() with invalid length value" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    /* Get a large enough buffer */

    if ( ( buf = malloc( len ) ) == NULL )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Running out of memory in gpib_read()" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    /* Now read from the device */

    if ( gpib_read( dev_id, buf, &len ) != SUCCESS )
    {
        free( buf );
        strcpy( find_thread_data( pthread_self( ) )->err_msg, gpib_error_msg );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    /* First send the length of the buffer a line-feed terminated line,
       then all the data */

    mlen = sprintf( line, "%lu\n", len );
    if (    write( fd, line, mlen ) != mlen
         || write( fd, buf, len ) != len )
    {
        free( buf );
        return -1;
    }

    free( buf );
    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_serial_poll( int    fd,
                   char * line )
{
    int dev_id;
    int ret;
    unsigned char stb;


    /* Client should have sent device ID and timeout value */

    if ( ( ret = get_int( &line, '\n', &dev_id ) ) < 0 )
        return -1;
    else if ( ret )
    {
        sprintf( find_thread_data( pthread_self( ) )->err_msg,
                 "Call of gpib_serial_poll() with invalid device handle" );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( gpib_serial_poll( dev_id, &stb ) != SUCCESS )
    {
        strcpy( find_thread_data( pthread_self( ) )->err_msg, gpib_error_msg );
        if ( write( fd, "FAIL\n", 5 ) != 5 )
            return -1;
        return 0;
    }

    if ( write( fd, &stb, 1 ) != 1 )
        return -1;

    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
gpibd_last_error( int    fd,
                  char * line )
{
    char *err_msg = find_thread_data( pthread_self( ) )->err_msg;
    ssize_t len = strlen( err_msg );
	ssize_t llen;


    llen = sprintf( line, "%lu\n", ( unsigned long ) len );

    if (    write( fd, line, llen ) != llen
         || ( len > 0 && write( fd, err_msg, len ) != len ) )
        return -1;

    return 0;
}


/*----------------------------------------------------------------*
 * Reads a line of text of max_len characters ending in '\n' from
 * a socket. This is directly copied from W. R. Stevens, UNP1.2.
 *----------------------------------------------------------------*/

static ssize_t
read_line( int    fd,
           void * vptr,
           size_t max_len )
{
    ssize_t n,
            rc;
    char c,
         *ptr = vptr;


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


/*-----------------------------------------------------*
 * This is directly copied from W. R. Stevens, UNP1.2.
 *-----------------------------------------------------*/

static ssize_t
do_read( int    fd,
         char * ptr )
{
    static int read_cnt;
    static char *read_ptr;
    static char read_buf[ 1028 ];


    if ( read_cnt <= 0 )
    {
      again:
        if ( ( read_cnt = read( fd, read_buf, sizeof read_buf ) ) < 0 )
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
 *--------------------------------------------*/

static int
get_long( char ** line,
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
 *--------------------------------------------*/

static int
get_ulong( char          ** line,
           char             ec,
           unsigned long  * val )
{
    char *eptr;


    *val = strtoul( *line, &eptr, 10 );

    if (    eptr == *line
         || *eptr != ec )
        return -1;

    if ( *val == ULONG_MAX && errno == ERANGE )
        return 1;

    *line = eptr + 1;
    return 0;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static int
get_int( char ** line,
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

    unlink( GPIBD_SOCK_FILE );
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
