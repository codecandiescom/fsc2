/*
  $Id$
*/

#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/sem.h>



/*-------------------------------------------------------------------*/
/* Routine tries to get a shared memory segment - if this fails and  */
/* the reason is that no segments or no memory for segments are left */
/* it waits for some time hoping for the parent process to remove    */
/* other segments in the mean time.                                  */
/*-------------------------------------------------------------------*/

void *get_shm( int *shm_id, long len )
{
	void *buf;
	bool must_reset = UNSET;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}

	while ( ( *shm_id = shmget( IPC_PRIVATE, len,
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

	if ( must_reset )
		seteuid( getuid( ) );

	return buf;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

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

	return buf;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

void detach_shm( int key, void *buf )
{
	bool must_reset = UNSET;


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}
	
	shmdt( buf );
	if ( key >= 0 )
		shmctl( key, IPC_RMID, NULL );

	if ( must_reset )
		seteuid( getuid( ) );
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
		semctl( sema_id, 0, IPC_RMID );
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


	if ( geteuid( ) != EUID )
	{
		seteuid( EUID );
		must_reset = SET;
	}

	if ( semctl( sema_id, 0, IPC_RMID ) < 0 )
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
