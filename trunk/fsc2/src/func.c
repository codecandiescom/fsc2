/*
   $Id$
*/


#include "fsc2.h"


/* When in the input an identifier is found it is always tried first if it's a
   function by a calling func_get(). If it is a function, a pointer to a
   temporary variable on the stack of type FUNC is returned.  The variable's
   structure contains beside a pointer to the function a copy of the name of
   the function and the number of arguments and the access flag (the access
   flag tells in which sections the function may be used).

   The following input results in as may variables on the stack as there are
   function arguments. When the argument list ends, func_call() is called.
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
   the variable for the function.  */


/* The following variables are shared with loader.c which adds further 
   functions from the loaded modules */

int Num_Func;        /* number of built-in and listed functions */
Func *Fncts;         /* structure for list of functions */


/* Both these variables are shared with 'func_util.c' */

bool No_File_Numbers;
bool Dont_Save;


Func Def_Fncts[ ] =              /* List of built-in functions */
{
	{ "int",           f_int,      1, ACCESS_ALL,  UNSET },
	{ "float",         f_float,    1, ACCESS_ALL,  UNSET },
	{ "round",         f_round,    1, ACCESS_ALL,  UNSET },
	{ "floor",         f_floor,    1, ACCESS_ALL,  UNSET },
	{ "ceil",          f_ceil,     1, ACCESS_ALL,  UNSET },
	{ "abs",           f_abs,      1, ACCESS_ALL,  UNSET },
	{ "sin",           f_sin,      1, ACCESS_ALL,  UNSET },
	{ "cos",           f_cos,      1, ACCESS_ALL,  UNSET },
	{ "tan",           f_tan,      1, ACCESS_ALL,  UNSET },
	{ "asin",          f_asin,     1, ACCESS_ALL,  UNSET },
	{ "acos",          f_acos,     1, ACCESS_ALL,  UNSET },
	{ "atan",          f_atan,     1, ACCESS_ALL,  UNSET },
	{ "sinh",          f_sinh,     1, ACCESS_ALL,  UNSET },
	{ "cosh",          f_cosh,     1, ACCESS_ALL,  UNSET },
	{ "tanh",          f_tanh,     1, ACCESS_ALL,  UNSET },
	{ "exp",           f_exp,      1, ACCESS_ALL,  UNSET },
	{ "ln",            f_ln,       1, ACCESS_ALL,  UNSET },
	{ "log",           f_log,      1, ACCESS_ALL,  UNSET },
	{ "sqrt",          f_sqrt,     1, ACCESS_ALL,  UNSET },
	{ "random",        f_random,   0, ACCESS_ALL,  UNSET },
	{ "set_seed",      f_setseed,  1, ACCESS_ALL,  UNSET },
    { "time",          f_time,     0, ACCESS_ALL,  UNSET },
    { "date",          f_date,     0, ACCESS_ALL,  UNSET },
    { "delta_time",    f_dtime,    0, ACCESS_EXP,  UNSET },
	{ "print",         f_print,   -1, ACCESS_ALL,  UNSET },
	{ "wait",          f_wait,     1, ACCESS_ALL,  UNSET },
	{ "init_1d",       f_init_1d, -1, ACCESS_PREP, UNSET },
	{ "init_2d",       f_init_2d, -1, ACCESS_PREP, UNSET },
    { "change_scale",  f_cscale,  -1, ACCESS_EXP,  UNSET },
	{ "display",       f_display, -1, ACCESS_EXP,  UNSET },
	{ "clear_curve",   f_clearcv, -1, ACCESS_EXP,  UNSET },
	{ "dim",           f_dim,      1, ACCESS_ALL,  UNSET },
	{ "size",          f_size,     2, ACCESS_ALL,  UNSET },
	{ "sizes",         f_sizes,    1, ACCESS_ALL,  UNSET },
	{ "mean",          f_mean,    -1, ACCESS_ALL,  UNSET },
	{ "rms",           f_rms,      1, ACCESS_ALL,  UNSET },
	{ "slice",         f_slice,   -1, ACCESS_ALL,  UNSET },
	{ "square",        f_square,   1, ACCESS_ALL,  UNSET },
	{ "int_slice",     f_islice,   1, ACCESS_ALL,  UNSET },
	{ "float_slice",   f_fslice,   1, ACCESS_ALL,  UNSET },
	{ "slice",         f_slice,   -1, ACCESS_ALL,  UNSET },
	{ "get_file",      f_getf,    -1, ACCESS_EXP,  UNSET },
	{ "save",          f_save,    -1, ACCESS_EXP,  UNSET },
	{ "fsave",         f_fsave,   -1, ACCESS_EXP,  UNSET },
	{ "save_program",  f_save_p,  -1, ACCESS_EXP,  UNSET },
	{ "save_output",   f_save_o,  -1, ACCESS_EXP,  UNSET },
	{ "save_comment",  f_save_c,  -1, ACCESS_EXP,  UNSET },
    { "layout",        f_layout,   1, ACCESS_EXP,  UNSET },
    { "button_create", f_bcreate, -1, ACCESS_EXP,  UNSET },
    { "button_delete", f_bdelete, -1, ACCESS_EXP,  UNSET },
    { "button_state",  f_bstate,  -1, ACCESS_EXP,  UNSET },
    { "slider_create", f_screate, -1, ACCESS_EXP,  UNSET },
    { "slider_delete", f_sdelete, -1, ACCESS_EXP,  UNSET },
    { "slider_value",  f_svalue,  -1, ACCESS_EXP,  UNSET },
	{ NULL,            NULL,       0, 0,           UNSET}
	                   /* last set marks the very last entry, don't remove ! */
};


/* Locally used functions */

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
		Fncts = T_malloc( ( Num_Func + 1 ) * sizeof( Func ) );
		memcpy( Fncts, Def_Fncts, ( Num_Func + 1 ) * sizeof( Func ) );
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


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static int func_cmp1( const void *a, const void *b )
{
	return strcmp( ( ( Func * ) a )->name, ( ( Func * ) b )->name );
}


/*----------------------------------------------------*/
/* Function gets rid of all loaded functions and also */
/* unlinks the library files.                         */
/*----------------------------------------------------*/

void functions_exit( void )
{
	int i;

	if ( Fncts == NULL )
		return;

	/* Get rid of the names of loaded functions (but not the built-in ones) */

	for ( i = 0; i < Num_Func; i++ )
		if ( Fncts[ i ].to_be_loaded )
			T_free( ( char * ) Fncts[ i ].name );

	Fncts = T_free( Fncts );

	No_File_Numbers = UNSET;
	Dont_Save = UNSET;
	close_all_files( );
}


/*----------------------------------------------------------------------*/
/* Function tries to find a function in the list of built-in and loaded */
/* functions. If it finds it it creates a new variable on the variables */ 
/* stack with a pointer to the function and returns a pointer to the    */
/* variable. If the function can't be found it returns a NULL pointer.  */
/*----------------------------------------------------------------------*/

Var *func_get( const char *name, int *access )
{
	return func_get_long( name, access, 0 );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

Var *func_get_long( const char *name, int *access, bool flag )
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
		if ( ! flag )
		{
			eprint( FATAL, "%s:%ld: Function `%s' has not been "
					"loaded.\n", Fname, Lc, f->name );
			THROW( EXCEPTION );
		}
		else                     /* some callers do their own error handling */
			return NULL;
	}
	
	ret = vars_push( FUNC, f->fnct );
	ret->name = T_strdup( name );
	ret->dim = f->nargs;
	*access = f->access_flag;

	return ret;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static int func_cmp2( const void *a, const void *b )
{
	return strcmp( ( char * ) a, ( ( Func * ) b )->name );
}


/*--------------------------------------------------------*/
/* Here's the function that finally calls an EDL function */
/*--------------------------------------------------------*/

Var *func_call( Var *f )
{
	Var *ap;
	Var *ret = NULL;
	int ac;
	int i;
	

	/* Check (and double-check) that it's really a function variable - one
	   can never be sure someone really got it right... */

	if ( f->type != FUNC )
	{
		eprint( FATAL, "%s:%ld: Variable passed to `func_call()' isn't "
				"of type `FUNC'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	for ( i = 0; i < Num_Func; i++ )
		if ( Fncts[ i ].fnct == f->val.fnct )
			break;
	
	if ( i == Num_Func )
	{
		eprint( FATAL, "%s:%ld: Variable passed to `func_call()' is not "
				"a known function.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* If the number of function arguments isn't negative (indicating a
	   variable number of arguments) count number of variables on the stack */

	if ( f->dim >= 0 )
	{
		/* Count number of arguments on the stack */

		for ( ac = 0, ap = f->next; ap != NULL; ++ac, ap = ap->next )
			;

		/* If there are too many arguments utter a warning and remove the
		   superfluous ones */

		if ( ac > f->dim )
		{
			eprint( WARN, "%s:%ld: Too many arguments for function `%s'.\n",
					Fname, Lc, f->name );

			for ( ac = 0, ap = f->next; ac < f->dim; ++ac, ap = ap->next )
				;
			for ( ; ap != NULL; ap = vars_pop( ap ) )
				;
		}

		/* Less arguments than needed by the function is a fatal error. */

		if ( ac < f->dim )
		{
			eprint( FATAL, "%s:%ld: Function `%s' needs %d argument%s but "
					"found only %d.\n", Fname, Lc, f->name, f->dim,
					f->dim == 1 ? "" : "s", ac );
			THROW( EXCEPTION );
		}
	}
	else
		ac = -1;

	/* Now call the function */

	if ( ac != 0 )
		ret = ( *f->val.fnct )( f->next );
	else
		ret = ( *f->val.fnct )( NULL );

	/* Finally do a clean up, i.e. remove the variable with the function and
	   all parameters - just keep the return value. Before starting to delete
	   the defunct variables we do another sanity check. */

	if ( ! vars_exist( f ) )
	{
		eprint( FATAL, "%s:%ld: Function `%s' messed up the variable stack.\n",
				Fname, Lc, Fncts[ i ].name );
		THROW( EXCEPTION );
	}

	for ( ap = f; ap != NULL; ap = ap == ret ? ap->next : vars_pop( ap ) )
		;

	return ret;
}
