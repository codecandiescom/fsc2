/*
  $Id$
*/


#include "fsc2.h"


void device_lib_add( char *name )
{
	char *lib_name;

	lib_name = get_string_copy( name );
	string_to_lower( lib_name );
	
	load_functions( lib_name );
}



void device_add( char *name )
{
	char *dev_name;
	Device *cd;

	dev_name = get_string_copy( name );
	string_to_lower( dev_name );

	for ( cd = Dev_List; cd != NULL; cd = cd->next )
		if ( ! strcmp( cd->name, dev_name ) )
			break;

	if ( cd == NULL )
		THROW( DEVICES_EXCEPTION );

	TRY
	{
		device_lib_add( name );
		TRY_SUCCESS;
	}
	else
		THROW( DEVICES_EXCEPTION );

	cd->is_loaded = SET;
}


void clear_device_list( void )
{
	Device *cd, *cdn;

	for ( cd = Dev_List; cd != NULL; cd = cdn )
	{
		if ( cd->name != NULL )
			free( cd->name );
		cdn = cd->next;
		free( cd );
	}

	Dev_List = NULL;
}
