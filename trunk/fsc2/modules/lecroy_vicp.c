/* -*-C-*-
 *
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


/* The following is a set of routines that can be used by progams that need
   to communicate with devices using the LeCroy VICP (Versatile Instrument
   Controll Protocol) protocol, running on top of TCP/IP. It uses a TCP
   connection and runs over port 1861.

   Basically, the Lecroy VICP protocol just tells that before data are
   exchanged a header must be sent. The header has the form

   struct LeCroy_VICP_Header {
       unsigned char  operation;
       unsigned char  header version;
       unsigned char  sequence_number;    // only version 1A and later
       unsigned char  reserved;
       uint_32        block_length;       // in big-endian format
   }

   The bits of the 'operation' byte have the following meaning:

   bit 7    data will be send (if with or without EOI is determined by bit 0)
   bit 6    remote mode
   bit 5    local lockout
   bit 4    clear device (if send with data clear is done before data are
                          interpreted)
   bit 3    SRQ (only to be send by the device)
   bit 2    reserved
   bit 1    reserved
   bit 0    if set data block terminates in EOI character

   The EOI bit simply tells if the data block send to the device or received
   from it ends in an EOI character ('\n' or 0x0A).

   The 'header_version' field is a number - currently only 1 is an acceptable
   value (protocol version 1A is treated as a subversion of version 1).

   The 'sequence_number' field only has a meaning for devices supporting
   the version 1A of the protocol. For these it must be a number between
   1 and 255 (but never 0), where each data block must have a "sequence
   number" incremented by 1 relative to the previous block (if 255 is
   reached the next number is then 1). For all other devices the field
   will always be set to 0. Since one can't figure out from the version
   field if the device is using version 1 or 1A of the protocol one only
   can find out from checking if the sequence number is 0 or non-zero.

   Finally, the 'block_length' member is the length of the block of data
   (in bytes) to be send following the header. It is 32-bit number in
   big-endian format, i.e. the MSB is at the lowest memory address and
   the LSB at the highest address.

   The following functions for communicating with a device using the LeCroy
   VICP protocol can be used:

    void lecroy_vicp_init( const char * dev_name,
                           const char * address,
                           long         us_timeout,
                           bool         quit_on_signal )
   
    void lecroy_vicp_close( void )
   
    void lecroy_vicp_lock_out( bool lock_out )

    void lecroy_vicp_set_timeout( int  dir,
                                  long us_timeout )

    int lecroy_vicp_write( const char * buffer,
                           ssize_t    * length,
                           bool         with_eoi,
                           bool         quit_on_signal )

    int lecroy_vicp_read( char *    buffer,
                          ssize_t * length,
                          bool    * with_eoi,
                          bool      quit_on_signal )

    void lecroy_vicp_device_clear( void )

   The function lecroy_vicp_init() must be the first function to be called.
   It establishes the connection to the device which then is used for all
   further function calls. The function expects a symbolic name for the device
   (only to be used for the log file), its IP address, either as a hostname
   or a numerical IP address in dotted-quad format, a positive or zero
   maximum time (in micro-seconds, a zero value means indefinitely long
   timeout) the function is allowed to wait for successfully establishing
   the connection and a flag that tells if the function is supposed to return
   immediately on receipt of a signal. Please note that a timer, raising a
   SIGALRM signal on expiry, is used for controlling the timeout. Thus the
   function temporarily installs its own signal handler for SIGALRM, so the
   caller should make sure that it doesn't initate anything that would also
   raise such a signal. Please also note that the function can't be called
   when an connection has already been created. On failure (either because
   the connection has already been opened, connecting to the device fails or
   not enough memory is available) the function throws an exception.

   lecroy_vicp_close() is the opposite of lecroy_vicp_init(), i.e. it closes
   down the existing connection. It throws an exception when you try to
   close an already closed connection.

   lecroy_vicp_lock_out() allows you to control if the DSO is in
   local_lockout state (the default when a connection is made) or not.
   By calling the funtion with a true boolean value local lockout gets
   switched on, switch it off by calling it with a false value.

   lecroy_vicp_set_timeout() allows to set timeouts for read or write
   operations. If the first argument is the symbolic value READ (or 0),
   a timeout for read operations gets set, if it's WRITE (or 1) the
   timeout for writes gets set. Timeouts must be specified in micro-
   seconds and must be either positive numbers or zero, in which case an
   indefinitely long timeout is used. Without calling the function a
   default timeout of 5 seconds  will be used. Please note that the function
   can only be called after the connection has been established. Please
   also note that on some systems (those tht don't support the SO_RCVTIMEO
   and SO_SNDTIMEO socket options) the timeouts get created via a timer
   that raises a SIGALRM signal. Thus on these systems the caller may not
   initiate any action that would raise such a signal when calling one of
   the functions that can timeout.

   The function lecroy_vicp_write() is used to send data to the device.
   It requires four arguments, a buffer with the data to be send, a pointer
   to a variable with the number of bytes to be send, a flag that tells
   if the data are send with a trailing EOI and a flag that tells if the
   function is supposed to return immediately when receiving a signal. The
   function returns either SUCCESS (0) if the date could be send successfully,
   or FAILURE (-1) if sending the data aborted due to receiving a signal. On
   errors or timeouts the function closes the connection and throws an
   exception. On return the variable pointed to by the secand argument will
   contain the number of bytes that have been sent - this also is the case
   if the function returns FAILURE or did throw an exception.

   lecroy_vicp_read() is for reading data the device sends. It takes three
   arguments, a buffer for storing the data, a pointer to a variable with
   the length of the buffer, a pointer to a variable that tells on return if
   EOI was set for the data and a flag telling if the function is supposed
   to return immediately on signals. The function returns SUCCESS_BUT MORE
   (1) if a reply was received but not all data could be read because they
   wouldn't have fit into the user supplied buffer, SUCCESS (0) if the reply
   was read completely and FAILURE (-1) if the function aborted because
   a signal was received. On erros or timeouts the function closes the
   connection and throws an exception. On return the variable pointed to by
   the second argument is set to the number of bytes that have been received
   and the third one shows if the data sent have a trailing EOI, even if the
   function did return with FAILURE or threw an exception.

   if  the function returns less data than the device was willing to send
   (in which case SUCCESS_BUT_MORE gets returned) the next invocation of
   lecroy_vicp_read() will return these yet unread data, even if another
   reply by the device was initiated by sending another command in between.
   I.e. "new data" (data resulting from the next command) only will be
   returned after the function has been called until SUCCESS has been
   returned.

   Please note: If a read or write gets aborted due to a signal there may
   still be data to be read from the device or the device may still be
   waiting to receive remaining data. Unless you're closing the connection
   after receipt of a signal you must make sure that the remaining data
   are fetched from or get send to the device.

   lecroy_vicp_device_clear() allows to clear the device and reset the
   connection. Clearing the device includes clearing its input and output
   buffers and aborting the interpretation of the current connand (if any)
   as well as clearing all pending commands. Status and status enable
   registers remain unchanged. All of this may take several seconds to
   finish. The function also closes and reopens the connection to the
   device.


   The availability of source code of implementations written by Steve D.
   Sharples from Nottingham University (steve.sharples@nottingham.ac.uk),
   see

   http://osam.eee.nottingham.ac.uk/lecroy_tcp/

   and for the (Windows) LeCroyVICP Client Library written by Anthony Cake
   (anthonyrc@users.sourceforge.net) and Larry Salant
   (larryss@users.sourceforge.net), available at

   http://sourceforge.net/projects/lecroyvicp/

   were of great help in writing my own implementation, especially given
   the rather meager amount of information in the Remote Control Manuals
   by LeCroy, and are gratefully acknowledged.
*/


#include "lecroy_vicp.h"
#include "lan.h"
#include <sys/time.h>


/* Port the LeCroy-VICP server is listening on */

#define LECROY_VICP_PORT                        1861


/* Constants for the bits in the headers operation byte */

#define LECROY_VICP_DATA                        ( 1 << 7 )
#define LECROY_VICP_REMOTE                      ( 1 << 6 )
#define LECROY_VICP_LOCKOUT                     ( 1 << 5 )
#define LECROY_VICP_CLEAR                       ( 1 << 4 )
#define LECROY_VICP_SRQ                         ( 1 << 3 )
#define LECROY_VICP_SERIAL_POLL                 ( 1 << 2 )
#define LECROY_VICP_EOI                         ( 1 << 0 )


/* Size of the header to be sent or received with each data package */

#define LECROY_VICP_HEADER_SIZE                 8


/* Header version */

#define LECROY_VICP_HEADER_VERSION_VALUE        1


/* Positions of the different entries in the header */

#define LECROY_VICP_HEADER_OPERATION_OFFSET     0
#define LECROY_VICP_HEADER_VERSION_OFFSET       1
#define LECROY_VICP_HEADER_SEQUENCE_OFFSET      2
#define LECROY_VICP_HEADER_MSB_OFFSET           4
#define LECROY_VICP_HEADER_LSB_OFFSET           7


/* Default timeout for connect() call */

#define LECROY_VICP_DEFAULT_CONNECT_TIMEOUT     10000000    /* 10 s */


/* Default timeout for read an write operations */

#define LECROY_VICP_DEFAULT_READ_WRITE_TIMEOUT  5000000    /* 5 s */


/* Internal structure for storing states and information about the device */

typedef struct LeCroy_VICP LeCroy_VICP_T;

struct LeCroy_VICP {
    int             handle;
    unsigned char   seq_number;
    unsigned char   partial_header[ LECROY_VICP_HEADER_SIZE ];
    ssize_t         header_count;
    ssize_t         remaining;
    bool            eoi_was_set;
    char          * name;
    char          * address;
    long            us_write_timeout;
    long            us_read_timeout;
    bool            is_locked;
};


static LeCroy_VICP_T lecroy_vicp = { -1, 0, { 0 }, 0, 0, UNSET,
                                     NULL, NULL, 0, 0, UNSET };


static void lecroy_vicp_close_without_header( void );

static inline unsigned char lecroy_vicp_get_operation(
                                                      unsigned char * header );

static inline unsigned char lecroy_vicp_get_version( unsigned char * header );

static inline unsigned char lecroy_vicp_get_sequence( unsigned char * header );

static inline ssize_t lecroy_vicp_get_length( unsigned char * header );

static inline void lecroy_vicp_set_operation( unsigned char * header,
                                              unsigned char   operation );

static inline void lecroy_vicp_set_version( unsigned char * header );

static inline void lecroy_vicp_set_sequence( unsigned char * header );

static inline void lecroy_vicp_set_length( unsigned char * header,
                                           unsigned long   lenght );


/*--------------------------------------------------------------*
 * Function for opening a network connection to the device.
 * ->
 *    1. Name of the device
 *    2. IP address of the device (either in dotted-quad form
 *       or as name that can be resolved via a DNS request)
 *    3. Maximum timeout (in micro-seconds) to wait for the
 *       connection
 *       to succeeded
 *    4. Flag, if set the attempt to connect to the device can
 *       be aborted due to a signal, otherwise the connect()
 *       call will restarted if a signal gets caught.
 * On failures the function throws an exception.
 *-------------------------------------------------------------*/

void lecroy_vicp_init( const char * dev_name,
                       const char * address,
                       long         us_timeout,
                       bool         quit_on_signal )
{
    int            fd;
    unsigned char  header[ LECROY_VICP_HEADER_SIZE ] = { 0 };
    ssize_t        bytes_written;


    CLOBBER_PROTECT( us_timeout );

    if ( lecroy_vicp.handle >= 0 )
    {
        print( FATAL, "Internal error in module, connection already "
               "exists.\n" );
        THROW( EXCEPTION );
    }

    fsc2_assert( dev_name != NULL );
    fsc2_assert( address != NULL );

    if ( us_timeout < 0 )
        us_timeout = LECROY_VICP_DEFAULT_CONNECT_TIMEOUT;

    if ( ( fd = fsc2_lan_open( dev_name, address, LECROY_VICP_PORT,
                               us_timeout, quit_on_signal ) ) == -1 )
    {
        print( FATAL, "Failed to open connection to device.\n" );
        THROW( EXCEPTION );
    }

    lecroy_vicp.handle           = fd;
    lecroy_vicp.seq_number       = 0;
    lecroy_vicp.header_count     = 0;
    lecroy_vicp.remaining        = 0;
    lecroy_vicp.eoi_was_set      = UNSET;
    lecroy_vicp.us_write_timeout =
    lecroy_vicp.us_read_timeout  = LECROY_VICP_DEFAULT_READ_WRITE_TIMEOUT;
    lecroy_vicp.name             =
    lecroy_vicp.address          = NULL;

    /* Also store name and address for the device, this information is 
       needed in case we have to dis- and then reconnect. */

    TRY
    {
        lecroy_vicp.name    = T_strdup( dev_name );
        lecroy_vicp.address = T_strdup( address );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        lecroy_vicp_close_without_header( );
        RETHROW( );
    }

    /* Finally bring the device into the remote state by sending a header
       where just the remote and the lockout bits in the operations byte
       are set */

    lecroy_vicp_set_operation( header,
                               LECROY_VICP_REMOTE | LECROY_VICP_LOCKOUT );
    lecroy_vicp_set_version( header );
    lecroy_vicp_set_length( header, 0 );

    bytes_written = fsc2_lan_write( lecroy_vicp.handle, header,
                                    LECROY_VICP_HEADER_SIZE, us_timeout,
                                    UNSET );

    if ( bytes_written <= 0 )
    {
        lecroy_vicp_close_without_header( );
        THROW( EXCEPTION );
    }

    fsc2_assert( bytes_written == LECROY_VICP_HEADER_SIZE );

    lecroy_vicp.is_locked = SET;
}


/*--------------------------------------------------*
 * Local function for closing the connection to the
 * device and resetting some internal variables.
 *--------------------------------------------------*/

static void lecroy_vicp_close_without_header( void )
{
    fsc2_lan_close( lecroy_vicp.handle );

    lecroy_vicp.handle = -1;

    if ( lecroy_vicp.name )
        T_free( lecroy_vicp.name );

    if ( lecroy_vicp.address )
        T_free( lecroy_vicp.address );
}


/*---------------------------------------------------*
 * Function for closing the connection to the device
 * after bringing it back to local
 *---------------------------------------------------*/

void lecroy_vicp_close( void )
{
    unsigned char  header[ LECROY_VICP_HEADER_SIZE ] = { 0 };
    long           us_timeout = LECROY_VICP_DEFAULT_READ_WRITE_TIMEOUT;


    if ( lecroy_vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        THROW( EXCEPTION );
    }

    /* Send a header with all bits reset in order to get device out of
       remote (and possibly local lockout) state */

    lecroy_vicp_set_version( header );
    lecroy_vicp_set_length( header, 0 );

    fsc2_lan_write( lecroy_vicp.handle, header,
                    LECROY_VICP_HEADER_SIZE, us_timeout,
                    UNSET );

    /* Now close down the connection to the device */

    lecroy_vicp_close_without_header( );

    fsc2_lan_close( lecroy_vicp.handle );

    lecroy_vicp.handle = -1;

    if ( lecroy_vicp.name )
        T_free( lecroy_vicp.name );

    if ( lecroy_vicp.address )
        T_free( lecroy_vicp.address );
}


/*------------------------------------------------------*
 * Function to switch between locked and unlocked state
 * ->
 *    1. boolean value, if set lock out device,
 *       otherwise unlock it
 *------------------------------------------------------*/
 
void lecroy_vicp_lock_out( bool lock_state )
{
    unsigned char op = LECROY_VICP_REMOTE;
    unsigned char  header[ LECROY_VICP_HEADER_SIZE ] = { 0 };
    ssize_t        bytes_written;
    long           us_timeout = LECROY_VICP_DEFAULT_READ_WRITE_TIMEOUT;


    if ( lecroy_vicp.handle >= 0 )
    {
        print( FATAL, "Internal error in module, connection already "
               "exists.\n" );
        THROW( EXCEPTION );
    }

    if ( ( lecroy_vicp.is_locked && lock_state ) ||
         ( ! lecroy_vicp.is_locked && ! lock_state ) )
        return;

    if ( lock_state )
        op |= LECROY_VICP_LOCKOUT;

    if ( lecroy_vicp.eoi_was_set )
        op |= LECROY_VICP_EOI;
            
    lecroy_vicp_set_operation( header, op );
    lecroy_vicp_set_version( header );
    lecroy_vicp_set_length( header, 0 );

    bytes_written = fsc2_lan_write( lecroy_vicp.handle, header,
                                    LECROY_VICP_HEADER_SIZE, us_timeout,
                                    UNSET );

    if ( bytes_written <= 0 )
    {
        lecroy_vicp_close_without_header( );
        THROW( EXCEPTION );
    }

    fsc2_assert( bytes_written == LECROY_VICP_HEADER_SIZE );

    lecroy_vicp.is_locked = lock_state;
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

void lecroy_vicp_set_timeout( int  dir,
                              long us_timeout )
{
    if ( lecroy_vicp.handle < 0 )
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
        lecroy_vicp.us_read_timeout  = us_timeout;
    else
        lecroy_vicp.us_write_timeout = us_timeout;
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
 *     a) SUCCESS (0)           data have been send <successfully
 *     c) FAILURE (-1)          transmission was aborted due to a
 *                              signal
 *    If a timeout happens during the transmission an exception gets
 *    thrown.
 *-------------------------------------------------------------------*/

int lecroy_vicp_write( const char * buffer,
                       ssize_t    * length,
                       bool         with_eoi,
                       bool         quit_on_signal )
{
    unsigned char  header[ LECROY_VICP_HEADER_SIZE ] = { 0 };
    unsigned char  op = LECROY_VICP_DATA | LECROY_VICP_REMOTE;
    struct iovec   data[ 2 ];
    ssize_t        total_length;
    ssize_t        bytes_written;
    long           us_timeout = lecroy_vicp.us_write_timeout;
    struct timeval before,
                   after;


    /* Do nothing if there are no data to send */

    if ( *length == 0 )
        return SUCCESS;

    if ( buffer == NULL )
    {
        print( FATAL, "Internal error in module, write data buffer "
               "is NULL.\n" );
        THROW( EXCEPTION );
    }

    if ( lecroy_vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, connection "
               "already closed.\n" );
        THROW( EXCEPTION );
    }

    /* Set up a iovec structure in order to allow sending the header
       and the data in one go with writev() */

    data[ 0 ].iov_base = header;
    data[ 0 ].iov_len  = LECROY_VICP_HEADER_SIZE;
    data[ 1 ].iov_base = ( void * ) buffer;
    data[ 1 ].iov_len  = *length;

    /* Assemble the header, set the EOI flag if the user tells us so */

    if ( with_eoi )
        op |= LECROY_VICP_EOI;

    lecroy_vicp_set_operation( header, op );
    lecroy_vicp_set_version( header );
    lecroy_vicp_set_sequence( header );
    lecroy_vicp_set_length( header, *length );

    /* Now start pushing the data over to the device. Since there is no
       guarantee that all of them will be written in one go (e.g. because
       there are more than fit into a packet) we must go on trying if less
       then the requested amount of bytes could be send on the first try. */

    gettimeofday( &before, NULL );

    bytes_written = fsc2_lan_writev( lecroy_vicp.handle, data, 2,
                                     us_timeout, quit_on_signal );

    if ( bytes_written < 0 )
        THROW( EXCEPTION );

    if ( bytes_written == 0 )
    {
        *length = 0;
        return FAILURE;
    }

    total_length = bytes_written - LECROY_VICP_HEADER_SIZE;

    fsc2_assert( total_length >= 0 );

    while ( total_length < *length )
    {
        gettimeofday( &after, NULL );
        us_timeout -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                      - ( before.tv_sec * 1000000 + before.tv_usec );

        if ( us_timeout <= 0 )
            THROW( EXCEPTION );

        before = after;

        buffer += total_length;

        bytes_written = fsc2_lan_write( lecroy_vicp.handle, buffer,
                                        *length - total_length, us_timeout,
                                        quit_on_signal );

        if ( bytes_written < 0 )
            THROW( EXCEPTION );

        total_length += bytes_written;

        if ( bytes_written == 0 )
        {
            *length = total_length;
            return FAILURE;
        }
    }

    return SUCCESS;
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
 *     a) SUCCESS_BUT_MORE (1)  data have successfully received but
 *                              there are still data the device wants
 *                              to transmit but which wouldn't hav
 *                              fit into the user supplied buffer
 *     b) SUCCESS (0)           all data have been received successfully
 *     c) FAILURE (-1)          transmission was aborted due to a
 *                              signal
 *    If a timeout happens during the transmission an exception gets
 *    thrown.
 *---------------------------------------------------------------------*/

int lecroy_vicp_read( char *    buffer,
                      ssize_t * length,
                      bool *    with_eoi,
                      bool      quit_on_signal )
{
    unsigned char   header[ LECROY_VICP_HEADER_SIZE ];
    ssize_t         total_length = 0;
    ssize_t         bytes_read;
    unsigned long   bytes_to_expect;
    struct timeval  before,
                    after;
    long            us_timeout = lecroy_vicp.us_read_timeout;


    /* Do nothing if there are no data to be send */

    if ( *length == 0 )
        return SUCCESS;

    if ( buffer == NULL )
    {
        print( FATAL, "Internal error in module, read data buffer is "
               "NULL.\n" );
        THROW( EXCEPTION );
    }

    if ( lecroy_vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, connection is "
               "already closed.\n" );
        THROW( EXCEPTION );
    }

    /* Check if there are still outstanding bytes, i.e. bytes of wich we know
       from reading the last received header that they are in the process of
       being sent by the device. Get them first. */

    gettimeofday( &before, NULL );

    if ( lecroy_vicp.remaining > 0 )
    {
        *with_eoi = lecroy_vicp.eoi_was_set;

        while ( lecroy_vicp.remaining > 0 && total_length < *length )
        {
            gettimeofday( &after, NULL );
            us_timeout -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                          - ( before.tv_sec * 1000000 + before.tv_usec );

            if ( us_timeout <= 0 )
                THROW( EXCEPTION );
            before = after;

            bytes_read = fsc2_lan_read( lecroy_vicp.handle, buffer,
                                        ss_min( lecroy_vicp.remaining,
                                                *length ),
                                        us_timeout, quit_on_signal );

            if ( bytes_read > 0 )
            {
                lecroy_vicp.remaining -= bytes_read;
                total_length += bytes_read;
                buffer += bytes_read;
            }

            if ( bytes_read < 0 )
                THROW( EXCEPTION );

            if ( bytes_read == 0 )
                break;
        }

        return lecroy_vicp.remaining == 0 ? SUCCESS : SUCCESS_BUT_MORE;
    }

    gettimeofday( &before, NULL );
    bytes_read = fsc2_lan_read( lecroy_vicp.handle, header,
                                LECROY_VICP_HEADER_SIZE,
                                us_timeout, quit_on_signal );
    
    if ( bytes_read < 0 )
        THROW( EXCEPTION );

    if ( bytes_read == 0 )
    {
        *length = 0;
        return FAILURE;
    }

    fsc2_assert( bytes_read == LECROY_VICP_HEADER_SIZE );

    /* Check the version field - if this is not correct something must
       have gone seriously wrong */

    if ( lecroy_vicp_get_version( header ) !=
                                             LECROY_VICP_HEADER_VERSION_VALUE )
        THROW( EXCEPTION );

    /* The header tells us how many bytes we can expect - if there are
       none something must really be going wrong */

    if ( ( bytes_to_expect = lecroy_vicp_get_length( header ) ) == 0 )
        THROW( EXCEPTION );

    /* Examine and store the EOI bit */

    *with_eoi = lecroy_vicp.eoi_was_set =
                         lecroy_vicp_get_operation( header ) & LECROY_VICP_EOI;

    /* Otherwise we can now start to read the real data. Make sure we don't
       try to read more than the user asked for. If we could read more we
       store the number of bytes we couldn't fetch. */

    if ( ( ssize_t ) bytes_to_expect - total_length > *length )
    {
        lecroy_vicp.remaining = bytes_to_expect - *length + total_length;
        bytes_to_expect = *length - total_length;
    }

    while ( bytes_to_expect > 0 )
    {
        gettimeofday( &after, NULL );
        us_timeout -=   ( after.tv_sec  * 1000000 + after.tv_usec  )
                      - ( before.tv_sec * 1000000 + before.tv_usec );

        if ( us_timeout <= 0 )
            THROW( EXCEPTION );
        before = after;

        bytes_read = fsc2_lan_read( lecroy_vicp.handle, buffer,
                                    bytes_to_expect,
                                    us_timeout, quit_on_signal );

        if ( bytes_read == 0 )
        {
            lecroy_vicp.remaining += bytes_to_expect;
            break;
        }

        if ( bytes_read < 0 )
            THROW( EXCEPTION );

        buffer += bytes_read;
        total_length += bytes_read;
        bytes_to_expect -= bytes_read;
    }

    *length = total_length;

    return lecroy_vicp.remaining == 0 ? SUCCESS : SUCCESS_BUT_MORE;
}


/*--------------------------------------------------------*
 * Function for sending a CLEAR command to the device.
 * This will clear all input and output buffers, abort
 * the interpretation of the current command (if any)
 * and clear any pending commands. Status and status
 * enable registers remain unchanged. The command may
 * take several seconds to finish.
 * The function also closes and then reopens the network
 * connection to device.
 * +++ is dis- and re-connecting really necessary? +++
 *-------------------------------------------------------*/

void lecroy_vicp_device_clear( void )
{
    int             fd;
    unsigned char   header[ LECROY_VICP_HEADER_SIZE ] = { 0 };
    ssize_t         bytes_written;
    long            us_timeout = LECROY_VICP_DEFAULT_READ_WRITE_TIMEOUT;


    if ( lecroy_vicp.handle < 0 )
    {
        print( FATAL, "Internal error in module, connection "
               "already closed.\n" );
        THROW( EXCEPTION );
    }

    /* Set up the header, no data are to be sent and the operations byte
       must only have the CLEAR bit set. */

    lecroy_vicp_set_operation( header, LECROY_VICP_CLEAR );
    lecroy_vicp_set_version( header );
    lecroy_vicp_set_length( header, 0 );

    /* Try to send the header, don't abort on signals */

    bytes_written = fsc2_lan_write( lecroy_vicp.handle, header,
                                    LECROY_VICP_HEADER_SIZE, us_timeout,
                                    UNSET );

    if ( bytes_written <= 0 )
    {
        lecroy_vicp_close( );
        THROW( EXCEPTION );
    }

    fsc2_assert( bytes_written == LECROY_VICP_HEADER_SIZE );

    /* Sleeping for 100 ms after sending a clear seems to be necessary as far
       as I can see from the SourceForge LeCroyVICP projects sources, where
       some 'RebootScope' bug is mentioned as the reason. I don' know if this
       "bug" has been fixed (and if it's fixed for all devices), so I guess
       it's prudent to leave this "fix" in. */

    fsc2_usleep( 100000, UNSET );

    /* Next we seem to have to close the connection and reopen it again */

    fsc2_lan_close( lecroy_vicp.handle );

    if ( ( fd = fsc2_lan_open( lecroy_vicp.name, lecroy_vicp.address,
                               LECROY_VICP_PORT,
                               LECROY_VICP_DEFAULT_CONNECT_TIMEOUT,
                               UNSET ) ) == -1 )
    {
        print( FATAL, "Failed to clear device - can't reopen connection.\n" );
        T_free( lecroy_vicp.name );
        T_free( lecroy_vicp.address );
        lecroy_vicp.handle = -1;
        THROW( EXCEPTION );
    }

    lecroy_vicp.handle = fd;
}


/*--------------------------------------------------------------*
 * Function for evaluating the operation field of a VICP header
 *--------------------------------------------------------------*/

static inline unsigned char lecroy_vicp_get_operation( unsigned char * header )
{
    return header[ LECROY_VICP_HEADER_OPERATION_OFFSET ];
}


/*------------------------------------------------------------*
 * Function for evaluating the version field of a VICP header
 *------------------------------------------------------------*/

static inline unsigned char lecroy_vicp_get_version( unsigned char * header )
{
    return header[ LECROY_VICP_HEADER_VERSION_OFFSET ];
}


/*--------------------------------------------------------------------*
 * Function for evaluation the sequence number field of a VICP header
 *--------------------------------------------------------------------*/

static inline unsigned char lecroy_vicp_get_sequence( unsigned char * header )
{
    return header[ LECROY_VICP_HEADER_SEQUENCE_OFFSET ];
}


/*-----------------------------------------------------------*
 * Function for evaluating the length field of a VICP header
 * Please note: the length field is a big-endian number
 *-----------------------------------------------------------*/

static inline ssize_t lecroy_vicp_get_length( unsigned char * header )
{
    int i = LECROY_VICP_HEADER_MSB_OFFSET;
    unsigned long val = 0;


    for ( ; i <= LECROY_VICP_HEADER_LSB_OFFSET; i++ )
        val = ( val << 8 ) | header[ i ];

    return val;
}


/*-----------------------------------------------------------*
 * Function for setting the operation field of a VICP header
 *-----------------------------------------------------------*/

static inline void lecroy_vicp_set_operation( unsigned char * header,
                                              unsigned char   operation )
{
    header[ LECROY_VICP_HEADER_OPERATION_OFFSET ] = operation;
}


/*---------------------------------------------------------*
 * Function for setting the version field of a VICP header
 *---------------------------------------------------------*/

static inline void lecroy_vicp_set_version( unsigned char * header )
{
    header[ LECROY_VICP_HEADER_VERSION_OFFSET ] =
                                              LECROY_VICP_HEADER_VERSION_VALUE;
}

/*-----------------------------------------------------------------*
 * Function for setting the sequence number field of a VICP header
 *-----------------------------------------------------------------*/

static inline void lecroy_vicp_set_sequence( unsigned char * header )
{
    if ( ++lecroy_vicp.seq_number == 0 )
        lecroy_vicp.seq_number++;

    header[ LECROY_VICP_HEADER_SEQUENCE_OFFSET ] = lecroy_vicp.seq_number;
}


/*--------------------------------------------------------*
 * Function for setting the length field of a VICP header
 * Please note: the length field is a big-endian number
 *--------------------------------------------------------*/

static inline void lecroy_vicp_set_length( unsigned char * header,
                                           unsigned long   length )
{
    int i = LECROY_VICP_HEADER_LSB_OFFSET;


    for ( ; i >= LECROY_VICP_HEADER_MSB_OFFSET; length >>= 8, i-- )
        header[ i ] = length & 0xFF;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
