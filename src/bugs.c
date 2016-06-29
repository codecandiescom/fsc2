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


extern Fsc2_Assert_T Assert_Struct;    /* defined in fsc2_assert.c */
extern int Fail_Mess_Fd;               /* defined in dump.c */


/*------------------------------------------------------------------------*
 * Callback function for the bug report button. Creates a bug report with
 * some information about the input and output of the program etc and
 * additional input by the user. Finally it mails me the report.
 *------------------------------------------------------------------------*/

#if defined MAIL_ADDRESS
void
bug_report_callback( FL_OBJECT * a,
                     long        b  UNUSED_ARG )
{
    FILE *tmp;
    int tmp_fd;
    char filename[ ] = P_tmpdir "/fsc2.mail.XXXXXX";
    char cur_line[ FL_BROWSER_LINELENGTH ];
    char *clp;
    int lines;
    int i;
    char *cmd, *user = NULL;
    char cur_dir[ PATH_MAX ];
    char *ed;
    int res;
    struct sigaction sact, oact;


    /* Create a temporary file for the mail */

    if (    ( tmp_fd = mkstemp( filename ) ) < 0
         || ( tmp = fdopen( tmp_fd, "w+" ) ) == NULL )
    {
        if ( tmp_fd >= 0 )
        {
            unlink( filename );
            close( tmp_fd );
        }

        fl_show_messages( "Sorry, can't send a bug report." );
        return;
    }

    sact.sa_handler = ( void ( * )( int ) ) SIG_IGN;
    sigaction( SIGCHLD, &sact, &oact );

    fl_set_cursor( FL_ObjWin( a ), XC_watch );
    XFlush( fl_get_display( ) );

    /* Write a header and the contents of the two browsers into the file */

    fprintf( tmp,
             "Please describe the bug you found as exactly as possible.\n"
             "Also all information about the circumstances that triggered\n"
             "the bug might prove useful, even if they might look to be\n"
             "irrelevant. And, finally, were you able to reproduce the\n"
             "bug or did you encounter it just once?\n\n\n\n\n\n\n\n\n\n"
             "Please do not change anything below this line. Thank you.\n"
             "*********************************************************\n" );

    fprintf( tmp, "Content of program browser:\n\n" );
    lines = fl_get_browser_maxline( GUI.main_form->browser );
    for ( i = 1; i <= lines; i++ )
    {
        strcpy( cur_line, fl_get_browser_line( GUI.main_form->browser, i ) );
        clp = cur_line;
        if ( *clp == '@' )
        {
            if ( *( clp + 1 ) == 'n' )
                continue;
            while ( *clp++ != 'f' )
                /* empty */ ;
        }
        fprintf( tmp, "%s\n", clp );
    }

    fprintf( tmp, "\n--------------------------------------------------\n\n" );

    fprintf( tmp, "Content of output browser:\n\n" );
    lines = fl_get_browser_maxline( GUI.main_form->error_browser );
    for ( i = 1; i <= lines; i++ )
        fprintf( tmp, "%s\n",
                 fl_get_browser_line( GUI.main_form->error_browser, i ) );
    fprintf( tmp, "--------------------------------------------------\n\n" );

    /* Append other informations, i.e the user name and his current directory
       as well as the location of the library */

    fprintf( tmp, "Current user: %s\n", ( getpwuid( getuid( ) ) )->pw_name );
    if ( getcwd( cur_dir, PATH_MAX ) )
        fprintf( tmp, "Current dir:  %s\n", cur_dir );
    fprintf( tmp, "libdir:       " libdir "\n\n" );

    /* Append output of ulimit */

    fprintf( tmp, "\"ulimit -a -S\" returns:\n\n" );
    cmd = get_string( "ulimit -a -S >> %s", filename );
    fflush( tmp );
    int dummy = system( cmd );
    T_free( cmd );

    cmd = get_string( "echo >> %s", filename );
    fflush( tmp );
    dummy = system( cmd );
    T_free( cmd );

    /* Append current disk usage to the file to detect problems due to a
       full disk */

    cmd = get_string( "df >> %f", filename );
    fflush( tmp );
    dummy = system( cmd );
    T_free( cmd );

    /* Assemble the command for invoking the editor */

    ed = getenv( "EDITOR" );
    if ( ed == NULL || *ed == '\0' )
    {
        cmd = T_malloc( 16 + strlen( filename ) );
        strcpy( cmd, "xterm -e vi" );
    }
    else
    {
        cmd = T_malloc( 14 + strlen( ed ) + strlen( filename ) );
        sprintf( cmd, "xterm -e %s", ed );
    }

    strcat( cmd, " +6 " );         /* does this work with all editors ? */
    strcat( cmd, filename );       /* (it does for vi, emacs and pico...) */

    fl_set_cursor( FL_ObjWin( a ), XC_left_ptr );

    fflush( tmp );

    /* Invoke the editor - when finished ask the user how to proceed */

    do
    {
        dummy = system( cmd );
        res = fl_show_choices( "Please choose one of the following options:",
                               3, "Send", "Forget", "Edit", 1 );
    } while ( res == 3 );

    T_free( cmd );

    fflush( tmp );

    unlink( filename );                /* delete the temporary file */

    if ( dummy == -1 ) { /* silence stupdo compiler warning */ }

    /* Send the report if the user wants it. */

    if ( res == 1 )
    {
        /* Ask the user if he wants a carbon copy */

        if ( 1 ==
             fl_show_question( "Do you want a copy of the bug report ?", 0 ) )
            user = ( getpwuid( getuid( ) ) )->pw_name;

        rewind( tmp );
        if ( send_mail( "FSC2: Bug report", "fsc2", user,
                        MAIL_ADDRESS, tmp ) != 0 )
            fl_show_messages( "Sorry, sending the mail may\n"
                              "   have failed to work." );
    }

    close( tmp_fd );
    sigaction( SIGCHLD, &oact, NULL );
}
#else
void
bug_report_callback( FL_OBJECT * a UNUSED_ARG,
                     long        b UNUSED_ARG )
{
}
#endif


/*-------------------------------------------------------*
 * This function sends an email to me when fsc2 crashes.
 *-------------------------------------------------------*/

void
crash_report( void )
{
    FILE * fp;
    int tmp_fd;
    char cur_line[ FL_BROWSER_LINELENGTH ];
    char *clp;
    int lines;
    int i;
    
    /* Create a temporary file for the mail */

#if defined CRASH_REPORT_DIR
    char filename[ ] = CRASH_REPORT_DIR "/fsc2.crash.XXXXXX";
#else
    char filename[ ] = P_tmpdir "/fsc2.crash.XXXXXX";
#endif

    if (    ( tmp_fd = mkstemp( filename ) ) < 0
         || ( fp = fdopen( tmp_fd, "w+" ) ) == NULL )
    {
        if ( tmp_fd >= 0 )
        {
            unlink( filename );
            close( tmp_fd );
        }

        return;
    }


#if defined _GNU_SOURCE
    fprintf( fp, "fsc2 (%d, %s) killed by %s signal (%d).\n\n", getpid( ),
             Fsc2_Internals.I_am == CHILD ? "CHILD" : "PARENT",
             strsignal( Crash.signo ), Crash.signo );
#else
    fprintf( fp, "fsc2 (%d, %s) killed by signal %d.\n\n", getpid( ),
             Fsc2_Internals.I_am == CHILD ? "CHILD" : "PARENT", Crash.signo );
#endif

#ifndef NDEBUG
    if ( Crash.signo == SIGABRT )
        fprintf( fp, "%s:%d: failed assertion: %s\n\n",
                 Assert_Struct.filename, Assert_Struct.line,
                 Assert_Struct.expression );
#endif

#if ! defined NDEBUG && defined ADDR2LINE
    if ( Crash.trace_length > 0 )
        dump_stack( fp );
#endif

    if ( EDL.Fname != NULL )
        fprintf( fp, "\n\nIn EDL program %s at line = %ld\n\n",
                 EDL.Fname, EDL.Lc );

    if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
    {
        fputs( "Content of program browser:\n\n"
               "--------------------------------------------------\n\n",
               fp );

        lines = fl_get_browser_maxline( GUI.main_form->browser );
        for ( i = 0; i < lines; )
        {
            strcpy( cur_line,
                    fl_get_browser_line( GUI.main_form->browser, ++i ) );
            clp = cur_line;
            if ( *clp == '@' )
            {
                if ( *( clp + 1 ) == 'n' )
                    continue;
                while ( *clp++ != 'f' )
                    /* empty */ ;
            }
            fputs( clp, fp );
            fputc( '\n', fp );
        }

        fputs( "--------------------------------------------------\n\n"
               "Content of output browser:\n"
               "--------------------------------------------------\n", fp );

        lines = fl_get_browser_maxline( GUI.main_form->error_browser );
        for ( i = 0; i < lines; )
        {
            fputs( fl_get_browser_line( GUI.main_form->error_browser, ++i ),
                   fp );
            fputc( ( int ) '\n', fp );
        }
    }

    fclose( fp );
    close( tmp_fd );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
