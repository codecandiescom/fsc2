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


static void get_array_params( Var *v, size_t *len, long **ilp, double **idp );
static double gauss_random( void );

static bool grand_is_old = UNSET;



/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

Var *f_abort( Var *v )
{
	char *str;


	v = v;                       /* keeps the compiler happy */

	eprint( NO_ERROR, SET, "Exit due to call of %s().\n", Cur_Func );

	if ( ! TEST_RUN )
	{
		str = get_string( "Exit due to call of abort() in\n"
						  "%s at line %ld.", Fname, Lc );
		show_message( str );
		T_free( str );
		THROW( ABORT_EXCEPTION );
	}

	return NULL;
}


/*-------------------------------------------*/
/* This is called for the end() EDL function */
/*-------------------------------------------*/

Var *f_stopsim( Var *v )
{
	v = v;
	do_quit = SET;
	return NULL;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

static void get_array_params( Var *v, size_t *len, long **ilp, double **idp )
{
	*ilp = NULL;
	*idp = NULL;

	switch ( v->type )
	{
		case INT_CONT_ARR :
			if ( v->dim != 1 )
			{
				eprint( FATAL, SET, "Argument of function %s() is neither a "
						"number nor an 1-dimensional array.\n", Cur_Func );
				THROW( EXCEPTION );
			}
			*len = v->len;
			*ilp = v->val.lpnt;
			break;
			
		case FLOAT_CONT_ARR :
			if ( v->dim != 1 )
			{
				eprint( FATAL, SET, "Argument of function %s() is neither a "
						"number nor an 1-dimensional array.\n", Cur_Func );
				THROW( EXCEPTION );
			}
			*len = v->len;
			*idp = v->val.dpnt;
			break;
			
		case ARR_PTR :
			*len = v->from->sizes[ v->from->dim - 1 ];
			if ( v->from->type == INT_CONT_ARR )
				*ilp = ( long * ) v->val.gptr;
			else
				*idp = ( double * ) v->val.gptr;
			break;

		case ARR_REF :
			if ( v->from->dim != 1 )
			{
				eprint( FATAL, SET, "Argument of function %s() is neither a "
						"number nor an 1-dimensional array.\n", Cur_Func );
				THROW( EXCEPTION );
			}

			if ( v->from->flags & NEED_ALLOC )
			{
				eprint( FATAL, SET, "Argument of function %s() is a "
						"dynamically sized array of still unknown size.\n",
						Cur_Func );
				THROW( EXCEPTION );
			}

			*len = v->from->sizes[ 0 ];
			if ( v->from->type == INT_CONT_ARR )
				*ilp = v->from->val.lpnt;
			else
				*idp = v->from->val.dpnt;
			break;

		case INT_ARR :
			*len = v->len;
			*ilp = v->val.lpnt;
			break;

		case FLOAT_ARR :
			*len = v->len;
			*idp = v->val.dpnt;
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	fsc2_assert( ( *ilp != NULL ) ^ ( *idp != NULL ) );
}


/*-------------------------------------------------*/
/* Conversion float to integer (result is integer) */
/*-------------------------------------------------*/

Var *f_int( Var *v )
{
	Var *new_var;
	size_t i;
	size_t len;
	long *rlp = NULL;
	long *ilp;
	double *idp;
	


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( INT_VAR, v->val.lval );

		case FLOAT_VAR :
			if ( v->val.dval > LONG_MAX || v->val.dval < LONG_MIN )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			return vars_push( INT_VAR, ( long ) v->val.dval );

		default :
			get_array_params( v, &len, &ilp, &idp );
	}

	if ( ilp != NULL )
		new_var = vars_push( INT_ARR, ilp, len );
	else
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; idp++, i++ )
		{
			if ( *idp > LONG_MAX || *idp < LONG_MIN )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			rlp[ i ] = ( long ) *idp;
		}

		new_var = vars_push( INT_ARR, rlp, len );
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
	size_t i;
	size_t len;
	double *rdp = NULL;
	long *ilp;
	double *idp;
	


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( FLOAT_VAR, ( double ) v->val.lval );

		case FLOAT_VAR :
			return vars_push( FLOAT_VAR, v->val.dval );

		default :
			get_array_params( v, &len, &ilp, &idp );
	}

	if ( idp != NULL )
		new_var = vars_push( FLOAT_ARR, idp, len );
	else
	{
		rdp = T_malloc( len * sizeof( double ) );
		for ( i = 0; i < len; i++ )
			rdp[ i ] = ( double ) *ilp++;
		new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	long *rlp = NULL;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( INT_VAR, v->val.lval );

		case FLOAT_VAR :
			if ( v->val.dval >= LONG_MAX - 0.5 ||
				 v->val.dval <= LONG_MIN + 0.5 )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			return vars_push( INT_VAR,   ( long ) ( 2 * v->val.dval )
							           - ( long ) v->val.dval );

		default :
			get_array_params( v, &len, &ilp, &idp );
	}

	if ( ilp != NULL )
		new_var = vars_push( INT_ARR, ilp, len );
	else
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; idp++, i++ )
		{
			if ( *idp >= LONG_MAX - 0.5 || *idp <= LONG_MIN + 0.5 )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			rlp[ i ] = ( long ) ( 2 * *idp ) - ( long ) *idp;
		}

		new_var = vars_push( INT_ARR, rlp, len );
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
	size_t i;
	size_t len;
	long *rlp = NULL;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( INT_VAR, v->val.lval );

		case FLOAT_VAR :
			if ( v->val.dval < LONG_MIN )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			return vars_push( INT_VAR, lrnd( floor( v->val.dval )  ) );

		default :
			get_array_params( v, &len, &ilp, &idp );
	}

	if ( ilp != NULL )
		new_var = vars_push( INT_ARR, ilp, len );
	else
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; idp++, i++ )
		{
			if ( *idp < LONG_MIN )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			rlp[ i ] = lrnd( floor( *idp ) );
		}

		new_var = vars_push( INT_ARR, rlp, len );
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
	size_t i;
	size_t len;
	long *rlp = NULL;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( INT_VAR, v->val.lval );

		case FLOAT_VAR :
			if ( v->val.dval > LONG_MAX )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			return vars_push( INT_VAR, lrnd( ceil( v->val.dval ) ) );

		default :
			get_array_params( v, &len, &ilp, &idp );
	}

	if ( ilp != NULL )
		new_var = vars_push( INT_ARR, ilp, len );
	else
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; idp++, i++ )
		{
			if ( *idp < LONG_MIN )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			rlp[ i ] = lrnd( ceil( *idp ) );
		}

		new_var = vars_push( INT_ARR, rlp, len );
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
	size_t i;
	size_t len;
	long *rlp;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			if ( v->val.lval == LONG_MIN )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			return vars_push( INT_VAR, labs( v->val.lval ) );

		case FLOAT_VAR :
			return vars_push( FLOAT_VAR, fabs( v->val.dval ) );

		default :
			get_array_params( v, &len, &ilp, &idp );
	}

	if ( ilp != NULL )
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; ilp++, i++ )
		{
			if ( *ilp == LONG_MIN )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			rlp[ i ] = labs( *ilp );
		}
		new_var = vars_push( INT_ARR, rlp, len );
		T_free( rlp );
	}
	else
	{
		rdp = T_malloc( len * sizeof( double ) );
		for ( i = 0; i < len; idp++, i++ )
			rdp[ i ] = fabs( *idp );
		new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_push( FLOAT_VAR, sin( VALUE( v ) ) );

	get_array_params( v, &len, &ilp, &idp );

	rdp = T_malloc( len * sizeof( double ) );
	if ( ilp != NULL )
		for ( i = 0; i < len; i++ )
			rdp[ i ] = sin( ( double ) *ilp++ );
	else
		for ( i = 0; i < len; i++ )
			rdp[ i ] = sin( *idp++ );

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_push( FLOAT_VAR, cos( VALUE( v ) ) );

	get_array_params( v, &len, &ilp, &idp );

	rdp = T_malloc( len * sizeof( double ) );
	if ( ilp != NULL )
		for ( i = 0; i < len; i++ )
			rdp[ i ] = cos( ( double ) *ilp++ );
	else
		for ( i = 0; i < len; i++ )
			rdp[ i ] = cos( *idp++ );

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		res = tan( VALUE( v ) );
		if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n",
					Cur_Func );
		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		rdp[ i ] = tan( is_int ? ( double ) *ilp++ : *idp++ );
		if ( fabs( rdp[ i ] ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n",
					Cur_Func );
	}

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( fabs( arg ) > 1.0 )
		{
			eprint( FATAL, SET, "Argument of function %s() is out "
					"of range.\n", Cur_Func );
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, asin( arg ) );
	}

	get_array_params( v, &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;
		if ( fabs( arg ) > 1.0 )
		{
			eprint( FATAL, SET, "Argument of function %s() is out of range.\n",
					Cur_Func );
			THROW( EXCEPTION );
		}

		rdp[ i ] = asin( arg );
	}

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( fabs( arg ) > 1.0 )
		{
			eprint( FATAL, SET, "Argument of function %s() is out of range.\n",
					Cur_Func );
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, acos( arg ) );
	}

	get_array_params( v, &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;
		if ( fabs( arg ) > 1.0 )
		{
			eprint( FATAL, SET, "Argument of function %s() is out of range.\n",
					Cur_Func );
			THROW( EXCEPTION );
		}

		rdp[ i ] = acos( arg );
	}

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_push( FLOAT_VAR, atan( VALUE( v ) ) );

	get_array_params( v, &len, &ilp, &idp );

	rdp = T_malloc( len * sizeof( double ) );
	if ( ilp != NULL )
		for ( i = 0; i < len; i++ )
			rdp[ i ] = atan( ( double ) *ilp++ );
	else
		for ( i = 0; i < len; i++ )
			rdp[ i ] = atan( *idp++ );

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		res = sinh( VALUE ( v ) );
		if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );
		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		res = sinh( is_int ? ( double ) *ilp++ : *idp++ );
		if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		res = cosh( VALUE( v ) );
		if ( res == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );
		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		res = cosh( is_int ? ( double ) *ilp++ : *idp++ );
		if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_push( FLOAT_VAR, tanh( VALUE( v ) ) );

	get_array_params( v, &len, &ilp, &idp );

	rdp = T_malloc( len * sizeof( double ) );
	if ( ilp != NULL )
		for ( i = 0; i < len; i++ )
			rdp[ i ] = tanh( ( double ) *ilp++ );
	else
		for ( i = 0; i < len; i++ )
			rdp[ i ] = tanh( *idp++ );

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		res = exp( VALUE( v ) );
		if ( res == 0.0 && errno == ERANGE )
			eprint( WARN, SET, "Underflow in function %s() - result is 0.\n",
					Cur_Func );
		if ( res == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );
		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		res = tanh( is_int ? ( double ) *ilp++ : *idp++ );

		if ( res == 0.0 && errno == ERANGE )
			eprint( WARN, SET, "Underflow in function %s() - result is 0.\n",
					Cur_Func );
		if ( res == HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( arg <= 0.0 )
		{
			eprint( FATAL, SET, "Argument of %s() is out of range.\n",
					Cur_Func );
			THROW( EXCEPTION );
		}

		res = log( arg );
		if ( res == - HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );

		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;

		if ( arg <= 0.0 )
		{
			eprint( FATAL, SET, "Argument of %s() is out of range.\n",
					Cur_Func );
			THROW( EXCEPTION );
		}

		res = log( arg );
		if ( res == - HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( arg <= 0.0 )
		{
			eprint( FATAL, SET, "Argument of %s() is out of range.\n",
					Cur_Func );
			THROW( EXCEPTION );
		}

		res = log10( arg );
		if ( res == - HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );

		return vars_push( FLOAT_VAR, res );
	}

	get_array_params( v, &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;

		if ( arg <= 0.0 )
		{
			eprint( FATAL, SET, "Argument of %s() is out of range.\n",
					Cur_Func );
			THROW( EXCEPTION );
		}

		res = log10( arg );
		if ( res == - HUGE_VAL && errno == ERANGE )
			eprint( SEVERE, SET, "Overflow in function %s().\n", Cur_Func );

		rdp[ i ] = res;
	}

	new_var = vars_push( FLOAT_ARR, rdp, len );
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
	size_t i;
	size_t len;
	double *rdp;
	long *ilp;
	double *idp;
	bool is_int;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		arg = VALUE( v );
		if ( arg < 0.0 )
		{
			eprint( FATAL, SET, "Argument of function %s() is negative.\n",
					Cur_Func );
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, sqrt( arg ) );
	}

	get_array_params( v, &len, &ilp, &idp );

	is_int = ilp != NULL ? SET : UNSET;
	rdp = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
	{
		arg = is_int ? ( double ) *ilp++ : *idp++;

		if ( arg < 0.0 )
		{
			eprint( FATAL, SET, "Argument of function %s() is negative.\n",
					Cur_Func );
			THROW( EXCEPTION );
		}

		rdp[ i ] = sqrt( arg );
	}

	new_var = vars_push( FLOAT_ARR, rdp, len );
	new_var->flags |= v->flags & IS_DYNAMIC;

	T_free( rdp );

	return new_var;
}


/*----------------------------------------------------------------*/
/* Returns a random number between 0 and 1 (i.e. result is float) */
/*----------------------------------------------------------------*/


Var *f_random( Var *v )
{
	long len;
	long i;
	double *arr;
	Var *new_var;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, random( ) / ( double ) RAND_MAX );

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "Float number used as number of points in function "
				"%s().\n", Cur_Func );
	    len = lrnd( v->val.dval );
	}
	else
		len = v->val.lval;

	if ( len <= 0 )
	{
		eprint( FATAL, SET, "Zero or negative number (%ld) of points "
				 "specified in function %s().\n", len, Cur_Func );
		THROW( EXCEPTION );
	}

	arr = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
		*( arr + i ) = random( ) / ( double ) RAND_MAX;

	new_var = vars_push( FLOAT_ARR, arr, len );
	T_free( arr );

	return new_var;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *f_grand( Var *v )
{
	long len;
	long i;
	double *arr;
	Var *new_var;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, gauss_random( ) );
	
	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "Float number used as number of points in function "
				"%s().\n", Cur_Func );
	    len = lrnd( v->val.dval );
	}
	else
		len = v->val.lval;

	if ( len <= 0 )
	{
		eprint( FATAL, SET, "Zero or negative number (%ld) of points "
				"specified in function %s().\n", len, Cur_Func );
		THROW( EXCEPTION );
	}

	arr = T_malloc( len * sizeof( double ) );
	for ( i = 0; i < len; i++ )
		*( arr + i ) = gauss_random( );

	new_var = vars_push( FLOAT_ARR, arr, len );
	T_free( arr );

	return new_var;
}


/*----------------------------------------------------------------------*/
/* Returns random numbers with Gaussian distribution, zero mean value   */
/* and variance of 1. See W. H. Press, S. A. Teukolsky, W, T. Vettering */
/* and B. P. Flannery, "Numerical Recipes in C", 2nd ed., Cambridge     */
/* University Press 1992, pp. 288-290, for all the gory details.        */
/*----------------------------------------------------------------------*/


static double gauss_random( void )
{
	static double next_val;
	double factor, radius, val_1, val_2;


	if ( ! grand_is_old )
	{
		do {
			val_1 = 2.0 * random( ) / ( double ) RAND_MAX - 1.0;
			val_2 = 2.0 * random( ) / ( double ) RAND_MAX - 1.0;
			radius = val_1 * val_1 + val_2 * val_2;
		} while ( radius < 0.0 || radius >= 1.0 );

		factor = sqrt( - 2.0 * log( radius ) / radius );
		next_val = val_1 * factor;
		grand_is_old = SET;
		return val_2 * factor;
	}

	grand_is_old = UNSET;
	return next_val;
}


/*---------------------------------------------------------------------------*/
/* Sets a seed for the random number generator. It expects either a positive */
/* integer as argument or none, in which case the current time (in seconds   */
/* since 00:00:00 UTC, January 1, 1970) is used.                             */
/*---------------------------------------------------------------------------*/

Var *f_setseed( Var *v )
{
	unsigned int arg;


	if ( v != NULL )
	{
		vars_check( v, INT_VAR | FLOAT_VAR );

		if ( v->type == INT_VAR )
		{
			if ( v->val.lval < 0 )
				eprint( SEVERE, SET, "%s() needs a positive integer as "
						"argument, using absolute value.\n", Cur_Func );
			arg = ( unsigned int ) labs( v->val.lval );
		}
		else
		{
			eprint( SEVERE, SET, "%s() needs a positive integer and "
					"not a float variable as argument, using 1 instead.\n",
					Cur_Func );
			arg = 1;
		}

		if ( arg > RAND_MAX )
		{
			eprint( SEVERE, SET, "Seed for random generator too large in %s(),"
					" maximum is %ld. Using 1 instead\n", Cur_Func, RAND_MAX );
			arg = 1;
		}

		if ( v->next != NULL )
			eprint( WARN, SET, "Superfluous argument in call of %s().\n",
					Cur_Func );
	}
	else
	{
		arg = ( unsigned int ) time( NULL );
		while ( arg > RAND_MAX )
			arg >>= 1;
	}

	grand_is_old = UNSET;
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
		eprint( SEVERE, SET, "%s() returns invalid time string.\n",
				Cur_Func );
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
		eprint( SEVERE, SET, "%s() returns invalid date string.\n",
				Cur_Func );
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

	if ( v->next == NULL && v->from->dim == 1 )
		return vars_push( INT_VAR, ( long ) v->from->len );

	vars_check( v->next, INT_VAR | FLOAT_VAR );

	if ( v->next->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "WARNING: Float value used as index for array "
				"`%s' in function %s.\n", v->from->name, Cur_Func );
		size = ( int ) v->next->val.dval - ARRAY_OFFSET;
	}
	else
		size = ( int ) v->next->val.lval - ARRAY_OFFSET;

	if ( size >= v->from->dim )
	{
		eprint( FATAL, SET, "Array `%s' has only %d dimensions, can't return "
				"size of %d. dimension.\n",
				v->from->name, v->from->dim, size );
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
	return vars_push( INT_ARR, v->from->sizes, ( long ) v->from->dim );
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
	size_t i;
	size_t len;
	long *ilp = NULL;
	double *idp = NULL;
	double val = 0.0;
	long a_index;
	size_t slice_len;


	if ( v == NULL )
	{
		eprint( FATAL, SET, "Missing parameter in call of function %s().\n",
				Cur_Func );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_CONT_ARR | FLOAT_CONT_ARR | ARR_REF | ARR_PTR |
				   INT_ARR | FLOAT_ARR );

	get_array_params( v, &len, &ilp, &idp );
	slice_len = len;

	/* The following optional parameter are the start index into to the array
	   and the length of the subarray to be used for the calculation */

	if ( v->next != NULL )
	{
		vars_check( v->next, INT_VAR | FLOAT_VAR );

		if ( v->next->type == FLOAT_VAR )
		{
			eprint( WARN, SET, "Float value used as array index in "
					"function %s().\n", Cur_Func );
			a_index = lrnd( v->next->val.dval ) - ARRAY_OFFSET;
		}
		else
			a_index = v->next->val.lval - ARRAY_OFFSET;

		if ( a_index < 0 )
		{
			eprint( FATAL, SET, "Invalid array index (%ld) in function "
					"%s().\n", a_index + ARRAY_OFFSET, Cur_Func );
			THROW( EXCEPTION );
		}

		if ( ilp != NULL )
			ilp += a_index;
		else
			idp += a_index;

		if ( v->next->next != NULL )
		{
			vars_check( v->next->next, INT_VAR | FLOAT_VAR );

			if ( v->next->type == FLOAT_VAR )
			{
				eprint( WARN, SET, "Float value used as length of slice "
						"parameter in function %s().\n", Cur_Func );
				slice_len = lrnd( v->next->next->val.dval );
			}
			else
				slice_len = v->next->next->val.lval;

			if ( slice_len < 1 )
			{
				eprint( FATAL, SET, "Zero or negative slice length used in "
						"function %s().\n", Cur_Func );
				THROW( EXCEPTION );
			}

			/* Test that the slice is within the arrays range */

			if ( slice_len != 1 && a_index + slice_len > len ) {
				if ( TEST_RUN && ( v->flags & IS_DYNAMIC ) )
					slice_len = len - a_index;
				else
				{
					eprint( FATAL, SET, "Sum of index and slice length "
							"parameter exceeds length of array in function "
							"%s().\n", Cur_Func );
					THROW( EXCEPTION );
				}
			}
		}
		else
			slice_len = len - a_index;
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
	size_t i;
	size_t len;
	long *ilp;
	double *idp;
	double val = 0.0;


	vars_check( v, INT_CONT_ARR | FLOAT_CONT_ARR | ARR_REF | ARR_PTR |
				   INT_ARR | FLOAT_ARR );

	get_array_params( v, &len, &ilp, &idp );

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
	size_t len;
	long *ilp;
	double *idp;
	long a_index;
	size_t slice_len;


	if ( v == NULL || v->next == NULL )
	{
		eprint( FATAL, SET, "Not enough parameter in call of function "
				"%s().\n", Cur_Func );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_CONT_ARR | FLOAT_CONT_ARR | ARR_REF | ARR_PTR |
				   INT_ARR | FLOAT_ARR );

	get_array_params( v, &len, &ilp, &idp );

	vars_check( v->next, INT_VAR | FLOAT_VAR );

	if ( v->next->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "Float value used as array index in function "
				"%s().\n", Cur_Func );
		a_index = lrnd( v->next->val.dval ) - ARRAY_OFFSET;
	}
	else
		a_index = v->next->val.lval - ARRAY_OFFSET;

	if ( a_index < 0 )
	{
		eprint( FATAL, SET, "Negative array index used in function "
				"%s().\n", Cur_Func );
		THROW( EXCEPTION );
	}

	if ( v->next->next != NULL )
	{
		vars_check( v->next->next, INT_VAR | FLOAT_VAR );

		if ( v->next->type == FLOAT_VAR )
		{
			eprint( WARN, SET, "Float value used as length of slice "
					"parameter in function %s().\n", Cur_Func );
			slice_len = lrnd( v->next->next->val.dval );
		}
		else
			slice_len = v->next->next->val.lval;

		if ( slice_len < 1 )
		{
			eprint( FATAL, SET, "Zero or negative slice length used in "
					"function %s().\n", Cur_Func );
			THROW( EXCEPTION );
		}
	}
	else
		slice_len = len - a_index;

	/* Test that the slice is within the arrays range */

	if ( a_index + slice_len > len )
	{
		if ( TEST_RUN && ( v->flags & IS_DYNAMIC ) )
			slice_len = len - a_index;
		else
		{
			eprint( FATAL, SET, "Sum of index and slice length parameter "
					"exceeds length of array in function %s().\n", Cur_Func );
			THROW( EXCEPTION );
		}
	}

	if ( ilp != NULL )
		return vars_push( INT_ARR, ilp + a_index, slice_len );
	else
		return vars_push( FLOAT_ARR, idp + a_index, slice_len );
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/
				 
Var *f_square( Var *v )
{
	Var *new_var;
	size_t i;
	size_t len;
	long *rlp;
	double *rdp;
	long *ilp;
	double *idp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	switch ( v->type )
	{
		case INT_VAR :
			if ( ( double ) v->val.lval >= sqrt( ( double ) LONG_MAX ) )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			return vars_push( INT_VAR, v->val.lval * v->val.lval );

		case FLOAT_VAR :
			return vars_push( FLOAT_VAR, v->val.dval * v->val.dval );

		default :
			get_array_params( v, &len, &ilp, &idp );
	}

	if ( ilp != NULL )
	{
		rlp = T_malloc( len * sizeof( long ) );
		for ( i = 0; i < len; ilp++, i++ )
		{
			if ( ( double ) *ilp >= sqrt( ( double ) LONG_MIN ) )
				eprint( SEVERE, SET, "Integer overflow in function %s().\n",
						Cur_Func );
			rlp[ i ] = *ilp * *ilp;
		}
		new_var = vars_push( INT_ARR, rlp, len );
		T_free( rlp );
	}
	else
	{
		rdp = T_malloc( len * sizeof( double ) );
		for ( i = 0; i < len; idp++, i++ )
			rdp[ i ] = *idp * *idp;
		new_var = vars_push( FLOAT_ARR, rdp, len );
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
		eprint( SEVERE, SET, "Float value used as array size in function "
				"%s().\n", Cur_Func );
		size = lrnd( v->val.dval );
	}

	if ( size <= 0 )
	{
		if ( v->type == INT_VAR )
			eprint( FATAL, SET, "Negative value (%d) used as array size in "
					"function %s().\n", v->val.lval, Cur_Func );
		else
			eprint( FATAL, SET, "Negative value (%f) used as array size in "
					"function %s().\n", v->val.dval, Cur_Func );
		THROW( EXCEPTION );
	}

	array = T_calloc( ( size_t ) size, sizeof( long ) );
	ret = vars_push( INT_ARR, array, ( size_t ) size );
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
		eprint( SEVERE, SET, "Float value used as array size in function "
				"%s().\n", Cur_Func );
		size = lrnd( v->val.dval );
	}

	if ( size <= 0 )
	{
		if ( v->type == INT_VAR )
			eprint( FATAL, SET, "Negative value (%d) used as array size in "
					"function %s().\n", v->val.lval, Cur_Func );
		else
			eprint( FATAL, SET, "Negative value (%f) used as array size in "
					"function %s().\n", v->val.dval, Cur_Func );
		THROW( EXCEPTION );
	}

	array = T_malloc( size * sizeof( double ) );
	for( i = 0; i < size; i++ )
		*( array + i ) = 0.0;
	ret = vars_push( FLOAT_ARR, array, ( size_t ) size );
	T_free( array );

	return ret;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
