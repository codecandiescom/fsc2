/*
  $Id$
*/

#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/sem.h>



/*--------------------------------------------------------------------*/
/* Routine tries to get a shared memory segment - if this fails and   */
/* the reason is that no segments or no memory for segments are left  */
/* it waits for some time hoping for the parent process to remove     */
/* other segments in the mean time. On success it writes the 'magic   */
/* string' "fsc2" into the start of the segment and returns a pointer */
/* to the following memory. If it fails completely it returns -1.     */
/*--------------------------------------------------------------------*/

void *get_shm( int *shm_id, long len )
{
	void *buf;
	bool must_reset = UNSET;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}

	while ( ( *shm_id = shmget( IPC_PRIVATE, len + 4 * sizeof( char ),
								IPC_CREAT | SHM_R | SHM_A ) ) < 0 )
	{
		if ( errno == ENOSPC || errno == ENOMEM)  /* wait for 10 ms */
			usleep( 10000 );
		else                                      /* non-recoverable failure */
		{
			if ( must_reset )
				seteuid( getuid( ) );
			return ( void * ) -1;
		}
	}

	/* Attach to the shared memory segment - if this should fail (improbable)
	   return -1 and let the calling routine handle the mess... */

	if ( ( buf = shmat( *shm_id, NULL, 0 ) ) == ( void * ) - 1 )
	{
		if ( must_reset )
			seteuid( getuid( ) );
		return ( void * ) -1;
	}

	/* Now write 'magic string' into the start of the shared memory to make
	   it easier to identify it later */

	memcpy( buf, "fsc2", 4 * sizeof( char ) );         /* magic id */
	buf += 4 * sizeof( char );

	if ( must_reset )
		seteuid( getuid( ) );

	return buf;
}


/*---------------------------------------------------------------*/
/* Function tries to attach to the shared memory associated with */
/* 'key'. On success it returns a pointer to the memory region   */
/* (skipping the 'magic string' "fsc2"), on error it returns -1. */
/*---------------------------------------------------------------*/

void *attach_shm( int key )
{
	void *buf;
	bool must_reset = UNSET;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}

	if ( ( buf = shmat( key, NULL, SHM_RDONLY ) ) == ( void * ) - 1 )
	{
		shmctl( key, IPC_RMID, NULL );                 /* delete the segment */
		if ( must_reset )
			seteuid( getuid( ) );
		return ( void * ) -1;
	}

	if ( must_reset )
		seteuid( getuid( ) );

	return buf + 4 * sizeof( char );
}


/*---------------------------------------------------------------------*/
/* Function detaches from a shared memory segment and, if a valid key  */
/* (i.e. a non-negative key) is passed to the function it also deletes */
/* the shared memory region.                                           */
/*---------------------------------------------------------------------*/

void detach_shm( void *buf, int *key )
{
	bool must_reset = UNSET;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}
	
	shmdt( buf - 4 * sizeof( char ) );
	if ( key != NULL )
	{
		shmctl( *key, IPC_RMID, NULL );
		*key = -1;
	}

	if ( must_reset )
		seteuid( getuid( ) );
}


/*-----------------------------------------------------------------*/
/* Function tries to delete all shared memory. Shared memory is    */
/* used for data with the identifier stored in the message queue.  */
/* So if the message queue exists (i.e. isn't NULL) we run through */
/* all identifiers, and if they're non-negative we delete the thus */
/* indexed segment. Finally, we delete the memory segment used for */
/* the 'master key'.                                               */
/*-----------------------------------------------------------------*/

void delete_all_shm( void )
{
	int i;
	bool must_reset = UNSET;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}

	/* If message queue exists check that all memory segments indixed in it
	   are deleted */

	if ( Message_Queue != NULL )
	{
		for ( i = 0; i < QUEUE_SIZE; i++ )
			if ( Message_Queue[ i ].shm_id >= 0 )
			{
				shmctl( Message_Queue[ i ].shm_id, IPC_RMID, NULL );
				Message_Queue[ i ].shm_id = -1;
			}
	}

	/* Finally delete the master key (if its ID is valid, i.e. non-negative) */

	if ( Key_ID >= 0 )
		detach_shm( Key, &Key_ID );

	if ( must_reset )
		seteuid( getuid( ) );
}


/*------------------------------------------------------------------------*/
/* If fsc2 crashes while running an experiment shared memory segments may */
/* remain undeleted. To get rid of them we now check all shared segments  */
/* for the ones that belong to the user `fsc2' and start with the magic   */
/* 'fsc2'. They are obviously debris from a crash and have to be deleted  */
/* to avoid using up all segments after some time. Since the segments     */
/* belong to the user `fsc2' this routine must be run with the effective  */
/* ID of fsc2.                                                            */
/* This routine is more or less a copy of the code from the ipcs utility, */
/* hopefully it will continue to work with newer versions of Linux (it    */
/* seems to work with 2.0, 2.2 and 2.4 kernels)                           */
/*------------------------------------------------------------------------*/

/* These defines seem to be needed for older Linux versions, i.e. 2.0.36 */

#if ( ! defined ( SHM_STAT ) )
#define SHM_STAT 13
#endif

#if ( ! defined ( SHM_INFO ) )
#define SHM_INFO 14
#endif


void delete_stale_shms( void )
{
	int max_id, id, shm_id;
    struct shmid_ds shm_seg;
	int euid = geteuid( );
	void *buf;


	/* Get the current maximum shared memory segment id */

    max_id = shmctl( 0, SHM_INFO, ( struct shmid_ds * ) &shm_seg );

	/* Run through all of the possible IDs. If they belong to fsc2 and start
	   with the magic 'fsc2' they are deleted. */

    for ( id = 0; id <= max_id; id++ )
	{
        shm_id = shmctl( id, SHM_STAT, &shm_seg );
        if ( shm_id  < 0 ) 
            continue;

		if ( shm_seg.shm_perm.uid == euid )     /* segment belongs to fsc2 ? */
		{
			if ( ( buf = shmat( shm_id, NULL, 0 ) ) == ( void * ) - 1 )
				continue;                          /* can't attach... */

			if ( ! strncmp( ( char * ) buf, "fsc2", 4 ) )
			{
				shmdt( buf );

				if ( shm_seg.shm_nattch != 0 )          /* attach count != 0 */
					fprintf( stderr, "Stale shared memory segment has attach "
							 "count of %d.\nPossibly one of fsc2s processes "
							 "survived...\n", shm_seg.shm_nattch );
				else
					shmctl( shm_id, IPC_RMID, NULL );
			}
			else                                        /* wrong magic */
				shmdt( buf );
		}
	}
}


/*------------------------------------------------------------*/
/* Function creates a (System V) semaphore (with one set) and */
/* initializes to 'val'. It returns either the ID number of   */
/* the new semaphore or -1 on error.                          */
/*------------------------------------------------------------*/

int sema_create( int val )
{
	int sema_id;
	union semun sema_arg;
	bool must_reset = UNSET;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}

	if ( ( sema_id = semget( IPC_PRIVATE, 1,
							 IPC_CREAT | IPC_EXCL | SEM_R | SEM_A ) ) < 0 )
	{
		if ( must_reset )
			seteuid( getuid( ) );
		return -1;
	}

	sema_arg.val = val;
	if ( ( semctl( sema_id, 0, SETVAL, sema_arg ) ) < 0 )
	{
		semctl( sema_id, 0, IPC_RMID, sema_arg );
		if ( must_reset )
			seteuid( getuid( ) );
		return -1;
	}

	if ( must_reset )
		seteuid( getuid( ) );
	return sema_id;
}


/*--------------------------------------------------------------*/
/* Function deletes the semaphore with the ID number 'sema_id'. */
/* It returns 0 on success and -1 on error.                     */
/*--------------------------------------------------------------*/

int sema_destroy( int sema_id )
{
	bool must_reset = UNSET;
	union semun sema_arg;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}

	if ( semctl( sema_id, 0, IPC_RMID, sema_arg ) < 0 )
	{
		if ( must_reset )
			seteuid( getuid( ) );
		return -1;
	}

	if ( must_reset )
		seteuid( getuid( ) );
	return 0;
}


/*-------------------------------------------------------------------------*/
/* Function waits until the value of the semapore with ID number 'sema_id' */
/* becomes greater than 0 (and then decrement the semaphore by 1 ). It     */
/* returns 0 or -1 on error. The function will not return but continue to  */
/* wait when signals are received.                                         */
/*-------------------------------------------------------------------------*/

int sema_wait( int sema_id )
{
	struct sembuf sema_buf = { 0, -1, SEM_UNDO };
	bool must_reset = UNSET;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}

	while ( semop( sema_id, &sema_buf, 1 ) < 0 )
		if ( errno != EINTR )
		{
			if ( must_reset )
				seteuid( getuid( ) );
			return -1;
		}

	if ( must_reset )
		seteuid( getuid( ) );
	return 0;
}


/*--------------------------------------------------------------------*/
/* Function increments the semaphore with ID number 'sema_id' by one. */
/* It returns 0 on success and -1 on errors.                          */
/*--------------------------------------------------------------------*/

int sema_post( int sema_id )
{
	struct sembuf sema_buf = { 0, 1, SEM_UNDO };
	bool must_reset = UNSET;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}

	while ( semop( sema_id, &sema_buf, 1 ) < 0 )
		if ( errno != EINTR )
		{
			if ( must_reset )
				seteuid( getuid( ) );
			return -1;
		}

	if ( must_reset )
		seteuid( getuid( ) );
	return 0;
}
