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



TOOL_BOX *Tool_Box = NULL;

struct {
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
} FI_sizes = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };


extern FL_resource xresources[ ];
extern FL_IOPT xcntl;
static int tool_x, tool_y;
static bool is_frozen = UNSET;
static bool needs_pos = SET;


static Var *f_layout_child( long layout );
static void f_objdel_child( Var *v );
static void f_objdel_parent( Var *v );
static int tool_box_close_handler( FL_FORM *a, void *b );
static FL_OBJECT *append_object_to_form( IOBJECT *io );
static void tools_callback( FL_OBJECT *ob, long data );



/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

void tool_box_create( long layout )
{
	if ( Tool_Box != NULL )
		return;

	Tool_Box                 = T_malloc( sizeof *Tool_Box );
	Tool_Box->layout         = layout;
	Tool_Box->Tools          = NULL;                 /* no form created yet */
	Tool_Box->objs           = NULL;                 /* and also no objects */
	Tool_Box->has_been_shown = UNSET;

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
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

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

	Tool_Box = T_free( Tool_Box );
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

		/* Set a close handler that avoids that the tool box window can be
		   closed */

		fl_set_form_atclose( Tool_Box->Tools, tool_box_close_handler, NULL );

		Tool_Box->has_been_shown = SET;
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
		return f_layout_child( layout );

	/* Set up structure for tool box */

	tool_box_create( layout );

	return vars_push( INT_VAR, layout );
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

static Var *f_layout_child( long layout )
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

	/* Loop over all object numbers - since the object 'belong' to the parent,
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

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

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

	pos = buffer = T_malloc( len );

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


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

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
			vars_pop( f_odelete( vars_push( INT_VAR, v->val.lval ) ) );
			break;

		case MENU:
			vars_pop( f_mdelete( vars_push( INT_VAR, v->val.lval ) ) );
			break;

		default :
			eprint( FATAL, UNSET, "Internal error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
	}
}


/*----------------------------------------------------------------------*/
/* Returns a pointer to an object given its number or NULL if not found */
/*----------------------------------------------------------------------*/

IOBJECT *find_object_from_ID( long ID )
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

	Tool_Box = T_free( Tool_Box );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

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

		/* Set a close handler that avoids that the tool box window can be
		   closed */

		fl_set_form_atclose( Tool_Box->Tools, tool_box_close_handler, NULL );

		Tool_Box->has_been_shown = SET;
		fl_winminsize( Tool_Box->Tools->window,
					   FI_sizes.WIN_MIN_WIDTH, FI_sizes.WIN_MIN_HEIGHT );
	}
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static int tool_box_close_handler( FL_FORM *a, void *b )
{
	a = a;
	b = b;

	return FL_IGNORE;
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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
