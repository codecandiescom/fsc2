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

	TRY
	{
		pc = T_malloc( 20 + strlen( AWK_PROG ) + strlen( name ) );
		TRY_SUCCESS;
	}
	OTHERWISE
		return( -1 );                        /* FATAL: no memory */

	/* set up pipe to 'awk' (defined via AWK_PROG) and read number of lines */

	strcpy( pc, AWK_PROG" 'END{print NR}' " );
	strcat( pc, name );
	if ( ( pp = popen( pc, "r" ) ) == NULL )
	{
		T_free( pc );
		return( -2 );                  /* popen() failed */
	}

	fscanf( pp, "%ld", &lc );
	pclose( pp );
	T_free( pc );

	/* count number of digits of number of lines */

	for ( i = lc, *len = 1; ( i /= 10 ) > 0; ++( *len ) )
		;

	return( lc );
}


void eprint( int severity, const char *fmt, ... )
{
	static char buffer[ BROWSER_MAXLINE ];
	static char *cp = buffer;
	static int space_left = BROWSER_MAXLINE - 1;
	int written;
	va_list ap;

	compilation.error[ severity ] = SET;

	if ( ! just_testing )
	{
		if ( cp == buffer )
		{
			if ( severity == FATAL )
			{
				strcpy( cp, "@C1" );
				cp += 3;
				space_left -= 3;
			}

			if ( severity == SEVERE )
			{
				strcpy( cp, "@C3" );
				cp += 3;
				space_left -= 3;
			}

			if ( severity == WARN )
			{
				strcpy( cp, "@C2" );
				cp += 3;
				space_left -= 3;
			}
		}

		/* Avoid writting more than BROWSER_MAXLINE chars */

		if ( space_left > 0 )
		{
			va_start( ap, fmt );
			written = vsnprintf( cp, space_left, fmt, ap );
			va_end( ap );

			if ( written > 0 )
			{
				space_left -= written;
				cp += written;
			}
			else
				space_left = 0;
		}

		/* Output line only after newline char has been found */

		if ( buffer[ strlen( buffer ) - 1 ] != '\n' )
			return;

		buffer[ strlen( buffer ) - 1 ] = '\0';
		fl_freeze_form( main_form->error_browser->form );
		fl_add_browser_line( main_form->error_browser, buffer );

		fl_set_browser_topline( main_form->error_browser,
				   fl_get_browser_maxline( main_form->error_browser )
				 - fl_get_browser_screenlines( main_form->error_browser ) + 1);

		fl_unfreeze_form( main_form->error_browser->form );

		cp = buffer;
		space_left = BROWSER_MAXLINE - 1;
	}
	else
	{
		if ( severity != NO_ERROR )
			fprintf( stderr, "%c ", severity[ "FSW" ] );      /* Hehe... */

		va_start( ap, fmt );
		vfprintf( stderr, fmt, ap );
		va_end( ap );
		fflush( stderr );
	}

}


/* Here some more utility functions - they are that short that declaring
   them inline seems to be a good idea... */

inline long rnd( double x ) { return ( long ) ( 2 * x ) - ( long ) x; }

inline int    imax( int    a, int    b ) { return a > b ? a : b ; }
inline int    imin( int    a, int    b ) { return a < b ? a : b ; }
inline long   lmax( long   a, long   b ) { return a > b ? a : b ; }
inline long   lmin( long   a, long   b ) { return a < b ? a : b ; }
inline float  fmax( float  a, float  b ) { return a > b ? a : b; }
inline float  fmin( float  a, float  b ) { return a < b ? a : b; }
inline double dmax( double a, double b ) { return a > b ? a : b; }
inline double dmin( double a, double b ) { return a < b ? a : b; }
