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
	{
		Device_List = cd = T_malloc( sizeof( Device ) );
		cd->prev = NULL;
	}
	else
	{
		for ( cd = Device_List; cd->next != NULL; cd = cd->next )
			;
		cd->next = T_malloc( sizeof( Device ) );
		cd->next->prev = cd;
		cd = cd->next;
	}

	cd->name = get_string_copy( dev_name );
	cd->is_loaded = UNSET;
	cd->next = NULL;
}


void delete_devices( void )
{
	Device *cd, *cdp;


	if ( Device_List == NULL )
		return;

	/* Get last element of list */

	for( cd = Device_List; cd->next != NULL; cd = cd->next )
		;

	for ( ; cd != NULL; cd = cdp )
	{
		/* If there is a driver run the exit hooks and unload it */

		cdp = cd->prev;
		if ( cd->is_loaded )
		{
			if ( ! exit_hooks_are_run && cd->driver.is_exit_hook )
			{
				TRY                      /* catch exceptions from the exit   */
				{                        /* hooks, we've got to run them all */
					cd->driver.exit_hook( );
					TRY_SUCCESS;
				}
			}

			dlclose( cd->driver.handle );
		}

		if ( cd->name != NULL )
			T_free( cd->name );

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
