/*
 *  Copyright (C) 1999-2016 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <dlfcn.h>
#include <execinfo.h>
#include "fsc2.h"
#include "gpib.h"
#include "serial.h"
#include "exp_parser_common.h"

#if defined WITH_RULBUS
#include <rulbus.h>
#endif

#if defined WITH_MEDRIVER
#include <medriver.h>
#endif

#if defined WITH_LIBUSB_0_1
#include <usb.h>
#elif defined WITH_LIBUSB_1_0
#include <libusb-1.0/libusb.h>
#if defined WITH_LIBHIDAPI_LIBUSB
#include <hidapi/hidapi.h>
#endif
#endif


extern int exp_runparse( void );              /* from exp_run_parser.y */
extern int exp_runparser_init( void );        /* from exp_run_parser.y */


/* Routines of the main process exclusively used in this file */

static bool start_comm_libs( void );
static void error_while_iconified( void );
static bool no_prog_to_run( void );
static bool init_devs_and_graphics( void );
static void stop_while_exp_hook( FL_OBJECT * a,
                                 long        b );
static void setup_signal_handlers( void );
static void fork_failure( int stored_errno );
static void check_for_further_errors( Compilation_T * c_old,
                                      Compilation_T * c_all );
static void quitting_handler( int signo );
static void run_sigchld_handler( int signo );
static void set_buttons_for_run( int run_state );


/* Routines of the child process doing the measurement */

static void run_child( void );
static void setup_child_signals( void );
static void child_sig_handler( int signo );
static void do_measurement( void );
static void wait_for_confirmation( void );
static void child_confirmation_handler( int signo );
static void deal_with_program_tokens( void );


/* Global variables */

sigjmp_buf Alrm_Env;
volatile sig_atomic_t Can_Jmp_Alrm = 0;


/* Locally used global variables used in parent, child and signal handlers */

static struct sigaction Sigchld_old_act,
                        Quitting_old_act;
static volatile sig_atomic_t Child_return_status;
static bool Graphics_have_been_started = false;


/*------------------------------------------------------------------*
 * run() starts an experiment. To do so it initializes all needed
 * devices and opens a new window for displaying the measured data.
 * Then everything needed for the communication between parent and
 * the child to be created is set up, i.e. pipes, a semaphore and a
 * shared memory segment for storing the type of the data and a key
 * for a further shared memory segment that will contain the data.
 * Finally, after initializing some global variables and setting up
 * signal handlers used for synchronization of the processes the
 * child process is started and thus the experiments begins.
 *------------------------------------------------------------------*/

bool
run( void )
{
    /* We can't run more than one experiment - so quit if child_pid != 0 */

    if ( Fsc2_Internals.child_pid != 0 )
        return false;

    /* If there's no EXPERIMENT section at all (indicated by 'prg_length'
       being negative) we're already done */

    if ( EDL.prg_length < 0 )
    {
        if (    Fsc2_Internals.cmdline_flags & DO_CHECK
             || Fsc2_Internals.cmdline_flags & BATCH_MODE )
            fl_trigger_object( GUI.main_form->quit );

        if ( Fsc2_Internals.cmdline_flags & ICONIFIED_RUN )
            error_while_iconified( );

        return false;
    }

    /* Initialize libraries for communication with the devices and do some
       changes to the graphics */

    if ( ! start_comm_libs( ) )
    {
        if ( Fsc2_Internals.cmdline_flags & ICONIFIED_RUN )
            error_while_iconified( );
        return false;
    }

    /* If there are no commands except an EXPERIMENT section label we just
       run all the init hooks, then the exit hooks and are already done. */

    if ( EDL.prg_token == NULL )
    {
        if ( Fsc2_Internals.cmdline_flags & ICONIFIED_RUN )
            error_while_iconified( );
        return no_prog_to_run( );
    }

    /* Initialize devices and graphics */

    if ( ! init_devs_and_graphics( ) )
    {
        if ( Fsc2_Internals.cmdline_flags & ICONIFIED_RUN )
            error_while_iconified( );
        return false;
    }

    /* Setup the signal handlers for signals used for communication between
       parent and child process and an idle callback function for displaying
       new data. */

    setup_signal_handlers( );

    if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );

    /* We have to be careful: when the child process gets forked it may
       already be finished running *before* the fork() call returns in the
       parent. In this case the signal handlers of the parent don't know the
       PID of the child process and thus won't work correctly. Therefore all
       the signals the child may send are blocked until we can be sure the
       parent has got the child's PID. */

    sigset_t new_mask,
             old_mask;

    sigemptyset( &new_mask );
    sigaddset( &new_mask, SIGCHLD );
    sigaddset( &new_mask, QUITTING );
    sigprocmask( SIG_BLOCK, &new_mask, &old_mask );

    /* Here the experiment starts - the child process is forked */

    if ( ( Fsc2_Internals.child_pid = fork( ) ) == 0 )
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        if ( GUI.is_init )
            close( ConnectionNumber( GUI.d ) );
        run_child( );
    }

    int stored_errno = errno;        // stored for later examination...
    close_all_files( );              // only child is going to write to them

    close( Comm.pd[ READ ] );        /* close unused ends of pipes */
    close( Comm.pd[ 3 ] );
    Comm.pd[ READ ] = Comm.pd[ 2 ];

    if ( Fsc2_Internals.child_pid > 0 )   /* fork() was succeeded */
    {
        sigprocmask( SIG_SETMASK, &old_mask, NULL );
        Fsc2_Internals.mode = PREPARATION;
        return true;
    }

    /* If forking the child failed we end up here */

    sigprocmask( SIG_SETMASK, &old_mask, NULL );
    fork_failure( stored_errno );
    return false;
}


/*------------------------------------------------------------*
 * Called when the program was started with the main window
 * being iconified and an error happened. In this case the
 * main window must be de-iconified to make the error message
 * visible.
 *------------------------------------------------------------*/

static void
error_while_iconified( void )
{
    Fsc2_Internals.cmdline_flags &= ~ ICONIFIED_RUN;
    fl_raise_form( GUI.main_form->fsc2 );
    XMapWindow( fl_get_display( ), GUI.main_form->fsc2->window );
}


/*--------------------------------------------------------------------*
 * This function first does some smaller changes to the GUI and then
 * initializes the libraries that deal with GPIB, RULBUS, USB, serial
 * ports, LAN and Meilhaus cards.
 *--------------------------------------------------------------------*/

static bool
start_comm_libs( void )
{
#if defined WITH_RULBUS || defined WITH_MEDRIVER
    int retval;
#endif

    /* Disable some buttons and show a watch cursor */

    set_buttons_for_run( 1 );
    Fsc2_Internals.state = STATE_RUNNING;

    /* If there are devices that need the GPIB bus initialize it now */

    if ( Need_GPIB && gpib_init( ) == FAILURE )
    {
        eprint( FATAL, false, "Failed to initialize GPIB bus: %s\n",
                gpib_last_error( ) );
        goto gpib_fail;
    }

#if defined WITH_RULBUS
    /* If there are devices that are controlled via the RULBUS initialize it */

    if ( Need_RULBUS )
    {
        if ( fsc2_obtain_uucp_lock( "rulbus" ) == false )
        {
            eprint( FATAL, false, "RULBUS system is already locked by another "
                    "process.\n" );
            goto rulbus_fail;
        }

        raise_permissions( );

        if ( ( retval = rulbus_open( O_EXCL ) ) < 0 )
        {
            lower_permissions( );
            fsc2_release_uucp_lock( "rulbus" );
            eprint( FATAL, false, "Failed to initialize RULBUS: %s.\n",
                    rulbus_strerror( ) );
            goto rulbus_fail;
        }

        lower_permissions( );
    }
#endif

#if defined WITH_LIBUSB_0_1 || defined WITH_LIBUSB_1_0
    /* If there are devices that are controlled via USB initialize library */

    if ( Need_USB )
    {
        if ( fsc2_obtain_uucp_lock( "libusb" ) == false )
        {
            eprint( FATAL, false, "USB system is already locked by another "
                    "process.\n" );
            goto libusb_fail;
        }

        raise_permissions( );

#if defined WITH_LIBUSB_0_1
        usb_init( );
        usb_find_busses( );
        usb_find_devices( );
#else
        if ( libusb_init( NULL ) != 0 )
        {
            lower_permissions( );
            fsc2_release_uucp_lock( "libusb" );
            eprint( FATAL, false, "Failed to initialize USB.\n" );
            goto libusb_fail;
        }

//#if defined WITH_LIBHIDAPI_LIBUSB
//        if (hid_init( ))
//        {
//            lower_permissions( );
//            fsc2_release_uucp_lock( "libusb" );
//            eprint( FATAL, false, "Failed to initialize HIDAPI.\n" );
//            goto libusb_fail;
//        }
//#endif

#endif

        lower_permissions( );
    }


#endif

#if ! defined WITHOUT_SERIAL_PORTS
    if ( ! fsc2_serial_exp_init( SERIAL_LOG_LEVEL ) )
        goto serial_fail;
#endif

#if defined WITH_MEDRIVER
    /* Initialize Meilhaus library if there are devices that are controlled
       via it - no locking is needed, this is done during the initialization
       of the required subdevice */

    if ( Need_MEDRIVER )
    {
        raise_permissions( );

        if ( ( retval = meOpen( ME_OPEN_NO_FLAGS ) ) != ME_ERRNO_SUCCESS )
        {
            char msg[ ME_ERROR_MSG_MAX_COUNT ];

            meErrorGetMessage( retval, msg, sizeof msg );
            lower_permissions( );
            eprint( FATAL, false, "Failed to initialize Meilhaus driver: "
                    "%s\n", msg );
            goto medriver_fail;
        }

        lower_permissions( );
    }
#endif

    return true;

    /* The following is for de-intialization of all initialzed subsystens
       in case of failures */

#if defined WITH_MEDRIVER
 medriver_fail:
#endif

    if ( Need_LAN )
        fsc2_lan_cleanup( );

#if ! defined WITHOUT_SERIAL_PORTS
    fsc2_serial_cleanup( );

 serial_fail:
#endif

#if defined WITH_LIBUSB_0_1 || defined WITH_LIBUSB_1_0
    if ( Need_USB )
    {
#if defined WITH_LIBUSB_1_0
        
//#if defined WITH_LIBHIDAPI_LIBUSB
//        hid_exit( );
//#endif

        libusb_exit( NULL );
#endif
        fsc2_release_uucp_lock( "libusb" );
    }
 libusb_fail:
#endif

#if defined WITH_RULBUS
    if ( Need_RULBUS )
    {
        rulbus_close( );
        fsc2_release_uucp_lock( "rulbus" );
    }

 rulbus_fail:
#endif

    if ( Need_GPIB )
        gpib_shutdown( );

 gpib_fail:
    if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
    {
        set_buttons_for_run( 0 );
        fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );
        XFlush( fl_get_display( ) );
    }

    Fsc2_Internals.state = STATE_IDLE;
    return false;
}


/*----------------------------------------------------------------------*
 * This is all to be done when the experiment section does not contain
 * any commands: devices get initialized and then de-initialized again.
 *----------------------------------------------------------------------*/

static bool
no_prog_to_run( void )
{
    bool ret;

    TRY
    {
        EDL.do_quit = false;
        EDL.react_to_do_quit = true;

        if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        {
            fl_set_object_callback( GUI.main_form->run,
                                    stop_while_exp_hook, 0 );
            fl_set_object_label( GUI.main_form->run, "Stop" );
            fl_set_object_shortcut( GUI.main_form->run, "S", 1 );
            XFlush( fl_get_display( ) );
        }

        Fsc2_Internals.mode = EXPERIMENT;

        experiment_time( );
        EDL.experiment_time = 0.0;
        vars_pop( f_dtime( NULL ) );

        run_exp_hooks( );
        ret = true;
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        vars_del_stack( );
        ret = false;
    }

    EDL.do_quit = EDL.react_to_do_quit = false;

    run_end_of_exp_hooks( );

#if defined WITH_MEDRIVER
    if ( Need_MEDRIVER )
    {
        int retval = meClose( ME_CLOSE_NO_FLAGS );

        if ( retval != ME_ERRNO_SUCCESS )
        {
            char msg[ ME_ERROR_MSG_MAX_COUNT ];

            meErrorGetMessage( retval, msg, sizeof msg );
            eprint( WARN, false, "Failed to close Meilhaus driver: %s\n", msg );
        }
    }
#endif

    if ( Need_LAN )
        fsc2_lan_cleanup( );

#if ! defined WITHOUT_SERIAL_PORTS
    fsc2_serial_cleanup( );
#endif

#if defined WITH_LIBUSB_1_0
    if ( Need_USB )
    {
//#if defined WITH_LIBHIDAPI_LIBUSB
//        hid_exit( );
//#endif

        libusb_exit( NULL );
        fsc2_release_uucp_lock( "libusb" );
    }
#endif

#if defined WITH_RULBUS
    if ( Need_RULBUS )
    {
        rulbus_close( );
        fsc2_release_uucp_lock( "rulbus" );
    }
#endif

    if ( Need_GPIB )
        gpib_shutdown( );

    Fsc2_Internals.mode = PREPARATION;

    if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
    {
        fl_set_object_label( GUI.main_form->run, "Start" );
        fl_set_object_shortcut( GUI.main_form->run, "S", 1 );
        fl_set_object_callback( GUI.main_form->run, run_file, 0 );
        set_buttons_for_run( 0 );
        fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );
        XFlush( fl_get_display( ) );
    }

    Fsc2_Internals.state = STATE_IDLE;

    return ret;
}


/*--------------------------------------------------------------------------*
 * Function sets a zero point for the dtime() function, runs the experiment
 * hooks (while still allowing the user to break in between), initializes
 * the graphics and creates two pipes for two-way communication between the
 * parent and child process.
 *--------------------------------------------------------------------------*/

static bool
init_devs_and_graphics( void )
{
    /* Make a copy of the errors found while compiling the program */

    Compilation_T compile_test = EDL.compilation;
    Graphics_have_been_started = false;

    TRY
    {
        EDL.do_quit = false;
        EDL.react_to_do_quit = true;

        if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        {
            fl_set_object_callback( GUI.main_form->run,
                                    stop_while_exp_hook, 0 );
            fl_set_object_label( GUI.main_form->run, "Stop" );
            fl_set_object_shortcut( GUI.main_form->run, "S", 1 );
            XFlush( fl_get_display( ) );
        }

        Fsc2_Internals.mode = EXPERIMENT;

        experiment_time( );
        EDL.experiment_time = 0.0;
        vars_pop( f_dtime( NULL ) );

        run_exp_hooks( );

#if defined WITH_LIBUSB_1_0
        /* Set the close-on-exec flag on all file descriptors opened for
           USB devices */

        if ( Need_USB )
        {
            const struct libusb_pollfd **usb_list = libusb_get_pollfds( NULL );

            if ( usb_list != NULL )
            {
                size_t i;

                for ( i = 0; usb_list[ i ] != NULL; i++ )
                {
                    int fd_flags = fcntl( usb_list[ i ]->fd, F_GETFD );

                    if ( fd_flags < 0 )
                        fd_flags = 0;
                    fcntl( usb_list[ i ]->fd, F_SETFD, fd_flags | FD_CLOEXEC );
                }

                free( usb_list );
            }
        }
#endif

        EDL.react_to_do_quit = false;

        if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        {
            fl_deactivate_object( GUI.main_form->run );
            fl_set_object_label( GUI.main_form->run, "Start" );
            fl_set_object_shortcut( GUI.main_form->run, "S", 1 );
            fl_set_object_lcol( GUI.main_form->run, FL_INACTIVE_COL );
            fl_set_object_callback( GUI.main_form->run, run_file, 0 );
            XFlush( fl_get_display( ) );
        }

        check_for_further_errors( &compile_test, &EDL.compilation );

        if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        {
            start_graphics( );
            Graphics_have_been_started = true;

            if ( G.dim & 1 || ! G.is_init )
                fl_set_object_callback( GUI.run_form_1d->stop_1d,
                                        run_stop_button_callback, 0 );
            if ( G.dim & 2 )
                fl_set_object_callback( GUI.run_form_2d->stop_2d,
                                        run_stop_button_callback, 0 );
        }

        setup_comm( );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        EDL.do_quit = EDL.react_to_do_quit = false;

        end_comm( );

        run_end_of_exp_hooks( );
        vars_del_stack( );

#if defined WITH_MEDRIVER
        if ( Need_MEDRIVER )
        {
            int retval = meClose( ME_CLOSE_NO_FLAGS );

            if ( retval != ME_ERRNO_SUCCESS )
            {
                char msg[ ME_ERROR_MSG_MAX_COUNT ];

                meErrorGetMessage( retval, msg, sizeof msg );
                eprint( WARN, false, "Failed to close Meilhaus driver: %s\n",
                        msg );
            }
        }
#endif

#if ! defined WITHOUT_SERIAL_PORTS
        fsc2_serial_cleanup( );
#endif

        if ( Need_LAN )
            fsc2_lan_cleanup( );

#if defined WITH_LIBUSB_1_0
        if ( Need_USB )
        {
            libusb_exit( NULL );
            fsc2_release_uucp_lock( "libusb" );
        }
#endif

#if defined WITH_RULBUS
        if ( Need_RULBUS )
        {
            rulbus_close( );
            fsc2_release_uucp_lock( "rulbus" );
        }
#endif

        if ( Need_GPIB )
            gpib_shutdown( );

        Fsc2_Internals.mode = PREPARATION;

        if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        {
            run_close_button_callback( NULL, 0 );

            fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_left_ptr );
            fl_set_object_label( GUI.main_form->run, "Start" );
            fl_set_object_shortcut( GUI.main_form->run, "S", 1 );
            fl_set_object_callback( GUI.main_form->run, run_file, 0 );
            fl_set_object_lcol( GUI.main_form->run, FL_BLACK );
            fl_activate_object( GUI.main_form->run );
            XFlush( fl_get_display( ) );
        }

        return false;
    }

    if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        fl_set_object_callback( GUI.main_form->run, run_file, 0 );

    Fsc2_Internals.child_is_quitting = QUITTING_UNSET;

    return true;
}


/*-----------------------------------------------------------------*
 * Sets the signal handlers for all signals used for communication
 * between the parent and the child process. Finally, it activates
 * an idle callback routine which the parent uses for accepting
 * displaying of new data.
 *-----------------------------------------------------------------*/

static void
setup_signal_handlers( void )
{
    struct sigaction sact;

    sact.sa_handler = quitting_handler;
    sigemptyset( &sact.sa_mask );
    sact.sa_flags = 0;
    sigaction( QUITTING, &sact, &Quitting_old_act );

    sact.sa_handler = run_sigchld_handler;
    sigemptyset( &sact.sa_mask );
    sact.sa_flags = 0;
    sigaction( SIGCHLD, &sact, &Sigchld_old_act );
}


/*-----------------------------------------------------------*
 * Callback handler for the main "Stop" button during device
 * initialization (to allow stopping the experiment already
 * at this early stage).
 *-----------------------------------------------------------*/

static void
stop_while_exp_hook( FL_OBJECT * a  UNUSED_ARG,
                     long        b  UNUSED_ARG )
{
    EDL.do_quit = EDL.react_to_do_quit = true;
}


/*------------------------------------------------------------------------*
 * Things that remain to be done when forking the child process failed...
 *------------------------------------------------------------------------*/

static void
fork_failure( int stored_errno )
{
    sigaction( SIGCHLD,  &Sigchld_old_act,  NULL );
    sigaction( QUITTING, &Quitting_old_act, NULL );

    if ( Fsc2_Internals.cmdline_flags & ICONIFIED_RUN )
        error_while_iconified( );

    switch ( stored_errno )
    {
        case EAGAIN :
            eprint( FATAL, false, "Not enough system resources left to run "
                    "the experiment.\n" );
            if ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
                exit( EXIT_FAILURE );
            fl_show_alert( "FATAL Error", "Not enough system resources",
                           "left to run the experiment.", 1 );
            break;

        case ENOMEM :
            eprint( FATAL, false, "Not enough memory left to run the "
                    "experiment.\n" );
            if ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
                exit( EXIT_FAILURE );
            fl_show_alert( "FATAL Error", "Not enough memory left",
                           "to run the experiment.", 1 );
            break;

        default :
            eprint( FATAL, false, "System error \"%s\" when trying to "
                    "start experiment.\n", strerror( stored_errno ) );
            if ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
                exit( EXIT_FAILURE );
            fl_show_alert( "FATAL Error", "System error on start of "
                           "experiment.", NULL, 1 );
            break;
    }

    Fsc2_Internals.child_pid = 0;
    EDL.do_quit = EDL.react_to_do_quit = false;

    end_comm( );

    run_end_of_exp_hooks( );

#if defined WITH_MEDRIVER
    if ( Need_MEDRIVER )
    {
        int retval = meClose( ME_CLOSE_NO_FLAGS );

        if ( retval != ME_ERRNO_SUCCESS )
        {
            char msg[ ME_ERROR_MSG_MAX_COUNT ];

            meErrorGetMessage( retval, msg, sizeof msg );
            eprint( WARN, false, "Failed to close Meilhaus driver: %s\n", msg );
        }
    }
#endif

    if ( Need_LAN )
        fsc2_lan_cleanup( );

#if ! defined WITHOUT_SERIAL_PORTS
    fsc2_serial_cleanup( );
#endif

#if defined WITH_LIBUSB_1_0
    if ( Need_USB )
    {
        libusb_exit( NULL );
        fsc2_release_uucp_lock( "libusb" );
    }
#endif

#if defined WITH_RULBUS
    if ( Need_RULBUS )
    {
        rulbus_close( );
        fsc2_release_uucp_lock( "rulbus" );
    }
#endif

    if ( Need_GPIB )
        gpib_shutdown( );

    Fsc2_Internals.mode = PREPARATION;

    if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
    {
        run_close_button_callback( NULL, 0 );
        fl_set_object_lcol( GUI.main_form->run, FL_BLACK );
        fl_activate_object( GUI.main_form->run );
    }
}


/*-------------------------------------------------------------------*
 * Checks if new errors etc. were found while running the exp_hooks.
 * In this case ask the user if she wants to continue - unless an
 * exception was thrown.
 *-------------------------------------------------------------------*/

static void
check_for_further_errors( Compilation_T * c_old,
                          Compilation_T * c_all )
{
    char str1[ 128 ],
         str2[ 128 ];
    const char *mess = "During start of the experiment there where";

    Compilation_T diff;
    for ( int i = FATAL; i < NO_ERROR; i++ )
        diff.error[ i ] = c_all->error[ i ] - c_old->error[ i ];

    if (    Fsc2_Internals.cmdline_flags & NO_GUI_RUN
         && ( diff.error[ SEVERE ] != 0 || diff.error[ WARN ] != 0 ) )
        THROW( EXCEPTION );

    if (    ! ( Fsc2_Internals.cmdline_flags & BATCH_MODE )
         && ( diff.error[ SEVERE ] != 0 || diff.error[ WARN ] != 0 ) )
    {
        if ( diff.error[ SEVERE ] != 0 )
        {
            if ( diff.error[ WARN ] != 0 )
            {
                sprintf( str1, "%s %d severe", mess, diff.error[ SEVERE ] );
                sprintf( str2, "and normal %d warnings.", diff.error[ WARN ] );
            }
            else
            {
                sprintf( str1, "%s %d severe warnings.",
                         mess, diff.error[ SEVERE ] );
                str2[ 0 ] = '\0';
            }
        }
        else
        {
            sprintf( str1, "%s %d warnings.", mess, diff.error[ WARN ] );
            str2[ 0 ] = '\0';
        }

        if ( 1 == fl_show_choice( str1, str2, "Continue running the program?",
                                  2, "No", "Yes", "", 1 ) )
            THROW( EXCEPTION );
    }
}


/*---------------------------------------------------------------------*
 * This is the parents handler for the QUITTING signal sent by the child
 * when it exits normally: the handler sets a global variable and reacts
 * by sending the child a DO_QUIT signal.
 *---------------------------------------------------------------------*/

static void
quitting_handler( int signo )
{
    if ( signo != QUITTING )     /* should never happen... */
        return;

    Fsc2_Internals.child_is_quitting = QUITTING_RAISED;
}


/*-------------------------------------------------------------------*
 * Callback function for the 'Stop' button - used to kill the child
 * process. After the child is dead a different callback function is
 * used for this button, see run_close_button_callback().
 *-------------------------------------------------------------------*/

void
run_stop_button_callback( FL_OBJECT * obj,
                          long        b  UNUSED_ARG )
{
    /* Using the "Stop" button when the child is already dead (or has been
       asked to quit) shouldn't be possible, but to make real sure (in case
       of some real subtle timing problems) we better check */

    if (    Fsc2_Internals.child_pid == 0
         || kill( Fsc2_Internals.child_pid, 0 ) == -1 )
    {
        fl_set_object_callback( obj, NULL, 0 );
        return;
    }

    /* Only send DO_QUIT signal if the correct mouse button has been used */

    if ( GUI.stop_button_mask != 0 )
    {
        int bn = fl_get_button_numb( obj );
        if ( bn != FL_SHORTCUT + 'S' && bn != GUI.stop_button_mask )
            return;
    }

    kill( Fsc2_Internals.child_pid, DO_QUIT );

    /* Disable the stop button(s) until the child has died */

    if ( G.dim & 1 || ! G.is_init )
    {
        fl_set_object_callback( GUI.run_form_1d->stop_1d, NULL, 0 );
        fl_deactivate_object( GUI.run_form_1d->stop_1d );
        fl_set_object_lcol( GUI.run_form_1d->stop_1d, FL_INACTIVE_COL );
    }

    if ( G.dim & 2 )
    {
        fl_set_object_callback( GUI.run_form_2d->stop_2d, NULL, 0 );
        fl_deactivate_object( GUI.run_form_2d->stop_2d );
        fl_set_object_lcol( GUI.run_form_2d->stop_2d, FL_INACTIVE_COL );
    }

    /* If we're currently waiting for an event from the toolbox or for the
       timer to expire the child won't react to the DO_QUIT signal because
       it's still trying to read a reply from the parent. So we've got to
       give it this reply to let it die. */

    if ( Fsc2_Internals.tb_wait != TB_WAIT_NOT_RUNNING )
        tb_wait_handler( -1 );
}


/*-------------------------------------------------------------------*
 * run_sigchld_handler() is the handler for SIGCHLD signals received
 * while a measurement is running. Only the most basic things (i.e
 * waiting for the return status and resetting of signal handlers)
 * are done here since it's a signal handler. To get the remaining
 * stuff done an invisible button is triggered which leads to a call
 * of its callback function, run_sigchld_callback().
 *-------------------------------------------------------------------*/

static void
run_sigchld_handler( int signo )
{
    if ( signo != SIGCHLD )          /* should never happen... */
        return;

    int errno_saved = errno;

    pid_t pid;
    int return_status;

    while ( ( pid = waitpid( -1, &return_status, WNOHANG ) ) > 0 )
    {
        if ( pid == Fsc2_Internals.http_pid )
        {
            Fsc2_Internals.http_pid = -1;
            Fsc2_Internals.http_server_died = true;
            continue;
        }

        if ( pid != Fsc2_Internals.child_pid )
            continue;

#if ! defined NDEBUG
        if ( WIFSIGNALED( return_status ) )
            fprintf( stderr, "Child process %d died due to signal %d\n",
                     pid, WTERMSIG( return_status ) );
#endif

        Fsc2_Internals.child_pid = 0;                    /* child is dead... */
        sigaction( SIGCHLD, &Sigchld_old_act, NULL );

        /* Disable use of the 'Stop' button */

        EDL.do_quit = EDL.react_to_do_quit = false;

        if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        {
            GUI.main_form->sigchld->u_ldata =
                                         ( long ) ! WIFEXITED( return_status );
            fl_trigger_object( GUI.main_form->sigchld );
        }
        else
        {
            Fsc2_Internals.check_return = ! WIFEXITED( return_status );
        }
    }

    errno = errno_saved;
}


/*----------------------------------------------------------------*
 * run_sigchld_callback() is the callback for an invisible button
 * that is triggered on the death of the child. If the child died
 * prematurely, i.e. without notifying the parent by a QUITTING
 * signal or it signals an error via its return status an error
 * message is output. Then the post-measurement clean-up is done,
 * i.e. the now defunct communication channels to the child are
 * closed, the devices are reset and the graphic is set up to
 * reflect the new state. The display window isn't yet closed,
 * this is only done when the 'Stop' button, now labeled 'Close',
 * gets pressed.
 *----------------------------------------------------------------*/

void
run_sigchld_callback( FL_OBJECT * a,
                      long        b )
{
    const char * mess;
    int state = EXIT_SUCCESS;

    if ( Fsc2_Internals.child_is_quitting == QUITTING_UNSET )
                                    /* missing notification by the child ? */
    {
        if (    ! ( Fsc2_Internals.cmdline_flags & DO_CHECK )
             && ! ( Fsc2_Internals.cmdline_flags & BATCH_MODE )
             && ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        {
            Fsc2_Internals.state = STATE_WAITING;
            fl_show_alert( "Fatal Error", "Experiment stopped unexpectedly.",
                           NULL, 1 );
        }

        if ( Fsc2_Internals.cmdline_flags & ( BATCH_MODE | NO_GUI_RUN ) )
            fprintf( stderr, "Fatal Error: Experiment stopped unexpectedly: "
                     "'%s'.\n", EDL.files->name );

        mess = "Experiment stopped unexpectedly after ";
        state = EXIT_FAILURE;
    }
    else if (    ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN && b )
              || (    ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
                   && a->u_ldata ) )
    {
        if (    ! ( Fsc2_Internals.cmdline_flags & DO_CHECK )
             && ! ( Fsc2_Internals.cmdline_flags & BATCH_MODE )
             && ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        {
            Fsc2_Internals.state = STATE_WAITING;
            fl_show_alert( "Fatal Error", "Experiment had to be stopped.",
                           NULL, 1 );
        }

        if ( Fsc2_Internals.cmdline_flags & ( BATCH_MODE | NO_GUI_RUN ) )
            fprintf( stderr, "Fatal Error: Experiment had to be stopped: "
                     "'%s'.\n", EDL.files->name );

        mess = "Experiment had to be stopped after ";
        state = EXIT_FAILURE;
    }
    else                              /* normal death of child */
    {
        mess = "Experiment finished after ";
    }

    /* Remove the tool box */

    tools_clear( );

    /* Get and display all remaining data the child sent before it died,
       then close the communication channels */

    end_comm( );

    /* Reset the handler for the QUITTING signal */

    sigaction( QUITTING, &Quitting_old_act, NULL );

    Fsc2_Internals.state = STATE_FINISHED;

    /* Reset all the devices and finally the GPIB bus, RULBUS, LAN and serial
       port(s) */

    run_end_of_exp_hooks( );

#if defined WITH_MEDRIVER
    if ( Need_MEDRIVER )
    {
        int retval = meClose( ME_CLOSE_NO_FLAGS );

        if ( retval != ME_ERRNO_SUCCESS )
        {
            char msg[ ME_ERROR_MSG_MAX_COUNT ];

            meErrorGetMessage( retval, msg, sizeof msg );
            eprint( WARN, false, "Failed to close Meilhaus driver: %s\n", msg );
        }
    }
#endif

    if ( Need_LAN )
        fsc2_lan_cleanup( );

#if ! defined WITHOUT_SERIAL_PORTS
    fsc2_serial_cleanup( );
#endif

#if defined WITH_LIBUSB_1_0
    if ( Need_USB )
    {
        libusb_exit( NULL );
        fsc2_release_uucp_lock( "libusb" );
    }
#endif

#if defined WITH_RULBUS
    if ( Need_RULBUS )
    {
        rulbus_close( );
        fsc2_release_uucp_lock( "rulbus" );
    }
#endif

    if ( Need_GPIB )
        gpib_shutdown( );

    if ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
        return;

    /* Print out for how long the experiment had been running */

    int secs  = irnd( experiment_time( ) );
    int hours = secs / 3600;
    int mins  = ( secs / 60 ) % 60;
    secs %= 60;

    if ( hours > 0 )
    {
        eprint( NO_ERROR, false, "%s %d h, %d m and %d s.\n",
                mess, hours, mins, secs );
    }
    else
    {
        if ( mins > 0 )
            eprint( NO_ERROR, false, "%s %d m and %d s.\n", mess, mins, secs );
        else
            eprint( NO_ERROR, false, "%s %d s.\n", mess, secs );
    }

    /* Re-enable the stop button (which was disabled when the DO_QUIT
       signal was send to the child) by associating it with a new
       handler that's responsible for closing the window. Also change the
       buttons label to 'Close'. */

    if ( G.dim & 1 || ! G.is_init )
    {
        fl_freeze_form( GUI.run_form_1d->run_1d );
        fl_set_object_label( GUI.run_form_1d->stop_1d,
                             ( G.dim == 1 || ! G.is_init ) ?
                             "Close" : "Close all" );
        fl_set_button_shortcut( GUI.run_form_1d->stop_1d, "C", 1 );
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.run_form_1d->stop_1d,
                                  "Remove all display windows" );
        fl_set_object_callback( GUI.run_form_1d->stop_1d,
                                run_close_button_callback, 0 );
        fl_activate_object( GUI.run_form_1d->stop_1d );
        fl_set_object_lcol( GUI.run_form_1d->stop_1d, FL_BLACK );
        fl_unfreeze_form( GUI.run_form_1d->run_1d );
    }

    if ( G.dim & 2 )
    {
        fl_freeze_form( GUI.run_form_2d->run_2d );
        fl_set_object_label( GUI.run_form_2d->stop_2d,
                             G.dim == 2 ? "Close" : "Close all" );
        fl_set_button_shortcut( GUI.run_form_2d->stop_2d, "C", 1 );
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.run_form_2d->stop_2d,
                                  "Remove all display windows" );
        fl_set_object_callback( GUI.run_form_2d->stop_2d,
                                run_close_button_callback, 0 );
        fl_activate_object( GUI.run_form_2d->stop_2d );
        fl_set_object_lcol( GUI.run_form_2d->stop_2d, FL_BLACK );
        fl_unfreeze_form( GUI.run_form_2d->run_2d );
    }

    if (    Fsc2_Internals.cmdline_flags & DO_CHECK
         || Fsc2_Internals.cmdline_flags & BATCH_MODE )
    {
        run_close_button_callback( NULL, 0 );
        Fsc2_Internals.check_return = state;
        fl_trigger_object( GUI.main_form->quit );
    }
}


/*-------------------------------------------------------*
 * This function gets called whenever the 'Close' button
 * in (one of) the display window(s) gets pressed.
 *-------------------------------------------------------*/

void
run_close_button_callback( FL_OBJECT * a  UNUSED_ARG,
                           long        b  UNUSED_ARG )
{
    if ( Graphics_have_been_started )
    {
        stop_graphics( );
        Graphics_have_been_started = false;
    }

    set_buttons_for_run( 0 );
    Fsc2_Internals.state = STATE_IDLE;

    if ( Fsc2_Internals.cmdline_flags & ICONIFIED_RUN )
        switch( is_iconic( fl_get_display( ), GUI.main_form->fsc2->window ) )
        {
            case 1 :
                fl_trigger_object( GUI.main_form->quit );
                break;

            case 0 :
                Fsc2_Internals.cmdline_flags &= ~ ICONIFIED_RUN;
                break;

            case -1 :
                error_while_iconified( );
                break;
        }
}


/*----------------------------------------------------------*
 * set_buttons_for_run() makes the buttons in the main form
 * for running another experiment and quitting inaccessible
 * while a measurement is running and makes them accessible
 * again when the experiment is finished.
 *----------------------------------------------------------*/

static void
set_buttons_for_run( int run_state )
{
    if ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
        return;

    fl_freeze_form( GUI.main_form->fsc2 );

    if ( run_state == 1 )
    {
        fl_set_cursor( FL_ObjWin( GUI.main_form->run ), XC_watch );

        fl_deactivate_object( GUI.main_form->Load );
        fl_set_object_lcol( GUI.main_form->Load, FL_INACTIVE_COL );

        fl_deactivate_object( GUI.main_form->reload );
        fl_set_object_lcol( GUI.main_form->reload, FL_INACTIVE_COL );

        fl_deactivate_object( GUI.main_form->quit );
        fl_set_object_lcol( GUI.main_form->quit, FL_INACTIVE_COL );

        fl_deactivate_object( GUI.main_form->bug_report );
        fl_set_object_lcol( GUI.main_form->bug_report, FL_INACTIVE_COL );

        XFlush( fl_get_display( ) );
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
    }

    fl_unfreeze_form( GUI.main_form->fsc2 );
}


/**************************************************************/
/*                                                            */
/*         Here's the code for the child process...           */
/*                                                            */
/**************************************************************/


/*-------------------------------------------------------------------*
 * run_child() is the child process' code for doing the measurement.
 * It does some initialization (e.g. setting up signal handlers) and
 * then runs the experiment by calling do_measurement(). The
 * experiment stops either when all lines of the program have been
 * dealt with, an unrecoverable error happened or the user told it
 * to stop (by pressing the "Stop" button). When returning from the
 * do_measurement() routine it signals the parent process that it's
 * going to die and waits for the arrival of an acknowledging
 * signal. When the child gets an signal it can't deal with it does
 * not return from do_measurement() but dies in the signal handler.
 *-------------------------------------------------------------------*/

static void
run_child( void )
{
    Fsc2_Internals.I_am = CHILD;

    /* Set up pipes for communication with parent process */

    close( Comm.pd[ WRITE ] );
    close( Comm.pd[ 2 ] );
    Comm.pd[ WRITE ] = Comm.pd[ 3 ];

    /* Set up pointers, global variables and signal handlers */

    Child_return_status = true;
    EDL.cur_prg_token = EDL.prg_token;
    EDL.do_quit = false;
    setup_child_signals( );

#ifndef NDEBUG
    /* Setting the environment variable FSC2_CHILD_DEBUG to a non-empty
       string will induce the child to stop, making it possible to attach
       with a debugger at this point. */

    const char * fcd = getenv( "FSC2_CHILD_DEBUG" );
    if ( fcd && *fcd != '\0' )
    {
        fprintf( stderr, "Child process pid = %d\n", getpid( ) );
        raise( SIGSTOP );
    }
#endif

    /* Initialization is done and the child can start doing its real work */

    TRY
    {
        do_measurement( );               /* run the experiment */
        Child_return_status = true;
        TRY_SUCCESS;
    }
    OTHERWISE                            /* catch all exceptions */
        Child_return_status = false;

    run_child_exit_hooks( );

    close_all_files( );
    close( Comm.pd[ READ ] );            /* close read end of pipe */
    close( Comm.pd[ WRITE ] );           /* close also write end of pipe */

    wait_for_confirmation( );            /* wait for license to die... */
}


/*-------------------------------------------------------------------*
 * Set up the signal handlers for all kinds of signals the child may
 * receive. This probably looks a bit like overkill, but I just want
 * to make sure the child doesn't get killed by meaningless signals
 * and, on the other hand, that on deadly signals it gets a chance
 * to try to get rid of shared memory that the parent didn't destroy
 * (in case it was killed by a signal it couldn't catch).
 *-------------------------------------------------------------------*/

static void
setup_child_signals( void )
{
    struct sigaction sact;
    int sig_list[ ] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE,
                        SIGSEGV, SIGPIPE, SIGTERM, SIGUSR1, SIGCHLD, SIGCONT,
                        SIGTTIN, SIGTTOU, SIGBUS, SIGVTALRM };

    for ( size_t i = 0; i < NUM_ELEMS( sig_list ); i++ )
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


/*----------------------------------------------------------------*
 * This is the signal handler for the signals the child receives.
 * There are two signals that have a real meaning, SIGUSR2 (aka
 * DO_QUIT) and SIGALRM. The others are either ignored or kill the
 * child (but in a hopefully controlled way to allow to delete
 * shared memory if the parent didn't do it itself).
 * There's a twist: the SIGALRM signal can only come from the
 * f_wait() function (see func_util.c). Here we wait in a pause()
 * for SIGALRM to get a reliable timer. On the other hand, the
 * pause() also has to be interruptible by the DO_QUIT signal, so
 * by falling through from the switch for this signal it is
 * guaranteed that also this signal will end the pause() - it
 * works even when the handler, while handling a SIGALRM signal,
 * is interrupted by a DO_QUIT signal. In all other cases (i.e.
 * when we're not waiting in the pause() in f_wait()) nothing
 * unexpected happens.
 *----------------------------------------------------------------*/

static void
child_sig_handler( int signo )
{
    switch ( signo )
    {
        case DO_QUIT :                          /* aka SIGUSR2 */
            if ( ! EDL.react_to_do_quit )
                return;
            EDL.do_quit = true;
            /* fall through */

        case SIGALRM :
            if ( Can_Jmp_Alrm )
            {
                Can_Jmp_Alrm = 0;
                siglongjmp( Alrm_Env, 1 );
            }
            return;

        case SIGHUP :                           /* Ignored signals : */
        case SIGINT :
        case SIGUSR1 :
        case SIGCHLD :
        case SIGCONT :
        case SIGTTIN :
        case SIGTTOU :
        case SIGVTALRM :
        case SIGTRAP :
            return;

        case SIGPIPE:
            if ( Fsc2_Internals.cmdline_flags & ( TEST_ONLY | NO_GUI_RUN ) )
                return;
    }

    /* All remaining signals are deadly... */

    Crash.signo = signo;

    /* We may be able to determine where the crash happened and then create
       a crash report file with information about the crash. */

#if ! defined NDEBUG && defined ADDR2LINE
    if (    ! Crash.already_crashed
         && Crash.signo != SIGABRT
         && Crash.signo != SIGTERM )
    {
        Crash.already_crashed = true;
        Crash.trace_length = backtrace( Crash.trace, MAX_TRACE_LEN );
        crash_report( );
    }
#else
    Crash.already_crashed = true;
#endif

    close_all_files( );

    /* Test if parent still exists - if not (i.e. the parent died without
       sending a SIGTERM signal) destroy the semaphore and shared memory (as
       far as the child knows about it) and also kill the child for
       connections (if it exists). */

    if ( getppid( ) == 1 )
    {
        delete_all_shm( );
        sema_destroy( Comm.mq_semaphore );
    }

    _exit( EXIT_FAILURE );
}


/*---------------------------------------------------------------------*
 * The child got to wait for the parent to tell it that it now expects
 * the child to die (if the child dies unexpectedly the parent assumes
 * that something went wrong during the experiment). This functions
 * sets everything up so that the parents DO_QUIT signal will be
 * handled by the correct function, sends a QUITTING signal to the
 * parent and then waits for the parent to allow it to exit.
 *---------------------------------------------------------------------*/

static void
wait_for_confirmation( void )
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
       we can't send the signal to the parent stop the child immediately. */

    if ( getppid( ) == 1 || kill( getppid( ), QUITTING ) == -1 )
        _exit( Child_return_status );           /* commit controlled suicide */

    /* The following infinite loop looks worse than it is, the child really
       exits in the signal handler for DO_QUIT. */

    while ( 1 )
        pause( );
}


/*-----------------------------------------------------------------*
 * Child's handler for the DO_QUIT signal while its waiting to die
 *-----------------------------------------------------------------*/

static void
child_confirmation_handler( int signo )
{
    if ( signo == DO_QUIT )
        _exit( Child_return_status );                /* ...and that's it ! */
}


/*-------------------------------------------------------------------*
 * do_measurement() runs through and executes all commands. Since it
 * is executed by the child it has got to honor the 'do_quit' flag.
 *-------------------------------------------------------------------*/

static void
do_measurement( void )
{
    EDL.react_to_do_quit = true;

    exp_runparser_init( );

    while (    EDL.cur_prg_token != NULL
            && EDL.cur_prg_token < EDL.prg_token + EDL.prg_length )
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
                EDL.do_quit = EDL.react_to_do_quit = false;
                continue;
            }

            /* Don't react to STOP button anymore after ON_STOP label has been
               reached */

            if ( EDL.cur_prg_token == EDL.prg_token + EDL.On_Stop_Pos )
                EDL.react_to_do_quit = EDL.do_quit = false;

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
            RETHROW;
    }
}


/*-----------------------------------------------------------------------*
 * The following is just a lengthy switch to deal with tokens that can't
 * be handled by the parser directly, the tokens for flow control.
 * Control is transfered back to this function whenever the parser finds
 * the end-of-line delimiter (a colon), a closing curly brace or one of
 * the flow control tokens.
 *-----------------------------------------------------------------------*/

static void
deal_with_program_tokens( void )
{
    Prg_Token_T * cur = EDL.cur_prg_token;

    switch ( cur->token )
    {
        case '}' :
            EDL.cur_prg_token = cur->end;
            break;

        case WHILE_TOK :
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
            EDL.cur_prg_token = cur->start;
            break;

        case BREAK_TOK :
            cur->start->counter = 0;
            EDL.cur_prg_token = cur->start->end;
            break;

        case NEXT_TOK :
            EDL.cur_prg_token = cur->start;
            break;

        case IF_TOK :
        case UNLESS_TOK :
            EDL.cur_prg_token = test_condition( cur ) ? cur->start : cur->end;
            break;

        case ELSE_TOK :
            if ( ( ++EDL.cur_prg_token )->token == '{' )
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
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
