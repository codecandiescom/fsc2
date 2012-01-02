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


static void start_help_browser( void );


/*-------------------------------------------------------------*
 * This function is the callback function for the help button.
 * It forks a process that execs a browser displaying the HTML
 * documentation.
 *-------------------------------------------------------------*/

void
run_help( FL_OBJECT * a,
          long        b  UNUSED_ARG )
{
    int res;
    int bn;


    bn = fl_get_button_numb( a );
    if ( bn != FL_SHORTCUT + 'S' && bn == FL_RIGHT_MOUSE )
    {
        eprint( NO_ERROR, UNSET,
                ( GUI.G_Funcs.size == ( bool ) LOW ) ?
                "@n-------------------------------------------\n" :
                "@n-----------------------------------------------\n" );
        eprint( NO_ERROR, UNSET,
                "@nLeft mouse button (LMB):   Zoom after drawing box\n"
                "@nMiddle mouse button (MMB): Move curves\n"
                "@nRight mouse button (RMB):  Zoom in or out by moving mouse\n"
                "@nLMB + MMB: Show data values at mouse position\n"
                "@nLMB + RMB: Show differences of data values\n"
                "@n<Shift> LMB: Show cross section (2D only)\n"
                "@nLMB + MMB + <Space>: Switch between x/y cross section\n" );
        eprint( NO_ERROR, UNSET,
                ( GUI.G_Funcs.size == ( bool ) LOW ) ?
                "@n-------------------------------------------\n" :
                "@n-----------------------------------------------\n" );
        return;
    }

    /* Fork and run help browser in child process */

    if ( ( res = fork( ) ) == 0 )
        start_help_browser( );

    if ( res == -1 )                                /* fork failed ? */
        fl_show_alert( "Error", "Sorry, unable to start the help browser.",
                       NULL, 1 );
}


/*--------------------------------------------------------------*
 * This function starts a browser with the HTML documantation.
 * It is called after a fork(), so it may not return. Which
 * browser is used depends on the environment variable BROWSER,
 * currently the program only knows how to deal with Netscape,
 * mozilla, firefox, opera, konqueror, galeon, lnyx or w3m. If
 * BROWSER isn't set Netscape is used as the default.
 *--------------------------------------------------------------*/

static void
start_help_browser( void )
{
    char *browser;
    char *bn;
    const char *av[ 5 ] = { NULL, NULL, NULL, NULL, NULL };


    /* Try to figure out which browser to use, first look for user preference
       by checking the BROWSER environment variable, then, if the user hasn't
       set a preference, check if there's a default browser compiled into the
       program */

    browser = getenv( "BROWSER" );

#if defined BROWSER
    if ( ! browser )
        browser = ( char * ) BROWSER;
#endif

    if ( browser ) {
        if ( ( bn = strrchr( browser, '/' ) ) != NULL )
            bn += 1;
        else
            bn = browser;
    }

    if ( browser && ! strcasecmp( bn, "opera" ) )
    {
        av[ 0 ] = T_strdup( "opera" );
        av[ 1 ] = "-newwindow";
        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            av[ 1 ] = T_strdup( "file:/" ldocdir "fsc2.html,new-window" );
        else
            av[ 1 ] = T_strdup( "file:/" docdir "html/fsc2.html,new-window" );
    }
    else if ( browser && ! strcasecmp( bn, "konqueror" ) )
    {
        av[ 0 ] = T_strdup( "konqueror" );
        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            av[ 1 ] = T_strdup( "file:" ldocdir "fsc2.html" );
        else
            av[ 1 ] = T_strdup( "file:" docdir "html/fsc2.html" );
    }
    else if ( browser && ! strcasecmp( bn, "galeon" ) )
    {
        av[ 0 ] = T_strdup( "galeon" );
        av[ 1 ] = T_strdup( "--new-window" );
        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            av[ 2 ] = T_strdup( "file://" ldocdir "fsc2.html" );
        else
            av[ 2 ] = T_strdup( "file://" docdir "html/fsc2.html" );
    }
    else if ( browser && (    ! strcasecmp( bn, "lynx" )
                           || ! strcasecmp( bn, "w3m" ) ) )
    {
        av[ 0 ] = T_strdup( "xterm" );
        av[ 1 ] = T_strdup( "-e" );
        av[ 2 ] = T_strdup( browser );
        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            av[ 3 ] = T_strdup( ldocdir "fsc2.html" );
        else
            av[ 3 ] = T_strdup( docdir "html/fsc2.html" );
    }
    else if ( browser && (    strcasecmp( bn, "mozilla" )
                           || strcasecmp( bn, "MozillaFirebird" )
                           || strcasecmp( bn, "firefox" ) ) )
    {
        av[ 0 ] = T_strdup( browser );
        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            av[ 1 ] = T_strdup( "file:" ldocdir "fsc2.html" );
        else
            av[ 1 ] = T_strdup( "file:" docdir "html/fsc2.html" );
    }
    else
    {
        /* If netscape isn't running start it, otherwise ask it to just open
           a new window */

        av[ 0 ] = browser ? T_strdup( browser ) : T_strdup( "netscape" );

        if ( system( "xwininfo -name Netscape >/dev/null 2>&1" ) )
        {
            if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
                av[ 1 ] = T_strdup( "file:" ldocdir "fsc2.html" );
            else
                av[ 1 ] = T_strdup( "file:" docdir "html/fsc2.html" );
        }
        else
        {
            av[ 1 ] = T_strdup( "-remote" );
            if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
                av[ 2 ] = T_strdup( "openURL(file:" ldocdir
                                    "fsc2.html,new-window)" );
            else
                av[ 2 ] = T_strdup( "openURL(file:" docdir
                                    "html/fsc2.html,new-window)" );
        }
    }

    execvp( av[ 0 ], ( char ** ) av );
    _exit( EXIT_FAILURE );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
