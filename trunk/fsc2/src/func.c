/*
   $Id$
*/


#include <dlfcn.h>

#include "fsc2.h"



static void f_wait_alarm_handler( int sig_type );

static int num_def_func;    /* number of built-in functions */
static int num_func;        /* number of built-in  and listed functions */
static Func *fncts;         /* structure for list of functions */



/* When in the input an identifier is found it is always tried first if the
   identifier is a function by a call to func_get(). If it is a function, a
   pointer to a temporary variable on the stack of type FUNC is returned.
   The variable's structure contains beside a pointer to the function a copy
   of the name of the function and the number of arguments and the access flag
   (the access flag tells in which sections the function may be used).

   The following input should result in as may variables on the stack as there
   are function arguments. When the argument list ends, func_call() is called.
   This function first checks if there are at least as many variables on the
   stack as the function needs. If there are more arguments than needed, a
   warning is printed and the superfluous variables (i.e. the last ones) are
   removed. The only exception are functions with a variable number of
   arguments, e.g. the print() function. For these functions a negative number
   of arguments is specified and no checking can be done.

   As the next step the function is called. The function gets passed a pointer
   to the first argument on the stack. It has to check by itself that the
   variables have the correct type and are reasonable. Each function has to
   push its result onto the stack. This can be either a simple integer or
   float variable (i.e. of type INT_VAR or FLOAT_VAR) or a transient array.
   In the later case the function has to allocate memory for the array, set
   its elements and push a variable of either INT_TRANS_ARR or FLOAT_TRANS_ARR
   onto the variable stack with the pointer to the array and its length (as a
   long!) as additional arguments.

   On return func_call() will remove all arguments from the stack as well as
   the variable for the function. Again, for functions with a variable number
   of arguments removing the arguments is impossible - the function has to do
   it by itself. Finally the result of the function is pushed onto the stack.
*/


Var *f_int( Var *v  );
Var *f_float( Var *v  );
Var *f_round( Var *v  );
Var *f_floor( Var *v  );
Var *f_ceil( Var *v  );
Var *f_abs( Var *v );
Var *f_sin( Var *v  );
Var *f_cos( Var *v  );
Var *f_tan( Var *v  );
Var *f_asin( Var *v  );
Var *f_acos( Var *v  );
Var *f_atan( Var *v  );
Var *f_sinh( Var *v  );
Var *f_cosh( Var *v  );
Var *f_tanh( Var *v  );
Var *f_exp( Var *v  );
Var *f_ln( Var *v  );
Var *f_log( Var *v  );
Var *f_sqrt( Var *v  );
Var *f_print( Var *v  );
Var *f_wait( Var *v  );
Var *f_init_display( Var *v );


static Func def_fncts[ ] =              /* List of built-in functions */
{
	{ "int",          f_int,           1, ACCESS_ALL_SECTIONS, 0 },
	{ "float",        f_float,         1, ACCESS_ALL_SECTIONS, 0 },
	{ "round",        f_round,         1, ACCESS_ALL_SECTIONS, 0 },
	{ "floor",        f_floor,         1, ACCESS_ALL_SECTIONS, 0 },
	{ "ceil",         f_ceil,          1, ACCESS_ALL_SECTIONS, 0 },
	{ "abs",          f_abs,           1, ACCESS_ALL_SECTIONS, 0 },
	{ "sin",          f_sin,           1, ACCESS_ALL_SECTIONS, 0 },
	{ "cos",          f_cos,           1, ACCESS_ALL_SECTIONS, 0 },
	{ "tan",          f_tan,           1, ACCESS_ALL_SECTIONS, 0 },
	{ "asin",         f_asin,          1, ACCESS_ALL_SECTIONS, 0 },
	{ "acos",         f_acos,          1, ACCESS_ALL_SECTIONS, 0 },
	{ "atan",         f_atan,          1, ACCESS_ALL_SECTIONS, 0 },
	{ "sinh",         f_sinh,          1, ACCESS_ALL_SECTIONS, 0 },
	{ "cosh",         f_cosh,          1, ACCESS_ALL_SECTIONS, 0 },
	{ "tanh",         f_tanh,          1, ACCESS_ALL_SECTIONS, 0 },
	{ "exp",          f_exp,           1, ACCESS_ALL_SECTIONS, 0 },
	{ "ln",           f_ln,            1, ACCESS_ALL_SECTIONS, 0 },
	{ "log",          f_log,           1, ACCESS_ALL_SECTIONS, 0 },
	{ "sqrt",         f_sqrt,          1, ACCESS_ALL_SECTIONS, 0 },
	{ "print",        f_print,        -1, ACCESS_ALL_SECTIONS, 0 },
	{ "wait",         f_wait,          1, ACCESS_ALL_SECTIONS, 0 },
	{ "init_display", f_init_display, -1, ACCESS_ALL_SECTIONS, 0 },
	{ NULL,           NULL,            0, 0,                   0 }  
	                                     /* marks last entry, don't remove ! */
};


/*--------------------------------------------------------------------*/
/* Function parses the function data base in `Functions' and makes up */
/* a complete list of all built-in and user-supplied functions.       */
/*--------------------------------------------------------------------*/

bool functions_init( void )
{
	/* count number of built-in functions */

	for ( num_def_func = 0; def_fncts[ num_def_func ].fnct != NULL;
		  num_def_func++ )
		;

	num_func = num_def_func;

	/* 1. Get new memory for the functions structures and copy the built in
	      functions into it.
	   2. Parse the function name data base `Functions' where all additional
	      functions have to be listed.
	*/

	TRY
	{
		fncts = NULL;
		fncts = T_malloc( ( num_def_func + 1 ) * sizeof( Func ) );
		memcpy( fncts, def_fncts, ( num_def_func + 1 ) * sizeof( Func ) );
		func_list_parse( &fncts, num_def_func, &num_func );
   		TRY_SUCCESS;
	}
	OTHERWISE
	{
		functions_exit( );
		return FAIL;
	}

	return OK;
}


/*-----------------------------------------------------------*/
/* Function gets rid of all loaded loaded functions and also */
/* unlinks the library files.                                */
/*-----------------------------------------------------------*/

void functions_exit( void )
{
	int i;

	if ( fncts == NULL )
		return;

	/* Get rid of the structures for the functions */

	for ( i = num_def_func; i < num_func; i++ )
		if ( fncts[ i ].name != NULL )
			T_free( ( char * ) fncts[ i ].name );
	T_free( fncts );
	fncts = NULL;
}


/*------------------------------------------------------------------------*/
/* Function to be called after the DEVICES section is read in: It links   */
/* the library files and tries to resolve the references to the functions */
/* listed in `Files' and stores pointers to the functions in `Fncts'.     */
/*------------------------------------------------------------------------*/

void load_all_drivers( void )
{
	Device *cd;


	/* Treat "User_Functions" also as a kind of device driver and append */
	/* the device structure at the end of the list opf devices*/

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
/* adding the extension `so') and then tries to find still unresolved    */
/* references to functions listed in the function data base `Functions'. */
/*-----------------------------------------------------------------------*/

void load_functions( Device *dev )
{
	int num;
	char *lib_name;
	char *hook_func;
	void *cur;


	/* Put together name of library to be loaded */

	lib_name = get_string( strlen( dev->name ) + 3 );
	strcpy( lib_name, dev->name );
	strcat( lib_name, ".so" );

	/* Increase memory allocated for library handles and try to open
	   dynamically loaded library */

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
	dev->driver.is_init_hook = UNSET;
	dev->driver.is_test_hook = UNSET;
	dev->driver.is_exp_hook = UNSET;
	dev->driver.is_exit_hook = UNSET;

	/* If there is function with the name of the library file and the
	   appended string "_init_hook" store it and set corresponding flag */

	hook_func = T_malloc( strlen( dev->name ) + 11 );
	strcpy( hook_func, dev->name );
	strcat( hook_func, "_init_hook" );	

	dev->driver.init_hook = dlsym( dev->driver.handle, hook_func );
	if ( dlerror( ) == NULL )
		dev->driver.is_init_hook = SET;
	T_free( hook_func );

	/* Get test hook function if available */
	
	hook_func = T_malloc( strlen( dev->name ) + 11 );
	strcpy( hook_func, dev->name );
	strcat( hook_func, "_test_hook" );	

	dev->driver.test_hook = dlsym( dev->driver.handle, hook_func );
	if ( dlerror( ) == NULL )
		dev->driver.is_test_hook = SET;
	T_free( hook_func );

	/* Get pre-experiment hook function if available */
	
	hook_func = T_malloc( strlen( dev->name ) + 10 );
	strcpy( hook_func, dev->name );
	strcat( hook_func, "_exp_hook" );	

	dev->driver.exp_hook = dlsym( dev->driver.handle, hook_func );
	if ( dlerror( ) == NULL )
		dev->driver.is_exp_hook = SET;
	T_free( hook_func );

	/* Finally check if there's also an exit hook function */

	hook_func = T_malloc( strlen( dev->name ) + 11 );
	strcpy( hook_func, dev->name );
	strcat( hook_func, "_exit_hook" );	

	dev->driver.exit_hook = dlsym( dev->driver.handle, hook_func );
	if ( dlerror( ) == NULL )
		dev->driver.is_exit_hook = SET;
	T_free( hook_func );

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
		   just print strong warning and do nothing */

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



/*------------------------------------------------------------------------*/
/* Function tries to find a function in the list of built-in and loaded   */
/* functions. If it does it creates a new variable on the variables stack */
/* with a pointer to the function and returns a pointer to the variable.  */
/* If the function can't be found it returns a NULL pointer.              */
/*------------------------------------------------------------------------*/

Var *func_get( const char *name, int *access )
{
	int i;
	Var *ret;


	/* try to find the function by its name and if found create a variable
	   on the variable stack with a pointer to the actual function and the
	   number of arguments. Also copy the functions name and access flag. */

	for ( i = 0; i < num_func; i++ )
	{
		if ( fncts[ i ].name != NULL && ! strcmp( fncts[ i ].name, name ) )
		{
			if ( fncts[ i ].fnct == NULL )
			{
				eprint( FATAL, "%s:%ld: Function `%s' has not (yet) been "
                        "loaded.\n", Fname, Lc, fncts[ i ].name );
				THROW( EXCEPTION );
			}
						
			ret = vars_push( FUNC, fncts[ i ].fnct );
			ret->name = get_string_copy( name );
			ret->dim = fncts[ i ].nargs;
			*access = fncts[ i ].access_flag;
			return ret;
		}
	}

	return NULL;
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

Var *func_call( Var *f )
{
	Var *ap;
	Var *apn;
	Var *ret;
	int ac;
	int i;
	

	/* Check that it's really a function variable - you can never be sure
	   someone isn't trying to be using some undocumented features but didn't
	   got ot right. */

	if ( f->type != FUNC )
	{
		eprint( FATAL, "%s:%ld: Variable passed to `func_call()' doesn't "
				"have type `FUNC'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	for ( i = 0; i < num_func; i++ )
		if ( fncts[ i ].fnct == f->val.fnct )
			break;
	
	if ( i == num_func )
	{
		eprint( FATAL, "%s:%ld: Variable passed to `func_call()' is not "
				"a valid function.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* If the number of function arguments isn't negative (indicating a
	   variable number of arguments) count number of variables on the stack */

	if ( f->dim >= 0 )
	{
		for ( ac = 0, ap = f->next; ap != NULL; ++ac, ap = ap->next )
			;

		/* If there are too many arguments utter a warning and remove the
		   superfluous variables */

		if ( ac > f->dim )
		{
			eprint( SEVERE, "%s:%ld: Too many arguments for function `%s'.\n",
					Fname, Lc, f->name );

			for ( ac = 0, ap = f->next; ac < f->dim; ++ac, ap = ap->next )
				;
			for ( ; ap != NULL; ap = apn )
			{
				apn = ap->next;
				vars_pop( ap );
			}
		}

		/* Less arguments on the stack than needed by the function is a fatal
		   error. */

		if ( ac < f->dim )
		{
			eprint( FATAL, "%s:%ld: Function `%s' needs %d arguments but "
					"found only %d.\n",
					Fname, Lc, f->name, f->dim, ac );
			THROW( EXCEPTION );
		}
	}

	/* Now call the function */

	if ( ac != 0 )
		ret = ( *f->val.fnct )( f->next );
	else
		ret = ( *f->val.fnct )( NULL );

	/* Finally do clean up, remove the variable with the function and all
	   parameters - just keep the return value */

	for ( ap = f; ap != ret; ap = apn )
	{
		apn = ap->next;
		vars_pop( ap );
	}

	return ret;
}


/****************************************************************/
/*                                                              */
/*     Here follow the definitions of all built-in functions    */
/*                                                              */
/****************************************************************/



/* Conversion float to integer (result is integer) */

Var *f_int( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
		return vars_push( INT_VAR, ( long ) v->val.dval );
}


/* Conversion int to floating point (result is float) */

Var *f_float( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, ( double ) v->val.lval );
	else
		return vars_push( FLOAT_VAR, v->val.dval );
}


/* Rounding of floating point numbers (result is integer) */

Var *f_round( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
		return vars_push( INT_VAR,   ( long ) ( 2 * v->val.dval )
                                   - ( long ) v->val.dval );
}


/* Floor value (result is integer) */

Var *f_floor( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
		return vars_push( INT_VAR, ( long ) floor( v->val.dval ) );
}


/* Ceiling value (result is integer) */

Var *f_ceil( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
		return vars_push( INT_VAR, ( long ) ceil( v->val.dval ) );
}


/* abs of value (result has same as type argument) */

Var *f_abs( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, labs( v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, fabs( v->val.dval ) );
}


/* sin of argument (in radian) (result is float) */

Var *f_sin( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, sin( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, sin( v->val.dval ) );
}


/* cos of argument (in radian) (result is float) */

Var *f_cos( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, cos( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, cos( v->val.dval ) );
}


/* tan of argument (in radian) (result is float) */

Var *f_tan( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, tan( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, tan( v->val.dval ) );
}


/* asin (in radian) of argument (with -1 <= x <= 1) (result is float) */

Var *f_asin( Var *v )
{
	double arg;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		arg = ( double ) v->val.lval;
	else
		arg = v->val.dval;

	if ( fabs( arg ) > 1.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `asin' is out of "
				"range.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, asin( arg ) );
}


/* acos (in radian) of argument (with -1 <= x <= 1) (result is float) */

Var *f_acos( Var *v )
{
	double arg;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		arg = ( double ) v->val.lval;
	else
		arg = v->val.dval;

	if ( fabs( arg ) > 1.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `acos' is out of "
				"range.\n",  Fname, Lc );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, acos( arg ) );
}


/* atan (in radian) of argument (result is float) */

Var *f_atan( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, atan( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, atan( v->val.dval ) );
}


/* sinh of argument (result is float) */

Var *f_sinh( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, sinh( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, sinh( v->val.dval ) );
}


/* cosh of argument (result is float) */

Var *f_cosh( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, cosh( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, cosh( v->val.dval ) );
}


/* tanh of argument (result is float) */

Var *f_tanh( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, tanh( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, tanh( v->val.dval ) );
}


/* exp of argument (result is float) */

Var *f_exp( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, exp( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, exp( v->val.dval ) );
}


/* ln of argument (with x > 0) (result is float) */

Var *f_ln( Var *v )
{
	double arg;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		arg = ( double ) v->val.lval;
	else
		arg = v->val.dval;

	if ( arg <= 0.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `ln' is out of "
				"range.\n", Fname, Lc );
		THROW( EXCEPTION );
	}
	return vars_push( FLOAT_VAR, log( arg ) );
}


/* log of argument (with x > 0) (result is float) */

Var *f_log( Var *v )
{
	double arg;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		arg = ( double ) v->val.lval;
	else
		arg = v->val.dval;

	if ( arg <= 0.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `log' is out of "
				"range.\n", Fname, Lc );
		THROW( EXCEPTION );
	}
	return vars_push( FLOAT_VAR, log10( arg ) );
}


/* sqrt of argument (with x >= 0) (result is float) */

Var *f_sqrt( Var *v )
{
	double arg;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		arg = ( double ) v->val.lval;
	else
		arg = v->val.dval;

	if ( arg < 0.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `sqrt' is negative.\n", 
				Fname, Lc );
		THROW( EXCEPTION );
	}
	return vars_push( FLOAT_VAR, sqrt( arg ) );
}


/* Prints variable number of arguments using a format string supplied as
   the first argument. Types of arguments to be printed are integer and
   float data and strings. To get a value printed the format string has
   to contain the character `#'. The escape character is the backslash,
   with a double backslash for printing one backslash. Beside the `\#'
   combination to print a `#' most of the escape sequences from printf()
   ('\a', '\b', '\f', '\n', '\r', '\t', '\v' and '\"') do function.

   The function returns the number of variables it printed, not counting
   the format string. */

Var *f_print( Var *v )
{
	char *fmt;
	char *cp;
	char *ep;
	Var *cv;
	char *sptr;
	int in_format,
		on_stack,
		s;
	bool print_anyway = UNSET;

	
	/* a call to print() without any argument is legal but rather
	   unreasonable... */

	if ( v == NULL )
		return vars_push( INT_VAR, 0 );

	/* make sure the first argument is a string */

	vars_check( v, STR_VAR );
	sptr = cp = v->val.sptr;
	if ( *sptr == '\\' && *( sptr + 1 ) == 'T' )
	{
		sptr += 2;
		print_anyway = SET;
	}

	/* count the number of specifiers `#' in the format string but don't count
	   escaped `#' (i.e "\#") */

	for ( in_format = 0, sptr; ( cp = strchr( cp, '#' ) ) != NULL;
		  ++cp )
		if ( cp == fmt || *( cp - 1 ) != '\\' )
			in_format++;

	/* check and count number of variables on the stack following the format
	   string */

	for  ( cv = v->next, on_stack = 0; cv != NULL; ++on_stack, cv = cv->next )
		vars_check( cv, INT_VAR | FLOAT_VAR | STR_VAR );

	/* check that there are at least as many variables are on the stack 
	   as there specifiers in the format string */

	if ( on_stack < in_format )
		eprint( SEVERE, "%s:%ld: Less data than format descriptors in print() "
				"format string.\n", Fname, Lc );

	/* utter a warning if there are more data than format descriptors */

	if ( on_stack > in_format )
		eprint( SEVERE, "%s:%ld: More data than format descriptors in print() "
				"format string.\n", Fname, Lc );

	/* count number of `#' */

	for ( cp = sptr, s = 0; *cp != '\0' ; ++cp )
		if ( *cp == '#' )
			s++;

	/* get string long enough to replace each `#' by a 4-char sequence */

	fmt = get_string( strlen( sptr ) + 4 * s + 2 );
	strcpy( fmt, sptr );

	for ( cp = fmt; *cp != '\0' ; ++cp )
	{
		/* skip normal characters */

		if ( *cp != '\\' && *cp != '#' )
			continue;

		/* convert format descriptor (unescaped `#') to 4 \x01 */

		if ( *cp == '#' )
		{
			for ( ep = fmt + strlen( fmt ) + 1; ep != cp; --ep )
				*( ep + 4 ) = *ep;
			*cp++ = '\x01';
			*cp++ = '\x01';
			*cp++ = '\x01';
			*cp++ = '\x01';
			*cp = '\x01';
			continue;
		}

		/* replace escape sequences */

		switch ( *( cp + 1 ) )
		{
			case '#' :
				*cp = '#';
				break;

			case 'a' :
				*cp = '\a';
				break;

			case 'b' :
				*cp = '\b';
				break;

			case 'f' :
				*cp = '\f';
				break;

			case 'n' :
				*cp = '\n';
				break;

			case 'r' :
				*cp = '\r';
				break;

			case 't' :
				*cp = '\t';
				break;

			case 'v' :
				*cp = '\v';
				break;

			case '\\' :
				*cp = '\\';
				break;

			case '\"' :
				*cp = '"';
				break;

			default :
				eprint( WARN, "%s:%ld: Unknown escape sequence \\%c in "
						"print() format string.\n", Fname, Lc, *( cp + 1 ) );
				*cp = *( cp + 1 );
				break;
		}
		
		for ( ep = cp + 1; *ep != '\0'; ep++ )
			*ep = *( ep + 1 );
	}

	/* now lets start printing... */

	cp = fmt;
	strcat( cp, "\x7E" );
	cv = v->next;
	while ( ( ep = strstr( cp, "\x01\x01\x01\x01\x01" ) ) != NULL )
	{
		if ( cv != NULL )      /* skip printing if there are not enough data */
		{
			if ( ! TEST_RUN || print_anyway )
				switch ( cv->type )
				{
					case INT_VAR :
						strcpy( ep, "%ld\x7F" );
						eprint( NO_ERROR, cp, cv->val.lval );
						break;

					case FLOAT_VAR :
						strcpy( ep, "%#g\x7F" );
						eprint( NO_ERROR, cp, cv->val.dval );
						break;

					case STR_VAR :
						strcpy( ep, "%s\x7F" );
						eprint( NO_ERROR, cp, sptr );
						break;
				}

			cv = cv->next;
		}

		cp = ep + 5;
	}

	if ( ! TEST_RUN || print_anyway ) 
		eprint( NO_ERROR, cp );

	/* finally free the copy of the format string and return number of
	   printed varaibles */

	T_free( fmt );
	return vars_push( INT_VAR, in_format );
}


/*-------------------------------------------------------------------*/
/* f_wait() is kind of a version of usleep() that isn't disturbed by */
/* signals - only exception: If a DO_QUIT signal is delivered to the */
/* caller of f_wait() (i.e. the child) it returns immediately.       */
/* ->                                                                */
/*  * number of nanoseconds to sleep                                 */
/*-------------------------------------------------------------------*/

Var *f_wait( Var *v )
{
	struct itimerval sleepy;
	long how_long;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	if ( v->type == INT_VAR )
		how_long = v->val.lval;
	else
		how_long = rnd( v->val.dval );

	if ( how_long < 0 )
	{
		eprint( FATAL, "%s:%ld: Negative time or more than 2s for `wait()' "
				"function.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* set everything up for sleeping */

	sleepy.it_interval.tv_sec = sleepy.it_interval.tv_usec = 0;
	sleepy.it_value.tv_sec = how_long / 1000000000L;
	sleepy.it_value.tv_usec =
		( how_long - sleepy.it_value.tv_sec * 1000000000L ) / 1000;

	signal( SIGALRM, f_wait_alarm_handler );
	setitimer( ITIMER_REAL, &sleepy, NULL );

	/* wake up only after sleeping time or if DO_QUIT signal was received */

	do
	{
		pause( );
		getitimer( ITIMER_REAL, &sleepy );
	} while ( ! do_quit &&
			  ( sleepy.it_value.tv_sec > 0 || sleepy.it_value.tv_usec > 0 ) );

	signal( SIGALRM, SIG_DFL );

	/* return 1 if end of sleping time was reached, 0 if do_quit was set */

	return vars_push( INT_VAR, do_quit ? 0 : 1 );
}


/*--------------------------------------------------------------------------*/
/* f_wait_alarm_handler() accepts the alarm signal, it doesn't got to do    */
/* anything since the alarm signal will wake up f_wait() from its pause() - */
/* but without it the alarm signal would kill the process, i.e. the child.  */
/*--------------------------------------------------------------------------*/

void f_wait_alarm_handler( int sig_type )
{
	if ( sig_type != SIGALRM )
		return;

	signal( SIGALRM, f_wait_alarm_handler );
}


Var *f_init_display( Var *v )
{
	long dim;
	char *l1, *l2;

	if ( ! TEST_RUN && I_am == CHILD )
	{
		int r;
		r = show_choices( "Please set sweep speed on magnet front\n"
						  "panel to maximum value of 6666 Oe/min.\n"
						  "Also make sure remote control is enabled !",
						  2, "Abort", "Done", "", 2 );
		fprintf( stderr, "CHILD: Got a %d from show_choices().\n", r );
		return vars_push( INT_VAR, 1 );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		dim = v->val.lval;
	else
		dim = rnd( v->val.dval );

	if ( v->next != NULL )
	{
		vars_check( v->next, STR_VAR );
		l1 = v->next->val.sptr;

		if ( v->next->next != NULL )
		{
			vars_check( v->next->next, STR_VAR );
			l2 = v->next->next->val.sptr;

		}
		else
			l2 = NULL;
	}
	else
		l1 = l2 = NULL;

	graphics_init( dim, l1, l2 );
	return vars_push( INT_VAR, 1 );
}
