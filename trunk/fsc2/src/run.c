#include "fsc2.h"
#include "gpib.h"


extern int prim_exp_runparse( void );     /* from prim_exp__run_parser.y */

/* Routines of the main process exclusively used in this file */

void new_data_handler( int sig_type, void *data );
void quitting_handler( int sig_type, void *data );
void run_sigchld_handler( int sig_type, void *data );
void init_exp_grafics( void );
FILE *get_save_file( void );
void clear_up_after_measurement( void );
void set_buttons( int active );


/* Routines of the child process doing the measurement */

void run_child( void );
void do_send_handler( int sig_type );
void do_quit_handler_0( int sig_type );
void do_quit_handler_1( int sig_type );
bool do_measurement( void );


/* Global variables used by parent, child and signal handlers */

bool is_data_saved;
bool child_is_ready;
FILE *tf = NULL;

volatile int quitting;


/* Signals sent by the parent and accepted by the child */

#define DO_SEND   SIGUSR1
#define DO_QUIT   SIGUSR2

/* Signals sent by the child and accepted by the parent */

#define NEW_DATA  SIGUSR1
#define QUITTING  SIGUSR2


/*-------------------------------------------------------------------*/
/* run() starts an experiment. To do so it initializes all needed    */
/* devices and opens a new window for displaying the measured data.  */
/* Then a pipe is opened for passing measured data from the child    */
/* process (to be created for acquiring the data) and the main       */
/* process (that just accepts and displays the data). After          */
/* initialising some global variables and setting up signal handlers */
/* used for synchronisation the processes the child process is       */
/* started.                                                          */
/*-------------------------------------------------------------------*/

bool run( void )
{
	const char *line;
	int i;
    char *gpib_log = ( char * ) GPIB_LOG_FILE;


	/* If there are no commands we're already done :) */

	if ( prg_token == NULL || prg_length == 0 )
		return OK;

	/* There can't be more than one experiment - so quit if child_pid != 0 */

	if ( child_pid != 0 )
		return FAIL;

	/* Disable some buttons */

	set_buttons( 0 );
	fl_set_cursor( FL_ObjWin( main_form->run ), XC_watch );

	/* If the devices need the GPIB bus initialize it now */

	if ( need_GPIB && gpib_init( &gpib_log, LL_ERR ) == FAILURE )
	{
		eprint( FATAL, "Can't initialize GPIB bus.\n" );
		THROW( EXCEPTION );
	}

	/* Run all the experiment hooks - on failure reset GPIB bus */

	exit_hooks_are_run = UNSET;
	TRY
	{
		run_exp_hooks( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		run_exit_hooks( );
		gpib_shutdown( );
		set_buttons( 1 );
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		return FAIL;
	}

	/* Open window for displaying measured data and disable save button */

	init_exp_grafics( );
	fl_deactivate_object( run_form->save );
	fl_set_object_lcol( run_form->save, FL_INACTIVE_COL );
	fl_show_form( run_form->run, FL_PLACE_MOUSE, FL_FULLBORDER,
				  "fsc: Run" );
	fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );

	/* Open pipe for passing data from child to parent process */

    if ( pipe( pd ) != 0 )                 /* try to open pipe */
	{
		eprint( FATAL, "Not enough file handles to run experiment." );
		run_exit_hooks( );
		if ( need_GPIB )
			gpib_shutdown( );
		is_data_saved = SET;
		clear_up_after_measurement( );
		return FAIL;
	}

	/* Save current content of the browser into temporary file 'tf' so that
	   new files can be read in while an experiment is still running (adding
	   "%%" to start of every line makes writing out MATHLAB compatible files
	   easier later on) */

	if ( tf != NULL )
		fclose( tf );
	if ( ( tf = tmpfile( ) ) != NULL )
	{
		i = 0;
		while ( ( line = fl_get_browser_line( main_form->browser, ++i ) )
			    != NULL )
			fprintf( tf, "%% %s\n", line );
	}

	/* Here the experiment really starts - process for doing it is forked. */

	is_data_saved = child_is_ready = quitting = UNSET;

	fl_add_signal_callback( NEW_DATA, new_data_handler, NULL );
	fl_add_signal_callback( QUITTING, quitting_handler, NULL );
	fl_add_signal_callback( SIGCHLD, run_sigchld_handler, NULL );

	if ( ( child_pid = fork( ) ) == 0 )     /* start child */
	{
		I_am = CHILD;
		run_child( );
	}

	if ( child_pid != -1 )                  /* return if fork() succeeded */
		return OK;

	switch ( errno )
	{
		case EAGAIN :
			eprint( FATAL, "Not enough system resources left to run the "
					"experiment.\n" );
			break;

		case ENOMEM :
			eprint( FATAL, "Not enough memory left to run the experiment.\n" );
			break;
	}

	fl_remove_signal_callback( SIGCHLD );
	fl_add_signal_callback( SIGCHLD, sigchld_handler, NULL );
	run_exit_hooks( );
	if ( need_GPIB )
		gpib_shutdown( );
	stop_measurement( NULL, 1 );
	is_data_saved = SET;
	clear_up_after_measurement( );
	child_pid = 0;
	return FAIL;
}


/*-------------------------------------------------------------------*/
/* new_data_handler() is the handler for the NEW_DATA signal sent by */
/* the child to the parent if there are new data - only exception:   */
/* the first instance of the signal (when 'child_is_ready' is still  */
/* zero) just tells the parent that the child has now installed its  */
/* signal handlers and is ready to accept signals. The handler       */
/* always sends the DO_SEND signal to the child to allow it to       */
/* continue with whatever it needs to do (the child waits after      */
/* sending data for this signal in order to avoid flooding the       */
/* parent with excessive amounts of data and signals).               */
/*-------------------------------------------------------------------*/

void new_data_handler( int sig_type, void *data )
{
	data = data;

	if ( sig_type != NEW_DATA )
		return;

	if ( ! child_is_ready )         /* is this the very first signal ? */
	{
		child_is_ready = SET;
		kill( child_pid, DO_SEND );
	}
	else
		fl_trigger_object( run_form->redraw_dummy);
}


/*----------------------------------------------------------------*/
/* quitting_handler() is the handler for the QUITTING signal sent */
/* by the child when it exits normally: It sets a global variable */
/* and reacts by sending (another) DO_QUIT signal.                */
/*----------------------------------------------------------------*/

void quitting_handler( int sig_type, void *data )
{
	data = data;

	if ( sig_type != QUITTING )
		return;

	quitting = SET;
	kill( child_pid, DO_QUIT );
}


/*-------------------------------------------------------------------*/
/* run_sigchld_handler() is the handler for SIGCHLD signals received */
/* while a measurement is running. It checks if the child doing the  */
/* measurement died - if either it died prematurely, i.e. without    */
/* notifying the parent by a QUITTING signal, or it signals a hard-  */
/* ware error via its return status an error message is output. If   */
/* the child died the post-measurement clean-up is done.             */
/*-------------------------------------------------------------------*/

void run_sigchld_handler( int sig_type, void *data )
{
	int return_status;


	data = data;

	if ( sig_type != SIGCHLD )
		return;

	if ( wait( &return_status ) != child_pid )
		return;                       /* if some other child process died... */

	fl_remove_signal_callback( SIGCHLD );
	fl_add_signal_callback( SIGCHLD, sigchld_handler, NULL );

	if ( ! quitting )            /* missing notification by the child ? */
		fl_show_alert( "FATAL Error", "Experiment stopped prematurely.",
					   NULL, 1 );

	if ( ! return_status )       /* return status indicates hardware error ? */
		fl_show_alert( "FATAL Error", "Experiment was stopped due to",
					   "some kind of hardware problem.", 1 );

	stop_measurement( NULL, 1 );
	child_pid = 0;                   /* experiment is completely finished */
}


/*----------------------------------------------------------------------*/
/* stop_measurement() has two functions - if called with a parameter    */
/* b == 0 it serves as the callback for the stop button and just sends  */
/* a DO_QUIT signal to the child telling it to exit. This in turn will  */
/* cause another call of the function, this time with b == 1 by         */
/* run_sigchld_handler(), catching the SIGCHLD from the child, in order */
/* to get the post-measurement clean-up done.                           */
/*----------------------------------------------------------------------*/

void stop_measurement( FL_OBJECT *a, long b )
{
	a = a;

	if ( b == 0 )                      /* callback from stop button ? */
	{
		if ( child_pid != 0 )          /* is child still alive ? */
		{
			fl_set_cursor( FL_ObjWin( run_form->stop ), XC_watch );
			kill( child_pid, DO_QUIT );
		}
		else
		{
			fl_activate_object( run_form->save );
			fl_set_object_lcol( run_form->save, FL_BLACK );
			clear_up_after_measurement( );
		}

		return;
	}

	close( pd[ READ ] );               /* close also end of pipe */
	close( pd[ WRITE ] );              /* close also write end of pipe */
	fl_remove_signal_callback( NEW_DATA );
	fl_remove_signal_callback( QUITTING );

	/* reset all the devices and finally the GPIB bus */

	run_exit_hooks( );
	if ( need_GPIB )
		gpib_shutdown( );

	fl_activate_object( run_form->save );
	fl_set_object_lcol( run_form->save, FL_BLACK );
	fl_set_object_label( run_form->stop, "Quit" );
	fl_set_cursor( FL_ObjWin( run_form->stop ), XC_left_ptr );
}


/*----------------------------------------------------------------*/
/* init_exp_grafics() does everything necessary to initialize the */
/* display of the measured data.                                  */
/*----------------------------------------------------------------*/

void init_exp_grafics( void )
{
	fl_clear_xyplot( run_form->xy_plot );
}


/*----------------------------------------------------------*/
/* save_data() allowes to store the measured data after the */
/* measurement is finished.                                 */
/*----------------------------------------------------------*/

void save_data( FL_OBJECT *a, long b )
{
/*	FILE *sf;
	long i;
	int key;
*/

	a = a;
	b = b;

	/* First, get a file name for saving the data */
/*
	if ( ( sf = get_save_file( ) ) == NULL )
		return;
*/
	/* Write program describing the experiment to the file */
/*
	if ( tf != NULL )
	{
		rewind( tf );
		while ( ( key = fgetc( tf ) ) != EOF )
			fputc( key, sf );
		fclose( tf );
		fprintf( sf, "%%\n" );
	}
	else
		fl_show_alert( "Warning", "Sorry, can't save program together",
					   "with the experimental data.", 1 );
*/
	/* Now add factor used for scaling of the data... */
/*
	if ( dev_list.digitizer )
		fprintf( sf, "%% SCALE FACTOR : %e Vs\n%%\n\n",
				 1.0 / digitizer.scale_factor );
	else
		fprintf( sf, "%%\n\n" );
*/
	/* ...and follow with all data (this currently assumes that DataTye is
	   simple long... !!!!!!) */
/*
	for ( i = 0; i < m.act_point; ++i )
		fprintf( sf, "%ld\n", ( long ) *( m.data + i ) );

	fclose( sf );
*/
	is_data_saved = SET;
}


/*------------------------------------------------------------------*/
/* get_svae_file() returns a file pointer or NULL of an opened file */
/* for saving the data. It asks the user for a file name, warns him */
/* if the file already exists and opens the file.                   */
/*------------------------------------------------------------------*/

FILE *get_save_file( void )
{
	const char *save_file;
	FILE *sf = NULL;
	char *err_msg;
	int result;


	while ( sf == NULL )
	{
		save_file = fl_show_fselector( "Select file:", NULL, "*.*", NULL );
			
		/* No file was selected - ask if user changed her mind about saving
		   the data */

		if ( save_file == NULL )
		{
			if ( fl_show_question( "Save data ?", 1 ) == 0 )
			{
				is_data_saved = 1;
				return NULL;
			}
			else
				continue;
		}

		/* Test if file does already exists - if it does ask for confirmation
		   that overwriting the file is really what the user wants to do */

		if ( access( save_file, F_OK ) == 0 && errno != ENOENT )
		{
			if ( ( err_msg = ( char * ) malloc( ( strlen( save_file ) + 200 )
											    * sizeof( char ) ) ) == NULL )
			{
				strcpy( err_msg, "File " );
				strcat( err_msg, save_file );
				strcat( err_msg, "\ndoes already exist.\n"
					             "Really overwrite the file ?" );
				result = fl_show_question( err_msg, 0 );
				free( err_msg );
				if ( ! result )
					continue;
			}
			else
			{
				if ( ! fl_show_question( "File does already exist.\n"
										 "Really overwrite the file ?", 0 ) )
					continue;
			}
		}

		/* Test if the file can be opened for writing - if not tell user */

		if ( ( sf = fopen( save_file, "w" ) ) == NULL )
			fl_show_alert( "Error:", "Sorry, write access denied for file:",
						   save_file, 1 );
	}

	return sf;
}


/*--------------------------------------------------------------*/
/* clear_up_after_measurement() allows the experimental data to */
/* be stored, frees the memory needed for the data and deletes  */
/* the window for displaying the measured data.                 */
/*--------------------------------------------------------------*/

void clear_up_after_measurement( void )
{
	if ( ! is_data_saved && 
		fl_show_question( "Data are not saved yet.\nSave them now ?", 1 ) )
		save_data( NULL, 0 );

	if ( tf != NULL )
	{
		fclose( tf );
		tf = NULL;
	}

	fl_set_object_label( run_form->stop, "Stop" );
	if ( fl_form_is_visible( run_form->run ) )
		fl_hide_form( run_form->run );

	set_buttons( 1 );
}


/*----------------------------------------------------------*/
/* set_buttons() makes the buttons in the main form for     */
/* running another experient and quitting inaccesible while */
/* a measurement is running and makes them accesible again  */
/* when the experiment is finished.                         */
/*----------------------------------------------------------*/

void set_buttons( int active )
{
	if ( active == 0 )
	{
		fl_freeze_form( main_form->fsc2 );
		fl_deactivate_object( main_form->run );
		fl_set_object_lcol( main_form->run, FL_INACTIVE_COL );
		fl_deactivate_object( main_form->quit );
		fl_set_object_lcol( main_form->quit, FL_INACTIVE_COL );
		fl_unfreeze_form( main_form->fsc2 );
	}
	else
	{
		fl_freeze_form( main_form->fsc2 );
		fl_activate_object( main_form->run );
		fl_set_object_lcol( main_form->run, FL_BLACK );
		fl_activate_object( main_form->quit );
		fl_set_object_lcol( main_form->quit, FL_BLACK );
		fl_unfreeze_form( main_form->fsc2 );
	}

	XFlush( fl_get_display( ) );
}



/**************************************************************/
/*                                                            */
/*         Here's the code for the child process...           */
/*                                                            */
/**************************************************************/


volatile bool do_send;        /* globals used with the signal handlers */
volatile bool do_quit;
int return_status;


/*------------------------------------------------------------------*/
/* run_child() is the child process for doing the real measurement. */
/* It sets up the signal handlers and its global variables and then */
/* goes into an (infinite) loop. In the loop it first sends a       */
/* NEW_DATA signal (by this first signal it just tells the parent   */
/* that it's ready to accept signals, later on to signal available  */
/* data). Then it sets the DO_QUIT signal handler to the handler    */
/* that just sets a global variable and starts to try to acquire    */
/* data. If this fails or the program reached its end (indicated    */
/* by a zero return of the function 'do_measurement) or it received */
/* a DO_QUIT signal in the  meantime (as indicated by the global    */
/* variable 'do_quit') it exits. After switching back to the "real" */
/* DO_QUIT" handler as the next and final step of the loop it waits */
/* for the global variable indicating a DO_SEND signal to become    */
/* true - this is necessary in order to avoid loosing data by       */
/* flooding the parent with data and signals it might not be able   */
/* to handle.                                                       */
/*------------------------------------------------------------------*/

void run_child( void )
{
	/* Set up pointers and global variables used with the signal handlers,
	   and set handler for DO_SEND signals */

	return_status = OK;
	cur_prg_token = prg_token;
	do_send = do_quit = UNSET;
	signal( DO_SEND, do_send_handler );

	while ( 1 )
	{
		signal( DO_QUIT, do_quit_handler_1 );
		kill( getppid( ), NEW_DATA );             /* tell parent we're ready */

		while ( ! do_send )                       /* wait for parents reply */
			pause( );
		do_send = UNSET;

		if ( ! do_measurement( ) || do_quit )
		{
			if ( cur_prg_token != prg_token + prg_length && ! do_quit )
				return_status = FAIL;             /* signal hardware failure */
			do_quit_handler_0( DO_QUIT );
		}

		signal( DO_QUIT, do_quit_handler_0 );

	}
}


/*--------------------------------------------------------------*/
/* do_send_handler() is the handler for the DO_SEND signal sent */
/* by the parent to the child asking it to send (further) data. */
/*--------------------------------------------------------------*/

void do_send_handler( int sig_type )
{
	if ( sig_type != DO_SEND )
		return;

	signal( DO_SEND, do_send_handler );
	do_send = SET;
}


/*-------------------------------------------------------------------------*/
/* do_quit_handler_0() is one of two handlers for the DO_QUIT signal sent  */
/* by the parent to the child telling it to exit. While the other handler, */
/* do_quit_handler_1(), just sets a flag, this handler really kills the    */
/* child process. It is either invoked by receipt of a DO_QUIT signal or   */
/* when the child exits - either because it received a DO_QUIT signal,     */
/* handled by the other handler, the experiment is finished or there was   */
/* a hardware problem.                                                     */
/* It sets the DO_QUIT handler to the other handler, sends the parent a    */
/* QUITTING signal and waits for the parent to reply by sending  a DO_QUIT */
/* signal.                                                                 */
/*-------------------------------------------------------------------------*/

void do_quit_handler_0( int sig_type )
{
	if ( sig_type != DO_QUIT )
		return;

	close( pd[ READ ] );                    /* close read end of pipe */
	close( pd[ WRITE ] );                   /* close also write end of pipe */
	signal( DO_QUIT, do_quit_handler_1 );     /* set (other) signal handler */
	do_quit = UNSET;
	kill( getppid( ), QUITTING );    /* signal parent that child is exiting */
	while ( ! do_quit )              /* wait for acceptance of this signal  */
		pause( );
	_exit( return_status );
}


/*----------------------------------------------------------------------*/
/* do_quit_handler_1() is the other of the two handlers for the DO_QUIT */
/* signal sent by the parent to the child telling it to exit. It just   */
/* sets a flag and is used while run_child() is acquiring data, when    */
/* killing the child might lead to problems with devices expecting to   */
/* be serviced.                                                         */
/*----------------------------------------------------------------------*/

void do_quit_handler_1( int sig_type )
{
	if ( sig_type != DO_QUIT )
		return;

	do_quit = SET;
}


/*----------------------------------------------------------------------*/
/* do_measurement() runs through and executes all commands. Since it is */
/* executed by the child it's got to honour the `do_quit' flag.         */
/*----------------------------------------------------------------------*/

bool do_measurement( void )
{
	Prg_Token *cur;


	TRY
	{
		if ( cur_prg_token == prg_token )
		{
			save_restore_variables( SET );
			save_restore_pulses( SET );
		}

		while ( cur_prg_token != NULL &&
				cur_prg_token < prg_token + prg_length &&
				! do_quit && ! Is_Written )
		{
			switch ( cur_prg_token->token )
			{
				case '}' :
					cur_prg_token = cur_prg_token->end;
					break;

				case WHILE_TOK :
					cur = cur_prg_token;
					if ( test_condition( cur ) )
					{
						cur->counter++;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case REPEAT_TOK :
					cur = cur_prg_token;
					if ( cur->counter == 0 )
						get_max_repeat_count( cur );
					if ( ++cur->count.repl.act <= cur->count.repl.max )
					{
						cur->counter++;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case FOR_TOK :
					cur = cur_prg_token;
					if ( cur->counter == 0 )
						get_for_cond( cur );

					if ( test_for_cond( cur ) )
					{
						cur->counter++;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case BREAK_TOK :
					cur_prg_token = cur_prg_token->start->end;
					break;

				case CONT_TOK :
					cur_prg_token = cur_prg_token->start;
					break;

				case IF_TOK :
					cur = cur_prg_token;
					cur_prg_token
						       = test_condition( cur ) ? cur->start : cur->end;
					break;

				case ELSE_TOK :
					if ( ( cur_prg_token + 1 )->token == '{' )
						cur_prg_token += 2;
					else
						cur_prg_token++;
					break;

				default :
					prim_exp_runparse( );           /* (re)start the parser */
					break;
			}
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		save_restore_pulses( UNSET );
		save_restore_variables( UNSET );
		return FAIL;
	}

	if ( Is_Written )
	{
		Is_Written = UNSET;
		return OK;
	}
	
	save_restore_pulses( UNSET );
	save_restore_variables( UNSET );
	return FAIL;
}
