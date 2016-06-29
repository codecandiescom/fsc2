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


#include "fsc2.h"


static bool check_lock_file( int          fd,
                             const char * fn );


/*----------------------------------------------------------------*
 * Tries to create a UUCP style lock file. According to version 2.2
 * of the Filesystem Hierarchy Standard lock files for serial ports
 * must be created in '/var/lock', following  the naming convention
 * that the file name starts with "LCK..", followed by the base name
 * of the device file. E.g. for the device file '/dev/ttyS0' the lock
 * file 'LCK..ttyS0' has to be created. According to the same stan-
 * dard, the lock file must contain the process identifier (PID)
 * as a ten byte ASCII decimal number (left-padded wit spaces),
 * with a trailing new-line (HDB UUCP format). This function takes
 * the device name as the argument and then tries to obtain the lock.
 *----------------------------------------------------------------*/

bool
fsc2_obtain_uucp_lock( const char * volatile name )
{
    int fd;
	char *fn = NULL;
    char buf[ 13 ];
    mode_t mask;


    fsc2_assert( name && *name );

    if ( ( fn = strrchr( name, '/' ) ) )
    {
        if ( ! *++fn )
        {
            print( FATAL, "Invalid device file name '%s'.\n", name );
            return false;
        }

        name = fn;
        fn = NULL;
    }

    TRY
    {
        fn = get_string( "%s%sLCK..%s", LOCK_DIR, slash( LOCK_DIR ), name );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
        return false;

    raise_permissions( );

	/* Try to open the lock file. This might fail either because we don't
	   have the permissions or because it doesn't exist (which is ok). In
       the latter case open it, If it can be opened check it's content and
       that no other existing process is holding the lock. Then rewind it. */

    if ( ( fd = open( fn, O_RDWR ) ) == -1 )         /* File can't be opened */
	{
        if ( errno == EACCES )                       /* no access permission */
        {
            lower_permissions( );
            print( FATAL, "No permission to access lock file '%s'.\n", fn );
            T_free( fn );
            return false;
        }

        if ( errno != ENOENT )    /* other errors except file does not exist */
        {
            lower_permissions( );
            print( FATAL, "Can't access lock file '%s'.\n", fn );
            T_free( fn );
            return false;
        }

        mask = umask( 022 );

        if ( ( fd = open( fn, O_WRONLY | O_CREAT | O_EXCL, 0666 ) ) == -1 )
        {
            lower_permissions( );
            print( FATAL, "Can't create lock file '%s'.\n", fn );
            T_free( fn );
            return false;
        }

        lower_permissions( );
        umask( mask );
    }
    else
    {
        if ( ! check_lock_file( fd, fn ) )
        {
            T_free( fn );
            return false;
        }

        if ( lseek( fd, 0, SEEK_SET ) != 0 )
        {
            close( fd );
            print( FATAL, "Failed to rewind lock file '%s'.\n", fn );
            T_free( fn );
            return false;
        }
    }

    snprintf( buf, sizeof buf, "%10ld\n", ( long ) getpid( ) );
    if ( write( fd, buf, strlen( buf ) ) != ( ssize_t ) strlen( buf ) )
    {
        close( fd );
        print( FATAL, "Failed to write to lock file '%s'.\n", fn );
        T_free( fn );
        return false;
    }

    close( fd );
	T_free( fn );
    return true;
}


/*---------------------------------------------------*
 * Deletes a previously created UUCP style lock file
 *---------------------------------------------------*/

void
fsc2_release_uucp_lock( const char * volatile name )
{
	char *fn = NULL;


    fsc2_assert( name && *name );

    if ( ( fn = strrchr( name, '/' ) ) )
    {
        if ( ! *++fn )
        {
            print( FATAL, "Invalid device file name '%s'.\n", name );
            return;
        }

        name = fn;
        fn = NULL;
    }

    TRY
    {
        fn = get_string( "%s%sLCK..%s", LOCK_DIR, slash( LOCK_DIR ), name );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
        return;

	raise_permissions( );
	if ( unlink( fn ) < 0 )
		print( SEVERE, "Can't remove lock file '%s'.\n", fn );
	lower_permissions( );
	T_free( fn );
}


/*----------------------------------------------------------------*
 * Tries to obtain a fcntl() write or read lock on a file.
 *----------------------------------------------------------------*/

bool
fsc2_obtain_fcntl_lock( FILE * fp,
                        int    lock_type,
                        bool   wait_for_lock )
{
    struct flock fl;


    fsc2_assert( fp && ( lock_type == F_RDLCK || lock_type == F_WRLCK ) );

    fl.l_type   = lock_type;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;

    return fcntl( fileno( fp ), wait_for_lock ? F_SETLKW : F_SETLK, &fl ) == 0;
}

    
/*-------------------------------*
 * Releases a fcntl() on a file.
 *-------------------------------*/

void
fsc2_release_fcntl_lock( FILE * fp )
{
    struct flock fl;


    fsc2_assert( fp );

    fl.l_type   = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;

    fcntl( fileno( fp ), F_SETLK, &fl );
}


/*----------------------------------------------------------------*
 * Checks the content of an existing lock file an deletes it.
 * Function gets called with raised permissions and lowers
 * them of any failure.
 *----------------------------------------------------------------*/

static
bool
check_lock_file( int          fd,
                 const char * fn )
{
    char buf[ 12 ];
    const char *bp;
	char *eptr;
    long pid = -1;


    lower_permissions( );

    /* The file should just contain a few spaces, a number and a line-feed,
       in total exactly 11 bytes */

    if ( read( fd, buf, sizeof buf ) != 11 )
    {
        close( fd );
        print( FATAL, "Can't read lock file '%s' - or it has an unknown "
               "format.\n", fn );
        return false;
    }

    buf[ 11 ] = '\0';
    bp = buf;
    while ( *bp && *bp == ' ' )
        bp++;

    errno = 0;
    if (    ! isdigit( ( int ) *bp )
         || ( pid = strtol( bp, &eptr, 10 ) ) < 0
		 || eptr == bp
		 || *eptr != '\n'
         || ( pid == LONG_MAX && errno == ERANGE ) )
    {
        close( fd );
        print( FATAL, "Lock file '%s' has unknown format.\n", fn );
        return false;
    }

    /* Check if the lock file belongs to a running process (i.e, if not try
       to delete it */

    raise_permissions( );

    if ( kill( ( pid_t ) pid, 0 ) == 0 )
    {
        lower_permissions( );
        close( fd );
        print( FATAL, "Lock file '%s' is held by another process (%ld).\n ",
               fn, pid );
        return false;
    }

    if ( errno != ESRCH )
    {
        lower_permissions( );
        close( fd );
        print( FATAL, "Can't determine if lock file '%s' is held by "
               "another process.\n", fn );
        return false;
    }

    lower_permissions( );
    return true;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
