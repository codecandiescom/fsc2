Var *vars_add_to_iv( Var *v1, Var *v2 )
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
					lp[ i ] = v1->val.lval + v2->val.lptr[ i ];
				new_var = vars_push( INT_TRANS_ARR, lp, elems );
				T_free( lp );
			}
			else
			{
				dp = T_malloc( elems * sizeof( double ) );
				for ( i = 0; i < elems; i++ )
					dp[ i ] = ( double ) v1->val.lval
						      + v2->val.dptr[ i ];
				new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
				T_free( dp );
			}
			return new_var;

		case INT_TRANS_ARR :
			elems = v2->len;
			lp = T_malloc( elems * sizeof( long ) );
			for ( i = 0; i < elems; i++ )
				lp[ i ] = v1->val.lval + v2->val.lptr[ i ];
			new_var = vars_push( INT_TRANS_ARR, lp, elems );
			T_free( lp );
			return new_var;

		case FLOAT_TRANS_ARR :
			elems = v2->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = ( double ) v1->val.lval + v2->val.dptr[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			T_free( dp );
			return new_var;

		case ARR_PTR :
			vars_check( v2->from, INT_ARR | FLOAT_ARR );
			elems = v2->from->sizes[ v2->from->dim - 1 ];
			if ( v2->from->type == INT_ARR )
			{
				lp = T_malloc( elems * sizeof( long ) );
				for ( i = 0; i < elems; i++ )
					lp[ i ] = v1->val.lval + * ( ( long * ) v2->val.vptr + i );
				new_var = vars_push( INT_TRANS_ARR, lp, elems );
				T_free( lp );
			}
			else
			{
				dp = T_malloc( elems * sizeof( double ) );
				for ( i = 0; i < elems; i++ )
					dp[ i ] = ( double ) v1->val.lval
						      + * ( ( double * ) v2->val.vptr + i );
				new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
				T_free( dp );
			}
			return new_var;

		default :
			assert( 1 == 0 );
	}

	return NULL;
}


Var *vars_add_to_fv( Var *v1, Var *v2 )
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
					dp[ i ] = v1->val.dval + ( double ) v2->val.lptr[ i ];
			else
				for ( i = 0; i < elems; i++ )
					dp[ i ] = v1->val.dval + v2->val.dptr[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, lp, elems );
			T_free( dp );
			return new_var;

		case INT_TRANS_ARR :
			elems = v2->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = v1->val.dval + ( double ) v2->val.lptr[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, lp, elems );
			T_free( dp );
			return new_var;

		case FLOAT_TRANS_ARR :
			elems = v2->len;
			dp = T_malloc( elems * sizeof( double ) );
			for ( i = 0; i < elems; i++ )
				dp[ i ] = v1->val.dval + v2->val.dptr[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			T_free( dp );
			return new_var;

		case ARR_PTR :
			vars_check( v2->from, INT_ARR | FLOAT_ARR );
			elems = v2->from->sizes[ v2->from->dim - 1 ];
			dp = T_malloc( elems * sizeof( double ) );
			if ( v2->from->type == INT_ARR )
				for ( i = 0; i < elems; i++ )
					dp[ i ] = v1->val.dval
						      + ( double ) ( ( long * ) v2->val.vptr )[ i ];
			else
				for ( i = 0; i < elems; i++ )
					dp[ i ] = v1->val.dval
						      + ( ( double * ) v2->val.vptr )[ i ];
			new_var = vars_push( FLOAT_TRANS_ARR, dp, elems );
			T_free( dp );
			return new_var;

		default :
			assert( 1 == 0 );
	}

	return NULL;
}


