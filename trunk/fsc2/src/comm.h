/*
  $Id$
*/

#if ! defined COMM_HEADER
#define COMM_HEADER

#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/shm.h>


/* Maximum number of shared memory segments - I didn't find a better way to
   determine this number yet. In principal, it should be taken from one of the
   include files, but alas, for the current version of Linux the header files
   seem to be broken in this respect... */

#if ! defined SHMMNI
#define SHMMNI 128
#endif

/* Maximum number of shared memory segments plus some safety margin - the
   queue has always to be larger than the maximum number of shared segments,
   so make sure SHMMNI is correct ! */

# define QUEUE_SIZE ( SHMMNI + 8 ) 



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
};


enum {
	D_CLEAR_CURVE = -1
};


typedef struct {
	int type;
	union {
		long len;           /* length of following string (without '\0') */
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


bool setup_comm( void );
void end_comm( void );
int new_data_callback( XEvent *a, void *b );
long reader( void *ret );
void writer( int type, ... );


#endif  /* ! COMM_HEADER */
