/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2.h"


/* locally used functions */


static int vars_check_lhs_indices( Var_T ** v,
                                   int *    range_count );
static Var_T * vars_setup_new_array( Var_T * v,
                                     int     dim );
#if 0
static Var_T * vars_init_elements( Var_T * a,
                                   Var_T * v );
#endif
static void vars_do_init( Var_T * src,
                          Var_T * dest );
static Var_T * vars_lhs_pointer( Var_T * v,
                                 int     dim );
static Var_T * vars_lhs_sub_pointer( Var_T * v,
                                     int     dim,
                                     int     range_count );
static Var_T * vars_lhs_simple_pointer( Var_T * a,
                                        Var_T * cv,
                                        Var_T * v,
                                        int     dim );
static Var_T * vars_lhs_range_pointer( Var_T * a,
                                       Var_T * cv,
                                       Var_T * v,
                                       int     dim );


/*----------------------------------------------------------------------*
 * The function is called when the end of an array access (indicated by
 * the ']') is found on the left hand side of an assignment. If it is
 * called for a new array the indices found on the stack are the sizes
 * for the different dimensions of the array and are used to set up the
 * the array.
 *----------------------------------------------------------------------*/

Var_T *
vars_arr_lhs( Var_T * v )
{
    int dim;
    int range_count = 0;


    /* Move up the stack until we find the variable indicating the array or
       matrix itself */

    dim = vars_check_lhs_indices( &v, &range_count );

    /* If the array is new (i.e. we're still in the VARIABLES section) set
       it up, otherwise return a pointer to the referenced element or a
       variables of type SUB_REF_PTR if the indices did contain ranges */

    if ( v->from->type == UNDEF_VAR )
        return vars_setup_new_array( v, dim );
    else if ( range_count == 0 )
        return vars_lhs_pointer( v, dim );
    else
        return vars_lhs_sub_pointer( v, dim, range_count );
}


/*--------------------------------------------------------------*
 * Picks the indices from the stack, does sanity checks on them
 * and converts ranges, consistiting of a pair of numbers with
 * a colon in between, into a pair of a positive and a negative
 * number.
 *--------------------------------------------------------------*/

static int
vars_check_lhs_indices( Var_T ** v,
                        int *    range_count )
{
    Var_T *cv = *v;
    Var_T *ref;
    int index_count = 0;


    /* If the index variable has the type UNDEF_VAR the whole array has to
       be used */

    if ( cv->type == UNDEF_VAR )
    {
        *v = cv->prev;
        fsc2_assert( ( *v )->type == REF_PTR );
        return 0;
    }

    while ( cv->type != REF_PTR && cv != NULL )
        cv = cv->prev;

    fsc2_assert( cv != NULL );

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

            if (    ( ! ( cv->flags & IS_DYNAMIC ) && cv->val.lval < 1 )
                 || cv->val.lval < 0 )
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


/*-----------------------------------------------------------------------*
 * Function is called when the closing ']' is found in the definition of
 * an array in the VARIABLES section. It creates the array as far as
 *  possible (i.e. if the sizes are specified including the sub-arrays).
 *-----------------------------------------------------------------------*/

static Var_T *
vars_setup_new_array( Var_T * v,
                      int     dim )
{
    Var_T *a = v->from;


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


/*----------------------------------------------------------------------*
 * Function creates a new array by creating recursively all sub-arrays.
 *----------------------------------------------------------------------*/

void
vars_arr_create( Var_T * a,
                 Var_T * v,
                 int     dim,
                 bool    is_temp )
{
    Var_T *c;
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
            a->val.lpnt = T_calloc( len, sizeof *a->val.lpnt );
        else
        {
            a->val.dpnt = T_malloc( len * sizeof *a->val.dpnt );
            for ( i = 0; i < len; i++ )
                a->val.dpnt[ i ] = 0.0;
        }

        a->len = len;
        return;
    }

    /* Otherwise we need an array of references to arrays of lower dimensions
       which then in turn must be created */

    a->val.vptr = T_malloc( len * sizeof *a->val.vptr );

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


/*---------------------------------------------------------------------*
 * Function gets called when a list of initializers (enclosed in curly
 * braces) gets found in the input of the VARIABLES section. Since the
 * initializer list can contain further list etc. the function can get
 * several times until we have reached the highest level list. During
 * this process the variable stack can become quite messed up and for
 * bookkeeping purposes two extra flags, INIT_ONLY and DONT_RECURSE,
 * are used with the stack variables involved. INIT_ONLY signifies
 * that the variable is on the stack for good reasons and DONT_RECURSE
 * tells the functions for popping variables not to delete sub-arrays
 * of the variable.
 *---------------------------------------------------------------------*/

Var_T *
vars_init_list( Var_T * volatile v,
                ssize_t          level )
{
    ssize_t count = 0;
    ssize_t i;
    Var_T * cv;
    Var_T * volatile nv;
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
            nv = vars_push( INT_ARR, NULL, ( long ) count );
            nv->flags |= INIT_ONLY;
            nv->val.lpnt = T_malloc( nv->len * sizeof *nv->val.lpnt );
            for ( i = 0; i < nv->len; i++, v = vars_pop( v ) )
                nv->val.lpnt[ i ] = v->val.lval;
        }
        else
        {
            nv = vars_push( FLOAT_ARR, NULL, ( long ) count );
            nv->flags |= INIT_ONLY;
            nv->val.dpnt = T_malloc( nv->len * sizeof *nv->val.dpnt );
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
        nv->val.vptr = T_malloc( nv->len * sizeof *nv->val.vptr );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        nv->len = 0;
        RETHROW;
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


/*--------------------------------------------------------------------------*
 * Function initializes a new array. When called the stack at 'v' contains
 * all the data for the initialization (the last data on top of the stack)
 * and, directly below the data, an REF_PTR to the array to be initialized.
 * If 'v' is NULL no initialization has to be done.
 *--------------------------------------------------------------------------*/

void
vars_arr_init( Var_T * v )
{
    Var_T *dest;


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


/*------------------------------------------------------------------*
 * Function does the assignment of data from an initializer list,
 * being distributed over the stack to the left hand side variable.
 *------------------------------------------------------------------*/

static void
vars_do_init( Var_T * src,
              Var_T * dest )
{
    ssize_t i;


    switch ( dest->type )
    {
        case INT_ARR :
            if ( dest->flags & IS_DYNAMIC )
            {
                dest->len = src->len;
                dest->val.lpnt = T_malloc( dest->len * sizeof *dest->val.lpnt );
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
                dest->val.dpnt = T_malloc( dest->len * sizeof *dest->val.dpnt );
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
                dest->val.vptr = T_malloc( src->len * sizeof *dest->val.vptr );
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
                dest->val.vptr = T_malloc( src->len * sizeof *dest->val.vptr );
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

        default :
            eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
                    __FILE__, __LINE__ );
            THROW( EXCEPTION );
    }
}


/*--------------------------------------------------------------------*
 * Initializes the elements of array or matrix 'a' with the values of
 * the variables on the stack (first of which is 'v').
 *--------------------------------------------------------------------*/

#if 0
static Var_T *
vars_init_elements( Var_T * a,
                    Var_T * v )
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
#endif


/*----------------------------------------------------------------------*
 * Function pushes a variable with a pointer to an array element on the
 * left hand side of an EDL statement onto the stack. Depending on what
 * the left hand side evaluates to this is either a variable of INT_PTR
 * or FLOAT_PTR type (when a singe element of an array or matrix is
 * addressed) or of type REF_PTR (if the LHS resolves to a simple array
 * or (sub-) matrix but the indices do not involve ranges).
 *----------------------------------------------------------------------*/

static Var_T *
vars_lhs_pointer( Var_T * v,
                  int     dim )
{
    Var_T *a = v->from;
    Var_T *cv;


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
            THROW( EXCEPTION );
        }
#endif
        return vars_push( REF_PTR, a );
    }

    /* Check that there aren't too many indices */

    if ( dim > a->dim )
    {
        print( FATAL, "Array '%s' has only %d dimensions but there are "
               "%d indices.\n", a->name, a->dim, dim );
        THROW( EXCEPTION );
    }

    cv = vars_lhs_simple_pointer( a, a, v, a->dim );

    while ( ( v = vars_pop( v ) ) != cv )
        /* empty */ ;

    return cv;
}


/*------------------------------------------------------------------------*
 * This function is invoked when the indices on the stack did contain one
 * or more ranges. It first checks that the indices/ranges fit the sizes
 * of the indexed aray (or enlarges it if necessary and possible) and the
 * pushes a variable of type SUB_REF_PTR onto ths stack that has a 'from'
 * field pointing to the indexed array and contains an array of all the
 * indices (with negative values for range start values).
 *------------------------------------------------------------------------*/

static Var_T *
vars_lhs_sub_pointer( Var_T * v,
                      int     dim,
                      int     range_count )
{
    Var_T *a = v->from;
    Var_T *sv;
    ssize_t i;


    v = vars_pop( v );

    /* Check that there aren't too many indices or ranges */

    if ( dim > a->dim )
    {
        print( FATAL, "Array '%s' has only %d dimension%s but there are "
               "%d indices/ranges.\n",
               a->name, a->dim, a->dim > 1 ? "s" : "",dim );
        THROW( EXCEPTION );
    }

    /* Call the function that determines the indexed subarray - that's only
       necessary because we need to make sure that the array sizes are large
       enough, the returned pointer is of no interest */

    if ( v->val.lval >= 0 )
        vars_pop( vars_lhs_simple_pointer( a, a, v, a->dim ) );
    else
        vars_pop( vars_lhs_range_pointer( a, a, v, a->dim ) );

    /* Create a variable of type SUB_REF_PTR instead */

    sv = vars_push( SUB_REF_PTR, dim + range_count );
    sv->from = a;

    for ( i = 0; v != sv; i++, v = vars_pop( v ) )
        sv->val.index[ i ] = v->val.lval;

    return sv;
}


/*-----------------------------------------------------------------*
 * Function to be called when a simple index (not part of a range)
 * is found on the stack.
 *-----------------------------------------------------------------*/

static Var_T *
vars_lhs_simple_pointer( Var_T * a,
                         Var_T * cv,
                         Var_T * v,
                         int     dim )
{
    ssize_t ind;
    ssize_t i;


    ind = v->val.lval;
    v = v->next;

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
            cv->val.vptr = T_realloc( cv->val.vptr,
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
                    cv->val.lpnt = T_realloc( cv->val.lpnt,
                                                ( ind + 1 )
                                              * sizeof *cv->val.lpnt );
                    memset( cv->val.lpnt + cv->len, 0,
                            ( ind - cv->len + 1 ) * sizeof *cv->val.lpnt );
                    break;

                case FLOAT_ARR :
                    cv->val.dpnt = T_realloc( cv->val.dpnt,
                                                ( ind + 1 )
                                              * sizeof *cv->val.dpnt );
                    for ( i = cv->len; i <= ind; i++ )
                        cv->val.dpnt[ i ] = 0.0;
                    break;

                default :
                    eprint( FATAL, UNSET, "Internal error detected at "
                            "%s:%d.\n", __FILE__, __LINE__ );
                    THROW( EXCEPTION );
            }
        }

        cv->len = ind + 1;
    }

    if ( v == NULL || v->type != INT_VAR )
        switch ( cv->type )
        {
            case INT_ARR :
                return vars_push( INT_PTR, cv->val.lpnt + ind );

            case FLOAT_ARR :
                return vars_push( FLOAT_PTR, cv->val.dpnt + ind );

            case INT_REF : case FLOAT_REF :
                return vars_push( REF_PTR, cv->val.vptr[ ind ] );

            default :
                eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
                        __FILE__, __LINE__ );
                THROW( EXCEPTION );
        }

    return v->val.lval >= 0 ?
                vars_lhs_simple_pointer( a, cv->val.vptr[ ind ], v, --dim ) :
                vars_lhs_range_pointer( a, cv->val.vptr[ ind ], v, --dim );
}


/*--------------------------------------------------------------------*
 * Function to be called when a range (consisting of first a negative
 * and a positive value) is found on the stack.
 *--------------------------------------------------------------------*/

static Var_T *
vars_lhs_range_pointer( Var_T * a,
                        Var_T * cv,
                        Var_T * v,
                        int     dim )
{
    ssize_t i, range_start, range_end;


    /* Determine start and end of range */

    range_start = - v->val.lval - 1;
    v = v->next;
    range_end = v->val.lval;
    v = v->next;

    /* If the indexed range is larger than the (currently treated) array but
       the array has a fixed size we need to give up */

    if ( ! ( cv->flags & IS_DYNAMIC ) && range_end >= cv->len )
    {
        print( FATAL, "Invalid range for array '%s'.\n", a->name );
        THROW( EXCEPTION );
    }

    /* Otherwise extend its size as necessary */

    if ( range_end >= cv->len )
    {
        if ( dim > 1 )
        {
            cv->val.vptr = T_realloc( cv->val.vptr,
                                        ( range_end + 1 )
                                      * sizeof *cv->val.vptr );

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
                    cv->val.lpnt = T_realloc( cv->val.lpnt,
                                                ( range_end + 1 )
                                              * sizeof *cv->val.lpnt );
                    memset( cv->val.lpnt + cv->len, 0,
                            ( range_end - cv->len + 1 )
                            * sizeof *cv->val.lpnt );
                    break;

                case FLOAT_ARR :
                    cv->val.dpnt = T_realloc( cv->val.dpnt,
                                                ( range_end + 1 )
                                              * sizeof *cv->val.dpnt );
                    for ( i = cv->len; i <= range_end; i++ )
                        cv->val.dpnt[ i ] = 0.0;
                    break;

#ifndef NDEBUG
                default :
                    eprint( FATAL, UNSET, "Internal error detected at "
                            "%s:%d.\n", __FILE__, __LINE__ );
                    THROW( EXCEPTION );
#endif
            }
        }

        cv->len = range_end + 1;
    }

    /* These return values are never going to be used... */

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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
