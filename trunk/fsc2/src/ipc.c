/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/

#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/sem.h>


#if defined ( _SEM_SEMUN_UNDEFINED ) && ( _SEM_SEMUN_UNDEFINED == 1 )
union semun {
	  int val;                    /* value for SETVAL */
	  struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
	  unsigned short int *array;  /* array for GETALL, SETALL */
	  struct seminfo *__buf;      /* buffer for IPC_INFO */
};
#endif


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


	raise_permissions( );

	while ( ( *shm_id = shmget( IPC_PRIVATE, len + 4,
								IPC_CREAT | SHM_R | SHM_A ) ) < 0 )
	{
		if ( errno == ENOSPC || errno == ENOMEM ) /* wait for 10 ms */
			usleep( 10000 );
		else                                      /* non-recoverable failure */
		{
#if defined  ( DEBUG )
			eprint( FATAL, UNSET, "Internal error at %s:%u, shmget() failed "
					"with error number %d.\n*** PLEASE SEND A BUG REPORT "
					"CITING THESE LINES *** Thank you.\n",
					__FILE__, __LINE__, errno );
#endif
			lower_permissions( );
			return ( void * ) -1;
		}
	}

	/* Attach to the shared memory segment - if this should fail (improbable)
	   return -1 and let the calling routine handle the mess... */

	if ( ( buf = shmat( *shm_id, NULL, 0 ) ) == ( void * ) - 1 )
	{
		lower_permissions( );
		return ( void * ) -1;
	}

	/* Now write 'magic string' into the start of the shared memory to make
	   it easier to identify it later */

	memcpy( buf, "fsc2", 4 );                         /* magic id */

	lower_permissions( );
	return ( void * ) ( ( char * ) buf + 4 );
}


/*---------------------------------------------------------------*/
/* Function tries to attach to the shared memory associated with */
/* 'key'. On success it returns a pointer to the memory region   */
/* (skipping the 'magic string' "fsc2"), on error it returns -1. */
/*---------------------------------------------------------------*/

void *attach_shm( int key )
{
	void *buf;


	raise_permissions( );

	if ( ( buf = shmat( key, NULL, SHM_RDONLY ) ) == ( void * ) - 1 )
	{
#if defined ( DEBUG )
		eprint( FATAL, UNSET, "Internal error at %s:%u, shmat() with "
				"SHM_RDONLY failed for key %d with error number %d.\n"
				"*** PLEASE SEND A BUG REPORT CITING THESE LINES *** "
				"Thank you.\n", __FILE__, __LINE__, key, errno );
#endif
		shmctl( key, IPC_RMID, NULL );       /* delete the segment */
		lower_permissions( );
		return ( void * ) -1;
	}

	lower_permissions( );
	return ( void * ) ( ( char * ) buf + 4 );
}


/*---------------------------------------------------------------------*/
/* Function detaches from a shared memory segment and, if a valid key  */
/* (i.e. a non-negative key) is passed to the function it also deletes */
/* the shared memory region.                                           */
/*---------------------------------------------------------------------*/

void detach_shm( void *buf, int *key )
{
	raise_permissions( );

	shmdt( ( void * ) ( ( char * ) buf - 4 ) );
	if ( key != NULL )
	{
		shmctl( *key, IPC_RMID, NULL );
		*key = -1;
	}

	lower_permissions( );
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


	if ( Comm.MQ_ID < 0 )
		return;

	raise_permissions( );

	/* If message queue exists check that all memory segments indexed in it
	   are deleted */

	for ( i = 0; i < QUEUE_SIZE; i++ )
		if ( Comm.MQ->slot[ i ].shm_id >= 0 )
			shmctl( Comm.MQ->slot[ i ].shm_id, IPC_RMID, NULL );

	/* Finally delete the master key (if its ID is valid, i.e. non-negative) */

	detach_shm( Comm.MQ, &Comm.MQ_ID );
	shmctl( Comm.MQ_ID, IPC_RMID, NULL );
	Comm.MQ_ID = -1;

	lower_permissions( );
}


/*------------------------------------------------------------------------*/
/* If fsc2 crashes while running an experiment shared memory segments may */
/* remain undeleted. To get rid of them we now check all shared segments  */
/* for the ones that belong to the user `fsc2' and start with the magic   */
/* 'fsc2'. They are obviously debris from a crash and have to be deleted  */
/* to avoid using up all segments after some time. Since the segments     */
/* belong to the user `fsc2' this routine must be run with the effective  */
/* UID and GID of fsc2.                                                   */
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
	void *buf;


	raise_permissions( );

	/* Get the current maximum shared memory segment id */

    max_id = shmctl( 0, SHM_INFO, ( struct shmid_ds * ) &shm_seg );

	/* Run through all of the possible IDs. If they belong to fsc2 and start
	   with the magic 'fsc2' they are deleted. */

    for ( id = 0; id <= max_id; id++ )
	{
        shm_id = shmctl( id, SHM_STAT, &shm_seg );
        if ( shm_id  < 0 )
            continue;

		/* segment belongs to fsc2 ? */

		if ( shm_seg.shm_perm.uid == Internals.EUID )
		{
			if ( ( buf = shmat( shm_id, NULL, 0 ) ) == ( void * ) - 1 )
				continue;                          /* can't attach... */

			if ( ! strncmp( ( char * ) buf, "fsc2", 4 ) )
			{
				shmdt( buf );

				/* Take care: the shm_nattch field in the shmid_ds structure
				   has different types in different Linux versions, in older
				   ones it is an unsigned short, nowadays an unsigned long.
				   To get it right for all versions we simply cast is to the
				   larger type (this better should be handled by a configure
				   script, but that's still on the TODO list...). */

				if ( shm_seg.shm_nattch != 0 )          /* attach count != 0 */
					fprintf( stderr, "Stale shared memory segment has attach "
							 "count of %lu.\nPossibly one of fsc2's processes "
							 "survived...\n",
							 ( unsigned long ) shm_seg.shm_nattch );
				else
					shmctl( shm_id, IPC_RMID, NULL );
			}
			else                                        /* wrong magic */
				shmdt( buf );
		}
	}

	lower_permissions( );
}


/*----------------------------------------------------------------------*/
/* Function creates a (System V) semaphore with one set and initializes */
/* it to the value supplied to the function. It returns either the ID   */
/* of the new semaphore or -1 on error.                                 */
/*----------------------------------------------------------------------*/

int sema_create( int size )
{
	int sema_id;
	union semun sema_arg;


	raise_permissions( );

	if ( ( sema_id = semget( IPC_PRIVATE, 1,
							 IPC_CREAT | IPC_EXCL | SEM_R | SEM_A ) ) < 0 )
	{
		lower_permissions( );
		return -1;
	}

	sema_arg.val = size;
	if ( ( semctl( sema_id, 0, SETVAL, sema_arg ) ) < 0 )
	{
		semctl( sema_id, 0, IPC_RMID, sema_arg );
		lower_permissions( );
		return -1;
	}

	lower_permissions( );
	return sema_id;
}


/*--------------------------------------------------------------*/
/* Function deletes the semaphore with the ID number 'sema_id'. */
/* It returns 0 on success and -1 on error.                     */
/*--------------------------------------------------------------*/

int sema_destroy( int sema_id )
{
	union semun sema_arg;


	raise_permissions( );

	if ( semctl( sema_id, 0, IPC_RMID, sema_arg ) < 0 )
	{
		lower_permissions( );
		return -1;
	}

	lower_permissions( );
	return 0;
}


/*-------------------------------------------------------------------------*/
/* Function waits until the value of the semapore with ID number 'sema_id' */
/* becomes greater than 0 (and then decrement the semaphore by 1). It      */
/* returns 0 or -1 on error. The function will not return but continue to  */
/* wait when signals are received.                                         */
/*-------------------------------------------------------------------------*/

int sema_wait( int sema_id )
{
	struct sembuf wait_flags = { 0, -1, 0 };



	/* In the child process check that it hasn't become a zombie before doing
	   the wait on the semaphore and commit controlled suicide if it is. */

	if ( Internals.I_am == CHILD && getppid( ) == 1 )
		kill( getpid( ), SIGTERM );

	raise_permissions( );

	while ( semop( sema_id, &wait_flags, 1 ) < 0 )
		if ( errno != EINTR )
		{
			lower_permissions( );
			return -1;
		}

	lower_permissions( );
	return 0;
}


/*--------------------------------------------------------------------*/
/* Function increments the semaphore with ID number 'sema_id' by one. */
/* It returns 0 on success and -1 on errors. The function will not    */
/* return but continue to wait when signals are received.             */
/*--------------------------------------------------------------------*/

int sema_post( int sema_id )
{
	struct sembuf post = { 0, 1, 0 };


	raise_permissions( );

	while ( semop( sema_id, &post, 1 ) < 0 )
		if ( errno != EINTR )
		{
			lower_permissions( );
			return -1;
		}

	lower_permissions( );
	return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
