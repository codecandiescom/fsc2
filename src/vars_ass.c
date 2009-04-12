/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#include "fsc2.h"


/* locally used functions */


static long vars_assign_to_1d( Var_T * src,
                               Var_T * dest );
static long vars_assign_to_nd( Var_T * src,
                               Var_T * dest );
static long vars_assign_to_nd_from_1d( Var_T * src,
                                       Var_T * dest );
static long vars_assign_to_snd_from_1d( Var_T * src,
                                        Var_T * dest,
                                        Var_T * sub );
static long vars_assign_to_snd_range_from_1d( Var_T * dest,
                                              Var_T * sub,
                                              ssize_t cur,
                                              long    l,
                                              double  d );
static long vars_set_all( Var_T * v,
                          long    l,
                          double  d );
static long vars_assign_to_nd_from_nd( Var_T * src,
                                       Var_T * dest );
static void vars_size_check( Var_T * src,
                             Var_T * dest );
static long vars_assign_to_snd_from_nd( Var_T * src,
                                        Var_T * dest,
                                        Var_T * sub );
static long vars_assign_snd_range_from_nd( Var_T * dest,
                                           Var_T * sub,
                                           ssize_t cur,
                                           Var_T * src );
static long vars_arr_assign( Var_T * src,
                             Var_T * dest );
static long vars_arr_assign_1d( Var_T * src,
                                Var_T * dest );
static long vars_arr_assign_nd( Var_T * src,
                                Var_T * dest );
static long vars_assign_snd_range_from_nd_1( Var_T * dest,
                                             Var_T * src,
                                             Var_T * sub,
                                             ssize_t start,
                                             ssize_t end,
                                             ssize_t cur );
static long vars_assign_snd_range_from_nd_2( Var_T * dest,
                                             Var_T * src,
                                             Var_T * sub,
                                             ssize_t start,
                                             ssize_t end,
                                             ssize_t cur );


/*--------------------------------------------------------*
 * This is the central function for assigning new values.
 *--------------------------------------------------------*/

void
vars_assign( Var_T * src,
             Var_T * dest )
{
    long count = 0;


#ifndef NDEBUG
    /* Check that both variables exist and src has a reasonable type */

    if (    ! vars_exist( src )
         || ! vars_exist( dest )
         || src->type == STR_VAR
         || ! ( src->type & ( INT_VAR | FLOAT_VAR |
                              INT_ARR | FLOAT_ARR |
                              INT_REF | FLOAT_REF ) ) )
        fsc2_impossible( );
#endif

    /* First we distinguish between the different possible types of variables
       on the left hand side */

    switch ( dest->type )
    {
        case UNDEF_VAR :
        case INT_VAR : case FLOAT_VAR :
        case INT_PTR : case FLOAT_PTR :
            count = vars_assign_to_1d( src, dest );
            break;

        case INT_ARR :    case FLOAT_ARR :
        case INT_REF :    case FLOAT_REF :
        case SUB_REF_PTR: case REF_PTR :
            count = vars_assign_to_nd( src, dest );
            break;

        default :
            fsc2_impossible( );     /* This can't happen... */
    }

    vars_pop( dest );
    vars_pop( src );
}


/*----------------------------------------------------------*
 * Assignment to a variable or a single element of an array
 *----------------------------------------------------------*/

static long
vars_assign_to_1d( Var_T * src,
                   Var_T * dest )
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
                    fsc2_impossible( );
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
                    fsc2_impossible( );
            }
            break;

        default :
            fsc2_impossible( );
    }

    return 1;
}


/*-------------------------------------------------*
 * Assignment to an one- or more-dimensional array
 *-------------------------------------------------*/

static long
vars_assign_to_nd( Var_T * src,
                   Var_T * dest )
{
    long count = 0;


    switch ( src->type )
    {
        case INT_VAR : case FLOAT_VAR :
            switch ( dest->type )
            {
                case INT_ARR : case FLOAT_ARR :
                case INT_REF : case FLOAT_REF :
                    count = vars_assign_to_nd_from_1d( src, dest );
                    break;

                case SUB_REF_PTR :
                    count = vars_assign_to_snd_from_1d( src, dest->from,
                                                        dest );
                    break;

                case REF_PTR :
                    count = vars_assign_to_nd_from_1d( src, dest->from );
                    break;

                default :
                    fsc2_impossible( );
            }
            break;

        case INT_ARR : case FLOAT_ARR :
        case INT_REF : case FLOAT_REF :
            switch ( dest->type )
            {
                case INT_ARR : case FLOAT_ARR :
                case INT_REF : case FLOAT_REF :
                    count = vars_assign_to_nd_from_nd( src, dest );
                    break;

                case SUB_REF_PTR :
                    count = vars_assign_to_snd_from_nd( src, dest->from,
                                                        dest );
                    break;

                case REF_PTR :
                    count = vars_assign_to_nd_from_nd( src, dest->from );
                    break;

                default :
                    fsc2_impossible( );
            }
            break;

        default :
            fsc2_impossible( );     /* This can't happen... */
    }

    return count;
}


/*------------------------------------------------------------------*
 * Assignment of a single value to all elements of an one- or more-
 * dimensional array
 *------------------------------------------------------------------*/

static long
vars_assign_to_nd_from_1d( Var_T * src,
                           Var_T * dest )
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

    return vars_set_all( dest, lval, dval );
}


/*-----------------------------------------------------*
 * Assigns the same value to all elements of a one- or
 * more-dimensional array.
 *-----------------------------------------------------*/

static long
vars_set_all( Var_T * v,
              long    l,
              double  d )
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


/*-------------------------------------------------*
 * Assigns the same value to a slice of an one- or
 * more dimensional array
 *-------------------------------------------------*/

static long
vars_assign_to_snd_from_1d( Var_T * src,
                            Var_T * dest,
                            Var_T * sub )
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

    /* Do the actual assignment */

    return vars_assign_to_snd_range_from_1d( dest, sub, 0, lval, dval );
}


/*------------------------------------------------------------------*
 *------------------------------------------------------------------*/

static long
vars_assign_to_snd_range_from_1d( Var_T * dest,
                                  Var_T * sub,
                                  ssize_t cur,
                                  long    l,
                                  double  d )
{
    long count = 0;
    ssize_t i,
            ind,
            range_start,
            range_end;


    /* Descend into the submatrices of the destination matrix while there
       are non-negative indices and we're not at the last one */

    while ( sub->val.index[ cur ] >= 0 && cur < sub->len - 1 )
        dest = dest->val.vptr[ sub->val.index[ cur++ ] ];

    /* If we're at the last index the assignment must be done */

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

            count = 1;
        }

        return count;
    }

    range_start = - sub->val.index[ cur++ ] - 1;
    range_end = sub->val.index[ cur++ ];

    if ( dest->type & ( INT_ARR | FLOAT_ARR ) )
    {
        for ( i = range_start; i <= range_end; i++ )
            if ( dest->type == INT_ARR )
                dest->val.lpnt[ i ] = l;
            else
                dest->val.dpnt[ i ] = d;

        count = range_end - range_start + 1;
    }
    else
        for ( i = range_start; i <= range_end; i++ )
            count += vars_assign_to_snd_range_from_1d( dest->val.vptr[ i ],
                                                       sub, cur, l, d );

    return count;
}


/*--------------------------------*
 * Assign of an array to an array
 *--------------------------------*/

static long
vars_assign_to_nd_from_nd( Var_T * src,
                           Var_T * dest )
{
    ssize_t i;
    long count = 0;


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
                count += vars_assign_to_nd_from_nd( src, dest->val.vptr[ i ] );
        return count;
    }

    /* Unless the destination array is dynamically sized the sizes of
       both arrays must be identical */

    vars_size_check( src, dest );
    return vars_arr_assign( src, dest );
}


/*-------------------------------------------------*
 * Checks if the sizes of two arrays are identical
 *-------------------------------------------------*/

static void
vars_size_check( Var_T * src,
                 Var_T * dest )
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


/*------------------------------------------------------------------*
 *------------------------------------------------------------------*/

static long
vars_arr_assign( Var_T * src,
                 Var_T * dest )
{
    return src->type & ( INT_ARR | FLOAT_ARR ) ?
           vars_arr_assign_1d( src, dest ) : vars_arr_assign_nd( src, dest );
}


/*------------------------------------------------------------------*
 *------------------------------------------------------------------*/

static long
vars_arr_assign_1d( Var_T * src,
                    Var_T * dest )
{
    ssize_t i;


    /* If necessary resize the destination array */

    if ( dest->flags & IS_DYNAMIC && dest->len != src->len )
    {
        if ( dest->len != 0 )
        {
            if ( INT_TYPE( dest ) )
                dest->val.lpnt = T_free( dest->val.lpnt );
            else
                dest->val.dpnt = T_free( dest->val.dpnt );
        }

        dest->len = src->len;

        if ( src->len == 0 )
            return 0;

        if ( INT_TYPE( dest ) )
            dest->val.lpnt = T_malloc( dest->len * sizeof *dest->val.lpnt );
        else
            dest->val.dpnt = T_malloc( dest->len * sizeof *dest->val.dpnt );
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

    return dest->len;
}


/*------------------------------------------------------------------*
 *------------------------------------------------------------------*/

static long
vars_arr_assign_nd( Var_T * src,
                    Var_T * dest )
{
    ssize_t i;
    long count = 0;


    /* If necessary resize the array of references of the destination array */

    if ( dest->flags & IS_DYNAMIC )
    {
        if ( dest->len > src->len )
        {
            for ( i = dest->len - 1; i >= src->len; dest->len--, i-- )
                vars_free( dest->val.vptr[ i ], SET );

            if ( src->len > 0 )
                dest->val.vptr = T_realloc( dest->val.vptr,
                                            src->len * sizeof *dest->val.vptr );
            else
                dest->val.vptr = T_free( dest->val.vptr );
        }
        else if ( dest->len < src->len )
        {
            dest->val.vptr = T_realloc( dest->val.vptr,
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
        count += vars_arr_assign( src->val.vptr[ i ], dest->val.vptr[ i ] );

    return count;
}


/*----------------------------------------------------------*
 * Assignment of an array to an array specified with ranges
 *----------------------------------------------------------*/

static long
vars_assign_to_snd_from_nd( Var_T * src,
                            Var_T * dest,
                            Var_T * sub )
{
    ssize_t cur = 0;
    ssize_t single_indices = 0;
    ssize_t i;


    while ( sub->val.index[ cur ] >= 0 )
        dest = dest->val.vptr[ sub->val.index[ cur++ ] ];

    i = cur + 2;

    while ( i < sub->len )
        if ( sub->val.index[ i++ ] < 0 )
            i++;
        else if ( i != sub->len )
            single_indices++;

    /* The dimension of the destination array minus the number of single
       indices must be at least as large as the one of the source array */

    if ( dest->dim - single_indices < src->dim )
    {
        print( FATAL, "Left hand side of assignment has lower dimension than "
               "right hand side.\n" );
        THROW ( EXCEPTION );
    }

    return vars_assign_snd_range_from_nd( dest, sub, cur, src );
}


/*--------------------------------------------*
 *--------------------------------------------*/

static long
vars_assign_snd_range_from_nd( Var_T * dest,
                               Var_T * sub,
                               ssize_t cur,
                               Var_T * src )
{
    ssize_t ind,
            start,
            end,
            i;
    long count = 0;


    ind = sub->val.index[ cur++ ];

    if ( ind >= 0 )
    {
        if ( cur == sub->len )
            return vars_assign_to_nd( src, dest->val.vptr[ ind ] );
        else
            return vars_assign_snd_range_from_nd( dest->val.vptr[ ind ],
                                                  sub, cur, src );
    }

    start = - ind - 1;
    end = sub->val.index[ cur++ ];

    switch( src->type )
    {
        case INT_REF : case  FLOAT_REF :
            count = vars_assign_snd_range_from_nd_1( dest, src, sub,
                                                     start, end, cur );
            break;

        case INT_ARR : case FLOAT_ARR :
            count = vars_assign_snd_range_from_nd_2( dest, src, sub,
                                                     start, end, cur );
            break;

        default :
            for ( i = start; i <= end; i++ )
                count += vars_assign_snd_range_from_nd( dest->val.vptr[ i ],
                                                        sub, cur, src );
            break;
    }

    return count;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static long
vars_assign_snd_range_from_nd_1( Var_T * dest,
                                 Var_T * src,
                                 Var_T * sub,
                                 ssize_t start,
                                 ssize_t end,
                                 ssize_t cur )
{
    ssize_t range = end - start + 1;
    ssize_t i;
    long count = 0;


    if ( src->len != range )
    {
        print( FATAL, "Sizes of array slices don't fit in assignment.\n" );
        THROW( EXCEPTION );
    }

    switch ( dest->type )
    {
        case INT_ARR :
            if ( src->type == INT_REF )
                for ( i = start; i <= end; i++ )
                    dest->val.lpnt[ i ] = *src->val.lpnt;
            else
                for ( i = start; i <= end; i++ )
                    dest->val.lpnt[ i ] = lrnd( *src->val.dpnt );
            count = 1;
            break;

        case FLOAT_ARR :
            if ( src->type == INT_REF )
                for ( i = start; i <= end; i++ )
                    dest->val.dpnt[ i ] = *src->val.lpnt;
            else
                for ( i = start; i <= end; i++ )
                    dest->val.dpnt[ i ] = *src->val.dpnt;
            count = 1;
            break;

        default :
            for ( i = 0; i < range; i++ )
                count += vars_assign_snd_range_from_nd(
                                                dest->val.vptr[ i + start ],
                                                sub, cur, src->val.vptr[ i ] );
            break;
    }

    return count;
}


/*--------------------------------------------*
 *--------------------------------------------*/

static long
vars_assign_snd_range_from_nd_2( Var_T * dest,
                                 Var_T * src,
                                 Var_T * sub,
                                 ssize_t start,
                                 ssize_t end,
                                 ssize_t cur )
{
    ssize_t range = end - start + 1;
    ssize_t i;
    ssize_t idx;
    long count = 0;


    if ( src->len != range )
    {
        print( FATAL, "Sizes of array slices don't fit in assignment.\n" );
        THROW( EXCEPTION );
    }

    switch ( dest->type )
    {
        case INT_ARR :
            if ( src->type == INT_ARR )
                memcpy( dest->val.lpnt + start, src->val.lpnt,
                        range * sizeof *dest->val.lpnt );
            else
                for ( i = 0; i < range; i++ )
                    dest->val.lpnt[ i + start ] = lrnd( src->val.dpnt[ i ] );
            count = range;
            break;

        case FLOAT_ARR :
            if ( src->type == INT_ARR )
                for ( i = 0; i < range; i++ )
                    dest->val.lpnt[ i + start ] = src->val.lpnt[ i ];
            else
                memcpy( dest->val.dpnt + start, src->val.dpnt,
                        range * sizeof *dest->val.dpnt );
            count = range;
            break;

        default :
            if ( sub->val.index[ cur ] >= 0 )
            {
                idx = sub->val.index[ cur ];

                if ( src->type == INT_ARR )
                {
                    for ( i = 0; i < range; i++ )
                        if ( dest->type == INT_REF )
                            dest->val.vptr[ i + start ]->val.lpnt[ idx ] =
                                                             src->val.lpnt[ i ];
                        else
                            dest->val.vptr[ i + start ]->val.dpnt[ idx ] =
                                                             src->val.lpnt[ i ];
                }
                else if ( src->type == FLOAT_ARR )
                {
                    for ( i = 0; i < range; i++ )
                        if ( dest->type == INT_REF )
                            dest->val.vptr[ i + start ]->val.lpnt[ idx ] =
                                                     lrnd( src->val.dpnt[ i ] );
                        else
                            dest->val.vptr[ i + start ]->val.dpnt[ idx ] =
                                                            src->val.dpnt[ i ];
                }
                else
                    fsc2_assert( 1 == 0 );

                count = range;
            }
            else
                for ( i = start; i <= end; i++ )
                    count += vars_assign_snd_range_from_nd( dest->val.vptr[ i ],
                                                            sub, cur, src );
            break;
    }

    return count;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
