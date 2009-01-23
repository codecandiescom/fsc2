/*
 *  $Id$
 * 
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

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/timeb.h>


/* Definition of log levels allowed in calls of fsc2_log_exp_init().
   Since they already may have been defined in the GPIB or serial port
   module only define them if they aren't already known */

#if ! defined LL_NONE
#define  LL_NONE  0    /* log nothing */
#endif
#if ! defined LL_ERR
#define  LL_ERR   1    /* log errors only */
#endif
#if ! defined LL_CE
#define  LL_CE    2    /* log function calls and function exits */
#endif
#if ! defined LL_ALL
#define  LL_ALL   3    /* log calls with parameters and function exits */
#endif


/* Local variables */

static LAN_List_T            * lan_list = NULL;
static volatile sig_atomic_t   got_sigalrm;
static int                     lan_log_level = LL_ALL;
static FILE                  * fsc2_lan_log = NULL;


/* Functions used only locally */

static void timeout_init( int                dir,
                          LAN_List_T *       ll,
                          long *             us_timeout,
                          struct sigaction * old_sact,
                          struct timeval   * now );

static bool timeout_reset( int                dir,
                           LAN_List_T *       ll,
                           long *             us_timeout,
                           struct sigaction * old_sact,
                           struct timeval *   before );

static void timeout_exit( LAN_List_T *       ll,
                          struct sigaction * old_sact );

static void wait_alarm_handler( int sig_no  UNUSED_ARG );

static LAN_List_T * find_lan_entry( int handle );

static void get_ip_address( const char *     address,
                            struct in_addr * ip_addr);

static void fsc2_lan_log_date( void );

static void fsc2_lan_log_function_start( const char * function,
                                         const char * dev_name );

static void fsc2_lan_log_function_end( const char * function,
                                       const char * dev_name );

static void fsc2_lan_log_data( long         length,
                               const char * buffer );


/*----------------------------------------------------------*
 * Function for opening a connection to a device on the LAN
 * Returns the sockets file descriptor on success or -1 on
 * failure.
 *----------------------------------------------------------*/

int
fsc2_lan_open( const char * dev_name,
               const char * address,
               int          port,
               long         us_timeout,
               bool         quit_on_signal )
{
    LAN_List_T *       ll = lan_list;
    int                sock_fd;
    struct sockaddr_in dev_addr;
    int                switch_on;
    struct sigaction   sact,
                       old_sact;
    struct itimerval   wait_for_connect;
    int                conn_ret;
    int                ret;
    socklen_t          len;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    fsc2_assert( dev_name != NULL );

    /* Figure out which device connection it's meant for */

    fsc2_lan_log_function_start( "fsc2_lan_open", dev_name );

    /* Try to resolve the address we got from the caller */

    memset( &dev_addr, 0, sizeof dev_addr );
    get_ip_address( address, &dev_addr.sin_addr );
    if ( dev_addr.sin_addr.s_addr == 0 )
    {
        fsc2_lan_log_message( "Error: invalid IP address: %s\n", address );
        fsc2_lan_log_function_end( "fsc2_lan_open", dev_name );
        return -1;
    }

    /* Check that the port number is reasonable */

    if ( port < 0 || port > 0xFFFF )
    {
        fsc2_lan_log_message( "Error: invalid port number: %d\n", port );
        fsc2_lan_log_function_end( "fsc2_lan_open", dev_name );
        return -1;
    }

    /* Check if a connection for the address and port is made for another
       device and print a warning in this case */

    while ( ll != NULL )
    {
        if (    ! memcmp( &ll->address.s_addr, &dev_addr.sin_addr.s_addr, 4 )
             && port == ll->port )
            print( SEVERE, "IP addresses and ports for devices %s and %s "
                   "are identical.\n", ll->name, dev_name );
        ll = ll->next;
    }

    /* Try to create a socket for a TCP connection */

    if ( ( sock_fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
    {
        fsc2_lan_log_message( "Error: failed to create a socket: %s\n",
                              strerror( errno ) );
        fsc2_lan_log_function_end( "fsc2_lan_open", dev_name );
        return -1;
    }

    /* Set the SO_KEEPALIVE option to have a keepalive probe sent after
       two hours of inactivity */

    switch_on = 1;
    if ( setsockopt( sock_fd, SOL_SOCKET, SO_KEEPALIVE,
                     &switch_on, sizeof switch_on ) == -1 )
    {
        close( sock_fd );
        fsc2_lan_log_message( "Error: failed to set SO_KEEPALIVE option for "
                              "socket\n" );
        fsc2_lan_log_function_end( "fsc2_lan_open", dev_name );
        return -1;
    }

    /* Also disable the TCP Nagle algorithm which might slow down response
       times unnecessarily */

    switch_on = 1;
    if ( setsockopt( sock_fd, IPPROTO_TCP, TCP_NODELAY,
                     &switch_on, sizeof switch_on ) == -1 )
    {
        close( sock_fd );
        fsc2_lan_log_message( "Error: failed to set TCP_NODELAY option for "
                              "socket\n" );
        fsc2_lan_log_function_end( "fsc2_lan_open", dev_name );
        return -1;
    }

    /* Set everything up for the connect() call */

    dev_addr.sin_family = AF_INET;
    dev_addr.sin_port = htons( port );

    /* If required make sure we don't wait longer for the connect() call to
       succeed than the user specified time (take care of the case that
       setting up the handler for the SIGALRM signal or starting the timer
       failed, in which case we silently drop the request for a timeout) */

    if ( us_timeout > 0 )
    {
        wait_for_connect.it_value.tv_sec     = us_timeout / 1000000;
        wait_for_connect.it_value.tv_usec    = us_timeout % 1000000;
        wait_for_connect.it_interval.tv_sec  =
        wait_for_connect.it_interval.tv_usec = 0;

        sact.sa_handler = wait_alarm_handler;
        sigemptyset( &sact.sa_mask );
        sact.sa_flags = 0;

        got_sigalrm = 0;

        if ( sigaction( SIGALRM, &sact, &old_sact ) == -1 )
            us_timeout = 0;
        else
        {
            if ( setitimer( ITIMER_REAL, &wait_for_connect, NULL ) == -1 )
            {
                ret = sigaction( SIGALRM, &old_sact, NULL );
                fsc2_assert( ret != -1 );      /* this better doesn't happen */
                us_timeout = 0;
            }
        }
    }

    /* Try to connect to the other side - if connect failed due to a signal
       other than SIGALRM and we are not supposed to return on such signals
       retry the connect() call */
    do
    {
        conn_ret = connect( sock_fd, ( const struct sockaddr * ) &dev_addr,
                            sizeof dev_addr );
    } while (    conn_ret == -1
              && errno == EINTR
              && ! quit_on_signal
              && ! got_sigalrm );

    /* Stop the timer for the timeout and reset the signal handler */

    if ( us_timeout > 0 )
    {
        wait_for_connect.it_value.tv_sec    =
        wait_for_connect.it_value.tv_usec   =
        wait_for_connect.it_interval.tv_sec =
        wait_for_connect.it_interval.tv_usec = 0;

        ret = setitimer( ITIMER_REAL, &wait_for_connect, NULL );

        fsc2_assert( ret != -1 );              /* this better doesn't happen */
        ret = sigaction( SIGALRM, &old_sact, NULL );
        fsc2_assert( ret != -1 );              /* this neither... */
    }

    if ( conn_ret == -1 )
    {
        close( sock_fd );
        if (    us_timeout > 0
             && errno == EINTR
             && quit_on_signal
             && ! got_sigalrm )
        {
            fsc2_lan_log_message( "Error: connect() to socket failed due "
                                  "to signal\n" );
            return 0;
        }
        else if ( us_timeout > 0 && errno == EINTR && got_sigalrm )
        {
            fsc2_lan_log_message( "Error: connect() to socket failed due "
                                  "to timeout\n" );
            return -1;
        }
        else
            fsc2_lan_log_message( "Error: connect() to socket failed: %s\n",
                                  strerror( errno ) );
        fsc2_lan_log_function_end( "fsc2_lan_open", dev_name );
        return -1;
    }

    /* Since everything worked out satisfactory we now add the new connection
       to the list of connections, beginning by adding an entry to the end.
       If this fails due to runnning out of memory there's nothing left we can
       do and all we can do is close all already open connections and throwing
       an exception. */

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

        ll->fd = sock_fd;
        ll->name = NULL;
        ll->name = T_strdup( dev_name );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fsc2_lan_log_message( "Error: Running out of memory\n" );
        fsc2_lan_log_function_end( "fsc2_lan_open", dev_name );
        fsc2_lan_cleanup( );
        RETHROW( );
    }

    ll->next = NULL;

    /* Set the wait time for reads and writes to zero (meaning that none is
       set yet, i.e. the default timeout of the system is going to be used. */

    ll->us_read_timeout = ll->us_write_timeout = 0;
    ll->so_timeo_avail = SET;

    ll->address = dev_addr.sin_addr;
    ll->port    = port;

    /* Check if we really can set a timeout for sending and receiving -
       this only became possible with 2.3.41 kernels */

    if (    getsockopt( sock_fd, SOL_SOCKET, SO_RCVTIMEO,
                        &wait_for_connect.it_value, &len ) == -1
         || getsockopt( sock_fd, SOL_SOCKET, SO_SNDTIMEO,
                        &wait_for_connect.it_value, &len ) == -1 )
        ll->so_timeo_avail = UNSET;

    if ( lan_log_level == LL_ALL )
        fsc2_lan_log_message( "Opened connection to device %s: IP = \"%s\", "
                              "port = %d\n", dev_name, address, port );

    fsc2_lan_log_function_end( "fsc2_lan_open", dev_name );

    return ll->fd;
}


/*------------------------------------------------------------*
 * Function for closing the connection to a device on the LAN
 *------------------------------------------------------------*/

int
fsc2_lan_close( int handle )
{
    LAN_List_T * ll;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    if ( ( ll = find_lan_entry( handle ) ) == NULL )
    {
        fsc2_lan_log_function_start( "fsc2_lan_close", "" );
        fsc2_lan_log_message( "Error: invalid LAN device handle\n" );
        fsc2_lan_log_function_end( "fsc2_lan_close", "" );
        return -1;
    }

    fsc2_lan_log_function_start( "fsc2_lan_close", ll->name );

    /* Try to close the socket - if it fails give the cleanup routine
       that gets automatically called at the end of the experiment
       another chance */

    if ( close( ll->fd ) == -1 )
    {
        fsc2_lan_log_message( "Error: close() failed: %s\n",
                              strerror( errno ) );
        return -1;
    }

    fsc2_lan_log_function_end( "fsc2_lan_close", ll->name );

    if ( ll->prev != NULL )
        ll->prev->next = ll->next;

    if ( ll->next != NULL )
        ll->next->prev = ll->prev;

    if ( ll == lan_list )
        lan_list = ll->next;

    if ( ll->name )
        T_free( ( char * ) ll->name );

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
    LAN_List_T     * ll;
    ssize_t          bytes_written;
    struct timeval   before;
    struct sigaction old_sact;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    if ( ( ll = find_lan_entry( handle ) ) == NULL )
    {
        fsc2_lan_log_function_start( "fsc2_lan_write", "" );
        fsc2_lan_log_message( "Error: invalid LAN device handle\n" );
        fsc2_lan_log_function_end( "fsc2_lan_write", "" );
        return -1;
    }

    fsc2_lan_log_function_start( "fsc2_lan_write", ll->name );

    if ( length < 0 )
    {
        fsc2_lan_log_message( "Error: invalid buffer length: %ld\n", length );
        fsc2_lan_log_function_end( "fsc2_lan_write", ll->name );
        return -1;
    }

    if ( length == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( "Warning: premature end since no bytes are "
                                  "to be written\n" );
        fsc2_lan_log_function_end( "fsc2_lan_write", ll->name );
        return 0;
    }

    if ( buffer == NULL )
    {
        fsc2_lan_log_message( "Error: invalid buffer argument\n" );
        fsc2_lan_log_function_end( "fsc2_lan_write", ll->name );
        return -1;
    }

    if ( lan_log_level == LL_ALL )
    {
        if ( us_timeout <= 0 )
            fsc2_lan_log_message( "Expecting to write %ld byte(s) to LAN "
                                  "device %s:\n", ( long ) length, ll->name );
        else
            fsc2_lan_log_message( "Expecting to write %ld byte(s) within %ld "
                                  "ms to LAN device %s:\n", ( long ) length,
                                  us_timeout / 1000, ll->name );
        fsc2_lan_log_data( length, buffer );
    }

    /* Deal with setting a timeout - if possible a socket option is used,
       otherwise a timer is started */

    timeout_init( WRITE, ll, &us_timeout, &old_sact, &before );

    /* Start writting - if it's interrupted by a signal other than SIGALRM
       and we're supposed to continue on such signals and the timeout time
       hasn't already been reached retry the write() */

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
            fsc2_lan_log_message( "Error: writing aborted due to signal\n" );
            bytes_written = 0;
        }
        else if (    ( ll->so_timeo_avail && errno == EWOULDBLOCK )
                  || (    ! ll->so_timeo_avail
                       && errno == EINTR
                       && got_sigalrm ) )
        {
            fsc2_lan_log_message( "Error: writing aborted due to timeout\n" );
            bytes_written = -1;
        }
        else
            fsc2_lan_log_message( "Error: failed to write to socket: %s\n",
                                  strerror( errno ) );
    }
    else if ( lan_log_level == LL_ALL )
        fsc2_lan_log_message( "Wrote %ld byte(s) to LAN device %s\n",
                              ( long ) bytes_written, ll->name );

    fsc2_lan_log_function_end( "fsc2_lan_write", ll->name );

    return bytes_written;
}


/*----------------------------------------------------------------*
 * Function for writing from a set from buffers to the socket. If
 * 'us_timeout' is set to a positive (non-zero) value it doesn't
 * wait longer than that  many microseconds. If 'quit_on_signal'
 * is set it returns immediately when a signal gets caught.
 *----------------------------------------------------------------*/

ssize_t
fsc2_lan_writev( int                  handle,
                 const struct iovec * data,
                 int                  count,
                 long                 us_timeout,
                 bool                 quit_on_signal )
{
    LAN_List_T       * ll;
    ssize_t            bytes_written;
    struct timeval     before;
    struct sigaction   old_sact;
    int                i;
    size_t             length;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    if ( ( ll = find_lan_entry( handle ) ) == NULL )
    {
        fsc2_lan_log_function_start( "fsc2_lan_writev", "" );
        fsc2_lan_log_message( "Error: invalid LAN device handle\n" );
        fsc2_lan_log_function_end( "fsc2_lan_writev", "" );
        return -1;
    }

    fsc2_lan_log_function_start( "fsc2_lan_writev", ll->name );

    if ( count == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( "Warning: premature end since no bytes are "
                                  "to be written\n" );
        fsc2_lan_log_function_end( "fsc2_lan_writev", ll->name );
        return 0;
    }

    if ( count < 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( "Error: invalid number of data buffers\n" );
        fsc2_lan_log_function_end( "fsc2_lan_writev", ll->name );
        return 0;
    }

    if ( data == NULL )
    {
        fsc2_lan_log_message( "Error: invalid data argument\n" );
        fsc2_lan_log_function_end( "fsc2_lan_writev", ll->name );
        return -1;
    }

    for ( length = 0, i = 0; i < count; i++ )
    {
        if ( data[ i ].iov_base == NULL )
        {
            fsc2_lan_log_message( "Error: invalid data chunk %d\n", count );
            fsc2_lan_log_function_end( "fsc2_lan_writev", ll->name );
            return -1;
        }

        length += data[ i ].iov_len;
    }

    if ( length == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( "Warning: premature end since no bytes are "
                                  "to be written\n" );
        fsc2_lan_log_function_end( "fsc2_lan_writev", ll->name );
        return 0;
    }

    if ( lan_log_level == LL_ALL )
    {
        if ( us_timeout <= 0 )
            fsc2_lan_log_message( "Expecting to write %ld byte(s) (from %d "
                                  "buffers) to LAN device %s:\n",
                                  ( long ) length, count, ll->name );
        else
            fsc2_lan_log_message( "Expecting to write %ld byte(s) (from %d "
                                  "buffers) within %ld ms to LAN device "
                                  "%s:\n", ( long ) length, count,
                                  us_timeout / 1000, ll->name );

        for ( i = 0; i < count; i++ )
            fsc2_lan_log_data( data[ i ].iov_len, data[ i ].iov_base );
    }

    /* Deal with setting a timeout - if possible a socket option is used,
       otherwise a timer is started */

    timeout_init( WRITE, ll, &us_timeout, &old_sact, &before );

    /* Start writting - if it's interrupted by a signal other than SIGALRM
       and we are supposed to continue on such signals and the timeout time
       hasn't already been reached retry the write() */

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
            fsc2_lan_log_message( "Error: writing aborted due to signal\n" );
            bytes_written = 0;
        }
        else if (    ( ll->so_timeo_avail && errno == EWOULDBLOCK )
                  || (    ! ll->so_timeo_avail
                       && errno == EINTR
                       && got_sigalrm ) )
        {
            fsc2_lan_log_message( "Error: writing aborted due to timeout\n" );
            bytes_written = -1;
        }
        else
            fsc2_lan_log_message( "Error: failed to write to socket: %s\n",
                                  strerror( errno ) );
    }
    else if ( lan_log_level == LL_ALL )
        fsc2_lan_log_message( "Wrote %ld byte(s) to LAN device %s\n",
                              ( long ) bytes_written, ll->name );

    fsc2_lan_log_function_end( "fsc2_lan_writev", ll->name );

    return bytes_written;
}


/*----------------------------------------------------------------------*
 * Function for reading from the socket to a buffer. If 'us_timeout' is
 * set to a positive (non-zero) value it doesn't wait longer than that
 * many microseconds. If 'quit_on_signal' is set it returns immediately
 * if a signal gets caught.
 *----------------------------------------------------------------------*/

ssize_t
fsc2_lan_read( int    handle,
               char * buffer,
               long   length,
               long   us_timeout,
               bool   quit_on_signal )
{
    LAN_List_T       * ll;
    ssize_t            bytes_read;
    struct timeval     before;
    struct sigaction   old_sact;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    if ( ( ll = find_lan_entry( handle ) ) == NULL )
    {
        fsc2_lan_log_function_start( "fsc2_lan_read", "" );
        fsc2_lan_log_message( "Error: invalid LAN device handle\n" );
        fsc2_lan_log_function_end( "fsc2_lan_read", "" );
        return -1;
    }

    fsc2_lan_log_function_start( "fsc2_lan_read", ll->name );

    if ( length == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( "Warning: premature end since no bytes are "
                                  "to be read\n" );
        fsc2_lan_log_function_end( "fsc2_lan_read", ll->name );
        return 0;
    }

    if ( length < 0 )
    {
        fsc2_lan_log_message( "Error: invalid buffer length: %ld\n", length );
        fsc2_lan_log_function_end( "fsc2_lan_read", ll->name );
        return -1;
    }

    if ( buffer == NULL )
    {
        fsc2_lan_log_message( "Error: invalid buffer argument\n" );
        fsc2_lan_log_function_end( "fsc2_lan_read", ll->name );
        return -1;
    }

    if ( lan_log_level == LL_ALL )
    {
        if ( us_timeout <= 0 )
            fsc2_lan_log_message( "Expecting to read up to %ld byte(s) from "
                                  "LAN device %s\n",
                                  ( long ) length, ll->name );
        else
            fsc2_lan_log_message( "Expecting to read up to %ld byte(s) "
                                  "within %ld ms from LAN device %s\n",
                                  ( long ) length, us_timeout / 1000,
                                  ll->name );
    }

    /* Deal with setting a timeout - if possible a socket option is used,
       otherwise a timer is started */

    timeout_init( READ, ll, &us_timeout, &old_sact, &before );

    /* Start reading - if it's interrupted by a signal other than SIGALRM
       and we're supposed to continue on such signals and the timeout time
       hasn't already been reached retry the read() */

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
            fsc2_lan_log_message( "Error: reading aborted due to signal\n" );
            bytes_read = 0;
        }
        else if (    ( ll->so_timeo_avail && errno == EWOULDBLOCK )
                  || (    ! ll->so_timeo_avail
                       && errno == EINTR
                       && got_sigalrm ) )
        {
            fsc2_lan_log_message( "Error: reading aborted due to timeout\n" );
            bytes_read = -1;
        }
        else
            fsc2_lan_log_message( "Error: failed to read from socket: %s\n",
                                  strerror( errno ));
    }
    else if ( lan_log_level == LL_ALL )
    {
        fsc2_lan_log_message( "Read %ld byte(s) from LAN device %s:\n",
                              ( long ) bytes_read, ll->name );
        fsc2_lan_log_data( bytes_read, buffer );
    }

    fsc2_lan_log_function_end( "fsc2_lan_read", ll->name );

    return bytes_read;
}


/*-----------------------------------------------------------------*
 * Function for reading from the socket into a set of buffers. If
 * 'us_timeout' is set to a positive (non-zero) value it doesn't
 * wait longer than that many microseconds. If 'quit_on_signal' is
 * set it returns immediately if a signal gets caught.
 * Take care: differing from the behaviour of readv() the length
 * fields in the iovec structures get set to the number of bytes
 * read into them (that's why the 'buffer' argument isn't declared
 * as const as it is for the readv() fucntion).
 *-----------------------------------------------------------------*/

ssize_t
fsc2_lan_readv( int            handle,
                struct iovec * data,
                int            count,
                long           us_timeout,
                bool           quit_on_signal )
{
    LAN_List_T       * ll;
    ssize_t            bytes_read;
    struct timeval     before;
    struct sigaction   old_sact;
    int                i;
    size_t             length;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode == EXPERIMENT );

    /* Figure out which device connection it's meant for */

    if ( ( ll = find_lan_entry( handle ) ) == NULL )
    {
        fsc2_lan_log_function_start( "fsc2_lan_readv", "" );
        fsc2_lan_log_message( "Error: invalid LAN device handle\n" );
        fsc2_lan_log_function_end( "fsc2_lan_readv", "" );
        return -1;
    }

    fsc2_lan_log_function_start( "fsc2_lan_readv", ll->name );

    if ( count == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( "Warning: premature end since no bytes are "
                                  "to be read\n" );
        fsc2_lan_log_function_end( "fsc2_lan_readv", ll->name );
        return 0;
    }

    if ( count < 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( "Error: invalid number of data buffers\n" );
        fsc2_lan_log_function_end( "fsc2_lan_readv", ll->name );
        return 0;
    }

    if ( data == NULL )
    {
        fsc2_lan_log_message( "Error: invalid data argument\n" );
        fsc2_lan_log_function_end( "fsc2_lan_readv", ll->name );
        return -1;
    }

    for ( length = 0, i = 0; i < count; i++ )
    {
        if ( data[ i ].iov_base == NULL )
        {
            fsc2_lan_log_message( "Error: invalid data buffer %d\n", count );
            fsc2_lan_log_function_end( "fsc2_lan_readv", ll->name );
            return -1;
        }

        length += data[ i ].iov_len;
    }

    if ( length == 0 )
    {
        if ( lan_log_level == LL_ALL )
            fsc2_lan_log_message( "Warning: premature end since no bytes are "
                                  "to be read\n" );
        fsc2_lan_log_function_end( "fsc2_lan_readv", ll->name );
        return 0;
    }

    if ( lan_log_level == LL_ALL )
    {
        if ( us_timeout <= 0 )
            fsc2_lan_log_message( "Expecting to read up to %ld byte(s) (into "
                                  "%d buffers) from LAN device %s\n",
                                  ( long ) length, count, ll->name );
        else
            fsc2_lan_log_message( "Expecting to read up to %ld byte(s) (into "
                                  "%d buffer) within %ld ms from LAN device "
                                  "%s\n", ( long ) length, count,
                                  us_timeout / 1000, ll->name );
    }

    /* Deal with setting a timeout - if possible a socket option is used,
       otherwise a timer is started */

    timeout_init( READ, ll, &us_timeout, &old_sact, &before );

    /* Start writting - if it's interrupted by a signal other than SIGALRM
       and we're supposed to continue on such signals and the timeout time
       hasn't already been reached retry the read() */

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
            fsc2_lan_log_message( "Error: reading aborted due to signal\n" );
            bytes_read = 0;
        }
        else if (    ( ll->so_timeo_avail && errno == EWOULDBLOCK )
                  || (    ! ll->so_timeo_avail
                       && errno == EINTR
                       && got_sigalrm ) )
        {
            fsc2_lan_log_message( "Error: reading aborted due to timeout\n" );
            bytes_read = -1;
        }
        else
            fsc2_lan_log_message( "Error: failed to read from socket: %s\n",
                                  strerror( errno ));

        quit_on_signal = errno == EINTR && ! got_sigalrm;
    }

    if ( bytes_read >= 0 )
    {
        if ( length > ( unsigned long ) bytes_read )
            length = bytes_read;

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
            fsc2_lan_log_message( "Read %ld byte(s) from LAN device %s:\n",
                                  ( long ) bytes_read, ll->name );

            for ( i = 0; i < count && data[ i ].iov_len != 0; i++ )
                fsc2_lan_log_data( data[ i ].iov_len, data[ i ].iov_base );
        }
    }

    fsc2_lan_log_function_end( "fsc2_lan_readv", ll->name );

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
    int          failed = 0;


    fsc2_lan_log_message( "Call of fsc2_lan_cleanup()\n" );

    while ( ( ll = lan_list ) != NULL )
    {
        if ( ll->fd != 0 && close( ll->fd ) == -1 )
        {
            fsc2_lan_log_message( "Error: failed to close connection to "
                                  "LAN device %s: %s\n",
                                  ll->name != NULL ? ll->name : "?",
                                  strerror( errno ) );
            failed++;
        }

        lan_list = ll->next;

        if ( ll->name )
            T_free( ( char * ) ll->name );

        T_free( ll );
    }

    if ( fsc2_lan_log != NULL )
    {
        if ( failed == 0 )
            fsc2_lan_log_message( "Closed all LAN port connections\n" );
        raise_permissions( );
        fsc2_lan_log_message( "Exit of fsc2_lan_cleanup()\n" );
        if ( fsc2_lan_log != stderr )
            fclose( fsc2_lan_log );
        fsc2_lan_log = NULL;
        lower_permissions( );
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

static void
timeout_init( int                dir,
              LAN_List_T *       ll,
              long *             us_timeout,
              struct sigaction * old_sact,
              struct timeval   * now )
{
    struct sigaction sact;
    struct itimerval iwait;
    int              ret;


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

        sact.sa_handler = wait_alarm_handler;
        sigemptyset( &sact.sa_mask );
        sact.sa_flags = 0;

        if ( sigaction( SIGALRM, &sact, old_sact ) == -1 )
            *us_timeout = 0;
        else
        {
            if ( setitimer( ITIMER_REAL, &iwait, NULL ) == -1 )
            {
                ret = sigaction( SIGALRM, old_sact, NULL );
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

static bool
timeout_reset( int                dir,
               LAN_List_T *       ll,
               long *             us_timeout,
               struct sigaction * old_sact,
               struct timeval *   before )
{
    struct timeval now;


    /* If no timeout is set simply return, resuming is possible */

    if ( *us_timeout == 0 )
        return SET;

    /* Figure out the current time and how long we still may wait */

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
        return UNSET;
    }

    timeout_init( dir, ll, us_timeout, old_sact, before );

    return SET;
}


/*------------------------------------------------------------------*
 * Function for stopping the timout machinery - actually only for
 * cases where a timer got started the function does anything, i.e.
 * stops the timer and resets the SIGALRM signal handler.
 *------------------------------------------------------------------*/

static void
timeout_exit( LAN_List_T *       ll,
              struct sigaction * old_sact )
{
    struct itimerval iwait = { { 0, 0 }, { 0, 0 } };
    int              ret;


    /* Nothing to be done if timeouts are dealt with by socket options */

    if ( ! ll->so_timeo_avail )
        return;

    ret = setitimer( ITIMER_REAL, &iwait, NULL );
    fsc2_assert( ret != -1 );              /* this better doesn't happen */
    ret = sigaction( SIGALRM, old_sact, NULL );
    fsc2_assert( ret != -1 );              /* this neither... */
}
                          

/*------------------------------------------------------------------*
 * Handler for SIGALRM signals during connect(), write() and read()
 *------------------------------------------------------------------*/

static void
wait_alarm_handler( int sig_no  UNUSED_ARG )
{
    if ( sig_no == SIGALRM )
        got_sigalrm = 1;
}


/*-------------------------------------------*
 * Function for finding an entry in the list
 * of devices from its file descriptor
 *-------------------------------------------*/

static LAN_List_T *
find_lan_entry( int handle )
{
    LAN_List_T * ll = lan_list;


    while ( ll != NULL )
        if ( ll->fd == handle )
            return ll;

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

static void
get_ip_address( const char *     address,
                struct in_addr * ip_addr )
{
    struct hostent * he;


    /* For the time being we only deal with IPv4 addresses */

    fsc2_assert( sizeof ip_addr->s_addr == 4 );

    /* First try the address string for a dotted-quad format */

    if ( inet_pton( AF_INET, address, ip_addr ) > 0 )
        return;

    /* If this didn't do the trick try to resolve the name via a DNS query */

    if (    ( he = gethostbyname( address ) ) == NULL
         || he->h_addrtype != AF_INET
         || *he->h_addr_list == NULL )
        ip_addr->s_addr = 0;
    else
        memcpy( &ip_addr->s_addr, *he->h_addr_list, 4 );
}


/*-----------------------------------------------------------------------*
 * This function is called internally (i.e. not from modules) before the
 * start of an experiment in order to open the log file.
 * ->
 *  * Pointer to the name of log file - if the pointer is NULL or does
 *    not point to a non-empty string stderr used.
 *  * log level, either LL_NONE, LL_ERR, LL_CE or LL_ALL
 *    (if log level is LL_NONE 'log_file_name' is not used at all)
 *-----------------------------------------------------------------------*/

void
fsc2_lan_exp_init( const char * log_file_name,
                   int          log_level )
{
    lan_log_level = log_level;

    if ( lan_log_level < LL_NONE )
    {
        lan_log_level = LL_NONE;
        return;
    }

    if ( lan_log_level > LL_ALL )
        lan_log_level = LL_ALL;

    raise_permissions( );

    if ( log_file_name == NULL || *log_file_name == '\0' )
    {
        fsc2_lan_log = stderr;
        fprintf( stderr, "LAN port log file not specified, using stderr "
                 "instead\n" );
    }
    else
    {
        if ( ( fsc2_lan_log = fopen( log_file_name, "w" ) ) == NULL )
        {
            fsc2_lan_log = stderr;
            fprintf( stderr, "Can't open LAN port log file %s, using "
                     "stderr instead.\n", log_file_name );
        }
        else
            chmod( log_file_name,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
    }

    lower_permissions( );

    fsc2_lan_log_message( "Starting LAN port logging\n" );
}


/*------------------------------------------------------*
 * fsc2_lan_log_date() writes the date to the log file.
 *------------------------------------------------------*/

static void
fsc2_lan_log_date( void )
{
    char         tc[ 26 ];
    struct timeb mt;
    time_t       t;


    t = time( NULL );
    strcpy( tc, asctime( localtime( &t ) ) );
    tc[ 10 ] = '\0';
    tc[ 19 ] = '\0';
    tc[ 24 ] = '\0';
    ftime( &mt );
    fprintf( fsc2_lan_log, "[%s %s %s.%03d] ", tc, tc + 20, tc + 11,
             mt.millitm );
}


/*-----------------------------------------------------------*
 * fsc2_lan_log_function_start() logs the call of a function
 * by appending a short message to the log file.
 * ->
 *  * name of the function
 *  * name of the device involved
 *-----------------------------------------------------------*/

static void
fsc2_lan_log_function_start( const char * function,
                             const char * dev_name )
{
    if ( fsc2_lan_log == NULL || lan_log_level < LL_CE )
        return;

    raise_permissions( );
    fsc2_lan_log_date( );
    fprintf( fsc2_lan_log, "Call of %s(), dev = %s\n", function, dev_name );
    fflush( fsc2_lan_log );
    lower_permissions( );
}


/*--------------------------------------------------------*
 * fsc2_lan_log_function_end() logs the completion of a
 * function by appending a short message to the log file.
 * ->
 *  * name of the function
 *  * name of the device involved
 *--------------------------------------------------------*/

static void
fsc2_lan_log_function_end( const char * function,
                           const char * dev_name )
{
    if ( fsc2_lan_log == NULL || lan_log_level < LL_CE )
        return;

    raise_permissions( );
    fsc2_lan_log_date( );
    fprintf( fsc2_lan_log, "Exit of %s(), dev = %s\n", function, dev_name );
    fflush( fsc2_lan_log );
    lower_permissions( );
}


/*------------------------------------------------*
 * Function for writing a message to the log file
 *------------------------------------------------*/

void
fsc2_lan_log_message( const char * fmt,
                      ... )
{
    va_list ap;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( fsc2_lan_log == NULL || lan_log_level == LL_NONE )
        return;

    raise_permissions( );
    fsc2_lan_log_date( );
    va_start( ap, fmt );
    vfprintf( fsc2_lan_log, fmt, ap );
    va_end( ap );
    fflush( fsc2_lan_log );
    lower_permissions( );
}


/*------------------------------------------------*
 *------------------------------------------------*/

static void
fsc2_lan_log_data( long length, const char *buffer )
{
    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( fsc2_lan_log == NULL || lan_log_level == LL_NONE )
        return;

    raise_permissions( );
    while ( length-- > 0 )
        fputc( *buffer++, fsc2_lan_log );
    fputc( '\n', fsc2_lan_log );
    fflush( fsc2_lan_log );
    lower_permissions( );
}
    


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
