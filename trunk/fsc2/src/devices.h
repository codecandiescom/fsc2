/*
  $Id$
*/

#if ! defined DEVICE_HEADER
#define DEVICE_HEADER

#include "fsc2.h"


typedef struct Dev_ {
	char *name;
	Lib_Struct driver;
	bool is_loaded;
	struct Dev_ *next;
	struct Dev_ *prev;
	int count;
} Device;


typedef struct DN_ {
	char *name;
	struct DN_ *next;
} Device_Name;




void device_add( char *dev_name );
void device_append_to_list( const char *dev_name );
void delete_devices( void );

void delete_device_name_list( void );

/* from `device_list_lexer.flex' */

bool device_list_parse( void );


void load_functions( Device *dev );

#endif  /* ! DEVICE_HEADER */
