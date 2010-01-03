/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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
 *
 *
 *  This program expects an EDL program on standard input. It will then
 *  read it in, write it to a temporary file and try to start a new
 *  instance of fsc2 to run the script.
 *
 *  The best way to use this program is to create the EDL program to
 *  send to fsc2 and send it to this program, using popen(). If it was
 *  successful can be found out by examining the return status of the
 *  final call of pclose().
 *
 *  Return codes:
 *
 *   0: Everything ok
 *  -1: Internal error
 *   1: fsc2 could not be started
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "fsc2_config.h"


/* Stuff needed if we're still running an old libc */

#if defined IS_STILL_LIBC1
typedef unsigned int socklen_t;
#endif


/* FSC2_SOCKET must be identical to the definition in global.h ! */

#define FSC2_SOCKET  "/tmp/fsc2.uds"

#define MAXLINE      4096


static void make_tmp_file( char * fname );
static void sig_handler( int signo );


static volatile sig_atomic_t Sig_type = 0;

#if defined __GNUC__
#define UNUSED_ARG __attribute__ ((unused))
#else
#define UNUSED_ARG
#endif


/*-----------------------------------------------------------*
 *-----------------------------------------------------------*/

int
main( int    argc    UNUSED_ARG,
      char * argv[ ] )
{
    char fname[ ] = P_tmpdir "/fsc2.edl.XXXXXX";
    const char *av[ 6 ] = { bindir "fsc2", "--delete", "-s", NULL, NULL, NULL };
    int ac = 3;
    char *prog_name;
    pid_t pid;


    make_tmp_file( fname );

    if ( ( prog_name = strrchr( argv[ 0 ], '/' ) ) != NULL )
        prog_name++;
    else
        prog_name = argv[ 0 ];

    if ( ! strcmp( prog_name, "fsc2_start" ) )
        av[ ac++ ] = "-S";
    else if ( ! strcmp( prog_name, "fsc2_test" ) )
        av[ ac++ ] = "-T";
    else if ( ! strcmp( prog_name, "fsc2_iconic_start" ) )
        av[ ac++ ] = "-I";
    else if (    strcmp( prog_name, "fsc2_load" )
              && strcmp( prog_name, "fsc2_connect" ) )
    {
        unlink( fname );
        exit( -1 );
    }

    av[ ac++ ] = fname;

    signal( SIGUSR1, sig_handler );
    signal( SIGCHLD, sig_handler );

    if ( ( pid = fork( ) ) == 0 )
    {
        execvp( av[ 0 ], ( char ** ) av );
        _exit( -1 );                 /* kill child on failed execvp() */
    }

    if ( pid < 0 )                   /* fork failed ? */
    {
        unlink( fname );
        exit( -1 ) ;
    }

    while ( ! Sig_type )
    {
        pause( );                    /* wait for fsc2 to send signal or fail */

        if ( Sig_type == SIGCHLD )   /* fsc2 failed to start */
        {
            unlink( fname );
            exit( 1 );
        }
    }

    if ( argv[ 1 ] && ! strcmp( argv[ 1 ], "-w" ) )
        waitpid( pid, NULL, 0 );

    return 0;
}


/*-----------------------------------------------------------*
 *-----------------------------------------------------------*/

static void
make_tmp_file( char * fname )
{
    int tmp;
    ssize_t bytes_read;
    char line[ MAXLINE ];


    /* Try to open a temporary file */

    if ( ( tmp = mkstemp( fname ) ) < 0 )
        exit( -1 );

    fchmod( tmp, S_IRUSR | S_IWUSR );

    /* Now read in from stdin and write into the temporary file */

    while ( ( bytes_read = read( 0, line, MAXLINE ) ) != 0 )
    {
        if ( bytes_read == -1 )
        {
            if ( errno == EINTR )
                continue;
            else
            {
                close( tmp );
                unlink( fname );
                exit( -1 );
            }
        }

        if ( write( tmp, line, ( size_t ) bytes_read ) != bytes_read )
        {
            close( tmp );
            unlink( fname );
            exit( -1 );
        }
    }

    close( tmp );
}


/*-----------------------------------------------------------*
 *-----------------------------------------------------------*/

static void
sig_handler( int signo )
{
    /* Bomb out on unexpected signals */

    if ( signo != SIGCHLD && signo != SIGUSR1 )
        exit( -1 );

    Sig_type = signo;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
