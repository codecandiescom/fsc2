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


static int vars_check_lhs_indices( Var **v, int *range_count );
static Var *vars_setup_new_array( Var *v, int dim );
static Var *vars_init_elements( Var *a, Var *v );
static void vars_do_init( Var *src, Var *dest );
static Var *vars_lhs_pointer( Var *v, int dim );
static Var *vars_lhs_sub_pointer( Var *v, int dim, int range_count );
static Var *vars_lhs_simple_pointer( Var *a, Var *cv, Var *v, int dim );
static Var *vars_lhs_range_pointer( Var *a, Var *cv, Var *v, int dim );
static void vars_assign_to_1d( Var *src, Var *dest );
static void vars_assign_to_nd( Var *src, Var *dest );
static void vars_assign_to_nd_from_1d( Var *src, Var *dest );
static void vars_assign_to_snd_from_1d( Var *src, Var *dest, Var *sub );
static long vars_assign_to_snd_range_from_1d( Var *dest, Var *subm,
											  ssize_t cur, long l, double d );
static long vars_set_all( Var *v, long l, double d );
static void vars_assign_to_nd_from_nd( Var *src, Var *dest );
static void vars_size_check( Var *src, Var *dest );
static void vars_assign_to_snd_from_nd( Var *src, Var *dest, Var *sub );
static void vars_assign_snd_range_from_nd( Var *dest, Var *sub, ssize_t cur,
										   Var *src );
static void vars_arr_assign( Var *src, Var *dest );


/*----------------------------------------------------------------------*/
/* The function is called when the end of an array access (indicated by */
/* the ']') is found on the left hand side of an assignment. If it is   */
/* called for a new array the indices found on the stack are the sizes  */
/* for the different dimensions of the array and are used to set up the */
/* the array.                                                           */
/*----------------------------------------------------------------------*/

Var *vars_arr_lhs( Var *v )
{
	int dim;
	int range_count = 0;


	/* Move up the stack until we find the variable indicating the array or
	   matrix itself */

	dim = vars_check_lhs_indices( &v, &range_count );

	/* If the array is new (i.e. we're still in the VARIABLES section) set
	   it up, otherwise return a pointer to the referenced element */

	if ( v->from->type == UNDEF_VAR )
		return vars_setup_new_array( v, dim );
	else if ( range_count == 0 )
		return vars_lhs_pointer( v, dim );
	else
		return vars_lhs_sub_pointer( v, dim, range_count );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static int vars_check_lhs_indices( Var **v, int *range_count )
{
	Var *cv = *v;
	Var *ref;
	int index_count = 0;


	while ( cv->type != REF_PTR )
		cv = cv->prev;

	*v = cv;
	ref = cv->from;

	/* Loop over all indices */

	for ( cv = cv->next; cv != NULL; cv = cv->next )
	{
		index_count++;

		/* Do some sanity checks on the index and convert its value to
		   something we can use internally */

		vars_check( cv, INT_VAR | FLOAT_VAR );

		if ( ref->type != UNDEF_VAR )
		{
			if ( cv->type == INT_VAR )
				cv->val.lval -= ARRAY_OFFSET;
			else
			{
				if ( cv->next == NULL || cv->next->type != STR_VAR )
					print( WARN, "FLOAT value used as index for array '%s'.\n",
						   ref->name );
				else
					print( WARN, "FLOAT value used as start of range for "
						   "array '%s'.\n", ref->name );
				cv->val.lval = lrnd( cv->val.dval ) - ARRAY_OFFSET;
				cv->type = INT_VAR;
			}
								 
			if ( cv->val.lval < 0 )
			{
				if ( cv->next == NULL || cv->next->type != STR_VAR )
					print( FATAL, "Invalid index for array '%s'.\n",
						   ref->name );
				else
					print( FATAL, "Invalid start of range for array '%s'.\n",
						   ref->name );
				THROW( EXCEPTION );
			}
		}
		else
		{
			if ( cv->type == FLOAT_VAR )
			{
				print( WARN, "FLOAT value used as size array.\n" );
				cv->val.lval = lrnd( cv->val.dval ) - ARRAY_OFFSET;
				cv->type = INT_VAR;
			}

			if ( ( ! ( cv->flags & IS_DYNAMIC ) && cv->val.lval < 1 ) ||
				 cv->val.lval < 0 )
			{
				print( FATAL, "Invalid size for array.\n" );
				THROW( EXCEPTION );
			}

			continue;
		}

		/* If we're at the end of the list of indices or if the last index
		   wasn't a range start go back to the start of the loop */

		if ( cv->next == NULL || cv->next->type != STR_VAR )
			continue;

		cv = cv->next->next;

		/* Do some sanity checks on the end of range value and convert it
		   to something we can use internally */

		vars_check( cv, INT_VAR | FLOAT_VAR );

		if ( cv->type == INT_VAR )
			cv->val.lval -= ARRAY_OFFSET;
		else
		{
			print( WARN, "FLOAT value used as end of range for array '%s'.\n",
				   ref->name );
			cv->val.lval = lrnd( cv->val.dval ) - ARRAY_OFFSET;
			cv->type = INT_VAR;
		}
								 
		if ( cv->val.lval < 0 )
		{
			print( FATAL, "Invalid end of range for array '%s'.\n",
				   ref->name );
			THROW( EXCEPTION );
		}

		/* Range end must be at least as large as the start of the range */

		if ( cv->prev->prev->val.lval > cv->val.lval )
		{
			print( FATAL, "Start of range larger than end of range for "
				   "array '%s'.\n", ref->name );
			THROW( EXCEPTION );
		}

		/* If start and end of range are identical the range represents a
		   single index, so get rid of the start of range variable and the
		   intervening STRING variable. Otherwise also pop the STRING variable
		   but mark the range start index by making it negative. */

		if ( cv->val.lval == cv->prev->prev->val.lval )
		{
			print( WARN, "Start and end of range are identical for "
				   "array '%s'.\n", ref->name );
			vars_pop( vars_pop( cv->prev->prev ) );
		}
		else
		{
			vars_pop( cv->prev );
			cv->prev->val.lval = - cv->prev->val.lval -1;
			*range_count += 1;
		}
	}

	return index_count;
}


/*-----------------------------------------------------------------------*/
/* Function is called when the closing ']' is found in the definition of */
/* an array in the VARIABLES section. It creates the array as far as     */
/*  possible (i.e. if the sizes are specified including the sub-arrays). */
/*-----------------------------------------------------------------------*/

static Var* vars_setup_new_array( Var *v, int dim )
{
	Var *a = v->from;


	/* We can't continue without indices, i.e. with a definition like "a[ ]" */

	if ( v->next->type == UNDEF_VAR )
	{
		print( FATAL, "Missing sizes in definition of array '%s'.\n",
			   v->from->name );
		THROW( EXCEPTION );
	}

	/* Set the arrays type and create it as far as possible */

	a->type = VAR_TYPE( a ) == INT_VAR ? INT_REF : FLOAT_REF;

	vars_arr_create( a, v->next, dim, UNSET );

	/* Get rid of variables specifying the sizes that aren't needed anymore */

	while ( ( v = vars_pop( v ) ) != NULL )
		/* empty */ ;

	/* Finally push a marker on the stack to be used in array initialization */

	return vars_push( REF_PTR, a );
}


/*----------------------------------------------------------------------*/
/* Function creates a new array by creating recursively all sub-arrays. */
/*----------------------------------------------------------------------*/

void vars_arr_create( Var *a, Var *v, int dim, bool is_temp )
{
	Var *c;
	ssize_t i;
	ssize_t len;


	a->dim    = dim;
	a->flags &= ~ NEW_VARIABLE;

	vars_check( v, INT_VAR | FLOAT_VAR );

	/* If the size is not defined the whole of the rest of the array must be
	   dynamically sized and can be only set up by an assignment sometime
	   later. */

	if ( v->flags & IS_DYNAMIC )
	{
		a->len = 0;
		a->flags |= IS_DYNAMIC;

		/* If we're dealing with the last index we just need to adjust the
		   variable type */

		if ( a->dim == 1 )
		{
			a->type = a->type == INT_REF ? INT_ARR : FLOAT_ARR;
			return;
		}

		/* Otherwise check that all the following sizes are also dynamically
		   sized */

		for ( c = v->next; c != NULL; c = c->next )
			if ( ! ( c->flags & IS_DYNAMIC ) )
			{
				print( FATAL, "Fixed array size after a dynamically set "
					   "size.\n" );
				THROW( EXCEPTION );
			}

		return;
	}

	/* Determine the requested size */

	if ( v->type == INT_VAR )
		len = v->val.lval;
	else
	{
		print( WARN, "FLOAT value used as size of array.\n" );
		len = lrnd( v->val.dval );
	}

	if ( len < 1 )
	{
		print( FATAL, "Invalid size for array.\n" );
		THROW( EXCEPTION );
	}

	/* The current dimension is obviously not dynamically sized or we
	   wouldn't have gotten here */

	a->flags &= ~ IS_DYNAMIC;

	/* If this is the last dimension allocate memory (initialized to 0) for
	   a data array */

	if ( a->dim == 1 )
	{
		a->type = a->type == INT_REF ? INT_ARR : FLOAT_ARR;
		if ( a->type == INT_ARR )
		{
			a->val.lpnt = LONG_P T_malloc( len * sizeof *a->val.lpnt );
			for ( i = 0; i < len; i++ )
				a->val.lpnt[ i ] = 0;
		}
		else
		{
			a->val.dpnt = DOUBLE_P T_malloc( len * sizeof *a->val.dpnt );
			for ( i = 0; i < len; i++ )
				a->val.dpnt[ i ] = 0.0;
		}

		a->len = len;
		return;
	}
	
	/* Otherwise we need an array of references to arrays of lower dimensions
	   which then in turn must be created */

	a->val.vptr = VAR_PP T_malloc( len * sizeof *a->val.vptr );

	for ( i = 0; i < len; i++ )
		a->val.vptr[ i ] = NULL;

	a->len = len;

	for ( i = 0; i < a->len; i++ )
	{
		a->val.vptr[ i ] = vars_new( NULL );
		a->val.vptr[ i ]->type  = a->type;
		a->val.vptr[ i ]->from  = a;
		if ( is_temp )
			a->val.vptr[ i ]->flags |= IS_TEMP;
		vars_arr_create( a->val.vptr[ i ], v->next, dim - 1, is_temp );
	}
}


/*---------------------------------------------------------------------*/
/* Function gets called when a list of initializers (enclosed in curly */
/* braces) gets found in the input of the VARIABLES section. Since the */
/* initializer list can contain further list etc. the function can get */
/* several times until we have reached the highest level list. During  */
/* this process the variable stack can become quite messed up and for  */
/* bookkeeping purposes two extra flags, INIT_ONLY and DONT_RECURSE,   */
/* are used with the stack variables involved. INIT_ONLY signifies     */
/* that the variable is on the stack for good reasons and DONT_RECURSE */
/* tells the functions for popping variables not to delete sub-arrays  */
/* of the variable.                                                    */
/*---------------------------------------------------------------------*/

Var *vars_init_list( Var *v, ssize_t level )
{
	ssize_t count = 0;
	ssize_t i;
	Var *cv, *nv;
	int type = INT_VAR;


	CLOBBER_PROTECT( v );
	CLOBBER_PROTECT( nv );

	/* Find the start of the of list initializers, marked by a variable of
	   type REF_PTR */

	while ( v->type != REF_PTR )
		v = v->prev;

	v = vars_pop( v );

	/* Variables of lower dimensions than the one required here are left-
	   overs from previous calls, they are still needed for the final
	   assignment of the initialization data to the RHS array. */

	while ( v != NULL && v->dim < level && v->flags & INIT_ONLY )
		v = v->next;

	if ( v == NULL || v->dim < level )
	{
		print( FATAL, "Syntax error in initializer list (possibly a missing "
			   "set of curly braces).\n" );
		THROW( EXCEPTION );
	}

	/* Count the number of initializers, skipping variables with too low
	   a dimension (if they are left over from previous calls) and
	   complaining about variables with too high a dimensions */

	for ( cv = v; cv != NULL; cv = cv->next )
	{
		if ( cv->dim > level )
		{
			print( FATAL, "Object with too high a dimension in initializer "
				   "list.\n" );
			THROW( EXCEPTION );
		}
		else if ( cv->dim == level )
		{
			count++;
			if ( level == 0 && cv->type == FLOAT_VAR )
				type = FLOAT_VAR;
		}
		else if ( ! ( cv->flags & INIT_ONLY ) )
		{
			print( FATAL, "Syntax error in initializer list (possibly a "
				   "missing set of curly braces).\n" );
			THROW( EXCEPTION );
		}
	}

	/* If there's only a variable of UNDEF_TYPE this means we got just an
	   empty list */

	if ( count == 1 && v->type == UNDEF_VAR )
	{
		v->dim++;
		return v;
	}

	/* If we're at the lowest level (i.e. at the level of simple integer or
	   floating point variables) make an array out of them */

	if ( level == 0 )
	{
		if ( type == INT_VAR )
		{
			nv = vars_push( INT_ARR, NULL, count );
			nv->flags |= INIT_ONLY;
			nv->val.lpnt = LONG_P T_malloc( nv->len * sizeof *nv->val.lpnt );
			for ( i = 0; i < nv->len; i++, v = vars_pop( v ) )
				nv->val.lpnt[ i ] = v->val.lval;
		}
		else
		{
			nv = vars_push( FLOAT_ARR, NULL, count );
			nv->flags |= INIT_ONLY;
			nv->val.dpnt = DOUBLE_P T_malloc( nv->len * sizeof *nv->val.dpnt );
			for ( i = 0; i < nv->len; i++, v = vars_pop( v ) )
				if ( v->type == INT_VAR )
					nv->val.dpnt[ i ] = ( double ) v->val.lval;
				else
					nv->val.dpnt[ i ] = v->val.dval;
		}

		return nv;
	}

	/* Otherwise create an array of a dimension one higher than the current
	   level, set flag of variable to avoid that popping the variable also
	   deletes the sub-arrays it's pointing to */

	nv = vars_push( FLOAT_REF, NULL );
	nv->flags |= DONT_RECURSE | INIT_ONLY;
	nv->dim = v->dim + 1;
	nv->len = count;

	TRY
	{
		nv->val.vptr = VAR_PP T_malloc( nv->len * sizeof *nv->val.vptr );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		nv->len = 0;
		RETHROW( );
	}

	for ( i = 0; i < nv->len; i++ )
		nv->val.vptr[ i ] = NULL;

	for ( i = 0; i < nv->len; v = v->next )
	{
		if ( v->dim != level )
			continue;

		v->flags |= INIT_ONLY;

		if ( v->type == UNDEF_VAR )
		{
			nv->val.vptr[ i ] = vars_new( NULL );
			nv->val.vptr[ i ]->flags |= DONT_RECURSE | INIT_ONLY;
			nv->val.vptr[ i ]->dim = nv->dim - 1;
			nv->val.vptr[ i ]->from = nv;
			if ( nv->dim > 2 )
				nv->val.vptr[ i ]->type = nv->type;
			else
				nv->val.vptr[ i ]->type = FLOAT_ARR;
			nv->val.vptr[ i ]->len  = 0;
		}
		else
			nv->val.vptr[ i ] = v;

		nv->val.vptr[ i++ ]->from = nv;
	}

	return nv;
}


/*--------------------------------------------------------------------------*/
/* Function initializes a new array. When called the stack at 'v' contains  */
/* all the data for the initialization (the last data on top of the stack)  */
/* and, directly below the data, an REF_PTR to the array to be initialized. */
/* If 'v' is NULL no initialization has to be done.                         */
/*--------------------------------------------------------------------------*/

void vars_arr_init( Var *v )
{
	Var *dest;


	/* If there aren't any initializers we get v being set to NULL. All we
	   need to do is to clear up the variables stack that still contains
	   reference to the new array */

	if ( v == NULL )
	{
		vars_del_stack( );
		return;
	}

	/* Find the reference to the variable to be initialized */

	for ( dest = v; dest->prev != NULL; dest = dest->prev )
		/* empty */ ;

	dest = dest->from;

	/* If there are no initialization data this is indicated by a variable
	   of type UNDEF_VAR - just pop it as well as the array pointer */

	if ( v->type == UNDEF_VAR && v->dim == dest->dim )
	{
		vars_del_stack( );
		return;
	}

	if ( v->dim != dest->dim )
	{
		print( FATAL, "Dimension of variable '%s' and initializer list "
			   "differs.\n", dest->name );
		vars_del_stack( );
		THROW( EXCEPTION );
	}

	vars_do_init( v, dest );
	vars_del_stack( );
}


/*------------------------------------------------------------------*/
/* Function does the assignment of data from an initializer list,   */
/* being distributed over the stack to the left hand side variable. */
/*------------------------------------------------------------------*/

static void vars_do_init( Var *src, Var *dest )
{
	ssize_t i;


	switch ( dest->type )
	{
		case INT_ARR :
			if ( dest->flags & IS_DYNAMIC )
			{
				dest->len = src->len;
				dest->val.lpnt = LONG_P T_malloc( dest->len
												  * sizeof *dest->val.lpnt );
			}
			else if ( src->len > dest->len )
				print( WARN, "Superfluous initialization data.\n" );

			if ( src->type == INT_ARR )
				memcpy( dest->val.lpnt, src->val.lpnt,
						( dest->len < src->len ? dest->len : src->len )
						* sizeof *dest->val.lpnt );
			else
			{
				print( WARN, "Initialization of integer array with floating "
					   "point values.\n" );
				for ( i = 0; i < ( dest->len < src->len ?
								   dest->len : src->len ); i++ )
					dest->val.lpnt[ i ] = ( long ) src->val.dpnt[ i ];
			}

			for ( i = src->len; i < dest->len; i++ )
				dest->val.lpnt[ i ] = 0;

			return;

		case FLOAT_ARR :
			if ( dest->flags & IS_DYNAMIC )
			{
				dest->len = src->len;
				dest->val.dpnt = DOUBLE_P T_malloc( dest->len
													* sizeof *dest->val.dpnt );
			}
			else if ( src->len > dest->len )
				print( WARN, "Superfluous initialization data.\n" );

			if ( src->type == FLOAT_ARR )
				memcpy( dest->val.dpnt, src->val.dpnt,
						( dest->len < src->len ? dest->len : src->len )
						* sizeof *dest->val.dpnt );
			else
				for ( i = 0; i < ( dest->len < src->len ?
								   dest->len : src->len ); i++ )
					dest->val.dpnt[ i ] = ( double ) src->val.lpnt[ i ];

			for ( i = src->len; i < dest->len; i++ )
				dest->val.dpnt[ i ] = 0.0;

			return;

		case INT_REF :
			if ( dest->flags & IS_DYNAMIC )
			{
				dest->val.vptr = VAR_PP T_malloc( src->len
												  * sizeof *dest->val.vptr );
				for ( ; dest->len < src->len; dest->len++ )
					dest->val.vptr[ dest->len ] = NULL;
			}
			else if ( src->len > dest->len )
				print( WARN, "Superfluous initialization data.\n" );

			for ( i = 0; i < ( dest->len < src->len ?
							   dest->len : src->len ); i++ )
			{
				if ( dest->val.vptr[ i ] == NULL )
				{
					dest->val.vptr[ i ] = vars_new( NULL );
					dest->val.vptr[ i ]->from = dest;
					if ( dest->flags & IS_DYNAMIC )
						dest->val.vptr[ i ]->flags |= IS_DYNAMIC;
					dest->val.vptr[ i ]->dim = dest->dim - 1;
					if ( dest->dim > 2 )
						dest->val.vptr[ i ]->type = INT_REF;
					else
						dest->val.vptr[ i ]->type = INT_ARR;
				}

				if ( src->val.vptr[ i ] != NULL )
					vars_do_init( src->val.vptr[ i ], dest->val.vptr[ i ] );
			}
			return;

		case FLOAT_REF :
			if ( dest->flags & IS_DYNAMIC )
			{
				dest->val.vptr = VAR_PP T_malloc( src->len
												  * sizeof *dest->val.vptr );
				for ( ; dest->len < src->len; dest->len++ )
					dest->val.vptr[ dest->len ] = NULL;
			}
			else if ( src->len > dest->len )
				print( WARN, "Superfluous initialization data.\n" );

			for ( i = 0; i < ( dest->len < src->len ?
							   dest->len : src->len ); i++ )
			{
				if ( dest->val.vptr[ i ] == NULL )
				{
					dest->val.vptr[ i ] = vars_new( NULL );
					dest->val.vptr[ i ]->from = dest;
					if ( dest->flags & IS_DYNAMIC )
						dest->val.vptr[ i ]->flags |= IS_DYNAMIC;
					dest->val.vptr[ i ]->dim = dest->dim - 1;
					if ( dest->dim > 2 )
						dest->val.vptr[ i ]->type = FLOAT_REF;
					else
						dest->val.vptr[ i ]->type = FLOAT_ARR;
				}

				if ( src->val.vptr[ i ] != NULL )
					vars_do_init( src->val.vptr[ i ], dest->val.vptr[ i ] );
			}
			return;
	}

	fsc2_assert( 1 == 0 );
}


/*--------------------------------------------------------------------*/
/* Initializes the elements of array or matrix 'a' with the values of */
/* the variables on the stack (first of which is 'v').                */
/*--------------------------------------------------------------------*/

static Var *vars_init_elements( Var *a, Var *v )
{
	ssize_t i;


	if ( v == NULL )
		return NULL;

	if ( a->dim == 1 )
	{
		for ( i = 0; i < a->len; i++ )
		{
			if ( a->type == INT_VAR )
			{
				if ( v->type == INT_VAR )
					a->val.lpnt[ i ] = v->val.lval;
				else
				{
					print( WARN, "Floating point value used in initialization "
						   "of integer array.\n" );
					a->val.lpnt[ i ] = ( long ) v->val.dpnt;
				}
			}
			else
			{
				if ( v->type == INT_VAR )
					a->val.dpnt[ i ] = v->val.lval;
				else
					a->val.dpnt[ i ] = v->val.dval;
			}

			if ( ( v = vars_pop( v ) ) == NULL )
				return NULL;
		}
	}
	else
		for ( i = 0; v != NULL && i < a->len; i++ )
			v = vars_init_elements( a->val.vptr[ i ], v );

	return v;
}


/*----------------------------------------------------------------------*/
/* Function pushes a variable with a pointer to an array element on the */
/* left hand side of an EDL statement onto the stack. Depending on what */
/* the left hand side evaluates to this is either a variable of INT_PTR */
/* or FLOAT_PTR type (when a singe element of an array or matrix is     */
/* addressed) or of type REF_PTR (if the LHS resolves to an array or    */
/* (sub-) matrix).                                                      */
/*----------------------------------------------------------------------*/

static Var *vars_lhs_pointer( Var *v, int dim )
{
	Var *a = v->from;
	Var *cv;


	v = vars_pop( v );

	if ( v->type == UNDEF_VAR )
	{
#ifdef NDEBUG
		vars_pop( v );
#else
		if ( ( v = vars_pop( v ) ) != NULL )
		{
			eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
					__FILE__, __LINE__ );
#endif
			THROW( EXCEPTION );
		}
		return vars_push( REF_PTR, a );
	}

	/* Check that there aren't too many indices */

	if ( dim > a->dim )
	{
		print( FATAL, "Array '%s' has only %d dimensions but there are "
			   "%d indices.\n", a->dim, dim );
		THROW( EXCEPTION );
	}

	cv = vars_lhs_simple_pointer( a, a, v, a->dim );

	while ( ( v = vars_pop( v ) ) != cv )
		/* empty */ ;

	return cv;
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static Var *vars_lhs_sub_pointer( Var *v, int dim, int range_count )
{
	Var *a = v->from;
	Var *sv;
	ssize_t i;


	v = vars_pop( v );	

	/* Check that there aren't too many indices or ranges */

	if ( dim > a->dim )
	{
		print( FATAL, "Array '%s' has only %d dimensions but there are "
			   "%d indices/ranges.\n", a->dim, dim );
		THROW( EXCEPTION );
	}

	if ( v->val.lval >= 0 )
		vars_pop( vars_lhs_simple_pointer( a, a, v, a->dim ) );
	else
		vars_pop( vars_lhs_range_pointer( a, a, v, a->dim ) );

	sv = vars_push( SUB_REF_PTR, dim + range_count );
	sv->from = a;

	for ( i = 0; v != sv; i++, v = vars_pop( v ) )
		sv->val.index[ i ] = v->val.lval;

	return sv;
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static Var *vars_lhs_simple_pointer( Var *a, Var *cv, Var *v, int dim )
{
	ssize_t ind;
	ssize_t i;


	ind = v->val.lval;

	/* If the index is larger than the length of the array this is a
	   fatal error for a fixed length array, but for a dynamically
	   sized array it simply means we have to extend it */

	if ( ind >= cv->len )
	{
		if ( ! ( cv->flags & IS_DYNAMIC ) )
		{
			print( FATAL, "Invalid index for array '%s'.\n", a->name );
			THROW( EXCEPTION );
		}

		if ( dim > 1 )
		{
			cv->val.vptr = VAR_PP T_realloc( cv->val.vptr,
										  ( ind + 1 ) * sizeof *cv->val.vptr );

			for ( i = cv->len; i <= ind; i++ )
			{
				cv->val.vptr[ i ]           = vars_new( NULL );
				cv->val.vptr[ i ]->from     = cv;
				if ( dim > 2 )
					cv->val.vptr[ i ]->type = cv->type;
				else
					cv->val.vptr[ i ]->type = 
									 cv->type == INT_REF ? INT_ARR : FLOAT_ARR;
				cv->val.vptr[ i ]->dim      = dim - 1;
				cv->val.vptr[ i ]->len      = 0;
				cv->val.vptr[ i ]->flags   |= IS_DYNAMIC;
				cv->val.vptr[ i ]->flags   &= ~ NEW_VARIABLE;
			}
		}
		else
		{
			switch ( cv->type )
			{
				case INT_ARR :
					cv->val.lpnt = LONG_P T_realloc( cv->val.lpnt,
										  ( ind + 1 ) * sizeof *cv->val.lpnt );
					for ( i = cv->len; i <= ind; i++ )
						cv->val.lpnt[ i ] = 0;
					break;

				case FLOAT_ARR :
					cv->val.dpnt = DOUBLE_P T_realloc( cv->val.dpnt,
										  ( ind + 1 ) * sizeof *cv->val.dpnt );
					for ( i = cv->len; i <= ind; i++ )
						cv->val.dpnt[ i ] = 0.0;
					break;

				default :
					fsc2_assert( 1 == 0 );
			}
		}

		cv->len = ind + 1;
	}

	if ( v->next == NULL || v->next->type != INT_VAR )
		switch ( cv->type )
		{
			case INT_ARR :
				return vars_push( INT_PTR, cv->val.lpnt + ind );

			case FLOAT_ARR :
				return vars_push( FLOAT_PTR, cv->val.dpnt + ind );

			case INT_REF : case FLOAT_REF :
				return vars_push( REF_PTR, cv->val.vptr[ ind ] );

			default :
				fsc2_assert( 1 == 0 );
		}

	v = v->next;

	if ( v->val.lval >= 0 )
		return vars_lhs_simple_pointer( a, cv->val.vptr[ ind ], v, --dim );

	return vars_lhs_range_pointer( a, cv->val.vptr[ ind ], v, --dim );
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static Var *vars_lhs_range_pointer( Var *a, Var *cv, Var *v, int dim )
{
	ssize_t i, range_start, range_end;


	range_start = - v->val.lval - 1;
	v = v->next;
	range_end = v->val.lval;
	v = v->next;

	if ( ! ( cv->flags & IS_DYNAMIC ) && range_end >= cv->len )
	{
		print( FATAL, "Invalid range for array '%s'.\n", a->name );
		THROW( EXCEPTION );
	}
	else if ( range_end >= cv->len )
	{
		if ( dim > 1 )
		{
			cv->val.vptr = VAR_PP T_realloc( cv->val.vptr,
									( range_end + 1 ) * sizeof *cv->val.vptr );

			for ( i = cv->len; i <= range_end; i++ )
			{
				cv->val.vptr[ i ]           = vars_new( NULL );
				cv->val.vptr[ i ]->from     = cv;
				if ( dim > 2 )
					cv->val.vptr[ i ]->type = cv->type;
				else
					cv->val.vptr[ i ]->type = 
									 cv->type == INT_REF ? INT_ARR : FLOAT_ARR;
				cv->val.vptr[ i ]->dim      = dim - 1;
				cv->val.vptr[ i ]->len      = 0;
				cv->val.vptr[ i ]->flags   |= IS_DYNAMIC;
				cv->val.vptr[ i ]->flags   &= ~ NEW_VARIABLE;
			}
		}
		else
		{
			switch ( cv->type )
			{
				case INT_ARR :
					cv->val.lpnt = LONG_P T_realloc( cv->val.lpnt,
									( range_end + 1 ) * sizeof *cv->val.lpnt );
					for ( i = cv->len; i <= range_end; i++ )
						cv->val.lpnt[ i ] = 0;
					break;

				case FLOAT_ARR :
					cv->val.dpnt = DOUBLE_P T_realloc( cv->val.dpnt,
									( range_end + 1 ) * sizeof *cv->val.dpnt );
					for ( i = cv->len; i <= range_end; i++ )
						cv->val.dpnt[ i ] = 0.0;
					break;

				default :
					fsc2_assert( 1 == 0 );
			}
		}

		cv->len = range_end + 1;
	}

	if ( v == NULL )
		return vars_push( INT_VAR, 0 );

	if ( v->val.lval >= 0 )
		for ( i = range_start; i <= range_end; i++ )
			vars_pop( vars_lhs_simple_pointer( a, cv->val.vptr[ i ],
											   v, dim - 1 ) );
	else
		for ( i = range_start; i <= range_end; i++ )
			vars_pop( vars_lhs_range_pointer( a, cv->val.vptr[ i ], v,
											  dim - 1 ) );

	return vars_push( INT_VAR, 0 );
}


/*--------------------------------------------------------*/
/* This is the central function for assigning new values. */
/*--------------------------------------------------------*/

void vars_assign( Var *src, Var *dest )
{
#ifndef NDEBUG
	/* Check that both variables exist and src has a reasonable type */

	if ( ! vars_exist( src ) || ! vars_exist( dest ) )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	if ( src->type == STR_VAR )
	{
		print( FATAL, "Variable of type STRING can't be used in this "
			   "context.\n" );
		THROW( EXCEPTION );
	}

	if ( ! ( src->type & ( INT_VAR | FLOAT_VAR |
						   INT_ARR | FLOAT_ARR |
						   INT_REF | FLOAT_REF ) ) )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	/* First we distinguish between the different possible types of variables
	   on the left hand side */

	switch ( dest->type )
	{
		case UNDEF_VAR :
		case INT_VAR : case FLOAT_VAR :
		case INT_PTR : case FLOAT_PTR :
			vars_assign_to_1d( src, dest );
			break;

		case INT_ARR : case FLOAT_ARR :
		case INT_REF : case FLOAT_REF :
		case SUB_REF_PTR: case REF_PTR :
			vars_assign_to_nd( src, dest );
			break;

#ifndef NDEBUG
		default :
			eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
#endif
	}

	vars_pop( dest );
	vars_pop( src );

	return;
}


/*----------------------------------------------------------*/
/* Assignment to a variable or a single element of an array */
/*----------------------------------------------------------*/

static void vars_assign_to_1d( Var *src, Var *dest )
{
	/* If the destination variable is still completely new set its type */

	if ( dest->type == UNDEF_VAR )
	{
		dest->type = VAR_TYPE( dest );
		dest->flags &= ~NEW_VARIABLE;
	}

	if ( src->type & ( INT_ARR | FLOAT_ARR ) )
	{
		print( FATAL, "Can't assign array to a simple variable.\n" );
		THROW( EXCEPTION );
	}

	if ( src->type & ( INT_REF | FLOAT_REF ) )
	{
		print( FATAL, "Can't assign matrix to a simple variable.\n" );
		THROW( EXCEPTION );
	}

	switch ( src->type )
	{
		case INT_VAR :
			switch ( dest->type )
			{
				case INT_VAR :
					dest->val.lval = src->val.lval;
					break;

				case INT_PTR :
					*dest->val.lpnt = src->val.lval;
					break;

				case FLOAT_VAR :
					dest->val.dval = ( double ) src->val.lval;
					break;

				case FLOAT_PTR :
					*dest->val.dpnt = ( double ) src->val.lval;
					break;

				default :
					fsc2_assert( 1 == 0 );
			}
			break;

		case FLOAT_VAR :
			switch ( dest->type )
			{
				case INT_VAR :
					print( WARN, "Floating point value used in assignment to "
						   "integer variable.\n" );
					dest->val.lval = ( long ) src->val.dval;
					break;

				case INT_PTR :
					print( WARN, "Floating point value used in assignment to "
						   "integer variable.\n" );
					*dest->val.lpnt = ( long ) src->val.dval;
					break;

				case FLOAT_VAR :
					dest->val.dval = src->val.dval;
					break;

				case FLOAT_PTR :
					*dest->val.dpnt = src->val.dval;
					break;

				default :
					fsc2_assert( 1 == 0 );
			}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}
}


/*-------------------------------------------------*/
/* Assignment to an one- or more-dimensional array */
/*-------------------------------------------------*/

static void vars_assign_to_nd( Var *src, Var *dest )
{
	switch ( src->type )
	{
		case INT_VAR : case FLOAT_VAR :
			if ( dest->type & ( INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF ) )
				vars_assign_to_nd_from_1d( src, dest );
			else if ( dest->type == SUB_REF_PTR )
				vars_assign_to_snd_from_1d( src, dest->from, dest );
			else
				vars_assign_to_nd_from_1d( src, dest->from );
			break;

		case INT_ARR : case FLOAT_ARR :
		case INT_REF : case FLOAT_REF :
			if ( dest->type & ( INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF ) )
				vars_assign_to_nd_from_nd( src, dest );
			else if ( dest->type == SUB_REF_PTR )
				vars_assign_to_snd_from_nd( src, dest->from, dest );
			else
				vars_assign_to_nd_from_nd( src, dest->from );
			break;

#ifndef NDEBUG
		default :
			eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
#endif
	}
}


/*------------------------------------------------------------------*/
/* Assignment of a single value to all elements of an one- or more- */
/* dimensional array (a warning is printed when the array is not of */
/* fixed size and has no defined elements yet).                     */
/*------------------------------------------------------------------*/

static void vars_assign_to_nd_from_1d( Var *src, Var *dest )
{
	long lval = 0;
	double dval = 0.0;
	

	/* Determine the value to be assigned */

	if ( src->type == INT_VAR )
	{
		if ( INT_TYPE( dest ) )
			lval = src->val.lval;
		else
			dval = ( double ) src->val.lval;
	}
	else
	{
		if ( INT_TYPE( dest ) )
		{
			print( WARN, "Assignment of FLOAT value to integer array.\n" );
			lval = ( long ) src->val.dval;
		}
		else
			dval = src->val.dval;
	}

	/* Set all elements of the destination array */

	if ( dest->len == 0 )
	{
		print( FATAL, "Assignment of value to an array of unknown length.\n" );
		THROW( EXCEPTION );
	}

	if ( vars_set_all( dest, lval, dval ) == 0 )
		print( WARN, "No elements were set in assignment because size of "
			   "destination isn't known yet.\n" );
}


/*----------------------------------------*/
/* Assigns the same value to all elements */
/* of a one- or more-dimensional array.   */
/*----------------------------------------*/

static long vars_set_all( Var *v, long l, double d )
{
	long count = 0;
	ssize_t i;


	/* When we're at the lowest level (i.e. one-dimensional array) set
	   all of its elements, otherwise loop and recurse over all referenced
	   sub-arrays */

	if ( v->type & ( INT_REF | FLOAT_REF ) )
		for ( i = 0; i < v->len; i++ )
			count += vars_set_all( v->val.vptr[ i ], l, d );
	else
	{
		for ( count = 0; count < v->len; count++ )
			if ( v->type == INT_ARR )
				v->val.lpnt[ count ] = l;
			else
				v->val.dpnt[ count ] = d;
	}

	return count;
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

static void vars_assign_to_snd_from_1d( Var *src, Var *dest, Var *sub )
{
	long lval = 0;
	double dval = 0.0;


	/* Determine the value to be assigned */

	if ( src->type == INT_VAR )
	{
		if ( INT_TYPE( dest ) )
			lval = src->val.lval;
		else
			dval = ( double ) src->val.lval;
	}
	else
	{
		if ( INT_TYPE( dest ) )
		{
			print( WARN, "Assignment of FLOAT value to integer array.\n" );
			lval = ( long ) src->val.dval;
		}
		else
			dval = src->val.dval;
	}

	vars_assign_to_snd_range_from_1d( dest, sub, 0, lval, dval );	
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

static long vars_assign_to_snd_range_from_1d( Var *dest, Var *sub,
											  ssize_t cur, long l, double d )
{
	long count = 0;
	ssize_t i, ind, range_start, range_end;


	while ( sub->val.index[ cur ] >= 0 && cur < sub->len - 1 )
		dest = dest->val.vptr[ sub->val.index[ cur++ ] ];

	if ( cur == sub->len - 1 )
	{
		ind = sub->val.index[ cur ];

		if ( dest->type & ( INT_REF | FLOAT_REF ) )
			count += vars_set_all( dest->val.vptr[ ind ], l, d );
		else
		{
			if ( dest->type == INT_ARR )
				dest->val.lpnt[ ind ] = l;
			else
				dest->val.dpnt[ ind ] = d;

			count++;
		}

		return count;
	}

	range_start = - sub->val.index[ cur++ ] - 1;
	range_end = sub->val.index[ cur++ ];

	if ( dest->type & ( INT_ARR | FLOAT_ARR ) )
	{
		for ( i = range_start; i <= range_end; i++, count++ )
			if ( dest->type == INT_ARR )
				dest->val.lpnt[ i ] = l;
			else
				dest->val.dpnt[ i ] = d;
	}
	else
		for ( i = range_start; i <= range_end; i++ )
			count += vars_assign_to_snd_range_from_1d( dest->val.vptr[ i ],
													   sub, cur, l, d );

	return count;
}


/*--------------------------------*/
/* Assign of an array to an array */
/*--------------------------------*/

static void vars_assign_to_nd_from_nd( Var *src, Var *dest )
{
	ssize_t i;


	/* The dimensions of the destination array must be at least as large as
	   the one of the source array */

	if ( dest->dim < src->dim )
	{
		print( FATAL, "Left hand side of assignment has lower dimension than "
			   "right hand side.\n" );
		THROW ( EXCEPTION );
	}

	if ( dest->dim > src->dim )
	{
		for ( i = 0; i < dest->len; i++ )
			if ( dest->val.vptr[ i ] != NULL )
				vars_assign_to_nd_from_nd( src, dest->val.vptr[ i ] );
		return;
	}

	/* Unless the destination array is dynamically sized the sizes of
	   both arrays must be identical */

	vars_size_check( src, dest );
	vars_arr_assign( src, dest );
}


/*-------------------------------------------------*/
/* Checks if the sizes of two arrays are identical */
/*-------------------------------------------------*/

static void vars_size_check( Var *src, Var *dest )
{
	ssize_t i;


	if ( dest->flags & IS_DYNAMIC )
		return;

	if ( dest->len != src->len )
	{
		print( FATAL, "Sizes of arrays differ.\n" );
		THROW( EXCEPTION );
	}

	/* If this isn't an 1-dimensional array also do checks on the sub-arrays */

	if ( dest->type & ( INT_REF | FLOAT_REF ) )
		for( i = 0; i < dest->len; i++ )
			if ( dest->val.vptr[ i ] != NULL )
				vars_size_check( src->val.vptr[ i ], dest->val.vptr[ i ] );
}


/*------------------------------------------------------*/
/* Assign of an array to an array specified with ranges */
/*------------------------------------------------------*/

static void vars_assign_to_snd_from_nd( Var *src, Var *dest, Var *sub )
{
	ssize_t cur = 0;
	ssize_t single_indices = 0;
	ssize_t i;


	while ( sub->val.index[ cur ] >= 0 )
		dest = dest->val.vptr[ sub->val.index[ cur++ ] ];

	i = cur + 2;

	while ( i < sub->len )
		if ( sub->val.index[ i++ ] >= 0 )
			single_indices++;

	/* The dimension of the destination array must be at least as large as
	   the one of the source array */

	if ( dest->dim - single_indices < src->dim )
	{
		print( FATAL, "Left hand side of assignment has lower dimension than "
			   "right hand side.\n" );
		THROW ( EXCEPTION );
	}

	vars_assign_snd_range_from_nd( dest, sub, cur, src );
}


/*--------------------------------------------*/
/*--------------------------------------------*/

static void vars_assign_snd_range_from_nd( Var *dest, Var *sub, ssize_t cur,
										   Var *src )
{
	ssize_t range_start, range_end, range, i;
	Var *cv;


	if ( sub->val.index[ cur ] < 0 )
	{
		range_start = - sub->val.index[ cur++ ] - 1;
		range_end = sub->val.index[ cur++ ];
		range = range_end - range_start + 1;

		if ( range != src->len )
		{
			print( FATAL, "Sizes of array slices don't fit in "
				   "assignment.\n" );
			THROW( EXCEPTION );
		}

		if ( dest->type & ( INT_ARR | FLOAT_ARR ) )
		{
			/* Now copy all elements, taking care of possibly different types
			   of the arrays. */

			if ( INT_TYPE( dest ) )
			{
				if ( INT_TYPE( src ) )
					memcpy( dest->val.lpnt + range_start, src->val.lpnt,
							range * sizeof *dest->val.lpnt );
				else
					for ( i = 0; i < range; i++ )
						dest->val.lpnt[ i + range_start ] =
												    lrnd( src->val.dpnt[ i ] );
			}
			else
			{
				if ( INT_TYPE( src ) )
					for ( i = 0; i < range_end; i++ )
						dest->val.dpnt[ i + range_start ] =
												 ( double ) src->val.lpnt[ i ];
				else
					memcpy( dest->val.dpnt + range_start, src->val.dpnt,
							range * sizeof *dest->val.dpnt );
			}
		}
		else
			for ( i = 0; i < range; i++ )
				if ( src->dim == dest->dim )
					vars_assign_snd_range_from_nd(
											 dest->val.vptr[ i + range_start ],
											 sub, cur, src->val.vptr[ i ] );
				else
					vars_assign_snd_range_from_nd(
											 dest->val.vptr[ i + range_start ],
											 sub, cur, src );
		return;
	}

	for ( i = cur + 1; i < sub->len; i++ )
		if ( sub->val.index[ i ] < 0 )
			break;

	if ( i < sub->len )
	{
		while ( sub->val.index[ cur ] >= 0 )
			dest = dest->val.vptr[ sub->val.index[ cur++ ] ];

		vars_assign_snd_range_from_nd( dest, sub, cur, src );
		return;
	}
	else
	{
		i = sub->val.index[ cur ];

		if ( dest->type & ( INT_REF | FLOAT_REF ) &&
			 dest->val.vptr[ i ] == NULL )
		{

			dest->val.vptr[ i ]           = vars_new( NULL );
			dest->val.vptr[ i ]->from     = cv;
			if ( dest->dim > 2 )
				dest->val.vptr[ i ]->type = cv->type;
			else
				dest->val.vptr[ i ]->type = 
									 cv->type == INT_REF ? INT_ARR : FLOAT_ARR;
			dest->val.vptr[ i ]->dim      = dest->dim - 1;
			dest->val.vptr[ i ]->len      = 0;
			dest->val.vptr[ i ]->flags   |= IS_DYNAMIC;
			dest->val.vptr[ i ]->flags   &= ~ NEW_VARIABLE;
		}

		if ( ++cur == sub->len )
			vars_arr_assign( src, dest );
		else
			vars_assign_snd_range_from_nd( dest->val.vptr[ i ], sub,
										   cur, src );
	}
}


/*--------------------------------------------------------------------------*/
/* Finally really assign one array to another by looping over all elements. */
/*--------------------------------------------------------------------------*/

static void vars_arr_assign( Var *src, Var *dest )
{
	ssize_t i;


	/* If we're at the lowest level, i.e. are dealing with one-dimensional
	   arrays we may have to adjust the size of the destination array and
	   then we simply copy the array elements. If we're still at a higher
	   level we also may to change the number of sub-arrays of the
	   destination array and then set all sub-arrays by recursively calling
	   this function. */

	if ( src->type & ( INT_ARR | FLOAT_ARR ) )
	{
		/* If necessary resize the destination array */

		if ( dest->flags & IS_DYNAMIC && dest->len != src->len )
		{
			if ( src->len == 0 && dest->len > 0 )
			{
				print( WARN, "Assignment from 1D-array that has no "
					   "elements.\n" );
				dest->len = 0;
				return;
			}

			if ( dest->len != 0 )
			{
				if ( INT_TYPE( dest ) )
					dest->val.lpnt = LONG_P T_free( dest->val.lpnt );
				else
					dest->val.dpnt = DOUBLE_P T_free( dest->val.dpnt );
			}

			dest->len = src->len;

			if ( INT_TYPE( dest ) )
				dest->val.lpnt = LONG_P T_malloc( dest->len
												  * sizeof *dest->val.lpnt );
			else
				dest->val.dpnt = DOUBLE_P T_malloc( dest->len
													* sizeof *dest->val.dpnt );
		}

		/* Now copy all elements, taking care of possibly different types
		   of the arrays. */

		if ( INT_TYPE( dest ) )
		{
			if ( INT_TYPE( src ) )
				memcpy( dest->val.lpnt, src->val.lpnt,
						dest->len * sizeof *dest->val.lpnt );
			else
				for ( i = 0; i < dest->len; i++ )
					dest->val.lpnt[ i ] = lrnd( src->val.dpnt[ i ] );
		}
		else
		{
			if ( INT_TYPE( src ) )
				for ( i = 0; i < dest->len; i++ )
					dest->val.dpnt[ i ] = ( double ) src->val.lpnt[ i ];
			else
				memcpy( dest->val.dpnt, src->val.dpnt,
						dest->len * sizeof *dest->val.dpnt );
		}
	}
	else
	{
		/* If necessary resize the array of references of the destination
		   array */

		if ( dest->flags & IS_DYNAMIC )
		{
			if ( dest->len > src->len )
			{
				for ( i = dest->len - 1; i >= src->len; i-- )
				{
					vars_free( dest->val.vptr[ i ], SET );
					dest->len--;
				}

				if ( src->len > 0 )
					dest->val.vptr = VAR_PP T_realloc( dest->val.vptr,
										   src->len * sizeof *dest->val.vptr );
				else
				{
					dest->val.vptr = VAR_PP T_free( dest->val.vptr );
					print( WARN, "Assignment from %dD-array that has no "
						   "sub-arrays.\n", src->dim );
					dest->len = 0;
				}
			}
			else if ( dest->len < src->len )
			{
				dest->val.vptr = VAR_PP T_realloc( dest->val.vptr,
										   src->len * sizeof *dest->val.vptr );
				for ( ; dest->len < src->len; dest->len++ )
				{
					dest->val.vptr[ dest->len ] = vars_new( NULL );
					dest->val.vptr[ dest->len ]->from = dest;
					dest->val.vptr[ dest->len ]->len = 0;
					dest->val.vptr[ dest->len ]->dim = dest->dim - 1;
					dest->val.vptr[ dest->len ]->flags |= IS_DYNAMIC;
					if ( dest->dim > 2 )
						dest->val.vptr[ dest->len ]->type = dest->type;
					else
						dest->val.vptr[ dest->len ]->type =
										INT_TYPE( dest ) ? INT_ARR : FLOAT_ARR;
				}
			}
		}

		/* Now copy the sub-arrays by calling the function recursively */

		for ( i = 0; i < dest->len; i++ )
			vars_arr_assign( src->val.vptr[ i ], dest->val.vptr[ i ] );
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
