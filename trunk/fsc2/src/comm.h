/*
  $Id$
*/

#if ! defined COMM_HEADER
#define COMM_HEADER

#include "fsc2.h"


enum {
	C_EPRINT = 0,
};



typedef struct {
	int type;
	union {
		size_t eprint_len;    /* length of following string (without '\0') */
	} data;
} CS;



void reader( void );
void writer( int type, ... );



#endif  /* ! COMM_HEADER */
