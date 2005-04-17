/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
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

static const char *handle_input( const char *content, const char *label );


/*--------------------------------------------------------*
 * Shows a message box with an "OK" button - use embedded
 * newline characters to get multi-line messages.
 *--------------------------------------------------------*/

void show_message( const char *str )
{
	if ( Fsc2_Internals.I_am == PARENT )
	{
		if ( Fsc2_Internals.cmdline_flags & DO_CHECK ||
			 Fsc2_Internals.cmdline_flags & BATCH_MODE )
			fprintf( stdout, "%s\n", str );
		else
		{
			switch_off_special_cursors( );
			Fsc2_Internals.state = STATE_WAITING;
			fl_show_messages( str );
			Fsc2_Internals.state = STATE_RUNNING;
		}
	}
	else
	{
		if ( ! writer( C_SHOW_MESSAGE, str ) || ! reader( NULL ) )
			THROW( EXCEPTION );
	}
}


/*-------------------------------------------------------*
 * Shows an alert box with an "OK" button - use embedded
 * newline characters to get multi-line messages.
 *-------------------------------------------------------*/

void show_alert( const char *str )
{
	char *strc, *strs[ 3 ];
	int i;


	if ( Fsc2_Internals.I_am == PARENT )
	{
		strc = T_strdup( str );
		strs[ 0 ] = strc;
		if ( ( strs[ 1 ] = strchr( strs[ 0 ], '\n' ) ) != NULL )
		{
			*strs[ 1 ]++ = '\0';
			if ( ( strs[ 2 ] = strchr( strs[ 1 ], '\n' ) ) != NULL )
				*strs[ 2 ]++ = '\0';
			else
				strs[ 2 ] = NULL;
		}
		else
			strs[ 1 ] = strs[ 2 ] = NULL;

		if ( Fsc2_Internals.cmdline_flags & DO_CHECK ||
			 Fsc2_Internals.cmdline_flags & BATCH_MODE )
		{
			for ( i = 0; i < 3 && strs[ i ] != NULL; i++ )
				fprintf( stdout, "%s", strs[ i ] );
			fprintf( stdout, "\n" );
		}
		else
		{
			switch_off_special_cursors( );
			Fsc2_Internals.state = STATE_WAITING;
			fl_show_alert( strs[ 0 ], strs[ 1 ], strs[ 2 ], 1 );
			Fsc2_Internals.state = STATE_RUNNING;
		}

		T_free( strc );
	}
	else
	{
		if ( ! writer( C_SHOW_ALERT, str ) || ! reader( NULL ) )
			THROW( EXCEPTION );
	}
}


/*------------------------------------------------------------------------*
 * Shows a choice box with up to three buttons, returns the number of the
 * pressed button (in the range from 1 - 3).
 * ->
 *    1. message text, use embedded newlines for multi-line messages
 *    2. number of buttons to show
 *    3. texts for the three buttons
 *    4. number of button to be used as default button (range 1 - 3)
 *------------------------------------------------------------------------*/

int show_choices( const char *text, int numb, const char *b1, const char *b2,
				  const char *b3, int def, bool is_batch )
{
	int ret;

	if ( Fsc2_Internals.I_am == PARENT )
	{
		if ( Fsc2_Internals.cmdline_flags & DO_CHECK ||
			 ( Fsc2_Internals.cmdline_flags & BATCH_MODE && is_batch ) )
		{
			fprintf( stdout, "%s\n%s %s %s\n", text, b1, b2, b3 );
			return def != 0 ? def : 1;
		}
		else
		{
			switch_off_special_cursors( );
			Fsc2_Internals.state = STATE_WAITING;
			ret = fl_show_choices( text, numb, b1, b2, b3, def );
			Fsc2_Internals.state = STATE_RUNNING;
			return ret;
		}
	}
	else
	{
		if ( ! writer( C_SHOW_CHOICES, text, numb, b1, b2, b3, def ) ||
			 ! reader( ( void * ) &ret ) )
			THROW( EXCEPTION );
		return ret;
	}
}


/*---------------------------------------------------------------*
 * Shows a file selector box and returns the selected file name.
 * ->
 *    1. short message to be shown
 *    2. name of directory the file might be in
 *    3. pattern for files to be listed (e.g. "*.edl")
 *    4. default file name
 * <-
 * Returns either a static buffer with the file name or NULL.
 *---------------------------------------------------------------*/

const char *show_fselector( const char *message, const char *directory,
							const char *pattern, const char *def )
{
	const char *ret = NULL;


	if ( Fsc2_Internals.I_am == PARENT )
	{
		switch_off_special_cursors( );
		Fsc2_Internals.state = STATE_WAITING;
		ret = fsc2_show_fselector( message, directory, pattern, def );
		Fsc2_Internals.state = STATE_RUNNING;
		return ret;
	}
	else
	{
		if ( ! writer( C_SHOW_FSELECTOR, message, directory, pattern, def ) ||
			 ! reader( ( void * ) &ret ) )
			THROW( EXCEPTION );
		return ret;
	}
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

const char *show_input( const char *content, const char *label )
{
	char *ret = NULL;

	if ( Fsc2_Internals.I_am == PARENT )
	{
		switch_off_special_cursors( );
		return handle_input( content, label );
	}
	else
	{
		if ( ! writer( C_INPUT, content, label ) ||
			 ! reader( ( void * ) &ret ) )
			THROW( EXCEPTION );
		return ret;
	}
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static const char *handle_input( const char *content, const char *label )
{
	if ( label != NULL && label != '\0' )
		fl_set_object_label( GUI.input_form->comm_input, label );
	else
		fl_set_object_label( GUI.input_form->comm_input,
                             "Enter your comment:" );

	fl_set_input( GUI.input_form->comm_input, content );

	fl_show_form( GUI.input_form->input_form,
				  FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
				  "fsc2: Comment editor" );

	Fsc2_Internals.state = STATE_WAITING;

	while ( fl_do_forms( ) != GUI.input_form->comm_done )
		/* empty */ ;

	Fsc2_Internals.state = STATE_RUNNING;

	if ( fl_form_is_visible( GUI.input_form->input_form ) )
		fl_hide_form( GUI.input_form->input_form );

	return fl_get_input( GUI.input_form->comm_input );
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the layout() function.
 *--------------------------------------------------------------*/

bool exp_layout( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		if ( ! writer( C_LAYOUT, len, buffer ) )
		{
			T_free( buffer );
			return FAIL;
		}
		T_free( buffer );
		return reader( NULL );
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		int acc;
		char *pos;
		long type;


		TRY
		{
			/* Get variable with address of function to set the layout */

			func_ptr = func_get( "layout", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &type, pos, sizeof type );        /* get layout type */
			vars_push( INT_VAR, type );
			pos += sizeof type;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( func_ptr ) );
			writer( C_LAYOUT_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_LAYOUT_REPLY, 0L );

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		return OK;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the button_create() function.
 *--------------------------------------------------------------*/

long *exp_bcreate( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_BCREATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		long val;
		int acc;
		long result[ 2 ];
		char *pos;
		long type;


		TRY
		{
			/* Get variable with address of function to create a button */

			func_ptr = func_get( "button_create", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &type, pos, sizeof type );
			vars_push( INT_VAR, type );
			pos += sizeof type;

			memcpy( &val, pos, sizeof val );         /* get colleague ID */
			if ( val >= 0 )
				vars_push( INT_VAR, val );
			pos += sizeof val;

			EDL.Fname = pos;                         /* current file name */
			pos += strlen( pos ) + 1;

			vars_push( STR_VAR, pos );               /* get label string */
			pos += strlen( pos ) + 1;

			if ( *pos != '\0' )                      /* get help text */
				vars_push( STR_VAR, pos );

			/* Call the function */

			ret = func_call( func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_BCREATE_REPLY, sizeof result, result ) )
			THROW( EXCEPTION );

		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the button_delete() function.
 *--------------------------------------------------------------*/

bool exp_bdelete( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		if ( ! writer( C_BDELETE, len, buffer ) )
		{
			T_free( buffer );
			return FAIL;
		}
		T_free( buffer );
		return reader( NULL );
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get variable with address of function to delete a button */

			func_ptr = func_get( "button_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );  /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                        /* current file name */

			/* Call the function */

			vars_pop( func_call( func_ptr ) );
			writer( C_BDELETE_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_BDELETE_REPLY, 0L );

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		return SET;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the button_state() function .
 *--------------------------------------------------------------*/

long *exp_bstate( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_BSTATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		long val;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		char *pos;
		long ID;
		long result[ 2 ];


		TRY
		{
			/* Get variable with address of function to get a button state */

			func_ptr = func_get( "button_state", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			memcpy( &val, pos, sizeof val );         /* get state to be set */
			if ( val >= 0 )
				vars_push( INT_VAR, val );
			pos += sizeof val;

			EDL.Fname = pos;                         /* current file name */

			/* Call the function */

			ret = func_call( func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_BSTATE_REPLY, sizeof result, result ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the button_changed() function .
 *--------------------------------------------------------------*/

long *exp_bchanged( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_BCHANGED, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		char *pos;
		long ID;
		long result[ 2 ];


		TRY
		{
			/* Get variable with address of function to determine a button
			   state change */

			func_ptr = func_get( "button_changed", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                         /* current file name */

			/* Call the function */

			ret = func_call( func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_BCHANGED_REPLY, sizeof result, result ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the slider_create() function .
 *--------------------------------------------------------------*/

long *exp_screate( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_SCREATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		long result[ 2 ];
		char *pos;
		long type;
		double val;
		int i;


		TRY
		{
			/* Get variable with address of function to create a slider */

			func_ptr = func_get( "slider_create", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &type, pos, sizeof type );
			vars_push( INT_VAR, type );
			pos += sizeof type;

			for ( i = 0; i < 3; i++ )
			{
				memcpy( &val, pos, sizeof val );
				vars_push( FLOAT_VAR, val );
				pos += sizeof val;
			}

			EDL.Fname = pos;                          /* current file name */
			pos += strlen( pos ) + 1;

			vars_push( STR_VAR, pos );               /* get label string */
			pos += strlen( pos ) + 1;

			if ( *pos != '\0' )                      /* get help text */
				vars_push( STR_VAR, pos );

			/* Call the function */

			ret = func_call( func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_SCREATE_REPLY, sizeof result, result ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the slider_delete() function.
 *--------------------------------------------------------------*/

bool exp_sdelete( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		if ( ! writer( C_SDELETE, len, buffer ) )
		{
			T_free( buffer );
			return FAIL;
		}
		T_free( buffer );
		return reader( NULL );
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get variable with address of function to delete a slider */

			func_ptr = func_get( "slider_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( func_ptr ) );
			writer( C_SDELETE_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_SDELETE_REPLY, 0L );

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		return SET;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing and returning the
 * arguments and results of the slider_value() function
 *--------------------------------------------------------------*/

double *exp_sstate( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		double *result;


		if ( ! writer( C_SSTATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = DOUBLE_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = -1.0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		long val;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		char *pos;
		double res[ 2 ];
		long ID;


		TRY
		{
			/* Get variable with address of function to set/get slider value */

			func_ptr = func_get( "slider_value", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			memcpy( &val, pos, sizeof val );
			pos += sizeof val;

			if ( val > 0 )
			{
				double dval;

				memcpy( &dval, pos, sizeof dval );
				vars_push( FLOAT_VAR, dval );
			}
			pos += sizeof( double );

			EDL.Fname = pos;                       /* get current file name */

			/* Call the function */

			ret = func_call( func_ptr );
			res[ 0 ] = 1.0;
			res[ 1 ] = ret->val.dval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			res[ 0 ] = -1.0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_SSTATE_REPLY, sizeof res, res ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing and returning the
 * arguments and results of the slider_changed() function
 *--------------------------------------------------------------*/

long *exp_schanged( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_SCHANGED, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = -1;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		char *pos;
		long res[ 2 ];
		long ID;


		TRY
		{
			/* Get variable with address of function to determine state
			   changes */

			func_ptr = func_get( "slider_changed", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                       /* get current file name */

			/* Call the function */

			ret = func_call( func_ptr );
			res[ 0 ] = 1;
			res[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			res[ 0 ] = -1;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_SCHANGED_REPLY, sizeof res, res ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the input_create() function.
 *--------------------------------------------------------------*/

long *exp_icreate( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_ICREATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof( long ) );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		long result[ 2 ];
		char *pos;
		long type;


		TRY
		{
			/* Get function to create an input object */

			func_ptr = func_get( "input_create", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &type, pos, sizeof type );
			vars_push( INT_VAR, type );              /* type of input object */
			pos += sizeof type;

			if ( type == INT_INPUT || type == INT_OUTPUT )
			{
				long lval;

				memcpy( &lval, pos, sizeof lval );
				vars_push( INT_VAR, lval );
				pos += sizeof lval;
			}
			else
			{
				double dval;

				memcpy( &dval, pos, sizeof dval );
				vars_push( FLOAT_VAR, dval );
				pos += sizeof dval;
			}

			EDL.Fname = pos;                         /* current file name */
			pos += strlen( pos ) + 1;

			vars_push( STR_VAR, pos );               /* get label string */
			pos += strlen( pos ) + 1;

			if ( *pos != '\0' )                      /* get help text */
			{
				if ( * ( ( unsigned char * ) pos ) == 0xff )
				{
					vars_push( STR_VAR, "" );
					pos += 1;
				}
				else
				{
					vars_push( STR_VAR, pos );
					pos += strlen( pos ) + 1;
				}
			}
			else
				pos++;

			if ( *pos != '\0' )                      /* get C format string */
				vars_push( STR_VAR, pos );

			/* Call the function */

			ret = func_call( func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_ICREATE_REPLY, sizeof result, result ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the input_delete() function.
 *--------------------------------------------------------------*/

bool exp_idelete( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		if ( ! writer( C_IDELETE, len, buffer ) )
		{
			T_free( buffer );
			return FAIL;
		}
		T_free( buffer );
		return reader( NULL );
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get function to delete an input object */

			func_ptr = func_get( "input_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( func_ptr ) );
			writer( C_IDELETE_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_IDELETE_REPLY, 0L );

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		return SET;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing and returning the
 * arguments and results of the input_value() function
 *--------------------------------------------------------------*/

Input_Res_T *exp_istate( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		Input_Res_T *input_res;


		if ( ! writer( C_ISTATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		input_res = INPUT_RES_P T_malloc( sizeof *input_res );
		if ( ! reader( ( void * ) input_res ) )
			input_res->res = -1;
		return input_res;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		long type;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		char *pos;
		Input_Res_T input_res;
		long ID;


		TRY
		{
			/* Get variable with address of function to set/get slider value */

			func_ptr = func_get( "input_value", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );            /* get object ID */
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			memcpy( &type, pos, sizeof type );
			pos += sizeof type;

			if ( type == 1 )                           /* new integer value */
			{
				long lval;

				memcpy( &lval, pos, sizeof lval );
				vars_push( INT_VAR, lval );
				pos += sizeof lval;
			}
			else if ( type == 2 )                      /* new float value */
			{
				double dval;

				memcpy( &dval, pos, sizeof dval );
				vars_push( FLOAT_VAR, dval );
				pos += sizeof dval;
			}

			EDL.Fname = pos;                            /* current file name */

			/* Call the function */

			ret = func_call( func_ptr );
			if ( ret->type == INT_VAR )
			{
				input_res.res = 0;
				input_res.val.lval = ret->val.lval;
			}
			else
			{
				input_res.res = 1;
				input_res.val.dval = ret->val.dval;
			}
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			input_res.res = -1;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_ISTATE_REPLY, sizeof input_res, &input_res ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing and returning the
 * arguments and results of the input_value() function
 *--------------------------------------------------------------*/

long *exp_ichanged( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *res;


		if ( ! writer( C_ICHANGED, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		res = LONG_P T_malloc( 2 * sizeof *res );
		if ( ! reader( ( void * ) res ) )
			res[ 0 ] = -1;
		return res;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		char *pos;
		long res[ 2 ];
		long ID;


		TRY
		{
			/* Get variable with address of function to change state */

			func_ptr = func_get( "input_changed", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );            /* object ID */
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			ret = func_call( func_ptr );
			res[ 0 ] = 0;
			res[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			res[ 0 ] = -1;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_ICHANGED_REPLY, sizeof res, res ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the menu_create() function.
 *--------------------------------------------------------------*/

long *exp_mcreate( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_MCREATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		long result[ 2 ];
		char *pos;
		long num_strs;
		long i;


		TRY
		{
			/* Get function to create an input object */

			func_ptr = func_get( "menu_create", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &num_strs, pos, sizeof num_strs );
			pos += sizeof num_strs;

			EDL.Fname = pos;                         /* current file name */
			pos += strlen( pos ) + 1;

			for ( i = 0; i < num_strs; i++ )
			{
				vars_push( STR_VAR, pos );
				pos += strlen( pos ) + 1;
			}

			ret = func_call( func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_MCREATE_REPLY, sizeof result, result ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool exp_mdelete( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		if ( ! writer( C_MDELETE, len, buffer ) )
		{
			T_free( buffer );
			return FAIL;
		}
		T_free( buffer );
		return reader( NULL );
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get variable with address of function to delete a menu */

			func_ptr = func_get( "menu_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );            /* get menu ID */
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( func_ptr ) );
			writer( C_MDELETE_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_MDELETE_REPLY, 0L );

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		return SET;
	}
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

long *exp_mchoice( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_MCHOICE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		long val;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		char *pos;
		long ID;
		long result[ 2 ];


		TRY
		{
			/* Get function for dealing with menu state */

			func_ptr = func_get( "menu_choice", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			memcpy( &val, pos, sizeof val );         /* menu item to be set */
			if ( val > 0 )
				vars_push( INT_VAR, val );
			pos += sizeof val;

			EDL.Fname = pos;                         /* current file name */

			/* Call the function */

			ret = func_call( func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_MCHOICE_REPLY, sizeof result, result ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

long *exp_mchanged( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_MCHANGED, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		char *pos;
		long ID;
		long result[ 2 ];


		TRY
		{
			/* Get function for dealing with menu state */

			func_ptr = func_get( "menu_changed", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                         /* current file name */

			/* Call the function */

			ret = func_call( func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_MCHANGED_REPLY, sizeof result, result ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

long *exp_tbchanged( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_TBCHANGED, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		Var_T *ret = NULL;
		int acc;
		char *pos;
		long var_count;
		long ID;
		long result[ 2 ];


		TRY
		{
			/* Get function for dealing with menu state */

			func_ptr = func_get( "toolbox_changed", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &var_count, pos, sizeof var_count );
			pos += sizeof var_count;

			while ( var_count-- )
			{
				memcpy( &ID, pos, sizeof ID );
				vars_push( INT_VAR, ID );
				pos += sizeof ID;
			}
				
			EDL.Fname = pos;                         /* current file name */

			/* Call the function */

			ret = func_call( func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			vars_pop( ret );
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		if ( ! writer( C_TBCHANGED_REPLY, sizeof result, result ) )
			THROW( EXCEPTION );
		return NULL;
	}
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

long *exp_tbwait( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_TBWAIT, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = LONG_P T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		int acc;
		char *pos;
		double duration;
		long var_count;
		long ID;


		TRY
		{
			/* Get function for waiting for toolbox state change */

			func_ptr = func_get( "toolbox_wait", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &duration, pos, sizeof duration );
			vars_push( FLOAT_VAR, duration );
			pos += sizeof duration;

			memcpy( &var_count, pos, sizeof var_count );
			pos += sizeof var_count;

			while ( var_count-- )
			{
				memcpy( &ID, pos, sizeof ID );
				vars_push( INT_VAR, ID );
				pos += sizeof ID;
			}
				
			EDL.Fname = pos;                         /* current file name */

			/* Call the function */

			vars_pop( func_call( func_ptr ) );

			EDL.Fname = old_Fname;
			EDL.Lc = old_Lc;

			TRY_SUCCESS;
		}
		OTHERWISE
		{
			long result[ 2 ] = { 0, 0 };


			EDL.Fname = old_Fname;
			EDL.Lc = old_Lc;
			if ( ! writer( C_TBWAIT_REPLY, sizeof result, result ) )
				THROW( EXCEPTION );
		}

		return NULL;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the object_delete() function.
 *--------------------------------------------------------------*/

bool exp_objdel( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		if ( ! writer( C_ODELETE, len, buffer ) )
		{
			T_free( buffer );
			return FAIL;
		}
		T_free( buffer );
		return reader( NULL );
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get function to delete an input object */

			func_ptr = func_get( "object_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );            /* get object ID */
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( func_ptr ) );
			writer( C_ODELETE_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_ODELETE_REPLY, 0L );

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		return SET;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the object_change_label() function.
 *--------------------------------------------------------------*/

bool exp_clabel( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		if ( ! writer( C_CLABEL, len, buffer ) )
		{
			T_free( buffer );
			return FAIL;
		}
		T_free( buffer );
		return reader( NULL );
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get function to delete an input object */

			func_ptr = func_get( "object_change_label", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );            /* get object ID */
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */
			pos += strlen( pos ) + 1;

			vars_push( STR_VAR, pos );                /* get label string */

			/* Call the function */

			vars_pop( func_call( func_ptr ) );
			writer( C_CLABEL_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_CLABEL_REPLY, 0L );

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		return SET;
	}
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the object_enable() function.
 *--------------------------------------------------------------*/

bool exp_xable( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		if ( ! writer( C_XABLE, len, buffer ) )
		{
			T_free( buffer );
			return FAIL;
		}
		T_free( buffer );
		return reader( NULL );
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var_T *func_ptr;
		int acc;
		char *pos;
		long ID;
		long state;


		TRY
		{
			/* Get function to enable or disable an input object */

			func_ptr = func_get( "object_enable", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );            /* get object ID */
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */
			pos += strlen( pos ) + 1;

			memcpy( &state , pos, sizeof state );
			vars_push( INT_VAR, state );

			/* Call the function */

			vars_pop( func_call( func_ptr ) );
			writer( C_XABLE_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_XABLE_REPLY, 0L );

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
		return SET;
	}
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

double *exp_getpos( char *buffer, ptrdiff_t len )
{
	if ( Fsc2_Internals.I_am == CHILD )
	{
		double *result;


		if ( ! writer( C_GETPOS, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}

		T_free( buffer );
		result = DOUBLE_P T_malloc( ( 2 * MAX_CURVES + 2 ) * sizeof *result );

		if ( ! reader( ( void * ) result ) )
		{
			T_free( result );
			THROW( EXCEPTION );
		}

		return result;
	}
	else
	{
		char *pos;
		double result[ 2 * MAX_CURVES + 2 ];
		long buttons;
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		int i;
		unsigned int keys;


		pos = buffer;

		memcpy( &buttons, pos, sizeof buttons );	/* buttons to be handed */
		pos += sizeof buttons;

		memcpy( &EDL.Lc, pos, sizeof EDL.Lc );	    /* current line number */
		pos += sizeof EDL.Lc;

		EDL.Fname = pos;	                        /* current file name */

		result[ 0 ] = 0.0;

		if ( buttons < 0 || G.button_state == buttons )
		{
			if ( G.focus & WINDOW_1D )
				result[ 0 ] = ( double ) get_mouse_pos_1d( result + 1, &keys );
			else if ( G.focus & WINDOW_2D )
				result[ 0 ] = ( double ) get_mouse_pos_2d( result + 1, &keys );
			else if ( G.focus & WINDOW_CUT )
				result[ 0 ] = ( double ) get_mouse_pos_cut( result + 1,
															&keys );
		}

		if ( result[ 0 ] == 0.0 )
			for ( i = 1; i < 2 * MAX_CURVES + 1; i++ )
				result[ i ] = 0.0;

		result[ 2 * MAX_CURVES + 1 ] = 0;
		if ( keys & ShiftMask )
			result[ 2 * MAX_CURVES + 1 ] += ( 1 << 0 );
		if ( keys & LockMask )
			result[ 2 * MAX_CURVES + 1 ] += ( 1 << 1 );
		if ( keys & ControlMask )
			result[ 2 * MAX_CURVES + 1 ] += ( 1 << 2 );
		if ( keys & Mod1Mask )
			result[ 2 * MAX_CURVES + 1 ] += ( 1 << 3 );
		if ( keys & Mod2Mask )
			result[ 2 * MAX_CURVES + 1 ] += ( 1 << 4 );
		if ( keys & Mod3Mask )
			result[ 2 * MAX_CURVES + 1 ] += ( 1 << 5 );
		if ( keys & Mod4Mask )
			result[ 2 * MAX_CURVES + 1 ] += ( 1 << 6 );
		if ( keys & Mod5Mask )
			result[ 2 * MAX_CURVES + 1 ] += ( 1 << 7 );

		writer( C_GETPOS_REPLY, sizeof result, result );

		EDL.Fname = old_Fname;
		EDL.Lc = old_Lc;
	}

	return NULL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
