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
#include <dirent.h>
#include <sys/un.h>


/* Stuff needed if we're still running an old libc */

#if defined IS_STILL_LIBC1
typedef unsigned int socklen_t;
#endif


/* Number of seconds after which the "daemon" checks if some or all
   instances of fsc2 have exited */

#define FSC2D_CHECK_TIME  5


#define FSC2_MAX_INSTANCES 128

#define MAX_SOCK_FILE_LEN  512


static volatile sig_atomic_t Fsc2d_replied;
static int Cwd_ok;

static pid_t instances[ FSC2_MAX_INSTANCES ];
static size_t num_instances = 0;


static int connect_to_fsc2d( void );
static int start_fsc2d( FILE * in_file_fp );
static void fsc2d_sig_handler( int signo );
static void fsc2d( int fd );
static void check_instances( int is_new_connect );
static bool is_tmp_edl( const char * name );
static void new_client( int fd );
static void set_fs2d_signals( void );


/*--------------------------------------------------------------------*
 * Function talks to the daemon process. If the daemon doesn't exist
 * it gets created. The function returns -1 when the current instance
 * isn't allowed to run, 0 when it's allowed to run but may not start
 * a child listening for external connections and 1 when it may run
 * and must start a child process for external connections.
 *--------------------------------------------------------------------*/

pid_t
check_spawn_fsc2d( FILE * in_file_fp )
{
    int sock_fd;
    char line[ MAX_LINE_LENGTH ];


    /* If there's no fsc2 "daemon" process running yet start one. */

    if ( ( sock_fd = connect_to_fsc2d( ) ) == 0 )
        return start_fsc2d( in_file_fp );

    if ( sock_fd == -1 )
    {
        fprintf( stderr, "Can't start fsc2 for unknown reasons.\n" );
        return -1;
    }

    /* Send the PID, the daemon must reply with "OK\n" */

    snprintf( line, sizeof line, "%ld\n", ( long ) getpid( ) );
    if (    writen( sock_fd, line, strlen( line ) ) !=
                                                     ( ssize_t ) strlen( line )
         || read_line( sock_fd, line, sizeof line ) != 3
         || strncmp( line, "OK\n", 3 ) )
    {
        fprintf( stderr, "Can't start fsc2 for unknown reasons.\n" );
        close( sock_fd );
        return -1;
    }

    close( sock_fd );
    return 0;
}


/*-----------------------------------------------------------------*
 * Try to get a connection to the socket the "daemon" is listening
 * on. If this fails we must conclude that there's no "daemon" yet
 * and we have to create it.
 *-----------------------------------------------------------------*/

static int
connect_to_fsc2d( void )
{
    int sock_fd;
    struct sockaddr_un serv_addr;


    /* Try to get a socket (but first make sure the name isn't too long) */

    if (    sizeof P_tmpdir + 8 > sizeof serv_addr.sun_path
         || ( sock_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
        return -1;

    memset( &serv_addr, 0, sizeof serv_addr );
    serv_addr.sun_family = AF_UNIX;
    strcpy( serv_addr.sun_path, P_tmpdir "/fsc2.uds" );

    /* If connect fails due to connection refused or because there's no socket
       file (or it exists but isn't a socket file) it means fsc2d isn't running
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


/*----------------------------------------------------------*
 * Try to create the "daemon" process that new instances of
 * fsc2 must get permission from before they may start.
 *----------------------------------------------------------*/

static int
start_fsc2d( FILE * in_file_fp )
{
    pid_t pid;
    int listen_fd;
    struct sockaddr_un serv_addr;
    mode_t old_mask;
    struct sigaction sact;
    struct sigaction old_sact;


    raise_permissions( );

    /* Create UNIX domain socket */

    if ( ( listen_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
        return -1;

    unlink( P_tmpdir "/fsc2.uds" );
    memset( &serv_addr, 0, sizeof serv_addr );
    serv_addr.sun_family = AF_UNIX;
    strcpy( serv_addr.sun_path, P_tmpdir "/fsc2.uds" );

    old_mask = umask( 0 );

    if ( bind( listen_fd, ( struct sockaddr * ) &serv_addr,
               sizeof serv_addr ) == -1 )
    {
        umask( old_mask );
        close( listen_fd );
        unlink( P_tmpdir "/fsc2.uds" );
        lower_permissions( );
        return -1;
    }

    umask( old_mask );

    if ( listen( listen_fd, SOMAXCONN ) == -1 )
    {
        close( listen_fd );
        unlink( P_tmpdir "/fsc2.uds" );
        lower_permissions( );
        return -1;
    }

    /* Set handler for a signal from the "daemon" that tells us when the
       "daemon" has finished its initialization */

    Fsc2d_replied = 0;

    sact.sa_handler = fsc2d_sig_handler;
    sigemptyset( &sact.sa_mask );
    sact.sa_flags = 0;
    if ( sigaction( SIGUSR2, &sact, &old_sact ) == -1 )
    {
        unlink( P_tmpdir "/fsc2.uds" );
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

        fsc2d( listen_fd );
        _exit( 0 );
    }

    close( listen_fd );

    if ( pid < 0 )
    {
        unlink( P_tmpdir "/fsc2.uds" );
        lower_permissions( );
        sigaction( SIGUSR2, &old_sact, NULL );
        return -1;
    }

    lower_permissions( );

    while ( ! Fsc2d_replied )
    {
        if ( waitpid( pid, NULL, WNOHANG ) )
            return -1;
        fsc2_usleep( 20000, SET );
    }

    sigaction( SIGUSR2, &old_sact, NULL );

    return 1;
}


/*--------------------------------------------------------------*
 * Signal handler for the main program for signal by the daemon
 *--------------------------------------------------------------*/

static void
fsc2d_sig_handler( int signo )
{
    if ( signo == SIGUSR2 )
        Fsc2d_replied = 1;
}


/*-------------------------------------------------------------------------*
 * The following is the code run by the "daemon". It waits for incoming
 * connections by new instances of fsc2 and tells them if they are allowed
 * to run. The "daemon" also reguarly checks for instances to have exited
 * and, if the last instance has died deletes possibly left-over shared
 * memory segments and then also quits.
 *-------------------------------------------------------------------------*/

static void
fsc2d( int fd )
{
    fd_set fds;
    struct timeval timeout;
    int sr;


    /* Ignore all signals */

    set_fs2d_signals( );

    /* Set up the first entry in the list of instances */

    num_instances = 1;
    instances[ 0 ] = getppid( );

    /* We're running with the default temporary directory as our current
       working directory */

    Cwd_ok = chdir( P_tmpdir );

    /* We're done with initialization, send signal to parent */

    kill( instances[ 0 ], SIGUSR2 );

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

        check_instances( sr );

        if ( sr == 1 )
            new_client( fd );
    }
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static void
check_instances( int is_new_connect )
{
    pid_t *ip = instances;
    size_t i = 0;
    DIR *dir;
    struct dirent *ent;


    /* Check if instances of fsc2 are still alive */

    while ( i < num_instances )
    {
        if ( kill( *ip, 0 ) == -1 && errno == ESRCH )
        {

            if ( i < --num_instances )
                memcpy( ip, ip + 1, ( num_instances - i ) * sizeof *ip );

            continue;
        }

        ip++;
        i++;
    }

    if ( num_instances == 0 )
    {
        /* If there are no instances left remove shared memory segments which
           might have leaked */

        delete_stale_shms( );

        /* We're done when the last instance of fsc2 is dead and there's not
           already a new instance trying to talk to us. In this case left-over
           temporary EDL files and sockets are deleted as well as our own
           socket file */

        if ( ! is_new_connect )
        {
            if ( Cwd_ok == 0 && ( dir = opendir( "." ) ) != NULL )
            {
                while ( ( ent = readdir( dir ) ) != NULL )
                    if ( is_tmp_edl( ent->d_name ) )
                        unlink( ent->d_name );

                closedir( dir );
            }

            unlink( P_tmpdir "/fsc2.uds" );

            exit( EXIT_SUCCESS );
        }
    }
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static bool
is_tmp_edl( const char * name )
{
    return    strlen( name ) == 15
           && ! strncmp( name, "fsc2.edl.", 9 );
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static void
new_client( int fd )
{
    struct sockaddr_un cli_addr;
    socklen_t cli_len = sizeof cli_addr;
    int cli_fd;
    char line[ MAX_LINE_LENGTH ];
    long pid;
    ssize_t len;
    char *eptr;
    size_t i;


    if ( ( cli_fd = accept( fd, ( struct sockaddr * ) &cli_addr,
                            &cli_len ) ) < 0 )
        return;

    /* Read line with the PID of the connecting process */

    if ( ( len = read_line( cli_fd, line, MAX_LINE_LENGTH ) ) <= 2 )
    {
        close( cli_fd );
        return;
    }

    pid = strtol( line, &eptr, 10 );

    if (    num_instances == FSC2_MAX_INSTANCES
         || eptr == line
         || *eptr != '\n'
         || pid < 0
         || ( pid == LONG_MAX && errno == ERANGE ) )
    {
        writen( cli_fd, "FAIL\n", 5 );
        close( cli_fd );
        return;
    }

    /* Check if it's not already in the list */

    for ( i = 0; i < num_instances; i++ )
        if ( instances[ i ] == pid )
            break;

    if ( i != num_instances )
        instances[ num_instances++ ] = pid;

    writen( cli_fd, "OK\n", 3 );
    close( cli_fd );
}


/*------------------------------------------------------------------*
 * Signal handler for the "demon" process: all signals are ignored.
 *------------------------------------------------------------------*/

static void
set_fs2d_signals( void )
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
