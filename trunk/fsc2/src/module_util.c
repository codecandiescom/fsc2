/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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


/*-----------------------------------------------------*/
/* This function is called by modules to determine the */
/* current mode without them being able to change it.  */
/*-----------------------------------------------------*/

inline int get_mode( void )
{
	return Internals.mode;
}


/*-------------------------------------------------------------*/
/* This function is called by modules to determine if this is  */
/* just a check run without graphical interface or a check run */
/*-------------------------------------------------------------*/

inline int get_check_state( void )
{
	return Internals.cmdline_flags & DO_CHECK || Internals.just_testing;
}


/*------------------------------------------------------*/
/* Function tests if the user has hit the "Stop" button */
/* and throws an USER_BREAK_EXCEPTION in this case.     */
/*------------------------------------------------------*/

inline void stop_on_user_request( void )
{
	if ( Internals.I_am == PARENT )
		fl_check_only_forms( );
	if ( EDL.do_quit && EDL.react_to_do_quit )
		THROW( USER_BREAK_EXCEPTION );
}


/*--------------------------------------------------------------*/
/* This function might be called to check if there are any more */
/* variables on the variable stack, representing superfluous    */
/* arguments to a function.                                     */
/*--------------------------------------------------------------*/

inline void too_many_arguments( Var *v )
{
	if ( v == NULL || ( v = vars_pop( v ) ) == NULL )
		return;

	print( WARN, "Too many arguments, discarding superfluous arguments.\n",
		   v->next != NULL ? "s" : "" );

	while ( ( v = vars_pop( v ) ) != NULL )
		/* empty */ ;
}


/*------------------------------------------------------------*/
/* This function just tells the user that a function can't be */
/* called ithout an argument (unless we are in the EXPERIMENT */
/* section) and then trows an exception.                      */
/*------------------------------------------------------------*/

inline void no_query_possible( void )
{
	print( FATAL, "Function can be used for queries in the EXPERIMENT "
		   "section only.\n" );
	THROW( EXCEPTION );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

inline long get_long( Var *v, const char *snippet )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == FLOAT_VAR && snippet != NULL )
		print( WARN, "Floating point number used as %s.\n", snippet );

	if ( v->type == FLOAT_VAR && 
		 ( v->val.dval > LONG_MAX || v->val.dval < LONG_MIN ) )
		print( SEVERE, "Integer argument overflow.\n" );

	return v->type == INT_VAR ? v->val.lval : lrnd( v->val.dval );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

inline double get_double( Var *v, const char *snippet )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR && snippet != NULL )
		print( WARN, "Integer number used as %s.\n", snippet );

	return v->type == INT_VAR ? v->val.lval : v->val.dval;
}


/*----------------------------------------------------------------------*/
/* This function can be called to get the value of a variable that must */
/* be an integer variable. If it isn't an error message is printed and  */
/* only when the program is interpreting the EXPERIMENT section and the */
/* variable is a floating point variable no exception is thrown but its */
/* value is converted to an int and this value returned.                */
/*----------------------------------------------------------------------*/

inline long get_strict_long( Var *v, const char *snippet )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == FLOAT_VAR )
	{
		if ( Internals.mode == EXPERIMENT )
		{
			if ( snippet != NULL )
				print( SEVERE, "Floating point number used as %s, "
					   "trying to continue!\n", snippet );

			if ( v->val.dval > LONG_MAX || v->val.dval < LONG_MIN )
				print( SEVERE, "Integer argument overflow.\n" );

			return lrnd( v->val.dval );
		}

		if ( snippet != NULL  )
			print( FATAL, "Floating point number can't be used as %s.\n",
				   snippet );
		THROW( EXCEPTION );
	}

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

inline bool get_boolean( Var *v )
{
	const char *alt[ 2 ] = { "OFF", "ON" };
	int res;


	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == FLOAT_VAR )
	{
		if ( Internals.mode != EXPERIMENT )
		{
			print( FATAL, "Floating point number found where boolean value "
				   "was expected.\n" );
			THROW( EXCEPTION );
		}

		print( SEVERE, "Floating point number found where boolean value "
			   "was expected, trying to continue.\n" );
		return v->val.dval != 0.0;
	}
	else if ( v->type == STR_VAR )
	{
		if ( ( res = is_in( v->val.sptr, alt, 2 ) ) >= 0 )
			return res != 0 ? SET : UNSET;

		print( FATAL, "Invalid boolean argument (\"%s\").\n",  v->val.sptr );
		THROW( EXCEPTION );
	}

	return v->val.lval != 0;
}


/*-------------------------------------------------------------------*/
/* This function can be used by modules to get a very rough estimate */
/* of the time since the start of the test run. This is useful for   */
/* example in cases where an automatic sweep is done and during the  */
/* test run an estimate for the swept value has to be returned that  */
/* is not completely bogus. But take care, the value returned by the */
/* function might easily be off by an order of magnitude.            */
/* The method to estimate the time is as simple as possible: To the  */
/* real time used for interpretating the EDL program is added the    */
/* time spend in calls of the EDL function wait() (see f_wait() in   */
/* func_util.c) plus the value MODULE_CALL ESTIMATE (defined in the  */
/* header file) for each module function call.                       */
/* During the experiment the fucntion returns the correct time since */
/* the start of the experiment.                                      */
/*-------------------------------------------------------------------*/

double experiment_time( void )
{
	struct timeval t_new;
	static struct timeval t_old = { 0, 0 };
	long dsec, dusec;


	gettimeofday( &t_new, NULL );

	dsec = t_new.tv_sec - t_old.tv_sec;
	dusec = t_new.tv_usec - t_old.tv_usec;

	t_old.tv_sec = t_new.tv_sec;
	t_old.tv_usec = t_new.tv_usec;

	return EDL.experiment_time += ( double ) dsec + 1.e-6 * ( double ) dusec;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
