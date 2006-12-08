/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "fsc2_config.h"


#define MAX_FILE_LEN 256
#define MAX_ADDR_LEN 20
#define MAX_BUF      1024


static char *get_string( const char * /* fmt */,
						 ... );

int main( void )
{
	FILE *fp;
	char file[ MAX_FILE_LEN ];
	char addr[ MAX_ADDR_LEN ];
	char buf[ MAX_BUF ];
	char *cmd;


	while ( 1 )
	{
        /* Read in the filename (with fill path!) */

		if ( fgets( file, MAX_FILE_LEN, stdin ) == NULL )
			return EXIT_SUCCESS;
		file[ strlen( file ) - 1 ] = '\0';

        /* Read in the address */

		if ( fgets( addr, MAX_ADDR_LEN, stdin ) == NULL )
			return EXIT_FAILURE;
		addr[ strlen( addr ) - 1 ] = '\0';

        /* Construct command line to start addr2line and pass it to popen() */

        cmd = get_string( ADDR2LINE " -C -f -e %s %s", file, addr );

		if ( ( fp = popen( cmd, "r" ) ) == NULL )
		{
			free( cmd );
			return EXIT_FAILURE;
		}

		free( cmd );

        /* Read the output of addr2line and pass it on to the parent */

		if ( fgets( buf, MAX_FILE_LEN, fp ) == NULL )
			return EXIT_FAILURE;
		fprintf( stdout, "%s", buf );

		if ( fgets( buf, MAX_FILE_LEN, fp ) == NULL )
			return EXIT_FAILURE;
		fprintf( stdout, "%s", buf );
		fflush( stdout );

		pclose( fp );
	}

	return EXIT_SUCCESS;
}


#define GET_STRING_TRY_LENGTH 128

static char *get_string( const char * fmt,
						 ... )
{
    char *c = NULL;
	char *tmp;
    size_t len = GET_STRING_TRY_LENGTH;
    va_list ap;
    int wr;


    while ( 1 )
    {
		tmp = c;
        c = realloc( c, len );
		if ( c == NULL )
		{
			if ( tmp != NULL )
				free( tmp );
			exit( EXIT_FAILURE );
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

    return c;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
