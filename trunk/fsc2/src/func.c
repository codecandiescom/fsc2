/*
   $Id$
*/



#include "fsc2.h"



static void f_wait_alarm_handler( int sig_type );



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
Var *f_display( Var *v );
Var *f_dim( Var *v );
Var *f_size( Var *v );
Var *f_sizes( Var *v );


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
	{ "init_display", f_init_display, -1, ACCESS_ALL, 0 },
	{ "display",      f_display,      -1, ACCESS_EXP, 0 },
	{ "dim",          f_dim,           1, ACCESS_ALL, 0 },
	{ "size",         f_size,          2, ACCESS_ALL, 0 },
	{ "sizes",        f_sizes,         1, ACCESS_ALL, 0 },
	{ NULL,           NULL,            0, 0,          0 }
	                                     /* marks last entry, don't remove ! */
};


/*--------------------------------------------------------------------*/
/* Function parses the function data base in `Functions' and makes up */
/* a complete list of all built-in and user-supplied functions.       */
/*--------------------------------------------------------------------*/

bool functions_init( void )
{
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
						eprint( NO_ERROR, cp, cv->val.sptr );
						break;

					default :
						eprint( FATAL, "XXXXXX\n" );
						THROW( EXCEPTION );
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


/*-------------------------------------------------------------------------*/
/* f_init_display() has to be called to initialize the display system. It  */
/* expects a variable number of arguments but at least two. The first arg- */
/* ument has to be the dimensionality, 2 or 3. The second argument is the  */
/* number of points in x-direction, and for 3D graphics the third must be  */
/* the number of points in y-direction. If the number of points isn't      */
/* known a 0 has to be used. Then follow optional label strings for the x- */
/* and y-axis.                                                             */
/*-------------------------------------------------------------------------*/

Var *f_init_display( Var *v )
{
	long dim;
	long nx, ny;
	char *l1, *l2;

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing arguments for function "
				"`init_display()'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );                /* get dimension */

	if ( v->type == INT_VAR )
		dim = v->val.lval;
	else
		dim = rnd( v->val.dval );

	if ( dim < 2 || dim > 3 )
	{
		eprint( FATAL, "%s:%ld: Invalid display dimension (%ld) in "
				"`init_display()', valid values are 2 or 3.\n",
				Fname, Lc, dim );
		THROW( EXCEPTION );
	}

	v = v->next;
	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing number of points in x-direction in "
				"`init_display()'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );  /* get # of points in x-direction */

	if ( v->type == INT_VAR )
		nx = v->val.lval;
	else
		nx = rnd( v->val.dval );

	if ( nx < 0 )
	{
		eprint( FATAL, "%s:%ld: Negative number of points in x-direction - "
				"use 0 if the number isn't known yet.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	v = v->next;

	if ( dim == 3 )                  /* for 3D get # of points in y-direction*/
	{
		if ( v == NULL )
		{
			eprint( FATAL, "%s:%ld: Missing number of points in y-direction "
					"in init_display()'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		vars_check( v, INT_VAR | FLOAT_VAR );

		if ( v->type == INT_VAR )
			ny = v->val.lval;
		else
			ny = rnd( v->val.dval );

		if ( ny < 0 )
		{
			eprint( FATAL, "%s:%ld: Negative number of points in y-direction "
					"- use 0 if the number isn't known yet.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		v = v->next;
	}

	if ( v != NULL )                  /* get x-label */
	{
		vars_check( v, STR_VAR );
		l1 = v->val.sptr;
		if ( strlen( l1 ) == 0 )
			l1 = NULL;
		v = v->next;
	}
	else
		l1 = NULL;

	if ( v != NULL && dim == 3 )
	{
		vars_check( v, STR_VAR );
		l2 = v->val.sptr;
		if ( strlen( l2 ) == 0 )
			l2 = NULL;
	}
	else
		l2 = NULL;

	graphics_init( dim, nx, ny, l1, l2 );
	return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------------------*/
/* f_display() is used to send new data to the display system. */
/*-----------------------------------------------------------------*/

Var *f_display( Var *v )
{
	int nx, ny;
	int overdraw_flag = 1;


	/* We can't display daa without a previous initialization */

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

	v = v->next;

	if ( G.dim == 3 )                          /* get y-index for 3D data */
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

	if ( v->next != NULL )
		overdraw_flag = 0;

	if ( I_am == PARENT )
		return vars_push( INT_VAR, 1 );

	switch( v->type )
	{
		case INT_VAR :
			writer( C_DATA, nx, ny, INT_VAR, v->val.lval, overdraw_flag );
			break;

		case FLOAT_VAR :
			writer( C_DATA, nx, ny, FLOAT_VAR, v->val.dval, overdraw_flag );
			break;

		case INT_TRANS_ARR :
			writer( C_DATA, nx, ny, INT_TRANS_ARR, v->len, v->val.lpnt,
					overdraw_flag );
			break;

		case FLOAT_TRANS_ARR :
			writer( C_DATA, nx, ny, FLOAT_TRANS_ARR, v->len, v->val.dpnt,
					overdraw_flag );
			break;
	}

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
