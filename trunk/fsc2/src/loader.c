/*
  $Id$
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
static void add_function( int index, void *new_func, Device *new_dev );
static int func_cmp( const void *a, const void *b );


/*------------------------------------------------------------------------*/
/* Function to be called after the DEVICES section is read in. It loads   */
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

	/* Because some multiply defined functions may have been added resort
	   the function list to make searching via bsearch() possible */

	qsort( Fncts, Num_Func, sizeof( Func ), func_cmp );

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
			eprint( WARN, "Initialisation of module `%s.so' failed.\n",
					cd->name );

		if ( need_GPIB == UNSET && saved_need_GPIB == SET )
			need_GPIB = SET;
		for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
			if ( need_Serial_Port[ i ] == UNSET &&
				 saved_need_Serial_Port[ i ] == SET )
				need_Serial_Port[ i ] = SET;
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
			 ! strcmp( strchr( cd->name, '/' ) == NULL ?
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

	dev->driver.handle = dlopen( lib_name, RTLD_LAZY );

	if ( dev->driver.handle == NULL )
		dev->driver.handle = dlopen( strrchr( lib_name, '/' ) + 1, RTLD_LAZY );

	if ( dev->driver.handle == NULL )
	{
		strcpy( lib_name, dev->name );
		strcat( lib_name, ".so" );
		dev->driver.handle = dlopen( lib_name, RTLD_LAZY );
	}

	if ( dev->driver.handle == NULL )
	{
		eprint( FATAL, "Can't open module for device `%s'.\n", 
				strchr( dev->name, '/' ) == NULL ?
				dev->name : strrchr( dev->name, '/' ) + 1 );
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
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void resolve_hook_functions( Device *dev, char *dev_name )
{
	char *hook_func_name;


	dev->driver.is_init_hook =
		dev->driver.is_test_hook =
			dev->driver.is_end_of_test_hook =
				dev->driver.is_exp_hook =
					dev->driver.is_end_of_exp_hook =
						dev->driver.is_exit_hook = UNSET;

	/* If there is function with the name of the library file and the
	   appended string "_init_hook" store it and set corresponding flag
	   (the string will be reused for the other hook functions, so make
	   it long enough that the longest name will fit into it) */

	hook_func_name = get_string( strlen( dev_name ) + 18 );
	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_init_hook" );	

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.init_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_init_hook = SET;

	/* Get test hook function if available */
	
	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_test_hook" );	

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.test_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_test_hook = SET;

	/* Get end-of-test hook function if available */
	
	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_end_of_test_hook" );	

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.end_of_test_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_end_of_test_hook = SET;

	/* Get pre-experiment hook function if available */
	
	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_exp_hook" );	

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.exp_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_exp_hook = SET;

	/* Get end-of-experiment hook function if available */

	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_end_of_exp_hook" );	

	dlerror( );           /* make sure it's NULL before we continue */
	dev->driver.end_of_exp_hook = dlsym( dev->driver.handle, hook_func_name );
	if ( dlerror( ) == NULL )
		dev->driver.is_end_of_exp_hook = SET;

	/* Finally check if there's also an exit hook function */

	strcpy( hook_func_name, dev_name );
	strcat( hook_func_name, "_exit_hook" );	

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
	char buf[ 100 ];
	char *temp;
	long len;


	for ( num = 0; num < Num_Func; num++ )
	{
		/* Don't try to load functions that are not listed in `Functions' */

		if ( ! Fncts[ num ].to_be_loaded )
			continue;

		dlerror( );                /* make sure it's NULL before we continue */

		/* If the functions name is still the original one try it, otherwise
		   first remove the extension with the '#' and the device count */

		if ( ( temp = strchr( Fncts[ num ].name, '#' ) ) == NULL )
			 cur = dlsym( dev->driver.handle, Fncts[ num ].name );
		else
		{
			len = temp - Fncts[ num ].name;
			temp = T_malloc( len + 1 );
			strncpy( temp, Fncts[ num ].name, len );
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

		if ( Fncts[ num ].fnct == NULL )
		{
			Fncts[ num ].fnct = cur;
			Fncts[ num ].device = ( void * ) dev;
			if ( dev->count != 1 )
			{
				snprintf( buf, 100, "#%d", dev->count );
				new_func_name = T_malloc(   strlen( Fncts[ num ].name )
										  + strlen( buf ) + 1 );
				strcpy( new_func_name, Fncts[ num ].name );
				strcat( new_func_name, buf );
				T_free( ( char * ) Fncts[ num ].name );
				Fncts[ num ].name = new_func_name;
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

static void add_function( int index, void *new_func, Device *new_dev )
{
	int i;
	char *new_func_name;
	char buf[ 100 ];
	long len;
	char *temp;


	/* Because, when adding a multiple defined function, it is appended to
	   the function list, the function just added will be found when running
	   through the list in resolve_functions(). This can be easily recognized
	   because the 'new' function is from the current library, which can
	   never happen (or the linker would complain about multiple defined
	   functions). So, if the function has already been defined in the
	   current library we just return. */

	if ( Fncts[ index ].device == new_dev )
		return;

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
		for ( i = 0; i < Num_Func; i++ )
			if ( Fncts[ i ].device == new_dev )
			{
				snprintf( buf, 100, "#%d", new_dev->count );
				new_func_name = T_malloc(   strlen( Fncts[ i ].name )
										  + strlen( buf ) );
				strcpy( new_func_name, Fncts[ i ].name );
				strcat( new_func_name, buf );
				T_free( ( char * ) Fncts[ i ].name );
				Fncts[ i ].name = new_func_name;
			}
	}
	else
	{
		if ( ( temp = strchr( Fncts[ index ].name, '#' ) ) != NULL 
			 && atoi( temp + 1 ) >= new_dev->count )
		{
			new_dev->count++;
			for ( i = 0; i < Num_Func; i++ )
				if ( Fncts[ i ].device == new_dev )
				{
					len = strchr( Fncts[ i ].name, '#' ) - Fncts[ i ].name;
					snprintf( buf, 100, "#%d", new_dev->count );
					new_func_name = T_malloc( len + strlen( buf ) + 1 );
					strncpy( new_func_name, Fncts[ i ].name, len );
					strcpy( new_func_name + len, buf );
					T_free( ( char * ) Fncts[ i ].name );
					Fncts[ i ].name = new_func_name;
				}
		}
	}

	/* Add an entry for the new function to the list of functions */

	Fncts = T_realloc( Fncts, ( Num_Func + 2 ) * sizeof( Func ) );
	memcpy( Fncts + Num_Func + 1, Fncts + Num_Func, sizeof( Func ) );
	memcpy( Fncts + Num_Func, Fncts + index, sizeof( Func ) );
	
	Fncts[ Num_Func ].fnct = new_func;
	Fncts[ Num_Func ].device = ( void * ) new_dev;
	snprintf( buf, 100, "#%d", new_dev->count );
	
	if ( ( temp = strchr( Fncts[ index ].name, '#' ) ) == NULL )
	{
		Fncts[ Num_Func ].name = T_malloc(   strlen( Fncts[ index ].name )
										   + strlen( buf ) + 1 );
		strcpy( ( char * ) Fncts[ Num_Func ].name, Fncts[ index ].name );
		strcat( ( char * ) Fncts[ Num_Func ].name, buf );
	}
	else
	{
		len = temp - Fncts[ index ].name;
		Fncts[ Num_Func ].name = T_malloc( len + strlen( buf ) + 1 );
		strncpy( ( char * ) Fncts[ Num_Func ].name, Fncts[ index ].name, len );
		strcpy( ( char * ) Fncts[ Num_Func ].name + len, buf );
	}

	Num_Func++;
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
			eprint( SEVERE, "Final checks after test run failed for module "
					"`%s'.\n", cd->name );
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
					"module `%s'.\n", cd->name );
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
						"failed.\n", cd->name );
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
			 ( ! strcmp( cd->name, from ) || 
			   ( strchr( cd->name, '/' ) != NULL &&
				 ! strcmp( strrchr( cd->name, '/' ) + 1, from ) ) ) )
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
