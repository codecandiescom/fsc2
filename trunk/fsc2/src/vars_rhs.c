/*
   $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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


/* locally used functions */

static int vars_check_rhs_indices( Var **v, Var **a, int *range_count );
static Var *vars_arr_rhs_slice( Var *a, Var *v, int index_count,
								int range_count );
static void vars_arr_rhs_slice_prune( Var *nv, Var *v, Var *a, Var *end );
static void vars_fix_dims( Var *v, int max_dim );


/*--------------------------------------------------------------------------*/
/* Function is called if an element of an array or a sub-array is accessed. */
/* Prior to the call of this function vars_arr_start() has been called and  */
/* a REF_PTR type variable, pointing to the array variable, had been pushed */
/* onto the stack.                                                          */
/*--------------------------------------------------------------------------*/

Var *vars_arr_rhs( Var *v )
{
	Var *a, *cv;
	ssize_t ind;
	int index_count;
	int range_count = 0;


	/* The variable pointer this function gets passed is a pointer to the very
	   last index on the variable stack. Now we've got to work our way up in
	   the stack until we find the first non-index variable, a reference. */

	index_count = vars_check_rhs_indices( &v, &a, &range_count );

	if ( index_count == 0 )
		return a;

	if ( index_count > a->dim )
	{
		print( FATAL, "Too many indices for array '%s'.\n", a->name );
		THROW( EXCEPTION );
	}

	if ( range_count > 0 )
		return vars_arr_rhs_slice( a, v, index_count, range_count );

	/* Iterate over all indices to find the referenced element or subarray */

	for ( cv = a; cv->type & ( INT_REF | FLOAT_REF ) && v != NULL;
		  v = vars_pop( v ) )
	{
		if ( v->val.lval >= cv->len )
		{
			print( FATAL, "Invalid index for array '%s'.\n", a->name );
			THROW( EXCEPTION );
		}

		cv = cv->val.vptr[ v->val.lval ];
	}

	if ( v == NULL )
	{
		if ( cv->type == INT_ARR )
			return vars_push( cv->type, cv->val.lpnt, cv->len );
		else if ( cv->type == FLOAT_ARR )
			return vars_push( cv->type, cv->val.dpnt, cv->len );
		else
			return vars_push( cv->type, cv );
	}

	ind = v->val.lval;

	if ( ind >= cv->len )
	{
		print( FATAL, "Invalid index for array '%s'.\n", a->name );
		THROW( EXCEPTION );
	}

	vars_pop( v );

	return cv->type == INT_ARR ? vars_push( INT_VAR, cv->val.lpnt[ ind ] ) :
								 vars_push( FLOAT_VAR, cv->val.dpnt[ ind ] );
}
		

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static int vars_check_rhs_indices( Var **v, Var **a, int *range_count )
{
	Var *cv = *v;
	int index_count = 0;


	while ( cv->type != REF_PTR )
		cv = cv->prev;

	*a = cv->from;
	*v = cv = vars_pop( cv );

	/* If there are no indices or the index variable has the type UNDEF_VAR
	   the whole array has to be used */

	if ( cv != NULL && cv->type == UNDEF_VAR )
		cv = vars_pop( cv );

	if ( cv == NULL )
		return 0;

	/* Loop over all indices */

	while ( cv )
	{
		index_count++;

		/* Do some sanity checks on the index and convert its value to
		   something we can use internally */

		vars_check( cv, INT_VAR | FLOAT_VAR );

		if ( cv->type == INT_VAR )
			cv->val.lval -= ARRAY_OFFSET;
		else
		{
			if ( cv->next == NULL || cv->next->type != STR_VAR )
				print( WARN, "FLOAT value used as index for array '%s'.\n",
					   ( *a )->name );
			else
				print( WARN, "FLOAT value used as start of range for array "
					   "'%s'.\n", ( *a )->name );
			cv->val.lval = lrnd( cv->val.dval ) - ARRAY_OFFSET;
			cv->type = INT_VAR;
		}
								 
		if ( cv->val.lval < 0 )
		{
			if ( cv->next == NULL || cv->next->type != STR_VAR )
				print( FATAL, "Invalid index for array '%s'.\n",
					   ( *a )->name );
			else
				print( FATAL, "Invalid start of range for array '%s'.\n",
					   ( *a )->name );
			THROW( EXCEPTION );
		}

		/* If we're at the end of the list of indices or if the last index
		   wasn't a range start go back to the start of the loop */

		if ( ( cv = cv->next ) == NULL || cv->type != STR_VAR )
			continue;

		cv = cv->next;

		/* Do some sanity checks on the end of range value and convert it
		   to something we can use internally */

		vars_check( cv, INT_VAR | FLOAT_VAR );

		if ( cv->type == INT_VAR )
			cv->val.lval -= ARRAY_OFFSET;
		else
		{
			print( WARN, "FLOAT value used as end of range for array '%s'.\n",
				   ( *a )->name );
			cv->val.lval = lrnd( cv->val.dval ) - ARRAY_OFFSET;
			cv->type = INT_VAR;
		}
								 
		if ( cv->val.lval < 0 )
		{
			print( FATAL, "Invalid end of range for array '%s'.\n",
				   ( *a )->name );
			THROW( EXCEPTION );
		}

		/* Range end must be at least as large as the start of the range */

		if ( cv->prev->prev->val.lval > cv->val.lval )
		{
			print( FATAL, "Start of range larger than end of range for "
				   "array '%s'.\n", ( *a )->name );
			THROW( EXCEPTION );
		}

		/* If start and end of range are identical the range represents a
		   single index, so get rid of the start of range variable and the
		   intervening STRING variable. Otherwise also pop the STRING variable
		   but mark the range start index by making it negative. */

		if ( cv->val.lval == cv->prev->prev->val.lval )
		{
			print( WARN, "Start and end of range are identical for "
				   "array '%s'.\n", ( *a )->name );
			vars_pop( vars_pop( cv->prev->prev ) );
		}
		else
		{
			vars_pop( cv->prev );
			cv->prev->val.lval = - cv->prev->val.lval -1;
			*range_count += 1;
		}

		cv = cv->next;
	}

	return index_count;
}


/*-------------------------------------------------------------*/
/* Function to extract a subarray or submatrix from an array   */
/* or matrix that appears somewhere on the RHS of a statement. */
/*-------------------------------------------------------------*/

static Var *vars_arr_rhs_slice( Var *a, Var *v, int index_count,
								int range_count )
{
	Var *cv = a;


	/* Go down to the first submatrix that's indexed by a range */

	for ( ; v->val.lval >= 0; v = vars_pop( v ) )
	{
		if ( v->val.lval >= cv->len )
		{
			print( FATAL, "Invalid index for array '%s'.\n", a->name );
			THROW( EXCEPTION );
		}

		cv = cv->val.vptr[ v->val.lval ];
		index_count--;
	}
	
	/* Make a copy of it */

	if ( cv->type == INT_ARR )
		cv = vars_push( cv->type, cv->val.lpnt, cv->len );
	else if ( cv->type == FLOAT_ARR )
		cv = vars_push( cv->type, cv->val.dpnt, cv->len );
	else
		cv = vars_push_copy( cv );

	/* Remove everything from the copy not covered by ranges */

	vars_arr_rhs_slice_prune( cv, v, a, cv );

	/* We might end up with a matrix of lower dimension than we started
	   with, so we've got to fix the dim entries of all the variables
	   involved. */

	if ( index_count > range_count )
		vars_fix_dims( cv, cv->dim - index_count + range_count );

	/* Pop the remaining indices from the stack */

	while ( ( v = vars_pop( v ) ) != cv )
		/* empty */ ;

	return cv;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static void vars_arr_rhs_slice_prune( Var *nv, Var *v, Var *a, Var *end )
{
	ssize_t range_start, range_end, i;
	Var *cv;
	Var **old_vptr_list;


	CLOBBER_PROTECT( v );

	fsc2_assert( v->val.lval < 0 );

	/* Whenever we get here the next two indices make up a range. We now must
	   prune the array of either values or variable pointers of the current
	   submatrix according to these indices */

	range_start = - v->val.lval - 1;
	v = v->next;
	range_end = v->val.lval;
	v = v->next;

	if ( range_start > nv->len )
	{
		print( FATAL, "Invalid start of range for array '%s'.\n",
			   a->name );
		THROW( EXCEPTION );
	}

	if ( range_end > nv->len )
	{
		print( FATAL, "Invalid end of range for array '%s'.\n",
			   a->name );
		THROW( EXCEPTION );
	}

	if ( nv->type & ( INT_ARR | FLOAT_ARR ) )
	{
		nv->len = range_end - range_start + 1;

		if ( nv->type == INT_ARR )
		{
			if ( range_start > 0 )
				memmove( nv->val.lpnt, nv->val.lpnt + range_start,
						 nv->len * sizeof *nv->val.lpnt );
			nv->val.lpnt = T_realloc( nv->val.lpnt,
									  nv->len * sizeof *nv->val.lpnt );
		}
		else
		{
			if ( range_start > 0 )
				memmove( nv->val.dpnt, nv->val.dpnt + range_start,
						 nv->len * sizeof *nv->val.dpnt );
			nv->val.dpnt = T_realloc( nv->val.dpnt,
									  nv->len * sizeof *nv->val.dpnt );
		}
	}
	else
	{
		for ( i = 0; i < range_start; i++ )
			vars_free( nv->val.vptr[ i ], SET );

		for ( i = range_end + 1; i < nv->len; i++ )
			vars_free( nv->val.vptr[ i ], SET );

		nv->len = range_end - range_start + 1;
		if ( range_start > 0 )
			memmove( nv->val.vptr, nv->val.vptr + range_start,
					 nv->len * sizeof *nv->val.vptr );
		nv->val.vptr = T_realloc( nv->val.vptr,
								  nv->len * sizeof *nv->val.vptr );

		/* If we're not already at the end of the list of indices and the
		   next indices make up a range again we've got to prune all the
		   submatrices of the current submatrix recursively */

		if ( v != end && v->val.lval < 0 )
		{
			for ( i = 0; i < nv->len; i++ )
				vars_arr_rhs_slice_prune( nv->val.vptr[ i ], v, a, end );
			return;
		}
	}

	/* If we get here we have an index and not a range and the submatrix we're
	   operating on has at least two dimensions. */

	while ( v != end && v->val.lval >= 0 )
	{
		fsc2_assert( nv->type & ( INT_REF | FLOAT_REF ) );

		if ( ! ( nv->val.vptr[ 0 ]->type & ( INT_ARR | FLOAT_ARR ) ) )
		{
			/* Throw away all subarrays that aren't indexed */

			for ( i = 0; i < nv->len; i++ )
				if ( i != v->val.lval )
					vars_free( nv->val.vptr[ i ], SET );

			/* Delete the old 'vptr' array and replace it by the one of the
			   indexed element one level down */

			cv = nv->val.vptr[ v->val.lval ];

			T_free( nv->val.vptr );
			nv->val.vptr = cv->val.vptr;

			/* Adjust the length of the current level and make all subarrays
			   'from' fields to it */

			nv->len = cv->len;
			for ( i = 0; i < nv->len; i++ )
				nv->val.vptr[ i ]->from = nv;

			/* Finally throw away the variable representing the former
			   intermediate level (but not its still linked subarrays, they
			   now belong to the current level!) */

			cv->flags |= DONT_RECURSE;
			vars_pop( cv );
		}
		else
		{
			/* If we're on the lowest level pick the indexed elements of all
			   the subarrays */

			for ( i = 0; i < nv->len; i++ )
				if ( v->val.lval >= nv->val.vptr[ i ]->len )
				{
					print( FATAL, "Invalid index for array '%s'.\n", a->name );
					THROW( EXCEPTION );
				}

			old_vptr_list = nv->val.vptr;

			if ( nv->val.vptr[ 0 ]->type == INT_ARR )
			{
				TRY
				{
					nv->val.lpnt = T_malloc( nv->len * sizeof *nv->val.lpnt );
					TRY_SUCCESS;
				}
				OTHERWISE
				{
					nv->val.vptr = old_vptr_list;
					RETHROW( );
				}

				for ( i = 0; i < nv->len; i++ )
				{
					nv->val.lpnt[ i ] =
								   old_vptr_list[ i ]->val.lpnt[ v->val.lval ];
					vars_free( old_vptr_list[ i ], SET );
				}

				nv->type = INT_ARR;
			}
			else
			{
				TRY
				{
					nv->val.dpnt = T_malloc( nv->len * sizeof *nv->val.dpnt );
					TRY_SUCCESS;
				}
				OTHERWISE
				{
					nv->type = FLOAT_REF;
					nv->val.vptr = old_vptr_list;
					RETHROW( );
				}

				for ( i = 0; i < nv->len; i++ )
				{
					nv->val.dpnt[ i ] =
								   old_vptr_list[ i ]->val.dpnt[ v->val.lval ];
					vars_free( old_vptr_list[ i ], SET );
				}

				nv->type = FLOAT_ARR;
			}

			T_free( old_vptr_list );
		}

		v = v->next;
	}

	if ( v != end )
		vars_arr_rhs_slice_prune( nv, v, a, end );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static void vars_fix_dims( Var *v, int max_dim )
{
	ssize_t i;


	v->dim = max_dim;

	if ( --max_dim == 0 )
		return;

	for ( i = 0; i < v->len; i++ )
		vars_fix_dims( v->val.vptr[ i ], max_dim );
}


/*----------------------------------------------------------------------*/
/* This function is needed when on the left hand side of a statement an */
/* array is specified with ranges and it's also going to be needed on   */
/* the right hand side (because an operator like "+=" is used). Then we */
/* have to convert the SUB_REF_PTR from the LHS into an array usuable   */
/* on the RHS which this function does.                                 */
/*----------------------------------------------------------------------*/

Var *vars_subref_to_rhs_conv( Var *v )
{
	Var *sv, *nv;
	ssize_t i;
	int range_count = 0;


	sv = vars_push( INT_VAR, ( long ) v->val.index[ 0 ] );
	if ( v->val.index[ 0 ] < 0 )
		range_count++;

	for ( i = 1; i < v->len; i++ )
	{
		vars_push( INT_VAR, ( long ) v->val.index[ i ] );

		if ( v->val.index[ i ] < 0 )
			range_count++;
	}

	nv = vars_arr_rhs_slice( v->from, sv, v->len - range_count,
							 range_count );

	return nv;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
