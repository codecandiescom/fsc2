/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2.h"

#if defined PATH_MAX
static long pathmax = PATH_MAX;
#else
static long pathmax = 0;
#endif

#define PATH_MAX_GUESS 4095      /* guess for maximum file name length... */


/*--------------------------------------------------------------------------*/
/* This function is called for each device found in the DEVICES section of  */
/* the EDL file. It first checks if the device is listed in the device data */
/* base file "Devices". Then it appends a new structure for the device to   */
/* the end of the linked list.                                              */
/*--------------------------------------------------------------------------*/

void device_add( const char *name )
{
	Device_Name *dl;
	static char *dev_name;
	static char *real_name;         /* static because otherwise possibly
									   clobbered by TRY (longjmp) */
	const char *search_name;
	char *lib_name;
	struct stat buf;
	int length;


	dev_name = T_strdup( name );
	string_to_lower( dev_name );
	real_name = NULL;

	/* First we've got to check if the name refers to a device driver that is
	   a symbolic link to the `real' device. If so get the real name by
	   following the link. This way it's possible to have more convenient,
	   locally adjustable names for the devices. Therefor, we assemble the
	   name of the modules corresponding to the device name */

	lib_name = get_string( "%s%s%s.so", libdir, slash( libdir ), dev_name );

	/* Try to access the module (also allow the name to be defined via
	   LD_LIBRARY_PATH) - don't follow links yet */

	if ( lstat( lib_name, &buf ) < 0 &&
		 lstat( strip_path( lib_name ), &buf ) < 0 )
	{
		eprint( FATAL, UNSET, "Can't find or access module `%s.so'.\n",
				dev_name );
		T_free( lib_name );
		T_free( dev_name );
		THROW( EXCEPTION );
	}

	/* If the module is a symbolic link try to figure out the name of the file
	   the symbolic link points to and store it in `real_name' */

	if ( S_ISLNK( buf.st_mode ) )
	{
		/* We need memory for the name of the file the link points to, but
		   we may not know the maximum length of a file name... */

		if ( pathmax == 0 )
		{
			if ( ( pathmax = pathconf( "/", _PC_PATH_MAX ) ) < 0 )
			{
				if ( errno == 0 )
					pathmax = PATH_MAX_GUESS;
				else
				{
					eprint( FATAL, UNSET, "%s:%d: This operating system "
							"sucks!\n", __FILE__, __LINE__ );
					T_free( lib_name );
					T_free( dev_name );
					THROW( EXCEPTION );
				}
			}
		}

		real_name = T_malloc( pathmax + 1 );
		if ( ( length = readlink( lib_name, real_name, pathmax ) ) < 0 )
		{
			eprint( FATAL, UNSET, "Can't follow symbolic link for `%s'.\n",
					lib_name );
			T_free( lib_name );
			T_free( dev_name );
			T_free( real_name );
			THROW( EXCEPTION );
		}

		real_name[ length ] = '\0';

		/* Now check that module has the extension ".so" and strip it off */

		if ( strcmp( real_name + length - 3, ".so" ) )
		{
			eprint( FATAL, UNSET, "Module `%s' used for device `%s' hasn't "
					"extension \".so\".\n", real_name, dev_name );
			T_free( lib_name );
			T_free( dev_name );
			T_free( real_name );
			THROW( EXCEPTION );
		}

		*( real_name + length - 3 ) = '\0';
	}

	T_free( lib_name );

	/* Now test if the device is in the list of device names, either with the
	   real name or the alternate name - because `real_name' might start with
	   a path but the names in `Devices' are just names without a path
	   compare only after stripping off the path */

	search_name = strip_path( real_name );

	for ( dl = Device_Name_List; dl != NULL; dl = dl->next )
		if ( ! strcmp( dl->name, dev_name ) ||
			 ! strcmp( dl->name, search_name ) )
			break;

	if ( dl == NULL )
	{
		eprint( FATAL, SET, "Device `%s' not found in device name data "
				"base.\n", dev_name );
		T_free( real_name );
		T_free( dev_name );
		THROW( EXCEPTION );
	}

	/* Now append the device to the end of the device list */

	TRY
	{
		device_append_to_list( real_name != NULL ? real_name : dev_name );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( real_name );
		T_free( dev_name );
		PASSTHROU( );
	}

	T_free( real_name );
	T_free( dev_name );
}


/*-----------------------------------------------------*/
/* Function creates a new Device structure and appends */
/* it to the list of devices.                          */
/*-----------------------------------------------------*/

void device_append_to_list( const char *dev_name )
{
	Device *cd;


	/* Let's first run through the device list and check that the device isn't
	   already in it and (if this turns out ok) append a new new Device
	   structure to the list of devices */

	if ( Device_List != NULL )
	{
		for ( cd = Device_List; cd->next != NULL; cd = cd->next )
			if ( ! strcmp( cd->name, dev_name ) )
			{
				eprint( FATAL, SET, "Device `%s' is already listed in the "
						"DEVICES section (possibly using an alternate "
						"name).\n", dev_name );
				THROW( EXCEPTION );
			}

		cd->next = T_malloc( sizeof( Device ) );
		cd->next->prev = cd;
		cd = cd->next;
	}
	else
	{
		Device_List = cd = T_malloc( sizeof( Device ) );
		cd->prev = NULL;
	}

	cd->name = T_strdup( dev_name );
	cd->is_loaded = UNSET;
	cd->next = NULL;
	cd->count = 1;
}


/*---------------------------------------------------------------------*/
/* Function deletes the whole list of device structures after running  */
/* the corresponding exit hook functions unloading the modules.        */
/*---------------------------------------------------------------------*/

void delete_devices( void )
{
	Device *cd, *cdp;


	if ( Device_List == NULL )         /* list is empty (does not exist) ? */
		return;

	/* Get last element of list - always delete last entry first */

	for( cd = Device_List; cd->next != NULL; cd = cd->next )
		;

	for ( ; cd != NULL; cd = cdp )
	{
		if ( cd->is_loaded )
			unload_device( cd );         /* run exit hooks and unload module */
		T_free( cd->name );
		cdp = cd->prev;
		T_free( cd );
	}

	Device_List = NULL;
}


/*-------------------------------------------------------------*/
/* Function deletes the list of known devices as read from the */
/* device data base file "Devices".                            */
/*-------------------------------------------------------------*/

void delete_device_name_list( void )
{
	Device_Name *cd, *cdn;

	for ( cd = Device_Name_List; cd != NULL; cd = cdn )
	{
		T_free( cd->name );
		cdn = cd->next;
		T_free( cd );
	}

	Device_Name_List = NULL;
}
