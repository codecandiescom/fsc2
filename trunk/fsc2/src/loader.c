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


#include <dlfcn.h>
#include "fsc2.h"


/* Variables imported from func.c */

extern size_t Num_Func;     /* number of built-in and listed functions */
extern Func *Fncts;         /* structure for list of functions */
extern Func Def_Fncts[ ];   /* structures for list of built-in functions */


static size_t num_func;

static void resolve_hook_functions( Device *dev, const char *dev_name );
static void load_functions( Device *dev );
static void resolve_functions( Device *dev );
static void add_function( int num, void *new_func, Device *new_dev );
static int func_cmp( const void *a, const void *b );
static void resolve_generic_type( Device *dev );


/*-------------------------------------------------------------------------*/
/* Function is called after the DEVICES section has been read in. It loads */
/* the library files and tries to resolve the references to the functions  */
/* listed in `Functions' and stores pointers to the functions in `Fncts'.  */
/*-------------------------------------------------------------------------*/

void load_all_drivers( void )
{
	Device *cd;
	bool saved_need_GPIB;


	/* Treat "User_Functions" also as a kind of device driver and append
	   the device structure to the end of the list of devices */

	device_append_to_list( "User_Functions" );

	/* Link all functions from the device drivers (including "User_Functions",
	   which always comes last) and try to resolve the functions from the
	   function list */

	Max_Devices_of_a_Kind = 1;

	num_func = Num_Func;
	for ( cd = Device_List; cd != NULL; cd = cd->next )
		load_functions( cd );

	/* Because some multiply defined functions may have been added resort
	   the function list to make searching via bsearch() possible */

	qsort( Fncts, Num_Func, sizeof( Func ), func_cmp );

	/* This done run the init hooks (if they exist) and warn if they don't
	   return successfully (if an init hook thinks it should kill the whole
	   program it is supposed to throw an exception). To keep modules writers
	   from erroneously unsetting the global variable `need_GPIB' it's stored
	   before each init_hook() function is called and is restored to its
	   previous values if necessary. */

	for ( cd = Device_List; cd != NULL; cd = cd->next )
	{
		saved_need_GPIB = need_GPIB;

		if ( cd->is_loaded && cd->driver.is_init_hook &&
			 ! cd->driver.init_hook( ) )
			eprint( WARN, UNSET, "Initialisation of module `%s.so' failed.\n",
					cd->name );

		if ( need_GPIB == UNSET && saved_need_GPIB == SET )
			need_GPIB = SET;
	}
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static int func_cmp( const void *a, const void *b )
{
	return strcmp( ( ( const Func * ) a )->name,
				   ( ( const Func * ) b )->name );
}


/*--------------------------------------------*/
/* Function tests if the device driver passed */
/* to the function by name is loaded.         */
/*--------------------------------------------*/

bool exists_device( const char *name )
{
	Device *cd;

	for ( cd = Device_List; cd != NULL; cd = cd->next )
		if ( cd->is_loaded &&
			 ! strcasecmp( strip_path( cd->name ), name ) )
			return OK;

	return FAIL;
}


/*-------------------------------------------------------------------*/
/* Routine tests if a function passed to the routine by name exists. */
/*-------------------------------------------------------------------*/

bool exists_function( const char *name )
{
	size_t i;


	for ( i = 0; i < Num_Func; i++ )
		if ( Fncts[ i ].name != NULL &&
			 ! strcmp( Fncts[ i ].name, name ) &&
			 Fncts[ i ].fnct != NULL )			 
			return OK;

	return FAIL;
}


/*-------------------------------------------------------------------*/
/* Function links a library file with the name passed to it (after   */
/* adding the extension `.so') and then tries to find all references */
/* to functions listed in the function data base `Functions'.        */
/*-------------------------------------------------------------------*/

static void load_functions( Device *dev )
{
	char *lib_name;
	const char *dev_name;


	/* Assemble name of library to be loaded - this will also work for cases
	   where the device name contains a relative path */

	lib_name = get_string( "%s%s%s.so", libdir, slash( libdir ), dev->name );

	/* Try to open the library. If it can't be found in the place defined at
	   compilation time give it another chance by checking the paths defined
	   by LD_LIBRARY_PATH - but only if the device name doesn't contain a
	   path. The last possibility is that the device name already contains a
	   absolute path, probably because it's set via a link. */

	dev->driver.handle = dlopen( lib_name, RTLD_NOW );

	if ( dev->driver.handle == NULL )
		dev->driver.handle = dlopen( strip_path( lib_name ), RTLD_NOW );

	if ( dev->driver.handle == NULL )
	{
		strcpy( lib_name, dev->name );
		strcat( lib_name, ".so" );
		dev->driver.handle = dlopen( lib_name, RTLD_NOW );
	}

	if ( dev->driver.handle == NULL )
	{
		eprint( FATAL, UNSET, "Can't open module for device `%s': %s\n", 
				strip_path( dev->name ), dlerror( ) );
		T_free( lib_name );
		THROW( EXCEPTION )
	}

	T_free( lib_name );

	dev->is_loaded = SET;

	/* The device name used as prefix in the hook functions may not contain
	   a path */

	dev_name = strip_path( dev->name );

	/* Now that we know that the module exists and can be used try to resolve
	   all functions we may need */

	resolve_generic_type( dev );
	resolve_hook_functions( dev, dev_name );
	resolve_functions( dev );
}


/*--------------------------------------------------------------------*/
/* Function tries to find out which hook functions exist for a device */
/* and determines the pointers to these functions.                    */
/*--------------------------------------------------------------------*/

static void resolve_hook_functions( Device *dev, const char *dev_name )
{
	char *hook_func_name;
	char *app;


	dev->driver.is_init_hook =
		dev->driver.is_test_hook =
			dev->driver.is_end_of_test_hook =
				dev->driver.is_exp_hook =
					dev->driver.is_end_of_exp_hook =
						dev->driver.is_exit_hook =
							dev->driver.exp_hook_is_run = UNSET;

	/* If there is function with the name of the library file and the
	   appended string "_init_hook" store it and set corresponding flag
	   (the string will be reused for the other hook functions, so make
	   it long enough that the longest name will fit into it) */

	hook_func_name = T_malloc( strlen( dev_name ) + 18 );
	strcpy( hook_func_name, dev_name );
	app = hook_func_name + strlen( dev_name );
	strcpy( app, "_init_hook" );

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.init_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_init_hook = SET;

	/* Get test hook function if available */
	
	strcpy( app, "_test_hook" );

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.test_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_test_hook = SET;

	/* Get end-of-test hook function if available */
	
	strcpy( app, "_end_of_test_hook" );

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.end_of_test_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_end_of_test_hook = SET;

	/* Get pre-experiment hook function if available */
	
	strcpy( app, "_exp_hook" );

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.exp_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_exp_hook = SET;

	/* Get end-of-experiment hook function if available */

	strcpy( app, "_end_of_exp_hook" );

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.end_of_exp_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_end_of_exp_hook = SET;

	/* Finally check if there's also an exit hook function */

	strcpy( app, "_exit_hook" );

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.exit_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_exit_hook = SET;

	T_free( hook_func_name );

	exit_hooks_are_run = UNSET;
}


/*----------------------------------------------------------------------*/
/* Runs through all the functions in the function list and if they need */
/* to be resolved it tries to find them in the device driver functions. */
/*----------------------------------------------------------------------*/

static void resolve_functions( Device *dev )
{
	size_t num;
	void *cur;
	Func *f = Fncts;


	for ( num = 0; num < num_func; f++, num++ )
	{
		/* Don't try to load functions that are not listed in `Functions' */

		if ( ! f->to_be_loaded )
			continue;

		dlerror( );                /* make sure it's NULL before we continue */
		cur = dlsym( dev->driver.handle, f->name );

		if ( dlerror( ) != NULL )
			continue;

		/* If the function is completely new just set the pointer to the
		   place the function is to be found in the library and. If it's
		   not the first device of the same kind (i.e. same generic_type
		   field) also add a function with '#' and the number. Otherwise
		   append a new function */

		if ( f->fnct == NULL )
		{
			f->fnct = cur;
			f->device = dev;
			if ( dev->count != 1 )
			{
				eprint( NO_ERROR, UNSET, "Functions %s() and %s#%d() are both "
						"defined by module `%s'.\n", f->name, f->name,
						dev->count, dev->name );
				add_function( num, cur, dev );
			}
		}
		else
			add_function( num, cur, dev );
	}
}


/*----------------------------------------------------------------------*/
/* This function is called when a function found in a device driver has */
/* already been defined by a different device driver. In this case we   */
/* have to extend the function list for this function, adding '#n' to   */
/* the name of the function (with 'n' being the number of times the     */
/* function has already been defined) and all functions defined for the */
/* current device.                                                      */
/*----------------------------------------------------------------------*/

static void add_function( int num, void *new_func, Device *new_dev )
{
	Func *f;


	if ( new_dev->count == 1 ||
		 ( new_dev->generic_type != NULL &&
		   Fncts[ num ].device->generic_type != NULL &&
		   strcmp( new_dev->generic_type,
				   Fncts[ num ].device->generic_type ) != 0 ) )
	{
		eprint( FATAL, SET, "Functions both with name %s() are defined in "
				"modules of different types, `%s' and `%s'.\n",
				Fncts[ num ].name, Fncts[ num ].device->name, new_dev->name );
		THROW( EXCEPTION )
	}

	/* Add an entry for the new function to the list of functions */

	Fncts = T_realloc( Fncts, ( Num_Func + 1 ) * sizeof( Func ) );
	f = Fncts + Num_Func++;
	memcpy( f, Fncts + num, sizeof( Func ) );
	
	f->fnct   = new_func;
	f->device = new_dev;
	f->name = get_string( "%s#%d", Fncts[ num ].name, new_dev->count );
}


/*----------------------------------------------------------------------*/
/* In each device module a generic type string should be defined. This  */
/* string should be the same for devices with the same function, i.e.   */
/* for all lock-in amplifiers the string is "lockin" etc. Here we try   */
/* to get a pointer to this string from the library. If none exists the */
/* pointer is set to NULL. If it exist we also run through the list of  */
/* devices for which functions have already been loaded and try to find */
/* out if this is the first of a generic type or how many there already */
/* are - the result is stored in the Device structure.                  */
/*----------------------------------------------------------------------*/

static void resolve_generic_type( Device *dev )
{
	Device *cd;


	dev->count = 1;
	dlerror( );                    /* make sure it's NULL before we continue */
	dev->generic_type = ( const char * ) dlsym( dev->driver.handle,
												"generic_type" );

	if ( dlerror( ) != NULL )               /* symbol not found in library ? */
	{
		dev->generic_type = NULL;
		return;
	}

	for ( cd = Device_List; cd != dev; cd = cd->next )
		if ( cd->generic_type != NULL &&
			 ! strcmp( cd->generic_type, dev->generic_type ) )
			dev->count++;

	Max_Devices_of_a_Kind = i_max( Max_Devices_of_a_Kind, dev->count );
}


/*-------------------------------------------------------*/
/* Functions runs the test hook functions of all modules */
/*-------------------------------------------------------*/

void run_test_hooks( void )
{
	Device *cd;

	for ( cd = Device_List; cd != NULL; cd = cd->next )
		if ( cd->is_loaded && cd->driver.is_test_hook &&
			 ! cd->driver.test_hook( ) )
			eprint( SEVERE, UNSET, "Initialisation of test run failed for "
					"module `%s'.\n", cd->name );
}


/*--------------------------------------------------------------*/
/* Functions runs the end-of-test hook functions of all modules */
/*--------------------------------------------------------------*/

void run_end_of_test_hooks( void )
{
	Device *cd;

	for ( cd = Device_List; cd != NULL; cd = cd->next )
		if ( cd->is_loaded && cd->driver.is_end_of_test_hook &&
			 ! cd->driver.end_of_test_hook( ) )
			eprint( SEVERE, UNSET, "Final checks after test run failed for "
					"module `%s'.\n", cd->name );
}


/*-------------------------------------------------------------*/
/* Functions runs the experiment hook functions of all modules */
/*-------------------------------------------------------------*/

void run_exp_hooks( void )
{
	Device *cd;

	for ( cd = Device_List; cd != NULL; cd = cd->next )
	{
		if ( cd->is_loaded && cd->driver.is_exp_hook &&
			 ! cd->driver.exp_hook( ) )
			eprint( SEVERE, UNSET, "Initialisation of experiment failed for "
					"module `%s'.\n", cd->name );
		else
			cd->driver.exp_hook_is_run = SET;

		/* Give user a chance to stop while running the experiment hooks */

		fl_check_only_forms( );
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION )
	}
}


/*--------------------------------------------------------------------*/
/* Functions runs the end-of-experiment hook functions of all modules */
/*--------------------------------------------------------------------*/

void run_end_of_exp_hooks( void )
{
	static Device *cd;


	/* Each of the end-of-experiment hooks must be run to get all instruments
	   back into a usable state, even if the function fails for one of them.
	   The only exception are devices for which the exp-hook has not been
	   run, probably because the exp-hook for a previous device in the list
	   failed. */

	for ( cd = Device_List; cd != NULL; cd = cd->next )
	{
		if ( ! cd->driver.exp_hook_is_run )
			continue;

		TRY
		{
			cd->driver.exp_hook_is_run = UNSET;

			if ( cd->is_loaded && cd->driver.is_end_of_exp_hook &&
				 ! cd->driver.end_of_exp_hook( ) )
				eprint( SEVERE, UNSET, "Resetting module `%s' after "
						"experiment failed.\n", cd->name );
			TRY_SUCCESS;
		}
	}
}


/*-------------------------------------------------------*/
/* Functions runs the exit hook functions of all modules */
/*-------------------------------------------------------*/

void run_exit_hooks( void )
{
	static Device *cd;


	if ( Device_List == NULL )
		return;

	/* Run all exit hooks starting with the last device and ending with the
	   very first one in the list. Also make sure that all exit hooks are run
	   even if some of them fail with an exception. */

	for( cd = Device_List; cd->next != NULL; cd = cd->next )
		;

	for ( ; cd != NULL; cd = cd->prev )
	{
		TRY
		{
			if ( cd->is_loaded && cd->driver.is_exit_hook )
				cd->driver.exit_hook( );
			TRY_SUCCESS;
		}
	}

	/* Set global variable to show that exit hooks already have been run */

	exit_hooks_are_run = SET;
}


/*------------------------------------------------------------------------*/
/* This function is intended to allow user defined modules access to the  */
/* symbols defined in another module. Probably it's a BAD thing if they   */
/* do, but sometimes it might be inevitable, so we better include this    */
/* instead of having the writer of a module trying to figure out some     */
/* other and probably more difficult or dangerous method to do it anyway. */
/*                                                                        */
/* ->                                                                     */
/*    1. Name of the module (without the `.so' extension) the symbol is   */
/*       to be loaded from                                                */
/*    2. Name of the symbol to be loaded                                  */
/*    3. Pointer to void pointer for returning the address of the symbol  */
/*                                                                        */
/* <-                                                                     */
/*    Return value indicates success or failure (see global.h for defi-   */
/*    nition of the the return codes). On success `symbol_ptr' contains   */
/*    the address of the symbol.                                          */ 
/*------------------------------------------------------------------------*/

int get_lib_symbol( const char *from, const char *symbol, void **symbol_ptr )
{
	Device *cd;


	/* Try to find the library fitting the name */

	for ( cd = Device_List; cd != 0; cd = cd->next )
		if ( cd->is_loaded &&
			 ! strcasecmp( strip_path( cd->name ), from ) )
			break;

	if ( cd == NULL )                    /* library not found ? */
		return LIB_ERR_NO_LIB;

	/* Try to load the symbol */

	dlerror( );                    /* make sure it's NULL before we continue */
	*symbol_ptr = dlsym( cd->driver.handle, symbol );

	if ( dlerror( ) != NULL )               /* symbol not found in library ? */
		return LIB_ERR_NO_SYM;

	return LIB_OK;
}


/*------------------------------------------------------------------------*/
/* This routine expects the name of a device and returns the position in  */
/* the list of devices with the same function, as indicated by the        */
/* generic type string. I.e. if you have loaded modules for three lock-in */
/* amplifiers and you pass this function the name of one of them it looks */
/* at the sequence the devices were listed in the DEVICES section and     */
/* returns the sequence number of the device, in this case either 1, 2 or */
/* 3. In case of errors (or if the devices generic_type string isn't set) */
/* the funtion returns 0.                                                 */
/*------------------------------------------------------------------------*/

int get_lib_number( const char *name )
{
	Device *cd;
	Device *sd = NULL;                   /* the device we're looking for */
	int num;


	for ( cd = Device_List; cd != 0; cd = cd->next )
		if ( cd->is_loaded && cd->generic_type != NULL &&
			 ! strcasecmp( strip_path( cd->name ), name ) )
		{
			sd = cd;
			break;
		}

	if ( cd == NULL || sd == NULL )
		return 0;

	for ( num = 1, cd = Device_List; cd != 0; cd = cd->next )
	{
		if ( ! cd->is_loaded )
			continue;

		if ( cd == sd )             /* device found -> we're done */
			return num;

		if ( cd->generic_type != NULL && sd->generic_type != NULL &&
			 ! strcasecmp( cd->generic_type, sd->generic_type ) )
			num++;
	}

	return 0;
}


/*-------------------------------------------------------------*/
/* The function runs the exit hook functions for a modules (if */
/* this hasn't already been done and if there exists one) and  */
/* than closes the connection to the modules.                  */
/*-------------------------------------------------------------*/

void unload_device( Device *dev )
{
	if ( dev->driver.handle &&
		 ! exit_hooks_are_run && dev->driver.is_exit_hook )
	{
		TRY                             /* catch exceptions from the exit   */
		{                               /* hooks, we've got to run them all */
			dev->driver.exit_hook( );
			TRY_SUCCESS;
		}
	}
	
	dlclose( dev->driver.handle );
	dev->driver.handle = NULL;
}
