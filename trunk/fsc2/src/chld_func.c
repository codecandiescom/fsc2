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

static const char *handle_input( const char *content, const char *label );


/*-------------------------------------------------------*/
/* Shows a message box with an "OK" button - use embedded */
/* newline characters to get multi-line messages.        */
/*-------------------------------------------------------*/

void show_message( const char *str )
{
	if ( Internals.I_am == PARENT )
	{
		switch_off_special_cursors( );
		fl_show_messages( str );
	}
	else
	{
		if ( ! writer( C_SHOW_MESSAGE, str ) || ! reader( NULL ) )
			THROW( EXCEPTION );
	}
}


/*-------------------------------------------------------*/
/* Shows an alert box with an "OK" button - use embedded */
/* newline characters to get multi-line messages.        */
/*-------------------------------------------------------*/

void show_alert( const char *str )
{
	char *strc, *strs[ 3 ];


	if ( Internals.I_am == PARENT )
	{
		switch_off_special_cursors( );

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

		fl_show_alert( strs[ 0 ], strs[ 1 ], strs[ 2 ], 1 );
		T_free( strc );
	}
	else
	{
		if ( ! writer( C_SHOW_ALERT, str ) || ! reader( NULL ) )
			THROW( EXCEPTION );
	}
}


/*------------------------------------------------------------------------*/
/* Shows a choice box with up to three buttons, returns the number of the */
/* pressed button (in the range from 1 - 3).                              */
/* ->                                                                     */
/*    1. message text, use embedded newlines for multi-line messages      */
/*    2. number of buttons to show                                        */
/*    3. texts for the three buttons                                      */
/*    4. number of button to be used as default button (range 1 - 3)      */
/*------------------------------------------------------------------------*/

int show_choices( const char *text, int numb, const char *b1, const char *b2,
				  const char *b3, int def )
{
	int ret;

	if ( Internals.I_am == PARENT )
	{
		switch_off_special_cursors( );
		return fl_show_choices( text, numb, b1, b2, b3, def );
	}
	else
	{
		if ( ! writer( C_SHOW_CHOICES, text, numb, b1, b2, b3, def ) ||
			 ! reader( ( void * ) &ret ) )
			THROW( EXCEPTION );
		return ret;
	}
}


/*---------------------------------------------------------------*/
/* Shows a file selector box and returns the selected file name. */
/* ->                                                            */
/*    1. short message to be shown                               */
/*    2. name of directory the file might be in                  */
/*    3. pattern for files to be listed (e.g. "*.edl")           */
/*    4. default file name                                       */
/* <-                                                            */
/* Returns either a static buffer with the file name or NULL.    */
/*---------------------------------------------------------------*/

const char *show_fselector( const char *message, const char *directory,
							const char *pattern, const char *def )
{
	const char *ret = NULL;


	if ( Internals.I_am == PARENT )
	{
		switch_off_special_cursors( );
		return fl_show_fselector( message, directory, pattern, def );
	}
	else
	{
		if ( ! writer( C_SHOW_FSELECTOR, message, directory, pattern, def ) ||
			 ! reader( ( void * ) &ret ) )
			THROW( EXCEPTION );
		return ret;
	}
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

const char *show_input( const char *content, const char *label )
{
	char *ret = NULL;

	if ( Internals.I_am == PARENT )
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


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

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

	while ( fl_do_forms( ) != GUI.input_form->comm_done )
		;

	if ( fl_form_is_visible( GUI.input_form->input_form ) )
		fl_hide_form( GUI.input_form->input_form );

	return fl_get_input( GUI.input_form->comm_input );
}


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return value of the layout() function.                   */
/*--------------------------------------------------------------*/

bool exp_layout( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
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
		Var *Func_ptr;
		int acc;
		char *pos;
		long type;


		TRY
		{
			/* Get variable with address of function to create a button */

			Func_ptr = func_get( "layout", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &type, pos, sizeof type );        /* get layout type */
			vars_push( INT_VAR, type );
			pos += sizeof type;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( Func_ptr ) );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return values of the button_create() function.           */
/*--------------------------------------------------------------*/

long *exp_bcreate( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_BCREATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var *Func_ptr;
		Var *ret = NULL;
		long val;
		int acc;
		long result[ 2 ];
		char *pos;
		long ID;


		TRY
		{
			/* Get variable with address of function to create a button */

			Func_ptr = func_get( "button_create", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

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

			ret = func_call( Func_ptr );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return value of the button_delete() function.            */
/*--------------------------------------------------------------*/

bool exp_bdelete( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
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
		Var *Func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get variable with address of function to create a button */

			Func_ptr = func_get( "button_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );  /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                        /* current file name */

			/* Call the function */

			vars_pop( func_call( Func_ptr ) );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return values of the button_state() function .           */
/*--------------------------------------------------------------*/

long *exp_bstate( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_BSTATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		long val;
		Var *Func_ptr;
		Var *ret = NULL;
		int acc;
		char *pos;
		long ID;
		long result[ 2 ];


		TRY
		{
			/* Get variable with address of function to create a button */

			Func_ptr = func_get( "button_state", &acc );

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

			ret = func_call( Func_ptr );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return values of the slider_create() function .          */
/*--------------------------------------------------------------*/

long *exp_screate( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_SCREATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var *Func_ptr;
		Var *ret = NULL;
		int acc;
		long result[ 2 ];
		char *pos;
		long ID;
		double val;
		int i;


		TRY
		{
			/* Get variable with address of function to create a button */

			Func_ptr = func_get( "slider_create", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

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

			ret = func_call( Func_ptr );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return value of the slider_delete() function.            */
/*--------------------------------------------------------------*/

bool exp_sdelete( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
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
		Var *Func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get variable with address of function to create a button */

			Func_ptr = func_get( "slider_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( Func_ptr ) );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing and returning the */
/* arguments and results of the slider_value() function         */
/*--------------------------------------------------------------*/

double *exp_sstate( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
	{
		double *result;


		if ( ! writer( C_SSTATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = -1.0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		long val;
		Var *Func_ptr;
		Var *ret = NULL;
		int acc;
		char *pos;
		double res[ 2 ];
		long ID;


		TRY
		{
			/* Get variable with address of function to set/get slider value */

			Func_ptr = func_get( "slider_value", &acc );

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

			ret = func_call( Func_ptr );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return values of the input_create() function.            */
/*--------------------------------------------------------------*/

long *exp_icreate( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_ICREATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = T_malloc( 2 * sizeof( long ) );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var *Func_ptr;
		Var *ret = NULL;
		int acc;
		long result[ 2 ];
		char *pos;
		long type;


		TRY
		{
			/* Get function to create an input object */

			Func_ptr = func_get( "input_create", &acc );

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

			ret = func_call( Func_ptr );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return value of the input_delete() function.             */
/*--------------------------------------------------------------*/

bool exp_idelete( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
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
		Var *Func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get function to delete an input object */

			Func_ptr = func_get( "input_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( Func_ptr ) );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing and returning the */
/* arguments and results of the input_value() function          */
/*--------------------------------------------------------------*/

INPUT_RES *exp_istate( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
	{
		INPUT_RES *input_res;


		if ( ! writer( C_ISTATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		input_res = T_malloc( sizeof *input_res );
		if ( ! reader( ( void * ) input_res ) )
			input_res->res = -1;
		return input_res;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		long type;
		Var *Func_ptr;
		Var *ret = NULL;
		int acc;
		char *pos;
		INPUT_RES input_res;
		long ID;


		TRY
		{
			/* Get variable with address of function to set/get slider value */

			Func_ptr = func_get( "input_value", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );            /* get menu ID */
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

			ret = func_call( Func_ptr );
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


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return values of the menu_create() function.             */
/*--------------------------------------------------------------*/

long *exp_mcreate( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_MCREATE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		Var *Func_ptr;
		Var *ret = NULL;
		int acc;
		long result[ 2 ];
		char *pos;
		long num_strs;
		long i;


		TRY
		{
			/* Get function to create an input object */

			Func_ptr = func_get( "menu_create", &acc );

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

			ret = func_call( Func_ptr );
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


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

bool exp_mdelete( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
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
		Var *Func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get variable with address of function to delete a menu */

			Func_ptr = func_get( "menu_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );            /* get menu ID */
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( Func_ptr ) );
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


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

long *exp_mchoice( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
	{
		long *result;


		if ( ! writer( C_MCHOICE, len, buffer ) )
		{
			T_free( buffer );
			THROW( EXCEPTION );
		}
		T_free( buffer );
		result = T_malloc( 2 * sizeof *result );
		if ( ! reader( ( void * ) result ) )
			result[ 0 ] = 0;
		return result;
	}
	else
	{
		char *old_Fname = EDL.Fname;
		long old_Lc = EDL.Lc;
		long val;
		Var *Func_ptr;
		Var *ret = NULL;
		int acc;
		char *pos;
		long ID;
		long result[ 2 ];


		TRY
		{
			/* Get function for dealing with menu state */

			Func_ptr = func_get( "menu_choice", &acc );

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

			ret = func_call( Func_ptr );
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
		return 0;
	}
}


/*--------------------------------------------------------------*/
/* Child and parent side function for passing the arguments and */
/* the return value of the object_delete() function.            */
/*--------------------------------------------------------------*/

bool exp_objdel( char *buffer, ptrdiff_t len )
{
	if ( Internals.I_am == CHILD )
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
		Var *Func_ptr;
		int acc;
		char *pos;
		long ID;


		TRY
		{
			/* Get function to delete an input object */

			Func_ptr = func_get( "object_delete", &acc );

			/* Unpack parameter and push them onto the stack */

			pos = buffer;
			memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( &ID, pos, sizeof ID );            /* get object ID */
			vars_push( INT_VAR, ID );
			pos += sizeof ID;

			EDL.Fname = pos;                          /* current file name */

			/* Call the function */

			vars_pop( func_call( Func_ptr ) );
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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
