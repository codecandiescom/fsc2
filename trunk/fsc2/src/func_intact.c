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


TOOL_BOX *Tool_Box = NULL;

struct {
	int	VERT_OFFSET;
	int	HORI_OFFSET;

	int NORMAL_BUTTON_WIDTH;

	int NORMAL_BUTTON_ADD_X;
	int NORMAL_BUTTON_ADD_Y;

	int MENUE_WIDTH;
	int MENUE_ADD_X;
	int MENUE_ADD_Y;

	int PUSH_BUTTON_SIZE;
	int RADIO_BUTTON_SIZE;

	int	SLIDER_HEIGHT;
	int	SLIDER_WIDTH;

	int IN_OUT_WIDTH;
	int IN_OUT_HEIGHT;

	int SYMBOL_SIZE_IN;
} FI_sizes;


extern FL_resource xresources[ ];
extern FL_IOPT xcntl;
static int tool_x, tool_y;
static bool is_frozen = UNSET;
static bool needs_pos = SET;


static Var *f_layout_child( long layout );
static void f_objdel_child( Var *v );
static void f_objdel_parent( Var *v );
static Var *f_obj_clabel_child( long ID, char *label );
static Var *f_obj_xable_child( long ID, long state );
static int tool_box_close_handler( FL_FORM *a, void *b );
static FL_OBJECT *append_object_to_form( IOBJECT *io, int *w, int *h );
static void normal_button_setup( IOBJECT *io );
static void push_button_setup( IOBJECT *io );
static void radio_button_setup( IOBJECT *io );
static void slider_setup( IOBJECT *io );
static void val_slider_setup( IOBJECT *io );
static void int_input_setup( IOBJECT *io );
static void float_input_setup( IOBJECT *io );
static void int_output_setup( IOBJECT *io );
static void float_output_setup( IOBJECT *io );
static void menue_setup( IOBJECT *io );
static void tools_callback( FL_OBJECT *ob, long data );
static Var *f_tb_changed_child( Var *v );
static Var *f_tb_wait_child( Var *v );


/*---------------------------------------*/
/* Function for initializing the toolbox */
/*---------------------------------------*/

void tool_box_create( long layout )
{
	if ( Tool_Box != NULL )
		return;

	Tool_Box                 = TOOL_BOX_P T_malloc( sizeof *Tool_Box );
	Tool_Box->layout         = layout;
	Tool_Box->Tools          = NULL;                 /* no form created yet */
	Tool_Box->objs           = NULL;                 /* and also no objects */
	Tool_Box->has_been_shown = UNSET;
	Tool_Box->next_ID        = ID_OFFSET;

	if ( GUI.G_Funcs.size == LOW )
	{
		FI_sizes.VERT_OFFSET         = 10;
		FI_sizes.HORI_OFFSET         = 10;

		FI_sizes.NORMAL_BUTTON_WIDTH = 150;

		FI_sizes.NORMAL_BUTTON_ADD_X = 15;
		FI_sizes.NORMAL_BUTTON_ADD_Y = 15;

		FI_sizes.PUSH_BUTTON_SIZE    = 30;
		FI_sizes.RADIO_BUTTON_SIZE   = 30;

		FI_sizes.SLIDER_HEIGHT       = 25;
		FI_sizes.SLIDER_WIDTH        = 150;

		FI_sizes.IN_OUT_HEIGHT       = 25;
		FI_sizes.IN_OUT_WIDTH        = 150;

		FI_sizes.MENUE_WIDTH         = 150;
		FI_sizes.MENUE_ADD_X         = 30;
		FI_sizes.MENUE_ADD_Y         = 20;

		FI_sizes.SYMBOL_SIZE_IN      = 30;
	}
	else
	{
		FI_sizes.VERT_OFFSET         = 30;
		FI_sizes.HORI_OFFSET         = 20;

		FI_sizes.NORMAL_BUTTON_WIDTH = 210;

		FI_sizes.NORMAL_BUTTON_ADD_X = 20;
		FI_sizes.NORMAL_BUTTON_ADD_Y = 20;

		FI_sizes.PUSH_BUTTON_SIZE    = 40;
		FI_sizes.RADIO_BUTTON_SIZE   = 40;

		FI_sizes.SLIDER_HEIGHT       = 35;
		FI_sizes.SLIDER_WIDTH        = 210;

		FI_sizes.IN_OUT_HEIGHT       = 35;
		FI_sizes.IN_OUT_WIDTH        = 210;

		FI_sizes.MENUE_WIDTH         = 210;
		FI_sizes.MENUE_ADD_X         = 40;
		FI_sizes.MENUE_ADD_Y         = 30;

		FI_sizes.SYMBOL_SIZE_IN      = 40;
	}
}


/*-------------------------------------------*/
/* Function for finally deleting the toolbox */
/*-------------------------------------------*/

void tool_box_delete( void )
{
	if ( Internals.mode != TEST && Tool_Box->Tools )
	{
		if ( fl_form_is_visible( Tool_Box->Tools ) )
		{
			store_geometry( );
			fl_hide_form( Tool_Box->Tools );
		}

		fl_free_form( Tool_Box->Tools );
	}

	Tool_Box = TOOL_BOX_P T_free( Tool_Box );
}


/*------------------------------------------------------------------*/
/* This function gets called for the EDL function "toolbox_freeze". */
/*------------------------------------------------------------------*/

Var *f_freeze( Var *v )
{
	bool is_now_frozen;


	is_now_frozen = get_boolean( v );

	if ( Internals.I_am == CHILD &&
		 ! writer( C_FREEZE, ( int ) is_now_frozen ) )
		THROW( EXCEPTION );

	return vars_push( INT_VAR, ( long ) is_now_frozen );
}


/*-------------------------------------------------------------*/
/* Function that gets called via the message passing mechanism */
/* when the child executes f_freeze().                         */
/*-------------------------------------------------------------*/

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
			fl_show_form( Tool_Box->Tools, FL_PLACE_SIZE, FL_FULLBORDER,
						  "fsc2: Tools" );
		}
		else
		{
			fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE, FL_FULLBORDER,
						  "fsc2: Tools" );
			tool_x = Tool_Box->Tools->x;
			tool_y = Tool_Box->Tools->y;
		}

		/* Set a close handler that avoids that the tool box window can be
		   closed */

		fl_set_form_atclose( Tool_Box->Tools, tool_box_close_handler, NULL );

		Tool_Box->has_been_shown = SET;
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
					print( FATAL, "Unknown layout keyword '%s'.\n",
						   v->val.sptr );
					THROW( EXCEPTION );
				}
				else
				{
					print( FATAL, "Unknown layout keyword '%s', using default "
						   "vertical layout.\n", v->val.sptr );
					return vars_push( INT_VAR, 0L );
				}
		}

	/* The child has no control over the graphical stuff, it has to pass all
	   requests to the parent... */

	if ( Internals.I_am == CHILD )
		return f_layout_child( layout );

	/* Set up structure for tool box */

	tool_box_create( layout );

	return vars_push( INT_VAR, layout );
}


/*----------------------------------------------------------------*/
/* Part of the f_layout() function run by the child process only, */
/* indirectly invoking f_layout() function in the parent via the  */
/* message passing mechanism.                                     */
/*----------------------------------------------------------------*/

static Var *f_layout_child( long layout )
{
	char *buffer, *pos;
	size_t len;

	len = sizeof EDL.Lc + sizeof layout;

	if ( EDL.Fname )
		len += strlen( EDL.Fname ) + 1;
	else
		len++;

	pos = buffer = CHAR_P T_malloc( len );

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


/*--------------------------------------------------------------------*/
/* Deletes one or more objects, parameter are one or more object IDs. */
/*--------------------------------------------------------------------*/

Var *f_objdel( Var *v )
{
	/* We need the ID of the object to delete */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Loop over all object numbers - since the object 'belongs' to the parent,
	   the child needs to ask the parent to delete the object. The ID of each
	   object to be deleted gets passed to the parent in a buffer and the
	   parent is asked to delete the object. */

	do
	{
		if ( Internals.I_am == CHILD )
			f_objdel_child( v );
		else
			f_objdel_parent( v );
	} while ( ( v = vars_pop( v ) ) != NULL );

	return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------*/
/* Part of the f_objdel() function run by the child only, */
/* indirectly invoking f_objdel() function in the parent  */
/* via the message passing mechanism.                     */
/*--------------------------------------------------------*/

static void f_objdel_child( Var *v )
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

	pos = buffer = CHAR_P T_malloc( len );

	memcpy( pos, &EDL.Lc, sizeof EDL.Lc );    	/* current line number */
	pos += sizeof EDL.Lc;

	memcpy( pos, &ID, sizeof ID );            	/* object ID */
	pos += sizeof ID;

	if ( EDL.Fname )
	{
		strcpy( pos, EDL.Fname );             	/* current file name */
		pos += strlen( EDL.Fname ) + 1;
	}
	else
		*pos++ = '\0';

	/* Ask parent to delete the object, bomb out on failure */

	if ( ! exp_objdel( buffer, pos - buffer ) )
		THROW( EXCEPTION );
}


/*---------------------------------------------------------------*/
/* Part of the f_objdel() function run by the parent exclusively */
/*---------------------------------------------------------------*/

static void f_objdel_parent( Var *v )
{
	IOBJECT *io = NULL;


	/* No tool box -> no objects -> no objects to delete... */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No objects have been defined yet.\n" );
		THROW( EXCEPTION );
	}

	/* Do checks on parameters */

	io = find_object_from_ID( get_strict_long( v, "object ID" ) );

	if ( io == NULL )
	{
		print( FATAL, "Invalid object identifier.\n" );
		THROW( EXCEPTION );
	}

	switch ( io->type )
	{
		case NORMAL_BUTTON : case PUSH_BUTTON : case RADIO_BUTTON :
			vars_pop( f_bdelete( vars_push( INT_VAR, v->val.lval ) ) );
			break;

		case NORMAL_SLIDER : case VALUE_SLIDER :
		case SLOW_NORMAL_SLIDER : case SLOW_VALUE_SLIDER :
			vars_pop( f_sdelete( vars_push( INT_VAR, v->val.lval ) ) );
			break;

		case INT_INPUT  : case FLOAT_INPUT :
		case INT_OUTPUT : case FLOAT_OUTPUT :
			vars_pop( f_odelete( vars_push( INT_VAR, v->val.lval ) ) );
			break;

		case MENU:
			vars_pop( f_mdelete( vars_push( INT_VAR, v->val.lval ) ) );
			break;

#ifndef NDEBUG
		default :
			eprint( FATAL, UNSET, "Internal error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
#endif
	}
}


/*----------------------------------------------*/
/* Function for changing the label of an object */
/*----------------------------------------------*/

Var *f_obj_clabel( Var *v )
{
	IOBJECT *io;
	char *label = NULL;
	long ID = 0;


	CLOBBER_PROTECT( v );
	CLOBBER_PROTECT( label );

	/* We first need the ID of the button */

	if ( Internals.mode != TEST )
	{
		ID = get_strict_long( v, "object ID" );

		if ( ID < ID_OFFSET )
		{
			print( FATAL, "Invalid object identifier.\n" );
			THROW( EXCEPTION );
		}

		v = vars_pop( v );
	}

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Second argument isn't a string.\n" );
		THROW( EXCEPTION );
	}

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1 );

	/* The child has to get parent to change the label */

	if ( Internals.I_am == CHILD )
		return f_obj_clabel_child( ID, v->val.sptr );

	/* No tool box -> no objects -> no object label change possible... */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No objects have been defined yet.\n" );
		THROW( EXCEPTION );
	}

	/* Check the ID parameter */

	io = find_object_from_ID( ID );

	if ( io == NULL )
	{
		print( FATAL, "Invalid object identifier.\n" );
		THROW( EXCEPTION );
	}

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

	T_free( io->label );
	io->label = label;

	fl_set_object_label( io->self, io->label );

	return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------------*/
/* Part of the f_ob_clabel() function run by the child only, */
/* indirectly invoking f_obj_clabel() function in the parent */
/* via the message passing mechanism.                        */
/*-----------------------------------------------------------*/

static Var *f_obj_clabel_child( long ID, char *label )
{
	char *buffer, *pos;
	size_t len;
	bool result;


	len = sizeof EDL.Lc + sizeof ID + strlen( label ) + 1;

	if ( EDL.Fname )
		len += strlen( EDL.Fname ) + 1;
	else
		len++;

	pos = buffer = CHAR_P T_malloc( len );

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

	strcpy( pos, label );
	pos += strlen( label ) + 1;

	/* Ask the parent to change the label */

	result = exp_clabel( buffer, pos - buffer );

	if ( result == FAIL )      /* failure -> bomb out */
		THROW( EXCEPTION );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------*/
/* Function for enabling or disabling an object */
/*----------------------------------------------*/

Var *f_obj_xable( Var *v )
{
	IOBJECT *io;
	long ID;
	bool state;


	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, get_boolean( v->next ) ? 1L : 0L );

	/* We first need the ID of the button */

	ID = get_strict_long( v, "object ID" );

	if ( ID < ID_OFFSET )
	{
		print( FATAL, "Invalid object identifier.\n" );
		THROW( EXCEPTION );
	}

	v = vars_pop( v );

	state = get_boolean( v );

	/* The child has to get parent to change the state */

	if ( Internals.I_am == CHILD )
		return f_obj_xable_child( ID, state ? 1L : 0L );

	/* No tool box -> no objects -> no object state change possible... */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No objects have been defined yet.\n" );
		THROW( EXCEPTION );
	}

	/* Check the ID parameter */

	io = find_object_from_ID( ID );

	if ( io == NULL )
	{
		print( FATAL, "Invalid object identifier.\n" );
		THROW( EXCEPTION );
	}

	if ( state )
	{
		if ( io->type != FLOAT_INPUT && io->type != INT_OUTPUT )
			fl_activate_object( io->self );
		fl_set_object_lcol( io->self, FL_BLACK );
	}
	else
	{
		if ( io->type != FLOAT_INPUT && io->type != INT_OUTPUT )
			fl_deactivate_object( io->self );
		fl_set_object_lcol( io->self, FL_INACTIVE_COL );
	}

	return vars_push( INT_VAR, state ? 1L : 0L );
}


/*----------------------------------------------------------*/
/* Part of the f_ob_xable() function run by the child only, */
/* indirectly invoking f_obj_xable() function in the parent */
/* via the message passing mechanism.                       */
/*----------------------------------------------------------*/

static Var *f_obj_xable_child( long ID, long state )
{
	char *buffer, *pos;
	size_t len;
	bool result;


	len = sizeof EDL.Lc + sizeof ID + sizeof state;

	if ( EDL.Fname )
		len += strlen( EDL.Fname ) + 1;
	else
		len++;

	pos = buffer = CHAR_P T_malloc( len );

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

	memcpy( pos, &state, sizeof state );      /* button ID */
	pos += sizeof ID;

	/* Ask the parent to change the label */

	result = exp_xable( buffer, pos - buffer );

	if ( result == FAIL )      /* failure -> bomb out */
		THROW( EXCEPTION );

	return vars_push( INT_VAR, state ? 1L : 0L );
}


/*----------------------------------------------------------------------*/
/* Returns a pointer to an object given its number or NULL if not found */
/*----------------------------------------------------------------------*/

IOBJECT *find_object_from_ID( long ID )
{
	IOBJECT *io;


	if ( Tool_Box == NULL )            /* no objects defined yet ? */
		return NULL;

	if ( ID < ID_OFFSET || ID >= Tool_Box->next_ID )
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

	Tool_Box = TOOL_BOX_P T_free( Tool_Box );
}


/*--------------------------------------------------------------------*/
/* Function for redrawing the toolbox after changes have been applied */
/*--------------------------------------------------------------------*/

void recreate_Tool_Box( void )
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

		if ( ! Tool_Box->has_been_shown &&
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

	if ( Tool_Box->has_been_shown )
		needs_pos = SET;

	Tool_Box->w = 2 * FI_sizes.VERT_OFFSET;
	Tool_Box->h = 2 * FI_sizes.HORI_OFFSET;

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
	{
		append_object_to_form( io, &Tool_Box->w, &Tool_Box->h );
		last_io = io;
	}

	fl_end_form( );

	fl_set_form_size( Tool_Box->Tools, Tool_Box->w, Tool_Box->h );

	if ( ! is_frozen )
	{
		if ( needs_pos )
		{
			fl_set_form_position( Tool_Box->Tools, tool_x, tool_y );
			fl_show_form( Tool_Box->Tools, FL_PLACE_SIZE, FL_FULLBORDER,
						  "fsc2: Tools" );
		}
		else
		{
			fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE, FL_FULLBORDER,
						  "fsc2: Tools" );
			tool_x = Tool_Box->Tools->x;
			tool_y = Tool_Box->Tools->y;
		}

		/* Set a close handler that avoids that the tool box window can be
		   closed */

		fl_set_form_atclose( Tool_Box->Tools, tool_box_close_handler, NULL );

		Tool_Box->has_been_shown = SET;
	}
}


/*--------------------------------------------------------------------*/
/* Function is the handler when the tool box is about to be closed by */
/* user intervention, i.e. by clicking on the close button. We make   */
/* it impossible to close the tool box this way by simply ignoring    */
/* the event.                                                         */
/*--------------------------------------------------------------------*/

static int tool_box_close_handler( FL_FORM *a, void *b )
{
	UNUSED_ARGUMENT( a );
	UNUSED_ARGUMENT( b );
	return FL_IGNORE;
}


/*---------------------------------------------------------------------*/
/* Function appends an object to the tool box. In vertical layout mode */
/* it will be drawn at the bottom of the tool box, in horizontal mode  */
/* to the right of the other objects.                                  */
/*---------------------------------------------------------------------*/

static FL_OBJECT *append_object_to_form( IOBJECT *io, int *w, int *h )
{
	/* Calculate the x- and y-position of the new object */

	if ( io->prev == NULL )
	{
		io->x = FI_sizes.VERT_OFFSET;
		io->y = FI_sizes.HORI_OFFSET;
	}
	else
	{
		if ( Tool_Box->layout == VERT )
		{
			io->x = FI_sizes.VERT_OFFSET;
			io->y = io->prev->y + io->prev->ht;

			/* The vertical distance between two buttons would look too large
			   if we use the whole horizontal spacing, so take only the half */

			if ( (  io->prev->type == NORMAL_BUTTON ||
					io->prev->type == PUSH_BUTTON   ||
					io->prev->type == RADIO_BUTTON     ) &&
				 ( io->type == NORMAL_BUTTON ||
				   io->type == PUSH_BUTTON   ||
				   io->type == RADIO_BUTTON ) )
				 io->y += FI_sizes.HORI_OFFSET / 2;
			else
				 io->y += FI_sizes.HORI_OFFSET;
		}
		else
		{
			io->x = io->prev->x + io->prev->wt + FI_sizes.VERT_OFFSET;
			io->y = FI_sizes.HORI_OFFSET;
		}
	}

	switch ( io->type )
	{
		case NORMAL_BUTTON :
			normal_button_setup( io );
			break;

		case PUSH_BUTTON :
			push_button_setup( io );
			break;

		case RADIO_BUTTON :
			radio_button_setup( io );
			break;

		case NORMAL_SLIDER : case SLOW_NORMAL_SLIDER :
			slider_setup( io );
			break;

		case VALUE_SLIDER : case SLOW_VALUE_SLIDER :
			val_slider_setup( io );
			break;

		case INT_INPUT :
			int_input_setup( io );
			break;

		case FLOAT_INPUT :
			float_input_setup( io );
			break;

		case INT_OUTPUT :
			int_output_setup( io );
			break;

		case FLOAT_OUTPUT :
			float_output_setup( io );
			break;

		case MENU :
			menue_setup( io );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	fl_set_object_resize( io->self, FL_RESIZE_NONE );
	fl_set_object_gravity( io->self, FL_NorthWest, FL_NoGravity );
	fl_set_object_callback( io->self, tools_callback, 0 );
	if ( io->help_text != NULL && *io->help_text != '\0' )
		fl_set_object_helper( io->self, io->help_text );

	/* Return the maximum width and height of all objects (plus a bit for
	   the border) */

	*w = i_max( *w, io->x + io->wt + FI_sizes.VERT_OFFSET );
	*h = i_max( *h, io->y + io->ht + FI_sizes.HORI_OFFSET );

	return io->self;
}


/*-----------------------------------------------------------------------*/
/* Creates a normal button, determines its size and sets some properties */
/*-----------------------------------------------------------------------*/

static void normal_button_setup( IOBJECT *io )
{
	if ( io->label != NULL )
	{
		if ( *io->label != '@' )
		{
			fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.buttonFontSize,
									 io->label, strlen( io->label ),
									 &io->w, &io->h );

			io->wt = io->w += FI_sizes.NORMAL_BUTTON_ADD_X;
			io->ht = io->h += FI_sizes.NORMAL_BUTTON_ADD_Y;

			if ( io->w < FI_sizes.NORMAL_BUTTON_WIDTH )
				io->wt = io->w = FI_sizes.NORMAL_BUTTON_WIDTH;
		}
		else
		{
			io->ht = io->h = FI_sizes.SYMBOL_SIZE_IN;

			if ( Tool_Box->layout == VERT )
				io->wt = io->w = FI_sizes.NORMAL_BUTTON_WIDTH;
			else
				io->wt = io->w = FI_sizes.SYMBOL_SIZE_IN;
		}
	}
	else
	{
		io->w = FI_sizes.SYMBOL_SIZE_IN;
		io->h = FI_sizes.SYMBOL_SIZE_IN;
	}

	io->self = fl_add_button( FL_NORMAL_BUTTON, io->x, io->y,
							  io->w, io->h, io->label );
	fl_set_object_color( io->self, FL_MCOL, FL_GREEN );
}


/*---------------------------------------------------------------------*/
/* Creates a push button, determines its size and sets some properties */
/*---------------------------------------------------------------------*/

static void push_button_setup( IOBJECT *io )
{
	io->w = io->h = FI_sizes.PUSH_BUTTON_SIZE;
	io->self = fl_add_checkbutton( FL_PUSH_BUTTON, io->x, io->y,
								   io->w, io->h, io->label );

	/* Setting the alignment should not be necessary, but there
	   seems to be a bug in Xforms that makes symbols appear only
	   extremely tiny unless the alignment is explicitely set */

	fl_set_object_lalign( io->self, FL_ALIGN_RIGHT );
	fl_set_object_lsize( io->self, xcntl.labelFontSize );

	if ( io->label != NULL )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.labelFontSize,
								 io->label, strlen( io->label ),
								 &io->wt, &io->ht );

		io->wt += io->w;

		if ( io->ht < io->h )
			io->ht = io->h;
		else if ( io->ht > io->h )
			fl_set_object_position( io->self, io->x,
									io->y + ( io->ht - io->h ) / 2 );
	}
	else
	{
		io->wt = io->w;
		io->ht = io->h;
	}

	fl_set_object_color( io->self, FL_MCOL, FL_YELLOW );
	fl_set_button( io->self, io->state ? 1 : 0 );
}


/*----------------------------------------------------------------------*/
/* Creates a radio button, determines its size and sets some properties */
/*----------------------------------------------------------------------*/

static void radio_button_setup( IOBJECT *io )
{
	IOBJECT *nio;


	if ( io->group != NULL )
		fl_addto_group( io->group );
	else
	{
		io->group = fl_bgn_group( );
		for ( nio = io->next; nio != NULL; nio = nio->next )
			if ( nio->partner == io->ID )
				nio->group = io->group;
	}

	io->w = io->h = FI_sizes.RADIO_BUTTON_SIZE;

	io->self = fl_add_round3dbutton( FL_RADIO_BUTTON, io->x, io->y,
									 io->w, io->h, io->label );


	/* Setting the alignment should not be necessary, but there
	   seems to be a bug in Xforms that makes symbols appear only
	   extremely tiny unless the alignment is explicitely set */

	fl_set_object_lalign( io->self, FL_ALIGN_RIGHT );
	fl_set_object_lsize( io->self, xcntl.labelFontSize );

	if ( io->label != NULL )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.labelFontSize,
								 io->label, strlen( io->label ),
								 &io->wt, &io->ht );

		io->wt += io->w;

		if ( io->ht < io->h )
			io->ht = io->h;
		else if ( io->ht > io->h )
			fl_set_object_position( io->self, io->x,
									io->y + ( io->ht - io->h ) / 2 );
	}
	else
	{
		io->wt = io->w;
		io->ht = io->h;
	}

	/* In horizontal mode line up radio buttons belonging to the same
	   group so that they appear at the same vertical position */

	if ( Tool_Box->layout == HORI )
		for ( nio = Tool_Box->objs; nio != io; nio = nio->next )
		{
			if ( nio->type != RADIO_BUTTON ||
				 nio->group != io->group )
				continue;
			if ( io->y + ( io->ht - io->h ) / 2 >
				 nio->y + ( nio->ht - nio->h ) / 2 )
				fl_set_object_position( nio->self, nio->x,
										io->y + ( io->ht - io->h ) / 2 );
			else if ( io->y + ( io->ht - io->h ) / 2 <
					  nio->y + ( nio->ht - nio->h ) / 2 )
				fl_set_object_position( io->self, io->x,
										nio->y + ( nio->ht - nio->h ) / 2 );
		}

	fl_end_group( );

	fl_set_object_color( io->self, FL_MCOL, FL_RED );
	fl_set_button( io->self, io->state ? 1 : 0 );
}


/*-----------------------------------------------------------------------*/
/* Creates a normal slider, determines its size and sets some properties */
/*-----------------------------------------------------------------------*/

static void slider_setup( IOBJECT *io )
{
	io->w = FI_sizes.SLIDER_WIDTH;
	io->h = FI_sizes.SLIDER_HEIGHT;

	io->self = fl_add_slider( FL_HOR_BROWSER_SLIDER, io->x, io->y,
							  io->w, io->h, io->label );

	fl_set_object_lsize( io->self, xcntl.sliderFontSize );
	fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );

	if ( io->label != NULL )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.sliderFontSize,
								 io->label, strlen( io->label ),
								 &io->wt, &io->ht );

		if ( io->wt < io->w )
			io->wt = io->w;
		else if ( io->wt > io->w )
			fl_set_object_lalign( io->self, FL_ALIGN_LEFT_BOTTOM );

		io->ht += io->h;
	}
	else
	{
		io->wt = io->w;
		io->ht = io->h;
	}

	fl_set_slider_bounds( io->self, io->start_val, io->end_val );
	fl_set_slider_value( io->self, io->value );
	fl_set_slider_return( io->self, io->type == NORMAL_SLIDER ?
						  FL_RETURN_CHANGED : FL_RETURN_END_CHANGED );
	if ( io->step != 0.0 )
		fl_set_slider_step( io->self, io->step );
	else
		fl_set_slider_step( io->self,
							fabs( io->end_val - io->start_val ) / 200.0 );
}


/*----------------------------------------------------------------------*/
/* Creates a value slider, determines its size and sets some properties */
/*----------------------------------------------------------------------*/

static void val_slider_setup( IOBJECT *io )
{
	double prec;


	io->w = FI_sizes.SLIDER_WIDTH;
	io->h = FI_sizes.SLIDER_HEIGHT;

	io->self = fl_add_valslider( FL_HOR_BROWSER_SLIDER, io->x, io->y,
								 io->w, io->h, io->label );

	fl_set_object_lsize( io->self, xcntl.sliderFontSize );
	fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );

	if ( io->label != NULL )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.sliderFontSize,
								 io->label, strlen( io->label ),
								 &io->wt, &io->ht );

		if ( io->wt < io->w )
			io->wt = io->w;
		else if ( io->wt > io->w )
			fl_set_object_lalign( io->self, FL_ALIGN_LEFT_BOTTOM );

		io->ht += io->h;
	}
	else
	{
		io->wt = io->w;
		io->ht = io->h;
	}

	fl_set_slider_bounds( io->self, io->start_val, io->end_val );
	fl_set_slider_value( io->self, io->value );
	fl_set_slider_return( io->self, io->type == VALUE_SLIDER ?
						  FL_RETURN_CHANGED : FL_RETURN_END_CHANGED );
	prec = - floor( log10 ( ( io->end_val - io->start_val ) /
							( 0.825 * io->w ) ) );
	fl_set_slider_precision( io->self,
							 prec <= 0.0 ? 0 : ( int ) lrnd( prec ) );
	if ( io->step != 0.0 )
		fl_set_slider_step( io->self, io->step );
	else
		fl_set_slider_step( io->self,
							fabs( io->end_val - io->start_val ) / 200.0 );
}


/*------------------------------------------------------*/
/* Creates an integer input object, determines its size */
/* and sets some properties                             */
/*------------------------------------------------------*/

static  void int_input_setup( IOBJECT *io )
{
	char buf[ MAX_INPUT_CHARS + 1 ];


	io->w = FI_sizes.IN_OUT_WIDTH;
	io->h = FI_sizes.IN_OUT_HEIGHT;

	io->self = fl_add_input( FL_INT_INPUT, io->x, io->y,
							 io->w, io->h, io->label );

	fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );

	if ( io->label != NULL )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.labelFontSize,
								 io->label, strlen( io->label ),
								 &io->wt, &io->ht );

		if ( io->wt < io->w )
			io->wt = io->w;
		else if ( io->wt > io->w )
			fl_set_object_lalign( io->self, FL_ALIGN_LEFT_BOTTOM );

		io->ht += io->h;
	}
	else
	{
		io->wt = io->w;
		io->ht = io->h;
	}

	fl_set_input_return( io->self, FL_RETURN_END_CHANGED );
	fl_set_input_maxchars( io->self, MAX_INPUT_CHARS );
	snprintf( buf, MAX_INPUT_CHARS + 1, "%ld", io->val.lval );
	fl_set_input( io->self, buf );
}


/*-------------------------------------------------------*/
/* Creates a floating point input object, determines its */
/* size and sets some properties                         */
/*-------------------------------------------------------*/

static  void float_input_setup( IOBJECT *io )
{
	char buf[ MAX_INPUT_CHARS + 1 ];


	io->w = FI_sizes.IN_OUT_WIDTH;
	io->h = FI_sizes.IN_OUT_HEIGHT;

	io->self = fl_add_input( FL_FLOAT_INPUT, io->x, io->y,
							 io->w, io->h, io->label );

	fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );

	if ( io->label != NULL )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.labelFontSize,
								 io->label, strlen( io->label ),
								 &io->wt, &io->ht );

		if ( io->wt < io->w )
			io->wt = io->w;
		else if ( io->wt > io->w )
			fl_set_object_lalign( io->self, FL_ALIGN_LEFT_BOTTOM );

		io->ht += io->h;
	}
	else
	{
		io->wt = io->w;
		io->ht = io->h;
	}

	fl_set_input_return( io->self, FL_RETURN_END_CHANGED );
	fl_set_input_maxchars( io->self, MAX_INPUT_CHARS );
	snprintf( buf, MAX_INPUT_CHARS, io->form_str, io->val.dval );
	fl_set_input( io->self, buf );
}


/*-------------------------------------------------------*/
/* Creates an integer output object, determines its size */
/* and sets some properties                              */
/*-------------------------------------------------------*/

static  void int_output_setup( IOBJECT *io )
{
	char buf[ MAX_INPUT_CHARS + 1 ];


	io->w = FI_sizes.IN_OUT_WIDTH;
	io->h = FI_sizes.IN_OUT_HEIGHT;

	io->self = fl_add_input( FL_INT_INPUT, io->x, io->y,
							 io->w, io->h, io->label );

	fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );

	if ( io->label != NULL )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.labelFontSize,
								 io->label, strlen( io->label ),
								 &io->wt, &io->ht );

		if ( io->wt < io->w )
			io->wt = io->w;
		else if ( io->wt > io->w )
			fl_set_object_lalign( io->self, FL_ALIGN_LEFT_BOTTOM );

		io->ht += io->h;
	}
	else
	{
		io->wt = io->w;
		io->ht = io->h;
	}

	fl_set_object_boxtype( io->self, FL_EMBOSSED_BOX );
	fl_set_input_return( io->self, FL_RETURN_ALWAYS );
	fl_set_input_maxchars( io->self, MAX_INPUT_CHARS );
	fl_set_object_color( io->self, FL_COL1, FL_COL1 );
	fl_set_input_color( io->self, FL_BLACK, FL_COL1 );
	fl_deactivate_object( io->self );
	snprintf( buf, MAX_INPUT_CHARS + 1, "%ld", io->val.lval );
	fl_set_input( io->self, buf );
}


/*--------------------------------------------------------*/
/* Creates a floating point output object, determines its */
/* size and sets some properties                          */
/*--------------------------------------------------------*/

static  void float_output_setup( IOBJECT *io )
{
	char buf[ MAX_INPUT_CHARS + 1 ];


	io->w = FI_sizes.IN_OUT_WIDTH;
	io->h = FI_sizes.IN_OUT_HEIGHT;

	io->self = fl_add_input( FL_FLOAT_INPUT, io->x, io->y,
							 io->w, io->h, io->label );

	fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );

	if ( io->label != NULL )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.labelFontSize,
								 io->label, strlen( io->label ),
								 &io->wt, &io->ht );

		if ( io->wt < io->w )
			io->wt = io->w;
		else if ( io->wt > io->w )
			fl_set_object_lalign( io->self, FL_ALIGN_LEFT_BOTTOM );

		io->ht += io->h;
	}
	else
	{
		io->wt = io->w;
		io->ht = io->h;
	}

	fl_set_object_boxtype( io->self, FL_EMBOSSED_BOX );
	fl_set_input_return( io->self, FL_RETURN_ALWAYS );
	fl_set_input_maxchars( io->self, MAX_INPUT_CHARS );
	fl_set_object_color( io->self, FL_COL1, FL_COL1 );
	fl_set_input_color( io->self, FL_BLACK, FL_COL1 );
	fl_deactivate_object( io->self );
	snprintf( buf, MAX_INPUT_CHARS, io->form_str, io->val.dval );
	fl_set_input( io->self, buf );
}


/*---------------------------------------------------------------*/
/* Creates a menue, determines its size and sets some properties */
/*---------------------------------------------------------------*/

static void menue_setup( IOBJECT *io )
{
	long i;
	int wt, ht;


	io->w = io->h = 0;
	for ( i = 0; i < io->num_items; i++ )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.choiceFontSize,
								 io->menu_items[ i ],
								 strlen( io->menu_items[ i ] ),
								 &wt, &ht );

		io->w = i_max( wt, io->w );
		io->h = i_max( ht, io->h );
	}

	io->w += FI_sizes.MENUE_ADD_X;
	if ( io->w < FI_sizes.MENUE_WIDTH )
		io->w = FI_sizes.MENUE_WIDTH;
	io->h += FI_sizes.MENUE_ADD_Y;

	io->self = fl_add_choice( FL_NORMAL_CHOICE2, io->x, io->y,
							  io->w, io->h, io->label );
	
	fl_set_object_lalign( io->self, FL_ALIGN_BOTTOM );

	if ( io->label != NULL )
	{
		fl_get_string_dimension( FL_NORMAL_STYLE, xcntl.labelFontSize,
								 io->label, strlen( io->label ),
								 &io->wt, &io->ht );

		if ( io->wt < io->w )
			io->wt = io->w;
		else if ( io->wt > io->w )
			fl_set_object_lalign( io->self, FL_ALIGN_LEFT_BOTTOM );

		io->ht += io->h;
	}
	else
	{
		io->wt = io->w;
		io->ht = io->h;
	}

	for ( i = 0; i < io->num_items; i++ )
		fl_addto_choice( io->self, io->menu_items[ i ] );

	fl_set_choice( io->self, io->state );
}


/*----------------------------------------------------------*/
/* Callback function for all the objects in the tool box.   */
/* The states or values are stored in the object structures */
/* to be used by the functions button_state() etc.          */
/*----------------------------------------------------------*/

static void tools_callback( FL_OBJECT *obj, long data )
{
	IOBJECT *io, *oio;
	long lval;
	double dval;
	const char *buf;
	int old_state;
	char obuf[ MAX_INPUT_CHARS + 1 ];


	UNUSED_ARGUMENT( data );

	/* Find out which object got changed */

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		if ( io->self == obj )
			break;

	fsc2_assert( io != NULL );            /* this can never happen :) */

	switch ( io->type )
	{
		case NORMAL_BUTTON :
			io->state += 1;
			io->is_changed = SET;
			if ( io->report_change )
				tb_wait_handler( io->ID );
			break;

		case PUSH_BUTTON :
			io->state = fl_get_button( obj );
			io->is_changed = SET;
			if ( io->report_change )
				tb_wait_handler( io->ID );
			break;

		case RADIO_BUTTON :
			old_state = io->state;
			io->state = fl_get_button( obj );
			if ( io->state != old_state )
			{
				for ( oio = Tool_Box->objs; oio != NULL; oio = oio->next )
				{
					if ( oio == io || oio->type != RADIO_BUTTON ||
						 oio->group != io->group || io->state == 0 )
						continue;
					oio->state = 0;
				}

				io->is_changed = SET;
				if ( io->report_change )
					tb_wait_handler( io->ID );
			}
			break;

		case NORMAL_SLIDER : case VALUE_SLIDER :
		case SLOW_NORMAL_SLIDER : case SLOW_VALUE_SLIDER :
			io->value = fl_get_slider_value( obj );
			io->is_changed = SET;
			if ( io->report_change )
				tb_wait_handler( io->ID );
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

			fl_set_input( io->self, obuf );
			if ( lval != io->val.lval )
			{
				io->val.lval = lval;
				io->is_changed = SET;
				if ( io->report_change )
					tb_wait_handler( io->ID );
			}
			break;

		case FLOAT_INPUT :
			buf = fl_get_input( io->self );

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
			sscanf( obuf, "%lf", &dval );

			snprintf( obuf, MAX_INPUT_CHARS + 1, io->form_str, io->val.dval );
			if ( strcasecmp( obuf, fl_get_input( io->self ) ) )
			{
				io->val.dval = dval;
				io->is_changed = SET;
				if ( io->report_change )
					tb_wait_handler( io->ID );
			}

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
			old_state = io->state;
			io->state = fl_get_choice( io->self );
			if ( io->state != old_state )
			{
				io->is_changed = SET;
				if ( io->report_change )
					tb_wait_handler( io->ID );
			}
			break;

		default :                 /* this can never happen :) */
			fsc2_assert( 1 == 0 );
			break;
	}
}


/*----------------------------------------------------------*/
/* Input and output objects can have a C-like format string */
/* that tells how the numbers are to be formated. Here we   */
/* check if the syntax of the format string the user gave   */
/* us is valid.                                             */
/*----------------------------------------------------------*/

bool check_format_string( char *buf )
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

void convert_escapes( char *str )
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

void check_label( char *str )
{
	const char *sym[ ] = { "->", "<-", ">", "<", ">>", "<<", "<->", "->|",
						   ">|", "|>", "-->", "=", "arrow", "returnarrow",
						   "square", "circle", "line", "plus", "UpLine",
						   "DnLine", "UpArrow", "DnArrow" };
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


/*---------------------------------------------------*/
/* Another function for dealing with a XForms bug... */
/*---------------------------------------------------*/

void store_geometry( void )
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


/*-----------------------------------------------------------------------*/
/* Function returns the ID of the first object from a list (or of all    */
/* objects in the toolbox when an empty list was passed to the function) */
/* which has been changed (by the user by manipulating the toolbox). If  */
/* none of the objects were changed 0 is returned. Output objects aren't */
/* taken into account because they can't be changed by the user via the  */
/* toolbox but only from within the EDL script.                          */
/*-----------------------------------------------------------------------*/

Var *f_tb_changed( Var *v )
{
	IOBJECT *io;


	/* The child process has it's own way of dealing with this */

	if ( Internals.I_am == CHILD )
		return f_tb_changed_child( v );

	/* No toolbox -> no objects -> no object can have changed */

	if ( Tool_Box == NULL )
		return vars_push( INT_VAR, 0L );	

	/* If there's a list of objects loop over it until the first changed one
	   is found. If none of them were changed return 0. Don't tell about
	   output objects. */

	if ( v != NULL )
	{
		for ( ; v != NULL; v = vars_pop( v ) )
		{
			io = find_object_from_ID( get_strict_long( v, "object ID" ) );
			if ( io->type == INT_OUTPUT || io->type == FLOAT_OUTPUT )
				continue;
			if ( io->is_changed )
				return vars_push( INT_VAR, io->ID );
		}

		return vars_push( INT_VAR, 0L );
	}

	/* If there were no arguments loop over all objects */

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
	{
		if ( io->type == INT_OUTPUT || io->type == FLOAT_OUTPUT )
			continue;
		if ( io->is_changed )
			return vars_push( INT_VAR, io->ID );
	}

	return vars_push( INT_VAR, 0L );
}


/*--------------------------------------------------------------------*/
/* To figure out if any of the objects changed the child process must */
/* pass the list of objects (which may be empty) to the parent and    */
/* wait for it's reply.                                               */
/*--------------------------------------------------------------------*/

static Var *f_tb_changed_child( Var *v )
{
	char *buffer, *pos;
	Var *cv;
	size_t len;
	long *result;
	long cid;
	long var_count = 0;


	/* Calculate length of buffer needed */

	len = sizeof EDL.Lc + sizeof var_count + 1;
	if ( EDL.Fname )
		len += strlen( EDL.Fname );

	for ( cv = v; cv != NULL; cv = cv->next )
	{
		vars_check( v, INT_VAR );
		if ( cv->val.lval < ID_OFFSET )
		{
			print( FATAL, "Invalid object identifier.\n" );
			THROW( EXCEPTION );
		}

		len += sizeof cv->val.lval;
		var_count++;
	}

	pos = buffer = CHAR_P T_malloc( len );

	memcpy( pos, &EDL.Lc, sizeof EDL.Lc );         /* current line number */
	pos += sizeof EDL.Lc;

	memcpy( pos, &var_count, sizeof var_count );   /* number of arguments */
	pos += sizeof var_count;

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		memcpy( pos, &v->val.lval, sizeof v->val.lval );
		pos += sizeof v->val.lval;
	}

	if ( EDL.Fname )
	{
		strcpy( pos, EDL.Fname );                  /* current file name */
		pos += strlen( EDL.Fname ) + 1;
	}
	else
		*pos++ = '\0';

	/* Ask parent process to return the ID of the first changed object */

	result = exp_tbchanged( buffer, pos - buffer );

	/* Bomb out if parent returns failure */

	if ( result[ 0 ] <= 0 )
	{
		T_free( result );
		THROW( EXCEPTION );
	}

	cid = result[ 1 ];
	T_free( result );

	return vars_push( INT_VAR, cid );
}


/*-----------------------------------------------------------------*/
/* This function is called for the EDL function "toolbox_wait". It */
/* pauses the execution of the EDL program until either the state  */
/* of one of the objects (either all or one from a list the user   */
/* passed to us) changes or the specified maximum waiting time is  */
/* exceeded.                                                       */
/*-----------------------------------------------------------------*/

Var *f_tb_wait( Var *v )
{
	IOBJECT *io;
	double duration;
	double secs;
	struct itimerval sleepy;


	/* The child process has it's own way of dealing with this */

	if ( Internals.I_am == CHILD )
		return f_tb_wait_child( v );

	/* No tool box -> no objects -> no object state changes we could wait
	   for... */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No objects have been defined yet.\n" );
		THROW( EXCEPTION );
	}

	if ( v != NULL )
	{
		duration = get_double( v, "maximum wait time for object change" );
		v = vars_pop( v );
	}
	else
		duration = -1.0;

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 0L );

	Internals.tb_wait = 0;

	/* First check if there's already an object with its state being marked
	   as changed. If there is one the ID of the first one we find is returned
	   immediately. If there's a list of objects onl check for changes of
	   objects in the list, otherwise check all objects. Don't tell about
	   changed output objects, they only can be changed by calls of EDL
	   function, not by user intervention. */

	if ( v != NULL )
	{
		for ( ; v != NULL; v = v->next )
		{
			io = find_object_from_ID( get_strict_long( v, "object ID" ) );
			if ( io->type == INT_OUTPUT || io->type == FLOAT_OUTPUT )
				continue;
			if ( io->is_changed )
			{
				tb_wait_handler( io->ID );
				return vars_push( INT_VAR, 0L );
			}
		}
	}
	else    /* if there were no arguments loop over all objects */
	{
		for ( io = Tool_Box->objs; io != NULL; io = io->next )
		{
			if ( io->type == INT_OUTPUT || io->type == FLOAT_OUTPUT )
				continue;
			if ( io->is_changed )
			{
				tb_wait_handler( io->ID );
				return vars_push( INT_VAR, 0L );
			}
		}
	}

	/* None of the objects have changed - set up all objects we're asked to
	   wait for to report changes. */

	if ( v != NULL )
	{
		for ( ; v != NULL; v = vars_pop( v ) )
		{
			io = find_object_from_ID( get_strict_long( v, "object ID" ) );
			if ( io->type == INT_OUTPUT || io->type == FLOAT_OUTPUT )
				continue;
			io->report_change = SET;
		}
	}
	else    /* if there were no arguments loop over all objects */
	{
		for ( io = Tool_Box->objs; io != NULL; io = io->next )
		{
			if ( io->type == INT_OUTPUT || io->type == FLOAT_OUTPUT )
				continue;
			io->report_change = SET;
		}
	}

	/* Only really wait if the duration is at least 1 ms, we don't have
	   a better time resolution anyway. */

	if ( duration > 0.0 && duration < 1.0e-3 )
	{
		tb_wait_handler( 0 );
		return vars_push( INT_VAR, 0L );
	}

	/* If the duration is a real time (i.e. larger than 1 ms) set up a timer */

	if ( duration > 0.0 )
	{ 
		sleepy.it_interval.tv_sec = sleepy.it_interval.tv_usec = 0;
		sleepy.it_value.tv_usec = lrnd( modf( duration, &secs ) * 1.0e6 );
		sleepy.it_value.tv_sec = lrnd( secs );
		Internals.tb_wait = TB_WAIT_TIMER_RUNNING;
		setitimer( ITIMER_REAL, &sleepy, NULL );
	}
	else
		Internals.tb_wait = TB_WAIT_RUNNING_WITH_NO_TIMER;

	return vars_push( INT_VAR, 0L );
}


/*---------------------------------------------------------------------*/
/* This function is executed by the child process doing the experiment */
/* for calls of the EDL function "toolbox_wait". Since the tool box    */
/* belongs to the parent it just tells the parent about the parameter  */
/* and lets it deal with the rest of the work. It only waits for the   */
/* parent returning a result which gets passed on to the EDL script.   */
/*---------------------------------------------------------------------*/

static Var *f_tb_wait_child( Var *v )
{
	char *buffer, *pos;
	Var *cv;
	size_t len;
	double duration;
	long *result;
	long cid;
	long var_count = 0;


	/* Calculate length of buffer needed */

	len = sizeof EDL.Lc + sizeof duration + sizeof var_count + 1;
	if ( EDL.Fname )
		len += strlen( EDL.Fname );

	if ( v != NULL )
	{
		duration = get_double( v, "maximum wait time for object change" );
		v = vars_pop( v );
	}
	else
		duration = -1.0;

	for ( cv = v; cv != NULL; cv = cv->next )
	{
		vars_check( v, INT_VAR );
		if ( cv->val.lval < ID_OFFSET )
		{
			print( FATAL, "Invalid object identifier.\n" );
			THROW( EXCEPTION );
		}

		len += sizeof cv->val.lval;
		var_count++;
	}

	pos = buffer = CHAR_P T_malloc( len );

	memcpy( pos, &EDL.Lc, sizeof EDL.Lc );         /* current line number */
	pos += sizeof EDL.Lc;

	memcpy( pos, &duration, sizeof duration );
	pos += sizeof( duration );

	memcpy( pos, &var_count, sizeof var_count );   /* number of arguments */
	pos += sizeof var_count;

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		memcpy( pos, &v->val.lval, sizeof v->val.lval );
		pos += sizeof v->val.lval;
	}

	if ( EDL.Fname )
	{
		strcpy( pos, EDL.Fname );                  /* current file name */
		pos += strlen( EDL.Fname ) + 1;
	}
	else
		*pos++ = '\0';

	/* Ask parent process to return the ID of the first changed object */

	result = exp_tbwait( buffer, pos - buffer );

	/* Bomb out if parent returns failure (but only if we didn't got a
	   legal DO_QUIT signal) */

	if ( result[ 0 ] <= 0 && ! EDL.do_quit && ! EDL.react_to_do_quit )
	{
		T_free( result );
		THROW( EXCEPTION );
	}

	cid = result[ 1 ];
	T_free( result );

	return vars_push( INT_VAR, cid );
}


/*------------------------------------------------------------------*/
/* This function gets called if either the timer for waiting for an */
/* object change expired, an object we are waiting for changed its  */
/* state or the STOP button was pressed while we are waiting.       */
/*------------------------------------------------------------------*/

void tb_wait_handler( long ID )
{
	long result[ 2 ];
	IOBJECT *io;
	struct itimerval sleepy;


	/* Do nothing if the timer has expired and we arrive here from the
	   callback for an object or the callback for the 'STOP' button (in
	   which case we're called with an argument of -1). */

	if ( Internals.tb_wait == TB_WAIT_TIMER_EXPIRED && ID != 0 )
		return;

	/* If the timer hasn't expired yet stop it */

	if ( Internals.tb_wait == TB_WAIT_TIMER_RUNNING )
	{
		sleepy.it_value.tv_usec = sleepy.it_value.tv_sec = 0;
		setitimer( ITIMER_REAL, &sleepy, NULL );
	}

	result[ 0 ] = 1;
	result[ 1 ] = ID >= 0 ? ID : 0;

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		io->report_change = UNSET;

	Internals.tb_wait = TB_WAIT_NOT_RUNNING;

	writer( C_TBWAIT_REPLY, sizeof result, result );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
