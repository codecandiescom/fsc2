/*
  $Id$
*/


#include "fsc2.h"


long reader( void *ret )
{
	CS header;
	char *str;


	read( pd[ READ ], &header, sizeof( CS ) );

	switch( header.type )
	{
		case C_EPRINT :
			str = T_malloc( header.data.len + 1 );
			read( pd[ READ ], str, header.data.len );
			str[ header.data.len ] = '\0';
			eprint( NO_ERROR, "%s", str );
			T_free( str );
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
			eprint( FATAL, "INTERNAL COMMUNICATION ERROR\n" );
			THROW( EXCEPTION );
	}

	return 0;
}


void writer( int type, ... )
{
	CS header;
	va_list ap;
	char *str;


	header.type = type;
	va_start( ap, type );

	switch( type )
	{
		case C_EPRINT :
			str = va_arg( ap, char * );
			header.data.len = strlen( str );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str, strlen( str ) );
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
			eprint( FATAL, "INTERNAL COMMUNICATION ERROR (%d)\n", type );
			THROW( EXCEPTION );
	}


	Is_Written = SET;
	va_end( ap );
}
