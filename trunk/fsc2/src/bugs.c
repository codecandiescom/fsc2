/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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
	char filename[ ] = P_tmpdir "/fsc2XXXXXX";
	char cur_line[ FL_BROWSER_LINELENGTH ];
	char *clp;
	int lines;
	int i;
	char *cmd, *user = NULL;
	char cur_dir[ PATH_MAX ];
	char *ed;
	int res;
	struct sigaction sact, oact;



	a = a;
	b = b;

	notify_conn( BUSY_SIGNAL );

	/* Create a temporary file for the mail */

	if ( ( tmp_fd = mkstemp( filename ) ) < 0 ||
		 ( tmp = fdopen( tmp_fd, "w" ) ) == NULL )
	{
		fl_show_messages( "Sorry, can't send a bug report." );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	sact.sa_handler = SIG_IGN;
	sigaction( SIGCHLD, &sact, &oact );

	fl_set_cursor( FL_ObjWin( a ), XC_watch );
	XFlush( fl_get_display( ) );

	/* Write a header and the contents of the two browsers into the file */

	fprintf( tmp,
			 "Please enter a description of the bug you found. Also list\n"
			 "information about the circumstances that triggered the bug\n"
			 "as far as you think they might be relevant. Were you able\n"
			 "to reproduce the bug?\n"
			 "\n\n\n\n\n\n\n\n\n"
			 " Please do not change anything below this line. Thank you.\n"
		     "***********************************************************\n" );
	
	fprintf( tmp, "Content of program browser:\n\n" );
	lines = fl_get_browser_maxline( main_form->browser );
	for ( i = 1; i <= lines; i++ )
	{
		strcpy( cur_line, fl_get_browser_line( main_form->browser, i ) );
		clp = cur_line;
		if ( *clp == '@' )
			while ( *clp++ != 'f' )
				;
		fprintf( tmp, "%s\n", clp );
	}

	fprintf( tmp, "\n--------------------------------------------------\n\n" );

	fprintf( tmp, "Content of output browser:\n\n" );
	lines = fl_get_browser_maxline( main_form->error_browser );
	for ( i = 1; i <= lines; i++ )
		fprintf( tmp, "%s\n",
				 fl_get_browser_line( main_form->error_browser, i ) );
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
	cmd = get_string( strlen( "ulimit -a -S >> " ) + strlen( filename ) );
	strcpy( cmd, "ulimit -a -S >> " );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	cmd = get_string( strlen( "echo >> " ) + strlen( filename ) );
	sprintf( cmd, "echo >> %s", filename );
	system( cmd );
	system( cmd );
	T_free( cmd );

	/* Append current disk usage to the file to detect problems due to a
	   full disk */

	cmd = get_string( strlen( "df >> " ) + strlen( filename ) );
	strcpy( cmd, "df >> " );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	/* Finally append the file with the version informations so that I'll know
	   what the sender is really using */

	cmd = get_string( strlen( "echo \"\n\nVersion:\n\" >> " )
					  + strlen( filename ) );
	strcpy( cmd, "echo \"\n\nVersion:\n\" >> " );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	cmd = get_string( strlen( "cat  >> " ) + strlen( libdir )
					  + strlen( "/version " ) + strlen( filename ) );
	strcpy( cmd, "cat " );
	strcat( cmd, libdir );
	if ( libdir[ strlen( libdir ) - 1 ] != '/' )
		strcat( cmd, "/" );
	strcat( cmd, "version" );
	strcat( cmd, " >> " );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	/* Assemble the command for invoking the editor */

	ed = getenv( "EDITOR" );
	if ( ed == NULL || *ed == '\0' )
	{
		cmd = get_string( 6 + strlen( filename ) );
		strcpy( cmd, "vi" );
	}
	else
	{
		cmd = get_string( 4 + strlen( ed ) + strlen( filename ) );
		strcpy( cmd, ed );

		/* If the EDITOR environment variable contains the '-nw' option
		   get rid of it - we need a new window! */

		if ( ( clp = strstr( cmd, "-nw" ) ) != NULL && clp - 1 > cmd &&
			 isblank( *( clp - 1 ) ) &&
			 ( *( clp + 3 ) == '\0' || isblank( *( clp + 3 ) ) ) )
			strcpy( clp, clp + 3 );
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

		cmd = get_string( strlen( MAIL_PROGRAM MAIL_ADDRESS
								  " -s \"fsc2 bug report\" -c    < " ) +
						  + strlen( filename )
						  + ( user != NULL ? strlen( user ) : 0 ) );
		strcpy( cmd, MAIL_PROGRAM " -s \"fsc2 bug report\" " );

		if ( user != NULL )            /* append option for the carbon copy */
		{
			strcat( cmd, "-c " );
			strcat( cmd, user );
			strcat( cmd, " " );
		}
		strcat( cmd, MAIL_ADDRESS );

		strcat( cmd, " < " );          /* append the file name to be mailed */
		strcat( cmd, filename );

		system( cmd );                 /* send the mail */
		T_free( cmd );
	}

	unlink( filename );                /* delete the temporary file */
	sigaction( SIGCHLD, &oact, NULL );
	notify_conn( UNBUSY_SIGNAL );
#endif
}


/*-------------------------------------------------------*/
/* This function sends an email to me when fsc2 crashes. */
/*-------------------------------------------------------*/

void death_mail( int signo )
{
#if defined ( MAIL_ADDRESS ) && defined ( MAIL_PROGRAM )

	FILE *mail;
	char cur_line[ FL_BROWSER_LINELENGTH ];
	char *clp;
	int lines;
	int i;
	char buffer[ 512 ];
	int count;


	if ( ( mail = popen( MAIL_PROGRAM " -s 'fsc2 crash' " MAIL_ADDRESS, "w" ) )
		 == NULL )
		return;

	fprintf( mail, "fsc2 (%d, %s) killed by %s signal.\n\n", getpid( ),
			 I_am == CHILD ? "CHILD" : "PARENT", strsignal( signo ) );

#ifndef NDEBUG
	if ( signo == SIGABRT )
		fprintf( mail, "%s:%u: failed assertion: %s\n\n",
				 Assert_struct.filename, Assert_struct.line,
				 Assert_struct.expression );
#endif

	if ( fail_mess_fd >= 0 )
	{
		while ( ( count = read( fail_mess_fd, buffer, 512 ) ) > 0 )
			fwrite( buffer, count, 1, mail );
		fprintf( mail, "\n" );
		close( fail_mess_fd );
	}

	if ( Fname != NULL )
		fprintf( mail, "In EDL program %s at line = %ld\n\n", Fname, Lc );

	fputs( "Content of program browser:\n\n"
		   "--------------------------------------------------\n\n", mail );

	lines = fl_get_browser_maxline( main_form->browser );
	for ( i = 0; i < lines; )
	{
		strcpy( cur_line, fl_get_browser_line( main_form->browser, ++i ) );
		clp = cur_line;
		if ( *clp == '@' )
			while ( *clp++ != 'f' )
				;
		fputs( clp, mail );
		fputc( '\n', mail );
	}

	fputs( "--------------------------------------------------\n\n"
		   "Content of output browser:\n"
		   "--------------------------------------------------\n", mail );

	lines = fl_get_browser_maxline( main_form->error_browser );
	for ( i = 0; i < lines; )
	{
		fputs( fl_get_browser_line( main_form->error_browser, ++i ), mail );
		fputc( ( int ) '\n', mail );
	}

	pclose( mail );
#endif
}
