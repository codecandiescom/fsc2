/*
  $Id$
*/


#include "fsc2.h"


void device_clear_list( void )
{
	Device *cd, *cdn;


	if ( Dev_List == NULL )
		return;

	for ( cd = Dev_List; cd != NULL; cd = cdn )
	{
		free( cd->name );
		cdn = cd->next;
		free( cd );
	}

	Dev_List = NULL;
}


void device_add( char *name )
{
	Device *new_device;

	new_device = T_malloc( sizeof( Device ) );
	new_device->name = get_string_copy( name );
	new_device->next = Dev_List;
	Dev_List = new_device;
}

