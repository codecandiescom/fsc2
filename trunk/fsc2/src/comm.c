/*
  $Id$
*/


/* 
   There are two types of messages to be exchanged between the child and the
   parent. The first one is data to be displayed by the parent. These should
   be accepted as fast as possible so that the child can continue with the
   measurement immediately. The other type of messages are requests by the
   child for the parent to do some things for it, e.g. display an alert box,
   and possibly return the user selection. The protocoll for requests is
   quite simple - the child sends the request and waits for the answer by
   the parent, knowing exactly what kind of data to expect.

   The first problem to be addressed is how to make the parent aware of data
   send by the child. This is handled by two signals, DO_SEND and NEW_DATA.
   The DO_SEND signal is raised by the parent when it is prepared to accept
   data. The child always has to wait for this signal before it may send any
   data. Then it posts its message and raises the NEW_DATA signal. Now the
   parent has to accept the data and when it is done it again raises the
   DO_SEND signal. Using the DO_SEND signal avoids flooding the parent with
   too many requests and data.

   The request type of messages is easy to implement - a simple pair of
   pipes will do. All needed beside is a protocoll for the differerent types
   of requests. Since requests are not real time critical the parent does
   not have to reply immediately and the child waits until the parent honors
   the request. Thus the way requests are handled by the parent is simple:
   The parent catches the signal and triggers an invisible button which in
   turn leads to the buttons handler function being called by the main loop
   of the program as for every other event. Then the parent reads the pipe
   and replies by sending the answer to the request via another pipe. No
   synchronization is needed since the child will be blocked while reading
   the reply pipe until the parent finishes writing its answer to the pipe.

   The implementation for the exchange of data is a bit more complicated. Here
   the child should not be forced to wait for the parent process to finally
   react to the (self-created) event since this may take quite some time while
   the parent is busy updating the display or is even waiting while the user
   resizes one of the windows. On the other hand, the parent can't store the
   data already in the handler for the NEW_DATA signal, because it would have
   to call malloc() which is, unfortunately, not reentrant. Thus, the
   alternative is to use shared memory segments.

   Thus, when the child process needs to send data to the parent it gets a
   new shared mory segment, copies the data into the segment and then sends
   the parent the identifier of the memory segment. Within the NEW_DATA
   signal handler all the parent does is to store this identifier and then
   it raises the DO_SEND immediately. This way the child can continue with
   the measurement while the parent has time to handle the new data whenever
   it is ready to do so.

   How does the child send memory segment identifiers to the parent? This
   also is done via a shared memory segment created before the child is
   started and not removed before the child exits. All to be stored in this
   memory segment is the memory segment identifier for the segment with the
   data and the type of the message, i.e. DATA or REQUEST. The latter has
   also to be send because the parent otherwise wouldn't know what kind of
   message it is going to receive.

   Thus, in the NEW_DATA signal handler the parent just copies the segment
   identifier and the type flag and, for DATA messages, raises the DO_SEND
   signal. Finally, it creates a synthetic event for the invisible button.
   Where do the segment identifieres get stored? Together with the segment
   for storing the identifiers a queue is created for storing the the
   identifiers and message type flags. The size of the queue can be rather
   limited since there is only a limited amount of shared memory
   segments. Thus this queue to has have only the maximum number of shared
   memory segments plus one entries (the extra entry is for REQUEST messages
   - there can always be only one). Beside the message queue also two
   pointers are needed, on pointing to the oldest unhandled entry and one
   just above the newest one.

   In the handler for the synthetically triggered, invisible button the
   parent runs through all the entries in the message queue starting with
   the oldest ones. For REQUEST type messages he reads the pipe and sends
   its reply. For DATA messages it copies the data to the appropriate places
   and then removes the shared memory segment for euse by the child. Finally,
   it displays the new data.

   This scheme will work as long as the parent doesn't get to much behind with
   handling DATA messages. But if the parent is very busy and the child very
   fast creating (and sending) new data we will finally run out of shared
   memory segments. Thus, if getting a new shared memory segment fails for the
   child it will sleep a short while and then retry, hoping that the parent
   removed one of the previously used segments in the mean time.

   The only problem still unaddressed is data sets exceeding the maximum
   size of a shared memory segment. But this limit seems to be rather high
   (32768 kB !), so I hope this never going to happen...  If it should ever
   hapen this will result in the measurement getting stopped with an
   `internal communication error'.
*/


#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/param.h>

/* locally used routines */

static bool pipe_read( int fd, void *buf, size_t bytes_to_read );

static int Key_Area;


/*----------------------------------------------------------------*/
/* This routine sets up the resources needed for the interprocess */
/* communication. It is called before the child is forked.        */
/*----------------------------------------------------------------*/

bool setup_comm( void )
{
	int pr;
	int i;
	struct shmid_ds shm_buf;


	/* Open pipes for passing data between child and parent process - we need
	   two pipes, one for the parent process to write to the child process and
	   another one for the other way round.*/

    if ( ( pr = pipe( pd ) ) < 0 || pipe( pd + 2 ) < 0 )      /* open pipes */
	{
		if ( pr == 0 )
		{
			close( pd[ 0 ] );
			close( pd[ 1 ] );
		}

		return FAIL;
	}

	/* Beside the pipes we need a shared memory segment to pass the key to
	   further shared memory segments used to send data from the child to the
	   parent */

	Key_Area = shmget( IPC_PRIVATE, 2 * sizeof( int ), IPC_CREAT | 0600 );

	if ( Key_Area < 0 )
	{
		for ( i = 0; i < 4; i++ )
			close( pd[ i ] );
		return FAIL;
	}

	if ( ( Key = ( KEY * ) shmat( Key_Area, NULL, 0 ) ) == ( KEY * ) - 1 )
	{
		for ( i = 0; i < 4; i++ )
			close( pd[ i ] );

		shmctl( Key_Area, IPC_RMID, &shm_buf );
		return FAIL;
	}

	/* Finally we need a message queue where the parent stores the keys and
	   message types it gets from the child for later handling */

	TRY
    {
		Message_Queue = T_malloc( QUEUE_SIZE * sizeof( KEY ) );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		for ( i = 0; i < 4; i++ )
			close( pd[ i ] );

		shmdt( ( void * ) Key );
		shmctl( Key_Area, IPC_RMID, &shm_buf );
		return FAIL;
	}

	message_queue_low = message_queue_high = 0;

	return OK;
}


/*--------------------------------------------------------------*/
/* This routine is called after the child process died to clean */
/* up the resources used in the interprocess communication.     */
/*--------------------------------------------------------------*/

void end_comm( void )
{
	struct shmid_ds shm_buf;


	/* Handle remaining messages */

	if ( message_queue_low != message_queue_high )
		new_data_callback( NULL, 0 );

	/* Deallocate the message queue */

	T_free( Message_Queue );

	/* Detach from and remove the shared memory segment */

	shmdt( ( void * ) Key );
	shmctl( Key_Area, IPC_RMID, &shm_buf );

	/* Close parents side of read and write pipe */

	close( pd[ READ ] );
	close( pd[ WRITE ] );
}


/*-----------------------------------------------------------*/
/* new_data_callback() is responsible for either honoring a  */
/* a REQUEST or storing and displaying DATA from the child.  */
/* Actually, this routine is just a callback function for an */
/* invisible button in the form shown while the experiment   */
/* is running and that is synthetically triggered from the   */
/* parents NEW_DATA signal handler.                          */ 
/* The message queue is read as long as the low marker has   */
/* not reached the high marker, both being incremented in a  */
/* wrap-around fashion.                                      */
/*-----------------------------------------------------------*/

void new_data_callback( FL_OBJECT *a, long b )
{
	struct shmid_ds shm_buf;

	a = a;
	b = b;

	while ( message_queue_low != message_queue_high )
	{
		if ( Message_Queue[ message_queue_low ].type == REQUEST )
			reader( NULL );
		else
		{
			/* Lots of other stuff still to come... */

			/* Finally remove the shared memory segment, we're done with it */

			shmctl( Message_Queue[ message_queue_low ].shm_id,
					IPC_RMID, &shm_buf );
		}

		message_queue_low++;
		message_queue_low %= QUEUE_SIZE;

	}
}


/*----------------------------------------------------------------------*/
/* This function handles the reading from the pipe for both the parent  */
/* and the child process. In each case a message is started by a header */
/* that contains at least information about the type of message. Many   */
/* of the messages can only be read by the parent. These are:           */
/* C_EPRINT, C_SHOW_MESSAGE, C_SHOW_ALERT, C_SHOW_CHOICES,              */
/* C_SHOW_FSELECTOR and C_INIT_GRAPHICS                                 */
/* These are the implemented requests.                                  */
/* The remaining types are used for passing the replies to the request  */
/* back to the child process. THese are also the ones where a return    */
/* value is be needed. Since the child child itself triggered the reply */
/* by its request it also knows what type of return it has to expect    */
/* and thus has to pass a pointer to a variable of the appropriate type */
/* (cast to void *) in which the return value will be stored. For       */
/* strings the calling routine has to pass a pointer to a character     */
/* pointer, in this pointer a pointer to the return string is stored    */
/* and the string remains usuable until the next call of reader().      */
/*----------------------------------------------------------------------*/

long reader( void *ret )
{
	CS header;
	char *str[ 4 ];
	long dim;
	int i;
	int n1, n2;
	long nx, ny;
	long retval;
	static char *retstr = NULL;


	/* Get the header - failure indicates that the child is dead */

	if ( ! pipe_read( pd[ READ ], &header, sizeof( CS ) ) )
		return 0;

	switch ( header.type )
	{
		case C_EPRINT :          
			assert( I_am == PARENT );       /* only to be read by the parent */

			/* get the string to be printed... */

			str[ 0 ] = get_string( header.data.len );
			pipe_read( pd[ READ ], str[ 0 ], header.data.len );
			str[ 0 ][ header.data.len ] = '\0';

			/* ...and print it via eprint() */

			eprint( NO_ERROR, "%s", str[ 0 ] );

			/* get rid of the string and return */

			T_free( str[ 0 ] );
			retval = 0;
			break;

		case C_SHOW_MESSAGE :
			assert( I_am == PARENT );       /* only to be read by the parent */

			str[ 0 ] = get_string( header.data.len );
			pipe_read( pd[ READ ], str[ 0 ], header.data.len );
			str[ 0 ] [ header.data.len ] = '\0';
			fl_show_messages( str[ 0 ] );

			/* send back just one character as indicator that the message has
			   been read by the user */

			write( pd[ WRITE ], str[ 0 ], sizeof( char ) );
			T_free( str[ 0 ] );
			retval = 0;
			break;

		case C_SHOW_ALERT :
			assert( I_am == PARENT );       /* only to be read by the parent */

			str[ 0 ] = get_string( header.data.len );
			pipe_read( pd[ READ ], str[ 0 ], header.data.len );
			show_alert( str[ 0 ] );

			/* send back just one character as indicator that the alert has
			   been read by te user */

			write( pd[ WRITE ], str[ 0 ], sizeof( char ) );

			T_free( str[ 0 ] );
			retval = 0;
			break;

		case C_SHOW_CHOICES :
			assert( I_am == PARENT );       /* only to be read by the parent */

			/* get number of buttons and number of default button */

			pipe_read( pd[ READ ], &n1, sizeof( int ) );
			pipe_read( pd[ READ ], &n2, sizeof( int ) );

			/* get message text and button labels */

			for ( i = 0; i < 4; i++ )
			{
				if ( header.data.str_len[ i ] != 0 )
				{
					str[ i ] = get_string( header.data.str_len[ i ] );
					pipe_read( pd[ READ ], str[ i ],
							   header.data.str_len[ i ] );
					str[ i ][ header.data.str_len[ i ] ] = '\0';
				}
				else
					str[ i ] = get_string_copy( "" );
			}

			/* show the question, get the button number and pass it back to
			   the child process (which does a read in the mean time) */

			writer( C_INT, show_choices( str[ 0 ], n1,
										 str[ 1 ], str[ 2 ], str[ 3 ], n2 ) );

			/* get rid of the strings */

			for ( i = 0; i < 4; i++ )
				T_free( str[ i ] );
			break;

		case C_SHOW_FSELECTOR :
			assert( I_am == PARENT );       /* only to be read by the parent */

			/* get the 4 parameter strings */

			for ( i = 0; i < 4; i++ )
			{
				if ( header.data.str_len[ i ] != 0 )
				{
					str[ i ] = get_string( header.data.str_len[ i ] );
					pipe_read( pd[ READ ], str[ i ],
							   header.data.str_len[ i ] );
					str[ i ][ header.data.str_len[ i ] ] = '\0';
				}
				else
					str[ i ] = NULL;
			}

			/* call fl_show_fselector() and send the result back to the child
			   process (which does a read in the mean time) */

			writer( C_STR, fl_show_fselector( str[ 0 ], str[ 1 ],
											  str[ 2 ], str[ 3 ] ) );

			/* get rid of parameter strings */

			for ( i = 0; i < 4; i++ )
				T_free( str[ i ] );
			retval = 0;
			break;

		case C_INIT_GRAPHICS :
			assert( I_am == PARENT );       /* only to be read by the parent */

			/* read the dimension and the numbers of points */

			pipe_read( pd[ READ ], &dim, sizeof( long ) );
			pipe_read( pd[ READ ], &nx, sizeof( long ) );
			pipe_read( pd[ READ ], &ny, sizeof( long ) );

			/* get length of both label strings from header and read them */

			for ( i = 0; i < 2 ; i++ )
				if ( header.data.str_len[ i ] != 0 )
				{
					str[ i ] = get_string( header.data.str_len[ i ] );
					pipe_read( pd[ READ ], str[ i ],
							   header.data.str_len[ i ] );
					str[ i ][ header.data.str_len[ i ] ] = '\0';
				}
				else
					str[ i ] = NULL;

			/* call the function with the parameters just read */

			graphics_init( dim, nx, ny, str[ 0 ], str[ 1 ] );

			/* get rid of the label strings */

			for ( i = 0; i < 2 ; i++ )
				if ( str[ i ] != NULL )
					T_free( str[ i ] );
			break;

		case C_STR :
			assert( I_am == CHILD );         /* only to be read by the child */

			if ( header.data.len == 0 )
			{
				ret = NULL;
				break;
			}

			if ( retstr != NULL )
				T_free( retstr );
			retstr = get_string( header.data.len );
			pipe_read( pd[ READ ], retstr, header.data.len );
			retstr[ header.data.len ] = '\0';
			if ( ret != NULL )
				*( ( char ** ) ret ) = retstr;
			break;

		case C_INT :
			assert( I_am == CHILD );         /* only to be read by the child */

			if ( ret != NULL )
				*( ( int * ) ret ) = header.data.int_data;
			retval = 1;
			break;

		case C_LONG :
			assert( I_am == CHILD );         /* only to be read by the child */

			if ( ret != NULL )
				*( ( long * ) ret ) = header.data.long_data;
			retval = 1;
			break;

		case C_FLOAT :
			assert( I_am == CHILD );         /* only to be read by the child */

			if ( ret != NULL )
				*( ( float * ) ret ) = header.data.float_data;
			retval = 1;
			break;

		case C_DOUBLE :
			assert( I_am == CHILD );         /* only to be read by the child */
			if ( ret != NULL )
				*( ( double * ) ret ) = header.data.double_data;
			retval = 1;
			break;

		default :                     /* this should never be reached... */
			assert( 1 == 0 );
	}

	/* The parent process now tells the child process that it's again ready
	   to accept further data */

	if ( I_am == PARENT )
		kill( child_pid, DO_SEND );

	return retval;
}


/*--------------------------------------------------------------*/
/* Functions reads from the pipe in chunks of the maximum size. */
/*                                                              */
/* ->                                                           */
/*    1. File descriptor of read end of pipe                    */
/*    2. Buffer for data                                        */
/*    3. Number of bytes to read                                */
/*--------------------------------------------------------------*/

bool pipe_read( int fd, void *buf, size_t bytes_to_read )
{
	size_t bytes_read;
	size_t already_read = 0;

	while ( bytes_to_read > 0 )
	{
		bytes_read = read( fd, buf + already_read, bytes_to_read );
		if ( bytes_read == 0 )
			return FAIL;
		bytes_to_read -= bytes_read;
		already_read += bytes_read;
	}

	return OK;
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
/* C_INIT_GRAPHICS  : dimensionality of experiment (1 or 2) (long), number */
/*                    of points in x- and y-direction (long) and the x-    */
/*                    and y-label strings (char *)                         */
/* C_SHOW_FSELECTOR : 4 strings (4 char *) with identical meaning as the   */
/*                    parameter for fl_show_fselector()                    */
/* C_STR            : string (char *)                                      */
/* C_INT            : integer data (int)                                   */
/* C_LONG,          : long integer data (long)                             */
/* C_FLOAT          : float data (float)                                   */
/* C_DOUBLE         : double data (double)                                 */
/*                                                                         */
/* The types C_EPRINT, C_SHOW_MESSAGE, C_SHOW_ALERT, C_SHOW_CHOICES,       */
/* C_SHOW_FSELECTOR, C_INIT_GRAPHICS are only to be used by the child      */
/* process, the other only by the parent!                                  */
/*-------------------------------------------------------------------------*/


void writer( int type, ... )
{
	CS header;
	va_list ap;
	char *str[ 4 ];
	long dim;
	int n1, n2;
	long nx, ny;
	int i;
	char ack;


	/* The child process has to wait for the parent process to become ready to
	   accept data and then to tell it that it's now going to send new data.
	   The other way round this isn't necessary since the child process is
	   only reading when it actually waits for data. */

	if ( I_am == CHILD )
	{
		while ( ! do_send )             /* wait for parent to become ready */
			pause( );
		do_send = UNSET;

		Key->type = REQUEST;
		kill( getppid( ), NEW_DATA );   /* tell parent to read new data */
	}

	header.type = type;
	va_start( ap, type );

	switch ( type )
	{
		case C_EPRINT :          
			assert( I_am == CHILD );      /* only to be written by the child */

			str[ 0 ] = va_arg( ap, char * );
			header.data.len = strlen( str[ 0 ] );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str[ 0 ], header.data.len );
			break;

		case C_SHOW_MESSAGE :
			assert( I_am == CHILD );      /* only to be written by the child */

			str[ 0 ] = va_arg( ap, char * );
			header.data.len = strlen( str[ 0 ] );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str[ 0 ], header.data.len );

			/* wait for a random character to be sent back as acknowledgment */

			read( pd[ READ ], &ack, sizeof( char ) );
			break;

		case C_SHOW_ALERT :
			assert( I_am == CHILD );      /* only to be written by the child */

			str[ 0 ] = va_arg( ap, char * );
			header.data.len = strlen( str[ 0 ] );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str[ 0 ], header.data.len );

			/* wait for a random character to be sent back as acknowledgment */

			pipe_read( pd[ READ ], &ack, sizeof( char ) );
			break;

		case C_SHOW_CHOICES :
			assert( I_am == CHILD );      /* only to be written by the child */

			str[ 0 ] = va_arg( ap, char * );
			n1 = va_arg( ap, int );
			for ( i = 1; i < 4; i++ )
				str[ i ] = va_arg( ap, char * );
			n2 = va_arg( ap, int );

			for ( i = 0; i < 4; i++ )
				if ( str[ i ] != NULL )
					header.data.str_len[ i ] = strlen( str[ i ] );
				else
					header.data.str_len[ i ] = 0;

			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], &n1, sizeof( int ) );
			write( pd[ WRITE ], &n2, sizeof( int ) );

			for ( i = 0; i < 4; i++ )
				if ( str[ i ] != NULL && strlen( str[ i ] ) != 0 )
					write( pd[ WRITE ], str[ i ], header.data.str_len[ i ] );
			break;

		case C_INIT_GRAPHICS :
			assert( I_am == CHILD );      /* only to be written by the child */

			dim = va_arg( ap, long );
			nx = va_arg( ap, long );
			ny = va_arg( ap, long );
			for ( i = 0; i < 2; i++ )
			{
				str[ i ] = va_arg( ap, char * );
				if ( str[ i ] != NULL )
					header.data.str_len[ i ] = strlen( str[ i ] );
				else
					header.data.str_len[ i ] = 0;
			}

			/* Send header, dimension, numbers of points  and label strings */

			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], &dim, sizeof( long ) );
			write( pd[ WRITE ], &nx, sizeof( long ) );
			write( pd[ WRITE ], &ny, sizeof( long ) );

			if ( str[ 0 ] != NULL )
				write( pd[ WRITE ], str[ 0 ], header.data.str_len[ 0 ] );
			if ( str[ 1 ] != NULL )
				write( pd[ WRITE ], str[ 1 ], header.data.str_len[ 0 ] );
			break;

		case C_SHOW_FSELECTOR :
			assert( I_am == CHILD );      /* only to be written by the child */

			/* set up header and write it */

			for ( i = 0; i < 4; i++ )
			{
				str[ i ] = va_arg( ap, char * );
				if ( str[ i ] != NULL )
					header.data.str_len[ i ] = strlen( str[ i ] );
				else
					header.data.str_len[ i ] = 0;
			}
			write( pd[ WRITE ], &header, sizeof( CS ) );

			/* write out all four strings */

			for ( i = 0; i < 4; i++ )
				if ( str[ i ] != NULL )
					write( pd[ WRITE ], str[ i ], header.data.str_len[ i ] );
			break;

		case C_STR :
			assert( I_am == PARENT );    /* only to be written by the parent */

			str[ 0 ] = va_arg( ap, char * );
			if ( str[ 0 ] != NULL )
				header.data.len = strlen( str[ 0 ] );
			else
				header.data.len = 0;
			write( pd[ WRITE ], &header, sizeof( CS ) );
			if ( str[ 0 ] != NULL )
				write( pd[ WRITE ], str[ 0 ], header.data.len );
			break;

		case C_INT :
			assert( I_am == PARENT );    /* only to be written by the parent */

			header.data.int_data = va_arg( ap, int );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			break;

		case C_LONG :
			assert( I_am == PARENT );    /* only to be written by the parent */

			header.data.long_data = va_arg( ap, long );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			break;

		case C_FLOAT :
			assert( I_am == PARENT );    /* only to be written by the parent */

			header.data.float_data = va_arg( ap, float );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			break;

		case C_DOUBLE :
			assert( I_am == PARENT );    /* only to be written by the parent */

			header.data.double_data = va_arg( ap, double );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			break;

		default :                     /* this should never be reached... */
			assert( 1 == 0 );
	}


	va_end( ap );
}
