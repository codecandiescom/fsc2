/*
 *  Copyright (C) 1999-2016 Jens Thoms Toerring
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include "fsc2_config.h"


static char *get_string( const char * /* fmt */,
						 ... );

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

#define MAX_FILE_LEN 256
#define MAX_ADDR_LEN 20
#define MAX_BUF      1024

int
main( void )
{
	while ( true )
	{
        /* Read in the filename (with full path!) */

        char file[ MAX_FILE_LEN ];
		if ( ! fgets( file, MAX_FILE_LEN, stdin ) )
			return EXIT_SUCCESS;
		file[ strlen( file ) - 1 ] = '\0';

        /* Read in the address */

        char addr[ MAX_ADDR_LEN ];
		if ( ! fgets( addr, MAX_ADDR_LEN, stdin ) )
			return EXIT_FAILURE;
		addr[ strlen( addr ) - 1 ] = '\0';

        /* Construct command line to start addr2line and pass it to popen() */

        char * cmd = get_string( ADDR2LINE " -C -f -e %s %s", file, addr );

        FILE *fp = popen( cmd, "r" );
		if ( ! fp )
		{
			free( cmd );
			return EXIT_FAILURE;
		}

		free( cmd );

        /* Read the output of addr2line and pass it on to the parent */

        char buf[ MAX_BUF ];
		if ( ! fgets( buf, MAX_FILE_LEN, fp ) )
        {
            pclose( fp );
			return EXIT_FAILURE;
        }

		fprintf( stdout, "%s", buf );

        if ( ! fgets( buf, MAX_FILE_LEN, fp ) )
        {
            pclose( fp );
            return EXIT_FAILURE;
        }

        fprintf( stdout, "%s", buf );
        fflush( stdout );

		pclose( fp );
	}

	return EXIT_SUCCESS;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

#define GET_STRING_TRY_LENGTH 128

static
char *
get_string( const char * fmt,
            ... )
{
    char * c = NULL;
    size_t len = GET_STRING_TRY_LENGTH;

    while ( true )
    {
		char * tmp = c;
        c = realloc( c, len );
		if ( c == NULL )
		{
			if ( tmp != NULL )
				free( tmp );
			exit( EXIT_FAILURE );
		}

        va_list ap;
        va_start( ap, fmt );
        int wr = vsnprintf( c, len, fmt, ap );
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

    return c;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
