/*
  $Id$
*/


#include <dlfcn.h>
#include "fsc2.h"


/* Variables imported from func.c */

extern int Num_Def_Func;    /* number of built-in functions */
extern int Num_Func;        /* number of built-in and listed functions */
extern Func *Fncts;         /* structure for list of functions */
extern Func Def_Fncts[ ];   /* structures for list of built-in functions */



/*------------------------------------------------------------------------*/
/* Function to be called after the DEVICES section is read in. It links   */
/* the library files and tries to resolve the references to the functions */
/* listed in `Functions' and stores pointers to the functions in `Fncts'. */
/*------------------------------------------------------------------------*/

void load_all_drivers( void )
{
	Device *cd;
	bool saved_need_GPIB;
	bool saved_need_Serial_Port[ NUM_SERIAL_PORTS ];
	int i;


	/* Treat "User_Functions" also as a kind of device driver and append
	   the device structure to the end of the list of devices */

	device_append_to_list( "User_Functions" );

	/* Link all functions from the device drivers (including "User_Functions",
	   which always comes last) and try to resolve the functions from the
	   function list */

	for ( cd = Device_List; cd != NULL; cd = cd->next )
		load_functions( cd );

	/* This done we run the init hooks (if they exist) and warn if they don't
	   return successfully (if an init hook thinks it should kill the whole
	   program it is supposed to throw an exception). To keep the modules
	   writers from erroneously unsetting the global variables `need_GPIB' and
	   `need_Serial_Port' they are stored before each init_hook() function is
	   called and, if necessary, are restored to their previous values. */

	for ( cd = Device_List; cd != NULL; cd = cd->next )
	{
		saved_need_GPIB = need_GPIB;
		memcpy( saved_need_Serial_Port, need_Serial_Port,
				NUM_SERIAL_PORTS * sizeof( bool ) );

		if ( cd->is_loaded && cd->driver.is_init_hook &&
			 ! cd->driver.init_hook( ) )
			eprint( WARN, "Initialisation of module `%s.so' failed.",
					cd->name );

		if ( need_GPIB == UNSET && saved_need_GPIB == SET )
			need_GPIB = SET;
		for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
			if ( need_Serial_Port[ i ] == UNSET &&
				 saved_need_Serial_Port[ i ] == SET )
				need_Serial_Port[ i ] = SET;
	}
}


/*--------------------------------------------*/
/* Function tests if hte device driver passed */
/* to the function by name is loaded.         */
/*--------------------------------------------*/

bool exist_device( const char *name )
{
	Device *cd;

	for ( cd = Device_List; cd != NULL; cd = cd->next )
		if ( cd->is_loaded &&
			 ! strcmp( strchr( cd->name, '/' ) == NULL ?
					   cd->name : strrchr( cd->name, '/' ) + 1, name ) )
			return OK;

	return FAIL;
}


/*-------------------------------------------------------------------*/
/* Routine tests if a function passed to the routine by name exists. */
/*-------------------------------------------------------------------*/

bool exist_function( const char *name )
{
	int i;


	for ( i = 0; i < Num_Func; i++ )
		if ( Fncts[ i ].name != NULL &&
			 ! strcmp( Fncts[ i ].name, name ) &&
			 Fncts[ i ].fnct != NULL )			 
			return OK;

	return FAIL;
}


/*-----------------------------------------------------------------------*/
/* Function links a library file with the name passed to it (after       */
/* adding the extension `.so') and then tries to find still unresolved   */
/* references to functions listed in the function data base `Functions'. */
/*-----------------------------------------------------------------------*/

void load_functions( Device *dev )
{
	int num;
	char *lib_name;
	char *hook_func_name;
	char *dev_name;
	void *cur;


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

	dev->driver.handle = dlopen( lib_name, RTLD_LAZY );

	if ( dev->driver.handle == NULL )
		dev->driver.handle = dlopen( strrchr( lib_name, '/' ) + 1, RTLD_LAZY );

	if ( dev->driver.handle == NULL && strchr( dev->name, '/' ) == NULL )
	{
		strcpy( lib_name, dev->name );
		strcat( lib_name, ".so" );
		dev->driver.handle = dlopen( lib_name, RTLD_LAZY );
	}

	if ( dev->driver.handle == NULL )
	{
		eprint( FATAL, "Can't open module for device `%s'.", 
				strchr( dev->name, '/' ) == NULL ?
				dev->name : strrchr( dev->name, '/' ) + 1 );
		T_free( lib_name );
		THROW( EXCEPTION );
	}
	T_free( lib_name );

	dev->is_loaded = SET;
	dev->driver.is_init_hook = dev->driver.is_test_hook =
		dev->driver.is_end_of_test_hook = dev->driver.is_exp_hook =
		dev->driver.is_end_of_exp_hook = dev->driver.is_exit_hook = UNSET;

	/* The device name used as prefix in the hook functions may not contain
	   a path */

	if ( strchr( dev->name, '/' ) != NULL )
		dev_name = strrchr( dev->name, '/' ) + 1;
	else
		dev_name = dev->name;

	/* If there is function with the name of the library file and the
	   appended string "_init_hook" store it and set corresponding flag
	   (the string will be reused for the other hook functions, so make
	   it long enough that the longest name will fit into it) */

	hook_func_name = get_string( strlen( dev_name ) + 18 );
	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_init_hook" );	

	dev->driver.init_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_init_hook = SET;

	/* Get test hook function if available */
	
	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_test_hook" );	

	dev->driver.test_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_test_hook = SET;

	/* Get end-of-test hook function if available */
	
	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_end_of_test_hook" );	

	dev->driver.end_of_test_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_end_of_test_hook = SET;

	/* Get pre-experiment hook function if available */
	
	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_exp_hook" );	

	dev->driver.exp_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_exp_hook = SET;

	/* Get end-of-experiment hook function if available */

	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_end_of_exp_hook" );	

	dev->driver.end_of_exp_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_end_of_exp_hook = SET;

	/* Finally check if there's also an exit hook function */

	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_exit_hook" );	

	dev->driver.exit_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_exit_hook = SET;
	T_free( hook_func_name );

	exit_hooks_are_run = UNSET;

	/* Run through all the functions in the function list and if they need
	   to be resolved try to find them in the device driver functions - check
	   that the function has not already been loaded (but overloading built-in
	   functions is acceptable). */

	for ( num = 0; num < Num_Func; num++ )
	{
		/* Don't try to load functions that are not listed in `Functions' */

		if ( ! Fncts[ num ].to_be_loaded )
			continue;

		cur = dlsym( dev->driver.handle, Fncts[ num ].name );

		if ( dlerror( ) != NULL )     /* function not found in library ? */
			continue;

		/* Utter strong warning and don't load if function would overload
		   an already loaded (i.e. non-built-in) function */

		if ( num >= Num_Def_Func && Fncts[ num ].fnct != NULL )
		{
			eprint( SEVERE, " Function `%s()' found in module `%s.so' has "
					"already been loaded'.", Fncts[ num ].name, dev->name );
			continue;
		}

		/* Allow overloading of built-in functions - but only once, next time
		   print severe warning and don't overload! */

		if ( num < Num_Def_Func && Fncts[ num ].fnct != NULL )
		{
			if ( Fncts[ num ].fnct != Def_Fncts[ num ].fnct )
			{
				eprint( SEVERE, "  Built-in function `%s()' found in module "
						"`%s.so' has already been overloaded.",
						Fncts[ num ].name, dev->name );
				continue;
			}

			eprint( WARN, "  Overloading built-in function `%s()' with "
					"function from module `%s.so'.",
					Fncts[ num ].name, dev->name );
		}

		Fncts[ num ].fnct = cur;
	}
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
			eprint( SEVERE, "Initialisation of test run failed for "
					"module `%s'.", cd->name );
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
			eprint( SEVERE, "Final checks after test run failed for module "
					"`%s'.", cd->name );
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
			eprint( SEVERE, "Initialisation of experiment failed for "
					"module `%s'.", cd->name );
}


/*--------------------------------------------------------------------*/
/* Functions runs the end-of-experiment hook functions of all modules */
/*--------------------------------------------------------------------*/

void run_end_of_exp_hooks( void )
{
	Device *cd;


	/* Each of the end-of-experiment hooks must be run to get all instruments
	   back in a usable state, even if the function fails for one of them */

	for ( cd = Device_List; cd != NULL; cd = cd->next )
	{
		TRY
		{
			if ( cd->is_loaded && cd->driver.is_end_of_exp_hook &&
				 ! cd->driver.end_of_exp_hook( ) )
				eprint( SEVERE, "Resetting module `%s' after experiment "
						"failed.", cd->name );
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
		if ( cd->is_loaded && ! strcmp( cd->name, from ) )
			break;

	if ( cd == NULL )                    /* library not found ? */
		return LIB_ERR_NO_LIB;

	/* Try to load the symbol */

	*symbol_ptr = dlsym( cd->driver.handle, symbol );

	if ( dlerror( ) != NULL )            /* symbol not found in library ? */
		return LIB_ERR_NO_SYM;

	return LIB_OK;
}


/*-------------------------------------------------------------*/
/* The function runs the exit hook functions for a modules (if */
/* this hasn't already been done and if there exists one) and  */
/* than closes the connection to the modules.                  */
/*-------------------------------------------------------------*/

void unload_device( Device *dev )
{

	if ( ! dev->is_loaded )
		return;

	if ( ! exit_hooks_are_run && dev->driver.is_exit_hook )
	{
		TRY                             /* catch exceptions from the exit   */
		{                               /* hooks, we've got to run them all */
			dev->driver.exit_hook( );
			TRY_SUCCESS;
		}
	}
	
	dlclose( dev->driver.handle );
}
