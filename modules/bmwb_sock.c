/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
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


#include "bmwb.h"


static int listen_fd;
static volatile int client_fd = -1;
static volatile sig_atomic_t client_died = 0;


static void * conn_handler( void * null );

static void * client_handler( void * null );

static int handle_request( int fd );

static int req_compare( const void * what,
                        const void * req );

static int freq_req( int          fd,
                     const char * req );

static int att_req( int          fd,
                    const char * req );

static int sigph_req( int          fd,
                      const char * req );

static int bias_req( int          fd,
                     const char * req );

static int mode_req( int          fd,
                     const char * req );

static int iris_req( int          fd,
                     const char * req );

static int detsig_req( int          fd,
                       const char * req );

static int afcsig_req( int          fd,
                       const char * req );

static int unlvl_req( int          fd,
                      const char * req );

static int uncal_req( int          fd,
                      const char * req );

static int afcst_req( int          fd,
                      const char * req );

static int lck_req( int          fd,
                    const char * req );

static void write_sig_handler( int sig_no );

static ssize_t swrite( int          fd,
                       const char * buf,
                       ssize_t      len );

static ssize_t sread( int       fd,
                      char    * buf,
                      ssize_t   len );

static int finish_with_update( int fd );


/*-------------------------------------------------------*
 * Opens the socket we're going to listen on and creates
 * a thread for dealing with external connections.
 *-------------------------------------------------------*/

int
bmwb_open_sock( void )
{
    struct sockaddr_un serv_addr;
    pthread_attr_t attr;


    raise_permissions( );

    /* Create a UNIX domain socket */

    if ( ( listen_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
    {
        lower_permissions( );
        sprintf( bmwb.error_msg, "Can't create required socket." );
        return 1;
    }

    unlink( P_tmpdir "/bmwb.uds" );
    memset( &serv_addr, 0, sizeof serv_addr );
    serv_addr.sun_family = AF_UNIX;
    strcpy( serv_addr.sun_path, P_tmpdir "/bmwb.uds" );

    umask( 0 );

    if ( bind( listen_fd, ( struct sockaddr * ) &serv_addr,
               sizeof serv_addr ) == -1 )
    {
        close( listen_fd );
        unlink( P_tmpdir "/bmwb.uds" );
        lower_permissions( );
        sprintf( bmwb.error_msg, "Can't bind to socket." );
        return 1;
    }

    if ( listen( listen_fd, 5 ) == -1 )
    {
        close( listen_fd );
        unlink( P_tmpdir "/bmwb.uds" );
        lower_permissions( );
        sprintf( bmwb.error_msg, "Can't listen on socket." );
        return 1;
    }

    /* Create the thread that's responsible for accepting connections to the
       socket */

    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

    if ( pthread_create( &bmwb.a_thread, &attr, conn_handler, NULL ) )
    {
        pthread_attr_destroy( &attr );
        close( listen_fd );
        unlink( P_tmpdir "/bmwb.uds" );
        lower_permissions( );
        sprintf( bmwb.error_msg, "Can't create thread." );
        return 1;
    }

    lower_permissions( );
    pthread_attr_destroy( &attr );
    return 0;
}


/*------------------------------------------------------*
 * Thread function that deals with incoming connections
 *------------------------------------------------------*/

static void *
conn_handler( void * null  UNUSED_ARG )
{
    int fd;
    struct sockaddr_un cli_addr;
    socklen_t cli_len = sizeof cli_addr;
    pthread_attr_t attr;


    bmwb.a_is_active = 1;

    /* Wait for connections and create a new thread for dealing with the
       client. Note that there only can be a single client using the
       bridge, so reject connections from further clients when the
       client handling thread is running. */

    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

    while ( 1 )
    {
        if ( ( fd = accept( listen_fd, ( struct sockaddr * ) &cli_addr,
                            &cli_len ) ) < 0 )
            continue;

        /* If client_fd isn't -1 some other process is still using the
           device, signal this to the caller and close connection */

        if ( client_fd != -1 )
        {
            swrite( fd, "B\n", 2 );         /* "B\n" indicates busy */
            shutdown( fd, SHUT_RDWR );
            close( fd );
            continue;
        }

        /* Set global client file descriptor and create a thread that deals
           with the client */

        client_fd = fd;

        if ( pthread_create( &bmwb.c_thread, &attr, client_handler, NULL ) )
        {
            swrite( fd, "Z\n", 2 );         /* "Z\n" indicates failure */
            shutdown( fd, SHUT_RDWR );
            close( fd );
            client_fd = -1;
        }
    }       

    pthread_attr_destroy( &attr );
    bmwb.a_is_active = 0;
    pthread_exit( NULL );
}


/*-----------------------------------------------------*
 * Handles requests by the client as long as it exists
 *-----------------------------------------------------*/

static void *
client_handler( void * null  UNUSED_ARG )
{
    fd_set fds;
    struct sigaction sact;


    bmwb.c_is_active = 1;

    sact.sa_handler = write_sig_handler;
    sigemptyset( &sact.sa_mask );
    sact.sa_flags = 0;

    if ( sigaction( SIGPIPE, &sact, NULL ) == -1 )
    {
        client_fd = -1;
        bmwb.c_is_active = 0;
        pthread_exit( NULL );
    }

    /* At the very start tell the client which bridge this program thinks
       it's dealing with */

    if ( swrite( client_fd, bmwb.type == Q_BAND ? "Q\n" : "X\n", 2 ) == 2 )
    {
        while ( 1 )
        {
            FD_ZERO( &fds );
            FD_SET( client_fd, &fds );

            /* Wait for next request from the client and deal with it until
               either the client closed the socket or a communication error
               is detected */

            if (    select( client_fd + 1, &fds, NULL, NULL, NULL ) != 1
                 || handle_request( client_fd ) )
                break;
        }
    }

    shutdown( client_fd, SHUT_RDWR );
    close( client_fd );

    pthread_mutex_lock( &bmwb.mutex );
    set_iris( 0 );
    lock_objects( 0 );
    pthread_mutex_unlock( &bmwb.mutex );

    client_fd = -1;

    bmwb.c_is_active = 0;
    pthread_exit( NULL );
}


/*---------------------------------*
 * Handles a request by the client
 *---------------------------------*/

typedef struct {
    const char * req;
    size_t       len;
    int          ( * fnc ) ( int, const char * );
} req_type;


static int
handle_request( int fd )
{
    char buf[ 64 ];
    ssize_t n;
    req_type *res;
    static req_type reqs[ ] =
          { { "AFCSIG?", 7, afcsig_req },   /* get AFC signal */
            { "AFCST?",  6, afcst_req  },   /* get AFC state */
            { "ATT",     3, att_req    },   /* set/get attenuation */
            { "BIAS",    4, bias_req   },   /* set/get microwave bias */
            { "DETSIG?", 7, detsig_req },   /* get detector signal */
            { "FREQ",    4, freq_req   },   /* set /get microwave frequency */
            { "IRIS ",   5, iris_req   },   /* change iris request */
            { "LCK ",    4, lck_req    },   /* set/release lock */
            { "MODE",    4, mode_req   },   /* set/get mode */
            { "SIGPH",   5, sigph_req  },   /* set/get signal phase */
            { "UNCAL?",  6, uncal_req  },   /* get uncalibrated signal */
            { "UNLVL?",  6, unlvl_req  }    /* get unleveled signal */
          };


    if ( ( n = sread( fd, buf, sizeof buf ) ) <= 0 )
        return 1;

    buf[ n - 1 ] = '\0';

    if ( ( res = bsearch( buf, reqs, sizeof( req_type ),
                          sizeof reqs / sizeof *reqs, req_compare ) ) != NULL )
        return res->fnc( fd, buf + res->len );

    fprintf( stderr, "Received unknown command \"%s\"\n", buf );
    return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

static int
req_compare( const void * what,
             const void * req )
{
    return strncasecmp( what, 
                        ( ( req_type * ) req )->req,
                        ( ( req_type * ) req )->len );
} 


/*----------------------------------------------------------------------*
 * Handles a request to return the microwave frequency or set a new one
 *----------------------------------------------------------------------*/

static int
freq_req( int          fd,
          const char * req )
{
    char buf[ 50 ];
    double val;
    char *eptr = NULL;


    if ( req[ 0 ] == '?' && req[ 1 ] == '\0' )
    {
        pthread_mutex_lock( &bmwb.mutex );
        sprintf( buf, "%.5f\n", bmwb.freq );
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, buf, strlen( buf ) ) != -1 ? 0 : 1;
    }

    if ( *req == ' ' )
        val = strtod( req + 1, &eptr );

    if ( ! eptr || eptr == req + 1 || *eptr || val > 1.0 || val < 0.0 )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( set_mw_freq( val ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg,strlen(  bmwb.error_msg ) ) == -1 ?
               1 : 0;
    }

    fl_set_slider_value( bmwb.rsc->freq_slider, val );
    val = bmwb.min_freq + val * ( bmwb.max_freq - bmwb.min_freq );
    sprintf( buf, "(ca. %.3f GHz / %.0f G)", val, FAC * val );
    fl_set_object_label( bmwb.rsc->freq_text, buf );

    return finish_with_update( fd );
}


/*------------------------------------------------------------------------*
 * Handles a request to return the microwave attenuation or set a new one
 *------------------------------------------------------------------------*/

static int
att_req( int          fd,
         const char * req )
{
    char buf[ 20 ];
    long val;
    char *eptr = NULL;


    if ( req[ 0 ] == '?' && req[ 1 ] == '\0' )
    {
        pthread_mutex_lock( &bmwb.mutex );
        sprintf( buf, "%d\n", bmwb.attenuation );
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, buf, strlen( buf ) ) != -1 ? 0 : 1;
    }

    val = strtol( req + 1, &eptr, 10 );

    if (    *req != ' '
         || eptr == req + 1
         || *eptr
         || val > MAX_ATTENUATION
         || val < MIN_ATTENUATION )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( set_mw_attenuation( val ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1 ?
               1 : 0;
    }

    fl_set_counter_value( bmwb.rsc->attenuation_counter, val );

    return finish_with_update( fd );
}


/*---------------------------------------------------------------*
 * Handles a request to return the signal phase or set a new one
 *---------------------------------------------------------------*/

static int
sigph_req( int          fd,
           const char * req )
{
    char buf[ 50 ];
    double val;
    char *eptr = NULL;

    if ( req[ 0 ] == '?' && req[ 1 ] == '\0' )
    {
        pthread_mutex_lock( &bmwb.mutex );
        sprintf( buf, "%.5f\n", bmwb.signal_phase );
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, buf, strlen( buf ) ) != -1 ? 0 : 1;
    }

    if ( *req == ' ' )
        val = strtod( req, &eptr );

    if ( ! eptr || eptr == req || *eptr || val > 1.0 || val < 0.0 )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( set_signal_phase( val ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1 ?
               1 : 0;
    }

    fl_set_slider_value( bmwb.rsc->signal_phase_slider, val );
    sprintf( buf, "Signal phase (%d%%)", irnd( 100.0 * val ) );
    fl_set_object_label( bmwb.rsc->signal_phase_slider, buf );

    return finish_with_update( fd );
}


/*-----------------------------------------------------------------*
 * Handles a request to return the microwave bias or set a new one
 *-----------------------------------------------------------------*/

static int
bias_req( int          fd,
          const char * req )
{
    char buf[ 50 ];
    double val;
    char *eptr = NULL;

    if (  req[ 0 ] == '?' && req[ 1 ] == '\0' )
    {
        pthread_mutex_lock( &bmwb.mutex );
        sprintf( buf, "%.5f\n", bmwb.bias );
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, buf, strlen( buf ) ) != -1 ? 0 : 1;
    }

    if ( *req == ' ' )
        val = strtod( req + 1, &eptr );

    if ( ! eptr || eptr == req + 1 || *eptr || val > 1.0 || val < 0.0 )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( set_mw_bias( val ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1 ?
               1 : 0;
    }

    fl_set_slider_value( bmwb.rsc->bias_slider, val );
    sprintf( buf, "Microwave bias (%d%%)", irnd( 100.0 * val ) );
    fl_set_object_label( bmwb.rsc->bias_slider, buf );

    return finish_with_update( fd );
}


/*-------------------------------------------------------------------*
 * Handles a request to return the mode or switch to a different one
 *-------------------------------------------------------------------*/

static int
mode_req( int          fd,
          const char * req )
{
    const char *buf[ ] = { "STANDBY\n", "TUNE\n", "OPERATE\n" };
    int mode;
    FL_POPUP_ENTRY *e;


    if (  req[ 0 ] == '?' && req[ 1 ] == '\0' )
    {
        pthread_mutex_lock( &bmwb.mutex );
        mode = bmwb.mode;
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, buf[ mode ], strlen( buf[ mode ] ) ) != -1 ? 0 : 1;
    }

    if ( ! strcasecmp( req, " STANDBY" ) )
         mode = MODE_STANDBY;
    else if ( ! strcasecmp( req, " TUNE" ) )
        mode = MODE_TUNE;
    else if ( ! strcasecmp( req, " OPERATE" ) )
        mode = MODE_OPERATE;
    else
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( set_mode( mode ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1 ?
               1 : 0;
    }

    e = fl_get_select_item_by_value( bmwb.rsc->mode_select, mode );
    fl_set_select_item( bmwb.rsc->mode_select, e );
    if ( mode != MODE_TUNE )
        display_mode_pic( mode );

    return finish_with_update( fd );
}


/*-------------------------------------------------------*
 * Handles a request to move the iris for a certain time
 *-------------------------------------------------------*/

static int
iris_req( int          fd,
          const char * req )
{
    int dir;
    long val;
    struct timespec t;
    char *eptr;


    if ( ! strncasecmp( req, "UP ", 3 ) )
    {
        req += 3;
        dir = 1;
    }
    else if ( ! strncasecmp( req, "DOWN ", 5 ) )
    {
        req += 5;
        dir = -1;
    }
    else
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    val = strtol( req, &eptr, 10 );

    if (    eptr == req
         || *eptr
         || val <= 0
         || val > LONG_MAX )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    t.tv_sec  = val / 1000;
    t.tv_nsec = 1000000 * ( val % 1000 );

    pthread_mutex_lock( &bmwb.mutex );

    if ( set_iris( dir ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1 ?
               1 : 0;
    }

    pthread_mutex_unlock( &bmwb.mutex );

    nanosleep( &t, NULL );

    pthread_mutex_lock( &bmwb.mutex );

    if ( set_iris( 0 ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1 ?
               1 : 0;
    }

    return finish_with_update( fd );
}


/*---------------------------------------------------------*
 * Handles a request to return the detector current signal
 *---------------------------------------------------------*/

static int
detsig_req( int          fd,
            const char * req )
{
    double val;
    char buf[ 20 ];


    if ( *req )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( measure_dc_signal( &val ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1 ?
               1 : 0;
    }

    pthread_mutex_unlock( &bmwb.mutex );

    sprintf( buf, "%.5f\n", val );
    return swrite( fd, buf, strlen( buf ) ) == ( ssize_t ) strlen( buf ) ?
           0 : 1;
}


/*--------------------------------------------*
 * Handles a request to return the AFC signal
 *--------------------------------------------*/

static int
afcsig_req( int          fd,
            const char * req )
{
    double val;
    char buf[ 20 ];


    if ( *req )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( measure_afc_signal( &val ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) )== -1 ?
               1 : 0;
    }

    pthread_mutex_unlock( &bmwb.mutex );

    sprintf( buf, "%.5f\n", val );
    return swrite( fd, buf, strlen( buf ) ) == ( ssize_t ) strlen( buf ) ?
           0 : 1;
}


/*-------------------------------------------------*
 * Handles a request to return the unleveled signal
 *-------------------------------------------------*/

static int
unlvl_req( int          fd,
           const char * req )
{
    double val;
    char buf[ 20 ];


    if ( *req )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( measure_unleveled_signal( &val ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1 ?
               1 : 0;
    }

    pthread_mutex_unlock( &bmwb.mutex );

    sprintf( buf, "%.5f\n", val );
    return swrite( fd, buf, strlen( buf ) ) == ( ssize_t ) strlen( buf ) ?
           0 : 1;
}


/*-----------------------------------------------------*
 * Handles a request to return the uncalibrated signal
 *-----------------------------------------------------*/

static int
uncal_req( int          fd,
           const char * req )
{
    double val;
    char buf[ 20 ];


    if ( *req )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( measure_unleveled_signal( &val ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1
               ? 1 : 0;
    }

    pthread_mutex_unlock( &bmwb.mutex );

    sprintf( buf, "%.5f\n", val );
    return swrite( fd, buf, strlen( buf ) ) == ( ssize_t ) strlen( buf ) ?
           0 : 1;
}


/*-------------------------------------------*
 * Handles a request to return the AFC state
 *-------------------------------------------*/

static int
afcst_req( int          fd,
           const char * req )
{
    int val;
    char buf[ 20 ];


    if ( *req )
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;

    pthread_mutex_lock( &bmwb.mutex );

    if ( measure_afc_state( &val ) )
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) ) == -1
               ? 1 : 0;
    }

    pthread_mutex_unlock( &bmwb.mutex );

    sprintf( buf, "%d\n", val );
    return swrite( fd, buf, strlen( buf ) ) == ( ssize_t ) strlen( buf )  ?
           0 : 1;
}


/*------------------------------------------------------------------*
 * Handles a request to lock or unlock the graphical user interface
 *------------------------------------------------------------------*/

static int
lck_req( int          fd,
         const char * req )
{
    pthread_mutex_lock( &bmwb.mutex );

    if (  req[ 0 ] == '1' && req[ 1 ] == '\0' )
        lock_objects( 1 );
    else if (  req[ 0 ] == '0' && req[ 1 ] == '\0' )
        lock_objects( 0 );
    else
    {
        pthread_mutex_unlock( &bmwb.mutex );
        return swrite( fd, "INV\n", 4 ) == 4 ? 0 : 1;
    }

    pthread_mutex_unlock( &bmwb.mutex );

    return swrite( fd, "OK\n", 3 ) == 3 ? 0 : 1;
}


/*------------------------------------------------------*
 * Handler for SIGPIPE signal that may be received when
 * trying to write to the client
 *------------------------------------------------------*/

static void
write_sig_handler( int sig_no )
{
    if ( sig_no == SIGPIPE )
        client_died = 1;
}


/*-----------------------------------------------------------*
 * Writes as many bytes as was asked for to file descriptor,
 * returns the number of bytes on success and -1 otherwise.
 *-----------------------------------------------------------*/

static ssize_t
swrite( int          fd,
        const char * buf,
        ssize_t      len )
{
    ssize_t n = len,
            ret;


    if ( len == 0 )
        return 0;

    client_died = 0;

    do
    {
        if ( ( ret = write( fd, buf, n ) ) < 1 )
        {
            if ( ret == -1 && ( client_died || errno != EINTR ) )
                return -1;
            continue;
        }
        buf += ret;
    } while ( ( n -= ret ) > 0 );

    return len;
}


/*------------------------------------------------------------*
 * Reads as many bytes as was asked for from file descriptor,
 * returns the number of bytes on success and -1 otherwise.
 *------------------------------------------------------------*/

static ssize_t
sread( int       fd,
       char    * buf,
       ssize_t   len )
{
    ssize_t n = len,
            ret;


    if ( len == 0 )
        return 0;

    len = 0;

    do
    {
        if ( ( ret = read( fd, buf, n ) ) < 1 )
        {
            if ( ret == 0 || errno != EINTR )
                return -1;
            continue;
        }
        buf += ret;
        len += ret;
    } while ( ( n -= ret ) > 0 && buf[ -1 ] != '\n' );

    return len;
}
               

/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

static int
finish_with_update( int fd )
{
    pthread_mutex_unlock( &bmwb.mutex );

    update_display( NULL, NULL );

    if ( *bmwb.error_msg )
    {
        int ret = swrite( fd, bmwb.error_msg, strlen( bmwb.error_msg ) );

        *bmwb.error_msg = '\0';
        return ret;
    }

    return swrite( fd, "OK\n", 3 ) == 3 ? 0 : 1;
}
    

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
