/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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
#include "serial.h"


#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0
static struct {
	bool in_use;
	const char* devname;
	char* dev_file;
	char *lock_file;
	bool have_lock;
	bool is_open;
	int fd;
	struct termios old_tio,
		           new_tio;
} Serial_Port[ NUM_SERIAL_PORTS ];
#endif

static bool get_serial_lock( int sn );
static void remove_serial_lock( int sn );


/*-------------------------------------------------------------------*/
/* This function must be called by device modules that need a serial */
/* port. Here it is checked if the requested serial port is still    */
/* available and if the user has access permissions to the serial    */
/* ports device file. If one of these conditions isn't satisfied the */
/* function throws an exception.                                     */
/* -> 1. Serial port number - must be smaller than compiled in       */
/*       constant NUM_SERIAL_PORTS                                   */
/*    2. Name of the device the serial port is requested for         */
/*-------------------------------------------------------------------*/

void fsc2_request_serial_port( int sn, const char *devname )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0

	/* Do some sanity checks on the serial port number */

	if ( sn >= NUM_SERIAL_PORTS || sn < 0 )
	{
		if ( NUM_SERIAL_PORTS > 1 )
			eprint( FATAL, UNSET, "%s: Serial port number %d out of valid "
					"range (0-%d).\n", sn, NUM_SERIAL_PORTS - 1, devname );
		else
			eprint( FATAL, UNSET, "%s: Serial port number %d out of valid "
					"range (0 is allowed only).\n", sn, devname );
		THROW( EXCEPTION );
	}

	/* Check that serial port hasn't already been requested by another
	   module */

	if ( Serial_Port[ sn ].in_use )
	{
		eprint( FATAL, UNSET, "%s: Requested serial port %d (i.e. /dev/ttyS%d "
				"or COM%d) is already used by device %s.\n", devname, sn, sn,
				sn + 1, Serial_Port[ sn ].devname );
		THROW( EXCEPTION );
	}

	Serial_Port[ sn ].in_use    = SET;
	Serial_Port[ sn ].have_lock = UNSET;
	Serial_Port[ sn ].is_open   = UNSET;
	Serial_Port[ sn ].devname   = devname;

	/* Assemble name of device file and (optionally) lock file */

	Serial_Port[ sn ].dev_file  = get_string( "/dev/ttyS%d", sn );
#ifdef SERIAL_LOCK_DIR
	if ( *( SERIAL_LOCK_DIR + strlen( SERIAL_LOCK_DIR ) - 1 ) == '/' )
		Serial_Port[ sn ].lock_file = get_string( "%sLCK..ttyS%d",
												  SERIAL_LOCK_DIR, sn );
	else
		Serial_Port[ sn ].lock_file = get_string( "%s/LCK..ttyS%d",
												  SERIAL_LOCK_DIR, sn );
#endif

#else
	sn = sn;

	eprintf( FATAL, UNSET, "%s: Device needs serial port but fsc2 was "
			 "not compiled with support for serial port access.\n", devname );
	THROW( EXCEPTION );
#endif
}


/*----------------------------------------------------------------------*/
/* This function is called only once at the start of fsc2 to initialise */
/* the structure used in granting access to the serial ports.           */
/*----------------------------------------------------------------------*/

void fsc2_serial_init( void )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0
	int i;


	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
	{
		Serial_Port[ i ].dev_file  = NULL;
		Serial_Port[ i ].devname   = NULL;
		Serial_Port[ i ].lock_file = UNSET;
		Serial_Port[ i ].in_use    = UNSET;
		Serial_Port[ i ].have_lock = UNSET;
		Serial_Port[ i ].is_open   = UNSET;
		Serial_Port[ i ].fd        = -1;
	}
#endif
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void fsc2_serial_cleanup( void )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0
	int i;


	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
		if ( Serial_Port[ i ].is_open )
			fsc2_serial_close( i );
#endif
}


/*-----------------------------------------------------------------------*/
/* This function is called after the end of an experiment (or when a new */
/* EDL file is loaded) to reset the structure used in granting access to */
/* the serial ports.                                                     */
/*-----------------------------------------------------------------------*/

void fsc2_final_serial_cleanup( void )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0
	int i;


	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
	{
		if ( Serial_Port[ i ].is_open )
			fsc2_serial_close( i );

		if ( Serial_Port[ i ].in_use )
		{
			Serial_Port[ i ].dev_file = T_free( Serial_Port[ i ].dev_file );
			Serial_Port[ i ].lock_file = T_free( Serial_Port[ i ].lock_file );
		}

		Serial_Port[ i ].devname   = NULL;
		Serial_Port[ i ].in_use    = UNSET;
	}
#endif
}


/*--------------------------------------------------------------------*/
/* This function should be called by device modules that need to open */
/* a serial port device file. Instead of the device file name as in   */
/* the open() function this routine expects the number of the serial  */
/* port and the name of the device to make it possible to check if    */
/* the device has requested this port. The third parameter is, as in  */
/* the open() function, the flags used for opening the device file.   */
/*--------------------------------------------------------------------*/

struct termios *fsc2_serial_open( int sn, const char *devname, int flags )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0
	int fd;


	/* Check that the serial prot number is within the allowed range */

	if ( sn >= NUM_SERIAL_PORTS || sn < 0 )
	{
		errno = EBADF;
		return NULL;
	}

	/* Check if serial port has been requested by the module */

	if ( ! Serial_Port[ sn ].in_use ||
		 strcmp( Serial_Port[ sn ].devname, devname ) )
	{
		errno = EACCES;
		return NULL;
	}

	/* If the port has already been opened just return the structure with the
	   current terminal settings */

	if ( Serial_Port[ sn ].is_open )
	{
		raise_permissions( );
		tcgetattr( Serial_Port[ sn ].fd, &Serial_Port[ sn ].new_tio );
		lower_permissions( );
		return &Serial_Port[ sn ].new_tio;
	}

	/* Try to create a lock file */

	if ( ! get_serial_lock( sn ) )
	{
		errno = EACCES;
		return NULL;
	}

	/* Try to open the serial port */

	raise_permissions( );
	if ( ( fd = open( Serial_Port[ sn ].dev_file, flags ) ) < 0 )
	{
		remove_serial_lock( sn );
		lower_permissions( );
		errno = EACCES;
		return NULL;
	}

	/* Get the the current terminal settings and copy them to a structure
	   that gets passed back to the caller */

	tcgetattr( fd, &Serial_Port[ sn ].old_tio );
	memcpy( &Serial_Port[ sn ].new_tio, &Serial_Port[ sn ].old_tio,
			sizeof( struct termios ) );
	lower_permissions( );

	Serial_Port[ sn ].fd = fd;
	Serial_Port[ sn ].is_open = SET;

	return &Serial_Port[ sn ].new_tio;
#else
	sn = sn;
	devname = devname;
	flags = flags;


	errno = EACCES;
	return NULL;
#endif
}


/*-----------------------------------------------------------------------*/
/* Closes the device file for the serial port and removes the lock file. */
/*-----------------------------------------------------------------------*/

void fsc2_serial_close( int sn )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0

	/* Flush the port, reset the settings back to the original state and
	   close the port */

	if ( Serial_Port[ sn ].is_open )
	{
		raise_permissions( );
		tcflush( Serial_Port[ sn ].fd, TCIFLUSH );
		tcsetattr( Serial_Port[ sn ].fd, TCSANOW, &Serial_Port[ sn ].old_tio );
		close( Serial_Port[ sn ].fd );
		lower_permissions( );
		Serial_Port[ sn ].is_open = UNSET;
	}

	/* Also remove the lock file for the serial port */

	if ( Serial_Port[ sn ].have_lock )
		remove_serial_lock( sn );
#else
	sn = sn;
#endif
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

ssize_t fsc2_serial_write( int sn, const void *buf, size_t count )
{
	ssize_t write_count;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	write_count =  write( Serial_Port[ sn ].fd, buf, count );
	lower_permissions( );

	return write_count;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

ssize_t fsc2_serial_read( int sn, void *buf, size_t count )
{
	ssize_t read_count;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	read_count = read( Serial_Port[ sn ].fd, buf, count );
	lower_permissions( );

	return read_count;
}


/*--------------------------------------------------------------*/
/* Tries to create a UUCP style lock file for a serial port.    */
/* According to version 2.2 of the Filesystem Hierachy Standard */
/* the lock files must be stored in /var/lock following the     */
/* naming convention, that the file name starts with "LCK..",   */
/* followed by the base name of the device file. E.g. for the   */
/* device file '/dev/ttyS0' the lock file 'LCK..ttyS0' has to   */
/* be created.                                                  */
/* According to the same standard, the lockfile must contain    */
/* the process identifier (PID) as a ten byte ASCII decimal     */
/* number, with a trailing newline (HDB UUCP format).           */
/*--------------------------------------------------------------*/

static bool get_serial_lock( int sn )
{
#ifdef SERIAL_LOCK_DIR
	int fd;
	char buf[ 128 ];
	const char *bp;
	int n;
	int pid = -1;
	int mask;


	/* Try to open lock file - if it exists we can check its content and
	   decide what to do, i.e. find out if the process it belonged to is
	   dead, in which case it can be removed */

	raise_permissions( );

	if ( ( fd = open( Serial_Port[ sn ].lock_file, O_RDONLY ) ) >= 0 )
	{
		n = read( fd, buf, sizeof( buf ) - 1 );
		close( fd );

		if ( n == 11 )                    /* expect standard HDB UUCP format */
		{
			buf[ n ] = '\0';
			n = 0;
			bp = buf;
			while ( *bp && *bp == ' ' )
				bp++;

			if ( *bp && isdigit( *bp ) )
				n = sscanf( bp, "%d", &pid );

			if ( n == 0 || n == EOF )
			{
				lower_permissions( );
				fprintf( stderr, "Lock file '%s' for serial port %d (COM%d) "
						 "has unknown format.\n",
						 Serial_Port[ sn ].lock_file, sn, sn + 1 );
				return FAIL;
			}

			/* Check if the lock file belongs to a running process, if not
			   try to delete it */

			if ( pid > 0 && kill( ( pid_t ) pid, 0 ) < 0 &&
				 errno == ESRCH )
			{
				if ( unlink( Serial_Port[ sn ].lock_file ) < 0 )
				{
					lower_permissions( );
					fprintf( stderr, "Can't delete stale lock '%s' file "
							 "for serial port %d (COM%d).\n",
							 Serial_Port[ sn ].lock_file, sn, sn + 1 );
					return FAIL;
				}
			}
			else
			{
				lower_permissions( );
				fprintf( stderr, "Serial port %d (COM%d) is locked by "
						 "another program according to lock file '%s'.\n",
						 sn, sn + 1, Serial_Port[ sn ].lock_file );
				return FAIL;
			}
		}
		else
		{
			lower_permissions( );
			fprintf( stderr, "Can't read lock file '%s' for serial port "
					 "%d (COM%d) or it has has unknown format.\n",
					 Serial_Port[ sn ].lock_file, sn, sn + 1 );
			return FAIL;
		}
	}
	else                               /* couldn't open lock file, check why */
	{
		if ( errno == EACCES )                       /* no access permission */
		{
			lower_permissions( );
			fprintf( stderr, "No permission to access lock file '%s' for "
					 "serial port %d (COM%d).\n",
					 Serial_Port[ sn ].lock_file, sn, sn + 1 );
			return FAIL;
		}

		if ( errno != ENOENT )    /* other errors except file does not exist */
		{
			lower_permissions( );
			fprintf( stderr, "Can't access lock file '%s' for serial port %d "
					 "(COM%d).\n", Serial_Port[ sn ].lock_file, sn, sn + 1 );
			return FAIL;
		}
	}

    /* Create lockfile compatible with UUCP-1.2 */

    mask = umask( 022 );
    if ( ( fd = open( Serial_Port[ sn ].lock_file,
					  O_WRONLY | O_CREAT | O_EXCL, 0666 ) ) < 0 )
	{
		lower_permissions( );
        fprintf( stderr, "Can't create lockfile '%s' for serial port %d "
				 "(COM%d).\n", Serial_Port[ sn ].lock_file, sn, sn + 1 );
        return FAIL;
    }

	umask( mask );
    chown( Serial_Port[ sn ].lock_file, Internals.EUID, Internals.EGID );
    snprintf( buf, sizeof( buf ), "%10d\n", ( int ) getpid( ) );
    write( fd, buf, strlen( buf ) );
    close( fd );

	lower_permissions( );
	Serial_Port[ sn ].have_lock = SET;
#else
	sn = sn;
#endif
	return OK;
}


/*-------------------------------------------------------------*/
/* Deletes the previously created lock file for a serial port. */
/*-------------------------------------------------------------*/

static void remove_serial_lock( int sn )
{
#ifdef SERIAL_LOCK_DIR
	if ( Serial_Port[ sn ].have_lock )
	{
		raise_permissions( );
		unlink( Serial_Port[ sn ].lock_file );
		lower_permissions( );
		Serial_Port[ sn ].have_lock = UNSET;
	}
#endif
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int fsc2_tcgetattr( int sn, struct termios *termios_p )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcgetattr( Serial_Port[ sn ].fd, termios_p );
	lower_permissions( );

	return ret_val;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int fsc2_tcsetattr( int sn, int optional_actions, struct termios *termios_p )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcsetattr( Serial_Port[ sn ].fd, optional_actions, termios_p );
	lower_permissions( );

	return ret_val;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int fsc2_tcsendbreak( int sn, int duration )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val =  tcsendbreak( Serial_Port[ sn ].fd, duration );
	lower_permissions( );

	return ret_val;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int fsc2_tcdrain( int sn )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcdrain( Serial_Port[ sn ].fd );
	lower_permissions( );

	return ret_val;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int fsc2_tcflush( int sn, int queue_selector )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcflush( Serial_Port[ sn ].fd, queue_selector );
	lower_permissions( );

	return ret_val;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int fsc2_tcflow( int sn, int action )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcflow( Serial_Port[ sn ].fd, action );
	lower_permissions( );

	return ret_val;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
