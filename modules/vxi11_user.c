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
 *  Please note: for devices that support a async channel (as needed for
 *  device_abort()) you must define the macro 'DEVICE_SUPPORTS_ASYNC_CHANNEL'.
 */


#include "vxi11_user.h"

/* Client and link for core channel */

static CLIENT          * core_client   = NULL;
static Create_LinkResp * core_link;

/* Client and link for async channel (needed for device_abort()), optional */

#if defined DEVICE_SUPPORTS_ASYNC_CHANNEL
static CLIENT          * async_client  = NULL;
#endif
static Create_LinkResp * async_link;

static const char      * name          = NULL;
static const char      * ip            = NULL;
static long              read_timeout  = VXI11_DEFAULT_TIMEOUT;
static long              write_timeout = VXI11_DEFAULT_TIMEOUT;

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
								 "undefined error (20)",          /* 22 */
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
 *       or as name that can be resolved via a DNS request)
 *    3. Name of the VXI-11 name of the device
 *    4. Maximum timeout (in micro-seconds) to wait for the
 *       connection to succeeded (0 is interpreted to mean
 *       a nearly infinite timeout)
 *------------------------------------------------------------*/

int
vxi11_open( const char * dev_name,
			const char * address,
			const char * vxi11_name,
			long         us_timeout )
{
	Create_LinkParms link_parms;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( core_client || core_link || name || ip )
    {
        print( FATAL, "Internal error in module, connection already "
               "exists.\n" );
        return FAILURE;
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
        return FAILURE;
	}

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_start( "vxi11_open", name );

	if ( ! ( core_client = clnt_create( ip, DEVICE_CORE,
										DEVICE_CORE_VERSION, "tcp" ) ) )
	{
		if ( fsc2_lan_log_level( ) >= LL_ERR )
			fsc2_lan_log_message( "%s", clnt_spcreateerror( "Error on "
															"vxi11_open()" ) );
		if ( fsc2_lan_log_level( ) >= LL_CE )
			fsc2_lan_log_function_end( "vxi11_open", name );

		T_free( ( char * ) ip );
		T_free( ( char * ) name );

		print( FATAL, "%s",
			   clnt_spcreateerror( "Failed to connect to device" ) );
        return FAILURE;
	}

	/* Set up link parameters - we need to lock the device */

	link_parms.clientId	    = 0;         /* not needed here */
	link_parms.lockDevice	= 1;
	link_parms.lock_timeout	=
                            us_timeout ? lrnd( 0.001 * us_timeout ) : LANG_MAX;
	link_parms.device	    = ( char * ) vxi11_name;

	core_link = create_link_1( &link_parms, core_client );

	if ( core_link->error )
	{
		if ( fsc2_lan_log_level( ) >= LL_ERR )
			fsc2_lan_log_message( "Error on vxi11_open(): %s",
								  vxi11_sperror( core_link->error ) );
		if ( fsc2_lan_log_level( ) >= LL_CE )
			fsc2_lan_log_function_end( "vxi11_open", name );

		T_free( ( char * ) ip );
		T_free( ( char * ) name );
		core_link = NULL;

		print( FATAL, "Failed to connect to device.\n" );

		core_link = NULL;
		clnt_destroy( core_client );
		return FAILURE;
	}

	/* Steve D. Sharples found that some Agilent Infiniium scopes return 0 as
	   the maximum size of data they're prepared to accept. That's obviously
	   in violation of Rule B.6..3, item 3, which requires a size of at least
	   1024. To make sure things works correct this to the minimum required
	   value of 1024. */

	if ( core_link->maxRecvSize == 0 )
		core_link->maxRecvSize = 1024;

#if defined DEVICE_SUPPORTS_ASYNC_CHANNEL
	if ( ( async_client = clnt_create( ip, DEVICE_ASYNC,
									   DEVICE_ASYNC_VERSION, "tcp" ) ) )
	{
		async_link = create_link_1( &link_parms, async_client );

		if ( async->link )
		{
			if ( fsc2_lan_log_level( ) >= LL_ERR )
				fsc2_lan_log_message( "Failed to set up async channel: %s",
									  vxi11_sperror( async_link->error ) );
			clnt_destroy( async_client );
			async_link   = NULL;
			async_client = NULL;
		}
	}
#endif

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_end( "vxi11_open", name );

	return SUCCESS;
}


/*----------------------------------------------------*
 * Function for closing the connection to the device.
 *----------------------------------------------------*/

int
vxi11_close( void )
{
	Device_Error *dev_error;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return FAILURE;
    }

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_start( "vxi11_close", name );

#if defined DEVICE_SUPPORTS_ASYNC_CHANNEL
	if ( async_link )
	{
		dev_error = destroy_link_1( &async_link->lid, async_client );
		if ( dev_error->error && fsc2_lan_log_level( ) >= LL_ERR )
			fsc2_lan_log_message( "Error on vxi11_close() for async channel: "
								  "%s", vxi11_sperror( dev_error->error ) );
		clnt_destroy( async_client );
		async_link   = NULL;
		async_client = NULL;
	}
#endif

	dev_error = destroy_link_1( &core_link->lid, core_client );

	if ( dev_error->error )
	{
		if ( fsc2_lan_log_level( ) >= LL_ERR )
			fsc2_lan_log_message( "Error on vxi11_close(): %s",
								  vxi11_sperror( dev_error->error ) );
		if ( fsc2_lan_log_level( ) >= LL_CE )
			fsc2_lan_log_function_end( "vxi11_close", name );
		print( FATAL, "Failed to close connection to device.\n" );
        return FAILURE;
	}

	core_link   = NULL;

	clnt_destroy( core_client );

	core_client = NULL;
	ip          = T_free( ( char * ) ip );
	name        = T_free( ( char * ) ip );

	return SUCCESS;
}


/*-------------------------------------------------------------*
 * Function for setting the maximum allowed time for a read or
 * write operation.
 * ->
 *    1. Flag that tells if the timeout is for read or write
 *       operations - 0 stands for read, everything else for
 *       write operations.
 *    2. Timeout value in micro-seconds (o is interpreted to
 *       mean a nearly infinite timeout
 *-------------------------------------------------------------*/

void
vxi11_set_timeout( int  dir,
				   long us_timeout )
{
    if ( us_timeout < 0 )
    {
        print( FATAL, "Internal error in module, invalid timeout.\n" );
        return FAILURE;
    }

    if ( dir == READ )
        read_timeout  = us_timeout ? lrnd( 0.001 * us_timeout ) : LONG_MAX;
    else
        write_timeout = us_timeout : lrnd( 0.001 * us_timeout ) : LONG_MAX;
}


/*--------------------------------------------------------*
 * Function for asking the device to send its status byte
 *--------------------------------------------------------*/

int
vxi11_read_stb( unsigned char * stb )
{
	Device_GenericParms generic_parms;
	Device_ReadStbResp *readstb_resp;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return FAILURE;
    }

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_start( "vxi11_read_stb", name );

	if ( fsc2_lan_log_level( ) == LL_ALL )
		fsc2_lan_log_message( "Trying to read STB within %ld ms",
							  read_timeout );

	generic_parms.lid           = core_link->lid;
	generic_parms.flags         = 0;
	generic_parms.lock_timeout  = 0;
	generic_parms.io_timeout	= read_timeout;

	readstb_resp = device_readstb_1( &generic_parms, core_client );

	if ( readstb_resp->error )
	{
		if ( fsc2_lan_log_level( ) == LL_ERR )
			fsc2_lan_log_message( "Error on vxi11_read_stb(): %s",
								  vxi11_sperror( readstb_resp->error ) );
		if ( fsc2_lan_log_level( ) >= LL_CE )
			fsc2_lan_log_function_end( "vxi11_read_stb", name );

		print( FATAL, "Failed to read STB.\n" );
		return FAILURE;
	}

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_end( "vxi11_read_stb", name );

    if ( stb )
        *stb = readstb_resp->stb;

	return SUCCESS;
}


/*----------------------------------------------------------*
 * Function to switch between locked and unlocked state
 * ->
 *    1. boolean value, if set lock out device,
 *       otherwise unlock it
 *
 * Please note: some devices may not support this operation
 * and it's not always possible to detect this. i.e. the
 * function may return indicating SUCCESS even though
 * nothing happend.
 *----------------------------------------------------------*/

int
vxi11_lock_out( bool lock_state )
{
	Device_GenericParms generic_parms;
	Device_Error *dev_error;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return FAILURE;
    }

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_start( "vxi11_lock_out", name );

	if ( fsc2_lan_log_level( ) == LL_ALL )
		fsc2_lan_log_message( "Trying to put device in %s state withing %ld ms",
							  lock_state ? "local" : "remote", write_timeout );

	generic_parms.lid           = core_link->lid;
	generic_parms.flags         = 0;
	generic_parms.lock_timeout  = 0;
	generic_parms.io_timeout	= write_timeout;

	if ( lock_state )
		dev_error = device_remote_1( &generic_parms, core_client );
	else
		dev_error = device_local_1( &generic_parms, core_client );

	if ( dev_error->error )
	{
		if ( fsc2_lan_log_level( ) == LL_ERR )
			fsc2_lan_log_message( "Error on vxi11_lock_out(): %s",
								  vxi11_sperror( dev_error->error ) );
		if ( fsc2_lan_log_level( ) >= LL_CE )
			fsc2_lan_log_function_end( "vxi11_lock_out", name );

		print( FATAL, "Failed to set device into %s state.\n",
			   lock_state ? "local" : "remote" );
   		return FAILURE;
	}

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_end( "vxi11_lock_out", name );

	return SUCCESS;
}


/*---------------------------------------------------*
 * Function for sending a device clear to the device.
 *
 * Please note: some devices may not support this operation
 * and it's not always possible to detect this. i.e. the
 * function may return indicating SUCCESS even though
 * nothing happend.
 *---------------------------------------------------*/

int
vxi11_device_clear( void )
{
	Device_GenericParms generic_parms;
	Device_Error *dev_error;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return FAILURE;
    }

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_start( "vxi11_device_clear", name );

	if ( fsc2_lan_log_level( ) == LL_ALL )
		fsc2_lan_log_message( "Trying to send device clear within %ld ms",
							  write_timeout );

	generic_parms.lid           = core_link->lid;
	generic_parms.flags         = 0;
	generic_parms.lock_timeout  = 0;
	generic_parms.io_timeout	= write_timeout;

	dev_error = device_clear_1( &generic_parms, core_client );

	if ( dev_error->error )
	{
		if ( fsc2_lan_log_level( ) == LL_ERR )
			fsc2_lan_log_message( "Error on vxi11_device_clear(): %s",
								  vxi11_sperror( dev_error->error ) );
		if ( fsc2_lan_log_level( ) >= LL_CE )
			fsc2_lan_log_function_end( "vxi11_device_clear", name );

		print( FATAL, "Failed to send device clear.\n" );
		return FAILURE;
	}

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_end( "vxi11_device_clear", name );

	return SUCCESS;
}


/*-----------------------------------------------*
 * Function for sending a trigger to the device.
 *
 * Please note: some devices may not support this operation
 * and it's not always possible to detect this. i.e. the
 * function may return indicating SUCCESS even though
 * nothing happend.
 *-----------------------------------------------*/

int
vxi11_device_trigger( void )
{
	Device_GenericParms generic_parms;
	Device_Error *dev_error;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
        return FAILURE;
    }

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_start( "vxi11_device_trigger", name );

	if ( fsc2_lan_log_level( ) == LL_ALL )
		fsc2_lan_log_message( "Trying to send device trigger within %ld ms",
							  write_timeout );

	generic_parms.lid           = core_link->lid;
	generic_parms.flags         = 0;
	generic_parms.lock_timeout  = 0;
	generic_parms.io_timeout	= write_timeout;

	dev_error = device_trigger_1( &generic_parms, core_client );

	if ( dev_error->error )
	{
		if ( fsc2_lan_log_level( ) == LL_ERR )
			fsc2_lan_log_message( "Error on vxi11_device_trigger(): %s",
								  vxi11_sperror( dev_error->error ) );
		if ( fsc2_lan_log_level( ) >= LL_CE )
			fsc2_lan_log_function_end( "vxi11_device_trigger", name );

		print( FATAL, "Failed to send device trigger.\n" );

		return FAILURE;
	}

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_end( "vxi11_device_trigger", name );

	return SUCCESS;
}


/*--------------------------------------------------*
 * Function for aborting an in-progress call.
 *
 * Please note: A devices may not support this function. In
 * that case define the macro DEVICE_SUPPORTS_ASYNC_CHANNEL
 * in which case the function does nothing.
 *--------------------------------------------------*/

static int
vxi11_abort( void )
{
#if defined DEVICE_SUPPORTS_ASYNC_CHANNEL
	Device_Error *dev_error;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( ! async_link )
	{
		print( FATAL, "Internal module error, async channel isn't open.\n" );
		return FAILURE:
	}

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_start( "vxi11_abort", name );

	dev_error = device_abort_1( &async_link->lid );

	if ( dev_error->error )
	{
		if ( fsc2_lan_log_level( ) == LL_ERR )
			fsc2_lan_log_message( "Error on vxi11_abort(): %s",
								  vxi11_sperror( dev_error->error ) );
		if ( fsc2_lan_log_level( ) >= LL_CE )
			fsc2_lan_log_function_end( "vxi11_abort", name );

		print( FATAL, "Failed to abort.\n" );

		return FAILURE;
	}

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_end( "vxi11_abort", name );
#endif
	return SUCCESS;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

int
vxi11_write( const char * buffer,
			 size_t     * length,
			 bool         allow_abort )
{
	Device_WriteParms write_parms;
	size_t bytes_left = *length;
    struct timeval before,
		           now;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
		return FAILURE;
    }

    if ( *length == 0 )
        return SUCCESS;

    if ( ! buffer )
    {
        print( FATAL, "Internal error in module, write data buffer is "
			   "NULL.\n" );
		return FAILURE;
    }

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_start( "vxi11_write", name );

	/* Send data to the device taking into account the maximum amount of
	   data the device is prepared to accept */

	if ( fsc2_lan_log_level( ) >= LL_ALL )
	{
		fsc2_lan_log_message( "Expect to write %lu byte(s) within %lu ms to "
							  "LAN device %s:\n", ( unsigned long ) *length,
							  write_timeout, name );
		fsc2_lan_log_data( *length, buffer );
	}

	*length = 0;

	write_parms.lid			  = core_link->lid;
	write_parms.lock_timeout  = 0;
	write_parms.io_timeout	  = write_timeout;

	gettimeofday( &before, NULL );

	/* If necessary do more than a single write if the messages is longer
	   than the device can accept - in that case set the END flag only
	   with the last chunk! */

	do
	{
		Device_WriteResp *write_resp;

		/* Give the user a chance to interrupt the transfer */

		if ( async_link && allow_abort && check_user_request( ) )
		{
			vxi11_abort( );
			THROW( USER_BREAK_EXCEPTION );
		}

		write_parms.data.data_val = ( char * ) ( buffer + *length );

		if ( bytes_left > core_link->maxRecvSize )
		{
			write_parms.data.data_len = core_link->maxRecvSize;
			write_parms.flags		  = 0;
		}
		else
		{
			write_parms.data.data_len = bytes_left;
			write_parms.flags		  = VXI11_END_FLAG;
		}

        gettimeofday( &now, NULL );

		write_parms.io_timeout -= 
			 lrnd( 1e-3 * (   ( 1000000 * now.tv_sec    + now.tv_usec  )
						    - ( 1000000 * before.tv_sec + before.tv_usec ) ) );

		before = now;

		write_resp = device_write_1( &write_parms, core_client );

		if ( write_resp->error )
		{
			if ( fsc2_lan_log_level( ) >= LL_ERR )
				fsc2_lan_log_message( "Error on vxi11_write(): failed to "
									  "write to device, only %lu of %lu got "
									  "transmitted: %s\n",
									  ( unsigned long ) *length,
									  ( unsigned long )
									                  ( *length + bytes_left ),
									  vxi11_sperror( write_resp->error ) );

			if ( fsc2_lan_log_level( ) >= LL_CE )
				fsc2_lan_log_function_end( "vxi11_write", name );
			
			print( FATAL, "Failed to send data to device: %s.\n",
				   vxi11_sperror( write_resp->error ) );

			return FAILURE;
		}

		*length += write_parms.data.data_len;
		bytes_left -= write_parms.data.data_len;

	} while ( bytes_left > 0 );

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_end( "vxi11_write", name );

	return SUCCESS;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

int
vxi11_read( char   * buffer,
			size_t * length,
			bool     allow_abort )
{
	Device_ReadParms read_parms;
	Device_ReadResp *read_resp;
	size_t to_read = *length;
    struct timeval before,
		           now;


    /* Keep the module writers from calling the function anywhere else
       than in the exp- and end_of_exp-hook functions and the EXPERIMENT
       section */

    fsc2_assert(    Fsc2_Internals.state == STATE_RUNNING
                 || Fsc2_Internals.state == STATE_FINISHED
                 || Fsc2_Internals.mode  == EXPERIMENT );

	if ( ! core_link || ! core_client || ! name || ! ip )
    {
        print( FATAL, "Internal error in module, no connection exists.\n" );
		return FAILURE;
    }

    /* Do nothing if no data are to be read */

    if ( *length == 0 )
        return SUCCESS;

    if ( buffer == NULL )
    {
        print( FATAL, "Internal error in module, read data buffer is "
               "NULL.\n" );
        return FAILURE;
    }

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_start( "vxi11_read", name );

	read_parms.lid			= core_link->lid;
	read_parms.flags		= 0;
	read_parms.termChar		= 0;
	read_parms.lock_timeout	= 0;
	read_parms.io_timeout	= read_timeout;

	*length = 0;

	/* Keep reading until either as many bytes has been transmitted as
	   the user asled for or the end of the message has been read */

	gettimeofday( &before, NULL );

	do
	{
		/* Give the user a chance to interrupt the transfer */

		if ( async_link && allow_abort && check_user_request( ) )
		{
			vxi11_abort( );
			THROW( USER_BREAK_EXCEPTION );
		}

        gettimeofday( &now, NULL );

		read_parms.io_timeout -= 
			lrnd( 1e-3 * (   ( 1000000 * now.tv_sec    + now.tv_usec  )
						   - ( 1000000 * before.tv_sec + before.tv_usec ) ) );

		before = now;

		read_parms.requestSize = to_read - *length;

		read_resp = device_read_1( &read_parms, core_client );

		/* In principle the device should never send more data then it
		   was asked for, so the following test is probably only a result
		   of paranoia... */

		if (    ! read_resp->error
			 && *length + read_resp->data.data_len > to_read )
		{
			if ( fsc2_lan_log_level( ) >= LL_ERR )
				fsc2_lan_log_message( "Error on vxi11_read(): device send "
									  "more data than it was asked for (%lu "
									  "instead of %lu).\n",
									  ( unsigned long )
									  ( *length + read_resp->data.data_len ),
									  ( unsigned long ) to_read );

			if ( fsc2_lan_log_level( ) >= LL_CE )
				fsc2_lan_log_function_end( "vxi11_read", name );
			
			print( FATAL, "Failed to read too many data from device.\n" );

			return FAILURE;
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
				fsc2_lan_log_message( "Timeout: on read from device, only "
									  "%lu of %lu bytes got transmitted: %s\n",
									  ( unsigned long ) *length,
									  ( unsigned long ) to_read,
									  vxi11_sperror( read_resp->error ) );

			if ( fsc2_lan_log_level( ) >= LL_CE )
				fsc2_lan_log_function_end( "vxi11_read", name );

			return TIMEOUT;
		}

		if ( read_resp->error && read_resp->error != VXI11_TIMEOUT_ERROR )
		{
			if ( fsc2_lan_log_level( ) >= LL_ERR )
				fsc2_lan_log_message( "Error on vxi11_read(): failed to read "
									  "from device, only %lu of %lu bytes got "
									  "transmitted before timeout: %s\n",
									  ( unsigned long ) *length,
									  ( unsigned long ) to_read,
									  vxi11_sperror( read_resp->error ) );

			if ( fsc2_lan_log_level( ) >= LL_CE )
				fsc2_lan_log_function_end( "vxi11_read", name );
			
			print( FATAL, "Failed to read data from device: %s.\n",
				   vxi11_sperror( read_resp->error ) );

			return FAILURE;
		}
	} while ( *length < to_read && ! ( read_resp->reason & VXI11_END_REASON ) );

	if ( fsc2_lan_log_level( ) >= LL_CE )
		fsc2_lan_log_function_end( "vxi11_read", name );

	return SUCCESS;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static const char *
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
