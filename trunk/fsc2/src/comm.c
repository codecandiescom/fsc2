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


/*
   The main problem with the communication between the parent and the child
   process is that there can be situations where the child has new data it
   needs to pass to the parent before it can continue while the parent process
   is too busy to accept the data immediately. To avoid (or at least to reduce
   the impact of the problem) pointers to the data as well as the data are
   stored in shared memory buffers that can be written to by the child also
   when the parent is busy and that the parent reads out when it has time.

   The buffer with pointers, the message queue, is a basically a ring buffer
   with slots into which the type of the data and a pointer to a shared memory
   segment, containing the data, are written by the child process. Beside,
   there are two pointers to the slots of the message queue, one pointer,
   updated by the child only, which indicats the last slot written into by the
   child, and a second which points to the next slot that the parent process
   has to remove from the message queue and which is updated by the parent
   process. When both pointers are identical the message queue is empty.

   To avoid the child process overflowing the message queue with more data
   than the parent can handle there is also a data semaphore that gets
   initialized to the number of slots in the message queue. Before the child
   may write into the next slot it must wait on the semaphore. In turn, the
   parent always posts the semaphore when a slot has become free. The removal
   of the data from the message queue by the parent is done in an idle
   handler, i.e. when it's not busy doing something else.

   There are two types of messages to be exchanged between the child and the
   parent. The first one is mainly data to be displayed by the parent. These
   should be accepted as fast as possible so that the child can continue
   immediately with the measurement. The other type of messages are requests
   by the child for the parent to do something for it, e.g. display an alert
   box, and possibly return the users input. The protocol for requests is
   quite simple - the child sends the request and waits for the answer by the
   parent, knowing exactly what kind of data to expect.

   The exchange of data for REQUESTs isn't done via shared memory segments
   but using a pair of pipes. To avoid having the child write more than
   one message to the pipe there's a second semaphore initialized to one.
   For a request the child has first to put a REQUEST message into the mesage
   queue and then to wait also for the second semaphore before the writing
   to the pipe. The parent, in turn, will post this second semaphore when it
   receives a REQUEST message and will then read the pipe (and if necessary
   return data to the child via a second pipe).

   This scheme will work as long as the parent doesn't get to much behind with
   handling DATA messages. But if the parent is very busy and the child very
   fast creating (and sending) new data we will finally run out of shared
   memory segments. Thus, if getting a new shared memory segment fails for the
   child it will sleep a short while and then retry, hoping that the parent
   removes one of the previously used segments in the mean time.

   The only problem still un-addressed is data sets exceeding the maximum
   size of a shared memory segment. But this limit seems to be rather high
   (32768 kB!), so I hope this is never going to happen...  If it should ever
   happen this will result in the measurement getting stopped with an
   `internal communication error' message printed out.

   A final problem that can't be handled by the program is what happens if the
   program gets killed in a way that it can't release the shared memory
   segments. In this case these memory segments will remain intact since the
   system won't remove them. To make it simpler to find and then remove
   orphaned shared memory segments, i.e. segments that no process is
   interested in anymore, each memory segment allocated by fsc2 is labelled by
   the four byte magic number 'fsc2' at its very start. Using this label at
   the next start of fsc2 all orphaned segments can be identified and released.
*/


#include "fsc2.h"
#include <sys/shm.h>


/* locally used routines */

static bool	parent_reader( CommStruct *header );
static bool	child_reader( void *ret, CommStruct *header );
static bool pipe_read( char *buf, size_t bytes_to_read );
static bool pipe_write( char *buf, size_t bytes_to_write );
static bool send_browser( FL_OBJECT *browser );


/*----------------------------------------------------------------*/
/* This routine sets up the resources needed for the interprocess */
/* communication. It is called before the child is forked.        */
/*----------------------------------------------------------------*/

void setup_comm( void )
{
	int pr;
	int i;


	/* Open pipes for passing data between child and parent process - we need
	   two pipes, one for the parent process to write to the child process and
	   another one for the other way round.*/

    if ( ( pr = pipe( Comm.pd ) ) < 0 || pipe( Comm.pd + 2 ) < 0 )
	{
		if ( pr == 0 )                    /* if first open did succeed */
		{
			close( Comm.pd[ 0 ] );
			close( Comm.pd[ 1 ] );
		}

		eprint( FATAL, UNSET, "Can't set up internal communication "
				"channels.\n" );
		THROW( EXCEPTION );
	}

	if ( ( Comm.MQ = ( MESSAGE_QUEUE * ) get_shm( &Comm.MQ_ID,
                                                  sizeof *Comm.MQ ) )
		 == ( MESSAGE_QUEUE * ) -1 )
	{
		Comm.MQ_ID = -1;
		for ( i = 0; i < 4; i++ )
			close( Comm.pd[ i ] );

		eprint( FATAL, UNSET, "Can't set up internal communication "
				"channels.\n" );
		THROW( EXCEPTION );
	}

	Comm.MQ->low = Comm.MQ->high = 0;
	for ( i = 0; i < QUEUE_SIZE; i++ )
		Comm.MQ->slot[ i ].shm_id = -1;

	/* Beside the pipes we need a semaphore which allows the parent to control
	   when the child is allowed to send data and messages. Its size has to be
	   identcal to the message queue size to avoid having the child send more
	   messages than queue can hold. */

	if ( ( Comm.data_semaphore = sema_create( QUEUE_SIZE ) ) < 0 )
	{
		delete_all_shm( );
		for ( i = 0; i < 4; i++ )
			close( Comm.pd[ i ] );

		eprint( FATAL, UNSET, "Can't set up internal communication "
				"channels.\n" );
		THROW( EXCEPTION );
	}

	if ( ( Comm.request_semaphore = sema_create( 0 ) ) < 0 )
	{
		sema_destroy( Comm.data_semaphore );
		Comm.data_semaphore = -1;
		delete_all_shm( );
		for ( i = 0; i < 4; i++ )
			close( Comm.pd[ i ] );

		eprint( FATAL, UNSET, "Can't set up internal communication "
				"channels.\n" );
		THROW( EXCEPTION );
	}
}


/*--------------------------------------------------------------*/
/* This routine is called after the child process died to clean */
/* up the resources used in the interprocess communication.     */
/*--------------------------------------------------------------*/

void end_comm( void )
{
	/* Handle remaining messages */

	if ( Comm.MQ->low != Comm.MQ->high )
		new_data_callback( NULL, 0 );

	/* Get rid of all the shared memory segments */

	delete_all_shm( );

	/* Delete the semaphores */

	sema_destroy( Comm.request_semaphore );
	Comm.request_semaphore = -1;
	sema_destroy( Comm.data_semaphore );
	Comm.data_semaphore = -1;

	/* Close parents side of read and write pipe */

	close( Comm.pd[ READ ] );
	close( Comm.pd[ WRITE ] );
}


/*------------------------------------------------------------------*/
/* new_data_callback() is an idle callback function in which the    */
/* parent checks for new messages in the message queue. The message */
/* queue is read as long as the low marker does not reach the high  */
/* marker, both being incremented in a wrap-around fashion.         */
/* When accepting new data or honoring a REQUEST things may fail,   */
/* most probably by running out of memory. Therefore all the code   */
/* has to be run in a TRY environment and when something fails we   */
/* have to kill the child to prevent it sending further data we     */
/* can't accept anymore. We also need to stop this function being   */
/* an idle callback, and, because end_comm() calls this function a  */
/* last time we also have to set the low and the high marker to the */
/* same value which keeps the function from being executed because  */
/* the child is now already dead and can't change the high marker   */
/* anymore.                                                         */
/*------------------------------------------------------------------*/

int new_data_callback( XEvent *a, void *b )
{
	a = a;
	b = b;

	while ( Comm.MQ->low != Comm.MQ->high )
		TRY
		{
			if ( Comm.MQ->slot[ Comm.MQ->low ].type == REQUEST )
			{
				Comm.MQ->low = ( Comm.MQ->low + 1 ) % QUEUE_SIZE;
				sema_post( Comm.data_semaphore );
				sema_post( Comm.request_semaphore );
				if ( ! reader( NULL ) )
					THROW( EXCEPTION );
			}
			else
				accept_new_data( );

			TRY_SUCCESS;
		}
		OTHERWISE
		{
			/* Before killing the child test that its pid hasn't already been
			   set to 0 (in which case we would commit suicide) and that it's
			   still alive. The death of the child and all the necessary
			   cleaning up is dealt with by the SIGCHLD handlers in run.c, so
			   no need to worry about it here. */

			if ( Internals.child_pid > 0 && ! kill( Internals.child_pid, 0 ) )
				kill( Internals.child_pid, SIGTERM );

			fl_set_idle_callback( 0, NULL );
			Comm.MQ->low = Comm.MQ->high;
		}

	return 0;
}


/*-------------------------------------------------------------*/
/* Called whenever the child needs to send data to the parent. */
/*-------------------------------------------------------------*/

void send_data( int type, int shm_id )
{
	/* Wait until parent can accept more data */

	sema_wait( Comm.data_semaphore );

	/* Put type of data (DATA or REQUEST) into type field of next free slot */

	Comm.MQ->slot[ Comm.MQ->high ].type = type;

	/* For DATA pass parent the ID of shared memory segment with the data */

	if ( type == DATA )
		Comm.MQ->slot[ Comm.MQ->high ].shm_id = shm_id;

	/* Increment the high mark pointer (wraps around) */

	Comm.MQ->high = ( Comm.MQ->high + 1 ) % QUEUE_SIZE;

	/* For REQUESTS also wait for the request semaphore (no more than one
	   REQUEST at a time!) */

	if ( type == REQUEST )
		sema_wait( Comm.request_semaphore );
}


/*----------------------------------------------------------------------*/
/* This function handles the reading from the pipe for both the parent  */
/* and the child process. In each case a message is started by a header */
/* that contains at least information about the type of message.        */
/*----------------------------------------------------------------------*/

bool reader( void *ret )
{
	CommStruct header;


	/* Get the header - failure indicates that the child is dead */

	if ( ! pipe_read( ( char * ) &header, sizeof header ) )
		return FAIL;

	return Internals.I_am == PARENT ?
		   parent_reader( &header ) : child_reader( ret, &header );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool parent_reader( CommStruct *header )
{
	char *str[ 4 ] = { NULL, NULL, NULL, NULL };
	int i;
	int n1, n2;
	void *data = NULL;


	switch ( header->type )
	{
		case C_EPRINT :
			/* Get the string to be printed... */

			TRY
			{
				if ( header->data.str_len[ 0 ] > 0 )
				{
					str[ 0 ] = T_malloc( ( size_t ) header->data.str_len[ 0 ]
										 + 1 );
					if ( ! pipe_read( str[ 0 ],
									  ( size_t ) header->data.str_len[ 0 ] ) )
						THROW( EXCEPTION );
					str[ 0 ][ header->data.str_len[ 0 ] ] = '\0';
				}
				else if ( header->data.str_len[ 0 ] == 0 )
					str[ 0 ] = T_strdup( "" );

				eprint( NO_ERROR, UNSET, "%s", str[ 0 ] );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( str[ 0 ] );
				return FAIL;
			}

			/* Get rid of the string and return */

			str[ 0 ] = T_free( str[ 0 ] );

			break;

		case C_SHOW_MESSAGE :
			str[ 0 ] = NULL;

			TRY
			{
				if ( header->data.str_len[ 0 ] > 0 )
				{
					str[ 0 ] = T_malloc( ( size_t ) header->data.str_len[ 0 ]
										 + 1 );
					if ( ! pipe_read( str[ 0 ],
									  ( size_t ) header->data.str_len[ 0 ] ) )
						THROW( EXCEPTION );
					str[ 0 ][ header->data.str_len[ 0 ] ] = '\0';
				}
				else if ( header->data.str_len[ 0 ] == 0 )
					str[ 0 ] = T_strdup( "" );

				fl_show_messages( str[ 0 ] );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( str[ 0 ] );
				return FAIL;
			}

			str[ 0 ] = T_free( str[ 0 ] );

			/* Send back an acknowledgement that the message has been read
			   by the user */

			return writer( C_ACK );

		case C_SHOW_ALERT :
			TRY
			{
				if ( header->data.str_len[ 0 ] > 0 )
				{
					str[ 0 ] = T_malloc( ( size_t ) header->data.str_len[ 0 ]
										 + 1 );
					if ( ! pipe_read( str[ 0 ],
									  ( size_t ) header->data.str_len[ 0 ] ) )
						THROW( EXCEPTION );
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
				return FAIL;
			}

			str[ 0 ] = T_free( str[ 0 ] );

			/* Send back an acknowledgement that alert has been read by the
			   user */

			return writer( C_ACK );

		case C_SHOW_CHOICES :
			/* Get number of buttons and number of default button */

			if ( ! pipe_read( ( char * ) &n1, sizeof( int ) ) || 
				 ! pipe_read( ( char * ) &n2, sizeof( int ) ) )
				return FAIL;

			/* Get message text and button labels */

			TRY
			{
				for ( i = 0; i < 4; i++ )
				{
					if ( header->data.str_len[ i ] > 0 )
					{
						str[ i ] =
						  T_malloc( ( size_t ) header->data.str_len[ i ] + 1 );
						if ( ! pipe_read( str[ i ],
									   ( size_t ) header->data.str_len[ i ] ) )
							THROW( EXCEPTION );
						str[ i ][ header->data.str_len[ i ] ] = '\0';
					}
					else if ( header->data.str_len[ i ] == 0 )
						str[ i ] = T_strdup( "" );
				}

				/* Show the question, get the button number and pass it back to
				   the child process (which does a read in the mean time) */

				if ( ! writer( C_INT, show_choices( str[ 0 ], n1, str[ 1 ],
												   str[ 2 ], str[ 3 ], n2 ) ) )
					THROW( EXCEPTION );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				for ( i = 0; i < 4 && str[ i ] != NULL; i++ )
					T_free( str[ i ] );
				return FAIL;
			}

			/* Get rid of the strings and return */

			for ( i = 0; i < 4; i++ )
				str[ i ] = T_free( str[ i ] );
			break;

		case C_SHOW_FSELECTOR :
			TRY
			{
				for ( i = 0; i < 4; i++ )
				{
					if ( header->data.str_len[ i ] > 0 )
					{
						str[ i ] =
						  T_malloc( ( size_t ) header->data.str_len[ i ] + 1 );
						if ( ! pipe_read( str[ i ],
									   ( size_t ) header->data.str_len[ i ] ) )
							THROW( EXCEPTION );
						str[ i ][ header->data.str_len[ i ] ] = '\0';
					}
					else if ( header->data.str_len[ i ] == 0 )
						str[ i ] = T_strdup( "" );
				}

				/* Call fl_show_fselector() and send the result back to the
				   child process (which is waiting to read in the mean time) */

				if ( ! writer( C_STR, fl_show_fselector( str[ 0 ], str[ 1 ],
													   str[ 2 ], str[ 3 ] ) ) )
					THROW( EXCEPTION );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				for ( i = 0; i < 4 && str[ i ] != NULL; i++ )
					T_free( str[ i ] );
				return FAIL;
			}

			for ( i = 0; i < 4; i++ )
				str[ i ] = T_free( str[ i ] );
			break;

		case C_PROG : case C_OUTPUT :
				return send_browser( header->type == C_PROG ?
									 GUI.main_form->browser :
									 GUI.main_form->error_browser );

		case C_INPUT :
			TRY
			{
				for ( i = 0; i < 2 ; i++ )
					if ( header->data.str_len[ i ] > 0 )
					{
						str[ i ] =
						  T_malloc( ( size_t ) header->data.str_len[ i ] + 1 );
						if ( ! pipe_read( str[ i ],
									   ( size_t ) header->data.str_len[ i ] ) )
							THROW( EXCEPTION );
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
				for ( i = 0; i < 2 && str[ i ] != NULL; i++ )
					T_free( str[ i ] );
				return FAIL;
			}
			break;

		case C_LAYOUT :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_layout( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_BCREATE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_bcreate( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_BDELETE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_bdelete( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_BSTATE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_bstate( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_SCREATE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_screate( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_SDELETE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_sdelete( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_SSTATE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_sstate( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_ICREATE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_icreate( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_IDELETE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_idelete( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_ISTATE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_istate( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_MCREATE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_mcreate( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_MDELETE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_mdelete( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_MCHOICE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_mchoice( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_ODELETE :
			TRY
			{
				data = T_malloc( ( size_t ) header->data.len );
				if ( ! pipe_read( data, ( size_t ) header->data.len ) )
					THROW( EXCEPTION );
				exp_objdel( data, header->data.len );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( data );
				return FAIL;
			}
			T_free( data );
			break;

		case C_FREEZE :
			parent_freeze( header->data.int_data );
			break;

		default :                     /* this better never gets triggered... */
			fsc2_assert( 1 == 0 );
	}

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool child_reader( void *ret, CommStruct *header )
{
	static char *retstr = NULL;


	switch ( header->type )
	{
		case C_ACK :
			return OK;

		case C_STR :
			if ( header->data.len == -1 )
			{
                if ( ret != NULL )
				     *( ( char ** ) ret ) = NULL;
				return OK;
			}

			T_free( retstr );

			TRY
			{
				retstr = T_malloc( ( size_t ) header->data.len + 1 );
				if ( header->data.len > 0 )
				{
					if ( ! pipe_read( retstr, ( size_t ) header->data.len ) )
						THROW( EXCEPTION );
					retstr[ header->data.len ] = '\0';
				}
				else
					strcpy( retstr, "" );
			}
			OTHERWISE
			{
				T_free( retstr );
				return FAIL;
			}

			if ( ret != NULL )
				*( ( char ** ) ret ) = retstr;
			return OK;

		case C_INT :
			if ( ret != NULL )
				*( ( int * ) ret ) = header->data.int_data;
			return OK;

		case C_LONG :
			if ( ret != NULL )
				*( ( long * ) ret ) = header->data.long_data;
			return OK;

		case C_FLOAT :
			if ( ret != NULL )
				*( ( float * ) ret ) = header->data.float_data;
			return OK;

		case C_DOUBLE :
			if ( ret != NULL )
				*( ( double * ) ret ) = header->data.double_data;
			return OK;

		case C_BCREATE_REPLY : case C_BSTATE_REPLY  :
		case C_SCREATE_REPLY : case C_SSTATE_REPLY  :
		case C_ICREATE_REPLY : case C_ISTATE_REPLY  :
		case C_MCREATE_REPLY : case C_MCHOICE_REPLY :
			if ( ret != NULL )
				return pipe_read( ret, ( size_t ) header->data.len );
			return OK;

		case C_LAYOUT_REPLY  :
		case C_BDELETE_REPLY :
		case C_SDELETE_REPLY :
		case C_IDELETE_REPLY :
		case C_MDELETE_REPLY :
		case C_ODELETE_REPLY :
			return header->data.long_data != 0 ? OK : FAIL;
	}

	fsc2_assert( 1 == 0 );            /* this better never gets triggered... */
	return FAIL;
}


/*-------------------------------------------------------------------------*/
/* This function is the opposite of reader() (see above) - it writes the   */
/* data into the pipe.                                                     */
/*-------------------------------------------------------------------------*/

bool writer( int type, ... )
{
	CommStruct header;
	va_list ap;
	char *str[ 4 ];
	int n1, n2;
	int i;
	void *data;


	/* The child process has to wait for the parent process to become ready to
	   accept and then to write to the pipe. The other way round waiting isn't
	   necessary since the child process is only reading when it actually
	   waits for data. */

	if ( Internals.I_am == CHILD )
		send_data( REQUEST, -1 );

	header.type = type;
	va_start( ap, type );

	switch ( type )
	{
		case C_EPRINT :
			fsc2_assert( Internals.I_am == CHILD );

			str[ 0 ] = va_arg( ap, char * );
			va_end( ap );
			if ( str[ 0 ] == NULL )
				header.data.str_len[ 0 ] = -1;
			else if ( *str[0 ] == '\0' )
				header.data.str_len[ 0 ] = 0;
			else
				header.data.str_len[ 0 ] = strlen( str[ 0 ] );

			if ( ! pipe_write( ( char * ) &header, sizeof header ) )
				return FAIL;

			if ( header.data.str_len[ 0 ] > 0 &&
				 ! pipe_write( str[ 0 ], ( size_t ) header.data.len ) )
				return FAIL;
			break;

		case C_SHOW_MESSAGE :
			fsc2_assert( Internals.I_am == CHILD );

			str[ 0 ] = va_arg( ap, char * );
			va_end( ap );
			if ( str[ 0 ] == NULL )
				header.data.str_len[ 0 ] = -1;
			else if ( *str[ 0 ] == '\0' )
				header.data.str_len[ 0 ] = 0;
			else
				header.data.str_len[ 0 ] = strlen( str[ 0 ] );

			if ( ! pipe_write( ( char * ) &header, sizeof header ) )
				return FAIL;
			if ( header.data.str_len[ 0 ] > 0 &&
				 ! pipe_write( str[ 0 ], ( size_t ) header.data.len ) )
				return FAIL;
			break;

			/* Wait for some acknowledgement to be sent back */


		case C_SHOW_ALERT :
			fsc2_assert( Internals.I_am == CHILD );

			str[ 0 ] = va_arg( ap, char * );
			va_end( ap );
			if ( str[ 0 ] == NULL )
				header.data.str_len[ 0 ] = -1;
			else if ( *str[ 0 ] == '\0' )
				header.data.str_len[ 0 ] = 0;
			else
				header.data.str_len[ 0 ] = strlen( str[ 0 ] );

			if ( ! pipe_write( ( char * ) &header, sizeof header ) )
				return FAIL;
				 
			if ( header.data.str_len[ 0 ] > 0 &&
				 ! pipe_write( str[ 0 ], ( size_t ) header.data.len ) )
				return FAIL;
			break;

		case C_SHOW_CHOICES :
			fsc2_assert( Internals.I_am == CHILD );

			str[ 0 ] = va_arg( ap, char * );
			va_end( ap );
			if ( str[ 0 ] == NULL )
				header.data.str_len[ 0 ] = -1;
			else if ( *str[ 0 ] == '\0' )
				header.data.str_len[ 0 ] = 0;
			else
				header.data.str_len[ 0 ] = strlen( str[ 0 ] );

			n1 = va_arg( ap, int );

			for ( i = 1; i < 4; i++ )
			{
				str[ i ] = va_arg( ap, char * );
				if ( str[ i ] == NULL )
					header.data.str_len[ i ] = -1;
				else if ( *str[ i ] == '\0' )
					header.data.str_len[ i ] = 0;
				else
					header.data.str_len[ i ] = strlen( str[ i ] );
			}

			n2 = va_arg( ap, int );
			va_end( ap );

			if ( ! pipe_write( ( char * ) &header, sizeof header ) ||
				 ! pipe_write( ( char * ) &n1, sizeof( int ) ) ||
				 ! pipe_write( ( char * ) &n2, sizeof( int ) ) )
				return FAIL;

			for ( i = 0; i < 4; i++ )
				if ( header.data.str_len[ i ] > 0 )
					if ( ! pipe_write( str[ i ],
									   ( size_t ) header.data.str_len[ i ] ) )
						return FAIL;
			break;

		case C_SHOW_FSELECTOR :
			fsc2_assert( Internals.I_am == CHILD );

			/* Set up header and write it */

			for ( i = 0; i < 4; i++ )
			{
				str[ i ] = va_arg( ap, char * );
				va_end( ap );
				if ( str[ i ] == NULL )
					header.data.str_len[ i ] = -1;
				else if ( *str[ i ] == '\0' )
					header.data.str_len[ i ] = 0;
				else
					header.data.str_len[ i ] = strlen( str[ i ] );
			}

			if ( ! pipe_write( ( char * ) &header, sizeof header ) )
				return FAIL;

			/* write out all four strings */

			for ( i = 0; i < 4; i++ )
			{
				if ( header.data.str_len[ i ] > 0 )
					if ( ! pipe_write( str[ i ],
									   ( size_t ) header.data.str_len[ i ] ) )
						return FAIL;
			}

			break;

		case C_PROG : case C_OUTPUT :
			fsc2_assert( Internals.I_am == CHILD );
			va_end( ap );
			return pipe_write( ( char * ) &header, sizeof header );

		case C_INPUT :
			fsc2_assert( Internals.I_am == CHILD );

			/* Set up the two argument strings */

			for ( i = 0; i < 2; i++ )
			{
				str[ i ] = va_arg( ap, char * );
				if ( str[ i ] == NULL )
					header.data.str_len[ i ] = -1;
				else if ( *str[ i ] == '\0' )
					header.data.str_len[ i ] = 0;
				else
					header.data.str_len[ i ] = strlen( str[ i ] );
			}
			va_end( ap );

			/* Send header and the two strings */

			if ( ! pipe_write( ( char * ) &header, sizeof header ) )
				return FAIL;
			for ( i = 0; i < 2; i++ )
				if ( header.data.str_len[ i ] > 0 &&
					 ! pipe_write( str[ i ],
								   ( size_t ) header.data.str_len[ i ] ) )
					return FAIL;
			break;

		case C_LAYOUT  : case C_BCREATE :
		case C_BDELETE : case C_BSTATE :
		case C_SCREATE : case C_SDELETE : case C_SSTATE :
		case C_ICREATE : case C_IDELETE : case C_ISTATE :
		case C_MCREATE : case C_MDELETE : case C_MCHOICE :
		case C_ODELETE :
			fsc2_assert( Internals.I_am == CHILD );

			header.data.len = va_arg( ap, ptrdiff_t );
			if ( ! pipe_write( ( char * ) &header, sizeof header ) )
			{
				va_end( ap );
				return FAIL;
			}
			data = va_arg( ap, void * );
			va_end( ap );
			return pipe_write( data, ( size_t ) header.data.len );

		case C_FREEZE :
			fsc2_assert( Internals.I_am == CHILD );
			header.data.int_data = va_arg( ap, int );
			va_end( ap );
			return pipe_write( ( char * ) &header, sizeof header );

		case C_ACK :
			fsc2_assert( Internals.I_am == PARENT );
			va_end( ap );
			return pipe_write( ( char * ) &header, sizeof header );

		case C_BCREATE_REPLY : case C_BSTATE_REPLY :
		case C_SCREATE_REPLY : case C_SSTATE_REPLY :
		case C_ICREATE_REPLY : case C_ISTATE_REPLY :
		case C_MCREATE_REPLY : case C_MCHOICE_REPLY :
			fsc2_assert( Internals.I_am == PARENT );

			header.data.len = va_arg( ap, ptrdiff_t );

			/* Don't try to continue writing on EPIPE (SIGPIPE is ignored) */

			if ( ! pipe_write( ( char * ) &header, sizeof header ) &&
				 errno == EPIPE )
			{
				va_end( ap );
				return FAIL;
			}

			data = va_arg( ap, void * );
			va_end( ap );
			return pipe_write( data, ( size_t ) header.data.len );

		case C_LAYOUT_REPLY  :
		case C_BDELETE_REPLY :
		case C_SDELETE_REPLY :
		case C_IDELETE_REPLY :
		case C_MDELETE_REPLY :
		case C_ODELETE_REPLY :
			fsc2_assert( Internals.I_am == PARENT );

			header.data.long_data = va_arg( ap, long );
			va_end( ap );
			return pipe_write( ( char * ) &header, sizeof header );

		case C_STR :
			fsc2_assert( Internals.I_am == PARENT );

			str[ 0 ] = va_arg( ap, char * );
			va_end( ap );
			if ( str[ 0 ] == NULL )
				header.data.len = -1;
			else if ( *str[ 0 ] == '\0' )
				header.data.len = 0;
			else
				header.data.len = ( ptrdiff_t ) strlen( str[ 0 ] );
			va_end( ap );

			/* Don't try to continue writing on EPIPE (SIGPIPE is ignored) */

			if ( ! pipe_write( ( char * ) &header, sizeof header ) &&
				 errno == EPIPE )
				return FAIL;

			if ( header.data.len > 0 &&
				 ! pipe_write( str[ 0 ], ( size_t ) header.data.len ) )
				return FAIL;
			break;

		case C_INT :
			fsc2_assert( Internals.I_am == PARENT );
			header.data.int_data = va_arg( ap, int );
			va_end( ap );
			return pipe_write( ( char * ) &header, sizeof header );

		case C_LONG :
			fsc2_assert( Internals.I_am == PARENT );
			header.data.long_data = va_arg( ap, long );
			va_end( ap );
			return pipe_write( ( char * ) &header, sizeof header );

		case C_FLOAT :
			fsc2_assert( Internals.I_am == PARENT );
			header.data.float_data = va_arg( ap, float );
			va_end( ap );
			return pipe_write( ( char * ) &header, sizeof header );

		case C_DOUBLE :
			fsc2_assert( Internals.I_am == PARENT );
			header.data.double_data = va_arg( ap, double );
			va_end( ap );
			return pipe_write( ( char * ) &header, sizeof header );

		default :                     /* this should never be reached... */
			fsc2_assert( 1 == 0 );
	}

	return OK;
}


/*------------------------------------------------------------------*/
/* Sends all lines of one of the browsers successively to the child */
/*------------------------------------------------------------------*/

static bool send_browser( FL_OBJECT *browser )
{
	const char *line;
	int i = 0;


	while ( ( line = fl_get_browser_line( browser, ++i ) ) != NULL )
		if ( ! writer( C_STR, line ) )
			return FAIL;
	return writer( C_STR, NULL );
}


/*--------------------------------------------------------------*/
/* Functions reads from the pipe in chunks of the maximum size. */
/*                                                              */
/* ->                                                           */
/*    1. Pointer to buffer with data                            */
/*    2. Number of bytes to be read                             */
/*--------------------------------------------------------------*/

static bool pipe_read( char *buf, size_t bytes_to_read )
{
	long bytes_read;
	long already_read = 0;
	sigset_t new_mask, old_mask;


	/* From man 2 read(): POSIX allows a read that is interrupted after
	   reading some data to return -1 (with errno set to EINTR) or to return
	   the number of bytes already read.  The latter happens on newer Linux
	   system with e.g. kernel 2.2.12 while on older systems, e.g. 2.0.36,
	   -1 is returned when the read is interrupted. Blocking all expected
	   signals while reading and using this rather lengthy loop tries to get
	   it right in both cases. */


	sigemptyset( &new_mask );
	if ( Internals.I_am == CHILD )
		sigaddset( &new_mask, DO_QUIT );
	else
		sigaddset( &new_mask, QUITTING );

	sigprocmask( SIG_BLOCK, &new_mask, &old_mask );

	while ( bytes_to_read > 0 )
	{
		bytes_read = read( Comm.pd[ READ ], buf + already_read,
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
	return bytes_to_read == 0 ? OK : FAIL;
}


/*--------------------------------------------------------------*/
/* Functions writes to the pipe in chunks of the maximum size.  */
/*                                                              */
/* ->                                                           */
/*    1. Pointer to buffer for storing the data                 */
/*    2. Number of bytes to be written                          */
/*--------------------------------------------------------------*/

static bool pipe_write( char *buf, size_t bytes_to_write )
{
	long bytes_written;
	long already_written = 0;
	sigset_t new_mask, old_mask;


	sigemptyset( &new_mask );
	if ( Internals.I_am == CHILD )
		sigaddset( &new_mask, DO_QUIT );
	else
		sigaddset( &new_mask, QUITTING );

	sigprocmask( SIG_BLOCK, &new_mask, &old_mask );

	while ( bytes_to_write > 0 )
	{
		bytes_written = write( Comm.pd[ WRITE ], buf + already_written,
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
	return bytes_to_write == 0 ? OK : FAIL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
