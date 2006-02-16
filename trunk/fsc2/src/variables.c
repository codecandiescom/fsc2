/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
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
   size or a dynamically adjustable size. In the later case the size will
   change automatically when an 1D-array of a different size is assigned
   to the array (the size can get both larger and smaller!) or when a value
   is assigned to an element that didn't exist yet (in which case the size
   will grow if necessary to allow storing the new element). The 'len'
   field of the Var structures allows to determine the current length of
   the array (for dynamically sized arrays it's legal to have 'len' set
   to 0, in this case the size hasn't been set yet). 

   To allow for two- and more dimensional arrays there are two further
   variable types

     INT_REF
	 FLOAT_REF

   These variables contain pointers to further variables of the same kind
   (only with the 'dim' field set to a value smaller by 1) or, for the
   lowest dimension, INT_ARRs or FLOAT_ARRs. Again, there are two flavors,
   fixed and dynamically sized references. The 'dim' field of the Var
   structure allows to determine the dimensionality of the array, the
   'len' field the size of the current dimension.

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
   reference have different lengths, i.e. the definition

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

   Finally, if on the left hand side an array slice (or subatrix) is found
   a variable of the type

   SUB_REF_PTR

   is pushed onto the stack. Its 'from' member points to the original matrix
   and the 'val.index' array the indices and ranges.

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

static void free_all_vars( void );
static Var_T *vars_push_submatrix( Var_T *    from,
								   Var_Type_T type,
								   int        dim,
								   ssize_t *  sizes );
static void vars_ref_copy( Var_T * nsv,
						   Var_T * cp,
						   bool    exact_copy );
static void vars_ref_copy_create( Var_T * nsv,
								  Var_T * src,
								  bool    exact_copy );
static void *vars_get_pointer( ssize_t * iter,
							   ssize_t   depth,
							   Var_T *   p );


/*----------------------------------------------------------------------*
 * vars_get() is called when a token is found that might be a variable
 * identifier. The function checks if it is an already defined variable
 * and in this case returns a pointer to the corresponding structure,
 * otherwise it returns NULL.
 * ->
 *   * string with name of variable
 * <-
 *   * pointer to VAR structure or NULL
 *----------------------------------------------------------------------*/

Var_T *vars_get( const char * name )
{
	Var_T *v;


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


/*----------------------------------------------------------*
 * vars_new() sets up a new variable by getting memory for
 * a variable structure and setting the important elements.
 * ->
 *   * string with variable name
 * <-
 *   * pointer to variable structure
 *----------------------------------------------------------*/

Var_T *vars_new( const char * name )
{
	static Var_T template = { NULL, UNDEF_VAR, { 0 }, 0, 0,
							  NEW_VARIABLE, NULL, NULL, NULL };
	Var_T *vp;


	/* Get memory for a new variable structure, initialize it (from a
	   template variable, that might be a bit faster than setting all
	   elements individually), and get memory for storing the name */

	vp = VAR_P T_malloc( sizeof *vp );

	*vp = template;

	if ( name != NULL )
		vp->name    = T_strdup( name );

	/* Make the new variable the first element of the variable list */

	vp->next = EDL.Var_List;
	if ( EDL.Var_List != NULL )      /* set previous pointer in successor */
		EDL.Var_List->prev = vp;     /* (if this isn't the very first) */
    EDL.Var_List = vp;               /* make it the head of the list */
	
	return vp;
}


/*-------------------------------------------------------------------*
 * This function is called when a 'VAR_TOKEN [' combination is found
 * in the input. For a new array it sets its type. Everything else
 * it does is pushing a variable with a pointer to the array onto
 * the stack.
 *-------------------------------------------------------------------*/


Var_T *vars_arr_start( Var_T * v )
{
	if ( v->type != UNDEF_VAR )
		vars_check( v, INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF );

	/* Push variable with generic pointer to an array onto the stack */

	return vars_push( REF_PTR, v );
}


/*--------------------------------------------------------------*
 * Removes a variable from the linked list of variables, Unless
 * the flag passed as the second argument is set only variables
 * with names get removed (this is used to control if also sub-
 * variables for more-dimensional arrays get deleted).
 *--------------------------------------------------------------*/

Var_T *vars_free( Var_T * v,
				  bool    also_nameless )
{
	ssize_t i;
	Var_T *ret;


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
			if ( ! ( v->flags & DONT_RECURSE ) )
				for ( i = 0; i < v->len; i++ )
					if ( v->val.vptr[ i ] != NULL )
						vars_free( v->val.vptr[ i ], SET );
			v->val.vptr = VAR_PP T_free( v->val.vptr );
			break;

		default :
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


/*---------------------------------------------------------------*
 * free_vars() removes all variables from the list of variables.
 *---------------------------------------------------------------*/

static void free_all_vars( void )
{
	Var_T *v;


	for ( v = EDL.Var_List; v != NULL; )
		v = vars_free( v, UNSET );
}


/*----------------------------------------------------------------*
 * vars_del_stack() deletes all entries from the variables stack.
 *----------------------------------------------------------------*/

void vars_del_stack( void )
{
	while ( vars_pop( EDL.Var_Stack ) )
		/* empty */ ;
}


/*----------------------------------------------------------*
 * vars_clean_up() deletes the variable and array stack and
 * removes all variables from the list of variables
 *----------------------------------------------------------*/

void vars_clean_up( void )
{
	vars_del_stack( );
	free_all_vars( );
	vars_iter( NULL );
}


/*-------------------------------------------------------------*
 * Pushes a copy of a variable onto the stack (but only if the
 * variable to be copied isn't already a stack variable).
 *-------------------------------------------------------------*/

Var_T *vars_push_copy( Var_T * v )
{
	Var_T *nv = NULL;


	if ( v->flags & ON_STACK )
		return v;

	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			nv = vars_push( v->type, v->val.lval );
			break;

		case FLOAT_VAR :
			nv = vars_push( v->type, v->val.dval );
			break;

		case INT_ARR :
			nv = vars_push( v->type, v->val.lpnt, v->len );
			break;

		case FLOAT_ARR :
			nv = vars_push( v->type, v->val.dpnt, v->len );
			break;

		case INT_REF : case FLOAT_REF :
			nv = vars_push( v->type, v );
			break;

		default:
			fsc2_assert( 1 == 0 );
	}

	return nv;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

Var_T *vars_push_matrix( Var_Type_T type,
						 int        dim,
						 ... )
{
	Var_T *nv;
	va_list ap;
	ssize_t *sizes;
	ssize_t i;


#ifndef NDEBUG
	if ( ! ( type & ( INT_REF | FLOAT_REF ) ) || dim < 2 )
		fsc2_assert( 1 == 0 );
#endif

	nv = vars_push( type, NULL );
	nv->from = NULL;

	sizes = SSIZE_T_P T_malloc( dim * sizeof *sizes );

	va_start( ap, dim );
	
	for ( i = 0; i < dim; i++ )
	{
		sizes[ i ] = ( ssize_t ) va_arg( ap, long );

#ifndef NDEBUG
		if ( sizes[ i ] == 0 )
			fsc2_assert( 1 == 0 );
#endif
	}

	va_end( ap );

	TRY
	{
		nv->val.vptr = VAR_PP T_malloc( sizes[ 0 ] * sizeof *nv->val.vptr );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( sizes );
		RETHROW( );
	}

	for ( i = 0; i < sizes[ 0 ]; i++ )
		nv->val.vptr[ i ] = NULL;

	TRY
	{
		for ( i = 0; i < sizes[ 0 ]; i++ )
			nv->val.vptr[ i ] = vars_push_submatrix( nv, type, dim - 1,
													 sizes + 1 );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		for ( i = 0; i < sizes[ 0 ] && nv->val.vptr[ i ] != NULL; i++ )
			vars_free( nv->val.vptr[ i ], SET );
		T_free( sizes );
		RETHROW( );
	}

	nv->len = sizes[ 0 ];
	nv->dim = dim;

	T_free( sizes );
	return nv;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static Var_T *vars_push_submatrix( Var_T *    from,
								   Var_Type_T type,
								   int        dim,
								   ssize_t *  sizes )
{
	Var_T *nv;
	ssize_t i;


	nv = vars_new( NULL );
	nv->flags |= IS_TEMP;
	nv->from = from;
	nv->dim = dim;
	nv->len = sizes[ 0 ];

	if ( dim == 1 )
	{
		if ( type == INT_REF )
		{
			nv->type = type = INT_ARR;
			nv->val.lpnt = LONG_P T_calloc( nv->len, sizeof *nv->val.lpnt );
		}
		else
		{
			nv->type = FLOAT_ARR;
			nv->val.dpnt = DOUBLE_P T_malloc( nv->len * sizeof *nv->val.dpnt );
			for ( i = 0; i < nv->len; i++ )
				nv->val.dpnt[ i ] = 0.0;
		}

		return nv;
	}

	nv->type = type;
	nv->val.vptr = VAR_PP T_malloc( nv->len * sizeof *nv->val.vptr );

	for ( i = 0; i < nv->len; i++ )
		nv->val.vptr[ i ] = NULL;

	for ( i = 0; i < nv->len; i++ )
		nv->val.vptr[ i ] = vars_push_submatrix( nv, type,
												 dim - 1, sizes + 1 );

	return nv;
}


/*-----------------------------------------------------------------------*
 * vars_push() creates a new entry on the variable stack (which actually
 * is not really a stack but a linked list) and sets its type and value.
 * The following sets of arguments are possible:
 * INT_VAR, long                      value is the value to be assigned
 * FLOAT_VAR, double                  value is the value to be assigned
 * INT_ARR, long * or NULL, long      first value is pointer to array to
 *                                    be assigned or NULL for initiali-
 *                                    zation with 0, second length of
 *                                    array
 * FLOAT_ARR, double * or NULL, long  first value is pointer to array to
 *                                    be assigned or NULL for initiali-
 *                                    zation with 0, second length of
 *                                    array
 * INT_PTR, long *
 * FLOAT_PTR, long *
 * INT_REF, Var_T *
 * FLOAT_REF, Var_T *
 * REF_PTR, Var_T *
 * FUNC_PTR, struct Func_T *
 *-----------------------------------------------------------------------*/

Var_T *vars_push( Var_Type_T type,
				  ... )
{
	Var_T *nsv, *stack, *src;
	va_list ap;
	ssize_t i;
	const char *str;


	/* Get memory for the new variable to be appended to the stack, set its
	   type and initialize some fields */

	nsv         = VAR_P T_malloc( sizeof *nsv );
	nsv->name   = NULL;
	nsv->type   = type;
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
			str = va_arg( ap, const char * );
			if ( str != NULL )
				nsv->val.sptr = T_strdup( str );
			else
				nsv->val.sptr = NULL;
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
				{
					nsv->val.dpnt = DOUBLE_P T_malloc( nsv->len *
													   sizeof *nsv->val.dpnt );
					for ( i = 0; i < nsv->len; i++ )
						nsv->val.dpnt[ i ] = 0.0;
				}
			}
			break;

		case INT_PTR :
			nsv->val.lpnt = va_arg( ap, long * );
			break;

		case FLOAT_PTR :
			nsv->val.dpnt = va_arg( ap, double * );
			break;

		case INT_REF : case FLOAT_REF :
			src = va_arg( ap, Var_T * );
			if ( src != NULL )
				vars_ref_copy( nsv, src, UNSET );
			break;

		case SUB_REF_PTR :
			nsv->len = va_arg( ap, ssize_t );
			nsv->val.index = SSIZE_T_P T_malloc( nsv->len *
												 sizeof *nsv->val.index );
			break;

		case REF_PTR :
			nsv->from = va_arg( ap, Var_T * );
			break;

		case FUNC :
			nsv->val.fnct = va_arg( ap, struct Func * );
			break;

		default :
			fsc2_assert( 1 == 0 );     /* This can't happen... */
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


/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/

Var_T *vars_make( Var_Type_T type,
				  Var_T *    src )
{
	Var_T *nv = NULL;
	Var_T *stack;
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
			break;

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
			break;

		case INT_REF : case FLOAT_REF :
			nv->dim = src->dim;
			if ( src->len != 0 )
			{
				nv->val.vptr = VAR_PP T_malloc(   src->len
												* sizeof *nv->val.vptr );
				for ( nv->len = 0; nv->len < src->len; nv->len++ )
					nv->val.vptr[ nv->len ] = NULL;
			}
			else
			{
				nv->val.vptr = NULL;
				nv->len = 0;
			}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return nv;
}


/*------------------------------------------------------------------------*
 * Function for making a copy of an INT_REF or FLOAT_REF via vars_push().
 * This also works with making a FLOAT type copy of an INT type matrix.
 * Note that sub-matrices do not go onto the stack but into the variables
 * list (but without a name attached to them and with the IS_TEMP flag
 * being set) - this is needed to keep the convention working that an EDL
 * functions gets one variable one the stack for each of its arguments.
 *------------------------------------------------------------------------*/

static void vars_ref_copy( Var_T * nsv,
						   Var_T * src,
						   bool    exact_copy )
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


/*------------------------------------------------------*
 * This function does the real work for vars_ref_copy()
 * and gets called recursively if necessary.
 *------------------------------------------------------*/

static void vars_ref_copy_create( Var_T * nsv,
								  Var_T * src,
								  bool    exact_copy )
{
	Var_T *vd;
	ssize_t i;


	/* If we're already at the lowest level, i.e. there are only one-
	   dimensional arrays, copy the contents. Then we're done. */

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

	TRY
	{
		nsv->val.vptr = VAR_PP T_malloc( nsv->len * sizeof *nsv->val.vptr );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		nsv->len = 0;
		RETHROW( );
	}

	for ( i = 0; i < nsv->len; i++ )
		nsv->val.vptr[ i ] = NULL;

	for ( i = 0; i < nsv->len; i++ )
	{
		vd = nsv->val.vptr[ i ] = vars_new( NULL );
		vd->from = nsv;
		vd->flags &= ~ NEW_VARIABLE;
		if ( ! exact_copy )
			vd->flags |= IS_DYNAMIC | IS_TEMP;
		vd->len = src->val.vptr[ i ]->len;
		vd->type = nsv->type;
		vd->dim = src->val.vptr[ i ]->dim;

		vars_ref_copy_create( vd, src->val.vptr[ i ], exact_copy );
	}
}


/*-----------------------------------------------------------------*
 * vars_pop() checks if a variable is on the variable stack and
 * if it does removes it from the linked list making up the stack
 * To make running through a list of variables easier for some
 * functions, this function returns a pointer to the next variable
 * on the stack (if the popped variable was on the stack and had a
 * successor).
 *-----------------------------------------------------------------*/

Var_T *vars_pop( Var_T * v )
{
	Var_T *ret = NULL;
	ssize_t i;
#ifndef NDEBUG
	Var_T *stack,
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
		fsc2_assert( 1 == 0 );
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
			break;

		case SUB_REF_PTR :
			T_free( v->val.index );
			break;

		default :
			break;
	}

	T_free( v );
	return ret;
}


/*-------------------------------------------------------------------*
 * vars_check() first checks that the variable passed to it as the
 * first argument really exists and then tests if the variable has
 * the type passed as the second argument. The type to be tested for
 * can not only be a simple type but tests for different types at
 * once are possible by specifying the types logically ored, i.e. to
 * test if a variable has integer or floating point type use
 *               INT_VAR | FLOAT_VAR
 * as the second argument.
 *-------------------------------------------------------------------*/

void vars_check( Var_T * v,
				 int     types )
{
	int i;
	int t;
	const char *type_names[ ] = { "STRING", "INTEGER", "FLOAT",
								  "1D INTEGER ARRAY", "1D FLOAT ARRAY",
								  "INTEGER MATRIX", "FLOAT MATRIX",
								  "INTEGER REFERENCE", "FLOAT REFERENCE",
								  "SUBARRAY REFERENCE", "ARRAY REFERENCE",
								  "FUNCTION" };


#ifndef NDEBUG
	/* Someone might call the function with a NULL pointer - handle this
	   gracefully, i.e. by throwing an exception and don't crash (even
	   though this clearly is a bug) */

	if ( v == NULL )
		fsc2_assert( 1 == 0 );

	/* Being real paranoid we check that the variable exists at all -
	   probably this can vanish later. */

	if ( ! vars_exist( v ) )
		fsc2_assert( 1 == 0 );
#endif

	/* Check that the variable has a value assigned to it */

	if ( v->type == UNDEF_VAR )
	{
		fsc2_assert( v->name != NULL );         /* just a bit paranoid ? */

		print( FATAL, "The accessed variable '%s' has not been assigned a "
			   "value.\n", v->name );
		THROW( EXCEPTION );
	}

	/* Check that the variable has the correct type */

	if ( ! ( v->type & types ) )
	{
		for ( i = 0, t = v->type; ! ( t & 1 ); t >>= 1, i++ )
			/* empty */ ;
		if ( v->name != NULL )
			print( FATAL, "The variable '%s' of type %s can't be used in "
				   "this context.\n", v->name, type_names[ i ] );
		else
			print( FATAL, "Variable of type %s can't be used in this "
				   "context.\n", type_names[ i ] );
		THROW( EXCEPTION );
	}

	if ( v->name != NULL && v->flags & NEW_VARIABLE )
	{
		print( WARN, "Variable '%s' has not been assigned a value.\n",
			   v->name );
		THROW( EXCEPTION );
	}
}


/*---------------------------------------------------------------*
 * vars_exist() checks if a variable really exists by looking it
 * up in the variable list or on the variable stack (depending
 * on what type of variable it is).
 *---------------------------------------------------------------*/

bool vars_exist( Var_T * v )
{
	Var_T *lp;


	fsc2_assert( v != NULL );

	if ( v->flags & ON_STACK )
		for ( lp = EDL.Var_Stack; lp != NULL && lp != v; lp = lp->next )
			/* empty */ ;
	else
		for ( lp = EDL.Var_List; lp != NULL && lp != v; lp = lp->next )
			/* empty */ ;

	return lp == v;
}


/*-------------------------------------------------------------------*
 * This function is used to iterate over all elements of a (more-
 * dimensional) array. On each call a (void) pointer to the next
 * element of the array is returned. When there are no more elements
 * a NULL pointer gets returned.
 * Important: the function *must* be called until NULL gets returned
 *            (i.e. there are no elements left) or, if this can't be
 *            done, must be called with a NULL argument.
 *-------------------------------------------------------------------*/

void *vars_iter( Var_T * v )
{
	static ssize_t *iter = NULL;
	void *ret;
	ssize_t i;


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
		iter = SSIZE_T_P T_malloc( v->dim * sizeof *iter );
		for ( i = 0; i < v->dim - 1; i++ )
			iter[ i ] = 0;
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


/*--------------------------------------------------------------*
 * Returns a pointer to an element within an array or matrix
 * as indexed by the 'iter' array. If the indices in the 'iter'
 * array are to large it corrects them by stepping up the index
 * one dimension "further up". If there is no more following
 * element a NULL pointer is returned.
 *--------------------------------------------------------------*/

static void *vars_get_pointer( ssize_t * iter,
							   ssize_t   depth,
							   Var_T *   p )
{
	Var_T *p_next;


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
	{
		p_next = p->val.vptr[ iter[ depth ] ];
		return vars_get_pointer( iter, ++depth, p_next );
	}
}


/*------------------------------------------------------------------------*
 * This functions saves or restores all variables depending on 'flag'. If
 * 'flag' is set the variables are saved, otherwise they are copied back
 * from the backup into the normal variables space.
 * This function is needed to save the state of all variables during the
 * test run and then reset it afterwards.
 *------------------------------------------------------------------------*/

void vars_save_restore( bool flag )
{
	Var_T *src;
	Var_T *cpy;
	static Var_T *cpy_area = NULL;
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
					cpy->val.dpnt = DOUBLE_P get_memcpy( src->val.dpnt,
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

		/* Remove all sub-matrices that got created during the test run */

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
