/*
   $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


/*
   There are five types of simple variables:

     INT_VAR        stores a single integer value (long)
	 FLOAT_VAR      stores a single float value (double)
	 STR_VAR        stores a string
	 INT_ARR        stores a 1d array of (long) integer values
	 FLOAT_ARR      stores a 1d array of (double) float values

   STR_VARs are only used internally and can't be explicitely created by
   the user (they are for storing literal strings in the EDL code).
   Both INT_ARRs and FLOAT_ARRs exist in two flavors. Depending on how
   they were defined by the user they can have either a fixed, immutable
   size or can have a dynamically adjustable size. In the later case the
   size will change when an 1D-array of a different size is assigned
   to the array (in which case the size can get both larger and smaller)
   or when a value is assigned to an element that didn't exist yet (in
   which case the size will grow if necessary to allow storing the new
   element). The 'len' field of the Var structures allows to determine
   the current length of the array (for dynamically sized arrays it's
   legal to have 'len' set to 0, in this case the size hasn't been set
   yet). 

   To allow for two- and more dimensional arrays there are two further
   variable types

     INT_REF
	 FLOAT_REF

   These variables contain pointers to further variables of the same kind
   or, for the lowest dimension, INT_ARRs or FLOAT_ARRs. Again, there are
   two flavors, fixed and dynamically sized references. The 'dim' field of
   the Var structure allows to determine the dimensionality of the array,
   the 'len' field the size of the current dimension.

   The only restriction here is that if a reference is dynamically sized
   also all referenced references and arrays (down to the lowest level)
   must be dynamically sized. Thus one may have definitions in an EDL
   script like

     a[ *, *, * ];
	 b[ 3, *, * ];
	 c[ 5, 3, * ];
	 d[ 2, 7, 9 ];

   but not

     e[ *, 3, * ];
	 f[ *, 4, 2 ];

   It is also possible that the referenced references or arrays of a
   reference have different dimensions, i.e. the definition

     g[ 3, * ];

   allows situations where g[ 1 ] is an array with 7 elements, while g[ 2 ]
   has 127 elements and g[ 3 ] has a still unknown number of elements.

   There are a few more variable types:

   When on the left hand side of a statement an array element is found (i.e.
   the left hand side resolves to a simple number) a variable of the types

     INT_PTR
     FLOAT_PTR

   is pushed on the stack. The 'val.lpnt' or 'val.dpnt' field of the Var
   structure is pointing to the element of the array that needs to be
   changed. If on the left hand side an array or (sub-) matrix is found a
   variable of type 

     REF_PTR

   with its 'from' field being a pointer to the referenced array or matrix
   (or sub-matrix) is pushed on the stack.

   If on the right hand side an array element is found a variable of type
   INT_VAR or FLOAT_VAR is pushed onto the stack instead with the value
   of the indexed element.

   If on the right hand side an array or a matrix (or a sub-matrix) is found
   a copy of it is pushed on the stack. These temporary copies have all the
   'IS_TEMP' bit in the 'flags' field set. The sub-variables for temporary
   INT_REF and FLOAT_REF variables aren't on the stack (i.e. the 'ON_STACK'
   bit in the 'flags' field isn't set) and go into the list of normal
   variables (but will be automatically deleted whenever the referencing
   stack variable gets removed). The reason for this is that EDL functions
   expect their variables continuously on the stack. Thus having these
   extra variables on the stack (that aren't arguments) would mess up this
   convention completely and would require a complete rewrite of all EDL
   functions to skip such additional variables.

   Finally, there are two more variable types:

     UNDEF_VAR
	 FUNC

   UNDEF_VAR is the state in which every variable is born, i.e. it has no
   type yet. Thus variables of this type usually will only exist shortly
   during interpretation of the VARIABLES section. The other situation
   where such a variable may appear is as a dummy variable when an array
   reference is found in the EDL script with no indices within the square
   braces.

   FUNC, pointing to an EDL function is used during the evaluation of
   functions (the variables below the FUNC variable are the arguments).
   The 'name' field of the Var structure contains the name of the function
   to be called.

   All variables of the types STR_VAR, INT_PTR, FLOAT_PTR, REF_PTR and FUNC
   can only appear on the variable stack but never as a "real" variable in
   the variables list.
*/


#include "fsc2.h"


/* locally used functions */

static Var *vars_setup_new_array( Var *v, int dim );
static Var *vars_init_elements( Var *a, Var *v );
static void vars_do_init( Var *src, Var *dest );
static Var *vars_lhs_pointer( Var *v, int dim );
static void vars_assign_to_1d( Var *src, Var *dest );
static void vars_assign_to_nd( Var *src, Var *dest );
static void vars_assign_to_nd_from_1d( Var *src, Var *dest );
static void vars_assign_to_nd_from_nd( Var *src, Var *dest );
static long vars_set_all( Var *v, long l, double d );
static void vars_size_check( Var *src, Var *dest );
static void vars_arr_assign( Var *src, Var *dest );
static void free_all_vars( void );
static void vars_ref_copy( Var *nsv, Var *cp, bool exact_copy );
static void vars_ref_copy_create( Var *nsv, Var *src, bool exact_copy );
static void *vars_get_pointer( ssize_t *iter, ssize_t depth, Var *p );


/*----------------------------------------------------------------------*/
/* vars_get() is called when a token is found that might be a variable  */
/* identifier. The function checks if it is an already defined variable */
/* and in this case returns a pointer to the corresponding structure,   */
/* otherwise it returns NULL.                                           */
/* ->                                                                   */
/*   * string with name of variable                                     */
/* <-                                                                   */
/*   * pointer to VAR structure or NULL                                 */
/*----------------------------------------------------------------------*/

Var *vars_get( char *name )
{
	Var *v;


	/* Try to find the variable with the name passed to the function */

	for ( v = EDL.Var_List; v != NULL; v = v->next )
	{
		if ( v->name == NULL )
			continue;
		if ( ! strcmp( v->name, name ) )
			return v;
	}

	return NULL;
}


/*----------------------------------------------------------*/
/* vars_new() sets up a new variable by getting memory for  */
/* a variable structure and setting the important elements. */
/* ->                                                       */
/*   * string with variable name                            */
/* <-                                                       */
/*   * pointer to variable structure                        */
/*----------------------------------------------------------*/

Var *vars_new( char *name )
{
	Var *vp;


	/* Get memory for a new structure and for storing the name */

	vp = VAR_P T_malloc( sizeof *vp );
	if ( name != NULL )
		vp->name    = T_strdup( name );
	else
		vp->name    = NULL;

	/* Set relevant entries in the new structure and make it the very first
	   element in the list of variables */

	vp->type     = UNDEF_VAR;           /* set type to still undefined */
	vp->from     = NULL;
	vp->flags    = NEW_VARIABLE;
	vp->len      = 0;
	vp->dim      = 0;
	vp->val.vptr = NULL;

	vp->prev = NULL;
	vp->next = EDL.Var_List;         /* set pointer to it's successor */
	if ( EDL.Var_List != NULL )      /* set previous pointer in successor */
		EDL.Var_List->prev = vp;     /* (if this isn't the very first) */
    EDL.Var_List = vp;               /* make it the head of the list */
	
	return vp;
}


/*-------------------------------------------------------------------*/
/* This function is called when a 'VAR_TOKEN [' combination is found */
/* in the input. For a new array it sets its type. Everything else   */
/* it does is pushing a variable with a pointer to the array onto    */
/* the stack.                                                        */
/*-------------------------------------------------------------------*/


Var *vars_arr_start( Var *v )
{
	if ( v->type != UNDEF_VAR )
		vars_check( v, INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF );

	/* Push variable with generic pointer to an array onto the stack */

	return vars_push( REF_PTR, v );
}


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
	Var *cv;


	/* Move up the stack to the variable indicating the array or matrix
	   itself */

	while ( v->type != REF_PTR )
		v = v->prev;

	for ( dim = 0, cv = v->next; cv != 0; dim++, cv = cv->next )
		/* empty */ ;

	/* If the array is new (i.e. we're still in the VARIABLES section) set
	   it up, otherwise return a pointer to the referenced element */

	if ( v->from->type == UNDEF_VAR )
		return vars_setup_new_array( v, dim );
	else
		return vars_lhs_pointer( v, dim );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static Var* vars_setup_new_array( Var *v, int dim )
{
	Var *a = v->from;


	/* We can't continue with a (LHS) definition like "a[ ]"...*/

	if ( v->next->type == UNDEF_VAR )
	{
		print( FATAL, "Missing sizes in definition of array '%s'.\n",
			   v->from->name );
		THROW( EXCEPTION );
	}

	/* Create the array as far as possible */

	a->type = VAR_TYPE( a->name ) == INT_VAR ? INT_REF : FLOAT_REF;
	a->flags &= ~ NEW_VARIABLE;

	vars_arr_create( a, v->next, dim, UNSET );

	/* Get rid of variables that are not needed anymore */

	while ( ( v = vars_pop( v ) ) != NULL )
		/* empty */ ;

	/* Finally push a marker on the stack to be used in array initialization */

	return vars_push( REF_PTR, a );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void vars_arr_create( Var *a, Var *v, int dim, bool is_temp )
{
	Var *c;
	ssize_t i;
	ssize_t len;


	a->dim = dim;

	vars_check( v, INT_VAR | FLOAT_VAR );

	/* If the size is not defined the whole of the rest of the array is
	   dynamically sized and can be only set up by an assignement sometime
	   later. */

	if ( v->flags & IS_DYNAMIC )
	{
		a->len = 0;
		a->flags |= IS_DYNAMIC;

		if ( v->next == NULL )
			a->type = INT_TYPE( a ) ? INT_ARR : FLOAT_ARR;

		/* Also all the following sizes must be dynamically sized */

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
		print( WARN, "FLOAT value used as size/index of array.\n" );
		len = lrnd( v->val.dval );
	}

	if ( len < 1 )
	{
		print( FATAL, "Invalid size/index for array.\n" );
		THROW( EXCEPTION );
	}

	/* The current dimension is obviously not dynamically sized or we
	   wouldn't have gotten here */

	a->len = len;
	a->flags &= ~ IS_DYNAMIC;

	/* If this is the last dimension we really allocate memory (intialized
	   to 0) for a data array and then are done */

	if ( v->next == NULL )
	{
		a->type = a->type == INT_REF ? INT_ARR : FLOAT_ARR;
		if ( a->type == INT_ARR )
			a->val.lpnt = LONG_P T_calloc( a->len, sizeof *a->val.lpnt );
		else
			a->val.dpnt = DOUBLE_P T_calloc( a->len, sizeof *a->val.dpnt );

		return;
	}
	
	/* Otherwise we need an array of references to arrays of lower
	   dimensions */

	a->val.vptr = VAR_PP T_malloc( a->len * sizeof *a->val.vptr );

	for ( i = 0; i < a->len; i++ )
		a->val.vptr[ i ] = NULL;

	for ( i = 0; i < a->len; i++ )
	{
		a->val.vptr[ i ] = vars_new( NULL );
		a->val.vptr[ i ]->type = a->type;
		a->val.vptr[ i ]->from = a;
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
	nv->val.vptr = VAR_PP T_malloc( nv->len * sizeof *nv->val.vptr );
	for ( i = 0; i < nv->len; v = v->next )
	{
		if ( v->dim != level )
			continue;

		v->flags |= INIT_ONLY;

		if ( v->type == UNDEF_VAR )
			nv->val.vptr[ i++ ] = NULL;
		else
			nv->val.vptr[ i++ ] = v;
	}

	return nv;
}


/*--------------------------------------------------------------------------*/
/* Function initializes a new array. When called the stack at 'v' contains  */
/* all the data for the initialization (the last data on top of the stack)  */
/* and, directly below the data, an REF_PTR to the array to be initialized. */
/* If 'v' is an UNDEF_VAR no initialization has to be done.                 */
/*--------------------------------------------------------------------------*/

void vars_arr_init( Var *v )
{
	Var *dest;


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
				dest->val.lpnt = LONG_P T_calloc( dest->len,
												  sizeof *dest->val.lpnt );
			}
			else if ( src->len > dest->len )
				print( WARN, "Superfluous intitialization data.\n" );

			if ( src->type == INT_ARR )
				memcpy( dest->val.lpnt, src->val.lpnt,
						( dest->len < src->len ? dest->len : src->len )
						* sizeof *dest->val.lpnt );
			else
			{
				print( WARN, "Intialization of integer array with floating "
					   "point values.\n" );
				for ( i = 0; i < ( dest->len < src->len ?
								   dest->len : src->len ); i++ )
					dest->val.lpnt[ i ] = ( long ) src->val.dpnt[ i ];
			}
			return;

		case FLOAT_ARR :
			if ( dest->flags & IS_DYNAMIC )
			{
				dest->len = src->len;
				dest->val.dpnt = DOUBLE_P T_calloc( dest->len,
													sizeof *dest->val.dpnt );
			}
			else if ( src->len > dest->len )
				print( WARN, "Superfluous intitialization data.\n" );

			if ( src->type == FLOAT_ARR )
				memcpy( dest->val.dpnt, src->val.dpnt,
						( dest->len < src->len ? dest->len : src->len )
						* sizeof *dest->val.dpnt );
			else
				for ( i = 0; i < ( dest->len < src->len ?
								   dest->len : src->len ); i++ )
					dest->val.dpnt[ i ] = ( double ) src->val.lpnt[ i ];
			return;

		case INT_REF :
			if ( dest->flags & IS_DYNAMIC )
			{
				dest->len = src->len;
				dest->val.dpnt = VAR_PP T_malloc( dest->len
												  * sizeof *dest->val.vptr );
				for ( i = 0; i < dest->len; i++ )
					dest->val.vptr[ i ] = NULL;
			}
			else if ( src->len > dest->len )
				print( WARN, "Superfluous intitialization data.\n" );

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
				dest->len = src->len;
				dest->val.dpnt = VAR_PP T_malloc( dest->len
												  * sizeof *dest->val.vptr );
				for ( i = 0; i < dest->len; i++ )
					dest->val.vptr[ i ] = NULL;
			}
			else if ( src->len > dest->len )
				print( WARN, "Superfluous intitialization data.\n" );

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
	Var *a, *cv;
	ssize_t ind;
	int cur_dim;
	ssize_t i;


	a = v->from;
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

	/* Check the indices, all must be numbers */

	for ( cv = v; cv != NULL; cv = cv->next )
		vars_check( cv, INT_VAR | FLOAT_VAR );

	/* Now loop over the indices to get at the addressed array element - for
	   dynamically sized arrays create it if it does not already exist */

	for ( cv = a, cur_dim = a->dim - 1;
		  cur_dim > 0 && v != NULL; cur_dim--, v = vars_pop( v ) )
	{
		if ( v->type == INT_VAR )
			ind = v->val.lval;
		else
		{
			print( WARN, "FLOAT value used as index for array '%s'.\n",
				   a->name );
			ind = lrnd( v->val.dval );
		}

		ind -= ARRAY_OFFSET;

		if ( ind < 0 )
		{
			print( FATAL, "Invalid index for array '%s'.\n", a->name );
			THROW( EXCEPTION );
		}

		/* If the index is larger than the length of the array this is a
		   fatal error for fixed length arrays, but for dynamically sized
		   arrays it simply means we have to extend them */

		if ( cv->len <= ind )
		{
			if ( ! ( cv->flags & IS_DYNAMIC ) )
			{
				print( FATAL, "Invalid index for array '%s'.\n", a->name );
				THROW( EXCEPTION );
			}

			cv->val.vptr = VAR_PP T_realloc( cv->val.vptr,
										  ( ind + 1 ) * sizeof *cv->val.vptr );

			for ( i = cv->len; i <= ind; i++ )
				cv->val.vptr[ i ] = NULL;

			for ( i = cv->len; i <= ind; i++ )
			{
				cv->val.vptr[ i ] = vars_new( NULL );
				cv->val.vptr[ i ]->from = cv;
				if ( cur_dim > 1 )
					cv->val.vptr[ i ]->type = cv->type;
				else
					cv->val.vptr[ i ]->type = 
									 cv->type == INT_REF ? INT_ARR : FLOAT_ARR;
				cv->val.vptr[ i ]->dim = cur_dim;
				cv->val.vptr[ i ]->len = 0;
				cv->val.vptr[ i ]->flags |= IS_DYNAMIC;
				cv->val.vptr[ i ]->flags &= ~ NEW_VARIABLE;
			}

			cv->len = ind + 1;
		}

		cv = cv->val.vptr[ ind ];
	}

	if ( v == NULL )
		return vars_push( REF_PTR, cv );

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		ind = v->val.lval;
	else
	{
		print( WARN, "FLOAT value used as index for array '%s'.\n",
			   a->name );
		ind = lrnd( v->val.dval );
	}

	ind -= ARRAY_OFFSET;

	if ( ind < 0 )
	{
		print( FATAL, "Invalid index for array '%s'.\n", a->name );
		THROW( EXCEPTION );
	}

	/* If necessary increase the size of dynamic arrays */

	if ( ind >= cv->len )
	{
		if ( ! ( cv->flags & IS_DYNAMIC ) )
		{
			print( FATAL, "Invalid index for array '%s'.\n", a->name );
			THROW( EXCEPTION );
		}

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

			case INT_REF :
				cv->type = INT_ARR;
				cv->val.lpnt = LONG_P T_calloc( ind + 1,
												sizeof *cv->val.lpnt );

			case FLOAT_REF :
				cv->type = FLOAT_ARR;
				cv->val.dpnt = DOUBLE_P T_calloc( ind + 1,
												  sizeof *cv->val.dpnt );
				break;

			default :
				fsc2_assert( 1 == 0 );
		}

		cv->len = ind + 1;
	}

	vars_pop( v );

	if ( cv->type == INT_ARR )
		return vars_push( INT_PTR, cv->val.lpnt + ind );
	return vars_push( FLOAT_PTR, cv->val.dpnt + ind );
}


/*-----------------------------------------------------------------------*/
/* Function is called if an element an array or a sub-array is accessed. */
/* Prior to the call of this function vars_arr_start() has been called   */
/* and a REF_PTR type variable, pointing to the array variable, had been */
/* pushed onto the stack.                                                */
/*-----------------------------------------------------------------------*/

Var *vars_arr_rhs( Var *v )
{
	Var *a, *cv;
	int dim = 0;
	ssize_t ind;


	/* The variable pointer this function gets passed is a pointer to the very
	   last index on the variable stack. Now we've got to work our way up in
	   the stack until we find the first non-index variable a reference. */

	while ( v->type != REF_PTR )
	{
		dim++;
		v = v->prev;
	}

	a = v->from;
	v = vars_pop( v );

	/* When a complete array is passed this can be either written with empty
	   square braces, in which case we get a single variable of UNDEF_VAR, or
	   by no square braces at all, in which case we get no index variables
	   at all. */

	if ( v->type == UNDEF_VAR )
	{
		vars_pop( v );
		return a;
	}

	if ( v == NULL  )
		return a;

	if ( dim > a->dim )
	{
		print( FATAL, "Too many indices for array '%s'.\n", a->name );
		THROW( EXCEPTION );
	}

	for ( cv = a; cv->type & ( INT_REF | FLOAT_REF ) && v != NULL;
		  v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR | FLOAT_VAR );

		if ( v->type == INT_VAR )
			ind = v->val.lval;
		else
		{
			print( WARN, "FLOAT value used as index for array '%s'.\n",
				   a->name );
			ind = lrnd( v->val.dval );
		}
		
		ind -= ARRAY_OFFSET;

		if ( ind >= cv->len )
		{
			print( FATAL, "Invalid index for array '%s'.\n", a->name );
			THROW( EXCEPTION );
		}

		cv = cv->val.vptr[ ind ];
	}

	if ( v == NULL )
	{
		if ( cv->type == INT_ARR )
			return vars_push( cv->type,cv->val.lpnt, cv->len );
		else if ( cv->type == FLOAT_ARR )
			return vars_push( cv->type, cv->val.dpnt, cv->len );
		else
			return vars_push( cv->type, cv );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		ind = v->val.lval;
	else
	{
		print( WARN, "FLOAT value used as index for array '%s'.\n",
			   a->name );
		ind = lrnd( v->val.dval );
	}
		
	ind -= ARRAY_OFFSET;

	if ( ind >= cv->len )
	{
		print( FATAL, "Invalid index for array '%s'.\n", a->name );
		THROW( EXCEPTION );
	}

	vars_pop( v );

	if ( cv->type & INT_ARR )
		return vars_push( INT_VAR, cv->val.lpnt[ ind ] );

	return vars_push( FLOAT_VAR, cv->val.dpnt[ ind ] );
}
		

/*--------------------------------------------------------*/
/* This is the central function for assigning new values. */
/*--------------------------------------------------------*/

void vars_assign( Var *src, Var *dest )
{
	/* <PARANOID> Check that both variables exist </PARANOID> */

	fsc2_assert( vars_exist( src ) && vars_exist( dest ) );

#ifndef NDEBUG
	if ( ! ( src->type & ( INT_VAR | FLOAT_VAR |
						   INT_ARR | FLOAT_ARR |
						   INT_REF | FLOAT_REF ) ) )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	/* First we distingush between the different possible types of variables
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
		case REF_PTR :
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


/*------------------------------------------------------*/
/* Assignment to a variable or the element of an array. */
/*------------------------------------------------------*/

static void vars_assign_to_1d( Var *src, Var *dest )
{
	/* If the destination variable is still completely new set its type */

	if ( dest->type == UNDEF_VAR )
	{
		dest->type = VAR_TYPE( dest->name );
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
/* Assignment to a one- or more-dimensional array. */
/*-------------------------------------------------*/

static void vars_assign_to_nd( Var *src, Var *dest )
{
	switch ( src->type )
	{
		case INT_VAR : case FLOAT_VAR :
			if ( dest->type & ( INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF ) )
				vars_assign_to_nd_from_1d( src, dest );
			else
				vars_assign_to_nd_from_1d( src, dest->from );
			break;

		case INT_ARR : case FLOAT_ARR :
		case INT_REF : case FLOAT_REF :
			if ( dest->type & ( INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF ) )
				vars_assign_to_nd_from_nd( src, dest );
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
/* Assignment of a single value to all elements of a one- or more-  */
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


/*------------------------------*/
/* Assign an array to an array. */
/*------------------------------*/

static void vars_assign_to_nd_from_nd( Var *src, Var *dest )
{
	ssize_t i;


	/* The dimensions of both arrays must be identical */

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
			if ( dest->len != 0 )
			{
				if ( INT_TYPE( dest ) )
					dest->val.lpnt = LONG_P T_free( dest->val.lpnt );
				else
					dest->val.dpnt = DOUBLE_P T_free( dest->val.dpnt );
			}

			if ( src->len == 0 && dest->len > 0 )
			{
				print( WARN, "Assignment from array that has no "
					   "elements.\n" );
				dest->len = 0;
				return;
			}

			dest->len = src->len;

			if ( INT_TYPE( dest ) )
				dest->val.lpnt = LONG_P T_malloc(   dest->len
												  * sizeof *dest->val.lpnt );
			else
				dest->val.dpnt = DOUBLE_P T_malloc(   dest->len
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
					dest->val.lpnt[ i ] = ( long ) src->val.dpnt[ i ];
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
				for ( i = src->len; i < dest->len; i++ )
					vars_free( dest->val.vptr[ i ], SET );
				dest->val.vptr = VAR_PP T_realloc( dest->val.vptr,
										   src->len * sizeof *dest->val.vptr );
			}
			else if ( dest->len < src->len )
			{
				dest->val.vptr = VAR_PP T_realloc( dest->val.vptr,
										   src->len * sizeof *dest->val.vptr );
				for ( i = dest->len; i < src->len; i++ )
				{
					dest->val.vptr[ i ] = vars_new( NULL );
					dest->val.vptr[ i ]->from = dest;
					dest->val.vptr[ i ]->len = 0;
					dest->val.vptr[ i ]->dim = dest->dim - 1;
					dest->val.vptr[ i ]->flags |= IS_DYNAMIC;
					if ( dest->dim > 2 )
						dest->val.vptr[ i ]->type = dest->type;
					else
						dest->val.vptr[ i ]->type =
										INT_TYPE( dest ) ? INT_ARR : FLOAT_ARR;
				}
			}

			dest->len = src->len;
		}

		/* Now copy the sub-arrays by calling the function recursively */

		for ( i = 0; i < dest->len; i++ )
			vars_arr_assign( src->val.vptr[ i ], dest->val.vptr[ i ] );
	}
}


/*--------------------------------------------------------------*/
/* Removes a variable from the linked list of variables, Unless */
/* the flag passed as the second argument is set only variables */
/* with names get removed (this is used to control if also sub- */
/* variables for more-dimensional arrays get deleted).          */
/*--------------------------------------------------------------*/

Var *vars_free( Var *v, bool also_nameless )
{
	ssize_t i;
	Var *ret;


	fsc2_assert( ! ( v->flags & ON_STACK ) );

	if ( v->name == NULL && ! also_nameless )
		return v->next;

	switch ( v->type )
	{
		case INT_ARR :
			if ( v->len != 0 )
				v->val.lpnt = LONG_P T_free( v->val.lpnt );
			break;

		case FLOAT_ARR :
			if ( v->len != 0 )
				v->val.dpnt = DOUBLE_P T_free( v->val.dpnt );
			break;

		case STR_VAR :
			if ( v->val.sptr != NULL )
				v->val.sptr = CHAR_P T_free( v->val.sptr );
				break;

		case INT_REF : case FLOAT_REF :
			if ( v->len == 0 )
				break;
			for ( i = 0; i < v->len && v->val.vptr[ i ] != NULL; i++ )
				vars_free( v->val.vptr[ i ], SET );
			v->val.vptr = VAR_PP T_free( v->val.vptr );
			break;
	}

	if ( v->name != NULL )
		v->name = CHAR_P T_free( v->name );

	if ( v->prev == NULL )
		EDL.Var_List = v->next;
	else
		v->prev->next = v->next;

	if ( v->next != NULL )
		v->next->prev = v->prev;

	ret = v->next;
	T_free( v );
	return ret;
}


/*---------------------------------------------------------------*/
/* free_vars() removes all variables from the list of variables. */
/*---------------------------------------------------------------*/

static void free_all_vars( void )
{
	Var *v;


	for ( v = EDL.Var_List; v != NULL; )
		v = vars_free( v, UNSET );
}


/*----------------------------------------------------------------*/
/* vars_del_stack() deletes all entries from the variables stack. */
/*----------------------------------------------------------------*/

void vars_del_stack( void )
{
	while ( vars_pop( EDL.Var_Stack ) )
		/* empty */ ;
}


/*----------------------------------------------------------*/
/* vars_clean_up() deletes the variable and array stack and */
/* removes all variables from the list of variables         */
/*----------------------------------------------------------*/

void vars_clean_up( void )
{
	vars_del_stack( );
	free_all_vars( );
	vars_iter( NULL );
}


/*-------------------------------------------------------------*/
/* Pushes a copy of a variable onto the stack (but only if the */
/* variable to be copied isn't already a stack variable).      */
/*-------------------------------------------------------------*/

Var *vars_push_copy( Var *v )
{
	if ( v->flags & ON_STACK )
		return v;

	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			return vars_push( v->type, v->val.lval );

		case FLOAT_VAR :
			return vars_push( v->type, v->val.dval );

		case INT_ARR :
			return vars_push( v->type, v->val.lpnt, v->len );

		case FLOAT_ARR :
			return vars_push( v->type, v->val.dpnt, v->len );

		case INT_REF : case FLOAT_REF :
			return vars_push( v->type, v );
	}

	fsc2_assert( 1 == 0 );
	return NULL;
}


/*-----------------------------------------------------------------------*/
/* vars_push() creates a new entry on the variable stack (which actually */
/* is not really a stack but a linked list) and sets its type and value. */
/*-----------------------------------------------------------------------*/

Var *vars_push( int type, ... )
{
	Var *nsv, *stack, *src;
	va_list ap;


	/* Get memory for the new variable to be appended to the stack, set its
	   type and initialize some fields */

	nsv         = VAR_P T_malloc( sizeof *nsv );
	nsv->type   = type;
	nsv->name   = NULL;
	nsv->name   = 0;
	nsv->next   = NULL;
	nsv->flags  = ON_STACK;

	/* Get the data for the new variable */

	va_start( ap, type );

	switch ( type )
	{
		case UNDEF_VAR :
			break;

		case INT_VAR :
			nsv->dim = 0;
			nsv->val.lval = va_arg( ap, long );
			break;

		case FLOAT_VAR :
			nsv->dim = 0;
			nsv->val.dval = va_arg( ap, double );
			break;

		case STR_VAR :
			nsv->val.sptr = T_strdup( va_arg( ap, char * ) );
			break;

		case INT_ARR :
			nsv->val.lpnt = va_arg( ap, long * );
			nsv->len = va_arg( ap, ssize_t );

			fsc2_assert( nsv->len >= 0 );

			nsv->dim = 1;
			if ( nsv->len == 0 )
				nsv->val.lpnt = NULL;
			else
			{
				if ( nsv->val.lpnt != NULL )
					nsv->val.lpnt = LONG_P get_memcpy( nsv->val.lpnt,
											nsv->len * sizeof *nsv->val.lpnt );
				else
					nsv->val.lpnt = LONG_P T_calloc( nsv->len,
													 sizeof *nsv->val.lpnt );
			}
			break;

		case FLOAT_ARR :
			nsv->val.dpnt = va_arg( ap, double * );
			nsv->len = va_arg( ap, ssize_t );

			fsc2_assert( nsv->len >= 0 );

			nsv->dim = 1;
			if ( nsv->len == 0 )
				nsv->val.dpnt = NULL;
			else
			{
				if ( nsv->val.dpnt != NULL )
					nsv->val.dpnt = DOUBLE_P get_memcpy( nsv->val.dpnt,
											nsv->len * sizeof *nsv->val.dpnt );
				else
					nsv->val.dpnt = DOUBLE_P T_calloc( nsv->len,
													   sizeof *nsv->val.dpnt );
			}
			break;

		case INT_PTR :
			nsv->val.lpnt = va_arg( ap, long * );
			break;

		case FLOAT_PTR :
			nsv->val.dpnt = va_arg( ap, double * );
			break;

		case INT_REF : case FLOAT_REF :
			src = va_arg( ap, Var * );
			if ( src != NULL )
			{
				if ( src->type == REF_PTR )
					src = src->from;
				vars_ref_copy( nsv, src, UNSET );
			}
			break;

		case REF_PTR :
			nsv->from = va_arg( ap, Var * );
			break;

		case FUNC :
			nsv->val.fnct = va_arg( ap, struct Func * );
			break;

#ifndef NDEBUG
		default :
			eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
					__FILE__, __LINE__ );
			T_free( nsv );
			va_end( ap );
			THROW( EXCEPTION );
#endif
	}

	va_end( ap );

	/* Finally append the new variable to the stack */

	if ( ( stack = EDL.Var_Stack ) == NULL )
	{
		EDL.Var_Stack = nsv;
		nsv->prev = NULL;
	}
	else
	{
		while ( stack->next != NULL )
			stack = stack->next;
		stack->next = nsv;
		nsv->prev = stack;
	}

	/* Return the address of the new stack variable */

	return nsv;
}


/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

Var *vars_make( int type, Var *src )
{
	Var *nv, *stack;
	ssize_t i;


	if ( src->flags & ON_STACK )
	{
		nv        = VAR_P T_malloc( sizeof *nv );
		nv->name  = NULL;
		nv->next  = NULL;
		nv->flags = ON_STACK;

		if ( ( stack = EDL.Var_Stack ) == NULL )
		{
			EDL.Var_Stack = nv;
			nv->prev = NULL;
		}
		else
		{
			while ( stack->next != NULL )
				stack = stack->next;
			stack->next = nv;
			nv->prev = stack;
		}
	}
	else
		nv = vars_new( NULL );

	nv->type = type;

	if ( src->flags & IS_DYNAMIC )
		nv->flags |= IS_DYNAMIC;

	switch ( type )
	{
		case INT_ARR :
			nv->len = src->len;
			nv->dim = 1;
			if ( nv->len != 0 )
				nv->val.lpnt = LONG_P T_calloc( nv->len,
												sizeof *nv->val.lpnt );
			else
				nv->val.lpnt = NULL;
			return nv;

		case FLOAT_ARR :
			nv->len = src->len;
			nv->dim = 1;
			if ( nv->len != 0 )
			{
				nv->val.dpnt = DOUBLE_P T_malloc(   nv->len
												  * sizeof *nv->val.dpnt );
				for ( i = 0; i < nv->len; i++ )
					nv->val.dpnt[ i ] = 0.0;
			}
			else
				nv->val.dpnt = NULL;
			return nv;

		case INT_REF : case FLOAT_REF :
			nv->len = src->len;
			nv->dim = src->dim;
			if ( nv->len != 0 )
			{
				nv->val.vptr = VAR_PP T_malloc(   nv->len
												* sizeof *nv->val.vptr );
				for ( i = 0; i < nv->len; i++ )
					nv->val.vptr[ i ] = NULL;
			}
			else
				nv->val.vptr = NULL;
			return nv;
	}

	fsc2_assert( 1 == 0 );
	return NULL;
}


/*------------------------------------------------------------------------*/
/* Function for making a copy of an INT_REF or FLOAT_REF via vars_push(). */
/* This also works with making a FLOAT type copy of an INT type matrix.   */
/* Note that sub-matrices do not go onto the stack but into the variables */
/* list (but without a name attached to them and the with IS_TEMP flag    */
/* being set) - this is needed to keep the convention working that an EDL */
/* functions gets one variable one the stack for each of its arguments.   */
/*------------------------------------------------------------------------*/

static void vars_ref_copy( Var *nsv, Var *src, bool exact_copy )
{
	if ( ! exact_copy )
		nsv->flags |= IS_DYNAMIC | IS_TEMP;
	if ( nsv->type == INT_REF && src->type == FLOAT_REF )
		nsv->type = FLOAT_REF;
	nsv->from = src;
	nsv->dim = src->dim;
	nsv->len = src->len;

	if ( nsv->len != 0 )
		vars_ref_copy_create( nsv, src, exact_copy );
	else
		nsv->val.vptr = NULL;
}


/*------------------------------------------------------*/
/* This function does the real work for vars_ref_copy() */
/* and gets called recursively if necessary.            */
/*------------------------------------------------------*/

static void vars_ref_copy_create( Var *nsv, Var *src, bool exact_copy )
{
	Var *vd;
	ssize_t i;


	/* If we're already at the lowest level, i.e. there are only one-
	   dimensional arrays copy the contents. Then we're done. */

	if ( src->type & ( INT_ARR | FLOAT_ARR ) )
	{
		nsv->dim = 1;

		if ( src->type == INT_ARR )
		{
			if ( INT_TYPE( nsv ) )
			{
				nsv->type = INT_ARR;
				if ( nsv->len != 0 )
				{
					nsv->val.lpnt = LONG_P T_malloc(   nsv->len 
													 * sizeof *nsv->val.lpnt );
					memcpy( nsv->val.lpnt, src->val.lpnt,
							nsv->len * sizeof *nsv->val.lpnt );
				}
				else
					nsv->val.lpnt = NULL;
			}
			else
			{
				nsv->type = FLOAT_ARR;
				if ( nsv->len != 0 )
				{
					nsv->val.dpnt = DOUBLE_P T_malloc( nsv->len 
													 * sizeof *nsv->val.dpnt );
					for ( i = 0; i < nsv->len; i++ )
						nsv->val.dpnt[ i ] = ( double ) src->val.lpnt[ i ];
				}
				else
					nsv->val.dpnt = NULL;
			}
		}
		else
		{
			nsv->type = FLOAT_ARR;
			if ( nsv->len != 0 )
			{
				nsv->val.dpnt = DOUBLE_P T_malloc(   nsv->len 
												   * sizeof *nsv->val.dpnt );
				memcpy( nsv->val.dpnt, src->val.dpnt,
						nsv->len * sizeof *nsv->val.dpnt );
			}
			else
				nsv->val.dpnt = NULL;
		}

		return;
	}

	/* Otherwise create as many new sub-matrices as necessary and then
	   recurse, going to the next lower level. */

	if ( nsv->len == 0 )
	{
		nsv->val.vptr = NULL;
		return;
	}

	nsv->val.vptr = VAR_PP T_malloc( nsv->len * sizeof *nsv->val.vptr );
	for ( i = 0; i < nsv->len; i++ )
		nsv->val.vptr[ i ] = NULL;

	for ( i = 0; i < src->len; i++ )
	{
		/* If some of the sub-matrices of the source matrix do not exist
		   (i.e. are still completely non-existent or have zero length )
		   we don't create a copy... */

		if ( src->val.vptr[ i ] == NULL || src->val.vptr[ i ]->len == 0 )
		{
			nsv->val.vptr[ i ] = NULL;
			continue;
		}

		vd = nsv->val.vptr[ i ] = vars_new( NULL );
		vd->from = nsv;
		vd->flags &= ~ NEW_VARIABLE;
		if ( ! exact_copy )
			vd->flags |= IS_DYNAMIC | IS_TEMP;
		vd->len = src->val.vptr[ i ]->len;
		vd->type = nsv->type;
		vd->from = nsv;
		vd->dim = src->val.vptr[ i ]->dim;

		vars_ref_copy_create( vd, src->val.vptr[ i ], exact_copy );
	}
}


/*-----------------------------------------------------------------*/
/* vars_pop() checks if a variable is on the variable stack and    */
/* if it does removes it from the linked list making up the stack  */
/* To make running through a list of variables easier for some     */
/* functions, this function returns a pointer to the next variable */
/* on the stack (if the popped variable was on the stack and had a */
/* successor).                                                     */
/*-----------------------------------------------------------------*/

Var *vars_pop( Var *v )
{
	Var *ret = NULL;
	ssize_t i;
#ifndef NDEBUG
	Var *stack,
		*prev = NULL;
#endif


	/* Check that this is a variable that can be popped from the stack */

	if ( v == NULL || ! ( v->flags & ON_STACK ) )
		return NULL;

#ifndef NDEBUG
	/* Figure out if 'v' is really on the stack - otherwise we have found
	   a new bug */

	for ( stack = EDL.Var_Stack; stack && stack != v; stack = stack->next )
		prev = stack;

	if ( stack == NULL )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	/* Now get rid of the variable */

	ret = v->next;

	if ( v->prev != NULL )
	{
		v->prev->next = v->next;
		if ( v->next != NULL )
			v->next->prev = v->prev;
	}
	else
	{
		EDL.Var_Stack = v->next;
		if ( v->next != NULL )
			v->next->prev = NULL;
	}

	switch( v->type )
	{
		case STR_VAR :
			T_free( v->val.sptr );
			break;

		case FUNC :
			T_free( v->name );
			break;

		case INT_ARR :
			T_free( v->val.lpnt );
			break;

		case FLOAT_ARR :
			T_free( v->val.dpnt );
			break;

		case INT_REF : case FLOAT_REF :
			if ( ! ( v->flags & DONT_RECURSE ) )
			{
				for ( i = 0; i < v->len; i++ )
					if ( v->val.vptr[ i ] != NULL )
						vars_free( v->val.vptr[ i ], SET );
			}
			T_free( v->val.vptr );
	}

	T_free( v );
	return ret;
}


/*-------------------------------------------------------------------*/
/* vars_check() first checks that the variable passed to it as the   */
/* first argument really exists and then tests if the variable has   */
/* the type passed as the second argument. The type to be tested for */
/* can not only be a simple type but tests for different types at    */
/* once are possible by specifying the types logically ored, i.e. to */
/* test if a variable has integer or floating point type use         */
/*               INT_VAR | FLOAT_VAR                                 */
/* as the second argument.                                           */
/*-------------------------------------------------------------------*/

void vars_check( Var *v, int type )
{
	int i;
	int t;
	const char *types[ ] = { "STRING", "INTEGER", "FLOAT", "1D INTEGER ARRAY",
							 "1D FLOAT ARRAY", "INTEGER MATRIX",
							 "FLOAT MATRIX", "INTEGER REFERENCE",
							 "FLOAT REFERENCE", "ARRAY REFERENCE",
							 "FUNCTION" };


	/* Someone might call the function with a NULL pointer - handle this
	   gracefully, i.e. by throwing an exception and don't crash (even
	   though this clearly is a bug) */

#ifndef NDEBUG
	if ( v == NULL )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	/* Being real paranoid we check that the variable exists at all -
	   probably this can vanish later. */

	fsc2_assert( vars_exist( v ) );

	/* Check that the variable has a value assigned to it */

	if ( v->type == UNDEF_VAR )
	{
		fsc2_assert( v->name != NULL );         /* just a bit paranoid ? */

		print( FATAL, "The accessed variable '%s' has not been assigned a "
			   "value.\n", v->name );
		THROW( EXCEPTION );
	}

	/* Check that the variable has the correct type */

	if ( ! ( v->type & type ) )
	{
		i = 1;
		t = v->type;
		while ( ! ( ( t >>= 1 ) & 1 ) )
			i++;
		print( FATAL, "Variable of type %s can't be used in this context.\n",
			   types[ i ] );
		THROW( EXCEPTION );
	}

	if ( v->name != NULL && v->flags & NEW_VARIABLE )
	{
		print( WARN, "Variable '%s' has not been assigned a value.\n",
			   v->name );
		THROW( EXCEPTION );
	}
}


/*---------------------------------------------------------------*/
/* vars_exist() checks if a variable really exists by looking it */
/* up in the variable list or on the variable stack (depending   */
/* on what type of variable it is).                              */
/*---------------------------------------------------------------*/

bool vars_exist( Var *v )
{
	Var *lp;


	if ( v->flags & ON_STACK )
		for ( lp = EDL.Var_Stack; lp != NULL && lp != v; lp = lp->next )
			/* empty */ ;
	else
		for ( lp = EDL.Var_List; lp != NULL && lp != v; lp = lp->next )
			/* empty */ ;

	/* If the variable can't be found in the lists we've got a problem... */

	fsc2_assert( lp == v );
	return lp == v;
}


/*-------------------------------------------------------------------*/
/* This function is used to iterate over all elements of a (more-    */
/* dimensional) array. On each call a (void) pointer to the next     */
/* element of the array is returned. When there are no more elements */
/* a NULL pointer gets returned.                                     */
/* Important: the function *must* be called until NULL gets returned */
/*            (i.e. there are no elements left) or, if this can't be */
/*            done, must be called with a NULL argument.             */
/*-------------------------------------------------------------------*/

void *vars_iter( Var *v )
{
	static ssize_t *iter = NULL;
	void *ret;


	/* If called with a NULL argument just reset the iter array */

	if ( v == NULL )
	{
		if ( iter != NULL )
			iter = SSIZE_T_P T_free( iter );
		return NULL;
	}

	/* If this is the first call of a sequence set up the iter array */

	if ( iter == NULL )
	{
		iter = T_calloc( v->dim, sizeof *iter );
		iter[ v->dim - 1 ] = -1;
	}

	/* Increment the index for the lowest dimension */

	iter[ v->dim - 1 ]++;

	/* Find the element associated with the indices in iter, reset the iter
	   array when there were no more elements */

	if ( ( ret = vars_get_pointer( iter, 0, v ) ) == NULL )
	{
		iter = SSIZE_T_P T_free( iter );
		return NULL;
	}

	return ret;
}


/*--------------------------------------------------------------*/
/* Returns a pointer to an element within an array or matrix    */
/* as indxed by the 'iter' array. If the indeces in the 'iter'  */
/* array are to large it corrects them by stepping up the index */
/* one dimension "further up". If there is no more following    */
/* element a NULL pointer is returned.                          */
/*--------------------------------------------------------------*/

static void *vars_get_pointer( ssize_t *iter, ssize_t depth, Var *p )
{
	/* If the index for the current dimension is too large reset it to
	   0 and increment the index for the next higher dimension (if we're
	   already at the top level we have returned all elements of the array
	   and thus return a NULL pointer). */

	if ( iter[ depth ] >= p->len )
	{
		if ( depth == 0 )
			return NULL;
		else
		{
			iter[ depth ] = 0;
			iter[ --depth ]++;
			return vars_get_pointer( iter, depth, p->from );
		}
	}

	/* If we're already at the lowest level return a pointer to the
	   element, otherwise go down another level. */

	if ( p->type == INT_ARR )
		return ( void * ) ( p->val.lpnt + iter[ depth ] );
	else if ( p->type == FLOAT_ARR )
		return ( void * ) ( p->val.dpnt + iter[ depth ] );
	else
		return vars_get_pointer( iter, ++depth, p->val.vptr[ iter[ depth ] ] );
}


/*------------------------------------------------------------------------*/
/* This functions saves or restores all variables depending on 'flag'. If */
/* 'flag' is set the variables are saved, otherwise they are copied back  */
/* from the backup into the normal variables space.                       */
/* This function is needed to save the state of all variables during the  */
/* test run and then reset it afterwards.                                 */
/*------------------------------------------------------------------------*/

void vars_save_restore( bool flag )
{
	Var *src;
	Var *cpy;
	static Var *cpy_area = NULL;
	static bool exists_copy = UNSET;
	ssize_t var_count;
	ssize_t i;


	if ( flag )
	{
		fsc2_assert( ! exists_copy );                  /* don't save twice ! */

		if ( EDL.Var_List == NULL )
		{
			exists_copy = SET;
			return;
		}

		for ( var_count = 0, src = EDL.Var_List; src != NULL; src = src->next )
			var_count++;

		cpy_area = VAR_P T_malloc( var_count * sizeof *cpy_area );

		for ( cpy = cpy_area, src = EDL.Var_List; src != NULL;
			  src = src->next, cpy++ )
		{
			memcpy( cpy, src, sizeof *src );

			switch ( src->type )
			{
				case UNDEF_VAR : case INT_VAR : case FLOAT_VAR :
					break;

				case INT_ARR :
					if ( src->len == 0 )
						break;
					cpy->val.lpnt = NULL;
					cpy->val.lpnt = LONG_P get_memcpy( src->val.lpnt,
											src->len * sizeof *src->val.lpnt );
					break;

				case FLOAT_ARR :
					if ( src->len == 0 )
						break;
					cpy->val.dpnt = NULL;
					cpy->val.dpnt = LONG_P get_memcpy( src->val.dpnt,
											src->len * sizeof *src->val.dpnt );
					break;

				case INT_REF : case FLOAT_REF :
					if ( src->len == 0 )
						break;
					cpy->val.vptr = NULL;
					cpy->val.vptr = VAR_PP get_memcpy( src->val.vptr,
											src->len * sizeof *src->val.vptr );
					break;

				default :
					fsc2_assert( 1 == 0 );
			}

			src->flags |= EXISTS_BEFORE_TEST;
		}

		exists_copy = SET;
	}
	else
	{
		fsc2_assert( exists_copy );             /* no restore without save ! */

		/* Remove all submatrices that got created during the test run */

		for ( cpy = EDL.Var_List; cpy != NULL; cpy = cpy->next )
		{
			if ( cpy->name == NULL ||
				 ! ( cpy->type & ( INT_REF | FLOAT_REF ) ) )
				continue;

			for ( i = 0; i < cpy->len; i++ )
				if ( cpy->val.vptr != NULL &&
					 ! ( cpy->val.vptr[ i ]->flags & EXISTS_BEFORE_TEST ) )
					vars_free( cpy->val.vptr[ i ], SET );
		}

		/* Reset all variables to what they were before the rest run */

		for ( cpy = EDL.Var_List, src = cpy_area; cpy != NULL;
			  cpy = cpy->next, src++ )
		{
			switch ( src->type )
			{
				case UNDEF_VAR : case INT_VAR : case FLOAT_VAR :
					break;

				case INT_ARR :
					if ( cpy->len != 0 )
						cpy->val.lpnt = LONG_P T_free( cpy->val.lpnt );
					break;

				case FLOAT_ARR :
					if ( cpy->len != 0 )
						cpy->val.dpnt = DOUBLE_P T_free( cpy->val.dpnt );
					break;

				case INT_REF : case FLOAT_REF :
					if ( cpy->len != 0 )
						cpy->val.vptr = VAR_PP T_free( cpy->val.vptr );
					break;

				default :
					fsc2_assert( 1 == 0 );
			}

			memcpy( cpy, src, sizeof *src );
		}

		cpy_area = VAR_P T_free( cpy_area );
		exists_copy = UNSET;
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
