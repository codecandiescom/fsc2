/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


/*----------------------------------------------------------------------*/
/* This function is called by modules to determine the current state of */
/* the global variable FSC2_MODE without them being able to change it.  */
/*----------------------------------------------------------------------*/

inline int get_mode( void )
{
	return FSC2_MODE;
}


/*--------------------------------------------------------------*/
/* This function might be called to check if there are any more */
/* variables on the variable stack, representing superfluous    */
/* arguments to a function.                                     */
/*--------------------------------------------------------------*/

inline void too_many_arguments( Var *v, const char *device )
{
	if ( v == NULL || ( v = vars_pop( v ) ) == NULL )
		return;

	if ( device != NULL )
		eprint( WARN, SET, "%s: Superfluous argument%s in call of function "
				"%s().\n", device, v->next != NULL ? "s" : "", Cur_Func );
	while ( ( v = vars_pop( v ) ) != NULL )
		;
}


/*------------------------------------------------------------*/
/* This function just tells the user that a function can't be */
/* called ithout an argument (unless we are in the EXPERIMENT */
/* section) and then trows an exception.                      */
/*------------------------------------------------------------*/

inline void no_query_possible( const char *device )
{
	if (  device != NULL )
		eprint( FATAL, SET, "%s: %s() can be used for queries in the "
				"EXPERIMENT section only.\n", device, Cur_Func );
	THROW( EXCEPTION );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

inline long get_long( Var *v, const char *snippet, const char *device )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == FLOAT_VAR && snippet != NULL && device != NULL )
		eprint( WARN, SET, "%s: Floating point number used as %s in %s().\n",
				device, snippet, Cur_Func );

	return v->type == INT_VAR ? v->val.lval : ( long ) v->val.dval;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

inline double get_double( Var *v, const char *snippet, const char *device )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR && snippet != NULL && device != NULL )
		eprint( WARN, SET, "%s: Integer number used as %s in %s().\n",
				device, snippet, Cur_Func );

	return v->type == INT_VAR ? v->val.lval : v->val.dval;
}


/*----------------------------------------------------------------------*/
/* This function can be called to get the value of a variable that must */
/* be an integer variable. If it isn't an error message is printed and  */
/* only when the program is interpreting the EXPERIMENT section and the */
/* variable is a floating point variable no exception is thrown but its */
/* value is converted to an int and this value returned.                */
/*----------------------------------------------------------------------*/

inline long get_strict_long( Var *v, const char *snippet, const char *device )
{
	if ( v->type == FLOAT_VAR )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			if ( snippet != NULL  && device != NULL )
				eprint( SEVERE, SET, "%s: Floating point number used as %s "
						"in %s(), trying to continue!\n",
						device, snippet, Cur_Func );
			vars_check( v, FLOAT_VAR );
			return lrnd( v->val.dval );
		}

		if ( snippet != NULL  && device != NULL )
			eprint( FATAL, SET, "%s: Floating point number can't be used as "
					"%s in %s().\n", device, snippet, Cur_Func );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR );
	return v->val.lval;
}


/*---------------------------------------------------------------------------*/
/* This function can be called when a variable that should contain a boolean */
/* is expected, where boolean means either an integer (where 0 corresponds   */
/* to FALSE and a non-zero value means TRUE) or a string, either "ON" or     */
/* "OFF" (capitalization doesn't matter). If the value is a floating point   */
/* it is accepted (after printing an error message) during the EXPERIMENT    */
/* only, otherwise an exception is thrown. When it's a string and doesn't    */
/* match either "ON" or "OFF" and exception is thrown in every case.         */
/*---------------------------------------------------------------------------*/

inline bool get_boolean( Var *v, const char *device )
{
	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == FLOAT_VAR )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			if ( device != NULL )
				eprint( FATAL, SET, "%s: Floating point number found where "
						"boolean type value was expected in %s().\n", device,
						Cur_Func );
			THROW( EXCEPTION );
		}

		if ( device != NULL )
			eprint( SEVERE, SET, "%s: Floating point number found where "
					"boolean type value was expected in %s().\n", device,
					Cur_Func );
		return v->val.dval != 0.0;
	}
	else if ( v->type == STR_VAR )
	{
		if ( ! strcasecmp( v->val.sptr, "OFF" ) )
			return UNSET;

		if ( ! strcasecmp( v->val.sptr, "ON" ) )
			return SET;

		if ( device != NULL )
			eprint( FATAL, SET, "%s: Invalid boolean argument (\"%s\") in "
					"%s().\n", device, v->val.sptr, Cur_Func );
		THROW( EXCEPTION );
	}

	return v->val.lval != 0;
}
