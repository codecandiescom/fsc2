/*
  $Id$
*/

#include "fsc2.h"


char *get_string_copy( const char *str )
{
	char *new;

	new = T_malloc( ( strlen( str ) + 1 ) * sizeof( char ) );
	strcpy( new, str );
	return new;
}


char *get_string( size_t len )
{
	return T_malloc( ( len + 1 ) * sizeof( char ) );
}


char *string_to_lower( char *str )
{
	char *ptr;


	for ( ptr = str; *ptr; ptr++ )
		if ( isupper( *ptr ) )
			*ptr = tolower( *ptr );

	return( str );
}


void *get_memcpy( const void *array, size_t size )
{
	void *new;

	new = T_malloc( size );
	memcpy( new, array, size );
	return( new );
}


/*-----------------------------------------------*/
/* strip_path() returns pointer to bare name the */
/* function from a path like "/usr/bin/emacs".   */
/*-----------------------------------------------*/

const char *strip_path( const char *path )
{
    char *cp;
    
    if ( ( cp = strrchr( path, '/' ) ) == NULL )
        return( path );
    else
        return( ++cp );
}


/*-----------------------------------------------------------------*/
/* get_file_length() returns the number of lines in a file as well */
/* as the number of digits in the number of lines. To do so a      */ 
/* short awk script is used (wc can't be used since it returns a   */
/* number of lines to short by one if the last line does not end   */
/* with a newline char).                                           */
/* ->                                                              */
/*   * name of file                                                */
/*   * pointer for returning number of digits of number of lines   */
/* <-                                                              */
/*   * number of lines or -1: not enough memory, -2: popen failure */
/*-----------------------------------------------------------------*/

long get_file_length( char *name, int *len )
{
	char *pc;
	FILE *pp;
	long lc, i;


	/* Get some memory for the pipe command */

	pc = ( char * ) malloc( 20 + strlen( AWK_PROG ) + strlen( in_file ) );
	if ( pc  == NULL )
		return( -1 );                        /* FATAL: no memory */

	/* set up pipe to 'awk' (defined via AWK_PROG) and read number of lines */

	strcpy( pc, AWK_PROG" 'END{print NR}' " );
	strcat( pc, name );
	if ( ( pp = popen( pc, "r" ) ) == NULL )
	{
		free( pc );
		return( -2 );                  /* popen() failed */
	}

	fscanf( pp, "%ld", &lc );
	pclose( pp );
	free( pc );

	/* count number of digits of number of lines */

	for ( i = lc, *len = 1; ( i /= 10 ) > 0; ++( *len ) )
		;

	return( lc );
}


void eprint( int severity, const char *fmt, ... )
{
	va_list ap;

	compilation.error[ severity ] = SET;

	if ( severity != NO_ERROR )
		printf( "%c ", severity[ "FSW" ] );      /* Hehe... */

	va_start( ap, fmt );
	vprintf( fmt, ap );
	va_end( ap );
	fflush( stdout );
}


/* Here some more utility functions - they are that short that declaring
   them inline seems to be a good idea... */

inline long rnd( double x ) { return ( long ) ( 2 * x ) - ( long ) x; }

inline int    imax( int    a, int    b ) { return a > b ? a : b ;  }
inline int    imin( int    a, int    b ) { return a < b ? a : b ;  }
inline long   lmax( long   a, long   b ) { return a > b ? a : b ;  }
inline long   lmin( long   a, long   b ) { return a < b ? a : b ;  }
inline float  fmax( float  a, float  b ) { return a > b ? a : b;  }
inline float  fmin( float  a, float  b ) { return a < b ? a : b;  }
inline double dmax( double a, double b ) { return a > b ? a : b;  }
inline double dmin( double a, double b ) { return a < b ? a : b;  }
