/*
  $Id$
*/


#include "fsc2.h"


Var *vars_add_to_int_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	double *dp;


	switch ( v2->type )
	{
		case INT_VAR :
			return vars_push( INT_VAR, v1->val.lval + v2->val.lval );

		case FLOAT_VAR :
			return vars_push( FLOAT_VAR,
							  ( double ) v1->val.lval + v2->val.dval );

		case ARR_REF :
			if ( v2->from->dim != 1 )
			{
				eprint( FATAL, "%s:%ld: Arithmetic can be only done "
						"on array slices.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			vars_check( v2->from, INT_ARR | FLOAT_ARR );
			elems = v2->from->len;
			if ( v2->from->type == INT_ARR )
			{
				lp = T_malloc( elems * sizeof( long ) );
				for ( i = 0; i < elems; i++ )
					lp[ i ] = v1->val.lval + v2->from->val.lpnt[ i ];
				new_var = vars_push( INT_TRANS_ARR, lp, elems );
				T_free( lp );
			}
			else
			{
				dp = T_malloc( elems * sizeof( double ) );
				for ( i = 0; i < elems; i++ )
					dp[ i ] = ( double ) v1->val.lval
						      + v2->from->val.dpnt[ i ];
				new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
				T_free( dp );
			}
			return new_var;

		case INT_TRANS_ARR :
			elems = v2->len;
			lp = T_malloc( elems * sizeof( long ) );
			for ( i = 0; i < elems; i++ )
				lp[ i ] = v1->val.lval + v2->val.lpnt[ i ];
			new_var = vars_push( INT_TRANS_ARR, lp, elems );
			T_free( lp );
			return new_var;

		case FLOAT_TRANS_ARR :
			elems = v2->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = ( double ) v1->val.lval + v2->val.dpnt[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			T_free( dp );
			return new_var;

		default :
			assert( 1 == 0 );
	}

	return NULL;
}


Var *vars_add_to_float_var( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	double *dp;


	switch ( v2->type )
	{
		case INT_VAR :
			return vars_push( FLOAT_VAR, v1->val.dval
							             + ( double ) v2->val.lval );

		case FLOAT_VAR :
			return vars_push( FLOAT_VAR, v1->val.dval + v2->val.dval );

		case ARR_REF :
			if ( v2->from->dim != 1 )
			{
				eprint( FATAL, "%s:%ld: Arithmetic can be only done "
						"on array slices.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			vars_check( v2->from, INT_ARR | FLOAT_ARR );
			elems = v2->from->len;
			dp = T_malloc( elems * sizeof( long ) );
			if ( v2->from->type == INT_ARR )
				for ( i = 0; i < elems; i++ )
					dp[ i ] = v1->val.dval
						      + ( double ) v2->from->val.lpnt[ i ];
			else
				for ( i = 0; i < elems; i++ )
					dp[ i ] = v1->val.dval + v2->from->val.dpnt[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			break;

		case INT_TRANS_ARR :
			elems = v2->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = v1->val.dval + ( double ) v2->val.lpnt[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			break;

		case FLOAT_TRANS_ARR :
			elems = v2->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = v1->val.dval + v2->val.dpnt[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			break;

		default :
			assert( 1 == 0 );
	}

	T_free( dp );
	return new_var;
}


Var *vars_add_to_int_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	long *lp;
	double *dp;


	assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done "
				"on array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	switch ( v2->type )
	{
		case INT_VAR :
			elems = v1->len;
			lp = T_malloc( elems * sizeof( long ) );
			for ( i = 0; i < elems; i++ )
				lp[ i ] = v1->val.lpnt[ i ] + v2->val.lval;
			new_var = vars_push( INT_TRANS_ARR, lp, elems );
			T_free( lp );
			return new_var;

		case FLOAT_VAR :
			elems = v1->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = ( double ) v1->val.lpnt[ i ] + v2->val.dval;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			T_free( dp );
			return new_var;

		case ARR_REF :
			if ( v2->from->dim != 1 )
			{
				eprint( FATAL, "%s:%ld: Arithmetic can be only done "
						"on array slices.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			vars_check( v2->from, INT_ARR | FLOAT_ARR );
			if ( v1->len != v2->from->len )
			{
				eprint( FATAL, "%s:%ld: Sizes of array slices to be added "
						"differ.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			elems = v1->len;
			if ( v2->from->type == INT_ARR )
			{
				lp = T_malloc( elems * sizeof( long ) );
				for ( i = 0; i < elems; i++ )
					lp[ i ] = v1->val.lpnt[ i ] + v2->from->val.lpnt[ i ];
				new_var = vars_push( INT_TRANS_ARR, lp, elems );
				T_free( lp );
			}
			else
			{
				dp = T_malloc( elems * sizeof( double ) );
				for ( i = 0; i < elems; i++ )
					dp[ i ] = ( double ) v1->val.lpnt[ i ]
						      + v2->from->val.dpnt[ i ];
				new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
				T_free( dp );
			}
			return new_var;

		case INT_TRANS_ARR :
			if ( v1->len != v2->len )
			{
				eprint( FATAL, "%s:%ld: Sizes of array slices to be added "
						"differ.\n", Fname, Lc );
				THROW( EXCEPTION );
			}
			elems = v1->len;
			lp = T_malloc( elems * sizeof( long ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = v1->val.lpnt[ i ] + v2->val.lpnt[ i ];
			new_var = vars_push( INT_TRANS_ARR, lp, elems );
			T_free( lp );
			return new_var;

		case FLOAT_TRANS_ARR :
			if ( v1->len != v2->len )
			{
				eprint( FATAL, "%s:%ld: Sizes of array slices to be added "
						"differ.\n", Fname, Lc );
				THROW( EXCEPTION );
			}
			elems = v1->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = ( double ) v1->val.lpnt[ i ] + v2->val.dpnt[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			T_free( dp );
			return new_var;

		default :
			assert( 1 == 0 );
	}

	return NULL;
}


Var *vars_add_to_float_arr( Var *v1, Var *v2 )
{
	Var *new_var;
	long i;
	long elems;
	double *dp;


	assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) );

	if ( v1->type  == FLOAT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done "
				"on array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	switch ( v2->type )
	{
		case INT_VAR :
			elems = v1->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = v1->val.dpnt[ i ] + ( double ) v2->val.lval;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			break;

		case FLOAT_VAR :
			elems = v1->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = v1->val.dpnt[ i ] + v2->val.dval;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			break;

		case ARR_REF :
			if ( v2->from->dim != 1 )
			{
				eprint( FATAL, "%s:%ld: Arithmetic can be only done "
						"on array slices.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			vars_check( v2->from, INT_ARR | FLOAT_ARR );
			if ( v1->len != v2->from->len )
			{
				eprint( FATAL, "%s:%ld: Sizes of array slices to be added "
						"differ.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			elems = v1->len;
			dp = T_malloc( elems * sizeof( double ) );
			if ( v2->from->type == INT_ARR )
				for ( i = 0; i < elems; i++ )
					dp[ i ] = v1->val.dpnt[ i ]
						      + ( double ) v2->from->val.lpnt[ i ];
			else
				for ( i = 0; i < elems; i++ )
					dp[ i ] = v1->val.dpnt[ i ] + v2->from->val.dpnt[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			break;

		case INT_TRANS_ARR :
			if ( v1->len != v2->len )
			{
				eprint( FATAL, "%s:%ld: Sizes of array slices to be added "
						"differ.\n", Fname, Lc );
				THROW( EXCEPTION );
			}
			elems = v1->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = v1->val.dpnt[ i ] + ( double ) v2->val.lpnt[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			break;

		case FLOAT_TRANS_ARR :
			if ( v1->len != v2->len )
			{
				eprint( FATAL, "%s:%ld: Sizes of array slices to be added "
						"differ.\n", Fname, Lc );
				THROW( EXCEPTION );
			}
			elems = v1->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = v1->val.dpnt[ i ] + v2->val.dpnt[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			break;

		default :
			assert( 1 == 0 );
	}

	T_free( dp );
	return new_var;
}
