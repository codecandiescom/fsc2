/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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
#include "serial.h"
#include <sys/timeb.h>


#if ! defined WITHOUT_SERIAL_PORTS


#define READ_MODE   1
#define WRITE_MODE  2

static struct {
    char           * dev_file;
    char           * dev_name;
    bool             is_open;
    bool             have_lock;
    int              fd;
    int              flags;
    int              mode;
    struct termios   old_tio,
                     new_tio;
    FILE           * log_fp;
} *Serial_Ports = NULL;

static int Num_Serial_Ports = 0;

static int ll;                       /* log level                            */

static void fsc2_serial_log_date( int sn );
static void fsc2_serial_log_message( int          sn,
                                     const char * fmt,
                                     ...               );
static void fsc2_serial_log_function_start( int          sn,
                                            const char * function );
static void fsc2_serial_log_function_end( int          sn,
                                          const char * function );

static void open_serial_log( int sn );

static void close_serial_log( int sn );


/*-------------------------------------------------------------------*
 * This function must be called by device modules that need a serial
 * port. It checks if the requested serial port is still available
 * and if the user has access permissions to the serial ports device
 * file. If one of these conditions isn't satisfied the function
 * throws an exception.
 * -> 1. Serial port device file
 *    2. Name of the device the serial port is requested for
 * <- Index the serial port is to be accessed by
 *-------------------------------------------------------------------*/

int
fsc2_request_serial_port( const char * dev_file,
                          const char * dev_name )
{
    int i;
    struct stat stat_buf;
    char *real_name;


    /* Check that the device name is reasonable */

    if ( dev_name == NULL || *dev_name == '\0' )
    {
        eprint( FATAL, UNSET, "Invalid 'dev_name' argument in call "
                "of function fsc2_request_serial_port()\n" );
		THROW( EXCEPTION );
    }

	/* Check that the device file is a char device file, check the "real"
       file and not symbolic links */

    if ( stat( dev_file, &stat_buf ) == -1 )
    {
        if ( errno == EACCES )
            eprint( FATAL, UNSET, "%s: Missing permissions to access serial "
                    "port device file '%s'.\n", dev_name, dev_file );
        else if ( errno == ENOENT || errno == ENOTDIR )
            eprint( FATAL, UNSET, "%s: Serial port evice file '%s' does not "
                    "exist.\n", dev_name, dev_file );
        else if ( errno == ENOMEM )
            eprint( FATAL, UNSET, "%s: Running out of memory while testing "
                    "serial port device file '%s'.\n", dev_name, dev_file );
        else if ( errno == ENAMETOOLONG )
            eprint( FATAL, UNSET, "%s: Name of serial port device file '%s' "
                    "is too long.\n", dev_name, dev_file );
        else
            eprint( FATAL, UNSET, "%s: Unexpected error while testing serial "
                    "port device file '%s'.\n", dev_name, dev_file );
        THROW( EXCEPTION );
    }

    if ( ! S_ISCHR( stat_buf.st_mode ) )
    {
        eprint( FATAL, UNSET, "%s: File '%s' isn't a device file for a serial "
                "port.\n", dev_name, dev_file );
        THROW( EXCEPTION );
    }

    /* If the name given as the device file is a symbolic link obtain the
       real name - otherwise we might be fooled into accepting the same
       device file more than once */

    if ( lstat( dev_file, &stat_buf ) == -1 )
    {
        if ( errno == EACCES )
            eprint( FATAL, UNSET, "%s: Missing permissions to access serial "
                    "port device file '%s'.\n", dev_name, dev_file );
        else if ( errno == ENOENT || errno == ENOTDIR )
            eprint( FATAL, UNSET, "%s: A serial port device file named '%s' "
                    "does not exist.\n",
                    dev_name, dev_file );
        else if ( errno == ENOMEM )
            eprint( FATAL, UNSET, "%s: Running out of memory while testing "
                    "serial port device file '%s'.\n", dev_name, dev_file );
        else if ( errno == ENAMETOOLONG )
            eprint( FATAL, UNSET, "%s: Name of serial port device file '%s' "
                    "is too long.\n", dev_name, dev_file );
        else
            eprint( FATAL, UNSET, "%s: Unexpected error while testing serial "
                    "port device file '%s'.\n", dev_name, dev_file );
        THROW( EXCEPTION );
    }

    if ( S_ISLNK( stat_buf.st_mode ) )
    {
        size_t pathmax = get_pathmax( );
        ssize_t length;

        /* We need memory for the name of the file the link points to */

        real_name = T_malloc( pathmax + 2 );
        if (    ( length = readlink( dev_file, real_name, pathmax + 2 ) ) < 0
             || length > pathmax )
        {
            eprint( FATAL, UNSET, "%s: Can't follow symbolic link that is the "
                    "file given as the serial port device file '%s'.\n",
                    dev_name, dev_file );
            T_free( real_name );
            THROW( EXCEPTION );
        }

        real_name[ length ] = '\0';

        TRY
        {
            real_name = T_realloc( real_name, strlen( real_name ) + 1 );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( real_name );
            RETHROW( ) ;
        }
    }
    else
        real_name = T_strdup( dev_file );

    /* Check that device file for the serial port hasn't already been claimed
       by another module */

	for ( i = 0; i < Num_Serial_Ports; i++ )
		if ( ! strcmp( Serial_Ports[ i ].dev_file, real_name ) )
		{
			eprint( FATAL, UNSET, "%s: Requested serial port '%s' is already "
					"in use by device '%s'.\n", dev_name, real_name,
                    Serial_Ports[ i ].dev_name );
            T_free( real_name );
			THROW( EXCEPTION );
		}

    /* Get memory for one more structure and initialize it */

    TRY
    {
        Serial_Ports = T_realloc( Serial_Ports,
							  ( Num_Serial_Ports + 1 ) * sizeof *Serial_Ports );
        Serial_Ports[ Num_Serial_Ports ].dev_name  = T_strdup( dev_name );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( real_name );
        RETHROW( );
    }

    Serial_Ports[ Num_Serial_Ports ].dev_file  = real_name;
    Serial_Ports[ Num_Serial_Ports ].have_lock = UNSET;
    Serial_Ports[ Num_Serial_Ports ].is_open   = UNSET;
	Serial_Ports[ Num_Serial_Ports ].fd        = -1;
	Serial_Ports[ Num_Serial_Ports ].log_fp    = NULL;

	return Num_Serial_Ports++;
}


/*-----------------------------------------------------------------------*
 * This function is called internally (i.e. not from modules) before the
 * start of an experiment in order to open the log file.
 * ->
 *  * log level, either LL_NONE, LL_ERR, LL_CE or LL_ALL
 *    (if log level is LL_NONE 'log_file_name' is not used at all)
 * Returns 1 on success, 0 otherwise
 *-----------------------------------------------------------------------*/

bool
fsc2_serial_exp_init( int log_level )
{
    int i;


    if ( Num_Serial_Ports == 0 )
        return OK;

    ll = log_level;

    if ( ll <= LL_NONE )
        ll = LL_NONE;

    if ( ll > LL_ALL )
        ll = LL_ALL;

    /* Lock and create log files for all devices */

    for ( i = 0; i < Num_Serial_Ports; i++ )
    {
        if ( ! ( Serial_Ports[ i ].have_lock =
	               fsc2_obtain_uucp_lock( strrchr( Serial_Ports[ i ].dev_file,
                                                   '/' ) + 1 ) ) )
        {
            print( FATAL, "Device %s is locked by another process.\n",
                   Serial_Ports[ i ].dev_name );

            for ( i--; i > 0; i-- )
            {
                fsc2_release_uucp_lock( strrchr( Serial_Ports[ i ].dev_file,
                                                 '/' ) + 1 );
                Serial_Ports[ i ].have_lock = UNSET;
                close_serial_log( i );
            }

            return FAIL;
        }

        open_serial_log( i );
    }

    return OK;
}


/*-------------------------------------------------------------------*
 * Function that gets called after the end of an experiment to close
 * the serial ports used during the experiment as well as log files
 *-------------------------------------------------------------------*/

void
fsc2_serial_cleanup( void )
{
    int i = Num_Serial_Ports;


    /* Close the device files and log files and release the locks for
       all devices */

    while ( i-- > 0 )
    {
        if ( Serial_Ports[ i ].is_open )
            fsc2_serial_close( i );
        else if ( Serial_Ports[ i ].have_lock )
        {
            fsc2_release_uucp_lock( strrchr( Serial_Ports[ i ].dev_file, '/' )
                                    + 1 );
            Serial_Ports[ i ].have_lock = UNSET;
        }

        close_serial_log( i );
    }
}


/*----------------------------------------------------------------*
 * This function is called when a new EDL file is loaded to reset
 * the structures used in granting access to the serial ports.
 *----------------------------------------------------------------*/

void
fsc2_serial_final_reset( void )
{
    int i = Num_Serial_Ports;


    while ( i-- > 0 )
    {
        if ( Serial_Ports[ i ].dev_name )
            Serial_Ports[ i ].dev_name  = T_free( Serial_Ports[ i ].dev_name );
        if ( Serial_Ports[ i ].dev_file )
            Serial_Ports[ i ].dev_file  = T_free( Serial_Ports[ i ].dev_file );
    }

    if ( Serial_Ports )
        Serial_Ports = T_free( Serial_Ports );
    Num_Serial_Ports = 0;
}


/*--------------------------------------------------------------------*
 * This function should be called by device modules that need to open
 * a serial port device file. Instead of the device file name as the open()
 * function expects this routine is to receive the number of the serial
 * port. The second parameter is, as for the open() function, the flags
 * used for opening the device file (this will normally be either O_RDONLY,
 * O_WRONLY or O_RDWR, further necessary flags get set automatically).
 *--------------------------------------------------------------------*/

struct termios *
fsc2_serial_open( int sn,
                  int flags )
{
    int fd;
    int fd_flags;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Make sure the serial port index argument is ok */

    if ( sn < 0 || sn >= Num_Serial_Ports )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_serial_open().\n", sn );
        THROW( EXCEPTION );
    }

    /* Check that the flags contain either O_WRONLY, O_RDONLY or O_RDWR */

    if (    ( flags & ( O_WRONLY | O_RDONLY | O_RDWR ) ) != O_WRONLY
         && ( flags & ( O_WRONLY | O_RDONLY | O_RDWR ) ) != O_RDONLY
         && ( flags & ( O_WRONLY | O_RDONLY | O_RDWR ) ) != O_RDWR )
    {
        print( FATAL, "Invalid flags specified for serial port '%s' for "
               "device %s in call of fsc2_serial_open()\n",
               Serial_Ports[ sn ].dev_file, Serial_Ports[ sn ].dev_name );
        THROW( EXCEPTION );
    }

    fsc2_serial_log_function_start( sn, "fsc2_serial_open" );

    /* First make sure we have a lock for the device file */

    if ( ! Serial_Ports[ sn ].have_lock )
    {
        fsc2_serial_log_message( sn, "Don't hold lock Serial port '%s' for "
                                 "module\n", Serial_Ports[ sn ].dev_file );
        fsc2_serial_log_function_end( sn, "fsc2_serial_open" );

        print( FATAL, "Not holding a lock for serial port '%s' of module\n",
               Serial_Ports[ sn ].dev_file );
        THROW( EXCEPTION );
    }

    /* If the port has already been opened with the same flags just return
       the structure with the current terminal settings. If the flags differ
       close the port (after flushing it and resetting the attributes to the
       original state) and afterwards re-open it with the new flags. */

    if ( Serial_Ports[ sn ].is_open )
    {
        if ( flags == Serial_Ports[ sn ].flags )
        {
            raise_permissions( );
            tcgetattr( Serial_Ports[ sn ].fd, &Serial_Ports[ sn ].new_tio );
            lower_permissions( );
            fsc2_serial_log_message( sn, "Serial port for module is already "
                                     "open\n" );
            fsc2_serial_log_function_end( sn, "fsc2_serial_open" );
            return &Serial_Ports[ sn ].new_tio;
        }
        else
        {
            raise_permissions( );
            tcflush( Serial_Ports[ sn ].fd, TCIFLUSH );
            tcsetattr( Serial_Ports[ sn ].fd, TCSANOW,
                       &Serial_Ports[ sn ].old_tio );
            close( Serial_Ports[ sn ].fd );
            lower_permissions( );
            Serial_Ports[ sn ].is_open = UNSET;
            Serial_Ports[ sn ].fd = -1;
            fsc2_serial_log_message( sn, "Closed alteady open serial port "
                                     "for device '%s', to be re-opened in the "
                                     "following with different flags\n",
                                     Serial_Ports[ sn ].dev_name );
        }
    }

    /* We need exclussive access to the serial port and we also need
       non-blocking mode (already to avoid hanging indefinitely if the
       other side does not react). O_NOCTTY is set because the serial
       port should not become the controlling terminal, otherwise line
       noise read as a CTRL-C might kill the program. */

    Serial_Ports[ sn ].flags = flags;

    flags |= O_EXCL | O_NONBLOCK | O_NOCTTY;

    /* Determine the mode and store it */

    if ( ( flags & ( O_WRONLY | O_RDONLY | O_RDWR ) ) == O_WRONLY )
        Serial_Ports[ sn ].mode = WRITE_MODE;
    else if ( ( flags & ( O_WRONLY | O_RDONLY | O_RDWR ) ) == O_RDONLY )
        Serial_Ports[ sn ].mode = READ_MODE;
    else
        Serial_Ports[ sn ].mode = READ_MODE | WRITE_MODE;

    /* Now try to open the serial port */

    raise_permissions( );

    if ( ( fd = open( Serial_Ports[ sn ].dev_file, flags ) ) < 0 )
    {
        int stored_errno = errno;

        lower_permissions( );

        fsc2_serial_log_message( sn, "Error: Failed to open serial port '%s' "
                                 "for device %s in function "
                                 "fsc2_serial_open()\n",
                                 Serial_Ports[ sn ].dev_file,
                                 Serial_Ports[ sn ].dev_name );
        fsc2_serial_log_function_end( sn, "fsc2_serial_open" );

        errno = stored_errno;
        return NULL;
    }

    /* Set the close-on-exec flag for the file descriptor */

    if ( ( fd_flags = fcntl( fd, F_GETFD ) ) < 0 )
        fd_flags = 0;
    fcntl( fd, F_SETFD, fd_flags | FD_CLOEXEC );

    /* Get the the current terminal settings and copy them to a structure
       that gets passed back to the caller. */

    tcgetattr( fd, &Serial_Ports[ sn ].old_tio );
    memcpy( &Serial_Ports[ sn ].new_tio, &Serial_Ports[ sn ].old_tio,
            sizeof Serial_Ports[ sn ].new_tio );
    lower_permissions( );

    Serial_Ports[ sn ].fd = fd;
    Serial_Ports[ sn ].is_open = SET;

    fsc2_serial_log_message( sn, "Successfully opened serial port '%s' for "
                             "device %s\n", Serial_Ports[ sn ].dev_file,
                             Serial_Ports[ sn ].dev_name );
    fsc2_serial_log_function_end( sn, "fsc2_serial_open" );

    return &Serial_Ports[ sn ].new_tio;
}


/*-----------------------------------------------------------------------*
 * Closes the device file for the serial port and removes the lockfile.
 *-----------------------------------------------------------------------*/

void
fsc2_serial_close( int sn )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Make sure the argument is ok */

    if ( sn < 0 || sn >= Num_Serial_Ports )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_serial_close().\n", sn );
        THROW( EXCEPTION );
    }

    fsc2_serial_log_function_start( sn, "fsc2_serial_close" );

    /* If device file is open flush the port, reset the settings back to the
       original state and close the device file */

    if ( Serial_Ports[ sn ].is_open )
    {
        raise_permissions( );
        tcflush( Serial_Ports[ sn ].fd, TCIFLUSH );
        tcsetattr( Serial_Ports[ sn ].fd, TCSANOW,
                   &Serial_Ports[ sn ].old_tio );
        close( Serial_Ports[ sn ].fd );
        lower_permissions( );
        Serial_Ports[ sn ].is_open = UNSET;
        fsc2_serial_log_message( sn, "Closed serial port '%s'\n",
                                 Serial_Ports[ sn ].dev_file );
        fsc2_serial_log_function_end( sn, "fsc2_serial_close" );
    }

    /* Relase the lock on the device file */

    if ( Serial_Ports[ sn ].have_lock )
    {
        fsc2_release_uucp_lock( strrchr( Serial_Ports[ sn ].dev_file, '/' )
                                + 1 );
        Serial_Ports[ sn ].have_lock = UNSET;
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

ssize_t
fsc2_serial_write( int          sn,
                   const void * buf,
                   size_t       count,
                   long         us_wait,
                   bool         quit_on_signal )
{
    ssize_t write_count;
    fd_set wrds;
    struct timeval timeout;
    struct timeval before, after;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Check if serial port number is reasonable */

    if ( sn < 0 || sn >= Num_Serial_Ports )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_serial_write().\n", sn );
        THROW( EXCEPTION );
    }

    if ( ! Serial_Ports[ sn ].is_open )
    {
        fsc2_serial_log_message( sn, "Error: Device file isn't open in call "
                                 "of function fsc2_serial_write()\n" );
        errno = EBADF;
        return -1;
    }

    if ( ! ( Serial_Ports[ sn ].mode | WRITE_MODE ) )
    {
        print( FATAL, "Attempt to write to serial port '%s' of device %s "
               "opened in read-only mode.\n",
               Serial_Ports[ sn ].dev_file, Serial_Ports[ sn ].dev_name );
        THROW( EXCEPTION );
    }

    fsc2_serial_log_function_start( sn, "fsc2_serial_write" );

    if ( ll == LL_ALL )
    {
        if ( us_wait == 0 )
            fsc2_serial_log_message( sn, "Expect to write %ld bytes without "
                                     "delay:\n%.*s\n", ( long int ) count,
                                     ( int ) count, buf );
        else if ( us_wait < 0 )
            fsc2_serial_log_message( sn, "Expect to write %ld bytes:\n%.*s\n",
                                     ( long int ) count, ( int ) count, buf );
        else
            fsc2_serial_log_message( sn, "Expect to write %ld bytes within %ld "
                                     "ms:\n%.*s\n", ( long int ) count,
                                     us_wait / 1000, ( int ) count, buf );
    }

    raise_permissions( );

    if ( us_wait != 0 )
    {
        FD_ZERO( &wrds );
        FD_SET( Serial_Ports[ sn ].fd, &wrds );

    write_retry:

        if ( us_wait > 0 )
        {
            timeout.tv_sec  = us_wait / 1000000;
            timeout.tv_usec = us_wait % 1000000;
        }

        gettimeofday( &before, NULL );

        switch ( select( Serial_Ports[ sn ].fd + 1, NULL, &wrds, NULL,
                         us_wait > 0 ? &timeout : NULL ) )
        {
            case -1 :
                if ( errno != EINTR )
                {
                    fsc2_serial_log_message( sn, "Error: select() returned "
                                             "value indicating an error\n" );
                    fsc2_serial_log_function_end( sn, "fsc2_serial_write" );
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

                fsc2_serial_log_message( sn, "Error: select() aborted due to "
                                         "the receipt of a signal\n" );
                fsc2_serial_log_function_end( sn, "fsc2_serial_write" );
                lower_permissions( );
                return 0;

            case 0 :
                fsc2_serial_log_message( sn, "Error: writing aborted due to "
                                         "timeout\n" );
                fsc2_serial_log_function_end( sn, "fsc2_serial_write" );
                lower_permissions( );
                return 0;
        }
    }

    while (    ( write_count = write( Serial_Ports[ sn ].fd, buf, count ) ) < 0
            && errno == EINTR
            && ! quit_on_signal )
        /* empty */ ;


    if ( write_count < 0 && errno == EINTR )
    {
        fsc2_serial_log_message( sn, "Error: writing aborted due to signal, "
                                 "0 bytes got written\n" );
        write_count = 0;
    }
    else
    {
        if ( ll == LL_ALL )
            fsc2_serial_log_message( sn, "Wrote %ld bytes\n",
                                     ( long ) write_count, sn );
    }

    lower_permissions( );

    fsc2_serial_log_function_end( sn, "fsc2_serial_write" );

    return write_count;
}


/*---------------------------------------------------------------------*
 * Function for reading data from one of the serial ports. It expects
 * 6 arguments, first the number of the serial port, then a buffer and
 * its maximum length for returning the read in data, an (optional
 * string with the character(s) that act as termination for the data
 * send by the device, a timeout in micro-seconds we are supposed to
 * wait for data to be read from the serial port and finally a flag
 * that tells if the function is to return immediately if a signal
 * is received.
 * If the timeout value in 'us_wait' is zero the function won't wait
 * for data to appear on the serial port, when it is negative the
 * function waits indefinitely long for data.
 * The function returns the number of read in data or -1 when an error
 * happened. A value of 0 is returned when no data could be read in,
 * possibly because a signal was received before reading started.
 *---------------------------------------------------------------------*/

ssize_t
fsc2_serial_read( int          sn,
                  void       * buf,
                  size_t       count,
                  const char * term,
                  long         us_wait,
                  bool         quit_on_signal )
{
    ssize_t read_count;
    size_t total_count = 0;
    fd_set rfds;
    struct timeval timeout;
    struct timeval before, after;
    long still_to_wait = us_wait;
    int ret;
    unsigned char *p = buf;
    size_t term_len = term ? strlen( term ) : 0;
    bool is_term = UNSET;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Check that serial port number is reasonable */

    if ( sn >= Num_Serial_Ports || sn < 0 )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_serial_read().\n", sn );
        THROW( EXCEPTION );
    }

    if ( ! Serial_Ports[ sn ].is_open )
    {
        fsc2_serial_log_message( sn, "Error: Device file isn't open in call "
                                 "of function fsc2_serial_read()\n" );
        errno = EBADF;
        return -1;
    }

    if ( ! ( Serial_Ports[ sn ].mode | READ_MODE ) )
    {
        print( FATAL, "Attempt to read from serial port '%s' of device %s "
               "opened in write-only mode.\n",
               Serial_Ports[ sn ].dev_file, Serial_Ports[ sn ].dev_name );
        THROW( EXCEPTION );
    }

    fsc2_serial_log_function_start( sn, "fsc2_serial_read" );

    if ( ll == LL_ALL )
    {
        if ( still_to_wait == 0 )
            fsc2_serial_log_message( sn, "Expect to read up to %ld bytes "
                                     "without delay\n", ( long ) count, sn );
        else if ( still_to_wait < 0 )
            fsc2_serial_log_message( sn, "Expect to read up to %ld bytes\n",
                                     ( long ) count, sn );
        else
            fsc2_serial_log_message( sn, "Expect to read up to %ld bytes "
                                     "within %ld ms\n", ( long ) count,
                                     still_to_wait / 1000, sn );
    }

    do {

        /* If there is a non-zero timeout wait using select() for data to
           become readable (negative timeout means wait forever). */

        if ( us_wait != 0 )
        {
          read_retry:

            FD_ZERO( &rfds );
            FD_SET( Serial_Ports[ sn ].fd, &rfds );

            if ( us_wait > 0 )
            {
                timeout.tv_sec  = still_to_wait / 1000000;
                timeout.tv_usec = still_to_wait % 1000000;
                gettimeofday( &before, NULL );
            }

            raise_permissions( );

            ret = select( Serial_Ports[ sn ].fd + 1, &rfds, NULL, NULL,
                          us_wait > 0 ? &timeout : NULL );

            lower_permissions( );

            if ( ret == -1 )
            {
                if ( errno != EINTR )
                {
                    fsc2_serial_log_message( sn, "Error: select() returned "
                                             "value indicating error in "
                                             "fsc2_serial_read()\n" );
                    fsc2_serial_log_function_end( sn, "fsc2_serial_read" );
                    return -1;
                }

                if ( ! quit_on_signal )
                {
                    if ( us_wait < 0 )
                        goto read_retry;

                    gettimeofday( &after, NULL );
                    still_to_wait -=
                                   ( after.tv_sec  * 1000000 + after.tv_usec  )
                                 - ( before.tv_sec * 1000000 + before.tv_usec );

                    if ( still_to_wait > 0 )
                        goto read_retry;
                    else
                        break;
                }

                fsc2_serial_log_message( sn, "Error: select() call aborted due "
                                         "to receipt of signal\n" );
                fsc2_serial_log_function_end( sn, "fsc2_serial_read" );
                return 0;
            }
            else if ( ret == 0 )
            {
                if ( total_count == 0 )
                {
                    fsc2_serial_log_message( sn, "Error: reading aborted due "
                                             "to timeout\n" );
                    fsc2_serial_log_function_end( sn, "fsc2_serial_write" );
                    return 0;
                }
                else
                    break;
            }

            if ( us_wait > 0 )
            {
                gettimeofday( &after, NULL );
                still_to_wait -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                                 - ( before.tv_sec * 1000000 + before.tv_usec );
                gettimeofday( &before, NULL );
            }
        }

        /* Now try to read */

        raise_permissions( );

        /* We've finally come to really trying to read from the serial port.
           If there's no termination string be prepared to read as many bytes
           as fit into the buffer. Otherwise we can read only one byte to be
           able to check if, with this byte, we got the the termination string
           and not read past that... */

        while (    ( read_count = read( Serial_Ports[ sn ].fd, p,
                                term_len ? 1 : ( count - total_count  ) ) ) < 0
                && errno == EINTR
                && ! quit_on_signal )
            /* empty */ ;

        lower_permissions( );

        /* Getting no bytes is possible if we were asked to read without
           waiting and then it's ok, but under strange circumstances it could
           also happen despite select() telling us there's something to read,
           in which case this has to be considered an error */

        if ( read_count == 0 )
        {
            if ( us_wait != 0 )
            {
                if ( ll == LL_ALL )
                    fsc2_serial_log_message( sn, "No bytes could be read, "
                                             "aborting\n" );
                return -1;
            }

            break;
        }
        else if ( read_count < 0 )
        {
            if ( errno == EINTR )
                fsc2_serial_log_message( sn, "Error: reading aborted due to "
                                         "signal\n" );
            else
                fsc2_serial_log_message( sn, "Error: read() returned value "
                                         "indicating error in "
                                         "fsc2_serial_read()\n" );

            fsc2_serial_log_function_end( sn, "fsc2_serial_read" );

            return errno == EINTR ? 0 : -1;
        }

        total_count += read_count;
        p += read_count;

        /* Check if we read in the termination sequence */

        if (    term
             && term_len
             && total_count >= term_len
                && ! strncmp( ( char * ) p - term_len, term, term_len ) )
            is_term = SET;

        if ( us_wait > 0 )
        {
            gettimeofday( &after, NULL );
            still_to_wait -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                             - ( before.tv_sec * 1000000 + before.tv_usec );
        }
    } while (    ! is_term
              && total_count < count
              && ( us_wait < 0 || still_to_wait > 0 ) );

    if ( ll == LL_ALL )
    {
        if ( total_count > 0 )
            fsc2_serial_log_message( sn, "Read %ld bytes:\n%.*s\n",
                                     ( long ) total_count,
                                     ( int ) total_count, buf );
        else
            fsc2_serial_log_message( sn, "Read 0 bytes\n" );
    }

    fsc2_serial_log_function_end( sn, "fsc2_serial_read" );

    return total_count;
}


/*------------------------------*
 * Replacement for tcgetattr(3)
 *------------------------------*/

int
fsc2_tcgetattr( int              sn,
                struct termios * termios_p )
{
    int ret_val;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Check that serial port number is reasonable */

    if ( sn >= Num_Serial_Ports || sn < 0 )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_tcgetattr().\n", sn );
        THROW( EXCEPTION );
    }

    if ( ! Serial_Ports[ sn ].is_open )
    {
        fsc2_serial_log_message( sn, "Error: Device file isn't open in call "
                                 "of function fsc2_tcgetattr()\n" );
        errno = EBADF;
        return -1;
    }

    raise_permissions( );
    ret_val = tcgetattr( Serial_Ports[ sn ].fd, termios_p );
    lower_permissions( );

    return ret_val;
}


/*------------------------------*
 * Replacement for tcsetattr(3)
 *------------------------------*/

int
fsc2_tcsetattr( int              sn,
                int              optional_actions,
                struct termios * termios_p )
{
    int ret_val;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Check that serial port number is reasonable */

    if ( sn >= Num_Serial_Ports || sn < 0 )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_tcsetattr().\n", sn );
        THROW( EXCEPTION );
    }

    if ( ! Serial_Ports[ sn ].is_open )
    {
        fsc2_serial_log_message( sn, "Error: Device file isn't open in call "
                                 "of function fsc2_tcsetattr()\n" );
        errno = EBADF;
        return -1;
    }

    raise_permissions( );
    ret_val = tcsetattr( Serial_Ports[ sn ].fd, optional_actions, termios_p );
    lower_permissions( );

    return ret_val;
}


/*--------------------------------*
 * Replacement for tcsendbreak(3)
 *--------------------------------*/

int
fsc2_tcsendbreak( int sn,
                  int duration )
{
    int ret_val;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Check that serial port number is reasonable */

    if ( sn >= Num_Serial_Ports || sn < 0 )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_tcsendbreak().\n", sn );
        THROW( EXCEPTION );
    }

    if ( ! Serial_Ports[ sn ].is_open )
    {
        fsc2_serial_log_message( sn, "Error: Device file isn't open in call "
                                 "of function fsc2_tcsendbreak()\n" );
        errno = EBADF;
        return -1;
    }

    raise_permissions( );
    ret_val = tcsendbreak( Serial_Ports[ sn ].fd, duration );
    lower_permissions( );

    return ret_val;
}


/*----------------------------*
 * Replacement for tcdrain(3)
 *----------------------------*/

int
fsc2_tcdrain( int sn )
{
    int ret_val;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Check that serial port number is reasonable */

    if ( sn >= Num_Serial_Ports || sn < 0 )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_tcdrain().\n", sn );
        THROW( EXCEPTION );
    }

    if ( ! Serial_Ports[ sn ].is_open )
    {
        fsc2_serial_log_message( sn, "Error: Device file isn't open in call "
                                 "of function fsc2_tcdrain()\n" );
        errno = EBADF;
        return -1;
    }

    raise_permissions( );
    ret_val = tcdrain( Serial_Ports[ sn ].fd );
    lower_permissions( );

    return ret_val;
}


/*----------------------------*
 * Replacement for tcflush(3)
 *----------------------------*/

int
fsc2_tcflush( int sn,
              int queue_selector )
{
    int ret_val;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Check that serial port number is reasonable */

    if ( sn >= Num_Serial_Ports || sn < 0 )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_tcflush().\n", sn );
        THROW( EXCEPTION );
    }

    if ( ! Serial_Ports[ sn ].is_open )
    {
        fsc2_serial_log_message( sn, "Error: Device file isn't open in call "
                                 "of function fsc2_tcflush()\n" );
        errno = EBADF;
        return -1;
    }

    raise_permissions( );
    ret_val = tcflush( Serial_Ports[ sn ].fd, queue_selector );
    lower_permissions( );

    return ret_val;
}


/*---------------------------*
 * Replacement for tcflow(3)
 *---------------------------*/

int
fsc2_tcflow( int sn,
             int action )
{
    int ret_val;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    /* Check that serial port number is reasonable */

    if ( sn >= Num_Serial_Ports || sn < 0 )
    {
        print( FATAL, "Invalid serial port index %d in call of "
               "fsc2_tcflow().\n", sn );
        THROW( EXCEPTION );
    }

    if ( ! Serial_Ports[ sn ].is_open )
    {
        fsc2_serial_log_message( sn, "Error: Device file isn't open in call "
                                 "of function fsc2_tcflow()\n" );
        errno = EBADF;
        return -1;
    }

    raise_permissions( );
    ret_val = tcflow( Serial_Ports[ sn ].fd, action );
    lower_permissions( );

    return ret_val;
}



/*---------------------------------------------------------*
 * fsc2_serial_log_date() writes the date to the log file.
 *---------------------------------------------------------*/

static void
fsc2_serial_log_date( int sn )
{
    char tc[ 26 ];
    struct timeb mt;
    time_t t;


    fsc2_assert( sn >= 0 && sn <= Num_Serial_Ports );

    if ( ! Serial_Ports[ sn ].log_fp )
        return;

    t = time( NULL );
    strcpy( tc, asctime( localtime( &t ) ) );
    tc[ 10 ] = '\0';
    tc[ 19 ] = '\0';
    tc[ 24 ] = '\0';
    ftime( &mt );
    fprintf( Serial_Ports[ sn ].log_fp, "[%s %s %s.%03d] ",
             tc, tc + 20, tc + 11, mt.millitm );
}


/*--------------------------------------------------------------*
 * fsc2_serial_log_function_start() logs the call of a function
 * by appending a short message to the log file.
 * ->
 *  * name of the function
 *  * name of the device involved
 *--------------------------------------------------------------*/

static void
fsc2_serial_log_function_start( int          sn,
                                const char * function )
{
    fsc2_assert( sn >= 0 && sn <= Num_Serial_Ports );

    if ( ll < LL_CE || ! Serial_Ports[ sn ].log_fp )
        return;

    raise_permissions( );
    fsc2_serial_log_date( sn );
    fprintf( Serial_Ports[ sn ].log_fp, "Call of %s()\n", function );
    fflush( Serial_Ports[ sn ].log_fp );
    lower_permissions( );
}


/*---------------------------------------------------------*
 * fsc2_serial_log_function_end() logs the completion of a
 * function by appending a short message to the log file.
 * ->
 *  * name of the function
 *  * name of the device involved
 *---------------------------------------------------------*/

static void
fsc2_serial_log_function_end( int          sn,
                              const char * function )
{
    fsc2_assert( sn >= 0 && sn <= Num_Serial_Ports );

    if ( ll < LL_CE || ! Serial_Ports[ sn ].log_fp )
        return;

    raise_permissions( );
    fsc2_serial_log_date( sn );
    fprintf( Serial_Ports[ sn ].log_fp, "Exit of %s()\n", function );
    fflush( Serial_Ports[ sn ].log_fp );
    lower_permissions( );
}


/*-----------------------------------------------------*
 * Function for printing out a message to the log file
 *-----------------------------------------------------*/

static void
fsc2_serial_log_message( int          sn,
                         const char * fmt,
                         ... )
{
    va_list ap;


    fsc2_assert( sn >= 0 && sn <= Num_Serial_Ports );

    if ( ll == LL_NONE || ! Serial_Ports[ sn ].log_fp )
        return;

    raise_permissions( );
    fsc2_serial_log_date( sn );
    va_start( ap, fmt );
    vfprintf( Serial_Ports[ sn ].log_fp, fmt, ap );
    va_end( ap );
    fflush( Serial_Ports[ sn ].log_fp );
    lower_permissions( );
}


/*--------------------*
 * Opens the log file 
 *--------------------*/

static void
open_serial_log( int sn )
{
    char *log_file_name = NULL;


    if ( ll == LL_NONE )
        return;

    TRY
    {
#if defined SERIAL_LOG_DIR
        log_file_name = get_string( "%s%sfsc2_%s.log", SERIAL_LOG_DIR,
                                    slash( SERIAL_LOG_DIR ),
                                    Serial_Ports[ sn ].dev_name );
#else
        log_file_name = get_string( P_tmpdir "/fsc2_%s.log",
                                    Serial_Ports[ sn ].dev_name );
#endif
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )     /* extremely unlikely... */
    {
        Serial_Ports[ sn ].log_fp = stderr;
        return;
    }

    raise_permissions( );

    if ( ! ( Serial_Ports[ sn ].log_fp = fopen( log_file_name, "w" ) ) )
    {
        Serial_Ports[ sn ].log_fp = stderr;
        fprintf( stderr, "Can't open serial log file for %s, using "
                 "stderr instead.\n", Serial_Ports[ sn ].dev_name );
    }
    else
    {
        int fd_flags = fcntl( fileno( Serial_Ports[ sn ].log_fp ),
                              F_GETFD );

        if ( fd_flags < 0 )
            fd_flags = 0;
        fcntl( fileno( Serial_Ports[ sn ].log_fp ), F_SETFD,
               fd_flags | FD_CLOEXEC );
        chmod( log_file_name,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
    }

    lower_permissions( );

    T_free( log_file_name );

    fsc2_serial_log_message( sn, "Starting serial port logging for "
                             "device %s\n", Serial_Ports[ sn ].dev_name );
}


/*---------------------*
 * Closes the log file
 *---------------------*/

static void
close_serial_log( int sn )
{
    fsc2_assert( sn >= 0 && sn <= Num_Serial_Ports );

    fsc2_serial_log_message( sn, "Stopping serial port logging for "
                             "device %s\n", Serial_Ports[ sn ].dev_name );

    if ( Serial_Ports[ sn ].log_fp && Serial_Ports[ sn ].log_fp != stderr )
    {
        raise_permissions( );
        fclose( Serial_Ports[ sn ].log_fp );
        lower_permissions( );
    }

    Serial_Ports[ sn ].log_fp = NULL;
}

#else

/* Here come some dummy functions for the case that fsc2 was compiled without
   support for serial ports. They are needed for the case that from a previous
   install meodules using the serial port still exist - an attempt to load
   these outdated modules would result in a failure with an error message
   telling that fsc2 is missing some serial port functions without an
   explanation that they aren't compiled in anymore. By supplying dummy
   functions loading these modules still works but once the modules try to
   call a function needing a serial port they fail with a error message the
   user can understand, i.e. one that tells him/her that fsc2 was compiled
   without support for serial ports... */


int
fsc2_request_serial_port( const char * dev_file  UNUSED_ARG,
                          const char * dev_name )
{
    eprint( FATAL, UNSET, "Module for device '%s' requires support "
            "for serial ports but fsc2 was compiled without.\n",
            ( dev_name && *dev_name ) ? dev_name : "UNKNOWN" );
    THROW( EXCEPTION );

    return -1;
}

struct termios *
fsc2_serial_open( int sn     UNUSED_ARG,
                  int flags  UNUSED_ARG )
{
    errno = EACCES;
    return NULL;
}


void
fsc2_serial_close( int sn  UNUSED_ARG )
{
}


ssize_t
fsc2_serial_write( int          sn              UNUSED_ARG,
                   const void * buf             UNUSED_ARG ,
                   size_t       count           UNUSED_ARG,
                   long         us_wait         UNUSED_ARG,
                   bool         quit_on_signal  UNUSED_ARG )
{
    errno = EBADF;
    return -1;
}


ssize_t
fsc2_serial_read( int          sn              UNUSED_ARG,
                  void       * buf             UNUSED_ARG,
                  size_t       count           UNUSED_ARG,
                  const char * term            UNUSED_ARG,
                  long         us_wait         UNUSED_ARG,
                  bool         quit_on_signal  UNUSED_ARG )
{
    errno = EBADF;
    return -1;
}


int
fsc2_tcgetattr( int              sn         UNUSED_ARG,
                struct termios * termios_p  UNUSED_ARG )
{
    errno = EBADF;
    return -1;
}


int
fsc2_tcsetattr( int              sn                UNUSED_ARG,
                int              optional_actions  UNUSED_ARG,
                struct termios * termios_p         UNUSED_ARG )
{
    errno = EBADF;
    return -1;
}


int
fsc2_tcsendbreak( int sn        UNUSED_ARG,
                  int duration  UNUSED_ARG )
{
    errno = EBADF;
    return -1;
}


int
fsc2_tcdrain( int sn UNUSED_ARG )
{
    errno = EBADF;
    return -1;
}


int
fsc2_tcflush( int sn              UNUSED_ARG,
              int queue_selector  UNUSED_ARG )
{
    errno = EBADF;
    return -1;
}

int
fsc2_tcflow( int sn      UNUSED_ARG,
             int action  UNUSED_ARG )
{
    errno = EBADF;
    return -1;
}

#endif

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
