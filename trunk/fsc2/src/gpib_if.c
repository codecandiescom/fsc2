/*
  $Id$
*/

/*
    Problems:                    (9-10-98)

    1. Shouldn't there be some kind of locking mechanism in order to avoid
       that programs try to use the bus concurrently ?

    2. What about ibtmo() ? Does it really set a timeout period
	   for an individual device or just for all devices ?
*/


#define __GPIB__


#include "fsc2.h"
#include "gpib.h"


/*----------------------------------------------------------------*/
/* GPIB_LOG_FILE is the name of the default file to which logging */
/* information about GPIB operations are written - the name can   */
/* changed by passing a different name to gpib_init().            */
/*----------------------------------------------------------------*/


#define CONTROLLER  "gpib0"      /* symbolic name of the controller */


#define MLA      0x20
#define TLA      0x40
#define SRQ      0x40    /* SRQ bit in device status register */
#define TIMEOUT  T10s    /* GPIB timeout period set at initialisation */


#define ON  1
#define OFF 0


#define AWK_DEL_COMMENTS AWK_PROG" 'BEGIN{FS=\"\"}{for(i=1;i<=length($0);++i)"\
                         "{if(c!=1){if($i==\"/\"&&$(i+1)==\"*\"){c=1;++i}"\
                         "else printf$i}else{if($i==\"*\"&&$(i+1)==\"/\")"\
                         "{c=0;++i}}}print""}' "


#define TEST_BUS_STATE                                               \
        if ( ! gpib_is_active )                                      \
        {                                                            \
            strcpy( gpib_error_msg, "GPIB bus not initialised.\n" ); \
            return FAILURE;                                          \
        }


/*------------------------------------------------------------------------*/
/* functions used only locally                                            */
/*------------------------------------------------------------------------*/


int   gpib_init_controller( void );
void  gpib_init_log( char **log_file_name );
void  gpib_set_msg( const char *msg, int device );
void  gpib_read_end( int device, char *buffer, long received, long expected );
void  gpib_log_date( void );
void  gpib_log_error( const char *type );
void  gpib_write_start( int device, const char *buffer, long length );
void  gpib_log_function_start( const char *function, int device );
void  gpib_log_function_end( const char *function, int device );
bool  gpib_get_dev_add( int device );
int   gpib_get_max_dev( void );
char *gpib_get_conf_file( char *file );




/*------------------------------------------------------------------------*/
/* global variables                                                       */
/*------------------------------------------------------------------------*/


int ll;                         /* log level                              */
int gpib_is_active = 0;         /* flag, set after initialisation of bus  */
int controller;                 /* device number assigned to controller   */
int timeout;                    /* stores actual timeout period           */
FILE *gpib_log;                 /* file pointer of GPIB log file          */
int max_devices;                /* maximum number of devices              */
GPIB_DEV *gpib_dev_list;        /* list of symbolic names of devices etc. */



/*-------------------------------------------------------------------------*/
/* gpib_init() initialises the GPIB bus by starting the logging mechanism  */
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
/*    GPIB_LOG_FILE will be used. If the pointer is not NULL but points to */
/*    NULL or to an empty string it will, on return, point to the name of  */
/*    the log file or, if the log file could not be opened, to NULL.       */
/*  * log level, either LL_NONE, LL_ERR, LL_CE or LL_ALL                   */
/*    (if log level is LL_NONE 'log_file_name' is not used at all)         */
/* <-                                                                      */
/*  * SUCCESS: bus is initialised                                          */
/*  * FAILURE: error, GPIB bus can't be used                               */
/*-------------------------------------------------------------------------*/

int gpib_init( char **log_file_name, int log_level )
{
     if ( gpib_is_active )
         return SUCCESS;

     /* Get memory for the list of devices */

     if ( ( max_devices = gpib_get_max_dev( ) ) < 0 )
		 return FAILURE;

     if ( NULL == ( gpib_dev_list =
          ( GPIB_DEV * ) calloc( max_devices + 1, sizeof( GPIB_DEV ) ) ) )
     {
         strcpy( gpib_error_msg, "Not enough memory." );
         return FAILURE;
     }
     ++gpib_dev_list;       /* first element is used in initialisation only */

     ll = log_level;
     if ( ll < LL_NONE )
         ll = LL_NONE;
     if ( ll > LL_ALL )
         ll = LL_ALL;

    gpib_init_log( log_file_name );             /* initialise logging */

    if ( gpib_init_controller( ) != SUCCESS )   /* initialise the controller */
    {
        strcpy( gpib_error_msg, "Can't initialise GPIB bus !" );
        return FAILURE;
    }

    gpib_is_active = 1;
    gpib_timeout( TIMEOUT );   /* set timeout period to a known value */
    return SUCCESS;
}


/*-------------------------------------------------------------*/
/* gpib_init_controller() initialises the controller by first  */
/* getting the device/board descriptor, testing, if this is    */
/* really the controller and finally "switching on" the board, */
/* clearing the interface and asserting the REN line.          */
/* <-                                                          */
/*  * SUCCESS: OK, FAILURE: error                              */
/*-------------------------------------------------------------*/

int gpib_init_controller( void )
{
    if ( gpib_init_device( CONTROLLER, &controller ) != SUCCESS )
        return FAILURE;

    if( ! ibIsMaster( controller ) )
        return FAILURE;

    if  ( ( ibonl( controller, ON ) | ibsic( controller ) |
            ibsre( controller, ON ) ) & ERR )
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
    int i;


    TEST_BUS_STATE;

    for ( i = 1; i < max_devices; ++i )  /* set all devices to local state */
    {
        if ( gpib_dev_list[ i ].name[ 0 ] != '\0' )
            gpib_local( i );
    }

    ibsre( controller, OFF );       /* pull down the REN line */
    ibonl( controller, OFF );       /* "switch off" the controller */

    if ( ll > LL_NONE )
    {
        gpib_log_date( );
        fprintf( gpib_log, "GPIB bus is being shut down.\n\n" );
        if ( gpib_log != stderr )
            fclose( gpib_log );                 /* close log file */
    }

    gpib_is_active = 0;
    free( --gpib_dev_list );

    return SUCCESS;
}


/*-------------------------------------------------------------------------*/
/* gpib_init_log() initialises the logging mechanism. If the logging level */
/* is not LL_NONE, a log file will be opened. The name to be used for the  */
/*  log file can be passed to the function, see below. If the file cannot  */
/* be opened 'sterr' is used instead.                                      */
/* ->                                                                      */
/*  * Pointer to the name of log file - if the pointer is NULL or does not */
/*    point to a non-empty string the default log file name defined by     */
/*    GPIB_LOG_FILE will be used. If the pointer is not NULL but points to */
/*    NULL or to an empty string it will point to the name of the log file */
/*    or, if the log file cannot be opened, to NULL on return.             */
/*-------------------------------------------------------------------------*/

void gpib_init_log( char **log_file_name )
{
    static char name[ FILENAME_MAX ];


    if ( ll == LL_NONE )
        return;

    if ( log_file_name != NULL && *log_file_name != NULL
                               && **log_file_name != '\0' )
        strcpy( name, *log_file_name );
    else
    {
        strcpy( name, GPIB_LOG_FILE );
        if ( log_file_name != NULL )
            *log_file_name = name;
    }

    if ( ( gpib_log = fopen( name, "a+" ) ) == NULL )
    {
        gpib_log = stderr;
        if ( log_file_name != NULL )
            *log_file_name = NULL;
        fprintf( stderr, "Can't open log file %s - using stderr instead.\n",
                 name );
    }

    gpib_log_date( );
    fprintf( gpib_log, "GPIB bus is being initialised.\n" );
    fflush( gpib_log );
}


/*-----------------------------------------------------------------------*/
/* gpib_init_device() initialises a device.                              */
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
    int device;


    if ( ! gpib_is_active && strcmp( device_name, CONTROLLER ) )
    {
        strcpy( gpib_error_msg, "GPIB bus not initialised.\n" );
        return FAILURE;
    }

    if ( ll > LL_ERR )
    {
        strcpy( gpib_dev_list[ -1 ].name, device_name );
        gpib_log_function_start( "gpib_init_device", -1 );
    }

    device = ibfind( ( char * ) device_name );

    if ( ! ( ibsta & ERR ) )
    {
        strcpy( gpib_dev_list[ device ].name, device_name );
        if ( ! gpib_get_dev_add( device ) )
			return FAILURE;
    }

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_init_device", device );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't initialise device %s, ibsta = 0x%x",
				 device_name, ibsta );
        return FAILURE;
    }
    else
    {
        *dev = device;
        return SUCCESS;
    }
}


/*-----------------------------------------------------------*/
/* gpib_timeout() sets the period the controller is supposed */
/* to wait for a reaction from a device before timing out.   */
/* ->                                                        */
/*  * timeout period (cmp. definitions of TNONE to T1000s)   */
/* <-                                                        */
/*  * SUCCESS: OK, FAILURE: error                            */
/*-----------------------------------------------------------*/

int gpib_timeout( int period )
{
    const char tc[ ][ 9 ] = { "infinity", "10us", "30us", "100us",
							  "300us", "1ms", "3ms", "10ms", "30ms",
							  "100ms", "300ms", "1s", "3s", "10s",
							  "30s", "100s", "300s", "1000s" };


    TEST_BUS_STATE;              /* bus not initialised yet ? */

    if ( period < TNONE )        /* check validity of parameter */
        period = TNONE;
    if ( period > T1000s )
        period = T1000s;

    if ( ll > LL_ERR )
    {
        gpib_log_date( );
        fprintf( gpib_log, "CALL of gpib_timeout, " );
        fprintf( gpib_log, "-> timeout is set to %s\n", tc[ period ] );
        fflush( gpib_log );
    }

    ibtmo( controller, period );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't set timeout period, ibsta = 0x%x.",
				 ibsta );
        return FAILURE;
    }

    if ( ll > LL_ERR )
    {
        gpib_log_date( );
        fprintf( gpib_log, "EXIT of gpib_timeout\n" );
        fflush( gpib_log );
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
    TEST_BUS_STATE;              /* bus not initialised yet ? */

    if ( ll > LL_ERR )
        gpib_log_function_start( "gpib_clear_device", device );

    ibclr( device );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_clear_device", device );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't clear device %s, ibsta = 0x%x",
				 gpib_dev_list[ device ].name, ibsta );
        return FAILURE;
    }

    return SUCCESS;
}


/*-------------------------------------------------------*/
/* gpib_local() moves a device from remote to local mode */
/* by sending the GTO multi-line message.                */
/* ->                                                    */
/*  * number of device to be set into local mode         */
/* <-                                                    */
/*  * SUCCESS: OK, FAILURE: error                        */
/*-------------------------------------------------------*/

int gpib_local( int device )
{
    char gtl_msg[ 2 ];


    TEST_BUS_STATE;              /* bus not initialised yet ? */

    gtl_msg[ 0 ] = GTL;             /* set up the GTL message */
    gtl_msg[ 1 ] = gpib_dev_list[ device ].mla;

    if ( ll > LL_ERR )
        gpib_log_function_start( "gpib_local", device );

    ibcmd( controller, ( void * ) gtl_msg, 2L );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_local", device );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't send 'GOTO LOCAL' message to device "
				 "%s, ibsta = 0x%x", gpib_dev_list[ device ].name, ibsta );
        return FAILURE;
    }

    return SUCCESS;
}


/*--------------------------------------------------------------*/
/* gpib_lock() moves a device to local lock out mode by sending */
/* the LLO multi-line message.                                  */
/* ->                                                           */
/*  * number of device to be set into local lock out mode       */
/* <-                                                           */
/*  * SUCCESS: OK, FAILURE: error                               */
/*--------------------------------------------------------------*/

int gpib_lock( int device )
{
    char llo_msg[ 2 ];


    TEST_BUS_STATE;              /* bus not initialised yet ? */

    llo_msg[ 0 ] = LLO;            /* set up the LLO message */
    llo_msg[ 1 ] = gpib_dev_list[ device ].mla;

    if ( ll > LL_ERR )
        gpib_log_function_start( "gpib_lock", device );

    ibcmd( controller, ( void * ) llo_msg, 2L );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_lock", device );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't send 'LOCAL LOCK OUT' message to "
				 "device %s, ibsta = 0x%x", gpib_dev_list[ device ].name,
				 ibsta );
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
    TEST_BUS_STATE;              /* bus not initialised yet ? */

    if ( ll > LL_ERR )
        gpib_log_function_start( "gpib_trigger", device );

    ibtrg( device );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_trigger", device );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't trigger device %s, ibsta = 0x%x",
				 gpib_dev_list[ device ].name, ibsta );
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


    TEST_BUS_STATE;              /* bus not initialised yet ? */

    if ( ll > LL_ERR )
    {
        gpib_log_function_start( "gpib_wait", device );
        fprintf( gpib_log, "wait mask = 0x0%X\n", mask );
        if ( mask & ~( TIMO | END | RQS | CMPL ) )
            fprintf( gpib_log, "=> Setting mask to 0x%X <=\n",
                     mask & ( TIMO | END | RQS | CMPL ) );
        fflush( gpib_log );
    }

    mask &= TIMO | END | RQS | CMPL;    /* remove invalid bits */

    if ( ! ( mask & TIMO ) && timeout != TNONE )
        gpib_timeout( T3s );//NONE );

    ibwait( device, mask );

    if ( status != NULL )
        *status = ibsta;

	if ( ll > LL_ERR )
        fprintf( gpib_log, "wait return status = 0x0%X\n", ibsta );

    if ( ! ( mask & TIMO ) && old_timeout != TNONE )
        gpib_timeout( old_timeout );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_wait", device );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't wait for device %s, ibsta = 0x%x",
				 gpib_dev_list[ device ].name, ibsta );
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
    TEST_BUS_STATE;              /* bus not initialised yet ? */

    if ( length <= 0 )           /* check validity of length parameter */
    {
        if ( ll != LL_NONE )
        {
            gpib_log_date( );
            fprintf( gpib_log, "ERROR in in call of gpib_write: "
                               "Invalid parameter: %ld bytes.\n", length );
            fflush( gpib_log );
        }
        sprintf( gpib_error_msg, "Can't write %ld bytes.", length );
        return FAILURE;
    }

    if ( ll > LL_ERR )
        gpib_write_start( device, buffer, length );

	ibwrt( device, ( char * ) buffer, length );

    if ( ll > LL_NONE )
        gpib_log_function_end( "gpib_write", device );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't send data to device %s, ibsta = 0x%x",
				 gpib_dev_list[ device ].name, ibsta );
        return FAILURE;
    }

    return SUCCESS;
}


/*----------------------------------------------------------------*/
/* gpib_write_start() logs the call to the function gpib_write(). */
/* ->                                                             */
/*  * number of device the data are to be sent to                 */
/*  * buffer containing the bytes to be sent                      */
/*  * number of bytes to be sent                                  */
/*----------------------------------------------------------------*/

void gpib_write_start( int device, const char *buffer, long length )
{
    long i;


    gpib_log_function_start( "gpib_write", device );
    fprintf( gpib_log, "-> There are %ld bytes to be sent\n", length );

    if ( ll == LL_ALL )
    {
        for ( i = 0; i < length; ++i )
            fputc( ( int ) buffer[ i ], gpib_log );
        fputc( ( int) '\n', gpib_log );
    }
    fflush( gpib_log );
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


    TEST_BUS_STATE;              /* bus not initialised yet ? */

    if ( *length <= 0 )          /* check validity of length parameter */
    {
        if ( ll != LL_NONE )
        {
            gpib_log_date( );
            fprintf( gpib_log, "ERROR in call of gpib_read: "
                               "Invalid parameter: %ld bytes\n", *length );
            fflush( gpib_log );
        }
        sprintf( gpib_error_msg, "Can't read %ld bytes.", *length );
        return FAILURE;
    }

    if ( ll > LL_ERR )
    {
        gpib_log_function_start( "gpib_read", device );
        fprintf( gpib_log, "-> Expecting up to %ld bytes\n", *length );
        fflush( gpib_log );
    }

    ibrd( device, buffer, expected );
    *length = ibcnt;

    if ( ll > LL_NONE )
        gpib_read_end( device, buffer, *length, expected );

    if ( ibsta & ERR )
    {
        sprintf( gpib_error_msg, "Can't read data from device %s, ibsta = "
				 "0x%x.", gpib_dev_list[ device ].name, ibsta );
        return FAILURE;
    }

    return SUCCESS;
}


/*------------------------------------------------------------------*/
/* gpib_read_end() logs the completion of the function gpib_read(). */
/* ->                                                               */
/*  * number of device the data were received from                  */
/*  * buffer for storing the data                                   */
/*  * maximum number of data to read                                */
/*  * number of bytes actually read                                 */
/*------------------------------------------------------------------*/

void gpib_read_end( int device, char *buffer, long received, long expected )
{
    long i;


    if ( ll > LL_ERR || ( ibsta & ERR ) )
        gpib_log_function_end( "gpib_read", device );

    if ( ll < LL_CE )
        return;

    fprintf( gpib_log, "-> Received %ld of up to %ld bytes\n",
             received, expected );

    if ( ll == LL_ALL )
    {
        for ( i = 0; i < received; ++i )
            fputc( ( int ) buffer[ i ], gpib_log );
        fputc( ( int ) '\n', gpib_log );
    }

    fflush( gpib_log );
}


/*--------------------------------------------------*/
/* gpib_log_date() writes the date to the log file. */
/*--------------------------------------------------*/

void gpib_log_date( void )
{
    static char tc[ 26 ];
    time_t t;


    t = time( NULL );
    strcpy( tc, asctime( localtime( &t ) ) );
    tc[ 24 ] = '\0';
    fprintf( gpib_log, "[%s] ", tc );
}


/*--------------------------------------------------------------*/
/* gpib_log_error() writes an error message to the log file.    */
/* ->                                                           */
/*  * string with short description of the type of the function */
/*    the error happened in                                     */
/*--------------------------------------------------------------*/

void gpib_log_error( const char *type )
{
    static int stat[ 16 ] = { 0x8000, 0x4000, 0x2000, 0x1000,
                              0x0800, 0x0400, 0x0200, 0x0100,
                              0x0080, 0x0040, 0x0020, 0x0010,
                              0x0008, 0x0004, 0x0002, 0x0001 };
    static char is[ 16 ][ 5 ] = {  "ERR", "TIMO",  "END", "SRQI",
                                   "RQS",   "\b",   "\b", "CMPL",
                                   "LOK",  "REM",  "CIC",  "ATN",
                                   "TACS", "LACS", "DTAS", "DCAS" };
    static char ie[ 24 ][ 5 ] = { "EDVR", "ECIC", "ENOL", "EADR",
                                  "EARG", "ESAC", "EABO", "ENEB",
                                  "EDMA", "EBTO", "EOIP", "ECAP",
                                  "EFSO", "EOWN", "EBUS", "ESTB",
                                  "ESRQ", "ECFG", "EPAR", "ETAB",
                                  "ENSD", "ENWE", "ENTF", "EMEM" };
    int i;


    gpib_log_date( );
    fprintf( gpib_log, "ERROR in function %s: <", type );
    for ( i = 15; i >= 0; i-- )
    {
        if ( ibsta & stat[ 15 - i ] )
            fprintf( gpib_log, " %s", is[ 15 - i ] );
    }
    fprintf( gpib_log, " > -> %s\n", ie[ iberr ] );
    fflush( gpib_log );
}


/*------------------------------------------------------------------*/
/* gpib_set_msg() stores a text with information about a GPIB error */
/* in the global char array 'gpib_error_msg'.                       */
/* ->                                                               */
/*  * text to be copied into 'gpib_error_msg'                       */
/*  * number of device involved in the GPIB error                   */
/*------------------------------------------------------------------*/

void gpib_set_msg( const char *msg, int device )
{
    sprintf( gpib_error_msg, "%s %s", msg, gpib_dev_list[ device ].name );
}


/*------------------------------------------------------------*/
/* gpib_log_function_start() logs the call of a GPIB function */
/* by appending a short message to the log file.              */
/* ->                                                         */
/*  * name of the function                                    */
/*  * number of the device involved                           */
/*------------------------------------------------------------*/

void gpib_log_function_start( const char *function, int device )
{
    gpib_log_date( );

    fprintf( gpib_log, "CALL of %s", function );

    fprintf( gpib_log, ", dev = %s", gpib_dev_list[ device ].name );

    if ( device >= 0 )
        fprintf( gpib_log, " (%d)", gpib_dev_list[ device ].add );

    fputc( ( int ) '\n', gpib_log );

    fflush( gpib_log );
}


/*--------------------------------------------------------*/
/* gpib_log_function_end() logs the completion of a GPIB  */
/* function by appending a short message to the log file. */
/* gpib_init_device().                                    */
/* ->                                                     */
/*  * name of the function                                */
/*  * number of the device involved                       */
/*--------------------------------------------------------*/

void gpib_log_function_end( const char *function, int device )
{
    if ( ibsta & ERR )
        gpib_log_error( function );
    else
    {
        if ( ll > LL_ERR && device >= 0 )
        {
            gpib_log_date( );
            fprintf( gpib_log, "EXIT of %s, dev = %s (%d)\n", function,
                     gpib_dev_list[ device ].name,
                     gpib_dev_list[ device ].add );
        }
    }

    fflush( gpib_log );
}


/*--------------------------------------------------------------------*/
/* gpib_get_dev_add() extracts the GPIB address of a device from the  */
/* configuration file and calculates the listener and talker address. */
/* On return, the corresponding entries in 'gpib_dev_list' are set.   */
/* No error checking is necessary since if there were an error this   */
/* would already have been detected by ibfind() (hopefully...).       */
/* ->                                                                 */
/*  * number of the device                                            */
/*--------------------------------------------------------------------*/

bool gpib_get_dev_add( int device )
{
    char awk[ 350 + FILENAME_MAX ];
    FILE *pipe;
	char *pos;


    /* These rather ugly lines make up two awk commands - the first one is
	   supposed to remove all comments from the configuration file while the
	   second one should find the the primary GPIB address of the device... */

    strcpy( awk, AWK_DEL_COMMENTS );
	pos = awk + strlen( awk );
    gpib_get_conf_file( pos );
	if ( *pos == '\0' )
		return 0;
    strcat( awk, "|awk '(tolower($1)~/^device=*/){p=f=0;while($1!=\"}\")"
                 "{if(tolower($1)==\"name\"){if($3!=\"" );
    strcat( awk, gpib_dev_list[ device ].name );
    strcat( awk, "\")next;f=1};if(tolower($1)==\"pad\")p=$3;"
                 "if(f&&p){print p;exit};getline}}'" );

    pipe = popen( awk, "r" );
    fscanf( pipe, "%d", &gpib_dev_list[ device ].add );
    pclose( pipe );

    gpib_dev_list[ device ].mla = ( char ) gpib_dev_list[ device ].add | MLA;
    gpib_dev_list[ device ].tla = ( char ) gpib_dev_list[ device ].add | TLA;

	return 1;
}


/*---------------------------------------------------------*/
/* gpib_max_dev() determines the maximum number of devices */
/* listed in the GPIB_CONF_FILE configuration file.        */
/* ->                                                      */
/*  * maximum number of devices in configuration file      */
/*---------------------------------------------------------*/

int gpib_get_max_dev( void )
{
    char awk[ 250 + FILENAME_MAX ];
    FILE *pipe;
    int max_dev;
	char *pos;

    /* The following lines count the number of devices defined in the
       configuration file GPIB_CONF_FILE again using awk */

    strcpy( awk, AWK_DEL_COMMENTS );

	pos = awk + strlen( awk );
    gpib_get_conf_file( pos );
	if ( *pos == '\0' )
		return -1;
    strcat( awk, "|awk '(tolower($1)~/^device=*/){++c}END{print c}'" );

    pipe = popen( awk, "r" );
    fscanf( pipe, "%d", &max_dev );
    pclose( pipe );

    return max_dev;
}


/*-----------------------------------------------------*/
/* gpib_get_conf_file() determines the name of the     */
/* GPIB_CONF_FILE configuration file.                  */
/* ->                                                  */
/*  * pointer to string for passing back the file name */
/*-----------------------------------------------------*/

char *gpib_get_conf_file( char *file )
{
    char *env;
	struct stat file_stat;


    if ( ( env = getenv( "IB_CONFIG" ) ) == NULL )
        strcpy( file, GPIB_CONF_FILE );
    else
    {
        strcpy( file, strchr( env, '=' ) + 1 );
        if ( *( env + strlen( env ) - 1 ) != '/' )
            strcat( file, "/" );
        strcat( file, "gpib.conf" );
    }

	if ( stat ( file, &file_stat ) < 0 )
		switch ( errno )
		{
			case ENOENT :
				eprint( FATAL, "gpib: GPIB configuration file `%s' does not "
						"exist.", file );
				strcpy( file, "" );
				break;

			case EACCES :
				eprint( FATAL, "gpib: No permission to access GPIB "
						"configuration file `%s'.", file );
				strcpy( file, "" );
				break;

			default :
				eprint( FATAL, "gpib: Can't read GPIB configuration file "
						"`%s'.", file );
				strcpy( file, "" );
				break;
		}

	return file;
}
