/*
  $Id$
*/

#if ! defined COMM_HEADER
#define COMM_HEADER

#include "fsc2.h"


enum {
	C_EPRINT = 0,
	C_SHOW_MESSAGE,
	C_SHOW_ALERT,
	C_SHOW_CHOICES,
	C_INIT_GRAPHICS,
	C_SHOW_FSELECTOR,
	C_STR,
	C_INT,
	C_LONG,
	C_FLOAT,
	C_DOUBLE
};



typedef struct {
	int type;
	union {
		size_t len;    /* length of following string (without '\0') */
		int int_data;
		long long_data;
		float float_data;
		double double_data;
		size_t str_len[ 4 ];
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


# define QUEUE_SIZE 129     /* max number of shared memory segments + 1 */
                            /* (as determined by 'ipcs -ul') */

bool setup_comm( void );
void end_comm( void );
long reader( void *ret );
void writer( int type, ... );



#endif  /* ! COMM_HEADER */
