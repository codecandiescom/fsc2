#include "fsc2.h"


char *get_string_copy( char *str )
{
	char *new;

	new = ( char * ) T_malloc( ( strlen( str ) + 1 ) * sizeof( char ) );
	strcpy( new, str );
	return( new );
}


char *get_string( size_t len )
{
	return( ( char * ) T_malloc( ( len + 1 ) * sizeof( char ) ) );
}


void eprint( int severity, const char *fmt, ... )
{
	va_list ap;

	if ( severity == FATAL )
		vars_clean_up( );

	compilation.error[ severity ] = SET;

	printf( "%c ", severity[ "FSW " ] );   /* Kids, don't do this at home... */
	va_start( ap, fmt );
	vprintf( fmt, ap );
	va_end( ap );
	fflush( stdout );
}


/* Here some more utility functions - they are that short that declaring
   them inline seems to be a good idea... */

inline long rnd( double x ) { return( ( long ) ( 2 * x ) - ( long ) x ); }

inline int    imax( int    a, int    b ) { return( a > b ? a : b );  }
inline int    imin( int    a, int    b ) { return( a < b ? a : b );  }
inline long   lmax( long   a, long   b ) { return( a > b ? a : b );  }
inline long   lmin( long   a, long   b ) { return( a < b ? a : b );  }
inline float  fmax( float  a, float  b ) { return( a > b ? a : b );  }
inline float  fmin( float  a, float  b ) { return( a < b ? a : b );  }
inline double dmax( double a, double b ) { return( a > b ? a : b );  }
inline double dmin( double a, double b ) { return( a < b ? a : b );  }
