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
	C_INIT_GRAPHICS,
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
		struct {
			size_t str1_len;
			size_t str2_len;
		} init_graphics;
	} data;
} CS;



long reader( void *ret );
void writer( int type, ... );



#endif  /* ! COMM_HEADER */
