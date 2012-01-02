/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
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


#if defined( FSC2_MDEBUG ) || defined( LIBC_MDEBUG )
#include <mcheck.h>
#endif

#include <execinfo.h>
#include "serial.h"
#include <sys/un.h>


/* Locally used global variables */

static bool Is_loaded       = UNSET;    /* set when EDL file is loaded */
static bool Is_tested       = UNSET;    /* set when EDL file has been tested */
static bool Parse_result    = UNSET;    /* set when EDL passed the tests */
static FILE *In_file_fp     = NULL;
static bool Delete_file     = UNSET;
static bool Delete_old_file = UNSET;


/* Imported global variables */

extern FL_resource Xresources[ ];         /* defined in xinit.c */
extern const char *Prog_Name;             /* defined in global.c */
extern FILE *fsc2_confin;                 /* defined in fsc2_conf_lexer.l */

extern int fsc2_confparse( void );        /* from fsc2_conf_parser.y */


/* Locally used functions */

static void globals_init( const char * pname );
static void fsc2_get_conf( void );
static void check_run( void );
static void no_gui_run( void );
static int scan_args( int   * argc,
                      char  * argv[ ],
                      char ** fname );
static void final_exit_handler( void );
static bool display_file( char * name,
                          FILE * fp );
static void set_main_signals( void );
static void main_sig_handler( int signo );


/**************************/
/*     Here we go...      */
/**************************/

int
main( int    argc,
      char * argv[ ] )
{
    char *fname = NULL;


#if defined LIBC_MDEBUG
    mcheck( NULL );
    mtrace( );
#endif

    /* Initialization of global variables */

    globals_init( *argv );

    /* Run a first test of the command line arguments */

    Fsc2_Internals.cmdline_flags = scan_args( &argc, argv, &fname );

    /* Check if other instances of fsc2 are already running. If there are
       and the current instance of fsc2 wasn't started with the non-exclusive
       option we've got to quit. If there's no other instance we've got to
       spawn a process that deals with further instances of fsc2 and takes
       care of shared memory segments. */

    if ( check_spawn_fsc2d( In_file_fp ) == -1 )
    {
        fprintf( stderr, "Failed to start or connect to daemon process for "
                 "fsc2.\n" );
        return EXIT_FAILURE;
    }

    /* Initialize xforms stuff, quit on error */

    else if (    ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
              && ! xforms_init( &argc, argv ) )
    {
        fprintf( stderr, "Graphic setup failed.\n" );
        return EXIT_FAILURE;
    }

    /* If we don't know yet about an input file and there are command
       line arguments left take it to be the input file */

    if ( argc > 1 && ! fname )
    {
        Fsc2_Internals.cmdline_flags |= DO_LOAD;
        fname = argv[ 1 ];
    }

    /* If there is a file argument try to load it */

    if ( Fsc2_Internals.cmdline_flags & DO_LOAD )
    {
        if ( ! get_edl_file( fname ) )
        {
            if ( access( fname, R_OK ) == -1 )
                fl_show_alert( "Error", "Can't access file", fname, 1 );
            else
            {
                fprintf( stderr, "Failed to set up full name of EDL file.\n" );
                return EXIT_FAILURE;
            }
        }
        else
            load_file( GUI.main_form->browser, 1 );
    }

    /* If '--delete' was given on the command line set flags that says that
       the input files needs to be deleted */

    if ( fname && Fsc2_Internals.cmdline_flags & DO_DELETE )
        Delete_file = Delete_old_file = SET;

    /* In batch mode try to get the next file if the first input file
       could not be accessed until we have one. */

    while ( Fsc2_Internals.cmdline_flags & BATCH_MODE && ! Is_loaded )
    {
        int i;

        if ( argc == 1 )
        {
            fprintf( stderr, "No input files found that could be read in.\n" );
            return EXIT_FAILURE;
        }

        if ( EDL.files )
        {
            while ( EDL.file_count > 0 )
                T_free( EDL.files[ --EDL.file_count ].name );
            EDL.files = T_free( EDL.files );
        }

        fname = argv[ 1 ];

        if ( ! get_edl_file( fname ) )
        {
            if ( access( fname, R_OK ) == -1 )
                fl_show_alert( "Error", "Can't access file", fname, 1 );
            else
            {
                fprintf( stderr, "Failed to set up full name of EDL file.\n" );
                return EXIT_FAILURE;
            }
        }
        else
            load_file( GUI.main_form->browser, 1 );

        for ( i = 1; i < argc; i++ )
            argv[ i ] = argv[ i + 1 ];
        argc -= 1;
    }

    /* Set up handler for signals and a handler function that gets called when
       the program exits */

    set_main_signals( );
    atexit( final_exit_handler );

    /* If the program was started in test mode call the appropriate function
       (which never returns) */

    if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
        check_run( );

    if ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
        no_gui_run( );

    /* Trigger test or start of current EDL program if the appropriate
       flags were passed to the program on the command line */

    if ( Fsc2_Internals.cmdline_flags & DO_TEST && Is_loaded )
        fl_trigger_object( GUI.main_form->test_file );
    if ( Fsc2_Internals.cmdline_flags & DO_START && Is_loaded )
        fl_trigger_object( GUI.main_form->run );

    /* If required send signal to the invoking process */

    if ( Fsc2_Internals.cmdline_flags & DO_SIGNAL )
        kill( getppid( ), SIGUSR1 );

    /* And, finally, here's the main loop of the program - it first checks
       the objects of the GUI, if necessary executing the associated
       callbacks, then calls a function that does everything else required
       (while no experiment is run just dealing with requests from the
       child process handling external connections and from the HTTP
       server, during an experiment also accepting new data and requests
       from the child process running the experiment). In batch mode we
       then try to get the next file from the command line and start it. */

 run_next:

    while ( fl_check_forms( ) != GUI.main_form->quit )
        if ( Fsc2_Internals.child_pid == 0 )
            idle_handler( );
        else
            new_data_handler( );

    if ( Fsc2_Internals.cmdline_flags & BATCH_MODE && argc > 1 )
    {
        Is_loaded = UNSET;

        while ( argc > 1 && ! Is_loaded )
        {
            int i;

            if ( EDL.files )
            {
                while ( EDL.file_count > 0 )
                    T_free( EDL.files[ --EDL.file_count ].name );
                EDL.files = T_free( EDL.files );
            }

            fname = argv[ 1 ];

            if ( ! get_edl_file( fname ) )
                break;

            load_file( GUI.main_form->browser, 1 );

            for ( i = 1; i < argc; i++ )
                argv[ i ] = argv[ i + 1 ];
            argc -= 1;
        }

        if ( Is_loaded )
        {
            fl_trigger_object( GUI.main_form->run );
            goto run_next;
        }
    }

    clean_up( );
    xforms_close( );
    T_free( Fsc2_Internals.title );

    return EXIT_SUCCESS;
}


/*-------------------------------------*
 * Initialization of global variables
 *-------------------------------------*/

static void
globals_init( const char * pname )
{
    /* As the very first action the effective UID and GID gets stored and
       then the program lowers the permissions to the ones of the real UID
       and GID, only switching back when creating or attaching to shared
       memory segments or accessing device or log files. */

    Fsc2_Internals.EUID = geteuid( );
    Fsc2_Internals.EGID = getegid( );

    lower_permissions( );

    Crash.already_crashed = UNSET;
    Crash.signo = 0;
    Crash.trace_length = 0;

    Fsc2_Internals.child_pid = 0;
    Fsc2_Internals.fsc2_clean_pid = 0;
    Fsc2_Internals.fsc2_clean_died = SET;
    Fsc2_Internals.http_pid = 0;
    Fsc2_Internals.state = STATE_IDLE;
    Fsc2_Internals.mode = PREPARATION;
    Fsc2_Internals.cmdline_flags = 0;
    Fsc2_Internals.in_hook = UNSET;
    Fsc2_Internals.I_am = PARENT;
    Fsc2_Internals.exit_hooks_are_run = UNSET;
    Fsc2_Internals.tb_wait = TB_WAIT_NOT_RUNNING;
    Fsc2_Internals.rsc_handle = NULL;
    Fsc2_Internals.http_server_died = UNSET;
    Fsc2_Internals.title = NULL;
    Fsc2_Internals.child_is_quitting = QUITTING_UNSET;

    GUI.win_has_pos = UNSET;
    GUI.win_has_size = UNSET;
    GUI.display_1d_has_pos = UNSET;
    GUI.display_1d_has_size = UNSET;
    GUI.display_2d_has_pos = UNSET;
    GUI.display_2d_has_size = UNSET;
    GUI.cut_win_has_pos = UNSET;
    GUI.cut_win_has_size = UNSET;
    GUI.toolbox_has_pos = UNSET;
    GUI.main_form     = NULL;
    GUI.run_form_1d   = NULL;
    GUI.run_form_2d   = NULL;
    GUI.input_form    = NULL;
    GUI.cut_form      = NULL;
    GUI.print_comment = NULL;

    fsc2_get_conf( );

    if ( pname )
        Prog_Name = pname;
    else
        Prog_Name = "fsc2";

    EDL.Lc = 0;
    EDL.files = NULL;
    EDL.file_count = 0;
    EDL.Fname = NULL;
    EDL.Call_Stack = NULL;
    EDL.prg_token = NULL;
    EDL.On_Stop_Pos = -1;
    EDL.Var_List = NULL;
    EDL.Var_Stack = NULL;
    EDL.do_quit = UNSET;
    EDL.react_to_do_quit = SET;

    /* Initialize list of open files EDL scripts can write to. Per default
       two are open, one is stdout and the other stderr. */

    EDL.File_List = T_malloc( 2 * sizeof *EDL.File_List );
    EDL.File_List_Len = 2;

    EDL.File_List[ 0 ].fp = stdout;
    setbuf( stdout, NULL );
    EDL.File_List[ 0 ].name = ( char * ) "stdout";

    EDL.File_List[ 1 ].fp = stderr;
    setbuf( stderr, NULL );
    EDL.File_List[ 1 ].name = ( char * ) "stderr";

    /* The list of used devices is still empty */

    EDL.Device_List = NULL;
    EDL.Device_Name_List = NULL;
    EDL.Num_Pulsers = 0;

    /* Channels for communication with process spawned to do an experiment
       aren't open yet. */

    Comm.mq_semaphore = -1;
    Comm.MQ = NULL;
    Comm.MQ_ID = -1;

    GUI.is_init = UNSET;

    G.color_hash = NULL;
}


/*----------------------------------------*
 * Read in some configuration information
 *----------------------------------------*/

static void
fsc2_get_conf( void )
{
    char *fname;
    struct passwd *ue;


    Fsc2_Internals.def_directory = NULL;

    if (    ! ( ue = getpwuid( getuid( ) ) )
         || ! ue->pw_dir
         || ! *ue->pw_dir  )
         return;

    TRY
    {
        fname = get_string( "%s/.fsc2/fsc2_config", ue->pw_dir );
        TRY_SUCCESS;
    }
    OTHERWISE
        return;

    fsc2_confin = NULL;
    if (    ! ( fsc2_confin = fopen( fname, "r" ) ) 
         || ! fsc2_obtain_fcntl_lock( fsc2_confin, F_RDLCK, SET ) )
    {
        if ( fsc2_confin )
            fclose( fsc2_confin );
        T_free( fname );
        return;
    }

    T_free( fname );

    TRY
    {
        fsc2_confparse( );
        fsc2_release_fcntl_lock( fsc2_confin );
        fclose( fsc2_confin );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fsc2_release_fcntl_lock( fsc2_confin );
        fclose( fsc2_confin );
        if ( Fsc2_Internals.def_directory )
            Fsc2_Internals.def_directory =
                                        T_free( Fsc2_Internals.def_directory );
    }
}


/*----------------------------------------------------------------------*
 * Function sets 'EDL.infile' to the complete name of an EDL input file
 *----------------------------------------------------------------------*/

bool
get_edl_file( const char * fname )
{
    struct stat file_stat;


    TRY
    {
        EDL.files = T_realloc( EDL.files,
                               ( EDL.file_count + 1 ) * sizeof *EDL.files );
        TRY_SUCCESS;
    }
    OTHERWISE
        return FAIL;
    
    EDL.files[ EDL.file_count ].name = NULL;
    EDL.files[ EDL.file_count++ ].mod_date = 0;

    TRY
    {
        /* We need the name of the file with the full path name (which
           needs to be passed on to fsc2_clean to allow include files
           in scripts to be specified relative to the path of the script
           instead of relative to the directory fsc2 was started in. */

        if ( *fname == '/' )
            EDL.files[ EDL.file_count - 1 ].name = T_strdup( fname );
        else
        {
            size_t size;
            char *buf;

            size = ( size_t ) pathconf( ".", _PC_PATH_MAX );
            buf = T_malloc( size );
            EDL.files[ EDL.file_count - 1 ].name =
                            get_string( "%s/%s", getcwd( buf, size ), fname );
            T_free( buf );
        }

        if ( stat( EDL.files[ EDL.file_count - 1 ].name, &file_stat ) == -1 )
        {
            TRY_SUCCESS;
            return FAIL;
        }

        EDL.files[ EDL.file_count - 1 ].mod_date = file_stat.st_mtime;

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( EDL.file_count > 1 )
            EDL.files = T_realloc( EDL.files,
                                   --EDL.file_count * sizeof *EDL.files );
        else
        {
            EDL.files = T_free( EDL.files );
            EDL.file_count = 0;
        }

        return FAIL;
    }

    return OK;
}


/*-------------------------------------------------------------------*
 * Function for running an experiment without using any GUI elements
 *-------------------------------------------------------------------*/

static void
no_gui_run( void )
{
    /* Read in the EDL file and analyze it */

    if (    ! scan_main( EDL.files->name, In_file_fp )
         || EDL.compilation.error[ FATAL ]  != 0
         || EDL.compilation.error[ SEVERE ] != 0
         || EDL.compilation.error[ WARN ]   != 0 )
        exit( EXIT_FAILURE );

    /* Start the child process for running the experiment */

    if ( ! run( ) )
        exit( EXIT_FAILURE );

    /* Wait until the child process raised the QUITTING signal, then run
       the handler for new data (which sends a confirmation to the child
       to allow it to finally quit) and then wait for it to finally die
       before exiting. Also ake care of the case that the chidl died for
       some unforseen reason. */

    while (    Fsc2_Internals.child_is_quitting == QUITTING_UNSET
            && Fsc2_Internals.child_pid > 0 )
        pause( );

    if ( Fsc2_Internals.child_pid > 0 )
        new_data_handler( );

    while ( Fsc2_Internals.child_pid > 0 )
        pause( );

    run_sigchld_callback( NULL, Fsc2_Internals.check_return );

    exit( Fsc2_Internals.check_return );
}


/*------------------------------------------------------------------*
 *------------------------------------------------------------------*/

static void
check_run( void )
{
    static bool user_break = UNSET;
    long i;
    int fd_flags;


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

    if ( ! ( In_file_fp = fopen( EDL.files->name, "r" ) )  )
        exit( EXIT_FAILURE );

    if ( ( fd_flags = fcntl( fileno( In_file_fp ), F_GETFD ) ) < 0 )
        fd_flags = 0;
    fcntl( fileno( In_file_fp ), F_SETFD, fd_flags | FD_CLOEXEC );

    user_break = UNSET;

    fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_watch );

    if (    ! scan_main( EDL.files->name, In_file_fp )
         || user_break
         || EDL.compilation.error[ FATAL ]  != 0
         || EDL.compilation.error[ SEVERE ] != 0
         || EDL.compilation.error[ WARN ]   != 0 )
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

    for ( i = 0; i < Fsc2_Internals.num_test_runs; i++ )
    {
        if ( ! run( ) )
            exit( EXIT_FAILURE );

        while ( fl_check_forms( ) != GUI.main_form->quit )
            if ( Fsc2_Internals.child_pid == 0 )
                idle_handler( );
            else
                new_data_handler( );

        if ( Fsc2_Internals.check_return != EXIT_SUCCESS )
            exit( Fsc2_Internals.check_return );
    }

    exit( EXIT_SUCCESS );
}


/*-------------------------------------------------------*
 * Do a preliminary check of the command line arguments.
 * The main handling of arguments is only done after the
 * graphics initialisation, but some arguments have to
 * be dealt with earlier.
 *-------------------------------------------------------*/

static int
scan_args( int   * argc,
           char  * argv[ ],
           char ** fname )
{
    int flags = getenv( "FSC2_LOCAL_EXEC" ) ? LOCAL_EXEC : 0;
    int cur_arg = 1;
    int i;


    if ( *argc == 1 )
        return flags;

    while ( cur_arg < *argc )
    {
        /* Check for option that's mostly used for debugging when everything
         (mostly shared libraries) is to be loaded from the current directory
         (and below) instead of what's installed in the usual places */

        if (    strlen( argv[ cur_arg ] ) == 11
             && ! strcmp( argv[ cur_arg ], "-local_exec" ) )
        {
            flags |= LOCAL_EXEC;
            for ( i = cur_arg; i < *argc; i++ )
                argv[ i ] = argv[ i + 1 ];
            *argc -= 1;
            continue;
        }

        /* Check for '-t' option that means "do a test run without graphics".
           If it's given test for options that aren't compatible and start
           the test and return its result */

        if (    strlen( argv[ cur_arg ] ) == 2
             && ! strcmp( argv[ cur_arg ], "-t" ) )
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

            if ( flags & NO_GUI_RUN )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-t' and "
                        "'-ng' at once.\n" );
                usage( EXIT_FAILURE );
            }

            /* Missing file name with "-t" option? */

            if ( ! argv[ ++cur_arg ] )
            {
                fprintf( stderr, "fsc2 -t: No input file.\n" );
                exit( EXIT_FAILURE );
            }

            Fsc2_Internals.cmdline_flags |= TEST_ONLY;

            lower_permissions( );

            if (    ! get_edl_file( argv[ cur_arg ] )
                 || ! ( In_file_fp = fopen( EDL.files->name, "r" ) ) )
                exit( EXIT_FAILURE );

            set_main_signals( );

            exit( scan_main( EDL.files->name, In_file_fp ) ?
                  EXIT_SUCCESS : EXIT_FAILURE );
        }

        /* Check for '-nw' and '-ng' option (they are identical) that tells
           to run the script passed as another argument without graphics.
           The option must be followed directly by the script file name. */

        if (    strlen( argv[ cur_arg ] ) == 3
             && (    ! strcmp( argv[ cur_arg ], "-ng" )
                  || ! strcmp( argv[ cur_arg ], "-nw" ) ) )
        {
            if ( flags & DO_CHECK )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-ng' or "
                         "'-nw' and '-X' at once.\n" );
                usage( EXIT_FAILURE );
            }

            if ( flags & BATCH_MODE )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-ng' or "
                         "'-nw' and '-B' at once.\n" );
                usage( EXIT_FAILURE );
            }

            if ( flags & ICONIFIED_RUN )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-ng' or "
                         "'-nw' and once.\n" );
                usage( EXIT_FAILURE );
            }

            /* No file name with "-ng" or "-nw" option ? */

            if ( ! argv[ ++cur_arg ] )
            {
                fprintf( stderr, "fsc2 -ng (or '-nw'): No input file.\n" );
                exit( EXIT_FAILURE );
            }

            if (    ! get_edl_file( argv[ cur_arg ] )
                 && access( EDL.files->name, R_OK ) == -1
                 && errno != ENOENT )
                return EXIT_FAILURE;

            if ( ! ( In_file_fp = fopen( EDL.files->name, "r" ) ) )
            {
                fprintf( stderr, "Can't open file '%s' for reading.\n",
                         EDL.files->name );
                exit( EXIT_FAILURE );
            }

            flags |= NO_GUI_RUN;

            *argc -= 2;
            if ( *argc > 1 )
            {
                fprintf( stderr, "Superfluous arguments\n" );
                exit( EXIT_FAILURE );
            }

            return flags;
        }

        /* Check for the '-h' or '--help' option (they are identical) and
           just make the program output usage information */

        if (    (    strlen( argv[ cur_arg ] ) == 2
                  && ! strcmp( argv[ cur_arg ], "-h" ) )
             || ! strcmp( argv[ cur_arg ], "--help" ) )
            usage( EXIT_SUCCESS );

        /* Check for '-s' flag that instructs the program to send a SIGUSR1
           signal to the parent process when initialization is finished */

        if (    strlen( argv[ cur_arg ] ) == 2
             && ! strcmp( argv[ cur_arg ], "-s" ) )
        {
            flags |= DO_SIGNAL;
            for ( i = cur_arg; i < *argc; i++ )
                argv[ i ] = argv[ i + 1 ];
            *argc -= 1;
            continue;
        }

        /* Check for '--delete' flag that tells us to remove the EDL script
           file when done with it (typically used by scripts that generate
           a temporary file and then start fsc2) */

        if ( ! strcmp( argv[ cur_arg ], "--delete" ) )
        {
            flags |= DO_DELETE;
            for ( i = cur_arg; i < *argc; i++ )
                argv[ i ] = argv[ i + 1 ];
            *argc -= 1;
            continue;
        }

        /* Check for '-noCrashMail' flag that tells us not to try to send
           an email if fsc2 crashed */

        if ( ! strcmp( argv[ cur_arg ], "-noCrashMail" ) )
        {
            flags |= NO_MAIL;
            for ( i = cur_arg; i < *argc; i++ )
                argv[ i ] = argv[ i + 1 ];
            *argc -= 1;
            continue;
        }

        /* Check for '--noBalloons' flag that tells us not to show help
           messages ("ballons") when the mouse hovers over a button for some
           time */

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
            fprintf( stderr, "Option '-non-exclusive' is deprecated.\n" );
            for ( i = cur_arg; i < *argc; i++ )
                argv[ i ] = argv[ i + 1 ];
            *argc -= 1;
            continue;
        }

        /* Check for '-S' flag that tells us the user wants the EDL script
           (which name has to be the next argument) to be tested and run
           immediately without any further interaction */

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

            if ( flags & BATCH_MODE )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-S' and "
                        "'-ng' at once.\n" );
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

        /* Check for '-T' flag that tells us the user wants the EDL script
           (which name has to be the next argument) to be tested immediately
           without any further interaction */

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

        /* Test for the '-B' flag that tells us to run in batch mode (i.e
           all following arguments are taken to be EDL script file names
           that are tested and run, one after another, without any user
           interaction */

        if (    strlen( argv[ cur_arg ] ) == 2
             && ! strcmp( argv[ cur_arg ], "-B" ) )
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

        /* Check for the '-I' option that requests the EDL script (which name
           nust be the next argument to be tested and run immediately with
           the main window being iconified */

        if (    strlen( argv[ cur_arg ] ) == 2
             && ! strcmp( argv[ cur_arg ], "-I" ) )
        {
            if ( flags & DO_CHECK )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-I' and "
                        "'-t' at once.\n" );
                usage( EXIT_FAILURE );
            }

            if ( flags & DO_START )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-I' and "
                        "'-S' at once.\n" );
                usage( EXIT_FAILURE );
            }

            if ( flags & DO_TEST )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-I' and "
                        "'-T' at once.\n" );
                usage( EXIT_FAILURE );
            }

            if ( flags & NO_GUI_RUN )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-I' and "
                        "'-ng' at once.\n" );
                usage( EXIT_FAILURE );
            }

            if ( argv[ cur_arg ][ 2 ] == '\0' && *argc == cur_arg + 1 )
            {
                fprintf( stderr, "fsc2 -I: No input files.\n" );
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

            flags |= DO_LOAD | ICONIFIED_RUN | DO_START;

            break;
        }

        /* Check for the '-X' option. This is for test purposes only and
           tells how many times a script (which must be the next argument)
           is to be run */

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

            if ( flags & NO_GUI_RUN )
            {
                fprintf( stderr, "fsc2: Can't have both flags '-X' and "
                        "'-ng' at once.\n" );
                usage( EXIT_FAILURE );
            }

            if ( argv[ cur_arg ][ 2 ] == '\0' )
                Fsc2_Internals.num_test_runs = 1;
            else
                for ( Fsc2_Internals.num_test_runs = 0, i = 2;
                      argv[ cur_arg ][ i ] != '\0'; i++ )
                    if ( isdigit( ( unsigned char ) argv[ cur_arg ][ i ] ) )
                        Fsc2_Internals.num_test_runs =
                                             Fsc2_Internals.num_test_runs * 10
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

            if ( Fsc2_Internals.num_test_runs == 0 )
            {
                Fsc2_Internals.cmdline_flags |= TEST_ONLY | NO_MAIL;
                
                lower_permissions( );
                if (    ! get_edl_file( *fname )
                     || ! ( In_file_fp = fopen( EDL.files->name, "r" ) ) )
                    exit( EXIT_FAILURE );
                exit( scan_main( EDL.files->name, In_file_fp ) ?
                      EXIT_SUCCESS : EXIT_FAILURE );
            }

            flags |= NO_MAIL | DO_LOAD | DO_CHECK;
        }

        cur_arg++;
    }

    return flags;
}


/*----------------------------------------------------------*
 * This function is called after either exit() is called or
 * fsc2 returns from main(). Here some cleanup is done that
 * is necessary even if the program crashed. Please take
 * care: this function also gets called when the processes
 * doing the experiment are finished!
 *----------------------------------------------------------*/

static void
final_exit_handler( void )
{
    size_t len = sizeof( ( ( struct sockaddr_un * ) NULL )->sun_path );
    char sock_file[ len ];


    /* Stop the child process and the HTTP server */

    if ( Fsc2_Internals.child_pid > 0 )
        kill( Fsc2_Internals.child_pid, SIGTERM );

    if ( Fsc2_Internals.http_pid > 0 )
        kill( Fsc2_Internals.http_pid, SIGTERM );

    /* Do everything required to end the program */

    T_free( EDL.File_List );

    if ( In_file_fp )
        fclose( In_file_fp );

    if ( Delete_old_file && EDL.files->name )
        unlink( EDL.files->name );

    TRY
    {
        snprintf( sock_file, len, P_tmpdir "/fsc2_%lu.uds", ( long ) getpid );
        unlink( sock_file );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fprintf( stderr, "Couldn't delete socket file '" P_tmpdir
                 "/fsc2_%lu.uds'.\n", ( long ) getpid );
    }

    if ( EDL.files )
    {
        while ( EDL.file_count > 0 )
            T_free( EDL.files[ --EDL.file_count ].name );
        EDL.files = T_free( EDL.files );
    }

    T_free( G.color_hash );

    /* Delete all shared memory and the semaphore (if it still exists) */

    delete_all_shm( );
    if ( Comm.mq_semaphore >= 0 )
        sema_destroy( Comm.mq_semaphore );

    /* If the program was killed by a signal indicating an unrecoverable
       error print out a message and (if this feature isn't switched off)
       send an email */

    if ( Crash.signo != 0 && Crash.signo != SIGTERM )
    {
        if (    * ( ( int * ) Xresources[ NOCRASHMAIL ].var ) == 0
             && ! ( Fsc2_Internals.cmdline_flags & NO_MAIL ) )
        {
            death_mail( );
            fprintf( stderr, "A crash report has been sent to %s\n",
                     MAIL_ADDRESS );
        }

#if defined _GNU_SOURCE
        fprintf( stderr, "fsc2 (%d) killed by %s signal.\n", getpid( ),
                 strsignal( Crash.signo ) );
#else
        fprintf( stderr, "fsc2 (%d) killed by signal %d.\n", getpid( ),
                 Crash.signo );
#endif
    }
}


/*-------------------------------------------------------------------*
 * load_file() is used for loading or reloading a file, depending on
 * the value of the flag 'reload'. It's also the callback function
 * for the "Load" and "Reload" buttons.
 * reload == 0: read new file, reload == 1: reload file
 *-------------------------------------------------------------------*/

void
load_file( FL_OBJECT * a  UNUSED_ARG,
           long        reload )
{
    const char *fn;
    EDL_Files_T *old_list = EDL.files;
    size_t old_count = EDL.file_count;
    FILE *fp;
    int fd_flags;
    char tmp_file[ ] = P_tmpdir "/fsc2.stash.XXXXXX";
    int c;
    int tmp_fd;
    FILE *tmp_fp;


    CLOBBER_PROTECT( old_count );

    /* If new file is to be loaded get its name and store it, otherwise use
       the previous name */

    if ( ! reload )
    {
        if (    ! GUI.main_form->Load->u_ldata
             && ! GUI.main_form->Load->u_cdata )
        {
            fn = fsc2_show_fselector( "Select EDL file:", NULL, "*.edl",
                                      NULL );
            if ( ! fn || ! *fn )
                return;
        }
        else
        {
            fn = GUI.main_form->Load->u_cdata;
            GUI.main_form->Load->u_cdata = NULL;
        }

        EDL.files = NULL;
        EDL.file_count = 0;

        if ( ! get_edl_file( fn ) )
        {
            EDL.files = old_list;
            EDL.file_count = old_count;
            return;
        }
    }
    else                              /* call via reload button */
    {
        /* Quit if no file was loaded */

        if ( ! EDL.files )
        {
            fl_show_alert( "Error", "No file is loaded yet.", NULL, 1 );
            return;
        }

        EDL.files = NULL;
        EDL.file_count = 0;

        if ( ! get_edl_file( old_list->name ) )
        {
            if ( access( old_list->name, R_OK ) == -1 )
                fl_show_alert( "Error", "Can't reload, file isn't readable "
                               "anymore.", old_list->name, 1 );
            EDL.files = old_list;
            EDL.file_count = old_count;
            return;
        }

        if ( In_file_fp )
        {
            fclose( In_file_fp );
            In_file_fp = NULL;
        }
    }

    /* Test if we've got a chance to open the file for reading */

    if ( access( EDL.files->name, R_OK ) == -1 )
    {
        if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
        {
            if ( old_list && old_list != EDL.files )
            {
                while ( old_count > 0 )
                    T_free( old_list[ --old_count ].name );
                T_free( old_list );
            }

            exit( EXIT_FAILURE );
        }

        if ( Fsc2_Internals.cmdline_flags & BATCH_MODE )
        {
            fprintf( stderr, "%s: '%s'.\n",
                     errno == ENOENT ?
                     "File not found" : "Can't open file for reading",
                     EDL.files->name );

            Is_loaded = UNSET;
            Is_tested = UNSET;

            if ( old_list && old_list != EDL.files )
            {
                while ( old_count > 0 )
                    T_free( old_list[ --old_count ].name );
                T_free( old_list );
            }

            T_free( EDL.files[ --EDL.file_count ].name );
            T_free( EDL.files );

            return;
        }

        fl_show_alert( "Error",
                       errno == ENOENT ? "File not found:" :
                                         "Can't open file for reading:",
                       EDL.files->name, 1 );

        if ( old_list && old_list != EDL.files )
        {
            T_free( EDL.files[ --EDL.file_count ].name );
            T_free( EDL.files );
        }   

        EDL.file_count = old_count;
        EDL.files = old_list;

        return;
    }

    /* Now we create a copy of the file so that we can still operate on the
       file in the currently loaded form at later times even when the user
       changes the file in between. In all the following we work on the
       file pointer 'fp' of this temporary file even though the file name
       'EDL.files->name' shown to the user still refers to the original file */

    if ( ( tmp_fd = mkstemp( tmp_file ) ) < 0 )
    {
        if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
        {
            if ( old_list && old_list != EDL.files )
            {
                while ( old_count > 0 )
                    T_free( old_list[ --old_count ].name );
                T_free( old_list );
            }
            exit( EXIT_FAILURE );
        }

        if ( Fsc2_Internals.cmdline_flags & BATCH_MODE )
        {
            fprintf( stderr, "Can't open a temporary file, skipping input "
                     "file '%s'.\n", EDL.files->name );

            Is_loaded = UNSET;
            Is_tested = UNSET;

            if ( old_list && old_list != EDL.files )
            {
                while ( old_count > 0 )
                    T_free( old_list[ --old_count ].name );
                T_free( old_list );
            }

            T_free( EDL.files[ --EDL.file_count ].name );
            T_free( EDL.files );

            return;
        }

        fl_show_alert( "Error", "Sorry, can't open a temporary file.",
                       NULL, 1 );

        if ( old_list && old_list != EDL.files )
        {
            T_free( EDL.files[ --EDL.file_count ].name );
            T_free( EDL.files );
        }
            
        EDL.file_count = old_count;
        EDL.files = old_list;

        return;
    }

    if ( ( fd_flags = fcntl( tmp_fd, F_GETFD ) ) < 0 )
        fd_flags = 0;
    fcntl( tmp_fd, F_SETFD, fd_flags | FD_CLOEXEC );

    unlink( tmp_file );

    if ( ( tmp_fp = fdopen( tmp_fd, "w+" ) ) == 0 )
    {
        close( tmp_fd );

        if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
        {
            if ( old_list && old_list != EDL.files )
            {
                while ( old_count > 0 )
                    T_free( old_list[ --old_count ].name );
                T_free( old_list );
            }

            exit( EXIT_FAILURE );
        }

        if ( Fsc2_Internals.cmdline_flags & BATCH_MODE )
        {
            fprintf( stderr, "Can't open a temporary file, skipping input "
                     "file '%s'.\n", EDL.files->name );

            Is_loaded = UNSET;
            Is_tested = UNSET;

            if ( old_list && old_list != EDL.files )
            {
                while ( old_count > 0 )
                    T_free( old_list[ --old_count ].name );
                T_free( old_list );
            }

            T_free( EDL.files[ --EDL.file_count ].name );
            T_free( EDL.files );

            return;
        }

        fl_show_alert( "Error", "Sorry, can't open a temporary file.",
                       NULL, 1 );

        if ( old_list && old_list != EDL.files )
        {
            T_free( EDL.files[ --EDL.file_count ].name );
            T_free( EDL.files );
        }
            
        EDL.file_count = old_count;
        EDL.files = old_list;

        return;
    }

    if ( ! ( fp = fopen( EDL.files->name, "r" ) ) )
    {
        fclose( tmp_fp );

        if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
        {
            if ( old_list && old_list != EDL.files )
            {
                while ( old_count > 0 )
                    T_free( old_list[ --old_count ].name );
                T_free( old_list );
            }

            exit( EXIT_FAILURE );
        }

        if ( Fsc2_Internals.cmdline_flags & BATCH_MODE )
        {
            fprintf( stderr, "Can't open input file: '%s'.\n",
                     EDL.files->name );

            Is_loaded = UNSET;
            Is_tested = UNSET;

            if ( old_list && old_list != EDL.files )
            {
                while ( old_count > 0 )
                    T_free( old_list[ --old_count ].name );
                T_free( old_list );
            }

            T_free( EDL.files[ --EDL.file_count ].name );
            T_free( EDL.files );

            return;
        }

        fl_show_alert( "Error", "Sorry, can't open file:",
                       EDL.files->name, 1 );

        if ( old_list && old_list != EDL.files )
        {
            T_free( EDL.files[ --EDL.file_count ].name );
            T_free( EDL.files );
        }
            
        EDL.file_count = old_count;
        EDL.files = old_list;

        return;
    }

    /* Copy the contents of the EDL file into our temporary file */

    while ( ( c = fgetc( fp ) ) != EOF )
        fputc( c, tmp_fp );

    fclose( fp );
    fp = tmp_fp;

    rewind( fp );
    lseek( tmp_fd, 0, SEEK_SET );           /* paranoia... */

    /* Now that we're sure that we can work with the new file we can delete
       the old file (but only if the new and the old file are different !) */

    if ( old_list && old_list != EDL.files )
    {
        if ( strcmp( old_list->name, EDL.files->name ) )
        {
            if ( Delete_old_file )
                unlink( old_list->name );
            Delete_old_file = Delete_file;
        }
        else      /* don't delete reloaded files - they may have been edited */
            Delete_old_file = UNSET;

        while ( old_count > 0 )
            T_free( old_list[ --old_count ].name );
        T_free( old_list );

        if ( In_file_fp )
            fclose( In_file_fp );
    }

    In_file_fp = fp;

    Delete_file = UNSET;

    /* Set a new window title */

    if ( Fsc2_Internals.title )
        Fsc2_Internals.title = T_free( Fsc2_Internals.title );
    Fsc2_Internals.title = get_string( "fsc2: %s", EDL.files->name );
    fl_set_form_title( GUI.main_form->fsc2, Fsc2_Internals.title );

    /* Read in and display the new file */

    Is_loaded = display_file( EDL.files->name, In_file_fp );

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

    Parse_result = FAIL;
    Is_tested = UNSET;
}


/*-----------------------------------------------------*
 * test_file() does a syntax and plausibility check of
 * the currently loaded file.
 *-----------------------------------------------------*/

void
test_file( FL_OBJECT * a,
           long        b  UNUSED_ARG )
{
    static bool running_test = UNSET;
    static bool user_break = UNSET;
    struct stat file_stat;
    static bool in_test = UNSET;


    a->u_ldata = 0;

    /* While program is being tested the test can be aborted by pressing the
       test button again - in this case we simply throw an exception */

    if ( running_test )
    {
        fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );
        user_break = SET;
        eprint( FATAL, UNSET,
                "Test of program aborted, received user break.\n" );
        running_test = UNSET;
        THROW( EXCEPTION );
    }

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

    if ( ! Is_loaded )
    {
        fl_show_alert( "Error", "Sorry, but no file is loaded.", NULL, 1 );
        in_test = UNSET;
        return;
    }

    if ( Is_tested )
    {
        fl_show_alert( "Warning", "File has already been tested.", NULL, 1 );
        in_test = UNSET;
        return;
    }

    /* Before scanning the file reload it if the file (here we only care
       for the main EDL files, included files haven't been read in yet,
       that only happens during testing) on disk has changed in between and
       the user wants the new version - quit if file can't be read again. */

    stat( EDL.files->name, &file_stat );

    if (    EDL.files->mod_date != file_stat.st_mtime
         && 2 == fl_show_choice( "EDL file on disk is newer than the",
                                 "loaded file. Reload file from disk?",
                                 "", 2, "No", "Yes", "", 1 ) )
    {
        load_file( GUI.main_form->browser, 1 );
        if ( ! Is_loaded )
        {
            in_test = UNSET;
            return;
        }
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
    XFlush( fl_get_display( ) );
    user_break = UNSET;

    /* Parse the input file and close it when we're done with it, everything
       relevant is now stored in memory (unless the parsing was interrupted
       by the user) */

    Parse_result = scan_main( EDL.files->name, In_file_fp );
    if ( ! user_break )
    {
        fclose( In_file_fp );
        In_file_fp = NULL;
    }

    fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );
    running_test = UNSET;

    if ( ! user_break )
    {
        Is_tested = SET;                  /* show that file has been tested */
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

    in_test = UNSET;
}


/*---------------------------------------------------------------*
 * run_file() first does a check of the current file (if this is
 * not already done) and on success starts the experiment.
 *---------------------------------------------------------------*/

void
run_file( FL_OBJECT * a  UNUSED_ARG,
          long        b  UNUSED_ARG )
{
    struct stat file_stat;
    char str1[ 128 ],
         str2[ 128 ];


    if ( ! Is_loaded )              /* check that there is a file loaded */
    {
        fl_show_alert( "Error", "Sorry, but no file is loaded.", NULL, 1 );
        return;
    }

    if ( ! Is_tested )              /* test file if not already done */
    {
        test_file( GUI.main_form->test_file, 1 );
        if ( GUI.main_form->test_file->u_ldata == 1 )  /* user break ? */
            return;
    }
    else
    {
        size_t i;

        /* Test the files making up the EDL progam aren't newer than what
           we tested, otherwise ask user if (s)he want a reload (and a new
           test) first */

        for ( i = 0; i < EDL.file_count; i++ )
        {

            if ( stat( EDL.files[ i ].name, &file_stat ) == -1 )
                continue;

            if (    EDL.files[ i ].mod_date != file_stat.st_mtime
                 && 2 == fl_show_choice( "EDL file (or an included file) on "
                                         "disk", "is newer than the loaded "
                                         "EDL script.",
                                         "Reload the file from disk?",
                                         2, "No", "Yes", "", 1 ) )
            {
                load_file( GUI.main_form->browser, 1 );
                if ( ! Is_loaded )
                    return;
                Is_tested = UNSET;
                test_file( GUI.main_form->test_file, 1 );
                if ( GUI.main_form->test_file->u_ldata == 1 ) /* user break ? */
                    return;
                break;
            }
        }
    }

    if ( ! Parse_result )          /* return if program failed the test */
    {
        /* In batch mode write out an error message and trigger the "Quit"
           button to start dealing with the next EDL script. Otherwise
           show an error message. */

        if ( Fsc2_Internals.cmdline_flags & BATCH_MODE )
        {
            fprintf( stderr, "Test of file failed: '%s'.\n",
                     EDL.files->name );
            fl_trigger_object( GUI.main_form->quit );
        }
        else
            fl_show_alert( "Error", "Test of file failed.", NULL, 1 );
        return;
    }

    /* If there were non-fatal errors ask user if she wants to continue.
       In batch mode run even if there were non-fatal errors. */

    if (    ! ( Fsc2_Internals.cmdline_flags & BATCH_MODE )
         && (    EDL.compilation.error[ SEVERE ] != 0
              || EDL.compilation.error[ WARN ] != 0 ) )
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
                *str2 = '\0';
            }
        }
        else
        {
            sprintf( str1, "There where %d warnings.",
                     EDL.compilation.error[ WARN ] );
            *str2 = '\0';
        }

        if ( 1 == fl_show_choice( str1, str2, "Continue to run the program?",
                                  2, "No", "Yes", "", 1 ) )
        {
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
        fl_show_alert( "Error", "Can't run the experiment.",
                       "See browser for more information.", 1 );
}


/*--------------------------------------------------------------------*
 * display_file() is used to put the contents of a file into the main
 * browser, numbering the lines and expanding tab chars. Arguments
 * are the name of the file to be displayed and the FILE pointer of
 * the file.
 *--------------------------------------------------------------------*/

static
bool display_file( char * name,
                   FILE * fp )
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
        while (    ( key = fgetc( fp ) ) != '\n'
                && key != EOF
                && ++cc < FL_BROWSER_LINELENGTH - len - 9 )
        {
            if ( ( char ) key != '\t' )
                *lp++ = ( char ) key;
            else                         /* handling of tab chars */
            {
                do
                    *lp++ = ' ';
                while (    cc++ % TAB_LENGTH
                        && cc < FL_BROWSER_LINELENGTH - len - 3 )
                    /* empty */ ;
                cc--;
            }
        }
        *lp = '\0';

        /* Color section headings */

        lp = line + len + 2;
        if (    ! strncmp( lp, "DEVICES:", 8 )
             || ! strncmp( lp, "DEVICE:", 7 )
             || ! strncmp( lp, "DEVS:\n", 7 )
             || ! strncmp( lp, "DEV:", 4 )
             || ! strncmp( lp, "VARIABLES:", 10 )
             || ! strncmp( lp, "VARIABLE:", 9 )
             || ! strncmp( lp, "VARS:", 5 )
             || ! strncmp( lp, "VAR:", 4 )
             || ! strncmp( lp, "ASSIGNMENTS:", 12 )
             || ! strncmp( lp, "ASSIGNMENT:", 11 )
             || ! strncmp( lp, "ASS:", 4 )
             || ! strncmp( lp, "PHA:", 4 )
             || ! strncmp( lp, "PHAS:", 5 )
             || ! strncmp( lp, "PHASE:", 6 )
             || ! strncmp( lp, "PHASES:", 7 )
             || ! strncmp( lp, "PREPARATIONS:", 13 )
             || ! strncmp( lp, "PREPARATION:", 12 )
             || ! strncmp( lp, "PREPS:", 6 )
             || ! strncmp( lp, "PREP:", 5 )
             || ! strncmp( lp, "EXPERIMENT:", 11 )
             || ! strncmp( lp, "EXP:", 4 )
             || ! strncmp( lp, "ON_STOP:", 8 ) )
        {
            memmove( line + 6, line, strlen( line ) + 1 );
            memcpy( line, "@C24@f", 6 );
        }

        /* Also color all lines staring with a '#' */

        if ( *lp == '#' )
        {
            memmove( line + 5, line, strlen( line ) + 1 );
            memcpy( line, "@C4@f", 5 );
        }

        fl_add_browser_line( GUI.main_form->browser, line );
    }

    rewind( fp );

    /* Unfreeze and thus redisplay the browser */

    fl_unfreeze_form( GUI.main_form->browser->form );
    return OK;
}


/*--------------------------------------------------------------------*
 * Does everything that needs to be done (i.e. deallocating memory,
 * unloading the device drivers, reinitializing all kinds of internal
 * structures etc.) before a new file can be loaded.
 *--------------------------------------------------------------------*/

void
clean_up( void )
{
    int i;


    /* Store some settings of the program */

    fsc2_save_conf( );

    /* Get rid of the last remains of graphics */

    for ( i = X; i <= Y; i++ )
        if ( G_1d.label_orig[ i ] )
            G_1d.label_orig[ i ] = T_free( G_1d.label_orig[ i ] );

    for ( i = X; i <= Z; i++ )
        if ( G_2d.label_orig[ i ] )
            G_2d.label_orig[ i ] = T_free( G_2d.label_orig[ i ] );

    G.is_init = UNSET;
    G.dim = 0;

    /* Clear up the compilation structure */

    for ( i = 0; i < 3; ++i )
        EDL.compilation.error[ i ] = 0;
    for ( i = DEVICES_SECTION; i <= EXPERIMENT_SECTION; ++i )
        EDL.compilation.sections[ i ] = UNSET;

    /* Deallocate memory used for file names */

    EDL.Fname = T_free( EDL.Fname );

    /* Run exit hook functions and unlink modules */

    delete_devices( );
    Need_GPIB = UNSET;
    Need_RULBUS = UNSET;
    Need_LAN = UNSET;
#if ! defined WITHOUT_SERIAL_PORTS
    fsc2_serial_final_reset( );
#endif

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

    /* Delete stored program */

    forget_prg( );
}


/*-----------------------------------------------------------------------*
 * Sets up the signal handlers for all kinds of signals the main process
 * could receive. This probably looks a bit like overkill, but I just
 * want to make sure it doesn't get killed by some meaningless signals
 * and, on the other hand, that on deadly signals it still gets a chance
 * to try to get rid of shared memory and kill the other processes etc.
 *-----------------------------------------------------------------------*/

static void
set_main_signals( void )
{
    struct sigaction sact;
    int sig_list[ ] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT,
                        SIGFPE, SIGALRM, SIGSEGV, SIGPIPE, SIGTERM,
                        SIGUSR1, SIGUSR2, SIGCHLD, SIGCONT, SIGTTIN,
                        SIGTTOU, SIGBUS, SIGVTALRM };
    size_t i;


    for ( i = 0; i < NUM_ELEMS( sig_list ); i++ )
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


/*-------------------------------------*
 * Signal handler for the main program
 *-------------------------------------*/

static void
main_sig_handler( int signo )
{
    int errno_saved;
    pid_t pid;
    int status;


    switch ( signo )
    {
        case SIGCHLD :
            errno_saved = errno;
            while ( ( pid = waitpid( -1, &status, WNOHANG ) ) > 0 )
            {
                /* Remember when the HTTP server should have died, we need
                   then to reset the button for it */

                if ( pid == Fsc2_Internals.http_pid )
                {
                    Fsc2_Internals.http_pid = -1;
                    Fsc2_Internals.http_server_died = SET;
                }

                /* Also store the return status of the child process running
                   'fsc2_clean', it's used to check if everything went fine */

                if (    ! Fsc2_Internals.fsc2_clean_died
                     && pid == Fsc2_Internals.fsc2_clean_pid )
                {
                    Fsc2_Internals.fsc2_clean_pid = 0;
                    Fsc2_Internals.fsc2_clean_died = SET;
                    Fsc2_Internals.fsc2_clean_status_ok = WIFEXITED( status );
                    Fsc2_Internals.fsc2_clean_status = WEXITSTATUS( status );
                }
            }
            errno = errno_saved;
            return;

        case SIGUSR1 :
        case SIGUSR2 :
            return;

        case SIGALRM :
            if ( Fsc2_Internals.tb_wait == TB_WAIT_TIMER_RUNNING )
                Fsc2_Internals.tb_wait = TB_WAIT_TIMER_EXPIRED;
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

        /* All the remaining signals are deadly - we may be able to gather
           some information about the signal, were the crash happened and a
           backtrace which the final_exit_handler() function can use to do
           the right thing. */

        default :
#if ! defined NDEBUG && defined ADDR2LINE
            if (    ! Crash.already_crashed
                 && signo != SIGABRT
                 && ! ( Fsc2_Internals.cmdline_flags & NO_MAIL ) )
            {
                Crash.already_crashed = SET;
                Crash.trace_length = backtrace( Crash.trace, MAX_TRACE_LEN );
            }
#endif

            Crash.signo = signo;
            Crash.already_crashed = SET;
            exit( EXIT_FAILURE );
    }
}


/*----------------------------------------*
 * Function prints help message and exits
 *----------------------------------------*/

void
usage( int return_status )
{
    fprintf( stderr, "Usage: fsc2 [OPTIONS]... [FILE]\n"
             "A program for remote control of spectrometers\n"
             "OPTIONS:\n"
             "  -t FILE    run syntax check on FILE and exit (no graphics)\n"
             "  -T FILE    run syntax check on FILE\n"
             "  -S FILE    start experiment interpreting FILE\n"
             "  -B FILEs   run in batch mode\n"
             "  -I FILE    start with main window iconified\n"
             "  -ng FILE   run experiment without any graphics\n"
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
             "  -toolboxFontSize number\n"
             "             set the size of the font used in the toolbox\n"
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
             "For a complete documentation see either " docdir "fsc2.ps,\n"
             docdir "fsc2.pdf or " docdir "fsc2.html\n"
             "or type \"info fsc2\".\n" );
    exit( return_status );
}


/*---------------------------------------------------------*
 * Idle handler that's running while no experiment is done
 * and which deals with periodic tasks.
 *---------------------------------------------------------*/

int
idle_handler( void )
{
    /* Check for request by the HTTP server and its death */

#if defined WITH_HTTP_SERVER
    if ( Fsc2_Internals.http_pid > 0 )
        http_check( );
    else if ( Fsc2_Internals.http_server_died )
    {
        Fsc2_Internals.http_server_died = UNSET;
        fl_call_object_callback( GUI.main_form->server );
    }
#endif

    return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
