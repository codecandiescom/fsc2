/*
  $Id$
*/


#include "fsc2.h"


static void vars_params( Var *v, long *elems, long **lpnt, double **dpnt );
static void vars_div_icheck( long val );
static void vars_div_fcheck( double val );
static void vars_mod_icheck( long val );
static void vars_mod_fcheck( double val );


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

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
				eprint( FATAL, "%s:%ld: Arithmetic can be only done "
						"on numbers or array slices.\n", Fname, Lc );
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
			assert( 1 == 0 );
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

	if ( v2_lpnt != NULL )
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
	bool v2_is_int;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
		return vars_push( FLOAT_VAR, v1->val.dval
						  + ( v2_is_int ? ( double ) *v2_lpnt : *v2_dpnt ) );

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
		dp[ i ] = v1->val.dval
			      + ( v2_is_int ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
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


	assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt != NULL )
		{
			lp = T_malloc( v1->len * sizeof( long ) );
			for ( i = 0; i < v1->len; i++ )
				lp[ i ] = v1->val.lpnt[ i ] + *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
			T_free( lp );
		}
		else
		{
			dp = T_malloc( v1->len * sizeof( double ) );
			for ( i = 0; i < v1->len; i++ )
				dp[ i ] = ( double ) v1->val.lpnt[ i ] + *v2_dpnt;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
			T_free( dp );
		}

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be added differ.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v2_lpnt != NULL )
	{
		lp = T_malloc( v1->len * sizeof( long ) );
		for ( i = 0; i < v1->len; i++ )
			lp[ i ] = v1->val.lpnt[ i ] + *v2_lpnt++;
		new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		for ( i = 0; i < v1->len; i++ )
			dp[ i ] = ( double ) v1->val.lpnt[ i ] + *v2_dpnt++;
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );
	}

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
	bool v2_is_int;


	assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		for ( i = 0; i < v1->len; i++ )
			dp[ i ] = v1->val.dpnt[ i ]
				      + ( v2_is_int ? ( double ) *v2_lpnt : *v2_dpnt );
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be added differ.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	dp = T_malloc( v1->len * sizeof( double ) );
	for ( i = 0; i < v1->len; i++ )
		dp[ i ] = v1->val.dpnt[ i ]
			      + ( v2_is_int ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
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
		if ( v2_lpnt != NULL )
			return vars_push( INT_VAR, v1->val.lval - *v2_lpnt );
		else
			return vars_push( FLOAT_VAR, ( double ) v1->val.lval - *v2_dpnt );
	}

	if ( v2_lpnt != NULL )
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
	bool v2_is_int;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
		return vars_push( FLOAT_VAR, v1->val.dval
						  - ( v2_is_int ? ( double ) *v2_lpnt : *v2_dpnt ) );

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
		dp[ i ] = v1->val.dval
			      - ( v2_is_int ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
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


	assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt != NULL )
		{
			lp = T_malloc( v1->len * sizeof( long ) );
			for ( i = 0; i < v1->len; i++ )
				lp[ i ] = v1->val.lpnt[ i ] - *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
			T_free( lp );
		}
		else
		{
			dp = T_malloc( v1->len * sizeof( double ) );
			for ( i = 0; i < v1->len; i++ )
				dp[ i ] = ( double ) v1->val.lpnt[ i ] - *v2_dpnt;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
			T_free( dp );
		}

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be subtracted "
				"differ.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v2_lpnt != NULL )
	{
		lp = T_malloc( v1->len * sizeof( long ) );
		for ( i = 0; i < v1->len; i++ )
			lp[ i ] = v1->val.lpnt[ i ] - *v2_lpnt++;
		new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		for ( i = 0; i < v1->len; i++ )
			dp[ i ] = ( double ) v1->val.lpnt[ i ] - *v2_dpnt++;
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );
	}

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
	bool v2_is_int;


	assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		for ( i = 0; i < v1->len; i++ )
			dp[ i ] = v1->val.dpnt[ i ]
				      - ( v2_is_int ? ( double ) *v2_lpnt : *v2_dpnt );
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be subtracted "
				"differ.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	dp = T_malloc( v1->len * sizeof( double ) );
	for ( i = 0; i < v1->len; i++ )
		dp[ i ] = v1->val.dpnt[ i ]
			      - ( v2_is_int ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
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
		if ( v2_lpnt != NULL )
			return vars_push( INT_VAR, v1->val.lval * *v2_lpnt );
		else
			return vars_push( FLOAT_VAR, ( double ) v1->val.lval * *v2_dpnt );
	}

	if ( v2_lpnt != NULL )
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
	bool v2_is_int;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
		return vars_push( FLOAT_VAR, v1->val.dval
						  * ( v2_is_int ? ( double ) *v2_lpnt : *v2_dpnt ) );

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
		dp[ i ] = v1->val.dval
			      * ( v2_is_int ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
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


	assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt != NULL )
		{
			lp = T_malloc( v1->len * sizeof( long ) );
			for ( i = 0; i < v1->len; i++ )
				lp[ i ] = v1->val.lpnt[ i ] * *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
			T_free( lp );
		}
		else
		{
			dp = T_malloc( v1->len * sizeof( double ) );
			for ( i = 0; i < v1->len; i++ )
				dp[ i ] = ( double ) v1->val.lpnt[ i ] * *v2_dpnt;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
			T_free( dp );
		}

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be multiplied "
				"differ.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v2_lpnt != NULL )
	{
		lp = T_malloc( v1->len * sizeof( long ) );
		for ( i = 0; i < v1->len; i++ )
			lp[ i ] = v1->val.lpnt[ i ] * *v2_lpnt++;
		new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		for ( i = 0; i < v1->len; i++ )
			dp[ i ] = ( double ) v1->val.lpnt[ i ] * *v2_dpnt++;
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );
	}

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
	bool v2_is_int;


	assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		for ( i = 0; i < v1->len; i++ )
			dp[ i ] = v1->val.dpnt[ i ]
				      * ( v2_is_int ? ( double ) *v2_lpnt : *v2_dpnt );
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be multiplied "
				"differ.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	dp = T_malloc( v1->len * sizeof( double ) );
	for ( i = 0; i < v1->len; i++ )
		dp[ i ] = v1->val.dpnt[ i ]
			      * ( v2_is_int ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
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
		if ( v2_lpnt != NULL )
		{
			vars_div_icheck( *v2_lpnt );
			return vars_push( INT_VAR, v1->val.lval / *v2_lpnt );
		}
		else
		{
			vars_div_fcheck( *v2_dpnt );
			return vars_push( FLOAT_VAR, ( double ) v1->val.lval / *v2_dpnt );
		}
	}

	if ( v2_lpnt != NULL )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_div_icheck( *v2_lpnt );
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
			vars_div_fcheck( *v2_dpnt );
			dp[ i ] = ( double ) v1->val.lval / *v2_dpnt++;
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

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
	bool v2_is_int;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
	{
		if ( v2_is_int )
		{
			vars_div_icheck( *v2_lpnt );
			return vars_push( FLOAT_VAR, v1->val.dval / ( double ) *v2_lpnt );
		}
		else
		{
			vars_div_fcheck( *v2_dpnt );
			return vars_push( FLOAT_VAR, v1->val.dval / *v2_dpnt );
		}
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
	{
		if ( v2_is_int )
		{
			vars_div_icheck( *v2_lpnt );
			dp[ i ] = v1->val.dval / ( double ) *v2_lpnt++;
		}
		else
		{
			vars_div_fcheck( *v2_dpnt );
			dp[ i ] = v1->val.dval / *v2_dpnt++;
		}
	}

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
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


	assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt != NULL )
		{
			vars_div_icheck( *v2_lpnt );
			lp = T_malloc( v1->len * sizeof( long ) );
			for ( i = 0; i < v1->len; i++ )
				lp[ i ] = v1->val.lpnt[ i ] / *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
			T_free( lp );
		}
		else
		{
			vars_div_fcheck( *v2_dpnt );
			dp = T_malloc( v1->len * sizeof( double ) );
			for ( i = 0; i < v1->len; i++ )
				dp[ i ] = ( double ) v1->val.lpnt[ i ] / *v2_dpnt;
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
			T_free( dp );
		}

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be divided differ.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v2_lpnt != NULL )
	{
		lp = T_malloc( v1->len * sizeof( long ) );
		for ( i = 0; i < v1->len; i++ )
		{
			vars_div_icheck( *v2_lpnt );
			lp[ i ] = v1->val.lpnt[ i ] / *v2_lpnt++;
		}
		new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		for ( i = 0; i < v1->len; i++ )
		{
			vars_div_fcheck( *v2_dpnt );
			dp[ i ] = ( double ) v1->val.lpnt[ i ] / *v2_dpnt++;
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );
	}

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
	bool v2_is_int;


	assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		if ( v2_is_int )
			vars_div_icheck( *v2_lpnt );
		else
			vars_div_fcheck( *v2_dpnt );
		for ( i = 0; i < v1->len; i++ )
			dp[ i ] = v1->val.dpnt[ i ]
				      / ( v2_is_int ? ( double ) *v2_lpnt : *v2_dpnt );
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be divided differ.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	dp = T_malloc( v1->len * sizeof( double ) );
	for ( i = 0; i < v1->len; i++ )
	{
		if ( v2_is_int )
			vars_div_icheck( *v2_lpnt );
		else
			vars_div_fcheck( *v2_dpnt );
		dp[ i ] = v1->val.dpnt[ i ]
			      / ( v2_is_int ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	}
	new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
	T_free( dp );

	return new_var;
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
		if ( v2_lpnt != NULL )
		{
			vars_mod_icheck( *v2_lpnt );
			return vars_push( INT_VAR, v1->val.lval % *v2_lpnt );
		}
		else
		{
			vars_mod_fcheck( *v2_dpnt );
			return vars_push( FLOAT_VAR, fmod( ( double ) v1->val.lval,
											   *v2_dpnt ) );
		}
	}

	if ( v2_lpnt != NULL )
	{
		lp = T_malloc( elems * sizeof( long ) );
		for ( i = 0; i < elems; i++ )
		{
			vars_mod_icheck( *v2_lpnt );
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
			vars_mod_fcheck( *v2_dpnt );
			dp[ i ] = fmod( ( double ) v1->val.lval, *v2_dpnt++ );
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
		T_free( dp );
	}

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
	bool v2_is_int;


	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
	{
		if ( v2_is_int )
		{
			vars_mod_icheck( *v2_lpnt );
			return vars_push( FLOAT_VAR, fmod( v1->val.dval,
											   ( double ) *v2_lpnt ) );
		}
		else
		{
			vars_mod_fcheck( *v2_dpnt );
			return vars_push( FLOAT_VAR, fmod( v1->val.dval, *v2_dpnt ) );
		}
	}

	dp = T_malloc( elems * sizeof( double ) );
	for ( i = 0; i < elems; i++ )
	{
		if ( v2_is_int )
		{
			vars_mod_icheck( *v2_lpnt );
			dp[ i ] = fmod( v1->val.dval, ( double ) *v2_lpnt++ );
		}
		else
		{
			vars_mod_fcheck( *v2_dpnt );
			dp[ i ] = fmod( v1->val.dval, *v2_dpnt++ );
		}
	}

	new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
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


	assert( v1->type & ( INT_ARR | INT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );

	if ( elems == 1 )
	{
		if ( v2_lpnt != NULL )
		{
			vars_mod_icheck( *v2_lpnt );
			lp = T_malloc( v1->len * sizeof( long ) );
			for ( i = 0; i < v1->len; i++ )
				lp[ i ] = v1->val.lpnt[ i ] % *v2_lpnt;
			new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
			T_free( lp );
		}
		else
		{
			vars_mod_fcheck( *v2_dpnt );
			dp = T_malloc( v1->len * sizeof( double ) );
			for ( i = 0; i < v1->len; i++ )
				dp[ i ] = fmod( ( double ) v1->val.lpnt[ i ], *v2_dpnt );
			new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
			T_free( dp );
		}

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be divided differ.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v2_lpnt != NULL )
	{
		lp = T_malloc( v1->len * sizeof( long ) );
		for ( i = 0; i < v1->len; i++ )
		{
			vars_mod_icheck( *v2_lpnt );
			lp[ i ] = v1->val.lpnt[ i ] % *v2_lpnt++;
		}
		new_var = vars_push( INT_TRANS_ARR, lp, v1->len );
		T_free( lp );
	}
	else
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		for ( i = 0; i < v1->len; i++ )
		{
			vars_mod_fcheck( *v2_dpnt );
			dp[ i ] = fmod( ( double ) v1->val.lpnt[ i ], *v2_dpnt++ );
		}
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );
	}

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
	bool v2_is_int;


	assert( v1->type & ( FLOAT_ARR | FLOAT_TRANS_ARR ) );

	if ( v1->type == INT_ARR && v1->dim != 1 )
	{
		eprint( FATAL, "%s:%ld: Arithmetic can be only done on numbers and "
				"array slices.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_params( v2, &elems, &v2_lpnt, &v2_dpnt );
	v2_is_int = v2_lpnt != NULL ? SET : UNSET;

	if ( elems == 1 )
	{
		dp = T_malloc( v1->len * sizeof( double ) );
		if ( v2_is_int )
			vars_mod_icheck( *v2_lpnt );
		else
			vars_mod_fcheck( *v2_dpnt );
		for ( i = 0; i < v1->len; i++ )
			dp[ i ] = fmod( v1->val.dpnt[ i ],
							v2_is_int ? ( double ) *v2_lpnt : *v2_dpnt );
		new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
		T_free( dp );

		return new_var;
	}

	if ( elems != 1 && v1->len != elems )
	{
		eprint( FATAL, "%s:%ld: Sizes of array slices to be divided differ.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	dp = T_malloc( v1->len * sizeof( double ) );
	for ( i = 0; i < v1->len; i++ )
	{
		if ( v2_is_int )
			vars_mod_icheck( *v2_lpnt );
		else
			vars_mod_fcheck( *v2_dpnt );
		dp[ i ] = fmod( v1->val.dpnt[ i ],
						v2_is_int ? ( double ) *v2_lpnt++ : *v2_dpnt++ );
	}
	new_var = vars_push( FLOAT_TRANS_ARR, dp, v1->len );
	T_free( dp );

	return new_var;
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static void vars_div_icheck( long val )
{
	if ( val != 0 )
		return;
	eprint( FATAL, "%s:%ld: Division by zero.\n", Fname, Lc );
	THROW( EXCEPTION );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static void vars_div_fcheck( double val )
{
	if ( val != 0.0 )
		return;
	eprint( FATAL, "%s:%ld: Division by zero.\n", Fname, Lc );
	THROW( EXCEPTION );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static void vars_mod_icheck( long val )
{
	if ( val != 0 )
		return;
	eprint( FATAL, "%s:%ld: Modulo by zero.\n", Fname, Lc );
	THROW( EXCEPTION );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static void vars_mod_fcheck( double val )
{
	if ( val != 0.0 )
		return;
	eprint( FATAL, "%s:%ld: Modulo by zero.\n", Fname, Lc );
	THROW( EXCEPTION );
}
