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


static double gauss_random( void );
static void avg_data_check( Var_T * avg,
                            Var_T * data,
                            long    count );


#define C2K_OFFSET   273.16
#define D2R_FACTOR   ( atan( 1.0 ) / 45.0 )
#define R2D_FACTOR   ( 45.0 / atan( 1.0 ) )
#define WN2F_FACTOR  2.99792558e10         /* speed of light times 100 */


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
f_abort( Var_T * v  UNUSED_ARG )
{
    /* If we're already past the ON_STOP label just stop without complaints */

    if (    EDL.On_Stop_Pos >= 0
            && EDL.cur_prg_token >= EDL.prg_token + EDL.On_Stop_Pos )
        THROW( ABORT_EXCEPTION );

    /* Otherwise inform the user abut what happend */

    if ( Fsc2_Internals.mode != TEST )
    {
        char * str = get_string( "Exit due to call of abort() in\n"
                                 "%s at line %ld.", EDL.Fname, EDL.Lc );
        show_message( str );
        T_free( str );
        print( NO_ERROR, "Experiment stopped due to call of abort().\n" );
        THROW( ABORT_EXCEPTION );
    }
    else
        print( NO_ERROR, "Call of abort() in test run.\n" );

    return NULL;
}


/*-------------------------------------------*
 * This is called for the end() EDL function
 *-------------------------------------------*/

Var_T *
f_stopsim( Var_T * v  UNUSED_ARG )
{
    EDL.do_quit = true;
    return NULL;
}


/*-------------------------------------------------*
 * Conversion from float to integer (result is integer)
 *-------------------------------------------------*/

Var_T *
f_int( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;

    switch ( v->type )
    {
        case INT_VAR :
            return vars_push( INT_VAR, v->val.lval );

        case FLOAT_VAR :
            if (    v->val.dval >= LONG_MAX + 0.5
                 || v->val.dval < LONG_MIN - 0.5 )
            {
                print( SEVERE, "Integer argument overflow.\n" );
                if ( v->val.dval >= 0 )
                    return vars_push( INT_VAR, LONG_MAX );
                else
                    return vars_push( INT_VAR, LONG_MIN );
            }
            return vars_push( INT_VAR, ( long ) v->val.dval );

        case INT_ARR :
            new_var = vars_push( INT_ARR, v->val.lpnt, ( long ) v->len );
            new_var->flags = v->flags;
            break;

        case FLOAT_ARR :
            new_var = vars_make( INT_ARR, v );
            double * restrict src = v->val.dpnt;
            long * restrict dest = new_var->val.lpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                if ( *src >= LONG_MAX + 0.5 || *src < LONG_MIN - 0.5 )
                {
                    print( SEVERE, "Integer argument overflow.\n" );
                    if ( *src++ > 0 )
                        *dest++ = LONG_MAX;
                    else
                        *dest++ = LONG_MIN;
                }
                else
                    *dest++ = *src++;
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( INT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_int( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*----------------------------------------------------*
 * Conversion from integer to floating point (result is float)
 *----------------------------------------------------*/

Var_T *
f_float( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;

    switch ( v->type )
    {
        case INT_VAR :
            return vars_push( FLOAT_VAR, ( double ) v->val.lval );

        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, v->val.dval );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict src = v->val.lpnt;
            double * restrict dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = *src++;
            break;

        case FLOAT_ARR :
            new_var = vars_push( FLOAT_ARR, v->val.dpnt, ( long ) v->len );
            new_var->flags = v->flags;
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_float( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*--------------------------------------------------------*
 * Rounding of floating point numbers (result is integer)
 *--------------------------------------------------------*/

Var_T *
f_round( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;

    switch ( v->type )
    {
        case INT_VAR :
            return vars_push( INT_VAR, v->val.lval );

        case FLOAT_VAR :
            if (    v->val.dval >= LONG_MAX - 0.5
                 || v->val.dval <= LONG_MIN + 0.5 )
            {
                print( SEVERE, "Integer argument overflow.\n" );
                if (  v->val.dval >= 0 )
                    return vars_push( INT_VAR, LONG_MAX );
                else
                    return vars_push( INT_VAR, LONG_MIN );
            }
            return vars_push( INT_VAR, lrnd( v->val.dval ) );

        case INT_ARR :
            new_var = vars_push( INT_ARR, v->val.lpnt, ( long ) v->len );
            new_var->flags = v->flags;
            break;

        case FLOAT_ARR :
            new_var = vars_make( INT_ARR, v );
            double * restrict src = v->val.dpnt;
            long * restrict dest = new_var->val.lpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                if ( *src >= LONG_MAX - 0.5 || *src <= LONG_MIN + 0.5 )
                {
                    print( SEVERE, "Integer argument overflow.\n" );
                    if ( *src++ >= 0 )
                        *dest++ = LONG_MAX;
                    else
                        *dest++ = LONG_MIN;
                }
                else
                    *dest++ = lrnd( *src++ );
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( INT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_round( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*---------------------------------*
 * Floor value (result is integer)
 *---------------------------------*/

Var_T *
f_floor( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;

    switch ( v->type )
    {
        case INT_VAR :
            return vars_push( INT_VAR, v->val.lval );

        case FLOAT_VAR :
            if ( v->val.dval < LONG_MIN - 0.5 )
            {
                print( SEVERE, "Integer argument overflow.\n" );
                return vars_push( INT_VAR, LONG_MIN );
            }
            return vars_push( INT_VAR, lrnd( floor( v->val.dval ) ) );

        case INT_ARR :
            new_var = vars_push( INT_ARR, v->val.lpnt, ( long ) v->len );
            new_var->flags = v->flags;
            break;

        case FLOAT_ARR :
            new_var = vars_make( INT_ARR, v );
            double * restrict src = v->val.dpnt;
            long * restrict dest = new_var->val.lpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                if ( *src < LONG_MIN - 0.5 )
                {
                    print( SEVERE, "Integer argument overflow.\n" );
                    *dest++ = LONG_MIN;
                    src++;
                }
                else
                    *dest++ = floor( *src++ );
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( INT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_floor( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-----------------------------------*
 * Ceiling value (result is integer)
 *-----------------------------------*/

Var_T *
f_ceil( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;

    switch ( v->type )
    {
        case INT_VAR :
            return vars_push( INT_VAR, v->val.lval );

        case FLOAT_VAR :
            if ( v->val.dval > LONG_MAX )
            {
                print( SEVERE, "Integer argument overflow.\n" );
                return vars_push( INT_VAR, LONG_MAX );
            }
            return vars_push( INT_VAR, lrnd( ceil( v->val.dval ) ) );

        case INT_ARR :
            new_var = vars_push( INT_ARR, v->val.lpnt, ( long ) v->len );
            new_var->flags = v->flags;
            break;

        case FLOAT_ARR :
            new_var = vars_make( INT_ARR, v );
            double * restrict src = v->val.dpnt;
            long * restrict dest = new_var->val.lpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                if ( *src > LONG_MAX )
                {
                    print( SEVERE, "Integer argument overflow.\n" );
                    *dest++ = LONG_MAX;
                    src++;
                }
                else
                    *dest++ = ceil( *src++ );
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( INT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_ceil( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------*
 * abs of value (result has same as type argument)
 *-------------------------------------------------*/

Var_T *
f_abs( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    bool may_fail = LONG_MAX + LONG_MIN != 0;

    switch ( v->type )
    {
        case INT_VAR :
            if ( may_fail && v->val.lval == LONG_MIN )
            {
                print( SEVERE, "Integer argument overflow.\n" );
                return vars_push( INT_VAR, LONG_MAX );
            }
            return vars_push( INT_VAR, labs( v->val.lval ) );

        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, fabs( v->val.dval ) );

        case INT_ARR :
            new_var = vars_make( INT_ARR, v );
            long * restrict lsrc  = v->val.lpnt;
            long * ldest = new_var->val.lpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                if ( may_fail && *lsrc == LONG_MIN )
                {
                    print( SEVERE, "Integer argument overflow.\n" );
                    *ldest++ = LONG_MAX;
                    lsrc++;
                }
                else
                    *ldest++ = labs( *lsrc++ );
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc  = v->val.dpnt;
            double * restrict ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = fabs( *dsrc++ );
            break;

        case INT_REF :
            new_var = vars_make( INT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_abs( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_abs( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------------------------*
 * max of all values (result is a float variable unless
 * all inputs were integer values)
 *------------------------------------------------------*/

Var_T *
f_lmax( Var_T * v )
{
    double m = - HUGE_VAL;
    bool all_int = true;
    void *gp;

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                       INT_REF | FLOAT_REF );

        switch ( v->type )
        {
            case INT_VAR :
                if ( v->val.lval > m )
                    m = v->val.lval;
                break;

            case FLOAT_VAR :
                if ( v->val.dval > m )
                    m = v->val.dval;
                all_int = false;
                break;

            case INT_ARR :
                for ( ssize_t i = 0; i < v->len; i++ )
                    if ( v->val.lpnt[ i ] > m )
                        m = v->val.lpnt[ i ];
                break;

            case FLOAT_ARR :
                for ( ssize_t i = 0; i < v->len; i++ )
                    if ( v->val.dpnt[ i ] > m )
                        m = v->val.dpnt[ i ];
                all_int = false;
                break;

            case INT_REF :
                while ( ( gp = vars_iter( v ) ) != NULL )
                    if ( * ( long * ) gp > m )
                        m = * ( long * ) gp;
                break;

            case FLOAT_REF :
                while ( ( gp = vars_iter( v ) ) != NULL )
                    if ( * ( double * ) gp > m )
                        m = * ( double * ) gp;
                all_int = false;
                break;

            default :
                break;
        }
    }

    if ( all_int && m >= LONG_MIN && m <= LONG_MAX )
        return vars_push( INT_VAR, ( long ) m );
    return vars_push( FLOAT_VAR, m );
}


/*------------------------------------------------------*
 * min of all values (result is a float variable unless
 * all inputs were integer values)
 *------------------------------------------------------*/

Var_T *
f_lmin( Var_T * v )
{
    double m = HUGE_VAL;
    bool all_int = true;
    void *gp;

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                       INT_REF | FLOAT_REF );

        switch ( v->type )
        {
            case INT_VAR :
                if ( v->val.lval < m )
                    m = v->val.lval;
                break;

            case FLOAT_VAR :
                if ( v->val.dval < m )
                    m = v->val.dval;
                all_int = false;
                break;

            case INT_ARR :
                for ( ssize_t i = 0; i < v->len; i++ )
                    if ( v->val.lpnt[ i ] < m )
                        m = v->val.lpnt[ i ];
                break;

            case FLOAT_ARR :
                for ( ssize_t i = 0; i < v->len; i++ )
                    if ( v->val.dpnt[ i ] < m )
                        m = v->val.dpnt[ i ];
                all_int = false;
                break;

            case INT_REF :
                while ( ( gp = vars_iter( v ) ) != NULL )
                    if ( * ( long * ) gp < m )
                        m = * ( long * ) gp;
                break;

            case FLOAT_REF :
                while ( ( gp = vars_iter( v ) ) != NULL )
                    if ( * ( double * ) gp < m )
                        m = * ( double * ) gp;
                all_int = false;
                break;

            default :
                break;
        }
    }

    if ( all_int && m >= LONG_MIN && m <= LONG_MAX )
        return vars_push( INT_VAR, ( long ) m );
    return vars_push( FLOAT_VAR, m );
}


/*-----------------------------------------------*
 * sin of argument (in radian) (result is float)
 *-----------------------------------------------*/

Var_T *
f_sin( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, sin( VALUE( v ) ) );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = sin( *lsrc++ );
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = sin( *dsrc++ );
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_sin( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-----------------------------------------------*
 * cos of argument (in radian) (result is float)
 *-----------------------------------------------*/

Var_T *
f_cos( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, cos( VALUE( v ) ) );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = cos( *lsrc++ );
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = cos( *dsrc++ );
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_cos( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-----------------------------------------------*
 * tan of argument (in radian) (result is float)
 *-----------------------------------------------*/

Var_T *
f_tan( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, tan( VALUE( v ) ) );
            if ( errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++, dest++ )
            {
                *dest = tan( *lsrc++ );
                if ( errno == ERANGE )
                {
                    print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++, dest++ )
            {
                *dest = tan( *dsrc++ );
                if ( errno == ERANGE )
                {
                    print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_tan( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*--------------------------------------------------------------------*
 * asin (in radian) of argument (with -1 <= x <= 1) (result is float)
 *--------------------------------------------------------------------*/

Var_T *
f_asin( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, asin( VALUE( v ) ) );
            if ( errno == EDOM )
            {
                print( FATAL, "Argument (%f) out of range.\n",
                       ( double ) VALUE( v ) );
                THROW( EXCEPTION );
            }
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            dest = new_var->val.dpnt;
            long * restrict lsrc = v->val.lpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = asin( *lsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%ld) out of range.\n", *--lsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            dest = new_var->val.dpnt;
            double * restrict dsrc = v->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = asin( *dsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%f) out of range.\n", *--dsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_asin( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*--------------------------------------------------------------------*
 * acos (in radian) of argument (with -1 <= x <= 1) (result is float)
 *--------------------------------------------------------------------*/

Var_T *
f_acos( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, acos( VALUE( v ) ) );
            if ( errno == EDOM )
            {
                print( FATAL, "Argument (%f) out of range.\n",
                       ( double ) VALUE( v ) );
                THROW( EXCEPTION );
            }
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = acos( *lsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%fd) out of range.\n", *--lsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = acos( *dsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%f) out of range.\n", *--dsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_acos( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------------------*
 * atan (in radian) of argument (result is float)
 *------------------------------------------------*/

Var_T *
f_atan( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            new_var = vars_push( FLOAT_VAR, atan( VALUE( v ) ) );
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = atan( *lsrc++ );
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = atan( *dsrc++ );
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_atan( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------*
 * sinh of argument (result is float)
 *------------------------------------*/

Var_T *
f_sinh( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, sinh( VALUE ( v ) ) );
            if ( errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = sinh( *lsrc++ );
            if (  errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = sinh( *dsrc++ );
            if ( errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_sinh( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------*
 * cosh of argument (result is float)
 *------------------------------------*/

Var_T *
f_cosh( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, cosh( VALUE ( v ) ) );
            if ( errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = cosh( *lsrc++ );
            if ( errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = cosh( *dsrc++ );
            if ( errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_cosh( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------*
 * tanh of argument (result is float)
 *------------------------------------*/

Var_T *
f_tanh( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            new_var = vars_push( FLOAT_VAR, tanh( VALUE( v ) ) );
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = tanh( *lsrc++ );
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = tanh( *dsrc++ );
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_tanh( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-----------------------------------------------*
 * Inverse of sinh of argument (result is float)
 *-----------------------------------------------*/

Var_T *
f_asinh( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            new_var = vars_push( FLOAT_VAR, VALUE( v ) );
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = asinh( *lsrc++ );
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *dest++ = asinh( *dsrc++ );
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_asinh( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-----------------------------------------------*
 * Inverse of cosh of argument (result is float)
 *-----------------------------------------------*/

Var_T *
f_acosh( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, acosh( VALUE( v ) ) );
            if ( errno == EDOM )
            {
                print( FATAL, "Argument (%f) out of range.\n",
                       ( double ) VALUE( v ) );
                THROW( EXCEPTION );
            }
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = acosh( *lsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%ld) out of range.\n", *--lsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = acosh( *dsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%f) out of range.\n", *--dsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_acosh( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-----------------------------------------------*
 * Inverse of tanh of argument (result is float)
 *-----------------------------------------------*/

Var_T *
f_atanh( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T *new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, atanh( VALUE( v ) ) );
            if ( errno == EDOM )
            {
                print( FATAL, "Argument (%f) out of range.\n",
                       ( double ) VALUE( v ) );
                THROW( EXCEPTION );
            }
            else if (  errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest = atanh( *lsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%ld) out of range.\n", *--lsrc );
                    THROW( EXCEPTION );
                }
                else if (  errno == ERANGE )
                {
                    print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = atanh( *dsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%f) out of range.\n", *--dsrc );
                    THROW( EXCEPTION );
                }
                else if (  errno == ERANGE )
                {
                    print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_atanh( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-----------------------------------*
 * exp of argument (result is float)
 *-----------------------------------*/

Var_T *
f_exp( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, exp( VALUE( v ) ) );
            if ( errno == ERANGE )
            {
                if ( new_var->val.dval == 0.0 )
                    print( WARN, "Result underflow, result set to 0.\n" );
                else
                    print( SEVERE, "Result overflow.\n" );
            }
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++, dest++ )
            {
                *dest = exp( *lsrc++ );
                if ( errno == ERANGE )
                {
                    if ( *dest == 0.0 )
                        print( WARN, "Result underflow, result set to 0.\n" );
                    else
                        print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++, dest++ )
            {
                *dest = exp( *dsrc++ );
                if ( errno == ERANGE )
                {
                    if ( *dest == 0.0 )
                        print( WARN, "Result underflow, result set to 0.\n" );
                    else
                        print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_exp( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-----------------------------------------------*
 * ln of argument (with x > 0) (result is float)
 *-----------------------------------------------*/

Var_T *
f_ln( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, log( VALUE( v ) ) );
            if ( errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            else if ( errno == EDOM )
            {
                print( FATAL, "Argument (%f) out of range.\n",
                       ( double ) VALUE( v ) );
                THROW( EXCEPTION );
            }
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = log( *lsrc++ );
                if ( errno == ERANGE )
                {
                    print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
                else if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%ld) out of range.\n", *--lsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = log( *dsrc++ );
                if ( errno == ERANGE )
                {
                    print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
                else if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%ld) out of range.\n", *--dsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_ln( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------------------*
 * log of argument (with x > 0) (result is float)
 *------------------------------------------------*/

Var_T *
f_log( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, log10( VALUE( v ) ) );
            if ( errno == ERANGE )
                print( SEVERE, "Result overflow.\n" );
            else if ( errno == EDOM )
            {
                print( FATAL, "Argument (%f) out of range.\n",
                       ( double ) VALUE( v ) );
                THROW( EXCEPTION );
            }
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = log10( *lsrc++ );
                if ( errno == ERANGE )
                {
                    print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
                else if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%ld) out of range.\n", *--lsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = log10( *dsrc++ );
                if ( errno == ERANGE )
                {
                    print( SEVERE, "Result overflow.\n" );
                    errno = 0;
                }
                else if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%ld) out of range.\n", *--dsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_log( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*--------------------------------------------------*
 * sqrt of argument (with x >= 0) (result is float)
 *--------------------------------------------------*/

Var_T *
f_sqrt( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict dest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            errno = 0;
            new_var = vars_push( FLOAT_VAR, sqrt( VALUE( v ) ) );
            if ( errno == EDOM )
            {
                print( FATAL, "Argument (%f) out of range.\n",
                       ( double ) VALUE( v ) );
                THROW( EXCEPTION );
            }
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = sqrt( *lsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%ld) out of range.\n", *--lsrc );
                    THROW( EXCEPTION );
                }
            }
            break;


        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            dest = new_var->val.dpnt;
            errno = 0;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                *dest++ = sqrt( *dsrc++ );
                if ( errno == EDOM )
                {
                    print( FATAL, "Argument (%f) out of range.\n", *--dsrc );
                    THROW( EXCEPTION );
                }
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_sqrt( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------------------*
 * Returns random numbers from the interval [0:1] (i.e. larger
 * or equal to 0 and less or equal to 1)
 *-------------------------------------------------------------*/

Var_T *
f_random( Var_T * v )
{
    if ( v == NULL )
        return vars_push( FLOAT_VAR, random( ) / ( double ) RAND_MAX );

    long len = get_long( v, "number of points" );

    if ( len <= 0 )
    {
        print( FATAL, "Zero or negative number (%ld) of points specified.\n",
               len );
        THROW( EXCEPTION );
    }

    Var_T * new_var = vars_push( FLOAT_ARR, NULL, len );
    double * dest = new_var->val.dpnt;
    for ( long i = 0; i < len; i++ )
        *dest++ = random( ) / ( double ) RAND_MAX;

    return new_var;
}


/*-------------------------------------------------------------*
 * Returns random numbers from the interval [0:1[ (i.e. larger
 * or equal to 0 and less than 1)
 *-------------------------------------------------------------*/

Var_T *
f_random2( Var_T * v )
{
    if ( v == NULL )
        return vars_push( FLOAT_VAR, random( ) / ( RAND_MAX + 1.0 ) );

    long len = get_long( v, "number of points" );

    if ( len <= 0 )
    {
        print( FATAL, "Zero or negative number (%ld) of points specified.\n",
               len );
        THROW( EXCEPTION );
    }

    Var_T * new_var = vars_push( FLOAT_ARR, NULL, len );
    double * dest = new_var->val.dpnt;
    for ( long i = 0; i < len; i++ )
        *dest++ = random( ) / ( RAND_MAX + 1.0 );

    return new_var;
}


/*-----------------------------------*
 * Shuffles the elements of an array
 *-----------------------------------*/

Var_T *
f_shuffle( Var_T * v )
{
    Var_T * nv = NULL;

    vars_check( v, INT_ARR | FLOAT_ARR );

    if ( v->type == INT_ARR )
    {
        nv = vars_push( INT_ARR, v->val.lpnt, ( long ) v->len );

        for ( long n = v->len - 1; n > 0; n-- )
        {
            long k = ( n + 1 ) * ( random( ) / ( RAND_MAX + 1.0 ) );
            long tmp = nv->val.lpnt[ n ];

            nv->val.lpnt[ n ] = nv->val.lpnt[ k ];
            nv->val.lpnt[ k ] = tmp;
        }                
    }
    else
    {
        nv = vars_push( FLOAT_ARR, v->val.lpnt, ( long ) v->len );

        for ( long n = v->len - 1; n > 0; n-- )
        {
            long k = ( n + 1 ) * ( random( ) / ( RAND_MAX + 1.0 ) );
            double tmp = nv->val.dpnt[ n ];

            nv->val.dpnt[ n ] = nv->val.dpnt[ k ];
            nv->val.dpnt[ k ] = tmp;
        }                
    }

    return nv;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

static bool grand_is_old = false;

Var_T *
f_grand( Var_T * v )
{
    if ( v == NULL )
        return vars_push( FLOAT_VAR, gauss_random( ) );

    long len = get_long( v, "number of points" );

    if ( len <= 0 )
    {
        print( FATAL, "Zero or negative number (%ld) of points specified.\n",
               len );
        THROW( EXCEPTION );
    }

    Var_T * new_var = vars_push( FLOAT_ARR, NULL, len );
    double *dest = new_var->val.dpnt;
    for ( long i = 0; i < len; i++ )
        *dest++ = gauss_random( );

    return new_var;
}


/*----------------------------------------------------------------------*
 * Returns random numbers with Gaussian distribution, zero mean value
 * and variance of 1. See Press et al., "Numerical Recipes in C", 2nd
 * ed., Cambridge University Press 1992, pp. 288-290, for all the gory
 * details.
 *----------------------------------------------------------------------*/

static
double
gauss_random( void )
{
    static double next_val;

    if ( ! grand_is_old )
    {
        double radius, val_1, val_2;

        do
        {
            val_1 = 2.0 * random( ) / ( double ) RAND_MAX - 1.0;
            val_2 = 2.0 * random( ) / ( double ) RAND_MAX - 1.0;
            radius = val_1 * val_1 + val_2 * val_2;
        } while ( radius < 0.0 || radius >= 1.0 );

        double factor = sqrt( - 2.0 * log( radius ) / radius );
        next_val = val_1 * factor;
        grand_is_old = true;
        return val_2 * factor;
    }

    grand_is_old = false;
    return next_val;
}


/*---------------------------------------------------------------------*
 * Sets a seed for the random number generator. It expects either a
 * positive integer as its argument or none, in which case the current
 * time (in seconds since 00:00:00 UTC, January 1, 1970) is used.
 *---------------------------------------------------------------------*/

Var_T *
f_setseed( Var_T * v )
{
    unsigned int arg;

    if ( v != NULL )
    {
        vars_check( v, INT_VAR | FLOAT_VAR );

        if ( v->type == INT_VAR )
        {
            if ( v->val.lval < 0 )
                print( SEVERE, "Positive integer argument needed, "
                       "using absolute value.\n" );
            arg = ( unsigned int ) labs( v->val.lval );
        }
        else
        {
            print( SEVERE, "Positive integer argument needed and not a float "
                   "variable, using 1 instead.\n" );
            arg = 1;
        }

        if ( arg > RAND_MAX )
        {
            print( SEVERE, "Seed for random generator too large, "
                   "maximum is %ld. Using 1 instead\n", RAND_MAX );
            arg = 1;
        }
    }
    else
    {
        arg = ( unsigned int ) time( NULL );
        while ( arg > RAND_MAX )
            arg >>= 1;
    }

    grand_is_old = false;
    srandom( arg );
    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------------*
 * Returns a string with the current time in the form hh:mm:ss.
 * If a string argument is passed to the function the colons
 * are replaced by the first two elements of the string (or the
 * first if there's only one character in the string). The new
 * separator characters must be printable, i.e. between (and
 * including) 0x20 and 0x7E for a 7-bit ASCII character set.
 *--------------------------------------------------------------*/

Var_T *
f_time( Var_T * v )
{
    char ts[ 100 ];
    char sep[ 2 ] = { ':', ':' };
    char *sp;
    char *str = NULL;

    if ( v != NULL )
    {
        if ( v->type != STR_VAR )
        {
            print( FATAL, "Argument must be a string of one or two separator "
                   "characters.\n" );
            THROW( EXCEPTION );
        }

        str = handle_escape( v->val.sptr );

        if ( *str == '\0' )
            print( SEVERE, "Argument string does not contain any characters, "
                   "using ':' as separator.\n ");
        else
        {
            if ( strlen( str ) > 2 )
                print ( SEVERE, "Argument string contains more than two "
                        "characters, using only the first two.\n ");

            for ( sp = str; *sp; sp++ )
                if ( ! isprint( *sp ) )
                {
                    print( SEVERE, "Argument string contains non-printable "
                           "characters, using ':' as separator.\n" );
                    break;
                }

            if ( *sp == '\0' )
            {
                sep[ 0 ] = *str;
                if ( str[ 1 ] != '\0' )
                    sep[ 1 ] = str[ 1 ];
                else
                    sep[ 1 ] = *str;
            }
        }
    }

    time_t tp;
    time( &tp );

    if ( strftime( ts, 100, "%H:%M:%S", localtime( &tp ) ) == 0 )
    {
        print( SEVERE, "Returning invalid time string.\n" );
        strcat( ts, "(Unknown time)" );
    }

    sp = ts;
    for ( int i = 0; i < 2; i++ )
    {
        sp = strchr( sp, ':' );
        *sp = sep[ i ];
    }

    return vars_push( STR_VAR, ts );
}


/*----------------------------------------------------------*
 * Returns a floating point value with the time difference
 * since the last call of the function (in us resolution).
 * The function should be called automatically at the start
 * of an experiment so that the user gets the time since
 * the start of the experiment when calling this function
 * for the very first time.
 *----------------------------------------------------------*/

Var_T *
f_dtime( Var_T * v  UNUSED_ARG )
{
    static double old_time = 0.0;

    double new_time = experiment_time( );
    double diff_time = new_time - old_time;
    old_time = new_time;
    return vars_push( FLOAT_VAR, diff_time );
}


/*-------------------------------------------------------------------------*
 * When called without an argument the function returns a string with the
 * current date in a form like "Sat Jun 17, 2000". Alternatively, it may
 * be called with a format string acceptable to the strftime(3) function
 * as the only argument to force the function to return a string with the
 * date in a user specified way (with a maximum length of 255 characters).
 *-------------------------------------------------------------------------*/

#define DEFAULT_DATE_FORMAT          "%a %b %d, %Y"
#define DATE_FLAGS                   "aAbBcCdDeFGghHIjklmMnPrRsStTuUVWxXyYzZ+%"
#define DATE_FLAGS_WITH_E_MODIFIER   "cCxXyY"
#define DATE_FLAGS_WITH_O_MODIFIER   "deHImMSuUVwWy"

Var_T *
f_date( Var_T * v )
{
    time_t tp;
    char ts[ 256 ];
    char *str = ( char * ) DEFAULT_DATE_FORMAT;
    char *sp;


    if ( v != NULL )
    {
        if ( v->type != STR_VAR )
        {
            print( FATAL, "Argument must be a format string acceptable to "
                   "the strftime(3) function.\n" );
            THROW( EXCEPTION );
        }

        str = handle_escape( v->val.sptr );

        if ( *str == '\0' )
        {
            print( SEVERE, "Argument string is empty, using default date "
                   "format (\"%s\").\n", DEFAULT_DATE_FORMAT );
            str = ( char * ) DEFAULT_DATE_FORMAT;
        }

        for ( sp = str; *sp; sp++ )
        {
            if ( *sp != '%' )
            {
                if ( ! isprint( *sp ) )
                {
                    print( FATAL, "Format string contains non-printable "
                           "characters.\n" );
                    THROW( EXCEPTION );
                }

                continue;
            }

            if ( ! *++sp )
            {
                print( FATAL, "Missing conversion specifier after '%%'.\n" );
                THROW( EXCEPTION );
            }

            if ( strchr( DATE_FLAGS, *sp ) != NULL )
                continue;

            if ( *sp == 'E' )
            {
                if ( ! *++sp )
                {
                    print( FATAL, "Missing conversion specifier after "
                           "\"%%E\".\n" );
                    THROW( EXCEPTION );
                }

                if ( strchr( DATE_FLAGS_WITH_E_MODIFIER, *sp ) )
                     continue;

                print( FATAL, "Invalid conversion specifier '%c' after "
                       "\"%%E\".\n", *sp );
                THROW( EXCEPTION );
            }

            if ( *sp == 'O' )
            {
                if ( ! *++sp )
                {
                    print( FATAL, "Missing conversion specifier after "
                           "\"%%O\".\n" );
                    THROW( EXCEPTION );
                }

                if ( strchr( DATE_FLAGS_WITH_O_MODIFIER, *sp ) )
                     continue;

                print( FATAL, "Invalid conversion specifier '%c' after "
                       "\"%%O\".\n", *sp );
                THROW( EXCEPTION );
            }

            print( FATAL, "Invalid conversion specifier '%c'.\n", *sp );
            THROW( EXCEPTION );
        }
    }

    time( &tp );
    if ( strftime( ts, 256, str, localtime( &tp ) ) == 0 )
    {
        print( SEVERE, "Returning invalid date string.\n" );
        strcat( ts, "(Unknown date)" );
    }

    return vars_push( STR_VAR, ts );
}


/*---------------------------------------------*
 * Function returns the dimension of an array.
 *---------------------------------------------*/

Var_T *
f_dim( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );
    return vars_push( INT_VAR, ( long ) v->dim );
}


/*-----------------------------------------------------------------*
 * Function returns the length of an array or the number of rows
 * of a matrix (for simple numerical variables 1 is returned and
 * a warning is printed while for string variables the lenght of
 * the string is returned). For arrays or matrices 0 gets returned
 * when no elements exist yet.
 *-----------------------------------------------------------------*/

Var_T *
f_size( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR |
                   INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF );

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            print( WARN, "Argument is a number.\n" );
            return vars_push( INT_VAR, 1L );

        case STR_VAR :
            return vars_push( INT_VAR, ( long ) strlen( v->val.sptr ) );

        case INT_ARR :
        case FLOAT_ARR :
        case INT_REF :
        case FLOAT_REF :
            return vars_push( INT_VAR, ( long ) v->len );

        default :
            fsc2_impossible( );
    }

    fsc2_impossible( );
    return NULL;
}


/*----------------------------------------------------------------------*
 * Calculates the mean of the elements of an one dimensional array.
 * If only an array (or a pointer to an array is passed to the function
 * the mean of all array elements is calculated. If there's a second
 * argument it's taken to be an index into the array at which the
 * calculation of the mean starts. If there's a third argument it has
 * be the number of elements to be included into the mean.
 *----------------------------------------------------------------------*/

Var_T *
f_mean( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    if ( v == NULL )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long start = 0;
    ssize_t len = v->len;

    if ( ! ( v->type & ( INT_VAR | FLOAT_VAR ) ) && v->next != NULL )
    {
        start = get_long( v->next, "start index in array" ) - ARRAY_OFFSET;

        if ( start < 0 || start >= v->len )
        {
            print( FATAL, "Invalid start index (%ld).\n", start );
            THROW( EXCEPTION );
        }

        if ( v->next->next != NULL )
        {
            len = get_long( v->next->next, "array slice length" );

            if ( len < 1 )
            {
                print( FATAL, "Invalid array slice length (%ld).\n",
                       ( long ) len );
                THROW( EXCEPTION );
            }

            if ( start + len > v->len )
            {
                print( FATAL, "Sum of index and slice length parameter "
                       "exceeds length of array.\n" );
                THROW( EXCEPTION );
            }
        }
        else
            len = v->len - start;
    }

    ssize_t i;
    double sum = 0.0;
    long count = 0;
    void *gp;

    switch ( v->type )
    {
        case INT_VAR :
            print( WARN, "Argument is a number.\n" );
            return vars_push( INT_VAR, v->val.lval );

        case FLOAT_VAR :
            print( WARN, "Argument is a number.\n" );
            return vars_push( FLOAT_VAR, v->val.dval );

        case INT_ARR :
            if ( v->len == 0 )
            {
                count = 0;
                break;
            }

            for ( count = 0, i = start; i < start + len; count++, i++ )
                sum += ( double ) v->val.lpnt[ i ];
            break;

        case FLOAT_ARR :
            if ( v->len == 0 )
            {
                count = 0;
                break;
            }

            for ( count = 0, i = start; i < start + len; count++, i++ )
                sum += v->val.dpnt[ i ];
            break;

        case INT_REF :
            if ( v->len == 0 )
            {
                count = 0;
                break;
            }

            if ( start == 0 && len == v->len )
            {
                count = 0;
                while ( ( gp = vars_iter( v ) ) != NULL )
                {
                    sum += * ( long * ) gp;
                    count++;
                }
            }
            else
                for ( count = 0, i = start; i < start + len; i++ )
                    while ( ( gp = vars_iter( v->val.vptr[ i ] ) ) != NULL )
                    {
                        sum += * ( long * ) gp;
                        count++;
                    }
            break;

        case FLOAT_REF :
            if ( v->len == 0 )
            {
                count = 0;
                break;
            }

            if ( start == 0 && len == v->len )
            {
                count = 0;
                while ( ( gp = vars_iter( v ) ) != NULL )
                {
                    sum += * ( double * ) gp;
                    count++;
                }
            }
            else
                for ( count = 0, i = start; i < start + len; i++ )
                    while ( ( gp = vars_iter( v->val.vptr[ i ] ) ) != NULL )
                    {
                        sum += * ( double * ) gp;
                        count++;
                    }
            break;

        default :
            fsc2_impossible( );
    }

    if ( count == 0 )
    {
        print( FATAL, "Number of array or matrix elements isn't set.\n" );
        THROW( EXCEPTION );
    }

    return vars_push( FLOAT_VAR, sum / ( double ) count );
}


/*------------------------------------------------------------------*
 *------------------------------------------------------------------*/

Var_T *
f_rms( Var_T * v )
{
    vars_check( v, INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF );

    if ( v == NULL )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    long start = 0;
    ssize_t len = v->len;

    if ( len == 0 )
    {
        print( FATAL, "Length of array isn't know yet.\n" );
        THROW( EXCEPTION );
    }

    if ( v->next != NULL )
    {
        start = get_long( v->next, "start index in array" ) - ARRAY_OFFSET;

        if ( start < 0 || start >= v->len )
        {
            print( FATAL, "Invalid start index (%ld).\n", start );
            THROW( EXCEPTION );
        }

        if ( v->next->next != NULL )
        {
            len = get_long( v->next->next, "array slice length" );

            if ( len < 1 )
            {
                print( FATAL, "Invalid array slice length (%ld).\n",
                       ( long ) len );
                THROW( EXCEPTION );
            }

            if ( start + len > v->len )
            {
                print( FATAL, "Sum of index and slice length parameter "
                       "exceeds length of array.\n" );
                THROW( EXCEPTION );
            }
        }
        else
            len = v->len - start;
    }

    ssize_t i;
    double sum = 0.0;
    long count = 0;
    void *gp;

    switch ( v->type )
    {
        case INT_ARR :
            for ( count = 0, i = start; i < start + len; count++, i++ )
                sum +=
                     ( double ) v->val.lpnt[ i ] * ( double ) v->val.lpnt[ i ];
            break;

        case FLOAT_ARR :
            for ( count = 0, i = start; i < start + len; count++, i++ )
                sum += v->val.dpnt[ i ] * v->val.dpnt[ i ];
            break;

        case INT_REF :
            if ( start == 0 && len == v->len )
            {
                count = 0;
                while ( ( gp = vars_iter( v ) ) != NULL )
                {
                    sum += * ( long * ) gp * * ( long * ) gp;
                    count++;
                }
            }
            else
                for ( count = 0, i = start; i < start + len; i++ )
                    while ( ( gp = vars_iter( v->val.vptr[ i ] ) ) != NULL )
                    {
                        sum += * ( long * ) gp * * ( long * ) gp;
                        count++;
                    }
            break;

        case FLOAT_REF :
            if ( start == 0 && len == v->len )
            {
                count = 0;
                while ( ( gp = vars_iter( v ) ) != NULL )
                {
                    sum += * ( double * ) gp * * ( double * ) gp;
                    count++;
                }
            }
            else
                for ( count = 0, i = start; i < start + len; i++ )
                    while ( ( gp = vars_iter( v->val.vptr[ i ] ) ) != NULL )
                    {
                        sum += * ( double * ) gp * * ( double * ) gp;
                        count++;
                    }
            break;

        default :
            fsc2_impossible( );
    }

    return vars_push( FLOAT_VAR, sqrt( sum ) / ( double ) count );
}


/*----------------------------------------------*
 * Function for creating a slice of an 1D array
 * or a submatrix of a more-dimensional matrix
 *----------------------------------------------*/

Var_T *
f_slice( Var_T * v )
{
    vars_check( v, INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF );

    ssize_t len = v->len;
    if ( len == 0 )
    {
        print( FATAL, "Length of array isn't know yet.\n" );
        THROW( EXCEPTION );
    }

    long start = 0;
    if ( v->next != NULL )
    {
        start = get_long( v->next, "array index" ) - ARRAY_OFFSET;

        if ( start < 0 || start >= len )
        {
            print( FATAL, "Invalid array index (%ld).\n",
                   start + ARRAY_OFFSET );
            THROW( EXCEPTION );
        }

        if ( v->next->next != NULL )
        {
            len = get_long( v->next->next, "length of slice" );

            if ( len < 1 )
            {
                print( FATAL, "Invalid array slice length (%ld).\n",
                       ( long ) len );
                THROW( EXCEPTION );
            }

            if ( start + len > v->len )
            {
                print( FATAL, "Sum of index and slice length parameter "
                       "exceeds length of array.\n" );
                THROW( EXCEPTION );
            }

            if ( v->next->next->next != NULL )
                print( WARN, "Too many arguments, discarding superfluous "
                       "arguments.\n",
                       v->next->next->next->next != NULL ? "s" : "" );
        }
        else
            len = v->len - start;
    }

    Var_T * new_var = NULL;
    ssize_t volatile old_len;
    Var_T ** volatile old_vptr;

    switch ( v->type )
    {
        case INT_ARR :
            new_var = vars_push( INT_ARR, v->val.lpnt + start, ( long ) len );
            break;

        case FLOAT_ARR :
            new_var = vars_push( FLOAT_ARR, v->val.dpnt + start, ( long ) len );
            break;

        case INT_REF :
        case FLOAT_REF :
            old_len  = v->len;
            old_vptr = v->val.vptr;

            v->val.vptr = v->val.vptr + start;
            v->len      = len;

            TRY
            {
                new_var = vars_push( v->type, v );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                v->val.vptr = old_vptr;
                v->len      = old_len;
                RETHROW;
            }

            v->val.vptr = old_vptr;
            v->len      = old_len;
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

Var_T *
f_square( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;

    switch ( v->type )
    {
        case INT_VAR :
            if ( labs( v->val.lval ) >= sqrt( LONG_MAX ) )
                print( SEVERE, "Result overflow.\n" );
            return vars_push( INT_VAR, v->val.lval * v->val.lval );

        case FLOAT_VAR :
            if ( fabs( v->val.dval ) > sqrt( HUGE_VAL ) )
                print( SEVERE, "Result overflow.\n" );
            return vars_push( FLOAT_VAR, v->val.dval * v->val.dval );

        case INT_ARR :
            new_var = vars_make( INT_ARR, v );
            long lmax = sqrt( ( double ) LONG_MAX );
            long * restrict lsrc  = v->val.lpnt;
            long * restrict ldest = new_var->val.lpnt;
            for ( ssize_t i = 0; i < v->len; i++, lsrc++ )
            {
                if ( labs( *lsrc ) > lmax )
                    print( SEVERE, "Result overflow.\n" );
                *ldest++ = *lsrc * *lsrc;
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double dmax = sqrt( HUGE_VAL );
            double * restrict dsrc  = v->val.dpnt;
            double * restrict ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++, dsrc++ )
            {
                if ( fabs( *dsrc ) > dmax )
                    print( SEVERE, "Result overflow.\n" );
                *ddest++ = *dsrc * *dsrc;
            }
            break;

        case INT_REF :
            new_var = vars_make( INT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_square( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_square( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------------------*
 * Function for converting magnetic fields from Gauss to Tesla
 *-------------------------------------------------------------*/

Var_T *
f_G2T( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict ddest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, VALUE( v ) * 1.0e-4 );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            ddest = new_var->val.dpnt;
            long * restrict lsrc = v->val.lpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *lsrc++ * 1.0e-4;
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            ddest = new_var->val.dpnt;
            double * restrict dsrc = v->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *dsrc++ * 1.0e-4;
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_G2T( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------------------*
 * Function for converting magnetic fields from Tesla to Gauss
 *-------------------------------------------------------------*/

Var_T *
f_T2G( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict ddest;


    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, VALUE( v ) * 1.0e4 );
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *lsrc++ * 1.0e4;
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *dsrc++ * 1.0e4;
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_T2G( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------------------------*
 * Function for converting temperatures from degree Celsius to Kevin
 *-------------------------------------------------------------------*/

Var_T *
f_C2K( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T *new_var = NULL;
    double * restrict ddest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, VALUE( v ) + C2K_OFFSET );
            break;

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *lsrc++ + C2K_OFFSET;
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *dsrc++ + C2K_OFFSET;
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_C2K( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------------------------*
 * Function for converting temperatures from Kevin to degree Celsius
 *-------------------------------------------------------------------*/

Var_T *
f_K2C( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict ddest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, VALUE( v ) - C2K_OFFSET );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = ( double ) *lsrc++ - C2K_OFFSET;
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *dsrc++ - C2K_OFFSET;
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_K2C( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------------------------*
 * Function for converting values in degrees to radians
 *------------------------------------------------------*/

Var_T *
f_D2R( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict ddest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, VALUE( v ) * D2R_FACTOR );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = ( double ) *lsrc++ * D2R_FACTOR;
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *dsrc++ * D2R_FACTOR;
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_D2R( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------------------------*
 * Function for converting values in radians to degrees
 *------------------------------------------------------*/

Var_T *
f_R2D( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict ddest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, VALUE( v ) * R2D_FACTOR );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = ( double ) *lsrc++ * R2D_FACTOR;
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *dsrc++ * R2D_FACTOR;
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_R2D( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------------------------------*
 * Function for converting wave lengths (in m) to wavenumbers (i.e. cm^-1)
 *-------------------------------------------------------------------------*/

Var_T *
f_WL2WN( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict ddest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            if ( VALUE( v ) == 0.0 )
            {
                print( FATAL, "Can't convert 0 m to a wave number.\n" );
                THROW( EXCEPTION );
            }
            return vars_push( FLOAT_VAR, 0.01 / VALUE( v ) );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                if ( *lsrc == 0 )
                {
                    print( FATAL, "Can't convert 0 m to a wave number.\n" );
                    THROW( EXCEPTION );
                }
                *ddest++ = 0.01 / *lsrc++;
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                if ( *dsrc == 0.0 )
                {
                    print( FATAL, "Can't convert 0 m to a wave number.\n" );
                    THROW( EXCEPTION );
                }
                *ddest++ = 0.01 / *dsrc++;
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_WL2WN( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------------------------------*
 * Function for converting wavenumbers (i.e. cm^-1) to wave lengths (in m)
 *-------------------------------------------------------------------------*/

Var_T *
f_WN2WL( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict ddest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            if ( VALUE( v ) == 0.0 )
            {
                print( FATAL, "Can't convert 0 cm^-1 to a wavelength.\n" );
                THROW( EXCEPTION );
            }
            return vars_push( FLOAT_VAR, 0.01 / VALUE( v ) );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                if ( *lsrc == 0 )
                {
                    print( FATAL, "Can't convert 0 cm^-1 to a wavelength.\n" );
                    THROW( EXCEPTION );
                }
                *ddest++ = 0.01 / *lsrc++;
            }
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
            {
                if ( *dsrc == 0.0 )
                {
                    print( FATAL, "Can't convert 0 cm^-1 to a wavelength.\n" );
                    THROW( EXCEPTION );
                }
                *ddest++ = 0.01 / *dsrc++;
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_WN2WL( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------------------------------*
 * Function for converting frequencies (in Hz) to wavenumbers (i.e. cm^-1)
 *-------------------------------------------------------------------------*/

Var_T *
f_F2WN( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict ddest;

    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, VALUE( v ) / WN2F_FACTOR );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = ( double ) *lsrc++ / WN2F_FACTOR;
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = *dsrc++ / WN2F_FACTOR;
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_F2WN( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*-------------------------------------------------------------------------*
 * Function for converting wavenumbers (i.e. cm^-1) to frequencies (in Hz)
 *-------------------------------------------------------------------------*/

Var_T *
f_WN2F( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                   INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;
    double * restrict ddest;


    switch ( v->type )
    {
        case INT_VAR :
        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, VALUE( v ) * WN2F_FACTOR );

        case INT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            long * restrict lsrc = v->val.lpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = WN2F_FACTOR * ( double ) *lsrc++;
            break;

        case FLOAT_ARR :
            new_var = vars_make( FLOAT_ARR, v );
            double * restrict dsrc = v->val.dpnt;
            ddest = new_var->val.dpnt;
            for ( ssize_t i = 0; i < v->len; i++ )
                *ddest++ = WN2F_FACTOR *  *dsrc++;
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_WN2F( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

Var_T *
f_islice( Var_T * v )
{
    long size = get_long( v, "array size" );

    if ( size < 0 )
    {
        if ( v->type == INT_VAR )
            print( FATAL, "Negative value (%ld) used as array size.\n",
                   v->val.lval );
        else
            print( FATAL, "Negative value (%f) used as array size.\n",
                   v->val.dval );
        THROW( EXCEPTION );
    }

    return vars_push( INT_ARR, NULL, size );
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

Var_T *
f_fslice( Var_T * v )
{
    long size = get_long( v, "array size" );

    if ( size < 0 )
    {
        if ( v->type == INT_VAR )
            print( FATAL, "Negative value (%ld) used as array size.\n",
                   v->val.lval );
        else
            print( FATAL, "Negative value (%f) used as array size.\n",
                   v->val.dval );
        THROW( EXCEPTION );
    }

    return vars_push( FLOAT_ARR, NULL, size );
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

Var_T *
f_lspace( Var_T * v )
{
    double start = get_double( v, NULL );
    double end = get_double( v = vars_pop( v ), NULL );
    long num = get_long( v = vars_pop( v ), "number of points" );

    if ( num < 2 )
    {
        print( FATAL, "Invalid number of points.\n" );
        THROW( EXCEPTION );
    }

    double incr = ( end - start ) / ( num - 1 );

    Var_T * nv = vars_push( FLOAT_ARR, NULL, num );
    double * d = nv->val.dpnt;
    for ( long i = 0; i < num; start += incr, i++ )
        *d++ = start;

    return nv;
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

Var_T *
f_reverse( Var_T * v )
{
    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR |
                   INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF );

    Var_T * new_var = NULL;

    switch ( v->type )
    {
        case INT_VAR :
            return vars_push( INT_VAR, v->val.lval );

        case FLOAT_VAR :
            return vars_push( FLOAT_VAR, v->val.dval );


        case STR_VAR :
            new_var = vars_push( STR_VAR, v->val.sptr );
            char * sp = new_var->val.sptr;
            char * ep = sp + strlen( sp ) - 1;
            while ( sp < ep )
            {
                char tmp = *sp;
                *sp++    = *ep;
                *ep--    = tmp;
            }
            break;

        case INT_ARR :
            new_var = vars_push( INT_ARR, v->val.lpnt, ( long ) v->len );
            long * lsrc = new_var->val.lpnt;
            long * ldest = lsrc + new_var->len - 1;
            while ( lsrc < ldest )
            {
                long ltemp = *lsrc;
                *lsrc++    = *ldest;
                *ldest--   = ltemp;
            }
            break;

        case FLOAT_ARR :
            new_var = vars_push( FLOAT_ARR, v->val.dpnt, ( long ) v->len );
            double * dsrc = new_var->val.dpnt;
            double * ddest = dsrc + new_var->len - 1;
            while ( dsrc < ddest )
            {
                double dtemp = *dsrc;
                *dsrc++      = *ddest;
                *ddest--     = dtemp;
            }
            break;

        case INT_REF :
        case FLOAT_REF :
            new_var = vars_make( FLOAT_REF, v );
            for ( ssize_t i = 0; i < v->len; i++ )
                if ( v->val.vptr[ i ] == NULL )
                    new_var->val.vptr[ i ] = NULL;
                else
                {
                    new_var->val.vptr[ i ] = f_reverse( v->val.vptr[ i ] );
                    new_var->val.vptr[ i ]->from = new_var;
                }
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

static
int
sort_long_up( const void * a,
              const void * b )
{
    long x = * ( long * ) a;
    long y = * ( long * ) b;

    return x > y ? -1 : ( x == y ? 0 : 1 );
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

static
int
sort_long_down( const void * a,
                const void * b )
{
    long x = * ( long * ) a;
    long y = * ( long * ) b;

    return x < y ? -1 : ( x == y ? 0 : 1 );
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

static
int
sort_double_up( const void * a,
                const void * b )
{
    double x = * ( double * ) a;
    double y = * ( double * ) b;

    return x > y ? -1 : ( x == y ? 0 : 1 );
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

static
int
sort_double_down( const void * a,
                  const void * b )
{
    double x = * ( double * ) a;
    double y = * ( double * ) b;

    return x < y ? -1 : ( x == y ? 0 : 1 );
}


/*------------------------------------------------------------*
 *------------------------------------------------------------*/

Var_T *
f_sort( Var_T * v )
{
    Var_T *new_var = NULL;
    bool dir = 0;

    if ( v == NULL )
    {
        print( FATAL, "Missing argument.\n" );
        THROW( EXCEPTION );
    }

    if ( v->type & ( INT_VAR | FLOAT_VAR | STR_VAR | INT_REF | FLOAT_REF ) )
    {
        print( FATAL, "Argument isn't a 1-dimensiobal array.\n" );
        THROW( EXCEPTION );
    }

    vars_check( v, INT_ARR | FLOAT_ARR );

    if ( v->next != NULL )
    {
        vars_check( v->next, INT_VAR | FLOAT_VAR | STR_VAR );

        if ( v->next->type == INT_VAR )
            dir = v->next->val.lval ? true : false;
        else if ( v->next->type == FLOAT_VAR )
            dir = v->next->val.dval ? true : false;
        else
        {
            if (    ! strcmp( v->next->val.sptr, "UP" )
                 || ! strcmp( v->next->val.sptr, "UPWARDS" ) )
                dir = false;
            else if (    ! strcmp( v->next->val.sptr, "DOWN" )
                 || ! strcmp( v->next->val.sptr, "DOWNWARDS" ) )
                dir = true;
            else
            {
                print( FATAL, "Invalid second argument.\n" );
                THROW( EXCEPTION );
            }
        }
    }

    switch ( v->type )
    {
        case INT_ARR :
            new_var = vars_push( INT_ARR, v->val.lpnt, ( long ) v->len );
            qsort( new_var->val.lpnt, new_var->len, sizeof *new_var->val.lpnt,
                   dir ? sort_long_up : sort_long_down );
            break;

        case FLOAT_ARR :
            new_var = vars_push( FLOAT_ARR, v->val.dpnt, ( long ) v->len );
            qsort( new_var->val.dpnt, new_var->len, sizeof *new_var->val.dpnt,
                   dir ? sort_double_up : sort_double_down );
            break;

        default :
            fsc2_impossible( );
    }

    return new_var;
}


/*------------------------------------------------------------------------*
 * Function for adding a new value (or array or matrix) to an average so
 * that the new average (always as a float number or array) gets returned
 *------------------------------------------------------------------------*/

Var_T *
f_add2avg( Var_T * v )
{
    Var_T *avg = ( v->type & ( INT_VAR | INT_ARR | INT_REF ) ) ?
                 f_float( v ) : v;
    Var_T *data = v->next;
    Var_T *cnt = data->next;
    long count = get_long( cnt, "average count" );
    Var_T *scans_done = vars_push( FLOAT_VAR, count - 1.0 );


    if ( count < 1 )
    {
        print( FATAL, "Average count is less than 1.\n" );
        THROW( EXCEPTION );
    }

    avg_data_check( avg, data, count );

    /* The next line looks simple but actually involves some trickery. You may
       have wondered why all these extra variables of type 'Var_T *' were
       created above (one might be tempted to use v->next instead of 'data'
       and even v->next->next instead of 'cnt'). This is because all the
       vars_xxx() function return a new variable, destroying the ones passed
       to them (actually, that's not the full truth, the returned variable
       might be one of those passed to the function, but you can't tell if
       this happens and then which of them gets returned). Thus, depending on
       the way the compiler rearranges the code, v->next->next might be a
       pointer to the counter variable from the arguments or, if what was
       v->next got removed, a pointer to nothing at all, or to 'avg' or to
       'scans_done' - one simply can't tell. But the way the function is
       written now one can be sure that everything left on the stack at the
       end of the function is the return value - just as it should be. */

    return vars_div( vars_add( vars_mult( avg, scans_done ), data ), cnt );
}


/*------------------------------------------------------------------------*
 * Function for checking that the sizes of the data and average used in
 * f_add2avg() are set up correctly (if necessary and possible extending
 * the size(s) of the average and initializing it to zero)
 *------------------------------------------------------------------------*/

static
void
avg_data_check( Var_T * avg,
                Var_T * data,
                long    count )
{
    vars_check( data, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
                      INT_REF | FLOAT_REF );

    /* The dimension of the average must always at last as big as the one
       of the data */

    if ( avg->dim < data->dim )
    {
        print( FATAL, "Dimension of average is lower than the one of the "
               "data array or matrix.\n" );
        THROW( EXCEPTION );
    }

    /* Checks for the case that data is a simple number */

    if ( data->type & ( INT_VAR | FLOAT_VAR ) )
    {
        /* Nothing to check if the average is also just a number */

        if ( avg->type == FLOAT_VAR )
            return;

        /* Otherwise the length of the array or matrix must be known */

        if ( avg->len == 0 )
        {
            print( FATAL, "Size(s) of average array aren't set.\n" );
            THROW( EXCEPTION );
        }

        /* If the average is a matrix do the checks recursively */

        if ( avg->type == FLOAT_REF )
            for ( ssize_t i = 0; i < avg->len; i++ )
                avg_data_check( avg->val.vptr[ i ], data, count );

        /* If we got this far everything should be fine */

        return;
    }

    /* If data is an array or a matrix its size must be known */

    if ( data->len == 0 )
    {
        print( FATAL, "Length of data array is still unknown.\n" );
        THROW( EXCEPTION );
    }

    /* Checks for the case that data is a normal array */

    if ( data->type & ( INT_ARR | FLOAT_ARR ) )
    {
        /* If average is also just an array compare the sizes to the one
           of the data array. If the average size is unset set it to the
           same size as the data array (and initialize with zeros), if
           its smaller grudgingly increase the size (and set new elements
           to zero, but draw a line at the case where the average array is
           actually longer */

        if ( avg->type == FLOAT_ARR )
        {
            if ( avg->len == 0 )
            {
                if ( count > 1 )
                {
                    print( SEVERE, "Size of average array hasn't been set "
                           "despite the average counter being larger than "
                           "1.\n" );
                    count = 1;
                }

                avg->val.dpnt = T_malloc( data->len * sizeof *avg->val.dpnt );
                for ( ssize_t i = 0; i < data->len; i++ )
                    avg->val.dpnt[ i ] = 0.0;

                avg->len = data->len;
            }
            else if ( avg->len < data->len )
            {
                print( SEVERE, "Average array is shorter than data array, "
                       "padding it with zeros.\n" );

                avg->val.dpnt = T_realloc( avg->val.dpnt,
                                           data->len * sizeof *avg->val.dpnt );
                for ( ssize_t i = avg->len; i < data->len; i++ )
                    avg->val.dpnt[ i ] = 0.0;
            }
            else if ( avg->len > data->len )
            {
                print( FATAL, "Size of average array is larger than that "
                       "of the data array.\n" );
                THROW( EXCEPTION );
            }

            return;
        }

        /* Now remains the case that the average is a matrix - this requires
           that we know how many subarrays it has */

        if ( avg->len == 0 )
        {
            print( FATAL, "Subarrays of average don't exist yet.\n" );
            THROW( EXCEPTION );
        }

        /* Check recursively for each of the subarrays */

        for ( ssize_t i = 0; i < avg->len; i++ )
            avg_data_check( avg->val.vptr[ i ], data, count );

        return;
    }

    /* When we arrive here both the average as well as the data are both
       matrices. If the averages dimension is higher all its subarrays must
       exist and, if that's the case we can do the checks recursively */

    if ( avg->dim > data->dim )
    {
        if ( avg->len == 0 )
        {
            print( FATAL, "Subarrays of average don't exist yet.\n" );
            THROW( EXCEPTION );
        }

        for ( ssize_t i = 0; i < avg->len; i++ )
            avg_data_check( avg->val.vptr[ i ], data, count );

        return;
    }

    /* The only remaining case is that average and data have the same
       dimension. As in the case of arrays we require that the size of
       the average isn't larger than that of the data. */

    if ( avg->len > data->len )
    {
        print( FATAL, "Size of average matrix is smaller than that of the "
               "data matrix.\n" );
        THROW( EXCEPTION );
    }

    if ( avg->len == 0 )
    {
        if ( count > 1 )
        {
            print( SEVERE, "Size of average matrix hasn't been set "
                   "despite the average counter being larger than 1.\n" );
            count = 1;
        }

        avg->val.vptr = T_realloc( avg->val.vptr,
                                   data->len * sizeof *avg->val.vptr );
    }
    else if ( avg->len < data->len )
    {
        if ( ! ( ( avg->from ? avg->from->flags : avg->flags ) & IS_DYNAMIC ) )
        {
            print( FATAL, "Average matrix is shorter than data matrix.\n" );
            THROW( EXCEPTION );
        }

        print( SEVERE, "Average matrix is shorter than data matrix, "
                       "extending average matrix.\n" );

        avg->val.vptr = T_malloc( data->len * sizeof *avg->val.vptr );
    }

    if ( avg->len != data->len )
    {
        ssize_t start = avg->len;

        for ( ; avg->len < data->len; avg->len++ )
            avg->val.vptr[ avg->len ] = NULL;

        for ( ssize_t i = start; i < avg->len; i++ )
        {
            Var_T *v = avg->val.vptr[ i ] = vars_new( NULL );

            v->from = avg;
            v->flags &= ~ NEW_VARIABLE;
            v->flags |= IS_DYNAMIC | IS_TEMP;
            v->len = 0;
            v->dim = avg->dim - 1;

            if ( avg->dim > 2 )
            {
                v->type = avg->type;
                v->val.vptr = NULL;
            }

            v->type = FLOAT_ARR;
            v->val.dpnt = NULL;
        }
    }

    /* Now everything is set up for a further recursive check */

    for ( ssize_t i = 0; i < avg->len; i++ )
        avg_data_check( avg->val.vptr[ i ], data->val.vptr[ i ], count );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
