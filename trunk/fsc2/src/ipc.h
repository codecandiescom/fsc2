/*
  $Id$
*/


#if ! defined IPC_HEADER
#define IPC_HEADER

#include "fsc2.h"

union semun {
	int val;                    /* value for SETVAL */
	struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;  /* array for GETALL, SETALL */
	struct seminfo *__buf;      /* buffer for IPC_INFO */
};


#if ! defined ( SEM_R )
#define SEM_R 0400
#endif
#if ! defined ( SEM_A )
#define SEM_A 0200
#endif
#if ! defined ( SHM_R )
#define SHM_R 0400
#endif
#if ! defined ( SHM_A )
#define SHM_A 0200
#endif


void *get_shm( int *shm_id, long len );
void *attach_shm( int key );
void detach_shm( void *buf, int *key );
void delete_all_shm( void );
void delete_stale_shms( void );
int sema_create( int val );
int sema_destroy( int sema_id );
int sema_wait( int sema_id );
int sema_post( int sema_id );


#endif   /* ! IPC_HEADER */
