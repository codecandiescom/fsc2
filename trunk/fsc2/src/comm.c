/*
  $Id$: comm.c,v 1.3 1999/09/03 07:50:17 jens Exp jens $
*/


#include "fsc2.h"


static void T_read( int fd, void *buf, size_t bytes_to_read );



long reader( void *ret )
{
	CS header;
	char *str;
	char *strs[ 3 ];
	long dim;


	T_read( pd[ READ ], &header, sizeof( CS ) );

	switch ( header.type )
	{
		case C_EPRINT :
			str = T_malloc( header.data.len + 1 );
			T_read( pd[ READ ], str, header.data.len );
			str[ header.data.len ] = '\0';
			eprint( NO_ERROR, "%s", str );
			T_free( str );
			if ( ret != NULL )
				*( ( char ** ) ret ) = NULL;
			return 0;

		case C_SHOW_MESSAGE :
			str = T_malloc( header.data.len + 1 );
			T_read( pd[ READ ], str, header.data.len );
			str[ header.data.len ] = '\0';
			fl_show_messages( str );
			T_free( str );
			if ( ret != NULL )
				*( ( char ** ) ret ) = NULL;
			return 0;

		case C_SHOW_ALERT :
			str = T_malloc( header.data.len + 1 );
			T_read( pd[ READ ], str, header.data.len );
			str[ header.data.len ] = '\0';
			strs[ 0 ] = str;
			if ( ( strs[ 1 ] = strchr( strs[ 0 ], '\n' ) ) != NULL )
			{
				*strs[ 1 ]++ = '\0';
				if ( ( strs[ 2 ] = strchr( strs[ 1 ], '\n' ) ) != NULL )
					*strs[ 2 ]++ = '\0';
				else
					strs[ 2 ] = NULL;
			}
			else
				strs[ 1 ] = strs[ 2 ] = NULL;
			fl_show_alert( strs[ 0 ], strs[ 1 ], strs[ 2 ], 1 );
			T_free( str );
			if ( ret != NULL )
				*( ( char ** ) ret ) = NULL;
			return 0;

		case C_INIT_GRAPHICS :
			/* read in the dimension */

			T_read( pd[ READ ], &dim, sizeof( long ) );

			/* get length of first label string from header and read it */

			if ( header.data.init_graphics.str1_len != 0 )
			{
				strs[ 0 ] = get_string( header.data.init_graphics.str1_len );
				T_read( pd[ READ ], strs[ 0 ],
					  header.data.init_graphics.str1_len );
				strs[ 0 ][ header.data.init_graphics.str1_len ] = '\0';
			}
			else
				strs[ 0 ] = NULL;

			/* get length of second label string from header and read it */

			if ( header.data.init_graphics.str2_len != 0 )
			{
				strs[ 1 ] = get_string( header.data.init_graphics.str2_len );
				T_read( pd[ READ ], strs[ 1 ],
					  header.data.init_graphics.str2_len );
				strs[ 1 ][ header.data.init_graphics.str2_len ] = '\0';
			}
			else
				strs[ 1 ] = NULL;

			/* call the function with the parameters just read */

			graphics_init( dim, strs[ 0 ], strs[ 1 ] );

			/* get rid of the label strings */

			if ( strs[ 0 ] != NULL )
				T_free( strs[ 0 ] );
			if ( strs[ 1 ] != NULL )
				T_free( strs[ 1 ] );

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

void T_read( int fd, void *buf, size_t bytes_to_read )
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
	char *str;
	char *str1, *str2;
	long dim;


	/* The child first got to wait for the parent being ready to accept data
	   and than has to tell the parent that it's now going to send new data.
	   The other way round this isn't necessary since the child is only going
	   to read when it actually waits for data. */

	if ( I_am == CHILD )
	{
		while ( ! do_send )      /* wait for parent to become ready for data */
			pause( );
		do_send = UNSET;

		kill( getppid( ), NEW_DATA );   /* tell parent to read new data */
	}

	header.type = type;
	va_start( ap, type );

	switch ( type )
	{
		case C_EPRINT :
			str = va_arg( ap, char * );
			header.data.len = strlen( str );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str, header.data.len );
			break;

		case C_SHOW_MESSAGE :
			str = va_arg( ap, char * );
			header.data.len = strlen( str );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str, header.data.len );
			break;

		case C_SHOW_ALERT :
			str = va_arg( ap, char * );
			header.data.len = strlen( str );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str, header.data.len );
			break;

		case C_INIT_GRAPHICS :
			dim = va_arg( ap, long );
			str1 = va_arg( ap, char * );
			str2 = va_arg( ap, char * );
			if ( str1 != NULL )
				header.data.init_graphics.str1_len = strlen( str1 );
			else
				header.data.init_graphics.str1_len = 0;
			if ( str2 != NULL )
				header.data.init_graphics.str2_len = strlen( str2 );
			else
				header.data.init_graphics.str2_len = 0;

			/* Send header, dimension and (optionally) the label strings */

			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], &dim, sizeof( long ) );
			if ( str1 != NULL )
				write( pd[ WRITE ], str1, strlen( str1 ) );
			if ( str2 != NULL )
				write( pd[ WRITE ], str2, strlen( str2 ) );
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
