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


static Var_T *vars_str_var_add( Var_T * v1,
                                Var_T * v2 );
static Var_T *vars_int_var_add( Var_T * v1,
                                Var_T * v2 );
static Var_T *vars_float_var_add( Var_T * v1,
                                  Var_T * v2 );
static Var_T *vars_int_arr_add( Var_T * v1,
                                Var_T * v2 );
static Var_T *vars_float_arr_add( Var_T * v1,
                                  Var_T * v2 );
static Var_T *vars_ref_add( Var_T * v1,
                            Var_T * v2 );


/*---------------------------------------------------------*
 * Function for adding two variables of arbitrary types
 *---------------------------------------------------------*/

Var_T *
vars_add( Var_T * v1,
          Var_T * v2 )
{
    fsc2_assert( v1 != v2 );

    vars_check( v1, STR_VAR | RHS_TYPES | REF_PTR |
                    INT_PTR | FLOAT_PTR | SUB_REF_PTR );
    vars_check( v2, STR_VAR | RHS_TYPES );

    if (    ( v1->type == STR_VAR && v2->type != STR_VAR )
         || ( v1->type != STR_VAR && v2->type == STR_VAR ) )
    {
        print( FATAL, "Variable of type STRING can't be used in this "
               "context.\n" );
        THROW( EXCEPTION );
    }

    switch( v1->type )
    {
        case REF_PTR :
            v1 = v1->from;
            break;

        case INT_PTR :
            v1 = vars_push( INT_VAR, *v1->val.lpnt );
            break;

        case FLOAT_PTR :
            v1 = vars_push( FLOAT_VAR, *v1->val.dpnt );
            break;

        case SUB_REF_PTR :
            v1 = vars_subref_to_rhs_conv( v1 );
            break;

        default :
            break;
    }

    switch ( v1->type )
    {
        case STR_VAR :
            return vars_str_var_add( v1, v2 );

        case INT_VAR :
            return vars_int_var_add( v1, v2 );

        case FLOAT_VAR :
            return vars_float_var_add( v1, v2 );

        case INT_ARR :
            return vars_int_arr_add( v1, v2 );

        case FLOAT_ARR :
            return vars_float_arr_add( v1, v2 );

        case INT_REF :
        case FLOAT_REF :
            return vars_ref_add( v1, v2 );

        default :
            fsc2_impossible( );     /* This can't happen... */
        }

    fsc2_impossible( );
    return NULL;
}


/*-----------------------------------------------------------*
 * Function for "adding" (i.e. concatenating) two string variables
 *-----------------------------------------------------------*/

static Var_T *
vars_str_var_add( Var_T * v1,
                  Var_T * v2 )
{
    Var_T * new_var = vars_push( STR_VAR, NULL );
     new_var->val.sptr = get_string( "%s%s", v1->val.sptr, v2->val.sptr );

    vars_pop( v1 );
    vars_pop( v2 );
    return new_var;
}


/*-------------------------------------------------------------------------*
 * Function for adding a variable of arbitrary type to an integer variable
 *-------------------------------------------------------------------------*/

static Var_T *
vars_int_var_add( Var_T * v1,
                  Var_T * v2 )
{
    vars_arith_len_check( v1, v2, "addition" );

    Var_T * new_var = NULL;

    switch ( v2->type )
    {
        case INT_VAR :
            new_var = vars_push( INT_VAR, v1->val.lval + v2->val.lval );
            vars_pop( v1 );
            vars_pop( v2 );
            break;

        case FLOAT_VAR :
            new_var = vars_push( FLOAT_VAR,
                                 ( double ) v1->val.lval + v2->val.dval );
            vars_pop( v1 );
            vars_pop( v2 );
            break;

        case INT_ARR :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( INT_ARR, v2->val.lpnt, ( long ) v2->len );

            if ( v1->val.lval != 0 )
                for ( ssize_t i = 0; i < v2->len; i++ )
                    new_var->val.lpnt[ i ] += v1->val.lval;

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        case FLOAT_ARR :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( FLOAT_ARR, v2->val.dpnt,
                                     ( long ) v2->len );

            if ( v1->val.lval != 0 )
                for ( ssize_t i = 0; i < new_var->len; i++ )
                    new_var->val.dpnt[ i ] += v1->val.lval;

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        case INT_REF :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( v2->type, v2 );

            if ( v1->val.lval != 0 )
            {
                void * gp;
                while ( ( gp = vars_iter( new_var ) ) != NULL )
                    * ( long * ) gp += v1->val.lval;
            }

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        case FLOAT_REF :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( v2->type, v2 );

            if ( v1->val.lval != 0 )
            {
                void * gp;
                while ( ( gp = vars_iter( new_var ) ) != NULL )
                    * ( double * ) gp += v1->val.lval;
            }

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        default :
            fsc2_impossible( );     /* This can't happen... */
    }

    return new_var;
}


/*--------------------------------------------------*
 * Function for adding a variable of arbitrary type
 * to a floating point variable
 *--------------------------------------------------*/

static Var_T *
vars_float_var_add( Var_T * v1,
                    Var_T * v2 )
{
    vars_arith_len_check( v1, v2, "addition" );

    Var_T * new_var = NULL;

    switch ( v2->type )
    {
        case INT_VAR :
            new_var = vars_add( v2, v1 );
            break;

        case FLOAT_VAR :
            new_var = vars_push( FLOAT_VAR, v1->val.dval + v2->val.dval );
            vars_pop( v1 );
            vars_pop( v2 );
            break;

        case INT_ARR :
            new_var = vars_push( FLOAT_ARR, NULL, ( long ) v2->len );

            for ( ssize_t i = 0; i < new_var->len; i++ )
                new_var->val.dpnt[ i ] = v1->val.dval + v2->val.lpnt[ i ];

            vars_pop( v1 );
            vars_pop( v2 );

            break;

        case FLOAT_ARR :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( FLOAT_ARR, v2->val.dpnt,
                                     ( long ) v2->len );

            if ( v1->val.dval != 0.0 )
                for ( ssize_t i = 0; i < new_var->len; i++ )
                    new_var->val.dpnt[ i ] += v1->val.dval;

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        case INT_REF :
            new_var = vars_push( FLOAT_REF, v2 );

            if ( v1->val.dval != 0.0 )
            {
                void * gp;
                while ( ( gp = vars_iter( new_var ) ) != NULL )
                    * ( double * ) gp += v1->val.dval;
            }

            vars_pop( v1 );
            vars_pop( v2 );
            break;

        case FLOAT_REF :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( v2->type, v2 );

            if ( v1->val.dval != 0.0 )
            {
                void * gp;
                while ( ( gp = vars_iter( new_var ) ) != NULL )
                    * ( double * ) gp += v1->val.dval;
            }

            vars_pop( v1 );
            if ( v2 != new_var )
                vars_pop( v2 );
            break;

        default :
            fsc2_impossible( );     /* This can't happen... */
    }

    return new_var;
}


/*-------------------------------------------------------------------------*
 * Function for adding a variable of arbitrary type to an 1D integer array
 *-------------------------------------------------------------------------*/

static Var_T *
vars_int_arr_add( Var_T * v1,
                  Var_T * v2 )
{
    vars_arith_len_check( v1, v2, "addition" );

    Var_T * new_var = NULL;

    switch ( v2->type )
    {
        case INT_VAR :
            new_var = vars_add( v2, v1 );
            break;

        case FLOAT_VAR :
            new_var = vars_add( v2, v1 );
            break;

        case INT_ARR :
            if ( v1->flags & IS_TEMP )
            {
                Var_T * vt = v1;
                v1 = v2;
                v2 = vt;
            }

            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( INT_ARR, v2->val.lpnt, ( long ) v2->len );

            for ( ssize_t i = 0; i < new_var->len; i++ )
                new_var->val.lpnt[ i ] += v1->val.lpnt[ i ];

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        case FLOAT_ARR :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( FLOAT_ARR, v2->val.dpnt,
                                     ( long ) v2->len );

            for ( ssize_t i = 0; i < v1->len; i++ )
                new_var->val.dpnt[ i ] += v1->val.lpnt[ i ];

            if ( v1 != v2 )
                vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        case INT_REF : case FLOAT_REF :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( v2->type, v2 );

            for ( ssize_t i = 0; i < new_var->len; i++ )
                vars_add( vars_push( INT_ARR, v1->val.lpnt, ( long ) v1->len ),
                          new_var->val.vptr[ i ] );

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        default :
            fsc2_impossible( );     /* This can't happen... */
    }

    return new_var;
}


/*--------------------------------------------------*
 * Function for adding a variable of arbitrary type
 * to an 1D floating point array
 *--------------------------------------------------*/

static Var_T *
vars_float_arr_add( Var_T * v1,
                    Var_T * v2 )
{
    vars_arith_len_check( v1, v2, "addition" );

    Var_T  *new_var = NULL;

    switch ( v2->type )
    {
        case STR_VAR :
            print( FATAL, "Can't add a string to an array.\n" );
            THROW( EXCEPTION );

        case INT_VAR :
            new_var = vars_add( v2, v1 );
            break;

        case FLOAT_VAR :
            new_var = vars_add( v2, v1 );
            break;

        case INT_ARR :
            new_var = vars_add( v2, v1 );
            break;

        case FLOAT_ARR :
            if ( v1->flags & IS_TEMP )
            {
                Var_T * vt = v1;
                v1 = v2;
                v2 = vt;
            }

            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( FLOAT_ARR, v2->val.dpnt,
                                     ( long ) v2->len );

            for ( ssize_t i = 0; i < new_var->len; i++ )
                new_var->val.dpnt[ i ] += v1->val.dpnt[ i ];

            if ( v1 != v2 )
                vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        case INT_REF : case FLOAT_REF :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( v2->type, v2 );

            for ( ssize_t i = 0; i < new_var->len; i++ )
                vars_add( vars_push( FLOAT_ARR, v1->val.dpnt,
                                     ( long ) v1->len ),
                          new_var->val.vptr[ i ] );

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        default :
            fsc2_impossible( );     /* This can't happen... */
    }

    return new_var;
}


/*-----------------------------------------------------*
 * Function for adding a variable of arbitrary type to
 * a more-dimensional integer or floating point array
 *-----------------------------------------------------*/

static Var_T *
vars_ref_add( Var_T * v1,
              Var_T * v2 )
{
    vars_arith_len_check( v1, v2, "addition" );

    Var_T * new_var = NULL;

    switch ( v2->type )
    {
        case STR_VAR :
            print( FATAL, "Can't add a string to a multidimensional "
                   "array.\n" );
            THROW( EXCEPTION );

        case INT_VAR :
            new_var = vars_add( v2, v1 );
            break;

        case FLOAT_VAR :
            new_var = vars_add( v2, v1 );
            break;

        case INT_ARR :
            new_var = vars_add( v2, v1 );
            break;

        case FLOAT_ARR :
            new_var = vars_add( v2, v1 );
            break;

        case INT_REF : case FLOAT_REF :
            if ( v1->flags & IS_TEMP )
            {
                Var_T * vt = v1;
                v1 = v2;
                v2 = vt;
            }

            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( v2->type, v2 );

            if ( v1->dim > new_var->dim )
                for ( ssize_t i = 0; i < v1->len; i++ )
                    vars_add( v1->val.vptr[ i ], new_var );
            else if ( v1->dim < new_var->dim )
                for ( ssize_t i = 0; i < new_var->len; i++ )
                    vars_add( v1, new_var->val.vptr[ i ] );
            else
                for ( ssize_t i = 0; i < new_var->len; i++ )
                    vars_add( new_var->val.vptr[ i ], v1->val.vptr[ i ] );

            if ( v1 != v2 )
                vars_pop( v1 );
            if ( v2 != new_var )
                vars_pop( v2 );
            break;

        default :
            fsc2_impossible( );     /* This can't happen... */
    }

    return new_var;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
