/*
  $Id$
*/


#include "fsc2.h"

static const char *handle_input( const char *content, const char *label );


/*-------------------------------------------------------*/
/* Shows a message box with an "OK" button - use embedded */
/* newline characters to get multi-line messages.        */
/*-------------------------------------------------------*/

void show_message( const char *str )
{
	if ( I_am == PARENT )
	{
		switch_off_special_cursors( );
		fl_show_messages( str );
	}
	else
		writer( C_SHOW_MESSAGE, str );
}


/*-------------------------------------------------------*/
/* Shows an alert box with an "OK" button - use embedded */
/* newline characters to get multi-line messages.        */
/*-------------------------------------------------------*/

void show_alert( const char *str )
{
	char *strc, *strs[ 3 ];
	

	if ( I_am == PARENT )
	{
		switch_off_special_cursors( );

		strc = get_string_copy( str );
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
		writer( C_SHOW_ALERT, str );
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

	if ( I_am == PARENT )
	{
		switch_off_special_cursors( );
		return fl_show_choices( text, numb, b1, b2, b3, def );
	}
	else
	{
		writer( C_SHOW_CHOICES, text, numb, b1, b2, b3, def );
		reader( ( void * ) &ret );
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
	char *ret = NULL;

	if ( I_am == PARENT )
	{
		switch_off_special_cursors( );
		return fl_show_fselector( message, directory, pattern, def );
	}
	else
	{
		writer( C_SHOW_FSELECTOR, message, directory, pattern, def );
		reader( ( void * ) &ret );
		return ret;
	}
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

const char *show_input( const char *content, const char *label )
{
	char *ret = NULL;

	if ( I_am == PARENT )
	{
		switch_off_special_cursors( );
		return handle_input( content, label );
	}
	else
	{
		writer( C_INPUT, content, label );
		reader( ( void * ) &ret );
		return ret;
	}
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

long *exp_bcreate( void *buffer, long len )
{
	if ( I_am == CHILD )
	{
		long *result;


		writer( C_BCREATE, len, buffer );
		T_free( buffer );
		reader( ( void * ) result );
		return ( long * ) result;
	}
	else
	{
		char *old_Fname = Fname;
		long old_Lc = Lc;
		Var *Func_ptr;
		Var *ret = NULL;
		long val;
		int access;
		long result[ 2 ];
		void *pos;


		/* Get variable with address of function to create a button */

		Func_ptr = func_get( "button_create", &access );

		/* Unpack parameter and push them onto the stack */

		pos = buffer;
		memcpy( &Lc, pos, sizeof( long ) );      /* get current line number */
		pos += sizeof( long );

		vars_push( INT_VAR, * ( ( long * ) pos ) );
		pos += sizeof( long );

		memcpy( &val, pos, sizeof( long ) );     /* get colleague */
		if ( val >= 0 )
			vars_push( INT_VAR, val );
		pos += sizeof( long );

		Fname = ( char * ) pos;                  /* get current file name */
		pos += strlen( ( char * ) pos ) + 1;

		vars_push( STR_VAR, ( char * ) pos );    /* get label string */
		pos += strlen( ( char * ) pos ) + 1;

		if ( *( ( char * ) pos ) != '\0' )       /* get help text */
			vars_push( STR_VAR, ( char * ) pos );

		/* Call the function */

		TRY
		{
			ret = func_call( Func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		vars_pop( ret );
		Fname = old_Fname;
		Lc = old_Lc;
		writer( C_BCREATE_REPLY, 2 * sizeof( long ), result );

		return NULL;
	}
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

bool exp_bdelete( void *buffer, long len )
{
	if ( I_am == CHILD )
	{
		writer( C_BDELETE, len, buffer );
		T_free( buffer );
		return ( bool ) reader( NULL );
	}
	else
	{
		char *old_Fname = Fname;
		long old_Lc = Lc;
		Var *Func_ptr;
		int access;
		void *pos;


		/* Get variable with address of function to create a button */

		Func_ptr = func_get( "button_delete", &access );

		/* Unpack parameter and push them onto the stack */

		pos = buffer;
		memcpy( &Lc, pos, sizeof( long ) );    /* get current line number */
		pos += sizeof( long );

		vars_push( INT_VAR, * ( ( long * ) pos ) );
		pos += sizeof( long );

		Fname = ( char * ) pos;                /* get current file name */

		/* Call the function */

		TRY
		{
			vars_pop( func_call( Func_ptr ) );
			writer( C_BDELETE_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_BDELETE_REPLY, 0L );

		Fname = old_Fname;
		Lc = old_Lc;
		return SET;
	}
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

long exp_bstate( void *buffer, long len )
{
	if ( I_am == CHILD )
	{
		writer( C_BSTATE, len, buffer );
		T_free( buffer );
		return reader( NULL );
	}
	else
	{
		char *old_Fname = Fname;
		long old_Lc = Lc;
		long val;
		Var *Func_ptr;
		Var *ret = NULL;
		int access;
		void *pos;

		/* Get variable with address of function to create a button */

		Func_ptr = func_get( "button_state", &access );

		/* Unpack parameter and push them onto the stack */

		pos = buffer;
		memcpy( &Lc, pos, sizeof( long ) );    /* get current line number */
		pos += sizeof( long );

		vars_push( INT_VAR, * ( ( long * ) pos ) );
		pos += sizeof( long );

		memcpy( &val, pos, sizeof( long ) );   /* get state to be set */
		if ( val >= 0 )
			vars_push( INT_VAR, val );
		pos += sizeof( long );

		Fname = ( char * ) pos;                /* get current file name */

		/* Call the function */

		TRY
		{
			ret = func_call( Func_ptr );
			writer( C_BSTATE_REPLY, ret->val.lval );
			TRY_SUCCESS;
		}
		OTHERWISE
			writer( C_BSTATE_REPLY, -1L );

		Fname = old_Fname;
		Lc = old_Lc;
		vars_pop( ret );
		return 0;
	}
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

long *exp_screate( void *buffer, long len )
{
	if ( I_am == CHILD )
	{
		long *result;


		writer( C_SCREATE, len, buffer );
		T_free( buffer );
		result = T_malloc( 2 * sizeof( long ) );
		reader( ( void * ) result );
		return ( long * ) result;
	}
	else
	{
		char *old_Fname = Fname;
		long old_Lc = Lc;
		Var *Func_ptr;
		Var *ret = NULL;
		int access;
		long result[ 2 ];
		void *pos;


		/* Get variable with address of function to create a button */

		Func_ptr = func_get( "slider_create", &access );

		/* Unpack parameter and push them onto the stack */

		pos = buffer;
		memcpy( &Lc, pos, sizeof( long ) );      /* get current line number */
		pos += sizeof( long );

		vars_push( INT_VAR, *( ( long * ) pos ) );
		pos += sizeof( long );

		vars_push( FLOAT_VAR, *( ( double * ) pos ) );
		pos += sizeof( double );

		vars_push( FLOAT_VAR, *( ( double * ) pos ) );
		pos += sizeof( double );

		Fname = ( char * ) pos;                  /* get current file name */
		pos += strlen( ( char * ) pos ) + 1;

		vars_push( STR_VAR, ( char * ) pos );    /* get label string */
		pos += strlen( ( char * ) pos ) + 1;

		if ( *( ( char * ) pos ) != '\0' )       /* get help text */
			vars_push( STR_VAR, ( char * ) pos );

		/* Call the function */

		TRY
		{
			ret = func_call( Func_ptr );
			result[ 0 ] = 1;
			result[ 1 ] = ret->val.lval;
			TRY_SUCCESS;
		}
		OTHERWISE
			result[ 0 ] = 0;

		Fname = old_Fname;
		Lc = old_Lc;
		vars_pop( ret );
		writer( C_SCREATE_REPLY, 2 * sizeof( long ), result );

		return NULL;
	}
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

bool exp_sdelete( void *buffer, long len )
{
	if ( I_am == CHILD )
	{
		writer( C_SDELETE, len, buffer );
		T_free( buffer );
		return ( bool ) reader( NULL );
	}
	else
	{
		char *old_Fname = Fname;
		long old_Lc = Lc;
		Var *Func_ptr;
		int access;
		void *pos;


		/* Get variable with address of function to create a button */

		Func_ptr = func_get( "slider_delete", &access );

		/* Unpack parameter and push them onto the stack */

		pos = buffer;
		memcpy( &Lc, pos, sizeof( long ) );    /* get current line number */
		pos += sizeof( long );

		vars_push( INT_VAR, *( ( long * ) pos ) );
		pos += sizeof( long );

		Fname = ( char * ) pos;                /* get current file name */

		/* Call the function */

		TRY
		{
			vars_pop( func_call( Func_ptr ) );
			writer( C_SDELETE_REPLY, 1L );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( buffer );
			writer( C_SDELETE_REPLY, 0L );
		}

		Fname = old_Fname;
		Lc = old_Lc;
		return SET;
	}
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

double *exp_sstate( void *buffer, long len )
{
	if ( I_am == CHILD )
	{
		double *result;


		writer( C_SSTATE, len, buffer );
		T_free( buffer );
		result = T_malloc( 2 * sizeof( double ) );
		reader( ( void * ) result );
		return ( double * ) result;
	}
	else
	{
		char *old_Fname = Fname;
		long old_Lc = Lc;
		long lval;
		Var *Func_ptr;
		Var *ret = NULL;
		int access;
		void *pos;
		double res[ 2 ];


		/* Get variable with address of function to set/get slider value */

		Func_ptr = func_get( "slider_value", &access );

		/* Unpack parameter and push them onto the stack */

		pos = buffer;
		memcpy( &Lc, pos, sizeof( long ) );    /* get current line number */
		pos += sizeof( long );

		vars_push( INT_VAR, *( ( long * ) pos ) );
		pos += sizeof( long );

		memcpy( &lval, pos, sizeof( long ) );
		pos += sizeof( long );
		if ( lval > 0 )
			vars_push( FLOAT_VAR, * ( ( double * ) pos ) );
		pos += sizeof( double );

		Fname = ( char * ) pos;                /* get current file name */

		/* Call the function */

		TRY
		{
			ret = func_call( Func_ptr );
			res[ 0 ] = 10.0;
			res[ 1 ] = ret->val.dval;
			TRY_SUCCESS;
		}
		OTHERWISE
			res[ 0 ] = -10.0;

		vars_pop( ret );
		Fname = old_Fname;
		Lc = old_Lc;
		writer( C_SSTATE_REPLY, 2 * sizeof( double ), res );
		return NULL;
	}
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static const char *handle_input( const char *content, const char *label )
{
	if ( label != NULL && label != '\0' )
		fl_set_object_label( input_form->comm_input, label );
	else
		fl_set_object_label( input_form->comm_input, "Enter your comment:" );

	fl_set_input( input_form->comm_input, content );

	fl_show_form( input_form->input_form,
				  FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
				  "fsc2: Comment editor" );

	while ( fl_do_forms( ) != input_form->comm_done )
		;

	if ( fl_form_is_visible( input_form->input_form ) )
		fl_hide_form( input_form->input_form );

	return fl_get_input( input_form->comm_input );
}
