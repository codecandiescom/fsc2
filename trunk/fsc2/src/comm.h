/*
  $Id$
*/

#if ! defined COMM_HEADER
#define COMM_HEADER

#include "fsc2.h"
#include <sys/param.h>

#define SHMMNI 128


enum {
	C_EPRINT = 0,
	C_SHOW_MESSAGE,
	C_SHOW_ALERT,
	C_SHOW_CHOICES,
	C_SHOW_FSELECTOR,
	C_PROG,
	C_OUTPUT,
	C_INPUT,
	C_CLEAR_CURVE,
	C_STR,
	C_INT,
	C_LONG,
	C_FLOAT,
	C_DOUBLE
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
} CS;



typedef struct {
	int shm_id;
	int type;
} KEY;

enum {
	DATA,
	REQUEST
};


# define QUEUE_SIZE ( SHMMNI + 1 )   /* maximum number of shared memory
										segments + 1 */

bool setup_comm( void );
void end_comm( void );
long reader( void *ret );
void writer( int type, ... );


#endif  /* ! COMM_HEADER */
