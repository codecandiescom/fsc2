/*
  $Id$
*/

#define FSC2_MAIN

#include "fsc2.h"

#if defined MDEBUG
#include <mcheck.h>
#endif




/* Locally used global variables */

bool is_loaded = UNSET;       /* set when EDL file is loaded */
bool is_tested = UNSET;       /* set when EDL file has been  tested */
bool state = UNSET;           /* set when EDL passed the tests */
static char *in_file = NULL;  /* used for name of input file */
static time_t in_file_mod = 0;


float *x;
float *y;


/* Locally used functions */


static bool xforms_init( int *argc, char *argv[ ] );
static void xforms_close( void );
static bool display_file( char *name, FILE *fp );



/**************************/
/*     Here we go...      */
/**************************/

int main( int argc, char *argv[ ] )
{
#if defined MDEBUG
	if ( mcheck( NULL ) != 0 )
	{
		printf( "Can't start mcheck() !\n" );
		return EXIT_FAILURE;
	}
#endif

	/* With the option "-t" just test the file and output results to stderr
	   - this can be used to test a file e.g. with emacs' "M-x compile"
	   feature or from the shell without any graphics stuff involved */

	if ( argc > 1 && ! strcmp( argv[ 1 ], "-t" ) )
	{
		if ( argv[ 2 ] == NULL )          /* no file name with "-t" option ? */
		{
			fprintf( stderr, "fsc2 -t: No input file\n" );
			return EXIT_FAILURE;
		}

		just_testing = SET;        /* signal "just_testing"-mode to eprint() */
		return scan_main( argv[ 2 ] ) ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	else
		just_testing = UNSET;

	/* Check via the lock file if there is already a process holding a lock,
	   otherwise create one. This, as well as the following check for stale
	   shared memory segments has to be done with the effective user ID, i.e.
	   the UID of fsc2.*/

	if ( ! fsc2_locking( ) )
		return EXIT_FAILURE;

	/* Remove orphaned shared memory segments resulting from previous
	   crashes */

	delete_stale_shms( );

	/* Now we set the effective user ID to the real user ID, we only switch
	   back when creating or attaching shared memory segments */

	EUID = geteuid( );
	seteuid( getuid( ) );
	setegid( getgid( ) );

	/* Initialize xforms stuff, quit on error */

	if ( ! 	xforms_init( &argc, argv ) )
	{
		unlink( LOCKFILE );
		return EXIT_FAILURE;
	}

	fl_add_signal_callback( SIGCHLD, sigchld_handler, NULL );

	/* If there is a file as argument try to load it */

	if ( argc > 1 )
	{
		TRY
		{
			in_file = get_string_copy( argv[ 1 ] );
			TRY_SUCCESS;
		}
		OTHERWISE
			return EXIT_FAILURE;

		load_file( main_form->browser, 1 );
	}

	/* Loop until quit button is pressed and there is no experiment running */

	while ( fl_do_forms( ) != main_form->quit )
		;

	/* Do everything necessary to end the program */

	clean_up( );
	xforms_close( );
	if ( in_file != NULL )
		T_free( in_file );

	/* Delete the lock file */

	setuid( EUID );
	unlink( LOCKFILE );

	/* Make sure the TRY/CATCH stuff worked out right */

	assert( exception_env_stack_pos == 0 );

	return EXIT_SUCCESS;
}


/*-------------------------------------------------*/
/* xforms_init() registers the program with XFORMS */
/* and creates all the forms needed by the program.*/
/*-------------------------------------------------*/

static bool xforms_init( int *argc, char *argv[] )
{
	FL_Coord h, H;
	FL_Coord x1, y1, w1, h1, x2, y2, w2, h2;


	if ( fl_initialize( argc, argv, "fsc2", 0, 0 ) == NULL )
		return FAIL;

	/* Set some properties of goodies */

#if ( SIZE == HI_RES )
	fl_set_tooltip_font( FL_NORMAL_STYLE, FL_MEDIUM_SIZE );
	fl_set_fselector_fontsize( FL_LARGE_SIZE );
	fl_set_goodies_font( FL_NORMAL_STYLE, FL_LARGE_SIZE );
	fl_set_oneliner_font( FL_NORMAL_STYLE, FL_LARGE_SIZE );
#else
	fl_set_tooltip_font( FL_NORMAL_STYLE, FL_TINY_SIZE );
	fl_set_fselector_fontsize( FL_SMALL_SIZE );
	fl_set_goodies_font( FL_NORMAL_STYLE, FL_SMALL_SIZE );
	fl_set_oneliner_font( FL_NORMAL_STYLE, FL_SMALL_SIZE );
#endif	

	/* Create and display the main form */

	main_form = create_form_fsc2( );

	fl_set_object_helper( main_form->Load, "Load new EDL program" );
	fl_set_object_helper( main_form->Edit, "Edit loaded EDL program" );
	fl_set_object_helper( main_form->reload, "Reload EDL program" );
	fl_set_object_helper( main_form->test_file, "Start syntax and "
						  "plausibility check" );
	fl_set_object_helper( main_form->run, "Start loaded EDL program" );
	fl_set_object_helper( main_form->quit, "Quit fsc2" );
	fl_set_object_helper( main_form->bug_report, "Mail a bug report" );

#if ( SIZE == HI_RES )
	fl_set_browser_fontsize( main_form->browser, FL_LARGE_SIZE );
#else
	fl_set_browser_fontsize( main_form->browser, FL_SMALL_SIZE );
#endif
	fl_set_browser_fontstyle( main_form->browser, FL_FIXED_STYLE );

#if ( SIZE == HI_RES )
	fl_set_browser_fontsize( main_form->error_browser, FL_LARGE_SIZE );
#else
	fl_set_browser_fontsize( main_form->error_browser, FL_SMALL_SIZE );
#endif
	fl_set_browser_fontstyle( main_form->error_browser, FL_FIXED_STYLE );

	fl_get_object_geometry( main_form->browser, &x1, &y1, &w1, &h1 );
	fl_get_object_geometry( main_form->error_browser, &x2, &y2, &w2, &h2 );
	h = y2 - y1 - h1;
	H = h1 + h2 + h;
	fl_set_slider_value( main_form->win_slider,
						 ( double ) ( h1 + h / 2 ) / H );

	fl_show_form( main_form->fsc2, FL_PLACE_MOUSE | FL_FREE_SIZE,
				  FL_FULLBORDER, "fsc2" );

	/* Create the form for writing a comment */

	input_form = create_form_input_form( );

	return OK;
}


/*-----------------------------------------*/
/* xforms_close() closes and deletes all   */
/* forms previously needed by the program. */
/*-----------------------------------------*/

static void xforms_close( void )
{
	if ( fl_form_is_visible( main_form->fsc2 ) )
		fl_hide_form( main_form->fsc2 );
	fl_free_form( main_form->fsc2 );
}


/*-------------------------------------------------------------------*/
/* load_file() is used for loading or reloading a file, depending on */
/* the value of the flag 'reload'. It's also the callback function   */
/* for the Load- and Reload-buttons.                                 */
/* reload == 0: read new file, reload == 1: reload file              */
/*-------------------------------------------------------------------*/

void load_file( FL_OBJECT *a, long reload )
{
	static const char *fn;
	static char *old_in_file;
	FILE *fp;
	struct stat file_stat;


	a = a;

	/* If new file is to be loaded get its name, otherwise use previous name */

	if ( ! reload )
	{
		fn = fl_show_fselector( "Select input file:", NULL, "*.edl", NULL );
		if ( fn == NULL )
			return;
	}
	else
		fn = in_file;

	/* If this is not a reload save name of file */

	if ( ! reload )
	{
		old_in_file = in_file;

		TRY
		{
			in_file = get_string_copy( fn );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			in_file = old_in_file;
			return;
		}

		if ( old_in_file != NULL )
			T_free( old_in_file );
	}

	/* Quit if this is a reload but name of previous file is empty */

	if ( reload && fn == '\0' )
	{
		fl_show_alert( "Error", "Sorry, no file is loaded yet.", NULL, 1 );
		return;
	}

	/* Test if the file is readable and can be opened */

	if ( access( fn, R_OK ) == -1 )
	{
		if ( errno == ENOENT )
			fl_show_alert( "Error", "Sorry, file not found:", fn, 1 );
		else
			fl_show_alert( "Error", "Sorry, no permission to read file:",
						   fn, 1 );
		return;
	}

	if ( ( fp = fopen( fn, "r" ) ) == NULL )
	{
		fl_show_alert( "Error", "Sorry, can't open file:", fn, 1 );
		return;
	}

	/* get modification time of file */

	stat( fn, &file_stat );
	in_file_mod = file_stat.st_mtime;

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
}


/*---------------------------------------------------------------------*/
/* edit_file() allows to edit the current file (but it's also possible */
/* to edit a new file if there is no file loaded) and is the callback  */
/* function for the Edit-button.                                       */
/*---------------------------------------------------------------------*/

void edit_file( FL_OBJECT *a, long b )
{
	char *ed, *ep;
	char **argv;
	int argc = 1, i;
	int res;


	a = a;
	b = b;

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

	/* Fork and execute editor in child process */

	if ( ( res = fork( ) ) == 0 )
	{
		/* Special treatment for emacs and xemacs: if emacs is called without
		   the '-nw' option it will create its own window - so we don't embed
		   it into a xterm - the same holds for xemacs which always creates
		   its own window. */

		if ( ! strcmp( strip_path( argv[ 2 ] ), "xemacs" ) )
			execvp( argv[ 2 ], argv + 2 );

		if ( ! strcmp( strip_path( argv[ 2 ] ), "emacs" ) )
		{
			for ( i = 3; argv[ i ] && strcmp( argv[ i ], "-nw" ); ++i )
				;
			if ( argv[ i ] == NULL )                    /* no '-nw' flag */
				execvp( argv[ 2 ], argv + 2 );
			else
				execvp( "xterm", argv );
		}
		else
			execvp( "xterm", argv );                /* all other editors */

		/* If this point is reached at all the invocation of the editor failed
		   - tell the parent by setting a special return value */

		_exit( EDITOR_FAILED );
	}

	if ( res == -1 )                                /* fork failed ? */
		fl_show_alert( "Error", "Sorry, unable to start the editor.",
					   NULL, 1 );
	T_free( argv );
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


	a->u_ldata = 0;
	b = b;

	/* While program is being tested the test can be aborted by pressing the
	   test button again - in this case we simply throw an exception */

	if ( running_test )
	{
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		user_break = SET;
		delete_devices( );                       /* run the exit hooks ! */
		eprint( FATAL, "Test of program aborted, received user break.\n" );
		THROW( EXCEPTION );
	}

	/* Here starts the real action... */

	if ( ! is_loaded )
	{
		fl_show_alert( "Error", "Sorry, but no file is loaded.", NULL, 1 );
		return;
	}
		
    if ( is_tested )
	{
		fl_show_alert( "Warning", "File has already been tested.", NULL, 1 );
		return;
	}

	/* Before scanning the file reload it if the file on disk has changed in
	   between - quit if file can't be read again. */

	stat( in_file, &file_stat );
	if ( in_file_mod != file_stat.st_mtime )
	{
		if ( 1 == fl_show_choice( "EDL file on disk is newer than loaded.",
								  "file. Reload the file from disk?",
								  "",
								  2, "No", "Yes", "", 1 ) )
			return;
		load_file( main_form->browser, 1 );
		if ( ! is_loaded )
			return;
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

	if ( ! is_loaded )              /* check that there is a file loaded */
	{
		fl_show_alert( "Error", "Sorry, but no file is loaded.", NULL, 1 );
		return;
	}

	if ( ! is_tested )              /* test file if not already done */
	{
		test_file( main_form->test_file, 1 );
		if ( main_form->test_file->u_ldata == 1 )  /* user break ? */
			return;
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
			}
		}
	}

	if ( ! state )               /* return if program failed the test */
	{
		fl_show_alert( "Error", "Sorry, but test of file failed.", NULL, 1 );
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

		if ( 1 == fl_show_choice( str1, str2, "Continue running the program?",
								  2, "No", "Yes", "", 1 ) )
			return;
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
	char line[ FL_BROWSER_LINELENGTH + 1 ]; /* used to store the lines */
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
			    key != EOF && ++cc < FL_BROWSER_LINELENGTH - len - 2 )
		{
			if ( ( char ) key != '\t' )
				*lp++ = ( char ) key;
			else                         /* handling of tab chars */
			{
				do
					*lp++ = ' ';
				while ( cc++ % TAB_LENGTH &&
						cc < FL_BROWSER_LINELENGTH - len - 2 )
					;
				cc--;
			}
		}
		*lp = '\0';

		fl_add_browser_line( main_form->browser, line );
	}

	/* Unfreeze and thus redisplay the browser */

	fl_unfreeze_form( main_form->browser->form );
	return OK;
}


/*-------------------------------------------------------*/
/* sigchld_handler() is the default SIGCHLD handler used */
/* to avoid having to many zombies hanging around.       */
/*-------------------------------------------------------*/

void sigchld_handler( int sig_type, void *data )
{
	int status;


	data = data;

	assert( sig_type == SIGCHLD );

	/* Get exit status of child and display information if necessary */

	wait( &status );
	if ( status == EDITOR_FAILED )
		fl_show_alert( "Error", "Sorry, unable to open editor.",
					   NULL, 1 );
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

	if ( Fname != NULL )
		T_free( Fname );
	Fname = NULL;

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
/* Callback function for movements of the slider that adjusts */
/* the sizes of the program and the error/output browser      */
/*------------------------------------------------------------*/  

void win_slider_callback( FL_OBJECT *a, long b )
{
	FL_Coord h, H;
	FL_Coord new_h1 ;
	FL_Coord x1, y1, w1, h1, x2, y2, w2, h2;


	b = b;

	fl_freeze_form( main_form->fsc2 );

	fl_get_object_geometry( main_form->browser, &x1, &y1, &w1, &h1 );
	fl_get_object_geometry( main_form->error_browser, &x2, &y2, &w2, &h2 );

	h = y2 - y1 - h1;
	H = y2 - y1 + h2;

	new_h1 = ( FL_Coord ) ( H * fl_get_slider_value( a ) - h / 2 );
	if ( new_h1 < 50 )
	{
		new_h1 = 50;
		fl_set_slider_value( a, ( double ) ( new_h1 + h / 2 ) / H );
	}
	if ( new_h1 > H - h - 50 )
	{
		new_h1 = H - h - 50;
		fl_set_slider_value( a, ( double ) ( new_h1 + h / 2 ) / H );
	}

	fl_set_object_size( main_form->browser, w1, new_h1 );
	fl_set_object_geometry( main_form->error_browser, x2, y1 + new_h1 + h,
							w2, H - ( new_h1 + h ) );
	fl_unfreeze_form( main_form->fsc2 );
}
