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


static Var *f_screate_child( Var *v, long type, double start_val,
							 double end_val, double step );
static void f_sdelete_child( Var *v );
static void f_sdelete_parent( Var *v );
static Var *f_svalue_child( Var *v );


/*----------------------*/
/* Creates a new slider */
/*----------------------*/

Var *f_screate( Var *v )
{
	IOBJECT *new_io = NULL, *ioi;
	long type;
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

	/* The child process has to pass the parameter to the parent and ask it to
	   create the slider */

	if ( Internals.I_am == CHILD )
		return f_screate_child( v, type, start_val, end_val, step );

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *f_screate_child( Var *v, long type, double start_val,
							 double end_val, double step )
{
	char *buffer, *pos;
	long new_ID;
	long *result;
	size_t len;
	char *label = NULL;
	char *help_text = NULL;


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
	   elements, the first indicating if it was successful, the second being
	   the sliders ID */

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


/*-----------------------------*/
/* Deletes one or more sliders */
/*-----------------------------*/

Var *f_sdelete( Var *v )
{
	if ( ! FI_sizes.is_init )
		func_intact_init( );

	/* At least one slider ID is needed... */

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
			f_sdelete_child( v );
		else
			f_sdelete_parent( v );
	} while ( ( v = vars_pop( v ) ) != NULL );

	if ( Internals.I_am == CHILD || Internals.mode == TEST || ! Tool_Box )
		return vars_push( INT_VAR, 1 );

	/* Redraw the tool box without the slider */

	recreate_Tool_Box( );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void f_sdelete_child( Var *v )
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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void f_sdelete_parent( Var *v )
{
	IOBJECT *io;


	/* No tool box -> no sliders to delete */

	if ( Tool_Box == NULL || Tool_Box->objs == NULL )
	{
		print( FATAL, "No sliders have been defined yet.\n" );
		THROW( EXCEPTION );
	}

	/* Check that slider with the ID exists */

	io = find_object_from_ID( get_strict_long( v, "slider ID" ) );

	if ( io == NULL ||
		 ( io->type != NORMAL_SLIDER && io->type != VALUE_SLIDER ) )
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
	}
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
		return f_svalue_child( v );

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *f_svalue_child( Var *v )
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
	   with two doubles, the first indicating if it was successful (when the
	   value is positive) the second is the slider value */

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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
