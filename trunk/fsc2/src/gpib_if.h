/*
  $Id$
*/

/*------------------------------------------------------------------------*/
/* Contains all declarations possibly needed by routines using the GPIB   */
/* functions. Each of the functions returns either SUCCESS or FAILURE, in */
/* the latter case in 'gpib_error_msg' a short text is stored explaining  */
/* the cause of the error.                                                */
/*------------------------------------------------------------------------*/


#include <gpib/ib.h>


/* In older versions of the GPIB driver ERR was defined but this clashes
   with newer kernel versions. So, if you still use an old GPIB driver
   the following line is needed... */

#if ! defined( IBERR )
#warning "**************************************************"
#warning "* Using ERR will conflict with post-2.2 kernels! *"
#warning "**************************************************"
#define IBERR ERR
#endif


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

#if defined GPIB_CONF
#define GPIB_CONF_FILE GPIB_CONF
#else
#define GPIB_CONF_FILE "/etc/gpib.conf"
#endif

#if ! defined AWK_PROG
#if defined AWK
#define AWK_PROG AWK
#else
#define AWK_PROG "awk"
#endif
#endif



#define DMA_SIZE 64512    /* compare this with entry in /etc/gpib.conf ! */


typedef struct
{
    char name[ 128 ],           /* symbolic name of device         */
         mla,                   /* listener address of device      */
         tla;                   /* talker address of device        */
    int add;                    /* primary GPIB address  of device */
} GPIB_DEV;


GPIB_VARIABLE int gpib_init( char **log_file_name, int log_level );
GPIB_VARIABLE int gpib_shutdown( void );
GPIB_VARIABLE int gpib_init_device( const char *device_name, int *dev );
GPIB_VARIABLE int gpib_timeout( int period );
GPIB_VARIABLE int gpib_clear_device( int device );
GPIB_VARIABLE int gpib_local( int device );
GPIB_VARIABLE int gpib_lock( int device );
GPIB_VARIABLE int gpib_trigger( int device );
GPIB_VARIABLE int gpib_wait( int device, int mask, int *status );
GPIB_VARIABLE int gpib_write( int device, const char *buffer, long length );
GPIB_VARIABLE int gpib_read( int device, char *buffer, long *length );


GPIB_VARIABLE char gpib_error_msg[1024]; /* global for GPIB error messages */


#define SUCCESS   0
#define FAILURE  -1


/*----------------------------------------------------------*/
/* definition of log levels allowed in calls of gpib_init() */
/*----------------------------------------------------------*/

#define  LL_NONE  0    /* log nothing */
#define  LL_ERR   1    /* log errors only */
#define  LL_CE    2    /* log function calls and function exits */
#define  LL_ALL   3    /* log calls with parameters and function exits */
