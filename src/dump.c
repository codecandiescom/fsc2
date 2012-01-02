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


#include "fsc2.h"
#include <dlfcn.h>


/* The functions defined here are only needed in builts where NDEBUG
   and ADDR2LINE are defined ! */

#if ! defined NDEBUG && defined ADDR2LINE

static int write_dump( int  * pipe_fd,
                       FILE * fp,
                       int    k,
                       void * addr );

enum {
    DUMP_PARENT_READ = 0,
    DUMP_CHILD_WRITE,
    DUMP_CHILD_READ,
    DUMP_PARENT_WRITE
};


/*--------------------------------------------------------*
 * Function creates a new process that accepts file names
 * and addresses in that file and returns function name
 * and line number. This new process then gets passed the
 * data from a backtrace created after a crash and the
 * results get written to a file which gets passed to the
 * function.
 *--------------------------------------------------------*/

void
dump_stack( FILE * fp )
{
    int pipe_fd[ 4 ];
    pid_t pid;
    size_t i;
    struct sigaction sact;


    /* Childs death signal isn't needed anymore */

    sact.sa_handler = ( void ( * )( int ) ) SIG_DFL;
    sact.sa_flags = 0;
    sigaction( SIGCHLD, &sact, NULL );

    /* Don't crash on SIGPIPE if child process dies unexpectedly */

    sact.sa_handler = ( void ( * )( int ) ) SIG_IGN;
    sact.sa_flags = 0;
    sigaction( SIGPIPE, &sact, NULL );

    /* Set up two pipes for communication with child process */

    if ( pipe( pipe_fd ) < 0 )
        return;
    if ( pipe( pipe_fd + 2 ) < 0 )
    {
        for ( i = 0; i < 2; i++ )
            close( pipe_fd[ i ] );
        return;
    }

    /* Create child running ADDR2LINE with input and output redirected */

    if ( ( pid = fork( ) ) == 0 )
    {
        close( pipe_fd[ DUMP_PARENT_READ ] );
        close( pipe_fd[ DUMP_PARENT_WRITE ] );

        dup2( pipe_fd[ DUMP_CHILD_WRITE ], STDOUT_FILENO );
        dup2( pipe_fd[ DUMP_CHILD_WRITE ], STDERR_FILENO );
        dup2( pipe_fd[ DUMP_CHILD_READ  ], STDIN_FILENO );

        close( pipe_fd[ DUMP_CHILD_READ ] );
        close( pipe_fd[ DUMP_CHILD_WRITE ] );

        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            execl( srcdir "fsc2_addr2line", "fsc2_addr2line", NULL );
        else
            execl( bindir "fsc2_addr2line", "fsc2_addr2line", NULL );
        _exit( EXIT_FAILURE );
    }
    else if ( pid < 0 )               /* fork failed */
    {
        for ( i = 0; i < 4; i++ )
            close( pipe_fd[ i ] );
        return;
    }

    close( pipe_fd[ DUMP_CHILD_READ  ] );
    close( pipe_fd[ DUMP_CHILD_WRITE ] );

    /* Now we're ready to write the backtrace into the file we got passed */

    for ( i = 2; i < Crash.trace_length - 2; i++ )
        if ( write_dump( pipe_fd, fp, i - 2, Crash.trace[ i ] ) == -1 )
            break;

    /* Pipes to the child aren't needed anymore, closing them will also
       make the child exit */

    close( pipe_fd[ DUMP_PARENT_READ ] );
    close( pipe_fd[ DUMP_PARENT_WRITE ] );
}

/*-----------------------------------------------------------------------*
 * Function converts the address information into something we can feed
 * to the process that in the end calls addr2line, writes it to the pipe
 * to that process, reads the answer and puts the answer, after a few
 * modifications, into the the file we got passed.
 *-----------------------------------------------------------------------*/

#define MAX_STR_BUF  512

static int
write_dump( int  * pipe_fd,
            FILE * fp,
            int    k,
            void * addr )
{
    char buf[ MAX_STR_BUF ] = "";
    ssize_t ret;
    char c;
    Dl_info info;
    void *start_addr = addr;
    int dont_print = 0;


    if ( addr == NULL || ! dladdr( addr, &info ) )
    {
        fprintf( fp, "#%-3d Address unknown\n", k );
        return 0;
    }

    if ( info.dli_fname[ 0 ] == '/' )
    {
        /* For fsc2 itself we need the address as it is, for libraries we
           have to subtract the base address */

        if ( strcmp( info.dli_fname + strlen( info.dli_fname ) - 4, "fsc2" ) )
            start_addr =
                ( void * ) ( ( char * ) addr - ( char * ) info.dli_fbase );
        sprintf( buf, "%s\n", info.dli_fname );
    }
    else
    {
        if ( strchr( info.dli_fname, '/' ) )
        {
            if ( ! getcwd( buf, MAX_STR_BUF - strlen( info.dli_fname ) - 2 ) )
                *buf = '\0';
            sprintf( buf + strlen( buf ), "/%s\n", info.dli_fname );
        }
        else if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            sprintf( buf, srcdir "%s\n", info.dli_fname );
        else
            sprintf( buf, bindir "%s\n", info.dli_fname );
    }

    ret = write( pipe_fd[ DUMP_PARENT_WRITE ], buf, strlen( buf ) );
    sprintf( buf, "%p\n", start_addr );
    ret = write( pipe_fd[ DUMP_PARENT_WRITE ], buf, strlen( buf ) );

    fprintf( fp, "#%-3d %-16p  ", k, addr );

    /* Copy reply to the answer pipe */

    c = '\0';

    while ( c != '\n' )
    {
        if ( ( ret = read( pipe_fd[ DUMP_PARENT_READ ], &c, 1 ) ) == -1 )
        {
            fputs( " <error>\n", fp );
            return -1;
        }

        if ( ret && c != '\n' )
            fputc( c, fp );
    };

    fputs( "() at ", fp );

    do
    {
        if ( ( ret = read( pipe_fd[ DUMP_PARENT_READ ], &c, 1 ) ) == -1 )
        {
            fputs( " <error>\n", fp );
            return -1;
        }

        if ( ret && ! dont_print && c == '?' )
        {
            fprintf( fp, "unknown line in %s\n", info.dli_fname );
            dont_print = 1;
        }

        if ( ret && ! dont_print )
            fputc( c, fp );
    } while ( c != '\n' );

    return 0;
}


#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
