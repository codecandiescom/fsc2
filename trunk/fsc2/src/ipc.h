/*
  $Id$
*/


#if ! defined IPC_HEADER
#define IPC_HEADER

#include "fsc2.h"

//#if defined( __GNU_LIBRARY__ ) && ! defined( _SEM_SEMUN_UNDEFINED )
///* union semun is defined by including <sys/sem.h> */
//#else
///* according to X/OPEN we have to define it ourselves */
union semun {
	int val;                    /* value for SETVAL */
	struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;  /* array for GETALL, SETALL */
	struct seminfo *__buf;      /* buffer for IPC_INFO */
};
//#endif


#if ! defined ( SEM_R )
#define SEM_R 0400
#endif
#if ! defined ( SEM_A )
#define SEM_A 0200
#endif


int sema_create( int val );
int sema_destroy( int sema_id );
int sema_wait( int sema_id );
int sema_post( int sema_id );


#endif   /* ! IPC_HEADER */
