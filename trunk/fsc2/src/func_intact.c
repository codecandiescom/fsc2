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



static void func_intact_init( void );
static IOBJECT *find_object_from_ID( long ID );
static void recreate_Tool_Box( void );
static FL_OBJECT *append_object_to_form( IOBJECT *io );
static void tools_callback( FL_OBJECT *ob, long data );
static bool check_format_string( char *buf );
static void convert_escapes( char *str );
static void check_label( char *str );
static void store_geometry( void );


extern FL_resource xresources[ ];
extern FL_IOPT xcntl;
static int tool_x, tool_y;
static bool tool_has_been_shown = UNSET;
static bool is_frozen = UNSET;
static bool needs_pos = SET;
static TOOL_BOX *Tool_Box = NULL;



static struct {
	bool is_init;

	int	WIN_MIN_WIDTH;
	int	WIN_MIN_HEIGHT;

	int	OBJ_HEIGHT;
	int	OBJ_WIDTH;

	int	BUTTON_HEIGHT;
	int	BUTTON_WIDTH;
	int	NORMAL_BUTTON_DELTA;

	int	SLIDER_HEIGHT;
	int	SLIDER_WIDTH;

	int INPUT_HEIGHT;
	int INPUT_WIDTH;

	int	LABEL_VERT_OFFSET;
	int	VERT_OFFSET;
	int	HORI_OFFSET;

	int	OFFSET_X0;
	int	OFFSET_Y0;
} FI_sizes = { UNSET, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };



/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

static void func_intact_init( void )
{
	if ( GUI.G_Funcs.size == LOW )
	{
		FI_sizes.WIN_MIN_WIDTH       = 30;
		FI_sizes.WIN_MIN_HEIGHT      = 30;

		FI_sizes.OBJ_HEIGHT          = 35;
		FI_sizes.OBJ_WIDTH           = 140;

		FI_sizes.BUTTON_HEIGHT       = 35;
		FI_sizes.BUTTON_WIDTH        = 150;
		FI_sizes.NORMAL_BUTTON_DELTA = 5;

		FI_sizes.SLIDER_HEIGHT       = 25;
		FI_sizes.SLIDER_WIDTH        = 150;

		FI_sizes.INPUT_HEIGHT        = 25;
		FI_sizes.INPUT_WIDTH         = 150;

		FI_sizes.LABEL_VERT_OFFSET   = 15;
		FI_sizes.VERT_OFFSET         = 10;
		FI_sizes.HORI_OFFSET         = 10;

		FI_sizes.OFFSET_X0           = 20;
		FI_sizes.OFFSET_Y0           = 20;
	}
	else
	{
		FI_sizes.WIN_MIN_WIDTH       = 50;
		FI_sizes.WIN_MIN_HEIGHT      = 50;

		FI_sizes.OBJ_HEIGHT          = 50;
		FI_sizes.OBJ_WIDTH           = 210;

		FI_sizes.BUTTON_HEIGHT       = 50;
		FI_sizes.BUTTON_WIDTH        = 210;
		FI_sizes.NORMAL_BUTTON_DELTA = 10;

		FI_sizes.SLIDER_HEIGHT       = 35;
		FI_sizes.SLIDER_WIDTH        = 210;

		FI_sizes.INPUT_HEIGHT        = 35;
		FI_sizes.INPUT_WIDTH         = 210;

		FI_sizes.LABEL_VERT_OFFSET   = 15;
		FI_sizes.VERT_OFFSET         = 15;
		FI_sizes.HORI_OFFSET         = 20;

		FI_sizes.OFFSET_X0           = 20;
		FI_sizes.OFFSET_Y0           = 20;
	}

	FI_sizes.is_init = SET;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var *f_freeze( Var *v )
{
	bool is_now_frozen;


	is_now_frozen = get_boolean( v );

	if ( Internals.I_am == CHILD &&
		 ! writer( C_FREEZE, ( int ) is_now_frozen ) )
		THROW( EXCEPTION );

	return vars_push( INT_VAR, ( long ) is_now_frozen );
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

void parent_freeze( int freeze )
{
	if ( Tool_Box == NULL ||  Tool_Box->Tools == NULL )
	{
		is_frozen = freeze ? SET : UNSET;
		return;
	}

	if ( is_frozen && ! freeze )
	{
		if ( needs_pos )
		{
			fl_set_form_position( Tool_Box->Tools, tool_x, tool_y );
			fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
						  FL_FULLBORDER, "fsc2: Tools" );
		}
		else
		{
			fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
						  FL_FULLBORDER, "fsc2: Tools" );
			tool_x = Tool_Box->Tools->x;
			tool_y = Tool_Box->Tools->y;
		}

		tool_has_been_shown = SET;
		fl_winminsize( Tool_Box->Tools->window,
					   FI_sizes.WIN_MIN_WIDTH, FI_sizes.WIN_MIN_HEIGHT );
	}
	else if ( ! is_frozen && freeze )
	{
		if ( fl_form_is_visible( Tool_Box->Tools ) )
		{
			store_geometry( );
			fl_hide_form( Tool_Box->Tools );
		}

		needs_pos = SET;
	}

	is_frozen = freeze ? SET : UNSET;
}


/*------------------------------------------------------------*/
/* Function sets the layout of the tool box, either vertical  */
/* or horizontal by passing it either 0 or 1  or "vert[ical]" */
/* or "hori[zontal]" (case insensitive). Must be called       */
/* before any object (button or slider) is created (or after  */
/* all of them have been deleted again :).                    */
/*------------------------------------------------------------*/

Var *f_layout( Var *v )
{
	long layout;
	const char *str[ ] = { "VERT", "VERTICAL", "HORI", "HORIZONTAL" };


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	if ( Internals.I_am == PARENT && Tool_Box != NULL )
	{
		print( FATAL, "Layout of tool box must be set before any buttons or "
			   "sliders are created.\n" );
		THROW( EXCEPTION );
	}

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == INT_VAR )
		layout = v->val.lval != 0 ? HORI : VERT;
	else if ( v->type == FLOAT_VAR )
		layout = v->val.dval != 0.0 ? HORI : VERT;
	else
		switch ( is_in( v->val.sptr, str, 4 ) )
		{
			case 0 : case 1 :
				layout = 0;
				break;

			case 2 : case 3 :
				layout = 1;
				break;

			default :
				if ( Internals.I_am == PARENT )
				{
					print( FATAL, "Unknown layout keyword `%s'.\n",
						   v->val.sptr );
					THROW( EXCEPTION );
				}
				else
				{
					print( FATAL, "Unknown layout keyword `%s', using default "
						   "vertical layout.\n", v->val.sptr );
					return vars_push( INT_VAR, 0 );
				}
		}

	/* The child has no control over the graphical stuff, it has to pass all
	   requests to the parent... */

	if ( Internals.I_am == CHILD )
	{
		char *buffer, *pos;
		size_t len;

		len = sizeof EDL.Lc + sizeof layout;

		if ( EDL.Fname )
			len += strlen( EDL.Fname ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &EDL.Lc, sizeof EDL.Lc );   /* current line number */
		pos += sizeof EDL.Lc;

		memcpy( pos, &layout, sizeof layout );   /* type of layout */
		pos += sizeof layout;

		if ( EDL.Fname )
		{
			strcpy( pos, EDL.Fname );             /* current file name */
			pos += strlen( EDL.Fname ) + 1;
		}
		else
			*pos++ = '\0';

		/* Bomb out if parent failed to set layout type */

		if ( ! exp_layout( buffer, pos - buffer ) )
			THROW( EXCEPTION );

		return vars_push( INT_VAR, ( long ) layout );
	}

	/* Set up structure for tool box */

	Tool_Box = T_malloc( sizeof *Tool_Box );
	Tool_Box->layout = layout;
	Tool_Box->Tools = NULL;                       /* no form created yet */
	Tool_Box->objs = NULL;                        /* and also no objects */

	return vars_push( INT_VAR, layout );
}


/*----------------------*/
/* Creates a new button */
/*----------------------*/

Var *f_bcreate( Var *v )
{
	long type;
	long coll = -1;
	char *label = NULL;
	char *help_text = NULL;
	IOBJECT *new_io = NULL, *ioi, *cio;
	long ID = 0;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* At least the type of the button must be specified */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* First argument must be type of button ("NORMAL_BUTTON", "PUSH_BUTTON"
	   or "RADIO_BUTTON" or 0, 1, 2) */

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == INT_VAR || v->type == FLOAT_VAR )
	{
		type = get_strict_long( v, "button type" );

		if ( type != NORMAL_BUTTON &&
			 type != PUSH_BUTTON   &&
			 type != RADIO_BUTTON     )
		{
			print( FATAL, "Invalid button type (%ld).\n", type );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( ! strcasecmp( v->val.sptr, "NORMAL_BUTTON" ) )
			type = NORMAL_BUTTON;
		else if ( ! strcasecmp( v->val.sptr, "PUSH_BUTTON" ) )
			type = PUSH_BUTTON;
		else if ( ! strcasecmp( v->val.sptr, "RADIO_BUTTON" ) )
			type = RADIO_BUTTON;
		else
		{
			print( FATAL, "Unknown button type: '%s'.\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}

	v = vars_pop( v );

	/* For radio buttons the next argument is the ID of the first button
	   belonging to the group (except for the first button itself) */

	if ( type == RADIO_BUTTON )
	{
		if ( v != NULL && v->type & ( INT_VAR | FLOAT_VAR ) )
		{
			coll = get_strict_long( v, "radio button group leader ID" );

			/* Checking that the referenced group leader button exists
			   and is also a RADIO_BUTTON can only be done by the parent */

			if ( Internals.I_am == PARENT )
			{
				/* Check that other group member exists at all */

				if ( ( cio = find_object_from_ID( coll ) ) == NULL )
				{
					print( FATAL, "Specified group leader button does not "
						   "exist.\n", coll );
					THROW( EXCEPTION );
				}

				/* The group leader must also be radio buttons */

				if ( cio->type != RADIO_BUTTON )
				{
					print( FATAL, "Button specified as group leader isn't a "
						   "RADIO_BUTTON.\n", coll );
					THROW( EXCEPTION );
				}
			}

			v = vars_pop( v );
		}
	}

	/* Since the child process can't use graphics it has to write the
	   parameter into a buffer, pass it to the parent process and ask the
	   parent to create the button */

	if ( Internals.I_am == CHILD )
	{
		char *buffer, *pos;
		long new_ID;
		long *result;
		size_t len;


		/* Next argument is the label string */

		if ( v != NULL )
		{
			vars_check( v, STR_VAR );
			label = v->val.sptr;

			/* Final argument can be a help text */

			if ( v->next != NULL )
			{
				vars_check( v->next, STR_VAR );
				help_text = v->next->val.sptr;

				/* Warn and get rid of superfluous arguments */
			
				if ( v->next->next != NULL )
					print( WARN, "Too many arguments, discarding superfluous "
						   "arguments.\n" );
			}
		}

		/* Calculate length of buffer needed */

		len = sizeof EDL.Lc + sizeof type + sizeof coll;

		if ( EDL.Fname )
			len += strlen( EDL.Fname ) + 1;
		else
			len++;
		if ( label )
			len += strlen( label ) + 1;
		else
			len++;
		if ( help_text )
			len += strlen( help_text ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &EDL.Lc, sizeof EDL.Lc ); /* current line number */
		pos += sizeof EDL.Lc;

		memcpy( pos, &type, sizeof type );     /* button type */
		pos += sizeof type;

		memcpy( pos, &coll, sizeof coll );     /* group leaders ID */
		pos += sizeof coll;

		if ( EDL.Fname )
		{
			strcpy( pos, EDL.Fname );           /* current file name */
			pos += strlen( EDL.Fname ) + 1;
		}
		else
			*pos++ = '\0';

		if ( label )                            /* label string */
		{
			strcpy( pos, label );
			pos += strlen( label ) + 1;
		}
		else
			*pos++ = '\0';

		if ( help_text )                        /* help text string */
		{
			strcpy( pos, help_text );
			pos += strlen( help_text ) + 1;
		}
		else
			*pos++ = '\0';


		/* Pass buffer to the parent and ask it to create the button. It
		   returns a buffer with two longs, the first indicating success
		   or failure (value is 1 or 0), the second being the buttons ID */

		result = exp_bcreate( buffer, pos - buffer );

		if ( result[ 0 ] == 0 )      /* failure -> bomb out */
		{
			T_free( result );
			THROW( EXCEPTION );
		}

		new_ID = result[ 1 ];
		T_free( result );           /* free result buffer */

		return vars_push( INT_VAR, new_ID );
	}

	/* Here follows the part that is run by the parent process only */

	/* Next argument is the label string */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );

		TRY
		{
			label = T_strdup( v->val.sptr );
			check_label( label );
			convert_escapes( label );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( label );
			RETHROW( );
		}

		v = vars_pop( v );
	}

	/* Final argument can be a help text */

	if ( v != NULL )
	{
		TRY
		{
			vars_check( v, STR_VAR );
			help_text = T_strdup( v->val.sptr );
			convert_escapes( help_text );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( help_text );
			T_free( label );
			RETHROW( );
		}

		v = vars_pop( v );
	}

	/* Warn and get rid of superfluous arguments */

	too_many_arguments( v );

	/* Now that we're done with checking the parameters we can create the new
       button - if the Tool_Box doesn't exist yet we've got to create it now */

	TRY
	{
		if ( Tool_Box == NULL )
		{
			Tool_Box = T_malloc( sizeof *Tool_Box );
			Tool_Box->objs = NULL;
			Tool_Box->layout = VERT;
			Tool_Box->Tools = NULL;
		}

		new_io = T_malloc( sizeof *new_io );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( new_io );
		T_free( help_text );
		T_free( label );
		RETHROW( );
	}
		
	if ( Tool_Box->objs == NULL )
	{
		Tool_Box->objs = new_io;
		new_io->next = new_io->prev = NULL;
	}
	else
	{
		for ( ioi = Tool_Box->objs; ioi->next != NULL; ioi = ioi->next )
			/* empty */ ;
		ID = ioi->ID + 1;
		ioi->next = new_io;
		new_io->prev = ioi;
		new_io->next = NULL;
	}

	new_io->ID = ID;
	new_io->type = ( int ) type;
	if ( type == RADIO_BUTTON && coll == -1 )
		new_io->state = 1;
	else
		new_io->state = 0;
	new_io->self = NULL;
	new_io->group = NULL;
	new_io->label = label;
	new_io->help_text = help_text;
	new_io->partner = coll;

	/* Draw the new button */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, new_io->ID );
}


/*--------------------------------------*/
/* Deletes one or more buttons          */
/* Parameter are one or more button IDs */
/*--------------------------------------*/

Var *f_bdelete( Var *v )
{
	IOBJECT *io, *nio;
	long new_anchor = 0;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need the ID of the button to delete */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Loop over all button numbers */

	do
	{
		/* Since the buttons 'belong' to the parent, the child needs to ask
		   the parent to delete the button. The ID of each button to be deleted
		   gets passed to the parent in a buffer and the parent is asked to
		   delete the button. */

		if ( Internals.I_am == CHILD )
		{
			char *buffer, *pos;
			size_t len;
			long ID;

			
			/* Do all possible checking of the parameter */

			ID = get_strict_long( v, "button ID" );

			if ( ID < 0 )
			{
				print( FATAL, "Invalid button identifier.\n" );
				THROW( EXCEPTION );
			}

			/* Get a long enough buffer and write data */

			len = sizeof EDL.Lc + sizeof v->val.lval;

			if ( EDL.Fname )
				len += strlen( EDL.Fname ) + 1;
			else
				len++;

			pos = buffer = T_malloc( len );

			memcpy( pos, &EDL.Lc, sizeof EDL.Lc );   /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( pos, &ID, sizeof ID );           /* button ID */
			pos += sizeof ID;

			if ( EDL.Fname )
			{
				strcpy( pos, EDL.Fname );             /* current file name */
				pos += strlen( EDL.Fname ) + 1;
			}
			else
				*pos++ = '\0';

			/* Ask parent to delete the button, bomb out on failure */

			if ( ! exp_bdelete( buffer, pos - buffer ) )
				THROW( EXCEPTION );
		}
		else
		{

			/* No tool box -> no buttons -> no buttons to delete... */

			if ( Tool_Box == NULL || Tool_Box->objs == NULL )
			{
				print( FATAL, "No buttons have been defined yet.\n" );
				THROW( EXCEPTION );
			}

			/* Check the parameters */

			io = find_object_from_ID( get_strict_long( v, "button ID" ) );

			if ( io == NULL ||
				 ( io->type != NORMAL_BUTTON &&
				   io->type != PUSH_BUTTON   &&
				   io->type != RADIO_BUTTON ) )
			{
				print( FATAL, "Invalid button identifier.\n" );
				THROW( EXCEPTION );
			}

			/* Remove button from the linked list */

			if ( io->next != NULL )
				io->next->prev = io->prev;
			if ( io->prev != NULL )
				io->prev->next = io->next;
			else
				Tool_Box->objs = io->next;

			/* Delete the button (it's not drawn in a test run!) */

			if ( Internals.mode != TEST )
			{
				fl_delete_object( io->self );
				fl_free_object( io->self );
			}

			/* Special care has to be taken for the first radio buttons of a
			   group (i.e. the one the others refer to) is deleted - the next
			   button from the group must be made group leader and the
			   remaining buttons must get told about it) */

			if ( io->type == RADIO_BUTTON && io->partner == -1 )
			{
				for ( nio = io->next; nio != NULL; nio = nio->next )
					if ( nio->type == RADIO_BUTTON && nio->partner == io->ID )
					{
						new_anchor = nio->ID;
						nio->partner = -1;
						break;
					}

				if ( nio != NULL )
					for ( nio = nio->next; nio != NULL; nio = nio->next )
						if ( nio->type == RADIO_BUTTON &&
							 nio->partner == io->ID )
							nio->partner = new_anchor;
			}

			T_free( ( void * ) io->label );
			T_free( ( void * ) io->help_text );
			T_free( io );

			if ( Tool_Box->objs == NULL )
			{
				if ( Internals.mode != TEST )
				{
					if ( Tool_Box->Tools )
					{
						if ( fl_form_is_visible( Tool_Box->Tools ) )
						{
							store_geometry( );
							fl_hide_form( Tool_Box->Tools );
						}
						
						fl_free_form( Tool_Box->Tools );
					}
					else
						tool_has_been_shown = UNSET;
				}
				
				Tool_Box = T_free( Tool_Box );
				
				if ( ( v = vars_pop( v ) ) != NULL )
				{
					print( FATAL, "Invalid button identifier.\n" );
					THROW( EXCEPTION );
				}

				return vars_push( INT_VAR, 1 );
			}
		}
	} while ( ( v = vars_pop( v ) ) != NULL );

	/* The child process is already done here, and also a test run */

	if ( Internals.I_am == CHILD || Internals.mode == TEST || ! Tool_Box )
		return vars_push( INT_VAR, 1 );

	/* Redraw the form without the deleted buttons */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------*/
/* Sets or returns the state of a button */
/*---------------------------------------*/

Var *f_bstate( Var *v )
{
	IOBJECT *io, *oio;
	int state;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need at least the ID of the button */

	if ( v == NULL )
	{
		print( FATAL, "Missing argumnts.\n" );
		THROW( EXCEPTION );
	}

	/* Again, the child doesn't know about the button, so it got to ask the
	   parent process */

	if ( Internals.I_am == CHILD )
	{
		long ID;
		long chld_state = -1;
		char *buffer, *pos;
		size_t len;
		long *result;


		/* Basic check of button identifier - always the first parameter */

		ID = get_strict_long( v, "button ID" );

		if ( ID < 0 )
		{
			print( FATAL, "Invalid button identifier.\n" );
			THROW( EXCEPTION );
		}

		/* If there's a second parameter it's the state of button to set */

		if ( ( v = vars_pop( v ) ) != NULL )
			chld_state = get_boolean( v );

		/* No more parameters accepted... */

		too_many_arguments( v );

		/* Make up buffer to send to parent process */

		len = sizeof EDL.Lc + sizeof ID + sizeof chld_state;

		if ( EDL.Fname )
			len += strlen( EDL.Fname ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &EDL.Lc, sizeof EDL.Lc ); /* current line number */
		pos += sizeof EDL.Lc;

		memcpy( pos, &ID, sizeof ID );     /* buttons ID */
		pos += sizeof ID;

		memcpy( pos, &chld_state, sizeof chld_state );
		                                        /* state to be set (negative */
		pos += sizeof chld_state;               /* if not to be set)         */

		if ( EDL.Fname )
		{
			strcpy( pos, EDL.Fname );           /* current file name */
			pos += strlen( EDL.Fname ) + 1;
		}
		else
			*pos++ = '\0';

		/* Ask parent process to return or set the state - bomb out if it
		   returns a negative value, indicating a severe error */

		result = exp_bstate( buffer, pos - buffer );

		/* Bomb out if parent returns failure */

		if ( result[ 0 ] == 0 )
		{
			T_free( result );
			THROW( EXCEPTION );
		}

		chld_state = result[ 1 ];
		T_free( result );

		return vars_push( INT_VAR, chld_state );
	}

	/* No tool box -> no buttons -> no button state to set or get... */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No buttons have been defined yet.\n" );
		THROW( EXCEPTION );
	}

	/* Check the button ID parameter */

	io = find_object_from_ID( get_strict_long( v, "button ID" ) );

	if ( io == NULL ||
		 ( io->type != NORMAL_BUTTON &&
		   io->type != PUSH_BUTTON   &&
		   io->type != RADIO_BUTTON ) )
	{
		print( FATAL, "Invalid button identifier.\n" );
		THROW( EXCEPTION );
	}

	/* If there's no second parameter just return the button state - for
	   NORMAL_BUTTONs return the number it was pressed since the last call
	   and reset the counter to zero */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( io->type == NORMAL_BUTTON )
		{
			state = io->state;
			io->state = 0;
			return vars_push( INT_VAR, state );
		}

		return vars_push( INT_VAR, io->state != 0 ? 1 : 0 );
	}

	/* The optional second argument is the state to be set - but take care,
	   the state of NORMAL_BUTTONs can't be set */

	if ( io->type == NORMAL_BUTTON )
	{
		print( FATAL, "Can't set state of a NORMAL_BUTTON.\n" );
		THROW( EXCEPTION );
	}

	io->state = get_boolean( v );

	/* Can't switch off a radio button that is switched on */

	if ( io->type == RADIO_BUTTON && io->state == 0 &&
		 fl_get_button( io->self ) )
	{
		print( FATAL, "Can't switch off a RADIO_BUTTON.\n" );
		THROW( EXCEPTION );
	}

	/* If this isn't a test run set the button state */

	if ( Internals.mode != TEST )
		fl_set_button( io->self, io->state );

	/* If one of the radio buttons is set all the other buttons belonging
	   to the group must become unset */

	if ( io->type == RADIO_BUTTON && io->state == 1 )
	{
		for ( oio = Tool_Box->objs; oio != NULL; oio = oio->next )
		{
			if ( oio == io || oio->type != RADIO_BUTTON ||
				 oio->group != io->group )
				continue;

			oio->state = 0;
			if ( Internals.mode != TEST )
				fl_set_button( oio->self, oio->state );
		}
	}

	too_many_arguments( v );

	return vars_push( INT_VAR, io->state );
}


/*----------------------*/
/* Creates a new slider */
/*----------------------*/

Var *f_screate( Var *v )
{
	IOBJECT *new_io = NULL, *ioi;
	int type;
	double start_val, end_val;
	double step = 0.0;
	char *label = NULL;
	char *help_text = NULL;
	long ID = 0;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need at least the type of the slider and the minimum and maximum
	   value */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Check the type parameter */

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == INT_VAR || v->type == FLOAT_VAR )
	{
		type = get_strict_long( v, "slider type" );

		if ( type != NORMAL_SLIDER && type != VALUE_SLIDER )
		{
			print( FATAL, "Invalid slider type (%ld).\n", type );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( ! strcasecmp( v->val.sptr, "NORMAL_SLIDER" ) )
			type = NORMAL_SLIDER;
		else if ( ! strcasecmp( v->val.sptr, "VALUE_SLIDER" ) )
			type = VALUE_SLIDER;
		else
		{
			print( FATAL, "Unknown slider type: '%s'.\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}

	/* Get the minimum value for the slider */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing slider minimum value.\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	start_val = VALUE( v );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing slider maximum value.\n" );
		THROW( EXCEPTION );
	}

	/* Get the maximum value and make sure it's larger than the minimum */

	vars_check( v, INT_VAR | FLOAT_VAR );
	end_val = VALUE( v );

	if ( end_val <= start_val )
	{
		print( FATAL, "Maximum not larger than minimum slider value.\n" );
		THROW( EXCEPTION );
	}

	if ( v->next != NULL && v->next->type & ( INT_VAR | FLOAT_VAR ) )
	{
		v = vars_pop( v );
		vars_check( v, INT_VAR | FLOAT_VAR );
		step = fabs( VALUE( v ) );
		if ( step < 0.0 )
		{
			print( FATAL, "Negative slider step width.\n" );
			THROW( EXCEPTION );
		}

		if ( step > end_val - start_val )
		{
			print( FATAL, "Slider step size larger than difference between "
				   "minimum and maximum value.\n" );
			THROW( EXCEPTION );
		}
	}

	/* Again, the child process has to pass the parameter to the parent and
	   ask it to create the slider */

	if ( Internals.I_am == CHILD )
	{
		char *buffer, *pos;
		long new_ID;
		long *result;
		size_t len;


		if ( ( v = vars_pop( v ) ) != NULL )
		{
			vars_check( v, STR_VAR );
			label = v->val.sptr;

			if ( v->next != NULL )
			{
				vars_check( v->next, STR_VAR );
				help_text = v->next->val.sptr;

				if ( v->next->next != NULL )
					print( WARN, "Too many arguments, discarding superfluous "
						   "arguments.\n" );
			}
		}

		len =   sizeof EDL.Lc + sizeof type
			  + sizeof start_val + sizeof end_val + sizeof step;

		if ( EDL.Fname )
			len += strlen( EDL.Fname ) + 1;
		else
			len++;
		if ( label )
			len += strlen( label ) + 1;
		else
			len++;
		if ( help_text )
			len += strlen( help_text ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &EDL.Lc, sizeof EDL.Lc ); /* store current line number */
		pos += sizeof EDL.Lc;

		memcpy( pos, &type, sizeof type );     /* store slider type */
		pos += sizeof type;

		memcpy( pos, &start_val, sizeof start_val );
		pos += sizeof start_val;

		memcpy( pos, &end_val, sizeof end_val );
		pos += sizeof end_val;

		memcpy( pos, &step, sizeof step );
		pos += sizeof step;

		if ( EDL.Fname )
		{
			strcpy( pos, EDL.Fname );           /* store current file name */
			pos += strlen( EDL.Fname ) + 1;
		}
		else
			*pos++ = '\0';

		if ( label )                            /* store label string */
		{
			strcpy( pos, label );
			pos += strlen( label ) + 1;
		}
		else
			*pos++ = '\0';

		if ( help_text )                        /* store help text string */
		{
			strcpy( pos, help_text );
			pos += strlen( help_text ) + 1;
		}
		else
			*pos++ = '\0';

		/* Ask parent to create the slider - it returns an array width two
		   elements, the first indicating if it was successful, the second
		   being the sliders ID */

		result = exp_screate( buffer, pos - buffer );

		/* Bomb out if parent returns failure */

		if ( result[ 0 ] == 0 )
		{
			T_free( result );
			THROW( EXCEPTION );
		}

		new_ID = result[ 1 ];
		T_free( result );

		return vars_push( INT_VAR, new_ID );
	}

	/* Here follows the part that is run by the parent process only */

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check( v, STR_VAR );

		if ( *v->val.sptr != '\0' )
		{
			TRY
			{
				label = T_strdup( v->val.sptr );
				check_label( label );
				convert_escapes( label );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( label );
				RETHROW( );
			}
		}
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		TRY
		{
			vars_check( v, STR_VAR );
			help_text = T_strdup( v->val.sptr );
			convert_escapes( help_text );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( help_text );
			T_free( label );
			RETHROW( );
		}
	}

	too_many_arguments( v );

	/* Now that we're done with checking the parameters we can create the new
       button - if the Tool_Box doesn't exist yet we've got to create it now */

	TRY
	{
		if ( Tool_Box == NULL )
		{
			Tool_Box = T_malloc( sizeof *Tool_Box );
			Tool_Box->objs = NULL;
			Tool_Box->layout = VERT;
			Tool_Box->Tools = NULL;
		}

		new_io = T_malloc( sizeof *new_io );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( new_io );
		T_free( help_text );
		T_free( label );
		RETHROW( );
	}

	if ( Tool_Box->objs == NULL )
	{
		Tool_Box->objs = new_io;
		new_io->next = new_io->prev = NULL;
	}
	else
	{
		for ( ioi = Tool_Box->objs; ioi->next != NULL; ioi = ioi->next )
			/* empty */ ;
		ID = ioi->ID + 1;
		ioi->next = new_io;
		new_io->prev = ioi;
		new_io->next = NULL;
	}

	new_io->ID = ID;
	new_io->type = ( int ) type;
	new_io->self = NULL;
	new_io->start_val = start_val;
	new_io->end_val = end_val;
	new_io->step = step;
	new_io->value = 0.5 * ( end_val + start_val );
	if ( step != 0.0 )
		new_io->value = lrnd( ( new_io->value - start_val ) / step ) * step
		                + start_val;
	new_io->label = label;
	new_io->help_text = help_text;

	/* Draw the new slider */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, new_io->ID );
}


/*-----------------------------*/
/* Deletes one or more sliders */
/*-----------------------------*/

Var *f_sdelete( Var *v )
{
	IOBJECT *io;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* At least one slider ID is needed... */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Loop over all slider IDs */

	do
	{
		/* Child has no control over the sliders, it has to ask the parent
		   process to delete the button */

		if ( Internals.I_am == CHILD )
		{
			char *buffer, *pos;
			size_t len;
			long ID;


			/* Very basic sanity check */

			ID = get_strict_long( v, "slider ID" );

			if ( ID < 0 )
			{
				print( FATAL, "Invalid slider identifier.\n" );
				THROW( EXCEPTION );
			}

			len = sizeof EDL.Lc + sizeof v->val.lval;

			if ( EDL.Fname )
				len += strlen( EDL.Fname ) + 1;
			else
				len++;

			pos = buffer = T_malloc( len );

			memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( pos, &ID, sizeof ID );          /* slider ID */
			pos += sizeof ID;

			if ( EDL.Fname )
			{
				strcpy( pos, EDL.Fname );        /* current file name */
				pos += strlen( EDL.Fname ) + 1;
			}
			else
				*pos++ = '\0';

			/* Bomb out on failure */

			if ( ! exp_sdelete( buffer, pos - buffer ) )
				THROW( EXCEPTION );
		}
		else
		{
			/* No tool box -> no sliders to delete */

			if ( Tool_Box == NULL || Tool_Box->objs == NULL )
			{
				print( FATAL, "No sliders have been defined yet.\n" );
				THROW( EXCEPTION );
			}

			/* Check that slider with the ID exists */

			io = find_object_from_ID( get_strict_long( v, "slider ID" ) );

			if ( io == NULL ||
				 ( io->type != NORMAL_SLIDER &&
				   io->type != VALUE_SLIDER ) )
			{
				print( FATAL, "Invalid slider identifier.\n" );
				THROW( EXCEPTION );
			}

			/* Remove slider from the linked list */

			if ( io->next != NULL )
				io->next->prev = io->prev;
			if ( io->prev != NULL )
				io->prev->next = io->next;
			else
				Tool_Box->objs = io->next;

			/* Delete the slider object if its drawn */

			if ( Internals.mode != TEST && io->self )
			{
				fl_delete_object( io->self );
				fl_free_object( io->self );
			}

			T_free( ( void * ) io->label );
			T_free( ( void * ) io->help_text );
			T_free( io );

			/* If this was the very last object delete also the form */

			if ( Tool_Box->objs == NULL )
			{
				if ( Internals.mode != TEST )
				{
					if ( Tool_Box->Tools )
					{
						if ( fl_form_is_visible( Tool_Box->Tools ) )
						{
							store_geometry( );
							fl_hide_form( Tool_Box->Tools );
						}

						fl_free_form( Tool_Box->Tools );
					}
					else
						tool_has_been_shown = UNSET;
				}

				Tool_Box = T_free( Tool_Box );

				if ( v->next != NULL )
				{
					print( FATAL, "Invalid slider identifier.\n" );
					THROW( EXCEPTION );
				}

				return vars_push( INT_VAR, 1 );
			}
		}
	} while ( ( v = vars_pop( v ) ) != NULL );

	if ( Internals.I_am == CHILD || Internals.mode == TEST || ! Tool_Box )
		return vars_push( INT_VAR, 1 );

	/* Redraw the tool box without the slider */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------*/
/* Sets or returns the value of a slider */
/*---------------------------------------*/

Var *f_svalue( Var *v )
{
	IOBJECT *io;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need at least the sliders ID */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Again, the child has to pass the arguments to the parent and ask it
	   to set or return the slider value */

	if ( Internals.I_am == CHILD )
	{
		long ID;
		long state = 0;
		double val = 0.0;
		char *buffer, *pos;
		double *res;
		size_t len;


		/* Very basic sanity check... */

		ID = get_strict_long( v, "slider ID" );

		if ( v->val.lval < 0 )
		{
			print( FATAL, "Invalid slider identifier.\n" );
			THROW( EXCEPTION );
		}

		/* Another arguments means that the slider value is to be set */

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			vars_check( v, INT_VAR | FLOAT_VAR );
			state = 1;
			val = VALUE( v );
		}

		too_many_arguments( v );

		len = sizeof EDL.Lc + sizeof ID + sizeof state + sizeof val;

		if ( EDL.Fname )
			len += strlen( EDL.Fname ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &EDL.Lc, sizeof EDL.Lc );     /* current line number */
		pos += sizeof EDL.Lc;

		memcpy( pos, &ID, sizeof ID );             /* sliders ID */
		pos += sizeof ID;

		memcpy( pos, &state, sizeof state );       /* needs slider setting ? */
		pos += sizeof state;

		memcpy( pos, &val, sizeof val );           /* new slider value */
		pos += sizeof val;

		if ( EDL.Fname )
		{
			strcpy( pos, EDL.Fname );              /* current file name */
			pos += strlen( EDL.Fname ) + 1;
		}
		else
			*pos++ = '\0';

		/* Ask parent to set or get the slider value - it will return an array
		   with two doubles, the first indicating if it was successful (when
		   the value is positive) the second is the slider value */

		res = exp_sstate( buffer, pos - buffer );

		/* Bomb out on failure */

		if ( res[ 0 ] < 0.0 )
		{
			T_free( res );
			THROW( EXCEPTION );
		}

		val = res[ 1 ];
		T_free( res );
		return vars_push( FLOAT_VAR, val );
	}

	/* No tool box -> no sliders... */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No slider have been defined yet.\n" );
		THROW( EXCEPTION );
	}

	/* Check that ID is ID of a slider */

	io = find_object_from_ID( get_strict_long( v, "slider ID" ) );

	if ( io == NULL ||
		 ( io->type != NORMAL_SLIDER &&
		   io->type != VALUE_SLIDER ) )
	{
		print( FATAL, "Invalid slider identifier.\n" );
		THROW( EXCEPTION );
	}

	/* If there are no more arguments just return the sliders value */

	if ( ( v = vars_pop( v ) ) == NULL )
		return vars_push( FLOAT_VAR, io->value );

	/* Otherwise check the next argument, i.e. the value to be set - it
	   must be within the sliders range, if it doesn't fit reduce it to
	   the allowed range */

	vars_check( v, INT_VAR | FLOAT_VAR );

	io->value = VALUE( v );

	if ( io->value > io->end_val )
	{
		print( SEVERE, "Slider value too large.\n" );
		io->value = io->end_val;
	}

	if ( io->value < io->start_val )
	{
		print( SEVERE, "Slider value too small.\n" );
		io->value = io->start_val;
	}

	/* If a certain step width is used set the new value to the nearest
	   allowed value */

	if ( io->step != 0.0 )
		io->value = lrnd( ( io->value - io->start_val ) / io->step )
			        * io->step + io->start_val;

	if ( Internals.mode != TEST )
		fl_set_slider_value( io->self, io->value );

	too_many_arguments( v );

	return vars_push( FLOAT_VAR, io->value );
}


/*--------------------------------------*/
/* Creates a new input or output object */
/*--------------------------------------*/

Var *f_icreate( Var *v )
{
	long type;
	char *label = NULL;
	char *help_text = NULL;
	char *form_str = NULL;
	IOBJECT *new_io = NULL, *ioi;
	long ID = 0;
	long lval = 0;
	double dval = 0.0;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* At least the type of the input or output object must be specified */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* First argument must be type of "INT_INPUT", "FLOAT_INPUT",
	   "INT_OUTPUT" or "FLOAT_OUTPUT" or 0, 1, 2, or 3 */

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == INT_VAR || v->type == FLOAT_VAR )
	{
		type = get_strict_long( v, "input or output object type" );

		if ( type != INT_INPUT && type != FLOAT_INPUT &&
			 type != INT_OUTPUT && type != FLOAT_OUTPUT )
		{
			print( FATAL, "Invalid input or output object type (%ld).\n",
				   type );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( ! strcasecmp( v->val.sptr, "INT_INPUT" ) )
			type = INT_INPUT;
		else if ( ! strcasecmp( v->val.sptr, "FLOAT_INPUT" ) )
			type = FLOAT_INPUT;
		else if ( ! strcasecmp( v->val.sptr, "INT_OUTPUT" ) )
			type = INT_OUTPUT;
		else if ( ! strcasecmp( v->val.sptr, "FLOAT_OUTPUT" ) )
			type = FLOAT_OUTPUT;
		else
		{
			print( FATAL, "Unknown input or output object type: '%s'.\n" );
			THROW( EXCEPTION );
		}
	}

	v = vars_pop( v );

	/* Next argument could be a value to be set in the input or output
	   object */

	if ( v != NULL && v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( ( type == INT_INPUT || type == INT_OUTPUT ) &&
			 v->type == FLOAT_VAR )
		{
			print( SEVERE, "Float value used as initial value for new integer "
				   "input or output object.\n" );
			lval = lrnd( v->val.dval );
		}
		else
		{
			if ( type == INT_INPUT || type == INT_OUTPUT )
				lval = v->val.lval;
			else
				dval = VALUE( v );
		}

		v = vars_pop( v );
	}

	/* Since the child process can't use graphics it has to write the
	   parameter into a buffer, pass it to the parent process and ask the
	   parent to create the button */

	if ( Internals.I_am == CHILD )
	{
		char *buffer, *pos;
		long new_ID;
		long *result;
		size_t len;


		/* Next argument is the label string */

		if ( v != NULL )
		{
			vars_check( v, STR_VAR );
			label = v->val.sptr;

			/* Next argument can be a help text */
		
			if ( v->next != NULL )
			{
				vars_check( v->next, STR_VAR );
				help_text = v->next->val.sptr;

				/* Final argument can be a C format string */

				if ( v->next->next != NULL )
				{
					vars_check( v->next->next, STR_VAR );
					if ( type == INT_INPUT || type == INT_OUTPUT )
						print( WARN, "Can't set format string for integer "
							   "data.\n" );
					form_str = v->next->next->val.sptr;

					/* Warn and get rid of superfluous arguments */

					if ( v->next->next->next != NULL )
						print( WARN, "Too many arguments, discarding "
							   "superfluous arguments.\n" );
				}
			}
		}

		/* Calculate length of buffer needed */

		len = sizeof EDL.Lc + sizeof type
			  + ( ( type == INT_INPUT || type == INT_OUTPUT ) ?
				  sizeof lval : sizeof dval );

		if ( EDL.Fname )
			len += strlen( EDL.Fname ) + 1;
		else
			len++;

		if ( label )
			len += strlen( label ) + 1;
		else
			len++;

		if ( help_text )
			len += strlen( help_text ) + 1;
		else
			len++;

		if ( form_str )
			len += strlen( form_str ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &EDL.Lc, sizeof EDL.Lc );     /* current line number */
		pos += sizeof EDL.Lc;

		memcpy( pos, &type, sizeof type );         /* object type */
		pos += sizeof type;

		if ( type == INT_INPUT || type == INT_OUTPUT )
		{
			memcpy( pos, &lval, sizeof lval );
			pos += sizeof lval;
		}
		else
		{
			memcpy( pos, &dval, sizeof dval );
			pos += sizeof dval;
		}

		if ( EDL.Fname )
		{
			strcpy( pos, EDL.Fname );           /* current file name */
			pos += strlen( EDL.Fname ) + 1;
		}
		else
			*pos++ = '\0';

		if ( label )                            /* label string */
		{
			strcpy( pos, label );
			pos += strlen( label ) + 1;
		}
		else
			*pos++ = '\0';

		if ( help_text )                        /* help text string */
		{
			if ( *help_text == '\0' )
			{
				* ( unsigned char * ) pos = 0xff;
				pos += 1;
			}
			else
			{
				strcpy( pos, help_text );
				pos += strlen( help_text ) + 1;
			}
		}
		else
			*pos++ = '\0';

		if ( form_str )
		{
			strcpy( pos, form_str );
			pos += strlen( form_str ) + 1;
		}
		else
			*pos++ = '\0';

		/* Pass buffer to parent and ask it to create the input or input
		   object. It returns a buffer with two longs, the first one
		   indicating success or failure (1 or 0), the second being the
		   objects ID */

		result = exp_icreate( buffer, pos - buffer );

		if ( result[ 0 ] == 0 )      /* failure -> bomb out */
		{
			T_free( result );
			THROW( EXCEPTION );
		}

		new_ID = result[ 1 ];
		T_free( result );           /* free result buffer */

		return vars_push( INT_VAR, new_ID );
	}

	/* All the remaining code is run by the paren process only */

	/* Next argument is the label string */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );

		TRY
		{
			label = T_strdup( v->val.sptr );
			check_label( label );
			convert_escapes( label );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( label );
			RETHROW( );
		}

		v = vars_pop( v );
	}

	/* Next argument can be a help text */

	if ( v != NULL )
	{
		TRY
		{
			vars_check( v, STR_VAR );
			help_text = T_strdup( v->val.sptr );
			convert_escapes( help_text );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( help_text );
			T_free( label );
			RETHROW( );
		}

		v = vars_pop( v );
	}

	/* Final argument can be a C format string */

	if ( v != NULL )
	{
		TRY
		{
			vars_check( v, STR_VAR );
			if ( type == INT_INPUT || type == INT_OUTPUT )
				print( WARN, "Can't set format string for integer data.\n" );
			else
			{
				form_str = T_strdup( v->val.sptr );
				if ( ! check_format_string( form_str ) )
				{
					print( FATAL, "Invalid format string \"%s\".\n",
						   v->val.sptr );
					THROW( EXCEPTION );
				}
			}
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( form_str );
			T_free( help_text );
			T_free( label );
			RETHROW( );
		}
	}

	/* Warn and get rid of superfluous arguments */

	too_many_arguments( v );

	/* Now that we're done with checking the parameters we can create the new
       button - if the Tool_Box doesn't exist yet we've got to create it now */

	TRY
	{
		if ( Tool_Box == NULL )
		{
			Tool_Box = T_malloc( sizeof *Tool_Box );
			Tool_Box->objs = NULL;
			Tool_Box->layout = VERT;
			Tool_Box->Tools = NULL;
		}

		new_io = T_malloc( sizeof *new_io );

		/* We already do the following here while we're in the TRY block... */

		new_io->form_str = NULL;
		if ( type == FLOAT_INPUT || type == FLOAT_OUTPUT )
		{
			if ( form_str )
				new_io->form_str = T_strdup( form_str );
			else
				new_io->form_str = T_strdup( "%g" );
		}

		form_str = T_free( form_str );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( new_io );
		if ( type == FLOAT_INPUT || type == FLOAT_OUTPUT )
			T_free( new_io->form_str );
		T_free( help_text );
		T_free( label );
		RETHROW( );
	}
		
	if ( Tool_Box->objs == NULL )
	{
		Tool_Box->objs = new_io;
		new_io->next = new_io->prev = NULL;
	}
	else
	{
		for ( ioi = Tool_Box->objs; ioi->next != NULL; ioi = ioi->next )
			/* empty */ ;
		ID = ioi->ID + 1;
		ioi->next = new_io;
		new_io->prev = ioi;
		new_io->next = NULL;
	}

	new_io->ID = ID;
	new_io->type = ( int ) type;
	if ( type == INT_INPUT || type == INT_OUTPUT )
		new_io->val.lval = lval;
	else
		new_io->val.dval = dval;
	new_io->self = NULL;
	new_io->group = NULL;
	new_io->label = label;
	new_io->help_text = help_text;

	/* Draw the new object */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, new_io->ID );
}


/*----------------------------------------------*/
/* Deletes one or more input or output objects. */
/* Parameter are one or more object IDs         */
/*----------------------------------------------*/

Var *f_idelete( Var *v )
{
	IOBJECT *io;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need the ID of the button to delete */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Loop over all variables */

	do
	{
		/* Since the object 'belong' to the parent, the child needs to ask
		   the parent to delete the object. The ID of each object to be deleted
		   gets passed to te parent in a buffer and the parent is asked to
		   delete the object */

		if ( Internals.I_am == CHILD )
		{
			char *buffer, *pos;
			size_t len;
			long ID;


			/* Do all possible checking of the parameter */

			ID = get_strict_long( v, "input or output object ID" );
			if ( ID < 0 )
			{
				print( FATAL, "Invalid input or output object identifier.\n" );
				THROW( EXCEPTION );
			}

			/* Get a bufer long enough and write data */

			len = sizeof EDL.Lc + sizeof v->val.lval;

			if ( EDL.Fname )
				len += strlen( EDL.Fname ) + 1;
			else
				len++;

			pos = buffer = T_malloc( len );

			memcpy( pos, &EDL.Lc, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( pos, &ID, sizeof ID );            /* object ID */
			pos += sizeof ID;

			if ( EDL.Fname )
			{
				strcpy( pos, EDL.Fname );             /* current file name */
				pos += strlen( EDL.Fname ) + 1;
			}
			else
				*pos++ = '\0';

			/* Ask parent to delete the object, bomb out on failure */

			if ( ! exp_idelete( buffer, pos - buffer ) )
				THROW( EXCEPTION );
		}
		else
		{
			/* No tool box -> no objects -> no objects to delete... */

			if ( Tool_Box == NULL || Tool_Box->objs == NULL )
			{
				print( FATAL, "No input or output objects have been defined "
					   "yet.\n" );
				THROW( EXCEPTION );
			}

			/* Do checks on parameters */

			io = find_object_from_ID( get_strict_long( v,
											   "input or output object ID" ) );

			if ( io == NULL ||
				 ( io->type != INT_INPUT  && io->type != FLOAT_INPUT &&
				   io->type != INT_OUTPUT && io->type != FLOAT_OUTPUT ) )
			{
				print( FATAL, "Invalid input or output object identifier.\n" );
				THROW( EXCEPTION );
			}

			/* Remove object from the linked list */

			if ( io->next != NULL )
				io->next->prev = io->prev;
			if ( io->prev != NULL )
				io->prev->next = io->next;
			else
				Tool_Box->objs = io->next;

			/* Delete the object (its not drawn in a test run!) */

			if ( Internals.mode != TEST )
			{
				fl_delete_object( io->self );
				fl_free_object( io->self );
			}

			T_free( io->label );
			T_free( io->help_text );
			if ( io->type == FLOAT_INPUT || io->type == FLOAT_OUTPUT )
				T_free( io->form_str );
			T_free( io );

			if ( Tool_Box->objs == NULL )
			{
				if ( Internals.mode != TEST )
				{
					if ( Tool_Box->Tools )
					{
						if ( fl_form_is_visible( Tool_Box->Tools ) )
						{
							store_geometry( );
							fl_hide_form( Tool_Box->Tools );
						}

						fl_free_form( Tool_Box->Tools );
					}
					else
						tool_has_been_shown = UNSET;
				}

				Tool_Box = T_free( Tool_Box );

				if ( v->next != NULL )
				{
					print( FATAL, "Invalid input or output object "
						   "identifier.\n" );
					THROW( EXCEPTION );
				}

				return vars_push( INT_VAR, 1 );
			}
		}
	} while ( ( v = vars_pop( v ) ) != NULL );

	/* The child process is already done here, and in a test run we're also */

	if ( Internals.I_am == CHILD || Internals.mode == TEST || ! Tool_Box )
		return vars_push( INT_VAR, 1 );

	/* Redraw the form without the deleted objects */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------*/
/* Sets or returns the content of an input or output object */
/*----------------------------------------------------------*/

Var *f_ivalue( Var *v )
{
	IOBJECT *io;
	char buf[ MAX_INPUT_CHARS + 1 ];


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need at least the objects ID */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Again, the child has to pass the arguments to the parent and ask it
	   to set or return the objects value */

	if ( Internals.I_am == CHILD )
	{
		long ID;
		long state = 0;
		long lval = 0;
		double dval = 0.0;
		char *buffer, *pos;
		INPUT_RES *input_res;
		size_t len;


		/* Very basic sanity check... */

		ID = get_strict_long( v, "input or output object ID" );

		if ( ID < 0 )
		{
			print( FATAL, "Invalid input or output object identifier.\n" );
			THROW( EXCEPTION );
		}

		/* Another argument means that the objects value is to be set */

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			vars_check( v, INT_VAR | FLOAT_VAR );

			if ( v->type == INT_VAR )
			{
				state = 1;
				lval = v->val.lval;
			}
			else
			{
				state = 2;
				dval = VALUE( v );
			}
		}

		too_many_arguments( v );

		len =   sizeof EDL.Lc + sizeof ID + sizeof state
			  + ( state <= 1 ? sizeof lval : sizeof dval );

		if ( EDL.Fname )
			len += strlen( EDL.Fname ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  /* current line number */
		pos += sizeof EDL.Lc;

		memcpy( pos, &ID, sizeof ID );          /* sliders ID */
		pos += sizeof ID;

		memcpy( pos, &state, sizeof state );    /* needs input setting ? */
		pos += sizeof state;

		if ( state <= 1 )                       /* new object value */
		{
			memcpy( pos, &lval, sizeof lval );
			pos += sizeof lval;
		}
		else
		{
			memcpy( pos, &dval, sizeof dval );
			pos += sizeof dval;
		}

		if ( EDL.Fname )
		{
			strcpy( pos, EDL.Fname );           /* current file name */
			pos += strlen( EDL.Fname ) + 1;
		}
		else
			*pos++ = '\0';

		/* Ask parent to set or get the value - it will return a pointer
		   to an INPUT_RES structure, where the res entry indicates failure
		   (negative value) and the type of the returned value (0 is integer,
		   positive non-zero is float), and the second entry is a union for
		   the return value, i.e. the objects value. */

		input_res = exp_istate( buffer, pos - buffer );

		/* Bomb out on failure */

		if ( input_res->res < 0 )
		{
			T_free( input_res );
			THROW( EXCEPTION );
		}

		state = input_res->res;
		if ( state == 0 )
			lval = input_res->val.lval;
		else
			dval = input_res->val.dval;
		T_free( input_res );

		if ( state == 0 )
			return vars_push( INT_VAR, lval );
		else
			return vars_push( FLOAT_VAR, dval );
	}

	/* No tool box -> no input or output objects... */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No input or output objects have been defined yet.\n" );
		THROW( EXCEPTION );
	}

	/* Check that ID is ID of an input or output object */

	io = find_object_from_ID( get_strict_long( v,
											   "input or output object ID" ) );
	if ( io == NULL ||
		 ( io->type != INT_INPUT  && io->type != FLOAT_INPUT &&
		   io->type != INT_OUTPUT && io->type != FLOAT_OUTPUT ) )
	{
		print( FATAL, "Invalid input or output object identifier.\n" );
		THROW( EXCEPTION );
	}

	/* If there are no more arguments just return the objects value */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( io->type == INT_INPUT || io->type == INT_OUTPUT )
			return vars_push( INT_VAR, io->val.lval );
		else
			return vars_push( FLOAT_VAR, io->val.dval );
	}

	/* Otherwise check the next argument, i.e. the value to be set */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( ( io->type == INT_INPUT || io->type == INT_OUTPUT ) &&
		 v->type == FLOAT_VAR )
	{
		print( SEVERE, "Float number used as integer input or output object "
			   "value.\n" );
		io->val.lval = lrnd( v->val.dval );
	}
	else
	{
		if ( io->type == INT_INPUT || io->type == INT_OUTPUT )
			io->val.lval = v->val.lval;
		else
			io->val.dval = VALUE( v );
	}

	if ( Internals.mode != TEST )
	{
		if ( io->type == INT_INPUT || io->type == INT_OUTPUT )
		{
			snprintf( buf, MAX_INPUT_CHARS + 1, "%ld", io->val.lval );
			fl_set_input( io->self, buf );
		}
		else
		{
			snprintf( buf, MAX_INPUT_CHARS + 1, io->form_str, io->val.dval );
			fl_set_input( io->self, buf );
		}
	}

	too_many_arguments( v );

	if ( io->type == INT_INPUT || io->type == INT_OUTPUT )
		return vars_push( INT_VAR, io->val.lval );
	else
		return vars_push( FLOAT_VAR, io->val.dval );
}


/*---------------------------*/
/* Creates a new menu object */
/*---------------------------*/

Var *f_mcreate( Var *v )
{
	Var *lv;
	long num_strs = 0;
	size_t len = 0;
	IOBJECT *new_io, *ioi;
	long ID = 0;
	long i;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* At least a label and two menu entries must be specified */

	if ( v == NULL || v->next == NULL || v->next->next == NULL)
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	for ( lv = v; lv != NULL; num_strs++, lv = lv->next )
	{
		if ( lv->type != STR_VAR )
		{
			print( FATAL, "All arguments must be strings.\n" );
			THROW( EXCEPTION );
		}

		len += strlen( lv->val.sptr ) + 1;
	}

	if ( Internals.I_am == CHILD )
	{
		char *buffer, *pos;
		long new_ID;
		long *result;
		size_t l;


		len += sizeof EDL.Lc + sizeof num_strs;

		if ( EDL.Fname )
			len += strlen( EDL.Fname ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &EDL.Lc, sizeof EDL.Lc );     /* current line number */
		pos += sizeof EDL.Lc;

		memcpy( pos, &num_strs, sizeof num_strs );
		pos += sizeof num_strs;

		if ( EDL.Fname )
		{
			l = strlen( EDL.Fname ) + 1;
			memcpy( pos, EDL.Fname, l );           /* current file name */
			pos += l;
		}
		else
			*pos++ = '\0';

		for ( ; v != NULL; v = v->next )
		{
			l = strlen( v->val.sptr ) + 1;
			memcpy( pos, v->val.sptr, l );
			pos += l;
		}

		/* Pass buffer to parent and ask it to create the menu  object.
		   It returns a buffer with two longs, the first one indicating
		   success or failure (1 or 0), the second being the new objects ID */

		result = exp_mcreate( buffer, pos - buffer );

		if ( result[ 0 ] == 0 )      /* failure -> bomb out */
		{
			T_free( result );
			THROW( EXCEPTION );
		}

		new_ID = result[ 1 ];
		T_free( result );           /* free result buffer */

		return vars_push( INT_VAR, new_ID );
	}

	/* Now that we're done with checking the parameters we can create the new
       button - if the Tool_Box doesn't exist yet we've got to create it now */

	if ( Tool_Box == NULL )
	{
		Tool_Box = T_malloc( sizeof *Tool_Box );
		Tool_Box->objs = NULL;
		Tool_Box->layout = VERT;
		Tool_Box->Tools = NULL;
	}

	new_io = NULL;

	TRY
	{
		new_io = T_malloc( sizeof *new_io );
		if ( Tool_Box->objs == NULL )
		{
			Tool_Box->objs = new_io;
			new_io->next = new_io->prev = NULL;
		}
		else
		{
			for ( ioi = Tool_Box->objs; ioi->next != NULL; ioi = ioi->next )
				/* empty */ ;
			ID = ioi->ID + 1;
			ioi->next = new_io;
			new_io->prev = ioi;
			new_io->next = NULL;
		}

		new_io->ID = ID;
		new_io->type = MENU;
		new_io->self = NULL;
		new_io->state = 1;

		new_io->label = NULL;
		new_io->menu_items = NULL;

		new_io->label = T_strdup( v->val.sptr );
		v = vars_pop( v );

		check_label( new_io->label );
		convert_escapes( new_io->label );

		new_io->help_text = NULL;

		new_io->num_items = num_strs - 1;
		new_io->menu_items = T_malloc( new_io->num_items
									   * sizeof *new_io->menu_items );

		for  ( i = 0; i < new_io->num_items; i++ )
			new_io->menu_items[ i ] = NULL;
		for ( i = 0; v != NULL && i < new_io->num_items;
			  i++, v = vars_pop( v ) )
			new_io->menu_items[ i ] = T_strdup( v->val.sptr );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( new_io != NULL )
		{
			T_free( new_io->label );
			if ( new_io->menu_items != NULL )
			{
				for ( i = 0; i < new_io->num_items; i++ )
					T_free( new_io->menu_items[ i ] );
				T_free( new_io->menu_items );
			}

			if( Tool_Box->objs == new_io )
				Tool_Box->objs = NULL;
			else
				ioi->next = NULL;

			T_free( new_io );
		}

		RETHROW( );
	}

	/* Draw the new menu */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, new_io->ID );
}


/*----------------------------------*/
/* Deletes one or more menu objects */
/*----------------------------------*/

Var *f_mdelete( Var *v )
{
	IOBJECT *io;
	long i;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* At least one menu ID is needed... */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Loop over all slider IDs */

	do
	{
		/* Child has no control over the sliders, it has to ask the parent
		   process to delete the button */

		if ( Internals.I_am == CHILD )
		{
			char *buffer, *pos;
			size_t len;
			long ID;


			/* Very basic sanity check */

			ID = get_strict_long( v, "menu ID" );

			if ( ID < 0 )
			{
				print( FATAL, "Invalid slider identifier.\n" );
				THROW( EXCEPTION );
			}

			len = sizeof EDL.Lc + sizeof v->val.lval;

			if ( EDL.Fname )
				len += strlen( EDL.Fname ) + 1;
			else
				len++;

			pos = buffer = T_malloc( len );

			memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( pos, &ID, sizeof ID );          /* menu ID */
			pos += sizeof ID;

			if ( EDL.Fname )
			{
				strcpy( pos, EDL.Fname );            /* current file name */
				pos += strlen( EDL.Fname ) + 1;
			}
			else
				*pos++ = '\0';

			/* Bomb out on failure */

			if ( ! exp_mdelete( buffer, pos - buffer ) )
				THROW( EXCEPTION );
		}
		else
		{
			/* No tool box -> no menu to delete */

			if ( Tool_Box == NULL || Tool_Box->objs == NULL )
			{
				print( FATAL, "No menus have been defined yet.\n" );
				THROW( EXCEPTION );
			}

			/* Check that menu with the ID exists */

			io = find_object_from_ID( get_strict_long( v, "menu ID" ) );

			if ( io == NULL || io->type != MENU )
			{
				print( FATAL, "Invalid menu identifier.\n" );
				THROW( EXCEPTION );
			}

			/* Remove menu from the linked list */

			if ( io->next != NULL )
				io->next->prev = io->prev;
			if ( io->prev != NULL )
				io->prev->next = io->next;
			else
				Tool_Box->objs = io->next;

			/* Delete the menu object if its drawn */

			if ( Internals.mode != TEST && io->self )
			{
				fl_delete_object( io->self );
				fl_free_object( io->self );
			}

			T_free( ( void * ) io->label );
			for ( i = 0; i < io->num_items; i++ )
				T_free( ( void * ) io->menu_items[ i ] );
			T_free( io->menu_items );
			T_free( io );

			/* If this was the very last object delete also the form */

			if ( Tool_Box->objs == NULL )
			{
				if ( Internals.mode != TEST )
				{
					if ( Tool_Box->Tools )
					{
						if ( fl_form_is_visible( Tool_Box->Tools ) )
						{
							store_geometry( );
							fl_hide_form( Tool_Box->Tools );
						}

						fl_free_form( Tool_Box->Tools );
					}
					else
						tool_has_been_shown = UNSET;
				}

				Tool_Box = T_free( Tool_Box );

				if ( ( v = vars_pop( v ) ) != NULL )
				{
					print( FATAL, "Invalid menu identifier.\n" );
					THROW( EXCEPTION );
				}

				return vars_push( INT_VAR, 1 );
			}
		}
	} while ( ( v = vars_pop( v ) ) != NULL );

	if ( Internals.I_am == CHILD || Internals.mode == TEST || ! Tool_Box )
		return vars_push( INT_VAR, 1 );

	/* Redraw the tool box without the menu */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/* Sets or returns the selected item in a menu object */
/*----------------------------------------------------*/

Var *f_mchoice( Var *v )
{
	IOBJECT *io;
	long select_item = 0;


	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need at least the ID of the menu */

	if ( v == NULL )
	{
		print( FATAL, "Missing argumnts.\n" );
		THROW( EXCEPTION );
	}

	/* Again, the child doesn't know about the menu, so it got to ask the
	   parent process */

	if ( Internals.I_am == CHILD )
	{
		long ID;
		char *buffer, *pos;
		size_t len;
		long *result;


		/* Basic check of menu identifier - always the first parameter */

		ID = get_strict_long( v, "menu ID" );

		if ( ID < 0 )
		{
			print( FATAL, "Invalid menu identifier.\n" );
			THROW( EXCEPTION );
		}

		/* If there's a second parameter it's the item in the menu to set */

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			select_item = get_strict_long( v, "menu item number" );

			/* All menu item numbers must be larger than 0, but since we're
			   already running the experiment we don't bomb out but return the
			   currently selected item instead */

			if ( select_item <= 0 )
			{
				print( SEVERE, "Invalid menu item number %ld.\n",
					   select_item );
				select_item = 0;
			}
		}

		/* No more parameters accepted... */

		too_many_arguments( v );

		/* Make up buffer to send to parent process */

		len = sizeof EDL.Lc + sizeof ID + sizeof select_item;

		if ( EDL.Fname )
			len += strlen( EDL.Fname ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  /* current line number */
		pos += sizeof EDL.Lc;

		memcpy( pos, &ID, sizeof ID );          /* buttons ID */
		pos += sizeof ID;

		memcpy( pos, &select_item, sizeof select_item );
		                                        /* menu item to be set (or */
		pos += sizeof select_item;              /* 0 if not to be set)     */

		if ( EDL.Fname )
		{
			strcpy( pos, EDL.Fname );           /* current file name */
			pos += strlen( EDL.Fname ) + 1;
		}
		else
			*pos++ = '\0';

		/* Ask parent process to return or set the menu item - bomb out if it
		   returns a non-positive value, indicating a severe error */

		result = exp_mchoice( buffer, pos - buffer );

		if ( result[ 0 ] == 0 )      /* failure -> bomb out */
		{
			T_free( result );
			THROW( EXCEPTION );
		}

		select_item = result[ 1 ];
		T_free( result );           /* free result buffer */

		return vars_push( INT_VAR, select_item );
	}

	/* No tool box -> no menus -> no menu item to set or get... */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No menus have been defined yet.\n" );
		THROW( EXCEPTION );
	}

	/* Check the button ID parameter */

	io = find_object_from_ID( get_strict_long( v, "menu ID" ) );

	if ( io == NULL || io->type != MENU )
	{
		print( FATAL, "Invalid menu identifier.\n" );
		THROW( EXCEPTION );
	}

	/* If there's no second parameter just return the currently selected menu
	   item */

	if ( ( v = vars_pop( v ) ) == NULL )
		return vars_push( INT_VAR, io->state );

	/* The optional second argument is the menu item to be set */

	select_item = get_strict_long( v, "menu item number" );

	/* A number less than 1 can only happen during the testing stage and
	   we bomb out */

	if ( select_item <= 0 )
	{
		print( FATAL, "Invalid menu item number %ld.\n", select_item );
		THROW( EXCEPTION );
	}

	/* If the menu item number is larger than the total mumber of items
	   we better bomb out (during the experiment we deal with this by just
	   returning the currently selected choice) */

	if ( select_item > io->num_items )
	{
		if ( Internals.mode == TEST )
		{
			print( FATAL, "Invalid menu item number %ld, there are only %ld "
				   "items.\n", io->num_items );
			THROW( EXCEPTION );
		}

		print( SEVERE, "Invalid menu item number %ld, there are only %ld "
			   "items.\n", io->num_items );
		return vars_push( INT_VAR, io->state );
	}

	/* If this isn't a test run set the new menu item */

	io->state = select_item;
	if ( Internals.mode != TEST )
		fl_set_choice( io->self, select_item );

	too_many_arguments( v );

	return vars_push( INT_VAR, select_item );
}


/*--------------------------------------------------------------------*/
/* Deletes one or more objects, parameter are one or more object IDs. */
/*--------------------------------------------------------------------*/

Var *f_objdel( Var *v )
{
	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need the ID of the button to delete */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Loop over all object numbers */

	do
	{
		/* Since the object 'belong' to the parent, the child needs to ask
		   the parent to delete the object. The ID of each object to be deleted
		   gets passed to the parent in a buffer and the parent is asked to
		   delete the object */

		if ( Internals.I_am == CHILD )
		{
			char *buffer, *pos;
			size_t len;
			long ID;


			/* Do all possible checking of the parameter */

			ID = get_strict_long( v, "object ID" );

			if ( ID < 0 )
			{
				print( FATAL, "Invalid object identifier.\n" );
				THROW( EXCEPTION );
			}

			/* Get a bufer long enough and write data */

			len = sizeof EDL.Lc + sizeof v->val.lval;

			if ( EDL.Fname )
				len += strlen( EDL.Fname ) + 1;
			else
				len++;

			pos = buffer = T_malloc( len );

			memcpy( pos, &EDL.Lc, sizeof EDL.Lc );    /* current line number */
			pos += sizeof EDL.Lc;

			memcpy( pos, &ID, sizeof ID );            /* object ID */
			pos += sizeof ID;

			if ( EDL.Fname )
			{
				strcpy( pos, EDL.Fname );             /* current file name */
				pos += strlen( EDL.Fname ) + 1;
			}
			else
				*pos++ = '\0';

			/* Ask parent to delete the object, bomb out on failure */

			if ( ! exp_objdel( buffer, pos - buffer ) )
				THROW( EXCEPTION );
		}
		else
		{
			IOBJECT *io = NULL;


			/* No tool box -> no objects -> no objects to delete... */

			if ( Tool_Box == NULL || Tool_Box->objs == NULL )
			{
				print( FATAL, "No objects have been defined yet.\n" );
				THROW( EXCEPTION );
			}

			/* Do checks on parameters */

			io = find_object_from_ID(  get_strict_long( v, "object ID" ) );

			if ( io == NULL )
			{
				print( FATAL, "Invalid object identifier.\n" );
				THROW( EXCEPTION );
			}

			switch ( io->type )
			{
				case NORMAL_BUTTON :
				case PUSH_BUTTON :
				case RADIO_BUTTON :
					vars_pop( f_bdelete( vars_push( INT_VAR, v->val.lval ) ) );
					break;

				case NORMAL_SLIDER :
				case VALUE_SLIDER :
					vars_pop( f_sdelete( vars_push( INT_VAR, v->val.lval ) ) );
					break;

				case INT_INPUT :
				case FLOAT_INPUT :
				case INT_OUTPUT :
				case FLOAT_OUTPUT :
					vars_pop( f_idelete( vars_push( INT_VAR, v->val.lval ) ) );
					break;

				case MENU:
					vars_pop( f_mdelete( vars_push( INT_VAR, v->val.lval ) ) );
					break;

				default :
					eprint( FATAL, UNSET, "Internal error at %s:%u.\n",
							__FILE__, __LINE__ );
					THROW( EXCEPTION );
			}
		}
	} while ( ( v = vars_pop( v ) ) != NULL );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------------------*/
/* Returns a pointer to an object given its number or NULL if not found */
/*----------------------------------------------------------------------*/

static IOBJECT *find_object_from_ID( long ID )
{
	IOBJECT *io;


	if ( Tool_Box == NULL )            /* no objects defined yet ? */
		return NULL;

	if ( ID < 0 )                       /* all IDs are >= 0 */
		return NULL;

	/* Loop through linked list to find the object */

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		if ( io->ID == ID )
			break;

	return io;
}


/*---------------------------------------------------------------*/
/* Removes all buttons and sliders and the window they belong to */
/*---------------------------------------------------------------*/

void tools_clear( void )
{
	IOBJECT *io, *next;
	long i;


	is_frozen = UNSET;
	needs_pos = SET;
	tool_has_been_shown = UNSET;


	if ( Tool_Box == NULL )
		return;

	if ( Tool_Box->Tools )
	{
		if ( fl_form_is_visible( Tool_Box->Tools ) )
		{
			store_geometry( );
			fl_hide_form( Tool_Box->Tools );
		}
	}

	for ( io = Tool_Box->objs; io != NULL; io = next )
	{
		next = io->next;

		if ( Tool_Box->Tools && io->self )
		{
			fl_delete_object( io->self );
			fl_free_object( io->self );
		}

		T_free( io->label );
		T_free( io->help_text );

		if ( io->type == FLOAT_INPUT || io->type == FLOAT_OUTPUT )
			T_free( io->form_str );

		if ( io->type == MENU && io->menu_items != NULL )
		{
			for ( i = 0; i < io->num_items; i++ )
				T_free( io->menu_items[ i ] );
			T_free( io->menu_items );
		}

		T_free( io );
	}

	if ( Tool_Box->Tools )
		fl_free_form( Tool_Box->Tools );
	else
		tool_has_been_shown = UNSET;

	Tool_Box = T_free( Tool_Box );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void recreate_Tool_Box( void )
{
	IOBJECT *io, *last_io = NULL;
	int flags;
	unsigned int tool_w, tool_h;


	if ( Internals.mode == TEST )        /* no drawing in test mode */
		return;

	/* If the tool box already exists we've got to find out its position
	   and then delete all the objects */

	if ( Tool_Box->Tools != NULL )
	{
		if ( fl_form_is_visible( Tool_Box->Tools ) )
		{
			store_geometry( );
			fl_hide_form( Tool_Box->Tools );
		}

		for ( io = Tool_Box->objs; io != NULL; io = io->next )
		{
			if ( io->self )
			{
				fl_delete_object( io->self );
				fl_free_object( io->self );
				io->self = NULL;
			}

			io->group = NULL;
		}

		fl_free_form( Tool_Box->Tools );

		needs_pos = SET && ! is_frozen;
		Tool_Box->Tools = fl_bgn_form( FL_UP_BOX, 1, 1 );
	}
	else
	{
		needs_pos = UNSET;
		Tool_Box->Tools = fl_bgn_form( FL_UP_BOX, 1, 1 );

		if ( ! tool_has_been_shown &&
			 * ( char * ) xresources[ TOOLGEOMETRY ].var != '\0' )
		{
			flags = XParseGeometry( ( char * ) xresources[ TOOLGEOMETRY ].var,
									&tool_x, &tool_y, &tool_w, &tool_h );

			if ( XValue & flags && YValue & flags )
			{
				tool_x += GUI.border_offset_x;
				tool_y += GUI.border_offset_y;
				needs_pos = SET;
			}
		}
	}

	if ( tool_has_been_shown )
		needs_pos = SET;

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
	{
		append_object_to_form( io );
		last_io = io;
	}

	fl_end_form( );

	Tool_Box->w = last_io->x + FI_sizes.OBJ_WIDTH + FI_sizes.OFFSET_X0;
	Tool_Box->h = last_io->y + FI_sizes.OBJ_HEIGHT + FI_sizes.OFFSET_Y0;

	if ( last_io->type == MENU )
		Tool_Box->h += 10;

	fl_set_form_size( Tool_Box->Tools, Tool_Box->w, Tool_Box->h );
	fl_adjust_form_size( Tool_Box->Tools );

	/* The following  loop is be needed to get around what looks like bugs
	   in XForms... */

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
	{
		fl_set_object_size( io->self, io->w, io->h );
		if ( ( io->type == INT_INPUT   ||
			   io->type == FLOAT_INPUT ||
			   io->type == INT_OUTPUT  ||
			   io->type == FLOAT_OUTPUT   ) &&
			 io->label != NULL )
			fl_set_object_label( io->self, io->label );
	}

	if ( ! is_frozen )
	{
		if ( needs_pos )
		{
			fl_set_form_position( Tool_Box->Tools, tool_x, tool_y );
			fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
						  FL_FULLBORDER, "fsc2: Tools" );
		}
		else
		{
			fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
						  FL_FULLBORDER, "fsc2: Tools" );
			tool_x = Tool_Box->Tools->x;
			tool_y = Tool_Box->Tools->y;
		}

		tool_has_been_shown = SET;
		fl_winminsize( Tool_Box->Tools->window,
					   FI_sizes.WIN_MIN_WIDTH, FI_sizes.WIN_MIN_HEIGHT );
	}
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static FL_OBJECT *append_object_to_form( IOBJECT *io )
{
	double prec;
	char buf[ MAX_INPUT_CHARS + 1 ];
	IOBJECT *nio;
	long i;


	/* Calculate the width and height of the new object */

	if ( io->prev == NULL )
	{
		io->x = FI_sizes.OFFSET_X0 + ( io->type == NORMAL_BUTTON ?
									   FI_sizes.NORMAL_BUTTON_DELTA : 0 );
		io->y = FI_sizes.OFFSET_Y0;
	}
	else
	{
		if ( Tool_Box->layout == VERT )
		{
			io->x = FI_sizes.OFFSET_X0 + ( io->type == NORMAL_BUTTON ?
										   FI_sizes.NORMAL_BUTTON_DELTA : 0 );
			io->y = io->prev->y + io->prev->h + FI_sizes.VERT_OFFSET +
				( io->prev->type == NORMAL_SLIDER ||
				  io->prev->type == VALUE_SLIDER  ||
				  io->prev->type == INT_INPUT     ||
				  io->prev->type == FLOAT_INPUT   ||
				  io->prev->type == INT_OUTPUT    ||
				  io->prev->type == FLOAT_OUTPUT  ||
				  io->prev->type == MENU ?
				  FI_sizes.LABEL_VERT_OFFSET : 0 );
		}
		else
		{
			io->x = io->prev->x + io->prev->w + FI_sizes.HORI_OFFSET
				- ( io->type == NORMAL_BUTTON ?
					FI_sizes.NORMAL_BUTTON_DELTA : 0 )
				- ( io->prev->type == NORMAL_BUTTON ?
					FI_sizes.NORMAL_BUTTON_DELTA : 0 );
			io->y = FI_sizes.OFFSET_Y0;
		}
	}

	switch ( io->type )
	{
		case NORMAL_BUTTON :
			io->w = FI_sizes.BUTTON_WIDTH - 2 * FI_sizes.NORMAL_BUTTON_DELTA;
			io->h = FI_sizes.BUTTON_HEIGHT;
			io->self = fl_add_button( FL_NORMAL_BUTTON, io->x, io->y,
									  io->w, io->h, io->label );
			fl_set_object_color( io->self, FL_MCOL, FL_GREEN );
			break;

		case PUSH_BUTTON :
			io->w = FI_sizes.BUTTON_WIDTH;
			io->h = FI_sizes.BUTTON_HEIGHT;
			io->self = fl_add_checkbutton( FL_PUSH_BUTTON, io->x, io->y,
										   io->w, io->h, io->label );
			fl_set_object_color( io->self, FL_MCOL, FL_YELLOW );
			fl_set_button( io->self, io->state ? 1 : 0 );
			break;

		case RADIO_BUTTON :
			io->w = FI_sizes.BUTTON_WIDTH;
			io->h = FI_sizes.BUTTON_HEIGHT;

			if ( io->group != NULL )
				fl_addto_group( io->group );
			else
			{
				io->group = fl_bgn_group( );
				for ( nio = io->next; nio != NULL; nio = nio->next )
					if ( nio->partner == io->ID )
						nio->group = io->group;
			}

			io->self = fl_add_round3dbutton( FL_RADIO_BUTTON, io->x, io->y,
											 io->w, io->h, io->label );
			fl_end_group( );

			fl_set_object_color( io->self, FL_MCOL, FL_RED );
			fl_set_button( io->self, io->state ? 1 : 0 );
			break;

		case NORMAL_SLIDER :
			io->w = FI_sizes.SLIDER_WIDTH;
			io->h = FI_sizes.SLIDER_HEIGHT;
			io->self = fl_add_slider( FL_HOR_BROWSER_SLIDER, io->x, io->y,
									  io->w, io->h, io->label );
			fl_set_slider_bounds( io->self, io->start_val, io->end_val );
			fl_set_slider_value( io->self, io->value );
			fl_set_slider_return( io->self, FL_RETURN_END );
			fl_set_object_lsize( io->self, xcntl.sliderFontSize );
			if ( io->step != 0.0 )
				fl_set_slider_step( io->self, io->step );
			else
				fl_set_slider_step( io->self,
								 fabs( io->end_val - io->start_val ) / 200.0 );
			break;

		case VALUE_SLIDER :
			io->w = FI_sizes.SLIDER_WIDTH;
			io->h = FI_sizes.SLIDER_HEIGHT;
			io->self = fl_add_valslider( FL_HOR_BROWSER_SLIDER, io->x, io->y,
										 io->w, io->h, io->label );
			fl_set_slider_bounds( io->self, io->start_val, io->end_val );
			fl_set_slider_value( io->self, io->value );
			fl_set_slider_return( io->self, FL_RETURN_END );
			prec = - floor( log10 ( ( io->end_val - io->start_val ) /
									( 0.825 * io->w ) ) );
			fl_set_slider_precision( io->self,
									 prec <= 0.0 ? 0 : ( int ) lrnd( prec ) );
			if ( io->step != 0.0 )
				fl_set_slider_step( io->self, io->step );
			else
				fl_set_slider_step( io->self,
								 fabs( io->end_val - io->start_val ) / 200.0 );
			fl_set_object_lsize( io->self, xcntl.sliderFontSize );
			break;

		case INT_INPUT :
			io->w = FI_sizes.INPUT_WIDTH;
			io->h = FI_sizes.INPUT_HEIGHT;
			io->self = fl_add_input( FL_INT_INPUT, io->x, io->y, io->w, io->h,
									 NULL );
			fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );
			fl_set_input_return( io->self, FL_RETURN_END_CHANGED );
			fl_set_input_maxchars( io->self, MAX_INPUT_CHARS );
			snprintf( buf, MAX_INPUT_CHARS + 1, "%ld", io->val.lval );
			fl_set_input( io->self, buf );
			break;

		case FLOAT_INPUT :
			io->w = FI_sizes.INPUT_WIDTH;
			io->h = FI_sizes.INPUT_HEIGHT;
			io->self = fl_add_input( FL_FLOAT_INPUT, io->x, io->y,
									 io->w, io->h, NULL );
			fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );
			fl_set_input_return( io->self, FL_RETURN_END_CHANGED );
			fl_set_input_maxchars( io->self, MAX_INPUT_CHARS );
			snprintf( buf, MAX_INPUT_CHARS, io->form_str, io->val.dval );
			fl_set_input( io->self, buf );
			break;

		case INT_OUTPUT :
			io->w = FI_sizes.INPUT_WIDTH;
			io->h = FI_sizes.INPUT_HEIGHT;
			io->self = fl_add_input( FL_INT_INPUT, io->x, io->y, io->w, io->h,
									 NULL );
			fl_set_object_boxtype( io->self, FL_EMBOSSED_BOX );
			fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );
			fl_set_input_return( io->self, FL_RETURN_ALWAYS );
			fl_set_input_maxchars( io->self, MAX_INPUT_CHARS );
			fl_set_object_color( io->self, FL_COL1, FL_COL1 );
			fl_set_input_color( io->self, FL_BLACK, FL_COL1 );
			snprintf( buf, MAX_INPUT_CHARS + 1, "%ld", io->val.lval );
			fl_set_input( io->self, buf );
			break;

		case FLOAT_OUTPUT :
			io->w = FI_sizes.INPUT_WIDTH;
			io->h = FI_sizes.INPUT_HEIGHT;
			io->self = fl_add_input( FL_FLOAT_INPUT, io->x, io->y,
									 io->w, io->h, NULL );
			fl_set_object_boxtype( io->self, FL_EMBOSSED_BOX );
			fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );
			fl_set_input_return( io->self, FL_RETURN_ALWAYS );
			fl_set_input_maxchars( io->self, MAX_INPUT_CHARS );
			fl_set_object_color( io->self, FL_COL1, FL_COL1 );
			fl_set_input_color( io->self, FL_BLACK, FL_COL1 );
			snprintf( buf, MAX_INPUT_CHARS, io->form_str, io->val.dval );
			fl_set_input( io->self, buf );
			break;

		case MENU :
			io->w = FI_sizes.INPUT_WIDTH;
			io->h = FI_sizes.BUTTON_HEIGHT;
			io->self = fl_add_choice( FL_NORMAL_CHOICE2, io->x, io->y,
									  io->w, io->h, io->label );
			fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );
			for ( i = 0; i < io->num_items; i++ )
				fl_addto_choice( io->self, io->menu_items[ i ] );
			fl_set_choice( io->self, io->state );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	fl_set_object_resize( io->self, FL_RESIZE_X );
	fl_set_object_gravity( io->self, FL_NorthWest, FL_NoGravity );
	fl_set_object_callback( io->self, tools_callback, 0 );
	if ( io->help_text != NULL && *io->help_text != '\0' )
		fl_set_object_helper( io->self, io->help_text );

	return io->self;
}


/*----------------------------------------------------------*/
/* Callback function for all the objects in the tool box.   */
/* The states or values are stored in the object structures */
/* to be used by the functions button_state() and           */
/* slider_value()                                           */
/*----------------------------------------------------------*/

static void tools_callback( FL_OBJECT *obj, long data )
{
	IOBJECT *io, *oio;
	long lval;
	double dval;
	const char *buf;
	char obuf[ MAX_INPUT_CHARS + 1 ];


	data = data;

	/* Find out which button or slider got changed */

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		if ( io->self == obj )
			break;

	fsc2_assert( io != NULL );            /* this can never happen :) */

	switch ( io->type )
	{
		case NORMAL_BUTTON :
			io->state += 1;
			break;

		case PUSH_BUTTON :
			io->state = fl_get_button( obj );
			break;

		case RADIO_BUTTON :
			io->state = fl_get_button( obj );
			for ( oio = Tool_Box->objs; oio != NULL; oio = oio->next )
			{
				if ( oio == io || oio->type != RADIO_BUTTON ||
					 oio->group != io->group || io->state == 0 )
					continue;
				oio->state = 0;
			}
			break;

		case NORMAL_SLIDER : case VALUE_SLIDER :
			io->value = fl_get_slider_value( obj );
			break;

		case INT_INPUT :
			buf = fl_get_input( obj );

			if ( *buf == '\0' )
				lval = 0;
			else
				sscanf( buf, "%ld", &lval );

			snprintf( obuf, MAX_INPUT_CHARS + 1, "%ld", lval );

			if ( strcmp( buf, obuf ) )
			{
				fl_set_input( io->self, buf );
				break;
			}

			if ( lval != io->val.lval )
				io->val.lval = lval;
			fl_set_input( io->self, obuf );
			break;

		case FLOAT_INPUT :
			buf = fl_get_input( obj );

			if ( *buf == '\0' )
				dval = 0;
			else
				sscanf( buf, "%lf", &dval );

/* If macro isfinite() isn't defined we try the older BSD version */

#if defined( isfinite )
			if ( ! isfinite( dval ) )
			{
				fl_set_input( io->self, buf );
				break;
			}
#elif defined( finite )
			if ( ! finite( dval ) )
			{
				fl_set_input( io->self, buf );
				break;
			}
#endif

			snprintf( obuf, MAX_INPUT_CHARS + 1, io->form_str, dval );
			fl_set_input( io->self, obuf );

			if ( dval != io->val.dval )
				io->val.dval = dval;
			break;

		case INT_OUTPUT :
			snprintf( obuf, MAX_INPUT_CHARS + 1, "%ld", io->val.lval );
			fl_set_input( io->self, obuf );
			break;

		case FLOAT_OUTPUT :
			snprintf( obuf, MAX_INPUT_CHARS + 1, io->form_str, io->val.dval );
			fl_set_input( io->self, obuf );
			break;

		case MENU :
			io->state = fl_get_choice( io->self );
			break;

		default :                 /* this can never happen :) */
			fsc2_assert( 1 == 0 );
			break;
	}
}


/*------------------------------------------------------*/
/*------------------------------------------------------*/

static bool check_format_string( char *buf )
{
	const char *bp = buf;
	const char *lcp;


	if ( *bp != '%' )     /* format must start with '%' */
		return FAIL;

	lcp = bp + strlen( bp ) - 1;      /* last char must be g, f or e */
	if ( *lcp != 'g' && *lcp != 'G' &&
		 *lcp != 'f' && *lcp != 'F' &&
		 *lcp != 'e' && *lcp != 'E' )
		return FAIL;


	if ( *++bp != '.' )                 /* test for width parameter */
		while ( isdigit( *bp ) )
			bp++;

	if ( bp == lcp )                    /* no precision ? */
		return OK;

	if ( *bp++ != '.' )
		return FAIL;

	if ( bp == lcp )
		return OK;

	while ( isdigit( *bp ) )
		bp++;

	return lcp == bp ? OK : FAIL;
}


/*------------------------------------------------------*/
/* Function to convert character sequences in the label */
/* help text strings that are escape sequences into the */
/* corresponding ASCII characters.                      */
/*------------------------------------------------------*/

static void convert_escapes( char *str )
{
	char *ptr = str;


	while ( ( ptr = strchr( ptr, '\\' ) ) != NULL )
	{
		switch ( * ( ptr + 1 ) )
		{
			case '\\' :
				memcpy( ptr + 1, ptr + 2, strlen( ptr + 2 ) + 1 );
				break;

			case 'n' :
				memcpy( ptr + 1, ptr + 2, strlen( ptr + 2 ) + 1 );
				*ptr = '\n';
				break;
		}

		ptr++;
	}
}


/*-----------------------------------------------------------------*/
/* Labels strings may start with a '@' character to display one of */
/* several symbols instead of a text. The following function tests */
/* if the layout of these special label strings complies with the  */
/* requirements given in the XForms manual.                        */
/*-----------------------------------------------------------------*/

static void check_label( char *str )
{
	const char *sym[ ] = { "->", "<-", ">", "<", ">>", "<<", "<->", "->|",
						   ">|", "|>", "-->", "=", "arrow", "returnarrow",
						   "square", "circle", "line", "plus", "UpLine",
						   "DnLine", "UpArrrow", "DnArrow" };
	char *p = str;
	unsigned int i;
	bool is_ar = UNSET;


	/* If the label string does not start with a '@' character or with two of
	   them nothing further needs to be checked. */

	if ( *p != '@' || ( *p == '@' && *( p + 1 ) == '@' ) )
		return;

	/* If given the first part is either the '#' character (to guarantee a
	   fixed aspect ratio) or the size change (either '+' or '-', followed
	   by a single digit). */

	p++;
	if ( *p == '#' )
	{
		is_ar = SET;
		p++;
	}

	if ( *p == '+' )
	{
		if ( ! isdigit( *( p + 1 ) ) )
			goto bad_label_string;
		else
			p += 2;

		if ( *p == '#' )
		{
			if ( ! is_ar )
				p++;
			else
				goto bad_label_string;
		}
	}
	else if ( *p == '-' )
	{
		if ( ! isdigit( *( p + 1 ) ) &&
			 *( p + 1 ) != '>' && *( p + 1 ) != '-' )
			goto bad_label_string;

		if ( isdigit( *( p + 1 ) ) )
			p += 2;

		if ( *p == '#' )
		{
			if ( ! is_ar )
				p++;
			else
				goto bad_label_string;
		}
	}

	/* Next thing is the orientation, either a number from the range between
	   1 and 9 with the exception of 5, or an angle, starting with 0 and
	   followed by exactly three digits. */

	if ( isdigit( *p ) )
	{
		if ( *p == '5' )
			goto bad_label_string;
		else if ( *p == '0' )
		{
			if ( ! isdigit( *( p + 1 ) ) || ! isdigit( *( p + 2 ) ) ||
				 ! isdigit( *( p + 3 ) ) )
				goto bad_label_string;
			p += 4;
		}
		else
			p++;
	}

	/* The remaining must be one of the allowed strings as defined by the
	   aray 'sym'. */

	for ( i = 0; i < sizeof sym / sizeof sym[ 0 ]; i++ )
		if ( ! strcmp( p, sym[ i ] ) )
			return;

  bad_label_string:

	print( FATAL, "Invalid label string.\n" );
	THROW( EXCEPTION );
}


/*----------------------------------------------------*/
/* Another function for dealing with an XForms bug... */
/*----------------------------------------------------*/

static void store_geometry( void )
{
	if ( tool_x != Tool_Box->Tools->x - 1 &&
		 tool_y != Tool_Box->Tools->y - 1 )
	{
		tool_x = Tool_Box->Tools->x;
		tool_y = Tool_Box->Tools->y;
	}
	else
	{
		tool_x = Tool_Box->Tools->x - 1;
		tool_y = Tool_Box->Tools->y - 1;
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
