/*
 *  Copyright (C) 2015 Jens Thoms Toerring
 *
 *  This file is part of Fsc3.
 *
 *  Fsc3 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 *  Fsc3 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "VXI11_Client.hpp"
#include <limits>
#include <sys/time.h>
#include <cmath>
#include <stdexcept>


int const VXI11_Client::s_TIMEOUT_ERROR = 15;
std::vector< std::string > const VXI11_Client::s_err_list =
                           { "no error",                                //  0
                             "syntax error",                            //  1
                             "undefined error (2)",                     //  2
                             "device not accessible",                   //  3
                             "invalid link identifier",                 //  4
                             "parameter error",                         //  5
                             "channel not established",                 //  6
                             "undefined error (7)",                     //  7
                             "operation not supported",                 //  8
                             "out of resources",                        //  9
                             "undefined error (10)",                    // 10
                             "device locked by another link",           // 11
                             "no lock held by this link",               // 12
                             "undefined error (13)",                    // 13
                             "undefined error (14)",                    // 14
                             "I/O timeout",                             // 15
                             "undefined error (16)",                    // 16
                             "I/O error",                               // 17
                             "undefined error (18)",                    // 18
                             "undefined error (19)",                    // 19
                             "undefined error (20)",                    // 20
                             "invalid address",                         // 21
                             "undefined error (22)",                    // 22
                             "abort",                                   // 23
                             "undefined error (24)",                    // 24
                             "undefined error (25)",                    // 25
                             "undefined error (26)",                    // 26
                             "undefined error (27)",                    // 27
                             "undefined error (28)",                    // 28
                             "channel already established",             // 29
                             "undefined error"
                                     };
int const VXI11_Client::s_RPC_TIMEOUT_ERROR = 5;
std::vector< std::string > const VXI11_Client::s_rpc_err_list =
                           { "no_error",                                //  0
                             "can't encode arguments (RPC)",            //  1
                             "can't decode results (RPC)"               //  2
                             "failure in sending call (RPC)"            //  3
                             "failure in receiving result (RPC)",       //  4
                             "call timed out (RPC)",                    //  5
                             "RPC versions not compatible (RPC",        //  6
                             "authentication error (RPC)",              //  7
                             "program not available (RPC)",             //  8
                             "program version mismatched (RPC)",        //  9
                             "procedure unavailable (RPC)",             // 10
                             "decode arguments error (RPC)",            // 11
                             "generic other problem (RPC)",             // 12
                             "unknown host name (RPC)",                 // 13
                             "portmapper failed in its call (RPC)",     // 14
                             "remote program is not registered (RPC)",  // 15
                             "RPC_FAILED (RPC 16)",                     // 16
                             "unknown protocol (RPC)",                  // 17
                             "RPC_INTR (RPC 18)",                       // 18
                             "remote address unknown (RPC)",            // 19
                             "RPC_TLIERROR (RPC 20)",                   // 20
                             "broadcasting not supported (RPC)",        // 21
                             "name to addr translation failed (RPC)",   // 22
                             "RPC_UDERROR (RPC 23)",                    // 23
                             "RPC_INPROGRESS (RPC 24)",                 // 24
                             "RPC_STALERACHANDLE ( RPC 25)",            // 25
                             "undefined error (RPC)"
                           };


/*----------------------------------------------------*
 *----------------------------------------------------*/

/*----------------------------------------------------*
 *----------------------------------------------------*/

VXI11_Client::VXI11_Client( std::string const & device_name,
                            std::string const & address,
                            std::string const & vxi11_name,
                            Log_Level           log_level )
    : Client_Logger( device_name, log_level )
    , m_ip( address )
    , m_vxi11_name( vxi11_name )
{
    char const * tmpdir = getenv( "TMP" );
    if ( ! tmpdir )
        tmpdir = "/tmp";

    set_log_filename( device_name + " (VXI-11 at " + address + ").log",
                      tmpdir );
}


/*------------------------------------------------------------*
 * Opens an optionally locked connection to the device.
 * ->
 *    1. Flag, if set also an async connection is to be created
 *       (to ne able to abort commands, but not all devices
 *       seem to support it)
 *    2. Flag that tells if a exclusive lock on the device
 *       should be requested.
 *    3. Maximum time (in seconds) to wait for the connection
 *       if it's locked by another process (0 or negative value
 *       is interpreted to mean a nearly infinite time)
 *
 * Note: connecting can take quite a bit of time and it only
 * times out after about 6 s (this can't be controlled via the
 * 'timeout' argument!).
 *------------------------------------------------------------*/

bool
VXI11_Client::connect( bool   create_async,
                       bool   lock_device,
                       double lock_timeout )
{
    if ( is_connected( ) )
        return true;

    m_timed_out = false;

    log_function_start( "connect" );

    if ( m_ip.empty( ) )
    {
        log_error( "Invalid empty IP address" );
        log_function_end( "connect" );
        return false;
    }

    if ( m_vxi11_name.empty( ) )
    {
        log_error( "Invalid empty VXI-11 name" );
        log_function_end( "connect" );
        return false;
    }

    log_message( "Trying to connect to address '%s' using VXI-11 name '%s'",
                 m_ip.c_str( ), m_vxi11_name.c_str( ) );

    if ( ! ( m_core_client = clnt_create( m_ip.c_str( ), DEVICE_CORE,
                                          DEVICE_CORE_VERSION, "tcp" ) ) )
    {
        log_error( "Failed to open channel to device" );
        log_function_end( "connect" );
        return false;
    }

    /* Set up link parameter */

    Create_LinkParms link_parms;
    link_parms.clientId     = 0;
    link_parms.lockDevice   = lock_device;
    link_parms.lock_timeout = to_ms( lock_timeout, m_default_lock_timeout );
    link_parms.device       = const_cast< char * >( m_vxi11_name.c_str( ) );

    set_rpc_timeout( link_parms.lock_timeout );

    int rpc_res;
    if ( ( rpc_res = create_link_1( link_parms, &m_core_link, m_core_client ) )
                                                               != RPC_SUCCESS )
    {
        if ( timed_out( m_core_link.error, rpc_res, "connect" ) )
            log_error( "Failed to connect: %s",
                       sperror( m_core_link.error, rpc_res ) );
            
        log_function_end( "connect" );

        clnt_destroy( m_core_client );
        m_core_client = nullptr;
        return false;
    }

    /* Steve D. Sharples found that some Agilent Infiniium scopes return 0 for
       the maximum size of data they're prepared to accept. That's obviously
       a bug and in violation of Rule B.6.3, item 3, which requires a size of
       at least 1024. To make things work set it to the minimum value required
       by the standard. */

    if ( m_core_link.maxRecvSize == 0 )
        m_core_link.maxRecvSize = 1024;

    /* Now, if we're asked to, also establish an async connection */

    if ( create_async && ! async_connect( ) )
    {
        Device_Error dev_error = { 0 };
        destroy_link_1( m_core_link.lid, &dev_error, m_core_client );

        clnt_destroy( m_core_client );
        m_core_client = nullptr;

        return false;
    }

    log_function_end( "connect" );
    return true;
}


/*------------------------------------------------------------*
 * Sets up an asynchronous connection (as required for the abort()
 * method) beside an already existing normal connection.
 *------------------------------------------------------------*/

bool
VXI11_Client::async_connect( )
{
    if ( ! is_connected( ) )
    {
        log_error( "Attempt to create asyn link to unconnected device" );
        return false;
    }

    if ( is_async_connected( ) )
        return true;

    log_function_start( "async_connect" );

    if ( ! ( m_async_client = clnt_create( m_ip.c_str( ), DEVICE_ASYNC,
                                           DEVICE_ASYNC_VERSION, "tcp" ) ) )
    {
        std::string mess =   "Failed to open async channel to device "
                           + m_device_name;
        log_error( clnt_spcreateerror( mess.c_str( ) ) );
        log_function_end( "async_connect" );
        return false;
    }

    Create_LinkParms link_parms;
    link_parms.clientId     = 0;
    link_parms.lockDevice   = false;
    link_parms.lock_timeout = 0;
    link_parms.device       = const_cast< char * >( m_vxi11_name.c_str( ) );

    set_rpc_timeout( link_parms.lock_timeout );

    int rpc_res;
    if ( ( rpc_res = create_link_1( link_parms, &m_async_link,
                                    m_async_client ) ) != RPC_SUCCESS )
    {
        if ( ! timed_out( m_async_link.error, rpc_res, "create aync link" ) )
            log_error( "Failed to set up async channel: %s",
                       sperror( m_async_link.error, rpc_res ) );
        log_function_end( "async_connect" );

        clnt_destroy( m_async_client );
        m_async_client = nullptr;

        return false;
    }

    log_function_end( "async_connect" );
    return true;
}


/*----------------------------------------------------*
 * Closes all connections to the device.
 *----------------------------------------------------*/

bool
VXI11_Client::disconnect( )
{
    if ( ! is_connected( ) )
        return true;

    m_timed_out = false;

    log_function_start( "disconnect" );

    bool ret = true;

    if ( is_async_connected( ) )
        ret = async_disconnect( );

    Device_Error dev_error = { 0 };
    int rpc_res;
    if (    ( rpc_res = destroy_link_1( m_core_link.lid, &dev_error,
                                        m_core_client ) ) != RPC_SUCCESS
         || dev_error.error != 0 )
    {
        if ( ! timed_out( dev_error.error, rpc_res, "disconnect" ) )
            log_error( "Failed to close core link: %s",
                       sperror( dev_error.error, rpc_res ) );
        ret = false;
    }

    clnt_destroy( m_core_client );
    m_core_client = nullptr;

    log_function_end( "disconnect" );

    m_more_available = false;
    m_is_locked = false;
    return ret;
}


/*----------------------------------------------------*
 * Closes the async connection to the device.
 *----------------------------------------------------*/

bool
VXI11_Client::async_disconnect( )
{
    if ( ! is_async_connected( ) )
        return true;

    log_function_start( "async_disconnect" );

    bool ret = true;

    Device_Error dev_error = { 0 };
    int rpc_res;
    if (    ( rpc_res = destroy_link_1( m_async_link.lid, &dev_error,
                                        m_async_client ) ) != RPC_SUCCESS
         || dev_error.error != 0 )
    {
        log_error( "Failed to close async link: %s",
                   sperror( dev_error.error, rpc_res ) );
        ret = false;
    }

    clnt_destroy( m_async_client );
    m_async_client = nullptr;

    log_function_end( "async_disconnect" );
    return ret;
}


/*----------------------------------------------------*
 * Locks the device (i.e.makes the link exclusive)
 *----------------------------------------------------*/

bool
VXI11_Client::lock( double lock_timeout )
{
    if ( ! is_connected( ) )
    {
        log_error( "Attempt to lock unconnected device" );
        return false;
    }

    if ( m_is_locked )
        return true;

    log_function_start( "lock" );

    Device_LockParms lock_parms;
    lock_parms.lid          = m_core_link.lid;
    lock_parms.flags        = Flags::WAITLOCK_SET;
    lock_parms.lock_timeout = to_ms( lock_timeout, m_default_lock_timeout );

    set_rpc_timeout( lock_parms.lock_timeout );

    Device_Error dev_error = { 0 };
    int rpc_res;
    if (    ( rpc_res = device_lock_1( lock_parms, &dev_error,
                                       m_core_client ) ) != RPC_SUCCESS
         || dev_error.error != 0 )
    {
        if ( ! timed_out( dev_error.error, rpc_res, "lock" ) )
            log_error( "Failed lock device: %s",
                       sperror( dev_error.error, rpc_res ) );
        log_function_end( "lock" );
        return false;
    }

    m_is_locked = true;
    log_function_end( "lock" );
    return true;
}


/*----------------------------------------------------*
 * Unlocks the device
 *----------------------------------------------------*/

bool
VXI11_Client::unlock( )
{
    if ( ! is_connected( ) )
    {
        log_error( "Attempt to unlock unconnected device" );
        return false;
    }

    if ( ! m_is_locked )
        return true;

    log_function_start( "unlock" );

    m_timed_out = false;

    Device_Error dev_error = { 0 };
    int rpc_res;
    if (    ( rpc_res = device_unlock_1( m_core_link.lid, &dev_error,
                                         m_core_client ) ) != RPC_SUCCESS
         || dev_error.error != 0 )
    {
        if ( ! timed_out( dev_error.error, rpc_res, "unlock" ) )
            log_error( "Failed unlock device: %s",
                       sperror( dev_error.error, rpc_res ) );
        log_function_end( "unlock" );
        return false;
    }

    m_is_locked = false;
    log_function_end( "unlock" );
    return true;
}


/*--------------------------------------------------------*
 * Asks the device to send its status byte.
 *--------------------------------------------------------*/

bool
VXI11_Client::read_stb( unsigned char & stb,
                        double          timeout )
{
    if ( ! is_connected( ) )
    {
        log_error( "Attempt to read STB from unconnected device" );
        return false;
    }

    log_function_start( "read_stb" );

    unsigned long to = to_ms( timeout, m_default_read_timeout );

    log_message( "Trying to read STB within %lu ms", to );

    Device_GenericParms generic_parms = { m_core_link.lid, 0, 0, to };
    Device_ReadStbResp readstb_resp = { 0, 0 };

    set_rpc_timeout( to );

    int rpc_res;
    if (    ( rpc_res = device_readstb_1( generic_parms, &readstb_resp,
                                          m_core_client ) ) != RPC_SUCCESS
         || readstb_resp.error != 0 )
    {
        if ( ! timed_out( readstb_resp.error, rpc_res, "read STB" ) )
            log_error( "Failed to read STB: %s",
                       sperror( readstb_resp.error, rpc_res ) );
        log_function_end( "read_stb" );
        return false;
    }

    stb = readstb_resp.stb;
    log_function_end( "read_stb" );
    return true;
}


/*----------------------------------------------------------*
 * Switches between locked-out and unlocked state
 * ->
 *    1. boolean value, if set lock out device,
 *       otherwise unlock it
 *
 * Please note: some devices may not support this operation
 * and it's not always possible to detect this. i.e. the
 * function may return indicating success even though the
 * lock state of the device didn't change.
 *----------------------------------------------------------*/

bool
VXI11_Client::lock_out( bool   lock_state,
                        double timeout )
{
    if ( ! is_connected( ) )
    {
        log_error( "Attempt to %s unconnected device",
                   lock_state ? "lock-out" : "remove lock-out for" );
        return false;
    }

    log_function_start( "lock_out" );

    unsigned long to = to_ms( timeout, m_default_write_timeout );

    log_message( "Trying to put device in %s state within %lu ms",
                 lock_state ? "remote" : "local", to );

    Device_GenericParms generic_parms = { m_core_link.lid, 0, 0, to };

    set_rpc_timeout( to );

    Device_Error dev_error = { 0 };
    int rpc_res;

    if (    ( rpc_res = ( lock_state ? device_remote_1 : device_local_1 )
              ( generic_parms, &dev_error, m_core_client ) ) != RPC_SUCCESS
         || dev_error.error != 0 )
    {
        if ( ! timed_out( dev_error.error, rpc_res,
                          lock_state ? "lock out" : "remove lockout" ) )
            log_error( "Failed to %s device: %s",
                       lock_state ? "lock" : "remove lock for",
                       sperror( dev_error.error, rpc_res ) );
        log_function_end( "lock_out" );
        return false;
    }

    log_function_end( "lock_out" );
    return true;
}


/*----------------------------------------------------------*
 * Sends a device clear to the device.
 *
 * Please note: some devices may not support this operation
 * and it's not always possible to detect this. i.e. the
 * function may return indicating success even though
 * the device wasn't cleared.
 *----------------------------------------------------------*/

bool
VXI11_Client::clear( double timeout )
{
    if ( ! is_connected( ) )
    {
        log_error( "Attempt to clear unconnected device" );
        return false;
    }

    log_function_start( "clear" );

    unsigned long to = to_ms( timeout, m_default_write_timeout );

    log_message( "Trying to send device clear within %lu ms", to );
    
    Device_GenericParms generic_parms = { m_core_link.lid, 0, 0, to };

    set_rpc_timeout( to );

    Device_Error dev_error = { 0 };
    int rpc_res;

    if (    ( rpc_res = device_clear_1( generic_parms, &dev_error,
                                        m_core_client ) ) != RPC_SUCCESS
         || dev_error.error != 0 )
    {
        if ( ! timed_out( dev_error.error, rpc_res, "send device clear" ) )
            log_error( "Failed to send clear: %s",
                       sperror( dev_error.error, rpc_res ) );
        log_function_end( "clear" );
        return false;
    }

    log_function_end( "clear" );
    return true;
}


/*----------------------------------------------------------*
 * Sends a trigger to the device.
 *
 * Please note: some devices may not support this operation
 * and it's not always possible to detect this. i.e. the
 * function may return indicating success even though the
 * device wasn't triggered.
 *----------------------------------------------------------*/

bool
VXI11_Client::trigger( double timeout )
{
    if ( ! is_connected( ) )
    {
        log_error( "Attempt to trigger unconnected device" );
        return false;
    }
    
    log_function_start( "trigger" );

    unsigned long to = to_ms( timeout, m_default_write_timeout );

    log_message( "Trying to send trigger within %lu ms", to );

    Device_GenericParms generic_parms = { m_core_link.lid, 0, 0, to };

    set_rpc_timeout( to );

    Device_Error dev_error = { 0 };
    int rpc_res;

    if (    ( rpc_res = device_trigger_1( generic_parms, &dev_error,
                                          m_core_client ) ) != RPC_SUCCESS
         || dev_error.error != 0 )
    {
        if ( ! timed_out( dev_error.error, rpc_res, "send trigger" ) )
            log_error( "Failed to send trigger: %s",
                       sperror( dev_error.error, rpc_res ) );
        log_function_end( "trigger" );
        return false;
    }

    log_function_end( "trigger" );
    return true;
}


/*---------------------------------------------------------*
 * Aborts an in-progress call.
 *
 * Please note: Not all devices support this function and
 * it requires that a async connection exists.
 *---------------------------------------------------------*/

bool
VXI11_Client::abort( )
{
    if ( ! is_async_connected( ) )
    {
        log_error( "Attempt to abort operation without async link" );
        return false;
    }

    log_function_start( "abort" );

    m_timed_out = false;

    Device_Error dev_error = { 0 };
    int rpc_res;
    if (    ( rpc_res = device_abort_1( m_async_link.lid, &dev_error,
                                        m_async_client ) ) != RPC_SUCCESS
         || dev_error.error != 0 )
    {
        log_error( "Failed to abort operation: %s",
                   sperror( dev_error.error, rpc_res ) );
        log_function_end( "abort" );
        return false;
    }

    log_function_end( "abort" );
    return true;
}


/*-------------------------------------------------------*
 * Sends data to the device
 * ->
 *    1. string reference with the data
 *    2. timeout (in seconds), for a zero or negative value 
 *       the default write timeout is used
 *-------------------------------------------------------*/

bool
VXI11_Client::write( std::string const & data,
                     bool                without_eoi,
                     double              timeout )
{
    if ( ! is_connected() )
    {
        log_error( "Attempt to write to unconnected device" );
        return false;
    }

    m_timed_out = false;

    size_t bytes_left = data.size( );

    if ( ! bytes_left )
        return true;

    log_function_start( "write" );

    unsigned long write_timeout = to_ms( timeout, m_default_write_timeout );
    struct timeval before;
    gettimeofday( &before, nullptr );

    log_message( "Expect to write %zu bytes %swithin %lu ms:",
                 bytes_left, without_eoi ? "(without END flag) " : "",
                 write_timeout );
    log_data( data );

    Device_WriteParms write_parms;
    write_parms.lid = m_core_link.lid;
    write_parms.lock_timeout = 0;
    write_parms.flags        = 0;

    /* Do more than a single write if the message is longer than the device
       accepts in one go - set the END flag only with the very last chun
       k unless we're explicitely asked not to set it! */

    size_t bytes_written = 0;
    char * buf = const_cast< char * >( data.c_str( ) );

    while ( bytes_left > 0 )
    {
        update_timeout( write_timeout, before );

        write_parms.io_timeout    = write_timeout;
        write_parms.data.data_val = buf + bytes_written;

        if ( bytes_left > m_core_link.maxRecvSize )
            write_parms.data.data_len = m_core_link.maxRecvSize;
        else
        {
            write_parms.data.data_len = bytes_left;
            if ( ! without_eoi )
                write_parms.flags = Flags::END_SET;
        }

        Device_WriteResp write_resp = { 0, 0 };
        int rpc_res;
        if (    ( rpc_res = device_write_1( write_parms, &write_resp,
                                            m_core_client ) ) != RPC_SUCCESS
             || write_resp.error )
        {
            if ( ! timed_out( write_resp.error, rpc_res, "write" ) )
                log_error( "Failed to write(): %s",
                           sperror( write_resp.error, rpc_res ) );
            log_function_end( "write" );
            return false;
        }

        bytes_written += write_parms.data.data_len;
        bytes_left    -= write_parms.data.data_len;
    }

    log_function_end( "write" );
    return true;
}


/*--------------------------------------------------*
 * Reads data sent by the device. Stops when either as
 * many bytes as requested have been received or the
 * device signals that no more data are available. If
 * the number of requested bytes ('max_len') is 0 reads
 * until the device signals end of data.
 *--------------------------------------------------*/

bool
VXI11_Client::read( std::string   & data,
                    unsigned long   max_len,
                    double          timeout )
{
    if ( ! is_connected( ) )
    {
        log_error( "Attempt to read from unconnected device" );
        return false;
    }

    log_function_start( "read" );

    unsigned int req_size;
    if ( max_len == 0 )
        req_size = std::min( m_core_link.maxRecvSize,
                             std::numeric_limits< unsigned long >::max( ) );
    else if ( max_len < std::numeric_limits< unsigned int >::max( ) )
        req_size = max_len;
    else
        req_size = std::numeric_limits< unsigned int >::max( );

    Device_ReadParms read_parms;
    read_parms.lid          = m_core_link.lid;
    read_parms.flags        = 0;
    read_parms.termChar     = 0;
    read_parms.requestSize  = req_size;
    read_parms.lock_timeout = 0;

    unsigned long read_timeout = to_ms( timeout, m_default_read_timeout );
    struct timeval before;
    gettimeofday( &before, nullptr );

    if ( max_len == 0 )
    {
        max_len = std::numeric_limits< unsigned long >::max( );
        log_message( "Expect to read all available bytes within %lu ms",
                     read_timeout );
    }
    else
        log_message( "Expect to read up to %zu bytes within %lu ms",
                     max_len, read_timeout );

    Device_ReadResp read_resp = { 0, 0, { 0, nullptr } };
    char buffer[ req_size ];
    read_resp.data.data_val = buffer;

    size_t old_data_size = data.size( );
    m_more_available = false;
    size_t bytes_left_to_read = max_len;

    do
    {
        update_timeout( read_timeout, before );
        read_parms.io_timeout   = read_timeout;

        read_parms.requestSize = std::min( bytes_left_to_read,
                                           static_cast< size_t >( req_size ) );
        int rpc_res;

        if (    ( rpc_res = device_read_1( read_parms, &read_resp,
                                           m_core_client ) ) != RPC_SUCCESS
             || read_resp.error != 0 )
        {
            if ( ! timed_out( read_resp.error, rpc_res, "read" ) )
                log_error( "Failed to read(): %s",
                           sperror( read_resp.error, rpc_res ) );
            log_function_end( "read" );
            return false;
        }

        bytes_left_to_read -= read_resp.data.data_len;
        data += std::string( buffer, read_resp.data.data_len );
    } while (    bytes_left_to_read > 0
              && ! ( read_resp.reason & ( Reason::END ) ) );

    m_more_available = ! ( read_resp.reason & Reason::END );

    log_message( "Received %zu bytes%s",
                 data.size( ) - old_data_size,
                 m_more_available ? "" : " with END flag" );
    log_data( data.c_str( ) + old_data_size, data.size( ) - old_data_size );
    log_function_end( "read" );

    return true;
}


/*--------------------------------------------------*
 * Reads data from the device. Stops when either the
 * 'termchar' character was found in the data sent or
 * the device signals that no more data are available.
 *--------------------------------------------------*/

bool
VXI11_Client::read_eos( std::string & data,
                        char          termchar,
                        double        timeout )
{
    if ( ! is_connected( ) )
    {
        log_error( "Attempt to read (with EOS) from unconnected device" );
        return false;
    }

    log_function_start( "read_eos" );

    Device_ReadParms read_parms;
    read_parms.lid         = m_core_link.lid;
    read_parms.requestSize  = std::numeric_limits< unsigned long >::max( );
    read_parms.flags        = Flags::TERMCHR_SET;
    read_parms.termChar     = termchar;
    read_parms.requestSize  = m_core_link.maxRecvSize;
    read_parms.lock_timeout = 0;

    unsigned long read_timeout = to_ms( timeout, m_default_read_timeout );
    struct timeval before;
    gettimeofday( &before, nullptr );

    log_message( "Expect to read until '0x%02u' is received within %lu ms",
                 static_cast< unsigned int >( termchar ), read_timeout );

    Device_ReadResp read_resp = { 0, 0, { 0, nullptr } };
    char buffer[ m_core_link.maxRecvSize ];
    read_resp.data.data_val = buffer;

    size_t old_data_size = data.size( );
    m_more_available = false;

    do
    {
        update_timeout( read_timeout, before );
        read_parms.io_timeout = read_timeout;

        int rpc_res;
        if (    ( rpc_res = device_read_1( read_parms, &read_resp,
                                           m_core_client ) ) != RPC_SUCCESS
             || read_resp.error != 0 )
        {
            if ( ! timed_out( read_resp.error, rpc_res, "read (with EOS)" ) )
                log_error( "Failed to read(): %s",
                           sperror( read_resp.error, rpc_res ) );
            log_function_end( "read_eos" );
            return false;
        }

        data += std::string( buffer, read_resp.data.data_len );
    } while ( ! ( read_resp.reason & ( Reason::END | Reason::CHR ) ) );

    m_more_available = ! ( read_resp.reason & Reason::END );

    std::string f;
    if ( read_resp.reason & Reason::CHR )
        f = " with CHAR";
    if ( ! m_more_available )
        f += f.size( ) ? " and END" : " with END";
    f += f.size( ) ? " flag" : "";
    log_message( "Received %zu bytes%s",
                 data.size( ) - old_data_size, f.c_str( ) );
    log_data( data.c_str( ) + old_data_size, data.size( ) - old_data_size );
    log_function_end( "read_eos" );

    return true;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

char const *
VXI11_Client::sperror( Device_ErrorCode err,
                       int              rpc_res )
{
    if ( err != 0 )
    {
        if ( err >= static_cast< int >( s_err_list.size( ) ) )
            err = s_err_list.size( ) - 1;
        return s_err_list[ err ].c_str( );
    }

    if ( rpc_res >= static_cast< int >( s_rpc_err_list.size( ) ) )
        rpc_res = s_rpc_err_list.size( ) - 1;
    return s_rpc_err_list[ rpc_res ].c_str( );
}


/*--------------------------------------------------*
 * Utility method: reset timed-out flag and converts
 * timeout (given in seconds) into the corresponding
 * timeout in milli-seconds
 *--------------------------------------------------*/

unsigned long
VXI11_Client::to_ms( double        timeout,
                     unsigned long default_val )
{
    m_timed_out = false;
    return timeout < 0 ? default_val : ceil( 1000 * timeout );
}
        

/*--------------------------------------------------*
 *--------------------------------------------------*/

void
VXI11_Client::update_timeout( unsigned long  & timeout,
                              struct timeval & before )
{
    struct timeval now;
    gettimeofday( &now, nullptr );

    unsigned long diff =   ( 1000 * now.tv_sec    + now.tv_usec    / 1000 )
                         - ( 1000 * before.tv_sec + before.tv_usec / 1000 );

    if ( diff >= timeout )
        timeout = 0;
    else
        timeout -= diff;

    before = now;

    set_rpc_timeout( timeout );
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

bool
VXI11_Client::timed_out( Device_ErrorCode    err,
                         int                 rpc_res,
                         std::string const & snippet )
{
    if ( err != s_TIMEOUT_ERROR && rpc_res != s_RPC_TIMEOUT_ERROR )
        return false;

     m_timed_out = true;
     log_error( "Attempt to %s aborted due to timeout", snippet.c_str( ) );
     return true;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

void
VXI11_Client::set_default_timeout( unsigned long & which,
                                   double          value )
{
    if (    value < 0
            || 1000 * value + 1 > std::numeric_limits< unsigned long >::max( ) )
        which = std::numeric_limits< unsigned long >::max( );
    else
        which = ceil( 1000 * value );
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

void
VXI11_Client::set_rpc_timeout( unsigned long timeout_ms )
{
    struct timeval to;
    to.tv_sec  = timeout_ms / 1000;
    to.tv_usec = ( 1000 * timeout_ms ) % 1000000;
    clnt_control( m_core_client, CLSET_TIMEOUT,
                  reinterpret_cast< char * >( &to ) ); 
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
