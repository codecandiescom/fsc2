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


static void start_editor( void );


/*---------------------------------------------------------------------*
 * edit_file() allows to edit the current file (but it's also possible
 * to edit a new file if there is no file loaded) and is the callback
 * function for the Edit-button.
 *---------------------------------------------------------------------*/

void
edit_file( FL_OBJECT * a  UNUSED_ARG,
           long        b  UNUSED_ARG )
{
    /* Fork and execute editor in child process */

    int res;
    if ( ( res = fork( ) ) == 0 )
        start_editor( );

    if ( res == -1 )                                /* fork failed ? */
        fl_show_alert( "Error", "Sorry, unable to start the editor.",
                       NULL, 1 );
}


/*---------------------------------------------------------------------*
 * This function is called to start the editor on the currently loaded
 * file. It is called after a fork(), so it may not return. Which
 * editor is used depends on the environment variable EDITOR. If this
 * variable isn't set vi is used.
 *---------------------------------------------------------------------*/

static void
start_editor( void )
{
    char **argv;
    int argc = 1;
    int i;


    /* Check the TERM environment variable for the terminal program supposed
       to be use, and if it's not set, fall back to 'xterm' (hopefully it's
       installed, but with modern systems even that isn't a given:-( */

    char * term = getenv( "TERM" );

    if ( ! term || ! *term )
        term = ( char * ) "xterm";

    /* Try to find content of environment variable "EDITOR", "VISUAL"
       or "SELECTED_EDITOR" (one of these is  probably the users choice)
       - if it doesn't exist try to use the        one compiled into the
       program, and as a final option use either /usr/bin/sensible-editor
       (if it exists) or vi as the default editor */

    char * ed = getenv( "EDITOR" );

    if ( ! ed )
        ed = getenv( "VISUAL" );
    if ( ! ed )
        ed = getenv( "SELECTED_EDITOR" );

#if defined EDITOR
    if ( ! ed )
        ed = ( char * ) EDITOR;
#endif

    char * oed = ed;
    ed = T_strdup( ed );

    if ( ! ed || ! *ed )   /* nothing found, use system default or vi */
    {
        argv = T_malloc( 5 * sizeof *argv );

        argv[ 0 ] = term;
        argv[ 1 ] = ( char * ) "-e";

        struct stat buf;
        if (    ! stat( "/usr/bin/sensible-editor", &buf )
             && buf.st_mode & S_IXOTH )
            argv[ 2 ] = ( char * ) "/usr/bin/sensible-editor";
        else
            argv[ 2 ] = ( char * ) "vi";

        argv[ 3 ] = EDL.files->name;
        argv[ 4 ] = NULL;
    }
    else                   /* otherwise use the one given by EDITOR etc. */
    {
        char * ep = ed;
        while ( *ep && *ep != ' ' )
            ++ep;

        /* Check if there are command line options and if so, count them
           and set up the argv pointer array */

        if ( ! *ep )   /* no command line arguments */
        {
            argv = T_malloc( 5 * sizeof *argv );

            argv[ 0 ] = ( char * ) term;
            argv[ 1 ] = ( char * ) "-e";
            argv[ 2 ] = ed;
            argv[ 3 ] = EDL.files->name;
            argv[ 4 ] = NULL;
        }
        else
        {
            while ( 1 )          /* count command line options */
            {
                while ( *ep && *ep != ' ' )
                    ++ep;
                if ( ! *ep )
                    break;
                *ep++ = '\0';
                ++argc;
            }

            argv = T_malloc( ( argc + 5 ) * sizeof *argv );

            argv[ 0 ] = term;
            argv[ 1 ] = ( char * ) "-e";
            for ( ep = ed, i = 2; i < argc + 2; ++i )
            {
                argv[ i ] = ep;
                while ( *ep++ )
                    /* empty */ ;
            }

            argv[ i ]   = EDL.files->name;
            argv[ ++i ] = NULL;
        }
    }

    /* Special treatment for emacs and xemacs: if emacs is called without
       the '-nw' option it will create its own window - so we don't embed
       it into a xterm - the same holds for xemacs which, AFAIK, always
       creates its own window. */

    char ** final_argv = argv;

    char const * sed = strip_path( argv[ 2 ] );
    if ( ! strcmp( sed, "xemacs" ) )
    {
        final_argv = argv + 2;
    }
    else if ( ! strcmp( sed, "emacs" ) )
    {
        for ( i = 3; argv[ i ] && strcmp( argv[ i ], "-nw" ); ++i )
            /* empty */ ;
        if ( argv[ i ] == NULL )                    /* no '-nw' flag */
            final_argv = argv + 2;
    }

    if ( final_argv == argv )
    {
        argv[ 2 ] = get_string( "%s %s", oed, EDL.files->name );
        argv[ 3 ] = NULL;
    }

    execvp( final_argv[ 0 ], final_argv );
    if ( final_argv != argv )
        T_free( argv[ 2 ] );
    T_free( argv );
    T_free( ed );
    _exit( EXIT_FAILURE );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
