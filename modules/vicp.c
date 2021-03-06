/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


/* The following is a set of routines that can be used by programs that
   need to communicate with devices using LeCroy's VICP (Versatile
   Instrument Control Protocol), running on top of TCP/IP. It uses a
   TCP connection on port 1861.

   Basically, Lecroy's VICP protocol just tells that before data are
   exchanged a header must be sent. The header has the form

   struct VICP_Header {
       unsigned char operation;
       unsigned char header version;
       unsigned char sequence_number;    // only set in version 1A and later
       unsigned char reserved;
       uint_32       block_length;       // in big-endian format
   };

   The bits of the 'operation' byte have the following meaning:

   bit 7    data will be send (if with or without EOI is determined by bit 0)
   bit 6    remote mode
   bit 5    local lockout
   bit 4    clear device (if sent with data a clear is done before data are
            interpreted)
   bit 3    SRQ (only to be send by the device)
   bit 2    reserved
   bit 1    reserved
   bit 0    if set data block is terminater by EOI character

   The EOI bit simply tells if the data block send to the device or received
   from it ends in an EOI character ('\n' or 0x0A).

   The 'header_version' field is a number - currently only 1 is an allowed
   value (protocol version 1A is treated as a subversion of version 1).

   The 'sequence_number' field only has a meaning for devices supporting
   version 1A of the protocol. For these it must be a number between 1
   and 255 (but never 0), where each data block must have a "sequence
   number" incremented by 1 relative to the previous block (if 255 is
   reached the next number then is 1). For all other devices the field
   will always be set to 0. Since one can't figure out from the version
   field if the device is using version 1 or 1A of the protocol one only
   can find out from checking if the sequence number is 0 or non-zero.

   Finally, the 'block_length' member is the length of the block of data
   (in bytes) to be sent following the header. It's a 32-bit number in
   big-endian format, i.e. the MSB is at the lowest memory address and
   the LSB at the highest address.

   The following functions for communicating with a device using LeCroy's
   VICP protocol can be used:

    void vicp_open( const char * dev_name,
                    const char * address,
                    long         us_timeout,
                    bool         quit_on_signal )

    void vicp_close( void )

    void vicp_lock_out( bool lock_out )

    void vicp_set_timeout( int  dir,
                           long us_timeout )

    int vicp_write( const char * buffer,
                    ssize_t    * length,
                    bool         with_eoi,
                    bool         quit_on_signal )

    int vicp_read( char    * buffer,
                   ssize_t * length,
                   bool    * with_eoi,
                   bool      quit_on_signal )

    void vicp_device_clear( void )

   The function vicp_open() must be the first function to be called. It
   establishes the connection to the device which then is used for all
   further function calls. The function expects a symbolic name for the
   device (only to be used for the log file), its IP address, either as
   a hostname or a numerical IP address in dotted-quad format, a positive
   or zero maximum time (in micro-seconds, a zero value means never to
   time-out) the function is allowed to wait for successfully establishing
   the connection and a flag that tells if the function is supposed to return
   immediately on receipt of a signal. Please note that a timer, raising a
   SIGALRM signal on expiry, is used for controlling the timeout. Thus the
   function temporarily installs its own signal handler for SIGALRM, so the
   caller should make sure that it doesn't initiate anything that would also
   raise such a SIGALRM signal. Please also note that the function can't be
   called when an connection has already been created. On failure (either
   because the connection has already been opened, connecting to the device
   fails or not enough memory is available) the function throws an exception.

   vicp_close() is the opposite of vicp_open(), i.e. it shuts down the
   existing connection. It throws an exception when you try to close an
   already closed connection.

   vicp_lock_out() allows you to control if the device is in local lockout
   state (the default when a connection is made) or not. By calling the
   funtion with a true boolean value local lockout gets switched on,
   switch it off by calling it with a value evaluating to false.

   vicp_set_timeout() allows to set timeouts for read or write operations.
   If the first argument is the symbolic value READ (or 0), a timeout for
   read operations gets set, if it's WRITE (or 1) the timeout for writes
   gets set. Timeouts must be specified in microseconds and must be either
   positive numbers or zero (in which case an indefinitely long timeout is
   used). Without calling the function a default timeout of 5 seconds will
   be used. Please note that the function can only be called after the
   connection has been established. Please also note that on some systems
   (those that don't support the SO_RCVTIMEO and SO_SNDTIMEO socket options)
   the timeouts get created via a timer that raises a SIGALRM signal. Thus
   on these systems the caller may not initiate any action that would raise
   such a signal when calling one of the functions that can timeout.

   The function vicp_write() is used to send data to the device. It requires
   four arguments: a buffer with the data to be send, a pointer to a variable
   with the number of bytes to be send, a flag that tells if the data are
   sent with a trailing EOI, and a flag that tells if the function is supposed
   to return immediately when receiving a signal. The function returns either
   VICP_SUCCESS (0) if the date could be sent successfully, or VICP_FAILURE
   (-1) if sending the data aborted due to receiving a signal. On errors or
   timeouts the function closes the connection and throws an exception. On
   return the variable pointed to by the secand argument will contain the
   number of bytes that have been sent - this also is the case if the function
   returns VICP_FAILURE or did throw an exception.

   vicp_read() is for reading data the device sends. It takes four arguments:
   a buffer for storing the data, a pointer to a variable with the length of
   the buffer, a pointer to a variable that tells on return if EOI was set
   for the data and a flag telling if the function is supposed to return
   immediately on signals. The function returns VICP_SUCCESS_BUT MORE (1) if
   a reply was received but not all data could be read because they wouldn't
   have fit into the user supplied buffer, VICP_SUCCESS (0) if the reply was
   read completely and VICP_FAILURE (-1) if the function aborted because a
   signal was received. On erros or timeouts the function closes the connection
   and throws an exception. On return the variable pointed to by the second
   argument is set to the number of bytes that have been received and the
   third one shows if the data sent have a trailing EOI, even if the function
   did return with VICP_FAILURE or threw an exception.

   If  the function returns less data than the device was willing to send
   (in which case VICP_SUCCESS_BUT_MORE gets returned) the next invocation of
   vicp_read() will return these yet unread data, even if another reply by
   the device was initiated by sending another command in between. I.e.
   "new data" (data resulting from the next command) only will be returned
   after the function has been called until VICP_SUCCESS has been returned.

   Please note: If a read or write gets aborted due to a signal there may
   still be data to be read from the device or the device may still be
   waiting to receive more data. Unless you're closing the connection
   after receipt of a signal you must make sure that the remaining data
   are fetched from or get send to the device.

   The function vicp_device_clear() allows to clear the device and reset
   the connection. Clearing the device includes clearing its input and
   output buffers and aborting the interpretation of the current command
   (if any) as well as clearing all pending commands. Status and status
   enable registers remain unchanged. All of this may take several seconds
   to finish. The function also closes and reopens the connection to the
   device.

   The availability of source code of implementations written by Steve D.
   Sharples from Nottingham University (steve.sharples@nottingham.ac.uk),
   see

     http://osam.eee.nottingham.ac.uk/lecroy_tcp/

   and for the (Windows) LeCroyVICP Client Library written by Anthony Cake
   (anthonyrc@users.sourceforge.net) and Larry Salant
   (larryss@users.sourceforge.net), available at

     http://sourceforge.net/projects/lecroyvicp/

   were of great help in writing this implementation, especially given
   the rather meager amount of information in the Remote Control Manuals
   by LeCroy.
*/


#include "vicp.h"
#include "lan.h"
#include <sys/time.h>


/* Port the VICP server is listening on */

#define VICP_PORT   1861


/* Constants for the bits in the headers operation byte */

#define VICP_DATA          ( 1 << 7 )
#define VICP_REMOTE        ( 1 << 6 )
#define VICP_LOCKOUT       ( 1 << 5 )
#define VICP_CLEAR         ( 1 << 4 )
#define VICP_SRQ           ( 1 << 3 )
#define VICP_SERIAL_POLL   ( 1 << 2 )
#define VICP_EOI           ( 1 << 0 )


/* Size of the header to be sent or received with each data package */

#define VICP_HEADER_SIZE   8


/* Header version */

#define VICP_HEADER_VERSION_VALUE   1


/* Positions of the different entries in the header */

#define VICP_HEADER_OPERATION_OFFSET   0
#define VICP_HEADER_VERSION_OFFSET     1
#define VICP_HEADER_SEQUENCE_OFFSET    2
#define VICP_HEADER_MSB_OFFSET         4
#define VICP_HEADER_LSB_OFFSET         7


/* Default timeout for connect() call */

#define VICP_DEFAULT_CONNECT_TIMEOUT     10000000    /* 10 s */


/* Default timeout for read and write operations */

#define VICP_DEFAULT_READ_WRITE_TIMEOUT  5000000    /* 5 s */


/* Internal structure for storing states and information about the device */

typedef struct VICP VICP_T;

struct VICP {
    int             handle;
    unsigned char   seq_number;
    unsigned char   partial_header[ VICP_HEADER_SIZE ];
    ssize_t         header_count;
    ssize_t         remaining;
    bool            eoi_was_set;
    char          * name;
    char          * address;
    long            us_write_timeout;
    long            us_read_timeout;
    bool            is_locked;
};


static VICP_T vicp = { -1, 0, { 0 }, 0, 0, UNSET,
                       NULL, NULL, 0, 0, UNSET };


static void vicp_close_without_header( void );

static inline unsigned char vicp_get_operation( unsigned char * header );

static inline unsigned char vicp_get_version( unsigned char * header );

#if 0
static inline unsigned char vicp_get_sequence( unsigned char * header );
#endif

static inline ssize_t vicp_get_length( unsigned char * header );

static inline void vicp_set_operation( unsigned char * header,
                                       unsigned char   operation );

static inline void vicp_set_version( unsigned char * header );

static inline void vicp_set_sequence( unsigned char * header );

static inline void vicp_set_length( unsigned char * header,
                                    unsigned long   lenght );


/*--------------------------------------------------------------*
 * Function for opening a network connection to the device.
 * ->
 *    1. Name of the device
 *    2. IP address of the device (either in dotted-quad form
 *       or as name that can be resolved via a DNS request)
 *    3. Maximum timeout (in micro-seconds) to wait for the
 *       connection to succeeded
 *    4. Flag, if set the attempt to connect to the device can
 *       be aborted due to a signal, otherwise the connect()
 *       call will restarted if a signal gets caught.
 * On failures the function throws an exception.
 *-------------------------------------------------------------*/

void
vicp_open( const char    * dev_name,
           const char    * address,
           volatile long   us_timeout,
           bool            quit_on_signal )
{
    fsc2_assert( dev_name != NULL );
    fsc2_assert( address != NULL );

    if ( vicp.handle >= 0 )
    {
        print( FATAL, "Internal error in module, connection already "
               "exists.\n" );
        THROW( EXCEPTION );
    }

    if ( us_timeout < 0 )
        us_timeout = VICP_DEFAULT_CONNECT_TIMEOUT;

    int fd;
    if ( ( fd = fsc2_lan_open( dev_name, address, VICP_PORT,
                               us_timeout, quit_on_signal ) ) == -1 )
    {
        print( FATAL, "Failed to open connection to device.\n" );
        THROW( EXCEPTION );
    }

    vicp.handle           = fd;
    vicp.seq_number       = 0;
    vicp.header_count     = 0;
    vicp.remaining        = 0;
    vicp.eoi_was_set      = UNSET;
    vicp.us_write_timeout =
    vicp.us_read_timeout  = VICP_DEFAULT_READ_WRITE_TIMEOUT;
    vicp.name             = NULL;
    vicp.address          = NULL;

    /* Also store name and address for the device, this information is
       needed in case we have to dis- and then reconnect. */

    TRY
    {
        vicp.name    = T_strdup( dev_name );
        vicp.address = T_strdup( address );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        vicp_close_without_header( );
        RETHROW;
    }

    /* Finally bring the device into remote state by sending a header
       where just the remote and the lockout bits in the operations byte
       are set */

    unsigned char header[ VICP_HEADER_SIZE ] = { 0 };

    vicp_set_operation( header, VICP_REMOTE | VICP_LOCKOUT );
    vicp_set_version( header );
    vicp_set_length( header, 0 );

    ssize_t bytes_written = fsc2_lan_write( vicp.handle, ( char * ) header,
                                            VICP_HEADER_SIZE, us_timeout,
                                            UNSET );

    if ( bytes_written <= 0 )
    {
        vicp_close_without_header( );
        print( FATAL, "Error in communication with LAN device.\n" );
        THROW( EXCEPTION );
    }

    fsc2_assert( bytes_written == VICP_HEADER_SIZE );

    vicp.is_locked = SET;
}


/*--------------------------------------------------*
 * Local function for closing the connection to the
 * device and resetting some internal variables.
 *--------------------------------------------------*/

static
void
vicp_close_without_header( void )
{
    if ( vicp.handle >= 0 )
        fsc2_lan_close( vicp.handle );

    vicp.handle = -1;

    if ( vicp.name )
        vicp.name = T_free( vicp.name );

    if ( vicp.address )
        vicp.address = T_free( vicp.address );
}


/*---------------------------------------------------*
 * Function for closing the connection to the device
 * after bringing it back to local
 *---------------------------------------------------*/

void
vicp_close( void )
{
    if ( vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        THROW( EXCEPTION );
    }

    /* Send a header with all bits reset in order to get device out of
       remote (and possibly local lockout) state */

    unsigned char  header[ VICP_HEADER_SIZE ] = { 0 };

    vicp_set_version( header );
    vicp_set_length( header, 0 );

    long us_timeout = VICP_DEFAULT_READ_WRITE_TIMEOUT;

    fsc2_lan_write( vicp.handle, ( char * ) header,
                    VICP_HEADER_SIZE, us_timeout, UNSET );

    /* Now close down the connection to the device */

    vicp_close_without_header( );
}


/*------------------------------------------------------*
 * Function to switch between locked and unlocked state
 * ->
 *    1. boolean value, if set lock out device,
 *       otherwise unlock it
 *------------------------------------------------------*/

void
vicp_lock_out( bool lock_state )
{
    if ( vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        THROW( EXCEPTION );
    }

    if (    ( vicp.is_locked && lock_state )
         || ( ! vicp.is_locked && ! lock_state ) )
        return;

    unsigned char op = VICP_REMOTE;

    if ( lock_state )
        op |= VICP_LOCKOUT;

    if ( vicp.eoi_was_set )
        op |= VICP_EOI;

    unsigned char header[ VICP_HEADER_SIZE ] = { 0 };

    vicp_set_operation( header, op );
    vicp_set_version( header );
    vicp_set_length( header, 0 );

    long us_timeout = VICP_DEFAULT_READ_WRITE_TIMEOUT;

    ssize_t bytes_written = fsc2_lan_write( vicp.handle, ( char * ) header,
                                            VICP_HEADER_SIZE, us_timeout,
                                            UNSET );

    if ( bytes_written <= 0 )
    {
        vicp_close_without_header( );
        print( FATAL, "Error in communication with LAN device.\n" );
        THROW( EXCEPTION );
    }

    fsc2_assert( bytes_written == VICP_HEADER_SIZE );

    vicp.is_locked = lock_state;
}


/*----------------------------------------------------------------*
 * Function for setting the maximum allowed time for a read or
 * write operation.
 * ->
 *    1. Flag that tells if the timeout is for read or write
 *       operations - 0 stands for read, everything else for
 *       write operations.
 *    2. Timeout value in micro-seconds
 *----------------------------------------------------------------*/

void
vicp_set_timeout( int  dir,
                         long us_timeout )
{
    if ( vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        THROW( EXCEPTION );
    }

    if ( us_timeout < 0 )
    {
        print( FATAL, "Internal error in module, invalid timeout.\n" );
        THROW( EXCEPTION );
    }

    if ( dir == READ )
        vicp.us_read_timeout  = us_timeout;
    else
        vicp.us_write_timeout = us_timeout;
}


/*-------------------------------------------------------------------*
 * Function for sending data to the device.
 * ->
 *    1. Pointer to buffer with the data
 *    2. Pointer to variable with the amount of bytes to be send
 *    3. Flag that tells when set that the block of data is
 *       terminated by the EOI character
 *    4. Flag that tells if he function is supposed to return when
 *       a signal is received.
 * <- The return the variable pointed to by 'length' gets set to the
 *    number of bytes that have been send. There are two possible
 *    return values:
 *     a) VICP_SUCCESS (0)      data have been send <successfully
 *     c) VICP_FAILURE (-1)     transmission was aborted due to a
 *                              signal
 *    If a timeout happens during the transmission an exception gets
 *    thrown.
 *-------------------------------------------------------------------*/

int
vicp_write( const char * buffer,
            ssize_t    * length,
            bool         with_eoi,
            bool         quit_on_signal )
{
    /* Do nothing if there are no data to be sent */

    if ( *length == 0 )
        return VICP_SUCCESS;

    if ( buffer == NULL )
    {
        print( FATAL, "Internal error in module, write data buffer "
               "is NULL.\n" );
        THROW( EXCEPTION );
    }

    if ( vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, connection is already "
               "closed.\n" );
        THROW( EXCEPTION );
    }

    /* Set up a iovec structure in order to allow sending the header
       and the data in one go with writev() */

    unsigned char  header[ VICP_HEADER_SIZE ] = { 0 };
    struct iovec data[ 2 ];

    data[ 0 ].iov_base = header;
    data[ 0 ].iov_len  = VICP_HEADER_SIZE;
    data[ 1 ].iov_base = ( void * ) buffer;
    data[ 1 ].iov_len  = *length;

    /* Assemble the header, set the EOI flag if the user told us to */

    unsigned char  op = VICP_DATA | VICP_REMOTE;

    if ( with_eoi )
        op |= VICP_EOI;

    vicp_set_operation( header, op );
    vicp_set_version( header );
    vicp_set_sequence( header );
    vicp_set_length( header, *length );

    /* Now start pushing the data over to the device. Since there is no
       guarantee that all of them will be written in one go (e.g. because
       there are more than fit into a packet) we must go on trying if less
       than the requested amount of bytes were sent on the first try amd
       keep on trying until we're done. */

    struct timeval before;
    gettimeofday( &before, NULL );

    long us_timeout = vicp.us_write_timeout;
    ssize_t bytes_written = fsc2_lan_writev( vicp.handle, data, 2,
                                             us_timeout, quit_on_signal );

    if ( bytes_written < 0 )
    {
        print( FATAL, "Error in communication with LAN device.\n" );
        THROW( EXCEPTION );
    }

    if ( bytes_written == 0 )
    {
        *length = 0;
        return VICP_FAILURE;
    }

    ssize_t total_length = bytes_written - VICP_HEADER_SIZE;

    fsc2_assert( total_length >= 0 );

    while ( total_length < *length )
    {
        struct timeval after;
        gettimeofday( &after, NULL );

        us_timeout -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                      - ( before.tv_sec * 1000000 + before.tv_usec );

        if ( us_timeout <= 0 )
        {
            print( FATAL, "Timeout while writing to LAN device.\n" );
            THROW( EXCEPTION );
        }

        before = after;
        buffer += total_length;

        bytes_written = fsc2_lan_write( vicp.handle, buffer,
                                        *length - total_length, us_timeout,
                                        quit_on_signal );

        if ( bytes_written < 0 )
        {
            print( FATAL, "Error in communication with LAN device.\n" );
            THROW( EXCEPTION );
        }

        total_length += bytes_written;

        if ( bytes_written == 0 )
        {
            *length = total_length;
            return VICP_FAILURE;
        }
    }

    return VICP_SUCCESS;
}


/*-------------------------------------------------------------------*
 * Function for receiving data from the device.
 * ->
 *    1. Pointer to buffer for the data
 *    2. Pointer to variable with the maximum amount of bytes that
 *       can be received.
 *    3. Flag that tells if the data block is terminated by an EOI
 *       character
 *    3. Flag that tells if he function is supposed to return when
 *       a signal is received.
 * <- The function the variable pointed to by 'length' gets set to
 *    the number of bytes that have been received. It has three
 *    possible return values:
 *     a) VICP_SUCCESS_BUT_MORE (1)  data have successfully received but
 *                                   there are still data the device wants
 *                                   to transmit but which didn't fit into
 *                                   the user supplied buffer
 *     b) VICP_SUCCESS (0)           all data have been received successfully
 *     c) VICP_FAILURE (-1)          transmission was aborted due to a
 *                                   signal
 * If a timeout occurs during the transmission an exception gets thrown.
 *
 * Reading stops when we either got as many data as the user asked for
 * or the end of a message was reached (as determined from the EOI flag
 * in the header) or a read was interrupted by a signal.
 *---------------------------------------------------------------------*/

int
vicp_read( char    * buffer,
           ssize_t * length,
           bool    * with_eoi,
           bool      quit_on_signal )
{
    /* Do nothing if no data are to be read */

    if ( *length == 0 )
        return VICP_SUCCESS;

    if ( buffer == NULL )
    {
        print( FATAL, "Internal error in module, read data buffer is "
               "NULL.\n" );
        THROW( EXCEPTION );
    }

    if ( vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, connection is "
               "already closed.\n" );
        THROW( EXCEPTION );
    }

    /* Check if there are still outstanding bytes, i.e. bytes of which we
       know from reading the last header that they are in the process of
       being sent by the device. Get them first. */

    struct timeval before,
                   after;
    gettimeofday( &before, NULL );

    long    us_timeout = vicp.us_read_timeout;

    ssize_t total_length = 0;
    ssize_t bytes_read = 0;

    if ( vicp.remaining > 0 )
    {
        *with_eoi = vicp.eoi_was_set;

        while ( vicp.remaining > 0 && total_length < *length )
        {
            gettimeofday( &after, NULL );
            us_timeout -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                          - ( before.tv_sec * 1000000 + before.tv_usec );

            if ( us_timeout <= 0 )
            {
                print( FATAL, "Timeout while reading from LAN device.\n" );
                THROW( EXCEPTION );
            }

            before = after;

            bytes_read = fsc2_lan_read( vicp.handle, buffer,
                                        ss_min( vicp.remaining, *length ),
                                        us_timeout, quit_on_signal );

            if ( bytes_read < 0 )
            {
                print( FATAL, "Error in communication with LAN device.\n" );
                THROW( EXCEPTION );
            }

            if ( bytes_read > 0 )
            {
                vicp.remaining -= bytes_read;
                total_length += bytes_read;
                buffer += bytes_read;
            }

            if ( bytes_read == 0 )
                break;
        }

        /* If we're at the end of a message or have already read as many
           data as was asked for or if the read was interrupted by a
           signal (bytes_read is 0) return */

        if ( *with_eoi || total_length == *length || bytes_read == 0 )
            return vicp.remaining == 0 ? VICP_SUCCESS : VICP_SUCCESS_BUT_MORE;
    }

    /* Loop until either EOI is set or we got as many bytes as we were asked
       to read (or a read was interrupted by a signal). We need to loop since
       a message from the device can be split into several chunks, each
       starting with a new header. */

    do
    {
        unsigned char  header[ VICP_HEADER_SIZE ];
        char *h = ( char * ) header;
        ssize_t header_read = 0;

        /* Read a new header - also here we must expect to get only
           part of the header on a single read */

        do
        {
            bytes_read = fsc2_lan_read( vicp.handle, h,
                                        VICP_HEADER_SIZE - header_read,
                                        us_timeout, quit_on_signal );

            if ( bytes_read < 0 )
            {
                print( FATAL, "Error in communication with LAN device.\n" );
                THROW( EXCEPTION );
            }

            if ( bytes_read == 0 )
            {
                *length = total_length;
                return VICP_FAILURE;
            }

            header_read += bytes_read;
            h += bytes_read;
        } while ( header_read < VICP_HEADER_SIZE );

        /* Check the version field - if this is not correct something must
           have gone seriously wrong */

        if ( vicp_get_version( header ) != VICP_HEADER_VERSION_VALUE )
        {
            print( FATAL, "LAN device sent unexpected data.\n" );
            THROW( EXCEPTION );
        }

        /* The header tells us how many bytes we can expect - if there are
           none something must really be going wrong */

        unsigned long  bytes_to_expect;

        if ( ( bytes_to_expect = vicp_get_length( header ) ) == 0 )
        {
            print( FATAL, "LAN device sent unexpected data.\n" );
            THROW( EXCEPTION );
        }

        /* Examine and store the EOI bit */

        *with_eoi = vicp.eoi_was_set = vicp_get_operation( header ) & VICP_EOI;

        /* Now read the real data. Make sure we don't try to read more than
           the user asked for. If we could read more we store the number of
           bytes we could have read but didn't yet fetch. */

        if ( ( ssize_t ) bytes_to_expect > *length - total_length )
        {
            vicp.remaining = bytes_to_expect - *length + total_length;
            bytes_to_expect = *length - total_length;
        }

        while ( bytes_to_expect > 0 )
        {
            gettimeofday( &after, NULL );
            us_timeout -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                          - ( before.tv_sec * 1000000 + before.tv_usec );

            if ( us_timeout <= 0 )
            {
                print( FATAL, "Timeout while reading from LAN device.\n" );
                THROW( EXCEPTION );
            }
            before = after;

            bytes_read = fsc2_lan_read( vicp.handle, buffer, bytes_to_expect,
                                        us_timeout, quit_on_signal );

            if ( bytes_read == 0 )
            {
                vicp.remaining += bytes_to_expect;
                break;
            }

            if ( bytes_read < 0 )
            {
                print( FATAL, "Error in communication with LAN device.\n" );
                THROW( EXCEPTION );
            }

            buffer += bytes_read;
            total_length += bytes_read;
            bytes_to_expect -= bytes_read;
        }
    } while ( ! *with_eoi && total_length < *length && bytes_read != 0 );

    *length = total_length;

    return vicp.remaining == 0 ? VICP_SUCCESS : VICP_SUCCESS_BUT_MORE;
}


/*--------------------------------------------------------*
 * Function for sending a CLEAR command to the device.
 * This will clear all input and output buffers, abort
 * the interpretation of the current command (if any)
 * and clear any pending commands. Status and status
 * enable registers remain unchanged. The command may
 * take several seconds to finish.
 * The function also closes and then reopens the network
 * connection to the device.
 * +++ is dis- and re-connecting really necessary? +++
 *-------------------------------------------------------*/

void
vicp_device_clear( void )
{
    if ( vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, connection "
               "already closed.\n" );
        THROW( EXCEPTION );
    }

    /* Set up the header, no data are to be sent and the operations byte
       must only have the CLEAR bit set. */

    unsigned char   header[ VICP_HEADER_SIZE ] = { 0 };

    vicp_set_operation( header, VICP_CLEAR );
    vicp_set_version( header );
    vicp_set_length( header, 0 );

    /* Try to send the header, don't abort on signals */

    long us_timeout = VICP_DEFAULT_READ_WRITE_TIMEOUT;
    ssize_t bytes_written = fsc2_lan_write( vicp.handle, ( char * ) header,
                                            VICP_HEADER_SIZE, us_timeout,
                                            UNSET );

    if ( bytes_written <= 0 )
    {
        print( FATAL, "Error in communication with LAN device.\n" );
        THROW( EXCEPTION );
    }

    fsc2_assert( bytes_written == VICP_HEADER_SIZE );

    /* Sleeping for 100 ms after sending a clear seems to be necessary as far
       as I can see from the SourceForge LeCroyVICP projects sources, where
       some 'RebootScope' bug is mentioned as the reason. I don' know if this
       "bug" has been fixed (and if it's fixed for all devices), so I guess
       it's prudent to leave this "fix" in. */

    fsc2_usleep( 100000, UNSET );

    /* Next we seem to have to close the connection and reopen it again */

    fsc2_lan_close( vicp.handle );
    vicp.handle = -1;

    int fd;

    if ( ( fd = fsc2_lan_open( vicp.name, vicp.address, VICP_PORT,
                               VICP_DEFAULT_CONNECT_TIMEOUT, UNSET ) ) == -1 )
    {
        print( FATAL, "Failed to clear device - can't reopen connection.\n" );
        vicp.name = T_free( vicp.name );
        vicp.address = T_free( vicp.address );
        THROW( EXCEPTION );
    }

    vicp.handle = fd;
}


/*----------------------------------------------------------------*
 * Function for evaluating the "operation" field of a VICP header
 *----------------------------------------------------------------*/

static
inline
unsigned char
vicp_get_operation( unsigned char * header )
{
    return header[ VICP_HEADER_OPERATION_OFFSET ];
}


/*--------------------------------------------------------------*
 * Function for evaluating the "version" field of a VICP header
 *--------------------------------------------------------------*/

static
inline
unsigned char
vicp_get_version( unsigned char * header )
{
    return header[ VICP_HEADER_VERSION_OFFSET ];
}


/*----------------------------------------------------------------------*
 * Function for evaluating the "sequence number" field of a VICP header
 * (currently not used)
 *----------------------------------------------------------------------*/

#if 0
static
inline
unsigned char
vicp_get_sequence( unsigned char * header )
{
    return header[ VICP_HEADER_SEQUENCE_OFFSET ];
}
#endif


/*-------------------------------------------------------------*
 * Function for evaluating the "length" field of a VICP header
 * Please note: the length field is a big-endian number
 *-------------------------------------------------------------*/

static
inline
ssize_t
vicp_get_length( unsigned char * header )
{
    unsigned long val = 0;

    for ( int i = VICP_HEADER_MSB_OFFSET; i <= VICP_HEADER_LSB_OFFSET; i++ )
        val = ( val << 8 ) | header[ i ];

    return val;
}


/*-------------------------------------------------------------*
 * Function for setting the "operation" field of a VICP header
 *-------------------------------------------------------------*/

static
inline
void
vicp_set_operation( unsigned char * header,
                    unsigned char   operation )
{
    header[ VICP_HEADER_OPERATION_OFFSET ] = operation;
}


/*-----------------------------------------------------------*
 * Function for setting the "version" field of a VICP header
 *-----------------------------------------------------------*/

static
inline
void
vicp_set_version( unsigned char * header )
{
    header[ VICP_HEADER_VERSION_OFFSET ] = VICP_HEADER_VERSION_VALUE;
}

/*-------------------------------------------------------------------*
 * Function for setting the "sequence number" field of a VICP header
 *-------------------------------------------------------------------*/

static
inline
void
vicp_set_sequence( unsigned char * header )
{
    /* The sequence number wraps around but may never be 0 */

    if ( ++vicp.seq_number == 0 )
        vicp.seq_number++;

    header[ VICP_HEADER_SEQUENCE_OFFSET ] = vicp.seq_number;
}


/*----------------------------------------------------------*
 * Function for setting the "length" field of a VICP header
 * Please note: the length field is a big-endian number
 *----------------------------------------------------------*/

static
inline
void
vicp_set_length( unsigned char * header,
                 unsigned long   length )
{
    for ( int i = VICP_HEADER_LSB_OFFSET; i >= VICP_HEADER_MSB_OFFSET;
          length >>= 8 )
        header[ i-- ] = length & 0xFF;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
