/*
  $Id$
*/

#define FSC2_MAIN

#include "fsc2.h"


#if defined MDEBUG
#include <mcheck.h>
#endif


/* Locally used global variables */

static bool is_loaded = UNSET;       /* set when EDL file is loaded */
static bool is_tested = UNSET;       /* set when EDL file has been tested */
static bool state = UNSET;           /* set when EDL passed the tests */
static char *in_file = NULL;         /* name of input file */
static time_t in_file_mod = 0;
static bool delete_file = UNSET;
static bool delete_old_file = UNSET;


/* Locally used functions */

static void final_exit_handler( void );
static bool display_file( char *name, FILE *fp );
static void start_editor( void );
static void start_help_browser( void );
static void set_main_signals( void );
static void usage( void );



/**************************/
/*     Here we go...      */
/**************************/

int main( int argc, char *argv[ ] )
{
	bool do_load = UNSET;
	bool do_test = UNSET;
	bool do_start = UNSET;
	bool do_signal = UNSET;
	bool do_delete = UNSET;
	char *fname;
	int cur_arg;


#if defined MDEBUG
	if ( mcheck( NULL ) != 0 )
	{
		printf( "Can't start mcheck() !\n" );
		return EXIT_FAILURE;
	}
#endif

	/* First we have to test for the "-t" and the "-h" or "--help" option.
	   If they are present no graphics are started. When "-t" is found the
	   file is just tested and the output is sent to stderr -- this can be
	   used to test a file e.g. with emacs' "M-x compile" feature or from
	   the shell. For "-h" or "--help" the usage information is printed
	   and the program exits immediately. */

	if ( argc != 1 )
	{
		cur_arg = 0;
		while ( ++cur_arg < argc )
		{
			if ( ! strcmp( argv[ cur_arg ], "-t" ) )
			{
				/* no file name with "-t" option ? */

				if ( ++cur_arg == argc )
				{
					fprintf( stderr, "fsc2 -t: No input file.\n" );
					return EXIT_FAILURE;
				}

				just_testing = SET;      /* set "just_testing"-mode flag */

				seteuid( getuid( ) );
				setegid( getgid( ) );
				return scan_main( argv[ cur_arg ] ) ?
					EXIT_SUCCESS : EXIT_FAILURE;
			}

			if ( ! strcmp( argv[ cur_arg ], "-h" ) ||
				 ! strcmp( argv[ cur_arg ], "--help" ) )
				usage( );
		}
	}

	/* Check via the lock file if there is already a process holding a lock,
	   otherwise create one. This, as well as the following check for stale
	   shared memory segments has to be done with the effective user ID, i.e.
	   the UID of fsc2. */

	if ( ! fsc2_locking( ) )
		return EXIT_FAILURE;

	/* Remove orphaned shared memory segments resulting from previous
	   crashes */

	delete_stale_shms( );

	/* Now we set the effective user ID to the real user ID, we only switch
	   back when creating or attaching shared memory segments */

	EUID = geteuid( );
	EGID = getegid( );
	seteuid( getuid( ) );
	setegid( getgid( ) );

	/* Initialize xforms stuff, quit on error */

	if ( ! xforms_init( &argc, argv ) )
	{
		unlink( LOCKFILE );
		return EXIT_FAILURE;
	}

	/* Now we check for remaining  options. With the option '-S' the program
	   is started immediately, while with '-T' it is tested. If the option
	   '-s' is found fsc2 will send its parent process a SIGUSR1 signal when
	   it got started successfully. And with "-d" the input file will be
	   deleted automatically when the program is done with it. */

	if ( argc != 1 )
	{
		cur_arg = 1;
		while ( cur_arg < argc )
		{
			if ( ! strcmp( argv[ cur_arg ], "-s" ) )
			{
				do_signal = SET;
				cur_arg++;
				continue;
			}

			if ( ! strcmp( argv[ cur_arg ], "-d" ) )
			{
				do_delete = SET;
				cur_arg++;
				continue;
			}

			if ( ! strncmp( argv[ cur_arg ], "-S", 2 ) )
			{
				if ( do_test )
				{
					eprint( FATAL, "fsc2: Can't have both flags `-S' and "
							        "`-T'.\n" );
					cur_arg++;
					continue;
				}

				if ( argv[ cur_arg ][ 2 ] == '\0' && cur_arg + 1 >= argc )
				{
					eprint( FATAL, "fsc2 -S: No input file.\n" );
					cur_arg++;
					continue;
				}

				fname = argv[ cur_arg ][ 2 ] != '\0' ?
					    &argv[ cur_arg ][ 2 ] : argv[ ++cur_arg ];

				do_load = SET;
				do_start = SET;
				cur_arg++;
				continue;
			}

			if ( ! strncmp( argv[ cur_arg ], "-T", 2 ) )
			{
				if ( do_start )
				{
					eprint( FATAL, "fsc2: Can't have both flags `-S' and "
							"`-T'.\n" );
					cur_arg++;
					continue;
				}

				if ( argv[ cur_arg ][ 2 ] == '\0' && cur_arg + 1 >= argc )
				{
					eprint( FATAL, "fsc2 -T: No input file\n" );
					cur_arg++;
					continue;
				}

				fname = argv[ cur_arg ][ 2 ] != '\0' ?
					    &argv[ cur_arg ][ 2 ] : argv[ ++cur_arg ];

				do_load = SET;
				do_test = SET;
				cur_arg++;
				continue;
			}

			do_load = SET;
			fname = argv[ cur_arg ];
			break;
		}
	}

	/* If '-d' was given on the command line store flags that are tested to
	   find out if the files needs to be deleted */

	if ( do_delete )
		delete_file = delete_old_file = SET;

	/* If there is a file as argument try to load it */

	if ( do_load )
	{
		TRY
		{
			in_file = T_strdup( fname );
			TRY_SUCCESS;
		}
		OTHERWISE
			return EXIT_FAILURE;

		load_file( main_form->browser, 1 );
	}

	/* Set handler for signal that's going to be send by the process that
	   accepts external connections (to be spawned next) */

	set_main_signals( );
	atexit( final_exit_handler );

	/* If starting the server for external connections succeeds we can
	   really start the main loop */

	if ( ( conn_pid = spawn_conn( ( do_test || do_start ) && is_loaded ) )
		 != -1 )
	{
		/* Trigger test or start of current EDL program if the appropriate
		   flags were passed to the program on the command line */

		if ( do_test && is_loaded )
			fl_trigger_object( main_form->test_file );
		if ( do_start && is_loaded )
			fl_trigger_object( main_form->run );

		/* If required send signal to parent process, then loop until quit
		   button is pressed */

		if ( do_signal )
			kill( getppid( ), SIGUSR1 );

		/* And here's the main loop of the program... */

		while ( fl_do_forms( ) != main_form->quit )
			;
	}
	else
		fprintf( stderr, "fsc2: Internal failure on startup.\n" );

	clean_up( );
	xforms_close( );

	return EXIT_SUCCESS;
}


/*------------------------------------------------------------*/  
/*------------------------------------------------------------*/  

static void final_exit_handler( void )
{
	/* Stop the process that's waiting for external connections as well
	   as the child process */

	if ( conn_pid > 0 )
		kill( conn_pid, SIGTERM );

	if ( child_pid > 0 )
		kill( child_pid, SIGTERM );

	/* Do everything necessary to end the program */

	if ( delete_old_file && in_file != NULL )
		unlink( in_file );
	unlink( FSC2_SOCKET );

	/* Delete all shared memory and also semaphore (if it still exists) */

	delete_all_shm( );
	if ( semaphore >= 0 )
		sema_destroy( semaphore );

	/* Delete the lock file */

	setuid( EUID );
	unlink( LOCKFILE );
}


/*-------------------------------------------------------------------*/
/* load_file() is used for loading or reloading a file, depending on */
/* the value of the flag 'reload'. It's also the callback function   */
/* for the Load- and Reload-buttons.                                 */
/* reload == 0: read new file, reload == 1: reload file              */
/*-------------------------------------------------------------------*/

void load_file( FL_OBJECT *a, long reload )
{
	const char *fn;
	char *old_in_file = NULL;
	FILE *fp;
	struct stat file_stat;
	static char *title = NULL;


	notify_conn( BUSY_SIGNAL );

	a = a;

	/* If new file is to be loaded get its name and store it, otherwise use
	   previous name */

	if ( ! reload )
	{
		if ( main_form->Load->u_ldata == 0 &&
			 main_form->Load->u_cdata == NULL )
		{
			fn = fl_show_fselector( "Select input file:", NULL, "*.edl",
									NULL );
			if ( fn == NULL )
			{
				notify_conn( UNBUSY_SIGNAL );
				return;
			}

			old_in_file = in_file;

			TRY
			{
				in_file = T_strdup( fn );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				in_file = old_in_file;
				notify_conn( UNBUSY_SIGNAL );
				return;
			}
		}
		else
		{
			old_in_file = in_file;

			in_file = main_form->Load->u_cdata;
			main_form->Load->u_cdata = NULL;
		}
	}
	else                              /* call via reload buton */
	{
		/* Quit if name of previous file is empty */

		if ( in_file == '\0' )
		{
			fl_show_alert( "Error", "Sorry, no file is loaded yet.", NULL, 1 );
			old_in_file = T_free( old_in_file );
			notify_conn( UNBUSY_SIGNAL );
			return;
		}
	}

	/* Test if the file is readable and can be opened */

	if ( access( in_file, R_OK ) == -1 )
	{
		if ( errno == ENOENT )
			fl_show_alert( "Error", "Sorry, file not found:", in_file, 1 );
		else
			fl_show_alert( "Error", "Sorry, no permission to read file:",
						   in_file, 1 );
		old_in_file = T_free( old_in_file );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	if ( ( fp = fopen( in_file, "r" ) ) == NULL )
	{
		fl_show_alert( "Error", "Sorry, can't open file:", in_file, 1 );
		old_in_file = T_free( old_in_file );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	/* Now that we're sure that we can read the new file we can delete the
	   old file (but only if the new and the old file are different !) */

	if ( old_in_file != NULL )
	{
		if ( strcmp( old_in_file, in_file ) )
		{
			if ( delete_old_file )
				unlink( old_in_file );
			delete_old_file = delete_file;
		}
		else      /* don't delete reloaded files - they may have been edited */
			delete_old_file = UNSET;
	}

	delete_file = UNSET;
	old_in_file = T_free( old_in_file );

	/* Get modification time of file */

	stat( in_file, &file_stat );
	in_file_mod = file_stat.st_mtime;

	/* Set a new window title */

	T_free( title );
	title = get_string( 6 + strlen( in_file ) );
	strcpy( title, "fsc2: " );
	strcat( title, in_file );
	fl_set_form_title( main_form->fsc2, title );

	/* Read in and display the new file */

	is_loaded = display_file( in_file, fp );
	state = FAIL;
	is_tested = UNSET;

	/* Run all the exit hooks and zero number of compilation errors */

	if ( ! exit_hooks_are_run )
		run_exit_hooks( );

	compilation.error[ FATAL ] = 
		compilation.error[ SEVERE ] =
		    compilation.error[ WARN ] = 0;

	fclose( fp );

	if ( main_form->Load->u_ldata != 0 )
	{
		if ( main_form->Load->u_ldata == ( long ) 'S' )
			fl_trigger_object( main_form->run );
		if ( main_form->Load->u_ldata == ( long ) 'T' )
			fl_trigger_object( main_form->test_file );
		main_form->Load->u_ldata = 0;
	}

	notify_conn( UNBUSY_SIGNAL );
}


/*---------------------------------------------------------------------*/
/* edit_file() allows to edit the current file (but it's also possible */
/* to edit a new file if there is no file loaded) and is the callback  */
/* function for the Edit-button.                                       */
/*---------------------------------------------------------------------*/

void edit_file( FL_OBJECT *a, long b )
{
	int res;


	a = a;
	b = b;


	/* Fork and execute editor in child process */

	if ( ( res = fork( ) ) == 0 )
		start_editor( );

	if ( res == -1 )                                /* fork failed ? */
		fl_show_alert( "Error", "Sorry, unable to start the editor.",
					   NULL, 1 );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static void start_editor( void )
{
	char *ed, *ep;
	char **argv, **final_argv;
	int argc = 1, i;


	/* Try to find content of environment variable "EDITOR" - if it doesn't
	   exist use vi as standard editor */

	ed = getenv( "EDITOR" );

	if ( ed == NULL || *ed == '\0' )
	{
		argv = T_malloc( 5 * sizeof ( char * ) );

		argv[ 0 ] = ( char * ) "xterm";
		argv[ 1 ] = ( char * ) "-e";
		argv[ 2 ] = ( char * ) "vi";
		argv[ 3 ] = in_file;
		argv[ 4 ] = NULL;
	}
	else              /* otherwise use the one given by EDITOR */
	{
		ep = ed;
		while ( *ep && *ep != ' ' )
			++ep;

        /* Check if there are command line options and if so, count them
		   and set up the argv pointer array */

		if ( ! *ep )   /* no command line arguments */
		{
			argv = T_malloc( 5 * sizeof ( char * ) );

			argv[ 0 ] = ( char * ) "xterm";
			argv[ 1 ] = ( char * ) "-e";
			argv[ 2 ] = ed;
			argv[ 3 ] = in_file;
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
		
			argv = T_malloc( ( argc + 5 ) * sizeof ( char * ) );

			argv[ 0 ] = ( char * ) "xterm";
			argv[ 1 ] = ( char * ) "-e";
			for ( ep = ed, i = 2; i < argc + 2; ++i )
			{
				argv[ i ] = ep;
				while ( *ep++ )
					;
			}

			argv[ i ] = in_file;
			argv[ ++i ] = NULL;
		}
	}

	/* Special treatment for emacs and xemacs: if emacs is called without
	   the '-nw' option it will create its own window - so we don't embed
	   it into a xterm - the same holds for xemacs which always creates
	   its own window. */

	final_argv = argv;

	if ( ! strcmp( strip_path( argv[ 2 ] ), "xemacs" ) )
		final_argv = argv + 2;

	if ( ! strcmp( strip_path( argv[ 2 ] ), "emacs" ) )
	{
		for ( i = 3; argv[ i ] && strcmp( argv[ i ], "-nw" ); ++i )
			;
		if ( argv[ i ] == NULL )                    /* no '-nw' flag */
			final_argv = argv + 2;
	}

	execvp( final_argv[ 0 ], final_argv );

	_exit( EXIT_FAILURE );
}


/*-----------------------------------------------------*/
/* test_file() does a syntax and plausibility check of */
/* the currently loaded file.                          */
/*-----------------------------------------------------*/

void test_file( FL_OBJECT *a, long b )
{
	static bool running_test = UNSET;
	static bool user_break = UNSET;
	struct stat file_stat;
	static bool in_test = UNSET;


	a->u_ldata = 0;
	b = b;

	notify_conn( BUSY_SIGNAL );

	/* While program is being tested the test can be aborted by pressing the
	   test button again - in this case we simply throw an exception */

	if ( running_test )
	{
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		user_break = SET;
		delete_devices( );                       /* run the exit hooks ! */
		eprint( FATAL, "Test of program aborted, received user break.\n" );
		notify_conn( UNBUSY_SIGNAL );
		running_test = UNSET;
		THROW( EXCEPTION );
	}

	/* If fsc2 is too busy testing a program to react to clicks on the "Stop
	   Test" button (this seems to happen when what the program does is mostly
	   output to the error browser) and the user presses the button several
	   times strange things happen, especially the "Quit" button becoming
	   unusable.  The following helps avoiding to execute the handler again
	   while it's already running... */

	if ( in_test )
		return;
	else
		in_test = SET;

	/* Here starts the real action... */

	if ( ! is_loaded )
	{
		fl_show_alert( "Error", "Sorry, but no file is loaded.", NULL, 1 );
		notify_conn( UNBUSY_SIGNAL );
		in_test = UNSET;
		return;
	}
		
    if ( is_tested )
	{
		fl_show_alert( "Warning", "File has already been tested.", NULL, 1 );
		notify_conn( UNBUSY_SIGNAL );
		in_test = UNSET;
		return;
	}

	/* Before scanning the file reload it if the file on disk has changed in
	   between - quit if file can't be read again. */

	stat( in_file, &file_stat );
	if ( in_file_mod != file_stat.st_mtime )
	{
		if ( 1 == fl_show_choice( "EDL file on disk is newer than the loaded ",
								  "file. Reload file from disk?",
								  "",
								  2, "No", "Yes", "", 1 ) )
		{
			notify_conn( UNBUSY_SIGNAL );
			in_test = UNSET;
			return;
		}

		load_file( main_form->browser, 1 );
		if ( ! is_loaded )
		{
			in_test = UNSET;
			return;
		}
		notify_conn( BUSY_SIGNAL );
	}

	/* While the test is run the only accessible button is the test button
	   which serves as a stop button for the test - so all the others got to
	   be deactivated while the test is run */

	fl_clear_browser( main_form->error_browser );

	fl_deactivate_object( main_form->Load );
	fl_set_object_lcol( main_form->Load, FL_INACTIVE_COL );
	fl_deactivate_object( main_form->reload );
	fl_set_object_lcol( main_form->reload, FL_INACTIVE_COL );
	fl_deactivate_object( main_form->Edit );
	fl_set_object_lcol( main_form->Edit, FL_INACTIVE_COL );
	fl_deactivate_object( main_form->run );
	fl_set_object_lcol( main_form->run, FL_INACTIVE_COL );
	fl_deactivate_object( main_form->quit );
	fl_set_object_lcol( main_form->quit, FL_INACTIVE_COL );
	fl_set_object_label( main_form->test_file, "Stop Test" );
	fl_set_button_shortcut( main_form->test_file, "T", 1 );

	user_break = UNSET;
	running_test = SET;
	fl_set_cursor( FL_ObjWin( main_form->run ), XC_watch );
	state = scan_main( in_file );
	fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
	running_test = UNSET;

	if ( ! user_break )
	{
		is_tested = SET;                  /* show that file has been tested */
		a->u_ldata = 0;
	}
	else
	{
		a->u_ldata = 1;
		user_break = UNSET;
	}

	fl_set_object_label( main_form->test_file, "Test" );
	fl_set_button_shortcut( main_form->test_file, "T", 1 );
	fl_activate_object( main_form->Load );
	fl_set_object_lcol( main_form->Load, FL_BLACK );
	fl_activate_object( main_form->reload );
	fl_set_object_lcol( main_form->reload, FL_BLACK );
	fl_activate_object( main_form->Edit );
	fl_set_object_lcol( main_form->Edit, FL_BLACK );
	fl_activate_object( main_form->run );
	fl_set_object_lcol( main_form->run, FL_BLACK );
	fl_activate_object( main_form->quit );
	fl_set_object_lcol( main_form->quit, FL_BLACK );

	notify_conn( UNBUSY_SIGNAL );
	in_test = UNSET;
}


/*---------------------------------------------------------------*/
/* run_file() first does a check of the current file (if this is */
/* not already done) and on success starts the experiment.       */
/*---------------------------------------------------------------*/

void run_file( FL_OBJECT *a, long b )
{
	struct stat file_stat;
	char str1[ 128 ],
		 str2[ 128 ];


	a = a;
	b = b;

	notify_conn( BUSY_SIGNAL );

	if ( ! is_loaded )              /* check that there is a file loaded */
	{
		fl_show_alert( "Error", "Sorry, but no file is loaded.", NULL, 1 );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	if ( ! is_tested )              /* test file if not already done */
	{
		test_file( main_form->test_file, 1 );
		if ( main_form->test_file->u_ldata == 1 )  /* user break ? */
			return;
		notify_conn( BUSY_SIGNAL );
	}
	else
	{
		stat( in_file, &file_stat );
		if ( in_file_mod != file_stat.st_mtime )
		{
			if ( 1 != fl_show_choice( "EDL file on disk is newer than loaded",
									  "file. Reload the file from disk?",
									  "",
									  2, "No", "Yes", "", 1 ) )
			{
				load_file( main_form->browser, 1 );
				if ( ! is_loaded )
					return;
				is_tested = UNSET;
				test_file( main_form->test_file, 1 );
				if ( main_form->test_file->u_ldata == 1 )  /* user break ? */
					return;
				notify_conn( BUSY_SIGNAL );
			}
		}
	}

	if ( ! state )               /* return if program failed the test */
	{
		fl_show_alert( "Error", "Sorry, but test of file failed.", NULL, 1 );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	/* If there were non-fatal errors ask user if he wants to continue */

	if ( compilation.error[ SEVERE ] != 0 || compilation.error[ WARN ] != 0 )
	{
		if ( compilation.error[ SEVERE ] != 0 )
		{
			if ( compilation.error[ WARN ] != 0 )
			{
				sprintf( str1, "There where %d severe warnings",
						 compilation.error[ SEVERE ] );
				sprintf( str2, "and %d warnings.", compilation.error[ WARN ] );
			}
			else
			{
				sprintf( str1, "There where %d severe warnings.",
						 compilation.error[ SEVERE ] );
				str2[ 0 ] = '\0';
			}
		}
		else
		{
			sprintf( str1, "There where %d warnings.",
					 compilation.error[ WARN ] );
			str2[ 0 ] = '\0';
		}

		if ( 1 == fl_show_choice( str1, str2, "Continue to run the program?",
								  2, "No", "Yes", "", 1 ) )
		{
			notify_conn( UNBUSY_SIGNAL );
			return;
		}
	}		

	/* Finally start the experiment */

	TRY
	{
		run( );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		fl_show_alert( "Error", "Sorry, can't run the experiment.",
					   "See browser for more information.", 1 );
	}
}


/*--------------------------------------------------------------------*/
/* display_file() is used to put the contents of a file into the main */
/* browser, numbering the lines and expanding tab chars.              */
/* ->                                                                 */
/*   * name of the file to be displayed                               */
/*   * FILE pointer of the file                                       */
/*--------------------------------------------------------------------*/

static bool display_file( char *name, FILE *fp )
{
	int len, key;
	long lc, cc, i;                         /* line and char counter */
	char line[ FL_BROWSER_LINELENGTH ];
	char *lp;


	/* Determine number of lines (and maximum number of digits) in order to
	   find out about proper formating of line numbers */

	if ( ( lc = get_file_length( name, &len ) ) <= 0 )  /* error ? */
	{
		switch ( ( int ) lc )
		{
			case 0 :                  /* file length is zero */
				fl_show_alert( "Error", "File is empty.", NULL, 1 );
				return FAIL;

			case -1 :                 /* not enough memory left */
				return FAIL;

			case -2 :                 /* popen() failure */
				fl_show_alert( "Error", "Can't determine length of file:",
							   name, 1 );
				return FAIL;
		}
	}

	/* Freeze browser, read consecutive lines, prepend line numbers (after
	   expanding tab chars) and send lines to browser */

	fl_clear_browser( main_form->browser );
	fl_clear_browser( main_form->error_browser );
	fl_freeze_form( main_form->browser->form );

	for ( i = 1; i <= lc; ++i )
	{
		sprintf( line, "%*ld: ", len, i );
		lp = line + len + 2;
		cc = 0;
		while ( ( key = fgetc( fp ) ) != '\n' &&
			    key != EOF && ++cc < FL_BROWSER_LINELENGTH - len - 9 )
		{
			if ( ( char ) key != '\t' )
				*lp++ = ( char ) key;
			else                         /* handling of tab chars */
			{
				do
					*lp++ = ' ';
				while ( cc++ % TAB_LENGTH &&
						cc < FL_BROWSER_LINELENGTH - len - 3 )
					;
				cc--;
			}
		}
		*lp = '\0';

		/* Color section headings */

		lp = line + len + 2;
		if ( ! strncmp ( lp, "DEVICES:", 8 ) ||
			 ! strncmp ( lp, "DEVICE:", 7 ) ||
			 ! strncmp ( lp, "DEVS:\n", 7 ) ||
			 ! strncmp ( lp, "DEV:", 4 ) ||
			 ! strncmp ( lp, "VARIABLES:", 10 ) ||
			 ! strncmp ( lp, "VARIABLE:", 9 ) ||
			 ! strncmp ( lp, "VARS:", 5 ) ||
			 ! strncmp ( lp, "VAR:", 4 ) ||
			 ! strncmp ( lp, "ASSIGNMENTS:", 12 ) ||
			 ! strncmp ( lp, "ASSIGNMENT:", 11 ) ||
			 ! strncmp ( lp, "ASS:", 4 ) ||
			 ! strncmp ( lp, "PREPARATIONS:", 13 ) ||
			 ! strncmp ( lp, "PREPARATION:", 12 ) ||
			 ! strncmp ( lp, "PREPS:", 6 ) ||
			 ! strncmp ( lp, "PREP:", 5 ) ||
			 ! strncmp ( lp, "EXPERIMENT:", 11 ) ||
			 ! strncmp ( lp, "EXP:", 4 ) ||
			 ! strncmp ( lp, "ON_STOP:", 8 ) )
		{
			memmove( line + 6, line, strlen( line ) + 1 );
			line[ 0 ] = '@';
			line[ 1 ] = 'C';
			line[ 2 ] = '2';
			line[ 3 ] = '4';
			line[ 4 ] = '@';
			line[ 5 ] = 'f';
		}

		/* Also color all lines staring with a '#' */

		if ( *lp == '#' )
		{
			memmove( line + 5, line, strlen( line ) + 1 );
			line[ 0 ] = '@';
			line[ 1 ] = 'C';
			line[ 2 ] = '4';
			line[ 3 ] = '@';
			line[ 4 ] = 'f';
		}

		fl_add_browser_line( main_form->browser, line );
	}

	/* Unfreeze and thus redisplay the browser */

	fl_unfreeze_form( main_form->browser->form );
	return OK;
}


/*--------------------------------------------------------------------*/  
/* Does everything that needs to be done (i.e. deallocating memory,   */
/* unloading the device drivers, reinitializing all kinds of internal */
/* structures etc.) before a new file can be loaded.                  */
/*--------------------------------------------------------------------*/  

void clean_up( void )
{
	int i;


	/* clear up the compilation structure */

	for ( i = 0; i < 3; ++i )
		compilation.error[ 3 ] = 0;
	for ( i = DEVICES_SECTION; i <= EXPERIMENT_SECTION; ++i )
		compilation.sections[ i ] = UNSET;

	/* Deallocate memory used for file names */

	Fname = T_free( Fname );

	/* run exit hook functions and unlink modules */

	delete_devices( );
	need_GPIB = UNSET;
	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
		need_Serial_Port[ i ] = UNSET;

	/* delete function list */

	functions_exit( );

	/* delete device list */

	delete_device_name_list( );

	/* (re)initialize the structure for the pulser modules */

	pulser_struct_init( );

	/* clear up structures for phases */

	phases_clear( );

	/* Delete everything connected with user buttons */

	tools_clear( );

	/* delete variables and get rid of variable stack */

	vars_clean_up( );

	/* delete stored program */

	forget_prg( );

	/* unset used flags for all serial ports */

	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
		need_Serial_Port[ i ] = UNSET;

	free_graphics( );
}


/*------------------------------------------------------------*/  
/*------------------------------------------------------------*/  

void run_help( FL_OBJECT *a, long b )
{
	int res;


	/* Keep the compiler happy... */

	a = a;
	b = b;

	notify_conn( BUSY_SIGNAL );

	/* Fork and run help browser in child process */

	if ( ( res = fork( ) ) == 0 )
		start_help_browser( );

	if ( res == -1 )                                /* fork failed ? */
		fl_show_alert( "Error", "Sorry, unable to start the help browser.",
					   NULL, 1 );

	notify_conn( UNBUSY_SIGNAL );
}


/*------------------------------------------------------------*/  
/*------------------------------------------------------------*/  

static void start_help_browser( void )
{
	char *browser;
	char *av[ 4 ] = { NULL, NULL, NULL, NULL };


	/* Try to figure out which browser to use... */

	browser = getenv( "BROWSER" );

	if ( browser && ! strcasecmp( browser, "Opera" ) )
	{
		av[ 0 ] = T_strdup( "opera" );
		av[ 1 ] = T_strdup( "-newbrowser" );
		av[ 2 ] = get_string( 21 + strlen( docdir ) );
		strcpy( av[ 2 ], "file:/" );
		strcat( av[ 2 ], docdir );
		strcat( av[ 2 ], "fsc2_frame.html" );
	}
	else
	{
		/* If netscape isn't running start it, otherwise ask it to just open
		   a new window */

		av[ 0 ] = T_strdup( "netscape" );

		if ( system( "xwininfo -name Netscape >/dev/null 2>&1" ) )
		{
			av[ 1 ] = get_string( 20 + strlen( docdir ) );
			strcpy( av[ 1 ], "file:" );
			strcat( av[ 1 ], docdir );
			strcat( av[ 1 ], "fsc2_frame.html" );
		}
		else
		{
			av[ 1 ] = T_strdup( "-remote" );
			av[ 2 ] = get_string( 40 + strlen( docdir ) );
			strcpy( av[ 2 ], "openURL(file:" );
			strcat( av[ 2 ], docdir );
			strcat( av[ 2 ], "fsc2_frame.html,new-window)" );
		}
	}

	execvp( av[ 0 ], av );
	_exit( EXIT_FAILURE );
}


/*-----------------------------------------------------------------------*/
/* Sets up the signal handlers for all kinds of signals the main process */
/* could receive. This probably looks a bit like overkill, but I just    */
/* wan't to sure it doesn't get killed by some meaningless singnals and, */
/* on the other hand, that on deadly signals it still gets a chance to   */
/* try to get rid of shared memory and kill the other processes etc.     */
/*-----------------------------------------------------------------------*/

static void set_main_signals( void )
{
	struct sigaction sact;
	int sig_list[ ] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE,
						SIGSEGV, SIGPIPE, SIGTERM, SIGUSR1, SIGUSR2, SIGCHLD,
						SIGCONT, SIGTTIN, SIGTTOU, SIGBUS, SIGVTALRM, 0 };
	int i;


	for ( i = 0; sig_list[ i ] != 0; i++ )
	{
		sact.sa_handler = main_sig_handler;
		sigemptyset( &sact.sa_mask );
		sact.sa_flags = 0;
		if ( sigaction( sig_list[ i ], &sact, NULL ) < 0 )
		{
			fprintf( stderr, "Failed to initialize fsc2.\n" );
			exit( -1 );
		}
	}
}


/*------------------------------------------------------------*/  
/*------------------------------------------------------------*/  

void main_sig_handler( int signo )
{
	char line[ MAXLINE ];
	int count;
	int errno_saved;


	switch ( signo )
	{
		case SIGCHLD :
			errno_saved = errno;
			wait( NULL );
			errno = errno_saved;
			return;

		case SIGUSR2 :
			conn_child_replied = SET;
			return;

		case SIGUSR1 :
			errno_saved = errno;
			while ( ( count = read( conn_pd[ READ ], line, MAXLINE ) ) == -1 &&
					errno == EINTR )
				;
			line[ count - 1 ] = '\0';
			main_form->Load->u_ldata = ( long ) line[ 0 ];
			if ( line[ 1 ] == 'd' )
				delete_file = SET;
			main_form->Load->u_cdata = T_strdup( line + 2 );
			fl_trigger_object( main_form->Load );
			errno = errno_saved;
			return;

		/* Ignored signals : */

		case SIGHUP  : case SIGINT  : case SIGALRM : case SIGCONT :
		case SIGTTIN : case SIGTTOU : case SIGVTALRM :
			return;

		/* All the remaining signals are deadly... */

		default :
			final_exit_handler( );
			if ( signo != SIGTERM )
				fprintf( stderr, "fsc2 (%d) killed by %s signal.\n",
						 getpid( ), strsignal( signo ) );
			exit( -1 );
	}
}


/*------------------------------------------------------------*/  
/*------------------------------------------------------------*/  

void notify_conn( int signo )
{
	/* Don't send signal to the process responsible for connections if it
	   doesn't exist (in load_file() at the very start) or when the experiment
	   is running - in this case fsc2 is busy anyway and the connection
	   process has already been informed about this */

	if ( conn_pid <= 0 || child_pid > 0 )
		return;

	kill( conn_pid, signo );

	/* Wait for reply from child but avoid waiting when it in fact already
	   replied (as indicated by the variable) */

	while ( ! conn_child_replied )
		usleep( 50000 );
	conn_child_replied = UNSET;
}


/*------------------------------------------------------------*/  
/*------------------------------------------------------------*/  

void usage( void )
{
	char *dd;

	dd = T_strdup( docdir );
	if ( dd[ strlen( dd ) - 1 ] == '/' )
		dd[ strlen( dd ) - 1 ] = '\0';
	fprintf( stderr, "Usage: fsc2 [OPTIONS]... [FILE]\n"
			 "A program for remote control of EPR spectrometers\n"
			 "OPTIONS:\n"
			 "  -t FILE    run syntax check on FILE and exit (no graphics)\n"
			 "  -T FILE    run syntax check on FILE\n"
			 "  -S FILE    start interpreting FILE (i.e. start the "
			 "experiment)\n"
			 "  -d FILE    delete input file FILE when fsc2 is done with "
			 "it.\n"
			 "  -geometry geometry\n"
			 "             specify preferred size and position of main "
			 "window\n"
			 "  -displayGeometry geometry\n"
			 "             specify preferred size and position of the "
			 "display window\n"
			 "  -cutGeometry geometry\n"
			 "             set the preferred size and position of the "
			 "cross-section window\n"
			 "  -toolGeometry geometry\n"
			 "             specify preferred position of the user defined "
			 "tool window\n"
			 "  -browserFontSize number\n"
			 "             set the size of the font used for browsers\n"
			 "  -buttonFontSize number\n"
			 "             set the size of the font used for buttons\n"
			 "  -sliderFontSize number\n"
			 "             set the size of the font used for sliders\n"
			 "  -inputFontSize number\n"
			 "             set the size of the font for input and output "
			 "fields\n"
			 "  -helpFontSize number\n"
			 "             set the size of the font used for popup help "
			 "texts\n"
			 "  -axisFont font\n"
			 "             set the font to be used in the axis in the "
			 "display window\n"
			 "  -h, --help\n"
			 "             display this help text and exit\n\n"
			 "For a complete documentation see either %s/fsc2.ps,\n"
			 "%s/fsc2.pdf or %s/fsc2_frame.html\n"
			 "or type \"info fsc2\".\n", dd, dd, dd );
	T_free( dd );
	exit( EXIT_SUCCESS );
}
