/*
  $Id$
*/


#include <dlfcn.h>
#include "fsc2.h"


/* The string passed to the function is already allocated to the program
   and has to be deallocated here to avoid a memory leak! */

void device_add( char *dev_name )
{
	Device_Name *dl;


	string_to_lower( dev_name );

	/* First test if the device is in the list of device names */

	for ( dl = Device_Name_List; dl != NULL; dl = dl->next )
		if ( ! strcmp( dl->name, dev_name ) )
			break;

	if ( dl == NULL )
	{
		eprint( FATAL, "%s:%ld: Device `%s' not found in device name data "
				"base.\n", Fname, Lc, dev_name );
		T_free( dev_name );
		THROW( EXCEPTION );
	}

	/* Now append the device to the end of the device list */

	TRY
	{
		device_append_to_list( dev_name );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( dev_name );
		PASSTHROU( );
	}

	T_free( dev_name );
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
	cd->next = NULL;
}


void delete_devices( void )
{
	Device *cd, *cdn;

	if ( Device_List == NULL )
		return;

	for ( cd = Device_List; cd != NULL; cd = cdn )
	{
		/* If there is a driver run the exit hooks and unload it */

		if ( cd->is_loaded )
		{
			if ( cd->driver.is_exit_hook )
				cd->driver.exit_hook( );
			dlclose( cd->driver.handle );
		}

		if ( cd->name != NULL )
			T_free( cd->name );

		cdn = cd->next;
		T_free( cd );
	}

	Device_List = NULL;
}


void delete_device_name_list( void )
{
	Device_Name *cd, *cdn;

	for ( cd = Device_Name_List; cd != NULL; cd = cdn )
	{
		if ( cd->name != NULL )
			T_free( cd->name );
		cdn = cd->next;
		T_free( cd );
	}

	Device_Name_List = NULL;
}
