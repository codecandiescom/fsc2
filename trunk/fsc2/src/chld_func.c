/*
  $Id$
*/


#include "fsc2.h"

static const char *handle_input( const char *content, const char *label );


/*-------------------------------------------------------*/
/* Shows a messge box with an "OK" button - use embedded */
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
/* pressed button (int he range from 1 - 3).                              */
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


static const char *handle_input( const char *content, const char *label )
{
	if ( label != NULL && label != '\0' )
		fl_set_object_label( input_form->comm_input, label );
	else
		fl_set_object_label( input_form->comm_input, "Enter your comment:" );

	fl_set_input( input_form->comm_input, content );

	fl_show_form( input_form->input_form,
				  FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
				  "fsc: Comment editor" );

	while ( fl_do_forms( ) != input_form->comm_done )
		;

	if ( fl_form_is_visible( input_form->input_form ) )
		fl_hide_form( input_form->input_form );

	return fl_get_input( input_form->comm_input );
}
