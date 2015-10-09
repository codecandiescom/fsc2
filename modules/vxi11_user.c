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
 *
 *
 *  The C++ source code of the implementations written by Steve D. Sharples
 *  from Nottingham University (steve.sharples@nottingham.ac.uk), see
 *
 *  http://optics.eee.nottingham.ac.uk/vxi11/
 *
 *  was of great help in writing this implementation and is gratefully
 *  acknowleged.
 *
 *  For more information about the VXI-11 protocol see the "VMEbus Extensions
 *  for Instrumentation, TCP/IP Instroment Protocol Specifications" that can
 *  be downloaded from
 *
 *  http://www.vxibus.org/files/VXI_Specs/VXI-11.zip
 *
 *  Separate device locking (e.g. using a lock file) isn't necessary
 *  since it's part of the VXI11 protocol.
 */


#include "vxi11_user.h"


/* Client and link for core channel */

static CLIENT          * core_client   = NULL;
static Create_LinkResp * core_link     = NULL;


/* Client and link for (optional) asynchronous channel */

static CLIENT          * async_client  = NULL;
static Create_LinkResp * async_link    = NULL;


static const char * name          = NULL;
static const char * ip            = NULL;
static long         read_timeout  = VXI11_DEFAULT_TIMEOUT;
static long         write_timeout = VXI11_DEFAULT_TIMEOUT;

static FILE * log_fp = NULL;

static const char *vxi11_errors[ ] =
        { "no error",                      /*  0 */
          "syntax error",                  /*  1 */
          "undefined error (2)",           /*  2 */
          "device not accessible",         /*  3 */
          "invalid link identifier",       /*  4 */
          "parameter error",               /*  5 */
          "channel not established",       /*  6 */
          "undefined error (7)",           /*  7 */
          "operation not supported",       /*  8 */
          "out of resources",              /*  9 */
          "undefined error (10)",          /* 10 */
          "device locked by another link", /* 11 */
          "no lock held by this link",     /* 12 */
          "undefined error (13)",          /* 13 */
          "undefined error (14)",          /* 14 */
          "I/O timeout",                   /* 15 */
          "undefined error (16)",          /* 16 */
          "I/O error",                     /* 17 */
          "undefined error (18)",          /* 18 */
          "undefined error (19)",          /* 19 */
          "undefined error (20)",          /* 20 */
          "invalid address",               /* 21 */
          "undefined error (22)",          /* 22 */
          "abort",                         /* 23 */
          "undefined error (24)",          /* 24 */
          "undefined error (25)",          /* 25 */
          "undefined error (26)",          /* 26 */
          "undefined error (27)",          /* 27 */
          "undefined error (28)",          /* 28 */
          "channel already established"    /* 29 */
        };

static int vxi11_abort( void );
static const char * vxi11_sperror( Device_ErrorCode error );


/*------------------------------------------------------------*
 * Function for opening a (locked) connection to the device.
 * ->
 *    1. Name of the device to be used in error messages etc.
 *    2. IP address of the device (either in dotted-quad form
 *       or as a name that can be resolved via a DNS look-up)
 *    3. VXI-11 name of the device
 *    4. Flag that tells if a exclusive lock on the device
 *       should be requested.
 *    5. Flag, if set also an async connection is created
 *    6. Maximum timeout (in microseconds) to wait for the
 *       connection if it's locked by another process (0 is
 *       interpreted to mean a nearly infinite timeout)
 *
 * Note: Creating the connection can take quite a bit of time
 * and it only times out after about 25 s.
 *------------------------------------------------------------*/

int
vxi11_open( const char * dev_name,
            const char * address,
            const char * vxi11_name,
            bool         lock_device,
            bool         create_async,
            long         us_timeout )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and in the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( core_client || core_link || name || ip )
    {
        print( FATAL, "Internal error in module, connection already seems to "
               "exist.\n" );
        return VXI11_FAILURE;
    }

    TRY
    {
        name = T_strdup( dev_name );
        ip   = T_strdup( address );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( name )
            name = T_free( ( char * ) name );
        return VXI11_FAILURE;
    }

    /* Open log file after we have the connection open and locked */

    log_fp = fsc2_lan_open_log( name );
    fsc2_lan_log_function_start( log_fp, "vxi11_open" );

    if ( ! ( core_client = clnt_create( ip, DEVICE_CORE,
                                        DEVICE_CORE_VERSION, "tcp" ) ) )
    {
        fsc2_lan_log_message( log_fp, clnt_spcreateerror( "Failed to open a "
                                                          "channel to the "
                                                          "device" ) );
        fsc2_lan_log_function_end( log_fp, "vxi11_open" );
        log_fp = fsc2_lan_close_log( name, log_fp );

        ip   = T_free( ( char * ) ip );
        name = T_free( ( char * ) name );

        print( FATAL, "%s",
               clnt_spcreateerror( "Failed to connect to device" ) );
        return VXI11_FAILURE;
    }

    /* Set up link parameters */

    Create_LinkParms link_parms;

    link_parms.clientId     = 0;         /* not needed here */
    link_parms.lockDevice   = lock_device;
    link_parms.lock_timeout = us_timeout ?
                              lrnd( 0.001 * us_timeout ) : LONG_MAX;
    link_parms.device       = ( char * ) vxi11_name;

    core_link = create_link_1( &link_parms, core_client );

    if ( ! core_link || core_link->error )
    {
        fsc2_lan_log_message( log_fp, "Failed to connect to device: %s\n",
                              core_link
                              ? vxi11_sperror( core_link->error )
                              : "unknown reasons" );
        fsc2_lan_log_function_end( log_fp, "vxi11_open" );
        log_fp = fsc2_lan_close_log( name, log_fp );

        print( FATAL, "Failed to connect to device: %s\n",
               core_link ?
               vxi11_sperror( core_link->error ) : "unknown reasons" );

        if ( core_link )
            destroy_link_1( &core_link->lid, core_client );
        core_link = NULL;

        clnt_destroy( core_client );
        core_client = NULL;

        ip   = T_free( ( char * ) ip );
        name = T_free( ( char * ) name );

        return VXI11_FAILURE;
    }

    /* Steve D. Sharples found that some Agilent Infiniium scopes return 0 as
       the maximum size of data they're prepared to accept. That's obviously
       in violation of Rule B.6.3, item 3, which requires a size of at least
       1024. To make sure things work correctly set this to the minimum required
       value of 1024 (as according to Rule B.6.3). */

    if ( core_link->maxRecvSize == 0 )
        core_link->maxRecvSize = 1024;

    /* If no async connection is to be created we're done */

    if ( ! create_async )
    {
        fsc2_lan_log_function_end( log_fp, "vxi11_open" );
        return SUCCESS;
    }

    if ( ! ( async_client = clnt_create( ip, DEVICE_ASYNC,
                                         DEVICE_ASYNC_VERSION, "tcp" ) ) )
    {
        fsc2_lan_log_message( log_fp, clnt_spcreateerror( "Failed to open "
                                                          "async channel to "
                                                          "device" ) );
        fsc2_lan_log_function_end( log_fp, "vxi11_open" );
        log_fp = fsc2_lan_close_log( name, log_fp );

        destroy_link_1( &core_link->lid, core_client );
        core_link = NULL;

        clnt_destroy( core_client );
        core_client = NULL;

        ip   = T_free( ( char * ) ip );
        name = T_free( ( char * ) name );

        print( FATAL, "%s",
               clnt_spcreateerror( "Failed to open async connection to "
                                   "device" ) );
        return VXI11_FAILURE;
    }
        
    async_link = create_link_1( &link_parms, async_client );

    if ( ! async_link || async_link->error )
    {
        fsc2_lan_log_message( log_fp, "Failed to set up async "
                              "channel: %s",
                                  async_link
                              ? vxi11_sperror( async_link->error )
                              : "unknown reasons" );
        fsc2_lan_log_function_end( log_fp, "vxi11_open" );
        log_fp = fsc2_lan_close_log( name, log_fp );

        if ( async_link )
            destroy_link_1( &async_link->lid, async_client );
        async_link = NULL;

        clnt_destroy( async_client );
        async_client = NULL;

        destroy_link_1( &core_link->lid, core_client );
        core_link = NULL;

        clnt_destroy( core_client );
        core_client = NULL;

        ip          = T_free( ( char * ) ip );
        name        = T_free( ( char * ) name );

        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_end( log_fp, "vxi11_open" );
    return VXI11_SUCCESS;
}


/*----------------------------------------------------*
 * Function for closing the connection to the device.
 *----------------------------------------------------*/

int
vxi11_close( void )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection seems to "
               "exist.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_start( log_fp, "vxi11_close" );

    int ret = VXI11_SUCCESS;

    if ( async_client )
    {
        if ( async_link )
        {
            Device_Error *dev_error = destroy_link_1( &async_link->lid,
                                                      async_client );

            if ( dev_error && dev_error->error )
                fsc2_lan_log_message( log_fp, "Failed to close async link: %s",
                                      vxi11_sperror( dev_error->error ) );
            async_link = NULL;
            ret = VXI11_FAILURE;
        }

        clnt_destroy( async_client );
        async_client = NULL;
    }

    Device_Error *dev_error = destroy_link_1( &core_link->lid, core_client );

    if ( dev_error && dev_error->error )
    {
        fsc2_lan_log_message( log_fp, "Failed to close core link: %s",
                              vxi11_sperror( dev_error->error ) );
        ret = VXI11_FAILURE;
    }

    core_link = NULL;

    clnt_destroy( core_client );
    core_client = NULL;

    fsc2_lan_log_function_end( log_fp, "vxi11_close" );
    log_fp = fsc2_lan_close_log( name, log_fp );

    ip          = T_free( ( char * ) ip );
    name        = T_free( ( char * ) name );

    if ( ret == VXI11_FAILURE )
        print( WARN, "Couldn't properly close connection to device." );
    return ret;
}


/*----------------------------------------------------------------*
 * Function for setting the timeout for read or write operations
 * ->
 *    1. Flag that tells if the timeout is for read or write
 *       operations - 0 means read, 1 for write operations.
 *    2. Timeout value in micro-seconds (0 is interpreted to
 *       mean the longest possible timeout - nearly 25 days
 *       on 32-bit and about 300 Myears on 64-bit systems;-)
 *----------------------------------------------------------------*/

int
vxi11_set_timeout( int  dir,
                   long us_timeout )
{
    if ( us_timeout < 0 )
    {
        print( FATAL, "Internal error in module, invalid timeout.\n" );
        return VXI11_FAILURE;
    }

    if ( dir == VXI11_READ )
        read_timeout  = us_timeout ? lrnd( 0.001 * us_timeout ) : LONG_MAX;
    else if ( dir == VXI11_WRITE )
        write_timeout = us_timeout ? lrnd( 0.001 * us_timeout ) : LONG_MAX;
    else
    {
        print( FATAL, "Internal error in module, invalid direction for "
               "timeout.\n" );
        return VXI11_FAILURE;
    }

    return VXI11_SUCCESS;
}


/*--------------------------------------------------------*
 * Function for asking the device to send its status byte
 * and then reading it
 *--------------------------------------------------------*/

int
vxi11_read_stb( unsigned char * stb )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_start( log_fp, "vxi11_read_stb" );

    if ( fsc2_lan_log_level( ) == LL_ALL )
        fsc2_lan_log_message( log_fp, "Trying to read STB within %ld ms\n",
                              read_timeout );

    Device_GenericParms generic_parms;

    generic_parms.lid           = core_link->lid;
    generic_parms.flags         = 0;
    generic_parms.lock_timeout  = 0;
    generic_parms.io_timeout    = read_timeout;

    Device_ReadStbResp *readstb_resp = device_readstb_1( &generic_parms,
                                                         core_client );

    if ( readstb_resp->error )
    {
        if ( fsc2_lan_log_level( ) == LL_ERR )
            fsc2_lan_log_message( log_fp, "Error in vxi11_read_stb(): %s",
                                  vxi11_sperror( readstb_resp->error ) );
        fsc2_lan_log_function_end( log_fp, "vxi11_read_stb" );

        print( FATAL, "Failed to read STB.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_end( log_fp, "vxi11_read_stb" );

    if ( stb )
        *stb = readstb_resp->stb;

    return VXI11_SUCCESS;
}


/*----------------------------------------------------------*
 * Function to switch between locked and unlocked state
 * ->
 *    1. boolean value, if set lock out device,
 *       otherwise unlock it
 *
 * Please note: some devices may not support this operation
 * and it's not always possible to detect this. i.e. the
 * function may return indicating Success even though the
 * lock state of the device didn't change.
 *----------------------------------------------------------*/

int
vxi11_lock_out( bool lock_state )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_start( log_fp, "vxi11_lock_out" );

    if ( fsc2_lan_log_level( ) == LL_ALL )
        fsc2_lan_log_message( log_fp, "Trying to put device in %s state "
                              "withing %ld ms\n",
                              lock_state ? "remote" : "local",
                              write_timeout );

    Device_GenericParms generic_parms;

    generic_parms.lid          = core_link->lid;
    generic_parms.flags        = 0;
    generic_parms.lock_timeout = 0;
    generic_parms.io_timeout   = write_timeout;

    Device_Error *dev_error;

    if ( lock_state )
        dev_error = device_remote_1( &generic_parms, core_client );
    else
        dev_error = device_local_1( &generic_parms, core_client );

    if ( dev_error->error )
    {
        if ( fsc2_lan_log_level( ) == LL_ERR )
            fsc2_lan_log_message( log_fp,  "Error in vxi11_lock_out(): %s",
                                  vxi11_sperror( dev_error->error ) );
        fsc2_lan_log_function_end( log_fp, "vxi11_lock_out" );

        print( FATAL, "Failed to set device into %s state.\n",
               lock_state ? "local" : "remote" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_end( log_fp, "vxi11_lock_out" );

    return VXI11_SUCCESS;
}


/*----------------------------------------------------------*
 * Function for sending a device clear to the device.
 *
 * Please note: some devices may not support this operation
 * and it's not always possible to detect this. i.e. the
 * function may return indicating success even though
 * the device wasn't cleared.
 *----------------------------------------------------------*/

int
vxi11_device_clear( void )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_start( log_fp, "vxi11_device_clear" );

    if ( fsc2_lan_log_level( ) == LL_ALL )
        fsc2_lan_log_message( log_fp, "Trying to send device clear within "
                              "%ld ms\n", write_timeout );

    Device_GenericParms generic_parms;

    generic_parms.lid          = core_link->lid;
    generic_parms.flags        = 0;
    generic_parms.lock_timeout = 0;
    generic_parms.io_timeout   = write_timeout;

    Device_Error *dev_error = device_clear_1( &generic_parms, core_client );

    if ( dev_error->error )
    {
        if ( fsc2_lan_log_level( ) == LL_ERR )
            fsc2_lan_log_message( log_fp, "Error in vxi11_device_clear(): "
                                  "%s", vxi11_sperror( dev_error->error ) );
        fsc2_lan_log_function_end( log_fp, "vxi11_device_clear" );

        print( FATAL, "Failed to send device clear.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_end( log_fp, "vxi11_device_clear" );
    return VXI11_SUCCESS;
}


/*----------------------------------------------------------*
 * Function for sending a trigger to the device.
 *
 * Please note: some devices may not support this operation
 * and it's not always possible to detect this. i.e. the
 * function may return indicating success even though the
 * device wasn't triggered.
 *----------------------------------------------------------*/

int
vxi11_device_trigger( void )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_start( log_fp, "vxi11_device_trigger" );

    if ( fsc2_lan_log_level( ) == LL_ALL )
        fsc2_lan_log_message( log_fp, "Trying to send device trigger "
                              "within %ld ms\n", write_timeout );

    Device_GenericParms generic_parms;

    generic_parms.lid          = core_link->lid;
    generic_parms.flags        = 0;
    generic_parms.lock_timeout = 0;
    generic_parms.io_timeout   = write_timeout;

    Device_Error *dev_error = device_trigger_1( &generic_parms, core_client );

    if ( dev_error->error )
    {
        if ( fsc2_lan_log_level( ) == LL_ERR )
            fsc2_lan_log_message( log_fp, "Error in vxi11_device_trigger(): "
                                  "%s", vxi11_sperror( dev_error->error ) );
        fsc2_lan_log_function_end( log_fp, "vxi11_device_trigger" );

        print( FATAL, "Failed to send device trigger.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_end( log_fp, "vxi11_device_trigger" );
    return VXI11_SUCCESS;
}


/*---------------------------------------------------------*
 * Function for aborting an in-progress call.
 *
 * Please note: Not all devices support this function. If
 * the device was opened without a async channel calling
 * this function will throw an exception.
 *---------------------------------------------------------*/

static
int
vxi11_abort( void )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return VXI11_FAILURE;
    }

    if ( ! async_client || ! async_link )
    {
        print( FATAL, "Internal module error, async channel isn't open.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_start( log_fp, "vxi11_abort" );

    Device_Error *dev_error = device_abort_1( &async_link->lid, async_client );

    if ( dev_error->error )
    {
        if ( fsc2_lan_log_level( ) == LL_ERR )
            fsc2_lan_log_message( log_fp, "Error in vxi11_abort(): %s",
                                  vxi11_sperror( dev_error->error ) );
        fsc2_lan_log_function_end( log_fp, "vxi11_abort" );

        print( FATAL, "Failed to abort.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_end( log_fp, "vxi11_abort" );
    return VXI11_SUCCESS;
}


/*-------------------------------------------------------*
 * Function for sending data to the device
 * ->
 *    1. pointer to buffer with data
 *    2. pointer with length of bufffer
 *    3. boolean that indicates if a transfer may be
 *       aborted
 *
 * On return the value pointed to by the second argument
 * contains the number of bytes transfered.
 *-------------------------------------------------------*/

int
vxi11_write( const char * buffer,
             size_t     * length,
             bool         allow_abort )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return VXI11_FAILURE;
    }

    if ( *length == 0 )
        return VXI11_SUCCESS;

    if ( ! buffer )
    {
        print( FATAL, "Internal error in module, write data buffer is "
               "NULL.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_start( log_fp, "vxi11_write" );

    /* Send data to the device taking into account the maximum amount of
       data the device is prepared to accept */

    if ( fsc2_lan_log_level( ) >= LL_ALL )
    {
        fsc2_lan_log_message( log_fp, "Expect to write %zu byte(s) within "
                              "%lu ms:\n", *length, write_timeout );
        fsc2_lan_log_data( log_fp, *length, buffer );
    }

    Device_WriteParms write_parms;

    write_parms.lid          = core_link->lid;
    write_parms.lock_timeout = 0;
    write_parms.io_timeout   = write_timeout;

    /* If necessary do more than a single write if the messages is longer
       than the device can accept - in that case set the END flag only
       with the last chunk! */

    size_t bytes_left = *length;
    *length = 0;

    struct timeval before;
    gettimeofday( &before, NULL );

    do
    {

        /* If an async channel is open give the user a chance to interrupt
           the transfer */

        if (    async_client
             && async_link
             && allow_abort
             && check_user_request( ) )
        {
            vxi11_abort( );
            THROW( USER_BREAK_EXCEPTION );
        }

        Device_WriteResp *write_resp;
        write_parms.data.data_val = ( char * ) ( buffer + *length );

        if ( bytes_left > core_link->maxRecvSize )
        {
            write_parms.data.data_len = core_link->maxRecvSize;
            write_parms.flags         = 0;
        }
        else
        {
            write_parms.data.data_len = bytes_left;
            write_parms.flags         = VXI11_END_FLAG;
        }

        struct timeval now;
        gettimeofday( &now, NULL );

        write_parms.io_timeout -= 
             lrnd( 1e-3 * (   ( 1000000 * now.tv_sec    + now.tv_usec  )
                            - ( 1000000 * before.tv_sec + before.tv_usec ) ) );

        before = now;

        write_resp = device_write_1( &write_parms, core_client );

        if ( write_resp->error )
        {
            if ( fsc2_lan_log_level( ) >= LL_ERR )
                fsc2_lan_log_message( log_fp, "Error in vxi11_write(): "
                                      "failed to write to device, only "
                                      "%zu of %zu got transmitted: %s\n",
                                      *length, *length + bytes_left,
                                      vxi11_sperror( write_resp->error ) );

            fsc2_lan_log_function_end( log_fp, "vxi11_write" );
            
            print( FATAL, "Failed to send data to device: %s.\n",
                   vxi11_sperror( write_resp->error ) );
            return VXI11_FAILURE;
        }

        *length += write_parms.data.data_len;
        bytes_left -= write_parms.data.data_len;
    } while ( bytes_left > 0 );

    fsc2_lan_log_function_end( log_fp, "vxi11_write" );
    return VXI11_SUCCESS;
}


/*--------------------------------------------------*
 * Function for reading data send by the device
 * ->
 *    1. pointer to buffer for data
 *    2. pointer to value with maximum number of bytes
 *       to be read
 *    3. boolean that indicates if a transfer mey be
 *       aborted
 *
 * On return the value pointed to by the second argument
 * contains the number of bytes transferred.
 *--------------------------------------------------*/

int
vxi11_read( char   * buffer,
            size_t * length,
            bool     allow_abort )
{
    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

    if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return VXI11_FAILURE;
    }

    /* Do nothing if no data are to be read */

    if ( *length == 0 )
        return VXI11_SUCCESS;

    if ( buffer == NULL )
    {
        print( FATAL, "Internal error in module, buffer for data to be read "
               "is NULL.\n" );
        return VXI11_FAILURE;
    }

    fsc2_lan_log_function_start( log_fp, "vxi11_read" );

    Device_ReadParms read_parms;

    read_parms.lid          = core_link->lid;
    read_parms.flags        = 0;
    read_parms.termChar     = 0;
    read_parms.lock_timeout = 0;
    read_parms.io_timeout   = read_timeout;

    if ( fsc2_lan_log_level( ) >= LL_ALL )
        fsc2_lan_log_message( log_fp, "Expect to read up to %zu byte(s) within "
                              "%lu ms\n", *length, read_timeout );

    /* Keep reading until either as many bytes have been transmitted as
       the user asked for or the end of the message has been reached */

    size_t to_read = *length;
    size_t expected = *length;
    *length = 0;

    struct timeval before;
    gettimeofday( &before, NULL );

    Device_ReadResp * read_resp;

    do
    {
        /* If an async channel is open give the user a chance to interrupt
           the transfer */

        if (    async_client
             && async_link
             && allow_abort
             && check_user_request( ) )
        {
            vxi11_abort( );
            THROW( USER_BREAK_EXCEPTION );
        }

        struct timeval now;
        gettimeofday( &now, NULL );

        read_parms.io_timeout -= 
            lrnd( 1e-3 * (   ( 1000000 * now.tv_sec    + now.tv_usec  )
                           - ( 1000000 * before.tv_sec + before.tv_usec ) ) );
        before = now;

        read_parms.requestSize = to_read - *length;

        read_resp = device_read_1( &read_parms, core_client );

        /* In principle the device should never send more data than it was
           asked for, so the following test is probably only a result of
           paranoia... */

        if (    ! read_resp->error
             && *length + read_resp->data.data_len > to_read )
        {
            if ( fsc2_lan_log_level( ) >= LL_ERR )
                fsc2_lan_log_message( log_fp, "Error in vxi11_read(): "
                                      "device sent more data than it was "
                                      "asked for (%zu instead of %zu).\n",
                                      ( *length + read_resp->data.data_len ),
                                      to_read );

            fsc2_lan_log_function_end( log_fp, "vxi11_read" );
            
            print( FATAL, "Failed to read, too many data from device.\n" );
            return VXI11_FAILURE;
        }

        if ( ! read_resp->error || read_resp->error == VXI11_TIMEOUT_ERROR )
        {
            memcpy( buffer + *length, read_resp->data.data_val,
                    read_resp->data.data_len );
            *length += read_resp->data.data_len;
        }

        if ( read_resp->error && read_resp->error == VXI11_TIMEOUT_ERROR )
        {
            if ( fsc2_lan_log_level( ) >= LL_ERR )
                fsc2_lan_log_message( log_fp, "Timeout in vxi11_read(): only "
                                      "%zu of %zu bytes got transmitted: %s\n",
                                      *length, to_read,
                                      vxi11_sperror( read_resp->error ) );

            fsc2_lan_log_function_end( log_fp, "vxi11_read" );
            return VXI11_TIMEOUT;
        }

        if ( read_resp->error && read_resp->error != VXI11_TIMEOUT_ERROR )
        {
            if ( fsc2_lan_log_level( ) >= LL_ERR )
                fsc2_lan_log_message( log_fp, "Error in vxi11_read(): only %zu "
                                      "of %zu bytes got transmitted before "
                                      "timeout: %s\n", *length, to_read,
                                      vxi11_sperror( read_resp->error ) );

            fsc2_lan_log_function_end( log_fp, "vxi11_read" );
            
            print( FATAL, "Failed to read data from device: %s.\n",
                   vxi11_sperror( read_resp->error ) );
            return VXI11_FAILURE;
        }
    } while ( *length < to_read && ! ( read_resp->reason & VXI11_END_REASON ) );

    /* Transfer should be over because either all data requested were read
       (and the device also sees it that way) or because the device didn't
       hasany data to send anymore */

    fsc2_assert(    (    to_read - *length == 0
                      && read_resp->reason & VXI11_REQCNT_REASON )
                 || read_resp->reason & VXI11_END_REASON );

    if ( fsc2_lan_log_level( ) >= LL_CE )
    {
        fprintf( log_fp, "-> Received %ld of up to %ld bytes\n",
                 ( long ) *length, ( long ) expected );

        if ( fsc2_lan_log_level( ) == LL_ALL )
        {
            fwrite( buffer, *length, 1, log_fp );
            fputc( ( int ) '\n', log_fp );
        }
    }

    fsc2_lan_log_function_end( log_fp, "vxi11_read" );

    return VXI11_SUCCESS;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static
const char *
vxi11_sperror( Device_ErrorCode error )
{
    static char e[ 100 ];

    if (    error >= 0
         && error < ( int ) ( sizeof vxi11_errors / sizeof *vxi11_errors ) )
        return vxi11_errors[ error ];

    sprintf( e, "undefined error (%ld)", ( long ) error );

    return e;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
