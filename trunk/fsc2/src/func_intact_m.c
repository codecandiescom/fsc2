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


static Var *f_mcreate_child( Var *v, size_t len, long num_strs );
static void f_mdelete_child( Var *v );
static void f_mdelete_parent( Var *v );
static Var *f_mchoice_child( Var *v );


/*---------------------------*/
/* Creates a new menu object */
/*---------------------------*/

Var *f_mcreate( Var *var )
{
	static Var *v;
	Var *lv;
	static long num_strs;
	size_t len = 0;
	static IOBJECT *new_io;
	static IOBJECT *ioi;
	static long ID;
	long i;


	v = var;
	num_strs = 0;
	ID = 0;

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
		return f_mcreate_child( v, len, num_strs );

	/* Now that we're done with checking the parameters we can create the new
       button - if the Tool_Box doesn't exist yet we've got to create it now */

	if ( Tool_Box == NULL )
		tool_box_create( VERT );

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *f_mcreate_child( Var *v, size_t len, long num_strs )
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

	/* Pass buffer to parent and ask it to create the menu object. It returns
	   a buffer with two longs, the first one indicating success or failure (1
	   or 0), the second being the new objects ID */

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


/*----------------------------------*/
/* Deletes one or more menu objects */
/*----------------------------------*/

Var *f_mdelete( Var *v )
{
	/* At least one menu ID is needed... */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Loop over all slider IDs - since the child has no control over the
	   sliders, it has to ask the parent process to delete the button */

	do
	{
		if ( Internals.I_am == CHILD )
			f_mdelete_child( v );
		else
			f_mdelete_parent( v );
	} while ( ( v = vars_pop( v ) ) != NULL );

	if ( Internals.I_am == CHILD || Internals.mode == TEST || ! Tool_Box )
		return vars_push( INT_VAR, 1 );

	/* Redraw the tool box without the menu */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void f_mdelete_child( Var *v )
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

	memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  	/* current line number */
	pos += sizeof EDL.Lc;

	memcpy( pos, &ID, sizeof ID );          	/* menu ID */
	pos += sizeof ID;

	if ( EDL.Fname )
	{
		strcpy( pos, EDL.Fname );            	/* current file name */
		pos += strlen( EDL.Fname ) + 1;
	}
	else
		*pos++ = '\0';

	/* Bomb out on failure */

	if ( ! exp_mdelete( buffer, pos - buffer ) )
		THROW( EXCEPTION );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void f_mdelete_parent( Var *v )
{
	IOBJECT *io;
	long i;


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
		tool_box_delete( );

		Tool_Box = T_free( Tool_Box );

		if ( v->next != NULL )
		{
			print( FATAL, "Invalid menu identifier.\n" );
			THROW( EXCEPTION );
		}
	}
}


/*----------------------------------------------------*/
/* Sets or returns the selected item in a menu object */
/*----------------------------------------------------*/

Var *f_mchoice( Var *v )
{
	IOBJECT *io;
	long select_item = 0;


	/* We need at least the ID of the menu */

	if ( v == NULL )
	{
		print( FATAL, "Missing argumnts.\n" );
		THROW( EXCEPTION );
	}

	/* Again, the child doesn't know about the menu, so it got to ask the
	   parent process */

	if ( Internals.I_am == CHILD )
		return f_mchoice_child( v );

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *f_mchoice_child( Var *v )
{
	long ID;
	char *buffer, *pos;
	size_t len;
	long *result;
	long select_item = 0;


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
			print( SEVERE, "Invalid menu item number %ld.\n", select_item );
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
	pos += sizeof select_item; 		            /* 0 if not to be set)     */

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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
