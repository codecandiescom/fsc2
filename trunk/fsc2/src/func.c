#include "fsc2.h"


double f_int( long n, double *args );
double f_float( long n, double *args );
double f_round( long n, double *args );
double f_floor( long n, double *args );
double f_ceil( long n, double *args );
double f_sin( long n, double *args );
double f_cos( long n, double *args );
double f_tan( long n, double *args );
double f_asin( long n, double *args );
double f_acos( long n, double *args );
double f_atan( long n, double *args );
double f_sinh( long n, double *args );
double f_cosh( long n, double *args );
double f_tanh( long n, double *args );
double f_exp( long n, double *args );
double f_ln( long n, double *args );
double f_log( long n, double *args );
double f_sqrt( long n, double *args );



typedef struct
{
	const char *name;                    /* name of the function */
    double ( *fnct )( long, double * );  /* pointer to the function */
	long nargs;                          /* number of arguments */
	int ret_type;                        /* return type of function */
    int access_flag;                     /* asscessibility flag */
} Func;


/* extend this list to your heart's content but make sure the function
   to be called is defined and the following list still ends with two
   NULL's and three zeros...*/

Func fncts[ ] = { { "int",   f_int,   1, INT_VAR,   ACCESS_ALL_SECTIONS	},
				  { "float", f_float, 1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "round", f_round, 1, INT_VAR,   ACCESS_ALL_SECTIONS	},
				  { "floor", f_floor, 1, INT_VAR,   ACCESS_ALL_SECTIONS	},
				  { "ceil",  f_ceil,  1, INT_VAR,   ACCESS_ALL_SECTIONS	},
				  { "sin",   f_sin,   1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "cos",   f_cos,   1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
  				  { "tan",   f_tan,   1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "asin",  f_asin,  1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "acos",  f_acos,  1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
  				  { "atan",  f_atan,  1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "sinh",  f_sinh,  1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "cosh",  f_cosh,  1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
  				  { "tanh",  f_tanh,  1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "exp",   f_exp,   1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "ln",    f_ln,    1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
  				  { "log",   f_log,   1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "sqrt",  f_sqrt,  1, FLOAT_VAR, ACCESS_ALL_SECTIONS	},
				  { "",      NULL,    0, 0,         0 } /* marks last entry! */
                };



Var *func_get( char *name, int *access )
{
	int i;
	Var *ret;


	/* try to find the function by its name and if found create a variable
	   on the variable stack with a pointer to the actual function and the
	   number of arguments - also copy the functions name */

	for ( i = 0; *fncts[ i ].name != '\0'; ++i )
	{
		if ( ! strcmp( fncts[ i ].name, name ) )
		{
			ret = vars_push( FUNC, NULL );
			ret->val.fnct = fncts[ i ].fnct;
			ret->name = get_string_copy( name );
			ret->dim = fncts[ i ].nargs;
			ret->len = i;
			*access = fncts[ i ].access_flag;
			return( ret );
		}
	}

	return( NULL );
}


Var *func_call( Var *f )
{
	Var *sptr, *sptr_old, *ret;
	int ac;
	double *args = NULL;
	double dret;
	long iret;
	

	/* check that's really a function variable - just being paranoid... */

	assert( f->type == FUNC );

	/* count the number of arguments and make sure they all are of
	   integer or float type */

	for ( ac = 0, sptr = f->next; sptr != NULL; ++ac, sptr = sptr->next )
	{
		if ( sptr->type != INT_VAR && sptr->type != FLOAT_VAR )
		{
			eprint( FATAL, "%s:%ld: Argument #%d of function `%s' is "
					"non-numerical.\n", Fname, Lc, ac, fncts[ f->len ].name );
			THROW( FUNCTION_EXCEPTION );
		}

		if ( sptr->new_flag )
			eprint( WARN, "%s:%ld: WARNING: Argument #%d of function `%s' "
					"never has been assigned a value.\n", Fname, Lc, ac,
					fncts[ f->len ].name );
	}

	/* check that the number of arguments is correct */

	if ( ac > f->dim )
	{
		eprint( FATAL, "%s:%ld: Too many arguments for function `%s'.\n",
				Fname, Lc, fncts[ f->len ].name );
		THROW( FUNCTION_EXCEPTION );
	}
	if ( ac < f->dim )
	{
		eprint( FATAL, "%s:%ld: Not enough arguments for function `%s'.\n",
				Fname, Lc, fncts[ f->len ].name );
		THROW( FUNCTION_EXCEPTION );
	}

	/* get an array for the arguments and copy the arguments */

	if ( ac > 0 )
	{
		args = ( double * ) T_malloc( ac * sizeof( double ) );

		for ( ac = 0, sptr = f->next; sptr != NULL; ++ac )
		{
			args[ ac ] = ( sptr->type == INT_VAR ) ? 
				( double ) sptr->val.lval : sptr->val.dval;
			sptr_old = sptr;
			sptr = sptr->next;
			vars_pop( sptr_old );
		}
	}

	/* now call the function and push the return value onto the stack
	   (and don't forget to pop the variable for the function) */

	if ( fncts[ f->len ].ret_type == INT_VAR )
	{
		 iret = ( long ) ( *f->val.fnct )( f->len, args );
		 ret = vars_push( INT_VAR, &iret );
	}
	else
	{
		 dret = ( double ) ( *f->val.fnct )( f->len, args );
		 ret = vars_push( FLOAT_VAR, &dret );
	}

	/* finally do clean up and return result */

	if ( args )
		free( args );
	vars_pop( f );

	return( ret );
}


double f_int( long n, double *args )
{
	return( *args );
}


double f_float( long n, double *args )
{
	return( *args );
}


double f_round( long n, double *args )
{
	return( ( double ) ( ( long ) ( 2 * *args ) - ( long ) *args ) );
}


double f_floor( long n, double *args )
{
	return( floor( *args ) );
}


double f_ceil( long n, double *args )
{
	return( ceil( *args ) );
}


double f_sin( long n, double *args )
{
	return( sin( *args ) );
}


double f_cos( long n, double *args )
{
	return( cos( *args ) );
}


double f_tan( long n, double *args )
{
	return( tan( *args ) );
}


double f_asin( long n, double *args )
{
	if ( fabs( *args ) > 1.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `asin' is out of "
				"range.\n", Fname, Lc );
		THROW( FUNCTION_EXCEPTION );
	}

	return( asin( *args ) );
}


double f_acos( long n, double *args )
{
	if ( fabs( *args ) > 1.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `acos' is out of "
				"range.\n",  Fname, Lc );
		THROW( FUNCTION_EXCEPTION );
	}

	return( acos( *args ) );
}


double f_atan( long n, double *args )
{
	return( atan( *args ) );
}


double f_sinh( long n, double *args )
{
	return( sinh( *args ) );
}


double f_cosh( long n, double *args )
{
	return( cosh( *args ) );
}


double f_tanh( long n, double *args )
{
	return( tanh( *args ) );
}


double f_exp( long n, double *args )
{
	return( exp( *args ) );
}


double f_ln( long n, double *args )
{
	if ( *args <= 0.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `ln' is out of "
				"range.\n", Fname, Lc );
		THROW( FUNCTION_EXCEPTION );
	}

	return( log( *args ) );
}


double f_log( long n, double *args )
{
	if ( *args <= 0.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `log' is out of "
				"range.\n", Fname, Lc );
		THROW( FUNCTION_EXCEPTION );
	}

	return( log10( *args ) );
}


double f_sqrt( long n, double *args )
{
	if ( *args < 0.0 )
	{
		eprint( FATAL, "%s:%ld: Argument for function `sqrt' is negative.\n", 
				Fname, Lc );
		THROW( FUNCTION_EXCEPTION );
	}

	return( sqrt( *args ) );
}


Var *print_args( Var *print_statement )
{
	char *fmt = print_statement->name;
	char *cp = print_statement->name;
	char *ep;
	Var *cv;
	int in_format, on_stack, bs;

	/* start by counting the number of specifiers in the format string */

	in_format = 0;
	while ( ( cp = strchr( cp, '#' ) ) != NULL )
	{
		if ( cp == fmt || *( cp - 1 ) != '\\' )
			in_format++;
		cp++;
	}

	/* also count number of variables on stack following the print statement */

	cv = print_statement->next;
	on_stack = 0;
	while ( cv != NULL )
	{
		if ( cv->type == UNDEF_VAR )
		{

			eprint( FATAL, "%s:%ld: There's no variable named `%s'.\n",
				    Fname, Lc, cv->name );
			THROW( PRINT_SYNTAX_EXCEPTION );
		}

		if ( cv->type != INT_VAR && cv->type != FLOAT_VAR )
		{
			eprint( FATAL, "%s:%ld: There's no way to print a complete "
					"array.\n", Fname, Lc );
			THROW( PRINT_SYNTAX_EXCEPTION );
		}

		on_stack++;
		cv = cv->next;
	}

	/* check that both numbers are equal */

	if ( on_stack < in_format )
	{
		eprint( FATAL, "%s:%ld: Less data than format descriptors in print() "
				"format string.\n", Fname, Lc );
		THROW( PRINT_SYNTAX_EXCEPTION );
	}
		
	if ( on_stack > in_format )
	{
		eprint( FATAL, "%s:%ld: More data than format descriptors in print() "
				"format string.\n", Fname, Lc );
		THROW( PRINT_SYNTAX_EXCEPTION );
	}

	/* count number of backslashes */

	for ( cp = fmt, bs = 0; *cp != '\0' ; ++cp )
		if ( *cp == '\\' )
			bs++;

	/* get string long enough to replace each backslash by a 3-char sequence */

	fmt = get_string( strlen( print_statement->name ) + 2 * bs + 1 );
	strcpy( fmt, print_statement->name );

	for ( cp = fmt; *cp != '\0' ; ++cp )
	{
		/* skip normal characters */

		if ( *cp != '\\' && *cp != '#' )
			continue;

		/* convert format descriptor (#) to 3 \x01 */

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
	cv = print_statement->next;
	while ( ( ep = strstr( cp, "\x01\x01\x01" ) ) != NULL )
	{
		strcpy( ep, cv->type == INT_VAR ? "%d" : "%g" );
		if ( cv->type == INT_VAR )
			eprint( NO_ERROR, cp, cv->val.lval );
		else
			eprint( NO_ERROR, cp, cv->val.dval );

		cp = ep + 3;
		vars_pop( cv );
		cv = print_statement->next;
	}
	if ( *cp != '\0' )
		eprint( NO_ERROR, cp );

	free( fmt );
	vars_pop( print_statement );

	return( vars_push( INT_VAR, &in_format ) );
}
