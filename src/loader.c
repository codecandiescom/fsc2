/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#include <dlfcn.h>
#include "fsc2.h"


/* Variables imported from func.c */

extern size_t Num_Func;       /* number of built-in and listed functions */
extern Func_T *Fncts;         /* structure for list of functions */


static size_t num_func;
static int Max_Devices_of_a_Kind;

static void resolve_hook_functions( Device_T * dev );
static void load_functions( Device_T * dev );
static void resolve_functions( Device_T * dev );
static void add_function( size_t     num,
                          void *     new_func,
                          Device_T * new_dev );
static int func_cmp( const void * a,
                     const void * b );
static void resolve_device_name( Device_T * dev );
static void resolve_generic_type( Device_T * dev );


/*-------------------------------------------------------------------------*
 * Function is called after the DEVICES section has been read in. It loads
 * the library files and tries to resolve the references to the functions
 * listed in 'Functions' and stores pointers to the functions in 'Fncts'.
 *-------------------------------------------------------------------------*/

void
load_all_drivers( void )
{
    Device_T *cd;
    bool saved_need_GPIB;
    bool saved_need_USB;
#if defined WITH_RULBUS
    bool saved_need_RULBUS;
#endif
    bool saved_need_LAN;
#if defined WITH_MEDRIVER
    bool saved_need_MEDRIVER;
#endif

    CLOBBER_PROTECT( cd );

    Need_GPIB     = UNSET;
    Need_RULBUS   = UNSET;
    Need_LAN      = UNSET;
    Need_MEDRIVER = UNSET;
    Need_USB      = UNSET;

    /* Treat "User_Functions" also as a kind of device driver and append
       the device structure to the end of the list of devices */

    device_append_to_list( "User_Functions" );

    /* Link all functions from the device drivers (including "User_Functions",
       which always comes last) and try to resolve the functions from the
       function list */

    Max_Devices_of_a_Kind = 1;
    EDL.Num_Pulsers = 0;

    num_func = Num_Func;
    TRY
    {
        for ( cd = EDL.Device_List; cd != NULL; cd = cd->next )
            load_functions( cd );
        TRY_SUCCESS;
    }

    /* Because some multiple defined functions may have been added resort
       the function list to make searching via bsearch() possible */

    qsort( Fncts, Num_Func, sizeof *Fncts, func_cmp );

    /* Create and initialize a structure needed for the pulsers */

    pulser_struct_init( );
    Cur_Pulser = -1;

    /* This done run the init hooks (if they exist) and warn if they don't
       return successfully (if an init hook thinks it should kill the whole
       program it is supposed to throw an exception). To keep modules writers
       from erroneously unsetting the global variable 'Need_GPIB' it's stored
       before each init_hook() function is called and is restored to its
       previous values if necessary. */

    Fsc2_Internals.in_hook = SET;

    TRY
    {
        for ( cd = EDL.Device_List; cd != NULL; cd = cd->next )
        {
            saved_need_GPIB     = Need_GPIB;
            saved_need_LAN      = Need_LAN;
            saved_need_USB      = Need_USB;
#if defined WITH_RULBUS
            saved_need_RULBUS   = Need_RULBUS;
#endif
#if defined WITH_MEDRIVER
            saved_need_MEDRIVER = Need_MEDRIVER;
#endif

            if ( cd->is_loaded )
            {
                if (    cd->generic_type != NULL
                     && ! strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
                    Cur_Pulser++;

                if ( cd->driver.is_init_hook )
                {
                    call_push( NULL, cd, cd->device_name, cd->count );

                    TRY
                    {
                        if ( ! cd->driver.init_hook( ) )
                            eprint( WARN, UNSET, "Initialisation of module "
                                    "'%s.fsc2_so' did not return "
                                    "successfully.\n", cd->name );
                        TRY_SUCCESS;
                    }
                    OTHERWISE
                    {
                        eprint( FATAL, UNSET, "Failed to initialize module "
                                "'%s'.\n", cd->name );
                        call_pop( );
                        vars_del_stack( );
                        dlclose( cd->driver.handle );
                        RETHROW( );
                    }

                    call_pop( );
                    vars_del_stack( );

                    cd->driver.init_hook_is_run = SET;
                }
            }

            if ( ! Need_GPIB && saved_need_GPIB )
                Need_GPIB = SET;

#if defined WITH_RULBUS
            if ( ! Need_RULBUS && saved_need_RULBUS )
                Need_RULBUS = SET;
#else
            if ( Need_RULBUS )
            {
                eprint( FATAL, UNSET, "Module '%s' requires RULBUS but fsc2 "
                        "hasn't been built with RULBUS support.\n", cd->name );
                THROW( EXCEPTION );
            }
#endif

            if ( ! Need_LAN && saved_need_LAN )
                Need_LAN = SET;


#if defined WITH_MEDRIVER
            if ( ! Need_MEDRIVER && saved_need_MEDRIVER )
                Need_MEDRIVER = SET;
#else
            if ( Need_MEDRIVER )
            {
                eprint( FATAL, UNSET, "Module '%s' requires MEDRIVER but fsc2 "
                        "hasn't been built with MEDRIVER support.\n",
                        cd->name );
                THROW( EXCEPTION );
            }
#endif
            if ( ! Need_USB && saved_need_USB )
                Need_USB = SET;

#if ! defined WITH_LIBUSB_1_0 && ! defined WITH_LIBUSB_0_1
            if ( Need_USB )
            {
                eprint( FATAL, UNSET, "Module '%s' requires LIBUSB but fsc2 "
                        "hasn't been built with LIBUSB (0.1 or 1.0) support.\n",
                        cd->name );
                THROW( EXCEPTION );
            }
#endif
        }

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        Fsc2_Internals.in_hook = UNSET;
        delete_devices( );
        RETHROW( );
    }

    Fsc2_Internals.in_hook = UNSET;
}


/*----------------------------------------------------------*
 * Function for comparing two functions by there name, used
 * for sorting the function names by binary search.
 *----------------------------------------------------------*/

static int
func_cmp( const void * a,
          const void * b )
{
    return strcmp( ( ( const Func_T * ) a )->name,
                   ( ( const Func_T * ) b )->name );
}


/*---------------------------------------------------------------*
 * Function tests if the device driver passed to the function by
 * name is loaded. If it exists it returns the number that needs
 * to be appended to the function names, otherwise 0.
 *---------------------------------------------------------------*/

int
exists_device( const char * name )
{
    Device_T *cd;


    for ( cd = EDL.Device_List; cd != NULL; cd = cd->next )
        if (    cd->is_loaded
             && ! strcasecmp( strip_path( cd->name ), name ) )
            return cd->count;

    return 0;
}


/*---------------------------------------------------------------*
 * Function tests if a device driver for a device of the generic
 * type passed to the function as argument is loaded.
 *---------------------------------------------------------------*/

bool
exists_device_type( const char * type )
{
    Device_T *cd;


    for ( cd = EDL.Device_List; cd != NULL; cd = cd->next )
        if (    cd->is_loaded
             && ! strcasecmp( strip_path( cd->generic_type ), type ) )
            return OK;

    return FAIL;
}

/*-------------------------------------------------------------------*
 * Function tests if a function passed to the routine by name exists
 *-------------------------------------------------------------------*/

bool
exists_function( const char * name )
{
    size_t i;


    for ( i = 0; i < Num_Func; i++ )
        if (    Fncts[ i ].name != NULL
             && ! strcmp( Fncts[ i ].name, name )
             && Fncts[ i ].fnct != NULL )
            return OK;

    return FAIL;
}


/*-------------------------------------------------------------*
 * Function dlopens a library file with the name passed to it
 * (after adding the extension '.fsc2_so') and then tries to
 * find all references to functions listed in the function
 * data base 'Functions'.
 *-------------------------------------------------------------*/

static void
load_functions( Device_T * dev )
{
    char *lib_name;


    dev->driver.handle = NULL;

    /* If the '-l' option was used only look in the source directory
       for modules */

    if ( Fsc2_Internals.cmdline_flags & LOCAL_EXEC )
    {
        lib_name = get_string( moddir "%s.fsc2_so", dev->name );
        if ( ( dev->driver.handle = dlopen( lib_name, RTLD_NOW ) ) != NULL )
            dev->driver.lib_name = lib_name;
        else
            T_free( lib_name );
    }
    else
    {
        /* Try try to find the module automatically, i.e. in the directories
           defined by the environment variable "LD_LIBRARY_PATH" (the modules
           should never end up in the places were the linker looks for them
           by default). */

        lib_name = get_string( "%s.fsc2_so", dev->name );

        if ( ( dev->driver.handle = dlopen( lib_name, RTLD_NOW ) ) != NULL )
            dev->driver.lib_name = lib_name;
        else
            T_free( lib_name );

        /* If this didn't work try it the normal way, using the compiled in
           library path or, if the device name starts with an absolute path,
           using this path (this may happen when the device is specified using
           an alternative name and thus we have to follow a symbolic link).
           The exception is when the DO_CHECK flag is defined, where the
           compiled in path is *not* what we want... */

        if (    dev->driver.handle == NULL
             && ! ( Fsc2_Internals.cmdline_flags & DO_CHECK )
             && dev->name[ 0 ] != '/' )
        {
            lib_name = get_string( libdir "%s.fsc2_so", dev->name );

            if ( ( dev->driver.handle = dlopen( lib_name, RTLD_NOW ) )
                                                                      != NULL )
                dev->driver.lib_name = lib_name;
            else
                T_free( lib_name );
        }
    }

    if ( dev->driver.handle == NULL )
    {
        eprint( FATAL, UNSET, "Can't open module for device '%s': %s.\n",
                dev->name[ 0 ] != '/' ?
                dev->name : strrchr( dev->name, '/' ) + 1, dlerror( ) );
        THROW( EXCEPTION );
    }

    dev->is_loaded = SET;

    /* Now that we know that the module exists and can be used try to resolve
       all functions we may need */

    resolve_device_name( dev );
    resolve_generic_type( dev );
    resolve_hook_functions( dev );
    resolve_functions( dev );
}


/*-----------------------------------------------------------*
 * Function tries to find out which hook functions exist for
 * a device and determines the pointers to these functions.
 *-----------------------------------------------------------*/

static void
resolve_hook_functions( Device_T * dev )
{
    char *hook_func_name;
    char *app;


    /* If there is function with the name of the library file and the
       appended string "_init_hook" store it and set the corresponding flag
       (the string will be reused for the other hook functions, so make
       it long enough that the longest name will fit into it). Strip any
       path before the device name - it may be set via a symbolic link to
       some other directory than the normal module directory. */

    hook_func_name = T_malloc( strlen( strip_path( dev->name ) ) + 18 );
    strcpy( hook_func_name, strip_path( dev->name ) );
    app = hook_func_name + strlen( strip_path( dev->name ) );
    strcpy( app, "_init_hook" );

    dlerror( );
    dev->driver.init_hook =
             ( int ( * )( void ) ) dlsym( dev->driver.handle, hook_func_name );
    if ( dlerror( ) == NULL )
        dev->driver.is_init_hook = SET;

    /* Get the test hook function if available */

    strcpy( app, "_test_hook" );
    dlerror( );
    dev->driver.test_hook =
             ( int ( * )( void ) ) dlsym( dev->driver.handle, hook_func_name );
    if ( dlerror( ) == NULL )
        dev->driver.is_test_hook = SET;

    /* Get the end-of-test hook function if available */

    strcpy( app, "_end_of_test_hook" );
    dlerror( );
    dev->driver.end_of_test_hook =
             ( int ( * )( void ) ) dlsym( dev->driver.handle, hook_func_name );
    if ( dlerror( ) == NULL )
        dev->driver.is_end_of_test_hook = SET;

    /* Get the pre-experiment hook function if available */

    strcpy( app, "_exp_hook" );
    dlerror( );
    dev->driver.exp_hook =
             ( int ( * )( void ) ) dlsym( dev->driver.handle, hook_func_name );
    if ( dlerror( ) == NULL )
        dev->driver.is_exp_hook = SET;

    /* Get the end-of-experiment hook function if available */

    strcpy( app, "_end_of_exp_hook" );
    dlerror( );
    dev->driver.end_of_exp_hook =
             ( int ( * )( void ) ) dlsym( dev->driver.handle, hook_func_name );
    if ( dlerror( ) == NULL )
        dev->driver.is_end_of_exp_hook = SET;

    /* Get the exit hook function if available */

    strcpy( app, "_exit_hook" );
    dlerror( );
    dev->driver.exit_hook =
            ( void ( * )( void ) ) dlsym( dev->driver.handle, hook_func_name );
    if ( dlerror( ) == NULL )
        dev->driver.is_exit_hook = SET;

    Fsc2_Internals.exit_hooks_are_run = UNSET;

    /* Finally check if there's also an exit hook function for the child */

    strcpy( app, "_child_exit_hook" );
    dlerror( );
    dev->driver.child_exit_hook =
            ( void ( * )( void ) ) dlsym( dev->driver.handle, hook_func_name );
    if ( dlerror( ) == NULL )
        dev->driver.is_child_exit_hook = SET;

    T_free( hook_func_name );
}


/*---------------------------------------------------------------------*
 * Run through all the functions in the function list and if they need
 * to be resolved try to find them in the device driver functions.
 *---------------------------------------------------------------------*/

static void
resolve_functions( Device_T * dev )
{
    size_t num;
    void *cur;


    for ( num = 0; num < num_func; num++ )
    {
        /* Don't try to load functions that are not listed in 'Functions' */

        if ( ! Fncts[ num ].to_be_loaded )
            continue;

        dlerror( );                /* make sure it's NULL before we continue */

        cur = dlsym( dev->driver.handle, Fncts[ num ].name );

        if ( cur == NULL || dlerror( ) != NULL )
            continue;

        /* If the function is completely new just set the pointer to the
           place the function is to be found in the library. If it's not
           the first device of the same kind (i.e. same generic_type field)
           also add a function with '#' and the number. Otherwise append a
           new function */

        if ( Fncts[ num ].fnct == ( Var_T * ( * )( Var_T * ) ) NULL )
        {
            Fncts[ num ].fnct = ( Var_T * ( * )( Var_T * ) ) cur;
            Fncts[ num ].device = dev;
            if ( dev->count != 1 )
            {
                eprint( NO_ERROR, UNSET, "Functions %s() and %s#%d() are both "
                        "defined by module '%s'.\n", Fncts[ num ].name,
                        Fncts[ num ].name, dev->count, dev->name );
                add_function( num, cur, dev );
            }
        }
        else
            add_function( num, cur, dev );
    }
}


/*----------------------------------------------------------------------*
 * This function is called when a function found in a device driver has
 * already been defined by a different device driver. In this case we
 * have to extend the function list for this function, adding '#n' to
 * the name of the function (with 'n' being the number of times the
 * function has already been defined) and all functions defined for the
 * current device.
 *----------------------------------------------------------------------*/

static void
add_function( size_t     num,
              void *     new_func,
              Device_T * new_dev )
{
    Func_T *f;


    if (    new_dev->count == 1
         || (    new_dev->generic_type != NULL
              && Fncts[ num ].device->generic_type != NULL
              && strcasecmp( new_dev->generic_type,
                             Fncts[ num ].device->generic_type ) != 0 ) )
    {
        eprint( FATAL, UNSET, "Functions with name %s() are defined in "
                "modules of different types, '%s' and '%s'.\n",
                Fncts[ num ].name, Fncts[ num ].device->name, new_dev->name );
        THROW( EXCEPTION );
    }

    /* Add an entry for the new function to the list of functions */

    Fncts = T_realloc( Fncts, ( Num_Func + 1 ) * sizeof *Fncts );
    f = Fncts + Num_Func++;
    memcpy( f, Fncts + num, sizeof *f );

    f->fnct   = ( Var_T * ( * )( Var_T * ) ) new_func;
    f->device = new_dev;
    f->name = get_string( "%s#%d", Fncts[ num ].name, new_dev->count );
}


/*-------------------------------------------------------------------*
 * In each device module a device name string should be defined that
 * is going to be used in warnings and error messages. This function
 * tries to locate the variable with the name 'device_name' within
 * the module and stores it in the Device structure.
 *-------------------------------------------------------------------*/

static void
resolve_device_name( Device_T * dev )
{
    dlerror( );
    dev->device_name = ( const char * ) dlsym( dev->driver.handle,
                                               "device_name" );
    if ( dlerror( ) != NULL )               /* symbol not found in library ? */
        dev->device_name = NULL;
}


/*----------------------------------------------------------------------*
 * In each device module a generic type string should be defined. This
 * string should be the same for devices with the same function, i.e.
 * for all lock-in amplifiers the string is "lockin" etc. Here we try
 * to get a pointer to this string from the library. If none exists the
 * pointer is set to NULL. If it exist we also run through the list of
 * devices for which functions have already been loaded and try to find
 * out if this is the first of a generic type or how many there already
 * are - the result is stored in the Device structure.
 *----------------------------------------------------------------------*/

static void
resolve_generic_type( Device_T * dev )
{
    Device_T *cd;


    dev->count = 1;
    dlerror( );
    dev->generic_type = ( const char * ) dlsym( dev->driver.handle,
                                                "generic_type" );

    if ( dlerror( ) != NULL )               /* symbol not found in library ? */
    {
        dev->generic_type = NULL;
        return;
    }

    for ( cd = EDL.Device_List; cd != dev; cd = cd->next )
        if (    cd->generic_type != NULL
             && ! strcasecmp( cd->generic_type, dev->generic_type ) )
            dev->count++;

    Max_Devices_of_a_Kind = i_max( Max_Devices_of_a_Kind, dev->count );

    if (    dev->generic_type
         && ! strcasecmp( dev->generic_type, PULSER_GENERIC_TYPE ) )
        EDL.Num_Pulsers++;
}


/*------------------------------------------------------*
 * Function runs the test hook functions of all modules
 *------------------------------------------------------*/

void
run_test_hooks( void )
{
    Device_T *cd;


    Cur_Pulser = -1;
    Fsc2_Internals.in_hook = SET;

    TRY
    {
        for ( cd = EDL.Device_List; cd != NULL; cd = cd->next )
        {

            fsc2_assert( EDL.Call_Stack == NULL );

            if ( cd->is_loaded && cd->driver.is_test_hook )
            {
                if (    cd->generic_type != NULL
                     && ! strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
                    Cur_Pulser++;

                call_push( NULL, cd, cd->device_name, cd->count );

                if ( ! cd->driver.test_hook( ) )
                    eprint( SEVERE, UNSET, "Initialisation of test run failed "
                            "for module '%s'.\n", cd->name );
                call_pop( );
                vars_del_stack( );
            }
        }

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        call_pop( );
        vars_del_stack( );
        Fsc2_Internals.in_hook = UNSET;
        RETHROW( );
    }

    Fsc2_Internals.in_hook = UNSET;
}


/*-------------------------------------------------------------*
 * Function runs the end-of-test hook functions of all modules
 *-------------------------------------------------------------*/

void
run_end_of_test_hooks( void )
{
    Device_T *cd;


    Cur_Pulser = -1;
    Fsc2_Internals.in_hook = SET;

    TRY
    {
        for ( cd = EDL.Device_List; cd != NULL; cd = cd->next )
        {
            fsc2_assert( EDL.Call_Stack == NULL );

            if ( cd->is_loaded && cd->driver.is_end_of_test_hook )
            {
                if (    cd->generic_type != NULL
                     && ! strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
                    Cur_Pulser++;

                call_push( NULL, cd, cd->device_name, cd->count );

                if ( ! cd->driver.end_of_test_hook( ) )
                    eprint( SEVERE, UNSET, "Final checks after test run "
                            "failed for module '%s'.\n", cd->name );
                call_pop( );
                vars_del_stack( );
            }
        }

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        call_pop( );
        vars_del_stack( );
        Fsc2_Internals.in_hook = UNSET;
        RETHROW( );
    }

    Fsc2_Internals.in_hook = UNSET;
}


/*------------------------------------------------------------*
 * Function runs the experiment hook functions of all modules
 *------------------------------------------------------------*/

void
run_exp_hooks( void )
{
    Device_T *cd;


    Cur_Pulser = -1;
    Fsc2_Internals.in_hook = SET;

    TRY
    {
        for ( cd = EDL.Device_List; cd != NULL; cd = cd->next )
        {
            fsc2_assert( EDL.Call_Stack == NULL );

            if ( cd->is_loaded && cd->driver.is_exp_hook )
            {
                if (    cd->generic_type != NULL
                     && ! strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
                    Cur_Pulser++;

                call_push( NULL, cd, cd->device_name, cd->count );

                if ( ! cd->driver.exp_hook( ) )
                    eprint( SEVERE, UNSET, "Initialization of experiment "
                            "failed for module '%s'.\n", cd->name );
                else
                    cd->driver.exp_hook_is_run = SET;

                call_pop( );
                vars_del_stack( );
            }
            else
                cd->driver.exp_hook_is_run = SET;

            /* Give user a chance to stop while running the experiment hooks */

            if ( ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
            {
                fl_check_only_forms( );
                if ( EDL.do_quit && EDL.react_to_do_quit )
                    THROW( USER_BREAK_EXCEPTION );
            }
        }

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        call_pop( );
        vars_del_stack( );
        Fsc2_Internals.in_hook = UNSET;
        RETHROW( );
    }

    Fsc2_Internals.in_hook = UNSET;
}


/*-------------------------------------------------------------------*
 * Function runs the end-of-experiment hook functions of all modules
 *-------------------------------------------------------------------*/

void
run_end_of_exp_hooks( void )
{
    Device_T *cd;


    CLOBBER_PROTECT( cd );

    if ( EDL.Device_List == NULL )
        return;

    /* Each of the end-of-experiment hooks must be run to get all instruments
       back into a usable state, even if the function fails for one of them.
       The only exception are devices for which the exp-hook has not been
       run, probably because the exp-hook for a previous device in the list
       failed. The list of devices must be run through in reverse order so
       that devices depending on others berfore them in the list have their
       end_of_exp hooks run first! */

    Cur_Pulser = -1;
    Fsc2_Internals.in_hook = SET;

    for( cd = EDL.Device_List; cd->next != NULL; cd = cd->next )
        /* empty */ ;

    for ( ; cd != NULL; cd = cd->prev )
    {
        fsc2_assert( EDL.Call_Stack == NULL );

        if (    cd->generic_type != NULL
             && ! strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
            Cur_Pulser++;

        if ( ! cd->driver.exp_hook_is_run )
            continue;

        cd->driver.exp_hook_is_run = UNSET;

        if ( ! cd->is_loaded || ! cd->driver.is_end_of_exp_hook )
            continue;

        TRY
        {
            call_push( NULL, cd, cd->device_name, cd->count );
            if ( ! cd->driver.end_of_exp_hook( ) )
                eprint( SEVERE, UNSET, "Resetting module '%s' after "
                        "experiment failed.\n", cd->name );
            call_pop( );
            vars_del_stack( );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            call_pop( );
            vars_del_stack( );
        }
    }

    Fsc2_Internals.in_hook = UNSET;
}


/*------------------------------------------------------*
 * Function runs the exit hook functions of all modules
 *------------------------------------------------------*/

void
run_exit_hooks( void )
{
    Device_T *cd;


    CLOBBER_PROTECT( cd );

    if ( EDL.Device_List == NULL )
        return;

    /* Run all exit hooks starting with the last device and ending with the
       very first in the list. Also make sure that all exit hooks are run even
       if some of them fail with an exception. */

    for( cd = EDL.Device_List; cd->next != NULL; cd = cd->next )
        /* empty */ ;

    Fsc2_Internals.in_hook = SET;

    for ( ; cd != NULL; cd = cd->prev )
    {
        fsc2_assert( EDL.Call_Stack == NULL );

        if (    cd->generic_type != NULL
             && ! strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
            Cur_Pulser = EDL.Num_Pulsers - 1;

        if ( ! cd->is_loaded || ! cd->driver.is_exit_hook )
            continue;

        TRY
        {
            call_push( NULL, cd, cd->device_name, cd->count );
            cd->driver.exit_hook( );
            call_pop( );
            vars_del_stack( );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            call_pop( );
            vars_del_stack( );
        }

        if (    cd->generic_type != NULL
             && ! strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
            EDL.Num_Pulsers--;
    }

    Fsc2_Internals.in_hook = UNSET;

    /* Set global variable to show that exit hooks already have been run */

    Fsc2_Internals.exit_hooks_are_run = SET;
}


/*---------------------------------------------------*
 * Function runs the child exit hooks in all modules
 *---------------------------------------------------*/

void
run_child_exit_hooks( void )
{
    Device_T *cd;


    CLOBBER_PROTECT( cd );

    if ( EDL.Device_List == NULL )
        return;

    /* Run all hook functions starting with the last device and ending with
       the very first one in the list. Also make sure that all child exit
       hooks are run even if some of them fail with an exception. */

    for( cd = EDL.Device_List; cd->next != NULL; cd = cd->next )
        /* empty */ ;

    Fsc2_Internals.in_hook = SET;

    for ( ; cd != NULL; cd = cd->prev )
    {
        fsc2_assert( EDL.Call_Stack == NULL );

        if (    cd->generic_type != NULL
             && ! strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
            Cur_Pulser = EDL.Num_Pulsers - 1;

        if ( ! cd->is_loaded || ! cd->driver.is_child_exit_hook )
            continue;

        TRY
        {
            call_push( NULL, cd, cd->device_name, cd->count );
            cd->driver.child_exit_hook( );
            call_pop( );
            vars_del_stack( );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            call_pop( );
            vars_del_stack( );
        }

        if (    cd->generic_type != NULL
             && ! strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
            EDL.Num_Pulsers--;
    }

    Fsc2_Internals.in_hook = UNSET;
}


/*------------------------------------------------------------------------*
 * This function is intended to allow user-defined modules access to the
 * symbols defined in another module. Probably it's a BAD thing if they
 * do, but sometimes it might be inevitable, so we better include this
 * instead of having the writer of a module trying to figure out some
 * other and probably more difficult or dangerous method to do it anyway.
 *
 * ->
 *    1. Name of the module (without the '.fsc2_so' extension) the symbol
 *       is to be loaded from
 *    2. Name of the symbol to be loaded
 *    3. Pointer to void pointer for returning the address of the symbol
 *
 * <-
 *    Return value indicates success or failure (see global.h for defi-
 *    nition of the the return codes). On success 'symbol_ptr' contains
 *    the address of the symbol.
 *------------------------------------------------------------------------*/

int
get_lib_symbol( const char * from,
                const char * symbol,
                void **      symbol_ptr )
{
    Device_T *cd;


    /* Try to find the library fitting the name */

    for ( cd = EDL.Device_List; cd != 0; cd = cd->next )
        if (    cd->is_loaded
             && ! strcasecmp( strip_path( cd->name ), from ) )
            break;

    if ( cd == NULL )                    /* library not found ? */
        return LIB_ERR_NO_LIB;

    /* Try to load the symbol */

    dlerror( );
    *symbol_ptr = dlsym( cd->driver.handle, symbol );

    if ( dlerror( ) != NULL )               /* symbol not found in library ? */
        return LIB_ERR_NO_SYM;

    return LIB_OK;
}


/*-------------------------------------------------------------*
 * The function runs the exit hook functions for a modules (if
 * this hasn't already been done and if there exists one) and
 * than closes the connection to the modules.
 *-------------------------------------------------------------*/

void
unload_device( Device_T * dev )
{
    fsc2_assert( EDL.Call_Stack == NULL );

    if (    dev->driver.handle
         && ! Fsc2_Internals.exit_hooks_are_run
         && dev->driver.init_hook_is_run
         && dev->driver.is_exit_hook )
    {
        if (    dev->generic_type != NULL
             && ! strcasecmp( dev->generic_type, PULSER_GENERIC_TYPE ) )
            Cur_Pulser = EDL.Num_Pulsers - 1;

        Fsc2_Internals.in_hook = SET;
        TRY
        {
            call_push( NULL, dev, dev->device_name, dev->count );
            dev->driver.exit_hook( );
            call_pop( );
            vars_del_stack( );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            call_pop( );
            vars_del_stack( );
        }

        if (    dev->generic_type != NULL
             && ! strcasecmp( dev->generic_type, PULSER_GENERIC_TYPE ) )
            EDL.Num_Pulsers--;
        Fsc2_Internals.in_hook = UNSET;
    }

    dlclose( dev->driver.handle );
    dev->driver.handle = NULL;
    dev->driver.lib_name = T_free( dev->driver.lib_name );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
