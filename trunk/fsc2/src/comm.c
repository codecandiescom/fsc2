/*
  $Id$
*/


#include "fsc2.h"


void reader( void )
{
	CS header;
	char *str;


	if ( read( pd[ READ ], &header, sizeof( CS ) ) == 0 )
		return;

	switch( header.type )
	{
		case C_EPRINT :
			str = T_malloc( header.data.eprint_len + 1 );
			read( pd[ READ ], str, header.data.eprint_len );
			str[ header.data.eprint_len ] = '\0';
fprintf( stderr, "R: Receiving: %s", str );
fflush( stderr );
			eprint( NO_ERROR, "%s", str );
			T_free( str );
			break;

		default :
			eprint( FATAL, "INTERNAL COMMUNICATION ERROR\n" );
			THROW( EXCEPTION );
	}
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
			header.data.eprint_len = strlen( str );
			write( pd[ WRITE ], &header, sizeof( CS ) );
			write( pd[ WRITE ], str, strlen( str ) );
fprintf( stderr, "W: Sending: %s", str );
fflush( stderr );
			break;

		default :
			eprint( FATAL, "INTERNAL COMMUNICATION ERROR (%d)\n", type );
			THROW( EXCEPTION );
	}


	Is_Written = SET;
	va_end( ap );
}
