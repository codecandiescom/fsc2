/*
  $Id$
*/

/*------------------------------------------------------------------------*/
/* Contains all declarations possibly needed by routines using the GPIB   */
/* functions. Each of the functions returns either SUCCESS or FAILURE, in */
/* the latter case in 'gpib_error_msg' a short text is stored explaining  */
/* the cause of the error.                                                */
/*------------------------------------------------------------------------*/


#include <sys/ugpib.h>

#define GPIB_MAX_DEVICES 14
#define GPIB_NAME_MAX 14

/* End-of-string (EOS) modes */

#define GPIB_REOS     0x04      /* Terminate reads on EOS     */
#define GPIB_XEOS     0x08      /* Set EOI with EOS on writes */
#define GPIB_BIN      0x10      /* Do 8-bit compare on EOS    */
#define GPIB_EOT      0x01      /* Send END with last byte    */

#define ADDRESS( pad, sad ) \
                           ( ( ( pad ) & 0xff ) | ( ( ( sad ) & 0xff ) << 8 ) )


typedef struct {
	int is_online;
	int number;
    char name[ GPIB_NAME_MAX + 1 ];
    int pad;
    int sad;
    int eos;
    int eosmode;
    int timo;
    int flags;
} GPIB_Device;



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
#define GPIB_CONF_FILE  "/etc/gpib.conf"
#endif


GPIB_VARIABLE int gpib_init( char *log_file_name, int log_level );
GPIB_VARIABLE int gpib_dev_setup( Device *temp_dev );
GPIB_VARIABLE int gpib_shutdown( void );
GPIB_VARIABLE int gpib_init_device( const char *device_name, int *dev );
GPIB_VARIABLE int gpib_local( int device );
GPIB_VARIABLE int gpib_timeout( int device, int period );
GPIB_VARIABLE int gpib_clear_device( int device );
GPIB_VARIABLE int gpib_trigger( int device );
GPIB_VARIABLE int gpib_wait( int device, int mask, int *status );
GPIB_VARIABLE int gpib_write( int device, const char *buffer, long length );
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
