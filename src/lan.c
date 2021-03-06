/*
 *  Copyright (C) 1999-2016 Jens Thoms Toerring
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

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/timeb.h>


/* Local variables */

static LAN_List_T * lan_list = NULL;
static volatile sig_atomic_t got_sigalrm;
static int lan_log_level = LAN_LOG_LEVEL;


/* Functions used only locally */

static void timeout_init( int                dir,
                          LAN_List_T       * ll,
                          long             * us_timeout,
                          struct sigaction * old_sact,
                          struct timeval   * now );

static bool timeout_reset( int                dir,
                           LAN_List_T       * ll,
                           long             * us_timeout,
                           struct sigaction * old_sact,
                           struct timeval   * before );

static void timeout_exit( LAN_List_T       * ll,
                          struct sigaction * old_sact );

static void wait_alarm_handler( int sig_no  UNUSED_ARG );

static LAN_List_T * find_lan_entry( int handle );

static void get_ip_address( const char     * address,
                            struct in_addr * ip_addr );

static void fsc2_lan_log_date( FILE * fp );

#define LOG_FUNCTION_START( x ) fsc2_lan_log_function_start( x, __func__ )
#define LOG_FUNCTION_END( x )   fsc2_lan_log_function_end( x, __func__ )


/*-----------------------------------------------------------*
 * Function for opening a connection to a device on the LAN
 * (and, at the same time opening the log file). Returns the
 * sockets file descriptor on success or -1 on failure.
 *-----------------------------------------------------------*/

int
fsc2_lan_open( const char * dev_name,
               const char * address,
               int          port,
               long         us_timeout,
               bool         quit_on_signal )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    fsc2_assert( dev_name != NULL && * dev_name );

    /* Try to get a lock on the device */

    if ( ! fsc2_obtain_uucp_lock( dev_name ) )
    {
        print( FATAL, "Device %s is already locked by another process.\n",
               dev_name );
        return -1;
    }

    /* Try to open the log file */

    FILE * log_fp = fsc2_lan_open_log( dev_name );
    LOG_FUNCTION_START( log_fp );

    /* Try to resolve the address we got from the caller */

    struct sockaddr_in dev_addr;
    memset( &dev_addr, 0, sizeof dev_addr );
    get_ip_address( address, &dev_addr.sin_addr );

    if ( dev_addr.sin_addr.s_addr == 0 )
    {
        fsc2_lan_log_message( log_fp, "Error: invalid IP address: %s\n",
                              address );
        LOG_FUNCTION_END( log_fp );
        fsc2_lan_close_log( dev_name, log_fp );
        fsc2_release_uucp_lock( dev_name );
        return -1;
    }

    /* Check that the port number is reasonable */

    if ( port < 0 || port > 0xFFFF )
    {
        fsc2_lan_log_message( log_fp, "Error: invalid port number: %d\n",
                              port );
        LOG_FUNCTION_END( log_fp );
        fsc2_lan_close_log( dev_name, log_fp );
        fsc2_release_uucp_lock( dev_name );
        return -1;
    }

    /* Try to create a socket for a TCP connection */

    int sock_fd  = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sock_fd == -1 )
    {
        fsc2_lan_log_message( log_fp, "Error: failed to create a socket: %s\n",
                              strerror( errno ) );
        LOG_FUNCTION_END( log_fp );
        fsc2_lan_close_log( dev_name, log_fp );
        fsc2_release_uucp_lock( dev_name );
        return -1;
    }

    /* Set the SO_KEEPALIVE option to have a keepalive probe sent after
       two hours of inactivity */

    int switch_on = 1;
    if ( setsockopt( sock_fd, SOL_SOCKET, SO_KEEPALIVE,
                     &switch_on, sizeof switch_on ) == -1 )
    {
        shutdown( sock_fd, SHUT_RDWR );
        close( sock_fd );
        fsc2_lan_log_message( log_fp, "Error: failed to set SO_KEEPALIVE "
                              "option for socket\n" );
        LOG_FUNCTION_END( log_fp );
        fsc2_lan_close_log( dev_name, log_fp );
        fsc2_release_uucp_lock( dev_name );
        return -1;
    }

    /* Also disable the TCP Nagle algorithm which might slow down response
       times unnecessarily (it delays sending small packets for short
       delays of time) */

    switch_on = 1;
    if ( setsockopt( sock_fd, IPPROTO_TCP, TCP_NODELAY,
                     &switch_on, sizeof switch_on ) == -1 )
    {
        shutdown( sock_fd, SHUT_RDWR );
        close( sock_fd );
        fsc2_lan_log_message( log_fp, "Error: failed to set TCP_NODELAY "
                              "option for socket\n" );
        LOG_FUNCTION_END( log_fp );
        fsc2_lan_close_log( dev_name, log_fp );
        fsc2_release_uucp_lock( dev_name );
        return -1;
    }

    /* Set everything up for the connect() call */

    dev_addr.sin_family = AF_INET;
    dev_addr.sin_port = htons( port );

    /* If required make sure we don't wait longer for the connect() call to
       succeed than the user specified (take care of the case that setting
       up the handler for the SIGALRM signal or starting the timer failed,
       in which case we silently drop the request for a timeout) */

    struct sigaction sact,
                     old_sact;

    if ( us_timeout > 0 )
    {
        struct itimerval wait_for_connect;
        wait_for_connect.it_value.tv_sec     = us_timeout / 1000000;
        wait_for_connect.it_value.tv_usec    = us_timeout % 1000000;
        wait_for_connect.it_interval.tv_sec  = 0;
        wait_for_connect.it_interval.tv_usec = 0;

        sact.sa_handler = wait_alarm_handler;
        sigemptyset( &sact.sa_mask );
        sact.sa_flags = 0;

        got_sigalrm = 0;

        if ( sigaction( SIGALRM, &sact, &old_sact ) == -1 )
            us_timeout = 0;
        else if ( setitimer( ITIMER_REAL, &wait_for_connect, NULL ) == -1 )
        {
            int ret = sigaction( SIGALRM, &old_sact, NULL );
            fsc2_assert( ret != -1 );      /* this better doesn't happen */
            us_timeout = 0;
        }
    }

    /* Try to connect to the other side - if connect fails due to a signal
       other than SIGALRM and we are not supposed to return on such signals
       retry the connect() call */

    int conn_ret;
    do
    {
        conn_ret = connect( sock_fd, ( const struct sockaddr * ) &dev_addr,
                            sizeof dev_addr );
    } while (      conn_ret == -1
              &&   errno == EINTR
              && ! quit_on_signal
              && ! got_sigalrm );

    /* Stop the timer for the timeout and reset the signal handler */

    if ( us_timeout > 0 )
    {
        struct itimerval wait_for_connect;
        wait_for_connect.it_value.tv_sec     = 0;
        wait_for_connect.it_value.tv_usec    = 0;
        wait_for_connect.it_interval.tv_sec  = 0;
        wait_for_connect.it_interval.tv_usec = 0;

        int ret = setitimer( ITIMER_REAL, &wait_for_connect, NULL );
        fsc2_assert( ret != -1 );              /* this better doesn't happen */
        ret = sigaction( SIGALRM, &old_sact, NULL );
        fsc2_assert( ret != -1 );              /* this neither... */
    }

    if ( conn_ret == -1 )
    {
        shutdown( sock_fd, SHUT_RDWR );
        close( sock_fd );
        if (    us_timeout > 0
             && errno == EINTR
             && quit_on_signal
             && ! got_sigalrm )
            fsc2_lan_log_message( log_fp, "Error: connect() to socket "
                                  "failed due to signal\n" );
        else if ( us_timeout > 0 && errno == EINTR && got_sigalrm )
            fsc2_lan_log_message( log_fp, "Error: connect() to socket "
                                  "failed due to timeout\n" );
        else
            fsc2_lan_log_message( log_fp, "Error: connect() to socket "
                                  "failed: %s\n", strerror( errno ) );

        LOG_FUNCTION_END( log_fp );
        fsc2_lan_close_log( dev_name, log_fp );
        fsc2_release_uucp_lock( dev_name );
        return -1;
    }

    /* Since everything worked out satisfactory we now add the new connection
       to the list of connections, beginning by adding an entry to the end.
       If this fails due to runnning out of memory there's nothing left we can
       do except closing all open connections and throwing an exception. */

    LAN_List_T * ll = lan_list;

    TRY
    {
        if ( ( ll = lan_list ) == NULL )
        {
            ll = lan_list = T_malloc( sizeof *lan_list );
            ll->prev = NULL;
        }
        else
        {
            while ( ll->next != NULL )
                ll = ll->next;

            ll->next = T_malloc( sizeof *lan_list );
            ll->next->prev = ll;
            ll = ll->next;
        }

        ll->fd     = sock_fd;
        ll->name   = NULL;
        ll->name   = T_strdup( dev_name );
        ll->log_fp = log_fp;
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        bool need_close = ll->name == NULL;

        fsc2_lan_log_message( log_fp, "Error: Running out of memory\n" );
        LOG_FUNCTION_END( log_fp );
        fsc2_lan_cleanup( );
        if ( need_close )
        {
            fsc2_lan_close_log( dev_name, log_fp );
            fsc2_release_uucp_lock( dev_name );
        }

        RETHROW;
    }

    ll->next = NULL;

    /* Set the wait time for reads and writes to zero (meaning that none is
       set yet, i.e. the default timeout of the system is going to be used. */

    ll->us_read_timeout = ll->us_write_timeout = 0;
    ll->so_timeo_avail = true;

    ll->address = dev_addr.sin_addr;
    ll->port    = port;

    /* Check if we really can set a timeout for sending and receiving -
       this only became possible with 2.3.41 kernels */

    struct timeval tv;
    socklen_t len;
    if (    getsockopt( sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, &len ) == -1
         || getsockopt( sock_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, &len ) == -1 )
        ll->so_timeo_avail = false;

    if ( lan_log_level == LL_ALL )
        fsc2_lan_log_message( log_fp, "Opened connection to device %s: "
                              "IP = \"%s\", port = %d\n", dev_name,
                              address, port );

    LOG_FUNCTION_END( log_fp );

    return ll->fd;
}


/*------------------------------------------------------------*
 * Function for closing the connection to a device on the LAN
 *------------------------------------------------------------*/

int
fsc2_lan_close( int handle )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    LAN_List_T * ll = find_lan_entry( handle );
    if ( ! ll )
    {
        print( SEVERE, "Invalid handle passed to fsc2_lan_close().\n" );
        return -1;
    }

    LOG_FUNCTION_START( ll->log_fp );

    /* Try to close the socket - if it fails give the cleanup routine
       that gets automatically called at the end of the experiment
       another chance */

    shutdown( ll->fd, SHUT_RDWR );
    if ( close( ll->fd ) == -1 )
    {
        fsc2_lan_log_message( ll->log_fp, "Error: close() failed: %s\n",
                              strerror( errno ) );
        return -1;
    }

    LOG_FUNCTION_END( ll->log_fp );

    if ( ll->prev != NULL )
        ll->prev->next = ll->next;

    if ( ll->next != NULL )
        ll->next->prev = ll->prev;

    if ( ll == lan_list )
        lan_list = ll->next;

    fsc2_lan_close_log( ll->name ? ll->name : "?", ll->log_fp );

    if ( ll->name )
    {
        fsc2_release_uucp_lock( ll->name );
        T_free( ( char * ) ll->name );
    }

    T_free( ll );

    return 0;
}


/*---------------------------------------------------------------------------*
 * Function for writing data to the socket from a buffer. If 'us_timeout' is
 * set to a positive (non-zero) value don't it doesn't wait longer than that
 * many microseconds. If 'quit_on_signal' is set it returns immediately when
 * a signal gets caught.
 *---------------------------------------------------------------------------*/

ssize_t
fsc2_lan_write( int          handle,
                const char * buffer,
                long         length,
                long         us_timeout,
                bool         quit_on_signal )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    LAN_List_T * ll = find_lan_entry( handle );
    if ( ! ll )
    {
        print( SEVERE, "Invalid handle passed to fsc2_lan_write().\n" );
        return -1;
    }

    LOG_FUNCTION_START( ll->log_fp );

    if ( length < 0 )
    {
        fsc2_lan_log_message( ll->log_fp, "Error: invalid buffer length: %ld\n",
                              length );
        LOG_FUNCTION_END( ll->log_fp );
        return -1;
    }

    if ( length == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( ll->log_fp, "Warning: premature end since "
                                  "no bytes are to be written\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return 0;
    }

    if ( buffer == NULL )
    {
        fsc2_lan_log_message( ll->log_fp, "Error: invalid buffer argument\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return -1;
    }

    if ( lan_log_level == LL_ALL )
    {
        if ( us_timeout <= 0 )
            fsc2_lan_log_message( ll->log_fp, "%ld bytes to write:\n",
                                  ( long ) length );
        else
            fsc2_lan_log_message( ll->log_fp, " %ld bytes to write "
                                  "within %ld ms to:\n",
                                  ( long ) length, us_timeout / 1000 );
        fsc2_lan_log_data( ll->log_fp, length, buffer );
    }

    /* Deal with setting a timeout - if possible a socket option is used,
       otherwise a timer is started */

    struct sigaction old_sact;
    struct timeval   before;
    timeout_init( WRITE, ll, &us_timeout, &old_sact, &before );

    /* Start writting - if it's interrupted by a signal other than SIGALRM
       and we're supposed to continue on such signals and the timeout time
       hasn't already been reached retry the write() */

    ssize_t bytes_written;
    do
    {
        if ( ll->so_timeo_avail )
            errno = 0;
        bytes_written = write( ll->fd, buffer, length );
    } while (    bytes_written == -1
              && errno == EINTR
              && ! quit_on_signal
              && ! got_sigalrm
              && timeout_reset( WRITE, ll, &us_timeout, &old_sact, &before ) );

    /* Get rid of the timeout machinery */

    if ( us_timeout != 0 )
        timeout_exit( ll, &old_sact );

    if ( bytes_written == -1 )
    {
        if ( errno == EINTR && quit_on_signal && ! got_sigalrm )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: writing aborted due "
                                  "to signal\n" );
            bytes_written = 0;
        }
        else if (    ( ll->so_timeo_avail && errno == EWOULDBLOCK )
                  || (    ! ll->so_timeo_avail
                       && errno == EINTR
                       && got_sigalrm ) )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: writing aborted due "
                                  "to timeout\n" );
            bytes_written = -1;
        }
        else
            fsc2_lan_log_message( ll->log_fp, "Error: failed to write to "
                                  "socket: %s\n", strerror( errno ) );
    }
    else if ( lan_log_level == LL_ALL )
        fsc2_lan_log_message( ll->log_fp, "Wrote %ld bytes\n",
                              ( long ) bytes_written );

    LOG_FUNCTION_END( ll->log_fp );

    return bytes_written;
}


/*---------------------------------------------------------------*
 * Function for writing from a set of buffers to the socket. If
 * 'us_timeout' is set to a positive (non-zero) value it doesn't
 * wait longer than that many microseconds. If 'quit_on_signal'
 * is set it returns immediately when a signal is caught.
 *---------------------------------------------------------------*/

ssize_t
fsc2_lan_writev( int                  handle,
                 const struct iovec * data,
                 int                  count,
                 long                 us_timeout,
                 bool                 quit_on_signal )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    LAN_List_T * ll = find_lan_entry( handle );
    if ( ! ll )
    {
        print( SEVERE, "Invalid handle passed to fsc2_lan_writev().\n" );
        return -1;
    }

    LOG_FUNCTION_START( ll->log_fp );

    if ( count == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( ll->log_fp, "Warning: premature end "
                                  "since no bytes are to be written\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return 0;
    }

    if ( count < 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( ll->log_fp, "Error: invalid number of "
                                  "data buffers\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return 0;
    }

    if ( data == NULL )
    {
        fsc2_lan_log_message( ll->log_fp, "Error: invalid data argument\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return -1;
    }

    size_t length = 0;
    for ( int i = 0; i < count; i++ )
    {
        if ( data[ i ].iov_base == NULL )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: invalid data "
                                  "chunk %d\n", count );
            LOG_FUNCTION_END( ll->log_fp );
            return -1;
        }

        length += data[ i ].iov_len;
    }

    if ( length == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( ll->log_fp, "Warning: premature end "
                                  "since no bytes are to be written\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return 0;
    }

    if ( lan_log_level == LL_ALL )
    {
        if ( us_timeout <= 0 )
            fsc2_lan_log_message( ll->log_fp, "%ld bytes to write (from %d "
                                  "buffers):\n", ( long ) length, count );
        else
            fsc2_lan_log_message( ll->log_fp, "%ld bytes to write (from %d "
                                  "buffers) within %ld ms:\n",
                                  ( long ) length, count, us_timeout / 1000 );

        for ( int i = 0; i < count; i++ )
            fsc2_lan_log_data( ll->log_fp, data[ i ].iov_len,
                               data[ i ].iov_base );
    }

    /* Deal with setting a timeout - if possible a socket option is used,
       otherwise a timer is started */

    struct sigaction old_sact;
    struct timeval before;
    timeout_init( WRITE, ll, &us_timeout, &old_sact, &before );

    /* Start writting - if it's interrupted by a signal other than SIGALRM
       and we are supposed to continue on such signals and the timeout time
       hasn't already been reached retry the write() */

    ssize_t bytes_written;
    do
    {
        if ( ll->so_timeo_avail )
            errno = 0;
        bytes_written = writev( ll->fd, data, count );
    } while (    bytes_written == -1
              && errno == EINTR
              && ! quit_on_signal
              && ! got_sigalrm
              && timeout_reset( WRITE, ll, &us_timeout, &old_sact, &before ) );

    /* Get rid of the timeout machinery */

    if ( us_timeout != 0 )
        timeout_exit( ll, &old_sact );

    if ( bytes_written == -1 )
    {
        if ( errno == EINTR && quit_on_signal && ! got_sigalrm )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: writing aborted due "
                                  "to signal\n" );
            bytes_written = 0;
        }
        else if (    ( ll->so_timeo_avail && errno == EWOULDBLOCK )
                  || (    ! ll->so_timeo_avail
                       && errno == EINTR
                       && got_sigalrm ) )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: writing aborted due "
                                  "to timeout\n" );
            bytes_written = -1;
        }
        else
            fsc2_lan_log_message( ll->log_fp, "Error: failed to write to "
                                  "socket: %s\n", strerror( errno ) );
    }
    else if ( lan_log_level == LL_ALL )
        fsc2_lan_log_message( ll->log_fp, "Wrote %ld bytes\n",
                              ( long ) bytes_written );

    LOG_FUNCTION_END( ll->log_fp );

    return bytes_written;
}


/*----------------------------------------------------------------------*
 * Function for reading from the socket to a buffer. If 'us_timeout' is
 * set to a positive (non-zero) value it doesn't wait longer than that
 * many microseconds. If 'quit_on_signal' is set it returns immediately
 * when a signal is caught.
 *----------------------------------------------------------------------*/

ssize_t
fsc2_lan_read( int    handle,
               char * buffer,
               long   length,
               long   us_timeout,
               bool   quit_on_signal )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    LAN_List_T * ll = find_lan_entry( handle );
    if ( ! ll )
    {
        print( SEVERE, "Invalid handle passed to fsc2_lan_read().\n" );
        return -1;
    }

    LOG_FUNCTION_START( ll->log_fp );

    if ( length == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( ll->log_fp, "Warning: premature end "
                                     "since no bytes are to be read\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return 0;
    }

    if ( length < 0 )
    {
        fsc2_lan_log_message( ll->log_fp, "Error: invalid buffer length: "
                              "%ld\n", length );
        LOG_FUNCTION_END( ll->log_fp );
        return -1;
    }

    if ( buffer == NULL )
    {
        fsc2_lan_log_message( ll->log_fp, "Error: invalid buffer argument\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return -1;
    }

    if ( lan_log_level == LL_ALL )
    {
        if ( us_timeout <= 0 )
            fsc2_lan_log_message( ll->log_fp, "Up to %ld bytes to read\n",
                                  ( long ) length );
        else
            fsc2_lan_log_message( ll->log_fp, "Up to %ld bytes to read "
                                  "within %ld ms\n",
                                  ( long ) length, us_timeout / 1000 );
    }

    /* Deal with setting a timeout - if possible a socket option is used,
       otherwise a timer is started */

    struct sigaction old_sact;
    struct timeval before;
    timeout_init( READ, ll, &us_timeout, &old_sact, &before );

    /* Start reading - if interrupted by a signal other than SIGALRM
       and we're supposed to continue on such signals and the timeout
       time hasn't already been reached retry the read(2) */

    ssize_t bytes_read;
    do
    {
        if ( ll->so_timeo_avail )
            errno = 0;
        bytes_read = read( ll->fd, buffer, length );
    } while (    bytes_read == -1
              && errno == EINTR
              && ! quit_on_signal
              && ! got_sigalrm
              && timeout_reset( READ, ll, &us_timeout, &old_sact, &before ) );

    /* Get rid of the timeout machinery */

    if ( us_timeout != 0 )
        timeout_exit( ll, &old_sact );

    if ( bytes_read == -1 )
    {
        if ( errno == EINTR && quit_on_signal && ! got_sigalrm )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: reading aborted due "
                                  "to signal\n" );
            bytes_read = 0;
        }
        else if (    ( ll->so_timeo_avail && errno == EWOULDBLOCK )
                  || (    ! ll->so_timeo_avail
                       && errno == EINTR
                       && got_sigalrm ) )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: reading aborted due "
                                  "to timeout\n" );
            bytes_read = -1;
        }
        else
            fsc2_lan_log_message( ll->log_fp, "Error: failed to read from "
                                  "socket: %s\n", strerror( errno ));
    }
    else if ( lan_log_level == LL_ALL )
    {
        fsc2_lan_log_message( ll->log_fp, "Read %ld bytes:\n",
                              ( long ) bytes_read );
        fsc2_lan_log_data( ll->log_fp, bytes_read, buffer );
    }

    LOG_FUNCTION_END( ll->log_fp );

    return bytes_read;
}


/*-----------------------------------------------------------------*
 * Function for reading from the socket into a set of buffers. If
 * 'us_timeout' is set to a positive (non-zero) value it doesn't
 * wait longer than that many microseconds. If 'quit_on_signal' is
 * set it returns immediately when a signal is caught.
 * Take care: differing from the behaviour of readv(2) the length
 * fields in the iovec structures get set to the number of bytes
 * read into them (that's why the 'buffer' argument isn't const
 * qualified as it is for the readv(2) fucntion).
 *-----------------------------------------------------------------*/

ssize_t
fsc2_lan_readv( int            handle,
                struct iovec * data,
                int            count,
                long           us_timeout,
                bool           quit_on_signal )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    LAN_List_T * ll = find_lan_entry( handle );
    if ( ! ll )
    {
        print( SEVERE, "Invalid handle passed to fsc2_lan_readv().\n" );
        return -1;
    }

    LOG_FUNCTION_START( ll->log_fp );

    if ( count == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( ll->log_fp, "Warning: premature end "
                                  "since no bytes are to be read\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return 0;
    }

    if ( count < 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( ll->log_fp, "Error: invalid number of "
                                  "data buffers\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return 0;
    }

    if ( data == NULL )
    {
        fsc2_lan_log_message( ll->log_fp, "Error: invalid data argument\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return -1;
    }

    size_t length = 0;
    for ( int i = 0; i < count; i++ )
    {
        if ( data[ i ].iov_base == NULL )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: invalid data buffer %d\n",
                                  count );
            LOG_FUNCTION_END( ll->log_fp );
            return -1;
        }

        length += data[ i ].iov_len;
    }

    if ( length == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( ll->log_fp, "Warning: premature end "
                                  "since no bytes are to be read\n" );
        LOG_FUNCTION_END( ll->log_fp );
        return 0;
    }

    if ( lan_log_level == LL_ALL )
    {
        if ( us_timeout <= 0 )
            fsc2_lan_log_message( ll->log_fp, "Up to %ld bytes to read (into "
                                  "%d buffers)\n", ( long ) length, count );
        else
            fsc2_lan_log_message( ll->log_fp, "Up to %ld bytes to read (into "
                                  "%d buffer) within %ld ms\n",
                                  ( long ) length, count, us_timeout / 1000 );
    }

    /* Deal with setting a timeout - if possible a socket option is used,
       otherwise a timer is started */

    struct sigaction old_sact;
    struct timeval before;
    timeout_init( READ, ll, &us_timeout, &old_sact, &before );

    /* Start writting - if it's interrupted by a signal other than SIGALRM
       and we're supposed to continue on such signals and the timeout time
       hasn't already been reached retry the read() */

    ssize_t bytes_read;
    do
    {
        if ( ll->so_timeo_avail )
            errno = 0;
        bytes_read = readv( ll->fd, data, count );
    } while (    bytes_read == -1
              && errno == EINTR
              && ! quit_on_signal
              && ! got_sigalrm
              && timeout_reset( READ, ll, &us_timeout, &old_sact, &before ) );

    /* Get rid of the timeout machinery */

    if ( us_timeout != 0 )
        timeout_exit( ll, &old_sact );

    if ( bytes_read == -1 )
    {
        if ( errno == EINTR && quit_on_signal && ! got_sigalrm )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: reading aborted due "
                                  "to signal\n" );
            bytes_read = 0;
        }
        else if (    ( ll->so_timeo_avail && errno == EWOULDBLOCK )
                  || (    ! ll->so_timeo_avail
                       && errno == EINTR
                       && got_sigalrm ) )
        {
            fsc2_lan_log_message( ll->log_fp, "Error: reading aborted due "
                                  "to timeout\n" );
            bytes_read = -1;
        }
        else
            fsc2_lan_log_message( ll->log_fp, "Error: failed to read from "
                                  "socket: %s\n", strerror( errno ));

        quit_on_signal = errno == EINTR && ! got_sigalrm;
    }

    if ( bytes_read >= 0 )
    {
        if ( length > ( unsigned long ) bytes_read )
            length = bytes_read;

        int i;
        for ( i = 0; i < count; i++ )
        {
            if ( data[ i ].iov_len > length )
                data[ i ].iov_len = length;
            length -= data[ i ].iov_len;
        }

        for ( ; i < count; i++ )
            data[ i ].iov_len = 0;

        if ( bytes_read > 0 && lan_log_level == LL_ALL )
        {
            fsc2_lan_log_message( ll->log_fp, "Read %ld bytes:\n",
                                  ( long ) bytes_read );

            for ( i = 0; i < count && data[ i ].iov_len != 0; i++ )
                fsc2_lan_log_data( ll->log_fp, data[ i ].iov_len,
                                   data[ i ].iov_base );
        }
    }

    LOG_FUNCTION_END( ll->log_fp );

    return bytes_read;
}


/*--------------------------------------------------*
 * Function for closing all connections and freeing
 * all remaining memory
 *--------------------------------------------------*/

void
fsc2_lan_cleanup( void )
{
    LAN_List_T * ll;
    while ( ( ll = lan_list ) != NULL )
    {
        if ( ll->fd != 0 )
        {
            LOG_FUNCTION_START( ll->log_fp );

            shutdown( ll->fd, SHUT_RDWR );
            if ( close( ll->fd ) == -1 )
                fsc2_lan_log_message( ll->log_fp, "Error: failed to close "
                                      "connection to LAN device %s: %s\n",
                                      ll->name ? ll->name : "?",
                                      strerror( errno ) );
            else
                fsc2_lan_log_message( ll->log_fp, "Closed connection to "
                                      "device %s\n",
                                      ll->name ? ll->name : "?" );
            LOG_FUNCTION_END( ll->log_fp );

            fsc2_lan_close_log( ll->name ? ll->name : "?", ll->log_fp );

            if ( ll->name )
            {
                fsc2_release_uucp_lock( ll->name );
            }
        }

        lan_list = ll->next;
        T_free( ll->name );
        T_free( ll );
    }
}



/*--------------------------------------------------------------------*
 * Function to be called before a read() or write to set up a timeout
 * mechanism. For newer kernels (2.3.41 and newer) can use the socket
 * options SO_RCVTIMEO or SO_SNDTIMEO while for older ones we must
 * do with a timer (which ten also requires installation of a signal
 * handler). The function returns a timeval structure set to the time
 * the function was called at.
 *--------------------------------------------------------------------*/

static
void
timeout_init( int                dir,
              LAN_List_T       * ll,
              long             * us_timeout,
              struct sigaction * old_sact,
              struct timeval   * now )
{
    /* Negative (as well as zero) values for the timeout are interpreted
       to mean no timeout */

    if ( *us_timeout < 0 )
        *us_timeout = 0;

    /* Figure out the current time */

    gettimeofday( now, NULL );

    /* Reset the variable that may tell us later if a SIGALRM came in and
       set up the itimerval structure needed in both the setsockopt()
       (actually, there we only need the 'it_value' timeval structure
       embedded in the itimerval structure) as well as the setitimer() call */

    got_sigalrm = 0;

    struct itimerval iwait;
    iwait.it_value.tv_sec     = *us_timeout / 1000000;
    iwait.it_value.tv_usec    = *us_timeout % 1000000;
    iwait.it_interval.tv_sec  =
    iwait.it_interval.tv_usec = 0;

    /* Now treat the cases where we can either use a socket option or have
       to start a timer (kernels older than 2.3.41) */

    if ( ll->so_timeo_avail )
    {
        /* If the timeout didn't change nothing is to be done, the socket
           option is already set to the correct value */

        if (    ( dir == READ  && *us_timeout == ll->us_read_timeout  )
             || ( dir == WRITE && *us_timeout == ll->us_write_timeout ) )
            return;

        /* Try to set the socket option - if this doesn't work disregard the
           timeout setting, otherwise store the new timeout value */

        if ( setsockopt( ll->fd, SOL_SOCKET,
                         dir == READ ? SO_RCVTIMEO : SO_SNDTIMEO,
                         &iwait.it_value, sizeof iwait.it_value ) == -1 )
            *us_timeout = 0;
        else
        {
            if ( dir == READ )
                ll->us_read_timeout  = *us_timeout;
            else
                ll->us_write_timeout = *us_timeout;
        }
    }
    else
    {
        /* Nothing to be done if no timeout is requested */

        if ( *us_timeout == 0 )
            return;

        /* Try to set handler for SIGALRM signals and then to start a timer,
           on failures the timeout will be disregarded */

        struct sigaction sact;
        sact.sa_handler = wait_alarm_handler;
        sigemptyset( &sact.sa_mask );
        sact.sa_flags = 0;

        if ( sigaction( SIGALRM, &sact, old_sact ) == -1 )
            *us_timeout = 0;
        else
        {
            if ( setitimer( ITIMER_REAL, &iwait, NULL ) == -1 )
            {
                int ret = sigaction( SIGALRM, old_sact, NULL );
                fsc2_assert( ret != -1 );  /* this better doesn't happen */
                *us_timeout = 0;
            }
        }
    }
}


/*--------------------------------------------------------------------*
 * Function to be called if a read() or write() using a timeout is to
 * be resumed. Returns true if resuming is ok, otherwise false.
 *--------------------------------------------------------------------*/

static
bool
timeout_reset( int                dir,
               LAN_List_T       * ll,
               long             * us_timeout,
               struct sigaction * old_sact,
               struct timeval   * before )
{
    /* If no timeout is set simply return, resuming is possible */

    if ( *us_timeout == 0 )
        return true;

    /* Figure out the current time and how long we still may wait */

    struct timeval now;
    gettimeofday( &now, NULL );

    *us_timeout -=   ( now.tv_sec     * 1000000 + now.tv_usec     )
                   - ( before->tv_sec * 1000000 + before->tv_usec );

    /* Stop a possibly running timer */

    timeout_exit( ll, old_sact );

    /* If no time is left return immediately, indicating not to resume a read()
       or write(), otherwise start a new timeout with the remaining time */

    if ( *us_timeout <= 0 )
    {
        *us_timeout = 0;
        return false;
    }

    timeout_init( dir, ll, us_timeout, old_sact, before );

    return true;
}


/*------------------------------------------------------------------*
 * Function for stopping the timout machinery - actually only for
 * cases where a timer got started the function does anything, i.e.
 * stops the timer and resets the SIGALRM signal handler.
 *------------------------------------------------------------------*/

static
void
timeout_exit( LAN_List_T       * ll,
              struct sigaction * old_sact )
{
    /* Nothing to be done if timeouts are dealt with by socket options */

    if ( ll->so_timeo_avail )
        return;

    struct itimerval iwait = { { 0, 0 },
                               { 0, 0 } };
    int ret = setitimer( ITIMER_REAL, &iwait, NULL );
    fsc2_assert( ret != -1 );              /* this better doesn't happen */

    ret = sigaction( SIGALRM, old_sact, NULL );
    fsc2_assert( ret != -1 );              /* this neither... */
}


/*------------------------------------------------------------------*
 * Handler for SIGALRM signals during connect(), write() and read()
 *------------------------------------------------------------------*/

static
void
wait_alarm_handler( int sig_no )
{
    if ( sig_no == SIGALRM )
        got_sigalrm = 1;
}


/*-------------------------------------------*
 * Function for finding an entry in the list
 * of devices from its file descriptor
 *-------------------------------------------*/

static
LAN_List_T *
find_lan_entry( int handle )
{
    LAN_List_T * ll = lan_list;
    while ( ll != NULL )
    {
        if ( ll->fd == handle )
            return ll;

        ll = ll->next;
    }

    return NULL;
}


/*------------------------------------------------------*
 * Function expects a string with an IP address (either
 * as a hostname or as 4 numbers, separated by dots).
 * It tries to resolve the IP address into an 'in_addr'
 * structure. On failure the 's_addr' field of the
 * 'in_addr' structure is set to 0, otherwise to the
 * value that then can be used in a socket() call.
 *------------------------------------------------------*/

static
void
get_ip_address( const char     * address,
                struct in_addr * ip_addr )
{
    /* For the time being we only deal with IPv4 addresses */

    fsc2_assert( sizeof ip_addr->s_addr == 4 );

    /* First try the address string for a dotted-quad format */

    if ( inet_pton( AF_INET, address, ip_addr ) > 0 )
        return;

    /* If this didn't do the trick try to resolve the name via a DNS query */

    struct hostent * he = gethostbyname( address );
    if (    ! he
         || he->h_addrtype != AF_INET
         || *he->h_addr_list == NULL )
        ip_addr->s_addr = 0;
    else
        memcpy( &ip_addr->s_addr, *he->h_addr_list, 4 );
}


/*-----------------------------------------------------------------------*
 * This function is called internally (i.e. not from modules except
 * the VX11 module) before the start of an experiment in order to
 * open the log file.
 * ->
 *  * Pointer to the name of log file - if the pointer is NULL or does
 *    not point to a non-empty string stderr used.
 *  * log level, either LL_NONE, LL_ERR, LL_CE or LL_ALL
 *    (if log level is LL_NONE 'log_file_name' is not used at all)
 * <-
 *    FILE pointer for the log file (might be stderr)
 *-----------------------------------------------------------------------*/

FILE *
fsc2_lan_open_log( const char * dev_name )
{
    if ( lan_log_level <= LL_NONE )
    {
        lan_log_level = LL_NONE;
        return NULL;
    }

    if ( lan_log_level > LL_ALL )
        lan_log_level = LL_ALL;

    char * volatile fname = NULL;
    TRY
    {
#if defined LAN_LOG_DIR
        fname = get_string( "%s%sfsc2_%s.log", LAN_LOG_DIR,
                            slash( LAN_LOG_DIR ), dev_name );
#else
        fname = get_string( P_tmpdir "/fsc2_%s.log", dev_name );
#endif
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
        return NULL;

    raise_permissions( );

    FILE * fp = fopen( fname, "w" );
    if ( ! fp )
    {
        fp = stderr;
        fprintf( stderr, "Can't open log file %s for device %s, using "
                 "stderr instead.\n", fname, dev_name );
    }
    else
    {
        int fd_flags = fcntl( fileno( fp ), F_GETFD );

        if ( fd_flags  < 0 )
            fd_flags = 0;
        fcntl( fileno( fp ), F_SETFD, fd_flags | FD_CLOEXEC );
        chmod( fname, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
    }

    lower_permissions( );

    fsc2_lan_log_message( fp, "Starting logging for LAN device %s\n",
                          dev_name );

    return fp;
}


/*------------------------------------------------------*
 * Closes the LAN log file for the given device
 *------------------------------------------------------*/

FILE *
fsc2_lan_close_log( const char * dev_name,
                    FILE       * fp )
{
    if ( fp )
    {
        fsc2_lan_log_message( fp, "Stopping logging for device %s\n",
                              dev_name );
        if ( fp != stderr )
            fclose( fp );
    }

    return NULL;
}


/*------------------------------------------------*
 *------------------------------------------------*/

static
void
fsc2_lan_log_date( FILE * fp )
{
    if ( fp == NULL )
        return;

    time_t t = time( NULL );

    char tc[ 26 ];
    strcpy( tc, asctime( localtime( &t ) ) );

    tc[ 10 ] = '\0';
    tc[ 19 ] = '\0';
    tc[ 24 ] = '\0';

    struct timeb mt;
    ftime( &mt );

    fprintf( fp, "[%s %s %s.%03d] ", tc, tc + 20, tc + 11, mt.millitm );
}


/*-----------------------------------------------------------*
 * fsc2_lan_log_function_start() logs the call of a function
 * by appending a short message to the log file.
 * ->
 *  * pointer to log file
 *  * name of the function
 *-----------------------------------------------------------*/

void
fsc2_lan_log_function_start( FILE       * fp,
                             const char * function )
{
    if ( fp == NULL || lan_log_level < LL_CE )
        return;

    raise_permissions( );
    fsc2_lan_log_date( fp );
    fprintf( fp, "Call of %s()\n", function );
    fflush( fp );
    lower_permissions( );
}


/*--------------------------------------------------------*
 * fsc2_lan_log_function_end() logs the completion of a
 * function by appending a short message to the log file.
 * ->
 *  * pointer to log file
 *  * name of the function
 *--------------------------------------------------------*/

void
fsc2_lan_log_function_end( FILE       * fp,
                           const char * function )
{
    if ( fp == NULL || lan_log_level < LL_CE )
        return;

    raise_permissions( );
    fsc2_lan_log_date( fp );
    fprintf( fp, "Exit of %s()\n", function );
    fflush( fp );
    lower_permissions( );
}


/*------------------------------------------------*
 * Function for writing a message to the log file
 *------------------------------------------------*/

void
fsc2_lan_log_message( FILE       * fp,
                      const char * fmt,
                      ... )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( fp == NULL || lan_log_level == LL_NONE )
        return;

    raise_permissions( );
    fsc2_lan_log_date( fp );

    va_list ap;
    va_start( ap, fmt );
    vfprintf( fp, fmt, ap );
    va_end( ap );
    fflush( fp );
    lower_permissions( );
}


/*------------------------------------------------*
 *------------------------------------------------*/

void
fsc2_lan_log_data( FILE       * fp,
                   long         length,
                   const char * buffer )
{
    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( fp == NULL || lan_log_level == LL_NONE )
        return;

    raise_permissions( );
    while ( length-- > 0 )
        fputc( *buffer++, fp );
    fputc( '\n', fp );
    fflush( fp );
    lower_permissions( );
}


/*------------------------------------------------*
 *------------------------------------------------*/

int
fsc2_lan_log_level( void )
{
    return lan_log_level;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
