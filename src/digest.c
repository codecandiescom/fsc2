/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2.h"

static struct {
	size_t     count;
	size_t     orig_count;
	Digest_T * data;
} digests = { 0, 0, NULL };


static int digest_cmp( const void * a,
					   const void * b );
static char * digest_file_name( bool need_one );


/*-------------------------------------------------------------------*
 * Called on initialization of program, tries to read in a file with
 * SHA1 digest values of already tested EDL scripts and makes up a
 * (sorted) list of all these digest values.
 *-------------------------------------------------------------------*/

void
digest_init( void )
{
	FILE *fp;
	char * digest_name = digest_file_name( UNSET );


	raise_permissions( );

    if (    digest_name == NULL
		 || ( fp = fopen( digest_name, "r" ) ) == NULL )
	{
		lower_permissions( );
        return;
	}

	T_free( digest_name );

	if ( fread( &digests.count, sizeof digests.count, 1, fp ) != 1 )
	{
		fclose( fp );
		lower_permissions( );
		digests.count = 0;
		return;
	}

	if ( digests.count == 0 )
	{
		fclose( fp );
		lower_permissions( );
		return;
	}

	digests.data = T_malloc( digests.count * sizeof *digests.data );

	if ( fread( digests.data, sizeof *digests.data, digests.count, fp )
		                                                      != digests.count )
	{
		digests.data = T_free( digests.data );
		digests.count = 0;
	}

	fclose( fp );
	lower_permissions( );

	qsort( digests.data, digests.count, sizeof *digests.data, digest_cmp );
	digests.orig_count = digests.count;

	return;
}


/*------------------------------------------*
 * Returns if a digest value is in the list
 *------------------------------------------*/

bool
digest_check_if_tested( Digest_T digest )
{
	if ( digests.count == 0 )
		return UNSET;

	return bsearch( digest, digests.data, digests.count, sizeof *digests.data,
					digest_cmp ) ? SET : UNSET;
}


/*-------------------------------------*
 * Adds a new digest value to the list
 *-------------------------------------*/

void
digest_add( Digest_T digest )
{
	digests.data = T_realloc( digests.data,
							  ( digests.count + 1 ) * sizeof * digests.data );
	memcpy( digests.data + digests.count++, digest, sizeof *digests.data );
	qsort( digests.data, digests.count, sizeof *digests.data, digest_cmp );
}


/*----------------------------------------------------------*
 * Called when program ends, writes out the list of digests
 * and then releases memory
 *----------------------------------------------------------*/

void
digest_at_exit( void )
{
	FILE *fp;
	char *digest_name;


	if ( digests.orig_count == digests.count )
		return;

	raise_permissions( );

	if (    ( digest_name = digest_file_name( SET ) ) == NULL
		 || ( fp = fopen( digest_name, "w" ) ) == NULL )
	{
		lower_permissions( );
		return;
	}

	if (    fwrite( &digests.count, sizeof digests.count, 1, fp ) != 1
		 || fwrite( digests.data, sizeof *digests.data, digests.count, fp )
			                                                 != digests.count )
		unlink( digest_name );

	lower_permissions( );

	T_free( digest_name );
	T_free( digests.data );
}
	

/*---------------------------------------------------------------------*
 * Tries to find the name of the file with the SHA1 digests of scripts
 * that already have been tested.
 *---------------------------------------------------------------------*/

static char *
digest_file_name( bool need_one )
{
	char *digest_name = NULL;
    struct stat buf;
    char *ld_path;
    char *ld = NULL;
    char *ldc;


    /* If we're supposed only to use the files from the source directories
       only try to use those */

    if ( Fsc2_Internals.cmdline_flags & LOCAL_EXEC )
    {
        digest_name = get_string( confdir "Digests" );

        if ( lstat( digest_name, &buf ) < 0 && ! need_one )
            digest_name = T_free( digest_name );
    }
    else
    {
        /* As usual, we first check paths defined by the environment variable
           'LD_LIBRARY_PATH' and then the compiled-in path (except when this
           is a check run). */

        if ( ( ld_path = getenv( "LD_LIBRARY_PATH" ) ) != NULL )
        {
            ld = T_strdup( ld_path );

            for ( ldc = strtok( ld, ":" ); ldc != NULL;
                  ldc = strtok( NULL, ":" ) )
            {
                digest_name = get_string( "%s%sDigests", ldc, slash( ldc ) );
                if ( lstat( digest_name, &buf ) == 0 )
                    break;
                digest_name = T_free( digest_name );
            }

            T_free( ld );
        }

        if (    digest_name == NULL
             && ! ( Fsc2_Internals.cmdline_flags & DO_CHECK ) )
        {
            digest_name = T_strdup( libdir "Digests" );

            if ( lstat( digest_name, &buf ) < 0 && ! need_one )
                digest_name = T_free( digest_name );
        }
    }

	return digest_name;
}


/*------------------------------------------*
 * Function for comparing two digest values
 *------------------------------------------*/

static int
digest_cmp( const void * a,
			const void * b )
{
	size_t i;


	for ( i = 0; i < sizeof( Digest_T ); i++ )
		if ( ( ( Digest_T * ) a )[ 0 ][ i ] != ( ( Digest_T * ) b )[ 0 ][ i ] )
			return ( ( Digest_T * ) a )[ 0 ][ i ] <
				   ( ( Digest_T * ) b )[ 0 ][ i ] ? -1 : 1;

	return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
