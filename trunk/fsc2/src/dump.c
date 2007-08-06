/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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


/* The functions defined here are only needed in builts where NDEBUG
   and ADDR2LINE are defined ! */

#if ! defined( NDEBUG ) && defined( ADDR2LINE )

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

void dump_stack( FILE * fp )
{
    int pipe_fd[ 4 ];
    pid_t pid;
    int i;
    struct sigaction sact;
    Device_T *cd;


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

    for ( i = 0; i < Crash.trace_length; i++ )
        if ( write_dump( pipe_fd, fp, i, Crash.trace[ i ] ) == -1 )
            break;

    /* Pipes to the child aren't needed anymore, closing them will also
       make the child exit */

    close( pipe_fd[ DUMP_PARENT_READ ] );
    close( pipe_fd[ DUMP_PARENT_WRITE ] );

    if ( EDL.Device_List != NULL )
    {
        fprintf( fp, "\nDevice handles:\n" );

        for ( cd = EDL.Device_List; cd != NULL; cd = cd->next )
        {
            if ( ! cd->is_loaded )
                continue;
            fprintf( fp, "%s.so: %p\n",
                     cd->name, ( void * ) * ( int * ) cd->driver.handle );
        }
    }
}

/*-----------------------------------------------------------------------*
 * Function converts the address information into something we can feed
 * to the process that in the end calls the addr2line, writes it to the
 * pipe to that process, reads the answer and puts the answer, after a
 * few modifications, into the the file we got passed.
 *-----------------------------------------------------------------------*/

static int write_dump( int  * pipe_fd,
                       FILE * fp,
                       int    k,
                       void * addr )
{
    char buf[ 256 ] = "";
    ssize_t ret;
    char c;
    Device_T *cd1, *cd2;


    if ( addr == NULL )
    {
        fprintf( fp, "#%-3d Address unknown\n", k );
        return 0;
    }

    /* We need to figure out if the crash happened in one of the modules
       or not in order to be able to pass the the file name beside the
       crash address to the process deling with addr2line. We can only
       distinguish between crashes in the fsc2 executable or in the
       modules, so traces into some library will (falsly) treated as
       being within the fsc2 executable and the output in the mail will
       show nothing really useful. */

    for ( cd1 = EDL.Device_List; cd1 != NULL; cd1 = cd1->next )
        if ( cd1->is_loaded )
            break;

    if ( cd1 != NULL )
    {
        for ( cd2 = cd1->next; cd2 != NULL; cd2 = cd2->next )
        {
            if ( ! cd2->is_loaded )
                continue;

            if (    addr >= ( void * ) * ( int * ) cd1->driver.handle
                 && addr <  ( void * ) * ( int * ) cd2->driver.handle )
                break;

            cd1 = cd2;
        }

        if (    cd1->is_loaded
             && ( char * ) addr >= ( char * ) * ( int * ) cd1->driver.handle )
        {
            if ( cd1->driver.lib_name[ 0 ] == '/' )
                sprintf( buf, "%s\n", cd1->driver.lib_name );
            else if ( Fsc2_Internals.cmdline_flags &
                      ( DO_CHECK | LOCAL_EXEC ) )
                sprintf( buf, moddir "%s\n", cd1->driver.lib_name );
            else
                sprintf( buf, libdir "%s\n", cd1->driver.lib_name );
            write( pipe_fd[ DUMP_PARENT_WRITE ], buf, strlen( buf ) );
            sprintf( buf, "%p\n",
                     ( void * ) ( ( char * ) addr -
                                  ( char * ) * ( int * ) cd1->driver.handle) );
        }
    }

    if ( *buf == '\0' )
    {
        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            write( pipe_fd[ DUMP_PARENT_WRITE ], srcdir "fsc2\n",
                   strlen( srcdir ) + 5 );
        else
            write( pipe_fd[ DUMP_PARENT_WRITE ], bindir "fsc2\n",
                   strlen( bindir ) + 5 );
        sprintf( buf, "%p\n", addr );
    }

    write( pipe_fd[ DUMP_PARENT_WRITE ], buf, strlen( buf ) );

    fprintf( fp, "#%-3d %-10p  ", k, addr );

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

        if ( ret )
            fputc( c, fp );
    } while ( c != '\n' );

    return 0;
}


/*-----------------------------------------------------*
 * Function to assemble the addresses for a backtrace,
 * the argument is a pointer to where the next stack
 * frame is.
 *-----------------------------------------------------*/

int create_backtrace( unsigned int * bt )
{
    unsigned int *EBP = bt;         /* assumes size equals that of pointers */
    int size = 1;


    /* Loop over all stackframes, working up the way to the top - the topmost
       stack frame would be reached when the content of EBP is zero, but this
       is already _libc_start_main(), so stop one frame earlier */

    while ( size < MAX_TRACE_LEN && * ( unsigned int * ) * EBP != 0 )
    {
        /* Get return address of current subroutine and ask the process
           finally running ADDR2LINE to convert it into function name,
           source file and line number. (This fails for programs competely
           stripped of all debugging information.) */

        Crash.trace[ size++ ] = ( void * ) * ( EBP + 1 );
        EBP = ( unsigned int * ) * EBP;
    }

    return size;
}

#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
