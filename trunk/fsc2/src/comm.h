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
	C_DATA,
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


typedef struct Data_Buffer_ {
	int type;
	long nx;
	long ny;
	union {
		long lval;
		double dval;
		long *lpnt;
		double *dpnt;
	} data;
	size_t len;
	int overdraw_flag;

	struct Data_Buffer_ *next;
} Data_Buffer;



long reader( void *ret );
void writer( int type, ... );
void delete_data_buffer( Data_Buffer *buf );
void delete_all_data_buffers( void );



#endif  /* ! COMM_HEADER */
