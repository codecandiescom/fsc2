/*
  $Id$
*/


#include <dlfcn.h>
#include "fsc2.h"


/* Variables imported from func.c */

extern int num_def_func;    /* number of built-in functions */
extern int num_func;        /* number of built-in and listed functions */
extern Func *fncts;         /* structure for list of functions */
extern Func def_fncts[ ];   /* structures for list of built-in functions */



/*------------------------------------------------------------------------*/
/* Function to be called after the DEVICES section is read in: It links   */
/* the library files and tries to resolve the references to the functions */
/* listed in `Functions' and stores pointers to the functions in `Fncts'. */
/*------------------------------------------------------------------------*/

void load_all_drivers( void )
{
	Device *cd;


	/* Treat "User_Functions" also as a kind of device driver and append */
	/* the device structure at the end of the list of devices*/

	device_append_to_list( "User_Functions" );

	/* Link all functions from the device drivers (including "User_Functions"
	   which comes last) and try to resolve the functions from the function
	   list */

	for ( cd = Device_List; cd != NULL; cd = cd->next )
		load_functions( cd );

	/* This done we run the init hooks (if one exists) and warn if it didn't
	   return successfully (if the init hook thinks it should kill the whole
	   program it's supposed to throw an exception) */

	for ( cd = Device_List; cd != NULL; cd = cd->next )
		if ( cd->is_loaded && cd->driver.is_init_hook &&
			 ! cd->driver.init_hook( ) )
			eprint( WARN, "Initialization of module `%s.so' failed.\n",
					cd->name );
}


/*--------------------------------------------------------------------*/
/* Function tests if a device driver, passed to the function by name, */
/* is loaded.                                                         */
/*--------------------------------------------------------------------*/

bool exist_device( const char *name )
{
	Device *cd;

	for ( cd = Device_List; cd != NULL; cd = cd->next )
		if ( cd->is_loaded && ! strcmp( cd->name, name ) )
			return OK;

	return FAIL;
}


/*---------------------------------------------------------------------*/
/* Routine tests if a function, passed to the routine by name, exists. */
/*---------------------------------------------------------------------*/

bool exist_function( const char *name )
{
	int i;


	for ( i = 0; i < num_func; i++ )
		if ( fncts[ i ].name != NULL &&
			 ! strcmp( fncts[ i ].name, name ) &&
			 fncts[ i ].fnct != NULL )			 
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
	void *cur;


	/* Put together name of library to be loaded */

	lib_name = get_string( strlen( dev->name ) + 3 );
	strcpy( lib_name, dev->name );
	strcat( lib_name, ".so" );

	/* Increase memory allocated for library handles and try to open
	   the dynamically loaded library */

	dev->driver.handle = dlopen( lib_name, RTLD_LAZY );
	T_free( lib_name );
	if ( dev->driver.handle == NULL )
	{
		if ( ! strcmp( dev->name, "User_Functions" ) )
			eprint( FATAL, "Can't open module `User_Functions.so'.\n" );
		else
			eprint( FATAL, "Can't open module for device `%s'.\n",
					dev->name );
		THROW( EXCEPTION );
	}

	dev->is_loaded = SET;
	dev->driver.is_init_hook = dev->driver.is_test_hook =
		dev->driver.is_exp_hook = dev->driver.is_exit_hook = UNSET;

	/* If there is function with the name of the library file and the
	   appended string "_init_hook" store it and set corresponding flag */

	hook_func_name = get_string( strlen( dev->name ) + 10 );
	strcpy( hook_func_name, dev->name );
	strcat( hook_func_name, "_init_hook" );	

	dev->driver.init_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_init_hook = SET;

	/* Get test hook function if available */
	
	strcpy( hook_func_name, dev->name );
	strcat( hook_func_name, "_test_hook" );	

	dev->driver.test_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_test_hook = SET;

	/* Get pre-experiment hook function if available */
	
	strcpy( hook_func_name, dev->name );
	strcat( hook_func_name, "_exp_hook" );	

	dev->driver.exp_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_exp_hook = SET;

	/* Finally check if there's also an exit hook function */

	strcpy( hook_func_name, dev->name );
	strcat( hook_func_name, "_exit_hook" );	

	dev->driver.exit_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_exit_hook = SET;
	T_free( hook_func_name );

	/* Run through all the functions in the function list and if they need
	   to be resolved try to find them in the device driver functions - check
	   that the function has not already been loaded (but overloading built-in
	   functions is acceptable). */

	eprint( NO_ERROR, "Loading functions from module `%s.so'.\n", dev->name );

	for ( num = 0; num < num_func; num++ )
	{
		/* Don't try to load functions that are not listed in `Functions' */

		if ( ! fncts[ num ].to_be_loaded )
			continue;

		cur = dlsym( dev->driver.handle, fncts[ num ].name );

		if ( dlerror( ) != NULL )     /* function not found in library ? */
			continue;

		/* Utter strong warning and don't load if function would overload
		   an already loaded (i.e. non-built-in) function */

		if ( num >= num_def_func && fncts[ num ].fnct != NULL )
		{
			eprint( SEVERE, " Function `%s()' found in module `%s.so' has "
					"already been loaded'.\n", fncts[ num ].name, dev->name );
			continue;
		}

		/* Allow overloading of built-in functions - but only once, next time
		   just print severe warning and do nothing */

		if ( num < num_def_func && fncts[ num ].fnct != NULL )
		{
			if ( fncts[ num ].fnct != def_fncts[ num ].fnct )
			{
				eprint( SEVERE, "  Built-in function `%s()' found in module "
						"`%s.so' has already been overloaded.\n",
						fncts[ num ].name, dev->name );
				continue;
			}

			eprint( NO_ERROR, "  Overloading built-in function `%s()' from "
					"module `%s.so'.\n", fncts[ num ].name, dev->name );
		}
		else
			eprint( NO_ERROR, "  Loading function `%s()'.\n",
					fncts[ num ].name );
		fncts[ num ].fnct = cur;
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
			eprint( WARN, "Initialization for test run of module `%s.so' "
					"failed.\n", cd->name );
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
			eprint( WARN, "Initialization for experiment of module `%s.so' "
					"failed.\n", cd->name );
}


/*-------------------------------------------------------*/
/* Functions runs the test hook functions of all modules */
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


/*-----------------------------------------------------------------------*/
/* This function is intended to allow user defined modules access to the */
/* symbols in another module. Probably it's a BAD thing if they do, but  */
/* sometimes it might be inevitable, so we better include this instead   */
/* of having the writer of a module trying to figure out some other and  */
/* probably more difficult or dangerous method to do it anyway.          */
/*                                                                       */
/* ->                                                                    */
/*    1. Name of the module (without the `.so' extension) the symbol is  */
/*       to be loaded from                                               */
/*    2. Name of the symbol to be loaded                                 */
/*    3. Pointer to void pointer for returning the address of the symbol */
/*                                                                       */
/* <-                                                                    */
/*    Return value indicates success or failure (see global.h for defi-  */
/*    nition of the the return codes). On success `symbol_ptr' contains  */
/*    the address of the symbol.                                         */ 
/*-----------------------------------------------------------------------*/

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
