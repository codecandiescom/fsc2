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

extern int Num_Func;        /* number of built-in and listed functions */
extern Func *Fncts;         /* structure for list of functions */
extern Func Def_Fncts[ ];   /* structures for list of built-in functions */



static void resolve_hook_functions( Device *dev, char *dev_name );
static void load_functions( Device *dev );
static void resolve_functions( Device *dev );
static void add_function( int index, void *new_func, Device *new_dev,
						  int num_new );
static int func_cmp( const void *a, const void *b );
static void resolve_generic_type( Device *dev );


/*------------------------------------------------------------------------*/
/* Function to be called after the DEVICES section is read in. It loads   */
/* the library files and tries to resolve the references to the functions */
/* listed in `Functions' and stores pointers to the functions in `Fncts'. */
/*------------------------------------------------------------------------*/

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

	for ( cd = Device_List; cd != NULL; cd = cd->next )
		load_functions( cd );

	/* Because some multiply defined functions may have been added resort
	   the function list to make searching via bsearch() possible */

	qsort( Fncts, Num_Func, sizeof( Func ), func_cmp );

	/* This done we run the init hooks (if they exist) and warn if they don't
	   return successfully (if an init hook thinks it should kill the whole
	   program it is supposed to throw an exception). To keep the modules
	   writers from erroneously unsetting the global variable `need_GPIB' it
	   is stored before each init_hook() function is called and, if necessary,
	   is restored to its previous values. */

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
	return strcmp( ( ( Func * ) a )->name, ( ( Func * ) b )->name );
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
			 ! strcasecmp( strchr( cd->name, '/' ) == NULL ?
						   cd->name : strrchr( cd->name, '/' ) + 1, name ) )
			return OK;

	return FAIL;
}


/*-------------------------------------------------------------------*/
/* Routine tests if a function passed to the routine by name exists. */
/*-------------------------------------------------------------------*/

bool exists_function( const char *name )
{
	int i;


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
	char *dev_name;


	/* Assemble name of library to be loaded - this will also work for cases
	   where the device name contains a relative path */

	lib_name = get_string( strlen( libdir ) + strlen( dev->name ) + 4 );
	strcpy( lib_name, libdir );
	if ( libdir[ strlen( libdir ) - 1 ] != '/' )
		strcat( lib_name, "/" );
	strcat( lib_name, dev->name );
	strcat( lib_name, ".so" );

	/* Try to open the library. If it can't be found in the place defined at
	   compilation time give it another chance by checking the paths defined
	   by LD_LIBRARY_PATH - but only if the device name doesn't contain a
	   path. The last possibility is that the device name already contains a
	   absolute path, probably because it's set via a link. */

	dev->driver.handle = dlopen( lib_name, RTLD_NOW );

	if ( dev->driver.handle == NULL )
		dev->driver.handle = dlopen( strrchr( lib_name, '/' ) + 1, RTLD_NOW );

	if ( dev->driver.handle == NULL )
	{
		strcpy( lib_name, dev->name );
		strcat( lib_name, ".so" );
		dev->driver.handle = dlopen( lib_name, RTLD_NOW );
	}

	if ( dev->driver.handle == NULL )
	{
		eprint( FATAL, UNSET, "Can't open module for device `%s': %s\n", 
				strchr( dev->name, '/' ) == NULL ?
				dev->name : strrchr( dev->name, '/' ) + 1, dlerror( ) );
		T_free( lib_name );
		THROW( EXCEPTION );
	}

	T_free( lib_name );

	dev->is_loaded = SET;

	/* The device name used as prefix in the hook functions may not contain
	   a path */

	if ( strchr( dev->name, '/' ) != NULL )
		dev_name = strrchr( dev->name, '/' ) + 1;
	else
		dev_name = dev->name;

	/* Now that we know that the module exists and can be used try to resolve
	   all functions we may need */

	resolve_hook_functions( dev, dev_name );
	resolve_functions( dev );
	resolve_generic_type( dev );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void resolve_hook_functions( Device *dev, char *dev_name )
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

	hook_func_name = get_string( strlen( dev_name ) + 17 );
	strcpy( hook_func_name, dev_name );
	app = hook_func_name + strlen( dev_name );
	strcat( app, "_init_hook" );

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
	int num;
	void *cur;
	char *new_func_name;
	char buf[ 12 ];
	char *temp;
	long len;
	int num_new_funcs = 0;
	Func *f = Fncts;


	for ( num = 0; num < Num_Func; f++, num++ )
	{
		/* Don't try to load functions that are not listed in `Functions' */

		if ( ! f->to_be_loaded )
			continue;

		dlerror( );                /* make sure it's NULL before we continue */

		/* If the functions name is still the original one try it, otherwise
		   first remove the extension with the '#' and the device count */

		if ( ( temp = strchr( f->name, '#' ) ) == NULL )
			 cur = dlsym( dev->driver.handle, f->name );
		else
		{
			len = temp - f->name;
			temp = T_malloc( len + 1 );
			strncpy( temp, f->name, len );
			temp[ len ] = '\0';

			cur = dlsym( dev->driver.handle, temp );
			T_free( temp );
		}

		if ( dlerror( ) != NULL )
			continue;

		/* If the function is completely new just set the pointer to the
		   place the function is to be found in the library and - if
		   necessary, i.e. if another function from the current library
		   had a add twin in a different library, add '#' and the number
		   of the device to the functions name. Otherwise this is a multiple
		   defined function and it got to be appended to function list. */

		if ( f->fnct == NULL )
		{
			f->fnct = cur;
			f->device = dev;
			if ( dev->count != 1 )
			{
				snprintf( buf, 12, "#%d", dev->count );
				new_func_name = T_malloc(   strlen( f->name )
										  + strlen( buf ) + 1 );
				strcpy( new_func_name, f->name );
				strcat( new_func_name, buf );
				T_free( ( char * ) f->name );
				f->name = new_func_name;
			}
		}
		else
			add_function( num, cur, dev, num_new_funcs++ );
	}

	Num_Func += num_new_funcs;
}


/*----------------------------------------------------------------------*/
/* This function is called when a function found in a device driver has */
/* already been defined by a different device driver. In this case we   */
/* have to extend the function list for this function, adding '#n' to   */
/* the name of the function (with 'n' being the number of times the     */
/* function has already been defined) and all functions defined for the */
/* current device.                                                      */
/*----------------------------------------------------------------------*/

static void add_function( int index, void *new_func, Device *new_dev,
						  int num_new )
{
	int i;
	char *new_func_name;
	char buf[ 12 ];
	long len;
	char *temp;
	Func *f;


	/* Find out the correct device number - this is the next number after
	   the highest device number of all devices that had twins in the current
	   library. I.e., if module A exported th functions a1() and a2() and
	   module B exported b1() and b2() then the functions in module C,
	   also exporting a1() and b1() can be accessed as a1#2() and a2#2().
	   This device number is, following a '#', appended to the function name
	   (and the names of all functions from the current module).
	   While not being bulletproof this method hopefully will work correctly
	   with all modules written in a reasonable way... */

	if ( new_dev->count == 1 )
	{
		if ( ( temp = strchr( Fncts[ index ].name, '#' ) ) != NULL )
			new_dev->count = atoi( temp + 1 ) + 1;
		else
			new_dev->count = 2;

		for ( i = 0, f = Fncts; i < Num_Func + num_new; f++, i++ )
			if ( f->device == new_dev )
			{
				snprintf( buf, 12, "#%d", new_dev->count );
				new_func_name = get_string(   strlen( f->name )
											+ strlen( buf ) );
				strcpy( new_func_name, f->name );
				strcat( new_func_name, buf );
				T_free( ( char * ) f->name );
				f->name = new_func_name;
			}
	}
	else
	{
		if ( ( temp = strchr( Fncts[ index ].name, '#' ) ) != NULL 
			 && atoi( temp + 1 ) >= new_dev->count )
		{
			new_dev->count++;
			for ( i = 0, f = Fncts; i < Num_Func; f++, i++ )
				if ( Fncts[ i ].device == new_dev )
				{
					len = strchr( f->name, '#' ) - f->name;
					snprintf( buf, 12, "#%d", new_dev->count );
					new_func_name = get_string( len + strlen( buf ) );
					strncpy( new_func_name, f->name, len );
					strcpy( new_func_name + len, buf );
					T_free( ( char * ) f->name );
					f->name = new_func_name;
				}
		}
	}

	/* Add an entry for the new function to the list of functions */

	Fncts = T_realloc( Fncts, ( Num_Func + num_new + 1 ) * sizeof( Func ) );
	f = Fncts + Num_Func + num_new;
	memcpy( f, Fncts + index, sizeof( Func ) );
	
	f->fnct   = new_func;
	f->device = new_dev;
	snprintf( buf, 12, "#%d", new_dev->count );
	
	if ( ( temp = strchr( Fncts[ index ].name, '#' ) ) == NULL )
	{
		f->name = get_string( strlen( Fncts[ index ].name ) + strlen( buf ) );
		strcpy( ( char * ) f->name, Fncts[ index ].name );
		strcat( ( char * ) f->name, buf );
	}
	else
	{
		len = temp - Fncts[ index ].name;
		f->name = get_string( len + strlen( buf ) );
		strncpy( ( char * ) f->name, Fncts[ index ].name, len );
		strcpy( ( char * ) f->name + len, buf );
	}
}


/*----------------------------------------------------------------------*/
/* In each device module a generic type string should be defined. This  */
/* string should be the same for devices with the same function, i.e.   */
/* for all lock-in amplifiers the string is "lockin" etc. Here we try   */
/* to get a pointer to this string from the library. If none exists the */
/* pointer is set to NULL.                                              */
/*----------------------------------------------------------------------*/

static void resolve_generic_type( Device *dev )
{
	dlerror( );                    /* make sure it's NULL before we continue */
	dev->generic_type = ( const char * ) dlsym( dev->driver.handle,
												"generic_type" );

	if ( dlerror( ) != NULL )               /* symbol not found in library ? */
		dev->generic_type = NULL;
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
		if ( cd->is_loaded && cd->driver.is_exp_hook &&
			 ! cd->driver.exp_hook( ) )
			eprint( SEVERE, UNSET, "Initialisation of experiment failed for "
					"module `%s'.\n", cd->name );
		else
			cd->driver.exp_hook_is_run = SET;
}


/*--------------------------------------------------------------------*/
/* Functions runs the end-of-experiment hook functions of all modules */
/*--------------------------------------------------------------------*/

void run_end_of_exp_hooks( void )
{
	Device *cd;


	/* Each of the end-of-experiment hooks must be run to get all instruments
	   back in a usable state, even if the function fails for one of them.
	   The only exception are devices for which the exp-hook has not been
	   run, probably because the exp-hook for a provious device in the list
	   failed. */

	for ( cd = Device_List; cd != NULL; cd = cd->next )
	{
		TRY
		{
			if ( ! cd->driver.exp_hook_is_run )
				continue;
			else
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
	Device *cd;


	if ( Device_List == NULL )
		return;

	/* Run all exit hooks but starting with the last device and ending with
	   the very first one in the list. Also make sure that all exit hooks are
	   run even if some of them fail with an exception. */

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
			 ( ! strcasecmp( cd->name, from ) || 
			   ( strchr( cd->name, '/' ) != NULL &&
				 ! strcasecmp( strrchr( cd->name, '/' ) + 1, from ) ) ) )
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
/* This routine expects the name of a device and returns the the position */
/* in the list of devices with the same function, as indicated by the     */
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
	int num = 1;


	for ( cd = Device_List; cd != 0; cd = cd->next )
		if ( cd->is_loaded && cd->generic_type != NULL &&
			 ( ! strcasecmp( cd->name, name ) || 
			   ( strchr( cd->name, '/' ) != NULL &&
				 ! strcasecmp( strrchr( cd->name, '/' ) + 1, name ) ) ) )
		{
			sd = cd;
			break;
		}

	if ( cd == NULL || sd == NULL )
		return 0;

	for ( cd = Device_List; cd != 0; cd = cd->next )
	{
		if ( ! cd->is_loaded )
			continue;

		if ( cd == sd )             /* searched for device found -> finished */
			return num;

		if ( cd->generic_type != NULL &&
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
