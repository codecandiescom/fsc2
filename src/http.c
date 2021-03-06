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

/* Sets the maximum number of lines from the end of the error browser that
   get included into the HTTP page the server sends - avoids possibly
   sending hundreds and hundreds of unimportant warning messages. */

#define MAX_LINES_TO_SEND 30


static void spawn_server( void );
static void http_send_error_browser( int pd );
static void http_send_picture( int pd,
                               int type );


enum {
    HTTP_PARENT_READ = 0,
    HTTP_CHILD_WRITE,
    HTTP_CHILD_READ,
    HTTP_PARENT_WRITE
};


/*------------------------------------------------------------*
 * This function is the callback button for the "WWW" button
 * in the main form for starting and stopping the HTTP server
 *------------------------------------------------------------*/

void
server_callback( FL_OBJECT * obj,
                 long        a  UNUSED_ARG )
{
    char *www_help;


    if ( fl_get_button( obj ) && Fsc2_Internals.http_pid >= 0 )
    {
        if ( Fsc2_Internals.http_pid != 0 )
            return;

        TRY
        {
            spawn_server( );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            fl_set_button( obj, 0 );
            eprint( WARN, false, "Failed to start HTTP server.\n" );
            return;
        }

        www_help = get_string( "Kill the HTTP server (on port %d)\n"
                               "that allows to view fsc2's current\n"
                               "state via the internet.",
                               Fsc2_Internals.http_port );
        fl_set_object_helper( obj, www_help );
        T_free( www_help );
    }
    else
    {
        /* This might get called twice: First when the button gets switched
           off and then again (with the pid set to -1) when the signal handler
           for SIGCHLD gets triggered. */

        if (    ! fl_get_button( obj )
             && Fsc2_Internals.http_pid > 0
             && ! kill( Fsc2_Internals.http_pid, 0 ) )
            kill( Fsc2_Internals.http_pid, SIGTERM );
        else
        {
            Fsc2_Internals.http_pid = 0;
            close( Comm.http_pd[ HTTP_PARENT_READ ] );
            close( Comm.http_pd[ HTTP_PARENT_WRITE ] );

            fl_set_button( obj, 0 );
            www_help = get_string( "Run a HTTP server (on port %d)\n"
                                   "that allows to view fsc2's current\n"
                                   "state via the internet.",
                                   Fsc2_Internals.http_port );
            fl_set_object_helper( obj, www_help );
            T_free( www_help );
        }
    }
}


/*--------------------------------------------------------------------*
 * Function execs the web server as a child process with its standard
 * input and output redirected to two pipes. We're going to check the
 * read end of one of the pipes regularly to see if the server needs
 * some data and then send them via the other pipe.
 *--------------------------------------------------------------------*/

static void
spawn_server( void )
{
    int pr;
    const char *a[ 4 ] = { NULL, NULL, NULL, NULL };


    /* Open pipe for communication with child that's going to be spawned */

    if ( ( pr = pipe( Comm.http_pd ) ) < 0 || pipe( Comm.http_pd + 2 ) < 0 )
    {
        if ( pr == 0 )
        {
            close( Comm.http_pd[ HTTP_PARENT_READ ] );
            close( Comm.http_pd[ HTTP_CHILD_WRITE ] );
        }

        THROW( EXCEPTION );
    }

    if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
        a[ 0 ] = srcdir "fsc2_http_server";
    else
        a[ 0 ] = bindir "fsc2_http_server";

    if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
        a[ 2 ] = lauxdir;
    else
        a[ 2 ] = auxdir;

    TRY
    {
    	a[ 1 ] = get_string( "%d", Fsc2_Internals.http_port );
        TRY_SUCCESS;
	}
    OTHERWISE
    {
        close( Comm.http_pd[ HTTP_PARENT_READ ] );
        close( Comm.http_pd[ HTTP_PARENT_WRITE ] );
        close( Comm.http_pd[ HTTP_CHILD_READ ] );
        close( Comm.http_pd[ HTTP_CHILD_WRITE ] );

        RETHROW;
    }

    /* Start the HTTP server with its standard input and output redirected */

    if ( ( Fsc2_Internals.http_pid = fork( ) ) == 0 )
    {
        close( Comm.http_pd[ HTTP_PARENT_READ ] );
        close( Comm.http_pd[ HTTP_PARENT_WRITE ] );

        dup2( Comm.http_pd[ HTTP_CHILD_READ  ], STDIN_FILENO );
        dup2( Comm.http_pd[ HTTP_CHILD_WRITE ], STDOUT_FILENO );

        close( Comm.http_pd[ HTTP_CHILD_READ ] );
        close( Comm.http_pd[ HTTP_CHILD_WRITE ] );

        execv( a[ 0 ], ( char ** ) a );
        _exit( EXIT_FAILURE );
    }

    T_free( ( char * ) a[ 1 ] );

    close( Comm.http_pd[ HTTP_CHILD_READ  ] );
    close( Comm.http_pd[ HTTP_CHILD_WRITE ] );

    if ( Fsc2_Internals.http_pid < 0 )
    {
        close( Comm.http_pd[ HTTP_PARENT_READ  ] );
        close( Comm.http_pd[ HTTP_PARENT_WRITE ] );

        Fsc2_Internals.http_pid = 0;

        THROW( EXCEPTION );
    }
}


/*------------------------------------------------*
 * This function gets called from an idle handler
 * to check if the web server has asked for data.
 *------------------------------------------------*/

void
http_check( void )
{
    struct timeval tv;
    fd_set rfds;
    char reply[ 2 ];
    char query;


    tv.tv_sec = tv.tv_usec = 0;
    FD_ZERO( &rfds );
    FD_SET( Comm.http_pd[ HTTP_PARENT_READ ], &rfds );

    /* Answer only one request at a time, we don't want the experiment getting
       bogged down just because somebody has fallen asleep on the reload button
       of his browser... */

    if ( select( Comm.http_pd[ HTTP_PARENT_READ ] + 1, &rfds,
                 NULL, NULL, &tv ) > 0 )
    {
        if ( read( Comm.http_pd[ HTTP_PARENT_READ ], &query, 1 ) == 0 )
            return;

        reply[ 1 ] = '\n';

        ssize_t dummy = 0;

        switch ( query )
        {
            case 'S' :                             /* state of the program ? */
                reply[ 0 ]  = ( char ) Fsc2_Internals.state + '0';
                dummy = write( Comm.http_pd[ HTTP_PARENT_WRITE ], reply, 2 );
                break;

            case 'W' :                       /* wich windows are displayed ? */
                if ( ! G.is_init )
                    reply[ 0 ] = '0';
                else
                {
                    if ( G.dim == 1 )
                        reply[ 0 ] = '1';
                    else if ( G.dim == 2 )
                        reply[ 0 ] = ( G_2d.is_cut ? '6' : '2' );
                    else if ( G.dim == 3 )
                        reply[ 0 ] = ( G_2d.is_cut ? '7' : '3' );
                }

                dummy = write( Comm.http_pd[ HTTP_PARENT_WRITE ], reply, 2 );
                break;

            case 'C' :            /* which 2D curve is currently displayed ? */
                reply[ 0 ] = ( char ) ( G_2d.active_curve + 1 ) + '0';
                dummy = write( Comm.http_pd[ HTTP_PARENT_WRITE ], reply, 2 );
                break;

            case 'E' :             /* send the contents of the error browser */
                http_send_error_browser( Comm.http_pd[ HTTP_PARENT_WRITE ] );
                break;

            case 'a' :                         /* create file with 1D window */
                http_send_picture( Comm.http_pd[ HTTP_PARENT_WRITE ], 1 );
                break;

            case 'b' :                         /* create file with 2D window */
                http_send_picture( Comm.http_pd[ HTTP_PARENT_WRITE ], 2 );
                break;

            case 'c' :              /* create file with cross section window */
                http_send_picture( Comm.http_pd[ HTTP_PARENT_WRITE ], 3 );
                break;

#if ! defined NDEBUG
            default :
                fprintf( stderr, "Got stray request ('%c', %d) from http "
                         "server.\n", query, query );
#endif
        }

        if ( dummy == -1 ) { /* silence stupid compiler warning */ }
    }
}


/*----------------------------------------------------------------*
 * Sends the contents of the error browser (or at least the last
 * MAX_LINES_TO_SEND) to the server. The server expects each line
 * to end with a newline character and treats a line consisting
 * of a newline only as the end of the message. Thus we have to
 * look out for empty lines and send an extra  space char for
 * those.
 *----------------------------------------------------------------*/

static void
http_send_error_browser( int pd )
{
    FL_OBJECT *b = GUI.main_form->error_browser;
    const char *l;
    int i = 0;
    char newline = '\n';
    char space = ' ';


    if ( ( i = fl_get_browser_maxline( b ) - MAX_LINES_TO_SEND  ) < 0 )
        i = 0;

    ssize_t dummy = 0;

    while ( ( l = fl_get_browser_line( b, ++i ) ) != NULL )
    {
        if ( *l != '\0' )
            dummy = write( pd, l, strlen( l ) );
        else
            dummy = write( pd, &space, 1 );
        dummy = write( pd, &newline, 1 );
    }

    dummy = write( pd, &newline, 1 );

    if ( dummy == -1 ) { /* silence stupid compiler warning */ }
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

static void
http_send_picture( int pd,
                   int type )
{
    char filename[ ] = P_tmpdir "/fsc2.http.XXXXXX";
    char reply[ 2 ];
    int volatile tmp_fd = -1;


    reply[ 1 ] = '\n';

    ssize_t dummy = 0;

    /* If the server asks for a window that isn't shown anymore we send a '0'
       to indicate that it has to send the "Not available" picture instead of
       the one the client is looking for. */

    if (    ! G.is_init
         || ( type == 1 && G.dim == 2 )
         || ( type == 2 && G.dim == 1 )
         || ( type == 3 && ! G_2d.is_cut ) )
    {
        reply[ 0 ] = '0';
        dummy = write( pd, reply, 2 );
        return;
    }

    /* Try to create a file with a unique name, dump the window into it and
       send the server the filename */

    TRY
    {
        if ( ( tmp_fd = mkstemp( filename ) ) < 0 )
            THROW( EXCEPTION );

        /* Older versions of libc (2.0.6 and earlier) don't set the file
           permissions correctly to 0600 but 0666, so we do it here... */

        chmod( filename, S_IRUSR | S_IWUSR );

        dump_window( type, tmp_fd );

        reply[ 0 ] = '1';
        dummy = write( pd, reply, 2 );
        dummy = write( pd, filename, strlen( filename ) );
        dummy = write( pd, reply + 1, 1 );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( tmp_fd >= 0 )
        {
            unlink( filename );
            close( tmp_fd );
        }

        reply[ 0 ] = '0';
        dummy = write( pd, reply, 2 );
    }

    close( tmp_fd );

    if ( dummy == -1 ) { /* silence stupid compiler warning */ }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
