/*
  $Id$
*/


#if ! defined IPC_HEADER
#define IPC_HEADER

#include "fsc2.h"


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
int sema_create( void );
int sema_destroy( int sema_id );
int sema_wait( int sema_id );
int sema_post( int sema_id );


#endif   /* ! IPC_HEADER */
