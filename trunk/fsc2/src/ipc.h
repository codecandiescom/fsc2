/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
