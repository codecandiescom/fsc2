/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#if defined( FSC2_MDEBUG ) || defined( LIBC_MDEBUG )
#include <mcheck.h>
#endif

#include <sys/utsname.h>
#include "serial.h"


/* Global variables */

const char *prog_name;           /* Name the program was started with */

INTERNALS Internals;
EDL_Stuff EDL;
COMMUNICATION Comm;

bool need_GPIB = UNSET;          /* Flag, set if GPIB bus is needed */

const char *Channel_Names[ NUM_CHANNEL_NAMES ] =
	{ "CH1", "CH2", "CH3", "CH4", "MATH1", "MATH2", "MATH3",
	  "REF1", "REF2", "REF3", "REF4", "AUX", "AUX1", "AUX2", "LINE",
	  "MEM_C", "MEM_D", "FUNC_E", "FUNC_F", "EXT", "EXT10",
	  "CH0", "CH5", "CH6", "CH7", "CH8", "CH9", "CH10", "CH11",
	  "CH12", "CH13", "CH14", "CH15",
	  "DEFAULT_SOURCE", "SOURCE_0", "SOURCE_1", "SOURCE_2", "SOURCE_3",
	  "NEXT_GATE", "TIMEBASE_1", "TIMEBASE_2" };

const char *Phase_Types[ NUM_PHASE_TYPES ] = { "+X", "-X", "+Y", "-Y" };

volatile sig_atomic_t conn_child_replied;


/* Locally used global variables */

static bool is_loaded = UNSET;       /* set when EDL file is loaded */
static bool is_tested = UNSET;       /* set when EDL file has been tested */
static bool state = UNSET;           /* set when EDL passed the tests */
static FILE *in_file_fp = NULL;
static time_t in_file_mod = 0;
static char *title = NULL;
static bool delete_file = UNSET;
static bool delete_old_file = UNSET;
static volatile sig_atomic_t fsc2_death = 0;

extern FL_resource xresources[ ];    /* from xinit.c */


/* Locally used functions */

static void globals_init( const char *pname );
static bool get_edl_file( char *fname );
static void check_run( void );
static void test_machine_type( void );
static int scan_args( int *argc, char *argv[ ], char **fname );
static void final_exit_handler( void );
static bool display_file( char *name, FILE *fp );
static void start_editor( void );
static void start_help_browser( void );
static void set_main_signals( void );
static void conn_request_handler( void );


/**************************/
/*     Here we go...      */
/**************************/

int main( int argc, char *argv[ ] )
{
	char *fname = NULL;
	int conn_pid;


#if defined( FSC2_MDEBUG ) || defined( LIBC_MDEBUG )
	if ( mcheck( NULL ) != 0 )
	{
		fprintf( stderr, "Can't start mcheck() !\n" );
		return EXIT_FAILURE;
	}
#endif

#if defined LIBC_MDEBUG
	mtrace( );
#endif

	/* Initialization of global variables */

	globals_init( argv[ 0 ] );

	/* Figure out if the machine has an INTEL type processor */

	test_machine_type( );

	/* Run a first test of the command line arguments */

	Internals.cmdline_flags = scan_args( &argc, argv, &fname );

	/* Check if other instances of fsc2 are already running. If there are
	   and the current instance of fsc2 wasn't started with the non-exclusive
	   option we've got to quit. If there's no other instance we've got to
	   spawn a process that deals with further instances of fsc2 and takes
	   care of shared memory segments. */

	if ( ( Internals.conn_pid =
			  check_spawn_fsc2d( ! ( Internals.cmdline_flags & NON_EXCLUSIVE ),
								 in_file_fp ) ) == -1 )
		return EXIT_FAILURE;

	/* Initialize xforms stuff, quit on error */

	if ( ! xforms_init( &argc, argv ) )
	{
		raise_permissions( );
		lower_permissions( );
		return EXIT_FAILURE;
	}

	/* If we don't know yet about an input file and there are command
	   line arguments left take it to be the input file */

	if ( argc > 1 && fname == NULL )
	{
		Internals.cmdline_flags |= DO_LOAD;
		fname = argv[ 1 ];
	}

	/* If '--delete' was given on the command line set flags that says that
	   the input files needs to be deleted */

	if ( fname != NULL && Internals.cmdline_flags & DO_DELETE )
		delete_file = delete_old_file = SET;

	/* If there is a file argument try to load it */

	if ( Internals.cmdline_flags & DO_LOAD )
	{
		if ( ! get_edl_file( fname ) )
			return EXIT_FAILURE;

		conn_pid = Internals.conn_pid;
		Internals.conn_pid = 0;
		load_file( GUI.main_form->browser, 1 );
		Internals.conn_pid = conn_pid;
	}

	/* In batch mode try to get the next file if the first input file
	   could not be accessed until we have one. */

	while ( Internals.cmdline_flags & BATCH_MODE && ! is_loaded )
	{
		int i;

		if ( argc == 1 )
		{
			fprintf( stderr, "No input files found that could be read in.\n" );
			return EXIT_FAILURE;
		}

		EDL.in_file = T_free( EDL.in_file );
		fname = argv[ 1 ];

		if ( ! get_edl_file( fname ) )
			return EXIT_FAILURE;

		conn_pid = Internals.conn_pid;
		Internals.conn_pid = 0;
		load_file( GUI.main_form->browser, 1 );
		Internals.conn_pid = conn_pid;

		for ( i = 1; i < argc; i++ )
			argv[ i ] = argv[ i + 1 ];
		argc -= 1;
	}

	/* Set handler for signal that's going to be send by the process that
	   accepts external connections (to be spawned next) */

	set_main_signals( );
	atexit( final_exit_handler );

	fl_set_idle_callback( idle_handler, NULL );

	if ( Internals.cmdline_flags & DO_CHECK )
		check_run( );

	/* Only if starting the server for external connections succeeds really
	   start the main loop */

	if ( Internals.conn_pid == 0 ||
		 ( Internals.conn_pid =
			   spawn_conn( Internals.cmdline_flags &
			   	   ( DO_TEST | DO_START ) && is_loaded, in_file_fp ) ) != -1 )
	{
		/* Trigger test or start of current EDL program if the appropriate
		   flags were passed to the program on the command line */

		if ( Internals.cmdline_flags & DO_TEST && is_loaded )
			fl_trigger_object( GUI.main_form->test_file );
		if ( Internals.cmdline_flags & DO_START && is_loaded )
			fl_trigger_object( GUI.main_form->run );

		/* If required send signal to the invoking process */

		if ( Internals.cmdline_flags & DO_SIGNAL )
			kill( getppid( ), SIGUSR1 );

		/* And, finally, here's the main loop of the program. In batch mode
		   we try to get the next file from the command line and start it. */

	run_next:

		while ( fl_do_forms( ) != GUI.main_form->quit )
			/* empty */ ;

		if ( Internals.cmdline_flags & BATCH_MODE && argc > 1 )
		{
			is_loaded = UNSET;

			while ( argc > 1 && ! is_loaded )
			{
				int i;

				EDL.in_file = T_free( EDL.in_file );
				fname = argv[ 1 ];

				if ( ! get_edl_file( fname ) )
					break;

				conn_pid = Internals.conn_pid;
				Internals.conn_pid = 0;
				load_file( GUI.main_form->browser, 1 );
				Internals.conn_pid = conn_pid;

				for ( i = 1; i < argc; i++ )
					argv[ i ] = argv[ i + 1 ];
				argc -= 1;
			}

			if ( is_loaded )
			{
				fl_trigger_object( GUI.main_form->run );
				goto run_next;
			}
		}
	}
	else
		fprintf( stderr, "fsc2: Internal failure on startup.\n" );

	T_free( title );
	clean_up( );
	xforms_close( );

	return EXIT_SUCCESS;
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

static void globals_init( const char *pname )
{
	/* As the very first action the effective UID and GID gets stored and
	   then the program lowers the permissions to the ones of the real UID
	   and GID, only switching back when creating or attaching to shared
	   memory segments or accessing log files. */

	Internals.EUID = geteuid( );
	Internals.EGID = getegid( );

	lower_permissions( );

    Internals.child_pid = 0;
    Internals.http_pid = 0;

	/* Set up a lot of global variables */

	Internals.state = STATE_IDLE;
	Internals.mode = PREPARATION;
	Internals.in_hook = UNSET;
	Internals.I_am = PARENT;
	Internals.just_testing = UNSET;
	Internals.exit_hooks_are_run = UNSET;
	Internals.tb_wait = TB_WAIT_NOT_RUNNING;
	Internals.http_server_died = UNSET;
	Internals.conn_request = UNSET;

	if ( pname != NULL )
		prog_name = pname;
	else
		prog_name = "fsc2";

	EDL.Lc = 0;
	EDL.in_file = NULL;
	EDL.Fname = NULL;
	EDL.Call_Stack = NULL;
	EDL.prg_token = NULL;
	EDL.On_Stop_Pos = -1;
	EDL.Var_List = NULL;
	EDL.Var_Stack = NULL;
	EDL.do_quit = UNSET;
	EDL.react_to_do_quit = SET;
	EDL.File_List = NULL;
	EDL.File_List_Len = 0;
    EDL.Device_List = NULL;
    EDL.Device_Name_List = NULL;
    EDL.Num_Pulsers = 0;

	Comm.data_semaphore = -1;
	Comm.request_semaphore = -1;
	Comm.MQ = NULL;
	Comm.MQ_ID = -1;

	GUI.is_init = UNSET;

	G.color_hash = NULL;

	/* Setup a structure used when dealing with serial ports */

	fsc2_serial_init( );
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

static bool get_edl_file( char *fname )
{
	TRY
	{
		/* We need the name of the file with the full path name (which
		   needs to be passed on to fsc2_clean to allow include files
		   in scripts be specified relative to the path of the script
		   instead of relative to the current working directory fsc2
		   was started from. */

		if ( fname[ 0 ] == '/' )
			EDL.in_file = T_strdup( fname );
		else
		{
			size_t size;
			char *buf;

			size = ( size_t ) pathconf( ".", _PC_PATH_MAX );
			buf = CHAR_P T_malloc( size );
			EDL.in_file = get_string( "%s/%s",
									  getcwd( buf, size ), fname );
			T_free( buf );
		}

		TRY_SUCCESS;
	}
	OTHERWISE
		return FAIL;

	return OK;
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

static void check_run( void )
{
	static bool user_break = UNSET;
	long i;


	fl_set_object_callback( GUI.main_form->test_file, NULL, 0 );
	fl_deactivate_object( GUI.main_form->Load );
	fl_set_object_lcol( GUI.main_form->Load, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->reload );
	fl_set_object_lcol( GUI.main_form->reload, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->Edit );
	fl_set_object_lcol( GUI.main_form->Edit, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->run );
	fl_set_object_lcol( GUI.main_form->run, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->quit );
	fl_set_object_lcol( GUI.main_form->quit, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->test_file );
	fl_set_object_lcol( GUI.main_form->test_file, FL_INACTIVE_COL );

	if ( ( in_file_fp = fopen( EDL.in_file, "r" ) ) == NULL )
		exit( EXIT_FAILURE );

	user_break = UNSET;
	fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_watch );

	if ( ! scan_main( EDL.in_file, in_file_fp ) || user_break ||
		 EDL.compilation.error[ FATAL ]  != 0 ||
		 EDL.compilation.error[ SEVERE ] != 0 ||
		 EDL.compilation.error[ WARN ]   != 0 )
		exit( EXIT_FAILURE );

	fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );

	fl_activate_object( GUI.main_form->Load );
	fl_set_object_lcol( GUI.main_form->Load, FL_BLACK );
	fl_activate_object( GUI.main_form->reload );
	fl_set_object_lcol( GUI.main_form->reload, FL_BLACK );
	fl_activate_object( GUI.main_form->Edit );
	fl_set_object_lcol( GUI.main_form->Edit, FL_BLACK );
	fl_activate_object( GUI.main_form->run );
	fl_set_object_lcol( GUI.main_form->run, FL_BLACK );
	fl_activate_object( GUI.main_form->quit );
	fl_set_object_lcol( GUI.main_form->quit, FL_BLACK );

	for ( i = 0; i < Internals.num_test_runs; i++ )
	{
		if ( ! run( ) )
			exit( EXIT_FAILURE );
		while ( fl_do_forms( ) != GUI.main_form->quit )
			/* empty */ ;
		if ( Internals.check_return != EXIT_SUCCESS )
			exit( Internals.check_return );
	}

	exit( EXIT_SUCCESS );
}


/*------------------------------------------------------------------*/
/* Figure out the machine type from the value returned by uname(),  */
/* currently i[3-6]86 will be treated as having an Intel compatible */
/* processor. Only for these types of processors and when running   */
/* under Linux some stuff needing assembler and used to help with   */
/* debugging can be used.                                           */
/*------------------------------------------------------------------*/

static void test_machine_type( void )
{
	struct utsname utsbuf;


	if ( uname( &utsbuf ) == 0 &&
		 utsbuf.machine[ 0 ] == 'i' &&
		 utsbuf.machine[ 1 ] >= '3' && utsbuf.machine[ 1 ] <= '6' &&
		 ! strncmp( utsbuf.machine + 2, "86", 2 ) &&
		 ! strcasecmp( utsbuf.sysname, "linux" ) )
		Internals.is_linux_i386 = SET;
	else
		Internals.is_linux_i386 = UNSET;
}


/*-------------------------------------------------------*/
/* Do a preliminary check of the command line arguments. */
/* The main handling of arguments is only done after the */
/* graphics initialisation, but some arguments have to   */
/* be dealt with earlier.                                */
/*-------------------------------------------------------*/

static int scan_args( int *argc, char *argv[ ], char **fname )
{
	int flags = 0;
	int cur_arg = 1;
	int i;


	if ( *argc == 1 )
		return flags;

	while ( cur_arg < *argc )
	{
		if ( strlen( argv[ cur_arg ] ) == 2 &&
			 ! strcmp( argv[ cur_arg ], "-t" ) )
		{
			if ( flags & DO_CHECK )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-t' and "
						"'-X' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & BATCH_MODE )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-t' and "
						"'-B' at once.\n" );
				usage( EXIT_FAILURE );
			}

			/* no file name with "-t" option ? */

			if ( argv[ ++cur_arg ] == NULL )
			{
				fprintf( stderr, "fsc2 -t: No input file.\n" );
				exit( EXIT_FAILURE );
			}

			Internals.just_testing = SET;    /* set "just_testing"-mode flag */

			seteuid( getuid( ) );
			setegid( getgid( ) );
			if ( ( in_file_fp = fopen( argv[ cur_arg ], "r" ) ) == NULL )
				exit( EXIT_FAILURE );
			exit( scan_main( argv[ cur_arg ], in_file_fp ) ?
				  EXIT_SUCCESS : EXIT_FAILURE );
		}

		if ( ( strlen( argv[ cur_arg ] ) == 2 &&
			   ! strcmp( argv[ cur_arg ], "-h" ) ) ||
			 ! strcmp( argv[ cur_arg ], "--help" ) )
			usage( EXIT_SUCCESS );

		if ( strlen( argv[ cur_arg ] ) == 2 &&
			 ! strcmp( argv[ cur_arg ], "-s" ) )
		{
			flags |= DO_SIGNAL;
			for ( i = cur_arg; i < *argc; i++ )
				argv[ i ] = argv[ i + 1 ];
			*argc -= 1;
			continue;
		}

		if ( ! strcmp( argv[ cur_arg ], "--delete" ) )
		{
			flags |= DO_DELETE;
			for ( i = cur_arg; i < *argc; i++ )
				argv[ i ] = argv[ i + 1 ];
			*argc -= 1;
			continue;
		}

		if ( ! strcmp( argv[ cur_arg ], "-noCrashMail" ) )
		{
			flags |= NO_MAIL;
			for ( i = cur_arg; i < *argc; i++ )
				argv[ i ] = argv[ i + 1 ];
			*argc -= 1;
			continue;
		}

		if ( ! strcmp( argv[ cur_arg ], "-noBalloons" ) )
		{
			flags |= NO_BALLOON;
			for ( i = cur_arg; i < *argc; i++ )
				argv[ i ] = argv[ i + 1 ];
			*argc -= 1;
			continue;
		}

		if ( ! strcmp( argv[ cur_arg ], "-non-exclusive" ) )
		{
			flags |= NON_EXCLUSIVE;
			for ( i = cur_arg; i < *argc; i++ )
				argv[ i ] = argv[ i + 1 ];
			*argc -= 1;
			continue;
		}

		if ( ! strncmp( argv[ cur_arg ], "-S", 2 ) )
		{
			if ( flags & DO_CHECK )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-S' and "
						"'-X'.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & DO_TEST )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-S' and "
						"'-T'.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & BATCH_MODE )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-S' and "
						"'-B' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( argv[ cur_arg ][ 2 ] == '\0' && *argc == cur_arg + 1 )
			{
				fprintf( stderr, "fsc2 -S: No input file.\n" );
				usage( EXIT_FAILURE );
			}

			if ( argv[ cur_arg ][ 2 ] != '\0' )
			{
				*fname = argv[ cur_arg ] + 2;
				for ( i = cur_arg; i < *argc; i++ )
					argv[ i ] = argv[ i + 1 ];
				*argc -= 1;
			}
			else
			{
				*fname = argv[ cur_arg + 1 ];
				for ( i = cur_arg; i < *argc - 1; i++ )
					argv[ i ] = argv[ i + 2 ];
				*argc -= 2;
			}

			flags |= DO_LOAD | DO_START;
			break;
		}

		if ( ! strncmp( argv[ cur_arg ], "-T", 2 ) )
		{
			if ( flags & DO_CHECK )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-T' and "
						"'-X' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & DO_START )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-T' and "
						"'-S' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & BATCH_MODE )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-T' and "
						"'-B' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( argv[ cur_arg ][ 2 ] == '\0' && *argc == cur_arg + 1 )
			{
				fprintf( stderr, "fsc2 -T: No input file.\n" );
				usage( EXIT_FAILURE );
			}

			if ( argv[ cur_arg ][ 2 ] != '\0' )
			{
				*fname = argv[ cur_arg ] + 2;
				for ( i = cur_arg; i < *argc; i++ )
					argv[ i ] = argv[ i + 1 ];
				*argc -= 1;
			}
			else
			{
				*fname = argv[ cur_arg + 1 ];
				for ( i = cur_arg; i < *argc - 1; i++ )
					argv[ i ] = argv[ i + 2 ];
				*argc -= 2;
			}

			flags |= DO_LOAD | DO_TEST;
			break;
		}

		if ( strlen( argv[ cur_arg ] ) == 2 &&
			 ! strcmp( argv[ cur_arg ], "-B" ) )
		{
			if ( flags & DO_CHECK )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-B' and "
						"'-t' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & DO_START )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-B' and "
						"'-S' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & DO_TEST )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-B' and "
						"'-T' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( argv[ cur_arg ][ 2 ] == '\0' && *argc == cur_arg + 1 )
			{
				fprintf( stderr, "fsc2 -B: No input files.\n" );
				usage( EXIT_FAILURE );
			}

			if ( argv[ cur_arg ][ 2 ] != '\0' )
			{
				*fname = argv[ cur_arg ] + 2;
				for ( i = cur_arg; i < *argc; i++ )
					argv[ i ] = argv[ i + 1 ];
				*argc -= 1;
			}
			else
			{
				*fname = argv[ cur_arg + 1 ];
				for ( i = cur_arg; i < *argc - 1; i++ )
					argv[ i ] = argv[ i + 2 ];
				*argc -= 2;
			}

			flags |= DO_LOAD | BATCH_MODE | DO_START;

			break;
		}

		if ( ! strncmp( argv[ cur_arg ], "-X", 2 ) )
		{
			if ( flags & DO_CHECK )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-X' and "
						"'-t' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & DO_START )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-X' and "
						"'-S' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & DO_TEST )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-X' and "
						"'-T' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( flags & BATCH_MODE )
			{
				fprintf( stderr, "fsc2: Can't have both flags '-X' and "
						"'-B' at once.\n" );
				usage( EXIT_FAILURE );
			}

			if ( argv[ cur_arg ][ 2 ] == '\0' )
				Internals.num_test_runs = 1;
			else
				for ( Internals.num_test_runs = 0, i = 2;
					  argv[ cur_arg ][ i ] != '\0'; i++ )
					if ( isdigit( argv[ cur_arg ][ i ] ) )
						Internals.num_test_runs = Internals.num_test_runs * 10 
							+ ( argv[ cur_arg ][ i ] - '0' );
					else
					{
						fprintf( stderr, "Flag '-X' needs a numerical "
								 "argument but got '%s'.\n",
								 argv[ cur_arg ] + 2);
						usage( EXIT_FAILURE );
					}

			if ( *argc == cur_arg + 1 )
			{
				fprintf( stderr, "fsc2 -X: No input file.\n" );
				usage( EXIT_FAILURE );
			}
			else
			{
				*fname = argv[ cur_arg + 1 ];
				for ( i = cur_arg; i < *argc - 1; i++ )
					argv[ i ] = argv[ i + 2 ];
				*argc -= 2;
			}

			if ( Internals.num_test_runs == 0 )
			{
				Internals.just_testing = SET;
				Internals.cmdline_flags |= NO_MAIL;

				seteuid( getuid( ) );
				setegid( getgid( ) );
				if ( ( in_file_fp = fopen( *fname, "r" ) ) == NULL )
					exit( EXIT_FAILURE );
				exit( scan_main( *fname, in_file_fp ) ?
					  EXIT_SUCCESS : EXIT_FAILURE );
			}

			flags |= NO_MAIL | DO_LOAD | DO_CHECK;
		}

		cur_arg++;
	}

	return flags;
}


/*----------------------------------------------------------*/
/* This function is called after either exit() is called or */
/* it returns from main(). Here some cleanup is done that   */
/* is necessary even if the program crashed.                */
/*----------------------------------------------------------*/

static void final_exit_handler( void )
{
	/* Stop the process that is waiting for external connections as well
	   as the child process and the HTTP server */

	if ( Internals.conn_pid > 0 )
		kill( Internals.conn_pid, SIGTERM );

	if ( Internals.child_pid > 0 )
		kill( Internals.child_pid, SIGTERM );

	if ( Internals.http_pid > 0 )
		kill( Internals.http_pid, SIGTERM );

	/* Do everything necessary to end the program */

	if ( in_file_fp != NULL )
		fclose( in_file_fp );

	if ( delete_old_file && EDL.in_file != NULL )
		unlink( EDL.in_file );
	unlink( FSC2_SOCKET );

	T_free( EDL.in_file );

	T_free( G.color_hash );

	/* Delete all shared memory and also semaphore (if it still exists) */

	delete_all_shm( );
	if ( Comm.data_semaphore >= 0 )
		sema_destroy( Comm.data_semaphore );
	if ( Comm.request_semaphore >= 0 )
		sema_destroy( Comm.request_semaphore );

	/* If the program was killed by a signal indicating an unrecoverable
	   error print out a message and (if this feature isn't switched off) 
	   send an email */

	if ( fsc2_death != 0 && fsc2_death != SIGTERM )
	{
		if ( * ( ( int * ) xresources[ NOCRASHMAIL ].var ) == 0 &&
			  ! ( Internals.cmdline_flags & NO_MAIL ) )
		{
			death_mail( fsc2_death );
			fprintf( stderr, "A crash report has been sent to %s\n",
					 MAIL_ADDRESS );
		}

#if defined _GNU_SOURCE
		fprintf( stderr, "fsc2 (%d) killed by %s signal.\n", getpid( ),
				 strsignal( fsc2_death ) );
#else
		fprintf( stderr, "fsc2 (%d) killed by signal %d.\n", getpid( ),
				 fsc2_death);
#endif
	}
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
	static char *old_in_file;
	FILE *fp;
	struct stat file_stat;
	char tmp_file[ ] = P_tmpdir "/fsc2.stash.XXXXXX";
	int c;
	int tmp_fd;
	FILE *tmp_fp;


	UNUSED_ARGUMENT( a );

	notify_conn( BUSY_SIGNAL );
	old_in_file = NULL;

	/* If new file is to be loaded get its name and store it, otherwise use
	   the previous name */

	if ( ! reload )
	{
		if ( GUI.main_form->Load->u_ldata == 0 &&
			 GUI.main_form->Load->u_cdata == NULL )
		{
			fn = fl_show_fselector( "Select input file:", NULL, "*.edl",
									NULL );
			if ( fn == NULL || *fn == '\0' )
			{
				notify_conn( UNBUSY_SIGNAL );
				return;
			}

			old_in_file = EDL.in_file;

			TRY
			{
				EDL.in_file = T_strdup( fn );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				EDL.in_file = old_in_file;
				notify_conn( UNBUSY_SIGNAL );
				return;
			}
		}
		else
		{
			old_in_file = EDL.in_file;

			EDL.in_file = GUI.main_form->Load->u_cdata;
			GUI.main_form->Load->u_cdata = NULL;
		}
	}
	else                              /* call via reload button */
	{
		/* Quit if name of previous file is empty */

		if ( EDL.in_file == '\0' )
		{
			fl_show_alert( "Error", "Sorry, no file is loaded yet.", NULL, 1 );
			old_in_file = CHAR_P T_free( old_in_file );
			notify_conn( UNBUSY_SIGNAL );
			return;
		}

		if ( in_file_fp != NULL )
		{
			fclose( in_file_fp );
			in_file_fp = NULL;
		}
	}

	/* Test if the file is readable and can be opened */

	if ( access( EDL.in_file, R_OK ) == -1 )
	{
		if ( Internals.cmdline_flags & DO_CHECK )
			exit( EXIT_FAILURE );

		if ( Internals.cmdline_flags & BATCH_MODE )
		{
			fprintf( stderr, "Input file not found: '%s'.\n", EDL.in_file );
			is_loaded = UNSET;
			return;
		}

		if ( errno == ENOENT )
			fl_show_alert( "Error", "Sorry, file not found:", EDL.in_file, 1 );
		else
			fl_show_alert( "Error", "Sorry, no permission to read file:",
						   EDL.in_file, 1 );
		old_in_file = CHAR_P T_free( old_in_file );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	/* Now we create a copy of the file, so that we can still operate on the
	   file in the currently loaded form at later times even when the user
	   changes the file in between. In all the following we work on the
	   file pointer 'fp' of this temporary file even though the file name
	   'EDL.in_file' shown to the user still refers to the original file... */

	if ( ( tmp_fd = mkstemp( tmp_file ) ) < 0 )
	{
		if ( Internals.cmdline_flags & DO_CHECK )
			exit( EXIT_FAILURE );

		if ( Internals.cmdline_flags & BATCH_MODE )
		{
			fprintf( stderr, "Can't open a temporary file, skipping input "
					 "file '%s'.\n", EDL.in_file );
			is_loaded = UNSET;
			return;
		}

		fl_show_alert( "Error", "Sorry, can't open a temporary file.",
					   NULL, 1 );
		old_in_file = CHAR_P T_free( old_in_file );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	unlink( tmp_file );

	if ( ( tmp_fp = fdopen( tmp_fd, "w+" ) ) == 0 )
	{
		close( tmp_fd );
		if ( Internals.cmdline_flags & DO_CHECK )
			exit( EXIT_FAILURE );

		if ( Internals.cmdline_flags & BATCH_MODE )
		{
			fprintf( stderr, "Can't open a temporary file, skipping input "
					 "file '%s'.\n", EDL.in_file );
			is_loaded = UNSET;
			return;
		}

		fl_show_alert( "Error", "Sorry, can't open a temporary file.",
					   NULL, 1 );
		old_in_file = CHAR_P T_free( old_in_file );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	if ( ( fp = fopen( EDL.in_file, "r" ) ) == NULL )
	{
		fclose( tmp_fp );
		if ( Internals.cmdline_flags & DO_CHECK )
			exit( EXIT_FAILURE );

		if ( Internals.cmdline_flags & BATCH_MODE )
		{
			fprintf( stderr, "Can't open input file: '%s'.\n", EDL.in_file );
			is_loaded = UNSET;
			return;
		}

		fl_show_alert( "Error", "Sorry, can't open file:", EDL.in_file, 1 );
		old_in_file = CHAR_P T_free( old_in_file );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	/* Get modification time of file */

	stat( EDL.in_file, &file_stat );
	in_file_mod = file_stat.st_mtime;

	/* Copy the contents of the EDL file into our temporary file */

	while ( ( c = fgetc( fp ) ) != EOF )
		fputc( c, tmp_fp );

	fclose( fp );
	fp = tmp_fp;

	rewind( fp );
	lseek( tmp_fd, 0, SEEK_SET );           /* paranoia... */

	/* Now that we're sure that we can read the new file we can delete the
	   old file (but only if the new and the old file are different !) */

	if ( old_in_file != NULL )
	{
		if ( strcmp( old_in_file, EDL.in_file ) )
		{
			if ( delete_old_file )
				unlink( old_in_file );
			delete_old_file = delete_file;
		}
		else      /* don't delete reloaded files - they may have been edited */
			delete_old_file = UNSET;

		if ( in_file_fp != NULL )
			fclose( in_file_fp );
	}

	in_file_fp = fp;

	delete_file = UNSET;
	old_in_file = CHAR_P T_free( old_in_file );

	/* Set a new window title */

	T_free( title );
	title = get_string( "fsc2: %s", EDL.in_file );
	fl_set_form_title( GUI.main_form->fsc2, title );

	/* Read in and display the new file */

	is_loaded = display_file( EDL.in_file, in_file_fp );
	state = FAIL;
	is_tested = UNSET;

	fl_activate_object( GUI.main_form->reload );
	fl_set_object_lcol( GUI.main_form->reload, FL_BLACK );
	fl_activate_object( GUI.main_form->Edit );
	fl_set_object_lcol( GUI.main_form->Edit, FL_BLACK );
	fl_activate_object( GUI.main_form->test_file );
	fl_set_object_lcol( GUI.main_form->test_file, FL_BLACK );
	fl_activate_object( GUI.main_form->run );
	fl_set_object_lcol( GUI.main_form->run, FL_BLACK );

	if ( GUI.main_form->Load->u_ldata != 0 )
	{
		if ( GUI.main_form->Load->u_ldata == ( long ) 'S' )
			fl_trigger_object( GUI.main_form->run );
		if ( GUI.main_form->Load->u_ldata == ( long ) 'T' )
			fl_trigger_object( GUI.main_form->test_file );
		GUI.main_form->Load->u_ldata = 0;
	}

	fl_activate_object( GUI.main_form->test_file );
	fl_set_object_lcol( GUI.main_form->test_file, FL_BLACK );

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


	UNUSED_ARGUMENT( a );
	UNUSED_ARGUMENT( b );

	/* Fork and execute editor in child process */

	if ( ( res = fork( ) ) == 0 )
		start_editor( );

	if ( res == -1 )                                /* fork failed ? */
		fl_show_alert( "Error", "Sorry, unable to start the editor.",
					   NULL, 1 );
}


/*---------------------------------------------------------------------*/
/* This function is called to start the editor on the currently loaded */
/* file. It is called after a fork(), so it may not return. Which      */
/* editor is used depends on the environment variable EDITOR. If this  */
/* variable isn't set vi is used.                                      */
/*---------------------------------------------------------------------*/

static void start_editor( void )
{
	char *ed, *ep;
	char **argv, **final_argv;
	int argc = 1;
	int	i;


	/* Try to find content of environment variable "EDITOR" - if it doesn't
	   exist use vi as the default editor */

	ed = getenv( "EDITOR" );

	if ( ed == NULL || *ed == '\0' )
	{
		argv = CHAR_PP T_malloc( 5 * sizeof *argv );

		argv[ 0 ] = ( char * ) "xterm";
		argv[ 1 ] = ( char * ) "-e";
		argv[ 2 ] = ( char * ) "vi";
		argv[ 3 ] = EDL.in_file;
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
			argv = CHAR_PP T_malloc( 5 * sizeof *argv );

			argv[ 0 ] = ( char * ) "xterm";
			argv[ 1 ] = ( char * ) "-e";
			argv[ 2 ] = ed;
			argv[ 3 ] = EDL.in_file;
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

			argv = CHAR_PP T_malloc( ( argc + 5 ) * sizeof *argv );

			argv[ 0 ] = ( char * ) "xterm";
			argv[ 1 ] = ( char * ) "-e";
			for ( ep = ed, i = 2; i < argc + 2; ++i )
			{
				argv[ i ] = ep;
				while ( *ep++ )
					/* empty */ ;
			}

			argv[ i ]   = EDL.in_file;
			argv[ ++i ] = NULL;
		}
	}

	/* Special treatment for emacs and xemacs: if emacs is called without
	   the '-nw' option it will create its own window - so we don't embed
	   it into a xterm - the same holds for xemacs which, AFAIK, always
	   creates its own window. */

	final_argv = argv;

	if ( ! strcmp( strip_path( argv[ 2 ] ), "xemacs" ) )
		final_argv = argv + 2;

	if ( ! strcmp( strip_path( argv[ 2 ] ), "emacs" ) )
	{
		for ( i = 3; argv[ i ] && strcmp( argv[ i ], "-nw" ); ++i )
			/* empty */ ;
		if ( argv[ i ] == NULL )                    /* no '-nw' flag */
			final_argv = argv + 2;
	}

	execvp( final_argv[ 0 ], final_argv );
	T_free( argv );
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


	UNUSED_ARGUMENT( b );
	a->u_ldata = 0;

	/* While program is being tested the test can be aborted by pressing the
	   test button again - in this case we simply throw an exception */

	if ( running_test )
	{
		fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );
		user_break = SET;
		delete_devices( );                       /* run the exit hooks ! */
		eprint( FATAL, UNSET,
				"Test of program aborted, received user break.\n" );
		running_test = UNSET;
		THROW( EXCEPTION );
	}

	notify_conn( BUSY_SIGNAL );

	/* If fsc2 is too busy testing a program to react to clicks of the "Stop
	   Test" button and the user presses the button several times strange
	   things happen, especially the "Quit" button becomes unusable. The
	   following helps avoiding to execute the handler again while it's
	   already running... */

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
	   between and the user wants the new version - quit if file can't be read
	   again. */

	stat( EDL.in_file, &file_stat );
	if ( in_file_mod != file_stat.st_mtime &&
		 2 == fl_show_choice( "EDL file on disk is newer than the loaded ",
							  "file. Reload file from disk?",
							  "", 2, "No", "Yes", "", 1 ) )
	{
		load_file( GUI.main_form->browser, 1 );
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

	fl_clear_browser( GUI.main_form->error_browser );

	fl_deactivate_object( GUI.main_form->Load );
	fl_set_object_lcol( GUI.main_form->Load, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->reload );
	fl_set_object_lcol( GUI.main_form->reload, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->Edit );
	fl_set_object_lcol( GUI.main_form->Edit, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->run );
	fl_set_object_lcol( GUI.main_form->run, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->quit );
	fl_set_object_lcol( GUI.main_form->quit, FL_INACTIVE_COL );
	fl_set_object_label( GUI.main_form->test_file, "Stop Test" );
	fl_set_button_shortcut( GUI.main_form->test_file, "T", 1 );

	running_test = SET;
	fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_watch );
	user_break = UNSET;

	/* Parse the input file and, when we're done with it, close it, everything
	   relevant is now stored in memory (unless the parsing was interrupted
	   by the user) */

	state = scan_main( EDL.in_file, in_file_fp );
	if ( ! user_break )
	{
		fclose( in_file_fp );
		in_file_fp = NULL;
	}

	fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );
	running_test = UNSET;

	if ( ! user_break )
	{
		is_tested = SET;                  /* show that file has been tested */
		a->u_ldata = 0;
		fl_deactivate_object( GUI.main_form->test_file );
		fl_set_object_lcol( GUI.main_form->test_file, FL_INACTIVE );
	}
	else
	{
		a->u_ldata = 1;
		user_break = UNSET;
		fl_activate_object( GUI.main_form->test_file );
		fl_set_object_lcol( GUI.main_form->test_file, FL_BLACK );
	}

	fl_set_object_label( GUI.main_form->test_file, "Test" );
	fl_set_button_shortcut( GUI.main_form->test_file, "T", 1 );
	fl_activate_object( GUI.main_form->Load );
	fl_set_object_lcol( GUI.main_form->Load, FL_BLACK );
	fl_activate_object( GUI.main_form->reload );
	fl_set_object_lcol( GUI.main_form->reload, FL_BLACK );
	fl_activate_object( GUI.main_form->Edit );
	fl_set_object_lcol( GUI.main_form->Edit, FL_BLACK );
	fl_activate_object( GUI.main_form->run );
	fl_set_object_lcol( GUI.main_form->run, FL_BLACK );
	fl_activate_object( GUI.main_form->quit );
	fl_set_object_lcol( GUI.main_form->quit, FL_BLACK );

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


	UNUSED_ARGUMENT( a );
	UNUSED_ARGUMENT( b );

	notify_conn( BUSY_SIGNAL );

	if ( ! is_loaded )              /* check that there is a file loaded */
	{
		fl_show_alert( "Error", "Sorry, but no file is loaded.", NULL, 1 );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	if ( ! is_tested )              /* test file if not already done */
	{
		test_file( GUI.main_form->test_file, 1 );
		if ( GUI.main_form->test_file->u_ldata == 1 )  /* user break ? */
			return;
		notify_conn( BUSY_SIGNAL );
	}
	else
	{
		stat( EDL.in_file, &file_stat );
		if ( in_file_mod != file_stat.st_mtime &&
			 2 == fl_show_choice( "EDL file on disk is newer than loaded",
								  "file. Reload the file from disk?",
								  "", 2, "No", "Yes", "", 1 ) )
		{
			load_file( GUI.main_form->browser, 1 );
			if ( ! is_loaded )
				return;
			is_tested = UNSET;
			test_file( GUI.main_form->test_file, 1 );
			if ( GUI.main_form->test_file->u_ldata == 1 )/* user break ? */
				return;
			notify_conn( BUSY_SIGNAL );
		}
	}

	if ( ! state )               /* return if program failed the test */
	{
		/* In batch mode write out an error message and trigger the "Quit"
		   button to start dealing with the next EDL script. Otherwise
		   show an error message. */

		if ( Internals.cmdline_flags & BATCH_MODE )
		{
			fprintf( stderr, "Sorry, but test of file failed: '%s'.\n",
					 EDL.in_file );
			fl_trigger_object( GUI.main_form->quit );
		}
		else
			fl_show_alert( "Error", "Sorry, but test of file failed.",
						   NULL, 1 );
		notify_conn( UNBUSY_SIGNAL );
		return;
	}

	/* If there were non-fatal errors ask user if she wants to continue.
	   In batch mode run even if there were non-fatal errors. */

	if ( ! ( Internals.cmdline_flags & BATCH_MODE ) &&
		 ( EDL.compilation.error[ SEVERE ] != 0 ||
		   EDL.compilation.error[ WARN ] != 0 ) )
	{
		if ( EDL.compilation.error[ SEVERE ] != 0 )
		{
			if ( EDL.compilation.error[ WARN ] != 0 )
			{
				sprintf( str1, "There where %d severe warnings",
						 EDL.compilation.error[ SEVERE ] );
				sprintf( str2, "and %d warnings.",
						 EDL.compilation.error[ WARN ] );
			}
			else
			{
				sprintf( str1, "There where %d severe warnings.",
						 EDL.compilation.error[ SEVERE ] );
				str2[ 0 ] = '\0';
			}
		}
		else
		{
			sprintf( str1, "There where %d warnings.",
					 EDL.compilation.error[ WARN ] );
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
		fl_show_alert( "Error", "Sorry, can't run the experiment.",
					   "See browser for more information.", 1 );
}


/*--------------------------------------------------------------------*/
/* display_file() is used to put the contents of a file into the main */
/* browser, numbering the lines and expanding tab chars. Arguments    */
/* are the name of the file to be displayed and the FILE pointer of   */
/* the file.                                                          */
/*--------------------------------------------------------------------*/

static bool display_file( char *name, FILE *fp )
{
	int len, key;
	long lc, cc, i;                         /* line and char counter */
	char line[ FL_BROWSER_LINELENGTH ];
	char *lp;


	/* Determine number of lines (and maximum number of digits) in order to
	   find out about proper formating of line numbers */

	if ( ( lc = get_file_length( fp, &len ) ) <= 0 )  /* error ? */
	{
		switch ( ( int ) lc )
		{
			case 0 :                  /* file length is zero */
				fl_show_alert( "Error", "File is empty.", NULL, 1 );
				return FAIL;

			case -1 :                 /* not enough memory left */
				fl_show_alert( "Error", "Cannot read file:", name, 1 );
				return FAIL;

			default:
				fl_show_alert( "Error",
							   "Unexpected error while counting lines.",
							   "Please send a bug report.", 1 );
				return FAIL;
		}
	}

	/* Freeze browser, read consecutive lines, prepend line numbers (after
	   expanding tab chars) and send lines to browser */

	fl_clear_browser( GUI.main_form->browser );
	fl_clear_browser( GUI.main_form->error_browser );
	fl_freeze_form( GUI.main_form->browser->form );

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
					/* empty */ ;
				cc--;
			}
		}
		*lp = '\0';

		/* Color section headings */

		lp = line + len + 2;
		if ( ! strncmp( lp, "DEVICES:", 8 ) ||
			 ! strncmp( lp, "DEVICE:", 7 ) ||
			 ! strncmp( lp, "DEVS:\n", 7 ) ||
			 ! strncmp( lp, "DEV:", 4 ) ||
			 ! strncmp( lp, "VARIABLES:", 10 ) ||
			 ! strncmp( lp, "VARIABLE:", 9 ) ||
			 ! strncmp( lp, "VARS:", 5 ) ||
			 ! strncmp( lp, "VAR:", 4 ) ||
			 ! strncmp( lp, "ASSIGNMENTS:", 12 ) ||
			 ! strncmp( lp, "ASSIGNMENT:", 11 ) ||
			 ! strncmp( lp, "ASS:", 4 ) ||
			 ! strncmp( lp, "PHA:", 4 ) ||
			 ! strncmp( lp, "PHAS:", 5 ) ||
			 ! strncmp( lp, "PHASE:", 6 ) ||
			 ! strncmp( lp, "PHASES:", 7 ) ||
			 ! strncmp( lp, "PREPARATIONS:", 13 ) ||
			 ! strncmp( lp, "PREPARATION:", 12 ) ||
			 ! strncmp( lp, "PREPS:", 6 ) ||
			 ! strncmp( lp, "PREP:", 5 ) ||
			 ! strncmp( lp, "EXPERIMENT:", 11 ) ||
			 ! strncmp( lp, "EXP:", 4 ) ||
			 ! strncmp( lp, "ON_STOP:", 8 ) )
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

		fl_add_browser_line( GUI.main_form->browser, line );
	}

	rewind( fp );

	/* Unfreeze and thus redisplay the browser */

	fl_unfreeze_form( GUI.main_form->browser->form );
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


	/* Get rid of the last remains of graphics */

	for ( i = X; i <= Y; i++ )
		G1.label_orig[ i ] = CHAR_P T_free( G1.label_orig[ i ] );

	for ( i = X; i <= Z; i++ )
		G2.label_orig[ i ] = CHAR_P T_free( G2.label_orig[ i ] );

	G.is_init = UNSET;
	G.dim = 0;

	/* Clear up the compilation structure */

	for ( i = 0; i < 3; ++i )
		EDL.compilation.error[ i ] = 0;
	for ( i = DEVICES_SECTION; i <= EXPERIMENT_SECTION; ++i )
		EDL.compilation.sections[ i ] = UNSET;

	/* Deallocate memory used for file names */

	EDL.Fname = CHAR_P T_free( EDL.Fname );

	/* Run exit hook functions and unlink modules */

	delete_devices( );
	need_GPIB = UNSET;
	fsc2_final_serial_cleanup( );

	/* Delete function list */

	functions_exit( );

	/* Delete device list */

	delete_device_name_list( );

	/* Remove stuff needed for pulsers */

	pulser_cleanup( );

	/* Clear up structures for phases */

	phases_clear( );

	/* Delete everything connected with user buttons */

	tools_clear( );

	/* Delete variables and get rid of variable stack */

	vars_clean_up( );

	/* delete stored program */

	forget_prg( );
}


/*-------------------------------------------------------------*/
/* This function is the callback function for the help button. */
/* It forks a process that execs a browser displaying the HTML */
/* documentation.                                              */
/*-------------------------------------------------------------*/

void run_help( FL_OBJECT *a, long b )
{
	int res;
	int bn;


	UNUSED_ARGUMENT( b );

	bn = fl_get_button_numb( a );
	if ( bn != FL_SHORTCUT + 'S' && bn == FL_RIGHT_MOUSE )
	{
		eprint( NO_ERROR, UNSET,
				( GUI.G_Funcs.size == LOW ) ?
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
				( GUI.G_Funcs.size == LOW ) ?
				"@n-------------------------------------------\n" :
				"@n-----------------------------------------------\n" );
		return;
	}

	notify_conn( BUSY_SIGNAL );

	/* Fork and run help browser in child process */

	if ( ( res = fork( ) ) == 0 )
		start_help_browser( );

	if ( res == -1 )                                /* fork failed ? */
		fl_show_alert( "Error", "Sorry, unable to start the help browser.",
					   NULL, 1 );

	notify_conn( UNBUSY_SIGNAL );
}


/*--------------------------------------------------------------*/
/* This function starts a browser with the HTML documantation.  */
/* It is called after a fork(), so it may not return. Which     */
/* browser is used depends on the environment variable BROWSER, */
/* currently the program only knows how to deal with Netscape,  */
/* mozilla, opera, konqueror, galeon, lnyx or w3m. If BROWSER   */
/* isn't set Netscape is used as the default.                   */
/*--------------------------------------------------------------*/

static void start_help_browser( void )
{
	char *browser;
	char *av[ 5 ] = { NULL, NULL, NULL, NULL, NULL };


	/* Try to figure out which browser to use... */

	browser = getenv( "BROWSER" );

	if ( browser && ! strcasecmp( browser, "opera" ) )
	{
		av[ 0 ] = T_strdup( "opera" );
		av[ 1 ] = T_strdup( "-newbrowser" );
		av[ 2 ] = get_string( "file:%s%s%shtml/fsc2.html",
							  docdir[ 0 ] != '/' ? "/" : "", docdir,
							  slash( docdir ) );
	}
	else if ( browser && ! strcasecmp( browser, "konqueror" ) )
	{
		av[ 0 ] = T_strdup( "konqueror" );
		av[ 1 ] = get_string( "file:%s%s%shtml/fsc2.html",
							  docdir[ 0 ] != '/' ? "/" : "", docdir,
							  slash( docdir ) );
	}
	else if ( browser && ! strcasecmp( browser, "galeon" ) )
	{
		av[ 0 ] = T_strdup( "galeon" );
		av[ 1 ] = T_strdup( "--new-window" );
		av[ 2 ] = get_string( "file://%s%s%shtml/fsc2.html",
							  docdir[ 0 ] != '/' ? "/" : "", docdir,
							  slash( docdir ) );
	}
	else if ( browser && ( ! strcasecmp( browser, "lynx" ) ||
						   ! strcasecmp( browser, "w3m" ) ) )
	{
		av[ 0 ] = T_strdup( "xterm" );
		av[ 1 ] = T_strdup( "-e" );
		av[ 2 ] = T_strdup( browser );
		av[ 3 ] = get_string( "%s%shtml/fsc2.html", docdir, slash( docdir ) );
	}
	else if ( browser && ( strncasecmp( browser, "netscape", 8 ) ||
						   strcasecmp( browser, "mozilla" ) ) )
	{
		av[ 0 ] = T_strdup( browser );
		av[ 1 ] = get_string( "file:%s%s%shtml/fsc2.html",
							  docdir[ 0 ] != '/' ? "/" : "", docdir,
							  slash( docdir ) );
	}
	else
	{
		/* If netscape isn't running start it, otherwise ask it to just open
		   a new window */

		av[ 0 ] = T_strdup( "netscape" );

		if ( system( "xwininfo -name Netscape >/dev/null 2>&1" ) )
			av[ 1 ] = get_string( "file:%s%s%shtml/fsc2.html",
								  docdir[ 0 ] != '/' ? "/" : "", docdir,
								  slash( docdir ) );
		else
		{
			av[ 1 ] = T_strdup( "-remote" );
			av[ 2 ] =
				   get_string( "openURL(file:%s%s%shtml/fsc2.html,new-window)",
							   docdir[ 0 ] != '/' ? "/" : "", docdir,
							   slash( docdir ) );
		}
	}

	execvp( av[ 0 ], av );
	_exit( EXIT_FAILURE );
}


/*----------------------------------------------------------------*/
/* Function that deals with requests by the child process that is */
/* listening for external connections (to send a new EDL program) */
/*----------------------------------------------------------------*/

static void conn_request_handler( void )
{
	char line[ MAXLINE ];
	int count;


	while ( ( count = read( Comm.conn_pd[ READ ], line, MAXLINE ) ) == -1 &&
			errno == EINTR )
		/* empty */ ;
	line[ count - 1 ] = '\0';
	GUI.main_form->Load->u_ldata = ( long ) line[ 0 ];
	if ( line[ 1 ] == 'd' )
		delete_file = SET;
	GUI.main_form->Load->u_cdata = T_strdup( line + 2 );
	fl_trigger_object( GUI.main_form->Load );
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
						SIGALRM, SIGSEGV, SIGPIPE, SIGTERM, SIGUSR1, SIGUSR2,
						SIGCHLD, SIGCONT, SIGTTIN, SIGTTOU, SIGBUS, SIGVTALRM,
						0 };
	int i;


	for ( i = 0; sig_list[ i ] != 0; i++ )
	{
		sact.sa_handler = main_sig_handler;
		sigemptyset( &sact.sa_mask );
		sact.sa_flags = 0;
		if ( sigaction( sig_list[ i ], &sact, NULL ) < 0 )
		{
			fprintf( stderr, "Failed to initialize fsc2.\n" );
			exit( EXIT_FAILURE );
		}
	}
}


/*-------------------------------------*/
/* Signal handler for the main program */
/*-------------------------------------*/

void main_sig_handler( int signo )
{
	int errno_saved;
	pid_t pid;
#if ! defined( NDEBUG ) && defined( ADDR2LINE ) && ! defined __STRICT_ANSI__
	int *EBP;           /* assumes sizeof( int ) equals size of pointers */
#endif


	switch ( signo )
	{
		case SIGCHLD :
			errno_saved = errno;
			while ( ( pid = waitpid( -1, NULL, WNOHANG ) ) > 0 )
				if ( pid == Internals.http_pid )
				{
					Internals.http_pid = -1;
					Internals.http_server_died = SET;
				}
			errno = errno_saved;
			return;

		case SIGUSR1 :
			Internals.conn_request = SET;
			return;

		case SIGUSR2 :
			conn_child_replied = SET;
			return;

		case SIGALRM :
			if ( Internals.tb_wait == TB_WAIT_TIMER_RUNNING )
				Internals.tb_wait = TB_WAIT_TIMER_EXPIRED;
			return;

		/* Ignored signals : */

		case SIGHUP :
		case SIGINT :
		case SIGCONT :
		case SIGTTIN :
		case SIGTTOU :
		case SIGVTALRM :
		case SIGPIPE :
			return;

		/* All the remaining signals are deadly - we set 'fsc2_death' to the
		   signal number so final_exit_handler() can do the appropriate
		   things. */

		default :
			fsc2_death = signo;

			if ( ! ( Internals.cmdline_flags & NO_MAIL ) )
			{

#if ! defined( NDEBUG ) && defined( ADDR2LINE ) && ! defined __STRICT_ANSI__

				/* Of course, we're now deep in UB-land, so we can only hope
				   that it won't make nasal daemons come out of our nose... */

				if ( Internals.is_linux_i386 )
				{
					asm( "mov %%ebp, %0" : "=g" ( EBP ) );
					DumpStack( ( void * ) * ( EBP + CRASH_ADDRESS_OFFSET ) );
				}
#endif
			}

			exit( EXIT_FAILURE );
	}
}


/*-------------------------------------------------------------------*/
/* Frunction for sending signals to the child process that waits for */
/* external connections. It sends either BUSY_SIGNAL (aka SIGUSR1)   */
/* or UNBUSY_SIGNAL (aka SIGUSR2) and then waits for the child       */
/* to reply with its own signal.                                     */
/*-------------------------------------------------------------------*/

void notify_conn( int signo )
{
	/* Don't send signal to the process responsible for connections if it
	   doesn't exist (in load_file() at the very start) or when the
       experiment is running - in this case fsc2 is busy anyway and the
       connection process has already been informed about this. */

	if ( Internals.conn_pid <= 0 || Internals.child_pid > 0 ||
		 Internals.cmdline_flags & DO_CHECK )
		return;

	kill( Internals.conn_pid, signo );

	/* Wait for reply from child but avoid waiting when it in fact already
	   did reply (as indicated by the variable). */

	while ( ! conn_child_replied )
		fsc2_usleep( 50000, SET );
	conn_child_replied = UNSET;
}


/*----------------------------------------*/
/* Function prints help message and exits */
/*----------------------------------------*/

void usage( int return_status )
{
	fprintf( stderr, "Usage: fsc2 [OPTIONS]... [FILE]\n"
			 "A program for remote control of spectrometers\n"
			 "OPTIONS:\n"
			 "  -t FILE    run syntax check on FILE and exit (no graphics)\n"
			 "  -T FILE    run syntax check on FILE\n"
			 "  -S FILE    start experiment interpreting FILE\n"
			 "  -B FILE... run in batch mode\n"
			 "  --delete   delete input file when fsc2 is done with it\n"
			 "  -non-exclusive\n"
			 "             run in non-exclusive mode, allowing more than\n"
			 "             one instance of fsc2 to be run at a time\n"
             "  -stopMouseButton Number/Word\n"
			 "             mouse button to be used to stop an experiment\n"
			 "             1 = \"left\", 2 = \"middle\", 3 = \"right\" "
			 "button\n"
			 "  -size size\n"
			 "             specify the size to use, either L[ow] or H[igh]\n"
			 "  -geometry geometry\n"
			 "             specify preferred size and position of main "
			 "window\n"
			 "  -displayGeometry geometry\n"
			 "             specify preferred size and position of the "
			 "display windows\n"
			 "  -display1dGeometry geometry\n"
			 "             specify preferred size and position of the "
			 "1D data display window\n"
			 "  -display2dGeometry geometry\n"
			 "             specify preferred size and position of the "
			 "2D data display window\n"
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
			 "display windows\n"
			 "  -noCrashMail\n"
			 "             don't send email when fsc2 crashes\n"
#if defined WITH_HTTP_SERVER
			 "  -httpPort\n"
			 "             selects HTTP port to be used by web server\n"
#endif
			 "  -h, --help\n"
			 "             display this help text and exit\n\n"
			 "For a complete documentation see either %s%sfsc2.ps,\n"
			 "%s%sfsc2.pdf or %s%sfsc2.html\n"
             "or type \"info fsc2\".\n", docdir, slash( docdir ),
			 docdir, slash( docdir ), docdir, slash( docdir ) );
	exit( return_status );
}


/*---------------------------------------------------------*/
/* Idle handler that's running while no experiment is done */
/* and which deals with periodic tasks.                    */
/*---------------------------------------------------------*/

int idle_handler( XEvent *a, void *b )
{
	UNUSED_ARGUMENT( a );
	UNUSED_ARGUMENT( b );

	/* Check if a request from the child for external conections has
	   come in */

	if ( Internals.conn_request )
	{
		Internals.conn_request = UNSET;
		conn_request_handler( );
	}

	/* Check for request by the HTTP server and its death */

#if defined WITH_HTTP_SERVER
	if ( Internals.http_pid > 0 )
		http_check( );
	else if ( Internals.http_server_died )
	{
		Internals.http_server_died = UNSET;
		fl_call_object_callback( GUI.main_form->server );
	}
#endif

	return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
