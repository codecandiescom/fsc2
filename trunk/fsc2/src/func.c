/*
   $Id$
*/



#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/shm.h>



static void f_wait_alarm_handler( int sig_type );
static int get_save_file( Var **v, const char *calling_function );
static void print_array( Var *v, long cur_dim, long *start, int fid );
static void print_slice( Var *v, int fid );
static void print_browser( int browser, int fid, const char* comment );


static bool No_File_Number;


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
Var *f_init_1d( Var *v );
Var *f_init_2d( Var *v );
Var *f_display( Var *v );
Var *f_dim( Var *v );
Var *f_size( Var *v );
Var *f_sizes( Var *v );
Var *f_getf( Var *v );
Var *f_save( Var *v );
Var *f_fsave( Var *v );
Var *f_save_p( Var *v );
Var *f_save_o( Var *v );


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
	{ "get_file",     f_getf,         -1, ACCESS_ALL, 0 },
	{ "save",         f_save,         -1, ACCESS_ALL, 0 },
	{ "fsave",        f_fsave,        -1, ACCESS_ALL, 0 },
	{ "save_program", f_save_p,       -1, ACCESS_ALL, 0 },
	{ "save_output",  f_save_o,       -1, ACCESS_ALL, 0 },
	{ NULL,           NULL,            0, 0,          0 }
	                                     /* marks last entry, don't remove ! */
};


/*--------------------------------------------------------------------*/
/* Function parses the function data base in `Functions' and makes up */
/* a complete list of all built-in and user-supplied functions.       */
/*--------------------------------------------------------------------*/

bool functions_init( void )
{
	No_File_Number = UNSET;

	/* count number of built-in functions */

	for ( Num_Def_Func = 0; Def_Fncts[ Num_Def_Func ].fnct != NULL;
		  Num_Def_Func++ )
		;

	Num_Func = Num_Def_Func;

	/* 1. Get new memory for the functions structures and copy the built in
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

	No_File_Number = UNSET;
	close_all_files( );
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
	

	/* Check that it's really a function variable - you can never be sure
	   someone isn't trying to be using some undocumented features but doesn't
	   get it right. (Do we really need this?) */

	if ( f->type != FUNC )
	{
		eprint( FATAL, "%s:%ld: Variable passed to `func_call()' doesn't "
				"have type `FUNC'.\n", Fname, Lc );
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


/*-------------------------------------------------*/
/* Conversion float to integer (result is integer) */
/*-------------------------------------------------*/

Var *f_int( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
		return vars_push( INT_VAR, ( long ) v->val.dval );
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
		return vars_push( INT_VAR,   ( long ) ( 2 * v->val.dval )
                                   - ( long ) v->val.dval );
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
		return vars_push( INT_VAR, ( long ) floor( v->val.dval ) );
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
		return vars_push( INT_VAR, ( long ) ceil( v->val.dval ) );
}


/*-------------------------------------------------*/
/* abs of value (result has same as type argument) */
/*-------------------------------------------------*/

Var *f_abs( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, labs( v->val.lval ) );
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
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, tan( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, tan( v->val.dval ) );
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
		eprint( FATAL, "%s:%ld: Argument for function `asin' is out of "
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
		eprint( FATAL, "%s:%ld: Argument for function `acos' is out of "
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
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, sinh( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, sinh( v->val.dval ) );
}


/*------------------------------------*/
/* cosh of argument (result is float) */
/*------------------------------------*/

Var *f_cosh( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, cosh( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, cosh( v->val.dval ) );
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
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, exp( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, exp( v->val.dval ) );
}


/*-----------------------------------------------*/
/* ln of argument (with x > 0) (result is float) */
/*-----------------------------------------------*/

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


/*------------------------------------------------*/
/* log of argument (with x > 0) (result is float) */
/*------------------------------------------------*/

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
		eprint( FATAL, "%s:%ld: Argument for function `sqrt' is negative.\n", 
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
/*-------------------------------------------------------------------*/

Var *f_init_1d( Var *v )
{
	long n = -1;
	double rwc_start = 0.0, rwc_delta = 0.0;
	char *label;


	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing number of points in `init_1d()'.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );  /* get # of points in x-direction */

	if ( v->type == INT_VAR )
		n = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Floating point value used as number of "
				"points.\n", Fname, Lc );
		n = rnd( v->val.dval );
	}

	v = v->next;

	/* If next value is an integer or a float this is the real world coordinate
	   and its increment */

	if ( v != NULL )
	{

		/* If next value is an integer or a float this is the real world
		   coordinate foolowed by the increment */

		if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		{
			if ( v->next == NULL || 
				 ! ( v->next->type & ( INT_VAR | FLOAT_VAR ) ) )
			{
				eprint( FATAL, "%s:%ld: Real word coordinate given but no "
						"increment.\n" );
				THROW( EXCEPTION );
			}

			rwc_start = VALUE( v );
			rwc_delta = VALUE( v->next );
			v = v->next->next;
		}

		if ( v != NULL )
		{
			vars_check ( v, STR_VAR );
			label = get_string_copy( v->val.sptr );
		}
		else
			label = NULL;
	}
	
	graphics_init( 1, n, 0, rwc_start, rwc_delta, 0.0, 0.0, label, NULL );
	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/* f_init_2d() has to be called to initialize the display system for */
/* 2-dimensional experiments.                                        */
/*-------------------------------------------------------------------*/

Var *f_init_2d( Var *v )
{
	long dim;
	long nx, ny;
	double rwc_x_start = 0.0, rwc_x_delta = 0.0, 
		   rwc_y_start = 0.0, rwc_y_delta = 0.0;
	char *l1 = NULL, *l2 = NULL;

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing arguments in `init_2d()'.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );  /* get # of points in x-direction */

	if ( v->type == INT_VAR )
		nx = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Floating point value used as number of "
				"points in x-direction.\n", Fname, Lc );
		nx = rnd( v->val.dval );
	}


	v = v->next;

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing number of points in y-direction "
				"in init_2d()'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

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

	if ( v != NULL )
	{
		/* Now we expect either 4 real world coordinates (start and delta for
		   x- and y-direction) or none at all, labels instead */

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

	graphics_init( dim, nx, ny, 0.0, 0.0, 0.0, 0.0, l1, l2 );
	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------*/
/* f_display() is used to send new data to the display system. */
/*-------------------------------------------------------------*/

Var *f_display( Var *v )
{
	int nx, ny;
	int overdraw_flag = 1;
	int shm_id;
	struct shmid_ds shm_buf;
	long len = 0;                    /* total length of message to send */
	void *buf;
	void *ptr;


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

	/* We begin with checking the arguments */

	len += sizeof( long );                  /* for the length itself */

	/* Get the x-index for the data */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing x-index in `display()'.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	
	if ( v->type == INT_VAR )
		nx = v->val.lval;
	else
		nx = rnd( v->val.dval );
	len += sizeof( long );

	v = v->next;

	if ( G.dim == 2 )                       /* for 2D experiment get y-index */
	{
		if ( v == NULL )
		{
			eprint( FATAL, "%s:%ld: Missing y-index in `display()'.\n",
					Fname, Lc );
			THROW( EXCEPTION );
		}
		
		vars_check( v, INT_VAR | FLOAT_VAR );
	
		if ( v->type == INT_VAR )
			ny = v->val.lval;
		else
			ny = rnd( v->val.dval );

		len += sizeof( long );

		v = v->next;
	}
	else
		ny = 0;

	/* Now check type of data */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing data in `display()'.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	/* Check the overdraw flag */

	if ( v->next != NULL )
		overdraw_flag = 0;
	len += 2 * sizeof( int );                      /* for flag and data type */

	if ( I_am == PARENT )      /* i.e. as long as this is only a test run... */
		return vars_push( INT_VAR, 1 );

	/* We need to determine the amount of shared memory needed */

	switch( v->type )
	{
		case INT_VAR :
			len += sizeof( long );
			break;

		case FLOAT_VAR :
			len += sizeof( double );
			break;

		case INT_TRANS_ARR :
			len += v->len * sizeof( long );
			break;

		case FLOAT_TRANS_ARR :
			len += v->len * sizeof( double );
			break;
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
				eprint( FATAL, "Internal communication problem at %s%d.\n",
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
		eprint( FATAL, "Internal communication error at %s%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = buf;

	memcpy( ptr, &len, sizeof( long ) );          /* length */
	ptr += sizeof( long );

	memcpy( ptr, &nx, sizeof( long ) );           /* x-index */
	ptr += sizeof( long );

	memcpy( ptr, &ny, sizeof( long ) );           /* y-index */
	ptr += sizeof( long );

	memcpy( ptr, &overdraw_flag, sizeof( int ) ); /* overdraw flag */
	ptr += sizeof( int );

	memcpy( ptr, &v->type, sizeof( int ) );       /* type of data... */
	ptr += sizeof( int );
	
	switch( v->type )                             /* ...and now the data  */
	{
		case INT_VAR :
			memcpy( ptr, &v->val.lval, sizeof( long ) );
			ptr += sizeof( long );
			break;

		case FLOAT_VAR :
			memcpy( ptr, &v->val.dval, sizeof( double ) );
			ptr += sizeof( double );
			break;

		case INT_TRANS_ARR :
			memcpy( ptr, v->val.lpnt, v->len * sizeof( long ) );
			ptr += v->len * sizeof( long );
			break;

		case FLOAT_TRANS_ARR :
			memcpy( ptr, v->val.dpnt, v->len * sizeof( double ) );
			ptr += v->len * sizeof( double );
			break;

		default :                   /* this better never happens... */
			shmdt( ( void * ) buf );
			shmctl( shm_id, IPC_RMID, &shm_buf );       /* delete segment */
			eprint( FATAL, "Internal communication error at %s%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
	}

	/* Detach the the segment with the data segment */

	shmdt( ( void * ) buf );

	/* Finally tell parent about the identifier etc. */

	Key->shm_id = shm_id;
	Key->type = DATA;

	kill( getppid( ), NEW_DATA );           /* signal parent the new data */

	/* That's all, folks... */

	return vars_push( INT_VAR, 1 );
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
/* (Optional) Input variables:                                               */
/* 1. Message string                                                         */
/* 2. Default pattern for file name                                          */
/* 3. Default directory                                                      */
/* 4. Default file name                                                      */
/*---------------------------------------------------------------------------*/

Var *f_getf( Var *var )
{
	Var *cur;
	int i;
	char *s[ 4 ] = { NULL, NULL, NULL, NULL };
	const char *r;
	FILE *fp;
	long len;
	struct stat stat_buf;


	/* If there was a call of `f_save()' without a previous call to `f_getf()'
	   `f_save()' did call already call `f_getf()' by itself and now expects
	   no file identifiers at all anymore - in this case `No_File_Number' is
	   set.So, if we get a call to `f_getf()' while `No_File_Number' is set we
	   must tell the user that he can't have it both ways, i.e. he either has
	   to call `f_getf()' before any call to `f_save()' or never. */

	if ( No_File_Number )
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

	if ( s[ 0 ] == NULL || s[ 0 ] == "" )
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

	/* Try to get a filename - on 'Cancel' request confirmation */

	r = show_fselector( s[ 0 ], s[ 2 ], s[ 1 ], s[ 3 ] );

	if ( ( r == NULL || *r == '\0' ) &&
		 1 != show_choices( "Do you really want to cancel selecting\n"
							"the file name ? Data will not be saved!",
							2, "Yes", "No", NULL, 2 ) )
		 goto getfile_retry;

	if ( r == NULL || *r == '\0' )         /* on 'Cancel' with confirmation */
	{
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
		goto getfile_retry;

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

		goto getfile_retry;
	}

	File_List = T_realloc( File_List, 
						   ( File_List_Len + 1 ) * sizeof( FILE * ) );
	File_List[ File_List_Len ] = fp;

	for ( i = 0; i < 4; i++ )
		T_free( s[ i ] );

	return vars_push( INT_VAR, File_List_Len++ );
}


int get_save_file( Var **v, const char *calling_function )
{
	Var *get_file_ptr;
	Var *file;
	int file_num;
	int access;


	/* If no file has been selected yet get a file and use it exclusively
	   (i.e. also expect that no file identifier is given in later calls),
	   otherwise the first variable has to be the file identifier */

	if ( File_List_Len == 0 )
	{
		No_File_Number = UNSET;

		get_file_ptr = func_get( "get_file", &access );
		file = func_call( get_file_ptr );         /* get the file name */

		No_File_Number = SET;

		if ( file->val.lval < 0 )
		{
			vars_pop( file );
			return -1;
		}
		vars_pop( file );
		file_num = 0;
	}
	else if ( ! No_File_Number )                 /* file numbers are given */
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
		eprint( WARN, "%s:%ld: File never has been opened, skipping "
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


void print_browser( int browser, int fid, const char* comment )
{
	char *line;


	writer( browser ==  0 ? C_PROG : C_OUTPUT );
	while ( 1 )
	{
		reader( &line );
		if ( line != NULL )
			fprintf( File_List[ fid ], "%s%s\n", comment, line );
		else
			break;
	}
	fflush( File_List[ fid ] );
}
