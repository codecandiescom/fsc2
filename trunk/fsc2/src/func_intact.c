/*
  $Id$
*/


#include "fsc2.h"



static IOBJECT *find_object_from_ID( long ID );
static void recreate_Tool_Box( void );
static FL_OBJECT *append_object_to_form( IOBJECT *io );
static void tools_callback( FL_OBJECT *ob, long data );



Var *f_layout( Var *v )
{
	int layout;


	if ( I_am == PARENT && Tool_Box != NULL )
	{
		eprint( FATAL, "%s:%ld Layout of tool box must be set before any "
				"buttons or sliders are created.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		layout = v->val.lval != 0 ? HORI : VERT;
	else
		layout = v->val.dval != 0.0 ? HORI : VERT;

	if ( I_am == CHILD )
	{
		void *buffer, *pos;
		long len;

		len = 2 * sizeof( long );
		if ( Fname )
			len += strlen( Fname ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &Lc, sizeof( long ) );     /* current line number */
		pos += sizeof( long );

		* ( ( long * ) pos ) = ( long ) layout;   /* type of layout */
		pos += sizeof( long );

		if ( Fname )
		{
			strcpy( ( char * ) pos, Fname );    /* current file name */
			pos += strlen( Fname ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';

		if ( ! exp_layout( buffer, ( long ) ( pos - buffer ) ) )
			THROW( EXCEPTION );

		return vars_push( INT_VAR, ( long ) layout );
	}

	Tool_Box = T_malloc( sizeof( TOOL_BOX ) );
	Tool_Box->layout = layout;
	Tool_Box->x = Tool_Box->y = 0;
	Tool_Box->Tools = NULL;
	Tool_Box->objs = NULL;

	return vars_push( INT_VAR, ( long ) layout );
}
	

/*----------------------*/
/* Creates a new button */
/*----------------------*/

Var *f_bcreate( Var *v )
{
	long type;
	long coll = -1;
	IOBJECT *cio;
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

	/* First argument must be type of button ("NORMAL_BUTTON", "PUSH_BUTTON"
	   or "RADIO_BUTTON" or 0, 1, 2) */

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

	/* For radio buttons the next argument is the ID of the first button
	   belonging to the group (except for the first button itself) */

	if ( type == RADIO_BUTTON )
	{
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

			/* Checking that the referenced group leader button exists
			   and is also a RADIO_BUTTON can only be done by the parent */

			if ( I_am == PARENT )
			{
				/* Check that other group member exists at all */

				if ( ( cio = find_object_from_ID( coll ) ) == NULL )
				{
					eprint( FATAL, "%s:%ld: Button with ID %ld doesn't exist "
							"in `button_create'.\n", Fname, Lc, coll );
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
			}

			v = vars_pop( v );
		}
	}

	/* Next argument is the label string */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		label = get_string_copy( v->val.sptr );
		v = vars_pop( v );
	}

	/* Final argument can be a help text */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		help_text = get_string_copy( v->val.sptr );
		v = vars_pop( v );
	}

	/* Warn and get rid of superfluous arguments */

	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: Superfluous arguments in call of "
				"`button_create'.\n", Fname, Lc );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Since the child process can't use graphics it has to write the
	   parameter into a buffer, pass it to the parent process and ask the
	   parent to create the button */

	if ( I_am == CHILD )
	{
		void *buffer, *pos;
		long new_ID;
		long *result;
		size_t len;


		/* Calculate length of buffer needed */

		len = 3 * sizeof( long );
		if ( Fname )
			len += strlen( Fname ) + 1;
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

		memcpy( pos, &Lc, sizeof( long ) );     /* current line number */
		pos += sizeof( long );

		memcpy( pos, &type, sizeof( long ) );   /* button type */
		pos += sizeof( long );

		memcpy( pos, &coll, sizeof( long ) );   /* group leaders ID */
		pos += sizeof( long );

		if ( Fname )
		{
			strcpy( ( char * ) pos, Fname );    /* current file name */
			pos += strlen( Fname ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';

		if ( label )                            /* label string */
		{
			strcpy( ( char * ) pos, label );
			pos += strlen( label ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';

		if ( help_text )                        /* help text string */
		{
			strcpy( ( char * ) pos, help_text );
			pos += strlen( help_text ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';


		/* Pass buffer to parent and ask it to create the button. It returns a
		   buffer with two longs, the first one indicating success or failure
		   (value is 1 or 0), the second being the buttons ID */

		result = exp_bcreate( buffer, ( long ) ( pos - buffer ) );

		T_free( label );
		T_free( help_text );

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
		Tool_Box = T_malloc( sizeof( TOOL_BOX ) );
		Tool_Box->layout = VERT;
		Tool_Box->x = Tool_Box->y = 0;
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
	new_io->group = NULL;
	new_io->label = label;
	new_io->help_text = help_text;
	new_io->partner = coll;

	/* If this isn't just a test run really draw the new button */

	if ( ! TEST_RUN )
	{
		recreate_Tool_Box( );

		if ( Tool_Box->x == 0 && Tool_Box->y == 0 )
			fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
						  FL_FULLBORDER, "fsc2: Tools" );
		else
		{
			fl_set_form_position( Tool_Box->Tools, Tool_Box->x, Tool_Box->y );
			fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
						  FL_FULLBORDER, "fsc2: Tools" );
		}
		XFlush( fl_get_display( ) );
	}

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

	/* All the arguments are button numbers */

	while ( v != NULL )
	{
		/* Since the buttons 'belong' to the parent, the child needs to ask
		   the parent to delete the button. The ID of each button to be deleted
		   gets passed to te parent in a buffer and the parent is asked to
		   delete th button */

		if ( I_am == CHILD )
		{
			void *buffer, *pos;
			size_t len;


			/* Do all possible checking of the parameter */

			if ( v->type != INT_VAR || v->val.lval < 0 )
			{
				eprint( FATAL, "%s:%ld: Invalid button identifier in "
						"`button_delete'.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			/* Get a bufer long enough and write data */

			len = 2 * sizeof( long );
			if ( Fname )
				len += strlen( Fname ) + 1;
			else
				len++;

			pos = buffer = T_malloc( len );

			memcpy( pos, &Lc, sizeof( long ) );       /* current line number */
			pos += sizeof( long );
			memcpy( pos, &v->val.lval, sizeof( long ) );  /* button ID */
			pos += sizeof( long );
			if ( Fname )
			{
				strcpy( ( char * ) pos, Fname );    /* current file name */
				pos += strlen( Fname ) + 1;
			}
			else
				*( ( char * ) pos++ ) = '\0';

			v = vars_pop( v );

			/* Ask parent to delete the buton, bomb out on failure */

			if ( ! exp_bdelete( buffer, ( long ) ( pos - buffer ) ) )
				THROW( EXCEPTION );

			continue;
		}

		/* No tool box -> no buttons -> no buttons to delete... */

		if ( Tool_Box == NULL )
		{
			eprint( FATAL, "%s:%ld: No buttons have been defined yet.\n",
					Fname, Lc );
			THROW( EXCEPTION );
		}

		/* Do checks on parameters */

		if ( v->type != INT_VAR || v->val.lval < 0 ||
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

		/* Delete the button (its not drawn in a test run!) */

		if ( ! TEST_RUN )
		{
			fl_delete_object( io->self );
			fl_free_object( io->self );
		}

		/* Special care has to be taken for the first radio buttons of a group
		   (i.e. the other refer to) is deleted - the next button from the
		   group must be made group leader and the remaining buttons must get
		   told about it) */

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

		T_free( io->label );
		T_free( io->help_text );
		T_free( io );

		if ( Tool_Box->objs == NULL )
		{
			if ( ! TEST_RUN )
			{
				fl_hide_form( Tool_Box->Tools );

				fl_delete_object( Tool_Box->background_box );
				fl_free_object( Tool_Box->background_box );

				fl_free_form( Tool_Box->Tools );
			}

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

	/* The child process is already done here, and in a test run we're also */

	if ( I_am == CHILD || TEST_RUN )
		return vars_push( INT_VAR, 1 );

	/* Redraw the form without the deleted buttons */

	recreate_Tool_Box( );

	if ( Tool_Box->x == 0 && Tool_Box->y == 0 )
		fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "fsc2: Tools" );
	else
	{
		fl_set_form_position( Tool_Box->Tools, Tool_Box->x, Tool_Box->y );
		fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
					  FL_FULLBORDER, "fsc2: Tools" );
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

	/* Again, the child doesn't know about the button, so it got to ask the
	   parent process */

	if ( I_am == CHILD )
	{
		long ID;
		long state = -1;
		void *buffer, *pos;
		long len;


		/* Basid check of button identifier - always the first parameter */

		if ( v->type != INT_VAR || v->val.lval < 0 )
		{
			eprint( FATAL, "%s:%ld: Invalid button identifier in "
					"`button_state'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
		ID = v->val.lval;
		
		/* If there's a second parameter it's the state of button to set */

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			vars_check( v, INT_VAR | FLOAT_VAR );
			if ( v->type == INT_VAR )
				state = v->val.lval != 0 ? 1 : 0;
			else
				state = v->val.dval != 0.0 ? 1 : 0;
		}

		/* No more parameters accepted... */

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			eprint( WARN, "%s:%ld: Superfluous arguments in call of "
					"`button_state'.\n", Fname, Lc );
			while ( ( v = vars_pop( v ) ) != NULL )
				;
		}

		/* Make up buffer to send to parent process */

		len = 3 * sizeof( long );
		if ( Fname )
			len += strlen( Fname ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &Lc, sizeof( long ) );     /* current line number */
		pos += sizeof( long );

		memcpy( pos, &ID, sizeof( long ) );     /* buttons ID */
		pos += sizeof( long );

		memcpy( pos, &state, sizeof( long ) );  /* state to be set (negative */
		pos += sizeof( long );                  /* if not to be set)         */

		if ( Fname )
		{
			strcpy( ( char * ) pos, Fname );    /* current file name */
			pos += strlen( Fname ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';
		
		/* Ask parent process to return or set the state - bomb out if it
		   returns a negative value, indicating a severe error */

		state = exp_bstate( buffer, ( long ) ( pos - buffer ) );

		if ( state < 0 )
			THROW( EXCEPTION );

		return vars_push( INT_VAR, state );
	}

	/* No tool box -> no buttons -> no button state to set or get... */

	if ( Tool_Box == NULL )
	{
		eprint( FATAL, "%s:%ld: No buttons have been defined yet.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Check the button ID parameter */

	if ( v->type != INT_VAR || v->val.lval < 0 ||
		 ( io = find_object_from_ID( v->val.lval ) ) == NULL ||
		 ( io->type != NORMAL_BUTTON &&
		   io->type != PUSH_BUTTON   &&
		   io->type != RADIO_BUTTON ) )
	{
		eprint( FATAL, "%s:%ld: Invalid button identifier in "
				"`button_state'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* If there's no second parameter just return the button state - for
	   NORMAL_BUUTONs return the number it was pressed since the last call
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
	{
		eprint( WARN, "%s:%ld: Float value used as button state in "
				"`button_state'.\n", Fname, Lc );
		io->state = v->val.dval != 0.0 ? 1 : 0;
	}

	if ( ! TEST_RUN )
	{
		fl_set_button( io->self, io->state );
		XFlush( fl_get_display( ) );
	}

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
	
	if ( I_am == CHILD )
	{
		void *buffer, *pos;
		long new_ID;
		long *result;
		size_t len;


		len = 2 * sizeof( long ) + 2 * sizeof( double );
		if ( Fname )
			len += strlen( Fname ) + 1;
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

		memcpy( pos, &Lc, sizeof( long ) );     /* store current line number */
		pos += sizeof( long );

		memcpy( pos, &type, sizeof( long ) );   /* store slider type */
		pos += sizeof( long );

		memcpy( pos, &start_val, sizeof( double ) );
		pos += sizeof( double );

		memcpy( pos, &end_val, sizeof( double ) );
		pos += sizeof( double );

		if ( Fname )
		{
			strcpy( ( char * ) pos, Fname );    /* store current file name */
			pos += strlen( Fname ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';

		if ( label )                            /* store label string */
		{
			strcpy( ( char * ) pos, label );
			pos += strlen( label ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';

		if ( help_text )                        /* store help text string */
		{
			strcpy( ( char * ) pos, help_text );
			pos += strlen( help_text ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';

		result = exp_screate( buffer, ( long ) ( pos - buffer ) );

		T_free( label );
		T_free( help_text );

		if ( result[ 0 ] == 0 )
		{
			T_free( result );
			THROW( EXCEPTION );
		}

		new_ID = result[ 1 ];
		T_free( result );
		return vars_push( INT_VAR, new_ID );
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
	new_io->self = NULL;
	new_io->start_val = start_val;
	new_io->end_val = end_val;
	new_io->value = 0.5 * ( end_val + start_val );
	new_io->label = label;
	new_io->help_text = help_text;
	
	if ( ! TEST_RUN )
	{
		recreate_Tool_Box( );

		if ( Tool_Box->x == 0 && Tool_Box->y == 0 )
			fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
						  FL_FULLBORDER, "fsc2: Tools" );
		else
		{
			fl_set_form_position( Tool_Box->Tools, Tool_Box->x, Tool_Box->y );
			fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
						  FL_FULLBORDER, "fsc2: Tools" );
		}

		XFlush( fl_get_display( ) );
	}

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

	while ( v != NULL )
	{
		if ( I_am == CHILD )
		{
			void *buffer, *pos;
			long len;


			if ( v->type != INT_VAR || v->val.lval < 0 )
			{
				eprint( FATAL, "%s:%ld: Invalid slider identifier in "
						"`slider_delete'.\n", Fname, Lc );
				THROW( EXCEPTION );
			}

			len = 2 * sizeof( long );
			if ( Fname )
				len += strlen( Fname ) + 1;
			else
				len++;

			pos = buffer = T_malloc( len );

			memcpy( pos, &Lc, sizeof( long ) );
			pos += sizeof( long );

			memcpy( pos, &v->val.lval, sizeof( long ) );
			pos += sizeof( long );

			if ( Fname )
			{
				strcpy( ( char * ) pos, Fname );    /* current file name */
				pos += strlen( Fname ) + 1;
			}
			else
				*( ( char * ) pos++ ) = '\0';

			v = vars_pop( v );

			if ( ! exp_sdelete( buffer, ( long ) ( pos - buffer ) ) )
				THROW( EXCEPTION );

			continue;
		}

		if ( Tool_Box == NULL )
		{
			eprint( FATAL, "%s:%ld: No sliders have been defined yet.\n",
					Fname, Lc );
			THROW( EXCEPTION );
		}

		if ( v->type != INT_VAR || v->val.lval < 0 ||
			 ( io = find_object_from_ID( v->val.lval ) ) == NULL ||
			 ( io->type != NORMAL_SLIDER &&
			   io->type != VALUE_SLIDER ) )
		{
			eprint( FATAL, "%s:%ld: Invalid slider identifier in "
					"`slider_delete'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		/* Remove slider from the linked list */

		if ( io->next != NULL )
			io->next->prev = io->prev;
		if ( io->prev != NULL )
			io->prev->next = io->next;
		else
			Tool_Box->objs = io->next;

		/* Delete the button */

		if ( ! TEST_RUN )
		{
			fl_delete_object( io->self );
			fl_free_object( io->self );
		}

		T_free( io->label );
		T_free( io->help_text );
		T_free( io );

		if ( Tool_Box->objs == NULL )
		{
			if ( ! TEST_RUN )
			{
				fl_hide_form( Tool_Box->Tools );

				fl_delete_object( Tool_Box->background_box );
				fl_free_object( Tool_Box->background_box );

				fl_free_form( Tool_Box->Tools );
			}

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

	if ( I_am == CHILD || TEST_RUN )
		return vars_push( INT_VAR, 1 );

	recreate_Tool_Box( );

	if ( Tool_Box->x == 0 && Tool_Box->y == 0 )
		fl_show_form( Tool_Box->Tools, FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "fsc2: Tools" );
	else
	{
		fl_set_form_position( Tool_Box->Tools, Tool_Box->x, Tool_Box->y );
		fl_show_form( Tool_Box->Tools, FL_PLACE_POSITION,
					  FL_FULLBORDER, "fsc2: Tools" );
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

	if ( I_am == CHILD )
	{
		long ID;
		long state = 0;
		double val = 0.0;
		void *buffer, *pos;
		double *res;
		long len;


		if ( v->type != INT_VAR || v->val.lval < 0 )
		{
			eprint( FATAL, "%s:%ld: Invalid slider identifier in "
					"`slider_state'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
		ID = v->val.lval;
		
		if ( ( v = vars_pop( v ) ) != NULL )
		{
			state = 1;
			vars_check( v, INT_VAR | FLOAT_VAR );
			val = VALUE( v );
		}

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			eprint( WARN, "%s:%ld: Superfluous arguments in call of "
					"`slider_state'.\n", Fname, Lc );
			while ( ( v = vars_pop( v ) ) != NULL )
				;
		}

		len = 3 * sizeof( long ) + sizeof( double );
		if ( Fname )
			len += strlen( Fname ) + 1;
		else
			len++;

		pos = buffer = T_malloc( len );

		memcpy( pos, &Lc, sizeof( long ) );
		pos += sizeof( long );

		memcpy( pos, &ID, sizeof( long ) );
		pos += sizeof( long );

		memcpy( pos, &state, sizeof( long ) );
		pos += sizeof( long );

		memcpy( pos, &val, sizeof( double ) );
		pos += sizeof( double );

		if ( Fname )
		{
			strcpy( ( char * ) pos, Fname );    /* current file name */
			pos += strlen( Fname ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';
		
		res = exp_sstate( buffer, ( long ) ( pos - buffer ) );

		if ( res[ 0 ] < 0 )
		{
			T_free( res );
			THROW( EXCEPTION );
		}

		val = res[ 1 ];
		T_free( res );
		return vars_push( FLOAT_VAR, val );
	}

	if ( Tool_Box == NULL )
	{
		eprint( FATAL, "%s:%ld: No slider have been defined yet.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v->type != INT_VAR || v->val.lval < 0 ||
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
	
	if ( ! TEST_RUN )
	{
		fl_set_slider_value( io->self, io->value );
		XFlush( fl_get_display( ) );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: Superfluous arguments in call of "
				"`button_state'.\n", Fname, Lc );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	return vars_push( FLOAT_VAR, io->value );
}


/*----------------------------------------------------------------------*/
/* Returns a pointer to an object given its number or NULL if not found */
/*----------------------------------------------------------------------*/

static IOBJECT *find_object_from_ID( long ID )
{
	IOBJECT *io;


	if ( Tool_Box == NULL )            /* no objects defined yet ? */
		return NULL;

	/* Loop through linked list to find the object */

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

	if ( Tool_Box->Tools )
		fl_hide_form( Tool_Box->Tools );

	for ( io = Tool_Box->objs; io != NULL; io = next )
	{
		next = io->next;

		if ( Tool_Box->Tools )
		{
			fl_delete_object( io->self );
			fl_free_object( io->self );
		}

		T_free( io->label );
		T_free( io->help_text );
		T_free( io );
	}

	if ( Tool_Box->Tools )
	{
		fl_delete_object( Tool_Box->background_box );
		fl_free_object( Tool_Box->background_box );
		fl_free_form( Tool_Box->Tools );
	}

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


	if ( TEST_RUN )        /* just to make sure... */
		return;

	/* If the tool box already exists we've got to find out its position
	   and then delete the form amnd all objects */

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
			fl_delete_object( io->self );
			fl_free_object( io->self );
			io->group = NULL;
		}
		
		fl_delete_object( Tool_Box->background_box );
		fl_free_object( Tool_Box->background_box );

		fl_free_form( Tool_Box->Tools );
	}

	/* Now calculate the new size of the tool box and create it */

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

	Tool_Box->Tools = fl_bgn_form( FL_NO_BOX, Tool_Box->w, Tool_Box->h );
	Tool_Box->background_box = fl_add_box( FL_UP_BOX, 0, 0,
										   Tool_Box->w, Tool_Box->h, "" );

	/* Finally re-populate the tool box */

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
