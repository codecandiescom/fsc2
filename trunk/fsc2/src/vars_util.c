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


#include "fsc2.h"


static Var *vars_str_comp( int comp_type, Var *v1, Var *v2 );


/*---------------------------------------------------------------*/
/* vars_negate() negates the value of a variable, an 1D array or */
/* a complete matrix.                                            */
/*---------------------------------------------------------------*/

Var *vars_negate( Var *v )
{
	Var *new_var;
	ssize_t i;


	/* Make sure that 'v' exists and has RHS type */

	vars_check( v, RHS_TYPES );

	switch( v->type )
	{
		case INT_VAR :
			v->val.lval = - v->val.lval;
			return v;

		case FLOAT_VAR :
			v->val.dval = - v->val.dval;
			return v;

		case INT_ARR :
			if ( v->flags & IS_TEMP )
				 new_var = v;
			else
				new_var = vars_push( v->type, v->val.lpnt, v->len );

			for ( i = 0; i < new_var->len; i++ )
				new_var->val.lpnt[ i ] = - new_var->val.lpnt[ i ];

			break;

		case FLOAT_ARR :
			if ( v->flags & IS_TEMP )
				 new_var = v;
			else
				new_var = vars_push( v->type, v->val.dpnt, v->len );

			for ( i = 0; i < new_var->len; i++ )
				new_var->val.dpnt[ i ] = - new_var->val.dpnt[ i ];

			break;

		case INT_REF : case FLOAT_REF :
			if ( v->flags & IS_TEMP )
				new_var = v;
			else
				new_var = vars_push( v->type, v );

			for ( i = 0; i < new_var->len; i++ )
				vars_pop( vars_negate( new_var->val.vptr[ i ] ) );	

			break;

#ifndef NDEBUG
		default :
			eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
#endif
	}

	if ( new_var != v )
		vars_pop( v );

	return new_var;
}


/*--------------------------------------------------------------------------*/
/* vars_comp() is used for comparing the values of two variables. There are */
/* three types of comparison - it can be tested if two variables are equal, */
/* if the first one is less than the second or if the first is less or      */
/* equal than the second variable (tests for greater or greater or equal    */
/* can be done simply by switching the arguments).                          */
/* In comparisons between floating point numbers not only the numbers are   */
/* compared but, in order to reduce problems due to rounding errors, also   */
/* the numbers when the last significant bit is changed (if there's a       */
/* function in libc that allow us to do this...).                           */
/* ->                                                                       */
/*    * type of comparison (COMP_EQUAL, COMP_UNEQUAL, COMP_LESS,            */
/*      COMP_LESS_EQUAL, COMP_AND, COMP_OR or COMP_XOR)                     */
/*    * pointers to the two variables                                       */
/* <-                                                                       */
/*    * integer variable with value of 1 (true) or 0 (false) depending on   */
/*      the result of the comparison                                        */
/*--------------------------------------------------------------------------*/

Var *vars_comp( int comp_type, Var *v1, Var *v2 )
{
	Var *new_var = NULL;


	/* If both variables are strings we can also do some kind of comparisons */

	if ( v1 && v1->type == STR_VAR && v2 && v2->type == STR_VAR )
		return vars_str_comp( comp_type, v1, v2 );

	/* Make sure that 'v1' and 'v2' exist, are integers or float values
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	switch ( comp_type )
	{
#if ! defined IS_STILL_LIBC1     /* libc2 *has* nextafter() */
		case COMP_EQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT == v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) == VALUE( v2 ) ||
									 nextafter( VALUE( v1 ), VALUE( v2 ) )
									 == VALUE( v2 ) );
			break;

		case COMP_UNEQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT != v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) != VALUE( v2 ) &&
									 nextafter( VALUE( v1 ), VALUE( v2 ) )
									 != VALUE( v2 ) );
			break;

		case COMP_LESS :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT < v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) < VALUE( v2 ) &&
									 nextafter( VALUE( v1 ), VALUE( v2 ) )
									 < VALUE( v2 ) );
			break;

		case COMP_LESS_EQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT <= v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) <= VALUE( v2 ) ||
									 nextafter( VALUE( v1 ), VALUE( v2 ) )
									 <= VALUE( v2 ) );
			break;
#else
		case COMP_EQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT == v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) == VALUE( v2 ) );
			break;

		case COMP_UNEQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT != v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) != VALUE( v2 ) );

			break;

		case COMP_LESS :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT < v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) < VALUE( v2 ) );
			break;

		case COMP_LESS_EQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT <= v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) <= VALUE( v2 ) );
			break;
#endif

		case COMP_AND :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT != 0 && v2->INT != 0 );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) != 0.0 &&
									          VALUE( v2 ) != 0.0 );
			break;

		case COMP_OR :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT != 0 || v2->INT != 0 );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) != 0.0 ||
									          VALUE( v2 ) != 0.0 );
			break;

		case COMP_XOR :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR,
									 ( v1->INT != 0 && v2->INT == 0 ) ||
									 ( v1->INT == 0 && v2->INT != 0 ) );
			else
				new_var = vars_push( INT_VAR,
								( VALUE( v1 ) != 0.0 && VALUE( v2 ) == 0.0 ) ||
								( VALUE( v1 ) == 0.0 && VALUE( v2 ) != 0.0 ) );
			break;

		default:               /* this should never happen... */
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static Var *vars_str_comp( int comp_type, Var *v1, Var *v2 )
{
	Var *new_var = NULL;


	switch ( comp_type )
	{
		case COMP_EQUAL :
			vars_push( INT_VAR, strcmp( v1->val.sptr, v2->val.sptr ) ? 0 : 1 );
			break;
			
		case COMP_UNEQUAL :
			vars_push( INT_VAR, strcmp( v1->val.sptr, v2->val.sptr ) ? 1 : 0 );
			break;

		case COMP_LESS :
			vars_push( INT_VAR,
					   strcmp( v1->val.sptr, v2->val.sptr ) < 0 ? 1 : 0 );
			break;

		case COMP_LESS_EQUAL :
			vars_push( INT_VAR,
					   strcmp( v1->val.sptr, v2->val.sptr ) <= 0 ? 1 : 0 );
			break;

		case COMP_AND :
		case COMP_OR  :
		case COMP_XOR :
			print( FATAL, "Logical and, or and xor operators can't be used "
				   "with string variables.\n" );
			THROW( EXCEPTION );

		default:               /* this should never happen... */
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*-----------------------------------------------------------------------*/
/* vars_lnegate() does a logical negate of an integer or float variable, */
/* i.e. if the variables value is zero a value of 1 is returned in a new */
/* variable, otherwise 0.                                                */
/*-----------------------------------------------------------------------*/

Var *vars_lnegate( Var *v )
{
	Var *new_var;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( ( v->type == INT_VAR && v->INT == 0 ) ||
		 ( v->type == FLOAT_VAR && v->FLOAT == 0.0 ) )
		new_var = vars_push( INT_VAR, 1 );
	else
		new_var = vars_push( INT_VAR, 0 );

	vars_pop( v );

	return new_var;
}


