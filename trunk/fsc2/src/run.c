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
#include "serial.h"
#include "gpib_if.h"


extern int exp_runparse( void );              /* from exp_run_parser.y */
extern FL_resource xresources[ ];             /* from xinit.c */


/* Routines of the main process exclusively used in this file */

static bool start_gpib( void );
static bool no_prog_to_run( void );
static bool init_devs_and_graphics( void );
static void stop_while_exp_hook( FL_OBJECT *a, long b );
static void setup_signal_handlers( void );
static void fork_failure( int stored_errno );
static void check_for_further_errors( Compilation *c_old, Compilation *c_all );
static void quitting_handler( int signo	);
static void run_sigchld_handler( int signo );
static void set_buttons_for_run( int active );




/* Routines of the child process doing the measurement */

static void run_child( void );
static void setup_child_signals( void );
static void child_sig_handler( int signo );
static void do_measurement( void );
static void wait_for_confirmation( void );
static void child_confirmation_handler( int signo );
static void deal_with_program_tokens( void );


/* Locally used global variables used in parent, child and signal handlers */

static volatile bool child_is_quitting;

sigjmp_buf alrm_env;
volatile sig_atomic_t can_jmp_alrm = 0;

static struct sigaction sigchld_oact,
	                    quitting_oact;

static int child_return_status;


/*------------------------------------------------------------------*/
/* run() starts an experiment. To do so it initialises all needed   */
/* devices and opens a new window for displaying the measured data. */
/* Then everything needed for the communication between parent and  */
/* the child to be created is set up, i.e. pipes, a semaphore and a */
/* shared memory segment for storing the type of the data and a key */
/* for a further shared memory segment that will contain the data.  */
/* Finally, after initializing some global variables and setting up */
/* signal handlers used for synchronization of the processes the    */
/* child process is started and thus the experiments begins.        */
/*------------------------------------------------------------------*/

bool run( void )
{
	int stored_errno;


	/* We can't run more than one experiment - so quit if child_pid != 0 */

	if ( Internals.child_pid != 0 )
		return FAIL;

	/* If there's no EXPERIMENT section at all (indicated by 'prg_length'
	   being negative) we're already done */

	if ( EDL.prg_length < 0 )
		return 0;

	/* Start the GPIB bus (and do some changes to the graphics) */

	if ( ! start_gpib( ) )
		return FAIL;

	/* If there are no commands but an EXPERIMENT section label we just run
	   all the init hooks, then the exit hooks and are already done. */

	if ( EDL.prg_token == NULL )
		return no_prog_to_run( );

	/* Initialize devices and graphics */

	if ( ! init_devs_and_graphics(  ) )
		return FAIL;

	/* Setup the signal handlers for signals used for communication between
	   parent and child process and an idle callback function for displaying
	   new data. */

	setup_signal_handlers( );

	fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );

	/* Here the experiment starts - the child process is forked */

	if ( ( Internals.child_pid = fork( ) ) == 0 )
		run_child( );

	stored_errno = errno;            /* for later dissemination... */
	close_all_files( );              /* only child is going to write to them */

	close( Comm.pd[ READ ] );        /* close unused ends of pipes */
	close( Comm.pd[ 3 ] );
	Comm.pd[ READ ] = Comm.pd[ 2 ];

	if ( Internals.child_pid != -1 )           /* if fork() succeeded */
	{
		Internals.mode = PREPARATION;
		return OK;
	}

	/* If forking the child failed we end up here */

	fork_failure( stored_errno );
	return FAIL;
}


/*-------------------------------------------------------------------*/
/* This function first does some smaller changes to the GUI and then */
/* starts the GPIB bus if at least one of the devices needs it.      */
/*-------------------------------------------------------------------*/

static bool start_gpib( void )
{
	/* Disable some buttons and show a watch cusor */

	set_buttons_for_run( 0 );
	fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_watch );
	XFlush( fl_get_display( ) );

	/* If there are devices that need the GPIB bus initialize it now */

	if ( need_GPIB && gpib_init( GPIB_LOG_FILE, GPIB_LOG_LEVEL ) == FAILURE )
	{
		eprint( FATAL, UNSET, "Can't initialize GPIB bus: %s\n",
				gpib_error_msg );
		set_buttons_for_run( 1 );
		fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );
		return FAIL;
	}

	return OK;
}


/*----------------------------------------------------------------------*/
/* This all to be done when the experiment section does not contain any */
/* commands: devices get initialized, again de-initialized, and then we */
/* already return.                                                      */
/*----------------------------------------------------------------------*/

static bool no_prog_to_run( void )
{
	bool ret;


	TRY
	{
		experiment_time( );
		EDL.experiment_time = 0.0;
		vars_pop( f_dtime( NULL ) );

		run_exp_hooks( );
		ret = OK;
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		run_end_of_exp_hooks( );
		vars_del_stack( );                /* some stack variables might be left
										     over if an exception got thrown */
		ret = FAIL;
	}

	run_end_of_exp_hooks( );

	if ( need_GPIB )
		gpib_shutdown( );

	fsc2_serial_cleanup( );

	set_buttons_for_run( 1 );
	fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );

	return ret;
}


/*--------------------------------------------------------------------------*/
/* Function sets a zero point for the dtime() function, runs the experiment */
/* hooks (while still allowing the user to break in between), initializes   */
/* the graphics and creates two pipes for two-way communication between the */
/* parent and child process.                                                */
/*--------------------------------------------------------------------------*/

static bool init_devs_and_graphics( void )
{
	Compilation compile_test;


	/* Make a copy of the errors found while compiling the program */

	compile_test = EDL.compilation;

	TRY
	{
		EDL.do_quit = UNSET;
		EDL.react_to_do_quit = SET;
		fl_set_object_callback( GUI.main_form->run, stop_while_exp_hook, 0 );

		fl_set_object_label( GUI.main_form->run, "Stop" );

		Internals.mode = EXPERIMENT;

		experiment_time( );
		EDL.experiment_time = 0.0;
		vars_pop( f_dtime( NULL ) );

		run_exp_hooks( );

		EDL.react_to_do_quit = UNSET;

		fl_set_object_callback( GUI.main_form->run, run_file, 0 );
		fl_deactivate_object( GUI.main_form->run );
		fl_set_object_label( GUI.main_form->run, "Start" );
		fl_set_object_lcol( GUI.main_form->run, FL_INACTIVE_COL );
		XFlush( fl_get_display( ) );

		check_for_further_errors( &compile_test, &EDL.compilation );

		start_graphics( );
		setup_comm( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		EDL.do_quit = EDL.react_to_do_quit = UNSET;
		fl_set_object_label( GUI.main_form->run, "Start" );
		fl_set_object_callback( GUI.main_form->run, run_file, 0 );

		run_end_of_exp_hooks( );
		vars_del_stack( );             /* some stack variables might be left
										  over when an exception got thrown */
		if ( need_GPIB )
			gpib_shutdown( );
		fsc2_serial_cleanup( );

		Internals.mode = PREPARATION;

		set_buttons_for_run( 1 );
		stop_graphics( );
		fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );
		return FAIL;
	}

	fl_set_object_callback( GUI.main_form->run, run_file, 0 );
	child_is_quitting = UNSET;

	return OK;
}


/*-----------------------------------------------------------------*/
/* Sets the signal handlers for all signals used for communication */
/* between the parent and the child process. Finally, it activates */
/* an idle callback routine which the parent uses for accepting    */
/* displaying of new data.                                         */
/*-----------------------------------------------------------------*/

static void setup_signal_handlers( void )
{
	struct sigaction sact;


	sact.sa_handler = quitting_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	sigaction( QUITTING, &sact, &quitting_oact );

	sact.sa_handler = run_sigchld_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	sigaction( SIGCHLD, &sact, &sigchld_oact );

	fl_set_idle_callback( new_data_callback, NULL );
}


/*--------------------------------------------------------------------------*/
/* Callback handler for the main "Stop" button during device initialization */
/*--------------------------------------------------------------------------*/

static void stop_while_exp_hook( FL_OBJECT *a, long b )
{
	a = a;
	b = b;

	EDL.do_quit = EDL.react_to_do_quit = SET;
}


/*------------------------------------------------------------------------*/
/* Things that remain to be done when forking the child process failed... */
/*------------------------------------------------------------------------*/

static void fork_failure( int stored_errno )
{
	sigaction( SIGCHLD,  &sigchld_oact,  NULL );
	sigaction( QUITTING, &quitting_oact, NULL );

	switch ( stored_errno )
	{
		case EAGAIN :
			eprint( FATAL, UNSET, "Not enough system resources left to run "
					"the experiment.\n" );
			fl_show_alert( "FATAL Error", "Not enough system resources",
						   "left to run the experiment.", 1 );
			break;

		case ENOMEM :
			eprint( FATAL, UNSET, "Not enough memory left to run the "
					"experiment.\n" );
			fl_show_alert( "FATAL Error", "Not enough memory left",
						   "to run the experiment.", 1 );
			break;

		default :
			if ( errno < sys_nerr )
				eprint( FATAL, UNSET, "System error \"%s\" when trying to "
						"start experiment.\n", sys_errlist[ errno ] );
			else
				eprint( FATAL, UNSET, "Unrecognized system error (errno = %d) "
						"when trying to start experiment.\n", errno );
			fl_show_alert( "FATAL Error", "System error on start of "
						   "experiment.", "", 1 );
			break;
	}

	Internals.child_pid = 0;
	end_comm( );

	run_end_of_exp_hooks( );
	if ( need_GPIB )
		gpib_shutdown( );
	fsc2_serial_cleanup( );
	Internals.mode = PREPARATION;

	stop_measurement( NULL, 1 );
}


/*-------------------------------------------------------------------*/
/* Checks if new errors etc. were found while running the exp_hooks. */
/* In this case it asks the user if she wants to continue - unless   */
/* an exception was thrown.                                          */
/*-------------------------------------------------------------------*/

static void check_for_further_errors( Compilation *c_old, Compilation *c_all )
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
				sprintf( str1, "During start of the experiment there where %d "
						 "severe", diff.error[ SEVERE ] );
				sprintf( str2, "and %d warnings.", diff.error[ WARN ] );
			}
			else
			{
				sprintf( str1, "During start of the experiment there where %d "
						 "severe warnings.", diff.error[ SEVERE ] );
				str2[ 0 ] = '\0';
			}
		}
		else
		{
			sprintf( str1, "During start of the experiment there where %d "
					 "warnings.", diff.error[ WARN ] );
			str2[ 0 ] = '\0';
		}

		if ( 1 == fl_show_choice( str1, str2, "Continue running the program?",
								  2, "No", "Yes", "", 1 ) )
			THROW( EXCEPTION );
	}
}


/*---------------------------------------------------------------------*/
/* quitting_handler() is the parents handler for the QUITTING signal   */
/* sent by the child when it exits normally: the handler sets a global */
/* variable and reacts by sending the child a DO_QUIT signal.          */
/*---------------------------------------------------------------------*/

static void quitting_handler( int signo )
{
	int errno_saved;


	signo = signo;
	errno_saved = errno;
	child_is_quitting = SET;

	if ( kill( Internals.child_pid, DO_QUIT ) < 0 )
	{
		Internals.child_pid = 0;                         /* child is dead... */
		sigaction( SIGCHLD, &sigchld_oact, NULL );
		fl_trigger_object( GUI.run_form->sigchld );
		stop_measurement( NULL, 1 );
	}

	errno = errno_saved;
}


/*-------------------------------------------------------------------*/
/* run_sigchld_handler() is the handler for SIGCHLD signals received */
/* while a measurement is running. Only the most basic things (i.e   */
/* waiting for the return status and resetting of signal handlers)   */
/* are done here since it's a signal handler. To get the remaining   */
/* stuff done an invisible button is triggered which leads to a call */
/* of its callback function, run_sigchld_callback().                 */
/*-------------------------------------------------------------------*/

static void run_sigchld_handler( int signo )
{
	int return_status;
#if ! defined NDEBUG
	int pid;
#endif
	int errno_saved;


	signo = signo;
	errno_saved = errno;

#if defined NDEBUG
	if ( wait( &return_status ) != Internals.child_pid )
#else
	if ( ( pid = wait( &return_status ) ) != Internals.child_pid )
#endif
	{
		errno_saved = errno;
		return;                       /* if some other child process died... */
	}

#if ! defined NDEBUG
	if ( WIFSIGNALED( return_status ) )
	{
		fprintf( stderr, "Child process %d died due to signal %d\n",
				 pid, WTERMSIG( return_status ) );
		fflush( stderr );
	}
#endif

	Internals.child_pid = 0;                             /* child is dead... */
	sigaction( SIGCHLD, &sigchld_oact, NULL );

	GUI.run_form->sigchld->u_ldata = ( long ) return_status;
	fl_trigger_object( GUI.run_form->sigchld );

	errno = errno_saved;
}


/*-----------------------------------------------------------------*/
/* run_sigchld_callback() is the callback for an invisible button  */
/* that is triggered on the death of the child. If the child died  */
/* prematurely, i.e. without notifying the parent by a QUITTING    */
/* signal or it signals an hardware error via its return status an */
/* error message is output. Then the post-measurement clean-up is  */
/* done.                                                           */
/*-----------------------------------------------------------------*/

void run_sigchld_callback( FL_OBJECT *a, long b )
{
	b = b;


	if ( ! ( Internals.cmdline_flags & DO_CHECK ) )
	{
		if ( ! child_is_quitting )   /* missing notification by the child ? */
			fl_show_alert( "Fatal Error", "Experiment stopped prematurely.",
					   NULL, 2 );

		if ( ! a->u_ldata )          /* return status indicates error ? */
			fl_show_alert( "Fatal Error", "Experiment had to be stopped.",
						   NULL, 3 );
	}

	stop_measurement( NULL, 1 );
}


/*-----------------------------------------------------------------*/
/* stop_measurement() has two functions: if called with the second */
/* argument b set to 0 it serves as the callback for the "STOP"    */
/* button and just sends a DO_QUIT signal to the child telling it  */
/* to exit. This in turn will cause another call of the function,  */
/* this time with b set to 1 by run_sigchld_handler(), catching    */
/* the SIGCHLD from the child in order to get the post-measurement */
/* clean-up done.                                                  */
/*-----------------------------------------------------------------*/

void stop_measurement( FL_OBJECT *a, long b )
{
	int bn;
	int hours, mins;
	double secs;


	if ( b == 0 )                           /* callback from stop button ? */
	{
		if ( Internals.child_pid != 0 )          /* child is still running */
		{
			/* Only send DO_QUIT signal if the mouse button has been used
			   the user told us he/she would use... */

			if ( GUI.stop_button_mask != 0 )
			{
				bn = fl_get_button_numb( a );
				if ( bn != FL_SHORTCUT + 'S' && bn != GUI.stop_button_mask )
					return;
			}

			kill( Internals.child_pid, DO_QUIT );
		}
		else                             /* child already exited */
		{
			stop_graphics( );
			set_buttons_for_run( 1 );
		}

		return;
	}

	end_comm( );

	/* Remove the signal handlers */

	fl_set_idle_callback( 0, NULL );

	sigaction( QUITTING, &quitting_oact, NULL );

	/* Reset all the devices and finally the GPIB bus */

	tools_clear( );

	run_end_of_exp_hooks( );
	if ( need_GPIB )
		gpib_shutdown( );
	fsc2_serial_cleanup( );

	secs = experiment_time( );
	hours = ( int ) floor( secs / 3600.0 );
	secs -= hours * 3600.0;
	mins = ( int ) floor( secs / 60.0 );
	secs -= mins * 60.0;

	switch ( b )
	{
		case 1 :
			if ( hours > 0 )
				eprint( NO_ERROR, UNSET, "Experiment finished after running "
						"for %d h, %d m and %.2f s.\n", hours, mins, secs );
			else
			{
				if ( mins > 0 )
					eprint( NO_ERROR, UNSET, "Experiment finished after "
							"running for %d m and %.2f s.\n", mins, secs );
				else
					eprint( NO_ERROR, UNSET, "Experiment finished after "
							"running for %.2f s.\n", secs );
			}
			break;

		case 2 :
			if ( hours > 0 )
				eprint( NO_ERROR, UNSET, "Experiment stopped prematurely "
						"after running for %d h, %d m and %.2f s.\n",
						hours, mins, secs );
			else
			{
				if ( mins > 0 )
					eprint( NO_ERROR, UNSET, "Experiment stopped prematurely "
							"after running for %d m and %.2f s.\n",
							mins, secs );
				else
					eprint( NO_ERROR, UNSET, "Experiment stopped prematurely "
							"after running for %.2f s.\n", secs );
			}
			break;

		case 3 :
			if ( hours > 0 )
				eprint( NO_ERROR, UNSET, "Experiment had to be stopped after "
						"running for %d h, %d m and %.2f s.\n",
						hours, mins, secs );
			else
			{
				if ( mins > 0 )
					eprint( NO_ERROR, UNSET, "Experiment had to be stopped "
							"after running for %d m and %.2f s.\n",
							mins, secs );
				else
					eprint( NO_ERROR, UNSET, "Experiment had to be stopped "
							"after running for %.2f s.\n", secs );
			}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	fl_freeze_form( GUI.run_form->run );
	fl_set_object_label( GUI.run_form->stop, "Close" );
	fl_set_button_shortcut( GUI.run_form->stop, "C", 1 );
	if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
		fl_set_object_helper( GUI.run_form->stop, "Remove this window" );
	fl_unfreeze_form( GUI.run_form->run );

	if ( Internals.cmdline_flags & DO_CHECK )
	{
		stop_graphics( );
		set_buttons_for_run( 1 );
		Internals.check_return = b;
		fl_trigger_object( GUI.main_form->quit );
	}
}


/*----------------------------------------------------------*/
/* set_buttons_for_run() makes the buttons in the main form */
/* for running another experiment and quitting inaccessible */
/* while a measurement is running and makes them accessible */
/* again when the experiment is finished.                   */
/*----------------------------------------------------------*/

static void set_buttons_for_run( int active )
{
	fl_freeze_form( GUI.main_form->fsc2 );

	if ( active == 0 )
	{
		fl_deactivate_object( GUI.main_form->Load );
		fl_set_object_lcol( GUI.main_form->Load, FL_INACTIVE_COL );

		fl_deactivate_object( GUI.main_form->reload );
		fl_set_object_lcol( GUI.main_form->reload, FL_INACTIVE_COL );

		fl_deactivate_object( GUI.main_form->quit );
		fl_set_object_lcol( GUI.main_form->quit, FL_INACTIVE_COL );

		fl_deactivate_object( GUI.main_form->bug_report );
		fl_set_object_lcol( GUI.main_form->bug_report, FL_INACTIVE_COL );
	}
	else
	{
		fl_activate_object( GUI.main_form->Load );
		fl_set_object_lcol( GUI.main_form->Load, FL_BLACK );

		fl_activate_object( GUI.main_form->reload );
		fl_set_object_lcol( GUI.main_form->reload, FL_BLACK );

		fl_activate_object( GUI.main_form->run );
		fl_set_object_lcol( GUI.main_form->run, FL_BLACK );

		fl_activate_object( GUI.main_form->quit );
		fl_set_object_lcol( GUI.main_form->quit, FL_BLACK );

		fl_activate_object( GUI.main_form->bug_report );
		fl_set_object_lcol( GUI.main_form->bug_report, FL_BLACK );

		notify_conn( UNBUSY_SIGNAL );
	}

	fl_unfreeze_form( GUI.main_form->fsc2 );
}


/**************************************************************/
/*                                                            */
/*         Here's the code for the child process...           */
/*                                                            */
/**************************************************************/


/*-------------------------------------------------------------------*/
/* run_child() is the child process' code for doing the measurement. */
/* It does some initialisation (e.g. setting up signal handlers) and */
/* then runs the experiment by calling do_measurement(). The         */
/* experiment stops either when all lines of the program have been   */
/* dealt with, an unrecoverable error happend or the user told it    */
/* to stop (by pressing the "Stop" button). When returning from the  */
/* do_measurement() routine it signals the parent process that it's  */
/* going to die and waits for the arrival of an acknowledging        */
/* signal. When the child gets an signal it can't deal with it does  */
/* not return from do_measurement() but dies in the signal handler.  */
/*-------------------------------------------------------------------*/

static void run_child( void )
{
#ifndef NDEBUG
	const char *fcd;
#endif


	Internals.I_am = CHILD;

    /* Set up pipes for communication with parent process */

	close( Comm.pd[ WRITE ] );
	close( Comm.pd[ 2 ] );
	Comm.pd[ WRITE ] = Comm.pd[ 3 ];

	/* Set up pointers, global variables and signal handlers */

	child_return_status = OK;
	EDL.cur_prg_token = EDL.prg_token;
	EDL.do_quit = UNSET;
	setup_child_signals( );

#ifndef NDEBUG
	/* Setting the environment variable FSC2_CHILD_DEBUG to a non-empty
	   string will induce the child to sleep for about 10 hours or until
	   it receives a signal, e.g. from the debugger attaching to it. */

	if ( ( fcd = getenv( "FSC2_CHILD_DEBUG" ) ) != NULL &&
		 fcd[ 0 ] != '\0' )
	{
		fprintf( stderr, "Child process pid = %d\n", getpid( ) );
		fflush( stderr );
		sleep( 36000 );
	}
#endif

	/* Initialization is finished and child can start doing its real work */

	TRY
	{
		do_measurement( );               /* run the experiment */
		child_return_status = OK;
		TRY_SUCCESS;
	}
	OTHERWISE                            /* catch all exceptions */
		child_return_status = FAIL;

	close( Comm.pd[ READ ] );            /* close read end of pipe */
	close( Comm.pd[ WRITE ] );           /* close also write end of pipe */

	wait_for_confirmation( );            /* wait for license to die... */
}


/*-------------------------------------------------------------------*/
/* Set up the signal handlers for all kinds of signals the child may */
/* receive. This probably looks a bit like overkill, but I just want */
/* to make sure the child doesn't get killed by meaningless signals  */
/* and, on the other hand, that on deadly signals it gets a chance   */
/* to try to get rid of shared memory that the parent didn't destroy */
/* (in case it was killed by a signal it couldn't catch).            */
/*-------------------------------------------------------------------*/

static void setup_child_signals( void )
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
			_exit( EXIT_FAILURE );
	}

	sact.sa_handler = child_sig_handler;
	sigemptyset( &sact.sa_mask );
	sigaddset( &sact.sa_mask, SIGALRM );
	sact.sa_flags = 0;
	if ( sigaction( DO_QUIT, &sact, NULL ) < 0 )    /* aka SIGUSR2 */
		_exit( EXIT_FAILURE );

	sact.sa_handler = child_sig_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	if ( sigaction( SIGALRM, &sact, NULL ) < 0 )
		_exit( EXIT_FAILURE );
}


/*----------------------------------------------------------------*/
/* This is the signal handler for the signals the child receives. */
/* There are two signals that have a real meaning, SIGUSR2 (aka   */
/* DO_QUIT ) and SIGALRM. The others are either ignored or kill   */
/* the child (but in a, hopefully, controlled way to allow        */
/* deletion of shared memory if the parent didn't do it itself.   */
/* There's a twist: The SIGALRM signal can only come from the     */
/* f_wait() function (see func_util.c). Here we wait in a pause() */
/* for SIGALRM to get a reliable timer. On the other hand, the    */
/* pause() also has to be interruptible by the DO_QUIT signal, so */
/* by falling through from the switch for this signal it is       */
/* guaranteed that also this signal will end the pause() - it     */
/* works even when the handler, while handling a SIGALRM signal,  */
/* is interrupted by a DO_QUIT signal. In all other cases (i.e.   */
/* when we're not waiting in the pause() in	f_wait()) nothing     */
/* unexpected happens.                                            */
/*----------------------------------------------------------------*/

static void child_sig_handler( int signo )
{
	switch ( signo )
	{
		case DO_QUIT :                             /* aka SIGUSR2 */
			if ( ! EDL.react_to_do_quit )
				return;
			EDL.do_quit = SET;
			/* fall through ! */

		case SIGALRM :
			if ( can_jmp_alrm )
			{
				can_jmp_alrm = 0;
				siglongjmp( alrm_env, 1 );
			}
			return;

		/* Ignored signals : */

		case SIGHUP :
		case SIGINT :
		case SIGUSR1 :
		case SIGCHLD :
		case SIGCONT :
		case SIGTTIN :
		case SIGTTOU :
		case SIGVTALRM :
			return;

		/* All remaining signals are deadly... */

		default :
			if ( ! ( Internals.cmdline_flags & NO_MAIL ) &&
				 signo != SIGTERM )
			{
				DumpStack( );
				death_mail( signo );
			}

			/* Test if parent still exists - if not (i.e. the parent died
			   without sending a SIGTERM signal) destroy the semaphore and
			   shared memory (as far as the child knows about it), kill also
			   the child for connections and remove the lock file. */

			if ( getppid( ) == 1 ||
				 ( kill( getppid( ), 0 ) == -1 && errno == ESRCH ) )
			{
				kill( Internals.conn_pid, SIGTERM );
				delete_all_shm( );
				sema_destroy( Comm.data_semaphore );
				sema_destroy( Comm.request_semaphore );
				setuid( Internals.EUID );
				unlink( FSC2_LOCKFILE );
			}

			EDL.do_quit = SET;
			_exit( EXIT_FAILURE );
	}
}


/*---------------------------------------------------------------------*/
/* The child got to wait for the parent to tell it that it now expects */
/* the child to die (if the child dies unexpectedly the parent assumes */
/* that something went wrong during the experiment). This functions    */
/* sets everything up so that the parents DO_QUIT signal will be       */
/* handled by the correct function, sends a QUITTING signal to the     */
/* parent and then waits for the parent to allow it to exit.           */
/*---------------------------------------------------------------------*/

static void wait_for_confirmation( void )
{
	struct sigaction sact;


	sact.sa_handler = child_confirmation_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	if ( sigaction( DO_QUIT, &sact, NULL ) < 0 )    /* aka SIGUSR2 */
		_exit( EXIT_FAILURE );

	sact.sa_handler = child_confirmation_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	if ( sigaction( SIGALRM, &sact, NULL ) < 0 )
		_exit( EXIT_FAILURE );

	/* Tell parent that we're done and just wait for its DO_QUIT signal - if
	   we can't send the signal to the parent stop the child. */

    if ( kill( getppid( ), QUITTING ) == -1 )
		kill( getpid( ), SIGTERM );             /* commit controlled suicide */

	/* This seemingly infinite loop looks worse than it is, the child really
	   exits in the signal handler for DO_QUIT. */

	while ( 1 )
		pause( );
}


/*-----------------------------------------------------------------*/
/* Childs handler for the DO_QUIT signal while its waiting to die. */
/*-----------------------------------------------------------------*/

static void child_confirmation_handler( int signo )
{
	if ( signo == DO_QUIT )
		_exit( child_return_status );                /* ...and that's it ! */
}


/*-------------------------------------------------------------------*/
/* do_measurement() runs through and executes all commands. Since it */
/* is executed by the child it has got to honor the 'do_quit' flag.  */
/*-------------------------------------------------------------------*/

static void do_measurement( void )
{
	EDL.react_to_do_quit = SET;

	while ( EDL.cur_prg_token != NULL &&
			EDL.cur_prg_token < EDL.prg_token + EDL.prg_length )
	{
		TRY
		{
			/* Check if the 'do_quit' flag got set in the mean time, i.e. the
			   user pressed the STOP button. If there's an ON_STOP label jump
			   to it, otherwise quit immediately. */

			if ( EDL.do_quit && EDL.react_to_do_quit )
			{
				if ( EDL.On_Stop_Pos < 0 )            /* no ON_STOP part ? */
					return;

				EDL.cur_prg_token = EDL.prg_token + EDL.On_Stop_Pos;
				EDL.do_quit = EDL.react_to_do_quit = UNSET;
				continue;
			}

			/* Don't react to STOP button anymore after ON_STOP label has been
			   reached */

			if ( EDL.cur_prg_token == EDL.prg_token + EDL.On_Stop_Pos )
				EDL.react_to_do_quit = EDL.do_quit = UNSET;

			/* Do whatever is necessary to do for the program token */

			deal_with_program_tokens( );

			TRY_SUCCESS;
		}
		CATCH( USER_BREAK_EXCEPTION )
		{
			/* Clean up the variable stack, it may have become messed up */

			vars_del_stack( );

			/* If the exception arrived while we're already dealing with the
			   ON_STOP part this probably is a sign that there is some
			   severe hardware problem and we better stop even though the
			   ON_STOP part isn't been finished yet. */

			if ( ! EDL.react_to_do_quit )
				THROW( EXCEPTION );
		}
		OTHERWISE
			RETHROW( );
	}
}


/*-----------------------------------------------------------------------*/
/* The following is just a lengthy switch to deal with tokens that can't */
/* be handled by the parser directly, i.e. tokens for flow control.      */
/* Control is transfered back to this function whenever the parser finds */
/* the end-of-line delimiter (a colon), a closing curly brace or one of  */
/*  flow control tokens.                                                 */
/*-----------------------------------------------------------------------*/

static void deal_with_program_tokens( void )
{
	Prg_Token *cur;


	switch ( EDL.cur_prg_token->token )
	{
		case '}' :
			EDL.cur_prg_token = EDL.cur_prg_token->end;
			break;

		case WHILE_TOK :
			cur = EDL.cur_prg_token;
			if ( test_condition( cur ) )
			{
				cur->counter = 1;
				EDL.cur_prg_token = cur->start;
			}
			else
			{
				cur->counter = 0;
				EDL.cur_prg_token = cur->end;
			}
			break;

		case UNTIL_TOK :
			cur = EDL.cur_prg_token;
			if ( ! test_condition( cur ) )
			{
				cur->counter = 1;
				EDL.cur_prg_token = cur->start;
			}
			else
			{
				cur->counter = 0;
				EDL.cur_prg_token = cur->end;
			}
			break;

		case REPEAT_TOK :
			cur = EDL.cur_prg_token;
			if ( cur->counter == 0 )
				get_max_repeat_count( cur );
			if ( ++cur->count.repl.act <= cur->count.repl.max )
			{
				cur->counter++;
				EDL.cur_prg_token = cur->start;
			}
			else
			{
				cur->counter = 0;
				EDL.cur_prg_token = cur->end;
			}
			break;

		case FOR_TOK :
			cur = EDL.cur_prg_token;
			if ( cur->counter == 0 )
				get_for_cond( cur );

			if ( test_for_cond( cur ) )
			{
				cur->counter = 1;
				EDL.cur_prg_token = cur->start;
			}
			else
			{
				cur->counter = 0;
				EDL.cur_prg_token = cur->end;
			}
			break;

		case FOREVER_TOK :
			EDL.cur_prg_token = EDL.cur_prg_token->start;
			break;

		case BREAK_TOK :
			EDL.cur_prg_token->start->counter = 0;
			EDL.cur_prg_token = EDL.cur_prg_token->start->end;
			break;

		case NEXT_TOK :
			EDL.cur_prg_token = EDL.cur_prg_token->start;
			break;

		case IF_TOK : case UNLESS_TOK :
			cur = EDL.cur_prg_token;
			EDL.cur_prg_token = test_condition( cur ) ? cur->start : cur->end;
			break;

		case ELSE_TOK :
			if ( ( EDL.cur_prg_token + 1 )->token == '{' )
				EDL.cur_prg_token += 2;
			else
				EDL.cur_prg_token++;
			break;

		default :
			exp_runparse( );                         /* (re)start the parser */
			break;
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
