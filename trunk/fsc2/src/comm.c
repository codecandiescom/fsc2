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


/* 
   The main problem with the communication between the parent and the child
   process is that can be situations where the child has new data it needs
   to pass to the parent before it can continue while the parent process is
   too busy to accept the data immediately. To avoid (or at least to reduce
   the impact of the problem) pointers to the data are stored in a buffer
   that can be written to by the child also when the parent is busy and that
   the parent reads out when it has time.

   This buffer, the message queue, is a basically a ring buffer with slots
   into which the type of the data and a pointer to a shared memory segment,
   containing the data, are written by the child process. Beside, there are
   two pointers to the slots of the message queue, one pointer, updated by the
   child only, which indicats the last slot written into by the child, and a
   second which points to the next slot that the parent process has to remove
   from the message queue and which is updated by the parent process. When
   both pointers are identical the message queue is empty.

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
   `internal communication error'.

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

static long parent_reader( CommStruct *header );
static long child_reader( void *ret, CommStruct *header );
static bool pipe_read( int fd, char *buf, size_t bytes_to_read );
static void send_browser( FL_OBJECT *browser );


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

    if ( ( pr = pipe( pd ) ) < 0 || pipe( pd + 2 ) < 0 )      /* open pipes */
	{
		if ( pr == 0 )                    /* if first open did succeed */
		{
			close( pd[ 0 ] );
			close( pd[ 1 ] );
		}

		eprint( FATAL, UNSET, "Can't set up internal communication "
				"channels.\n" );
		THROW( EXCEPTION );
	}

	if ( ( MQ = ( MESSAGE_QUEUE * ) get_shm( &MQ_ID, sizeof *MQ ) )
		 == ( MESSAGE_QUEUE * ) -1 )
	{
		MQ_ID = -1;
		for ( i = 0; i < 4; i++ )
			close( pd[ i ] );

		eprint( FATAL, UNSET, "Can't set up internal communication "
				"channels.\n" );
		THROW( EXCEPTION );
	}

	MQ->low = MQ->high = 0;
	for ( i = 0; i < QUEUE_SIZE; i++ )
		MQ->slot[ i ].shm_id = -1;

	/* Beside the pipes we need a semaphore which allows the parent to control
	   when the child is allowed to send data and messages. Its size has to be
	   identcal to the message queue size to avoid having the child send more
	   messages than queue can hold. */

	if ( ( data_semaphore = sema_create( QUEUE_SIZE ) ) < 0 )
	{
		delete_all_shm( );
		for ( i = 0; i < 4; i++ )
			close( pd[ i ] );

		eprint( FATAL, UNSET, "Can't set up internal communication "
				"channels.\n" );
		THROW( EXCEPTION );
	}

	if ( ( request_semaphore = sema_create( 0 ) ) < 0 )
	{
		sema_destroy( data_semaphore );
		data_semaphore = -1;
		delete_all_shm( );
		for ( i = 0; i < 4; i++ )
			close( pd[ i ] );

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

	if ( MQ->low != MQ->high )
		new_data_callback( NULL, 0 );

	/* Get rid of all the shared memory segments */

	delete_all_shm( );

	/* Delete the semaphores */

	sema_destroy( request_semaphore );
	request_semaphore = -1;
	sema_destroy( data_semaphore );
	data_semaphore = -1;

	/* Close parents side of read and write pipe */

	close( pd[ READ ] );
	close( pd[ WRITE ] );
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

	TRY
	{
		while ( MQ->low != MQ->high )
			if ( MQ->slot[ MQ->low ].type == REQUEST )
			{
				MQ->low = ( MQ->low + 1 ) % QUEUE_SIZE;
				sema_post( data_semaphore );
				sema_post( request_semaphore );
				reader( NULL );
			}
			else
				accept_new_data( );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		/* Before killing the child test that its pid hasn't already been set
		   to 0 (in which case we would commit suicide) and that it's still
		   alive. The death of the child and all the necessary cleaning up is
		   dealt with by the SIGCHLD handlers in run.c, so no need to worry
		   about it here. */

		if ( child_pid > 0 && ! kill( child_pid, 0 ) )
			kill( child_pid, SIGTERM );

		fl_set_idle_callback( 0, NULL );
		MQ->low = MQ->high;
	}

	return 0;
}


/*----------------------------------------------------------------------*/
/* This function handles the reading from the pipe for both the parent  */
/* and the child process. In each case a message is started by a header */
/* that contains at least information about the type of message. Many   */
/* of the messages can only be read by the parent. These are:           */
/* C_EPRINT, C_SHOW_MESSAGE, C_SHOW_ALERT, C_SHOW_CHOICES,              */
/* C_SHOW_FSELECTOR, C_PROG and C_OUTPUT.                               */
/* These are the implemented requests.                                  */
/* The remaining types are used for passing the replies to the request  */
/* back to the child process. These are the only ones where a return    */
/* value is be needed. Since the child child itself triggered the reply */
/* by its request it also knows what type of return it has to expect    */
/* and thus has to pass a pointer to a variable of the appropriate type */
/* (cast to void *) in which the return value will be stored. For       */
/* strings the calling routine has to pass a pointer to a character     */
/* pointer, in this pointer a pointer to the return string is stored    */
/* and the string remains usable until the next call of reader().       */
/*                                                                      */
/* On C_PROG and C_OUTPUT the parent sends the child all lines (as      */
/* C_STR) from the main browser or the error browser, respectively,     */
/* followed by a NULL string as end marker.                             */
/*----------------------------------------------------------------------*/

long reader( void *ret )
{
	CommStruct header;


	/* Get the header - failure indicates that the child is dead */

	if ( ! pipe_read( pd[ READ ], ( char * ) &header, sizeof( CommStruct ) ) )
		return 0;

	return I_am == PARENT ?
		   parent_reader( &header ) : child_reader( ret, &header );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static long parent_reader( CommStruct *header )
{
	char *str[ 4 ] = { NULL, NULL, NULL, NULL };
	int i;
	int n1, n2;
	void *data;


	switch ( header->type )
	{
		case C_EPRINT :          
			/* Get the string to be printed... */

			if ( header->data.str_len[ 0 ] > 0 )
			{
				str[ 0 ] = T_malloc( ( size_t ) header->data.str_len[ 0 ]
									 + 1 );
				pipe_read( pd[ READ ], str[ 0 ],
						   ( size_t ) header->data.str_len[ 0 ] );
				str[ 0 ][ header->data.str_len[ 0 ] ] = '\0';
			}
			else if ( header->data.str_len[ 0 ] == 0 )
				str[ 0 ] = T_strdup( "" );
			else
				str[ 0 ] = NULL;

			/* ...and print it via eprint() */

			eprint( NO_ERROR, UNSET, "%s", str[ 0 ] );

			/* Get rid of the string and return */

			str[ 0 ] = T_free( str[ 0 ] );

			break;

		case C_SHOW_MESSAGE :
			if ( header->data.str_len[ 0 ] > 0 )
			{
				str[ 0 ] = T_malloc( ( size_t ) header->data.str_len[ 0 ]
									 + 1 );
				pipe_read( pd[ READ ], str[ 0 ],
						   ( size_t ) header->data.str_len[ 0 ] );
				str[ 0 ][ header->data.str_len[ 0 ] ] = '\0';
			}
			else if ( header->data.str_len[ 0 ] == 0 )
				str[ 0 ] = T_strdup( "" );
			else
				str[ 0 ] = NULL;

			fl_show_messages( str[ 0 ] );

			/* Get rid of the string  */

			str[ 0 ] = T_free( str[ 0 ] );

			/* Send back just one character as indicator that the message has
			   been read by the user */

			write( pd[ WRITE ], "X", 1 );
			break;

		case C_SHOW_ALERT :
			if ( header->data.str_len[ 0 ] > 0 )
			{
				str[ 0 ] = T_malloc( ( size_t ) header->data.str_len[ 0 ]
									 + 1 );
				pipe_read( pd[ READ ], str[ 0 ],
						   ( size_t ) header->data.str_len[ 0 ] );
				str[ 0 ][ header->data.str_len[ 0 ] ] = '\0';
			}
			else if ( header->data.str_len[ 0 ] == 0 )
				str[ 0 ] = T_strdup( "" );
			else
				str[ 0 ] = NULL;

			/* Get rid of the string */

			str[ 0 ] = T_free( str[ 0 ] );

			/* Send back just one character as indicator that the alert has
			   been acknowledged by the user */

			write( pd[ WRITE ], "X", 1 );
			break;

		case C_SHOW_CHOICES :
			/* Get number of buttons and number of default button */

			pipe_read( pd[ READ ], ( char * ) &n1, sizeof( int ) );
			pipe_read( pd[ READ ], ( char * ) &n2, sizeof( int ) );

			/* Get message text and button labels */

			for ( i = 0; i < 4; i++ )
			{
				if ( header->data.str_len[ i ] > 0 )
				{
					str[ i ] =
						  T_malloc( ( size_t ) header->data.str_len[ i ] + 1 );
					pipe_read( pd[ READ ], str[ i ],
							   ( size_t ) header->data.str_len[ i ] );
					str[ i ][ header->data.str_len[ i ] ] = '\0';
				}
				else if ( header->data.str_len[ i ] == 0 )
					str[ i ] = T_strdup( "" );
				else
					str[ i ] = NULL;
			}

			/* Show the question, get the button number and pass it back to
			   the child process (which does a read in the mean time) */

			writer( C_INT, show_choices( str[ 0 ], n1,
										 str[ 1 ], str[ 2 ], str[ 3 ], n2 ) );

			/* Get rid of the strings and return */

			for ( i = 0; i < 4; i++ )
				str[ i ] = T_free( str[ i ] );
			break;

		case C_SHOW_FSELECTOR :
			/* Get the 4 parameter strings */

			for ( i = 0; i < 4; i++ )
			{
				if ( header->data.str_len[ i ] > 0 )
				{
					str[ i ] =
						  T_malloc( ( size_t ) header->data.str_len[ i ] + 1 );
					pipe_read( pd[ READ ], str[ i ],
							   ( size_t ) header->data.str_len[ i ] );
					str[ i ][ header->data.str_len[ i ] ] = '\0';
				}
				else if ( header->data.str_len[ i ] == 0 )
					str[ i ] = T_strdup( "" );
				else
					str[ i ] = NULL;
			}

			/* Call fl_show_fselector() and send the result back to the child
			   process (which is waiting to read in the mean time) */

			writer( C_STR, fl_show_fselector( str[ 0 ], str[ 1 ],
											  str[ 2 ], str[ 3 ] ) );

			/* Get rid of parameter strings and return */

			for ( i = 0; i < 4; i++ )
				str[ i ] = T_free( str[ i ] );
			break;

		case C_PROG : case C_OUTPUT :
			send_browser( header->type == C_PROG ?
						  main_form->browser : main_form->error_browser );
			break;

		case C_INPUT :
			/* Get length of predefined content and label from header and 
			   read them */

			for ( i = 0; i < 2 ; i++ )
				if ( header->data.str_len[ i ] > 0 )
				{
					str[ i ] =
						  T_malloc( ( size_t ) header->data.str_len[ i ] + 1 );
					pipe_read( pd[ READ ], str[ i ],
							   ( size_t ) header->data.str_len[ i ] );
					str[ i ][ header->data.str_len[ i ] ] = '\0';
				}
				else if ( header->data.str_len[ i ] == 0 )
					str[ i ] = T_strdup( "" );
				else
					str[ i ] = NULL;

			/* Send string from input form to child */

			writer( C_STR, show_input( str[ 0 ], str[ 1 ] ) );
			break;

		case C_LAYOUT :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_layout( data, header->data.len );
			T_free( data );
			break;

		case C_BCREATE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_bcreate( data, header->data.len );
			T_free( data );
			break;

		case C_BDELETE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_bdelete( data, header->data.len );
			T_free( data );
			break;

		case C_BSTATE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_bstate( data, header->data.len );
			T_free( data );
			break;

		case C_SCREATE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_screate( data, header->data.len );
			T_free( data );
			break;

		case C_SDELETE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_sdelete( data, header->data.len );
			T_free( data );
			break;

		case C_SSTATE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_sstate( data, header->data.len );
			T_free( data );
			break;

		case C_ICREATE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_icreate( data, header->data.len );
			T_free( data );
			break;

		case C_IDELETE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_idelete( data, header->data.len );
			T_free( data );
			break;

		case C_ISTATE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_istate( data, header->data.len );
			T_free( data );
			break;

		case C_ODELETE :
			data = T_malloc( ( size_t ) header->data.len );
			pipe_read( pd[ READ ], data, ( size_t ) header->data.len );
			exp_objdel( data, header->data.len );
			T_free( data );
			break;

		case C_FREEZE :
			parent_freeze( header->data.int_data );
			break;

		default :                     /* this better never gets triggered... */
			fsc2_assert( 1 == 0 );
	}

	return 0;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static long child_reader( void *ret, CommStruct *header )
{
	static char *retstr = NULL;


	switch ( header->type )
	{
		case C_STR :
			if ( header->data.len == -1 )
			{
                if ( ret != NULL ) 
				     *( ( char ** ) ret ) = NULL;
				return 1;
			}

			T_free( retstr );

			retstr = T_malloc( ( size_t ) header->data.len + 1 );
            if ( header->data.len > 0 )
			{
				pipe_read( pd[ READ ], retstr, ( size_t ) header->data.len );
				retstr[ header->data.len ] = '\0';
			}
			else
				strcpy( retstr, "" );

			if ( ret != NULL )
				*( ( char ** ) ret ) = retstr;
			return 1;

		case C_INT :
			if ( ret != NULL )
				*( ( int * ) ret ) = header->data.int_data;
			return 1;

		case C_LONG :
			if ( ret != NULL )
				*( ( long * ) ret ) = header->data.long_data;
			return 1;

		case C_FLOAT :
			if ( ret != NULL )
				*( ( float * ) ret ) = header->data.float_data;
			return 1;

		case C_DOUBLE :
			if ( ret != NULL )
				*( ( double * ) ret ) = header->data.double_data;
			return 1;

		case C_BCREATE_REPLY :
		case C_SCREATE_REPLY : case C_SSTATE_REPLY :
		case C_ICREATE_REPLY : case C_ISTATE_REPLY :
			pipe_read( pd[ READ ], ret, ( size_t ) header->data.len );
			return 0;

		case C_LAYOUT_REPLY : case C_BDELETE_REPLY : case C_BSTATE_REPLY :
		case C_SDELETE_REPLY :
		case C_IDELETE_REPLY :
		case C_ODELETE_REPLY :
			return header->data.long_data;
	}

	fsc2_assert( 1 == 0 );            /* this better never gets triggered... */
	return 0;
}


/*--------------------------------------------------------------*/
/* Functions reads from the pipe in chunks of the maximum size. */
/*                                                              */
/* ->                                                           */
/*    1. File descriptor of read end of pipe                    */
/*    2. Buffer for data                                        */
/*    3. Number of bytes to be read                             */
/*--------------------------------------------------------------*/

static bool pipe_read( int fd, char *buf, size_t bytes_to_read )
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
	if ( I_am == CHILD )
		sigaddset( &new_mask, DO_QUIT );
	else
		sigaddset( &new_mask, QUITTING );

	sigprocmask( SIG_BLOCK, &new_mask, &old_mask );

	while ( bytes_to_read > 0 )
	{
		bytes_read = read( fd, buf + already_read, bytes_to_read );

		/* If child received a deadly signal... */

		if ( I_am == CHILD && do_quit )
			break;

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


/*-------------------------------------------------------------------------*/
/* This function is the opposite of reader() (see above) - it writes the   */
/* stuff to the pipe. The kind of data to be written for the different     */
/* `types' are given hear:                                                 */
/* C_EPRINT         : string (char *) to be output                         */
/* C_SHOW_MESSAGE   : message string (char *) to be shown (parameter as in */
/*                    fl_show_messages())                                  */
/* C_SHOW_ALERT     : alert string (char *) to be shown                    */
/* C_SHOW_CHOICES   : question string (char *), number of buttons (1-3)    */
/*                    (3 int), 3 strings with texts for buttons (char *),  */
/*                    number of default button (int) (parameter as in      */
/*                    fl_show_choices())                                   */
/* C_SHOW_FSELECTOR : 4 strings (4 char *) with identical meaning as the   */
/*                    parameter for fl_show_fselector()                    */
/* C_PROG, C_OUTPUT : None at all                                          */
/* C_STR            : string (char *)                                      */
/* C_INT            : integer data (int)                                   */
/* C_LONG,          : long integer data (long)                             */
/* C_FLOAT          : float data (float)                                   */
/* C_DOUBLE         : double data (double)                                 */
/*                                                                         */
/* The types C_EPRINT, C_SHOW_MESSAGE, C_SHOW_ALERT, C_SHOW_CHOICES and    */
/* C_SHOW_FSELECTOR are only to be used by the child process, the other    */
/* only by the parent!                                                     */
/*-------------------------------------------------------------------------*/


void writer( int type, ... )
{
	CommStruct header;
	va_list ap;
	char *str[ 4 ];
	int n1, n2;
	int i;
	char ack;
	void *data;


	/* The child process has to wait for the parent process to become ready to
	   accept and then to write to the pipe. The other way round waiting isn't
	   necessary since the child process is only reading when it actually
	   waits for data. */

	if ( I_am == CHILD )
		send_data( REQUEST, -1 );

	header.type = type;
	va_start( ap, type );

	switch ( type )
	{
		case C_EPRINT :          
			fsc2_assert( I_am == CHILD );

			str[ 0 ] = va_arg( ap, char * );
			if ( str[ 0 ] == NULL )
				header.data.str_len[ 0 ] = -1;
			else if ( *str[0 ] == '\0' )
				header.data.str_len[ 0 ] = 0;
			else
				header.data.str_len[ 0 ] = strlen( str[ 0 ] );

			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			if ( header.data.str_len[ 0 ] > 0 )
				write( pd[ WRITE ], str[ 0 ], ( size_t ) header.data.len );
			break;

		case C_SHOW_MESSAGE :
			fsc2_assert( I_am == CHILD );

			str[ 0 ] = va_arg( ap, char * );
			if ( str[ 0 ] == NULL )
				header.data.str_len[ 0 ] = -1;
			else if ( *str[ 0 ] == '\0' )
				header.data.str_len[ 0 ] = 0;
			else
				header.data.str_len[ 0 ] = strlen( str[ 0 ] );

			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			if ( header.data.str_len[ 0 ] > 0 )
				write( pd[ WRITE ], str[ 0 ], ( size_t ) header.data.len );

			/* wait for random character to be sent back as acknowledgement */

			read( pd[ READ ], &ack, 1 );
			break;

		case C_SHOW_ALERT :
			fsc2_assert( I_am == CHILD );

			str[ 0 ] = va_arg( ap, char * );
			if ( str[ 0 ] == NULL )
				header.data.str_len[ 0 ] = -1;
			else if ( *str[ 0 ] == '\0' )
				header.data.str_len[ 0 ] = 0;
			else
				header.data.str_len[ 0 ] = strlen( str[ 0 ] );

			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			if ( header.data.str_len[ 0 ] > 0 )
				write( pd[ WRITE ], str[ 0 ], ( size_t ) header.data.len );

			/* Wait for a random character to be sent back as acknowledgment */

			pipe_read( pd[ READ ], &ack, 1 );
			break;

		case C_SHOW_CHOICES :
			fsc2_assert( I_am == CHILD );

			str[ 0 ] = va_arg( ap, char * );
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

			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			write( pd[ WRITE ], &n1, sizeof( int ) );
			write( pd[ WRITE ], &n2, sizeof( int ) );

			for ( i = 0; i < 4; i++ )
				if ( header.data.str_len[ i ] > 0 )
					write( pd[ WRITE ], str[ i ],
						   ( size_t ) header.data.str_len[ i ] );
			break;

		case C_SHOW_FSELECTOR :
			fsc2_assert( I_am == CHILD );

			/* Set up header and write it */

			for ( i = 0; i < 4; i++ )
			{
				str[ i ] = va_arg( ap, char * );
				if ( str[ i ] == NULL )
					header.data.str_len[ i ] = -1;
				else if ( *str[ i ] == '\0' )
					header.data.str_len[ i ] = 0;
				else
					header.data.str_len[ i ] = strlen( str[ i ] );
			}

			write( pd[ WRITE ], &header, sizeof( CommStruct ) );

			/* write out all four strings */

			for ( i = 0; i < 4; i++ )
			{
				if ( header.data.str_len[ i ] > 0 )
					write( pd[ WRITE ], str[ i ],
						   ( size_t ) header.data.str_len[ i ] );
			}

			break;

		case C_PROG : case C_OUTPUT :
			fsc2_assert( I_am == CHILD );
			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			break;

		case C_INPUT :
			fsc2_assert( I_am == CHILD );

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

			/* Send header and the two strings */

			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			for ( i = 0; i < 2; i++ )
				if ( header.data.str_len[ i ] > 0 )
					write( pd[ WRITE ], str[ i ],
						   ( size_t ) header.data.str_len[ i ] );
			break;

		case C_LAYOUT : case C_BCREATE :
		case C_BDELETE : case C_BSTATE :
		case C_SCREATE : case C_SDELETE : case C_SSTATE :
		case C_ICREATE : case C_IDELETE : case C_ISTATE :
		case C_ODELETE :
			fsc2_assert( I_am == CHILD );

			header.data.len = va_arg( ap, ptrdiff_t );
			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			data = va_arg( ap, void * );
			write( pd[ WRITE ], data, ( size_t ) header.data.len );
			break;

		case C_FREEZE :
			fsc2_assert( I_am == CHILD );

			header.data.int_data = va_arg( ap, int );
			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			break;

		case C_BCREATE_REPLY :
		case C_SCREATE_REPLY : case C_SSTATE_REPLY :
		case C_ICREATE_REPLY : case C_ISTATE_REPLY :
			fsc2_assert( I_am == PARENT );

			header.data.len = va_arg( ap, ptrdiff_t );

			/* Don't try to continue writing on EPIPE (SIGPIPE is ignored) */

			if ( write( pd[ WRITE ], &header, sizeof( CommStruct ) ) < 0 &&
				 errno == EPIPE )
			{
				va_end( ap );
				return;
			}
			data = va_arg( ap, void * );
			write( pd[ WRITE ], data, ( size_t ) header.data.len );
			break;

		case C_LAYOUT_REPLY : case C_BDELETE_REPLY :
		case C_BSTATE_REPLY : case C_SDELETE_REPLY :
		case C_IDELETE_REPLY :
		case C_ODELETE_REPLY :
			fsc2_assert( I_am == PARENT );
			header.data.long_data = va_arg( ap, long );
			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			break;

		case C_STR :
			fsc2_assert( I_am == PARENT );

			str[ 0 ] = va_arg( ap, char * );
			if ( str[ 0 ] == NULL )
				header.data.len = -1;
			else if ( *str[ 0 ] == '\0' )
				header.data.len = 0;
			else 
				header.data.len = ( ptrdiff_t ) strlen( str[ 0 ] );

			/* Don't try to continue writing on EPIPE (SIGPIPE is ignored) */

			if ( write( pd[ WRITE ], &header, sizeof( CommStruct ) ) < 0 &&
				 errno == EPIPE )
			{
				va_end( ap );
				return;
			}

			if ( header.data.len > 0 )
				write( pd[ WRITE ], str[ 0 ], ( size_t ) header.data.len );
			break;

		case C_INT :
			fsc2_assert( I_am == PARENT );
			header.data.int_data = va_arg( ap, int );
			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			break;

		case C_LONG :
			fsc2_assert( I_am == PARENT );
			header.data.long_data = va_arg( ap, long );
			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			break;

		case C_FLOAT :
			fsc2_assert( I_am == PARENT );
			header.data.float_data = va_arg( ap, float );
			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			break;

		case C_DOUBLE :
			fsc2_assert( I_am == PARENT );
			header.data.double_data = va_arg( ap, double );
			write( pd[ WRITE ], &header, sizeof( CommStruct ) );
			break;

		default :                     /* this should never be reached... */
			fsc2_assert( 1 == 0 );
	}

	va_end( ap );
}


/*------------------------------------------------------------------*/
/* Sends all lines of one of the browsers successively to the child */
/*------------------------------------------------------------------*/

static void send_browser( FL_OBJECT *browser )
{
	const char *line;
	int i = 0;


	while ( ( line = fl_get_browser_line( browser, ++i ) ) != NULL )
		writer( C_STR, line );
	writer( C_STR, NULL );
}


/*-------------------------------------------------------------*/
/* Called whenever the child needs to send data to the parent. */
/*-------------------------------------------------------------*/

void send_data( int type, int shm_id )
{
	/* Wait until parent can accept more data */

	sema_wait( data_semaphore );

	/* Put type of data (DATA or REQUEST) into type field of next free slot */

	MQ->slot[ MQ->high ].type = type;

	/* For DATA pass parent the ID of shared memory segment with the data */

	if ( type == DATA )
		MQ->slot[ MQ->high ].shm_id = shm_id;

	/* Increment the high mark pointer (wraps around) */

	MQ->high = ( MQ->high + 1 ) % QUEUE_SIZE;

	/* For REQUESTS also wait for the request semaphore (no more than one
	   REQUEST at a time!) */

	if ( type == REQUEST )
		sema_wait( request_semaphore );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
