/*
  $Id$
*/

#include "fsc2.h"


char *get_string_copy( const char *str )
{
	char *new;

	if ( str == NULL )
		return NULL;
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


	if ( str == NULL )
		return NULL;
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


/*----------------------------------------------------------------------*/
/* Function replaces all occurences iof the character combination "\n"  */
/* in a string by line the break character '\n'. This is done in place, */
/* i.e. the string passed to the function is changed, not a copy. So,   */
/* never call it with a char array defined as const.                    */
/*----------------------------------------------------------------------*/

char *correct_line_breaks( char *str )
{
	char *p1 = str,
		 *p2;

	while ( ( p1 = strstr( p1, "\\n" ) ) != NULL )
	{
		p2 = p1++;
		*p2 = '\n';
		while ( *++p2 )
			*p2 = *( p2 + 1 );
	}

	return str;
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
				strcpy( cp, "@C2" );
				cp += 3;
				space_left -= 3;
			}

			if ( severity == WARN )
			{
				strcpy( cp, "@C4" );
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

		/* Catch stuff from f_print(), skip actual printing as long as last
		   character is '\x7F' */

		if ( buffer[ strlen( buffer ) - 1 ] == '\x7F' )
		{

			buffer[ strlen( buffer ) - 1 ] = '\0';
			cp--;
			space_left += 1;
			return;
		}

		/* Catch stuff from f_print() */

		if ( buffer[ strlen( buffer ) - 1 ] == '\x7E' )
			 buffer[ strlen( buffer ) - 1 ] = '\0';

		if ( I_am == PARENT )
		{
			fl_freeze_form( main_form->error_browser->form );
			fl_add_browser_line( main_form->error_browser, buffer );

			fl_set_browser_topline( main_form->error_browser,
				  fl_get_browser_maxline( main_form->error_browser )
				- fl_get_browser_screenlines( main_form->error_browser ) + 1 );

			fl_unfreeze_form( main_form->error_browser->form );
		}
		else
			writer( C_EPRINT, buffer );

		cp = buffer;
		space_left = BROWSER_MAXLINE - 1;
	}
	else
	{
		if ( severity != NO_ERROR )
			fprintf( stdout, "%c ", severity[ "FSW" ] );      /* Hehe... */

		va_start( ap, fmt );
		vfprintf( stdout, fmt, ap );
		va_end( ap );
		fflush( stdout );
	}

}


/*-----------------------------------------------------*/
/*-----------------------------------------------------*/


bool fsc2_locking( void )
{
	int fd, flags;
	struct flock lock;
	char message[ 128 ] = "running. ";
	char buf[ 10 ];
	int i;


	/* Open a lock file */

	if ( ( fd = open( LOCKFILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR ) ) < 0 )
	{
		fl_show_alert( "Error", "Sorry, can't create a lock file.",
					   LOCKFILE, 1 );
		return FAIL;
	}

	/* Set a write lock */

	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

	if ( fcntl( fd, F_SETLK, &lock ) == -1 )           /* if lock failed */
	{
		/* Try to read from the lock file the PID of the process holding
		   the lock and add it to the error message */

		i = 0;
		do
			read( fd, ( void * ) ( buf + i ), sizeof( char ) );
		while ( buf[ i ] != '\n' && i++ < 10 );
		if ( i != 10 )
		{
			buf[ i ] = '\0';
			strcat( message, " Its PID is " );
			strcat( message, buf );
			strcat( message, "." );
		}
		
		fl_show_alert( "Error", "Another instance of fsc2 is already",
					   message, 1 );
		return FAIL;
	}

	/* Truncate to zero length */

	if ( ftruncate( fd, 0 ) < 0 )
	{
		unlink( LOCKFILE );
		fl_show_alert( "Error", "Sorry, can't write lock file:",
					   LOCKFILE, 1 );
		return FAIL;
	}

	/* Write process ID into the lock file */

	sprintf( buf, "%d\n", getpid( ) );
	if ( write( fd, buf, strlen( buf ) ) != ( ssize_t ) strlen( buf ) )
	{
		unlink( LOCKFILE );
		fl_show_alert( "Error", "Sorry, can't write lock file:",
					   LOCKFILE, 1 );
		return FAIL;
	}

	/* Set the close-on-exec flag */

	if ( ( flags = fcntl( fd, F_GETFD, 0 ) ) < 0 )
	{
		unlink( LOCKFILE );
		fl_show_alert( "Error", "Sorry, can't write lock file:",
					   LOCKFILE, 1 );
		return FAIL;
	}

	flags |= FD_CLOEXEC;

	if ( fcntl( fd, F_GETFD, flags ) < 0 )
	{
		unlink( LOCKFILE );
		fl_show_alert( "Error", "Sorry, can't write lock file:",
					   LOCKFILE, 1 );
		return FAIL;
	}

	return OK;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

void delete_stale_shms( void )
{
	char cmd[ 128 ];
	FILE *pp;
	int shm_id;
	void *buf;


	/* Start the perl script 'shm_list' (must be located in `libdir') to get
	   a list of all shared memory segments */

	strcpy( cmd, libdir );
	strcat( cmd, "/shm_list" );
	if ( ( pp = popen( cmd, "r" ) ) == NULL )
		return;
	
	/* Run through the list of memory segments */

	while ( fscanf( pp, "%d", &shm_id ) != EOF )
	{
		/* Try to attach to the segment */

		if ( ( buf = shmat( shm_id, NULL, 0 ) ) == ( void * ) - 1 )
			continue;

		/* If the segments starts with the 'fsc2' delete it */

		if ( ! strncmp( ( char * ) buf, "fsc2", 4 ) )
		{
			shmdt( buf );
			shmctl( shm_id, IPC_RMID, NULL );
		}
		else
			shmdt( buf );
	}

	pclose( pp );
	return;
}


/*------------------------------------------------------------------------*/
/* Function converts intensities into rgb values (between 0 and 255). For */
/* values below 0 a dark kind of violet is returned, for values above 1 a */
/* creamy shade of white. The interval [ 0, 1 ] itself is subdivided into */
/* 6 subintervals at the points defined by the array `p' and rgb colors   */
/* ranging from blue via cyan, green and yellow to red are calculated     */
/* with `v' defining the intensities of the three primary colors at the   */
/* endpoints of the intervals and using linear interpolation in between.  */
/*------------------------------------------------------------------------*/

void i2rgb( double h, int *rgb )
{
	int i, j;
	double p[ 7 ] = { 0.0, 0.125, 0.4, 0.5, 0.6, 0.875, 1.0 };
	int v[ 3 ][ 7 ] = { {  64,   0,   0,  32, 233, 255, 191 },     /* RED   */
						{   0,  32, 233, 255, 233,  32,   0 },     /* GREEN */
						{ 191, 255, 233,  32,   0,   0,   0 } };   /* BLUE  */


	if ( h < p[ 0 ] )
	{
		rgb[ RED ]   = 72;
		rgb[ GREEN ] = 0;
		rgb[ BLUE ]  = 72;
		return;
	}

	for ( i = 0; i < 6; i++ )
		if ( p[ i ] != p[ i + 1 ] && h <= p[ i + 1 ] )
		{
			for ( j = RED; j <= BLUE; j++ )
				rgb[ j ]   = ( int ) ( v[ j ][ i ] +
									   ( v[ j ][ i + 1 ] - v[ j ][ i ] ) 
									   * ( h - p[ i ] )
									   / ( p[ i + 1 ] - p[ i ] ) );
			return;
		}

	rgb[ RED ] = 255;
	rgb[ GREEN ] = 248;
	rgb[ BLUE ] = 220;
}


/* Here some more utility functions - they are that short that inlining them
   seems to be a good idea... */

/* The next two functions do a conversion of double or integer values to
   short.  Both are exclusively used in the conversion of data to points to be
   drawn on the screen via XPoint structure which contain to short ints.  To
   avoid overflows in the calculations we restrict the values to half the
   allowed range of sort ints - thus allowing canvas sizes of up to half the
   size of a short int. */

#define SHRT_MAX_HALF ( SHRT_MAX >> 1 )
#define SHRT_MIN_HALF ( SHRT_MIN >> 1 )


inline short d2shrt( double a )
{
	if ( a > SHRT_MAX_HALF << 1 )
		return SHRT_MAX_HALF;
	if ( a < SHRT_MIN_HALF )
		return SHRT_MIN_HALF;
	return ( short ) lround( a );
}


inline short i2shrt( int a )
{
	if ( a > SHRT_MAX_HALF )
		return SHRT_MAX_HALF;
	if ( a < SHRT_MIN_HALF )
		return SHRT_MIN_HALF;
	return ( short ) a;
}


inline unsigned long d2color( double a )
{
	if ( a <= - 0.5 / ( double ) NUM_COLORS )
		return fl_get_pixel( NUM_COLORS + FL_FREE_COL1 + 2 );
	if ( a >= 1.0 + 0.5 / ( double ) NUM_COLORS )
		return fl_get_pixel( NUM_COLORS + FL_FREE_COL1 + 1 );
	else
		return fl_get_pixel( FL_FREE_COL1 + 1
							 + lround( a * ( NUM_COLORS - 1 ) ) );
}


inline int    i_max( int    a, int    b ) { return a > b ? a : b ; }
inline int    i_min( int    a, int    b ) { return a < b ? a : b ; }
inline long   l_max( long   a, long   b ) { return a > b ? a : b ; }
inline long   l_min( long   a, long   b ) { return a < b ? a : b ; }
inline float  f_max( float  a, float  b ) { return a > b ? a : b; }
inline float  f_min( float  a, float  b ) { return a < b ? a : b; }
inline double d_max( double a, double b ) { return a > b ? a : b; }
inline double d_min( double a, double b ) { return a < b ? a : b; }
