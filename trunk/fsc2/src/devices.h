/*
  $Id$
*/

#if ! defined DEVICE_HEADER
#define DEVICE_HEADER

#include "fsc2.h"


typedef struct Dev_ {
	char *name;
	struct Dev_ *next;
} Device;



void device_clear_list( void );
void device_add( char *name );


#endif  /* ! DEVICE_HEADER */
