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


static void vars_params( Var *v, long *elems, long **lpnt, double **dpnt );
static void vars_div_check( double val );
static void vars_mod_check( double val );
static Var *vars_int_pow( long v1, long v2 );
static void vars_pow_check( double v1, double v2 );


/*--------------------------------------------------------------------*/
/* If passed an integer or floating point variable or array slice the */
/* function returns the number of elements and a pointer to the data  */
/* (in lpnt or dpnt depending on the type of the data, the other      */
/* pointer is always set to NULL).                                    */
/*--------------------------------------------------------------------*/

static void vars_params( Var *v, long *elems, long **lpnt, double **dpnt )
{
	*lpnt = NULL;
	*dpnt = NULL;

	switch ( v->type )
	{
		case INT_VAR :
			*elems = 1;
			*lpnt = &v->val.lval;
			break;

		case FLOAT_VAR :
			*elems = 1;
			*dpnt = &v->val.dval;
			break;

		case ARR_REF :
			if ( v->from->dim != 1 )
			{
				eprint( FATAL, SET, "Arithmetic can be only done "
						"on numbers or array slices.\n" );
				THROW( EXCEPTION );
			}

			if ( v->from->flags & NEED_ALLOC )
			{
				eprint( FATAL, SET, "Array `%s' is a dynamically sized "
						"array of still unknown size.\n", v->from->name );
				THROW( EXCEPTION );
			}

			vars_check( v->from, INT_ARR | FLOAT_ARR );
			*elems = v->from->len;
			if ( v->from->type == INT_ARR )
				*lpnt = v->from->val.lpnt;
			else
				*dpnt = v->from->val.dpnt;
			break;

		case ARR_PTR :
			vars_check( v->from, INT_ARR | FLOAT_ARR );
			*elems = v->from->sizes[ v->from->dim - 1 ];
			if ( v->from->type == INT_ARR )
				*lpnt = ( long * ) v->val.gptr;
			else
				*dpnt = ( double * ) v->val.gptr;
			break;

		case INT_TRANS_ARR :
			*elems = v->len;
			*lpnt = v->val.lpnt;
			break;

		case FLOAT_TRANS_ARR :
			*elems = v->len;
			*dpnt = v->val.dpnt;
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_add_to_int_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt != NULL )
			return vars_push( INT_VAR, v1->val.lval + *v2_lpnt );
		else
			return vars_push( FLOAT_VAR, ( double ) v1->val.lval + *v2_dpnt );
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
			lp[ i ] = v1->val.lval + *v2_lpnt++;
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
			dp[ i ] = ( double ) v1->val.lval + *v2_dpnt++;
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v2->flags & IS_DYNAMIC;
	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_add_to_float_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
		return vars_push( FLOAT_VAR, v1->val.dval
						  + ( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt ) );

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
		dp[ i ] = v1->val.dval
			      + ( v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v2->flags & IS_DYNAMIC;
	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_add_to_int_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	long *v1_lpnt;
	long v1_len;


	fsc2_assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == INT_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_lpnt = ( long * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_lpnt = v1->val.lpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			lp = T_malloc( v1_len * sizeof( long ) );
			for ( i = 0; i < v1_len; i++ )
				lp[ i ] = *v1_lpnt++ + *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1_len );
			T_free( lp );
		}
		else
		{
			dp = T_malloc( v1_len * sizeof( double ) );
			for ( i = 0; i < v1_len; i++ )
				dp[ i ] = ( double ) *v1_lpnt++ + *v2_dpnt;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
			T_free( dp );
		}

		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			 eprint( FATAL, SET, "Sizes of array slices to be added "
					 "differ.\n" );
			 THROW( EXCEPTION );
		}
		else
			elems = l_min( elems, v1_len );
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
			lp[ i ] = *v1_lpnt++ + *v2_lpnt++;
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
			dp[ i ] = ( double ) *v1_lpnt++ + *v2_dpnt++;
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;

	return new_var;
}
		

/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_add_to_float_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	double *v1_dpnt;
	long v1_len;


	fsc2_assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == FLOAT_ARR ) );

	if ( v1->type == FLOAT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_dpnt = ( double * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_dpnt = v1->val.dpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		dp = T_malloc( v1_len * sizeof( double ) );
		for ( i = 0; i < v1_len; i++ )
			dp[ i ] = *v1_dpnt++
				      + ( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );

		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		T_free( dp );

		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices to be added "
					"differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
		dp[ i ] = *v1_dpnt++
			      + ( v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;

	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_sub_from_int_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt )
			return vars_push( INT_VAR, v1->val.lval - *v2_lpnt );
		else
			return vars_push( FLOAT_VAR, ( double ) v1->val.lval - *v2_dpnt );
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
			lp[ i ] = v1->val.lval - *v2_lpnt++;
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
			dp[ i ] = ( double ) v1->val.lval - *v2_dpnt++;
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v2->flags & IS_DYNAMIC;
	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_sub_from_float_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
		return vars_push( FLOAT_VAR, v1->val.dval
						  - ( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt ) );

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
		dp[ i ] = v1->val.dval
			      - ( v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v2->flags & IS_DYNAMIC;
	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_sub_from_int_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	long *v1_lpnt;
	long v1_len;


	fsc2_assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == INT_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_lpnt = ( long * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_lpnt = v1->val.lpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			lp = T_malloc( v1_len * sizeof( long ) );
			for ( i = 0; i < v1_len; i++ )
				lp[ i ] = *v1_lpnt++ - *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1_len );
			T_free( lp );
		}
		else
		{
			dp = T_malloc( v1_len * sizeof( double ) );
			for ( i = 0; i < v1_len; i++ )
				dp[ i ] = ( double ) *v1_lpnt++ - *v2_dpnt;

			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
			T_free( dp );
		}

		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices to be subtracted "
					"differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
			lp[ i ] = *v1_lpnt++ - *v2_lpnt++;
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
			dp[ i ] = ( double ) *v1_lpnt++ - *v2_dpnt++;
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	return new_var;
}
		

/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_sub_from_float_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	double *v1_dpnt;
	long v1_len;


	fsc2_assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == FLOAT_ARR ) );

	if ( v1->type == FLOAT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_dpnt = ( double * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_dpnt = v1->val.dpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		dp = T_malloc( v1_len * sizeof( double ) );
		for ( i = 0; i < v1_len; i++ )
			dp[ i ] = *v1_dpnt++
				      - ( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );

		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		T_free( dp );

		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices to be subtracted "
					"differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
		dp[ i ] = *v1_dpnt++
			      - ( v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_mult_by_int_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt )
			return vars_push( INT_VAR, v1->val.lval * *v2_lpnt );
		else
			return vars_push( FLOAT_VAR, ( double ) v1->val.lval * *v2_dpnt );
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
			lp[ i ] = v1->val.lval * *v2_lpnt++;
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
			dp[ i ] = ( double ) v1->val.lval * *v2_dpnt++;
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v2->flags & IS_DYNAMIC;
	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_mult_by_float_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
		return vars_push( FLOAT_VAR, v1->val.dval
						  * ( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt ) );

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
		dp[ i ] = v1->val.dval
			      * ( v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v2->flags & IS_DYNAMIC;
	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_mult_by_int_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	long *v1_lpnt;
	long v1_len;


	fsc2_assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == INT_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_lpnt = ( long * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_lpnt = v1->val.lpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			lp = T_malloc( v1_len * sizeof( long ) );
			for ( i = 0; i < v1_len; i++ )
				lp[ i ] = *v1_lpnt * *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1_len );
			T_free( lp );
		}
		else
		{
			dp = T_malloc( v1_len * sizeof( double ) );
			for ( i = 0; i < v1_len; i++ )
				dp[ i ] = ( double ) *v1_lpnt++ * *v2_dpnt;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
			T_free( dp );
		}

		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices to be multiplied "
					"differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
			lp[ i ] = *v1_lpnt++ * *v2_lpnt++;
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
			dp[ i ] = ( double ) *v1_lpnt++ * *v2_dpnt++;
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	return new_var;
}
		

/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_mult_by_float_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	double *v1_dpnt;
	long v1_len;


	fsc2_assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == FLOAT_ARR ) );

	if ( v1->type == FLOAT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_dpnt = ( double * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_dpnt = v1->val.dpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		dp = T_malloc( v1_len * sizeof( double ) );
		for ( i = 0; i < v1_len; i++ )
			dp[ i ] = *v1_dpnt++
				      * ( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );

		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		T_free( dp );

		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices to be multiplied "
					"differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
		dp[ i ] = *v1_dpnt++
			      * ( v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_div_of_int_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	long *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			vars_div_check( ( double ) *v2_lpnt );
			return vars_push( INT_VAR, v1->val.lval / *v2_lpnt );
		}
		else
		{
			vars_div_check( *v2_dpnt );
			return vars_push( FLOAT_VAR, ( double ) v1->val.lval / *v2_dpnt );
		}
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_div_check( ( double ) *v2_lpnt );
			lp[ i ] = v1->val.lval / *v2_lpnt++;
		}
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_div_check( *v2_dpnt );
			dp[ i ] = ( double ) v1->val.lval / *v2_dpnt++;
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v2->flags & IS_DYNAMIC;
	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_div_of_float_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			vars_div_check( ( double ) *v2_lpnt );
			return vars_push( FLOAT_VAR, v1->val.dval / ( double ) *v2_lpnt );
		}
		else
		{
			vars_div_check( *v2_dpnt );
			return vars_push( FLOAT_VAR, v1->val.dval / *v2_dpnt );
		}
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
	{
		if ( v2_lpnt )
		{
			vars_div_check( ( double ) *v2_lpnt );
			dp[ i ] = v1->val.dval / ( double ) *v2_lpnt++;
		}
		else
		{
			vars_div_check( *v2_dpnt );
			dp[ i ] = v1->val.dval / *v2_dpnt++;
		}
	}

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v2->flags & IS_DYNAMIC;
	T_free( dp );

	return NULL;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_div_of_int_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	long *v1_lpnt;
	long v1_len;


	fsc2_assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == INT_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_lpnt = ( long * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_lpnt = v1->val.lpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			vars_div_check( ( double ) *v2_lpnt );
			lp = T_malloc( v1_len * sizeof( long ) );
			for ( i = 0; i < v1_len; i++ )
				lp[ i ] = *v1_lpnt++ / *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1_len );
			T_free( lp );
		}
		else
		{
			vars_div_check( *v2_dpnt );
			dp = T_malloc( v1_len * sizeof( double ) );
			for ( i = 0; i < v1_len; i++ )
				dp[ i ] = ( double ) *v1_lpnt++ / *v2_dpnt;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
			T_free( dp );
		}

		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices to be divided "
					"differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_div_check( ( double ) *v2_lpnt );
			lp[ i ] = *v1_lpnt++ / *v2_lpnt++;
		}
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_div_check( *v2_dpnt );
			dp[ i ] = ( double ) *v1_lpnt++ / *v2_dpnt++;
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_div_of_float_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	double *v1_dpnt;
	long v1_len;


	fsc2_assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == FLOAT_ARR ) );

	if ( v1->type == FLOAT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_dpnt = ( double * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_dpnt = v1->val.dpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		vars_div_check( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );
		dp = T_malloc( v1_len * sizeof( double ) );
		for ( i = 0; i < v1_len; i++ )
			dp[ i ] = *v1_dpnt++
				      / ( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
		T_free( dp );

		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices to be divided "
					"differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
	{
		vars_div_check( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );
		dp[ i ] = *v1_dpnt++
			      / ( v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	}

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static void vars_div_check( double val )
{
	if ( val != 0.0 )
		return;
	eprint( FATAL, SET, "Division by zero.\n" );
	THROW( EXCEPTION );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_mod_of_int_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	long *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			vars_mod_check( ( double ) *v2_lpnt );
			return vars_push( INT_VAR, v1->val.lval % *v2_lpnt );
		}
		else
		{
			vars_mod_check( *v2_dpnt );
			return vars_push( FLOAT_VAR, fmod( ( double ) v1->val.lval,
											   *v2_dpnt ) );
		}
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_mod_check( ( double ) *v2_lpnt );
			lp[ i ] = v1->val.lval % *v2_lpnt++;
		}
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_mod_check( *v2_dpnt );
			dp[ i ] = fmod( ( double ) v1->val.lval, *v2_dpnt++ );
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v2->flags & IS_DYNAMIC;
	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_mod_of_float_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			vars_mod_check( ( double ) *v2_lpnt );
			return vars_push( FLOAT_VAR, fmod( v1->val.dval,
											   ( double ) *v2_lpnt ) );
		}
		else
		{
			vars_mod_check( *v2_dpnt );
			return vars_push( FLOAT_VAR, fmod( v1->val.dval, *v2_dpnt ) );
		}
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
	{
		if ( v2_lpnt )
		{
			vars_mod_check( ( double ) *v2_lpnt );
			dp[ i ] = fmod( v1->val.dval, ( double ) *v2_lpnt++ );
		}
		else
		{
			vars_mod_check( *v2_dpnt );
			dp[ i ] = fmod( v1->val.dval, *v2_dpnt++ );
		}
	}

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v2->flags & IS_DYNAMIC;
	T_free( dp );

	return NULL;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_mod_of_int_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	long *v1_lpnt;
	long v1_len;


	fsc2_assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == INT_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_lpnt = ( long * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_lpnt = v1->val.lpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			vars_mod_check( ( double ) *v2_lpnt );
			lp = T_malloc( v1_len * sizeof( long ) );
			for ( i = 0; i < v1_len; i++ )
				lp[ i ] = *v1_lpnt++ % *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1_len );
			T_free( lp );
		}
		else
		{
			vars_mod_check( *v2_dpnt );
			dp = T_malloc( v1_len * sizeof( double ) );
			for ( i = 0; i < v1_len; i++ )
				dp[ i ] = fmod( ( double ) *v1_lpnt++, *v2_dpnt );
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
			T_free( dp );
		}

		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices modulo is to be "
					"calculated from differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_mod_check( ( double ) *v2_lpnt );
			lp[ i ] = *v1_lpnt++ % *v2_lpnt++;
		}
		new_var = vars_push( INT_TRANS_ARR, lp, elems );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_mod_check( *v2_dpnt );
			dp[ i ] = fmod( ( double ) *v1_lpnt++, *v2_dpnt++ );
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_mod_of_float_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	double *v1_dpnt;
	long v1_len;


	fsc2_assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == FLOAT_ARR ) );

	if ( v1->type == FLOAT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_dpnt = ( double * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_dpnt = v1->val.dpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		vars_mod_check( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );
		dp = T_malloc( v1_len * sizeof( double ) );
		for ( i = 0; i < v1_len; i++ )
			dp[ i ] = fmod( *v1_dpnt,
							v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
		T_free( dp );

		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices modulo is to be "
					"calculated from differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
	{
		vars_mod_check( v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt);
		dp[ i ] = fmod( *v1_dpnt++,
						v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	}

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static void vars_mod_check( double val )
{
	if ( val != 0.0 )
		return;
	eprint( FATAL, SET, "Modulo by zero.\n" );
	THROW( EXCEPTION );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static Var *vars_int_pow( long v1, long v2 )
{
	long ires,
		 i;
	double v = 0.0;
		

	if ( v2 == 0 )
		return vars_push( INT_VAR, 1L );
	if ( v2 > 0 && ( v = pow( ( double ) v1, ( double ) v2 ) ) <= LONG_MAX )
	{
		for ( ires = v1, i = 1; i < v2; i++ )
			ires *= v1;
		return vars_push( INT_VAR, ires );
	}

	return vars_push( FLOAT_VAR, v );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_pow_of_int_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i, j;
	long elems;
	long *lp = NULL;
	long *v2_lpnt;
	double *dp = NULL;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt )
			return vars_int_pow( v1->val.lval, *v2_lpnt );
		else
		{
			vars_pow_check( ( double ) v1->val.lval, *v2_dpnt );
			return vars_push( FLOAT_VAR,
							  pow( ( double ) v1->val.lval, *v2_dpnt ) );
		}
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
		{
			if ( lp != NULL )
			{
				new_var = vars_int_pow( v1->val.lval, *v2_lpnt++ );
				if ( new_var->type == INT_VAR )
					lp[ i ] = new_var->val.lval;
				else
				{
					dp = T_malloc( elems * sizeof( double ) );
					for ( j = 0; j < i; j++ )
						dp[ j ] = lp[ j ];
					dp[ i ] = new_var->val.dval;
					lp = T_free( lp );
				}

				vars_pop( new_var );
				continue;
			}

			vars_pow_check( ( double ) v1->val.lval, ( double ) *v2_lpnt );
			dp[ i ] = pow( ( double ) v1->val.lval, ( double ) *v2_lpnt++ );
		}

		if ( lp != NULL )
		{
			new_var = vars_push( INT_TRANS_ARR, lp, elems );
			T_free( lp );
		}
		else
		{
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			T_free( dp );
		}
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_pow_check( ( double ) v1->val.lval, *v2_dpnt );
			dp[ i ] = pow( ( double ) v1->val.lval, *v2_dpnt++ );
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v2->flags & IS_DYNAMIC;
	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_pow_of_float_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		vars_pow_check( v1->val.dval,
						v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );
		return vars_push( FLOAT_VAR,
						  pow( v1->val.dval,
							   v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt ) );
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
	{
		vars_pow_check( v1->val.dval,
						v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
		dp[ i ] = pow( v1->val.dval,
					   v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	}

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v2->flags & IS_DYNAMIC;

	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_pow_of_int_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i, j;
	long elems;
	long *lp = NULL;
	long *v2_lpnt;
	double *dp = NULL;
	double *v2_dpnt;
	long *v1_lpnt;
	long v1_len;


	fsc2_assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == INT_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_lpnt = ( long * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_lpnt = v1->val.lpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		if ( v2_lpnt )
		{
			lp = T_malloc( v1_len * sizeof( long ) );
			for ( i = 0; i < v1_len; i++ )
			{
				if ( lp != NULL )
				{
					new_var = vars_int_pow( *v1_lpnt++, *v2_lpnt );
					if ( new_var->type == INT_VAR )
						lp[ i ] = new_var->val.lval;
					else
					{
						dp = T_malloc( v1_len * sizeof( double ) );
						for ( j = 0; j < i; j++ )
							dp[ j ] = lp[ j ];
						dp[ i ] = new_var->val.dval;
						lp = T_free( lp );
					}

					vars_pop( new_var );
					continue;
				}

				vars_pow_check( ( double ) *v1_lpnt, ( double ) *v2_lpnt );
				dp[ i ] = pow( ( double ) *v1_lpnt++, ( double ) *v2_lpnt );
			}

			if ( lp != NULL )
			{
				new_var = vars_push( INT_TRANS_ARR, lp, v1_len );
				T_free( lp );
			}
			else
			{
				new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
				T_free( dp );
			}
		}
		else
		{
			dp = T_malloc( v1_len * sizeof( double ) );
			for ( i = 0; i < v1_len; i++ )
			{
				vars_pow_check( ( double ) *v1_lpnt, *v2_dpnt );
				dp[ i ] = pow( ( double ) *v1_lpnt++, *v2_dpnt );
			}
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
			T_free( dp );
		}

		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices used in "
					"exponentiation differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	if ( v2_lpnt )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
		{
			if ( lp != NULL )
			{
				new_var = vars_int_pow( *v1_lpnt, *v2_lpnt++ );
				if ( new_var->type == INT_VAR )
					lp[ i ] = new_var->val.lval;
				else
				{
					dp = T_malloc( elems * sizeof( double ) );
					for ( j = 0; j < i; j++ )
						dp[ j ] = lp[ j ];
					dp[ i ] = new_var->val.dval;
					lp = T_free( lp );
				}

				vars_pop( new_var );
				continue;
			}

			vars_pow_check( ( double ) *v1_lpnt, ( double ) *v2_lpnt );
			dp[ i ] = pow( ( double ) *v1_lpnt++, ( double ) *v2_lpnt++ );
		}

		if ( lp != NULL )
		{
			new_var = vars_push( INT_TRANS_ARR, lp, elems );
			T_free( lp );
		}
		else
		{
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			T_free( dp );
		}
	}
	else
	{
		dp = T_malloc( elems * sizeof( double ) );

		for ( i = 0; i < elems; i++ )
		{
			vars_pow_check( ( double ) *v1_lpnt, *v2_dpnt );
			dp[ i ] = pow( ( double ) *v1_lpnt++, *v2_dpnt++ );
		}

		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	return new_var;
}
		

/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *vars_pow_of_float_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *v2_lpnt;
	double *dp;
	double *v2_dpnt;
	double *v1_dpnt;
	long v1_len;


	fsc2_assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) ||
				 ( v1->type == ARR_PTR && v1->from->type == FLOAT_ARR ) );

	if ( v1->type == FLOAT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, SET, "Arithmetic can be only done on numbers and "
				"array slices.\n" );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( v1->type == ARR_PTR )
	{
		v1_dpnt = ( double * ) v1->val.gptr;
		v1_len = v1->from->sizes[ v1->from->dim -1 ];
	}
	else
	{
		v1_dpnt = v1->val.dpnt;
		v1_len = v1->len;
	}

	if ( elems == 1 )
	{
		dp = T_malloc( v1_len * sizeof( double ) );
		for ( i = 0; i < v1_len; i++ )
		{
			vars_pow_check( *v1_dpnt,
							v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );
			dp[ i ] = pow( *v1_dpnt++, 
						   v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1_len );
		T_free( dp );

		new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
		return new_var;
	}

	if ( v1_len != elems )
	{
		if ( ! ( TEST_RUN && ( ( v1->flags | v2->flags ) & IS_DYNAMIC ) ) )
		{
			eprint( FATAL, SET, "Sizes of array slices used in "
					"exponentiation differ.\n" );
			THROW( EXCEPTION );
		}
		else
			elems = l_min( v1_len, elems );
	}

	dp = T_malloc( elems * sizeof( double ) );

	for ( i = 0; i < elems; i++ )
	{
		vars_pow_check( *v1_dpnt, v2_lpnt ? ( double ) *v2_lpnt : *v2_dpnt );
		dp[ i ] = pow( *v1_dpnt++,
					   v2_lpnt ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	}

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
	new_var->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static void vars_pow_check( double v1, double v2 )
{
	if ( v1 < 0.0 && fmod( fabs( v2 ), 1.0 ) != 0.0 )
	{
		eprint( FATAL, SET, "Negative base while exponent is not an "
				"integer value.\n" );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------------------------*/
/* When doing arithmetic with an array it can happen that the left hand */
/* side array has a still undefined size. In this case the size has to  */
/* be adjusted automatically to the size of the right hand side array   */
/* (or an exception must be thrown if the right hand side array also    */
/* has an undefined size).                                              */
/*----------------------------------------------------------------------*/

Var *vars_array_check( Var *v1, Var *v2 )
{
	Var *v = NULL;
	long length = 0;


	/* If the left hand side has a defined size nothing needs to be done */

	if ( v1->type == ARR_REF || v1->type == ARR_PTR )
	{
		if ( v1->type == ARR_REF && v1->from->dim != 1 )
		{
			eprint( FATAL, SET, "Arithmetic can be only done "
					"on array slices.\n" );
			THROW( EXCEPTION );
		}

		if ( ! ( v1->from->flags & NEED_ALLOC ) )
			return v1;
	}
	else
	{
		if ( ! ( v1->flags & NEED_ALLOC ) )
			return v1;
	}

	/* Make sure the right hand side is an array with a defined size */

	if ( ! ( v2->type &
			 ( ARR_REF | ARR_PTR | INT_TRANS_ARR | FLOAT_TRANS_ARR ) )
		 || ( v2->type & ( INT_TRANS_ARR | FLOAT_TRANS_ARR ) &&
			  v2->flags & NEED_ALLOC )
		 || ( v2->type & ( ARR_REF | ARR_PTR ) &&
			  v2->from->flags & NEED_ALLOC ) )
	{
		eprint( FATAL, SET, "Size of array can't be determined.\n" );
		THROW( EXCEPTION );
	}

	/* Find out the size of the right hand side array */

	switch ( v2->type )
	{
		case ARR_REF :
			if ( v2->from->dim != 1 )
			{
				eprint( FATAL, SET, "Arithmetic can be only done "
						"on numbers or array slices.\n" );
				THROW( EXCEPTION );
			}
			vars_check( v2->from, INT_ARR | FLOAT_ARR );
			length = v2->from->len;
			break;

		case ARR_PTR :
			vars_check( v2->from, INT_ARR | FLOAT_ARR );
			length = v2->from->sizes[ v2->from->dim - 1 ];
			break;

		case INT_TRANS_ARR : case FLOAT_TRANS_ARR :
			length = v2->len;
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	switch ( v1->type )
	{
		case INT_ARR : case INT_TRANS_ARR :
			v = vars_push( INT_TRANS_ARR, NULL, length );
			break;

		case FLOAT_ARR : case FLOAT_TRANS_ARR :
			v = vars_push( FLOAT_TRANS_ARR, NULL, length );
			break;

		case ARR_REF : case ARR_PTR :
			if ( v1->from->type == INT_ARR )
				v = vars_push( INT_TRANS_ARR, NULL, length );
			else
				v = vars_push( FLOAT_TRANS_ARR, NULL, length );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	v->flags |= v1->flags & v2->flags & IS_DYNAMIC;
	vars_pop( v1 );
	return v;
}
