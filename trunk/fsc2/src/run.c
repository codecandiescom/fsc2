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
#include "gpib_if.h"


volatile bool need_post;

extern int exp_runparse( void );              /* from exp_run_parser.y */
extern FL_resource xresources[ ];             /* from xinit.c */


/* Routines of the main process exclusively used in this file */

static void stop_while_exp_hook( FL_OBJECT *a, long b );
static bool no_prog_to_be_run( void );
static void check_for_further_errors( Compilation *c_old, Compilation *c_all );
static void new_data_handler( int signo	);
static void quitting_handler( int signo	);
static void run_sigchld_handler( int signo );
static void set_buttons_for_run( int active );


/* Routines of the child process doing the measurement */

static void run_child( void );
static void set_child_signals( void );
void child_sig_handler( int signo );
static void do_measurement( void );
static void wait_for_confirmation( void );
static void child_confirmation_handler( int signo );


/* Locally used global variables used in parent, child and signal handlers */

static volatile bool child_is_quitting;

sigjmp_buf alrm_env;
volatile sig_atomic_t can_jmp_alrm = 0;

static struct sigaction sigchld_oact,
	                    new_data_oact,
	                    quitting_oact;

static int child_return_status;



/*------------------------------------------------------------------*/
/* run() starts an experiment. To do so it initialises all needed   */
/* devices and opens a new window for displaying the measured data. */
/* Then everything needed for the communication between parent and  */
/* the child to be created is set up, i.e. pipes, a semaphore and a */
/* shared memory segment for storing the type of the data and a key */
/* for a further shared memory segment will contain the data.       */
/* Finally, after initialising some global variables and setting up */
/* signal handlers used for synchronization the processes the child */
/* is started and thus the experiments begins.                      */
/*------------------------------------------------------------------*/

bool run( void )
{
	Compilation compile_test;
	struct sigaction sact;


	/* If there are no commands but a EXPERIMENT section label we just run
	   all the init hooks, then the exit hooks and then are already done.
	   If, on the other hand, there isn't even an EXPERIMENT section label
	   (as indicated by prg_length being negative) we just return without
	   doing anything at all. */

	if ( prg_token == NULL || prg_length <= 0 )
		return prg_length < 0 ? OK : no_prog_to_be_run( );

	/* There can't be more than one experiment - so quit if child_pid != 0 */

	if ( child_pid != 0 )
		return FAIL;

	/* Disable some buttons */

	set_buttons_for_run( 0 );
	fl_set_cursor( FL_ObjWin( main_form->run ), XC_watch );
	XFlush( fl_get_display( ) );

	/* If there are devices that need the GPIB bus initialize it now */

	if ( need_GPIB && gpib_init( GPIB_LOG_FILE, GPIB_LOG_LEVEL ) == FAILURE )
	{
		eprint( FATAL, UNSET, "Can't initialize GPIB bus: %s\n",
				gpib_error_msg );
		set_buttons_for_run( 1 );
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		return FAIL;
	}

	/* Make a copy of the errors found while compiling the program */

	memcpy( &compile_test, &compilation, sizeof( Compilation ) );

	/* Set zero point for the dtime() function, run the experiment hooks
	   (while allowing the user to break in between), initialize the graphics
	   and create two pipes for two-way communication between the parent and
	   child process. */

	TRY
	{
		vars_pop( f_dtime( NULL ) );

		do_quit = react_to_do_quit = UNSET;
		fl_set_object_callback( main_form->run, stop_while_exp_hook, 0 );

		fl_set_object_label( main_form->run, "Stop" );
		run_exp_hooks( );

		fl_set_object_callback( main_form->run, run_file, 0 );
		fl_deactivate_object( main_form->run );
		fl_set_object_label( main_form->run, "Start" );
		fl_set_object_lcol( main_form->run, FL_INACTIVE_COL );
		XFlush( fl_get_display( ) );

		check_for_further_errors( &compile_test, &compilation );

		start_graphics( );
		setup_comm( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		do_quit = react_to_do_quit = UNSET;
		fl_set_object_label( main_form->run, "Start" );
		fl_set_object_callback( main_form->run, run_file, 0 );

		run_end_of_exp_hooks( );

		vars_del_stack( );             /* some stack variables might be left
										  over when an exception got thrown */
		if ( need_GPIB )
			gpib_shutdown( );
		set_buttons_for_run( 1 );
		stop_graphics( );
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		return FAIL;
	}

	fl_set_object_callback( main_form->run, run_file, 0 );

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

	/* Here the experiment starts - child process for doing it is forked */

	if ( ( child_pid = fork( ) ) == 0 )
		run_child( );

	close_all_files( );              /* only child is going to write to them */

	close( pd[ READ ] );             /* close unused ends of pipes */
	close( pd[ 3 ] );
	pd[ READ ] = pd[ 2 ];

	if ( child_pid != -1 )           /* if fork() succeeded */
	{
		sema_post( semaphore );      /* we're ready to read data */
		need_post = UNSET;
		return OK;
	}

	/* If forking the child failed we end up here */

	sigaction( SIGCHLD,  &sigchld_oact,  NULL );
	sigaction( NEW_DATA, &new_data_oact, NULL );
	sigaction( QUITTING, &quitting_oact, NULL );

	switch ( errno )
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
				eprint( FATAL, SET, "System error \"%s\" when trying to "
						"start experiment.\n", sys_errlist[ errno ] );
			else
				eprint( FATAL, SET, "Unrecognized system error (errno = %d) "
						"when trying to start experiment.\n", errno );
			fl_show_alert( "FATAL Error", "System error on start of "
						   "experiment.", "", 1 );			
	}

	child_pid = 0;
	end_comm( );
	run_end_of_exp_hooks( );
	if ( need_GPIB )
		gpib_shutdown( );
	stop_measurement( NULL, 1 );
	return FAIL;
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

static void stop_while_exp_hook( FL_OBJECT *a, long b )
{
	a = a;
	b = b;

	do_quit = react_to_do_quit = SET;
}


/*------------------------------------------------------------------*/
/* This all to be done when the experiment section does not has any */
/* commands: We just initialize all devices to get and then return. */
/*------------------------------------------------------------------*/

static bool no_prog_to_be_run( void )
{
	bool ret;


	fl_set_cursor( FL_ObjWin( main_form->run ), XC_watch );
	set_buttons_for_run( 0 );

	if ( need_GPIB && gpib_init( GPIB_LOG_FILE, GPIB_LOG_LEVEL ) == FAILURE )
	{
		eprint( FATAL, UNSET, "Can't initialize GPIB bus: %s\n",
				gpib_error_msg );
		set_buttons_for_run( 1 );
		fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );
		return FAIL;
	}

	TRY
	{
		vars_pop( f_dtime( NULL ) );
		run_exp_hooks( );
		ret = OK;
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		run_end_of_exp_hooks( );
		vars_del_stack( );             /* some stack variables might be left
										  over when an exception got thrown */
		ret = FAIL;
	}

	run_end_of_exp_hooks( );
	if ( need_GPIB )
		gpib_shutdown( );

	set_buttons_for_run( 1 );
	fl_set_cursor( FL_ObjWin( main_form->run ), XC_left_ptr );

	return ret;
} 


/*-------------------------------------------------------------------*/
/* Checks if new errors etc. were found while running the exp_hooks. */
/* If so ask the user if she wants to continue - unless an exception */
/* was thrown.                                                       */
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
			THROW( EXCEPTION )
	}
}


/*-------------------------------------------------------------------*/
/* new_data_handler() is the handler for the NEW_DATA signal sent by */
/* the child to the parent if there are new data.                    */
/* The child always stores the data in a shared memory segment. Then */
/* it waits for the parent to become ready to accept the data by     */
/* waiting for a semaphore to become posted. Next it puts the memory */
/* segment key plus the identifier of the type of data in a common   */
/* shared memory segment and finally sends the parent a NEW_DATA     */
/* signal.                                                           */
/* This in turn makes the parent execute this signal handler. The    */
/* parent has a buffer for storing the information about the data it */
/* gets from the child, the message queue. In this handler it stores */
/* the information about the data in the next free slot of the       */
/* message queue and increments the marker pointing to the next free */
/* slot.                                                             */
/* There are two types of data, DATA and REQUESTS: Messages of type  */
/* DATA simply get stored in the message queue and will be dealt     */
/* with whenever there is time (via an idle callback, see comm.c).   */
/* As long as the message queue isn't full yet, for DATA messages    */
/* semaphore gets posted again, telling the child that the parent is */
/* prepared to accept further messages. On the other hand, if the    */
/* messages queue is full a global variable is set, thus indicating  */
/* to the function removing the data from the message queue to post  */
/* the semaphore when it is done with the data.                      */
/* In contrast, messages of type REQUEST come from functions where   */
/* the child sends data not via shared mmory segments but a pipe.    */
/* In this case the child is waiting to write data to a pipe. It     */
/* starts writing only when the semaphore gets posted. On the other  */
/* side, the parent has to deal with all the earlier messages in the */
/* message queue before it starts reading the pipe. So, for this     */
/* kind of messages the semaphore isn't posted yet but only when the */
/* parent is ready for reading on the pipe.                          */
/*-------------------------------------------------------------------*/

static void new_data_handler( int signo )
{
	int errno_saved;


	signo = signo;
	errno_saved = errno;

	Message_Queue[ message_queue_high ].shm_id = Key->shm_id;
	Message_Queue[ message_queue_high ].type = Key->type;
	message_queue_high = ( message_queue_high + 1 ) % QUEUE_SIZE;

	/* If this are data tell child that it can send new data as long as
	   the message queue isn't full, in which case we have to delay posting
	   the semaphore until some old data have been removed - this is done
	   in accept_new_data(). */

	if ( Key->type == DATA )
	{
		if ( ( message_queue_high + 1 ) % QUEUE_SIZE != message_queue_low )
			sema_post( semaphore );
		else
			need_post = SET;
	}

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
	if ( kill( child_pid, DO_QUIT ) < 0 )
	{
		child_pid = 0;                                   /* child is dead... */
		sigaction( SIGCHLD, &sigchld_oact, NULL );
		fl_trigger_object( run_form->sigchld );
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
	{
		fprintf( stderr, "Child process %d died due to signal %d\n",
				 pid, WTERMSIG( return_status ) );
		fflush( stderr );
	}
#endif

	child_pid = 0;                                       /* child is dead... */
	sigaction( SIGCHLD, &sigchld_oact, NULL );

	run_form->sigchld->u_ldata = ( long ) return_status;
	fl_trigger_object( run_form->sigchld );

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

	if ( ! child_is_quitting )   /* missing notification by the child ? */
		fl_show_alert( "FATAL Error", "Experiment stopped prematurely.",
					   NULL, 1 );

	if ( ! a->u_ldata )          /* return status indicates error ? */
		fl_show_alert( "FATAL Error", "Experiment had to be stopped.", "", 1 );

	stop_measurement( NULL, 1 );
}


/*---------------------------------------------------------------------*/
/* stop_measurement() has two functions - if called with the parameter */
/* b set to 0 it serves as the callback for the stop button and just   */
/* sends a DO_QUIT signal to the child telling it to exit. This in     */
/* turn will cause another call of the function, this time with b set  */
/* to 1 by run_sigchld_handler(), catching the SIGCHLD from the child  */
/* in order to get the post-measurement clean-up done.                 */
/*---------------------------------------------------------------------*/

void stop_measurement( FL_OBJECT *a, long b )
{
	int bn;
	

	if ( b == 0 )                        /* callback from stop button ? */
	{
		if ( child_pid != 0 )            /* child is still running */
		{
			/* Only send DO_QUIT signal if the mouse button has been used
			   the user told us he/she would use... */

			if ( stop_button_mask != 0 )
			{
				bn = fl_get_button_numb( a );
				if ( bn != FL_SHORTCUT + 'S' && bn != stop_button_mask )
					return;
			}

			kill( child_pid, DO_QUIT );
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

	sigaction( NEW_DATA, &new_data_oact, NULL );
	sigaction( QUITTING, &quitting_oact, NULL );

	/* Reset all the devices and finally the GPIB bus */

	tools_clear( );
	run_end_of_exp_hooks( );
	if ( need_GPIB )
		gpib_shutdown( );

	fl_freeze_form( run_form->run );
	fl_set_object_label( run_form->stop, "Close" );
	fl_set_button_shortcut( run_form->stop, "C", 1 );
	if ( ! ( cmdline_flags & NO_BALLOON ) )
		fl_set_object_helper( run_form->stop, "Remove this window" );
	fl_unfreeze_form( run_form->run );
}


/*----------------------------------------------------------*/
/* set_buttons_for_run() makes the buttons in the main form */
/* for running another experiment and quitting inaccessible */
/* while a measurement is running and makes them accessible */
/* again when the experiment is finished.                   */
/*----------------------------------------------------------*/

static void set_buttons_for_run( int active )
{
	fl_freeze_form( main_form->fsc2 );

	if ( active == 0 )
	{
		fl_deactivate_object( main_form->Load );
		fl_set_object_lcol( main_form->Load, FL_INACTIVE_COL );

		fl_deactivate_object( main_form->reload );
		fl_set_object_lcol( main_form->reload, FL_INACTIVE_COL );

		fl_deactivate_object( main_form->quit );
		fl_set_object_lcol( main_form->quit, FL_INACTIVE_COL );

		fl_deactivate_object( main_form->bug_report );
		fl_set_object_lcol( main_form->bug_report, FL_INACTIVE_COL );
	}
	else
	{
		fl_activate_object( main_form->Load );
		fl_set_object_lcol( main_form->Load, FL_BLACK );

		fl_activate_object( main_form->reload );
		fl_set_object_lcol( main_form->reload, FL_BLACK );

		fl_activate_object( main_form->run );
		fl_set_object_lcol( main_form->run, FL_BLACK );

		fl_activate_object( main_form->quit );
		fl_set_object_lcol( main_form->quit, FL_BLACK );

		fl_activate_object( main_form->bug_report );
		fl_set_object_lcol( main_form->bug_report, FL_BLACK );

		notify_conn( UNBUSY_SIGNAL );
	}

	fl_unfreeze_form( main_form->fsc2 );
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


	I_am = CHILD;

    /* Set up pipes for communication with parent process */

	close( pd[ WRITE ] );
	close( pd[ 2 ] );
	pd[ WRITE ] = pd[ 3 ];

	/* Set up pointers, global variables and signal handlers */

	child_return_status = OK;
	cur_prg_token = prg_token;
	do_quit = UNSET;
	set_child_signals( );

#ifndef NDEBUG
	/* By stetting the environment variable FSC2_CHILD_DEBUG to something
	   not an empty string the child will sleep here for about 10 hours or
	   until it gets a non-deadly signal, e.g. by the debugger attaching. */

	if ( ( fcd = getenv( "FSC2_CHILD_DEBUG" ) ) != NULL &&
		 fcd[ 0 ] != '\0' )
	{
		fprintf( stderr, "Child process pid = %d\n", getpid( ) );
		fflush( stderr );
		sleep( 36000 );
	}
#endif

	TRY
	{
		do_measurement( );               /* run the experiment */
		child_return_status = OK;
		TRY_SUCCESS;
	}
	OTHERWISE                            /* catch all exceptions */
		child_return_status = FAIL;

	close( pd[ READ ] );                 /* close read end of pipe */
	close( pd[ WRITE ] );                /* close also write end of pipe */

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

void child_sig_handler( int signo )
{
	switch ( signo )
	{
		case DO_QUIT :                /* aka SIGUSR2 */
			do_quit = SET;
			/* really fall through ! */

		case SIGALRM :
			if ( can_jmp_alrm )
			{
				can_jmp_alrm = 0;
				siglongjmp( alrm_env, 1 );
			}
			break;

		/* Ignored signals : */

		case SIGHUP :  case SIGINT :  case SIGUSR1 : case SIGCHLD :
		case SIGCONT : case SIGTTIN : case SIGTTOU : case SIGVTALRM :
			break;
			
		/* All remaining signals are deadly... */

		default :
			if ( ! ( cmdline_flags & NO_MAIL ) )
			{
				DumpStack( );
				death_mail( signo );
			}

			/* Test if parent still exists - if not (i.e. if the parent died
			   without sending a SIGTERM signal) destroy the semaphore and
			   shared memory (as far as the child knows about it) */

			if ( kill( getppid( ), 0 ) == -1 && errno == ESRCH )
			{
				delete_all_shm( );
				sema_destroy( semaphore );
			}

			do_quit = SET;
			_exit( -1 );
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
		_exit( -1 );

	sact.sa_handler = child_confirmation_handler;
	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	if ( sigaction( SIGALRM, &sact, NULL ) < 0 )
		_exit( -1 );

	/* Tell parent that we're done and just wait for its DO_QUIT signal */

    kill( getppid( ), QUITTING );

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
/* is executed by the child it has got to honor the `do_quit' flag.  */
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

			/* Don't react to Stop button after ON_STOP label has been
			   reached */

			if ( cur_prg_token == prg_token + On_Stop_Pos )
			{
				react_to_do_quit = UNSET;
				do_quit = UNSET;
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
						cur->counter = 1;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case UNTIL_TOK :
					cur = cur_prg_token;
					if ( ! test_condition( cur ) )
					{
						cur->counter = 1;
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
						cur->counter = 1;
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
					cur_prg_token->start->counter = 0;
					cur_prg_token = cur_prg_token->start->end;
					break;

				case NEXT_TOK :
					cur_prg_token = cur_prg_token->start;
					break;

				case IF_TOK : case UNLESS_TOK :
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
				THROW( EXCEPTION )
		}
		OTHERWISE
			PASSTHROU( )
	}
}
