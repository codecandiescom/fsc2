/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/

#include "fsc2.h"


/*-----------------------------------------------------------------*/
/* Function allocates memory for a string with one extra character */
/* for the end-of-string null-byte.                                */
/*-----------------------------------------------------------------*/

char *get_string( size_t len )
{
	return T_malloc( len + 1 );
}


/*---------------------------------------------------------------*/
/* Converts all upper case characters in a string to lower case. */
/*---------------------------------------------------------------*/

char *string_to_lower( char *str )
{
	char *ptr;


	if ( str == NULL )
		return NULL;
	for ( ptr = str; *ptr; ptr++ )
		if ( isupper( *ptr ) )
			*ptr = tolower( *ptr );

	return str;
}


/*---------------------------------------------------*/
/* This routine returns a copy of a piece of memory. */
/*---------------------------------------------------*/

void *get_memcpy( const void *array, size_t size )
{
	void *new;

	new = T_malloc( size );
	memcpy( new, array, size );
	return new;
}


/*----------------------------------------------------------------------*/
/* Function replaces all occurrences of the character combination "\n"  */
/* in a string by the line break character '\n'. This is done in place, */
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
        return path;
    else
        return ++cp;
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
	struct sigaction sact;
	struct sigaction oact;


	/* Get some memory for the pipe command */

	TRY
	{
		pc = T_malloc( 20 + strlen( name ) );
		TRY_SUCCESS;
	}
	OTHERWISE
		return -1;                       /* FATAL: no memory */

	/* set up pipe to 'awk' (defined via AWK_PROG) and read number of lines */

	sprintf( pc, "wc -l %s 2>/dev/null", name );
	if ( ( pp = popen( pc, "r" ) ) == NULL )
	{
		T_free( pc );
		return -2;                       /* popen() failed */
	}

	fscanf( pp, "%ld", &lc );

	/* Since popen() / pclose() does a wait4() but we get the still get the
	   SIGCHLD signal we have to ignore it just here... */

	sact.sa_handler = SIG_IGN;
	sigaction( SIGCHLD, &sact, &oact );
	pclose( pp );
	sigaction( SIGCHLD, &oact, NULL );

	T_free( pc );

	/* count number of digits of number of lines */

	for ( i = lc + 1, *len = 1; ( i /= 10 ) > 0; ++( *len ) )
		;

	return lc + 1;
}


/*--------------------------------------------------------------------*/
/* This function is used all over the pogram to print error messages. */
/* Its first argument is the severity of the error, an integer in the */
/* range between 0 (FATAL) and 3 (NO_ERROR), see also global.h for    */
/* abbreviations. If the second argument, a boolean value, is set,    */
/* the name of the currently loaded EDL program and the line number   */
/* we are at is prepended to the output string. The next argument is  */
/* a format string having the same syntax as the format string for    */
/* functions like printf(). Then an unspecified number of arguments   */
/* may follow (which should correspond to the conversion specifiers   */
/* in the format string).                                             */
/* When the program is running with graphics te output goes to the    */
/* error browser, otherwise to stderr. The maximum line length to be  */
/* output is FL_BROWSER_LINELENGTH, which was defined to 2048 (at     */
/* least the last time I checked) in /usr/include/forms.h. Usually,   */
/* one can't use the complete length, because of the prepended file   */
/* name, and a few bytes are also used for other purposes. So better  */
/* keep the expected line length a bit lower (if the line would get   */
/* too long it gets truncated).                                       */
/*--------------------------------------------------------------------*/

void eprint( int severity, bool print_fl, const char *fmt, ... )
{
	char buffer[ FL_BROWSER_LINELENGTH + 1 ];
	char *cp = buffer;
	int space_left = FL_BROWSER_LINELENGTH;
	int count;
	va_list ap;


	if ( severity != NO_ERROR )
		compilation.error[ severity ] += 1;

	if ( ! just_testing )
	{
		if ( severity == FATAL )
		{
			strcpy( buffer, "@C1@f" );
			cp += 5;
			space_left -= 5;
		}

		if ( severity == SEVERE )
		{
			strcpy( buffer, "@C2@f" );
			cp += 5;
			space_left -= 5;
		}

		if ( severity == WARN )
		{
			strcpy( buffer, "@C4@f" );
			cp += 5;
			space_left -= 5;
		}

		if ( print_fl && Fname )
		{
			count = snprintf( cp, space_left, "%s:%ld: ", Fname, Lc );
			space_left -= count;
			cp += count;
		}

		va_start( ap, fmt );
		vsnprintf( cp, space_left, fmt, ap );
		va_end( ap );

		if ( I_am == PARENT )
		{
			fl_freeze_form( main_form->error_browser->form );
			fl_addto_browser_chars( main_form->error_browser, buffer );

			fl_set_browser_topline( main_form->error_browser,
				  fl_get_browser_maxline( main_form->error_browser )
				- fl_get_browser_screenlines( main_form->error_browser ) + 1 );

			fl_unfreeze_form( main_form->error_browser->form );
		}
		else
			writer( C_EPRINT, buffer );
	}
	else                               /* simple test run ? */
	{
		if ( severity != NO_ERROR )
			fprintf( stdout, "%c ", severity[ "FSW" ] );      /* Hehe... */

		if ( print_fl && Fname )
			fprintf( stdout, "%s:%ld: ", Fname, Lc );

		va_start( ap, fmt );
		vfprintf( stdout, fmt, ap );
		va_end( ap );
		fflush( stdout );
	}
}


/*------------------------------------------------------------------------*/
/* There can be only one instance of fsc2 running (except simple test     */
/* runs that only do a syntax check of an EDL program). To check if there */
/* is another instance already running a lock file is used - the file is  */
/* created at the start of the program and a write lock is set on the     */
/* whole length of the file. Then the PID and user name are written into  */
/* the lock file. Thus fsc2 can find out who is currently holding the     */
/* lock and tell the user about it. The lock automatically expires when   */
/* fsc2 exits (on normal termination it will also delete the lock file).  */
/* To make this work correctly for more then one user the lock file must  */
/* belong to a special user (e.g. a user named `fsc2' belonging to a      */
/* group also named `fsc2') and the program belong to this user and have  */
/* the setuid and the setgid bit set, i.e. install it with                */
/*                                                                        */
/*            chown fsc2.fsc2 fsc2                                        */
/*            chmod 6755 fsc2                                             */
/*                                                                        */
/* Another purpose of this scheme is to get rid of shared memory segments */
/* that remain after fsc2 crashed or was killed. Therefore, all shared    */
/* memory segment have to be created with the EUID of fsc2 so that even   */
/* another user may delete them when he starts fsc2.                      */
/* This routine is mostly taken from R. Stevens' "Advanced Programming in */
/* the UNIX Environment (Addison-Wesley 1997)                             */
/*------------------------------------------------------------------------*/

bool fsc2_locking( void )
{
	int fd, flags;
	struct flock lock;
	char buf[ 128 ];
	char *name, *name_end;


	/* Try to open a lock file */

	raise_permissions( );

	if ( ( fd = open( FSC2_LOCKFILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR ) )
		 < 0 )
	{
		lower_permissions( );
		fprintf( stderr, "Error: Can't access lock file `%s'.\n",
				 FSC2_LOCKFILE );
		return FAIL;
	}

	/* Set a write lock */

	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

	if ( fcntl( fd, F_SETLK, &lock ) == -1 )           /* if lock failed */
	{
		fprintf( stderr, "Error: fsc2 is already running." );

		/* Try to read the PID of the process holding the lock as well as the
		   user name from the lock file and append it to the error message */

		if ( read( fd, ( void * ) buf, 127 ) != -1 &&
			 ( name = strchr( buf, '\n' ) + 1 ) != NULL )
		{
			*( name - 1 ) = '\0';
			fprintf( stderr, " Its PID is %s", buf );
			if ( ( name_end = strchr( name, '\n' ) ) != NULL )
			{
				*name_end = '\0';
				fprintf( stderr, " and the user is `%s'.\n", name );
			}
			else
				fprintf( stderr, ".\n" );
		}
		else
			fprintf( stderr, "\n" );

		lower_permissions( );
		return FAIL;
	}

	/* Truncate to zero length, write process ID and user name into the lock
	   file and get the close-on-exec flag */

	sprintf( buf, "%d\n%s\n", getpid( ), getpwuid( getuid( ) )->pw_name );
	if ( ftruncate( fd, 0 ) < 0 ||
		 write( fd, buf, strlen( buf ) ) != ( ssize_t ) strlen( buf ) ||
		 ( flags = fcntl( fd, F_GETFD, 0 ) ) < 0 )
	{
		unlink( FSC2_LOCKFILE );
		lower_permissions( );
		fprintf( stderr, "Error: Can't write lock file `%s'.\n",
				 FSC2_LOCKFILE );
		return FAIL;
	}

	/* Finally set the close-on-exec flag */

	flags |= FD_CLOEXEC;

	if ( fcntl( fd, F_GETFD, flags ) < 0 )
	{
		unlink( FSC2_LOCKFILE );
		lower_permissions( );
		fprintf( stderr, "Error: Can't write lock file `%s'.\n",
				 FSC2_LOCKFILE );
		return FAIL;
	}

	lower_permissions( );
	return OK;
}


/*---------------------------------------------------------------------*/
/* Functions checks if a supplied input string is identical to one of  */
/* `max' alternatives, pointed to by `altern', but neglecting the case */
/* of the characters. Leading and trailing white space is removed from */
/* the input string. The comparison is case insensitive. The function  */
/* returns the index of the found altenative (i.e. a number between 0  */
/* and max - 1) or -1 if none of the alternatives was identical to the */
/* input string.                                                       */
/*---------------------------------------------------------------------*/

int is_in( const char *supplied_in, const char **altern, int max )
{
	char *in, *cpy;
	const char *a;
	int count;


	fsc2_assert( supplied_in != NULL && altern != NULL );

	/* Get copy of input string and get rid of leading and trailing white
	   space */

	in = cpy = T_strdup( supplied_in );
	while ( isspace( *in ) )
		in++;
	while( isspace( cpy[ strlen( cpy ) - 1 ] ) )
		cpy[ strlen( cpy ) - 1 ] = '\0';

	/* Now check if the cleaned up input string is identical to one of the
	   alternatives */

	for ( cpy = in, a = *altern, count = 0; a && count < max;
		  count++, a += strlen( a ) + 1 )
		if ( ! strcasecmp( in, a ) )
			break;

	T_free( cpy );

	return ( a && count < max ) ? count : -1;
}


/*------------------------------------------------------------------------*/
/* Function converts intensities into rgb values (between 0 and 255). For */
/* values below 0 a dark kind of violet is returned, for values above 1 a */
/* creamy shade of white. The interval [ 0, 1 ] itself is subdivided into */
/* 6 subintervals at the points defined by the array `p' and rgb colours  */
/* ranging from blue via cyan, green and yellow to red are calculated     */
/* with `v' defining the intensities of the three primary colours at the  */
/* endpoints of the intervals and using linear interpolation in between.  */
/*------------------------------------------------------------------------*/

void i2rgb( double h, int *rgb )
{
	int i, j;
	double p[ 7 ] = { 0.0, 0.125, 0.4, 0.5, 0.6, 0.875, 1.0 };
	int v[ 3 ][ 7 ] = { {  64,   0,   0,  32, 233, 255, 191 },     /* RED   */
						{   0,  32, 233, 255, 233,  32,   0 },     /* GREEN */
						{ 191, 255, 233,  32,   0,   0,   0 } };   /* BLUE  */
	double scale;


	if ( h < p[ 0 ] )           /* return very dark blue for values below 0 */
	{
		rgb[ RED   ] = 72;
		rgb[ GREEN ] =  0;
		rgb[ BLUE  ] = 72;
		return;
	}

	for ( i = 0; i < 6; i++ )
	{
		if ( p[ i ] == p[ i + 1 ] || h > p[ i + 1 ] )
			continue;

		scale = ( h - p[ i ] ) / ( p[ i + 1 ] - p[ i ] );
		for ( j = RED; j <= BLUE; j++ )
			rgb[ j ] = ( int ) ( v[ j ][ i ]
								 + ( v[ j ][ i + 1 ] - v[ j ][ i ] ) * scale );
		return;
	}

	rgb[ RED   ] = 255;           /* return creamy white for values above 1 */
	rgb[ GREEN ] = 248;
	rgb[ BLUE  ] = 220;
}


/*----------------------------------------------------------------*/
/* Function creates a set of colours in XFORMs internal color map */
/* for use in 2D graphics (NUM_COLORS is defined in global.h).    */
/*----------------------------------------------------------------*/

void create_colors( void )
{
    FL_COLOR i;
	int rgb[ 3 ];


	/* Create the colours between blue and red */

	for ( i = 0; i < NUM_COLORS; i++ )
	{
		i2rgb( ( double ) i / ( double ) ( NUM_COLORS - 1 ), rgb );
		fl_mapcolor( i + FL_FREE_COL1 + 1, 
					 rgb[ RED ], rgb[ GREEN ], rgb[ BLUE ] );
	}

	/* Finally create colours for values too small or too large */

	i2rgb( -1.0, rgb );
	fl_mapcolor( NUM_COLORS + FL_FREE_COL1 + 1,
				 rgb[ RED ], rgb[ GREEN ], rgb[ BLUE ] );
	i2rgb( 2.0, rgb );
	fl_mapcolor( NUM_COLORS + FL_FREE_COL1 + 2,
				 rgb[ RED ], rgb[ GREEN ], rgb[ BLUE ] );
}


/***********************************************************************/
/* The following functions are that short that inlining them seemed to */
/* be a good idea...                                                   */
/***********************************************************************/


/*---------------------------------------------------------------------*/
/* The program starts with the EUID and EGID set to the ones of fsc2,  */
/* but these privileges get dropped immediately. Only for some special */
/* actions (like dealing with shared memory and lock and log files)    */
/* this function is called to change the EUID and EGID to the one of   */
/* fsc2.                                                               */
/*---------------------------------------------------------------------*/

inline void raise_permissions( void )
{
	seteuid( EUID );
	setegid( EGID );
}


/*---------------------------------------------------------------------*/
/* This function sets the EUID and EGID to the one of the user running */
/* the program.                                                        */
/*---------------------------------------------------------------------*/

inline void lower_permissions( void )
{
	seteuid( getuid( ) );
	setegid( getgid( ) );
}


/*--------------------------------------------------------------------------*/
/* Returns the pixel value of an entry in XFORMs internal colour map from   */
/* the colours set in create_colors(). For values slightly above 1 as well  */
/* as for values just below 0 only one colour pixel value is returned while */
/* for intermediate values one of NUM_COLORS (as defined in global.h) is    */
/* returned, depending on the value.                                        */
/*--------------------------------------------------------------------------*/

inline unsigned long d2color( double a )
{
	long index;


	index = lround( a * ( NUM_COLORS - 1 ) );

	if ( index < 0 )
		return fl_get_pixel( NUM_COLORS + FL_FREE_COL1 + 1 );
	else if ( index < NUM_COLORS )
		return fl_get_pixel( FL_FREE_COL1 + 1 + index );
	else
		return fl_get_pixel( NUM_COLORS + FL_FREE_COL1 + 2 );
}


/*-------------------------------------------------------------------------*/
/* The next two functions do a conversion of double or integer values to   */
/* short. Both are exclusively used in the conversion of data to points to */
/* be drawn on the screen via a XPoint structure which contains two short  */
/* ints. To avoid overflows in the calculations we restrict the values to  */
/* half the allowed range of short ints - thus allowing canvas sizes of up */
/* to half the size of a short int.                                        */
/*-------------------------------------------------------------------------*/

#define SHRT_MAX_HALF ( SHRT_MAX >> 1 )
#define SHRT_MIN_HALF ( SHRT_MIN >> 1 )


inline short d2shrt( double a )
{
	if ( a > SHRT_MAX_HALF )
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


inline int    i_max( int    a, int    b ) { return a > b ? a : b; }
inline int    i_min( int    a, int    b ) { return a < b ? a : b; }
inline long   l_max( long   a, long   b ) { return a > b ? a : b; }
inline long   l_min( long   a, long   b ) { return a < b ? a : b; }
inline float  f_max( float  a, float  b ) { return a > b ? a : b; }
inline float  f_min( float  a, float  b ) { return a < b ? a : b; }
inline double d_max( double a, double b ) { return a > b ? a : b; }
inline double d_min( double a, double b ) { return a < b ? a : b; }


inline long lround( double x ) { return ( long ) ( 2 * x ) - ( long ) x; }

