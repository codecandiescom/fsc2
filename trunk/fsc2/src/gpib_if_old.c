/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


#define __GPIB__


#include "fsc2.h"
#include "gpib_if.h"
#include <sys/timeb.h>


/*----------------------------------------------------------------*/
/* GPIB_LOG_FILE is the name of the default file to which logging */
/* information about GPIB operations are written - the name can   */
/* changed by passing a different name to gpib_init().            */
/*----------------------------------------------------------------*/


#define CONTROLLER  "gpib0"      /* symbolic name of the controller */


#define SRQ      0x40    /* SRQ bit in device status register */
#define TIMEOUT  T10s    /* GPIB timeout period set at initialization */


#define ON  1
#define OFF 0


#define TEST_BUS_STATE                                               \
        if ( ! gpib_is_active )                                      \
        {                                                            \
            strcpy( gpib_error_msg, "GPIB bus not initialized.\n" ); \
            return FAILURE;                                          \
        }


/*------------------------------------------------------------------------*/
/* functions used only locally                                            */
/*------------------------------------------------------------------------*/


static int   gpib_init_controller( void );
static void  gpib_init_log( char *log_file_name );
static void  gpib_read_end( const char *dev_name, char *buffer, long received,
							long expected );
static void  gpib_log_date( void );
static void  gpib_log_error( const char *type );
static void  gpib_write_start( const char *dev_name, const char *buffer,
							   long length );
static void  gpib_log_function_start( const char *function,
									  const char *dev_name );
static void  gpib_log_function_end( const char *function,
									char const *dev_name );
static char *gpib_get_dev_name( int device );


/*------------------------------------------------------------------------*/
/* global variables                                                       */
/*------------------------------------------------------------------------*/


int ll;                         /* log level                              */
int gpib_is_active = 0;         /* flag, set after initialization of bus  */
int controller;                 /* device number assigned to controller   */
int timeout;                    /* stores actual timeout period           */
FILE *gpib_log;                 /* file pointer of GPIB log file          */
GPIB_DEV *gpib_dev_list = NULL; /* list of symbolic names of devices etc. */



/*-------------------------------------------------------------------------*/
/* gpib_init() initializes the GPIB bus by starting the logging mechanism  */
/* and determining the device descriptor of the controller board.          */
/* *** This function has to be called before any other bus activity ! ***  */
/* Calling this function a second time without a prior call of             */
/* gpib_shutdown() will do nothing but still return successfully (the name */
/* of the log file remains unchanged!).                                    */
/* In order to make opening the log file with write access work for all    */
/* users it should be created previously and be given the proper access    */
/* permissions, i.e. r and w for everyone .                                */
/* ->                                                                      */
/*  * Pointer to the name of log file - if the pointer is NULL or does not */
/*    point to a non-empty string the default log file name defined by     */
/*    GPIB_LOG_FILE will be used.                                          */
/*  * log level, either LL_NONE, LL_ERR, LL_CE or LL_ALL                   */
/*    (if log level is LL_NONE 'log_file_name' is not used at all)         */
/* <-                                                                      */
/*  * SUCCESS: bus is initialized                                          */
/*  * FAILURE: error, GPIB bus can't be used                               */
/*-------------------------------------------------------------------------*/

int gpib_init( char *log_file_name, int log_level )
{
	GPIB_DEV *cur_dev;


     if ( gpib_is_active )
         return SUCCESS;

     ll = log_level;
     if ( ll < LL_NONE )
         ll = LL_NONE;
     if ( ll > LL_ALL )
         ll = LL_ALL;

    gpib_init_log( log_file_name );             /* initialize logging */

    if ( gpib_init_controller( ) != SUCCESS )   /* initialize the controller */
    {
        strcpy( gpib_error_msg, "Initialization of controller failed!" );

		/* Get rid of the device list if it's already created */

		if ( gpib_dev_list != NULL )
		{
			for ( cur_dev = gpib_dev_list; cur_dev->next != NULL;
				  cur_dev = cur_dev->next )
				;

			while ( 1 )
			{
				T_free( cur_dev->name );
				if ( cur_dev->prev != NULL )
				{
					cur_dev = cur_dev->prev;
					T_free( cur_dev->next );
				}
				else
					break;
			}
		}

        return FAILURE;
    }

    gpib_is_active = 1;
    gpib_timeout( controller, TIMEOUT );
    return SUCCESS;
}


/*-------------------------------------------------------------*/
/* gpib_init_controller() initializes the controller by first  */
/* getting the device/board descriptor, testing, if this is    */
/* really the controller and finally "switching on" the board, */
/* clearing the interface and asserting the REN line.          */
/* <-                                                          */
/*  * SUCCESS: OK, FAILURE: error                              */
/*-------------------------------------------------------------*/

static int gpib_init_controller( void )
{
    if ( gpib_init_device( CONTROLLER, &controller ) != SUCCESS )
        return FAILURE;

    if( ! ibIsMaster( controller ) )
        return FAILURE;

    if  ( ( ibonl( controller, ON ) | ibsic( controller ) |
            ibsre( controller, ON ) ) & IBERR )
          return FAILURE;

    return SUCCESS;
}


/*--------------------------------------------------------*/
/* gpib_shutdown() shuts down the GPIB bus by setting all */
/* devices into the local state and closing the log file. */
/* <-                                                     */
/*    SUCCESS: bus is shut down,                          */
/*    FAILURE: bus is already inactive                     */
/*--------------------------------------------------------*/

int gpib_shutdown( void )
{
	GPIB_DEV *cur_dev;


    TEST_BUS_STATE;

	for ( cur_dev = gpib_dev_list; cur_dev->next != NULL;
		  cur_dev = cur_dev->next )
		;

	while ( 1 )
	{
		T_free( cur_dev->name );
		if ( cur_dev->prev != NULL )
		{
			cur_dev = cur_dev->prev;
			T_free( cur_dev->next );
		}
		else
			break;
	}

	T_free( cur_dev );
	gpib_dev_list = NULL;

    ibsre( controller, OFF );       /* pull down the REN line */
    ibonl( controller, OFF );       /* "switch off" the controller */

    if ( ll > LL_NONE )
    {
        gpib_log_date( );
		seteuid( EUID );
        fprintf( gpib_log, "GPIB bus is being shut down.\n\n" );
        if ( gpib_log != stderr )
            fclose( gpib_log );                 /* close log file */
		seteuid( getuid( ) );
    }

    gpib_is_active = 0;

    return SUCCESS;
}


/*-------------------------------------------------------------------------*/
/* gpib_init_log() initializes the logging mechanism. If the logging level */
/* is not LL_NONE, a log file will be opened. The name to be used for the  */
/* log file can be passed to the function, see below. If the file cannot   */
/* be opened 'sterr' is used instead. If the log file did not exist its    */
/* permissions are set to allow read and write access for everyone.        */
/* ->                                                                      */
/*  * Pointer to the name of log file - if the pointer is NULL or does not */
/*    point to a non-empty string the default log file name defined by     */
/*    GPIB_LOG_FILE will be used.                                          */
/*-------------------------------------------------------------------------*/

static void gpib_init_log( char *log_file_name )
{
	const char *name;
	bool access_ok = UNSET;
	bool set_perms = UNSET;


    if ( ll == LL_NONE )
        return;

    if ( log_file_name == NULL || *log_file_name == '\0' )
	{
		gpib_log = stderr;
		fprintf( stderr, "GPIB log file not specified, using stderr "
				 "instead\n" );
	}
	else
	{
        name = log_file_name;

		seteuid( EUID );

		access_ok = access( name, W_OK ) == 0 ? SET : UNSET;

		if ( ! access_ok && errno == ENOENT )    /* file doesn't exist yet ? */
			set_perms = SET;

		if ( ( gpib_log = fopen( name, "w" ) ) == NULL )     /* open fails ? */
		{
			gpib_log = stderr;
			fprintf( stderr, "Can't open log file %s, using stderr instead.\n",
					 name );
		}
		else if ( set_perms )  /* if file is new set its permissions to 0666 */
			chmod( name,
				   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

		seteuid( getuid( ) );
	}

    gpib_log_date( );

	seteuid( EUID );
    fprintf( gpib_log, "GPIB bus is being initialized.\n" );
    fflush( gpib_log );
	seteuid( getuid( ) );
}


/*-----------------------------------------------------------------------*/
/* gpib_init_device() initializes a device.                              */
/* Function has to be called before any other operations on the device ! */
/* ->                                                                    */
/*  * Symbolic name of the device (string, has to be identical to the    */
/*    corresponding entry in GPIB_CONF_FILE)                             */
/*  * pointer for returning assigned device number                       */
/* <-                                                                    */
/*  * SUCCESS: OK, assigned device number is returned in 'dev'           */
/*  * FAILURE: error, no valid data is returned in 'dev'                 */
/*-----------------------------------------------------------------------*/

int gpib_init_device( const char *device_name, int *dev )
{
	GPIB_DEV *cur_dev;


    if ( ! gpib_is_active && strcmp( device_name, CONTROLLER ) )
    {
        strcpy( gpib_error_msg, "GPIB bus not initialized yet.\n" );
        return FAILURE;
    }

	/* Append a new entry to the device list */

	if ( gpib_dev_list == NULL )
	{
		gpib_dev_list = cur_dev = T_malloc( sizeof( GPIB_DEV ) );
		cur_dev->prev = NULL;
	}
	else
	{
		for ( cur_dev = gpib_dev_list; cur_dev->next != NULL;
			  cur_dev = cur_dev->next )
			;
		cur_dev->next = T_malloc( sizeof( GPIB_DEV ) );
		cur_dev->next->prev = cur_dev;
		cur_dev = cur_dev->next;
	}

	cur_dev->name = T_strdup( device_name );
	cur_dev->next = NULL;

    if ( ll > LL_ERR )
        gpib_log_function_start( "gpib_init_device", device_name );

    cur_dev->number = ibfind( ( char * ) device_name );

    if ( ibsta & IBERR )
	{
		T_free( cur_dev->name );
		if ( cur_dev->prev != NULL )
			cur_dev->prev->next = NULL;
		else
			gpib_dev_list = NULL;
		T_free( cur_dev );
        sprintf( gpib_error_msg, "Can't initialize device %s, ibsta = 0x%x",
				 device_name, ibsta );
		return FAILURE;
	}

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_init_device", device_name );

	*dev = cur_dev->number;
	return SUCCESS;
}


int gpib_local( int device )
{
	device = device;
	return SUCCESS;
}

/*-----------------------------------------------------------*/
/* gpib_timeout() sets the period the controller is supposed */
/* to wait for a reaction from a device before timing out.   */
/* ->                                                        */
/*  * timeout period (cmp. definitions of TNONE to T1000s)   */
/* <-                                                        */
/*  * SUCCESS: OK, FAILURE: error                            */
/*-----------------------------------------------------------*/

int gpib_timeout( int device, int period )
{
	char *dev_name;
    const char tc[ ][ 9 ] = { "infinity", "10us", "30us", "100us",
							  "300us", "1ms", "3ms", "10ms", "30ms",
							  "100ms", "300ms", "1s", "3s", "10s",
							  "30s", "100s", "300s", "1000s" };


    TEST_BUS_STATE;              /* bus not initialized yet ? */

	if ( ( dev_name = gpib_get_dev_name( device ) ) == NULL )
	{
		sprintf( gpib_error_msg, "CALL of gpib_timeout for unknown device "
				 "(device number %d)\n", device );
		return FAILURE;
	}

    if ( period < TNONE )        /* check validity of parameter */
        period = TNONE;
    if ( period > T1000s )
        period = T1000s;

    if ( ll > LL_ERR )
    {
        gpib_log_date( );
		seteuid( EUID );
        fprintf( gpib_log, "CALL of gpib_timeout for device %s, ", dev_name );
        fprintf( gpib_log, "-> timeout is set to %s\n", tc[ period ] );
        fflush( gpib_log );
		seteuid( getuid( ) );
    }

    ibtmo( device, period );

    if ( ibsta & IBERR )
    {
        sprintf( gpib_error_msg, "Can't set timeout period for device %s, "
				 "ibsta = 0x%x.", dev_name, ibsta );
        return FAILURE;
    }

    if ( ll > LL_ERR )
    {
        gpib_log_date( );
		seteuid( EUID );
        fprintf( gpib_log, "EXIT of gpib_timeout\n" );
        fflush( gpib_log );
		seteuid( getuid( ) );
    }

    timeout = period;          /* store actual value of timeout period */

    return SUCCESS;
}


/*------------------------------------------------*/
/* gpib_clear() clears a device by sending it the */
/* Selected Device Clear (SDC) message.           */
/* ->                                             */
/*  * number of device to be cleared              */
/* <-                                             */
/*  * SUCCESS: OK, FAILURE: error                 */
/*------------------------------------------------*/

int gpib_clear_device( int device )
{
	char *dev_name;


    TEST_BUS_STATE;              /* bus not initialized yet ? */


	if ( ( dev_name = gpib_get_dev_name( device ) ) == NULL )
	{
		sprintf( gpib_error_msg, "CALL of gpib_clear_device for unknown "
				 "device (device number %d)\n", device );
		return FAILURE;
	}

    if ( ll > LL_ERR )
        gpib_log_function_start( "gpib_clear_device", dev_name );

    ibclr( device );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_clear_device", dev_name );

    if ( ibsta & IBERR )
    {
        sprintf( gpib_error_msg, "Can't clear device %s, ibsta = 0x%x",
				 dev_name, ibsta );
        return FAILURE;
    }

    return SUCCESS;
}


/*------------------------------------------------*/
/* gpib_trigger() triggers a device by sending it */
/* a Device Trigger Command.                      */
/* ->                                             */
/*  * number of device to be triggered            */
/* <-                                             */
/*  * SUCCESS: OK, FAILURE: error                 */
/*------------------------------------------------*/

int gpib_trigger( int device )
{
	char *dev_name;


    TEST_BUS_STATE;              /* bus not initialized yet ? */

	if ( ( dev_name = gpib_get_dev_name( device ) ) == NULL )
	{
		sprintf( gpib_error_msg, "CALL of gpib_trigger for unknown device "
				 "(device number %d)\n", device );
		return FAILURE;
	}

    if ( ll > LL_ERR )
        gpib_log_function_start( "gpib_trigger", dev_name );

    ibtrg( device );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_trigger", dev_name );

    if ( ibsta & IBERR )
    {
        sprintf( gpib_error_msg, "Can't trigger device %s, ibsta = 0x%x",
				 dev_name, ibsta );
        return FAILURE;
    }

    return SUCCESS;
}


/*-----------------------------------------------------------------*/
/* gpib_wait() monitors the events specified by 'mask' from a      */
/* device and delays processing until at least one of the events   */
/* occur. If 'mask' is zero, gpib_wait() returns immediately.      */
/* Valid events to be waited for are TIMO, END, RQS and CMPL.      */
/*                                                                 */
/* Since the function ibwait() returns after the timeout period    */
/* even when the TIMO bit is not set in 'mask' and flags an EBUS   */
/* error (14) the timeout period is set to TNONE before ibwait()   */
/* is called and reset to the old value on return.                 */
/*                                                                 */
/* ->                                                              */
/*  * number of device that should produce the event               */
/*  * mask specifying the events (TIMO, END, RQS and CMPL)         */
/*  * pointer to return state (or NULL)                            */
/* <-                                                              */
/*  * SUCCESS: OK, FAILURE: error                                  */
/*-----------------------------------------------------------------*/

int gpib_wait( int device, int mask, int *status )
{
    int old_timeout = timeout;
	char *dev_name;


    TEST_BUS_STATE;              /* bus not initialized yet ? */

	if ( ( dev_name = gpib_get_dev_name( device ) ) == NULL )
	{
		sprintf( gpib_error_msg, "CALL of gpib_wait for unknown device "
				 "(device number %d)\n", device );
		return FAILURE;
	}

    if ( ll > LL_ERR )
    {
        gpib_log_function_start( "gpib_wait", dev_name );
		seteuid( EUID );
        fprintf( gpib_log, "wait mask = 0x0%X\n", mask );
        if ( mask & ~( TIMO | END | RQS | CMPL ) )
            fprintf( gpib_log, "=> Setting mask to 0x%X <=\n",
                     mask & ( TIMO | END | RQS | CMPL ) );
        fflush( gpib_log );
		seteuid( getuid( ) );
    }

    mask &= TIMO | END | RQS | CMPL;    /* remove invalid bits */

    if ( ! ( mask & TIMO ) && timeout != TNONE )
        gpib_timeout( device, TNONE );

    ibwait( device, mask );

    if ( status != NULL )
        *status = ibsta;

	if ( ll > LL_ERR )
	{
		seteuid( EUID );
        fprintf( gpib_log, "wait return status = 0x0%X\n", ibsta );
		seteuid( getuid( ) );
	}

    if ( ! ( mask & TIMO ) && old_timeout != TNONE )
        gpib_timeout( device, old_timeout );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_wait", dev_name );

    if ( ibsta & IBERR )
    {
        sprintf( gpib_error_msg, "Can't wait for device %s, ibsta = 0x%x",
				 dev_name, ibsta );
        return FAILURE;
    }

    return SUCCESS;
}


/*---------------------------------------------------*/
/* gpib_write() sends a number of bytes to a device. */
/* ->                                                */
/*  * number of device the data are to be sent to    */
/*  * buffer containing the bytes to be sent         */
/*  * number of bytes to be sent                     */
/* <-                                                */
/*  * SUCCESS: OK, FAILURE: write error              */
/*---------------------------------------------------*/

int gpib_write( int device, const char *buffer, long length )
{
	char *dev_name;
	char *buf;


    TEST_BUS_STATE;              /* bus not initialized yet ? */

	if ( ( dev_name = gpib_get_dev_name( device ) ) == NULL )
	{
		sprintf( gpib_error_msg, "CALL of gpib_write for unknown device "
				 "(device number %d)\n", device );
		return FAILURE;
	}

    if ( length <= 0 )           /* check validity of length parameter */
    {
        if ( ll != LL_NONE )
        {
            gpib_log_date( );
			seteuid( EUID );
            fprintf( gpib_log, "ERROR in in call of gpib_write: "
                               "Invalid parameter: %ld bytes.\n", length );
            fflush( gpib_log );
			seteuid( getuid( ) );
        }
        sprintf( gpib_error_msg, "Can't write %ld bytes.", length );
        return FAILURE;
    }

	buf = T_malloc( length );
	buf[ --length ] = '\0';
	strncpy( buf, buffer, length );

    if ( ll > LL_ERR )
        gpib_write_start( dev_name, buf, length );

	ibwrt( device, buf, length );

	T_free( buf );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_write", dev_name );

    if ( ibsta & IBERR )
    {
        sprintf( gpib_error_msg, "Can't send data to device %s, ibsta = 0x%x",
				 dev_name, ibsta );
        return FAILURE;
    }

    return SUCCESS;
}


/*----------------------------------------------------------------*/
/* gpib_write_start() logs the call to the function gpib_write(). */
/* ->                                                             */
/*  * name of device the data are to be sent to                   */
/*  * buffer containing the bytes to be sent                      */
/*  * number of bytes to be sent                                  */
/*----------------------------------------------------------------*/

static void gpib_write_start( const char *dev_name, const char *buffer,
							  long length )
{
    long i;


    gpib_log_function_start( "gpib_write", dev_name );
	seteuid( EUID );
    fprintf( gpib_log, "-> There are %ld bytes to be sent\n", length );

    if ( ll == LL_ALL )
    {
		fwrite( buffer, sizeof( char ), length, gpib_log );
        fputc( ( int) '\n', gpib_log );
    }
    fflush( gpib_log );
	seteuid( getuid( ) );
}


/*----------------------------------------------------------*/
/* gpib_read() reads a number of bytes from a device.       */
/* ->                                                       */
/*  * number of device the data are to be received from     */
/*  * buffer for storing the data                           */
/*  * pointer to maximum number of bytes to be read         */
/* <-                                                       */
/*  * SUCCESS: OK, data are stored in 'buffer' and 'length' */
/*             is set to the number of bytes received       */
/*  * FAILURE: read error                                   */
/*----------------------------------------------------------*/

int gpib_read( int device, char *buffer, long *length )
{
    long expected = *length;
	char *dev_name;


    TEST_BUS_STATE;              /* bus not initialized yet ? */

	if ( ( dev_name = gpib_get_dev_name( device ) ) == NULL )
	{
		sprintf( gpib_error_msg, "CALL of gpib_read for unknown device "
				 "(device number %d)\n", device );
		return FAILURE;
	}

    if ( *length <= 0 )          /* check validity of length parameter */
    {
        if ( ll != LL_NONE )
        {
            gpib_log_date( );
			seteuid( EUID );
            fprintf( gpib_log, "ERROR in call of gpib_read: "
                               "Invalid parameter: %ld bytes\n", *length );
            fflush( gpib_log );
			seteuid( getuid( ) );
        }
        sprintf( gpib_error_msg, "Can't read %ld bytes.", *length );
        return FAILURE;
    }

    if ( ll > LL_ERR )
    {
        gpib_log_function_start( "gpib_read", dev_name );
		seteuid( EUID );
        fprintf( gpib_log, "-> Expecting up to %ld bytes\n", *length );
        fflush( gpib_log );
		seteuid( getuid( ) );
    }

    ibrd( device, buffer, expected );
    *length = ibcnt;

    if ( ll > LL_NONE )
        gpib_read_end( dev_name, buffer, *length, expected );

    if ( ibsta & IBERR )
    {
        sprintf( gpib_error_msg, "Can't read data from device %s, ibsta = "
				 "0x%x.", dev_name, ibsta );
        return FAILURE;
    }

    return SUCCESS;
}


/*------------------------------------------------------------------*/
/* gpib_read_end() logs the completion of the function gpib_read(). */
/* ->                                                               */
/*  * name of device the data were received from                    */
/*  * buffer for storing the data                                   */
/*  * maximum number of data to read                                */
/*  * number of bytes actually read                                 */
/*------------------------------------------------------------------*/

static void gpib_read_end( const char *dev_name, char *buffer, long received,
						   long expected )
{
    long i;


    if ( ll > LL_ERR || ( ibsta & IBERR ) )
        gpib_log_function_end( "gpib_read", dev_name );

    if ( ll < LL_CE )
        return;

	seteuid( EUID );
    fprintf( gpib_log, "-> Received %ld of up to %ld bytes\n",
             received, expected );

    if ( ll == LL_ALL )
    {
		fwrite( buffer, sizeof( char ), received, gpib_log );
        fputc( ( int ) '\n', gpib_log );
    }

    fflush( gpib_log );
	seteuid( getuid( ) );
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

int gpib_serial_poll( int device, unsigned char *stb )
{
	char *dev_name;


    TEST_BUS_STATE;              /* bus not initialised yet ? */

	if ( ( dev_name = gpib_get_dev_name( device ) ) == NULL )
	{
		sprintf( gpib_error_msg, "CALL of gpib_serial_poll for unknown device "
				 "(device number %d)\n", device );
		return FAILURE;
	}

    if ( ll > LL_ERR )
        gpib_log_function_start( "gpib_serial_poll", dev_name );

    ibrsp( device, stb );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_serial_poll", dev_name );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't serial poll device %s, "
				 "gpib_status = 0x%x", dev_name, ibsta );
        return FAILURE;
    }

    if ( ll >= LL_CE )
	{
		seteuid( EUID );
		fprintf( gpib_log, "-> Received status byte = 0x%x\n", *stb );
		fflush( gpib_log );
		seteuid( getuid( ) );
	}

    return SUCCESS;
}


/*----------------------------------------------------------------*/
/* Prints the date and a user supplied message into the log file. */
/* The user can call this function in exactely the same way as    */
/* the standard printf() function.                                */
/*----------------------------------------------------------------*/


void gpib_log_message( const char *fmt, ... )
{
	va_list ap;


	gpib_log_date( );
	seteuid( EUID );
	va_start( ap, fmt );
	vfprintf( gpib_log, fmt, ap );
	va_end( ap );
	seteuid( getuid( ) );
}


/*--------------------------------------------------*/
/* gpib_log_date() writes the date to the log file. */
/*--------------------------------------------------*/

static void gpib_log_date( void )
{
    static char tc[ 26 ];
	struct timeb mt;
    time_t t;


    t = time( NULL );
    strcpy( tc, asctime( localtime( &t ) ) );
	tc[ 10 ] = '\0';
	tc[ 19 ] = '\0';
    tc[ 24 ] = '\0';
	ftime( &mt );
	seteuid( EUID );
    fprintf( gpib_log, "[%s %s %s.%03d] ", tc, tc + 20, tc + 11, mt.millitm );
	seteuid( getuid( ) );
}


/*--------------------------------------------------------------*/
/* gpib_log_error() writes an error message to the log file.    */
/* ->                                                           */
/*  * string with short description of the type of the function */
/*    the error happened in                                     */
/*--------------------------------------------------------------*/

static void gpib_log_error( const char *type )
{
    static int stat[ 16 ] = { 0x8000, 0x4000, 0x2000, 0x1000,
                              0x0800, 0x0400, 0x0200, 0x0100,
                              0x0080, 0x0040, 0x0020, 0x0010,
                              0x0008, 0x0004, 0x0002, 0x0001 };
    static char is[ 16 ][ 6 ] = {  "IBERR", "TIMO", "END",  "SRQI",
                                   "RQS",   "\b",   "\b",   "CMPL",
                                   "LOK",   "REM",  "CIC",  "ATN",
                                   "TACS",  "LACS", "DTAS", "DCAS" };
    static char ie[ 24 ][ 5 ] = { "EDVR", "ECIC", "ENOL", "EADR",
                                  "EARG", "ESAC", "EABO", "ENEB",
                                  "EDMA", "EBTO", "EOIP", "ECAP",
                                  "EFSO", "EOWN", "EBUS", "ESTB",
                                  "ESRQ", "ECFG", "EPAR", "ETAB",
                                  "ENSD", "ENWE", "ENTF", "EMEM" };
    int i;


    gpib_log_date( );
	seteuid( EUID );
    fprintf( gpib_log, "ERROR in function %s: <", type );
    for ( i = 15; i >= 0; i-- )
    {
        if ( ibsta & stat[ 15 - i ] )
            fprintf( gpib_log, " %s", is[ 15 - i ] );
    }
    fprintf( gpib_log, " > -> %s\n", ie[ iberr ] );
    fflush( gpib_log );
	seteuid( getuid( ) );
}


/*------------------------------------------------------------*/
/* gpib_log_function_start() logs the call of a GPIB function */
/* by appending a short message to the log file.              */
/* ->                                                         */
/*  * name of the function                                    */
/*  * name of the device involved                             */
/*------------------------------------------------------------*/

static void gpib_log_function_start( const char *function,
									 const char *dev_name )
{
    gpib_log_date( );
	seteuid( EUID );
    fprintf( gpib_log, "CALL of %s, dev = %s\n", function, dev_name );
    fflush( gpib_log );
	seteuid( getuid( ) );
}


/*--------------------------------------------------------*/
/* gpib_log_function_end() logs the completion of a GPIB  */
/* function by appending a short message to the log file. */
/* ->                                                     */
/*  * name of the function                                */
/*  * name of the device involved                         */
/*--------------------------------------------------------*/

static void gpib_log_function_end( const char *function,
								   const char *dev_name )
{
    if ( ibsta & IBERR )
        gpib_log_error( function );
    else
    {
        if ( ll > LL_ERR )
        {
            gpib_log_date( );
			seteuid( EUID );
            fprintf( gpib_log, "EXIT of %s, dev = %s\n", function, dev_name );
			seteuid( getuid( ) );
        }
    }

	seteuid( EUID );
    fflush( gpib_log );
	seteuid( getuid( ) );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static char *gpib_get_dev_name( int device )
{
	GPIB_DEV *cur_dev = gpib_dev_list;


	while ( cur_dev != NULL && cur_dev->number != device )
		cur_dev = cur_dev->next;

	return cur_dev == NULL ? NULL : cur_dev->name;
}
