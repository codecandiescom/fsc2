/*
  $Id$
*/

/*------------------------------------------------------------------------*/
/* Contains all declarations possibly needed by routines using the GPIB   */
/* functions. Each of the functions returns either SUCCESS or FAILURE, in */
/* the latter case in 'gpib_error_msg' a short text is stored explaining  */
/* the cause of the error.                                                */
/*------------------------------------------------------------------------*/


#include <gpib.h>


#if defined ( __GPIB__ )
	#define GPIB_VARIABLE
#else
	#define GPIB_VARIABLE extern
#endif


/* GPIB_LOG, GPIB_CONF and AWK might be defined via compiler flags -
   otherwise define it here */

#if defined GPIB_LOG
#define GPIB_LOG_FILE GPIB_LOG
#else
#define GPIB_LOG_FILE  "/tmp/gpib.log"
#endif


#define DMA_SIZE 64512    /* compare this with entry in /etc/gpib.conf ! */


typedef struct _gd_
{
	int number;           /* device number */
    char *name;           /* symbolic name of device */
	struct _gd_ *next;    /* pointer to next GPIB_DEV structure */
	struct _gd_ *prev;    /* pointer to previous GPIB_DEV structure */
} GPIB_DEV;


GPIB_VARIABLE int gpib_init( char *log_file_name, int log_level );
GPIB_VARIABLE int gpib_shutdown( void );
GPIB_VARIABLE int gpib_init_device( const char *device_name, int *dev );
GPIB_VARIABLE int gpib_timeout( int device, int period );
GPIB_VARIABLE int gpib_clear_device( int device );
GPIB_VARIABLE int gpib_local( int device );
GPIB_VARIABLE int gpib_trigger( int device );
GPIB_VARIABLE int gpib_wait( int device, int mask, int *status );
GPIB_VARIABLE int gpib_write( int device, const char *buffer );
GPIB_VARIABLE int gpib_read( int device, char *buffer, long *length );
GPIB_VARIABLE void gpib_log_message( const char *fmt, ... );


GPIB_VARIABLE char gpib_error_msg[ 1024 ]; /* global for GPIB error messages */


#define SUCCESS   0
#define FAILURE  -1


/*----------------------------------------------------------*/
/* definition of log levels allowed in calls of gpib_init() */
/*----------------------------------------------------------*/

#define  LL_NONE  0    /* log nothing */
#define  LL_ERR   1    /* log errors only */
#define  LL_CE    2    /* log function calls and function exits */
#define  LL_ALL   3    /* log calls with parameters and function exits */
