/*
  $Id$
*/


#include "fsc2.h"
#include "gpib_if.h"


extern int exp_runparse( void );              /* from exp_run_parser.y */

/* Routines of the main process exclusively used in this file */

void check_for_further_errors( Compilation *c_old, Compilation *c_all );
static void new_data_handler( int signo	);
static void quitting_handler( int signo	);
static void run_sigchld_handler( int signo );
static void set_buttons_for_run( int active );


/* Routines of the child process doing the measurement */

static void run_child( void );
static void set_child_signals( void );
void child_sig_handler( int signo );
static void do_measurement( void );


/* Locally used global variables used in parent, child and signal handlers */

static volatile bool child_is_quitting;

jmp_buf alrm_env;
volatile sig_atomic_t can_jmp_alrm = 0;

static struct sigaction sigchld_oact,
	                    new_data_oact,
	                    quitting_oact;


/*-------------------------------------------------------------------*/
/* run() starts an experiment. To do so it initializes all needed    */
/* devices and opens a new window for displaying the measured data.  */
/* Then a pipe is opened for passing measured data from the child    */
/* process (to be created for acquiring the data) and the main       */
/* process (that just accepts and displays the data). After          */
/* initializing some global variables and setting up signal handlers */
/* used for synchronization the processes the child process is       */
/* started.                                                          */
/*-------------------------------------------------------------------*/

bool run( void )
{
	Compilation compile_test;
	struct sigaction sact;


	/* If there are no commands we're already done */

	if ( prg_token == NULL || prg_length == 0 )
		return OK;

	/* There can't be more than one experiment - so quit if child_pid != 0 */

	if ( child_pid != 0 )
		return FAIL;

	/* Disable some buttons */

	set_buttons_for_run( 0 );
	fl_set_cursor( FL_ObjWin( main_form->run ), XC_watch );
	XFlush( fl_get_display( ) );

	/* If the devices need the GPIB bus initialism it now */

	if ( need_GPIB && gpib_init( NULL, LL_ALL ) == FAILURE )
	{
		eprint( FATAL, "Can't initialize GPIB bus.\n" );
		set_buttons_for_run( 1 );
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		return FAIL;
	}

	/* Make a copy of the errors found while compiling the program */

	memcpy( &compile_test, &compilation, sizeof( Compilation ) );

	/* Run all the experiment hooks - on failure reset GPIB bus */

	TRY
	{
		vars_pop( f_dtime( NULL ) );
		run_exp_hooks( );
		check_for_further_errors( &compile_test, &compilation );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		run_end_of_exp_hooks( );
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
		run_end_of_exp_hooks( );
		if ( need_GPIB )
			gpib_shutdown( );
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		return FAIL;
	}

	/* Open window for displaying measured data */

	start_graphics( );

	child_is_quitting = UNSET;

	/* Set the signal handlers */

	sact.sa_handler = new_data_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	sigaction( NEW_DATA, &sact, &new_data_oact );

	sact.sa_handler = quitting_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	sigaction( QUITTING, &sact, &quitting_oact );

	sact.sa_handler = run_sigchld_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	sigaction( SIGCHLD, &sact, &sigchld_oact );

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
	{
		sema_post( semaphore );      /* we're ready to read data */
		return OK;
	}

	sigaction( SIGCHLD,  &sigchld_oact,  NULL );
	sigaction( NEW_DATA, &new_data_oact, NULL );
	sigaction( QUITTING, &quitting_oact, NULL );

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
	run_end_of_exp_hooks( );

	if ( need_GPIB )
		gpib_shutdown( );
	stop_measurement( NULL, 1 );
	return FAIL;
}


/*-------------------------------------------------------------------*/
/* Checks if new errors etc. were found while running the exp_hooks. */
/* If so ask the user if she wants to continue - unless an exception */
/* is thrown.                                                        */
/*-------------------------------------------------------------------*/

void check_for_further_errors( Compilation *c_old, Compilation *c_all )
{
	Compilation diff;
	char str1[ 128 ],
		 str2[ 128 ];
	int i;


	for ( i = FATAL; i < NO_ERROR; i++ )
		diff.error[ i ] = c_all->error[ i ] - c_old->error[ i ];


	if ( diff.error[ SEVERE ] != 0 || diff.error[ WARN ] != 0 )
	{
		if ( diff.error[ SEVERE ] != 0 )
		{
			if ( diff.error[ WARN ] != 0 )
			{
				sprintf( str1, "Starting the experiment there where %d "
						 "severe", diff.error[ SEVERE ] );
				sprintf( str2, "and %d warnings.", diff.error[ WARN ] );
			}
			else
			{
				sprintf( str1, "Starting the experiment there  where %d "
						 "severe warnings.", diff.error[ SEVERE ] );
				str2[ 0 ] = '\0';
			}
		}
		else
		{
			sprintf( str1, "Starting the experiment there where %d warnings.",
					 diff.error[ WARN ] );
			str2[ 0 ] = '\0';
		}

		if ( 1 == fl_show_choice( str1, str2, "Continue running the program?",
								  2, "No", "Yes", "", 1 ) )
			THROW( EXCEPTION );
	}
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

static void new_data_handler( int signo )
{
	int errno_saved;


	signo = signo;
	errno_saved = errno;

	Message_Queue[ message_queue_high ].shm_id = Key->shm_id;
	Message_Queue[ message_queue_high ].type = Key->type;
	message_queue_high = ( message_queue_high + 1 ) % QUEUE_SIZE;

	if ( Key->type == DATA )
		sema_post( semaphore );

	errno = errno_saved;
}


/*----------------------------------------------------------------*/
/* quitting_handler() is the handler for the QUITTING signal sent */
/* by the child when it exits normally: It sets a global variable */
/* and reacts by sending (another) DO_QUIT signal.                */
/*----------------------------------------------------------------*/

static void quitting_handler( int signo )
{
	int errno_saved;


	signo = signo;
	errno_saved = errno;
	child_is_quitting = SET;
	kill( child_pid, DO_QUIT );
	errno = errno_saved;
}


/*-------------------------------------------------------------------*/
/* run_sigchld_handler() is the handler for SIGCHLD signals received */
/* while a measurement is running. Only the most basic things (i.e   */
/* waiting for the return status and resetting of signal handlers)   */
/* are done here since it's a signal handler. To get the remaining   */
/* stuff done an invisible button is triggered which leads to a call */
/* of its callback function run_sigchld_callback().                  */
/*-------------------------------------------------------------------*/

static void run_sigchld_handler( int signo )
{
	int return_status;
	int pid;
	int errno_saved;


	signo = signo;
	errno_saved = errno;

	if ( ( pid = wait( &return_status ) ) != child_pid )
	{
		errno_saved = errno;
		return;                       /* if some other child process died... */
	}

#if defined ( DEBUG )
	if ( WIFSIGNALED( return_status ) )
		fprintf( stderr, "Child process died due to signal %d\n",
				 WTERMSIG( return_status ) );
#endif

	child_pid = 0;                          /* the child is dead... */
	sigaction( SIGCHLD, &sigchld_oact, NULL );

	run_form->sigchld->u_ldata = ( long ) return_status;
	fl_trigger_object( run_form->sigchld );

	errno = errno_saved;
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
	a = a;                               /* keeps the compiler happy */

	if ( b == 0 )                        /* callback from stop button ? */
	{
		if ( child_pid != 0 )            /* child is still kicking... */
		{
			fl_set_cursor( FL_ObjWin( run_form->stop ), XC_watch );
			kill( child_pid, DO_QUIT );
		}
		else                             /* child has already exited */
		{
			stop_graphics( );
			set_buttons_for_run( 1 );
		}

		return;
	}

	end_comm( );

	/* Remove the signal handlers */

	fl_set_idle_callback( 0, NULL );

	sigaction( NEW_DATA, &new_data_oact, NULL );
	sigaction( QUITTING, &quitting_oact, NULL );

	/* reset all the devices and finally the GPIB bus */

	tools_clear( );
	run_end_of_exp_hooks( );
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
/* for running another experiment and quitting inaccessible */
/* while a measurement is running and makes them accessible */
/* again when the experiment is finished.                   */
/*----------------------------------------------------------*/

static void set_buttons_for_run( int active )
{
	if ( active == 0 )
	{
		fl_freeze_form( main_form->fsc2 );

		fl_deactivate_object( main_form->Load );
		fl_set_object_lcol( main_form->Load, FL_INACTIVE_COL );

		fl_deactivate_object( main_form->reload );
		fl_set_object_lcol( main_form->reload, FL_INACTIVE_COL );

		fl_deactivate_object( main_form->test_file );
		fl_set_object_lcol( main_form->test_file, FL_INACTIVE_COL );

		fl_deactivate_object( main_form->run );
		fl_set_object_lcol( main_form->run, FL_INACTIVE_COL );

		fl_deactivate_object( main_form->quit );
		fl_set_object_lcol( main_form->quit, FL_INACTIVE_COL );

		fl_deactivate_object( main_form->bug_report );
		fl_set_object_lcol( main_form->bug_report, FL_INACTIVE_COL );

		fl_unfreeze_form( main_form->fsc2 );
	}
	else
	{
		fl_freeze_form( main_form->fsc2 );

		fl_activate_object( main_form->Load );
		fl_set_object_lcol( main_form->Load, FL_BLACK );

		fl_activate_object( main_form->reload );
		fl_set_object_lcol( main_form->reload, FL_BLACK );

		fl_activate_object( main_form->test_file );
		fl_set_object_lcol( main_form->test_file, FL_BLACK );

		fl_activate_object( main_form->run );
		fl_set_object_lcol( main_form->run, FL_BLACK );

		fl_activate_object( main_form->quit );
		fl_set_object_lcol( main_form->quit, FL_BLACK );

		fl_activate_object( main_form->bug_report );
		fl_set_object_lcol( main_form->bug_report, FL_BLACK );

		fl_unfreeze_form( main_form->fsc2 );

		notify_conn( UNBUSY_SIGNAL );
	}
}


/**************************************************************/
/*                                                            */
/*         Here's the code for the child process...           */
/*                                                            */
/**************************************************************/


static int return_status;


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
/* a DO_QUIT signal in the  mean time (as indicated by the global   */
/* variable 'do_quit') it exits. After switching back to the "real" */
/* DO_QUIT handler as the next and final step of the loop it waits  */
/* for the global variable indicating a DO_SEND signal to become    */
/* true - this is necessary in order to avoid loosing data by       */
/* flooding the parent with data and signals it might not be able   */
/* to handle.                                                       */
/*------------------------------------------------------------------*/

static void run_child( void )
{
	I_am = CHILD;

    /* Set up pipes for communication with parent process */

	close( pd[ WRITE ] );
	close( pd[ 2 ] );
	pd[ WRITE ] = pd[ 3 ];

	/* Set up pointers, global variables and signal handlers */

	return_status = OK;
	cur_prg_token = prg_token;
	do_quit = UNSET;
	set_child_signals( );

/*{
	bool h = SET;
	while ( h );
}*/

	TRY {
		do_measurement( );                 /* run the experiment */
		TRY_SUCCESS;
	}
	OTHERWISE                              /* catch all exceptions */
		return_status = FAIL;

	close( pd[ READ ] );                   /* close read end of pipe */
	close( pd[ WRITE ] );                  /* close also write end of pipe */

	do_quit = UNSET;
	kill( getppid( ), QUITTING );          /* tell parent that we're exiting */

	/* Using a pause() here is tempting but there exists a race condition
	   between the determination of the value of 'do_quit' and the start of
	   pause() - and it happens... */

	while ( ! do_quit )                    /* wait for acceptance of signal  */
		usleep( 50000 );

	_exit( return_status );                /* ...and that's it ! */
}


/*--------------------------------------------------------------------*/
/* Sets up the signal handlers for all kinds of signals the child may */
/* receive. This probably looks a bit like overkill, but I just wan't */
/* to sure the child doesn't get killed by some meaningless signals   */
/* and, on the other hand, that on deadly signals it still gets a     */
/* chance to try to get rid of shared memory that the parent didn't   */
/* destroyed (in case it was killed by an signal it couldn't catch).  */
/*--------------------------------------------------------------------*/

static void set_child_signals( void )
{
	struct sigaction sact;
	int sig_list[ ] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE,
						SIGSEGV, SIGPIPE, SIGTERM, SIGUSR1, SIGCHLD, SIGCONT,
						SIGTTIN, SIGTTOU, SIGBUS, SIGVTALRM, 0 };
	int i;


	for ( i = 0; sig_list[ i ] != 0; i++ )
	{
		sact.sa_handler = child_sig_handler;
		sigemptyset( &sact.sa_mask );
		sact.sa_flags = 0;
		if ( sigaction( sig_list[ i ], &sact, NULL ) < 0 )
			_exit( -1 );
	}

	sact.sa_handler = child_sig_handler;
	sigemptyset( &sact.sa_mask );
	sigaddset( &sact.sa_mask, SIGALRM );
	sact.sa_flags = 0;
	if ( sigaction( DO_QUIT, &sact, NULL ) < 0 )    /* aka SIGUSR2 */
		_exit( -1 );

	sact.sa_handler = child_sig_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	if ( sigaction( SIGALRM, &sact, NULL ) < 0 )
		_exit( -1 );
}


/*----------------------------------------------------------------*/
/* This is the signal handler for the signals the child receives. */
/* There are only two that are important, SIGUSR2 (aka DO_QUIT )  */
/* and SIGALRM. The other are either ignored or kill the child    */
/* (but in a controlled way to allow deletion of shared memory if */
/* the parent didn't do it as expected.                           */
/* There's a twist: The SIGALRM signal can only come from the     */
/* f_wait() function (see func_util.c). Here we we wait in a	  */
/* pause() for SIGALRM to get a reliable timer. On the other	  */
/* hand, the pause() also has to be interruptible by the		  */
/* DO_QUIT signal, so by falling through from the switch for	  */
/* this signal it is guaranteed that also this signal will end	  */
/* the pause() - it works even when the handler, while handling	  */
/* a SIGALRM signal, is interrupted by a DO_QUIT signal. In all	  */
/* other cases (i.e. when we're not waiting in the pause() in	  */
/* f_wait()) nothing unexpected happens.                          */
/*----------------------------------------------------------------*/

void child_sig_handler( int signo )
{
	switch ( signo )
	{
		case DO_QUIT :                /* aka SIGUSR2 */
			do_quit = SET;
			/* fall through ! */

		case SIGALRM :
			if ( can_jmp_alrm )
			{
				can_jmp_alrm = 0;
				siglongjmp( alrm_env, 1 );
			}
			return;

		/* Ignored signals : */

		case SIGHUP :  case SIGINT :  case SIGUSR1 : case SIGCHLD :
		case SIGCONT : case SIGTTIN : case SIGTTOU : case SIGVTALRM :
			return;
			
		/* All the remaining signals are deadly... */

		default :
			/* Test if parent still exists and if not (i.e. the parent died
			   without sending the child a SGTERM signal) destroy the
			   semaphore and shared memory (as far as the child knows about
			   it) */

			if ( kill( getppid( ), 0 ) == -1 && errno == ESRCH )
			{
				delete_all_shm( );
				sema_destroy( semaphore );
			}

			_exit( -1 );
	}
}
			

/*-------------------------------------------------------------------*/
/* do_measurement() runs through and executes all commands. Since it */
/* is executed by the child it's got to honor the `do_quit' flag.    */
/*-------------------------------------------------------------------*/

static void do_measurement( void )
{
	Prg_Token *cur;


	react_to_do_quit = SET;
	TEST_RUN = UNSET;

	while ( cur_prg_token != NULL &&
			cur_prg_token < prg_token + prg_length )
	{
		TRY
		{
			if ( do_quit && react_to_do_quit )
			{
				/* Don't react to 'do_quit' flag while we're handling the
				   ON_STOP part of he program */

				react_to_do_quit = UNSET;

				if ( On_Stop_Pos < 0 )                /* no ON_STOP part ? */
					cur_prg_token = NULL;             /* -> stop immediately */
				else                                  /* goto ON_STOP part */
					cur_prg_token = prg_token + On_Stop_Pos;

				do_quit = UNSET;

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

				case FOREVER_TOK :
					cur_prg_token = cur_prg_token->start;
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

			TRY_SUCCESS;
		}
		CATCH( USER_BREAK_EXCEPTION )
		{
			TRY_SUCCESS;
			vars_del_stack( );     /* variable stack is probably messed up */
			if ( ! react_to_do_quit )
				THROW( EXCEPTION );
		}
		OTHERWISE
			PASSTHROU( );
	}
}
