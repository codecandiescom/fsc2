/*
  $Id$
*/

#if ! defined DEVICE_HEADER
#define DEVICE_HEADER

#include "fsc2.h"


typedef struct Dev_ {
	char *name;
	bool is_loaded;
	struct Dev_ *next;
} Device;



void device_lib_add( char *name );
void device_add( char *name );
void clear_device_list( void );

void device_list_parse( void );

#endif  /* ! DEVICE_HEADER */
