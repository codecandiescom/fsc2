/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#include "fsc2.h"


/*----------------------------------------------------------------*
 * Tries to create a UUCP style lock file. According to version
 * 2.2 of the Filesystem Hierarchy Standard lock files for serial
 * ports must be created in '/var/lock', following  the naming
 * convention that the file name starts with "LCK..", followed
 * by the base name of the device file. E.g. for the device file
 * '/dev/ttyS0' the lock file 'LCK..ttyS0' has to be created.
 * According to the same standard, the lock file must contain
 * the process identifier (PID) as a ten byte ASCII decimal
 * number, with a trailing newline (HDB UUCP format).
 * This function takes the base name as the argument and then
 * tries to obtain the lock.
 *----------------------------------------------------------------*/

bool
fsc2_obtain_lock( const char * name )
{
    int fd;
	char *fn = NULL;
    char buf[ 13 ];
    const char *bp;
	char *eptr;
    long pid = -1;
    mode_t mask;


    TRY
    {
        fn = get_string( "%s%sLCK..%s", LOCK_DIR, slash( LOCK_DIR ), name );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
        return FAIL;

    raise_permissions( );

	/* Try to open the lock file. This might fail either because we don't
	   have the permissions or because it doesn't exist */

    if ( ( fd = open( fn, O_RDWR ) ) < 0 )
	{
        lower_permissions( );

        if ( errno == EACCES )                       /* no access permission */
        {
            print( FATAL, "No permission to access lock file '%s'.\n", fn );
            return FAIL;
        }

        if ( errno != ENOENT )    /* other errors except file does not exist */
        {
            print( FATAL, "Can't access lock file '%s'.\n", fn );
            return FAIL;
        }
	}
	else
    {
        lower_permissions( );

		/* It should just contain a few spaces, a number and a linefeed,
		   in total exactly 11 bytes */

        if ( read( fd, buf, sizeof buf ) == 11 )
        {
            buf[ 11 ] = '\0';
            bp = buf;
            while ( *bp && *bp == ' ' )
                bp++;

			if (    *bp == '\0'
				 || ( pid = strtol( bp, &eptr, 10 ) ) < 0
				 || eptr == bp
				 || *eptr != '\n'
				 || ( pid == LONG_MAX && errno == ERANGE ) )
			{
				close( fd );
				print( FATAL, "Lock file '%s' has unknown format.\n", fn );
                return FAIL;
            }

            /* Check if the lock file belongs to a running process, if not
               try to delete it */

			raise_permissions( );

            if ( kill( ( pid_t ) pid, 0 ) < 0 && errno == ESRCH )
            {
				lower_permissions( );

                if ( unlink( fn ) < 0 )
                {
					close( fd );
					print( FATAL, "Can't delete stale lock file '%s'.\n", fn );
                    return FAIL;
                }
            }
            else
            {
                lower_permissions( );
				close( fd );
                print( FATAL, "Lock file '%s' is hold by another process "
					   "(%ld).\n ", fn, pid );
                return FAIL;
            }
        }
        else
        {
			close( fd );
            print( FATAL, "Can't read lock file '%s' or it has an unknown "
				   "format.\n", fn );
            return FAIL;
        }
    }

	if ( fd < 0 )
	{

		/* Create lock file compatible with UUCP-1.2 */

		mask = umask( 022 );

		raise_permissions( );

		if ( ( fd = open( fn, O_WRONLY | O_CREAT | O_EXCL, 0666 ) ) < 0 )
		{
			lower_permissions( );
			print( FATAL, "Can't create lock file '%s'.\n", fn );
			return FAIL;
		}

		umask( mask );
	}
	else if ( lseek( fd, 0, SEEK_SET ) != 0 )
	{
		lower_permissions( );
		print( FATAL, "Can't rewind lock file '%s'.\n", fn );
		return FAIL;
	}

    snprintf( buf, sizeof buf, "%10ld\n", ( long ) getpid( ) );
    if ( write( fd, buf, strlen( buf ) ) != ( ssize_t ) strlen( buf ) )
        print( SEVERE, "Failed to write to lock file '%s', might cause trouble "
			   "later on.\n", fn );

    close( fd );
	T_free( fn );
    lower_permissions( );

    return OK;
}


/*----------------------------------------*
 * Deletes a previously created lock file
 *----------------------------------------*/

void
fsc2_release_lock( const char * name )
{
	char *fn = NULL;


    TRY
    {
        fn = get_string( "%s%sLCK..%s", LOCK_DIR, slash( LOCK_DIR ), name );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
        return;

	raise_permissions( );
	if ( unlink( fn ) < 0 )
		print( SEVERE, "Can't remove lock file '%s'\n", fn );
	lower_permissions( );
	T_free( fn );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
