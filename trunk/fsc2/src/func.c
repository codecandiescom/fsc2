/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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


/* When an identifier is found in the input it is always tested first if it
   is a function by a calling func_get(). If it is a function, a pointer to a
   temporary variable on the stack of type FUNC is returned.  The variable's
   structure contains a pointer to the function, a copy of the name of
   the function and the number of arguments and the access flag (the access
   flag tells in which sections the function may be used).

   The following input results in as may variables on the stack as there are
   function arguments. When the argument list ends, func_call() is called.
   This function first checks if there are at least as many variables on the
   stack as the function needs. If there are more arguments than needed, a
   warning is printed and the superfluous variables (i.e. the last ones) are
   removed. There are functions that accept a variable number of arguments,
   some with an upper limit and some without (e.g. the print() function). For
   these functions a negative number of arguments is specified and to
   distingish between them (and to switch off checking the number of arguments
   for the maximum number) for functions without an upper limit of arguments
   the number of arguments is set to 'INT_MIN'.

   As the next step the function is called. The function gets passed a pointer
   to the first argument on the stack. It has to check by itself that the
   variables have the correct type and are reasonable. Each function has to
   push its result onto the stack. This can be either a simple integer or
   float variable (i.e. of type INT_VAR or FLOAT_VAR) or a transient array.
   In the later case the function has to allocate memory for the array, set
   its elements and push a variable of either INT_ARR or FLOAT_ARR
   onto the variable stack with the pointer to the array and its length (as a
   long!) as additional arguments.

   On return func_call() will remove all arguments from the stack as well as
   the variable for the function.  */


/* The following variables are shared with loader.c which adds further
   functions from the loaded modules */

size_t Num_Func;     /* number of built-in and listed functions */
Func *Fncts;         /* structure for list of functions */


/* Both these variables are shared with 'func_util.c' */

bool No_File_Numbers;
bool Dont_Save;


/* Take care: The number of maximum parameters have to be changed for
              display() and clear_curve() if the maximum number of curves
			  (defined as MAX_CURVES in graphics.h) should ever be changed. */

Func Def_Fncts[ ] =              /* List of built-in functions */
{
	{ "int",           f_int,      		1, ACCESS_ALL,  NULL, UNSET },
	{ "float",         f_float,    		1, ACCESS_ALL,  NULL, UNSET },
	{ "round",         f_round,    		1, ACCESS_ALL,  NULL, UNSET },
	{ "floor",         f_floor,    		1, ACCESS_ALL,  NULL, UNSET },
	{ "ceil",          f_ceil,     		1, ACCESS_ALL,  NULL, UNSET },
	{ "abs",           f_abs,      		1, ACCESS_ALL,  NULL, UNSET },
	{ "sin",           f_sin,      		1, ACCESS_ALL,  NULL, UNSET },
	{ "cos",           f_cos,      		1, ACCESS_ALL,  NULL, UNSET },
	{ "tan",           f_tan,      		1, ACCESS_ALL,  NULL, UNSET },
	{ "asin",          f_asin,     		1, ACCESS_ALL,  NULL, UNSET },
	{ "acos",          f_acos,     		1, ACCESS_ALL,  NULL, UNSET },
	{ "atan",          f_atan,     		1, ACCESS_ALL,  NULL, UNSET },
	{ "sinh",          f_sinh,     		1, ACCESS_ALL,  NULL, UNSET },
	{ "cosh",          f_cosh,     		1, ACCESS_ALL,  NULL, UNSET },
	{ "tanh",          f_tanh,     		1, ACCESS_ALL,  NULL, UNSET },
	{ "asinh",         f_asinh,    		1, ACCESS_ALL,  NULL, UNSET },
	{ "acosh",         f_acosh,    		1, ACCESS_ALL,  NULL, UNSET },
	{ "atanh",         f_atanh,    		1, ACCESS_ALL,  NULL, UNSET },
	{ "exp",           f_exp,      		1, ACCESS_ALL,  NULL, UNSET },
	{ "ln",            f_ln,       		1, ACCESS_ALL,  NULL, UNSET },
	{ "log",           f_log,      		1, ACCESS_ALL,  NULL, UNSET },
	{ "sqrt",          f_sqrt,     		1, ACCESS_ALL,  NULL, UNSET },
	{ "random",        f_random,   	   -1, ACCESS_ALL,  NULL, UNSET },
	{ "grandom",       f_grand,    	   -1, ACCESS_ALL,  NULL, UNSET },
	{ "set_seed",      f_setseed,  	   -1, ACCESS_ALL,  NULL, UNSET },
	{ "time",          f_time,     		0, ACCESS_ALL,  NULL, UNSET },
	{ "date",          f_date,     		0, ACCESS_ALL,  NULL, UNSET },
	{ "delta_time",    f_dtime,    		0, ACCESS_EXP,  NULL, UNSET },
	{ "print",         f_print,   INT_MIN, ACCESS_ALL,  NULL, UNSET },
	{ "wait",          f_wait,     	    1, ACCESS_ALL,  NULL, UNSET },
	{ "init_1d",       f_init_1d,  	   -6, ACCESS_PREP, NULL, UNSET },
	{ "init_2d",       f_init_2d,  	  -10, ACCESS_PREP, NULL, UNSET },
	{ "change_scale",  f_cscale,   	   -4, ACCESS_EXP,  NULL, UNSET },
	{ "change_label",  f_clabel,   	   -3, ACCESS_EXP,  NULL, UNSET },
	{ "rescale",       f_rescale,  	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "display",       f_display,  	  -16, ACCESS_EXP,  NULL, UNSET },
	{ "clear_curve",   f_clearcv,  	   -4, ACCESS_EXP,  NULL, UNSET },
	{ "dim",           f_dim,      	    1, ACCESS_ALL,  NULL, UNSET },
	{ "size",          f_size,     	   -2, ACCESS_ALL,  NULL, UNSET },
	{ "sizes",         f_sizes,    	    1, ACCESS_ALL,  NULL, UNSET },
	{ "mean",          f_mean,     	   -3, ACCESS_ALL,  NULL, UNSET },
	{ "rms",           f_rms,      	   -3, ACCESS_ALL,  NULL, UNSET },
	{ "slice",         f_slice,    	   -3, ACCESS_ALL,  NULL, UNSET },
	{ "square",        f_square,   	    1, ACCESS_ALL,  NULL, UNSET },
	{ "int_slice",     f_islice,   	    1, ACCESS_ALL,  NULL, UNSET },
	{ "float_slice",   f_fslice,   	    1, ACCESS_ALL,  NULL, UNSET },
	{ "get_file",      f_getf,     	   -5, ACCESS_EXP,  NULL, UNSET },
	{ "open_file",     f_openf,    	   -6, ACCESS_EXP,  NULL, UNSET },
	{ "clone_file",    f_clonef,   	    3, ACCESS_EXP,  NULL, UNSET },
	{ "save",          f_save,    INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "fsave",         f_fsave,   INT_MIN, ACCESS_EXP,  NULL, UNSET },
    { "ffsave",        f_ffsave,  INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "save_program",  f_save_p,   	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "save_output",   f_save_o,   	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "save_comment",  f_save_c,   	   -4, ACCESS_EXP,  NULL, UNSET },
	{ "is_file",       f_is_file,  	    1, ACCESS_ALL,  NULL, UNSET },
	{ "layout",        f_layout,   	    1, ACCESS_EXP,  NULL, UNSET },
	{ "button_create", f_bcreate,  	   -4, ACCESS_EXP,  NULL, UNSET },
	{ "button_delete", f_bdelete, INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "button_state",  f_bstate,   	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "slider_create", f_screate,  	   -6, ACCESS_EXP,  NULL, UNSET },
	{ "slider_delete", f_sdelete, INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "slider_value",  f_svalue,   	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "input_create",  f_ocreate,  	   -5, ACCESS_EXP,  NULL, UNSET },
	{ "input_delete",  f_odelete, INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "input_value",   f_ovalue,   	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "output_create", f_ocreate,  	   -5, ACCESS_EXP,  NULL, UNSET },
	{ "output_delete", f_odelete, INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "output_value",  f_ovalue,   	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "menu_create",   f_mcreate, INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "menu_delete",   f_mdelete, INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "menu_choice",   f_mchoice,  	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "object_delete", f_objdel,  INT_MIN, ACCESS_EXP,  NULL, UNSET },
    { "hide_toolbox",  f_freeze,        1, ACCESS_EXP,  NULL, UNSET },
	{ "show_message",  f_showm,         1, ACCESS_EXP,  NULL, UNSET },
    { "draw_marker",   f_setmark,       2, ACCESS_EXP,  NULL, UNSET },
    { "clear_marker",  f_clearmark,     0, ACCESS_EXP,  NULL, UNSET },
	{ "end",           f_stopsim,  	    0, ACCESS_EXP,  NULL, UNSET },
	{ "abort",         f_abort,    	    0, ACCESS_EXP,  NULL, UNSET },
	{ NULL,             NULL,     	    0, 0,           NULL, UNSET }
	                   /* last set marks the very last entry, don't remove ! */
};


/* Locally used functions and variables */

static int func_cmp1( const void *a, const void *b );
static int func_cmp2( const void *a, const void *b );


/*--------------------------------------------------------------------*/
/* Function parses the function data base in 'Functions' and makes up */
/* a complete list of all built-in and user-supplied functions.       */
/*--------------------------------------------------------------------*/

bool functions_init( void )
{
	No_File_Numbers = UNSET;
	Dont_Save = UNSET;

	/* Count number of built-in functions */

	for ( Num_Func = 0; Def_Fncts[ Num_Func ].fnct != NULL; Num_Func++ )
		/* empty */ ;

	/*
	   1. Get new memory for the functions structures and copy the built-in
	      functions into it.
	   2. Parse the function name data base 'Functions' where all additional
	      functions have to be listed.
	   3. Sort the functions by name so that they can be found using bsearch()
	*/

	TRY
	{
		Fncts = FUNC_P T_malloc( Num_Func * sizeof *Fncts );
		memcpy( Fncts, Def_Fncts, Num_Func * sizeof *Fncts );
		qsort( Fncts, Num_Func, sizeof *Fncts, func_cmp1 );
		Num_Func = func_list_parse( &Fncts, Num_Func );
		qsort( Fncts, Num_Func, sizeof *Fncts, func_cmp1 );
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
/* Function for qsort'ing functions according to their names */
/*-----------------------------------------------------------*/

static int func_cmp1( const void *a, const void *b )
{
	return strcmp( ( ( const Func * ) a )->name,
				   ( ( const Func * ) b )->name );
}


/*-----------------------------------------------------------------*/
/* Function gets rid of all loaded functions (i.e. functions from  */
/* modules). It also closes all files opened due to user requests. */
/*-----------------------------------------------------------------*/

void functions_exit( void )
{
	size_t i;


	if ( Fncts == NULL )
		return;

	/* Get rid of the names of loaded functions (but not the built-in ones) */

	for ( i = 0; i < Num_Func; i++ )
		if ( Fncts[ i ].to_be_loaded )
			T_free( ( char * ) Fncts[ i ].name );

	Fncts = FUNC_P T_free( Fncts );

	/* Clean up the call stack */

#ifndef NDEBUG
	if ( EDL.Call_Stack != NULL )
		eprint( SEVERE, UNSET, "Internal error detected at %s:%d, call stack "
				"not empty!\n", __FILE__, __LINE__ );
#endif

	while ( call_pop( ) )
		/* empty */ ;

	No_File_Numbers = UNSET;
	Dont_Save = UNSET;
	close_all_files( );
}


/*-------------------------------------------------------------------*/
/* This function is thought for modules that just need to check if a */
/* function exists.                                                  */
/*-------------------------------------------------------------------*/

int func_exists( const char *name )
{
	return NULL != bsearch( name, Fncts, Num_Func, sizeof *Fncts, func_cmp2 );
}


/*----------------------------------------------------------------------*/
/* Function tries to find a function in the list of built-in and loaded */
/* functions. If it finds it it creates a new variable on the variables */
/* stack with a pointer to the function and returns a pointer to the    */
/* variable. If the function can't be found it returns a NULL pointer.  */
/* -> 1. Name of the function                                           */
/*    2. Pointer for returning the access flag, i.e. a flag indicating  */
/*       in which part of the EDL program the function can be used.     */
/* <- Pointer to variable on variable stack that can be used to execute */
/*    the function or NULL if function does not exist. If the function  */
/*    exists but has not been loaded an exception is thrown.            */
/*----------------------------------------------------------------------*/

Var *func_get( const char *name, int *acc )
{
	char *sec_name;
	Var *func_ptr;


	/* We have to help the writers of modules a bit: If they want a
	   pointer to a function from within the same module they don't know
	   if there are other modules with the same generic type and thus
	   have no chance to figure out if they need to append a '#' plus the
	   number of the device to the function name to get the correct
	   function within the same module.

	   We can here figure out if this is needed and which number to append
	   by checking the following:

	   1. There's still a function call that has not yet returned in which
	      case the call stack isn't empty.
	   2. The function which is still running comes from a module - in this
	      case the device member of the last call stack element isn't NULL.
	   3. The function that's still running came from a device that's not
	      the first of a set of devices with the same generic type, indicated 
		  by the dev_count member of the last call stack element being
		  larger than 1.
	   4. The functions name we got as the argument hasn't already a '#'
	      appended to it.

	   If now a function of the name passed to this function is found in
	   the module from which the call is we append the '#' plus the device
	   count number to the name of the function and use this adorned name
	   as the name of the function the caller is really looking for. */

	if ( EDL.Call_Stack != NULL && EDL.Call_Stack->device != NULL &&
		 EDL.Call_Stack->dev_count > 1 && strrchr( name, '#' ) == NULL )
	{
		dlerror( );
		dlsym( EDL.Call_Stack->device->driver.handle, name );
		if ( dlerror( ) == NULL )
		{
			sec_name = get_string( "%s#%d", name, EDL.Call_Stack->dev_count );
			TRY
			{
				func_ptr = func_get_long( sec_name, acc, SET );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( sec_name );
				RETHROW( );
			}
			T_free( sec_name );
			return func_ptr;
		}
	}

	return func_get_long( name, acc, SET );
}


/*----------------------------------------------------------------*/
/* This function is the one really used for finding a function    */
/* but with an additional argument that indicates if on failure   */
/* for existing but not loaded functions an error message is      */
/* printed and an exception is thrown or simply NULL is returned. */
/*----------------------------------------------------------------*/

Var *func_get_long( const char *name, int *acc, bool flag )
{
	Func *f;
	Var *ret;


	/* Try to find the function by its name and if found create a variable on
	   the variable stack with a pointer to the function and the number of
	   arguments. Also copy the functions name and access flag. */

	f = FUNC_P bsearch( name, Fncts, Num_Func, sizeof *Fncts, func_cmp2 );

	if ( f == NULL )             /* function not found */
		return NULL;

	if ( f->fnct == NULL )       /* function found but not loaded */
	{
		if ( flag )
		{
			print( FATAL, "%s(): Function has not been loaded.\n", f->name );
			THROW( EXCEPTION );
		}
		else                     /* some callers do their own error handling */
			return NULL;
	}

	ret = vars_push( FUNC, f );
	ret->name = T_strdup( name );
	ret->dim = f->nargs;
	if ( acc != NULL )
		*acc = f->access_flag;

	return ret;
}


/*------------------------------------------------------------*/
/* Function for bsearch to search for a function by its name. */
/*------------------------------------------------------------*/

static int func_cmp2( const void *a, const void *b )
{
	return strcmp( ( const char * ) a, ( ( const Func * ) b )->name );
}


/*----------------------------------------------------------------*/
/* This function is called to execute an EDL function. It must be */
/* able to also handle situations where, for example, an EDL      */
/* function calls another EDL function, which throws an exception */
/* that then is caught by the first function etc.                 */
/*----------------------------------------------------------------*/

Var *func_call( Var *f )
{
	Var *ap;
	Var *ret = NULL;
	long ac;
	long abs_len;
#ifndef NDEBUG
	size_t i;


	/* Check (and double-check) that it's really a function variable - one
	   can never be sure someone really got it right... */

	if ( f->type != FUNC )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

/* A weaker check than the one we're currently using....

	if ( f->val.fnct < Fncts || f->val.fnct >= Fncts + Num_Func )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
*/

	for ( i = 0; i < Num_Func; i++ )
		if ( Fncts[ i ].fnct == f->val.fnct->fnct )
			break;

	if ( i >= Num_Func )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

#endif

	/* If the number of function arguments isn't INT_MIN (indicating a
	   variable number of arguments without an upper limit) count the number
	   of variables on the stack */

	if ( f->dim > INT_MIN )
	{
		abs_len = f->dim > 0 ? f->dim : - f->dim;

		/* Count number of arguments on the stack */

		for ( ac = 0, ap = f->next; ap != NULL; ++ac, ap = ap->next )
			/* empty */ ;

		/* If there are too many arguments utter a warning and remove the
		   superfluous ones (distinuguish between functions with a fixed
		   and a variable number of arguments - in the first case f->dim
		   is positive, in the second negative) */

		if ( ac > abs_len )
		{
			print( WARN, "%s(): Too many arguments, discarding superfluous "
				   "arguments.\n", f->name );

			for ( ac = 0, ap = f->next; ac < abs_len; ++ac, ap = ap->next )
				/* empty */ ;
			while ( ( ap = vars_pop( ap ) ) != NULL )
				/* empty */ ;
		}

		/* For functions with a fixed number of arguments (a positive number
		   of arguments is specified in the nargs entry of the functions
		   structure, which has been copied to the dim entry of the variables
		   structure) less arguments than needed by the function is a fatal
		   error. */

		if ( f->dim >= 0 && ac < f->dim )
		{
			print( FATAL, "%s(): Function expects %d argument%s but only %d "
				   "where found.\n",
				   f->name, f->dim, f->dim == 1 ? "" : "s", ac );
			THROW( EXCEPTION );
		}
	}

	/* Now call the function after storing some information about the
	   function on the call stack */

	if ( call_push( f->val.fnct, NULL,
					f->val.fnct->device ? f->val.fnct->device->name : NULL,
					f->val.fnct->device ? f->val.fnct->device->count : 0 )
		 == NULL )
		THROW( OUT_OF_MEMORY_EXCEPTION );

	TRY
	{
		ret = f->val.fnct->fnct( f->next );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
#ifndef NDEBUG
		if ( ! vars_exist( f ) )
		{
			if ( EDL.Call_Stack->f != NULL &&
				 ! EDL.Call_Stack->f->to_be_loaded )
				eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
						__FILE__, __LINE__ );
			else
				print( FATAL, "Function %s() from module %s.so messed up the "
					   "variable stack at %s:%d.\n", EDL.Call_Stack->f->name,
					   EDL.Call_Stack->f->device->name, __FILE__, __LINE__ );
			call_pop( );
			RETHROW( );
		}
#endif
		call_pop( );
		for ( ap = f; ap != NULL; ap = vars_pop( ap ) )
			/* empty */ ;
		RETHROW( );
	}

#ifndef NDEBUG

	/* Before starting to delete the now defunct variables do another sanity
	   check, i.e. test that the variables stack didn't get corrupted. */

	if ( ! vars_exist( f ) )
	{
		if ( EDL.Call_Stack->f != NULL && ! EDL.Call_Stack->f->to_be_loaded )
			eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
					__FILE__, __LINE__ );
		else
			print( FATAL, "Function %s() from module %s.so messed up the "
				   "variables stack at %s:%d.\n", EDL.Call_Stack->f->name,
				   EDL.Call_Stack->f->device->name, __FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	call_pop( );

	/* Finally do the clean up, i.e. remove the variable with the function
	   and all parameters that survived - just keep the return value. */

	for ( ap = f; ap != ret; ap = vars_pop( ap ) )
		/* empty */ ;

	return ret;
}


/*------------------------------------------------------------------------*/
/* The following functions are for maintaining a stack of EDL (and other */
/* module) functions calls, which is actually implemented as a linked    */
/* list. The stack pointer, i.e. the pointer to the head of the linked   */
/* list, is the 'Call_Stack' member of the global 'EDL' structure. The   */
/* topmost element of the stack (i.e. the head of the linked list)       */
/* always refers to the currently running (EDL) function.                */
/* For the main program the functions only get called when an EDL        */
/* function is invoked. In contrast, for modules they also are called    */
/* for hook-function calls and, for pulser modules, also for calls of    */
/* the functions declared in the pulser structure (i.e. functions, that  */
/* the pulser module must export to allow the setting of the pulsers     */
/* properties and the creation of pulses). The information stored in the */
/* call stack elements is a pointer to the EDL function structure (if    */
/* applicable, i.e. for EDL function calls), a pointer to the module     */
/* structure (for calls from modules) and the name of the device         */
/* controlled by the module (i.e. not the module name, but the name as   */
/* it should appear in error messages for the device), and, finally, the */
/* number of the device (this can be larger than 1 if there's more than  */
/* one devices of the same generic type).                                */
/* These informations are used in two situations: First when printing    */
/* messages. The print() function is supposed to prepend EDL file name   */
/* and line number, device name (if applicable) and function name. It    */
/* gets the device and function name from the current call stack entry.  */
/* Second, when within a module an EDL function from the same module is  */
/* called (via func_get() and func_call()) the writer of the module has  */
/* no information if there are other modules with the same generic type  */
/* loaded, supplying a function by the same name. The information in the */
/* current call stack entry is used to determine this and thus to auto-  */
/* matically return the appropriate fucntion handle to the module, i.e.  */
/* the function from the same module the func_get() call came from.      */
/*-----------------------------------------------------------------------*/

CALL_STACK *call_push( Func *f, Device *device, const char *device_name,
					   int dev_count )
{
	const char *t;
	CALL_STACK *cs;


	cs = CALL_STACK_P T_malloc( sizeof *cs );
	cs->prev = EDL.Call_Stack;
	cs->f = f;
	if ( f != NULL )
		cs->device = f->device;
	else
	{
		fsc2_assert( device != NULL );
		cs->device = device;
	}
	cs->dev_name = device_name;
	cs->dev_count = dev_count;

	/* If this is a function for a pulser figure out the number of the pulser
	   (there might be more than one pulser) */

	if ( f != NULL && f->device != NULL && f->device->generic_type != NULL &&
		 ! strcasecmp( f->device->generic_type, PULSER_GENERIC_TYPE ) )
	{
		if ( ( t = strrchr( f->name, '#' ) ) != NULL )
			Cur_Pulser = cs->Cur_Pulser = T_atol( t + 1 ) - 1;
	}
	else
		cs->Cur_Pulser = Cur_Pulser;

	/* If this is call of function within one of the modules during the test
	   run add an extremely rough estimate for the mean time spend in the
	   function for the call to the global variable that is used to keep
	   an estimate time for the modules. */

	if ( Internals.mode == TEST && device_name != NULL )
		EDL.experiment_time += MODULE_CALL_ESTIMATE;

	return EDL.Call_Stack = cs;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

CALL_STACK *call_pop( void )
{
	CALL_STACK *cs;


	if ( EDL.Call_Stack == NULL )
		return NULL;

	cs = EDL.Call_Stack;
	EDL.Call_Stack = cs->prev;
	T_free( cs );

	if ( cs != NULL )
		Cur_Pulser = cs->Cur_Pulser;
	else
		Cur_Pulser = -1;

	return EDL.Call_Stack;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
