/*
 *  Copyright (C) 1999-2016 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2.h"


static char * search_for_lib( const char * dev_name );


/*--------------------------------------------------------------------------*
 * This function is called for each device found in the DEVICES section of
 * the EDL file. It first checks if the device is listed in the device data
 * base file "Devices". Then it appends a new structure for the device to
 * the end of the linked list.
 *--------------------------------------------------------------------------*/

void
device_add( const char * name )
{
    char * volatile dev_name = string_to_lower( T_strdup( name ) );
    char * volatile real_name = NULL;

    /* Try to locate the library for he device and look for alternate
       names for device (as can be used via symbolic links) */

    TRY
    {
        real_name = search_for_lib( dev_name );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( dev_name );
        RETHROW;
    }

    /* Now test if the device is in the list of device names, either with the
       real name or the alternate name (because 'real_name' might start with
       a path but the names in 'Devices' are just names without a path
       compare only after stripping off the path) */

    const char * search_name = real_name ? strip_path( real_name ) : NULL;

    Device_Name_T * dl;
    for ( dl = EDL.Device_Name_List; dl != NULL; dl = dl->next )
        if (    ! strcmp( dl->name, dev_name )
             || ( search_name && ! strcmp( dl->name, search_name ) ) )
            break;

    if ( dl == NULL )
    {
        if ( search_name == NULL )
            print( FATAL, "Device '%s' not found in device name data base.\n",
                   dev_name );
        else
            print( FATAL, "Device '%s' (or its alias '%s') not found in "
                   "device name data base.\n", dev_name, search_name );

        T_free( real_name );
        T_free( dev_name );
        THROW( EXCEPTION );
    }

    /* Make sure the device isn't already loaded */

    for ( Device_T * cd = EDL.Device_List; cd != NULL; cd = cd->next )
        if (    ! strcmp( cd->name, dev_name )
             || ( real_name && ! strcmp( cd->name, real_name ) ) )
        {
            if ( search_name == NULL )
                print( FATAL, "Device '%s' is listed twice in the DEVICES "
                       "section.\n", dev_name);
            print( FATAL, "Device '%s' is listed twice in the DEVICES "
                   "section, possibly also using the name '%s'.\n",
                   dev_name, search_name );
            T_free( real_name );
            T_free( dev_name );
            THROW( EXCEPTION );
        }

    /* Now append the device to the end of the device list */

    TRY
    {
        device_append_to_list( real_name ? real_name : dev_name );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( real_name );
        T_free( dev_name );
        RETHROW;
    }

    T_free( real_name );
    T_free( dev_name );
}


/*-----------------------------------------------------*
 * Function tries to determine if the library for the device
 * exists and if it has an alias name.
 *-----------------------------------------------------*/

static
char *
search_for_lib( const char * dev_name )
{
    struct stat buf;

    /* In case the '-local_exec' option was used just look for device
       modules in the source directory of the modules. Otherweise we
       first got to check if the name refers to a device driver that
       is a symbolic link to the 'real' device module. If so get the
       real name by following the link. This way it's possible to have
       more convenient, locally adjustable names for the devices. As
       usual, we first check paths defined by the environment variable
       'LD_LIBRARY_PATH' and then in the compiled-in path (except when
       this is a check run). */

    char * volatile lib_name = NULL;
    if ( Fsc2_Internals.cmdline_flags & LOCAL_EXEC )
    {
        lib_name = get_string( moddir "%s.fsc2_so", dev_name );
        if ( lstat( lib_name, &buf ) == -1 )
            lib_name = T_free( lib_name );
    }
    else
    {
        char * ld_path;
        if ( ( ld_path = getenv( "LD_LIBRARY_PATH" ) ) != NULL )
        {
            char * ld = T_strdup( ld_path );

            for ( char * ldc = strtok( ld, ":" ); ldc != NULL;
                  ldc = strtok( NULL, ":" ) )
            {
                lib_name = get_string( "%s%s%s.fsc2_so", ldc, slash( ldc ),
                                       dev_name );
                if ( lstat( lib_name, &buf ) == 0 )
                    break;
                lib_name = T_free( lib_name );
            }

            T_free( ld );
        }

        if (    lib_name == NULL
             && ! ( Fsc2_Internals.cmdline_flags & DO_CHECK ) )
        {
            lib_name = get_string( libdir "%s.fsc2_so", dev_name );
            if ( lstat( lib_name, &buf ) < 0 )
                lib_name = T_free( lib_name );
        }
    }

    if ( lib_name == NULL )
    {
        eprint( FATAL, false, "Can't find (or access) module '%s.fsc2_so'.\n",
                dev_name );
        THROW( EXCEPTION );
    }

    /* If the module is a symbolic link try to figure out the name of the
       file the symbolic link points to and store it in 'real_name' */

    char * volatile real_name = NULL;

    if ( S_ISLNK( buf.st_mode ) )
    {
        /* We need memory for the name of the file the link points to */

        size_t pathmax = get_pathmax( );
        real_name = T_malloc( pathmax + 1 );
        ssize_t length;

        if ( ( length = readlink( lib_name, real_name, pathmax ) ) != -1 )
            real_name[ length ] = '\0';

        /* Look-up may have failed or target of link could be unavailable */

        if ( length == -1 || stat( real_name, &buf ) == -1 )
        {
            eprint( FATAL, false, "Can't follow symbolic link for module "
                    "'%s'.\n", lib_name );
            T_free( lib_name );
            T_free( real_name );
            THROW( EXCEPTION );
        }

        /* Check that module has the extension ".fsc2_so" and strip it off */

        if ( strcmp( real_name + length - 8, ".fsc2_so" ) )
        {
            eprint( FATAL, false, "Module '%s' used for device '%s' hasn't "
                    "extension \".fsc2_so\".\n", real_name, dev_name );
            T_free( lib_name );
            T_free( real_name );
            THROW( EXCEPTION );
        }

        *( real_name + length - 8 ) = '\0';
    }

    T_free( lib_name );

    return real_name;
}


/*-----------------------------------------------------*
 * Function creates a new Device structure and appends
 * it to the list of devices.
 *-----------------------------------------------------*/

void
device_append_to_list( const char * dev_name )
{
    /* Append a new new Device structure to the list of devices */

    Device_T * cd;
    if ( EDL.Device_List != NULL )
    {
        for ( cd = EDL.Device_List; cd->next != NULL; cd = cd->next )
            /* empty */ ;

        cd->next = T_malloc( sizeof *cd->next );
        cd->next->prev = cd;
        cd = cd->next;
    }
    else
    {
        EDL.Device_List = cd = T_malloc( sizeof *cd );
        cd->prev = NULL;
    }

    /* Initialize the new structure */

    cd->name = T_strdup( dev_name );
    cd->is_loaded = false;
    cd->next = NULL;
    cd->count = 1;

    /* Initialize members of the driver structure */

    cd->driver.handle = NULL;
    cd->driver.is_init_hook =
        cd->driver.is_test_hook =
            cd->driver.is_end_of_test_hook =
                cd->driver.is_exp_hook =
                    cd->driver.is_end_of_exp_hook =
                        cd->driver.is_exit_hook =
                            cd->driver.exp_hook_is_run =
                                cd->driver.is_child_exit_hook =
                                    cd->driver.init_hook_is_run = false;
}


/*---------------------------------------------------------------------*
 * Function deletes the whole list of device structures after running
 * the corresponding exit hook functions and unloading the modules.
 *---------------------------------------------------------------------*/

void
delete_devices( void )
{
    if ( EDL.Device_List == NULL )  /* list is empty or does not exist */
        return;

    /* Get last element of list - always delete last entry first */

    Device_T * cd;
    for ( cd = EDL.Device_List; cd->next != NULL; cd = cd->next )
        /* empty */ ;

    for ( Device_T * cdp; cd != NULL; cd = cdp )
    {
        if ( cd->is_loaded )
            unload_device( cd );         /* run exit hooks and unload module */
        T_free( cd->name );
        cdp = cd->prev;
        T_free( cd );
    }

    EDL.Device_List = NULL;
}


/*-------------------------------------------------------------*
 * Function deletes the list of known devices as read from the
 * device data base file "Devices".
 *-------------------------------------------------------------*/

void
delete_device_name_list( void )
{
    Device_Name_T * cd = EDL.Device_Name_List;
    while ( cd )
    {
        Device_Name_T * cdn = cd->next;
        T_free( cd->name );
        T_free( cd );
        cd = cdn;
    }

    EDL.Device_Name_List = NULL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
