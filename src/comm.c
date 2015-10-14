/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


/*
 *   The main problem with the communication between the parent and the child
 *   process is that there can be situations where the child has new data it
 *   needs to pass to the parent before it can continue while the parent
 *   process is too busy to accept the data immediately. To avoid (or at least
 *   to reduce the impact of the problem) pointers to the data as well as the
 *   data are stored in shared memory buffers that can be written to by the
 *   child also when the parent is busy and that the parent reads out when
 *   it's got time.
 *
 *   The buffer with pointers, the message queue, is a basically a ring buffer
 *   with slots into which the type of the data and a pointer to a shared
 *   memory segment, containing the data, are written by the child
 *   process. Besides, there are two pointers to the slots of the message
 *   queue, one pointer, updated by the child only, which indicates the last
 *   slot written to by the child, and a second which points to the next slot
 *   that the parent process has to remove from the message queue and which is
 *   updated by the parent process. When both pointers are identical the
 *   message queue is empty.
 *
 *   To avoid the child process overflowing the message queue with more data
 *   than the parent can handle there is also a data semaphore that gets
 *   initialized to the number of slots in the message queue. Before the child
 *   may write into the next slot it must wait on the semaphore. In turn, the
 *   parent always posts the semaphore when a slot has become free. The
 *   removal of the data from the message queue by the parent is done in an
 *   idle handler, i.e. when it's not busy doing something else.
 *
 *   There are two types of messages to be exchanged between the child and the
 *   parent. The first one is mainly data to be displayed by the parent. These
 *   should be accepted as fast as possible so that the child can continue
 *   immediately with the measurement. The other type of messages are requests
 *   by the child for the parent to do something for it, e.g. display an alert
 *   box, and possibly return the users input. The protocol for requests is
 *   quite simple - the child sends the request and waits for the answer by
 *   the parent, knowing exactly what kind of data to expect.
 *
 *   The exchange of data for REQUESTs isn't done via shared memory segments
 *   but using a pair of pipes. For a request the child has first to put a
 *   REQUEST message into the mesage queue. The parent, in turn, will execute
 *   some action of behalf of the child and return some kind of answer (either
 *   data or just an acknowledgment) which the child must read from the pipe
 *   before it continues.
 *
 *   This scheme will work as long as the parent doesn't get to much behind
 *   with handling DATA messages. But if the parent is very busy and the child
 *   very fast creating (and sending) new data we will finally run out of
 *   shared memory segments. Thus, if getting a new shared memory segment
 *   fails for the child it will sleep a short while and then retry, hoping
 *   that the parent removes one of the previously used segments in the mean
 *   time.
 *
 *   The only problem still un-addressed is data sets exceeding the maximum
 *   size of a shared memory segment. But this limit seems to be rather high
 *   (32768 kB!), so I hope this is never going to happen...  If it should
 *   ever happen this will result in the measurement getting stopped with an
 *   'internal communication error' message printed out.
 *
 *   A final problem that can't be handled by the program is what happens if
 *   the program gets killed in a way that it can't release the shared memory
 *   segments. In this case these memory segments will remain intact since the
 *   system won't remove them. To make it simpler to find and then remove
 *   orphaned shared memory segments, i.e. segments that no process is
 *   interested in anymore, each memory segment allocated by fsc2 is labelled
 *   by the four byte magic number 'fsc2' at its very start. Using this label
 *   at the next start of fsc2 all orphaned segments can be identified and
 *   released.
 */


#include "fsc2.h"


/* locally used routines */

static bool parent_reader( Comm_Struct_T * header );
static bool child_reader( void          * ret,
                          Comm_Struct_T * header );
static void pipe_read( void    * buf,
                       ssize_t   bytes_to_read );
static bool pipe_write( void const * buf,
                        ssize_t      bytes_to_write );
static bool send_browser( FL_OBJECT * browser );

static bool Comm_is_setup = false;


/*----------------------------------------------------------------*
 * This routine sets up the resources needed for the interprocess
 * communication. It is called before the child is forked.
 *----------------------------------------------------------------*/

void
setup_comm( void )
{
    /* Open pipes for passing data between child and parent process - we need
       two pipes, one for the parent process to write to the child process and
       another one for the other way round.*/

    int pr;
    if ( ( pr = pipe( Comm.pd ) ) < 0 || pipe( Comm.pd + 2 ) < 0 )
    {
        if ( pr == 0 )                    /* if first open did succeed */
        {
            close( Comm.pd[ 0 ] );
            close( Comm.pd[ 1 ] );
        }

        eprint( FATAL, false, "Can't set up internal communication "
                "channels.\n" );
        THROW( EXCEPTION );
    }

    /* Then we need a circular buffer plus a a low and a high mark pointer
       in shared memory where the child deposits the keys for further shared
       memory buffers with the data the parent is supposed to display. */

    if ( ( Comm.MQ = ( Message_Queue_T * ) get_shm( &Comm.MQ_ID,
                                                  sizeof *Comm.MQ ) ) == NULL )
    {
        Comm.MQ_ID = -1;
        for ( int i = 0; i < 4; i++ )
            close( Comm.pd[ i ] );

        eprint( FATAL, false, "Can't set up internal communication "
                "channels.\n" );
        THROW( EXCEPTION );
    }

    Comm.MQ->low = Comm.MQ->high = 0;
    for ( int i = 0; i < QUEUE_SIZE; i++ )
        Comm.MQ->slot[ i ].shm_id = -1;

    /* Beside the pipes and the key buffer we need a semaphore which allows
       the parent to control when the child is allowed to send data and
       messages. Its size has to be at least by one element smaller than the
       message queue size to avoid having the child send more messages than
       message queue can hold. */

    if ( ( Comm.mq_semaphore = sema_create( QUEUE_SIZE - 1 ) ) < 0 )
    {
        delete_all_shm( );
        for ( int i = 0; i < 4; i++ )
            close( Comm.pd[ i ] );

        eprint( FATAL, false, "Can't set up internal communication "
                "channels.\n" );
        THROW( EXCEPTION );
    }

    Comm_is_setup = true;
}


/*--------------------------------------------------------------*
 * This routine is called after the child process died to clean
 * up the resources used in the interprocess communication.
 *--------------------------------------------------------------*/

void
end_comm( void )
{
    if ( ! Comm_is_setup )
        return;

    Comm_is_setup = false;

    /* Handle remaining messages */

    while ( Comm.MQ->low != Comm.MQ->high )
        new_data_handler( );

    /* Get rid of all the shared memory segments */

    delete_all_shm( );

    /* Delete the semaphore */

    sema_destroy( Comm.mq_semaphore );
    Comm.mq_semaphore = -1;

    /* Close parents side of read and write pipe */

    close( Comm.pd[ READ ] );
    close( Comm.pd[ WRITE ] );
}


/*-----------------------------------------------------------------*
 * In new_data_handler() the parent checks for new messages in the
 * message queue. The message queue is read as long as the low
 * marker does not reach the high marker, both being incremented
 * in a wrap-around fashion. Accepting new data or honoring a
 * REQUEST may fail, most probably due to running out of memory.
 * Therefore all the code has to be run in a TRY environment and
 * when something fails we have to kill the child to prevent it
 * sending further data we can't accept anymore. end_comm() calls
 * this function a last time, so we also have to set the low and
 * the high marker to the same value which keeps the function from
 * being executed because the child is now already dead and can't
 * change the high marker anymore.
 *-----------------------------------------------------------------*/

int
new_data_handler( void )
{
    /* Check if the child raised a signal to tell us it's done. If it did
       send it a DO_QUIT signal (but only if it's still alive) and remove
       the callback for the 'STOP' button */

    if ( Fsc2_Internals.child_is_quitting == QUITTING_RAISED )
    {
        if (    Fsc2_Internals.child_pid > 0
             && ! kill( Fsc2_Internals.child_pid, 0 ) )
            kill( Fsc2_Internals.child_pid, DO_QUIT );
        Fsc2_Internals.child_is_quitting = QUITTING_ACCEPTED;

        if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        {
            if ( G.dim & 1 || ! G.is_init )
                fl_set_object_callback( GUI.run_form_1d->stop_1d, NULL, 0 );
            if ( G.dim & 2 )
                fl_set_object_callback( GUI.run_form_2d->stop_2d, NULL, 0 );
        }
    }

    /* Check if the child is waiting for an answer in a call of the
       toolbox_wait() function and the timer expired. */

    if ( Fsc2_Internals.tb_wait == TB_WAIT_TIMER_EXPIRED )
        tb_wait_handler( 0 );

    /* Now handle new data send by the child, first look for new data and, if
       afterwards there are still entries, check if there's a REQUEST. */

    if ( Comm.MQ->low != Comm.MQ->high )
    {
        TRY
        {
            if ( Comm.MQ->slot[ Comm.MQ->low ].type != REQUEST )
                accept_new_data( Fsc2_Internals.child_is_quitting
                                                           != QUITTING_UNSET );

            if (    Comm.MQ->low != Comm.MQ->high
                 && Comm.MQ->slot[ Comm.MQ->low ].type == REQUEST )
            {
                Comm.MQ->low = ( Comm.MQ->low + 1 ) % QUEUE_SIZE;
                sema_post( Comm.mq_semaphore );
                if ( ! reader( NULL ) )
                    THROW( EXCEPTION );
            }

            TRY_SUCCESS;
        }
        OTHERWISE
        {
            /* Before killing the child on failures to get the new data test
               that its PID hasn't already been set to 0 (in which case we
               would commit suicide) and that it's still alive. The death of
               the child and all the necessary cleaning up is dealt with by
               the SIGCHLD handlers in run.c, so no need to worry about it
               here. */

            if (    Fsc2_Internals.child_pid > 0
                 && ! kill( Fsc2_Internals.child_pid, 0 ) )
                kill( Fsc2_Internals.child_pid, SIGTERM );

            Comm.MQ->low = Comm.MQ->high;
        }
    }

#if defined WITH_HTTP_SERVER

    /* Finally check for requests from the HTTP server and handle death
       of the HTTP server (but only if the queue is empty, we don't want
       to slow down the experiment by serving pages when the process is
       already struggling to keep up with data the child sends). */

    if (    ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
         && Comm.MQ->low == Comm.MQ->high )
    {
        if ( Fsc2_Internals.http_pid > 0 )
            http_check( );
        else if ( Fsc2_Internals.http_server_died )
        {
            Fsc2_Internals.http_server_died = false;
            fl_call_object_callback( GUI.main_form->server );
        }
    }
#endif

    return 0;
}


/*-------------------------------------------------------------*
 * Called whenever the child needs to send data to the parent.
 *-------------------------------------------------------------*/

void
send_data( int type,
           int shm_id )
{
    /* Wait until parent can accept more data */

    sema_wait( Comm.mq_semaphore );

    /* Put the type of the data (DATA_1D, DATA_2D or REQUEST) into the type
       field of the next free slot */

    Comm.MQ->slot[ Comm.MQ->high ].type = type;

    /* For DATA pass parent the ID of shared memory segment with the data */

    if ( type != REQUEST )
        Comm.MQ->slot[ Comm.MQ->high ].shm_id = shm_id;

    /* Increment the high mark pointer (wraps around) */

    Comm.MQ->high = ( Comm.MQ->high + 1 ) % QUEUE_SIZE;
}


/*----------------------------------------------------------------------*
 * This function handles the reading from the pipe for both the parent
 * and the child process. In each case a message is started by a header
 * that contains at least information about the type of message.
 *----------------------------------------------------------------------*/

bool
reader( void * ret )
{
    Comm_Struct_T header;

    /* Get the header - failure indicates that the child is dead */

    TRY
    {
        pipe_read( &header, sizeof header );
        TRY_SUCCESS;
    }
    OTHERWISE
        return false;

    return Fsc2_Internals.I_am == PARENT ?
           parent_reader( &header ) : child_reader( ret, &header );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

#define HANDLE_READ( func )                        \
    {                                              \
        char * volatile data = NULL;               \
        TRY                                        \
        {                                          \
            data = T_malloc( header->data.len );   \
            pipe_read( data, header->data.len );   \
            func( data, header->data.len );        \
            TRY_SUCCESS;                           \
        }                                          \
        OTHERWISE                                  \
        {                                          \
            T_free( data );                        \
            return false;                          \
        }                                          \
        T_free( data );                            \
        break;                                     \
    }


static bool
parent_reader( Comm_Struct_T * header )
{
    char *str[ 4 ] = { NULL, NULL, NULL, NULL };

    switch ( header->type )
    {
        case C_EPRINT :
            /* Get the string to be printed... */

            TRY
            {
                if ( header->data.str_len[ 0 ] > 0 )
                {
                    str[ 0 ] = T_malloc(
                                    ( size_t ) header->data.str_len[ 0 ] + 1 );
                    pipe_read( str[ 0 ], header->data.str_len[ 0 ] );
                    str[ 0 ][ header->data.str_len[ 0 ] ] = '\0';
                }
                else if ( header->data.str_len[ 0 ] == 0 )
                    str[ 0 ] = T_strdup( "" );

                eprint( NO_ERROR, false, "%s", str[ 0 ] );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                T_free( str[ 0 ] );
                return false;
            }

            /* Get rid of the string and return */

            T_free( str[ 0 ] );
            break;

        case C_SHOW_MESSAGE :
            str[ 0 ] = NULL;

            TRY
            {
                if ( header->data.str_len[ 0 ] > 0 )
                {
                    str[ 0 ] = T_malloc(
                                    ( size_t ) header->data.str_len[ 0 ] + 1 );
                    pipe_read( str[ 0 ], header->data.str_len[ 0 ] );
                    str[ 0 ][ header->data.str_len[ 0 ] ] = '\0';
                }
                else if ( header->data.str_len[ 0 ] == 0 )
                    str[ 0 ] = T_strdup( "" );

                show_message( str[ 0 ] );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                T_free( str[ 0 ] );
                return false;
            }

            T_free( str[ 0 ] );

            /* Send back an acknowledgement that the message has been read
               by the user */

            return writer( C_ACK );

        case C_SHOW_ALERT :
            TRY
            {
                if ( header->data.str_len[ 0 ] > 0 )
                {
                    str[ 0 ] = T_malloc(
                                    ( size_t ) header->data.str_len[ 0 ] + 1 );
                    pipe_read( str[ 0 ], header->data.str_len[ 0 ] );
                    str[ 0 ][ header->data.str_len[ 0 ] ] = '\0';
                }
                else if ( header->data.str_len[ 0 ] == 0 )
                    str[ 0 ] = T_strdup( "" );

                show_alert( str[ 0 ] );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                T_free( str[ 0 ] );
                return false;
            }

            T_free( str[ 0 ] );

            /* Send back an acknowledgement that alert has been read by the
               user */

            return writer( C_ACK );

        case C_SHOW_CHOICES :
            /* Get number of buttons and number of default button */

            TRY
            {
                int n1, n2;

                pipe_read( &n1, sizeof( int ) );
                pipe_read( &n2, sizeof( int ) );

                for ( int i = 0; i < 4; i++ )
                {
                    if ( header->data.str_len[ i ] > 0 )
                    {
                        str[ i ] =
                          T_malloc( ( size_t ) header->data.str_len[ i ] + 1 );
                        pipe_read( str[ i ], header->data.str_len[ i ] );
                        str[ i ][ header->data.str_len[ i ] ] = '\0';
                    }
                    else if ( header->data.str_len[ i ] == 0 )
                        str[ i ] = T_strdup( "" );
                }

                /* Show the question, get the button number and pass it back to
                   the child process (which does a read in the mean time) */

                if ( ! writer( C_INT, show_choices( str[ 0 ], n1, str[ 1 ],
                                                    str[ 2 ], str[ 3 ], n2,
                                                    false ) ) )
                    THROW( EXCEPTION );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                for ( int i = 0; i < 4 && str[ i ] != NULL; i++ )
                    T_free( str[ i ] );
                return false;
            }

            /* Get rid of the strings and return */

            for ( int i = 0; i < 4 && str[ i ] != NULL; i++ )
                T_free( str[ i ] );
            break;

        case C_SHOW_FSELECTOR :
            TRY
            {
                for ( int i = 0; i < 4; i++ )
                {
                    if ( header->data.str_len[ i ] > 0 )
                    {
                        str[ i ] =
                          T_malloc( ( size_t ) header->data.str_len[ i ] + 1 );
                        pipe_read( str[ i ], header->data.str_len[ i ] );
                        str[ i ][ header->data.str_len[ i ] ] = '\0';
                    }
                    else if ( header->data.str_len[ i ] == 0 )
                        str[ i ] = T_strdup( "" );
                }

                /* Call fsc2_show_fselector() and send the result back to the
                   child process (which is waiting to read in the mean time) */

                Fsc2_Internals.state = STATE_WAITING;
                if ( ! writer( C_STR, fsc2_show_fselector( str[ 0 ], str[ 1 ],
                                                           str[ 2 ],
                                                           str[ 3 ] ) ) )
                    THROW( EXCEPTION );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                for ( int i = 0; i < 4 && str[ i ] != NULL; i++ )
                    T_free( str[ i ] );
                Fsc2_Internals.state = STATE_RUNNING;
                return false;
            }

            for ( int i = 0; i < 4 && str[ i ] != NULL; i++ )
                T_free( str[ i ] );
            Fsc2_Internals.state = STATE_RUNNING;
            break;

        case C_PROG :
        case C_OUTPUT :
                return send_browser( header->type == C_PROG ?
                                     GUI.main_form->browser :
                                     GUI.main_form->error_browser );

        case C_INPUT :
            TRY
            {
                for ( int i = 0; i < 2 ; i++ )
                    if ( header->data.str_len[ i ] > 0 )
                    {
                        str[ i ] =
                          T_malloc( ( size_t ) header->data.str_len[ i ] + 1 );
                        pipe_read( str[ i ], header->data.str_len[ i ] );
                        str[ i ][ header->data.str_len[ i ] ] = '\0';
                    }
                    else if ( header->data.str_len[ i ] == 0 )
                        str[ i ] = T_strdup( "" );

                if ( ! writer( C_STR, show_input( str[ 0 ], str[ 1 ] ) ) )
                    THROW( EXCEPTION );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                for ( int i = 0; i < 2 && str[ i ] != NULL; i++ )
                    T_free( str[ i ] );
                return false;
            }
            break;

        case C_LAYOUT :
            HANDLE_READ( exp_layout );

        case C_BCREATE :
            HANDLE_READ( exp_bcreate );

        case C_BDELETE :
            HANDLE_READ( exp_bdelete );

        case C_BSTATE :
            HANDLE_READ( exp_bstate );

        case C_BCHANGED :
            HANDLE_READ( exp_bchanged );

        case C_SCREATE :
            HANDLE_READ( exp_screate );

        case C_SDELETE :
            HANDLE_READ( exp_sdelete );

        case C_SSTATE :
            HANDLE_READ( exp_sstate );

        case C_SCHANGED :
            HANDLE_READ( exp_schanged );

        case C_ICREATE :
            HANDLE_READ( exp_icreate );

        case C_IDELETE :
            HANDLE_READ( exp_idelete );

        case C_ISTATE :
            HANDLE_READ( exp_istate );

        case C_ICHANGED :
            HANDLE_READ( exp_ichanged );

        case C_MCREATE :
            HANDLE_READ( exp_mcreate );

        case C_MADD :
            HANDLE_READ( exp_madd );

        case C_MTEXT :
            HANDLE_READ( exp_mtext );

        case C_MDELETE :
            HANDLE_READ( exp_mdelete );

        case C_MCHOICE :
            HANDLE_READ( exp_mchoice );

        case C_MCHANGED :
            HANDLE_READ( exp_mchanged );

        case C_TBCHANGED :
            HANDLE_READ( exp_tbchanged );

        case C_TBWAIT :
            HANDLE_READ( exp_tbwait );

        case C_ODELETE :
            HANDLE_READ( exp_objdel );

        case C_FREEZE :
            parent_freeze( header->data.int_data );
            break;

        case C_GETPOS :
            HANDLE_READ( exp_getpos );

        case C_CLABEL :
            HANDLE_READ( exp_clabel );

        case C_XABLE :
            HANDLE_READ( exp_xable );

        case C_CB_1D :
            HANDLE_READ( exp_cb_1d );

        case C_CB_2D :
            HANDLE_READ( exp_cb_2d );

        case C_ZOOM_1D :
            HANDLE_READ( exp_zoom_1d );

        case C_ZOOM_2D :
            HANDLE_READ( exp_zoom_2d );

        case C_FSB_1D :
            HANDLE_READ( exp_fsb_1d );

        case C_FSB_2D :
            HANDLE_READ( exp_fsb_2d );

        default :                     /* this better never gets triggered... */
            fsc2_impossible( );
    }

    return true;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
child_reader( void          * ret,
              Comm_Struct_T * header )
{
    char * volatile retstr = NULL;

    switch ( header->type )
    {
        case C_ACK :
            return true;

        case C_STR :
            if ( header->data.str_len[ 0 ] == -1 )
            {
                if ( ret != NULL )
                     *( ( char ** ) ret ) = NULL;
                return true;
            }

            TRY
            {
                retstr = T_malloc( header->data.str_len[ 0 ] + 1 );
                if ( header->data.str_len[ 0 ] > 0 )
                    pipe_read( retstr, header->data.str_len[ 0 ] );
                retstr[ header->data.str_len[ 0 ] ] = '\0';
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                if ( retstr != NULL )
                    T_free( retstr );
                return false;
            }

            if ( ret != NULL )
                *( ( char ** ) ret ) = retstr;
            return true;

        case C_INT :
            if ( ret != NULL )
                *( ( int * ) ret ) = header->data.int_data;
            return true;

        case C_LONG :
            if ( ret != NULL )
                *( ( long * ) ret ) = header->data.long_data;
            return true;

        case C_FLOAT :
            if ( ret != NULL )
                *( ( float * ) ret ) = header->data.float_data;
            return true;

        case C_DOUBLE :
            if ( ret != NULL )
                *( ( double * ) ret ) = header->data.double_data;
            return true;

        case C_MTEXT_REPLY :
            * ( char ** ) ret = NULL;

            TRY
            {
                if ( header->data.str_len[ 0 ] > 0 )
                {
                    * ( char ** ) ret =
                         T_malloc( ( size_t ) header->data.str_len[ 0 ] + 1 );
                    pipe_read( * ( char ** ) ret,
                               header->data.str_len[ 0 ] );
                    * ( * ( char ** ) ret +  header->data.str_len[ 0 ] ) = '\0';
                }
                else if ( header->data.str_len[ 0 ] == 0 )
                    * ( char ** ) ret = T_strdup( "" );

                TRY_SUCCESS;
            }
            OTHERWISE
            {
                if ( * ( char ** ) ret )
                    T_free( * ( char ** ) ret );
                return false;
            }
            return true;

        case C_BCREATE_REPLY   :
        case C_BSTATE_REPLY    :
        case C_BCHANGED_REPLY  :
        case C_SCREATE_REPLY   :
        case C_SSTATE_REPLY    :
        case C_SCHANGED_REPLY  :
        case C_ICREATE_REPLY   :
        case C_ISTATE_REPLY    :
        case C_ICHANGED_REPLY  :
        case C_MCREATE_REPLY   :
        case C_MCHOICE_REPLY   :
        case C_MCHANGED_REPLY  :
        case C_TBCHANGED_REPLY :
        case C_TBWAIT_REPLY    :
        case C_GETPOS_REPLY    :
            if ( ret != NULL )
            {
                TRY
                {
                    pipe_read( ( char * ) ret, header->data.len );
                    TRY_SUCCESS;
                }
                OTHERWISE
                    return false;
            }
            return true;

        case C_ISTATE_STR_REPLY :
            TRY
            {
                retstr = T_malloc( ( size_t ) header->data.len + 1 );
                if ( header->data.len > 0 )
                    pipe_read( retstr, header->data.len );
                retstr[ header->data.len ] = '\0';
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                if ( retstr != NULL )
                    T_free( retstr );
                return false;
            }

            if ( ret != NULL )
            {
                ( ( Input_Res_T * ) ret )->res = STR_VAR;
                ( ( Input_Res_T * ) ret )->val.sptr = retstr;
            }
            return true;

        case C_LAYOUT_REPLY  :
        case C_BDELETE_REPLY :
        case C_SDELETE_REPLY :
        case C_IDELETE_REPLY :
        case C_MADD_REPLY    :
        case C_MDELETE_REPLY :
        case C_ODELETE_REPLY :
        case C_CLABEL_REPLY  :
        case C_XABLE_REPLY   :
        case C_CB_1D_REPLY   :
        case C_CB_2D_REPLY   :
        case C_ZOOM_1D_REPLY :
        case C_ZOOM_2D_REPLY :
        case C_FSB_1D_REPLY  : case C_FSB_2D_REPLY  :
            return header->data.long_data;
    }

    fsc2_impossible( );            /* this better never gets triggered... */
    return false;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
void
header_strlen_setup( Comm_Struct_T * h,
                     int             field,
                     char    const * str )
{
    if ( ! str )
        h->data.str_len[ field ] = -1;
    else if ( ! *str )
        h->data.str_len[ field ] = 0;
    else
        h->data.str_len[ field ] = strlen( str );
}


/*-----------------------------------------------------------------------*
 * This function is the opposite of reader() (see above) - it writes the
 * data into the pipe.
 *-----------------------------------------------------------------------*/

bool
writer( int type,
        ... )
{
    Comm_Struct_T header = { 0, { 0 } };    /* Initialization for valgrind */
    char *str[ 4 ];
    int n1, n2;
    void *data;

    /* The child process has to wait for the parent process to become ready to
       accept and then to write to the pipe. The other way round waiting isn't
       necessary since the child process is only reading when it actually
       waits for data. */

    if ( Fsc2_Internals.I_am == CHILD )
        send_data( REQUEST, -1 );

    header.type = type;

    va_list ap;
    va_start( ap, type );

    if ( Fsc2_Internals.I_am == CHILD )
    {
        switch ( type )
        {
            case C_EPRINT :
                str[ 0 ] = va_arg( ap, char * );
                va_end( ap );
                header_strlen_setup( &header, 0, str[ 0 ] );
                return    pipe_write( &header, sizeof header )
                       && pipe_write( str[ 0 ], header.data.str_len[ 0 ] );

            case C_SHOW_MESSAGE :
                str[ 0 ] = va_arg( ap, char * );
                va_end( ap );
                header_strlen_setup( &header, 0, str[ 0 ] );
                return    pipe_write( &header, sizeof header )
                       && pipe_write( str[ 0 ], header.data.str_len[ 0 ] );

            case C_SHOW_ALERT :
                str[ 0 ] = va_arg( ap, char * );
                va_end( ap );
                header_strlen_setup( &header, 0, str[ 0 ] );
                return    pipe_write( &header, sizeof header )
                       && pipe_write( str[ 0 ], header.data.str_len[ 0 ] );

            case C_SHOW_CHOICES :
                str[ 0 ] = va_arg( ap, char * );
                header_strlen_setup( &header, 0, str[ 0 ] );

                n1 = va_arg( ap, int );

                for ( int i = 1; i < 4; i++ )
                {
                    str[ i ] = va_arg( ap, char * );
                    header_strlen_setup( &header, i, str[ i ] );
                }

                n2 = va_arg( ap, int );
                va_end( ap );

                if (    ! pipe_write( &header, sizeof header )
                     || ! pipe_write( &n1, sizeof( int ) )
                     || ! pipe_write( &n2, sizeof( int ) ) )
                    return false;

                for ( int i = 0; i < 4; i++ )
                    if ( ! pipe_write( str[ i ], header.data.str_len[ i ] ) )
                        return false;
                break;

            case C_SHOW_FSELECTOR :
                /* Set up header and write it */

                for ( int i = 0; i < 4; i++ )
                {
                    str[ i ] = va_arg( ap, char * );
                    header_strlen_setup( &header, i, str[ i ] );
                }

                va_end( ap );

                if ( ! pipe_write( &header, sizeof header ) )
                    return false;

                /* Write out all four strings */

                for ( int i = 0; i < 4; i++ )
                    if ( ! pipe_write( str[ i ], header.data.str_len[ i ] ) )
                        return false;
                break;

            case C_PROG   :
            case C_OUTPUT :
                va_end( ap );
                return pipe_write( &header, sizeof header );

            case C_INPUT :
                /* Set up the two argument strings */

                for ( int i = 0; i < 2; i++ )
                {
                    str[ i ] = va_arg( ap, char * );
                    header_strlen_setup( &header, i, str[ i ] );
                }

                va_end( ap );

                /* Send header and the two strings */

                if ( ! pipe_write( &header, sizeof header ) )
                    return false;
                for ( int i = 0; i < 2; i++ )
                    if ( ! pipe_write( str[ i ], header.data.str_len[ i ] ) )
                        return false;
                break;

            case C_LAYOUT   :
            case C_BCREATE   :
            case C_BDELETE   :
            case C_BSTATE    :
            case C_BCHANGED  :
            case C_SCREATE   :
            case C_SDELETE   :
            case C_SSTATE    :
            case C_SCHANGED  :
            case C_ICREATE   :
            case C_IDELETE   :
            case C_ISTATE    :
            case C_ICHANGED  :
            case C_MCREATE   :
            case C_MADD      :
            case C_MTEXT     :
            case C_MDELETE   :
            case C_MCHOICE   :
            case C_MCHANGED  :
            case C_TBCHANGED :
            case C_TBWAIT    :
            case C_ODELETE   :
            case C_CLABEL    :
            case C_XABLE     :
            case C_GETPOS    :
            case C_CB_1D     :
            case C_CB_2D     :
            case C_ZOOM_1D   :
            case C_ZOOM_2D   :
            case C_FSB_1D    :
            case C_FSB_2D    :
                header.data.len = va_arg( ap, ptrdiff_t );
                data = va_arg( ap, void * );
                va_end( ap );
                return    pipe_write( &header, sizeof header )
                       && pipe_write( data, header.data.len );

            case C_FREEZE :
                header.data.int_data = va_arg( ap, int );
                va_end( ap );
                return pipe_write( &header, sizeof header );

            default :                     /* this should never be reached... */
                va_end( ap );
                fsc2_impossible( );
        }
    }
    else                   /* if this is the parent process */
    {
        switch ( type )
        {
            case C_ACK :
                va_end( ap );
                return pipe_write( &header, sizeof header );

            case C_BCREATE_REPLY   :
            case C_BSTATE_REPLY    :
            case C_BCHANGED_REPLY  :
            case C_SCREATE_REPLY   :
            case C_SSTATE_REPLY    :
            case C_SCHANGED_REPLY  :
            case C_ICREATE_REPLY   :
            case C_ISTATE_REPLY    :
            case C_ICHANGED_REPLY  :
            case C_MCREATE_REPLY   :
            case C_MCHOICE_REPLY   :
            case C_MCHANGED_REPLY  :
            case C_TBCHANGED_REPLY :
            case C_TBWAIT_REPLY    :
            case C_GETPOS_REPLY    :
                header.data.len = va_arg( ap, ptrdiff_t );
                data = va_arg( ap, void * );
                va_end( ap );
                return    pipe_write( &header, sizeof header )
                       && pipe_write( data, header.data.len );

            case C_MTEXT_REPLY :
                str[ 0 ] = va_arg( ap, char * );
                va_end( ap );
                header_strlen_setup( &header, 0, str[ 0 ] );
                return    pipe_write( ( char * ) &header, sizeof header )
                       && pipe_write( str[ 0 ], header.data.str_len[ 0 ] );

            case C_ISTATE_STR_REPLY :
                header.data.len = va_arg( ap, ptrdiff_t );
                data = va_arg( ap, void * );
                va_end( ap );
                return    pipe_write( ( char * ) &header, sizeof header )
                       && pipe_write( data, header.data.len );

            case C_LAYOUT_REPLY  :
            case C_BDELETE_REPLY :
            case C_SDELETE_REPLY :
            case C_IDELETE_REPLY :
            case C_MADD_REPLY    :
            case C_MDELETE_REPLY :
            case C_ODELETE_REPLY :
            case C_CLABEL_REPLY  :
            case C_XABLE_REPLY   :
            case C_CB_1D_REPLY   :
            case C_CB_2D_REPLY   :
            case C_ZOOM_1D_REPLY :
            case C_ZOOM_2D_REPLY :
            case C_FSB_1D_REPLY  :
            case C_FSB_2D_REPLY  :
                header.data.long_data = va_arg( ap, long );
                va_end( ap );
                return pipe_write( &header, sizeof header );

            case C_STR :
                str[ 0 ] = va_arg( ap, char * );
                va_end( ap );
                header_strlen_setup( &header, 0, str[ 0 ] );
                return    pipe_write( &header, sizeof header )
                       && pipe_write( str[ 0 ], header.data.str_len[ 0 ] );

            case C_INT :
                header.data.int_data = va_arg( ap, int );
                va_end( ap );
                return pipe_write( &header, sizeof header );

            case C_LONG :
                header.data.long_data = va_arg( ap, long );
                va_end( ap );
                return pipe_write( &header, sizeof header );

            case C_FLOAT :
                header.data.float_data = ( float ) va_arg( ap, double );
                va_end( ap );
                return pipe_write( &header, sizeof header );

            case C_DOUBLE :
                header.data.double_data = va_arg( ap, double );
                va_end( ap );
                return pipe_write( &header, sizeof header );

            default :                     /* this should never be reached... */
                va_end( ap );
                fsc2_impossible( );
        }
    }

    return true;
}


/*------------------------------------------------------------------*
 * Sends all lines of one of the browsers successively to the child
 *------------------------------------------------------------------*/

static bool
send_browser( FL_OBJECT * browser )
{
    const char *line;
    int i = 0;


    while ( ( line = fl_get_browser_line( browser, ++i ) ) != NULL )
        if ( ! writer( C_STR, line ) )
            return false;
    return writer( C_STR, NULL );
}


/*--------------------------------------------------------------*
 * Functions reads from the pipe in chunks of the maximum size.
 * ->
 *    1. Pointer to buffer with data
 *    2. Number of bytes to be read
 *    3. Flag, set when reading by the child can be interrupted
 *       by a DO_QUIT signal
 *--------------------------------------------------------------*/

static void
pipe_read( void    * vbuf,
           ssize_t   bytes_to_read )
{
    /* From man 2 read(): POSIX allows a read that is interrupted after
       reading some data to return -1 (with errno set to EINTR) or to return
       the number of bytes already read.  The latter happens on newer Linux
       system with e.g. kernel 2.2.12 while on older systems, e.g. 2.0.36,
       -1 is returned when read got interrupted. Blocking all expected
       signals while reading and using this rather lengthy loop tries to
       get it right in both cases. */

    sigset_t new_mask;
    sigemptyset( &new_mask );
    if ( Fsc2_Internals.I_am == CHILD )
        sigaddset( &new_mask, DO_QUIT );
    else
        sigaddset( &new_mask, QUITTING );

    sigset_t old_mask;
    sigprocmask( SIG_BLOCK, &new_mask, &old_mask );

    char * buf = vbuf;
    long already_read = 0;

    while ( bytes_to_read > 0 )
    {
        ssize_t bytes_read = read( Comm.pd[ READ ], buf + already_read,
                                   bytes_to_read );

        /* Non-deadly signal has been received */

        if ( bytes_read == -1 && errno == EINTR )
            continue;

        /* Real error while reading, not a signal */

        if ( bytes_read <= 0 )
            break;

        bytes_to_read -= bytes_read;
        already_read += bytes_read;
    }

    sigprocmask( SIG_SETMASK, &old_mask, NULL );
    if ( bytes_to_read != 0 )
        THROW( EXCEPTION );
}


/*--------------------------------------------------------------*
 * Functions writes to the pipe in chunks of the maximum size.
 * ->
 *    1. Pointer to buffer for storing the data
 *    2. Number of bytes to be written
 *--------------------------------------------------------------*/

static bool
pipe_write( void const * vbuf,
            ssize_t      bytes_to_write )
{
    if ( bytes_to_write <= 0 )
        return true;

    sigset_t new_mask;
    sigemptyset( &new_mask );
    if ( Fsc2_Internals.I_am == CHILD )
        sigaddset( &new_mask, DO_QUIT );
    else
        sigaddset( &new_mask, QUITTING );

    sigset_t old_mask;
    sigprocmask( SIG_BLOCK, &new_mask, &old_mask );

    char const * buf = vbuf;
    long already_written = 0;

    while ( bytes_to_write > 0 )
    {
        ssize_t bytes_written = write( Comm.pd[ WRITE ], buf + already_written,
                                       bytes_to_write );

        /* Non-deadly signal has been received */

        if ( bytes_written == -1 && errno == EINTR )
            continue;

        /* Real error while writing, not a signal */

        if ( bytes_written <= 0 )
            break;

        bytes_to_write -= bytes_written;
        already_written += bytes_written;
    }

    sigprocmask( SIG_SETMASK, &old_mask, NULL );
    return bytes_to_write == 0 ? true : false;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
