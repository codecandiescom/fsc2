/*
  $Id$
*/


#include "fsc2.h"


void tools_clear( void );


static IOBJECT *find_object_from_ID( long ID );
static void recreate_Tool_Box( void );
static FL_OBJECT *append_object_to_form( IOBJECT *io );
static void tools_callback( FL_OBJECT *ob, long data );


/*----------------------*/
/* Creates a new button */
/*----------------------*/

Var *f_bcreate( Var *v )
{
	long type;
	long coll;
	IOBJECT *cio;
	FL_OBJECT *group = NULL;
	char *label = NULL;
	char *help_text = NULL;
	IOBJECT *new_io, *ioi;
	long ID = 0;


	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameters in call of "
				"`button_create'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == INT_VAR || v->type == FLOAT_VAR )
	{
		if ( v->type == INT_VAR )
			type = v->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Float variable used as button type in "
					"`button_create'.\n", Fname, Lc );
			type = lround( v->val.dval );
		}
	}
	else
	{
		if ( ! strcmp( v->val.sptr, "NORMAL_BUTTON" ) )
			type = NORMAL_BUTTON;
		else if ( ! strcmp( v->val.sptr, "PUSH_BUTTON" ) )
			type = PUSH_BUTTON;
		else if ( ! strcmp( v->val.sptr, "RADIO_BUTTON" ) )
			type = RADIO_BUTTON;
		else
		{
			eprint( FATAL, "%s:%ld: Unknown button type (`%s') in function "
					"`button_create'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}

	v = vars_pop( v );

	/* For radio buttons the next argument could be the ID of a button
	   belonging to the same group */

	if ( type == RADIO_BUTTON )
	{
	    coll = -1;

		if ( v != NULL && v->type & ( INT_VAR | FLOAT_VAR ) )
		{
			vars_check( v, INT_VAR | FLOAT_VAR );

			if ( v->type == FLOAT_VAR )
			{
				eprint( WARN, "%s:%ld: Float variable used as button ID in "
						"`button_create'.\n", Fname, Lc );
				coll = lround( v->val.dval );
			}
			else
				coll = v->val.lval;

			/* Check that other group member exists at all */

			if ( ( cio = find_object_from_ID( coll ) ) == NULL )
			{
				eprint( FATAL, "%s:%ld: Button with ID %ld doesn't exist in "
						"`button_create'.\n", Fname, Lc, coll );
				THROW( EXCEPTION );
			}

			/* The other group members must also be radio buttons */

			if ( cio->type != RADIO_BUTTON )
			{
				eprint( FATAL, "%s:%ld: Button with ID %ld isn't a "
						"RADIO_BUTTON in `button_create'.\n",
						Fname, Lc, coll );
				THROW( EXCEPTION );
			}

			v = vars_pop( v );
		}
	}

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		label = get_string_copy( v->val.sptr );
		v = vars_pop( v );
	}

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		help_text = get_string_copy( v->val.sptr );
		v = vars_pop( v );
	}

	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: Superfluous arguments in call of "
				"`button_create'.\n", Fname, Lc );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Now that we're done with checking the parameters we can create the new
       button - if the Tool_Box doesn't exist yet we've got to create it now */

	if ( Tool_Box == NULL )
	{
		Tool_Box = T_malloc( sizeof( TOOL_BOX ) );
		Tool_Box->layout = VERT;
		Tool_Box->Tools = NULL;
		new_io = Tool_Box->objs = T_malloc( sizeof( IOBJECT ) );
		new_io->next = new_io->prev = NULL;
	}
	else
	{
		for ( ioi = Tool_Box->objs; ioi->next != NULL; ioi = ioi->next )
			;
		ID = ioi->ID + 1;
		new_io = ioi->next = T_malloc( sizeof( IOBJECT ) );
		new_io->prev = ioi;
		new_io->next = NULL;
	}

	new_io->ID = ID;
	new_io->type = ( int ) type;
	new_io->state = 0;
	new_io->group = group;
	new_io->label = label;
	new_io->help_text = help_text;
	new_io->partner = coll;
	new_io->is_created = UNSET;

	recreate_Tool_Box( );

	if ( Tool_Box->x == 0 && Tool_Box->y == 0 )
		fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "" );
	else
	{
		fl_set_form_position( Tool_Box->Tools, Tool_Box->x, Tool_Box->y );
		fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
					  FL_FULLBORDER, "" );
	}
	XFlush( fl_get_display( ) );

	return vars_push( INT_VAR, new_io->ID );
}


/*-----------------------------*/
/* Deletes one or more buttons */
/*-----------------------------*/

Var *f_bdelete( Var *v )
{
	IOBJECT *io, *nio; 
	long new_anchor;


	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameter in call of "
				"`button_delete'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( Tool_Box == NULL )
	{
		eprint( FATAL, "%s:%ld: No buttons have been defined yet.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	while ( v != NULL )
	{
		if ( v->type != INT_VAR ||
			 ( io = find_object_from_ID( v->val.lval ) ) == NULL ||
			 ( io->type != NORMAL_BUTTON &&
			   io->type != PUSH_BUTTON   &&
			   io->type != RADIO_BUTTON ) )
		{
			eprint( FATAL, "%s:%ld: Invalid button identifier in "
					"`button_delete'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		/* Remove button from the linked list */

		if ( io->next != NULL )
			io->next->prev = io->prev;
		if ( io->prev != NULL )
			io->prev->next = io->next;
		else
			Tool_Box->objs = io->next;

		/* Delete the button */

		fl_delete_object( io->self );
		fl_free_object( io->self );

		/* Special care has to be taken for radio buttons if the first is
		   deleted */

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
					if ( nio->type == RADIO_BUTTON && nio->partner == io->ID )
						nio->partner = new_anchor;
		}

		T_free( io );

		if ( Tool_Box->objs == NULL )
		{
			fl_hide_form( Tool_Box->Tools );

			fl_delete_object( Tool_Box->background_box );
			fl_free_object( Tool_Box->background_box );

			fl_free_form( Tool_Box->Tools );
			Tool_Box = T_free( Tool_Box );

			if ( ( v = vars_pop( v ) ) != NULL )
			{
				eprint( FATAL, "%s:%ld: Invalid button identifier in "
						"`button_delete'.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			return vars_push( INT_VAR, 1 );
		}


		v = vars_pop( v );
	}

	recreate_Tool_Box( );

	if ( Tool_Box->x == 0 && Tool_Box->y == 0 )
		fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "" );
	else
	{
		fl_set_form_position( Tool_Box->Tools, Tool_Box->x, Tool_Box->y );
		fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
					  FL_FULLBORDER, "" );
	}
	XFlush( fl_get_display( ) );

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------*/
/* Sets or returns the state of a button */
/*---------------------------------------*/

Var *f_bstate( Var *v )
{
	IOBJECT *io, *oio;
	int state;


	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameters in call of "
				"`button_state'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( Tool_Box == NULL )
	{
		eprint( FATAL, "%s:%ld: No buttons have been defined yet.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v->type != INT_VAR ||
		 ( io = find_object_from_ID( v->val.lval ) ) == NULL ||
		 ( io->type != NORMAL_BUTTON &&
		   io->type != PUSH_BUTTON   &&
		   io->type != RADIO_BUTTON ) )
	{
		eprint( FATAL, "%s:%ld: Invalid button identifier in "
				"`button_state'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( io->type == NORMAL_BUTTON )
		{
			state = io->state;
			io->state = 0;
			return vars_push( INT_VAR, state );
		}
		else
			return vars_push( INT_VAR, io->state != 0 ? 1 : 0 );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( io->type == NORMAL_BUTTON )
	{
		eprint( FATAL, "%s:%ld: Can't set state of a NORMAL_BUTTON.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v->type == INT_VAR )
		io->state = v->val.lval != 0 ? 1 : 0;
	else
		io->state = v->val.dval != 0.0 ? 1 : 0;

	fl_set_button( io->self, io->state );

	if ( io->type == RADIO_BUTTON )
	{
		for ( oio = Tool_Box->objs; oio != NULL; oio = oio->next )
		{
			if ( oio == io || oio->type != RADIO_BUTTON )
				continue;
			if ( oio->group == io->group )
				oio->state = io->state ? 0 : 1;
		}
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: Superfluous arguments in call of "
				"`button_state'.\n", Fname, Lc );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	return vars_push( INT_VAR, io->state );
}


/*----------------------*/
/* Creates a new slider */
/*----------------------*/

Var *f_screate( Var *v )
{
	IOBJECT *new_io, *ioi;
	int type;
	double start_val, end_val;
	char *label = NULL;
	char *help_text = NULL;
	long ID = 0;


	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameters in call of "
				"`slider_create'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == INT_VAR || v->type == FLOAT_VAR )
	{
		if ( v->type == INT_VAR )
			type = v->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Float variable used as slider type in "
					"`slider_create'.\n", Fname, Lc );
			type = lround( v->val.dval );
		}
	}
	else
	{
		if ( ! strcmp( v->val.sptr, "NORMAL_SLIDER" ) )
			type = NORMAL_SLIDER;
		else if ( ! strcmp( v->val.sptr, "VALUE_SLIDER" ) )
			type = VALUE_SLIDER;
		else
		{
			eprint( FATAL, "%s:%ld: Unknown slider type (`%s') in function "
					"`slider_create'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing start value in call of "
				"`slider_create'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	start_val = VALUE( v );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing end value in call of "
				"`slider_create'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	end_val = VALUE( v );

	if ( end_val <= start_val )
	{
		eprint( FATAL, "%s:%ld: End value not larger than start value in call "
				"of `slider_create'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check( v, STR_VAR );
		label = get_string_copy( v->val.sptr );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check( v, STR_VAR );
		help_text = get_string_copy( v->val.sptr );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: Superfluous arguments in call of "
				"`slider_create'.\n", Fname, Lc );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}
	
	/* Now that we're done with checking the parameters we can create the new
       button - if the Tool_Box doesn't exist yet we've got to create it now */

	if ( Tool_Box == NULL )
	{
		Tool_Box = T_malloc( sizeof( TOOL_BOX ) );
		Tool_Box->layout = VERT;
		Tool_Box->Tools = NULL;
		new_io = Tool_Box->objs = T_malloc( sizeof( IOBJECT ) );
		new_io->next = new_io->prev = NULL;
	}
	else
	{
		for ( ioi = Tool_Box->objs; ioi->next != NULL; ioi = ioi->next )
			;
		ID = ioi->ID + 1;
		new_io = ioi->next = T_malloc( sizeof( IOBJECT ) );
		new_io->prev = ioi;
		new_io->next = NULL;
	}

	new_io->ID = ID;
	new_io->type = ( int ) type;
	new_io->start_val = start_val;
	new_io->end_val = end_val;
	new_io->value = 0.5 * ( end_val - start_val );
	new_io->label = label;
	new_io->help_text = help_text;
	new_io->is_created = UNSET;
	
	recreate_Tool_Box( );

	if ( Tool_Box->x == 0 && Tool_Box->y == 0 )
		fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "" );
	else
	{
		fl_set_form_position( Tool_Box->Tools, Tool_Box->x, Tool_Box->y );
		fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
					  FL_FULLBORDER, "" );
	}
	XFlush( fl_get_display( ) );

	return vars_push( INT_VAR, new_io->ID );
}


/*-----------------------------*/
/* Deletes one or more sliders */
/*-----------------------------*/

Var *f_sdelete( Var *v )
{
	IOBJECT *io;


	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameter in call of "
				"`slider_delete'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( Tool_Box == NULL )
	{
		eprint( FATAL, "%s:%ld: No sliders have been defined yet.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	while ( v != NULL )
	{
		if ( v->type != INT_VAR ||
			 ( io = find_object_from_ID( v->val.lval ) ) == NULL ||
			 ( io->type != NORMAL_BUTTON &&
			   io->type != PUSH_BUTTON   &&
			   io->type != RADIO_BUTTON ) )
		{
			eprint( FATAL, "%s:%ld: Invalid slider identifier in "
					"`slider_delete'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		/* Remove button from the linked list */

		if ( io->next != NULL )
			io->next->prev = io->prev;
		if ( io->prev != NULL )
			io->prev->next = io->next;
		else
			Tool_Box->objs = io->next;

		/* Delete the button */

		fl_delete_object( io->self );
		fl_free_object( io->self );

		T_free( io );

		if ( Tool_Box->objs == NULL )
		{
			fl_hide_form( Tool_Box->Tools );

			fl_delete_object( Tool_Box->background_box );
			fl_free_object( Tool_Box->background_box );

			fl_free_form( Tool_Box->Tools );
			Tool_Box = T_free( Tool_Box );

			if ( ( v = vars_pop( v ) ) != NULL )
			{
				eprint( FATAL, "%s:%ld: Invalid slider identifier in "
						"`slider_delete'.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			return vars_push( INT_VAR, 1 );
		}


		v = vars_pop( v );
	}

	recreate_Tool_Box( );

	if ( Tool_Box->x == 0 && Tool_Box->y == 0 )
		fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "" );
	else
	{
		fl_set_form_position( Tool_Box->Tools, Tool_Box->x, Tool_Box->y );
		fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
					  FL_FULLBORDER, "" );
	}
	XFlush( fl_get_display( ) );

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------*/
/* Sets or returns the value of a slider */
/*---------------------------------------*/

Var *f_svalue( Var *v )
{
	IOBJECT *io;
	int type;


	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameters in call of "
				"`slider_value'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( Tool_Box == NULL )
	{
		eprint( FATAL, "%s:%ld: No slider have been defined yet.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v->type != INT_VAR ||
		 ( io = find_object_from_ID( v->val.lval ) ) == NULL ||
		 ( io->type != NORMAL_SLIDER &&
		   io->type != VALUE_SLIDER ) )
	{
		eprint( FATAL, "%s:%ld: Invalid slider identifier in "
				"`slider_value'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		return vars_push( FLOAT_VAR, io->value );

	vars_check( v, INT_VAR | FLOAT_VAR );
	type = v->type;

	io->value = VALUE( v );

	if ( io->value > io->end_val )
	{
		eprint( WARN, "%s:%ld: Value too large in `slider_value'.\n",
				Fname, Lc );
		io->value = io->end_val;
	}

	if ( io->value < io->start_val )
	{
		eprint( WARN, "%s:%ld: Value too small in `slider_value'.\n",
				Fname, Lc );
		io->value = io->start_val;
	}
	
	fl_set_slider_value( io->self, io->value );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: Superfluous arguments in call of "
				"`button_state'.\n", Fname, Lc );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( type == INT_VAR )
		return vars_push( INT_VAR, lround( io->value ) );
	else
		return vars_push( FLOAT_VAR, io->value );
}


/*----------------------------------------------------------------------*/
/* Returns a pointer to an object given its number or NULL if not found */
/*----------------------------------------------------------------------*/

static IOBJECT *find_object_from_ID( long ID )
{
	IOBJECT *io;


	if ( Tool_Box == NULL )
		return NULL;

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		if ( io->ID == ID )
			return io;
	return NULL;
}


/*---------------------------------------------------------------*/
/* Removes all buttons and sliders and the window they belong to */
/*---------------------------------------------------------------*/

void tools_clear( void )
{
	IOBJECT *io, *next;


	if ( Tool_Box == NULL )
		return;

	fl_hide_form( Tool_Box->Tools );

	for ( io = Tool_Box->objs; io != NULL; io = next )
	{
		next = io->next;

		T_free( io->label );
		T_free( io->help_text );

		fl_delete_object( io->self );
		fl_free_object( io->self );
		T_free( io );
	}

	fl_delete_object( Tool_Box->background_box );
	fl_free_object( Tool_Box->background_box );

	fl_free_form( Tool_Box->Tools );
	Tool_Box = T_free( Tool_Box );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void recreate_Tool_Box( void )
{
    XWindowAttributes attr;
    Window win, wdummy1, *wdummy2;
	IOBJECT *io;
    int temp;


	/* If the tool box already exists we've got to find out its position
	   and then delete the form */

	if ( Tool_Box->Tools != NULL )
	{
		win = fl_show_form_window( Tool_Box->Tools );
		XQueryTree( fl_get_display( ), win, &wdummy1, &win, &wdummy2, &temp );
		XQueryTree( fl_get_display( ), win, &wdummy1, &win, &wdummy2, &temp );
		XGetWindowAttributes( fl_get_display( ), win, &attr );

		Tool_Box->x = attr.x;
		Tool_Box->y = attr.y;

		fl_hide_form( Tool_Box->Tools );

		for ( io = Tool_Box->objs; io != NULL; io = io->next )
		{
			if ( io->is_created )
			{
				fl_delete_object( io->self );
				fl_free_object( io->self );
				io->is_created = UNSET;
			}

			io->group = NULL;
		}
		
		fl_delete_object( Tool_Box->background_box );
		fl_free_object( Tool_Box->background_box );

		fl_free_form( Tool_Box->Tools );
		T_free( Tool_Box->Tools );
	}
	else
	{
		Tool_Box->x = 0;
		Tool_Box->y = 0;
	}

	/* Now we've got to calculate its new size and create it */

	Tool_Box->w = 2 * OFFSET_X0;
	Tool_Box->h = 2 * OFFSET_Y0;

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		if ( Tool_Box->layout == VERT )
			Tool_Box->h += OBJ_HEIGHT + VERT_OFFSET;
		else
			Tool_Box->w += OBJ_WIDTH * HORI_OFFSET
				           - ( io->type == NORMAL_BUTTON ?
							   2 * NORMAL_BUTTON_DELTA : 0 );

	
	if ( Tool_Box->layout == VERT )
	{
		Tool_Box->w += OBJ_WIDTH;
		Tool_Box->h -= VERT_OFFSET;
	}
	else
	{
		Tool_Box->w -= HORI_OFFSET;
		Tool_Box->h += OBJ_HEIGHT;
	}

	/* Now recreate the tool box and re-populate it */

	Tool_Box->Tools = fl_bgn_form( FL_NO_BOX, Tool_Box->w, Tool_Box->h );
	Tool_Box->background_box = fl_add_box( FL_UP_BOX, 0, 0,
										   Tool_Box->w, Tool_Box->h, "" );
	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		append_object_to_form( io );
	fl_end_form( );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static FL_OBJECT *append_object_to_form( IOBJECT *io )
{
	double prec;
	IOBJECT *nio;


	/* Calculate the width and height of the new object */

	if ( io->prev == NULL )
	{
		io->x = OFFSET_X0 + ( io->type == NORMAL_BUTTON ?
							  NORMAL_BUTTON_DELTA : 0 );
		io->y = OFFSET_Y0;
	}
	else
	{
		if ( Tool_Box->layout == VERT )
		{
			io->x = OFFSET_X0 + ( io->type == NORMAL_BUTTON ?
								  NORMAL_BUTTON_DELTA : 0 );
			io->y = io->prev->y + io->prev->h + VERT_OFFSET +
				( io->prev->type == NORMAL_SLIDER ||
				  io->prev->type == VALUE_SLIDER ?
				  SLIDER_VERT_OFFSET : 0 );
		}
		else
		{
			io->x = io->prev->x + io->prev->w + HORI_OFFSET;
			io->y = OFFSET_Y0;
		}
	}

	switch ( io->type )
	{
		case NORMAL_BUTTON :
			io->w = BUTTON_WIDTH - 2 * NORMAL_BUTTON_DELTA;
			io->h = BUTTON_HEIGHT;
			io->self = fl_add_button( FL_NORMAL_BUTTON, io->x, io->y,
									  io->w, io->h, io->label );
			fl_set_object_color( io->self, FL_MCOL, FL_GREEN );
			fl_set_button( io->self, io->state ? 1 : 0 );
			break;

		case PUSH_BUTTON :
			io->w = BUTTON_WIDTH;
			io->h = BUTTON_HEIGHT;
			io->self = fl_add_checkbutton( FL_PUSH_BUTTON, io->x, io->y,
										   io->w, io->h, io->label );
			fl_set_object_color( io->self, FL_MCOL, FL_YELLOW );
			fl_set_button( io->self, io->state ? 1 : 0 );
			break;

		case RADIO_BUTTON :
			io->w = BUTTON_WIDTH;
			io->h = BUTTON_HEIGHT;

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
			io->w = SLIDER_WIDTH;
			io->h = SLIDER_HEIGHT;
			io->self = fl_add_slider( FL_HOR_BROWSER_SLIDER, io->x, io->y,
									  io->w, io->h, io->label );
			fl_set_slider_bounds( io->self, io->start_val, io->end_val );
			fl_set_slider_value( io->self, io->value );
			fl_set_slider_return( io->self, FL_RETURN_END );
			break;

		case VALUE_SLIDER :
			io->w = SLIDER_WIDTH;
			io->h = SLIDER_HEIGHT;
			io->self = fl_add_valslider( FL_HOR_BROWSER_SLIDER, io->x, io->y,
										 io->w, io->h, io->label );
			fl_set_slider_bounds( io->self, io->start_val, io->end_val );
			fl_set_slider_value( io->self, io->value );
			fl_set_slider_return( io->self, FL_RETURN_END );			
			prec = - floor( log10 ( ( io->end_val - io->start_val ) /
									( 0.825 * io->w ) ) );
			fl_set_slider_precision( io->self,
									prec <= 0.0 ? 0 : ( int ) lround( prec ) );
			break;
	}

	fl_set_object_gravity( io->self, FL_NoGravity, FL_NoGravity );
	fl_set_object_resize( io->self, FL_RESIZE_NONE );
	fl_set_object_lsize( io->self, FONT_SIZE );
	fl_set_object_lstyle( io->self, FL_BOLD_STYLE );
	fl_set_object_callback( io->self, tools_callback, 0 );
	if ( io->help_text != NULL && *io->help_text != '\0' )
		fl_set_object_helper( io->self, io->help_text );

	io->is_created = SET;

	return io->self;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void tools_callback( FL_OBJECT *obj, long data )
{
	IOBJECT *io, *oio;


	data = data;

	/* Find out which button or slider got changed */

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		if ( io->self == obj )
			break;

	if ( io == NULL )        /* should never happen */
		return;

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
				if ( oio == io || oio->type != RADIO_BUTTON )
					continue;
				if ( oio->group == io->group )
					oio->state = io->state ? 0 : 1;
			}
			break;

		case NORMAL_SLIDER : case VALUE_SLIDER :
			io->value = fl_get_slider_value( obj );
			break;

		default :
			assert( 1 == 0 );
	}
}
