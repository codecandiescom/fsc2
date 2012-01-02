/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "bmwb.h"


/*---------------------------------------------------------------------*
 * The program starts with the EUID and EGID set to the ones of fsc2,
 * but these privileges get dropped immediately. Only for some special
 * actions (like dealing with shared memory and lock and log files)
 * this function is called to change the EUID and EGID to the one of
 * fsc2.
 *---------------------------------------------------------------------*/

void
raise_permissions( void )
{
    seteuid( bmwb.EUID );
    setegid( bmwb.EGID );
}


/*---------------------------------------------------------------------*
 * This function sets the EUID and EGID to the one of the user running
 * the program.
 *---------------------------------------------------------------------*/

void
lower_permissions( void )
{
    seteuid( getuid( ) );
    setegid( getgid( ) );
}


/*------------------------------------------------------------------*
 * Function for opening files with the maximum permissions of fsc2.
 * Close-on-exec flag gets set for the newly opened file.
 *------------------------------------------------------------------*/

FILE *
bmwb_fopen( const char * path,
            const char * mode )
{
    FILE *fp;


    raise_permissions( );
    fp = fopen( path, mode );

    /* Probably rarely necessary, but make sure the close-on-exec flag is
       se for the file */

    if ( fp )
    {
        int fd_flags = fcntl( fileno( fp ), F_GETFD );

        if ( fd_flags < 0 )
            fd_flags = 0;
        fcntl( fileno( fp ), F_SETFD, fd_flags | FD_CLOEXEC );
    }

    lower_permissions( );
    return fp;
}


/*------------------------------------------------------------------*
 *------------------------------------------------------------------*/

double
d_max( double  a,
	   double  b )
{
	return a > b ? a : b;
}


/*------------------------------------------------------------------*
 *------------------------------------------------------------------*/

double
d_min( double  a,
	   double  b )
{
	return a < b ? a : b;
}


/*------------------------------------------------------------------------*
 * Rounds a double to the nearest int value. On overflow largest possible
 * int gets returned, and on underflow the smallest int value. In both
 * these cases errno gets set to ERANGE.
 *------------------------------------------------------------------------*/

int
irnd( double d )
{
    if ( d >= INT_MAX + 0.5 )
    {
        errno = ERANGE;
        return INT_MAX;
    }
    if ( d <= INT_MIN - 0.5 )
    {
        errno = ERANGE;
        return INT_MIN;
    }

    return ( int ) ( d < 0.0 ? ceil( d - 0.5 ) : floor( d + 0.5 ) );
}


/*------------------------------------------------------------------*
 * Function expects a format string as printf() and arguments which
 * must correspond to the given format string and returns a string
 * of the right length into which the arguments are written. The
 * caller of the function is responsible for free-ing the string.
 * -> 1. printf()-type format string
 *    2. As many arguments as there are conversion specifiers etc.
 *       in the format string.
 * <- Pointer to character array of exactly the right length into
 *    which the string characterized by the format string has been
 *    written. On failure, i.e. if there is not enough space, the
 *    function throws an OUT_OF_MEMORY exception.
 *------------------------------------------------------------------*/

#define GET_STRING_TRY_LENGTH 128

char *
get_string( const char * fmt,
            ... )
{
    char *c = NULL;
    size_t len = GET_STRING_TRY_LENGTH;
    va_list ap;
    int wr;


    while ( 1 )
    {
		char *oc = c;

		if ( ( c = realloc( c, len ) ) == NULL )
		{
			if ( oc )
				free( oc );
			return NULL;
		}

        va_start( ap, fmt );
        wr = vsnprintf( c, len, fmt, ap );
        va_end( ap );

        if ( wr < 0 )         /* indicates not enough space with older glibs */
        {
            len *= 2;
            continue;
        }

        if ( ( size_t ) wr + 1 > len )   /* newer glibs return the number of */
        {                                /* chars needed, not counting the   */
            len = wr + 1;                /* trailing '\0'                    */
            continue;
        }

        break;
    }

    /* Trim the string down to the number of required characters */

    if ( ( size_t ) wr + 1 < len )
	{
		char *oc = c;

		if ( ( c = realloc( c, ( size_t ) wr + 1 ) ) == NULL )
			c = oc;
	}

    return c;
}


/*-----------------------------------------------------------------*
 * Function returns the string "/" if the string passed to it does
 * not end with a slash, otherwise it returns the empty string "".
 *-----------------------------------------------------------------*/


const char *
slash( const char * path )
{
    return path[ strlen( path ) - 1 ] != '/' ? "/" : "";
}


const char *
pretty_print_attenuation( void )
{
    static char buf[ 100 ] = "Microwave attenuation [dB] (ca. ";
    static char *p = buf + 32;
    double power = bmwb.max_power * pow( 10.0, - 0.1 * bmwb.attenuation );


    int expo = floor( log10( power ) );
    double fac = pow( 10, - expo + 3 );
    power  = irnd( power * fac ) / fac;

    if ( expo >= -2 )
        sprintf( p, "%.0f0 mW)", 1.0e2 * power );
    else if ( expo == -3 )
        sprintf( p, "%.1f mW)", 1.0e3 * power );
    else if ( expo == -4 || expo == -5 )
        sprintf( p, "%.0f0 uW)", 1.0e5 * power );
    else if ( expo == -6 )
        sprintf( p, "%.1f uW)", 1.0e6 * power );
    else if ( expo == -7 || expo == -8 )
        sprintf( p, "%.0f0 nW)", 1.0e8 * power );
    else
        sprintf( p, "%.0f nW)", 1.0e9 * power );

    return buf;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
