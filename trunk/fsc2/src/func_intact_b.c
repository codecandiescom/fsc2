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


/* Globals declared in func_intact.c */

extern TOOL_BOX *Tool_Box;
extern bool tool_has_been_shown;
extern FI_SIZES FI_sizes;


static Var *f_bcreate_child( Var *v, long type, long coll );
static void f_bdelete_child( Var *v );
static void f_bdelete_parent( Var *v );
static Var *f_bstate_child( Var *v );


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

	/* Since the child process can't use graphics it has to pass the parameter
	   to the parent process and ask it to to create the button */

	if ( Internals.I_am == CHILD )
		return f_bcreate_child( v, type, coll );

	/* Next argument gto to be the label string */

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *f_bcreate_child( Var *v, long type, long coll )
{
	char *buffer, *pos;
	long new_ID;
	long *result;
	size_t len;
	char *label = NULL;
	char *help_text = NULL;


	/* We already got the type and the 'collge' buttons number, the next
	   argument is the label string */

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

	/* Pass buffer to the parent and ask it to create the button. It returns a
	   buffer with two longs, the first indicating success or failure (value
	   is 1 or 0), the second being the buttons ID */

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


/*--------------------------------------*/
/* Deletes one or more buttons          */
/* Parameter are one or more button IDs */
/*--------------------------------------*/

Var *f_bdelete( Var *v )
{
	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need the ID of the button to delete */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Loop over all button numbers - s^ince the buttons 'belong' to the
	   parent, the child needs to ask the parent to delete the button. The ID
	   of each button to be deleted gets passed to the parent and the parent
	   is asked to delete the button. */

	do
	{
		if ( Internals.I_am == CHILD )
			f_bdelete_child( v );
		else
			f_bdelete_parent( v );
	} while ( ( v = vars_pop( v ) ) != NULL );

	/* The child process is already done here and also a test run (or when the
	   tool box is already deleted) */

	if ( Internals.I_am == CHILD || Internals.mode == TEST || ! Tool_Box )
		return vars_push( INT_VAR, 1 );

	/* Redraw the form without the deleted buttons */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, 1 );
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

static void f_bdelete_child( Var *v )
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


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

static void f_bdelete_parent( Var *v )
{
	IOBJECT *io, *nio;
	long new_anchor = 0;


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

	/* Special care has to be taken for the first radio buttons of a group
	   (i.e. the one the others refer to) is deleted - the next button from
	   the group must be made group leader and the remaining buttons must get
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

		if ( v->next != NULL )
		{
			print( FATAL, "Invalid button identifier.\n" );
			THROW( EXCEPTION );
		}
	}
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
		return f_bstate_child( v );

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


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

static Var *f_bstate_child( Var *v )
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
	pos += sizeof chld_state;                   /* if not to be set)         */

	if ( EDL.Fname )
	{
		strcpy( pos, EDL.Fname );           /* current file name */
		pos += strlen( EDL.Fname ) + 1;
	}
	else
		*pos++ = '\0';

	/* Ask parent process to return or set the state */

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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
