/*
  $Id$
*/


/*
   This part of the program is necessary for exchanging data between the main
   process (called the parent process in the following) and the forked process
   that does the measurement (the child process). There are several reasons
   why both processes have to exchange data. The first one is that the child
   process can't (and shouldn't) take care of displaying the measured data.
   Thus a mechanism is needed for passing the measured data to be displayed to
   the parent process. The other reason is that the child process also can't
   use the functions from the XForms library for displaying message or alert
   boxes etc. For this we need the parent process to display them and then to
   return the result to the child process.

   The communication between both processes works via a pair of pipes (it also
   could be done using a socket pair - I don't know if this would be faster so
   I opted for the more conservative approach.

   An important point is that the child process is supposed not to flood the
   parent process with data - thus there is the following mechanism to avoid
   it: The parent pocess always has to send a DO_SEND signal (that's SIGUSR1)
   to the child process before it tries to send further data.

   Since the parent process will usually be busy displaying data and shouln't
   be forces to constantly check for new data, the child process always has to
   send a SIGNAL, NEW_DATA (that's SIGUSR2), before it actually sends the
   data. Thus the parent process is interrupted to read the data. If it has
   done so it again sends the DO_SEND signal to the child process.

   This way of controlling the communication between parent and child process
   has the advantage that we don't have to make the signal handlers reentrant
   - the child process will only send more data (and thus raise te NEW_DATA
   signal) when the parent process is finished reading previously sent data.
   But there is also a disadvantage: If the parent process takes a long time
   to accept new data, the child will not be able to continue with the
   experiment in between. Thus the code in the parent process should be
   written in a way that new data are read with no more delay than necessary.

   There is an asymmetry in the communication between the parent and the child
   process. The child process rarely needs data from the parent.  These cases
   are when the child needs the parent process to display a message or alert
   box etc. and than has to wait for the result. That means that the
   communication is always synchronous, i.e. the child has not to expect data
   at random moments but only as the result of a previous write operation.
   Thus for these functions we don't have to set up a scheme using signals
   (it even knows what kind of data to expect as reply).

   Because of this asymmetry there are several types of messages which can
   only be send by the child process and read only by the parent process.
   These are:
        C_EPRINT
		C_SHOW_MESSAGE
		C_SHOW_ALERT
		C_SHOW_CHOICES
		C_SHOW_FSELECTOR
		C_INIT_GRAPHICS */



#include "fsc2.h"


static void pipe_read( int fd, void *buf, size_t bytes_to_read );


long reader( void *ret )
{
	CS header;
	char *str[ 4 ];
	long dim;
	int i;
	int n1, n2;
	long retval;
	char *retstr = NULL;


	pipe_read( pd[ READ ], &header, sizeof( CS ) );

	switch ( header.type )
	{
		case C_EPRINT :                       /* only to be read by parent */
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

		case C_SHOW_MESSAGE :                 /* only to be read by parent */
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

		case C_SHOW_ALERT :                   /* only to be read by parent */
			str[ 0 ] = get_string( header.data.len );
			pipe_read( pd[ READ ], str[ 0 ], header.data.len );
			show_alert( str[ 0 ] );

			/* send back just one character as indicator that the alert has
			   been read by te user */

			write( pd[ WRITE ], str[ 0 ], sizeof( char ) );

			T_free( str[ 0 ] );
			retval = 0;
			break;

		case C_SHOW_CHOICES :                 /* only to be read by parent */
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

		case C_SHOW_FSELECTOR :               /* only to be read by parent */
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

		case C_INIT_GRAPHICS :                /* only to be read by parent */
			/* read in the dimension */

			pipe_read( pd[ READ ], &dim, sizeof( long ) );

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

			graphics_init( dim, str[ 0 ], str[ 1 ] );

			/* get rid of the label strings */

			for ( i = 0; i < 2 ; i++ )
				if ( str[ i ] != NULL )
					T_free( str[ i ] );
			break;

		case C_STR :
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
			if ( ret != NULL )
				*( ( int * ) ret ) = header.data.int_data;
			retval = 1;
			break;

		case C_LONG :
			if ( ret != NULL )
				*( ( long * ) ret ) = header.data.long_data;
			retval = 1;
			break;

		case C_FLOAT :
			if ( ret != NULL )
				*( ( float * ) ret ) = header.data.float_data;
			retval = 1;
			break;

		case C_DOUBLE :
			if ( ret != NULL )
				*( ( double * ) ret ) = header.data.double_data;
			retval = 1;
			break;

		default :
			eprint( FATAL, "INTERNAL COMMUNICATION ERROR (in reader(), "
					"type %d).\n", header.type );
			THROW( EXCEPTION );
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

void pipe_read( int fd, void *buf, size_t bytes_to_read )
{
	size_t bytes_read;
	size_t already_read = 0;

	while ( bytes_to_read > 0 )
	{
		bytes_read = read( fd, buf + already_read, bytes_to_read );
		bytes_to_read -= bytes_read;
		already_read += bytes_read;
	}
}



/*
    expected parameter for the different values of `type': 

	C_EPRINT         : string (char *) to be output
	C_SHOW_MESSAGE   : message string (char *) to be shown (parameter as in
	                   fl_show_messages())
	C_SHOW_ALERT     : alert string (char *) to be shown
	C_SHOW_CHOICES   : question string (char *), number of buttons (1-3) (int),
                       3 strings with texts for buttons (char *), number of
                       default button (int) (parameter as in fl_show_choices())
	C_INIT_GRAPHICS  : dimensionality of plot (2 or 3) (long), x- and y-label
                       strings (char *)
	C_SHOW_FSELECTOR : 4 strings (char *) with identical meaning as the para-
	                   meter for fl_show_fselector()
	C_STR            : string (char *)
	C_INT            : integer data (int)
	C_LONG,          : long integer data (long)
	C_FLOAT          : float data (float)
	C_DOUBLE         : double data (double)

	The types C_EPRINT, C_SHOW_MESSAGE, C_SHOW_ALERT, C_SHOW_CHOICES,
	C_SHOW_FSELECTOR and C_INIT_GRAPHICS are only to be used by the child
	process!
*/


void writer( int type, ... )
{
	CS header;
	va_list ap;
	char *str[ 4 ];
	long dim;
	int n1, n2;
	int i;
	char ack;


	/* The child process has to wait for the parent process to become ready to
	   accept data and than to tell it that it's now going to send new data.
	   The other way round this isn't necessary since the child process is
	   only reading when it actually waits for data. */

	if ( I_am == CHILD )
	{
		while ( ! do_send )             /* wait for parent to become ready */
			pause( );
		do_send = UNSET;

		kill( getppid( ), NEW_DATA );   /* tell parent to read new data */
	}

	header.type = type;
	va_start( ap, type );

	switch ( type )
	{
		case C_EPRINT :                   /* only to be written by child */
			str[ 0 ] = va_arg( ap, char * );
			header.data.len = strlen( str[ 0 ] );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str[ 0 ], header.data.len );
			break;

		case C_SHOW_MESSAGE :             /* only to be written by child */
			str[ 0 ] = va_arg( ap, char * );
			header.data.len = strlen( str[ 0 ] );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str[ 0 ], header.data.len );

			/* wait for a random character to be sent back as acknowledgment */

			read( pd[ READ ], &ack, sizeof( char ) );
			break;

		case C_SHOW_ALERT :               /* only to be written by child */
			str[ 0 ] = va_arg( ap, char * );
			header.data.len = strlen( str[ 0 ] );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str[ 0 ], header.data.len );

			/* wait for a random character to be sent back as acknowledgment */

			pipe_read( pd[ READ ], &ack, sizeof( char ) );
			break;

		case C_SHOW_CHOICES :             /* only to be written by child */
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

		case C_INIT_GRAPHICS :            /* only to be written by child */
			dim = va_arg( ap, long );
			for ( i = 0; i < 2; i++ )
			{
				str[ i ] = va_arg( ap, char * );
				if ( str[ i ] != NULL )
					header.data.str_len[ i ] = strlen( str[ i ] );
				else
					header.data.str_len[ i ] = 0;
			}

			/* Send header, dimension and the label strings */

			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], &dim, sizeof( long ) );
			if ( str[ 0 ] != NULL )
				write( pd[ WRITE ], str[ 0 ], header.data.str_len[ 0 ] );
			if ( str[ 1 ] != NULL )
				write( pd[ WRITE ], str[ 1 ], header.data.str_len[ 0 ] );
			break;

		case C_SHOW_FSELECTOR :             /* only to be written by child */
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
			str[ 0 ] = va_arg( ap, char * );
			header.data.len = strlen( str[ 0 ] );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str[ 0 ], header.data.len );
			break;

		case C_INT :
			header.data.int_data = va_arg( ap, int );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			break;

		case C_LONG :
			header.data.long_data = va_arg( ap, long );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			break;

		case C_FLOAT :
			header.data.float_data = va_arg( ap, float );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			break;

		case C_DOUBLE :
			header.data.double_data = va_arg( ap, double );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			break;

		default :
			eprint( FATAL, "INTERNAL COMMUNICATION ERROR (in writer(), "
					"type %d)\n", type );
			THROW( EXCEPTION );
	}


	va_end( ap );
}
