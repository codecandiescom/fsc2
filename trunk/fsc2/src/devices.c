/*
  $Id$
*/


#include <dlfcn.h>
#include "fsc2.h"



void device_add( char *name )
{
	char *dev_name;
	Device_Name *dl;


	dev_name = get_string_copy( name );
	string_to_lower( dev_name );

	/* First test if the device is in the list of device names */

	for ( dl = Device_Name_List; dl != NULL; dl = dl->next )
		if ( ! strcmp( dl->name, dev_name ) )
			break;

	if ( dl == NULL )
	{
		eprint( FATAL, "%s:%ld: Device `%s' not found in device name list.\n",
				Fname, Lc, name );
		free( dev_name );
		THROW( DEVICES_EXCEPTION );
	}

	/* Now append the device to the end of the device list */

	TRY
	{
		device_append_to_list( dev_name );
		TRY_SUCCESS;
	}
	CATCH( OUT_OF_MEMORY_EXCEPTION )
	{
		free( dev_name );
		PASSTHROU( );
	}

	free( dev_name );
}


void device_append_to_list( const char *dev_name )
{
	Device *cd;


	/* Now create a new Device structure and append it to the list of
	   devices */

	if ( Device_List == NULL )
		Device_List = cd = T_malloc( sizeof( Device ) );
	else
	{
		for ( cd = Device_List; cd->next != NULL; cd = cd->next )
			;
		cd->next = T_malloc( sizeof( Device ) );
		cd = cd->next;
	}

	cd->name = get_string_copy( dev_name );
	cd->is_loaded = UNSET;
}


void delete_devices( void )
{
	Device *cd, *cdn;

	if ( Device_List == NULL )
		return;

	for ( cd = Device_List; cd != NULL; cd = cdn )
	{
		/* If there is a driver run the exit hooks and unload it */

		if ( cd->driver.handle != NULL )
		{
			if ( cd->driver.is_exit_hook )
				cd->driver.lib_exit( );
			dlclose( cd->driver.handle );
		}

		if ( cd->name != NULL )
			free( cd->name );

		cdn = cd->next;
		free( cd );
	}

	Device_List = NULL;
}


void delete_device_name_list( void )
{
	Device_Name *cd, *cdn;

	for ( cd = Device_Name_List; cd != NULL; cd = cdn )
	{
		if ( cd->name != NULL )
			free( cd->name );
		cdn = cd->next;
		free( cd );
	}
}
