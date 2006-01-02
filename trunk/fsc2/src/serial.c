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


#include "fsc2.h"
#include "serial.h"


/* If NUM_SERIAL_PORTS isn't defined or its value is less than 1 serial
   ports can't be used. In this case we just supply functions that return
   something indicating a failure or throw an exception. */

#if ! defined( NUM_SERIAL_PORTS ) || NUM_SERIAL_PORTS < 1

/*-----------------------------------------------*
 * Just tell user about the errors of his way...
 *-----------------------------------------------*/

void fsc2_request_serial_port( int          sn  UNUSED_ARG,
							   const char * dev_name )
{
	eprint( FATAL, UNSET, "%s: Device needs serial port but fsc2 was "
			"not compiled with support for serial port access.\n", dev_name );
	THROW( EXCEPTION );
}


/*--------------------*
 * Nothing to be done
 *--------------------*/

void fsc2_serial_init( void )
{
}


/*--------------------*
 * Nothing to be done
 *--------------------*/

void fsc2_serial_exp_init( const char * log_file_name  UNUSED_ARG ,
						   int          log_level UNUSED_ARG )
{
}


/*--------------------*
 * Nothing to be done
 *--------------------*/

void fsc2_serial_end_of_exp_handling( void )
{
}


/*--------------------*
 * Nothing to be done
 *--------------------*/

void fsc2_serial_cleanup( void )
{
}


/*--------------------*
 * Nothing to be done
 *--------------------*/

void fsc2_final_serial_cleanup( void )
{
}


/*-------------------------------------------------------------*
 * Return NULL to indicate failure, a correctly written module
 * would never call the function anyway.
 *-------------------------------------------------------------*/

struct termios *fsc2_serial_open( int          sn        UNUSED_ARG,
								  const char * dev_name  UNUSED_ARG,
								  int          flags     UNUSED_ARG )
{
	errno = EACCES;
	return NULL;
}


/*--------------------*
 * Nothing to be done
 *--------------------*/

void fsc2_serial_close( int sn  UNUSED_ARG )
{
}


/*-------------------------------*
 * Return -1 to indicate failure
 *-------------------------------*/

ssize_t fsc2_serial_write( int          sn              UNUSED_ARG,
						   const void * buf             UNUSED_ARG ,
						   size_t       count           UNUSED_ARG,
						   long         us_wait         UNUSED_ARG,
						   bool         quit_on_signal  UNUSED_ARG )
{
	errno = EBADF;
	return -1;
}


/*-------------------------------*
 * Return -1 to indicate failure
 *-------------------------------*/

ssize_t fsc2_serial_read( int    sn              UNUSED_ARG,
						  void * buf             UNUSED_ARG ,
						  size_t count           UNUSED_ARG,
						  long   us_wait         UNUSED_ARG,
						  bool   quit_on_signal  UNUSED_ARG )
{
	errno = EBADF;
	return -1;
}


/*-------------------------------*
 * Return -1 to indicate failure
 *-------------------------------*/

int fsc2_tcgetattr( int              sn         UNUSED_ARG,
					struct termios * termios_p  UNUSED_ARG )
{
	errno = EBADF;
	return -1;
}


/*-------------------------------*
 * Return -1 to indicate failure
 *-------------------------------*/

int fsc2_tcsetattr( int              sn                UNUSED_ARG,
					int              optional_actions  UNUSED_ARG,
					struct termios * termios_p         UNUSED_ARG )
{
	errno = EBADF;
	return -1;
}


/*-------------------------------*
 * Return -1 to indicate failure
 *-------------------------------*/

int fsc2_tcsendbreak( int sn        UNUSED_ARG,
					  int duration  UNUSED_ARG )
{
	errno = EBADF;
	return -1;
}


/*-------------------------------*
 * Return -1 to indicate failure
 *-------------------------------*/

int fsc2_tcdrain( int sn UNUSED_ARG )
{
	errno = EBADF;
	return -1;
}


/*-------------------------------*
 * Return -1 to indicate failure
 *-------------------------------*/

int fsc2_tcflush( int sn              UNUSED_ARG,
				  int queue_selector  UNUSED_ARG )
{
	errno = EBADF;
	return -1;
}


/*-------------------------------*
 * Return -1 to indicate failure
 *-------------------------------*/

int fsc2_tcflow( int sn      UNUSED_ARG,
				 int action  UNUSED_ARG )
{
	errno = EBADF;
	return -1;
}


#else    /* if there are serial ports we can use */


#include <sys/timeb.h>

/* Definition of log levels allowed in calls of fsc2_serial_exp_init()
   since they are already may have been defined in the GPIB module only
   define them hen they aren't already known */

#if ! defined LL_NONE
#define  LL_NONE  0    /* log nothing */
#endif
#if ! defined LL_ERR
#define  LL_ERR   1    /* log errors only */
#endif
#if ! defined LL_CE
#define  LL_CE    2    /* log function calls and function exits */
#endif
#if ! defined LL_ALL
#define  LL_ALL   3    /* log calls with parameters and function exits */
#endif


/* If NUM_SERIAL_PORTS is defined and is larger than 0 we need full support
   for serial ports. */

static struct {
	bool in_use;
	const char* dev_name;
	char* dev_file;
	char *lock_file;
	bool have_lock;
	bool is_open;
	int fd;
	struct termios old_tio,
		           new_tio;
} Serial_Port[ NUM_SERIAL_PORTS ];

static int ll;                       /* log level                            */
static FILE *fsc2_serial_log = NULL; /* file pointer of serial port log file */



static bool get_serial_lock( int sn );
static void remove_serial_lock( int sn );
static void fsc2_serial_log_date( void );
static void fsc2_serial_log_function_start( const char *function,
											const char *dev_name );
static void fsc2_serial_log_function_end( const char *function,
										  const char *dev_name );
static void fsc2_serial_log_message( const char *fmt, ... );


/*-------------------------------------------------------------------*
 * This function must be called by device modules that need a serial
 * port. Here it is checked if the requested serial port is still
 * available and if the user has access permissions to the serial
 * ports device file. If one of these conditions isn't satisfied the
 * function throws an exception.
 * -> 1. Serial port number - must be smaller than compiled in
 *       constant NUM_SERIAL_PORTS
 *    2. Name of the device the serial port is requested for
 *-------------------------------------------------------------------*/

void fsc2_request_serial_port( int          sn,
							   const char * dev_name )
{
	/* Do some sanity checks on the serial port number */

	if ( sn >= NUM_SERIAL_PORTS || sn < 0 )
	{
		if ( NUM_SERIAL_PORTS > 1 )
			eprint( FATAL, UNSET, "%s: Serial port number %d out of valid "
					"range (0-%d).\n", dev_name, sn, NUM_SERIAL_PORTS - 1 );
		else if ( NUM_SERIAL_PORTS == 1 )
			eprint( FATAL, UNSET, "%s: Serial port number %d out of valid "
					"range (only 0 is allowed).\n", dev_name, sn );
		else
			eprint( FATAL, UNSET, "%s: Invalid serial port number %d.\n",
					dev_name, sn );
		THROW( EXCEPTION );
	}

	/* Check that serial port hasn't already been requested by another
	   module */

	if ( Serial_Port[ sn ].in_use )
	{
		eprint( FATAL, UNSET, "%s: Requested serial port %d (i.e. /dev/ttyS%d "
				"or COM%d) is already used by device %s.\n", dev_name, sn, sn,
				sn + 1, Serial_Port[ sn ].dev_name );
		THROW( EXCEPTION );
	}

	Serial_Port[ sn ].in_use    = SET;
	Serial_Port[ sn ].have_lock = UNSET;
	Serial_Port[ sn ].is_open   = UNSET;
	Serial_Port[ sn ].dev_name   = dev_name;

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
}


/*-----------------------------------------------------------------------*
 * This function is called internally (i.e. not from modules) before the
 * start of an experiment in order to open the log file.
 * ->
 *  * Pointer to the name of log file - if the pointer is NULL or does
 *    not point to a non-empty string stderr used.
 *  * log level, either LL_NONE, LL_ERR, LL_CE or LL_ALL
 *    (if log level is LL_NONE 'log_file_name' is not used at all)
 *-----------------------------------------------------------------------*/

void fsc2_serial_exp_init( const char * log_file_name,
						   int          log_level )
{
	int i;


	ll = log_level;

	if ( ll < LL_NONE )
	{
		ll = LL_NONE;
		return;
	}

	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
		if ( Serial_Port[ i ].in_use )
			return;

	if ( i == NUM_SERIAL_PORTS )
		return;

	if ( ll > LL_ALL )
		ll = LL_ALL;

	raise_permissions( );

    if ( log_file_name == NULL || *log_file_name == '\0' )
	{
        fsc2_serial_log = stderr;
		fprintf( stderr, "Serial port log file not specified, using stderr "
				 "instead\n" );
	}
	else
	{
		if ( ( fsc2_serial_log = fopen( log_file_name, "w" ) ) == NULL )
		{
			fsc2_serial_log = stderr;
			fprintf( stderr, "Can't open serial port log file %s, using "
					 "stderr instead.\n", log_file_name );
		}
		else
			chmod( log_file_name,
				   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
	}

	lower_permissions( );

    fsc2_serial_log_message( "Starting serial port logging\n" );
}


/*----------------------------------------------------------------------*
 * This function is called only once at the start of fsc2 to initialise
 * the structure used in granting access to the serial ports.
 *----------------------------------------------------------------------*/

void fsc2_serial_init( void )
{
	int i;


	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
	{
		Serial_Port[ i ].dev_file    = NULL;
		Serial_Port[ i ].dev_name    = NULL;
		Serial_Port[ i ].lock_file   = UNSET;
		Serial_Port[ i ].in_use      = UNSET;
		Serial_Port[ i ].have_lock   = UNSET;
		Serial_Port[ i ].is_open     = UNSET;
		Serial_Port[ i ].fd          = -1;
	}
}


/*-------------------------------------------------------------------*
 * Function that gets called after the end of an experiment to close
 * the serial ports used during the experiment.
 *-------------------------------------------------------------------*/

void fsc2_serial_cleanup( void )
{
	int i;


	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
		if ( Serial_Port[ i ].is_open )
			fsc2_serial_close( i );

	if ( fsc2_serial_log != NULL )
	{
		fsc2_serial_log_message( "Closed all serial ports\n" );
		raise_permissions( );
		fclose( fsc2_serial_log );
		fsc2_serial_log = NULL;
		lower_permissions( );
	}
}


/*----------------------------------------------------------------*
 * This function is called when a new EDL file is loaded to reset
 * the structures used in granting access to the serial ports.
 *----------------------------------------------------------------*/

void fsc2_final_serial_cleanup( void )
{
	int i;


	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
	{
		if ( Serial_Port[ i ].is_open )
			fsc2_serial_close( i );

		if ( Serial_Port[ i ].in_use )
		{
			Serial_Port[ i ].dev_file =
								   CHAR_P T_free( Serial_Port[ i ].dev_file );
			Serial_Port[ i ].lock_file =
								   CHAR_P T_free( Serial_Port[ i ].lock_file );
		}

		Serial_Port[ i ].dev_name   = NULL;
		Serial_Port[ i ].in_use    = UNSET;
	}
}


/*--------------------------------------------------------------------*
 * This function should be called by device modules that need to open
 * a serial port device file. Instead of the device file name as in
 * the open() function this routine expects the number of the serial
 * port and the name of the device to make it possible to check if
 * the device has requested this port. The third parameter is, as in
 * the open() function, the flags used for opening the device file.
 *--------------------------------------------------------------------*/

struct termios *fsc2_serial_open( int          sn,
								  const char * dev_name,
								  int          flags )
{
	int fd;
	int fd_flags;


	/* Check that the device name argument is reasonable */

	if ( dev_name == NULL || *dev_name == '\0' )
	{
		fsc2_serial_log_message( "Error: Invalid 'dev_name' argument in call "
								 "of function fsc2_serial_init()\n" );
		errno = EINVAL;
		return NULL;
	}

	fsc2_serial_log_function_start( "fsc2_serial_open", dev_name );

	/* Check that the serial prot number is within the allowed range */

	if ( sn >= NUM_SERIAL_PORTS || sn < 0 )
	{
		fsc2_serial_log_message( "Error: Invalid serial port number %d in "
								 "call from module %s\n", sn, dev_name );
		fsc2_serial_log_function_start( "fsc2_serial_open", dev_name );
		errno = EBADF;
		return NULL;
	}

	/* Check if serial port has been requested by the module */

	if ( ! Serial_Port[ sn ].in_use ||
		 strcmp( Serial_Port[ sn ].dev_name, dev_name ) )
	{
		fsc2_serial_log_message( "Error: Serial port %d hasn't been claimed "
								 "by module %s\n", sn, dev_name );
		fsc2_serial_log_function_start( "fsc2_serial_open", dev_name );
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
		fsc2_serial_log_function_end( "fsc2_serial_open", dev_name );
		fsc2_serial_log_message( "Serial port %d for module %s already "
								 "open\n", sn, dev_name );
		fsc2_serial_log_function_start( "fsc2_serial_open", dev_name );
		return &Serial_Port[ sn ].new_tio;
	}

	/* Try to create a lock file */

	if ( ! get_serial_lock( sn ) )
	{
		fsc2_serial_log_message( "Error: Failure to create lock file\n" );
		fsc2_serial_log_function_start( "fsc2_serial_open", dev_name );
		errno = EACCES;
		return NULL;
	}

	/* Try to open the serial port */

	raise_permissions( );
	if ( ( fd = open( Serial_Port[ sn ].dev_file, flags ) ) < 0 )
	{
		remove_serial_lock( sn );
		lower_permissions( );
		fsc2_serial_log_message( "Error: Failure to open serial port %d in "
								 "function fsc2_serial_init()\n", sn );
		fsc2_serial_log_function_start( "fsc2_serial_open", dev_name );
		errno = EACCES;
		return NULL;
	}

	/* Set the close-on-exec flag for the file descriptor */

	if ( ( fd_flags = fcntl( fd, F_GETFD, 0 ) ) < 0 )
		fd_flags = 0;

	fcntl( fd, F_SETFD, fd_flags | FD_CLOEXEC );

	/* Get the the current terminal settings and copy them to a structure
	   that gets passed back to the caller */

	tcgetattr( fd, &Serial_Port[ sn ].old_tio );
	memcpy( &Serial_Port[ sn ].new_tio, &Serial_Port[ sn ].old_tio,
			sizeof( struct termios ) );
	lower_permissions( );

	Serial_Port[ sn ].fd = fd;
	Serial_Port[ sn ].is_open = SET;

	fsc2_serial_log_message( "Successfully opened serial port %d for device ",
							 "%s\n", sn, dev_name );
	fsc2_serial_log_function_end( "fsc2_serial_open", dev_name );

	return &Serial_Port[ sn ].new_tio;
}


/*-----------------------------------------------------------------------*
 * Closes the device file for the serial port and removes the lock file.
 *-----------------------------------------------------------------------*/

void fsc2_serial_close( int sn )
{
	if ( Serial_Port[ sn ].dev_name )
		fsc2_serial_log_function_start( "fsc2_serial_close",
										Serial_Port[ sn ].dev_name );

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

	if ( Serial_Port[ sn ].dev_name )
	{
		fsc2_serial_log_message( "Closed serial port %d for device %s\n",
								 sn, Serial_Port[ sn ].dev_name );
		fsc2_serial_log_function_end( "fsc2_serial_close",
									  Serial_Port[ sn ].dev_name );
	}
}


/*-------------------------------------------------------------------*
 * Function for sending data via one of the serial ports. It expects
 * 5 arguments, first the number of the serial port, then a buffer
 * with the data and its length, a timeout in us we are supposed to
 * wait for data to become writeable to the serial port and finally
 * a flag that tells if the function is to return immediately if a
 * signal is received before any data could be send.
 * If the timeout value in 'us_wait' is zero the function won't wait
 * for the serial port to become ready for writing, if it's negative
 * the the function potentially will wait indefinitely long.
 * The function returns the number of written bytes or -1 when an
 * error happened. A value of 0 is returned when no data could be
 * written, possibly because a signal was received before writing
 * started.
 *-------------------------------------------------------------------*/

ssize_t fsc2_serial_write( int          sn,
						   const void * buf,
						   size_t       count,
						   long         us_wait,
						   bool         quit_on_signal )
{
	ssize_t write_count;
	fd_set wrds;
	struct timeval timeout;
	struct timeval before, after;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		fsc2_serial_log_message( "Error: Invalid serial port %d in call of "
								 "function fsc2_serial_write()\n", sn );
		errno = EBADF;
		return -1;
	}

	fsc2_serial_log_function_start( "fsc2_serial_write",
									Serial_Port[ sn ].dev_name );

	if ( ll == LL_ALL )
	{
		if ( us_wait == 0 )
			fsc2_serial_log_message( "Expected to write %ld bytes without "
									 "delay to serial port %d:\n%.*s\n",
									 ( long int ) count, sn, count, buf );
		else if ( us_wait < 0 )
			fsc2_serial_log_message( "Expected to write %ld bytes to serial "
									 "port %d:\n%.*s\n", ( long int ) count,
									 sn, count, buf );
		else
			fsc2_serial_log_message( "Expected to write %ld bytes within %ld "
									 "ms to serial port %d:\n%.*s\n",
									 ( long int ) count, us_wait / 1000, sn,
									 count, buf );
	}

	raise_permissions( );

	if ( us_wait != 0 )
	{
		FD_ZERO( &wrds );
		FD_SET( Serial_Port[ sn ].fd, &wrds );

	write_retry:

		if ( us_wait > 0 )
		{
			timeout.tv_sec  = us_wait / 1000000;
			timeout.tv_usec = us_wait % 1000000;
		}

		gettimeofday( &before, NULL );

		switch ( select( Serial_Port[ sn ].fd + 1, NULL, &wrds, NULL,
						 us_wait > 0 ? &timeout : NULL ) )
		{
			case -1 :
				if ( errno != EINTR )
				{
					fsc2_serial_log_message( "Error: select() returned "
											 "indicating error in "
											 "fsc2_serial_write()\n" );
					fsc2_serial_log_function_end( "fsc2_serial_write",
												  Serial_Port[ sn ].dev_name );
					lower_permissions( );
					return -1;
				}

				if ( ! quit_on_signal )
				{
					gettimeofday( &after, NULL );
					us_wait -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
							   - ( before.tv_sec * 1000000 + before.tv_usec );
					if ( us_wait > 0 )
						goto write_retry;
				}

				fsc2_serial_log_message( "Error: select aborted due to "
										 "signal in fsc2_serial_write()\n" );
				fsc2_serial_log_function_end( "fsc2_serial_write",
											  Serial_Port[ sn ].dev_name );
				lower_permissions( );
				return 0;
				
			case 0 :
				fsc2_serial_log_message( "Error: writing aborted due to "
										 "timeout in fsc2_serial_write()\n" );
				fsc2_serial_log_function_end( "fsc2_serial_write",
											  Serial_Port[ sn ].dev_name );
				lower_permissions( );
				return 0;
		}
	}

	while ( ( write_count = write( Serial_Port[ sn ].fd, buf, count ) ) < 0 
			&& errno == EINTR && ! quit_on_signal )
		/* empty */ ;


	if ( write_count < 0 && errno == EINTR )
	{
		fsc2_serial_log_message( "Error: writing aborted due to signal, "
								 "0 bytes got written in "
								 "fsc2_serial_write()\n" );
		write_count = 0;
	}
	else
	{
		if ( ll == LL_ALL )
			fsc2_serial_log_message( "Wrote %ul bytes to serial port %d\n",
									 ( unsigned long ) write_count );
	}

	lower_permissions( );

	fsc2_serial_log_function_end( "fsc2_serial_write",
								  Serial_Port[ sn ].dev_name );

	return write_count;
}


/*---------------------------------------------------------------------*
 * Function for reading data from one of the serial ports. It expects
 * 5 arguments, first the number of the serial port, then a buffer and
 * its length for returning the read in data, a timeout in us we are
 * supposed to wait for data to readable on the serial port and
 * finally a flag that tells if the function is to return immediately
 * if as signal is received before any data could be read.
 * if the timeout value in 'us_wait' is zero the function won't wait
 * for data to appear on the serial port, when it is negative the
 * function waits indefinitely long for data.
 * The function returns the number of read in data or -1 when an error
 * happened. A value of 0 is returned when no data could be read in,
 * possibly because a signal was received before reading started.
 *---------------------------------------------------------------------*/

ssize_t fsc2_serial_read( int    sn,
						  void * buf,
						  size_t count,
						  long   us_wait,
						  bool   quit_on_signal )
{
	ssize_t read_count;
	fd_set rfds;
	struct timeval timeout;
	struct timeval before, after;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		fsc2_serial_log_message( "Error: Invalid serial port %d in call of "
								 "function fsc2_serial_read()\n", sn );
		errno = EBADF;
		return -1;
	}

	fsc2_serial_log_function_start( "fsc2_serial_read",
									Serial_Port[ sn ].dev_name );

	if ( ll == LL_ALL )
	{
		if ( us_wait == 0 )
			fsc2_serial_log_message( "Expected to read up to %ld bytes "
									 "without delay from serial port "
									 "%d:\n%.*s\n", ( long int ) count, sn,
									 count, buf );
		else if ( us_wait < 0 )
			fsc2_serial_log_message( "Expected to read up to %ld bytes from "
									 "serial port %d:\n%.*s\n",
									 ( long int ) count, sn, count, buf );
		else
			fsc2_serial_log_message( "Expected to read up to  %ld bytes "
									 "within %ld ms from serial port "
									 "%d:\n%.*s\n", ( long int ) count,
									 us_wait / 1000, sn, count, buf );
	}

	raise_permissions( );

	/* If there is a zero timeout period wait for data for the specified
	   timeout. A negative timeout means wait forever. */

	if ( us_wait != 0 )
	{
		FD_ZERO( &rfds );
		FD_SET( Serial_Port[ sn ].fd, &rfds );

	read_retry:

		if ( us_wait > 0 )
		{
			timeout.tv_sec  = us_wait / 1000000;
			timeout.tv_usec = us_wait % 1000000;
		}

		gettimeofday( &before, NULL );

		switch ( select( Serial_Port[ sn ].fd + 1, &rfds, NULL, NULL,
						 us_wait > 0 ? &timeout : NULL ) )
		{
			case -1 :
				if ( errno != EINTR )
				{
					fsc2_serial_log_message( "Error: select() returned "
											 "indicating error in "
											 "fsc2_serial_read()\n" );
					fsc2_serial_log_function_end( "fsc2_serial_read",
												  Serial_Port[ sn ].dev_name );
					lower_permissions( );
					return -1;
				}

				if ( ! quit_on_signal )
				{
					gettimeofday( &after, NULL );
					us_wait -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
							   - ( before.tv_sec * 1000000 + before.tv_usec );
					if ( us_wait > 0 )
						goto read_retry;
				}

				fsc2_serial_log_message( "Error: select aborted due to "
										 "signal in fsc2_serial_read()\n" );
				fsc2_serial_log_function_end( "fsc2_serial_read",
											  Serial_Port[ sn ].dev_name );
				lower_permissions( );
				return 0;

			case 0 :
				fsc2_serial_log_message( "Error: reading aborted due to "
										 "timeout in fsc2_serial_read()\n" );
				fsc2_serial_log_function_end( "fsc2_serial_write",
											  Serial_Port[ sn ].dev_name );
				lower_permissions( );
				return 0;
		}
	}

	while ( ( read_count = read( Serial_Port[ sn ].fd, buf, count ) ) < 0 
			&& errno == EINTR && ! quit_on_signal )
		/* empty */ ;
		
	if ( read_count == 0 )
	{
		if ( ll == LL_ALL )
			fsc2_serial_log_message( "No bytes could be read\n" );
	}
	else if ( read_count < 0 && errno == EINTR )
	{
		fsc2_serial_log_message( "Error: reading aborted due to signal, "
								 "0 bytes got read\n" );
		read_count = 0;
	}
	else
	{
		if ( ll == LL_ALL )
			fsc2_serial_log_message( "Read %lu bytes:\n%.*s\n",
									 ( unsigned long ) read_count, read_count,
									 buf );
	}

	lower_permissions( );

	fsc2_serial_log_function_end( "fsc2_serial_read",
								  Serial_Port[ sn ].dev_name );

	return read_count;
}


/*--------------------------------------------------------------*
 * Tries to create a UUCP style lock file for a serial port.
 * According to version 2.2 of the Filesystem Hierachy Standard
 * the lock files must be stored in /var/lock following the
 * naming convention that the file name starts with "LCK..",
 * followed by the base name of the device file. E.g. for the
 * device file '/dev/ttyS0' the lock file 'LCK..ttyS0' has to
 * be created.
 * According to the same standard, the lockfile must contain
 * the process identifier (PID) as a ten byte ASCII decimal
 * number, with a trailing newline (HDB UUCP format).
 *--------------------------------------------------------------*/

#ifdef SERIAL_LOCK_DIR
static bool get_serial_lock( int sn )
{
	int fd;
	char buf[ 128 ];
	const char *bp;
	int n;
	long pid = -1;
	int mask;


	/* Try to open lock file - if it exists we can check its content and
	   decide what to do, i.e. find out if the process it belonged to is
	   dead, in which case it can be removed */

	fsc2_serial_log_function_start( "get_serial_lock",
									Serial_Port[ sn ].dev_name );

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

			if ( *bp && isdigit( ( unsigned char ) *bp ) )
				n = sscanf( bp, "%ld", &pid );

			if ( n == 0 || n == EOF )
			{
				lower_permissions( );
				fsc2_serial_log_message( "Error: Lock file '%s' for serial "
										 "port %d has unknown format.\n",
										 Serial_Port[ sn ].lock_file, sn );
				fsc2_serial_log_function_end( "get_serial_lock",
											  Serial_Port[ sn ].dev_name );
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
					fsc2_serial_log_message( "Error: Can't delete stale lock "
											 "'%s' file for serial port %d.\n",
											 Serial_Port[ sn ].lock_file, sn );
					fsc2_serial_log_function_end( "get_serial_lock",
												  Serial_Port[ sn ].dev_name );
					return FAIL;
				}
			}
			else
			{
				lower_permissions( );
				fsc2_serial_log_message( "Error: Serial port %d is locked by "
										 "another program according to lock "
										 "file '%s'.\n",
										 sn, Serial_Port[ sn ].lock_file );
				fsc2_serial_log_function_end( "get_serial_lock",
											  Serial_Port[ sn ].dev_name );
				return FAIL;
			}
		}
		else
		{
			lower_permissions( );
			fsc2_serial_log_message( "Error: Can't read lock file '%s' for "
									 "serial port %d or it has an unknown "
									 "format.\n",
									 Serial_Port[ sn ].lock_file, sn );
			fsc2_serial_log_function_end( "get_serial_lock",
										  Serial_Port[ sn ].dev_name );
			return FAIL;
		}
	}
	else                               /* couldn't open lock file, check why */
	{
		if ( errno == EACCES )                       /* no access permission */
		{
			lower_permissions( );
			fsc2_serial_log_message( "Error: No permission to access lock "
									 "file '%s' for serial port %d.\n",
									 Serial_Port[ sn ].lock_file, sn );
			fsc2_serial_log_function_end( "get_serial_lock",
										  Serial_Port[ sn ].dev_name );
			return FAIL;
		}

		if ( errno != ENOENT )    /* other errors except file does not exist */
		{
			lower_permissions( );
			fsc2_serial_log_message( "Error: Can't access lock file '%s' for "
									 "serial port %d .\n",
									 Serial_Port[ sn ].lock_file, sn );
			fsc2_serial_log_function_end( "get_serial_lock",
										  Serial_Port[ sn ].dev_name );
			return FAIL;
		}
	}

    /* Create lockfile compatible with UUCP-1.2 */

    mask = umask( 022 );
    if ( ( fd = open( Serial_Port[ sn ].lock_file,
					  O_WRONLY | O_CREAT | O_EXCL, 0666 ) ) < 0 )
	{
		lower_permissions( );
        fsc2_serial_log_message( "Error: Can't create lockfile '%s' for "
								 "serial port %d.\n",
								 Serial_Port[ sn ].lock_file, sn );
		fsc2_serial_log_function_end( "get_serial_lock",
									  Serial_Port[ sn ].dev_name );
        return FAIL;
    }

	umask( mask );
    chown( Serial_Port[ sn ].lock_file, Fsc2_Internals.EUID,
		   Fsc2_Internals.EGID );
    snprintf( buf, sizeof( buf ), "%10ld\n", ( long ) getpid( ) );
    write( fd, buf, strlen( buf ) );
    close( fd );

	lower_permissions( );
	Serial_Port[ sn ].have_lock = SET;

	fsc2_serial_log_function_end( "get_serial_lock",
								  Serial_Port[ sn ].dev_name );

	return OK;
}
#else
static bool get_serial_lock( int sn  UNUSED_ARG )
{
	return OK;
}
#endif


/*-------------------------------------------------------------*
 * Deletes the previously created lock file for a serial port.
 *-------------------------------------------------------------*/

static void remove_serial_lock( int sn )
{
#ifdef SERIAL_LOCK_DIR
	fsc2_serial_log_function_end( "remove_serial_lock",
								  Serial_Port[ sn ].dev_name );

	if ( Serial_Port[ sn ].have_lock )
	{
		raise_permissions( );
		unlink( Serial_Port[ sn ].lock_file );
		lower_permissions( );
		Serial_Port[ sn ].have_lock = UNSET;
	}

	fsc2_serial_log_function_end( "remove_serial_lock",
								  Serial_Port[ sn ].dev_name );
#endif
}


/*------------------------------*
 * Replacement for tcgetattr(3)
 *------------------------------*/

int fsc2_tcgetattr( int              sn,
					struct termios * termios_p )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		fsc2_serial_log_message( "Error: Invalid serial port %d in call of "
								 "function fsc2_tcgetattr()\n", sn );
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcgetattr( Serial_Port[ sn ].fd, termios_p );
	lower_permissions( );

	return ret_val;
}


/*------------------------------*
 * Replacement for tcsetattr(3)
 *------------------------------*/

int fsc2_tcsetattr( int              sn,
					int              optional_actions,
					struct termios * termios_p )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		fsc2_serial_log_message( "Error: Invalid serial port %d in call of "
								 "function fsc2_tcsetattr()\n", sn );
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcsetattr( Serial_Port[ sn ].fd, optional_actions, termios_p );
	lower_permissions( );

	return ret_val;
}


/*--------------------------------*
 * Replacement for tcsendbreak(3)
 *--------------------------------*/

int fsc2_tcsendbreak( int sn,
					  int duration )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		fsc2_serial_log_message( "Error: Invalid serial port %d in call of "
								 "function fsc2_tcsendbreak()\n", sn );
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcsendbreak( Serial_Port[ sn ].fd, duration );
	lower_permissions( );

	return ret_val;
}


/*----------------------------*
 * Replacement for tcdrain(3)
 *----------------------------*/

int fsc2_tcdrain( int sn )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		fsc2_serial_log_message( "Error: Invalid serial port %d in call of "
								 "function fsc2_tcdrain()\n", sn );
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcdrain( Serial_Port[ sn ].fd );
	lower_permissions( );

	return ret_val;
}


/*----------------------------*
 * Replacement for tcflush(3)
 *----------------------------*/

int fsc2_tcflush( int sn,
				  int queue_selector )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		fsc2_serial_log_message( "Error: Invalid serial port %d in call of "
								 "function fsc2_tcflush()\n", sn );
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcflush( Serial_Port[ sn ].fd, queue_selector );
	lower_permissions( );

	return ret_val;
}


/*---------------------------*
 * Replacement for tcflow(3)
 *---------------------------*/

int fsc2_tcflow( int sn,
				 int action )
{
	int ret_val;


	if ( sn >= NUM_SERIAL_PORTS || sn < 0 || ! Serial_Port[ sn ].is_open )
	{
		fsc2_serial_log_message( "Error: Invalid serial port %d in call of "
								 "function fsc2_tcflow()\n", sn );
		errno = EBADF;
		return -1;
	}

	raise_permissions( );
	ret_val = tcflow( Serial_Port[ sn ].fd, action );
	lower_permissions( );

	return ret_val;
}



/*---------------------------------------------------------*
 * fsc2_serial_log_date() writes the date to the log file.
 *---------------------------------------------------------*/

static void fsc2_serial_log_date( void )
{
    char tc[ 26 ];
	struct timeb mt;
    time_t t;


    t = time( NULL );
    strcpy( tc, asctime( localtime( &t ) ) );
	tc[ 10 ] = '\0';
	tc[ 19 ] = '\0';
    tc[ 24 ] = '\0';
	ftime( &mt );
    fprintf( fsc2_serial_log, "[%s %s %s.%03d] ", tc, tc + 20, tc + 11,
			 mt.millitm );
}


/*--------------------------------------------------------------*
 * fsc2_serial_log_function_start() logs the call of a function
 * by appending a short message to the log file.
 * ->
 *  * name of the function
 *  * name of the device involved
 *--------------------------------------------------------------*/

static void fsc2_serial_log_function_start( const char * function,
											const char * dev_name )
{
	if ( fsc2_serial_log == NULL || ll < LL_CE )
		return;

	raise_permissions( );
    fsc2_serial_log_date( );
    fprintf( fsc2_serial_log, "Call of %s(), dev = %s\n", function, dev_name );
    fflush( fsc2_serial_log );
	lower_permissions( );
}


/*---------------------------------------------------------*
 * fsc2_serial_log_function_end() logs the completion of a
 * function by appending a short message to the log file.
 * ->
 *  * name of the function
 *  * name of the device involved
 *---------------------------------------------------------*/

static void fsc2_serial_log_function_end( const char * function,
										  const char * dev_name )
{
	if ( fsc2_serial_log == NULL || ll < LL_CE )
		return;

	raise_permissions( );
	fsc2_serial_log_date( );
	fprintf( fsc2_serial_log, "Exit of %s(), dev = %s\n", function, dev_name );
	fflush( fsc2_serial_log );
	lower_permissions( );
}


/*-----------------------------------------------------*
 * Function for printing out a message to the log file
 *-----------------------------------------------------*/

static void fsc2_serial_log_message( const char *fmt,
									 ... )
{
	va_list ap;


	if ( fsc2_serial_log == NULL || ll == LL_NONE )
		return;

	raise_permissions( );
	fsc2_serial_log_date( );
	va_start( ap, fmt );
	vfprintf( fsc2_serial_log, fmt, ap );
	va_end( ap );
	lower_permissions( );
}


#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
