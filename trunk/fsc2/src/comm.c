/*
  $Id$
*/


#include "fsc2.h"


static void pipe_read( int fd, void *buf, size_t bytes_to_read );


long reader( void *ret )
{
	CS header;
	char *str[ 4 ];
	long dim;
	int i;
	int n1, n2;


	pipe_read( pd[ READ ], &header, sizeof( CS ) );

	switch ( header.type )
	{
		case C_EPRINT :                       /* only to be read by parent */
			str[ 0 ] = get_string( header.data.len );
			pipe_read( pd[ READ ], str[ 0 ], header.data.len );
			str[ 0 ][ header.data.len ] = '\0';
			eprint( NO_ERROR, "%s", str[ 0 ] );
			T_free( str[ 0 ] );
			if ( ret != NULL )
				*( ( char ** ) ret ) = NULL;
			return 0;

		case C_SHOW_MESSAGE :                 /* only to be read by parent */
			str[ 0 ] = get_string( header.data.len );
			pipe_read( pd[ READ ], str[ 0 ], header.data.len );
			str[ 0 ] [ header.data.len ] = '\0';
			fl_show_messages( str[ 0 ] );
			T_free( str[ 0 ] );
			if ( ret != NULL )
				*( ( char ** ) ret ) = NULL;
			return 0;

		case C_SHOW_ALERT :                   /* only to be read by parent */
			str[ 0 ] = get_string( header.data.len );
			pipe_read( pd[ READ ], str[ 0 ], header.data.len );
			show_alert( str[ 0 ] );
			T_free( str[ 0 ] );
			if ( ret != NULL )
				*( ( char ** ) ret ) = NULL;
			return 0;

		case C_SHOW_CHOICES :                 /* only to be read by parent */
			if ( I_am == CHILD )
			{
				fprintf( stderr, "CHILD4: In C_SHOW_CHOICES\n" );
				return 0;
			}

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
			   the chiold process */

			writer( C_INT, show_choices( str[ 0 ], n1,
										 str[ 1 ], str[ 2 ], str[ 3 ], n2 ) );

			/* get rid of the strings */

			for ( i = 0; i < 4; i++ )
				T_free( str[ i ] );

			if ( ret != NULL )
				*( ( char ** ) ret ) = NULL;
			return 0;

		case C_INIT_GRAPHICS :                /* only to be read by parent */
			/* read in the dimension */

			pipe_read( pd[ READ ], &dim, sizeof( long ) );

			/* get length of first label string from header and read it */

			if ( header.data.str_len[ 0 ] != 0 )
			{
				str[ 0 ] = get_string( header.data.str_len[ 0 ] );
				pipe_read( pd[ READ ], str[ 0 ],
					  header.data.str_len[ 0 ] );
				str[ 0 ][ header.data.str_len[ 0 ] ] = '\0';
			}
			else
				str[ 0 ] = NULL;

			/* get length of second label string from header and read it */

			if ( header.data.str_len[ 1 ] != 0 )
			{
				str[ 1 ] = get_string( header.data.str_len[ 1 ] );
				pipe_read( pd[ READ ], str[ 1 ],
					  header.data.str_len[ 1 ] );
				str[ 1 ][ header.data.str_len[ 1 ] ] = '\0';
			}
			else
				str[ 1 ] = NULL;

			/* call the function with the parameters just read */

			graphics_init( dim, str[ 0 ], str[ 1 ] );

			/* get rid of the label strings */

			if ( str[ 0 ] != NULL )
				T_free( str[ 0 ] );
			if ( str[ 1 ] != NULL )
				T_free( str[ 1 ] );

			if ( ret != NULL )
				*( ( char ** ) ret ) = NULL;
			return 0;

		case C_INT :
			if ( ret != NULL )
				*( ( int * ) ret ) = header.data.int_data;
			return 1;

		case C_LONG :
			if ( ret != NULL )
				*( ( long * ) ret ) = header.data.long_data;
			return 1;

		case C_FLOAT :
			if ( ret != NULL )
				*( ( float * ) ret ) = header.data.float_data;
			return 1;

		case C_DOUBLE :
			if ( ret != NULL )
				*( ( double * ) ret ) = header.data.double_data;
			return 1;

		default :
			eprint( FATAL, "INTERNAL COMMUNICATION ERROR (in reader(), "
					"type %d).\n", header.type );
			THROW( EXCEPTION );
	}

	/* The parent process still has to tell the child process that it's now
	   ready to accept further data */

	if ( I_am == PARENT )
		kill( child_pid, DO_SEND );

	return 0;
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


void writer( int type, ... )
{
	CS header;
	va_list ap;
	char *str[ 4 ];
	long dim;
	int n1, n2;
	int i;


	/* The child process first got to wait for the parent process to become
	   ready to accept data and than has to tell it that it's now going to
	   send new data. The other way round this isn't necessary since the
	   child process is only reading when it actually waits for data. */

	if ( I_am == CHILD )
	{
		fprintf( stderr, "CHILD2: do_send is %s.\n", do_send ? "SET" : "UNSET" );
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
			break;

		case C_SHOW_ALERT :               /* only to be written by child */
			str[ 0 ] = va_arg( ap, char * );
			header.data.len = strlen( str[ 0 ] );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str[ 0 ], header.data.len );
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
					write( pd[ WRITE ], str[ i ], strlen( str[ i ] ) );
			break;

		case C_INIT_GRAPHICS :            /* only to be written by child */
			dim = va_arg( ap, long );
			str[ 0 ] = va_arg( ap, char * );
			str[ 0 ] = va_arg( ap, char * );
			if ( str[ 0 ] != NULL )
				header.data.str_len[ 0 ] = strlen( str[ 0 ] );
			else
				header.data.str_len[ 0 ] = 0;
			if ( str[ 1 ] != NULL )
				header.data.str_len[ 1 ] = strlen( str[ 1 ] );
			else
				header.data.str_len[ 1 ] = 0;

			/* Send header, dimension and (optionally) the label strings */

			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], &dim, sizeof( long ) );
			if ( str[ 0 ] != NULL )
				write( pd[ WRITE ], str[ 0 ], strlen( str[ 0 ] ) );
			if ( str[ 1 ] != NULL )
				write( pd[ WRITE ], str[ 1 ], strlen( str[ 1 ] ) );
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
