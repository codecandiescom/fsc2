/*
  $Id$
*/

#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/sem.h>



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
