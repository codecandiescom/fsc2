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


static Var *f_ocreate_child( Var *v, long type, long lval, double dval );
static void f_odelete_child( Var *v );
static void f_odelete_parent( Var *v );
static Var *f_ovalue_child( Var *v );



/*--------------------------------------*/
/* Creates a new input or output object */
/*--------------------------------------*/

Var *f_ocreate( Var *v )
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

	/* Next argument could be a value to be set in the input or output
	   object */

	if ( ( v = vars_pop( v ) ) != NULL && v->type & ( INT_VAR | FLOAT_VAR ) )
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
		return f_ocreate_child( v, type, lval, dval );

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *f_ocreate_child( Var *v, long type, long lval, double dval )
{
	char *buffer, *pos;
	long new_ID;
	long *result;
	size_t len;
	char *label = NULL;
	char *help_text = NULL;
	char *form_str = NULL;


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

				/* Warn if there are superfluous arguments */

				if ( v->next->next->next != NULL )
					print( WARN, "Too many arguments, discarding "
						   "superfluous arguments.\n" );
			}
		}
	}

	/* Calculate length of buffer needed */

	len =   sizeof EDL.Lc + sizeof type
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
	   object. It returns a buffer with two longs, the first one indicating
	   success or failure (1 or 0), the second being the objects ID */

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


/*----------------------------------------------*/
/* Deletes one or more input or output objects. */
/* Parameter are one or more object IDs         */
/*----------------------------------------------*/

Var *f_odelete( Var *v )
{
	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* We need the ID of the button to delete */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Loop over all variables - since the object 'belong' to the parent, the
	   child needs to ask the parent to delete the object. The ID of each
	   object to be deleted gets passed to the parent in a buffer and the
	   parent is asked to delete the object */

	do
	{
		if ( Internals.I_am == CHILD )
			f_odelete_child( v );
		else
			f_odelete_parent( v );
	} while ( ( v = vars_pop( v ) ) != NULL );

	/* The child process is already done here, and in a test run we're also */

	if ( Internals.I_am == CHILD || Internals.mode == TEST || ! Tool_Box )
		return vars_push( INT_VAR, 1 );

	/* Redraw the form without the deleted objects */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void f_odelete_child( Var *v )
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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void f_odelete_parent( Var *v )
{
	IOBJECT *io;


	/* No tool box -> no objects -> no objects to delete... */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No input or output objects have been defined yet.\n" );
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
	}
}


/*----------------------------------------------------------*/
/* Sets or returns the content of an input or output object */
/*----------------------------------------------------------*/

Var *f_ovalue( Var *v )
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
		return f_ovalue_child( v );

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *f_ovalue_child( Var *v )
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

	/* Ask parent to set or get the value - it will return a pointer to an
	   INPUT_RES structure, where the res entry indicates failure (negative
	   value) and the type of the returned value (0 is integer, positive
	   non-zero is float), and the second entry is a union for the return
	   value, i.e. the objects value. */

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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
