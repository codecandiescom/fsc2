/*
   $Id$
*/


#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/shm.h>


typedef struct {
	long nx;
	long ny;
	long nc;
	int type;
	Var *v;
} DPoint;


static void f_wait_alarm_handler( int sig_type );
DPoint *eval_display_args( Var *v, int *npoints );
static int get_save_file( Var **v, const char *calling_function );
static void print_array( Var *v, long cur_dim, long *start, int fid );
static void print_slice( Var *v, int fid );
static void print_browser( int browser, int fid, const char* comment );


static bool No_File_Numbers;
static bool Dont_Save;



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


Var *f_int(     Var *v );
Var *f_float(   Var *v );
Var *f_round(   Var *v );
Var *f_floor(   Var *v );
Var *f_ceil(    Var *v );
Var *f_abs(     Var *v );
Var *f_sin(     Var *v );
Var *f_cos(     Var *v );
Var *f_tan(     Var *v );
Var *f_asin(    Var *v );
Var *f_acos(    Var *v );
Var *f_atan(    Var *v );
Var *f_sinh(    Var *v );
Var *f_cosh(    Var *v );
Var *f_tanh(    Var *v );
Var *f_exp(     Var *v );
Var *f_ln(      Var *v );
Var *f_log(     Var *v );
Var *f_sqrt(    Var *v );
Var *f_print(   Var *v );
Var *f_wait(    Var *v );
Var *f_init_1d( Var *v );
Var *f_init_2d( Var *v );
Var *f_display( Var *v );
Var *f_dim(     Var *v );
Var *f_size(    Var *v );
Var *f_sizes(   Var *v );
Var *f_getf(    Var *v );
Var *f_save(    Var *v );
Var *f_fsave(   Var *v );
Var *f_save_p(  Var *v );
Var *f_save_o(  Var *v );
Var *f_save_c(  Var *v );


/* The following variables are shared with loader.c which adds further 
   functions from the loaded modules */


int Num_Def_Func;    /* number of built-in functions */
int Num_Func;        /* number of built-in and listed functions */
Func *Fncts;         /* structure for list of functions */


Func Def_Fncts[ ] =              /* List of built-in functions */
{
	{ "int",          f_int,           1, ACCESS_ALL, 0 },
	{ "float",        f_float,         1, ACCESS_ALL, 0 },
	{ "round",        f_round,         1, ACCESS_ALL, 0 },
	{ "floor",        f_floor,         1, ACCESS_ALL, 0 },
	{ "ceil",         f_ceil,          1, ACCESS_ALL, 0 },
	{ "abs",          f_abs,           1, ACCESS_ALL, 0 },
	{ "sin",          f_sin,           1, ACCESS_ALL, 0 },
	{ "cos",          f_cos,           1, ACCESS_ALL, 0 },
	{ "tan",          f_tan,           1, ACCESS_ALL, 0 },
	{ "asin",         f_asin,          1, ACCESS_ALL, 0 },
	{ "acos",         f_acos,          1, ACCESS_ALL, 0 },
	{ "atan",         f_atan,          1, ACCESS_ALL, 0 },
	{ "sinh",         f_sinh,          1, ACCESS_ALL, 0 },
	{ "cosh",         f_cosh,          1, ACCESS_ALL, 0 },
	{ "tanh",         f_tanh,          1, ACCESS_ALL, 0 },
	{ "exp",          f_exp,           1, ACCESS_ALL, 0 },
	{ "ln",           f_ln,            1, ACCESS_ALL, 0 },
	{ "log",          f_log,           1, ACCESS_ALL, 0 },
	{ "sqrt",         f_sqrt,          1, ACCESS_ALL, 0 },
	{ "print",        f_print,        -1, ACCESS_ALL, 0 },
	{ "wait",         f_wait,          1, ACCESS_ALL, 0 },
	{ "init_1d",      f_init_1d,      -1, ACCESS_ALL, 0 },
	{ "init_2d",      f_init_2d,      -1, ACCESS_ALL, 0 },
	{ "display",      f_display,      -1, ACCESS_EXP, 0 },
	{ "dim",          f_dim,           1, ACCESS_ALL, 0 },
	{ "size",         f_size,          2, ACCESS_ALL, 0 },
	{ "sizes",        f_sizes,         1, ACCESS_ALL, 0 },
	{ "get_file",     f_getf,         -1, ACCESS_EXP, 0 },
	{ "save",         f_save,         -1, ACCESS_EXP, 0 },
	{ "fsave",        f_fsave,        -1, ACCESS_EXP, 0 },
	{ "save_program", f_save_p,       -1, ACCESS_EXP, 0 },
	{ "save_output",  f_save_o,       -1, ACCESS_EXP, 0 },
	{ "save_comment", f_save_c,       -1, ACCESS_EXP, 0 },
	{ NULL,           NULL,            0, 0,          0 }
	                   /* last set marks the very last entry, don't remove ! */
};


/*--------------------------------------------------------------------*/
/* Function parses the function data base in `Functions' and makes up */
/* a complete list of all built-in and user-supplied functions.       */
/*--------------------------------------------------------------------*/

bool functions_init( void )
{
	No_File_Numbers = UNSET;
	Dont_Save = UNSET;

	/* Count number of built-in functions */

	for ( Num_Def_Func = 0; Def_Fncts[ Num_Def_Func ].fnct != NULL;
		  Num_Def_Func++ )
		;

	Num_Func = Num_Def_Func;

	/*
	   1. Get new memory for the functions structures and copy the built in
	      functions into it.
	   2. Parse the function name data base `Functions' where all additional
	      functions have to be listed.
	*/

	TRY
	{
		Fncts = NULL;
		Fncts = T_malloc( ( Num_Def_Func + 1 ) * sizeof( Func ) );
		memcpy( Fncts, Def_Fncts, ( Num_Def_Func + 1 ) * sizeof( Func ) );
		func_list_parse( &Fncts, Num_Def_Func, &Num_Func );
   		TRY_SUCCESS;
	}
	OTHERWISE
	{
		functions_exit( );
		return FAIL;
	}

	return OK;
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

	/* Get rid of the structures for the functions */

	for ( i = Num_Def_Func; i < Num_Func; i++ )
		if ( Fncts[ i ].name != NULL )
			T_free( ( char * ) Fncts[ i ].name );
	T_free( Fncts );
	Fncts = NULL;

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
	int i;
	Var *ret;


	/* Try to find the function by its name and if found create a variable on
	   the variable stack with a pointer to the function and the number of
	   arguments. Also copy the functions name and access flag. */

	for ( i = 0; i < Num_Func; i++ )
	{
		if ( Fncts[ i ].name != NULL && ! strcmp( Fncts[ i ].name, name ) )
		{
			if ( Fncts[ i ].fnct == NULL )
			{
				eprint( FATAL, "%s:%ld: Function `%s' has not (yet) been "
                        "loaded.\n", Fname, Lc, Fncts[ i ].name );
				THROW( EXCEPTION );
			}
						
			ret = vars_push( FUNC, Fncts[ i ].fnct );
			ret->name = get_string_copy( name );
			ret->dim = Fncts[ i ].nargs;
			*access = Fncts[ i ].access_flag;
			return ret;
		}
	}

	return NULL;
}


/*--------------------------------------------------------*/
/* Here's the function that finally calls an EDL function */
/*--------------------------------------------------------*/

Var *func_call( Var *f )
{
	Var *ap;
	Var *apn;
	Var *ret;
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
				"a valid function.\n", Fname, Lc );
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

		/* Less arguments than needed by the function is a fatal error. */

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

	/* Finally do clean up, i.e. remove the variable with the function and all
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


/*-------------------------------------------------*/
/* Conversion float to integer (result is integer) */
/*-------------------------------------------------*/

Var *f_int( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
	{
		if ( v->val.dval > LONG_MAX || v->val.dval < LONG_MIN )
			eprint( SEVERE, "%s:%ld: Integer overflow in function `int()'.\n",
					Fname, Lc );
		return vars_push( INT_VAR, ( long ) v->val.dval );
	}
}


/*----------------------------------------------------*/
/* Conversion int to floating point (result is float) */
/*----------------------------------------------------*/

Var *f_float( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, ( double ) v->val.lval );
	else
		return vars_push( FLOAT_VAR, v->val.dval );
}


/*--------------------------------------------------------*/
/* Rounding of floating point numbers (result is integer) */
/*--------------------------------------------------------*/

Var *f_round( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
	{
		if ( v->val.dval >= LONG_MAX - 0.5 || v->val.dval <= LONG_MIN + 0.5 )
			eprint( SEVERE, "%s:%ld: Integer overflow in function "
					"`round()'.\n", Fname, Lc );
		return vars_push( INT_VAR,   ( long ) ( 2 * v->val.dval )
                                   - ( long ) v->val.dval );
	}
}


/*---------------------------------*/
/* Floor value (result is integer) */
/*---------------------------------*/

Var *f_floor( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
	{
		if ( v->val.dval < LONG_MIN )
			eprint( SEVERE, "%s:%ld: Integer overflow in function "
					"`floor()'.\n", Fname, Lc );
		return vars_push( INT_VAR, ( long ) floor( v->val.dval ) );
	}
}


/*-----------------------------------*/
/* Ceiling value (result is integer) */
/*-----------------------------------*/

Var *f_ceil( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
	{
		if ( v->val.dval > LONG_MAX )
			eprint( SEVERE, "%s:%ld: Integer overflow in function "
					"`ceil()'.\n", Fname, Lc );
		return vars_push( INT_VAR, ( long ) ceil( v->val.dval ) );
	}
}


/*-------------------------------------------------*/
/* abs of value (result has same as type argument) */
/*-------------------------------------------------*/

Var *f_abs( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	{
		if ( v->val.lval == LONG_MIN )
			eprint( SEVERE, "%s:%ld: Integer overflow in function `abs()'.\n",
					Fname, Lc );
		return vars_push( INT_VAR, labs( v->val.lval ) );
	}
	else
		return vars_push( FLOAT_VAR, fabs( v->val.dval ) );
}


/*-----------------------------------------------*/
/* sin of argument (in radian) (result is float) */
/*-----------------------------------------------*/

Var *f_sin( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, sin( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, sin( v->val.dval ) );
}


/*-----------------------------------------------*/
/* cos of argument (in radian) (result is float) */
/*-----------------------------------------------*/

Var *f_cos( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, cos( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, cos( v->val.dval ) );
}


/*-----------------------------------------------*/
/* tan of argument (in radian) (result is float) */
/*-----------------------------------------------*/

Var *f_tan( Var *v )
{
	double res;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		res = tan( ( double ) v->val.lval );
	else
		res = tan( v->val.dval );

	if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
		eprint( SEVERE, "%s:%ld: Overflow in function `tan()'.\n", Fname, Lc );

	return vars_push( FLOAT_VAR, res );
}


/*--------------------------------------------------------------------*/
/* asin (in radian) of argument (with -1 <= x <= 1) (result is float) */
/*--------------------------------------------------------------------*/

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
		eprint( FATAL, "%s:%ld: Argument of function `asin()' is out of "
				"range.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, asin( arg ) );
}


/*--------------------------------------------------------------------*/
/* acos (in radian) of argument (with -1 <= x <= 1) (result is float) */
/*--------------------------------------------------------------------*/

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
		eprint( FATAL, "%s:%ld: Argument of function `acos()' is out of "
				"range.\n",  Fname, Lc );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, acos( arg ) );
}


/*------------------------------------------------*/
/* atan (in radian) of argument (result is float) */
/*------------------------------------------------*/

Var *f_atan( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, atan( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, atan( v->val.dval ) );
}


/*------------------------------------*/
/* sinh of argument (result is float) */
/*------------------------------------*/

Var *f_sinh( Var *v )
{
	double res;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		res = sinh( ( double ) v->val.lval );
	else
		res = sinh( v->val.dval );

	if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
		eprint( SEVERE, "%s:%ld: Overflow in function `sinh()'.\n",
				Fname, Lc );

	return vars_push( FLOAT_VAR, res );
}


/*------------------------------------*/
/* cosh of argument (result is float) */
/*------------------------------------*/

Var *f_cosh( Var *v )
{
	double res;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		res = cosh( ( double ) v->val.lval );
	else
		res = cosh( v->val.dval );

	if ( res == HUGE_VAL && errno == ERANGE )
		eprint( SEVERE, "%s:%ld: Overflow in function `cosh()'.\n",
				Fname, Lc );

	return vars_push( FLOAT_VAR, res );
}


/*------------------------------------*/
/* tanh of argument (result is float) */
/*------------------------------------*/

Var *f_tanh( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, tanh( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, tanh( v->val.dval ) );
}


/*-----------------------------------*/
/* exp of argument (result is float) */
/*-----------------------------------*/

Var *f_exp( Var *v )
{
	double arg, res;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		arg = ( double ) v->val.lval;
	else
		arg = v->val.dval;

	res = exp( arg );

	if ( res == 0.0 && errno == ERANGE )
		eprint( WARN, "%s:%ld: Underflow in function `exp()' - result is 0.\n",
				Fname, Lc );

	if ( res == HUGE_VAL && errno == ERANGE )
		eprint( SEVERE, "%s:%ld: Overflow in function `exp()'.\n", Fname, Lc );

	return vars_push( FLOAT_VAR, res );
}


/*-----------------------------------------------*/
/* ln of argument (with x > 0) (result is float) */
/*-----------------------------------------------*/

Var *f_ln( Var *v )
{
	double arg, res;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		arg = ( double ) v->val.lval;
	else
		arg = v->val.dval;

	if ( arg <= 0.0 )
	{
		eprint( FATAL, "%s:%ld: Argument of function `ln()' is out of "
				"range.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	res = log( arg );

	if ( res == - HUGE_VAL && errno == ERANGE )
		eprint( SEVERE, "%s:%ld: Overflow in function `ln()'.\n", Fname, Lc );

	return vars_push( FLOAT_VAR, res );
}


/*------------------------------------------------*/
/* log of argument (with x > 0) (result is float) */
/*------------------------------------------------*/

Var *f_log( Var *v )
{
	double arg, res;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		arg = ( double ) v->val.lval;
	else
		arg = v->val.dval;

	if ( arg <= 0.0 )
	{
		eprint( FATAL, "%s:%ld: Argument of function `log()' is out of "
				"range.\n", Fname, Lc );
		THROW( EXCEPTION );
	}
	res = log10( arg );

	if ( res == - HUGE_VAL && errno == ERANGE )
		eprint( SEVERE, "%s:%ld: Overflow in function `log()'.\n", Fname, Lc );

	return vars_push( FLOAT_VAR, res );
}


/*--------------------------------------------------*/
/* sqrt of argument (with x >= 0) (result is float) */
/*--------------------------------------------------*/

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
		eprint( FATAL, "%s:%ld: Argument of function `sqrt()' is negative.\n", 
				Fname, Lc );
		THROW( EXCEPTION );
	}
	return vars_push( FLOAT_VAR, sqrt( arg ) );
}


/*------------------------------------------------------------------------
   Prints variable number of arguments using a format string supplied as
   the first argument. Types of arguments to be printed are integer and
   float data and strings. To get a value printed the format string has
   to contain the character `#'. The escape character is the backslash,
   with a double backslash for printing one backslash. Beside the `\#'
   combination to print a `#' aome of the escape sequences from printf()
   ('\n', '\t', and '\"') do work.

   The function returns the number of variables it printed, not counting
   the format string.
--------------------------------------------------------------------------*/

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

	
	/* a call to print() without any argument just prints a newline */

	if ( v == NULL )
	{
		if ( ! just_testing )
			eprint( NO_ERROR, "\x7E" );
		else
			eprint( NO_ERROR, "\n" );
		return vars_push( INT_VAR, 0 );
	}

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
		eprint( SEVERE, "%s:%ld: Less data than format descriptors in "
				"`print()' format string.\n", Fname, Lc );

	/* utter a warning if there are more data than format descriptors */

	if ( on_stack > in_format )
		eprint( SEVERE, "%s:%ld: More data than format descriptors in "
				"`print()' format string.\n", Fname, Lc );

	/* count number of `#' */

	for ( cp = sptr, s = 0; *cp != '\0' ; ++cp )
		if ( *cp == '#' )
			s++;

	/* get string long enough to replace each `#' by a 4-char sequence 
	   plus a '\0' */

	fmt = get_string( strlen( sptr ) + 4 * s + 2 );
	strcpy( fmt, sptr );

	for ( cp = fmt; *cp != '\0'; ++cp )
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

				break;

			case 'n' :
				*cp = '\n';
				break;

			case 't' :
				*cp = '\t';
				break;

			case '\\' :
				*cp = '\\';
				break;

			case '\"' :
				*cp = '"';
				break;

			default :
				eprint( WARN, "%s:%ld: Unknown escape sequence \\%c in "
						"`print()' format string.\n", Fname, Lc, *( cp + 1 ) );
				*cp = *( cp + 1 );
				break;
		}
		
		for ( ep = cp + 1; *ep != '\0'; ep++ )
			*ep = *( ep + 1 );
	}

	/* now lets start printing... */

	cp = fmt;
	if ( ! just_testing )
		strcat( cp, "\x7E" );
	else
		strcat( cp, "\n" );

	cv = v->next;
	while ( ( ep = strstr( cp, "\x01\x01\x01\x01\x01" ) ) != NULL )
	{
		if ( cv != NULL )      /* skip printing if there are not enough data */
		{
			if ( ! TEST_RUN || print_anyway )
				switch ( cv->type )
				{
					case INT_VAR :
						if ( ! just_testing )
							strcpy( ep, "%ld\x7F" );
						else
							strcpy( ep, "%ld" );
						eprint( NO_ERROR, cp, cv->val.lval );
						break;

					case FLOAT_VAR :
						if ( ! just_testing )
							strcpy( ep, "%#g\x7F" );
						else
							strcpy( ep, "%#g" );
						eprint( NO_ERROR, cp, cv->val.dval );
						break;

					case STR_VAR :
						if ( ! just_testing )
							strcpy( ep, "%s\x7F" );
						else
							strcpy( ep, "%s" );
						eprint( NO_ERROR, cp, cv->val.sptr );
						break;

					default :
						assert( 1 == 0 );
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

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	/* set everything up for sleeping */

	sleepy.it_interval.tv_sec = sleepy.it_interval.tv_usec = 0;
	sleepy.it_value.tv_sec = how_long / 1000000000L;
	sleepy.it_value.tv_usec =
		( how_long - sleepy.it_value.tv_sec * 1000000000L ) / 1000;


	signal( SIGALRM, f_wait_alarm_handler );
	setitimer( ITIMER_REAL, &sleepy, NULL );

	/* wake up only after sleeping time or if DO_QUIT signal is received */

	do
	{
		pause( );
		getitimer( ITIMER_REAL, &sleepy );
	} while ( ! do_quit &&
			  ( sleepy.it_value.tv_sec > 0 || sleepy.it_value.tv_usec > 0 ) );

	/* Return 1 if end of sleping time was reached, 0 if do_quit was set.
	   Do not reset the alarm signal handler, because after receipt of a
	   'do_quit' signal the timer may still be running and send a signal
	   that could kill the child prematurely ! */

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


/*-------------------------------------------------------------------*/
/* f_init_1d() has to be called to initialize the display system for */
/* 1-dimensional experiments.                                        */
/* Arguments:                                                        */
/* 1. Number of curves to be shown (optional, defaults to 1)         */
/* 2. Number of points (optional, 0 or negative if unknown)          */
/* 3. Real world coordinate and increment (optional)                 */
/* 4. x-axis label (optional)                                        */
/*-------------------------------------------------------------------*/

Var *f_init_1d( Var *v )
{
	long nc = 1;
	long n = 0;
	double rwc_start = 0.0,
		   rwc_delta = 0.0;
	char *label = NULL;


	if ( v != NULL )
	{
		if ( v->type == STR_VAR )
			goto label_1d;

		vars_check( v, INT_VAR | FLOAT_VAR );             /* get # of curves */

		if ( v->type == INT_VAR )
			nc = v->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Floating point value used as number of "
					"curves in `init_1d()'.\n", Fname, Lc );
			nc = rnd( v->val.dval );
		}

		if ( nc < 1 || nc > MAX_CURVES )
		{
			eprint( FATAL, "%s:%ld: Invalid number of curves (%ld) in "
					"`init_1d()'.\n", Fname, Lc, nc );
			THROW( EXCEPTION );
		}

		v = v->next;
	}

	if ( v != NULL )
	{
		if ( v->type == STR_VAR )
			goto label_1d;

		vars_check( v, INT_VAR | FLOAT_VAR );  /* # of points in x-direction */

		if ( v->type == INT_VAR )
			n = v->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Floating point value used as number of "
					"points in `init_1d()'.\n", Fname, Lc );
			n = rnd( v->val.dval );
		}

		v = v->next;
	}

	if ( v != NULL )
	{

		/* If next value is an integer or a float this is the real world
		   coordinate followed by the increment */

		if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		{
			if ( v->next == NULL || 
				 ! ( v->next->type & ( INT_VAR | FLOAT_VAR ) ) )
			{
				eprint( FATAL, "%s:%ld: Real word coordinate given but no "
						"increment in `init_1d()'.\n" );
				THROW( EXCEPTION );
			}

			rwc_start = VALUE( v );
			rwc_delta = VALUE( v->next );
			v = v->next->next;
		}

label_1d:

		if ( v != NULL )
		{
			vars_check ( v, STR_VAR );
			label = get_string_copy( v->val.sptr );
		}
	}
	
	graphics_init( 1, nc, n, 0, rwc_start, rwc_delta, 0.0, 0.0, label, NULL );
	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/* f_init_2d() has to be called to initialize the display system for */
/* 2-dimensional experiments.                                        */
/*-------------------------------------------------------------------*/

Var *f_init_2d( Var *v )
{
	long nx = 0,
		 ny = 0,
		 nc = 1;
	double rwc_x_start = 0.0,
		   rwc_x_delta = 0.0, 
		   rwc_y_start = 0.0,
		   rwc_y_delta = 0.0;
	char *l1 = NULL,
		 *l2 = NULL;


	if ( v != NULL )
	{
		if ( v->type == STR_VAR )
			goto labels_2d;

		vars_check( v, INT_VAR | FLOAT_VAR );             /* get # of curves */

		if ( v->type == INT_VAR )
			nc = v->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Floating point value used as number of "
					"curves in `init_1d()'.\n", Fname, Lc );
			nc = rnd( v->val.dval );
		}

		if ( nc < 1 || nc > MAX_CURVES )
		{
			eprint( FATAL, "%s:%ld: Invalid number of curves (%ld) in "
					"`init_1d()'.\n", Fname, Lc, nc );
			THROW( EXCEPTION );
		}

		v = v->next;
	}

	if ( v != NULL )
	{
		if ( v->type == STR_VAR )
			goto labels_2d;

		vars_check( v, INT_VAR | FLOAT_VAR );  /* # of points in x-direction */

		if ( v->type == INT_VAR )
			nx = v->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Floating point value used as number of "
					"points in x-direction.\n", Fname, Lc );
			nx = rnd( v->val.dval );
		}

		v = v->next;
	}

	if ( v != NULL )
	{
		if ( v->type == STR_VAR )
			goto labels_2d;

		vars_check( v, INT_VAR | FLOAT_VAR );

		if ( v->type == INT_VAR )
			ny = v->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Floating point value used as number of "
					"points in y-direction.\n", Fname, Lc );
			ny = rnd( v->val.dval );
		}

		v = v->next;
	}

	if ( v != NULL )
	{
		/* Now we expect either 4 real world coordinates (start and delta for
		   x- and y-direction) or none at all - look for labels instead */

		if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		{
			if ( v->next == NULL ||
				 ! ( v->next->type & ( INT_VAR | FLOAT_VAR ) ) ||
				 v->next->next == NULL ||
				 ! ( v->next->next->type & ( INT_VAR | FLOAT_VAR ) ) ||
				 v->next->next->next == NULL ||
				 ! ( v->next->next->next->type & ( INT_VAR | FLOAT_VAR ) ) )
			{
				eprint( FATAL, "%s:%ld: Incomplete real world coordinates in "
						"`init_2d()'.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			rwc_x_start = VALUE( v );
			v = v->next;
			rwc_x_delta = VALUE( v );
			v = v->next;
			rwc_y_start = VALUE( v );
			v = v->next;
			rwc_y_delta = VALUE( v );
			v = v->next;
		}

labels_2d:

		if ( v != NULL )                  /* get x-label */
		{
			vars_check( v, STR_VAR );
			l1 = v->val.sptr;
			v = v->next;
		}

		if ( v != NULL )                  /* get y-label */
		{
			vars_check( v, STR_VAR );
			l2 = v->val.sptr;
		}
	}

	graphics_init( 2, nc, nx, ny, rwc_x_start, rwc_x_delta,
				   rwc_y_start, rwc_y_delta, l1, l2 );
	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------*/
/* f_display() is used to send new data to the display system. */
/*-------------------------------------------------------------*/

Var *f_display( Var *v )
{
	DPoint *dp;
	int shm_id;
	struct shmid_ds shm_buf;
	long len = 0;                    /* total length of message to send */
	void *buf;
	void *ptr;
	int nsets;
	int i;


	/* We can't display data without a previous initialization */

	if ( ! G.is_init )
	{
		if ( ! G.is_warn )                         /* warn only once */
		{
			eprint( WARN, "%s:%ld: Can't display data, missing "
					"initialization.\n", Fname, Lc );
			G.is_warn = SET;
		}

		return vars_push( INT_VAR, 0 );
	}

	/* Check the arguments and get them into some reasonable form */

	dp = eval_display_args( v, &nsets );

	if ( I_am == PARENT )      /* i.e. as long as this is only a test run... */
		return vars_push( INT_VAR, 1 );

	/* We need to determine the amount of shared memory needed */

	len =   4 * sizeof( char )            /* identifier 'fsc2' */
		  + sizeof( len )                 /* length field itself */
		  + sizeof( int )                 /* number of sets to be sent */
		  + 3 * nsets * sizeof( long )    /* x-, y-index and curve */
		  + nsets * sizeof( int );        /* data type */
	
	for ( i = 0; i < nsets; i++ )
	{
		switch( dp[ i ].v->type )
		{
			case INT_VAR :
				len += sizeof( long );
				break;

			case FLOAT_VAR :
				len += sizeof( double );
				break;

			case INT_TRANS_ARR :
				len += ( dp[ i ].v->len + 1 ) * sizeof( long );
				break;

			case FLOAT_TRANS_ARR :
				len += sizeof( long ) + dp[ i ].v->len * sizeof( double );
				break;

			default :                   /* this better never happens... */
				eprint( FATAL, "Internal communication error at %s:%d.\n",
						__FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	while ( ! do_send )             /* wait for parent to become ready */
		pause( );
	do_send = UNSET;

	/* Now try to get a shared memory segment - if it fails and the reason is
	   that no segments or no memory for segments is left wait for some time
	   and hope for the parent process to remove other segments in between. */

	do
	{
		shm_id = shmget( IPC_PRIVATE, len, IPC_CREAT | 0600 );

		if ( shm_id < 0 )
		{
			if ( errno == ENOSPC )
				usleep( 10000 );
			else                             /* non-recoverable failure... */
			{
				eprint( FATAL, "Internal communication problem at %s:%d.\n",
						__FILE__, __LINE__ );
				THROW( EXCEPTION );
			}
		}
	} while ( shm_id < 0 );

	/* Attach to the shared memory segment - if this should fail (improbable)
	   delete the segment, print an error message and stop the measurement */

	if ( ( buf = shmat( shm_id, NULL, 0 ) ) == ( void * ) - 1 )
	{
		shmctl( shm_id, IPC_RMID, &shm_buf );       /* delete the segment */
		eprint( FATAL, "Internal communication error at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = buf;

	memcpy( ptr, "fsc2", 4 * sizeof( char ) );         /* magic id */
	ptr += 4 * sizeof( char );

	memcpy( ptr, &len, sizeof( long ) );               /* total length */
	ptr += sizeof( long );

	memcpy( ptr, &nsets, sizeof( int ) );              /* # data sets  */
	ptr += sizeof( int );

	for ( i = 0; i < nsets; i++ )
	{
		memcpy( ptr, &dp[ i ].nx, sizeof( long ) );     /* x-index */
		ptr += sizeof( long );

		memcpy( ptr, &dp[ i ].nx, sizeof( long ) );     /* y-index */
		ptr += sizeof( long );

		memcpy( ptr, &dp[ i ].nc, sizeof( long ) );     /* curve number */
		ptr += sizeof( int );
		
		memcpy( ptr, &dp[ i ].v->type, sizeof( int ) ); /* type of data */
		ptr += sizeof( int );
	
		switch( dp[ i ].v->type )                       /* and now the data  */
		{
			case INT_VAR :
				memcpy( ptr, &dp[ i ].v->val.lval, sizeof( long ) );
				ptr += sizeof( long );
				break;

			case FLOAT_VAR :
				memcpy( ptr, &dp[ i ].v->val.dval, sizeof( double ) );
				ptr += sizeof( double );
				break;

			case INT_TRANS_ARR :
				memcpy( ptr, &dp[ i ].v->len, sizeof( long ) );
				ptr += sizeof( long );
				memcpy( ptr, dp[ i ].v->val.lpnt,
						dp[ i ].v->len * sizeof( long ) );
				ptr += dp[ i ].v->len * sizeof( long );
				break;

			case FLOAT_TRANS_ARR :
				memcpy( ptr, &dp[ i ].v->len, sizeof( long ) );
				ptr += sizeof( long );
				memcpy( ptr, dp[ i ].v->val.dpnt,
						dp[ i ].v->len * sizeof( double ) );
				ptr += dp[ i ].v->len * sizeof( double );
				break;
		}
	}

	/* Detach from the segment with the data segment */

	shmdt( ( void * ) buf );

	/* Get rid of the array of structures returned by eval_display_args() */

	T_free( dp );
	
	/* Finally tell parent about the identifier etc. */

	Key->shm_id = shm_id;
	Key->type = DATA;

	kill( getppid( ), NEW_DATA );           /* signal parent the new data */

	/* That's all, folks... */

	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

DPoint *eval_display_args( Var *v, int *nsets )
{
	DPoint *dp = NULL;


	*nsets = 0;
	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing x-index in `display()'.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	do
	{
		/* Get (more) memory for the sets */

		dp = T_realloc( dp, ( *nsets + 1 ) * sizeof( DPoint ) );

		/* check and store the x-index */

		vars_check( v, INT_VAR | FLOAT_VAR );
	
		if ( v->type == INT_VAR )
			dp[ *nsets ].nx = v->val.lval - ARRAY_OFFSET;
		else
			dp[ *nsets ].nx = rnd( v->val.dval - ARRAY_OFFSET );

		if ( dp[ *nsets ].nx < 0 )
		{
			eprint( FATAL, "%s:%ld: Invalid x-index (%ld) in `display()'.\n",
					Fname, Lc, dp[ *nsets ].nx + ARRAY_OFFSET );
			THROW( EXCEPTION );
		}

		v = v->next;

		/* for 2D experiments test and get y-index */

		if ( G.dim == 2 )
		{
			if ( v == NULL )
			{
				eprint( FATAL, "%s:%ld: Missing y-index in `display()'.\n",
						Fname, Lc );
				THROW( EXCEPTION );
			}

			vars_check( v, INT_VAR | FLOAT_VAR );
	
			if ( v->type == INT_VAR )
				dp[ *nsets ].ny = v->val.lval - ARRAY_OFFSET;
			else
				dp[ *nsets ].ny = rnd( v->val.dval - ARRAY_OFFSET );

			if ( dp[ *nsets ].nx < 0 )
			{
				eprint( FATAL, "%s:%ld: Invalid y-index (%ld) in "
						"`display()'.\n",
						Fname, Lc, dp[ *nsets ].ny + ARRAY_OFFSET );
				THROW( EXCEPTION );
			}

			v = v->next;
		}

		/* Now test and get the data, i. e. store the pointer to the variable
		   containing it */

		if ( v == NULL )
		{
			eprint( FATAL, "%s:%ld: Missing data in `display()'.\n",
					Fname, Lc );
			THROW( EXCEPTION );
		}

		vars_check( v, INT_VAR | FLOAT_VAR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

		dp[ *nsets ].v = v;

		v = v->next;

		/* There can be several curves and we check if there's a curve number,
		   then we test and store it */

		if ( v == NULL )
		{
			dp[ *nsets ].nc = 0;
			( *nsets )++;
			return dp;
		}

		vars_check( v, INT_VAR | FLOAT_VAR );
	
		if ( v->type == INT_VAR )
			dp[ *nsets ].nc = v->val.lval - 1;
		else
			dp[ *nsets ].nc = rnd( v->val.dval - 1 );

		if ( dp[ *nsets ].nc < 0 || dp[ *nsets ].nc >= G.nc )
		{
			eprint( FATAL, "%s:%ld: Invalid curve number (%ld) in "
					"`display()'.\n", Fname, Lc, dp[ *nsets ].nc + 1 );
			THROW( EXCEPTION );
		}

		v = v->next;

		( *nsets )++;
	} while ( v != NULL );

	return dp;
}


/*---------------------------------------------*/
/* Function returns the dimension of an array. */
/*---------------------------------------------*/

Var *f_dim( Var *v )
{
	vars_check( v, ARR_REF );
	return vars_push( INT_VAR, ( long ) v->from->dim );
}


/*---------------------------------------------------------------------*/
/* Function returns the size of the dimension passed to the function,  */
/* i.e. the function takes two arguments, first the array and then the */
/* the dimension the size is needed for.                               */
/*---------------------------------------------------------------------*/

Var *f_size( Var *v )
{
	int size;


	vars_check( v, ARR_REF );
	vars_check( v->next, INT_VAR | FLOAT_VAR );

	if ( v->next->type == FLOAT_VAR )
	{
		eprint( WARN, "%s:%ld: WARNING: Float value used as index for array "
				"`%s' in function `size'.\n", Fname, Lc, v->from->name );
		size = ( int ) v->next->val.dval - ARRAY_OFFSET;
	}
	else
		size = ( int ) v->next->val.lval - ARRAY_OFFSET;

	if ( size >= v->from->dim )
	{
		eprint( FATAL, "%s:%ld: Array `%s' has only %d dimensions, can't "
				"return size of %d. dimension.\n", Fname, Lc, v->from->name,
				v->from->dim, size );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, ( long ) v->from->sizes[ size ] );
}


/*--------------------------------------------------------*/
/* Function returns the sizes of the different dimension  */
/* of an array as an 1-dimensional array.                 */
/*--------------------------------------------------------*/

Var *f_sizes( Var *v )
{
	vars_check( v, ARR_REF );
	return vars_push( INT_TRANS_ARR, v->from->sizes, ( long ) v->from->dim );
}


/*---------------------------------------------------------------------------*/
/* Function allows the user to select a file using the file selector. If the */
/* already file exists confirmation by the user is required. Then the file   */
/* is opened - if this fails the file selector is shown again. The FILE      */
/* pointer returned is stored in an array of FILE pointers for each of the   */
/* open files. The returned value is an INT_VAR with the index in the FILE   */
/* pointer array or -1 if no file was selected.                              */
/* (Optional) Input variables (each will replaced by a default string if the */
/* argument is either NULL or the empty string) :                            */
/* 1. Message string (not allowed to start with a backslash `\'!)            */
/* 2. Default pattern for file name                                          */
/* 3. Default directory                                                      */
/* 4. Default file name                                                      */
/* Alternatively, to hardcode a file name into the EDL program only send the */
/* file name instead of the message string, but with a backslash `\' as the  */
/* very first character (it will be skipped and not be used as part of the   */
/* file name). The other strings still can be set but will only be used if   */
/* opening the file fails.                                                   */
/*---------------------------------------------------------------------------*/

Var *f_getf( Var *var )
{
	Var *cur;
	int i;
	char *s[ 4 ] = { NULL, NULL, NULL, NULL };
	FILE *fp;
	long len;
	struct stat stat_buf;
	char *r = NULL;


	/* If there was a call of `f_save()' without a previous call to `f_getf()'
	   `f_save()' did call already call `f_getf()' by itself and now expects
	   no file identifiers at all anymore - in this case `No_File_Numbers' is
	   set.So, if we get a call to `f_getf()' while `No_File_Numbers' is set we
	   must tell the user that he can't have it both ways, i.e. he either has
	   to call `f_getf()' before any call to `f_save()' or never. */

	if ( No_File_Numbers )
	{
		eprint( FATAL, "%s:%ld: Call of `get_filename()' after call of "
				"`save()' without previous call of `get_filename()'.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* While test is running just return a dummy value */

	if ( TEST_RUN )
		return vars_push( INT_VAR, File_List_Len++ );

	/* Check the arguments and supply default values if necessary */

	for ( i = 0, cur = var; i < 4 && cur != NULL; i++, cur = cur->next )
	{
		vars_check( cur, STR_VAR );
		s[ i ] = cur->val.sptr;
	}

	/* First string is the message */

	if ( s[ 0 ] != NULL && s[ 0 ][ 0 ] == '\\' )
		r = get_string_copy( s[ 0 ] + 1 );

	if ( s[ 0 ] == NULL || s[ 0 ] == "" || s[ 0 ][ 0 ] == '\\' )
		s[ 0 ] = get_string_copy( "Please enter a file name:" );
	else
		s[ 0 ] = get_string_copy( s[ 0 ] );

	/* Second string is the is the file name pattern */

	if ( s[ 1 ] == NULL || s[ 1 ] == "" )
		s[ 1 ] = get_string_copy( "*.dat" );
	else
		s[ 1 ] = get_string_copy( s[ 1 ] );

	/* Third string is the default directory */

	if ( s[ 2 ] == NULL || s[ 2 ] == "" )
	{
		s[ 2 ] = NULL;
		len = 0;
		do
		{
			len += PATH_MAX;
			s[ 2 ] = T_realloc( s[ 2 ], len );
			getcwd( s[ 2 ], len );
		} while ( s[ 2 ] == NULL && errno == ERANGE );
		if ( s[ 2 ] == NULL )
			s[ 2 ] = get_string_copy( "" );
	}
	else
		s[ 2 ] = get_string_copy( s[ 2 ] );

	if ( s[ 3 ] == NULL )
		s[ 3 ] = get_string_copy( "" );
	else
		s[ 3 ] = get_string_copy( s[ 3 ] );
		   

getfile_retry:

	/* Try to get a filename - on 'Cancel' request confirmation (unless a
	   file name was passed to the routine and this is not a repeat call) */

	if ( r == NULL )
		r = get_string_copy( show_fselector( s[ 0 ], s[ 2 ], 
											 s[ 1 ], s[ 3 ] ) );

	if ( ( r == NULL || *r == '\0' ) &&
		 1 != show_choices( "Do you really want to cancel saving data?\n"
							"        The data will be lost!",
							2, "Yes", "No", NULL, 2 ) )
	{
		if ( r != NULL )
		{
			T_free( r );
			r = NULL;
		}
		goto getfile_retry;
	}

	if ( r == NULL || *r == '\0' )         /* on 'Cancel' with confirmation */
	{
		if ( r != NULL )
			T_free( r );
		for ( i = 0; i < 4; i++ )
			T_free( s[ i ] );
		return vars_push( INT_VAR, -1 );
	}

	/* Now ask for confirmation if the file already exists and try to open
	   it for writing */

	if  ( 0 == stat( r, &stat_buf ) &&
		  1 != show_choices( "The selected file does already exist!\n"
							 " Do you really want to overwrite it?",
							 2, "Yes", "No", NULL, 2 ) )
	{
		T_free( r );
		r = NULL;
		goto getfile_retry;
	}

	if ( ( fp = fopen( r, "w" ) ) == NULL )
	{
		switch( errno )
		{
			case EMFILE :
				show_message( "Sorry, you have too many open files!\n"
							  "Please close at least one and retry." );
				break;

			case ENFILE :
				show_message( "Sorry, system limit for open files exceeded!\n"
							  " Please try to close some files and retry." );
				break;

			case ENOSPC :
				show_message( "Sorry, no space left on device for more file!\n"
							  "    Please delete some files and retry." );
				break;

			default :
				show_message( "Sorry, can't open selected file for writing!\n"
							  "       Please select a different file." );
		}

		T_free( r );
		r = NULL;
		goto getfile_retry;
	}

	File_List = T_realloc( File_List, 
						   ( File_List_Len + 1 ) * sizeof( FILE * ) );
	File_List[ File_List_Len ] = fp;

	T_free( r );
	for ( i = 0; i < 4; i++ )
		T_free( s[ i ] );

	return vars_push( INT_VAR, File_List_Len++ );
}


/*---------------------------------------------------------------------*/
/* This function is called by the functions for saving. If they didn't */
/* get a file identifier it is assumed the user wants just one file    */
/* that is opened at the first call of a function of the `save_xxx()'  */
/* family of functions.                                                */
/*---------------------------------------------------------------------*/

int get_save_file( Var **v, const char *calling_function )
{
	Var *get_file_ptr;
	Var *file;
	int file_num;
	int access;


	/* If no file has been selected yet get a file and then use it exclusively
	   (i.e. also expect that no file identifier is given in later calls),
	   otherwise the first variable has to be the file identifier */

	if ( File_List_Len == 0 )
	{
		if ( Dont_Save )
			return -1;

		No_File_Numbers = UNSET;

		get_file_ptr = func_get( "get_file", &access );
		file = func_call( get_file_ptr );         /* get the file name */

		No_File_Numbers = SET;

		if ( file->val.lval < 0 )
		{
			vars_pop( file );
			Dont_Save = SET;
			return -1;
		}
		vars_pop( file );
		file_num = 0;
	}
	else if ( ! No_File_Numbers )                    /* file number is given */
	{
		if ( *v != NULL )
		{
			if ( ( *v )->type != INT_VAR )
			{
				eprint( FATAL, "%s:%ld: First argument in `%s' isn't a "
						"file identifier.\n", Fname, Lc, calling_function );
				THROW( EXCEPTION );
			}
			file_num = ( int ) file->val.lval;
		}
		else
		{
			eprint( WARN, "%s:%ld: Call of `%s' without any arguments.\n",
					Fname, Lc, calling_function );
			return -1;
		}
		*v = ( *v )->next;
	}
	else
		file_num = 0;

	/* Check that the file identifier is reasonable */

	if ( file_num < 0 )
	{
		eprint( WARN, "%s:%ld: File has never been opened, skipping "
				"`%s' command.\n", Fname, Lc, calling_function );
		return -1;
	}

	if ( file_num >= File_List_Len )
	{
		eprint( FATAL, "%s:%ld: Invalid file identifier in `%s'.\n",
				Fname, Lc, calling_function );
		THROW( EXCEPTION );
	}

	return file_num;
}


/*--------------------------------*/
/* Closes all opened output files */
/*--------------------------------*/

void close_all_files( void )
{
	int i;

	if ( File_List == NULL )
	{
		File_List_Len = 0;
		return;
	}

	for ( i = 0; i < File_List_Len; i++ )
		fclose( File_List[ i ] );
	T_free( File_List );
	File_List = NULL;
	File_List_Len = 0;
}


/*--------------------------------------------------------------------------*/
/* Saves data to a file. If `get_file()' hasn't been called yet it will be  */
/* called now - in this case the file opened this way is the only file to   */
/* be used and no file identifier is allowed as first argument to `save()'. */
/* This version of save writes the data in an unformated way, i.e. each     */
/* on its own line wih the only exception of arrays of more than one        */
/* dimension where a edmpty line is put between the slices.                 */
/*--------------------------------------------------------------------------*/

Var *f_save( Var *v )
{
	long i;
	long start;
	int file_num;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "save()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v == NULL )
	{
		eprint( WARN, "%s:%ld: Call of `save()' without any data.\n",
				Fname, Lc );
		return vars_push( INT_VAR, 0 );
	}

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	do
	{
		switch( v->type )
		{
			case INT_VAR :
				fprintf( File_List[ file_num ], "%ld\n", v->val.lval );
				break;

			case FLOAT_VAR :
				fprintf( File_List[ file_num ], "%#g\n", v->val.dval );
				break;

			case STR_VAR :
				fprintf( File_List[ file_num ], "%s\n", v->val.sptr );
				break;

			case INT_TRANS_ARR :
				for ( i = 0; i < v->len; i++ )
					fprintf( File_List[ file_num ], "%ld\n",
							 v->val.lpnt[ i ] );
				break;
				
			case FLOAT_TRANS_ARR :
				for ( i = 0; i < v->len; i++ )
					fprintf( File_List[ file_num ], "%#g\n", 
							 v->val.dpnt[ i ] );
				break;

			case ARR_PTR :
				print_slice( v, file_num );
				break;

			case ARR_REF :
				if ( v->from->flags && NEED_ALLOC )
				{
					eprint( WARN, "%s:%ld: Variable sized array `%s' is still "
							"undefined - skipping `save()'.\n", 
							Fname, Lc, v->from->name );
					break;
				}
				start = 0;
				print_array( v->from, 0, &start, file_num );
				break;

			default :
				assert( 1 == 0 );
		}

		v = v->next;
	} while ( v );

	fflush( File_List[ file_num ] );

	return vars_push( INT_VAR, 1 );
}


void print_array( Var *v, long cur_dim, long *start, int fid )
{
	long i;

	if ( cur_dim < v->dim - 1 )
		for ( i = 0; i < v->sizes[ cur_dim ]; i++ )
			print_array( v, cur_dim + 1, start, fid );
	else
	{
		for ( i = 0; i < v->sizes[ cur_dim ]; (*start)++, i++ )
		{
			if ( v->type == INT_ARR )
				fprintf( File_List[ fid ], "%ld\n", v->val.lpnt[ *start ] );
			else
				fprintf( File_List[ fid ], "%f\n", v->val.dpnt[ *start ] );
		}

		fprintf( File_List[ fid ], "\n" );
	}
}


void print_slice( Var *v, int fid )
{
	long i;

	for ( i = 0; i < v->from->sizes[ v->from->dim - 1 ]; i++ )
	{
		if ( v->from->type == INT_ARR )
			fprintf( File_List[ fid ], "%ld\n", 
					 * ( ( long * ) v->val.gptr + i ) );
		else
			fprintf( File_List[ fid ], "%f\n", 
					 * ( ( double * ) v->val.gptr + i ) );
	}
}


/*--------------------------------------------------------------------------*/
/* Saves data to a file. If `get_file()' hasn't been called yet it will be  */
/* called now - in this case the file opened this way is the only file to   */
/* be used and no file identifier is allowed as first argument to `save()'. */
/* This function is the formated save with the same meaning of the format   */
/* string as in `print()'.                                                  */
/*--------------------------------------------------------------------------*/

Var *f_fsave( Var *v )
{
	int file_num;
	char *fmt;
	char *cp;
	char *ep;
	Var *cv;
	char *sptr;
	int in_format,
		on_stack,
		s;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "fsave()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v == NULL )
	{
		eprint( WARN, "%s:%ld: Call of `fsave()' without format sting and "
				"data.\n", Fname, Lc );
		return vars_push( INT_VAR, 0 );
	}

	if ( v->type != STR_VAR )
	{
		eprint( FATAL, "%s:%ld: Missing format string in call of `fsave()'\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	sptr = cp = v->val.sptr;

	/* Count the number of specifiers `#' in the format string but don't count
	   escaped `#' (i.e "\#") */

	for ( in_format = 0, sptr; ( cp = strchr( cp, '#' ) ) != NULL;
		  ++cp )
		if ( cp == fmt || *( cp - 1 ) != '\\' )
			in_format++;

	/* Check and count number of variables on the stack following the format
	   string */

	for  ( cv = v->next, on_stack = 0; cv != NULL; ++on_stack, cv = cv->next )
		vars_check( cv, INT_VAR | FLOAT_VAR | STR_VAR );

	/* Check that there are at least as many variables are on the stack 
	   as there specifiers in the format string */

	if ( on_stack < in_format )
		eprint( SEVERE, "%s:%ld: Less data than format descriptors in "
				"`save()' format string.\n", Fname, Lc );

	/* Warn if there are more data than format descriptors */

	if ( on_stack > in_format )
		eprint( SEVERE, "%s:%ld: More data than format descriptors in "
				"`save()' format string.\n", Fname, Lc );

	/* Count number of `#' */

	for ( cp = sptr, s = 0; *cp != '\0' ; ++cp )
		if ( *cp == '#' )
			s++;

	/* Get string long enough to replace each `#' by a 4-char sequence 
	   plus a '\0' */

	fmt = get_string( strlen( sptr ) + 4 * s + 2 );
	strcpy( fmt, sptr );

	for ( cp = fmt; *cp != '\0'; ++cp )
	{
		/* Skip normal characters */

		if ( *cp != '\\' && *cp != '#' )
			continue;

		/* Convert format descriptor (unescaped `#') to 5 \x01 */

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

		/* Replace escape sequences */

		switch ( *( cp + 1 ) )
		{
			case '#' :
				*cp = '#';
				break;

				break;

			case 'n' :
				*cp = '\n';
				break;

			case 't' :
				*cp = '\t';
				break;

			case '\\' :
				*cp = '\\';
				break;

			case '\"' :
				*cp = '"';
				break;

			default :
				eprint( WARN, "%s:%ld: Unknown escape sequence \\%c in "
						"`save()' format string.\n", Fname, Lc, *( cp + 1 ) );
				*cp = *( cp + 1 );
				break;
		}
		
		for ( ep = cp + 1; *ep != '\0'; ep++ )
			*ep = *( ep + 1 );
	}

	/* Now lets start printing... */

	cp = fmt;
	cv = v->next;
	while ( ( ep = strstr( cp, "\x01\x01\x01\x01\x01" ) ) != NULL )
	{
		if ( cv != NULL )      /* skip printing if there are not enough data */
		{
			switch ( cv->type )
			{
				case INT_VAR :
					strcpy( ep, "%ld" );
					fprintf( File_List[ file_num ], cp, cv->val.lval );
					break;

				case FLOAT_VAR :
					strcpy( ep, "%#g" );
					fprintf( File_List[ file_num ], cp, cv->val.dval );
					break;

				case STR_VAR :
					strcpy( ep, "%s" );
					fprintf( File_List[ file_num ], cp, cv->val.sptr );
					break;

				default :
					assert( 1 == 0 );
			}

			cv = cv->next;
		}

		cp = ep + 5;
	}

	fprintf( File_List[ file_num ], cp );
	fflush( File_List[ file_num ] );

	/* Finally free the copy of the format string and return */

	T_free( fmt );

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------------*/
/* Saves the EDL program to a file. If `get_file()' hasn't been called yet */
/* it will be called now - in this case the file opened this way is the    */
/* only file to be used and no file identifier is allowed as first argu-   */
/* ment to `save()'.                                                       */
/* Beside the file identifier the other (optional) parameter is a string   */
/* that gets prepended to each line of the EDL program (i.e. a comment     */
/* character).                                                             */
/*-------------------------------------------------------------------------*/

Var *f_save_p( Var *v )
{
	int file_num;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "save_program()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v != NULL )
		vars_check( v, STR_VAR );

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	print_browser( 0, file_num, v != NULL ? v->val.sptr : "" );

	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------------------------*/
/* Saves the content of the output window to a file. If `get_file()' hasn't */
/* been called yet it will be called now - in this case the file opened     */
/* this way is the only file to be used and no file identifier is allowed   */
/* as first argument to `save()'.                                           */
/* Beside the file identifier the other (optional) parameter is a string    */
/* that gets prepended to each line of the output (i.e. a comment char).    */
/*--------------------------------------------------------------------------*/

Var *f_save_o( Var *v )
{
	int file_num;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "save_output()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v != NULL )
		vars_check( v, STR_VAR );

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	print_browser( 1, file_num, v != NULL ? v->val.sptr : "" );

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------------------*/
/* Writes either the contnt of the program or the error browser into a file. */
/* Input parameter:                                                          */
/* 1. 0: writes program browser, 1: error browser                            */
/* 2. File identifier                                                        */
/* 3. Comment string to prepend to each line                                 */
/*---------------------------------------------------------------------------*/

void print_browser( int browser, int fid, const char* comment )
{
	char *line;


	writer( browser ==  0 ? C_PROG : C_OUTPUT );
	if ( comment == NULL )
		comment = "";
	fprintf( File_List[ fid ], "%s\n", comment );
	while ( 1 )
	{
		reader( &line );
		if ( line != NULL )
			fprintf( File_List[ fid ], "%s%s\n", comment, line );
		else
			break;
	}
	fprintf( File_List[ fid ], "%s\n", comment );
	fflush( File_List[ fid ] );
}


/*---------------------------------------------------------------------*/
/* Writes a comment into a file.                                       */
/* Arguments:                                                          */
/* 1. If first argument is a number it's treated as a file identifier. */
/* 2. It follows a comment string to prepend to each line of text      */
/* 3. A text to appear in the editor                                   */
/* 4. The label for the editor                                         */
/*---------------------------------------------------------------------*/

Var *f_save_c( Var *v )
{
	int file_num;
	const char *cc = NULL;
	char *c = NULL,
		 *l = NULL,
		 *r,
		 *cl, *nl;

	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "save_comment()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );


	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	/* Try to get the comment chars to prepend to each line */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		cc = v->val.sptr;
		v = v->next;
	}

	/* Try to get the predefined content of the editor */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		c = v->val.sptr;
		correct_line_breaks( c );
		v = v->next;
	}

	/* Try to get a label string for the editor */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		l = v->val.sptr;
	}

	/* Show the comment editor and get the returned contents (just one string
	   with embedded newline chars) */

	r = get_string_copy( show_input( c, l ) );

	if ( r == NULL )
		return vars_push( INT_VAR, 1 );

	cl = r;
	if ( cc == NULL )
		cc = "";
	fprintf( File_List[ file_num ], "%s\n", cc );
	while ( cl != NULL )
	{
		nl = strchr( cl, '\n' );
		if ( nl != NULL )
			*nl++ = '\0';
		fprintf( File_List[ file_num ], "%s%s\n", cc, cl );
		cl = nl;
	}
	if ( cc != NULL )
		fprintf( File_List[ file_num ], "%s\n", cc );
	fflush( File_List[ file_num ] );

	T_free( r );

	return vars_push( INT_VAR, 1 );
}
