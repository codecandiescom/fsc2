/*
  $Id$
*/


#include "fsc2.h"
#include "gpib.h"


extern int exp_runparse( void );              /* from exp_run_parser.y */

/* Routines of the main process exclusively used in this file */

static void new_data_handler( int sig_type );
static void quitting_handler( int sig_type, void *data );
static void run_sigchld_handler( int sig_type );
static void set_buttons_for_run( int active );


/* Routines of the child process doing the measurement */

static void run_child( void );
static void do_send_handler( int sig_type );
static void do_quit_handler( int sig_type );
static void do_measurement( void );


/* Locally used global variables used in parent, child and signal handlers */

static volatile bool child_is_ready;
static volatile bool child_is_quitting;



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
    char *gpib_log = ( char * ) GPIB_LOG_FILE;


	/* If there are no commands we're already done */

	if ( prg_token == NULL || prg_length == 0 )
		return OK;

	/* There can't be more than one experiment - so quit if child_pid != 0 */

	if ( child_pid != 0 )
		return FAIL;

	/* Disable some buttons */

	set_buttons_for_run( 0 );
	fl_set_cursor( FL_ObjWin( main_form->run ), XC_watch );

	/* If the devices need the GPIB bus initialize it now */

	if ( need_GPIB && gpib_init( &gpib_log, LL_ERR ) == FAILURE )
	{
		eprint( FATAL, "Can't initialize GPIB bus.\n" );
		set_buttons_for_run( 1 );
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		return FAIL;
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
		set_buttons_for_run( 1 );
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		return FAIL;
	}

	/* Open pipes for passing data between child and parent process - we need
	 two pipes, one for the parent process to write to the child process and
	 another one for the other way round.*/

	if ( ! setup_comm( ) )
	{
		eprint( FATAL, "Can't set up internal communication channels.\n" );
		run_exit_hooks( );
		if ( need_GPIB )
			gpib_shutdown( );
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		return FAIL;
	}

	/* Open window for displaying measured data */

	start_graphics( );

	child_is_ready = child_is_quitting = UNSET;

	/* Set the signal handlers - for NEW_DATA signals we can't use XForms
	signal handlers because they become blocked by when the display is busy
	and no SEND_DATA is send back to the child, effectively stopping it
	completely, so we have to use the traditional approach for this type of
	signal. Also with SIGCHLD there was a problem - the XForms signal andler
	wasn't reliable enough, so... */

	signal( NEW_DATA, new_data_handler );
	fl_add_signal_callback( QUITTING, quitting_handler, NULL );

	fl_remove_signal_callback( SIGCHLD );
	signal( SIGCHLD, run_sigchld_handler );

	fl_set_idle_callback( new_data_callback, NULL );

	fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );

	/* Here the experiment really starts - process for doing it is forked. */

	if ( ( child_pid = fork( ) ) == 0 )     /* fork the child */
		run_child( );

	close_all_files( );              /* only child is going to write to them */

	close( pd[ READ ] );
	close( pd[ 3 ] );
	pd[ READ ] = pd[ 2 ];

	if ( child_pid != -1 )           /* if fork() succeeded */
		return OK;

	switch ( errno )
	{
		case EAGAIN :
			eprint( FATAL, "Not enough system resources left to run the "
					"experiment.\n" );
			fl_show_alert( "FATAL Error", "Not enough system resources",
						   "left to run the experiment.", 1 );			
			break;

		case ENOMEM :
			eprint( FATAL, "Not enough memory left to run the experiment.\n" );
			fl_show_alert( "FATAL Error", "Not enough memory left",
						   "to run the experiment.", 1 );			
			break;
	}

	child_pid = 0;

	end_comm( );

	fl_add_signal_callback( SIGCHLD, sigchld_handler, NULL );
	run_exit_hooks( );
	if ( need_GPIB )
		gpib_shutdown( );
	stop_measurement( NULL, 1 );
	return FAIL;
}


/*-------------------------------------------------------------------*/
/* new_data_handler() is the handler for the NEW_DATA signal sent by */
/* the child to the parent if there are new data - only exception:   */
/* the first instance of the signal (while 'child_is_ready' is still */
/* zero) just tells the parent that the child has now installed its  */
/* signal handlers and is ready to accept signals. The handler       */
/* always sends the DO_SEND signal to the child to allow it to       */
/* continue with whatever it needs to do (the child waits after      */
/* sending data for this signal in order to avoid flooding the       */
/* parent with excessive amounts of data and signals).               */
/*-------------------------------------------------------------------*/

void new_data_handler( int sig_type )
{
	assert( sig_type == NEW_DATA );

	if ( ! child_is_ready )         /* if this is the very first signal */
	{
		child_is_ready = SET;
		signal( NEW_DATA, new_data_handler );
		kill( child_pid, DO_SEND );
	}
	else
	{
		Message_Queue[ message_queue_high ].shm_id = Key->shm_id;
		Message_Queue[ message_queue_high ].type = Key->type;
		message_queue_high = ( message_queue_high + 1 ) % QUEUE_SIZE;

		signal( NEW_DATA, new_data_handler );
		if ( Key->type == DATA )
			kill( child_pid, DO_SEND );
	}
}


/*----------------------------------------------------------------*/
/* quitting_handler() is the handler for the QUITTING signal sent */
/* by the child when it exits normally: It sets a global variable */
/* and reacts by sending (another) DO_QUIT signal.                */
/*----------------------------------------------------------------*/

void quitting_handler( int sig_type, void *data )
{
	data = data;

	assert( sig_type == QUITTING );
	child_is_quitting = SET;
	kill( child_pid, DO_QUIT );
}


/*-------------------------------------------------------------------*/
/* run_sigchld_handler() is the handler for SIGCHLD signals received */
/* while a measurement is running. Only the most basic things (i.e   */
/* waiting for the return status and resetting of signal handlers)   */
/* are done here since it's a signal handler. To get the remaining   */
/* stuff done an invisible button is triggered which leads to a call */
/* of its callback function run_sigchld_callback().                  */
/*-------------------------------------------------------------------*/

void run_sigchld_handler( int sig_type )
{
	int return_status;
	int pid;


	assert( sig_type == SIGCHLD );

	signal( SIGCHLD, run_sigchld_handler );

	if ( ( pid = wait( &return_status ) ) != child_pid )
		return;                       /* if some other child process died... */

#if defined ( DEBUG )
	if ( WIFSIGNALED( return_status ) )
		fprintf( stderr, "Child process died due to signal %d\n",
				 WTERMSIG( return_status ) );
#endif

	child_pid = 0;                          /* the child is dead... */
	fl_add_signal_callback( SIGCHLD, sigchld_handler, NULL );

	run_form->sigchld->u_ldata = ( long ) return_status;
	fl_trigger_object( run_form->sigchld );
}


/*-----------------------------------------------------------------*/
/* run_sigchld_callback() is the callback for an invisible button  */
/* that is triggered on the death of the child. If the child died  */
/* prematurely, i.e. without notifying the parent by a QUITTING    */
/* signal, or it signals a hardware error via its return status an */
/* error message is output. Than the post-measurement clean-up is  */
/* done.                                                           */
/*-----------------------------------------------------------------*/

void run_sigchld_callback( FL_OBJECT *a, long b )
{
	b = b;

	if ( ! child_is_quitting )   /* missing notification by the child ? */
		fl_show_alert( "FATAL Error", "Experiment stopped prematurely.",
					   NULL, 1 );

	if ( ! a->u_ldata )          /* return status indicates error ? */
		fl_show_alert( "FATAL Error", "Experiment had to be stopped.", "", 1 );

	stop_measurement( NULL, 1 );
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

	if ( b == 0 )                /* callback from stop button ? */
	{
		if ( child_pid != 0 )            /* child is still kicking... */
			kill( child_pid, DO_QUIT );
		else                             /* child has already exited */
		{
			stop_graphics( );
			set_buttons_for_run( 1 );
		}

		return;
	}

	end_comm( );

	/* Remove the signal handlers */

	signal( NEW_DATA, SIG_DFL );
	fl_remove_signal_callback( QUITTING );
	fl_set_idle_callback( 0, NULL );

	/* reset all the devices and finally the GPIB bus */

	run_exit_hooks( );
	if ( need_GPIB )
		gpib_shutdown( );

	fl_freeze_form( run_form->run );
	fl_set_object_label( run_form->stop, "Close" );
	fl_set_button_shortcut( run_form->stop, "C", 1 );
	fl_set_object_helper( run_form->stop, "Remove this window" );
	fl_set_cursor( FL_ObjWin( run_form->stop ), XC_left_ptr );
	fl_unfreeze_form( run_form->run );
	fl_redraw_form( run_form->run );
}


/*----------------------------------------------------------*/
/* set_buttons_for_run() makes the buttons in the main form */
/* for running another experient and quitting inaccesible   */
/* while a measurement is running and makes them accesible  */
/* again when the experiment is finished.                   */
/*----------------------------------------------------------*/

void set_buttons_for_run( int active )
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
	I_am = CHILD;

    /* Set up pipes for communication with parent process */

	close( pd[ WRITE ] );
	close( pd[ 2 ] );
	pd[ WRITE ] = pd[ 3 ];

	/* Set up pointers and global variables used with the signal handlers
	   and set handler for DO_SEND signals */

	return_status = OK;
	cur_prg_token = prg_token;
	do_send = do_quit = UNSET;
	signal( DO_SEND, do_send_handler );
	signal( DO_QUIT, do_quit_handler );

	/* Send parent process a NEW_DATA signal thus indicating that the child
	   process is done with preparations and ready to start the experiment.
	   Wait for reply by parent process (i.e. a DO_SEND signal). */

	kill( getppid( ), NEW_DATA );
	while ( ! do_send )
		pause( );

	do_measurement( );                     /* run the experiment */

	/* Experiment ended prematurely if the end of EDL file wasn't reached */

	if ( cur_prg_token != prg_token + prg_length &&
		 cur_prg_token != NULL )
		return_status = FAIL;

	close( pd[ READ ] );                   /* close read end of pipe */
	close( pd[ WRITE ] );                  /* close also write end of pipe */

	do_quit = UNSET;
	kill( getppid( ), QUITTING );          /* tell parent that we're exiting */
	while ( ! do_quit )                    /* wait for acceptance of signal  */
		pause( );

	_exit( return_status );                /* ...and that's it ! */
}


/*--------------------------------------------------------------*/
/* do_send_handler() is the handler for the DO_SEND signal sent */
/* by the parent to the child asking it to send (further) data. */
/*--------------------------------------------------------------*/

void do_send_handler( int sig_type )
{
	assert( sig_type == DO_SEND );
	signal( DO_SEND, do_send_handler );
	do_send = SET;
}


/*----------------------------------------------------------------------*/
/* do_quit_handler() is the handlers for the DO_QUIT signal sent by the */
/* parent to the child telling it to exit. It just sets a flag instead  */
/* of killing the child immediately because this might lead to problems */
/* with devices expecting to be serviced.                               */
/*----------------------------------------------------------------------*/

void do_quit_handler( int sig_type )
{
	assert( sig_type == DO_QUIT );
	signal( DO_QUIT, do_quit_handler );
	do_quit = SET;
}


/*----------------------------------------------------------------------*/
/* do_measurement() runs through and executes all commands. Since it is */
/* executed by the child it's got to honour the `do_quit' flag.         */
/*----------------------------------------------------------------------*/

void do_measurement( void )
{
	Prg_Token *cur;
	bool react_to_do_quit = SET;


	TEST_RUN = UNSET;

	TRY
	{
		while ( cur_prg_token != NULL &&
				cur_prg_token < prg_token + prg_length )
		{
			if ( do_quit && react_to_do_quit )
			{
				react_to_do_quit = UNSET;

				if ( On_Stop_Pos < 0 )
					cur_prg_token = NULL;
				else
					cur_prg_token = prg_token + On_Stop_Pos;

				continue;
			}

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
					exp_runparse( );               /* (re)start the parser */
					break;
			}
		}

		TRY_SUCCESS;
	}
}
