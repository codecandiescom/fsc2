/*
   $Id$
*/


#include "fsc2.h"


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

   To add a new function append its declaration to the declarations just below
   this text and its properties (i.e. name as used in the EDL file, its name
   as declared here, its number of arguments and finally its accessibility) as
   another element to the `fncts' structure (don't change the very last entry
   marking the end of the structure's entries!). And, of course, define it
   somewhere here in this file...
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
Var *f_dummy( Var *v );



typedef struct
{
	const char *name;                    /* name of the function */
    Var * ( * fnct )( Var * );           /* pointer to the function */
	long nargs;                          /* number of arguments */
    int access_flag;                     /* asscessibility flag */
} Func;


/* extend this list to your heart's content but make sure the function
   to be called is defined and the following list still ends with two
   NULL's and two zeros...*/

Func fncts[ ] = { { "int",   f_int,    1, ACCESS_ALL_SECTIONS	},
				  { "float", f_float,  1, ACCESS_ALL_SECTIONS	},
				  { "round", f_round,  1, ACCESS_ALL_SECTIONS	},
				  { "floor", f_floor,  1, ACCESS_ALL_SECTIONS	},
				  { "ceil",  f_ceil,   1, ACCESS_ALL_SECTIONS	},
				  { "abs",   f_abs,    1, ACCESS_ALL_SECTIONS	},
				  { "sin",   f_sin,    1, ACCESS_ALL_SECTIONS	},
				  { "cos",   f_cos,    1, ACCESS_ALL_SECTIONS	},
  				  { "tan",   f_tan,    1, ACCESS_ALL_SECTIONS	},
				  { "asin",  f_asin,   1, ACCESS_ALL_SECTIONS	},
				  { "acos",  f_acos,   1, ACCESS_ALL_SECTIONS	},
  				  { "atan",  f_atan,   1, ACCESS_ALL_SECTIONS	},
				  { "sinh",  f_sinh,   1, ACCESS_ALL_SECTIONS	},
				  { "cosh",  f_cosh,   1, ACCESS_ALL_SECTIONS	},
  				  { "tanh",  f_tanh,   1, ACCESS_ALL_SECTIONS	},
				  { "exp",   f_exp,    1, ACCESS_ALL_SECTIONS	},
				  { "ln",    f_ln,     1, ACCESS_ALL_SECTIONS	},
  				  { "log",   f_log,    1, ACCESS_ALL_SECTIONS	},
				  { "sqrt",  f_sqrt,   1, ACCESS_ALL_SECTIONS	},
                  { "print", f_print, -1, ACCESS_ALL_SECTIONS	},
                  { "dummy", f_dummy,  1, ACCESS_ALL_SECTIONS	},
				  { "",      NULL,     0, 0 }          /* marks last entry! */
                };



Var *func_get( char *name, int *access )
{
	int i;
	Var *ret;


	/* try to find the function by its name and if found create a variable
	   on the variable stack with a pointer to the actual function and the
	   number of arguments. Also copy the functions name and access flag. */

	for ( i = 0; *fncts[ i ].name != '\0'; ++i )
	{
		if ( ! strcmp( fncts[ i ].name, name ) )
		{
			ret = vars_push( FUNC, fncts[ i ].fnct );
			ret->name = get_string_copy( name );
			ret->dim = fncts[ i ].nargs;
			*access = fncts[ i ].access_flag;
			return ret;
		}
	}

	return NULL;
}


Var *func_call( Var *f )
{
	Var *ap, *apn, *ret;
	int ac;
	

	/* <PARANOID> check that's really a function variable */

	assert( f->type == FUNC );

	/* </PARANOID> */

	/* if the number of function arguments isn't negative (indicating a
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
			THROW( FUNCTION_EXCEPTION );
		}
	}

	/* now call the function */

	ret = ( *f->val.fnct )( f->next );

	/* finally do clean up, i.e. for function with a known number of arguments
	   remove all variables from the stack, and also remove the function
	   variable und then return the result */

	if ( f->dim > 0 )
	{
		for ( ac = 0, ap = f->next; ac < f->dim; ++ac )
		{
			apn = ap->next;
			vars_pop( ap );
			ap = apn;
		}
	}
	vars_pop( f );

	return ret;
}


Var *f_int( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
		return vars_push( INT_VAR, (long ) v->val.dval );
}


Var *f_float( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, ( double ) v->val.lval );
	else
		return vars_push( FLOAT_VAR, v->val.dval );
}


Var *f_round( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
		return vars_push( INT_VAR,   ( long ) ( 2 * v->val.dval )
                                   - ( long ) v->val.dval );
}


Var *f_floor( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
		return vars_push( INT_VAR, ( long ) floor( v->val.dval ) );
}


Var *f_ceil( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->val.lval );
	else
		return vars_push( INT_VAR, ( long ) ceil( v->val.dval ) );
}


Var *f_abs( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, labs( v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, fabs( v->val.dval ) );
}


Var *f_sin( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, sin( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, sin( v->val.dval ) );
}


Var *f_cos( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, cos( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, cos( v->val.dval ) );
}


Var *f_tan( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, tan( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, tan( v->val.dval ) );
}


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
		THROW( FUNCTION_EXCEPTION );
	}

	return vars_push( FLOAT_VAR, asin( arg ) );
}


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
		THROW( FUNCTION_EXCEPTION );
	}

	return vars_push( FLOAT_VAR, acos( arg ) );
}


Var *f_atan( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, atan( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, atan( v->val.dval ) );
}


Var *f_sinh( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, sinh( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, sinh( v->val.dval ) );
}


Var *f_cosh( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, cosh( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, cosh( v->val.dval ) );
}


Var *f_tanh( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, tanh( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, tanh( v->val.dval ) );
}


Var *f_exp( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( FLOAT_VAR, exp( ( double ) v->val.lval ) );
	else
		return vars_push( FLOAT_VAR, exp( v->val.dval ) );
}


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
		THROW( FUNCTION_EXCEPTION );
	}
	return vars_push( FLOAT_VAR, log( arg ) );
}


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
		THROW( FUNCTION_EXCEPTION );
	}
	return vars_push( FLOAT_VAR, log10( arg ) );
}


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
		THROW( FUNCTION_EXCEPTION );
	}
	return vars_push( FLOAT_VAR, sqrt( arg ) );
}



/*------------------------------------------------------------------*/
/* The print() function returns the number of variables it printed, */
/* not counting the format string.                                  */
/*------------------------------------------------------------------*/

Var *f_print( Var *v )
{
	char *fmt;
	char *cp;
	char *ep;
	Var *cv;
	int in_format, on_stack, s;

	/* a call to print() without any argument is legal but rather
	   unreasonable... */

	if ( v == NULL )
		return vars_push( INT_VAR, 0 );

	/* make sure the first argument is a string */

	vars_check( v, STR_VAR );

	/* count the number of specifiers `#' in the format string but don't count
	   escaped `#' (i.e "\#") */

	for ( in_format = 0, cp = v->val.sptr; ( cp = strchr( cp, '#' ) ) != NULL;
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
	{
		eprint( FATAL, "%s:%ld: Less data than format descriptors in print() "
				"format string.\n", Fname, Lc );
		THROW( PRINT_SYNTAX_EXCEPTION );
	}

	/* utter a warning if there are more data than format descriptors */

	if ( on_stack > in_format )
		eprint( SEVERE, "%s:%ld: More data than format descriptors in print() "
				"format string.\n", Fname, Lc );

	/* count number of `#' */

	for ( cp = v->val.sptr, s = 0; *cp != '\0' ; ++cp )
		if ( *cp == '#' )
			s++;

	/* get string long enough to replace each `#' by a 3-char sequence */

	fmt = get_string( strlen( v->val.sptr ) + 2 * s + 1 );
	strcpy( fmt, v->val.sptr );

	for ( cp = fmt; *cp != '\0' ; ++cp )
	{
		/* skip normal characters */

		if ( *cp != '\\' && *cp != '#' )
			continue;

		/* convert format descriptor (unescaped `#') to 3 \x01 */

		if ( *cp == '#' )
		{
			for ( ep = fmt + strlen( fmt ) + 1; ep != cp; --ep )
				*( ep + 2 ) = *ep;
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
		}
		
		for ( ep = cp + 1; *ep != '\0'; ep++ )
			*ep = *( ep + 1 );
	}

	/* now lets start printing... */

	cp = fmt;
	cv = v->next;
	while ( ( ep = strstr( cp, "\x01\x01\x01" ) ) != NULL )
	{
		switch ( cv->type )
		{
			case INT_VAR :
				strcpy( ep, "%d" );
				eprint( NO_ERROR, cp, cv->val.lval );
				break;

			case FLOAT_VAR :
				strcpy( ep, "%g" );
				eprint( NO_ERROR, cp, cv->val.dval );
				break;

			case STR_VAR :
				strcpy( ep, "%s" );
				eprint( NO_ERROR, cp, cv->val.sptr );
				break;
		}

		cp = ep + 3;
		vars_pop( cv );
		cv = v->next;
	}

	if ( *cp != '\0' )
		eprint( NO_ERROR, cp );

	/* remove superfluous arguments (if there are any) */

	while ( ( cv = v->next ) != NULL )
		vars_pop( cv );

	/* finally free the copy of the format string as well as the variable with
	   the format string */

	free( fmt );
	vars_pop( v );

	return vars_push( INT_VAR, in_format );
}


Var *f_dummy( Var *v )
{
	long *x;
	long size;
	long i;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		size = v->val.lval;
	else
		size = ( long ) v->val.dval;

	x = T_calloc( size, sizeof( long ) );

	for ( i = 0; i < size; ++i )
		x[ i ] = i + 1;

	return vars_push( INT_TRANS_ARR, x, size );
}
