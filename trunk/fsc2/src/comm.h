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

#if ! defined COMM_HEADER
#define COMM_HEADER

#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/shm.h>


/* Maximum number of shared memory segments - I didn't find a better way to
   determine this number yet. In principle, it should be taken from one of the
   include files, but alas, for the current version of Linux the header files
   seem to be broken in this respect... */

#if ! defined SHMMNI
#define SHMMNI 128
#endif

/* Maximum number of shared memory segments plus some safety margin - the
   queue has always to be larger than the maximum number of shared segments,
   so make sure SHMMNI is correct ! */

#define QUEUE_SIZE ( SHMMNI + 8 )



enum {
	C_EPRINT = 0,
	C_SHOW_MESSAGE,
	C_SHOW_ALERT,
	C_SHOW_CHOICES,
	C_SHOW_FSELECTOR,
	C_PROG,
	C_OUTPUT,
	C_INPUT,
	C_STR,
	C_INT,
	C_LONG,
	C_FLOAT,
	C_DOUBLE,
	C_LAYOUT,
	C_LAYOUT_REPLY,
	C_BCREATE,
	C_BCREATE_REPLY,
	C_BDELETE,
	C_BDELETE_REPLY,
	C_BSTATE,
	C_BSTATE_REPLY,
	C_SCREATE,
	C_SCREATE_REPLY,
	C_SDELETE,
	C_SDELETE_REPLY,
	C_SSTATE,
	C_SSTATE_REPLY,
	C_ICREATE,
	C_ICREATE_REPLY,
	C_IDELETE,
	C_IDELETE_REPLY,
	C_ISTATE,
	C_ISTATE_REPLY,
	C_ODELETE,
	C_ODELETE_REPLY
};


enum {
	D_CLEAR_CURVE   = -1,
	D_CHANGE_SCALE  = -2,
	D_CHANGE_LABEL  = -3,
	D_CHANGE_POINTS = -4
};


typedef struct {
	int type;
	union {
		ptrdiff_t len;
		int int_data;
		long long_data;
		float float_data;
		double double_data;
		long str_len[ 4 ];
	} data;
} CommStruct;



typedef struct {
	int shm_id;
	int type;
} KEY;

enum {
	DATA,
	REQUEST
};


void setup_comm( void );
void end_comm( void );
int new_data_callback( XEvent *a, void *b );
long reader( void *ret );
void writer( int type, ... );


#endif  /* ! COMM_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
