/*
  $Id$
*/


#include "fsc2.h"


/*------------------------------------------------------------------------*/
/* Callback function for the bug report button. Creates a bug report with */
/* some information about the input and output of the program etc and     */
/* additional input by the user. Finally it mails me the report.          */
/*------------------------------------------------------------------------*/

void bug_report_callback( FL_OBJECT *a, long b )
{
	FILE *tmp;
	char filename[ ] = P_tmpdir "/fsc2XXXXXX";
	int lines;
	int i;
	char *cmd, *jens, *user;
	char cur_dir[ PATH_MAX ];
	char *ed;
	int res, cc;


	a = a;
	b = b;

	/* Create a temporary file for the mail */

	if ( mkstemp( filename ) < 0 || ( tmp = fopen( filename, "w" ) ) == NULL )
	{
		fl_show_messages( "Sorry can't send a bug report." );
		return;
	}

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
		fprintf( tmp, "%s\n", fl_get_browser_line( main_form->browser, i ) );
	fprintf( tmp, "--------------------------------------------------\n\n" );

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
	fclose( tmp );

	/* Append a directory listing of configuration files and modules
	   to allow a check of the permissions */

	cmd = get_string( strlen( "ls -al  >> " ) + strlen( libdir ) +
					  strlen( "/Devices" ) + strlen( filename ) );
	strcpy( cmd, "ls -al " );
	strcat( cmd, libdir );
	strcat( cmd, "/Devices >> " );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	cmd = get_string( strlen( "ls -al  >> " ) + strlen( libdir ) +
					  strlen( "/Functions" ) + strlen( filename ) );
	strcpy( cmd, "ls -al " );
	strcat( cmd, libdir );
	strcat( cmd, "/Functions >> " );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	cmd = get_string( strlen( "ls -al  >> " ) + strlen( libdir ) +
					  strlen( "/*.so" ) + strlen( filename ) );
	strcpy( cmd, "ls -al " );
	strcat( cmd, libdir );
	strcat( cmd, "/*.so >> " );
	strcat( cmd, filename );
	system( cmd );

	strcpy( cmd, "echo >> " );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	/* Append current disk usage to the file to detect problems due to a
	   full disk */

	cmd = get_string( strlen( "df >> " ) + strlen( filename ) );
	strcpy( cmd, "df >> " );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	/* Append the devices data base file */

	cmd = get_string( strlen( "echo \"\nDevices:\n\" >> " )
					  + strlen( filename ) );
	strcpy( cmd, "echo \"\nDevices:\n\" >>" );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	cmd = get_string( strlen( "cat  >> " ) + strlen( libdir )
					  + strlen( "/Devices" ) + strlen( filename ) );
	strcpy( cmd, "cat " );
	strcat( cmd, libdir );
	strcat( cmd, "/Devices" );
	strcat( cmd, " >> " );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	/* Append the Functions data base file */

	cmd = get_string( strlen( "echo \"\n\nFunctions:\n\" >> " )
					  + strlen( filename ) );
	strcpy( cmd, "echo \"\n\nFunctions:\n\" >>" );
	strcat( cmd, filename );
	system( cmd );
	T_free( cmd );

	cmd = get_string( strlen( "cat  >> " ) + strlen( libdir )
					  + strlen( "/Functions" ) + strlen( filename ) );
	strcpy( cmd, "cat " );
	strcat( cmd, libdir );
	strcat( cmd, "/Functions" );
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
		/* Check if I have an account on this machine, if not send mail to my 
		   account on masklin */

		if ( getpwnam( "jens" ) == NULL )
			jens =
			   get_string_copy( "jens@masklin.anorg.chemie.uni-frankfurt.de" );
		else
			jens = get_string_copy( "jens" );

		/* Ask the user if he wants a carbon copy */

		cc = fl_show_question( "Do you want a copy of the bug report ?", 0 );
		if ( cc == 1 )
			user = ( getpwuid( getuid( ) ) )->pw_name;

		/* Assemble the command for sending the mail */

		cmd = get_string( strlen( "mail -s \"fsc2 bug report\" -c    < " ) +
						  + strlen( jens ) + strlen( filename ) +
						  ( cc ? strlen( user ) : 0 ) );
		strcpy( cmd, "mail -s \"fsc2 bug report\" " );

		if ( cc == 1 )                 /* append option for the carbon copy */
		{
			strcat( cmd, "-c " );
			strcat( cmd, user );
			strcat( cmd, " " );
		}
		strcat( cmd, jens );           /* append my mail address */
		T_free( jens );

		strcat( cmd, " < " );          /* append the file name to be mailed */
		strcat( cmd, filename );

		system( cmd );                 /* send the mail */
		T_free( cmd );
	}

	unlink( filename );                /* delete the temporary file */
}
