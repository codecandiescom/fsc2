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


/* When in the input an identifier is found it is always tested first if it
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
	{ "exp",           f_exp,      		1, ACCESS_ALL,  NULL, UNSET },
	{ "ln",            f_ln,       		1, ACCESS_ALL,  NULL, UNSET },
	{ "log",           f_log,      		1, ACCESS_ALL,  NULL, UNSET },
	{ "sqrt",          f_sqrt,     		1, ACCESS_ALL,  NULL, UNSET },
	{ "random",        f_random,   	   -1, ACCESS_ALL,  NULL, UNSET },
	{ "grandom",       f_grand,    	   -1, ACCESS_ALL,  NULL, UNSET },
	{ "set_seed",      f_setseed,  		1, ACCESS_ALL,  NULL, UNSET },
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
	{ "rms",           f_rms,      	    1, ACCESS_ALL,  NULL, UNSET },
	{ "slice",         f_slice,    	   -3, ACCESS_ALL,  NULL, UNSET },
	{ "square",        f_square,   	    1, ACCESS_ALL,  NULL, UNSET },
	{ "int_slice",     f_islice,   	    1, ACCESS_ALL,  NULL, UNSET },
	{ "float_slice",   f_fslice,   	    1, ACCESS_ALL,  NULL, UNSET },
	{ "get_file",      f_getf,     	   -5, ACCESS_EXP,  NULL, UNSET },
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
	{ "input_create",  f_icreate,  	   -5, ACCESS_EXP,  NULL, UNSET },
	{ "input_delete",  f_idelete, INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "input_value",   f_ivalue,   	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "output_create", f_icreate,  	   -5, ACCESS_EXP,  NULL, UNSET },
	{ "output_delete", f_idelete, INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "output_value",  f_ivalue,   	   -2, ACCESS_EXP,  NULL, UNSET },
	{ "object_delete", f_objdel,  INT_MIN, ACCESS_EXP,  NULL, UNSET },
	{ "end",           f_stopsim,  	    0, ACCESS_EXP,  NULL, UNSET },
	{ "abort",         f_abort,    	    0, ACCESS_EXP,  NULL, UNSET },
	{ NULL,            NULL,       	    0, 0,           NULL, UNSET }
	                   /* last set marks the very last entry, don't remove ! */
};


/* Locally used functions and variables */

static int func_cmp1( const void *a, const void *b );
static int func_cmp2( const void *a, const void *b );


/*--------------------------------------------------------------------*/
/* Function parses the function data base in `Functions' and makes up */
/* a complete list of all built-in and user-supplied functions.       */
/*--------------------------------------------------------------------*/

bool functions_init( void )
{
	No_File_Numbers = UNSET;
	Dont_Save = UNSET;

	/* Count number of built-in functions */

	for ( Num_Func = 0; Def_Fncts[ Num_Func ].fnct != NULL; Num_Func++ )
		;

	/*
	   1. Get new memory for the functions structures and copy the built-in
	      functions into it.
	   2. Parse the function name data base `Functions' where all additional
	      functions have to be listed.
	   3. Sort the functions by name so that they can be found using bsearch()
	*/

	TRY
	{
		Fncts = T_malloc( Num_Func * sizeof( Func ) );
		memcpy( Fncts, Def_Fncts, Num_Func * sizeof( Func ) );
		qsort( Fncts, Num_Func, sizeof( Func ), func_cmp1 );
		Num_Func = func_list_parse( &Fncts, Num_Func );
		qsort( Fncts, Num_Func, sizeof( Func ), func_cmp1 );
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

	Fncts = T_free( Fncts );

	/* Clean up the call stack */

#ifndef NDEBUG
	if ( Call_Stack != NULL )
	{
		eprint( SEVERE, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		while ( call_pop( ) )
			;
	}
#else
	while ( call_pop( ) )
		;
#endif

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
	return NULL != bsearch( name, Fncts, Num_Func, sizeof( Func ), func_cmp2 );
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

	f = bsearch( name, Fncts, Num_Func, sizeof( Func ), func_cmp2 );

	if ( f == NULL )             /* function not found */
		return NULL;

	if ( f->fnct == NULL )       /* function found but not loaded */
	{
		if ( flag )
		{
			eprint( FATAL, SET, "%s(): Function has not been loaded.\n",
					f->name );
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


/*--------------------------------------------------*/
/* Function for bsearch for a function by its name. */
/*--------------------------------------------------*/

static int func_cmp2( const void *a, const void *b )
{
	return strcmp( ( const char * ) a, ( ( const Func * ) b )->name );
}


/*---------------------------------------------------------------*/
/* This function is called to really execute an EDL function.    */
/* It must be able to also handle situations where, for example, */
/* an EDL function calls another EDL function, which throws an   */
/* exception that then is caught by the first function etc.      */
/*---------------------------------------------------------------*/

Var *func_call( Var *f )
{
	Var *ap;
	Var *ret = NULL;
	long ac;
	long abs_len;
#ifndef NDEBUG
	Func *cur_func;
	size_t i;


	/* Check (and double-check) that it's really a function variable - one
	   can never be sure someone really got it right... */

	if ( f->type != FUNC )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	for ( i = 0; i < Num_Func; i++ )
		if ( Fncts[ i ].fnct == f->val.fnct->fnct )
			break;
	
	if ( i >= Num_Func )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	cur_func = Fncts + i ;
#endif

	/* If the number of function arguments isn't INT_MIN (indicating a
	   variable number of arguments without an upper limit) count the number
	   of variables on the stack */

	if ( f->dim > INT_MIN )
	{
		abs_len = f->dim > 0 ? f->dim : - f->dim;

		/* Count number of arguments on the stack */

		for ( ac = 0, ap = f->next; ap != NULL; ++ac, ap = ap->next )
			;

		/* If there are too many arguments utter a warning and remove the
		   superfluous ones (distinuguish between functions with a fixed
		   and a variable number of arguments - in the first case f->dim
		   is positive, in the second negative) */

		if ( ac > abs_len )
		{
			eprint( WARN, SET, "%s(): Too many arguments, discarding "
					"superfluous arguments.\n", f->name );

			for ( ac = 0, ap = f->next; ac < abs_len; ++ac, ap = ap->next )
				;
			while ( ( ap = vars_pop( ap ) ) != NULL )
				;
		}

		/* For functions with a fixed number of arguments (a positive number
		   of arguments is specified in the nargs entry of the functions
		   structure) less arguments than needed by the function is a fatal
		   error. */

		if ( f->dim >= 0 && ac < f->dim )
		{
			eprint( FATAL, SET, "%s(): Function expects %d argument%s but "
					"only %d where found.\n", f->name, f->dim,
					f->dim == 1 ? "" : "s", ac );
			THROW( EXCEPTION );
		}
	}

	/* Now call the function after storing some information about the
	   function on the call stack */

	call_push( f->val.fnct,
			   f->val.fnct->device ? f->val.fnct->device->name : NULL );

	TRY
	{
		ret = ( *f->val.fnct->fnct )( f->next );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );

#ifndef NDEBUG
		if ( ! vars_exist( f ) )
		{
			if ( ! cur_func->to_be_loaded )
				eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
						__FILE__, __LINE__ );
			else
				eprint( FATAL, SET, "Function %s() from module %s.so messed "
						"up the variable stack at %s:%d.\n", cur_func->name,
						cur_func->device->name, __FILE__, __LINE__ );
			PASSTHROUGH( );
		}
#endif
		for ( ap = f; ap != NULL; ap = vars_pop( ap ) )
			;
		PASSTHROUGH( );
	}

	call_pop( );

#ifndef NDEBUG
	/* Before starting to delete the now defunct variables do another sanity
	   check, i.e. test that the variables stack didn't get corrupted. */

	if ( ! vars_exist( f ) )
	{
		if ( ! cur_func->to_be_loaded )
			eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
					__FILE__, __LINE__ );
		else
			eprint( FATAL, SET, "Function %s() from module %s.so messed "
					"up the variable stack at %s:%d.\n", cur_func->name,
					cur_func->device->name, __FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	/* Finally do the clean up, i.e. remove the variable with the function
	   and all parameters that survived - just keep the return value. */

	for ( ap = f; ap != ret; ap = vars_pop( ap ) )
		;

	return ret;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

CALL_STACK *call_push( Func *f, const char *device_name )
{
	if ( Call_Stack == NULL )
	{
		Call_Stack = T_malloc( sizeof( CALL_STACK ) );
		Call_Stack->next = Call_Stack->prev = NULL;
	}
	else
	{
		Call_Stack->next = T_malloc( sizeof( CALL_STACK ) );
		Call_Stack->next->prev = Call_Stack;
		Call_Stack = Call_Stack->next;
		Call_Stack->next = NULL;
	}

	Call_Stack->f = f;
	Call_Stack->dev_name = device_name;

	return Call_Stack;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

CALL_STACK *call_pop( void )
{
	if ( Call_Stack == NULL )
		return NULL;

	if ( Call_Stack->prev != NULL )
	{
		Call_Stack = Call_Stack->prev;
		Call_Stack->next = T_free( Call_Stack->next );
	}
	else
		Call_Stack = T_free( Call_Stack );

	return Call_Stack;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
