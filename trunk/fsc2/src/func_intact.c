/*
  $Id$
*/


#include "fsc2.h"



static IOBJECT *find_object_from_ID( long ID );
static void recreate_Tool_Box( void );
static FL_OBJECT *append_object_to_form( IOBJECT *io );
static void tools_callback( FL_OBJECT *ob, long data );
static void convert_escapes( char *str );



/*------------------------------------------------------------*/
/* Function sets the layout of the tool box, either vertical  */
/* or horizontal by passing it either 0 or 1  or "vert[ical]" */
/* or "hori[zontal]" (case in-sensitive). Must be called      */
/* before any object (button or slider) is created (or after  */
/* all of them have been deleted again :).                    */
/*------------------------------------------------------------*/

Var *f_layout( Var *v )
{
	int layout;


	if ( I_am == PARENT && Tool_Box != NULL )
	{
		eprint( FATAL, "%s:%ld Layout of tool box must be set before any "
				"buttons or sliders are created.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameter in call of `layout'.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == INT_VAR )
		layout = v->val.lval != 0 ? HORI : VERT;
	else if ( v->type == FLOAT_VAR )
		layout = v->val.dval != 0.0 ? HORI : VERT;
	else
	{
		if ( ! strcasecmp( v->val.sptr, "VERT" ) ||
			 ! strcasecmp( v->val.sptr, "VERTICAL" ) )
			layout = 0;
		else if ( ! strcasecmp( v->val.sptr, "HORI" ) ||
				  ! strcasecmp( v->val.sptr, "HORIZONTAL" ) )
			layout = 1;
		else
		{
			eprint( FATAL, "%s:%ld: Unknown layout keyword `%s' in function "
					"`layout'.\n", Fname, Lc, v->val.sptr );
			THROW( EXCEPTION );
		}
	}

	/* The child has no control over the graphical stuff, it has to pass all
	   requests concerning them to the parent... */

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

		/* Bomb out if parent failed to set layout type */

		if ( ! exp_layout( buffer, ( long ) ( pos - buffer ) ) )
			THROW( EXCEPTION );

		return vars_push( INT_VAR, ( long ) layout );
	}

	/* Set up structure for tool box */

	Tool_Box = T_malloc( sizeof( TOOL_BOX ) );
	Tool_Box->layout = layout;
	Tool_Box->Tools = NULL;               /* no form created yet */
	Tool_Box->objs = NULL;                /* and also no objects */

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


	/* At least the type of the button must be specified */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameter in call of "
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
		if ( ! strcasecmp( v->val.sptr, "NORMAL_BUTTON" ) )
			type = NORMAL_BUTTON;
		else if ( ! strcasecmp( v->val.sptr, "PUSH_BUTTON" ) )
			type = PUSH_BUTTON;
		else if ( ! strcasecmp( v->val.sptr, "RADIO_BUTTON" ) )
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
		convert_escapes( label );
		v = vars_pop( v );
	}

	/* Final argument can be a help text */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		help_text = get_string_copy( v->val.sptr );
		convert_escapes( help_text );
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
		Tool_Box->objs = NULL;
		Tool_Box->layout = VERT;
		Tool_Box->Tools = NULL;
	}

	if ( Tool_Box->objs == NULL )
	{
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
	new_io->self = NULL;
	new_io->group = NULL;
	new_io->label = label;
	new_io->help_text = help_text;
	new_io->partner = coll;

	/* If this isn't just a test run really draw the new button */

	if ( ! TEST_RUN )
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
	long new_anchor;


	/* We need the ID of the button to delete */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameter in call of "
				"`button_delete'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Loop over all button numbers */

	while ( v != NULL )
	{
		/* Since the buttons 'belong' to the parent, the child needs to ask
		   the parent to delete the button. The ID of each button to be deleted
		   gets passed to te parent in a buffer and the parent is asked to
		   delete the button */

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

		if ( Tool_Box == NULL || Tool_Box->objs == NULL )
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

	if ( I_am == CHILD || TEST_RUN || ! Tool_Box )
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


	/* We need at least the ID of the button */

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


		/* Basic check of button identifier - always the first parameter */

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

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
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

	/* The optional second argument is the state to be set - but take care,
	   the state of NORMAL_BUTTONs can't be set */

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

	/* If this isn't a test run set the button state */

	if ( ! TEST_RUN )
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
			if ( ! TEST_RUN )
				fl_set_button( oio->self, oio->state );
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


	/* We need at least the type of the slider and the minimum and maximum
	   value */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameters in call of "
				"`slider_create'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Check the type parameter */

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
		if ( ! strcasecmp( v->val.sptr, "NORMAL_SLIDER" ) )
			type = NORMAL_SLIDER;
		else if ( ! strcasecmp( v->val.sptr, "VALUE_SLIDER" ) )
			type = VALUE_SLIDER;
		else
		{
			eprint( FATAL, "%s:%ld: Unknown slider type (`%s') in function "
					"`slider_create'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}

	/* Get the minimum value for the slider */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing minimum value in call of "
				"`slider_create'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	start_val = VALUE( v );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing maximum value in call of "
				"`slider_create'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Get the maximum value and make sure it's larger than the minimum */

	vars_check( v, INT_VAR | FLOAT_VAR );
	end_val = VALUE( v );

	if ( end_val <= start_val )
	{
		eprint( FATAL, "%s:%ld: Maxinmum not larger than minimum value in "
				"call of `slider_create'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check( v, STR_VAR );
		label = get_string_copy( v->val.sptr );
		convert_escapes( label );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check( v, STR_VAR );
		help_text = get_string_copy( v->val.sptr );
		convert_escapes( help_text );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: Superfluous arguments in call of "
				"`slider_create'.\n", Fname, Lc );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}
	
	/* Again, the child process has to pass the parameter to the parent and
	   ask it to create the slider */

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

		/* Ask parent to create the slider - it returns an array width two
		   elements, the first indicating if it was successful, the second
		   being the sliders ID */

		result = exp_screate( buffer, ( long ) ( pos - buffer ) );

		T_free( label );
		T_free( help_text );

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

	/* Now that we're done with checking the parameters we can create the new
       button - if the Tool_Box doesn't exist yet we've got to create it now */

	if ( Tool_Box == NULL )
	{
		Tool_Box = T_malloc( sizeof( TOOL_BOX ) );
		Tool_Box->objs = NULL;
		Tool_Box->layout = VERT;
		Tool_Box->Tools = NULL;
	}

	if ( Tool_Box->objs == NULL )
	{
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
	
	/* If this isn't a test run the slider must also be drawn */

	if ( ! TEST_RUN )
		recreate_Tool_Box( );

	return vars_push( INT_VAR, new_io->ID );
}


/*-----------------------------*/
/* Deletes one or more sliders */
/*-----------------------------*/

Var *f_sdelete( Var *v )
{
	IOBJECT *io;


	/* At least one slider ID is needed... */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameter in call of "
				"`slider_delete'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Loop over all slider IDs */

	while ( v != NULL )
	{
		/* Child has no control over the sliders, it has to ask the parent
		   process to delete the button */

		if ( I_am == CHILD )
		{
			void *buffer, *pos;
			long len;


			/* Very basic sanity check */

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

			memcpy( pos, &Lc, sizeof( long ) );  /* current line number */
			pos += sizeof( long );

			memcpy( pos, &v->val.lval, sizeof( long ) );
			pos += sizeof( long );               /* slider ID */

			if ( Fname )
			{
				strcpy( ( char * ) pos, Fname );    /* current file name */
				pos += strlen( Fname ) + 1;
			}
			else
				*( ( char * ) pos++ ) = '\0';

			v = vars_pop( v );

			/* Bomb out on failure */

			if ( ! exp_sdelete( buffer, ( long ) ( pos - buffer ) ) )
				THROW( EXCEPTION );

			continue;                   /* delete next slider */
		}

		/* No tool box -> no sliders to delete */

		if ( Tool_Box == NULL || Tool_Box->objs == NULL )
		{
			eprint( FATAL, "%s:%ld: No sliders have been defined yet.\n",
					Fname, Lc );
			THROW( EXCEPTION );
		}

		/* Check that slider with the ID exists */

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

		/* Delete the slider object if its drawn */

		if ( ! TEST_RUN && io->self )
		{
			fl_delete_object( io->self );
			fl_free_object( io->self );
		}

		T_free( io->label );
		T_free( io->help_text );
		T_free( io );

		/* If this was the very last object delete also the form */

		if ( Tool_Box->objs == NULL )
		{
			if ( ! TEST_RUN )
			{
				fl_hide_form( Tool_Box->Tools );
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

	if ( I_am == CHILD || TEST_RUN || ! Tool_Box )
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
	int type;


	/* We need at least the sliders ID */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameters in call of "
				"`slider_value'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Again, the child has to pass the arguments to the parent and ask it
	   to set or return the slider value */

	if ( I_am == CHILD )
	{
		long ID;
		long state = 0;
		double val = 0.0;
		void *buffer, *pos;
		double *res;
		long len;


		/* Very basic sanity check... */

		if ( v->type != INT_VAR || v->val.lval < 0 )
		{
			eprint( FATAL, "%s:%ld: Invalid slider identifier in "
					"`slider_state'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
		ID = v->val.lval;
		
		/* Another arguments means that the slider value is to be set */

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			vars_check( v, INT_VAR | FLOAT_VAR );
			state = 1;
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

		memcpy( pos, &Lc, sizeof( long ) );     /* current line number */
		pos += sizeof( long );

		memcpy( pos, &ID, sizeof( long ) );     /* sliders ID */
		pos += sizeof( long );

		memcpy( pos, &state, sizeof( long ) );  /* needs slider setting ? */
		pos += sizeof( long );

		memcpy( pos, &val, sizeof( double ) );  /* new slider value */
		pos += sizeof( double );

		if ( Fname )
		{
			strcpy( ( char * ) pos, Fname );    /* current file name */
			pos += strlen( Fname ) + 1;
		}
		else
			*( ( char * ) pos++ ) = '\0';
		
		/* Ask parent to set or get the slider value - it will return an array
		   with two doubles, the first indicating if it was successful (when
		   the value is positive) the second is the slider value */

		res = exp_sstate( buffer, ( long ) ( pos - buffer ) );

		/* Bomb out on failure */

		if ( res[ 0 ] < 0 )
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
		eprint( FATAL, "%s:%ld: No slider have been defined yet.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Check that ID is ID of a slider */

	if ( v->type != INT_VAR || v->val.lval < 0 ||
		 ( io = find_object_from_ID( v->val.lval ) ) == NULL ||
		 ( io->type != NORMAL_SLIDER &&
		   io->type != VALUE_SLIDER ) )
	{
		eprint( FATAL, "%s:%ld: Invalid slider identifier in "
				"`slider_value'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* If there are no more arguments just set the sliders value */

	if ( ( v = vars_pop( v ) ) == NULL )
		return vars_push( FLOAT_VAR, io->value );

	/* Otherwise check the next argument, i.e. the value to be set - it
	   must be within the sliders range, if it doesn't fit reduce it to
	   the allowed range */

	vars_check( v, INT_VAR | FLOAT_VAR );
	type = v->type;

	io->value = VALUE( v );

	if ( io->value > io->end_val )
	{
		eprint( SEVERE, "%s:%ld: Value too large in `slider_value'.\n",
				Fname, Lc );
		io->value = io->end_val;
	}

	if ( io->value < io->start_val )
	{
		eprint( SEVERE, "%s:%ld: Value too small in `slider_value'.\n",
				Fname, Lc );
		io->value = io->start_val;
	}
	
	if ( ! TEST_RUN )
		fl_set_slider_value( io->self, io->value );

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

		if ( Tool_Box->Tools && io->self )
		{
			fl_delete_object( io->self );
			fl_free_object( io->self );
		}

		T_free( io->label );
		T_free( io->help_text );
		T_free( io );
	}

	if ( Tool_Box->Tools )
		fl_free_form( Tool_Box->Tools );

	Tool_Box = T_free( Tool_Box );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void recreate_Tool_Box( void )
{
	IOBJECT *io;
	bool needs_pos = SET;


	if ( TEST_RUN )        /* just to make sure... */
		return;

	/* If the tool box already exists we've got to find out its position
	   and then delete all the objects */

	if ( Tool_Box->Tools != NULL )
	{
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
	}

	/* Now calculate the new size of the tool box and create it */

	Tool_Box->w = 2 * OFFSET_X0;
	Tool_Box->h = 2 * OFFSET_Y0;

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		if ( Tool_Box->layout == VERT )
			Tool_Box->h += OBJ_HEIGHT + VERT_OFFSET;
		else
			Tool_Box->w += OBJ_WIDTH + HORI_OFFSET
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

	if ( fl_form_is_visible ( Tool_Box->Tools ) )
	{
		fl_hide_form( Tool_Box->Tools );
		fl_set_form_size( Tool_Box->Tools, Tool_Box->w, Tool_Box->h );
		fl_addto_form( Tool_Box->Tools );
	}
	else
	{
		needs_pos = UNSET;
		Tool_Box->Tools = fl_bgn_form( FL_UP_BOX, Tool_Box->w, Tool_Box->h );
	}

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		append_object_to_form( io );

	fl_end_form( );

	fl_show_form( Tool_Box->Tools, needs_pos ?
				  FL_PLACE_POSITION : FL_PLACE_MOUSE | FL_FREE_SIZE,
				  FL_FULLBORDER, "fsc2: Tools" );
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
			io->x = io->prev->x + io->prev->w + HORI_OFFSET
				- ( io->type == NORMAL_BUTTON ? NORMAL_BUTTON_DELTA : 0 )
				- ( io->prev->type == NORMAL_BUTTON ?
					NORMAL_BUTTON_DELTA : 0 );
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

		default :
			assert( 1 == 0 );
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


/*----------------------------------------------------------*/
/* Callback function for all the objects in the tool box.   */
/* The states or values are stored in the object structures */
/* to be used by the functions button_state() and           */
/* slider_value()                                           */
/*----------------------------------------------------------*/

static void tools_callback( FL_OBJECT *obj, long data )
{
	IOBJECT *io, *oio;


	data = data;

	/* Find out which button or slider got changed */

	for ( io = Tool_Box->objs; io != NULL; io = io->next )
		if ( io->self == obj )
			break;

	assert( io != NULL );            /* this can never happen :) */

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

		default :                 /* this can never happen :) */
			assert( 1 == 0 );
	}
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

			case 't' :
				memcpy( ptr + 1, ptr + 2, strlen( ptr + 2 ) + 1 );
				*ptr = '\t';
				break;
				
			case 'r' :
				memcpy( ptr + 1, ptr + 2, strlen( ptr + 2 ) + 1 );
				*ptr = '\r';
				break;
		}

		ptr++;
	}
}
