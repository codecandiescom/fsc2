/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2.h"


extern Fsc2_Assert Assert_struct;      /* defined fsc2_assert.c */
extern int fail_mess_fd;               /* defined in dump.c     */


/*------------------------------------------------------------------------*/
/* Callback function for the bug report button. Creates a bug report with */
/* some information about the input and output of the program etc and     */
/* additional input by the user. Finally it mails me the report.          */
/*------------------------------------------------------------------------*/

void bug_report_callback( FL_OBJECT *a, long b )
{
#if defined ( MAIL_ADDRESS ) && defined ( MAIL_PROGRAM )

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


	b = b;
	notify_conn( BUSY_SIGNAL );

	/* Create a temporary file for the mail */

	if ( ( tmp_fd = mkstemp( filename ) ) < 0 ||
		 ( tmp = fdopen( tmp_fd, "w" ) ) == NULL )
	{
		if ( tmp_fd >= 0 )
		{
			unlink( filename );
			close( tmp_fd );
		}

		fl_show_messages( "Sorry, can't send a bug report." );
		notify_conn( UNBUSY_SIGNAL );

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
	getcwd( cur_dir, PATH_MAX );
	fprintf( tmp, "Current dir:  %s\n", cur_dir );
	fprintf( tmp, "libdir:       %s\n\n", libdir );

	/* Append output of ulimit */

	fprintf( tmp, "\"ulimit -a -S\" returns:\n\n" );
	fclose( tmp );
	cmd = get_string( "ulimit -a -S >> %s", filename );
	system( cmd );
	T_free( cmd );

	cmd = get_string( "echo >> %s", filename );
	system( cmd );
	T_free( cmd );

	/* Append current disk usage to the file to detect problems due to a
	   full disk */

	cmd = get_string( "df >> %f", filename );
	system( cmd );
	T_free( cmd );

	/* Finally append the file with the version informations so that I'll know
	   what the sender is really using */

	cmd = get_string( "echo \"\nVersion (use uudecode file "
					  "| gunzip -c to unpack):\n\" >> %s", filename );
	system( cmd );
	T_free( cmd );

	cmd = get_string( "cat %s%sversion.ugz >> %s", libdir, slash( libdir ),
					  filename);
	system( cmd );
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

	/* Invoke the editor - when finished ask the user how to proceed */

	do
	{
		system( cmd );
		res = fl_show_choices( "Please choose one of the following options:",
							   3, "Send", "Forget", "Edit", 1 );
	} while ( res == 3 );

	T_free( cmd );

	/* Send the report if the user wants it. */

	if ( res == 1 )
	{
		/* Ask the user if he wants a carbon copy */

		if ( 1 ==
			 fl_show_question( "Do you want a copy of the bug report ?", 0 ) )
			user = ( getpwuid( getuid( ) ) )->pw_name;

		/* Assemble the command for sending the mail */

		cmd = get_string( "%s -s \"fsc2 bug report\" %s %s %s < %s",
						  MAIL_PROGRAM, user != NULL ? "-c" : "",
						  user != NULL ? user : "", MAIL_ADDRESS, filename);
		system( cmd );                 /* send the mail */
		T_free( cmd );
	}

	unlink( filename );                /* delete the temporary file */
	sigaction( SIGCHLD, &oact, NULL );
	notify_conn( UNBUSY_SIGNAL );
#else
	a = a;
	b = b;
#endif
}


/*-------------------------------------------------------*/
/* This function sends an email to me when fsc2 crashes. */
/*-------------------------------------------------------*/

void death_mail( int signo )
{
#if ! defined( NDEBUG ) && defined ( MAIL_ADDRESS ) && defined ( MAIL_PROGRAM )

#define DM_BUF_SIZE 512

	FILE *mail;
	char cur_line[ FL_BROWSER_LINELENGTH ];
	char *clp;
	int lines;
	int i;
	char buffer[ DM_BUF_SIZE ];
	size_t count;
	char vfn[ PATH_MAX + 20 ];
	FILE *vfp;


	if ( ( mail = popen( MAIL_PROGRAM " -s 'fsc2 crash' " MAIL_ADDRESS, "w" ) )
		 == NULL )
		return;

#if defined _GNU_SOURCE
	fprintf( mail, "fsc2 (%d, %s) killed by %s signal.\n\n", getpid( ),
			 Internals.I_am == CHILD ? "CHILD" : "PARENT",
			 strsignal( signo ) );
#else
	fprintf( mail, "fsc2 (%d, %s) killed by signal %d.\n\n", getpid( ),
			 Internals.I_am == CHILD ? "CHILD" : "PARENT", signo );
#endif

#ifndef NDEBUG
	if ( signo == SIGABRT )
		fprintf( mail, "%s:%d: failed assertion: %s\n\n",
				 Assert_struct.filename, Assert_struct.line,
				 Assert_struct.expression );
#endif

	if ( fail_mess_fd >= 0 )
	{
		while ( ( count = read( fail_mess_fd, buffer, DM_BUF_SIZE ) ) > 0 )
			fwrite( buffer, count, 1, mail );
		fprintf( mail, "\n" );
		close( fail_mess_fd );
	}

	if ( EDL.Fname != NULL )
		fprintf( mail, "In EDL program %s at line = %ld\n\n",
				 EDL.Fname, EDL.Lc );

	fputs( "Content of program browser:\n\n"
		   "--------------------------------------------------\n\n", mail );

	lines = fl_get_browser_maxline( GUI.main_form->browser );
	for ( i = 0; i < lines; )
	{
		strcpy( cur_line, fl_get_browser_line( GUI.main_form->browser, ++i ) );
		clp = cur_line;
		if ( *clp == '@' )
		{
			if ( *( clp + 1 ) == 'n' )
				continue;
			while ( *clp++ != 'f' )
				/* empty */ ;
		}
		fputs( clp, mail );
		fputc( '\n', mail );
	}

	fputs( "--------------------------------------------------\n\n"
		   "Content of output browser:\n"
		   "--------------------------------------------------\n", mail );

	lines = fl_get_browser_maxline( GUI.main_form->error_browser );
	for ( i = 0; i < lines; )
	{
		fputs( fl_get_browser_line( GUI.main_form->error_browser, ++i ),
               mail );
		fputc( ( int ) '\n', mail );
	}

	snprintf( vfn, PATH_MAX + 20, "%s%sversion.ugz", libdir, slash( libdir ) );

	if ( ( vfp = fopen( vfn, "r" ) ) != NULL )
	{
		fputs( "\n\nVersion:\n\n", mail );
		while ( ( count = fread( buffer, 1, DM_BUF_SIZE, vfp ) ) != 0 )
			fwrite( buffer, 1, count, mail );
		fclose( vfp );
	}

	pclose( mail );
#else
	signo = signo;              /* keeps the compiler happy :-) */
#endif
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
