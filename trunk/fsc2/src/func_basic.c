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


static double gauss_random( void );
static double datanh( double arg );


#define C2K_OFFSET   273.16
#define D2R_FACTOR   ( atan( 1.0 ) / 45.0 )
#define R2D_FACTOR   ( 45.0 / atan( 1.0 ) )
#define WN2F_FACTOR  2.99792558e10         /* speed of light times 100 */


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

Var_T *f_abort( UNUSED_ARG Var_T *v )
{
	char *str;


	if ( Internals.mode != TEST )
	{
		str = get_string( "Exit due to call of abort() in\n"
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


/*-------------------------------------------*/
/* This is called for the end() EDL function */
/*-------------------------------------------*/

Var_T *f_stopsim( UNUSED_ARG Var_T *v )
{
	EDL.do_quit = SET;
	return NULL;
}


/*-------------------------------------------------*/
/* Conversion float to integer (result is integer) */
/*-------------------------------------------------*/

Var_T *f_int( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *dest;
	double *src;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( INT_VAR, v->val.lval );
			break;

		case FLOAT_VAR :
			if ( v->val.dval > LONG_MAX || v->val.dval < LONG_MIN )
				print( SEVERE, "Integer argument overflow.\n" );
			new_var = vars_push( INT_VAR, ( long ) v->val.dval );
			break;

		case INT_ARR :
			new_var = vars_push( INT_ARR, v->val.lpnt, v->len );
			new_var->flags = v->flags;
			break;

		case FLOAT_ARR :
			new_var = vars_make( INT_ARR, v );
			for ( src = v->val.dpnt, dest = new_var->val.lpnt, i = 0;
				  i < v->len; i++, src++, dest++ )
			{
				if ( *src > LONG_MAX || *src < LONG_MIN )
					print( SEVERE, "Integer argument overflow.\n" );
				*dest = ( long ) *src;
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( INT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_int( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*----------------------------------------------------*/
/* Conversion int to floating point (result is float) */
/*----------------------------------------------------*/

Var_T *f_float( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	double *dest;
	long *src;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR, ( double ) v->val.lval );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.dval );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( src = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, src++, dest++ )
				*dest = ( double ) *src;
			break;

		case FLOAT_ARR :
			new_var = vars_push( FLOAT_ARR, v->val.dpnt, v->len );
			new_var->flags = v->flags;
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_float( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*--------------------------------------------------------*/
/* Rounding of floating point numbers (result is integer) */
/*--------------------------------------------------------*/

Var_T *f_round( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *dest;
	double *src;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( INT_VAR, v->val.lval );
			break;

		case FLOAT_VAR :
			if ( v->val.dval >= LONG_MAX - 0.5 ||
				 v->val.dval <= LONG_MIN + 0.5 )
				print( SEVERE, "Integer argument overflow.\n" );
			new_var = vars_push( INT_VAR, lrnd( v->val.dval ) );
			break;

		case INT_ARR :
			new_var = vars_push( INT_ARR, v->val.lpnt, v->len );
			new_var->flags = v->flags;
			break;

		case FLOAT_ARR :
			new_var = vars_make( INT_ARR, v );
			for ( src = v->val.dpnt, dest = new_var->val.lpnt, i = 0;
				  i < v->len; i++, src++, dest++ )
			{
				if ( *src >= LONG_MAX - 0.5 || *src <= LONG_MIN + 0.5 )
					print( SEVERE, "Integer argument overflow.\n" );
				*dest = lrnd( *src );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( INT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_round( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*---------------------------------*/
/* Floor value (result is integer) */
/*---------------------------------*/

Var_T *f_floor( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *dest;
	double *src;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( INT_VAR, v->val.lval );
			break;

		case FLOAT_VAR :
			if ( v->val.dval < LONG_MIN )
				print( SEVERE, "Integer argument overflow.\n" );
			new_var = vars_push( INT_VAR, lrnd( floor( v->val.dval )  ) );
			break;

		case INT_ARR :
			new_var = vars_push( INT_ARR, v->val.lpnt, v->len );
			new_var->flags = v->flags;
			break;

		case FLOAT_ARR :
			new_var = vars_make( INT_ARR, v );
			for ( src = v->val.dpnt, dest = new_var->val.lpnt, i = 0;
				  i < v->len; i++, src++, dest++ )
			{
				if ( *src < LONG_MIN )
					print( SEVERE, "Integer argument overflow.\n" );
				*dest = lrnd( floor( *src ) );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( INT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_floor( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-----------------------------------*/
/* Ceiling value (result is integer) */
/*-----------------------------------*/

Var_T *f_ceil( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *dest;
	double *src;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( INT_VAR, v->val.lval );
			break;

		case FLOAT_VAR :
			if ( v->val.dval > LONG_MAX )
				print( SEVERE, "Integer argument overflow.\n" );
			new_var = vars_push( INT_VAR, lrnd( ceil( v->val.dval ) ) );
			break;

		case INT_ARR :
			new_var = vars_push( INT_ARR, v->val.lpnt, v->len );
			new_var->flags = v->flags;
			break;

		case FLOAT_ARR :
			new_var = vars_make( INT_ARR, v );
			for ( src = v->val.dpnt, dest = new_var->val.lpnt, i = 0;
				  i < v->len; i++, src++, dest++ )
			{
				if ( *src > LONG_MAX )
					print( SEVERE, "Integer argument overflow.\n" );
				*dest = lrnd( ceil( *src ) );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( INT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_ceil( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-------------------------------------------------*/
/* abs of value (result has same as type argument) */
/*-------------------------------------------------*/

Var_T *f_abs( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			if ( v->val.lval == LONG_MIN )
				print( SEVERE, "Integer argument overflow.\n" );
			new_var = vars_push( INT_VAR, labs( v->val.lval ) );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, fabs( v->val.dval ) );
			break;

		case INT_ARR :
			new_var = vars_make( INT_ARR, v );
			for ( lsrc = v->val.lpnt, ldest = new_var->val.lpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
			{
				if ( *lsrc == LONG_MIN )
					print( SEVERE, "Integer argument overflow.\n" );
				*ldest = labs( *lsrc );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
				*ddest = fabs( *dsrc );
			break;

		case INT_REF :
			new_var = vars_make( INT_REF, v );
			for ( i = 0; i < v->len; i++ )
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
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_abs( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------------------------*/
/* max of all values (result is a float variable unless */
/* all inputs were integer values)                      */
/*------------------------------------------------------*/

Var_T *f_lmax( Var_T *v )
{
	double m = - HUGE_VAL;
	bool all_int = SET;
	ssize_t i;
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
				all_int = UNSET;
				break;

			case INT_ARR :
				for ( i = 0; i < v->len; i++ )
					if ( v->val.lpnt[ i ] > m )
						m = v->val.lpnt[ i ];
				break;
					
			case FLOAT_ARR :
				for ( i = 0; i < v->len; i++ )
					if ( v->val.dpnt[ i ] > m )
						m = v->val.dpnt[ i ];
				all_int = UNSET;
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
				all_int = UNSET;
				break;

			default :
				break;
		}
	}

	if ( all_int && m <= LONG_MAX && m >= LONG_MIN )
		return vars_push( INT_VAR, ( long ) m );
	return vars_push( FLOAT_VAR, m );
}


/*------------------------------------------------------*/
/* min of all values (result is a float variable unless */
/* all inputs were integer values)                      */
/*------------------------------------------------------*/

Var_T *f_lmin( Var_T *v )
{
	double m = HUGE_VAL;
	bool all_int = SET;
	ssize_t i;
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
				all_int = UNSET;
				break;

			case INT_ARR :
				for ( i = 0; i < v->len; i++ )
					if ( v->val.lpnt[ i ] < m )
						m = v->val.lpnt[ i ];
				break;
					
			case FLOAT_ARR :
				for ( i = 0; i < v->len; i++ )
					if ( v->val.dpnt[ i ] < m )
						m = v->val.dpnt[ i ];
				all_int = UNSET;
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
				all_int = UNSET;
				break;

			default :
				break;
		}
	}

	if ( all_int && m <= LONG_MAX && m >= LONG_MIN )
		return vars_push( INT_VAR, ( long ) m );
	return vars_push( FLOAT_VAR, m );
}


/*-----------------------------------------------*/
/* sin of argument (in radian) (result is float) */
/*-----------------------------------------------*/

Var_T *f_sin( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dest, *dsrc;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR, sin( ( double ) v->val.lval ) );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, sin( v->val.dval ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
				*dest = sin( ( double ) *lsrc );
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
				*dest = sin( *dsrc );
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_sin( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-----------------------------------------------*/
/* cos of argument (in radian) (result is float) */
/*-----------------------------------------------*/

Var_T *f_cos( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dest, *dsrc;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR, cos( ( double ) v->val.lval ) );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, cos( v->val.dval ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
				*dest = cos( ( double ) *lsrc );
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
				*dest = cos( *dsrc );
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_cos( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-----------------------------------------------*/
/* tan of argument (in radian) (result is float) */
/*-----------------------------------------------*/

Var_T *f_tan( Var_T *v )
{
	double res;
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			res = tan( VALUE( v ) );
			if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
				print( SEVERE, "Result overflow.\n" );
			new_var = vars_push( FLOAT_VAR, res );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				res = tan( ( double ) *lsrc );
				if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
				*dest = res;
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				res = tan( *dsrc );
				if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
				*dest = res;
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_tan( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*--------------------------------------------------------------------*/
/* asin (in radian) of argument (with -1 <= x <= 1) (result is float) */
/*--------------------------------------------------------------------*/

Var_T *f_asin( Var_T *v )
{
	double arg;
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			arg = VALUE( v );
			if ( fabs( arg ) > 1.0 )
			{
				print( FATAL, "Argument (%f) out of range.\n", arg );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, asin( arg ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				if ( labs( *lsrc ) > 1 )
				{
					print( FATAL, "Argument (%ld) out of range.\n", *lsrc );
					THROW( EXCEPTION );
				}
				*dest = asin( ( double ) *lsrc );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				if ( fabs( *dsrc ) > 1.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", *dsrc );
					THROW( EXCEPTION );
				}
				*dest = asin( *dsrc );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_asin( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*--------------------------------------------------------------------*/
/* acos (in radian) of argument (with -1 <= x <= 1) (result is float) */
/*--------------------------------------------------------------------*/

Var_T *f_acos( Var_T *v )
{
	double arg;
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			arg = VALUE( v );
			if ( fabs( arg ) > 1.0 )
			{
				print( FATAL, "Argument (%f) out of range.\n", arg );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, acos( arg ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				if ( labs( *lsrc ) > 1 )
				{
					print( FATAL, "Argument (%ld) out of range.\n", *lsrc );
					THROW( EXCEPTION );
				}
				*dest = acos( ( double ) *lsrc );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				if ( fabs( *dsrc ) > 1.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", *dsrc );
					THROW( EXCEPTION );
				}
				*dest = acos( *dsrc );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_acos( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------------------*/
/* atan (in radian) of argument (result is float) */
/*------------------------------------------------*/

Var_T *f_atan( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, atan( VALUE( v ) ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
				*dest = atan( ( double ) *lsrc );
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
				*dest = atan( *dsrc );
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_atan( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------*/
/* sinh of argument (result is float) */
/*------------------------------------*/

Var_T *f_sinh( Var_T *v )
{
	double res;
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			res = sinh( VALUE ( v ) );
			if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
				print( SEVERE, "Result overflow.\n" );
			new_var = vars_push( FLOAT_VAR, res );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				*dest = sinh( ( double ) *lsrc );
				if ( fabs( *dest ) == HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				*dest = sinh( *dsrc );
				if ( fabs( *dest ) == HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_sinh( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------*/
/* cosh of argument (result is float) */
/*------------------------------------*/

Var_T *f_cosh( Var_T *v )
{
	double res;
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			res = cosh( VALUE ( v ) );
			if ( fabs( res ) == HUGE_VAL && errno == ERANGE )
				print( SEVERE, "Result overflow.\n" );
			new_var = vars_push( FLOAT_VAR, res );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				*dest = cosh( ( double ) *lsrc );
				if ( fabs( *dest ) == HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				*dest = cosh( *dsrc );
				if ( fabs( *dest ) == HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_cosh( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------*/
/* tanh of argument (result is float) */
/*------------------------------------*/

Var_T *f_tanh( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, tanh( VALUE( v ) ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
				*dest = tanh( ( double ) *lsrc );
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
				*dest = tanh( *dsrc );
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_tanh( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-----------------------------------------------*/
/* Inverse of sinh of argument (result is float) */
/*-----------------------------------------------*/

Var_T *f_asinh( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;
	double arg, new_arg;
	int sgn;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			arg = VALUE( v );
			sgn = arg >= 0.0 ? 1 : -1;
			new_arg = arg = sqrt( 1.0 / ( 1.0 + 1.0 / ( arg * arg ) ) );
			if ( new_arg >= 1.0 )
			{
				print( FATAL, "Argument (%f) out of range.\n", arg );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, sgn * datanh( new_arg ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				arg = ( double ) *lsrc;
				sgn = arg >= 0.0 ? 1 : -1;
				new_arg = sqrt( 1.0 / ( 1.0 + 1.0 / ( arg * arg ) ) );

				if ( new_arg >= 1.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", arg );
					THROW( EXCEPTION );
				}

				*dest = sgn * datanh( new_arg );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				arg = *dsrc;
				sgn = arg >= 0.0 ? 1 : -1;
				new_arg = sqrt( 1.0 / ( 1.0 + 1.0 / ( arg * arg ) ) );

				if ( new_arg >= 1.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", arg );
					THROW( EXCEPTION );
				}

				*dest = sgn * datanh( new_arg );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_asinh( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-----------------------------------------------*/
/* Inverse of cosh of argument (result is float) */
/*-----------------------------------------------*/

Var_T *f_acosh( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;
	double arg, new_arg;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			arg = VALUE( v );
			if ( arg < 1.0 ||
				 ( new_arg = sqrt( 1.0 - 1.0 / ( arg * arg ) ) ) >= 1.0 )
			{
				print( FATAL, "Argument (%f) out of range.\n", arg );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, datanh( new_arg ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				arg = ( double ) *lsrc;
				if ( arg < 1.0 ||
					 ( new_arg = sqrt( 1.0 - 1.0 / ( arg * arg ) ) ) >= 1.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", arg );
					THROW( EXCEPTION );
				}
				*dest = datanh( new_arg );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				arg = *dsrc;
				if ( arg < 1.0 ||
					 ( new_arg = sqrt( 1.0 - 1.0 / ( arg * arg ) ) ) >= 1.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", arg );
					THROW( EXCEPTION );
				}
				*dest = datanh( new_arg );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_acosh( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-----------------------------------------------*/
/* Inverse of tanh of argument (result is float) */
/*-----------------------------------------------*/

Var_T *f_atanh( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;
	double arg;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			arg = VALUE( v );
			if ( arg >= 1.0 || arg <= -1.0 )
			{
				print( FATAL, "Argument (%f) out of range.\n", arg );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, datanh( arg ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				arg = ( double ) *lsrc;
				if ( arg >= 1.0 || arg <= -1.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", arg );
					THROW( EXCEPTION );
				}

				*dest = datanh( arg );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				arg = *dsrc;
				if ( arg >= 1.0 || arg <= -1.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", arg );
					THROW( EXCEPTION );
				}
				*dest = datanh( arg );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_atanh( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-----------------------------------*/
/* exp of argument (result is float) */
/*-----------------------------------*/

Var_T *f_exp( Var_T *v )
{
	Var_T *new_var = NULL;
	double res;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			res = exp( VALUE( v ) );
			if ( res == 0.0 && errno == ERANGE )
				print( WARN, "Result underflow - result is set to 0.\n" );
			if ( res == HUGE_VAL && errno == ERANGE )
				print( SEVERE, "Result overflow.\n" );
			new_var = vars_push( FLOAT_VAR, res );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				*dest = exp( ( double ) *lsrc );
				if ( *dest == 0.0 && errno == ERANGE )
					print( WARN, "Result underflow - result is set to 0.\n" );
				if ( *dest == HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				*dest = exp( *dsrc );
				if ( *dest == 0.0 && errno == ERANGE )
					print( WARN, "Result underflow - result is set to 0.\n" );
				if ( *dest == HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_exp( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-----------------------------------------------*/
/* ln of argument (with x > 0) (result is float) */
/*-----------------------------------------------*/

Var_T *f_ln( Var_T *v )
{
	double arg, res;
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			arg = VALUE( v );
			if ( arg <= 0.0 )
			{
				print( FATAL, "Argument (%f) out of range.\n", arg );
				THROW( EXCEPTION );
			}
			res = log( arg );
			if ( res == - HUGE_VAL && errno == ERANGE )
				print( SEVERE, "Result overflow.\n" );
			new_var = vars_push( FLOAT_VAR, res );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				if ( *lsrc <= 0 )
				{
					print( FATAL, "Argument (%ld) out of range.\n", *lsrc );
					THROW( EXCEPTION );
				}

				*dest = log( ( double ) *lsrc );
				if ( *dest == - HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				if ( *dsrc <= 0.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", *dsrc );
					THROW( EXCEPTION );
				}

				*dest = log( *dsrc );
				if ( *dest == - HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_ln( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------------------*/
/* log of argument (with x > 0) (result is float) */
/*------------------------------------------------*/

Var_T *f_log( Var_T *v )
{
	double arg, res;
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			arg = VALUE( v );
			if ( arg <= 0.0 )
			{
				print( FATAL, "Argument (%f) out of range.\n", arg );
				THROW( EXCEPTION );
			}
			res = log10( arg );
			if ( res == - HUGE_VAL && errno == ERANGE )
				print( SEVERE, "Result overflow.\n" );
			new_var = vars_push( FLOAT_VAR, res );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				if ( *lsrc <= 0 )
				{
					print( FATAL, "Argument (%ld) out of range.\n", *lsrc );
					THROW( EXCEPTION );
				}

				*dest = log10( ( double ) *lsrc );
				if ( *dest == - HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				if ( *dsrc <= 0.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", *dsrc );
					THROW( EXCEPTION );
				}

				*dest = log10( *dsrc );
				if ( *dest == - HUGE_VAL && errno == ERANGE )
					print( SEVERE, "Result overflow.\n" );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_log( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*--------------------------------------------------*/
/* sqrt of argument (with x >= 0) (result is float) */
/*--------------------------------------------------*/

Var_T *f_sqrt( Var_T *v )
{
	double arg;
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc;
	double *dsrc, *dest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			arg = VALUE( v );
			if ( arg < 0.0 )
			{
				print( FATAL, "Argument (%f) out of range.\n", arg );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, sqrt( arg ) );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, dest++ )
			{
				if ( *lsrc < 0 )
				{
					print( FATAL, "Argument (%ld) out of range.\n", *lsrc );
					THROW( EXCEPTION );
				}
				*dest = sqrt( ( double ) *lsrc );
			}
			break;


		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, dest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, dest++ )
			{
				if ( *dsrc < 0.0 )
				{
					print( FATAL, "Argument (%f) out of range.\n", *dsrc );
					THROW( EXCEPTION );
				}
				*dest = sqrt( *dsrc );
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_sqrt( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*----------------------------------------------------------------*/
/* Returns a random number between 0 and 1 (i.e. result is float) */
/*----------------------------------------------------------------*/

Var_T *f_random( Var_T *v )
{
	long len;
	long i;
	double *arr;
	Var_T *new_var = NULL;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, random( ) / ( double ) RAND_MAX );

	len = get_long( v, "number of points" );

	if ( len <= 0 )
	{
		print( FATAL, "Zero or negative number (%ld) of points specified.\n",
			   len );
		THROW( EXCEPTION );
	}

	arr = DOUBLE_P T_malloc( len * sizeof *arr );
	for ( i = 0; i < len; i++ )
		*( arr + i ) = random( ) / ( double ) RAND_MAX;

	new_var = vars_push( FLOAT_ARR, arr, len );
	T_free( arr );

	return new_var;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static bool grand_is_old = UNSET;

Var_T *f_grand( Var_T *v )
{
	long len;
	long i;
	double *arr;
	Var_T *new_var = NULL;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, gauss_random( ) );

	len = get_long( v, "number of points" );

	if ( len <= 0 )
	{
		print( FATAL, "Zero or negative number (%ld) of points specified.\n",
			   len );
		THROW( EXCEPTION );
	}

	arr = DOUBLE_P T_malloc( len * sizeof *arr );
	for ( i = 0; i < len; i++ )
		*( arr + i ) = gauss_random( );

	new_var = vars_push( FLOAT_ARR, arr, len );
	T_free( arr );

	return new_var;
}


/*----------------------------------------------------------------------*/
/* Returns random numbers with Gaussian distribution, zero mean value   */
/* and variance of 1. See W. H. Press, S. A. Teukolsky, W, T. Vettering */
/* and B. P. Flannery, "Numerical Recipes in C", 2nd ed., Cambridge     */
/* University Press 1992, pp. 288-290, for all the gory details.        */
/*----------------------------------------------------------------------*/

static double gauss_random( void )
{
	static double next_val;
	double factor, radius, val_1, val_2;


	if ( ! grand_is_old )
	{
		do {
			val_1 = 2.0 * random( ) / ( double ) RAND_MAX - 1.0;
			val_2 = 2.0 * random( ) / ( double ) RAND_MAX - 1.0;
			radius = val_1 * val_1 + val_2 * val_2;
		} while ( radius < 0.0 || radius >= 1.0 );

		factor = sqrt( - 2.0 * log( radius ) / radius );
		next_val = val_1 * factor;
		grand_is_old = SET;
		return val_2 * factor;
	}

	grand_is_old = UNSET;
	return next_val;
}


/*---------------------------------------------------------------------------*/
/* Sets a seed for the random number generator. It expects either a positive */
/* integer as argument or none, in which case the current time (in seconds   */
/* since 00:00:00 UTC, January 1, 1970) is used.                             */
/*---------------------------------------------------------------------------*/

Var_T *f_setseed( Var_T *v )
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
			print( SEVERE, "Positive integer argument and not a float "
				   "variable as argument, using 1 instead.\n" );
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

	grand_is_old = UNSET;
	srandom( arg );
	return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------------*/
/* Returns a string with the current time in the form hh:mm:ss. */
/* If a string argument is passed to the function the colons    */
/* are replaced by the first two elements of the string (or the */
/* first if there's only one character in the string). The new  */
/* separator characters must be printable, i.e. between (and    */
/* including) 0x20 and 0x7E for a 7-bit ASCII character set.    */
/*--------------------------------------------------------------*/

Var_T *f_time( Var_T *v )
{
	time_t tp;
	char ts[ 100 ];
	char sep[ 2 ] = { ':', ':' };
	char *sp;
	char *str = NULL;
	size_t i;


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

	time( &tp );
	if ( strftime( ts, 100, "%H:%M:%S", localtime( &tp ) ) == 0 )
	{
		print( SEVERE, "Returning invalid time string.\n" );
		strcat( ts, "(Unknown time)" );
	}

	for ( i = 0, sp = ts; i < 2; i++ )
	{
		sp = strchr( sp, ':' );
		*sp = sep[ i ];
	}

	return vars_push( STR_VAR, ts );
}


/*----------------------------------------------------------*/
/* Returns a floating point value with the time difference  */
/* since the last call of the function (in us resolution).  */
/* The function should be called automatically at the start */
/* of an experiment so that the user gets the time since    */
/* the start of the experiment when she calls this function */
/* for the very first time.                                 */
/*----------------------------------------------------------*/

Var_T *f_dtime( UNUSED_ARG Var_T *v )
{
	double new_time;
	static double old_time = 0.0;
	double diff_time;


	new_time = experiment_time( );
	diff_time = new_time - old_time;
	old_time = new_time;
	return vars_push( FLOAT_VAR, diff_time );
}


/*-------------------------------------------------------------------------*/
/* When called without an argument the function returns a string with the  */
/* current date in a form like "Sat Jun 17, 2000". Alternatively, it may   */
/* be called with a format string acceptable to the strftime(3) function   */
/* as the only argument to force the function to return a string with the  */
/* date in a user specified way (with a maximum length of 255 characters). */
/*-------------------------------------------------------------------------*/

#define DEFAULT_DATE_FORMAT          "%a %b %d, %Y"
#define DATE_FLAGS                   "aAbBcCdDeFGghHIjklmMnPrRsStTuUVWxXyYzZ+%"
#define DATE_FLAGS_WITH_E_MODIFIER   "cCxXyY"
#define DATE_FLAGS_WITH_O_MODIFIER   "deHImMSuUVwWy"

Var_T *f_date( Var_T *v )
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


/*---------------------------------------------*/
/* Function returns the dimension of an array. */
/*---------------------------------------------*/

Var_T *f_dim( Var_T *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );
	return vars_push( INT_VAR, ( long ) v->dim );
}


/*---------------------------------------------------------------*/
/* Function returns the length of an array or the number of rows */
/* of a matrix (for simple variables 1 is returned). For arrays  */
/* or matrices 0 gets returned when no elements exist yet.       */
/*---------------------------------------------------------------*/

Var_T *f_size( Var_T *v )
{
	Var_T *new_var = NULL;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR : case FLOAT_VAR :
			print( WARN, "Argument is a number.\n" );
			new_var = vars_push( INT_VAR, 1L );
			break;

		case INT_ARR : case FLOAT_ARR : case INT_REF : case FLOAT_REF :
			new_var = vars_push( INT_VAR, ( long ) v->len );
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*----------------------------------------------------------------------*/
/* Calculates the mean of the elements of an one dimensional array.     */
/* If only an array (or a pointer to an array is passed to the function */
/* the mean of all array elements is calculated. If there's a second    */
/* argument it's taken to be an index into the array at which the       */
/* calculation of the mean starts. If there's a third argument it has   */
/* be the number of elements to be included into the mean.              */
/*----------------------------------------------------------------------*/

Var_T *f_mean( Var_T *v )
{
	ssize_t i;
	long start;
	ssize_t len;
	double sum = 0.0;
	long count = 0;
	void *gp;


	if ( v == NULL )
	{
		print( FATAL, "Missing argument(s).\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	start = 0;
	len = v->len;

	if ( ! ( v->type & ( INT_VAR | FLOAT_VAR ) ) && v->next != NULL )
	{
		start = get_strict_long( v->next, "start index in array" )
				- ARRAY_OFFSET;

		if ( start < 0 || start >= v->len )
		{
			print( FATAL, "Invalid start index (%ld).\n", start );
			THROW( EXCEPTION );
		}

		if ( v->next->next != NULL )
		{
			len = get_strict_long( v->next->next, "array slice length" );

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
			fsc2_assert( 1 == 0 );
	}

	if ( count == 0 )
	{
		print( FATAL, "Number of array or matrix elements is 0.\n" );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, sum / ( double ) count );
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

Var_T *f_rms( Var_T *v )
{
	ssize_t i;
	long start;
	ssize_t len;
	double sum = 0.0;
	long count = 0;
	void *gp;


	if ( v == NULL )
	{
		print( FATAL, "Missing argument(s).\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF );

	start = 0;
	len = v->len;

	if ( len == 0 )
	{
		print( FATAL, "Length of array isn't know yet.\n" );
		THROW( EXCEPTION );
	}

	if ( v->next != NULL )
	{
		start = get_strict_long( v->next, "start index in array" )
				- ARRAY_OFFSET;

		if ( start < 0 || start >= v->len )
		{
			print( FATAL, "Invalid start index (%ld).\n", start );
			THROW( EXCEPTION );
		}

		if ( v->next->next != NULL )
		{
			len = get_strict_long( v->next->next, "array slice length" );

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
			fsc2_assert( 1 == 0 );
	}

	return vars_push( FLOAT_VAR, sqrt( sum ) / ( double ) count );
}


/*----------------------------------------------*/
/* Function for creating a slice of an 1D array */
/* or a submatrix of a more-dimensional matrix  */
/*----------------------------------------------*/

Var_T *f_slice( Var_T *v )
{
	long start;
	ssize_t len;
	Var_T *new_var = NULL;
	ssize_t old_len;
	Var_T **old_vptr;


	vars_check( v, INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF );

	start = 0;
	len = v->len;

	if ( len == 0 )
	{
		print( FATAL, "Length of array isn't know yet.\n" );
		THROW( EXCEPTION );
	}

	if ( v->next != NULL )
	{
		start = get_long( v->next, "array index" ) - ARRAY_OFFSET;

		if ( start < 0 || start >= v->len )
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

	switch ( v->type )
	{
		case INT_ARR :
			new_var = vars_push( INT_ARR, v->val.lpnt + start, len );
			break;

		case FLOAT_ARR :
			new_var = vars_push( FLOAT_ARR, v->val.dpnt + start, len );
			break;

		case INT_REF : case FLOAT_REF :
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
				RETHROW( );
			}

			v->val.vptr = old_vptr;
			v->len      = old_len;
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var_T *f_square( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;
	double lmax, dmax;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			if ( labs( v->val.lval ) >= sqrt( ( double ) LONG_MAX ) )
				print( SEVERE, "Result overflow.\n" );
			new_var = vars_push( INT_VAR, v->val.lval * v->val.lval );
			break;

		case FLOAT_VAR :
			if ( fabs( v->val.dval ) > sqrt( HUGE_VAL ) )
				print( SEVERE, "Result overflow.\n" );
			new_var = vars_push( FLOAT_VAR, v->val.dval * v->val.dval );
			break;

		case INT_ARR :
			new_var = vars_make( INT_ARR, v );
			lmax = sqrt( ( double ) LONG_MAX );
			for ( lsrc = v->val.lpnt, ldest = new_var->val.lpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
			{
				if ( labs( *lsrc ) > lmax )
					print( SEVERE, "Result overflow.\n" );
				*ldest = *lsrc * *lsrc;
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			dmax = sqrt( HUGE_VAL );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
			{
				if ( fabs( *dsrc ) > dmax )
					print( SEVERE, "Result overflow.\n" );
				*ddest = *dsrc * *dsrc;
			}
			break;

		case INT_REF :
			new_var = vars_make( INT_REF, v );
			for ( i = 0; i < v->len; i++ )
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
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_square( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-------------------------------------------------------------*/
/* Function for converting magnetic fields from Gauss to Tesla */
/*-------------------------------------------------------------*/

Var_T *f_G2T( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR, ( double ) v->val.lval * 1.0e-4 );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.dval * 1.0e-4 );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
				*ddest = ( double ) *lsrc * 1.0e-4;
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
				*ddest = *dsrc * 1.0e-4;
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_G2T( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-------------------------------------------------------------*/
/* Function for converting magnetic fields from Tesla to Gauss */
/*-------------------------------------------------------------*/

Var_T *f_T2G( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR, ( double ) v->val.lval * 1.0e4 );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.dval * 1.0e4 );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
				*ddest = ( double ) *lsrc * 1.0e4;
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
				*ddest = *dsrc * 1.0e4;
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_T2G( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-------------------------------------------------------------------*/
/* Function for converting temperatures from degree Celsius to Kevin */
/*-------------------------------------------------------------------*/

Var_T *f_C2K( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR,
								 ( double ) v->val.lval + C2K_OFFSET );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.dval + C2K_OFFSET );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
				*ddest = ( double ) *lsrc + C2K_OFFSET;
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
				*ddest = *dsrc + C2K_OFFSET;
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_C2K( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-------------------------------------------------------------------*/
/* Function for converting temperatures from Kevin to degree Celsius */
/*-------------------------------------------------------------------*/

Var_T *f_K2C( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR,
								 ( double ) v->val.lval - C2K_OFFSET );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.dval - C2K_OFFSET );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
				*ddest = ( double ) *lsrc - C2K_OFFSET;
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
				*ddest = *dsrc - C2K_OFFSET;
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_K2C( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------------------------*/
/* Function for converting values in degrees to radians */
/*------------------------------------------------------*/

Var_T *f_D2R( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR,
								 ( double ) v->val.lval * D2R_FACTOR );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.dval * D2R_FACTOR );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
				*ddest = ( double ) *lsrc * D2R_FACTOR;
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
				*ddest = *dsrc * D2R_FACTOR;
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_D2R( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------------------------*/
/* Function for converting values in radians to degrees */
/*------------------------------------------------------*/

Var_T *f_R2D( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR,
								 ( double ) v->val.lval * R2D_FACTOR );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.dval * R2D_FACTOR );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
				*ddest = ( double ) *lsrc * R2D_FACTOR;
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
				*ddest = *dsrc * R2D_FACTOR;
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_R2D( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-------------------------------------------------------------------------*/
/* Function for converting wave lengths (in m) to wavenumbers (i.e. cm^-1) */
/*-------------------------------------------------------------------------*/

Var_T *f_WL2WN( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			if ( v->val.lval == 0 )
			{
				print( FATAL, "Can't convert 0 m to a wave number.\n" );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, 0.01 / ( double ) v->val.lval );
			break;

		case FLOAT_VAR :
			if ( v->val.dval == 0.0 )
			{
				print( FATAL, "Can't convert 0 m to a wave number.\n" );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, 0.01 / v->val.dval );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
			{
				if ( *lsrc == 0 )
				{
					print( FATAL, "Can't convert 0 m to a wave number.\n" );
					THROW( EXCEPTION );
				}
				*ddest = 0.01 / ( double ) *lsrc;
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
			{
				if ( *dsrc == 0.0 )
				{
					print( FATAL, "Can't convert 0 m to a wave number.\n" );
					THROW( EXCEPTION );
				}
				*ddest = 0.01 / *dsrc;
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_WL2WN( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-------------------------------------------------------------------------*/
/* Function for converting wavenumbers (i.e. cm^-1) to wave lengths (in m) */
/*-------------------------------------------------------------------------*/

Var_T *f_WN2WL( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			if ( v->val.lval == 0 )
			{
				print( FATAL, "Can't convert 0 cm^-1 to a wavelength.\n" );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, 0.01 / ( double ) v->val.lval );
			break;

		case FLOAT_VAR :
			if ( v->val.dval == 0.0 )
			{
				print( FATAL, "Can't convert 0 cm^-1 to a wavelength.\n" );
				THROW( EXCEPTION );
			}
			new_var = vars_push( FLOAT_VAR, 0.01 / v->val.dval );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
			{
				if ( *lsrc == 0 )
				{
					print( FATAL, "Can't convert 0 cm^-1 to a wavelength.\n" );
					THROW( EXCEPTION );
				}
				*ddest = 0.01 / ( double ) *lsrc;
			}
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
			{
				if ( *dsrc == 0.0 )
				{
					print( FATAL, "Can't convert 0 cm^-1 to a wavelength.\n" );
					THROW( EXCEPTION );
				}
				*ddest = 0.01 / *dsrc;
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_WN2WL( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-------------------------------------------------------------------------*/
/* Function for converting frequencies (in Hz) to wavenumbers (i.e. cm^-1) */
/*-------------------------------------------------------------------------*/

Var_T *f_F2WN( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.lval / WN2F_FACTOR );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.dval / WN2F_FACTOR );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
				*ddest = *lsrc / WN2F_FACTOR;
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
				*ddest = *dsrc / WN2F_FACTOR;
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_F2WN( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*-------------------------------------------------------------------------*/
/* Function for converting wavenumbers (i.e. cm^-1) to frequencies (in Hz) */
/*-------------------------------------------------------------------------*/

Var_T *f_WN2F( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest;
	double *dsrc, *ddest;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( FLOAT_VAR, WN2F_FACTOR * v->val.lval );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, WN2F_FACTOR * v->val.dval );
			break;

		case INT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( lsrc = v->val.lpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, lsrc++, ldest++ )
				*ddest = WN2F_FACTOR * *lsrc;
			break;

		case FLOAT_ARR :
			new_var = vars_make( FLOAT_ARR, v );
			for ( dsrc = v->val.dpnt, ddest = new_var->val.dpnt, i = 0;
				  i < v->len; i++, dsrc++, ddest++ )
				*ddest = WN2F_FACTOR *  *dsrc;
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_WN2F( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var_T *f_islice( Var_T *v )
{
	long size;
	Var_T *ret;
	Var_T *cv;
	int dim;


	size = get_long( v, "array size" );

	if ( size <= 0 )
	{
		if ( v->type == INT_VAR )
			print( FATAL, "Negative or zero value (%ld) used as array size.\n",
				   v->val.lval );
		else
			print( FATAL, "Negative or zero value (%f) used as array size.\n",
				   v->val.dval );
		THROW( EXCEPTION );
	}

	if ( v->next== NULL )
		ret = vars_push( INT_ARR, NULL, ( ssize_t ) size );
	else
	{
		for ( dim = 1, cv = v->next; cv != NULL; cv = cv->next )
			dim++;

		/* Create a new reference on the stack and move it from the end
		   of the stack to the position just before the first variable
		   with the sizes */

		ret = vars_push( INT_REF, NULL );

		v->prev->next = ret;
		ret->next = v;
		v->prev = ret;
		for ( cv = v; cv->next != ret; cv = cv->next )
			/* empty */ ;
		cv->next = NULL;

		vars_arr_create( ret, v, dim, SET );

		while ( ( v = vars_pop( v ) ) != NULL )
			/* empty */ ;
	}
		
	return ret;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var_T *f_fslice( Var_T *v )
{
	long size;
	Var_T *ret;
	Var_T *cv;
	int dim;


	size = get_long( v, "array size" );

	if ( size <= 0 )
	{
		if ( v->type == INT_VAR )
			print( FATAL, "Negative or zero value (%ld) used as array size.\n",
				   v->val.lval );
		else
			print( FATAL, "Negative or zero value (%f) used as array size.\n",
				   v->val.dval );
		THROW( EXCEPTION );
	}

	if ( v->next== NULL )
		ret = vars_push( FLOAT_ARR, NULL, ( ssize_t ) size );
	else
	{
		for ( dim = 1, cv = v->next; cv != NULL; cv = cv->next )
			dim++;

		/* Create a new reference on the stack and move it from the end
		   of the stack to the position just before the first variable
		   with the sizes */

		ret = vars_push( FLOAT_REF, NULL );

		v->prev->next = ret;
		ret->next = v;
		v->prev = ret;
		for ( cv = v; cv->next != ret; cv = cv->next )
			/* empty */ ;
		cv->next = NULL;

		vars_arr_create( ret, v, dim, SET );

		while ( ( v = vars_pop( v ) ) != NULL )
			/* empty */ ;
	}
		
	return ret;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var_T *f_lspace( Var_T *v )
{
	Var_T *nv;
	ssize_t i;
	double start;
	double end;
	double incr;
	long num;
	double *d, cv;


	start = get_double( v, NULL );
	end = get_double( v = vars_pop( v ), NULL );
	num = get_long( v = vars_pop( v ), "number of points" );

	if ( num < 2 )
	{
		print( FATAL, "Invalid number of points.\n" );
		THROW( EXCEPTION );
	}

	incr = ( end - start ) / ( num - 1 );

	nv = vars_push( FLOAT_ARR, NULL, num );
	for ( d = nv->val.dpnt, cv = start, i = 0; i < num;
		  d++, cv += incr, i++ )
		*d = cv;

	return nv;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var_T *f_reverse( Var_T *v )
{
	Var_T *new_var = NULL;
	ssize_t i;
	long *lsrc, *ldest, ltemp;
	double *dsrc, *ddest, dtemp;


	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	switch ( v->type )
	{
		case INT_VAR :
			new_var = vars_push( INT_VAR, v->val.lval );
			break;

		case FLOAT_VAR :
			new_var = vars_push( FLOAT_VAR, v->val.dval );
			break;

		case INT_ARR :
			new_var = vars_push( INT_ARR, v->val.lpnt, v->len );
			for ( lsrc = new_var->val.lpnt, ldest = lsrc + new_var->len - 1;
				  lsrc < ldest; lsrc++, ldest-- )
			{
				ltemp = *lsrc;
				*lsrc = *ldest;
				*ldest = ltemp;
			}
			break;

		case FLOAT_ARR :
			new_var = vars_push( FLOAT_ARR, v->val.dpnt, v->len );
			for ( dsrc = new_var->val.dpnt, ddest = dsrc + new_var->len - 1;
				  dsrc < ddest; dsrc++, ddest-- )
			{
				dtemp = *dsrc;
				*dsrc = *ddest;
				*ddest = dtemp;
			}
			break;

		case INT_REF : case FLOAT_REF :
			new_var = vars_make( FLOAT_REF, v );
			for ( i = 0; i < v->len; i++ )
				if ( v->val.vptr[ i ] == NULL )
					new_var->val.vptr[ i ] = NULL;
				else
				{
					new_var->val.vptr[ i ] = f_reverse( v->val.vptr[ i ] );
					new_var->val.vptr[ i ]->from = new_var;
				}
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	return new_var;
}


/*--------------------------------------------*/
/* Function is used in the calculation of the */
/* asinh(), acosh() and atanh() functions.    */
/*--------------------------------------------*/

static double datanh( double arg )
{
	int sgn;

	sgn = arg >= 0 ? 1 : -1;
	arg = fabs( arg );

	if ( 1.0 - arg < DBL_EPSILON )
	{
		print( SEVERE, "Argument overflow.\n" );
		arg = 1.0 - DBL_EPSILON;
	}

	return sgn * 0.5 * log( ( 1.0 + arg ) / ( 1.0 - arg ) );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
