/*
  $Id$
*/


#include "fsc2.h"


static void get_array_params( Var *v, const char *name, long *len,
							  long **ilp, double **idp );



/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

static void get_array_params( Var *v, const char *name, long *len,
							  long **ilp, double **idp )
{
	*ilp = NULL;
	*idp = NULL;

	switch ( v->type )
	{
		case INT_ARR :
			if ( v->dim != 1 )
			{
				eprint( FATAL, "%s:%ld: Argument of function `%s()' is "
						"neither a number nor a 1-dimensional array.\n",
						Fname, Lc, name );
				THROW( EXCEPTION );
			}
			*len = v->len;
			*ilp = v->val.lpnt;
			break;
			
		case FLOAT_ARR :
			if ( v->dim != 1 )
			{
				eprint( FATAL, "%s:%ld: Argument of function `%s()' is "
						"neither a number nor a 1-dimensional array.\n",
						Fname, Lc, name );
				THROW( EXCEPTION );
			}
			*len = v->len;
			*idp = v->val.dpnt;
			break;
			
		case ARR_PTR :
			*len = v->from->sizes[ v->from->dim - 1 ];
			if ( v->from->type == INT_ARR )
				*ilp = ( long * ) v->val.gptr;
			else
				*idp = ( double * ) v->val.gptr;
			break;

		case ARR_REF :
			if ( v->from->dim != 1 )
			{
				eprint( FATAL, "%s:%ld: Argument of function `%s()' is "
						"neither a number nor a 1-dimensional array.\n",
						Fname, Lc, name );
				THROW( EXCEPTION );
			}
			*len = v->from->sizes[ 0 ];
			if ( v->from->type == INT_ARR )
				*ilp = v->from->val.lpnt;
			else
				*idp = v->from->val.dpnt;
			break;

		case INT_TRANS_ARR :
			*len = v->len;
			*ilp = v->val.lpnt;
			break;

		case FLOAT_TRANS_ARR :
			*len = v->len;
			*idp = v->val.dpnt;
			break;

		default :
			assert( 1 == 0 );
	}

	assert( ( *ilp != NULL ) ^ ( *idp != NULL ) );
}


/*-------------------------------------------------*/
/* Conversion float to integer (result is integer) */
/*-------------------------------------------------*/

Var *f_int( Var *v )
{
	Var *new_var;
	long i;
	long len;
	long *rlp;
	long *ilp;
	double *idp;
	


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( INT_VAR, v->val.lval );

		case FLOAT_VAR :
			if ( v->val.dval > LONG_MAX || v->val.dval < LONG_MIN )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`int()'.\n", Fname, Lc );
			return vars_push( INT_VAR, ( long ) v->val.dval );

		default :
			get_array_params( v, "int", &len, &ilp, &idp );
	}

	if ( ilp != NULL )
		new_var = vars_push( INT_TRANS_ARR, ilp, len );
	else
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; idp++, i++ )
		{
			if ( *idp > LONG_MAX || *idp < LONG_MIN )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`int()'.\n", Fname, Lc );
			rlp[ i ] = ( long ) *idp;
		}

		new_var = vars_push( INT_TRANS_ARR, rlp, len );
	}

	new_var->flags |= v->flags & IS_DYNAMIC;
	T_free( rlp );

	return new_var;
}


/*----------------------------------------------------*/
/* Conversion int to floating point (result is float) */
/*----------------------------------------------------*/

Var *f_float( Var *v )
{
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( FLOAT_VAR, ( double ) v->val.lval );

		case FLOAT_VAR :
			return vars_push( FLOAT_VAR, v->val.dval );

		default :
			get_array_params( v, "float", &len, &ilp, &idp );
	}

	if ( idp != NULL )
		new_var = vars_push( FLOAT_TRANS_ARR, idp, len );
	else
	{
		rdp = T_malloc( len * sizeof( double ) );
		for ( i = 0; i < len; i++ )
			rdp[ i ] = ( double ) *ilp++;
		new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	}

	new_var->flags |= v->flags & IS_DYNAMIC;
	T_free( rdp );
	return new_var;
}


/*--------------------------------------------------------*/
/* Rounding of floating point numbers (result is integer) */
/*--------------------------------------------------------*/

Var *f_round( Var *v )
{
	Var *new_var;
	long i;
	long len;
	long *rlp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( INT_VAR, v->val.lval );

		case FLOAT_VAR :
			if ( v->val.dval >= LONG_MAX - 0.5 ||
				 v->val.dval <= LONG_MIN + 0.5 )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`round()'.\n", Fname, Lc );
			return vars_push( INT_VAR,   ( long ) ( 2 * v->val.dval )
							           - ( long ) v->val.dval );

		default :
			get_array_params( v, "round", &len, &ilp, &idp );
	}

	if ( ilp != NULL )
		new_var = vars_push( INT_TRANS_ARR, ilp, len );
	else
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; idp++, i++ )
		{
			if ( *idp >= LONG_MAX - 0.5 || *idp <= LONG_MIN + 0.5 )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`round()'.\n", Fname, Lc );
			rlp[ i ] = ( long ) ( 2 * *idp ) - ( long ) *idp;
		}

		new_var = vars_push( INT_TRANS_ARR, rlp, len );
	}

	new_var->flags |= v->flags & IS_DYNAMIC;
	T_free( rlp );
	return new_var;
}


/*---------------------------------*/
/* Floor value (result is integer) */
/*---------------------------------*/

Var *f_floor( Var *v )
{
	Var *new_var;
	long i;
	long len;
	long *rlp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( INT_VAR, v->val.lval );

		case FLOAT_VAR :
			if ( v->val.dval < LONG_MIN )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`floor()'.\n", Fname, Lc );
			return vars_push( INT_VAR, ( long ) floor( v->val.dval ) );

		default :
			get_array_params( v, "floor", &len, &ilp, &idp );
	}

	if ( ilp != NULL )
		new_var = vars_push( INT_TRANS_ARR, ilp, len );
	else
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; idp++, i++ )
		{
			if ( *idp < LONG_MIN )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`floor()'.\n", Fname, Lc );
			rlp[ i ] = ( long ) floor( *idp );
		}

		new_var = vars_push( INT_TRANS_ARR, rlp, len );
	}

	new_var->flags |= v->flags & IS_DYNAMIC;
	T_free( rlp );
	return new_var;
}


/*-----------------------------------*/
/* Ceiling value (result is integer) */
/*-----------------------------------*/

Var *f_ceil( Var *v )
{
	Var *new_var;
	long i;
	long len;
	long *rlp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( INT_VAR, v->val.lval );

		case FLOAT_VAR :
			if ( v->val.dval > LONG_MAX )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`ceil()'.\n", Fname, Lc );
			return vars_push( INT_VAR, ( long ) ceil( v->val.dval ) );

		default :
			get_array_params( v, "ceil", &len, &ilp, &idp );
	}

	if ( ilp != NULL )
		new_var = vars_push( INT_TRANS_ARR, ilp, len );
	else
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; idp++, i++ )
		{
			if ( *idp < LONG_MIN )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`ceil()'.\n", Fname, Lc );
			rlp[ i ] = ( long ) ceil( *idp );
		}

		new_var = vars_push( INT_TRANS_ARR, rlp, len );
	}

	new_var->flags |= v->flags & IS_DYNAMIC;
	T_free( rlp );

	return new_var;
}


/*-------------------------------------------------*/
/* abs of value (result has same as type argument) */
/*-------------------------------------------------*/

Var *f_abs( Var *v )
{
	Var *new_var;
	long i;
	long len;
	long *rlp;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			if ( v->val.lval == LONG_MIN )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`abs()'.\n", Fname, Lc );
			return vars_push( INT_VAR, labs( v->val.lval ) );

		case FLOAT_VAR :
			return vars_push( FLOAT_VAR, fabs( v->val.dval ) );

		default :
			get_array_params( v, "abs", &len, &ilp, &idp );
	}

	if ( ilp != NULL )
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; ilp++, i++ )
		{
			if ( *ilp == LONG_MIN )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`abs()'.\n", Fname, Lc );
			rlp[ i ] = labs( *ilp );
		}
		new_var = vars_push( INT_TRANS_ARR, rlp, len );
		T_free( rlp );
	}
	else
	{
		rdp = T_malloc( len * sizeof( double ) );
		for ( i = 0; i < len; idp++, i++ )
			rdp[ i ] = fabs( *idp );
		new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
		T_free( rdp );
	}

	new_var->flags |= v->flags & IS_DYNAMIC;
	return new_var;
}


/*-----------------------------------------------*/
/* sin of argument (in radian) (result is float) */
/*-----------------------------------------------*/

Var *f_sin( Var *v )
{
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_push( FLOAT_VAR, sin( VALUE( v ) ) );

	get_array_params( v, "sin", &len, &ilp, &idp );

	rdp = T_malloc( len * sizeof( double ) );
	if ( ilp != NULL )
		for ( i = 0; i < len; i++ )
			rdp[ i ] = sin( ( double ) *ilp++ );
	else
		for ( i = 0; i < len; i++ )
			rdp[ i ] = sin( *idp++ );

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*-----------------------------------------------*/
/* cos of argument (in radian) (result is float) */
/*-----------------------------------------------*/

Var *f_cos( Var *v )
{
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_push( FLOAT_VAR, cos( VALUE( v ) ) );

	get_array_params( v, "cos", &len, &ilp, &idp );

	rdp = T_malloc( len * sizeof( double ) );
	if ( ilp != NULL )
		for ( i = 0; i < len; i++ )
			rdp[ i ] = cos( ( double ) *ilp++ );
	else
		for ( i = 0; i < len; i++ )
			rdp[ i ] = cos( *idp++ );

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*-----------------------------------------------*/
/* tan of argument (in radian) (result is float) */
/*-----------------------------------------------*/

Var *f_tan( Var *v )
{
	double res;
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		res = tan( VALUE( v ) );
		if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `tan()'.\n",
					Fname, Lc );
		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, "tan", &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		rdp[ i ] = tan( is_int ? ( double ) *ilp++ : *idp++ );
		if ( fabs( rdp[ i ] ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `tan()'.\n",
					Fname, Lc );
	}

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*--------------------------------------------------------------------*/
/* asin (in radian) of argument (with -1 <= x <= 1) (result is float) */
/*--------------------------------------------------------------------*/

Var *f_asin( Var *v )
{
	double arg;
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( fabs( arg ) > 1.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `asin()' is out "
					"of range.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, asin( arg ) );
	}

	get_array_params( v, "asin", &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;
		if ( fabs( arg ) > 1.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `asin()' is out "
					"of range.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		rdp[ i ] = asin( arg );
	}

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*--------------------------------------------------------------------*/
/* acos (in radian) of argument (with -1 <= x <= 1) (result is float) */
/*--------------------------------------------------------------------*/

Var *f_acos( Var *v )
{
	double arg;
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( fabs( arg ) > 1.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `acos()' is out "
					"of range.\n",  Fname, Lc );
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, acos( arg ) );
	}

	get_array_params( v, "acos", &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;
		if ( fabs( arg ) > 1.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `acos()' is out "
					"of range.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		rdp[ i ] = acos( arg );
	}

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*------------------------------------------------*/
/* atan (in radian) of argument (result is float) */
/*------------------------------------------------*/

Var *f_atan( Var *v )
{
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_push( FLOAT_VAR, atan( VALUE( v ) ) );

	get_array_params( v, "atan", &len, &ilp, &idp );

	rdp = T_malloc( len * sizeof( double ) );
	if ( ilp != NULL )
		for ( i = 0; i < len; i++ )
			rdp[ i ] = atan( ( double ) *ilp++ );
	else
		for ( i = 0; i < len; i++ )
			rdp[ i ] = atan( *idp++ );

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*------------------------------------*/
/* sinh of argument (result is float) */
/*------------------------------------*/

Var *f_sinh( Var *v )
{
	double res;
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		res = sinh( VALUE ( v ) );
		if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `sinh()'.\n",
					Fname, Lc );
		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, "sinh", &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		res = sinh( is_int ? ( double ) *ilp++ : *idp++ );
		if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `sinh()'.\n",
					Fname, Lc );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*------------------------------------*/
/* cosh of argument (result is float) */
/*------------------------------------*/

Var *f_cosh( Var *v )
{
	double res;
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		res = cosh( VALUE( v ) );
		if ( res == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `cosh()'.\n",
					Fname, Lc );
		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, "cosh", &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		res = cosh( is_int ? ( double ) *ilp++ : *idp++ );
		if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `cosh()'.\n",
					Fname, Lc );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*------------------------------------*/
/* tanh of argument (result is float) */
/*------------------------------------*/

Var *f_tanh( Var *v )
{
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_push( FLOAT_VAR, tanh( VALUE( v ) ) );

	get_array_params( v, "tanh", &len, &ilp, &idp );

	rdp = T_malloc( len * sizeof( double ) );
	if ( ilp != NULL )
		for ( i = 0; i < len; i++ )
			rdp[ i ] = tanh( ( double ) *ilp++ );
	else
		for ( i = 0; i < len; i++ )
			rdp[ i ] = tanh( *idp++ );

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*-----------------------------------*/
/* exp of argument (result is float) */
/*-----------------------------------*/

Var *f_exp( Var *v )
{
	Var *new_var;
	double res;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		res = exp( VALUE( v ) );
		if ( res == 0.0 && errno == ERANGE )
			eprint( WARN, "%s:%ld: Underflow in function `exp()' - result "
					"is 0.\n", Fname, Lc );
		if ( res == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `exp()'.\n",
					Fname, Lc );
		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, "exp", &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		res = tanh( is_int ? ( double ) *ilp++ : *idp++ );

		if ( res == 0.0 && errno == ERANGE )
			eprint( WARN, "%s:%ld: Underflow in function `exp()' - result "
					"is 0.\n", Fname, Lc );
		if ( res == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `exp()'.\n",
					Fname, Lc );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*-----------------------------------------------*/
/* ln of argument (with x > 0) (result is float) */
/*-----------------------------------------------*/

Var *f_ln( Var *v )
{
	double arg, res;
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( arg <= 0.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `ln()' is out of "
					"range.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		res = log( arg );
		if ( res == - HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `ln()'.\n",
					Fname, Lc );

		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, "ln", &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;

		if ( arg <= 0.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `ln()' is out of "
					"range.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		res = log( arg );
		if ( res == - HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `ln()'.\n",
					Fname, Lc );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*------------------------------------------------*/
/* log of argument (with x > 0) (result is float) */
/*------------------------------------------------*/

Var *f_log( Var *v )
{
	double arg, res;
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( arg <= 0.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `log()' is out "
					"of range.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		res = log10( arg );
		if ( res == - HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `log()'.\n",
					Fname, Lc );

		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, "log", &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;

		if ( arg <= 0.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `log()' is out "
					"of range.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		res = log10( arg );
		if ( res == - HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, "%s:%ld: Overflow in function `log()'.\n",
					Fname, Lc );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*--------------------------------------------------*/
/* sqrt of argument (with x >= 0) (result is float) */
/*--------------------------------------------------*/

Var *f_sqrt( Var *v )
{
	double arg;
	Var *new_var;
	long i;
	long len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( arg < 0.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `sqrt()' is "
					"negative.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, sqrt( arg ) );
	}

	get_array_params( v, "sqrt", &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;

		if ( arg < 0.0 )
		{
			eprint( FATAL, "%s:%ld: Argument of function `sqrt()' is "
					"negative.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		rdp[ i ] = sqrt( arg );
	}

	new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*----------------------------------------------------------------*/
/* Returns a random number between 0 and 1 (i.e. result is float) */
/*----------------------------------------------------------------*/


Var *f_random( Var *v )
{
	v = v;

	return vars_push( FLOAT_VAR, ( double ) random( ) / ( double ) RAND_MAX );
}


/*---------------------------------------------*/
/* Sets a seed for the random number generator */
/*---------------------------------------------*/


Var *f_setseed( Var *v )
{
	unsigned int arg;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	{
		if ( v->val.lval < 0 )
			eprint( SEVERE, "%s:%ld: set_seed() needs a positive integer "
				"as argument, using absolute value.\n", Fname, Lc );
		arg = ( unsigned int ) labs( v->val.lval );
	}
	else
	{
		eprint( SEVERE, "%s:%ld: set_seed() needs a positive integer and not "
				"a float variable as argument, using 1 instead.\n",
				Fname, Lc );
		arg = 1;
	}

	srandom( arg );
	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------------*/
/* Returns a string with the current time in the form hh:mm:ss. */
/*--------------------------------------------------------------*/

Var *f_time( Var *v )
{
	time_t tp;
	char ts[ 100 ];


	v = v;
	time( &tp );
	if ( strftime( ts, 100, "%H:%M:%S", localtime( &tp ) ) == 0 )
	{
		eprint( SEVERE, "%s:%ld: time() returns invalid time string.\n",
				Fname, Lc );
		strcat( ts, "(Unknown time)" );
	}

	return vars_push( STR_VAR, ts );
}


/*----------------------------------------------------------*/
/* Returns a floating point value with the time difference  */
/* since the last call of the function (in us resolution).  */
/* The function should be called automatically at the start */
/* of an experiment so that the user gets the time since    */
/* the start of the experiment when she calls this function */
/* for the very first time.                                 */
/*----------------------------------------------------------*/

Var *f_dtime( Var *v )
{
	struct timeval t_new;
	static struct timeval t_old = { 0, 0 };
	long dsec, dusec;


	v = v;                          /* keep the compiler happy */

	gettimeofday( &t_new, NULL );

	dsec = t_new.tv_sec - t_old.tv_sec;
	dusec = t_new.tv_usec - t_old.tv_usec;

	t_old.tv_sec = t_new.tv_sec;
	t_old.tv_usec = t_new.tv_usec;

	return vars_push( FLOAT_VAR, ( double ) dsec + 1.e-6 * ( double ) dusec );
}


/*--------------------------------------------------------------------------*/
/* Returns a string with the current date in a form like "Sat Jun 17, 2000" */
/*--------------------------------------------------------------------------*/

Var *f_date( Var *v )
{
	time_t tp;
	char ts[ 100 ];


	v = v;
	time( &tp );
	if ( strftime( ts, 100, "%a %b %d, %Y", localtime( &tp ) ) == 0 )
	{
		eprint( SEVERE, "%s:%ld: date() returns invalid date string.\n",
				Fname, Lc );
		strcat( ts, "(Unknown date)" );
	}

	return vars_push( STR_VAR, ts );
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


/*----------------------------------------------------------------------*/
/* Calculates the mean of the elements of an one dimensional array.     */
/* If only an array (or a pointer to an array is passed to the function */
/* the mean of all array elements is calculated. If there's a second    */
/* argument it's taken to be an index into the array at which the       */
/* calculation of the maen starts. If there's a third argument it has   */
/* be the number of elements to be included into the mean.              */
/*----------------------------------------------------------------------*/

Var *f_mean( Var *v )
{
	long i;
	long len;
	long *ilp = NULL;
	double *idp = NULL;
	double val = 0.0;
	long index;
	long slice_len;


	if ( v == NULL || v->next == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameter in call of function "
				"`mean()'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_ARR | FLOAT_ARR | ARR_REF | ARR_PTR |
				   INT_TRANS_ARR | FLOAT_TRANS_ARR );

	get_array_params( v, "mean", &len, &ilp, &idp );
	slice_len = len;

	/* The following optional parameter are the start index into to the array
	   and the length of the subarray to be used for the calculation */

	if ( v->next != NULL )
	{
		vars_check( v->next, INT_VAR | FLOAT_VAR );

		if ( v->next->type == FLOAT_VAR )
		{
			eprint( WARN, "%s:%ld: Float value used as array index in "
					"function `mean()'.\n", Fname, Lc );
			index = lround( v->next->val.dval ) - ARRAY_OFFSET;
		}
		else
			index = v->next->val.lval - ARRAY_OFFSET;

		if ( index < 0 )
		{
			eprint( FATAL, "%s:%ld: Invalid array index (%ld) in function "
					"`mean()'.\n", Fname, Lc, index + ARRAY_OFFSET );
			THROW( EXCEPTION );
		}

		if ( ilp != NULL )
			ilp += index;
		else
			idp += index;

		if ( v->next->next != NULL )
		{
			vars_check( v->next->next, INT_VAR | FLOAT_VAR );

			if ( v->next->type == FLOAT_VAR )
			{
				eprint( WARN, "%s:%ld: Float value used as length of slice "
						"parameter in function `mean()'.\n", Fname, Lc );
				slice_len = lround( v->next->next->val.dval );
			}
			else
				slice_len = v->next->next->val.lval;

			if ( slice_len < 1 )
			{
				eprint( FATAL, "%s:%ld; Zero or negative slice length used in "
						"function `mean()'.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			/* Test that the slice is within the arrays range */

			if ( index + slice_len > len &&
				 ! ( TEST_RUN && ( v->flags & IS_DYNAMIC ) ) )
			{
				eprint( FATAL, "%s:%ld: Sum of index and slice length "
						"parameter exceeds length of array in function "
						"`mean()'.\n", Fname, Lc );
				THROW( EXCEPTION );
			}
		}
		else
			slice_len = len - index - 1;
	}

	for ( i = 0; i < slice_len; i++ )
		if ( ilp != NULL )
			val += ( double ) *ilp++;
		else
			val += *idp++;

	return vars_push( FLOAT_VAR, val / ( double ) slice_len );
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

Var *f_rms( Var *v )
{
	long i;
	long len;
	long *ilp;
	double *idp;
	double val = 0.0;


	vars_check( v, INT_ARR | FLOAT_ARR | ARR_REF | ARR_PTR |
				   INT_TRANS_ARR | FLOAT_TRANS_ARR );

	get_array_params( v, "rms", &len, &ilp, &idp );

	for ( i = 0; i < len; i++ )
		if ( ilp != NULL )
		{
			val += ( double ) *ilp * ( double ) *ilp;
			ilp++;
		}
		else
		{
			val += *idp * *idp;
			idp++;
		}

	return vars_push( FLOAT_VAR, sqrt( val ) / ( double ) len );
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

Var *f_slice( Var *v )
{
	long len;
	long *ilp;
	double *idp;
	long index;
	long slice_len;
	Var *new_var;
	double *ndp;
	long *nlp;


	if ( v == NULL || v->next == NULL )
	{
		eprint( FATAL, "%s:%ld: Not enough parameter in call of function "
				"`slice'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_ARR | FLOAT_ARR | ARR_REF | ARR_PTR |
				   INT_TRANS_ARR | FLOAT_TRANS_ARR );

	get_array_params( v, "slice", &len, &ilp, &idp );

	vars_check( v->next, INT_VAR | FLOAT_VAR );

	if ( v->next->type == FLOAT_VAR )
	{
		eprint( WARN, "%s:%ld: Float value used as array index in function "
				"`slice'.\n", Fname, Lc );
		index = lround( v->next->val.dval ) - ARRAY_OFFSET;
	}
	else
		index = v->next->val.lval - ARRAY_OFFSET;

	if ( index < 0 )
	{
		eprint( FATAL, "%s:%ld: Negative array index used in function "
				"`slice'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v->next->next != NULL )
	{
		vars_check( v->next->next, INT_VAR | FLOAT_VAR );

		if ( v->next->type == FLOAT_VAR )
		{
			eprint( WARN, "%s:%ld: Float value used as length of slice "
					"parameter in function `slice'.\n", Fname, Lc );
			slice_len = lround( v->next->next->val.dval );
		}
		else
			slice_len = v->next->next->val.lval;

		if ( slice_len < 1 )
		{
			eprint( FATAL, "%s:%ld; Zero or negative slice length used in "
					"function `slice'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}
	else
		slice_len = len - index - 1;

	/* Test that the slice is within the arrays range */

	if ( index + slice_len > len &&
		 ! ( TEST_RUN && ( v->flags & IS_DYNAMIC ) ) )
	{
		eprint( FATAL, "%s:%ld: Sum of index and slice length parameter "
				"exceeds length of array in function slice.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( index + slice_len >= len )
	{
		if ( ilp != NULL )
		{
			nlp = T_calloc( slice_len, sizeof( long ) );
			memcpy( nlp, ilp + index, ( len - index ) * sizeof( long ) );
			new_var = vars_push( INT_TRANS_ARR, nlp, slice_len );
			T_free( nlp );
		}
		else
		{
			ndp = T_calloc( slice_len, sizeof( double ) );
			memcpy( nlp, idp + index, ( len - index ) * sizeof( double ) );
			new_var = vars_push( FLOAT_TRANS_ARR, ndp, slice_len );
			T_free( ndp );
		}

		return new_var;
	}

	if ( ilp != NULL )
		return vars_push( INT_TRANS_ARR, ilp + index, slice_len );
	else
		return vars_push( FLOAT_TRANS_ARR, idp + index, slice_len );
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/
				 
Var *f_square( Var *v )
{
	Var *new_var;
	long i;
	long len;
	long *rlp;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			if ( ( double ) v->val.lval >= sqrt( LONG_MAX ) )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`square()'.\n", Fname, Lc );
			return vars_push( INT_VAR, v->val.lval * v->val.lval );

		case FLOAT_VAR :
			return vars_push( FLOAT_VAR, v->val.dval * v->val.dval );

		default :
			get_array_params( v, "square", &len, &ilp, &idp );
	}

	if ( ilp != NULL )
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; ilp++, i++ )
		{
			if ( ( double ) *ilp >= sqrt( LONG_MIN ) )
				eprint( SEVERE, "%s:%ld: Integer overflow in function "
						"`sqrt()'.\n", Fname, Lc );
			rlp[ i ] = *ilp * *ilp;
		}
		new_var = vars_push( INT_TRANS_ARR, rlp, len );
		T_free( rlp );
	}
	else
	{
		rdp = T_malloc( len * sizeof( double ) );
		for ( i = 0; i < len; idp++, i++ )
			rdp[ i ] = *idp * *idp;
		new_var = vars_push( FLOAT_TRANS_ARR, rdp, len );
		T_free( rdp );
	}

	new_var->flags |= v->flags & IS_DYNAMIC;
	return new_var;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var *f_islice( Var *v )
{
	long *array;
	long size;
	Var *ret;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		size = v->val.lval;
	else
	{
		eprint( SEVERE, "%s:%ld: Float value used as array size in function "
				"`int_slice()'.\n", Fname, Lc );
		size = lround( v->val.dval );
	}

	array = T_calloc( size, sizeof( long ) );
	ret = vars_push( INT_TRANS_ARR, array, size );
	T_free( array );

	return ret;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var *f_fslice( Var *v )
{
	double *array;
	long size;
	Var *ret;
	long i;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		size = v->val.lval;
	else
	{
		eprint( SEVERE, "%s:%ld: Float value used as array size in function "
				"`float_slice()'.\n", Fname, Lc );
		size = lround( v->val.dval );
	}

	array = T_malloc( size * sizeof( double ) );
	for( i = 0; i < size; i++ )
		*( array + i ) = 0.0;
	ret = vars_push( FLOAT_TRANS_ARR, array, size );
	T_free( array );

	return ret;
}
