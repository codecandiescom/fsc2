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


#include "fsc2.h"


static Var_T *vars_sub_i( Var_T * v1,
                          Var_T * v2,
                          bool    exc );
static Var_T *vars_int_var_sub( Var_T * v1,
                                Var_T * v2,
                                bool    exc );
static Var_T *vars_float_var_sub( Var_T * v1,
                                  Var_T * v2,
                                  bool    exc );
static Var_T *vars_int_arr_sub( Var_T * v1,
                                Var_T * v2,
                                bool    exc );
static Var_T *vars_float_arr_sub( Var_T * v1,
                                  Var_T * v2,
                                  bool    exc );
static Var_T *vars_ref_sub( Var_T * v1,
                            Var_T * v2,
                            bool    exc );


/*--------------------------------------------------------------*
 * Function for subtracting of two variables of arbitrary types
 *--------------------------------------------------------------*/

Var_T *vars_sub( Var_T * v1,
                 Var_T * v2 )
{
    vars_check( v1, RHS_TYPES | REF_PTR | INT_PTR | FLOAT_PTR | SUB_REF_PTR );
    vars_check( v2, RHS_TYPES );

    switch ( v1->type )
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

    return vars_sub_i( v1, v2, UNSET );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static Var_T *vars_sub_i( Var_T * v1,
                          Var_T * v2,
                          bool    exc )
{
    Var_T *new_var = NULL;


    switch ( v1->type )
    {
        case INT_VAR :
            new_var = vars_int_var_sub( v1, v2, exc );
            break;

        case FLOAT_VAR :
            new_var = vars_float_var_sub( v1, v2, exc );
            break;
    
        case INT_ARR :
            new_var = vars_int_arr_sub( v1, v2, exc );
            break;

        case FLOAT_ARR :
            new_var = vars_float_arr_sub( v1, v2, exc );
            break;

        case INT_REF : case FLOAT_REF :
            new_var = vars_ref_sub( v1, v2, exc );
            break;

        default :
            fsc2_assert( 1 == 0 );     /* This can't happen... */
    }

    return new_var;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static Var_T *vars_int_var_sub( Var_T * v1,
                                Var_T * v2,
                                bool    exc )
{
    Var_T *new_var = NULL;
    ssize_t i;
    void *gp;
    long ir;
    double dr;


    vars_arith_len_check( v1, v2, "subtraction" );

    switch ( v2->type )
    {
        case INT_VAR :
            if ( ! exc )
                ir = v1->val.lval - v2->val.lval;
            else
                ir = v2->val.lval - v1->val.lval;
            new_var = vars_push( INT_VAR, ir );

            vars_pop( v1 );
            vars_pop( v2 );
            break;

        case FLOAT_VAR :
            if ( ! exc )
                dr = ( double ) v1->val.lval - v2->val.dval;
            else
                dr = v2->val.dval - ( double ) v1->val.lval;
            new_var = vars_push( FLOAT_VAR, dr );

            vars_pop( v1 );
            vars_pop( v2 );
            break;

        case INT_ARR :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( INT_ARR, v2->val.lpnt, v2->len );

            for ( i = 0; i < v2->len; i++ )
            {
                if ( ! exc )
                    ir = v1->val.lval - new_var->val.lpnt[ i ];
                else
                    ir = new_var->val.lpnt[ i ] - v1->val.lval;
                new_var->val.lpnt[ i ] = ir;
            }

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );

            break;

        case FLOAT_ARR :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( FLOAT_ARR, v2->val.dpnt, v2->len );

            for ( i = 0; i < v2->len; i++ )
            {
                if ( ! exc )
                    dr = ( double ) v1->val.lval - new_var->val.dpnt[ i ];
                else
                    dr = new_var->val.dpnt[ i ] - ( double ) v1->val.lval;
                new_var->val.dpnt[ i ] = dr;
            }

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );

            break;

        case INT_REF :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( v2->type, v2 );

            while ( ( gp = vars_iter( new_var ) ) != NULL )
            {
                if ( ! exc )
                    ir = v1->val.lval - * ( long * ) gp;
                else
                    ir = * ( long * ) gp - v1->val.lval;
                * ( long * ) gp = ir;
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

            while ( ( gp = vars_iter( new_var ) ) != NULL )
            {
                if ( ! exc )
                    dr = ( double ) v1->val.lval - * ( double * ) gp;
                else
                    dr = * ( double * ) gp - ( double ) v1->val.lval;
                * ( double * ) gp = dr;
            }

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        default :
            fsc2_assert( 1 == 0 );     /* This can't happen... */
    }

    return new_var;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static Var_T *vars_float_var_sub( Var_T * v1,
                                  Var_T * v2,
                                  bool    exc )
{
    Var_T *new_var = NULL;
    ssize_t i;
    void *gp;
    double dr;


    vars_arith_len_check( v1, v2, "subtraction" );

    switch ( v2->type )
    {
        case INT_VAR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case FLOAT_VAR :
            if ( ! exc )
                dr = v1->val.dval - v2->val.dval;
            else
                dr = v2->val.dval - v1->val.dval;
            new_var = vars_push( FLOAT_VAR, dr );
            vars_pop( v1 );
            vars_pop( v2 );
            break;

        case INT_ARR :
            new_var = vars_push( FLOAT_ARR, NULL, v2->len );

            for ( i = 0; i < new_var->len; i++ )
            {
                if ( ! exc )
                    dr = v1->val.dval - ( double ) v2->val.lpnt[ i ];
                else
                    dr = ( double ) v2->val.lpnt[ i ] - v1->val.dval;
                new_var->val.dpnt[ i ] = dr;
            }

            vars_pop( v1 );
            vars_pop( v2 );

            break;

        case FLOAT_ARR :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( FLOAT_ARR, v2->val.dpnt, v2->len );

            for ( i = 0; i < new_var->len; i++ )
            {
                if ( ! exc )
                    dr = v1->val.dval - new_var->val.dpnt[ i ];
                else
                    dr = new_var->val.dpnt[ i ] - v1->val.dval;
                new_var->val.dpnt[ i ] = dr;
            }

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );

            break;

        case INT_REF :
            new_var = vars_push( FLOAT_REF, v2 );

            while ( ( gp = vars_iter( new_var ) ) != NULL )
            {
                if ( ! exc )
                    dr = v1->val.dval - * ( double * ) gp;
                else
                    dr = * ( double * ) gp - v1->val.dval;
                * ( double * ) gp = dr;
            }

            vars_pop( v1 );
            vars_pop( v2 );

            break;

        case FLOAT_REF :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( v2->type, v2 );

            while ( ( gp = vars_iter( new_var ) ) != NULL )
            {
                if ( ! exc )
                    dr = v1->val.dval - * ( double * ) gp;
                else
                    dr = * ( double * ) gp - v1->val.dval;
                * ( double * ) gp = dr;
            }

            vars_pop( v1 );
            if ( v2 != new_var )
                vars_pop( v2 );
            break;

        default :
            fsc2_assert( 1 == 0 );     /* This can't happen... */
    }

    return new_var;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static Var_T *vars_int_arr_sub( Var_T * v1,
                                Var_T * v2,
                                bool    exc )
{
    Var_T *new_var = NULL;
    Var_T *vt;
    ssize_t i;
    long ir;
    double dr;


    vars_arith_len_check( v1, v2, "subtraction" );

    switch ( v2->type )
    {
        case INT_VAR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case FLOAT_VAR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case INT_ARR :
            if ( v1->flags & IS_TEMP && v1 != v2 )
            {
                vt = v1;
                v1 = v2;
                v2 = vt;
                exc = ! exc;
            }

            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( INT_ARR, v2->val.lpnt, v2->len );

            for ( i = 0; i < new_var->len; i++ )
            {
                if ( ! exc )
                    ir = v1->val.lpnt[ i ] - new_var->val.lpnt[ i ];
                else
                    ir = new_var->val.lpnt[ i ] - v1->val.lpnt[ i ];
                new_var->val.lpnt[ i ] = ir;
            }

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );

            break;

        case FLOAT_ARR :
            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( FLOAT_ARR, v2->val.dpnt, v2->len );

            for ( i = 0; i < v1->len; i++ )
            {
                if ( ! exc )
                    dr = ( double ) v1->val.lpnt[ i ] - new_var->val.dpnt[ i ];
                else
                    dr = new_var->val.dpnt[ i ] - ( double ) v1->val.lpnt[ i ];
                new_var->val.dpnt[ i ] = dr;
            }

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

            for ( i = 0; i < new_var->len; i++ )
                vars_sub_i( vars_push( INT_ARR, v1->val.lpnt, v1->len ),
                            new_var->val.vptr[ i ], exc );

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        default :
            fsc2_assert( 1 == 0 );     /* This can't happen... */
    }

    return new_var;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static Var_T *vars_float_arr_sub( Var_T * v1,
                                  Var_T * v2,
                                  bool    exc )
{
    Var_T *new_var = NULL;
    Var_T *vt;
    ssize_t i;
    double dr;


    vars_arith_len_check( v1, v2, "subtraction" );

    switch ( v2->type )
    {
        case INT_VAR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case FLOAT_VAR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case INT_ARR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case FLOAT_ARR :
            if ( v1->flags & IS_TEMP )
            {
                vt = v1;
                v1 = v2;
                v2 = vt;
                exc = ! exc;
            }

            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( FLOAT_ARR, v2->val.dpnt, v2->len );

            for ( i = 0; i < new_var->len; i++ )
            {
                if ( ! exc )
                    dr = v1->val.dpnt[ i ] - new_var->val.dpnt[ i ];
                else
                    dr = new_var->val.dpnt[ i ] - v1->val.dpnt[ i ];
                new_var->val.dpnt[ i ] = dr;
            }

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

            for ( i = 0; i < new_var->len; i++ )
                vars_sub_i( vars_push( FLOAT_ARR, v1->val.dpnt, v1->len ),
                            new_var->val.vptr[ i ], exc );

            vars_pop( v1 );
            if ( new_var != v2 )
                vars_pop( v2 );
            break;

        default :
            fsc2_assert( 1 == 0 );     /* This can't happen... */
    }

    return new_var;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static Var_T *vars_ref_sub( Var_T * v1,
                            Var_T * v2,
                            bool    exc )
{
    Var_T *new_var = NULL;
    Var_T *vt;
    ssize_t i;


    vars_arith_len_check( v1, v2, "subtraction" );

    switch ( v2->type )
    {
        case INT_VAR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case FLOAT_VAR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case INT_ARR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case FLOAT_ARR :
            new_var = vars_sub_i( v2, v1, ! exc );
            break;

        case INT_REF : case FLOAT_REF :
            if ( v1->flags & IS_TEMP )
            {
                vt = v1;
                v1 = v2;
                v2 = vt;
                exc = ! exc;
            }

            if ( v2->flags & IS_TEMP )
                new_var = v2;
            else
                new_var = vars_push( v2->type, v2 );

            if ( v1->dim > new_var->dim )
                for ( i = 0; i < v1->len; i++ )
                    vars_sub_i( v1->val.vptr[ i ], new_var, exc );
            else if ( v1->dim < new_var->dim )
                for ( i = 0; i < new_var->len; i++ )
                    vars_sub_i( v1, new_var->val.vptr[ i ], exc );
            else
                for ( i = 0; i < new_var->len; i++ )
                    vars_sub_i( new_var->val.vptr[ i ], v1->val.vptr[ i ],
                                ! exc );

            if ( v1 != v2 )
                vars_pop( v1 );
            if ( v2 != new_var )
                vars_pop( v2 );
            break;

        default :
            fsc2_assert( 1 == 0 );     /* This can't happen... */
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
