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

#include "cursor1.xbm"             /* bitmaps for cursors */
#include "cursor2.xbm"
#include "cursor3.xbm"
#include "cursor4.xbm"
#include "cursor5.xbm"
#include "cursor6.xbm"
#include "cursor7.xbm"

#include "up_arrow.xbm"             /* arrow bitmaps */
#include "down_arrow.xbm"
#include "left_arrow.xbm"
#include "right_arrow.xbm"


extern Cut_Graphics CG;             /* exported by graph_cut.c */

static void fonts_init( void );
static void set_default_sizes( void );
static void set_defaults( void );
static void forms_adapt( void );
static int run_form_close_handler( FL_FORM *a, void *b );
static void G_struct_init( void );
static void G_init_curves_1d( void );
static void G_init_curves_2d( void );
static void setup_canvas( Canvas *c, FL_OBJECT *obj );
static void canvas_off( Canvas *c, FL_OBJECT *obj );

static Graphics *G_stored = NULL;
static Graphics_1d *G_1d_stored = NULL;
static Graphics_2d *G_2d_stored = NULL;


extern FL_resource xresources[ ];

static bool display_has_been_shown = UNSET;

static struct {
	unsigned int WIN_MIN_1D_WIDTH;
	unsigned int WIN_MIN_2D_WIDTH;
	unsigned int WIN_MIN_HEIGHT;

	int SMALL_FONT_SIZE;

	const char *DEFAULT_AXISFONT_1;
	const char *DEFAULT_AXISFONT_2;
	const char *DEFAULT_AXISFONT_3;

	int display_x,
		display_y;

	int display_1d_x,
		display_1d_y;

	int display_2d_x,
		display_2d_y;

	unsigned int display_w,
				 display_h,
				 display_1d_w,
				 display_1d_h,
				 display_2d_w,
				 display_2d_h;

	bool is_pos,
		 is_size,
		 is_1d_pos,
		 is_1d_size,
		 is_2d_pos,
		 is_2d_size;
} GI;


/*----------------------------------------------------------------------*/
/* Initializes and shows the window for displaying measurement results. */
/*----------------------------------------------------------------------*/

void start_graphics( void )
{
	int i;


	if ( ! display_has_been_shown )
		set_defaults( );

	G.font = NULL;

	CG.is_shown = UNSET;
	CG.curve = -1;
	CG.index = 0;

	if ( G.dim & 1 )
	{
		G1.nx = G1.nx_orig;
		G1.rwc_start[ X ] = G1.rwc_start_orig[ X ];
		G1.rwc_delta[ X ] = G1.rwc_delta_orig[ X ];

		for ( i = X; i <= Y; i++ )
			G1.label[ i ] = T_strdup( G1.label_orig[ i ] );

		G1.marker_1d = NULL;
	}

	if ( G.dim & 2 )
	{
		for ( i = 0; i < G2.nc; i++ )
			G2.curve_2d[ i ] = NULL;

		G2.nx = G2.nx_orig;
		G2.ny = G2.ny_orig;

		for ( i = X; i <= Y; i++ )
		{
			G2.rwc_start[ i ] = G2.rwc_start_orig[ i ];
			G2.rwc_delta[ i ] = G2.rwc_delta_orig[ i ];
		}

		for ( i = X; i <= Z; i++ )
			G2.label[ i ] = T_strdup( G2.label_orig[ i ] );

		cut_init( );
	}

	/* Create the forms for running experiments */

	if ( G.dim & 1 || ! G.is_init )
		GUI.run_form_1d = GUI.G_Funcs.create_form_run_1d( );
	if ( G.dim & 2 )
	{
		GUI.run_form_2d = GUI.G_Funcs.create_form_run_2d( );
		GUI.cut_form = GUI.G_Funcs.create_form_cut( );
	}

	/* Do some changes to the forms */

	forms_adapt( );

	/* Store the current state of the Graphics structure - to be restored
	   after the experiment */

	G_stored = GRAPHICS_P get_memcpy( &G, sizeof G );
	G_1d_stored = GRAPHICS_1D_P get_memcpy( &G1, sizeof G1 );
	G_2d_stored = GRAPHICS_2D_P get_memcpy( &G2, sizeof G2 );

	G2.active_curve = 0;

	/* Finally draw the form */

	set_default_sizes( );

	if ( ! G.is_init || G.dim & 1 )
	{
		fl_show_form( GUI.run_form_1d->run_1d, GI.is_pos || GI.is_1d_pos ?
					  FL_PLACE_POSITION : FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, G.mode == NORMAL_DISPLAY ?
					  "fsc2: 1D-Display" :
					  "fsc2: 1D-Display (sliding window)" );
		G.d = FL_FormDisplay( GUI.run_form_1d->run_1d );
	}

	if ( G.dim & 2 )
	{
		fl_show_form( GUI.run_form_2d->run_2d, GI.is_pos || GI.is_2d_pos ?
					  FL_PLACE_POSITION : FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "fsc2: 2D-Display" );
		G.d = FL_FormDisplay( GUI.run_form_1d->run_1d );
	}

	display_has_been_shown = SET;

	/* Set minimum size for display window and switch on full scale button */

	if ( G.dim & 1 || ! G.is_init )
		fl_winminsize( GUI.run_form_1d->run_1d->window,
					   GI.WIN_MIN_1D_WIDTH, GI.WIN_MIN_HEIGHT );
	if ( G.dim & 2 )
		fl_winminsize( GUI.run_form_2d->run_2d->window,
					   GI.WIN_MIN_2D_WIDTH, GI.WIN_MIN_HEIGHT );

	if ( G.dim & 1 )
		fl_set_button( GUI.run_form_1d->full_scale_button_1d, 1 );
	if ( G.dim & 2 )
		fl_set_button( GUI.run_form_2d->full_scale_button_2d, 1 );

	/* Load fonts */

	fonts_init( );

	/* Create the canvases for the axes and for drawing the data */

	if ( ! G.is_init || G.dim & 1 )
	{
		setup_canvas( &G1.x_axis, GUI.run_form_1d->x_axis_1d );
		setup_canvas( &G1.y_axis, GUI.run_form_1d->y_axis_1d );
		setup_canvas( &G1.canvas, GUI.run_form_1d->canvas_1d );
	}

	if ( G.dim & 2 )
	{
		setup_canvas( &G2.x_axis, GUI.run_form_2d->x_axis_2d );
		setup_canvas( &G2.y_axis, GUI.run_form_2d->y_axis_2d );
		setup_canvas( &G2.z_axis, GUI.run_form_2d->z_axis_2d );
		setup_canvas( &G2.canvas, GUI.run_form_2d->canvas_2d );
	}

	if ( G.is_init )
	{
		if ( G.dim & 1 )
			fl_set_form_atclose( GUI.run_form_1d->run_1d,
								 run_form_close_handler, NULL );
		if ( G.dim & 2 )
			fl_set_form_atclose( GUI.run_form_2d->run_2d,
								 run_form_close_handler, NULL );
		G_struct_init( );
	}

	/* Make sure the window gets drawn correctly immediately */

	if ( ! G.is_init || G.dim & 1 )
		redraw_all_1d( );
	if ( G.dim & 2 )
		redraw_all_2d( );

	if ( ! G.is_init || G.dim & 1 )
		fl_raise_form( GUI.run_form_1d->run_1d );
	else
		fl_raise_form( GUI.run_form_2d->run_2d );
	G.is_fully_drawn = SET;
}


/*-----------------------------------------------------------------------*/
/* Loads user defined font for the axis and label. If this font can't be */
/* loaded tries some fonts hopefully available on all machines.          */
/*-----------------------------------------------------------------------*/

static void fonts_init( void )
{
	XCharStruct font_prop;
	int dummy;


	if ( ! G.is_init )
		return;

	if ( * ( ( char * ) xresources[ AXISFONT ].var ) != '\0' )
		G.font = XLoadQueryFont( G.d, ( char * ) xresources[ AXISFONT ].var );

	if ( ! G.font )
		G.font = XLoadQueryFont( G.d, GI.DEFAULT_AXISFONT_1 );
	if ( ! G.font )
		G.font = XLoadQueryFont( G.d, GI.DEFAULT_AXISFONT_2 );
	if ( ! G.font )
		G.font = XLoadQueryFont( G.d, GI.DEFAULT_AXISFONT_3 );

	if ( G.font )
		XTextExtents( G.font, "Xp", 2, &dummy, &G.font_asc, &G.font_desc,
					  &font_prop );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void set_default_sizes( void )
{
	int flags;


	/* If the display windows have never been shown before evaluate the
	   positions and sizes specified on the command line */

	if ( ! display_has_been_shown )
	{
		if ( * ( ( char * ) xresources[ DISPLAYGEOMETRY ].var ) != '\0' )
		{
			flags = XParseGeometry(
								( char * ) xresources[ DISPLAYGEOMETRY ].var,
								&GI.display_x, &GI.display_y,
								&GI.display_w, &GI.display_h );

			if ( WidthValue & flags && HeightValue & flags )
				GI.is_size = SET;

			if ( XValue & flags && YValue & flags )
			{
				GI.display_x += GUI.border_offset_x - 1;
				GI.display_y += GUI.border_offset_y - 1;
				GI.is_pos = SET;
			}
		}

		if ( * ( ( char * ) xresources[ DISPLAY1DGEOMETRY ].var ) != '\0' )
		{
			flags = XParseGeometry(
								( char * ) xresources[ DISPLAY1DGEOMETRY ].var,
								&GI.display_1d_x, &GI.display_1d_y,
								&GI.display_1d_w, &GI.display_1d_h );

			if ( WidthValue & flags && HeightValue & flags )
			{
				if ( GI.display_1d_w < GI.WIN_MIN_1D_WIDTH )
					GI.display_1d_w = GI.WIN_MIN_1D_WIDTH;
				if ( GI.display_1d_h < GI.WIN_MIN_HEIGHT )
					GI.display_1d_h = GI.WIN_MIN_HEIGHT;
				GI.is_1d_size = SET;
			}

			if ( XValue & flags && YValue & flags )
			{
				GI.display_1d_x += GUI.border_offset_x - 1;
				GI.display_1d_y += GUI.border_offset_y - 1;
				GI.is_1d_pos = SET;
			}
		}

		if ( * ( ( char * ) xresources[ DISPLAY2DGEOMETRY ].var ) != '\0' )
		{
			flags = XParseGeometry(
								( char * ) xresources[ DISPLAY2DGEOMETRY ].var,
								&GI.display_2d_x, &GI.display_2d_y,
								&GI.display_2d_w, &GI.display_2d_h );

			if ( WidthValue & flags && HeightValue & flags )
			{
				if ( GI.display_2d_w < GI.WIN_MIN_2D_WIDTH )
					GI.display_2d_w = GI.WIN_MIN_2D_WIDTH;
				if ( GI.display_2d_h < GI.WIN_MIN_HEIGHT )
					GI.display_2d_h = GI.WIN_MIN_HEIGHT;
				GI.is_2d_size = SET;
			}

			if ( XValue & flags && YValue & flags )
			{
				GI.display_2d_x += GUI.border_offset_x - 1;
				GI.display_2d_y += GUI.border_offset_y - 1;
				GI.is_2d_pos = SET;
			}
		}
	}

	if ( GI.is_pos )
	{
		if ( ! GI.is_1d_pos )
		{
			GI.display_1d_x = GI.display_x;
			GI.display_1d_y = GI.display_y;
		}
				
		if ( ! GI.is_2d_pos )
		{
			GI.display_2d_x = GI.display_x;
			GI.display_2d_y = GI.display_y;
		}
	}

	if ( GI.is_size )
	{
		if ( ! GI.is_1d_size )
		{
			GI.display_1d_w = GI.display_w;
			if ( GI.display_1d_w < GI.WIN_MIN_1D_WIDTH )
				GI.display_1d_w = GI.WIN_MIN_1D_WIDTH;

			GI.display_1d_h = GI.display_h;
			if ( GI.display_1d_h < GI.WIN_MIN_HEIGHT )
				GI.display_1d_w = GI.WIN_MIN_HEIGHT;
		}

		if ( ! GI.is_2d_size )
		{
			GI.display_2d_w = GI.display_w;
			if ( GI.display_2d_w < GI.WIN_MIN_2D_WIDTH )
				GI.display_2d_w = GI.WIN_MIN_2D_WIDTH;

			GI.display_2d_h = GI.display_h;
			if ( GI.display_2d_h < GI.WIN_MIN_HEIGHT )
				GI.display_2d_w = GI.WIN_MIN_HEIGHT;
		}
	}

	if ( G.dim & 1 || ! G.is_init )
	{
		if ( GI.is_size || GI.is_1d_size )
			fl_set_form_size( GUI.run_form_1d->run_1d,
							  GI.display_1d_w, GI.display_1d_h );
		if ( GI.is_pos || GI.is_1d_pos )
			fl_set_form_position( GUI.run_form_1d->run_1d,
								  GI.display_1d_x, GI.display_1d_y );
	}

	if ( G.dim & 2 )
	{
		if ( GI.is_size || GI.is_2d_size )
			fl_set_form_size( GUI.run_form_2d->run_2d,
							  GI.display_2d_w, GI.display_2d_h );
		if ( GI.is_pos || GI.is_2d_pos )
			fl_set_form_position( GUI.run_form_2d->run_2d,
								  GI.display_2d_x, GI.display_2d_y );
	}
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void set_defaults( void )
{
	if ( GUI.G_Funcs.size == LOW )
	{
		GI.WIN_MIN_1D_WIDTH   = 300;
		GI.WIN_MIN_2D_WIDTH   = 350;
		GI.WIN_MIN_HEIGHT     = 380;
		GI.SMALL_FONT_SIZE    = FL_TINY_SIZE;
		GI.DEFAULT_AXISFONT_1 = "*-lucida-bold-r-normal-sans-10-*";
		GI.DEFAULT_AXISFONT_2 = "lucidasanstypewriter-10";
		GI.DEFAULT_AXISFONT_3 = "fixed";

		G.scale_tick_dist   =  4;
		G.short_tick_len    =  3;
		G.medium_tick_len   =  6;
		G.long_tick_len     =  8;
		G.label_dist        =  5;
		G.x_scale_offset    = 12;
		G.y_scale_offset    = 12;
		G.z_scale_offset    = 25;
		G.z_line_offset     =  5;
		G.z_line_width      =  8;
		G.enlarge_box_width =  3;
	}
	else
	{
		GI.WIN_MIN_1D_WIDTH   = 400;
		GI.WIN_MIN_2D_WIDTH   = 500;
		GI.WIN_MIN_HEIGHT     = 435;
		GI.SMALL_FONT_SIZE    = FL_SMALL_SIZE;
		GI.DEFAULT_AXISFONT_1 = "*-lucida-bold-r-normal-sans-14-*";
		GI.DEFAULT_AXISFONT_2 = "lucidasanstypewriter-14";
		GI.DEFAULT_AXISFONT_3 = "fixed";

		G.scale_tick_dist   =   6;
		G.short_tick_len    =   5;
		G.medium_tick_len   =  10;
		G.long_tick_len     =  14;
		G.label_dist        =   7;
		G.x_scale_offset    =  20;
		G.y_scale_offset    =  21;
		G.z_scale_offset    =  46;
		G.z_line_offset     =  10;
		G.z_line_width      =  14;
		G.enlarge_box_width =   5;
	}

	GI.is_pos = UNSET;
	GI.is_size = UNSET;
	GI.is_1d_pos = UNSET;
	GI.is_1d_size = UNSET;
	GI.is_2d_pos = UNSET;
	GI.is_2d_size = UNSET;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void forms_adapt( void )
{
	char *pixmap_file;


	if ( ! G.is_init )
	{
		fl_hide_object( GUI.run_form_1d->curve_1_button_1d );
		fl_hide_object( GUI.run_form_1d->curve_2_button_1d );
		fl_hide_object( GUI.run_form_1d->curve_3_button_1d );
		fl_hide_object( GUI.run_form_1d->curve_4_button_1d );

		fl_hide_object( GUI.run_form_1d->undo_button_1d );
		fl_hide_object( GUI.run_form_1d->print_button_1d );
		fl_hide_object( GUI.run_form_1d->full_scale_button_1d );
	}
	else
	{
		pixmap_file = get_string( "%s%sundo.xpm", auxdir, slash( auxdir ) );

		if ( access( pixmap_file, R_OK ) == 0 )
		{
			if ( G.dim & 1 )
				fl_set_pixmapbutton_file( GUI.run_form_1d->undo_button_1d,
										  pixmap_file );
			if ( G.dim & 2 )
				fl_set_pixmapbutton_file( GUI.run_form_2d->undo_button_2d,
										  pixmap_file );

			if ( G.dim & 1 )
				fl_set_object_lsize( GUI.run_form_1d->undo_button_1d,
									 GI.SMALL_FONT_SIZE );
			if ( G.dim & 2 )
				fl_set_object_lsize( GUI.run_form_2d->undo_button_2d,
									 GI.SMALL_FONT_SIZE );

			if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			{
				if ( G.dim & 1 )
					fl_set_object_helper( GUI.run_form_1d->undo_button_1d,
										  "Undo last rescaling operation" );
				if ( G.dim & 2 )
					fl_set_object_helper( GUI.run_form_2d->undo_button_2d,
										  "Undo last rescaling operation" );
			}

			if ( G.dim & 2 )
			{
				fl_set_pixmapbutton_file( GUI.cut_form->cut_undo_button,
										  pixmap_file );
				fl_set_object_lsize( GUI.cut_form->cut_undo_button,
									 GI.SMALL_FONT_SIZE );
				if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
					fl_set_object_helper( GUI.cut_form->cut_undo_button,
										  "Undo last rescaling operation" );
			}
		}
		T_free( pixmap_file );

		pixmap_file = get_string( "%s%sprinter.xpm", auxdir, slash( auxdir ) );

		if ( access( pixmap_file, R_OK ) == 0 )
		{
			if ( G.dim & 1 )
				fl_set_pixmapbutton_file( GUI.run_form_1d->print_button_1d,
										  pixmap_file );
			if ( G.dim & 2 )
				fl_set_pixmapbutton_file( GUI.run_form_2d->print_button_2d,
										  pixmap_file );

			if ( G.dim & 1 )
				fl_set_object_lsize( GUI.run_form_1d->print_button_1d,
									 GI.SMALL_FONT_SIZE );
			if ( G.dim & 2 )
				fl_set_object_lsize( GUI.run_form_2d->print_button_2d,
									 GI.SMALL_FONT_SIZE );

			if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			{
				if ( G.dim & 1 )
					fl_set_object_helper( GUI.run_form_1d->print_button_1d,
										  "Print window" );
				if ( G.dim & 2 )
					fl_set_object_helper( GUI.run_form_2d->print_button_2d,
										  "Print window" );
			}

			if ( G.dim & 2 )
			{
				fl_set_pixmapbutton_file( GUI.cut_form->cut_print_button,
										  pixmap_file );
				fl_set_object_lsize( GUI.cut_form->cut_print_button,
									 GI.SMALL_FONT_SIZE );
				if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
					fl_set_object_helper( GUI.cut_form->cut_print_button,
										  "Print window" );
			}
		}

		T_free( pixmap_file );

		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
		{
			if ( G.dim & 1 )
				fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
									  "Switch off automatic rescaling" );
			if ( G.dim & 2 )
				fl_set_object_helper( GUI.run_form_2d->full_scale_button_2d,
									  "Switch off automatic rescaling" );
		}
	}

	if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
	{
		if ( GUI.stop_button_mask == 0 )
		{
			if ( G.dim & 1 || ! G.is_init )
				fl_set_object_helper( GUI.run_form_1d->stop_1d,
									  "Stop the experiment" );
			if ( G.dim & 2 )
				fl_set_object_helper( GUI.run_form_2d->stop_2d,
									  "Stop the experiment" );
		}
		else if ( GUI.stop_button_mask == FL_LEFT_MOUSE )
		{
			if ( G.dim & 1 || ! G.is_init )
				fl_set_object_helper( GUI.run_form_1d->stop_1d,
									  "Stop the experiment\n"
									  "Use left mouse button" );
			if ( G.dim & 2 )
				fl_set_object_helper( GUI.run_form_2d->stop_2d,
									  "Stop the experiment\n"
									  "Use left mouse button" );
		}
		else if ( GUI.stop_button_mask == FL_MIDDLE_MOUSE )
		{
			if ( G.dim & 1 || ! G.is_init )
				fl_set_object_helper( GUI.run_form_1d->stop_1d,
									  "Stop the experiment\n"
									  "Use middle mouse button" );
			if ( G.dim & 2 )
				fl_set_object_helper( GUI.run_form_2d->stop_2d,
									  "Stop the experiment\n"
									  "Use middle mouse button" );
		}
		else if ( GUI.stop_button_mask == FL_RIGHT_MOUSE )
		{
			if ( G.dim & 1 || ! G.is_init )
				fl_set_object_helper( GUI.run_form_1d->stop_1d,
									  "Stop the experiment\n"
									  "Use right mouse button" );
			if ( G.dim & 2 )
				fl_set_object_helper( GUI.run_form_2d->stop_2d,
									  "Stop the experiment\n"
									  "Use right mouse button" );
		}
	}

	if ( G.dim == 2 && ! ( Internals.cmdline_flags & NO_BALLOON ) )
	{
		fl_set_object_helper( GUI.cut_form->cut_close_button,
							  "Close the window" );
		fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
							  "Switch off automatic rescaling" );
	}

	if ( G.dim & 1 )
	{
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
		{
			fl_set_object_helper( GUI.run_form_1d->curve_1_button_1d,
								 "Exempt curve 1 from\nrescaling operations" );
			fl_set_object_helper( GUI.run_form_1d->curve_2_button_1d,
								 "Exempt curve 2 from\nrescaling operations" );
			fl_set_object_helper( GUI.run_form_1d->curve_3_button_1d,
								 "Exempt curve 3 from\nrescaling operations" );
			fl_set_object_helper( GUI.run_form_1d->curve_4_button_1d,
								 "Exempt curve 4 from\nrescaling operations" );
		}

		fl_set_object_callback( GUI.run_form_1d->print_button_1d,
								print_it, 1 );
	}

	if ( G.dim & 2 )
	{
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
		{
			fl_set_object_helper( GUI.run_form_2d->curve_1_button_2d,
                                  "Hide curve 1" );
			fl_set_object_helper( GUI.run_form_2d->curve_2_button_2d,
                                  "Show curve 2" );
			fl_set_object_helper( GUI.run_form_2d->curve_3_button_2d,
                                  "Show curve 3" );
			fl_set_object_helper( GUI.run_form_2d->curve_4_button_2d,
                                  "Show curve 4" );
		}

		fl_set_button( GUI.run_form_2d->curve_1_button_2d, 1 );
		fl_set_button( GUI.run_form_2d->curve_2_button_2d, 0 );
		fl_set_button( GUI.run_form_2d->curve_3_button_2d, 0 );
		fl_set_button( GUI.run_form_2d->curve_4_button_2d, 0 );

		fl_set_object_callback( GUI.run_form_2d->print_button_2d,
								print_it, 2 );
	}

	/* fdesign is unable to set the box type attributes for canvases... */

	if ( G.dim & 1 || ! G.is_init )
	{
		fl_set_canvas_decoration( GUI.run_form_1d->x_axis_1d, FL_FRAME_BOX );
		fl_set_canvas_decoration( GUI.run_form_1d->y_axis_1d, FL_FRAME_BOX );
		fl_set_canvas_decoration( GUI.run_form_1d->canvas_1d, FL_NO_FRAME );
	}

	if ( G.dim & 2 )
	{
		fl_set_canvas_decoration( GUI.run_form_2d->x_axis_2d, FL_FRAME_BOX );
		fl_set_canvas_decoration( GUI.run_form_2d->y_axis_2d, FL_FRAME_BOX );
		fl_set_canvas_decoration( GUI.run_form_2d->canvas_2d, FL_NO_FRAME );
		fl_set_canvas_decoration( GUI.run_form_2d->z_axis_2d, FL_FRAME_BOX );

		fl_set_canvas_decoration( GUI.cut_form->cut_x_axis, FL_FRAME_BOX );
		fl_set_canvas_decoration( GUI.cut_form->cut_y_axis, FL_FRAME_BOX );
		fl_set_canvas_decoration( GUI.cut_form->cut_z_axis, FL_FRAME_BOX );
		fl_set_canvas_decoration( GUI.cut_form->cut_canvas, FL_NO_FRAME );
	}

	/* Show only buttons really needed */

	if ( G.is_init )
	{
		if ( G.dim & 1 )
		{
			if ( G1.nc < 4 )
				fl_hide_object( GUI.run_form_1d->curve_4_button_1d );
			if ( G1.nc < 3 )
				fl_hide_object( GUI.run_form_1d->curve_3_button_1d );
			if ( G1.nc < 2 )
			{
				fl_hide_object( GUI.run_form_1d->curve_2_button_1d );
				fl_hide_object( GUI.run_form_1d->curve_1_button_1d );
			}
		}

		if ( G.dim & 2 )
		{
			if ( G2.nc < 4 )
				fl_hide_object( GUI.run_form_2d->curve_4_button_2d );
			if ( G2.nc < 3 )
				fl_hide_object( GUI.run_form_2d->curve_3_button_2d );
			if ( G2.nc < 2 )
			{
				fl_hide_object( GUI.run_form_2d->curve_2_button_2d );
				fl_hide_object( GUI.run_form_2d->curve_1_button_2d );
			}
		}
	}
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int run_form_close_handler( FL_FORM *a, void *b )
{
	UNUSED_ARGUMENT( a );
	UNUSED_ARGUMENT( b );

	if ( Internals.child_pid == 0 )          /* if child has already exited */
		run_close_button_callback( GUI.run_form_1d->stop_1d, 0 );
	return FL_IGNORE;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void G_struct_init( void )
{
	static bool first_time = SET;
	static int cursor_1d[ 8 ];
	static int cursor_2d[ 8 ];
	int i, x, y;
	unsigned int keymask;


	/* Get the current mouse button state (useful and raw) */

	G.button_state = 0;
	G.button_state = 0;

	G.drag_canvas = DRAG_NONE;

	fl_get_mouse( &x, &y, &keymask );

	if ( keymask & Button1Mask )
		G.raw_button_state |= 1;
	if ( keymask & Button2Mask )
		G.raw_button_state |= 2;
	if ( keymask & Button3Mask )
		G.raw_button_state |= 4;

	/* Define colours for the curves (this could be made user-configurable)
	   - this must happen before color creation */

	G.colors[ 0 ] = FL_TOMATO;
	G.colors[ 1 ] = FL_GREEN;
	G.colors[ 2 ] = FL_YELLOW;
	G.colors[ 3 ] = FL_CYAN;

	/* Create the different cursors and the colours needed for 2D displays */

	if ( first_time )
	{
		cursor_1d[ ZOOM_BOX_CURSOR ] = G1.cursor[ ZOOM_BOX_CURSOR ] =
					fl_create_bitmap_cursor( cursor1_bits, cursor1_bits,
											 cursor1_width, cursor1_height,
						   					 cursor1_x_hot, cursor1_y_hot );
		cursor_1d[ MOVE_HAND_CURSOR ] = G1.cursor[ MOVE_HAND_CURSOR ] =
					fl_create_bitmap_cursor( cursor2_bits, cursor2_bits,
											 cursor2_width, cursor2_height,
											 cursor2_x_hot, cursor2_y_hot );
		cursor_1d[ ZOOM_LENS_CURSOR ] = G1.cursor[ ZOOM_LENS_CURSOR ] =
					fl_create_bitmap_cursor( cursor3_bits, cursor3_bits,
											 cursor3_width, cursor3_height,
											 cursor3_x_hot, cursor3_y_hot );
		cursor_1d[ CROSSHAIR_CURSOR ] = G1.cursor[ CROSSHAIR_CURSOR ] =
					fl_create_bitmap_cursor( cursor4_bits, cursor4_bits,
											 cursor4_width, cursor4_height,
											 cursor4_x_hot, cursor4_y_hot );
		cursor_1d[ TARGET_CURSOR ] = G1.cursor[ TARGET_CURSOR ] =
					fl_create_bitmap_cursor( cursor5_bits, cursor5_bits,
											 cursor5_width, cursor5_height,
											 cursor5_x_hot, cursor5_y_hot );
		cursor_1d[ ARROW_UP_CURSOR ] = G1.cursor[ ARROW_UP_CURSOR ] =
					fl_create_bitmap_cursor( cursor6_bits, cursor6_bits,
											 cursor6_width, cursor6_height,
											 cursor6_x_hot, cursor6_y_hot );
		cursor_1d[ ARROW_RIGHT_CURSOR ] = G1.cursor[ ARROW_RIGHT_CURSOR ] =
					fl_create_bitmap_cursor( cursor7_bits, cursor7_bits,
											 cursor7_width, cursor7_height,
											 cursor7_x_hot, cursor7_y_hot );

		cursor_2d[ ZOOM_BOX_CURSOR ] = G2.cursor[ ZOOM_BOX_CURSOR ] =
					fl_create_bitmap_cursor( cursor1_bits, cursor1_bits,
											 cursor1_width, cursor1_height,
						   					 cursor1_x_hot, cursor1_y_hot );
		cursor_2d[ MOVE_HAND_CURSOR ] = G2.cursor[ MOVE_HAND_CURSOR ] =
					fl_create_bitmap_cursor( cursor2_bits, cursor2_bits,
											 cursor2_width, cursor2_height,
											 cursor2_x_hot, cursor2_y_hot );
		cursor_2d[ ZOOM_LENS_CURSOR ] = G2.cursor[ ZOOM_LENS_CURSOR ] =
					fl_create_bitmap_cursor( cursor3_bits, cursor3_bits,
											 cursor3_width, cursor3_height,
											 cursor3_x_hot, cursor3_y_hot );
		cursor_2d[ CROSSHAIR_CURSOR ] = G2.cursor[ CROSSHAIR_CURSOR ] =
					fl_create_bitmap_cursor( cursor4_bits, cursor4_bits,
											 cursor4_width, cursor4_height,
											 cursor4_x_hot, cursor4_y_hot );
		cursor_2d[ TARGET_CURSOR ] = G2.cursor[ TARGET_CURSOR ] =
					fl_create_bitmap_cursor( cursor5_bits, cursor5_bits,
											 cursor5_width, cursor5_height,
											 cursor5_x_hot, cursor5_y_hot );
		cursor_2d[ ARROW_UP_CURSOR ] = G2.cursor[ ARROW_UP_CURSOR ] =
					fl_create_bitmap_cursor( cursor6_bits, cursor6_bits,
											 cursor6_width, cursor6_height,
											 cursor6_x_hot, cursor6_y_hot );
		cursor_2d[ ARROW_RIGHT_CURSOR ] = G2.cursor[ ARROW_RIGHT_CURSOR ] =
					fl_create_bitmap_cursor( cursor7_bits, cursor7_bits,
											 cursor7_width, cursor7_height,
											 cursor7_x_hot, cursor7_y_hot );

		create_colors( );
		memcpy( G_2d_stored->color_list, G2.color_list, sizeof G2.color_list );

#if defined WITH_HTTP_SERVER
		create_color_hash( );
		G_stored->color_hash = G.color_hash;
		G_stored->color_hash_size = G.color_hash_size;
#endif
	}
	else
		for ( i = 0; i < 7; i++ )
		{
			G1.cursor[ i ] = cursor_1d[ i ];
			G2.cursor[ i ] = cursor_2d[ i ];
		}

	G1.is_fs = SET;
	G1.scale_changed = UNSET;

	G2.is_fs = SET;
	G2.scale_changed = UNSET;

	G1.rw_min = HUGE_VAL;
	G1.rw_max = - HUGE_VAL;
	G1.is_scale_set = UNSET;

	G2.rw_min = HUGE_VAL;
	G2.rw_max = - HUGE_VAL;
	G2.is_scale_set = UNSET;

	if ( G.dim & 1 && G1.label[ Y ] != NULL && G.font != NULL )
		create_label_pixmap( &G1.y_axis, Y, G1.label[ Y ] );

	if ( G.dim & 2 && G2.label[ Y ] != NULL && G.font != NULL )
		create_label_pixmap( &G2.y_axis, Y, G2.label[ Y ] );

	if ( G.dim & 2 && G2.label[ Z ] != NULL && G.font != NULL )
		create_label_pixmap( &G2.z_axis, Z, G2.label[ Z ] );

	if ( G.dim & 1 )
		G_init_curves_1d( );

	if ( G.dim & 2 )
		G_init_curves_2d( );

	first_time = UNSET;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void G_init_curves_1d( void )
{
	int i, j;
	Curve_1d *cv;
	unsigned int depth;


	for ( i = 0; i < G1.nc; i++ )
		G1.curve[ i ] = NULL;

	depth = fl_get_canvas_depth( G1.canvas.obj );

	for ( i = 0; i < 5; i++ )
		fl_set_cursor_color( G1.cursor[ i ], FL_RED, FL_WHITE );

	for ( i = 0; i < G1.nc; i++ )
	{
		/* Allocate memory for the curve and its data */

		cv = G1.curve[ i ] = CURVE_1D_P T_malloc( sizeof *cv );

		cv->points = NULL;
		cv->xpoints = NULL;

		/* Create a GC for drawing the curve and set its colour */

		cv->gc = XCreateGC( G.d, G1.canvas.pm, 0, 0 );
		XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ i ] ) );

		/* Create pixmaps for the out-of-display arrows */

		cv->up_arrow =
			XCreatePixmapFromBitmapData( G.d, G1.canvas.pm, up_arrow_bits,
										 up_arrow_width, up_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		G.up_arrow_w = up_arrow_width;
		G.up_arrow_h = up_arrow_width;

		cv->down_arrow =
			XCreatePixmapFromBitmapData( G.d, G1.canvas.pm, down_arrow_bits,
										 down_arrow_width, down_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		G.down_arrow_w = down_arrow_width;
		G.down_arrow_h = down_arrow_width;

		cv->left_arrow =
			XCreatePixmapFromBitmapData( G.d, G1.canvas.pm, left_arrow_bits,
										 left_arrow_width, left_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		G.left_arrow_w = left_arrow_width;
		G.left_arrow_h = left_arrow_width;

		cv->right_arrow =
			XCreatePixmapFromBitmapData( G.d, G1.canvas.pm, right_arrow_bits,
										 right_arrow_width, right_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		G.right_arrow_w = right_arrow_width;
		G.right_arrow_h = right_arrow_width;

		/* Create a GC for the font and set the appropriate colour */

		cv->font_gc = XCreateGC( G.d, FL_ObjWin( G1.canvas.obj ), 0, 0 );
		if ( G.font != NULL )
			XSetFont( G.d, cv->font_gc, G.font->fid );
		XSetForeground( G.d, cv->font_gc, fl_get_pixel( G.colors[ i ] ) );
		XSetBackground( G.d, cv->font_gc, fl_get_pixel( FL_BLACK ) );

		/* Set the scaling factors for the curve */

		cv->s2d[ X ] = ( double ) ( G1.canvas.w - 1 )
					   / ( double ) ( G1.nx - 1 );
		cv->s2d[ Y ] = ( double ) ( G1.canvas.h - 1 );

		cv->shift[ X ] = cv->shift[ Y ] = 0.0;

		cv->count = 0;
		cv->active = SET;
		cv->can_undo = UNSET;

		/* Finally get memory for the data */

		cv->points = SCALED_POINT_P T_malloc( G1.nx * sizeof *cv->points );

		for ( j = 0; j < G1.nx; j++ )          /* no points are known in yet */
			cv->points[ j ].exist = UNSET;

		cv->xpoints = XPOINT_P T_malloc( G1.nx * sizeof *cv->xpoints );
	}
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void G_init_curves_2d( void )
{
	int i, j;
	Curve_2d *cv;
	Scaled_Point *sp;
	unsigned int depth;


	for ( i = 0; i < G2.nc; i++ )
		G2.curve_2d[ i ] = NULL;

	depth = fl_get_canvas_depth( G2.canvas.obj );

	for ( i = 0; i < 7; i++ )
		fl_set_cursor_color( G2.cursor[ i ], FL_BLACK, FL_WHITE );

	for ( i = 0; i < G2.nc; i++ )
	{
		/* Allocate memory for the curve */

		cv = G2.curve_2d[ i ] = CURVE_2D_P T_malloc( sizeof *cv );

		cv->points = NULL;
		cv->xpoints = NULL;
		cv->xpoints_s = NULL;

		/* Create a GC for drawing the curve and set its colour */

		cv->gc = XCreateGC( G.d, G2.canvas.pm, 0, 0 );
		XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ i ] ) );

		/* Create pixmaps for the out-of-display arrows */

		cv->up_arrow =
			XCreatePixmapFromBitmapData( G.d, G2.canvas.pm, up_arrow_bits,
										 up_arrow_width, up_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );
		G.up_arrow_w = up_arrow_width;
		G.up_arrow_h = up_arrow_width;

		cv->down_arrow =
			XCreatePixmapFromBitmapData( G.d, G2.canvas.pm, down_arrow_bits,
										 down_arrow_width, down_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );

		G.down_arrow_w = down_arrow_width;
		G.down_arrow_h = down_arrow_width;

		cv->left_arrow =
			XCreatePixmapFromBitmapData( G.d, G2.canvas.pm, left_arrow_bits,
										 left_arrow_width, left_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );

		G.left_arrow_w = left_arrow_width;
		G.left_arrow_h = left_arrow_width;

		cv->right_arrow =
			XCreatePixmapFromBitmapData( G.d, G2.canvas.pm, right_arrow_bits,
										 right_arrow_width, right_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );

		G.right_arrow_w = right_arrow_width;
		G.right_arrow_h = right_arrow_width;

		/* Create a GC for the font and set the appropriate colour */

		cv->font_gc = XCreateGC( G.d, FL_ObjWin( G2.canvas.obj ), 0, 0 );
		if ( G.font != NULL )
			XSetFont( G.d, cv->font_gc, G.font->fid );
		XSetForeground( G.d, cv->font_gc, fl_get_pixel( FL_WHITE ) );
		XSetBackground( G.d, cv->font_gc, fl_get_pixel( FL_BLACK ) );

		/* Set the scaling factors for the curve */

		cv->s2d[ X ] = ( double ) ( G2.canvas.w - 1 )
					   / ( double ) ( G2.nx - 1 );
		cv->s2d[ Y ] = ( double ) ( G2.canvas.h - 1 )
					   / ( double ) ( G2.ny - 1 );
		cv->s2d[ Z ] = ( double ) ( G2.z_axis.h - 1 );

		cv->shift[ X ] = cv->shift[ Y ] = cv->shift[ Z ] = 0.0;

		cv->z_factor = 1.0;

		cv->rwc_start[ X ] = G2.rwc_start[ X ];
		cv->rwc_start[ Y ] = G2.rwc_start[ Y ];
		cv->rwc_delta[ X ] = G2.rwc_delta[ X ];
		cv->rwc_delta[ Y ] = G2.rwc_delta[ Y ];

		cv->count = 0;
		cv->active = SET;
		cv->can_undo = UNSET;

		cv->is_fs = SET;
		cv->scale_changed = UNSET;

		cv->rw_min = HUGE_VAL;
		cv->rw_max = - HUGE_VAL;
		cv->is_scale_set = UNSET;

		/* Now get also memory for the data */

		cv->points = SCALED_POINT_P T_malloc( G2.nx * G2.ny
											  * sizeof *cv->points );

		for ( sp = cv->points, j = 0; j < G2.nx * G2.ny; sp++, j++ )
			sp->exist = UNSET;

		cv->xpoints = XPOINT_P T_malloc( G2.nx * G2.ny * sizeof *cv->xpoints );
		cv->xpoints_s = XPOINT_P T_malloc( G2.nx * G2.ny
										   * sizeof *cv->xpoints_s );

		cv->marker_2d = NULL;
	}
}


/*----------------------------------------------------------------------*/
/* This routine creates a pixmap with the label for either the y- or z- */
/* axis (set 'coord' to 1 for y and 2 for z). The problem is that it's  */
/* not possible to draw rotated text so we have to write the text to a  */
/* pixmap and then rotate this pixmap 'by hand'.                        */
/*----------------------------------------------------------------------*/

void create_label_pixmap( Canvas *c, int coord, char *label )
{
	Pixmap pm;
	int width, height;
	int i, j, k;
	int r_coord = coord;

	/* Make sure we don't do something stupid... */

	fsc2_assert( ( coord == Y && ( c == &G1.y_axis || c == &G2.y_axis ) ) ||
				 ( coord == Z &&
				   ( c == &G2.z_axis || c == &G2.cut_z_axis ) ) );

	/* Distinguish between labels for the primary window and the cut window
	   (this function is never called for the cut windows y-axis) */

	if ( c == &G2.cut_z_axis )
		r_coord += 3;

	/* Get size for intermediate pixmap */

	width = XTextWidth( G.font, label, strlen( label ) ) + 10;
	height = G.font_asc + G.font_desc + 5;

	/* Create the intermediate pixmap, fill with the colour of the axis canvas
	   and draw the text */

    pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), width, height,
						fl_get_canvas_depth( c->obj ) );

	XFillRectangle( G.d, pm, c->gc, 0, 0, width, height );
	XDrawString( G.d, pm, c->font_gc, 5, height - 1 - G.font_desc,
				 label, strlen( label ) );

	/* Create the real pixmap for the label */

	if ( c == &G1.y_axis )
	{
		G1.label_pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), height, width,
									 fl_get_canvas_depth( c->obj ) );
		G1.label_w = i2ushrt( height );
		G1.label_h = i2ushrt( width );
	}
	else
	{
		G2.label_pm[ r_coord ] = XCreatePixmap( G.d, FL_ObjWin( c->obj ),
											   height, width,
											   fl_get_canvas_depth( c->obj ) );
		G2.label_w[ r_coord ] = i2ushrt( height );
		G2.label_h[ r_coord ] = i2ushrt( width );
	}

	/* Now copy the contents of the intermediate pixmap to the final pixmap
	   but rotated by 90 degree ccw */

	if ( c == &G1.y_axis )
		for ( i = 0, k = width - 1; i < width; k--, i++ )
			for ( j = 0; j < height; j++ )
				XCopyArea( G.d, pm, G1.label_pm, c->gc, i, j, 1, 1, j, k );
	else
		for ( i = 0, k = width - 1; i < width; k--, i++ )
			for ( j = 0; j < height; j++ )
				XCopyArea( G.d, pm, G2.label_pm[ r_coord ], c->gc,
						   i, j, 1, 1, j, k );

	XFreePixmap( G.d, pm );
}


/*--------------------------------------------------------*/
/* Removes the window for displaying measurement results. */
/*--------------------------------------------------------*/

void stop_graphics( void )
{
	int i;
	Marker_1D *m, *mn;
	Marker_2D *m2, *mn2;


	G.is_fully_drawn = UNSET;

	if ( G.is_init )
	{
		graphics_free( );

		if ( G.dim & 1 )
			for ( i = X; i <= Y; i++ )
				G1.label[ i ] = CHAR_P T_free( G1.label[ i ] );
		if ( G.dim & 2 )
			for ( i = X; i <= Z; i++ )
				G2.label[ i ] = CHAR_P T_free( G2.label[ i ] );

		if ( G.font )
			XFreeFont( G.d, G.font );

		if ( GUI.run_form_1d )
		{
			if ( G1.x_axis.obj )
			{
				canvas_off( &G1.x_axis, GUI.run_form_1d->x_axis_1d );
				G1.x_axis.obj = NULL;
			}
			if ( G1.y_axis.obj )
			{
				canvas_off( &G1.y_axis, GUI.run_form_1d->y_axis_1d );
				G1.y_axis.obj = NULL;
			}
			if ( G1.canvas.obj )
			{
				canvas_off( &G1.canvas, GUI.run_form_1d->canvas_1d );
				G1.canvas.obj = NULL;
			}
		}

		if ( GUI.run_form_2d )
		{
			if ( G2.x_axis.obj )
			{
				canvas_off( &G2.x_axis, GUI.run_form_2d->x_axis_2d );
				G2.x_axis.obj = NULL;
			}
			if ( G2.y_axis.obj )
			{
				canvas_off( &G2.y_axis, GUI.run_form_2d->y_axis_2d );
				G2.y_axis.obj = NULL;
			}
			if ( G2.canvas.obj )
			{
				canvas_off( &G2.canvas, GUI.run_form_2d->canvas_2d );
				G2.canvas.obj = NULL;
			}

			if ( G2.z_axis.obj )
			{
				canvas_off( &G2.z_axis, GUI.run_form_2d->z_axis_2d );
				G2.z_axis.obj = NULL;
			}

			cut_form_close( );
		}
	}

	/* Store positions and sizes of display windows (unless they have
	   been set via command line options, we don't want to override the
	   user settings). Afterwards remove the windows. */

	if ( GUI.run_form_1d && fl_form_is_visible( GUI.run_form_1d->run_1d ) )
	{
		if ( ! GI.is_1d_pos )
		{
			GI.display_1d_x = GUI.run_form_1d->run_1d->x;
			GI.display_1d_y = GUI.run_form_1d->run_1d->y;
		}

		if ( ! GI.is_1d_size )
		{
			GI.display_1d_w = GUI.run_form_1d->run_1d->w;
			GI.display_1d_h = GUI.run_form_1d->run_1d->h;
		}

		GI.is_1d_pos = GI.is_1d_size = SET;

		fl_hide_form( GUI.run_form_1d->run_1d );
		fl_free_form( GUI.run_form_1d->run_1d );
		GUI.run_form_1d = NULL;
	}

	if ( GUI.run_form_2d && fl_form_is_visible( GUI.run_form_2d->run_2d ) )
	{
		if ( ! GI.is_2d_pos )
		{
			GI.display_2d_x = GUI.run_form_2d->run_2d->x;
			GI.display_2d_y = GUI.run_form_2d->run_2d->y;
		}

		if ( ! GI.is_2d_size )
		{
			GI.display_2d_w = GUI.run_form_2d->run_2d->w;
			GI.display_2d_h = GUI.run_form_2d->run_2d->h;
		}

		GI.is_2d_pos = GI.is_2d_size = SET;

		fl_hide_form( GUI.run_form_2d->run_2d );

		fl_free_form( GUI.run_form_2d->run_2d );
		GUI.run_form_2d = NULL;
	}

	for ( m = G1.marker_1d; m != NULL; m = mn )
	{
		XFreeGC( G.d, m->gc );
		mn = m->next;
		m = MARKER_1D_P T_free( m );
	}

	for ( i = 0; i < G2.nc; i++ )
	{
		if ( G2.curve_2d[ i ] == NULL )
			break;
;
		for ( m2 = G2.curve_2d[ i ]->marker_2d; m2 != NULL; m = mn )
		{
			XFreeGC( G.d, m2->gc );
			mn2 = m2->next;
			m2 = MARKER_2D_P T_free( m2 );
		}
	}

	if ( G_stored )
	{
		memcpy( &G, G_stored, sizeof G );
		G_stored = GRAPHICS_P T_free( G_stored );
	}

	if ( G_1d_stored )
	{
		memcpy( &G1, G_1d_stored, sizeof G1 );
		G_1d_stored = GRAPHICS_1D_P T_free( G_1d_stored );

		for ( i = X; i <= Y; i++ )
			G1.label[ i ] = NULL;
	}

	if ( G_2d_stored )
	{
		memcpy( &G2, G_2d_stored, sizeof G2 );
		G_2d_stored = GRAPHICS_2D_P T_free( G_2d_stored );

		for ( i = X; i <= Z; i++ )
			G2.label[ i ] = NULL;
	}
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void graphics_free( void )
{
	long i;
	int coord;
	Curve_1d *cv;
	Curve_2d *cv2;


	/* Deallocate memory for pixmaps, scaled data and XPoints. The function
	   must also work correctly when it is called because we run out of
	   memory. The way things are organized after allocating memory for a
	   curve first the graphical elements are created and afterwards memory
	   for the data is allocated. */

	if ( G.dim & 1 )
		for ( i = 0; i < G1.nc; i++ )
		{
			if ( ( cv = G1.curve[ i ] ) == NULL )
				 break;

			XFreeGC( G.d, cv->gc );
			XFreePixmap( G.d, cv->up_arrow );
			XFreePixmap( G.d, cv->down_arrow );
			XFreePixmap( G.d, cv->left_arrow );
			XFreePixmap( G.d, cv->right_arrow );
			XFreeGC( G.d, cv->font_gc );

			T_free( cv->points );
			T_free( cv->xpoints );
			cv = CURVE_1D_P T_free( cv );
		}

	if ( G.dim & 2 )
		for ( i = 0; i < G2.nc; i++ )
		{
			if ( ( cv2 = G2.curve_2d[ i ] ) == NULL )
				break;

			XFreeGC( G.d, cv2->gc );
			XFreePixmap( G.d, cv2->up_arrow );
			XFreePixmap( G.d, cv2->down_arrow );
			XFreePixmap( G.d, cv2->left_arrow );
			XFreePixmap( G.d, cv2->right_arrow );
			XFreeGC( G.d, cv2->font_gc );

			T_free( cv2->points );
			T_free( cv2->xpoints );
			T_free( cv2->xpoints_s );
			cv2 = CURVE_2D_P T_free( cv2 );
		}

	if ( G.font )
	{
		if ( G.dim & 1 )
			if ( G1.label[ Y ] )
				XFreePixmap( G.d, G1.label_pm );
		if ( G.dim & 2 )
		for ( coord = Y; coord <= Z; coord++ )
			if ( G2.label[ coord ] )
				XFreePixmap( G.d, G2.label_pm[ coord ] );
	}
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

static void canvas_off( Canvas *c, FL_OBJECT *obj )
{
	FL_HANDLE_CANVAS ch;


	if ( c == &G1.x_axis || c == &G1.y_axis || c == &G1.canvas )
		ch = canvas_handler_1d;
	else
		ch = canvas_handler_2d;

	fl_remove_canvas_handler( obj, Expose, ch );

	if ( G.is_init )
	{
		if ( G.dim == 1 )
		{
			fl_remove_canvas_handler( obj, ConfigureNotify, ch );
			fl_remove_canvas_handler( obj, ButtonPress, ch );
			fl_remove_canvas_handler( obj, ButtonRelease, ch );
			fl_remove_canvas_handler( obj, MotionNotify, ch );
		}

		XFreeGC( G.d, c->font_gc );
	}

	delete_pixmap( c );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void setup_canvas( Canvas *c, FL_OBJECT *obj )
{
	XSetWindowAttributes attributes;
	FL_HANDLE_CANVAS ch;


	if ( c == &G1.x_axis || c == &G1.y_axis || c == &G1.canvas )
		ch = canvas_handler_1d;
	else
		ch = canvas_handler_2d;

	c->obj = obj;

	/* We need to make sure the canvas hasn't a size less than 1 otherwise
	   hell will break loose... */

	if ( obj->w > 0 )
		c->w = obj->w;
	else
		c->w = 1;
	if ( obj->h > 0 )
		c->h = obj->h;
	else
		c->h = 1;

	create_pixmap( c );

	fl_add_canvas_handler( c->obj, Expose, ch, ( void * ) c );

	attributes.backing_store = NotUseful;
	attributes.background_pixmap = None;
	XChangeWindowAttributes( G.d, FL_ObjWin( c->obj ),
							 CWBackingStore | CWBackPixmap,
							 &attributes );
	c->is_box = UNSET;

	fl_add_canvas_handler( c->obj, ConfigureNotify, ch, ( void * ) c );

	if ( G.is_init )
	{
		fl_add_canvas_handler( c->obj, ButtonPress, ch, ( void * ) c );
		fl_add_canvas_handler( c->obj, ButtonRelease, ch, ( void * ) c );
		fl_add_canvas_handler( c->obj, MotionNotify, ch, ( void * ) c );

		/* Get motion events only when first or second button is pressed
		   (this got to be set *after* requesting motion events) */

		fl_remove_selected_xevent( FL_ObjWin( obj ),
								   PointerMotionMask | PointerMotionHintMask |
								   ButtonMotionMask );
		fl_add_selected_xevent( FL_ObjWin( obj ),
								Button1MotionMask | Button2MotionMask );

		c->font_gc = XCreateGC( G.d, FL_ObjWin( obj ), 0, 0 );
		XSetForeground( G.d, c->font_gc, fl_get_pixel( FL_BLACK ) );
		XSetFont( G.d, c->font_gc, G.font->fid );
	}
}


/*----------------------------------------------*/
/* Creates a pixmap for a canvas for buffering. */
/*----------------------------------------------*/

void create_pixmap( Canvas *c )
{
	char dashes[ ] = { 2, 2 };


    c->gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, 0 );
    c->pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h,
						   fl_get_canvas_depth( c->obj ) );

	if ( c == &G1.canvas || c == &G2.canvas )
	{
		if ( G.is_init && c == &G1.canvas )
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
		else
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_INACTIVE ) );
	}
	else
		XSetForeground( G.d, c->gc, fl_get_pixel( FL_LEFT_BCOL ) );

	if ( G.is_init )
	{
		c->box_gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, 0 );

		if ( c == &G1.canvas || c == &G1.x_axis || c == &G1.y_axis )
			XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );
		else
			XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_BLACK ) );
		XSetLineAttributes( G.d, c->box_gc, 0, LineOnOffDash, CapButt,
							JoinMiter );
		XSetDashes( G.d, c->box_gc, 0, dashes, 2 );
	}
}


/*------------------------------------------------*/
/* Deletes the pixmap for a canvas for buffering. */
/*------------------------------------------------*/

void delete_pixmap( Canvas *c )
{
	XFreeGC( G.d, c->gc );
	XFreePixmap( G.d, c->pm );

	if ( G.is_init )
		XFreeGC( G.d, c->box_gc );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void redraw_axis_1d( int coord )
{
	Canvas *c;
	Curve_1d *cv = NULL;
	int width;
	int i;


	fsc2_assert( coord == X || coord == Y );

	/* First draw the label - for the x-axis it's just done by drawing the
	   string while for the y- and z-axis we have to copy a pixmap since the
	   label is a string rotated by 90 degree that has been drawn in advance */

	if ( coord == X )
	{
		c = &G1.x_axis;
		if ( G1.label[ X ] != NULL && G.font != NULL )
		{
			width = XTextWidth( G.font, G1.label[ X ],
								strlen( G1.label[ X ] ) );
			XDrawString( G.d, c->pm, c->font_gc, c->w - width - 5,
						 c->h - 5 - G.font_desc,
						 G1.label[ X ], strlen( G1.label[ X ] ) );
		}
	}
	else
	{
		c = &G1.y_axis;
		if ( G1.label[ coord ] != NULL && G.font != NULL )
			XCopyArea( G.d, G1.label_pm, c->pm, c->gc, 0, 0,
					   G1.label_w, G1.label_h, 0, 0 );
	}

	if ( ! G1.is_scale_set )
		return;

	/* Find out the active curve for the axis */

	for ( i = 0; i < G1.nc; i++ )
	{
		cv = G1.curve[ i ];
		if ( cv->active )
			break;
	}

	if ( i == G1.nc )                         /* no active curve -> no scale */
		return;

	make_scale_1d( cv, c, coord );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void redraw_axis_2d( int coord )
{
	Canvas *c;
	int width;


	fsc2_assert( coord == X || coord == Y || coord == Z );

	/* First draw the label - for the x-axis it's just done by drawing the
	   string while for the y- and z-axis we have to copy a pixmap since the
	   label is a string rotated by 90 degree that has been drawn in advance */

	if ( coord == X )
	{
		c = &G2.x_axis;
		if ( G2.label[ X ] != NULL && G.font != NULL )
		{
			width = XTextWidth( G.font, G2.label[ X ],
								strlen( G2.label[ X ] ) );
			XDrawString( G.d, c->pm, c->font_gc, c->w - width - 5,
						 c->h - 5 - G.font_desc,
						 G2.label[ X ], strlen( G2.label[ X ] ) );
		}
	}
	else if ( coord == Y )
	{
		c = &G2.y_axis;
		if ( G2.label[ coord ] != NULL && G.font != NULL )
			XCopyArea( G.d, G2.label_pm[ coord ], c->pm, c->gc, 0, 0,
					   G2.label_w[ coord ], G2.label_h[ coord ], 0, 0 );
	}
	else
	{
		c = &G2.z_axis;
		if ( G2.label[ coord ] != NULL && G.font != NULL )
			XCopyArea( G.d, G2.label_pm[ coord ], c->pm, c->gc, 0, 0,
					   G2.label_w[ coord ], G2.label_h[ coord ],
					   c->w - 5 - G2.label_w[ coord ], 0 );
	}

	if ( G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
		return;

	/* Find out the active curve for the axis */

	if ( G2.active_curve != -1 )
		make_scale_2d( G2.curve_2d[ G2.active_curve ], c, coord );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void make_label_string( char *lstr, double num, int res )
{
	int n, mag;


	if ( num == 0 )
	{
		sprintf( lstr, "0" );
		return;
	}

   res++;

	mag = irnd( floor( log10( fabs( num ) ) ) );
	if ( mag >= 0 )
		mag++;

	/* n is the number of relevant digits */

	n = i_max( 1, mag - res );

	if ( mag > 5 )                              /* num > 10^5 */
		snprintf( lstr, MAX_LABEL_LEN, "%1.*E", n - 1, num );
	else if ( mag > 0 )                         /* num >= 1 */
	{
		if ( res >= 0 )
			snprintf( lstr, MAX_LABEL_LEN, "%*g", mag, num );
		else
			snprintf( lstr, MAX_LABEL_LEN, "%*.*f", n, abs( res ), num );
	}
	else if ( mag > -4 && res >= - 4 )          /* num > 10^-4 */
		snprintf( lstr, MAX_LABEL_LEN, "%1.*f", abs( res ), num );
	else                                        /* num < 10^-4 */
		if ( mag < res )
			snprintf( lstr, MAX_LABEL_LEN, "0" );
		else
			snprintf(  lstr, MAX_LABEL_LEN, "%1.*E", n, num );
}


/*---------------------------------------------------------------------------*/
/* This routine is called from functions (like the function showing the file */
/* selector box) to switch off the special states entered by pressing one or */
/* more of the mouse buttons. Otherwise, after these functions are finished  */
/* (usually with all mouse buttons released) the drawing routines would      */
/* think that the buttons are still pressed, forcing the user to press the   */
/* buttons again, just to get the ButtonRelease event.                       */
/*---------------------------------------------------------------------------*/

void switch_off_special_cursors( void )
{
	if ( ! G.is_init )
		return;

	if ( G.button_state != 0 )
	{
		G.button_state = G.raw_button_state = 0;

		if ( G.drag_canvas == DRAG_1D_X )         /* 1D x-axis window */
		{
			fl_reset_cursor( FL_ObjWin( G1.x_axis.obj ) );
			G1.x_axis.is_box = UNSET;
			repaint_canvas_1d( &G1.x_axis );
		}

		if ( G.drag_canvas == DRAG_1D_Y )         /* 1D y-axis window */
		{
			fl_reset_cursor( FL_ObjWin( G1.y_axis.obj ) );
			G1.y_axis.is_box = UNSET;
			repaint_canvas_1d( &G1.y_axis );
		}

		if ( G.drag_canvas == DRAG_1D_C )         /* 1D canvas window */
		{
			fl_reset_cursor( FL_ObjWin( G1.canvas.obj ) );
			G1.canvas.is_box = UNSET;
			repaint_canvas_1d( &G1.canvas );
		}

		if ( G.drag_canvas == DRAG_2D_X )         /* 2D x-axis window */
		{
			fl_reset_cursor( FL_ObjWin( G2.x_axis.obj ) );
			G2.x_axis.is_box = UNSET;
			repaint_canvas_2d( &G2.x_axis );
		}

		if ( G.drag_canvas == DRAG_2D_Y )         /* 2D y-axis window */
		{
			fl_reset_cursor( FL_ObjWin( G2.y_axis.obj ) );
			G2.y_axis.is_box = UNSET;
			repaint_canvas_2d( &G1.y_axis );
		}

		if ( G.drag_canvas == DRAG_2D_Z )         /* 2D z-axis window */
		{
			fl_reset_cursor( FL_ObjWin( G2.z_axis.obj ) );
			G2.z_axis.is_box = UNSET;
			repaint_canvas_2d( &G2.z_axis );
		}

		if ( G.drag_canvas == DRAG_2D_C )         /* 2D canvas window */
		{
			fl_reset_cursor( FL_ObjWin( G2.canvas.obj ) );
			G2.canvas.is_box = UNSET;
			repaint_canvas_2d( &G2.canvas );
		}
	}
}


/*-------------------------------------------------------------*/
/* Undoes the last action in the 1d window as far as possible. */
/*-------------------------------------------------------------*/

void undo_button_callback_1d( FL_OBJECT *a, long b )
{
	long i;
	bool is_undo = UNSET;
	Curve_1d *cv;
	double temp_s2d,
		   temp_shift;
	int j;


	UNUSED_ARGUMENT( a );
	UNUSED_ARGUMENT( b );

	for ( i = 0; i < G1.nc; i++ )
	{
		cv = G1.curve[ i ];

		if ( ! cv->can_undo )
			continue;

		for ( j = 0; j <= Y; j++ )
		{
			temp_s2d = cv->s2d[ j ];
			temp_shift = cv->shift[ j ];

			cv->s2d[ j ] = cv->old_s2d[ j ];
			cv->shift[ j ] = cv->old_shift[ j ];

			cv->old_s2d[ j ] = temp_s2d;
			cv->old_shift[ j ] = temp_shift;
		}

		is_undo = SET;

		recalc_XPoints_of_curve_1d( cv );
	}

	if ( is_undo && G1.is_fs )
	{
		G1.is_fs = UNSET;
		fl_set_button( GUI.run_form_1d->full_scale_button_1d, 0 );
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
	}

	if ( is_undo )
		redraw_all_1d( );
}


/*-------------------------------------------------------------*/
/* Undoes the last action in the 2d window as far as possible. */
/*-------------------------------------------------------------*/

void undo_button_callback_2d( FL_OBJECT *a, long b )
{
	Curve_2d *cv2;
	double temp_s2d,
		   temp_shift;
	double temp_z_factor;
	int j;


	UNUSED_ARGUMENT( a );
	UNUSED_ARGUMENT( b );

	if ( G2.active_curve == -1 || ! G2.curve_2d[ G2.active_curve ]->can_undo )
		return;

	cv2 = G2.curve_2d[ G2.active_curve ];

	for ( j = 0; j <= Z; j++ )
	{
		temp_s2d = cv2->s2d[ j ];
		temp_shift = cv2->shift[ j ];

		cv2->s2d[ j ] = cv2->old_s2d[ j ];
		cv2->shift[ j ] = cv2->old_shift[ j ];

		cv2->old_s2d[ j ] = temp_s2d;
		cv2->old_shift[ j ] = temp_shift;
	}


	temp_z_factor = cv2->z_factor;
	cv2->z_factor = cv2->old_z_factor;
	cv2->old_z_factor = temp_z_factor;

	recalc_XPoints_of_curve_2d( cv2 );

	if ( cv2->is_fs )
	{
		cv2->is_fs = UNSET;
		fl_set_button( GUI.run_form_2d->full_scale_button_2d, 0 );
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( GUI.run_form_2d->full_scale_button_2d,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
	}

	redraw_all_2d( );
}


/*----------------------------------------------------------------*/
/* Rescales the 1D display so that the canvas is completely used. */
/*----------------------------------------------------------------*/

void fs_button_callback_1d( FL_OBJECT *a, long b )
{
	int state;
	int i;


	UNUSED_ARGUMENT( a );
	UNUSED_ARGUMENT( b );

	/* Rescaling is useless if graphic isn't initialised */

	if ( ! G.is_init )
		return;

	/* Get new state of button */

	state = fl_get_button( GUI.run_form_1d->full_scale_button_1d );

	if ( state == 1 )        /* full scale got switched on */
	{
		G1.is_fs = SET;

		/* Store data of previous state... */

		for ( i = 0; i < G1.nc; i++ )
			save_scale_state_1d( G1.curve[ i ] );

		/* ... and rescale to full scale */

		fs_rescale_1d( );
		redraw_all_1d( );
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
								  "Switch off automatic rescaling" );
	}
	else        /* full scale got switched off */
	{
		G1.is_fs = UNSET;
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
	}
}


/*----------------------------------------------------------------*/
/* Rescales the 2D display so that the canvas is completely used. */
/*----------------------------------------------------------------*/

void fs_button_callback_2d( FL_OBJECT *a, long b )
{
	int state;


	UNUSED_ARGUMENT( a );
	UNUSED_ARGUMENT( b );

	/* Rescaling is useless if graphic isn't initialised */

	if ( ! G.is_init )
		return;

	/* Get new state of button */

	state = fl_get_button( GUI.run_form_2d->full_scale_button_2d );

	if ( state == 1 )        /* full scale got switched on */
	{
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( GUI.run_form_2d->full_scale_button_2d,
								  "Switch off automatic rescaling" );

		if ( G2.active_curve != -1 )
		{
			save_scale_state_2d( G2.curve_2d[ G2.active_curve ] );
			G2.curve_2d[ G2.active_curve ]->is_fs = SET;
			fs_rescale_2d( G2.curve_2d[ G2.active_curve ] );
			redraw_all_2d( );
		}
	}
	else                     /* full scale got switched off */
	{
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( GUI.run_form_2d->full_scale_button_2d,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
		if ( G2.active_curve != -1 )
			G2.curve_2d[ G2.active_curve ]->is_fs = UNSET;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void curve_button_callback_1d( FL_OBJECT *obj, long data )
{
	char hstr[ 128 ];


	/* Change the help string for the button */

	if ( ( G1.curve[ data - 1 ]->active = fl_get_button( obj ) ) )
		sprintf( hstr, "Exempt curve %ld from\nrescaling operations", data );
	else
		sprintf( hstr, "Include curve %ld into\nrescaling operations", data );

	if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
		fl_set_object_helper( obj, hstr );

	/* Redraw both axis to make sure the axis for the first active button
	   is shown. If markers are shown also redraw the canvas. */

	redraw_canvas_1d( &G1.x_axis );
	redraw_canvas_1d( &G1.y_axis );
	if ( G1.marker_1d != NULL )
		redraw_canvas_1d( &G1.canvas );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void curve_button_callback_2d( FL_OBJECT *obj, long data )
{
	char hstr[ 128 ];
	int bstate;


	/* We get a negative curve number if this handler is called via the
	   hidden curve buttons in the cross section window and have to set
	   the correct state of the buttons in the main display window */

	if ( data < 0 )
	{
		data = - data;

		if ( data > G2.nc )    /* button for non-existing curve triggered */
			return;            /* via the keyboard */

		switch( data )
		{
			case 1:
				obj = GUI.run_form_2d->curve_1_button_2d;
				fl_set_button( obj, G2.active_curve == 0 ? 0 : 1 );
				break;

			case 2:
				obj = GUI.run_form_2d->curve_2_button_2d;
				fl_set_button( obj, G2.active_curve == 1 ? 0 : 1 );
				break;

			case 3:
				obj = GUI.run_form_2d->curve_3_button_2d;
				fl_set_button( obj, G2.active_curve == 2 ? 0 : 1 );
				break;

			case 4:
				obj = GUI.run_form_2d->curve_4_button_2d;
				fl_set_button( obj, G2.active_curve == 3 ? 0 : 1 );
				break;
		}
	}
	else
	{
		bstate = fl_get_button( obj );
		if ( ( bstate && data - 1 == G2.active_curve ) ||
			 ( ! bstate && G2.active_curve == -1 ) )
			return;
	}

	/* Make buttons work like radio buttons but also allow switching off
	   all of them */

	if ( data - 1 == G2.active_curve )     /* shown curve is switched off */
	{
		G2.active_curve = -1;
		sprintf( hstr, "Show curve %ld", data );
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( obj, hstr );
	}
	else
	{
		switch ( G2.active_curve )
		{
			case 0 :
				fl_set_button( GUI.run_form_2d->curve_1_button_2d, 0 );
				if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
					fl_set_object_helper( GUI.run_form_2d->curve_1_button_2d,
										  "Show curve 1" );
				break;

			case 1 :
				fl_set_button( GUI.run_form_2d->curve_2_button_2d, 0 );
				if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
					fl_set_object_helper( GUI.run_form_2d->curve_2_button_2d,
										  "Show curve 2" );
				break;

			case 2 :
				fl_set_button( GUI.run_form_2d->curve_3_button_2d, 0 );
				if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
					fl_set_object_helper( GUI.run_form_2d->curve_3_button_2d,
										  "Show curve 3" );
				break;

			case 3 :
				fl_set_button( GUI.run_form_2d->curve_4_button_2d, 0 );
				if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
					fl_set_object_helper( GUI.run_form_2d->curve_4_button_2d,
										  "Show curve 4" );
				break;
		}

		sprintf( hstr, "Hide curve %ld", data );
		if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( obj, hstr );

		G2.active_curve = data - 1;

		fl_set_button( GUI.run_form_2d->full_scale_button_2d,
					   G2.curve_2d[ G2.active_curve ]->is_fs );
	}

	redraw_all_2d( );
	cut_new_curve_handler( );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void clear_curve_1d( long curve )
{
	long i;
	Scaled_Point *sp;


	for ( sp = G1.curve[ curve ]->points, i = 0; i < G1.nx; sp++, i++ )
		sp->exist = UNSET;
	G1.curve[ curve ]->count = 0;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void clear_curve_2d( long curve )
{
	long i;
	Scaled_Point *sp;


	for ( sp = G2.curve_2d[ curve ]->points, i = 0; i < G2.nx * G2.ny;
		  sp++, i++ )
		sp->exist = UNSET;
	G2.curve_2d[ curve ]->count = 0;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void change_scale_1d( int is_set, void *ptr )
{
	double vals[ 4 ];


	memcpy( vals, ptr, sizeof vals );

	if ( is_set & 1 )
		G1.rwc_start[ X ] = vals[ 0 ];
	if ( is_set & 2 )
		G1.rwc_delta[ X ] = vals[ 1 ];
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void change_scale_2d( int is_set, void *ptr )
{
	int i;
	double vals[ 4 ];


	memcpy( vals, ptr, sizeof vals );

	if ( is_set & 1 )
	{
		G2.rwc_start[ X ] = vals[ 0 ];
		for ( i = 0; i < G2.nc; i++ )
			G2.curve_2d[ i ]->rwc_start[ X ] = vals[ 0 ];
	}

	if ( is_set & 2 )
	{
		G2.rwc_delta[ X ] = vals[ 1 ];
		for ( i = 0; i < G2.nc; i++ )
			G2.curve_2d[ i ]->rwc_delta[ X ] = vals[ 1 ];
	}

	if ( is_set & 4 )
	{
		G2.rwc_start[ Y ] = vals[ 2 ];
		for ( i = 0; i < G2.nc; i++ )
			G2.curve_2d[ i ]->rwc_start[ Y ] = vals[ 2 ];
	}

	if ( is_set & 8 )
	{
		G2.rwc_delta[ Y ] = vals[ 3 ];
		for ( i = 0; i < G2.nc; i++ )
			G2.curve_2d[ i ]->rwc_delta[ Y ] = vals[ 3 ];
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void change_label_1d( char **label )
{
	if ( *label[ X ] != '\0' )
	{
		T_free( G1.label[ X ] );
		G1.label[ X ] = T_strdup( label[ X ] );
		redraw_canvas_1d( &G1.x_axis );
	}

	if ( *label[ Y ] != '\0' )
	{
		if ( G1.label[ Y ] != NULL )
		{
			G1.label[ Y ] = CHAR_P T_free( G1.label[ Y ] );
			if ( G.font != NULL )
				XFreePixmap( G.d, G1.label_pm );
		}

		G1.label[ Y ] = T_strdup( label[ Y ] );
		if ( G.font != NULL )
		{
			create_label_pixmap( &G1.y_axis, Y, G1.label[ Y ] );
			redraw_canvas_1d( &G1.y_axis );
		}
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void change_label_2d( char **label )
{
	int coord;


	if ( *label[ X ] != '\0' )
	{
		if ( G2.label[ X ] != NULL )
		{
			G2.label[ X ] = CHAR_P T_free( G2.label[ X ] );
			if ( CG.is_shown && CG.cut_dir == X )
				XFreePixmap( G.d, G2.label_pm[ Z + 3 ] );
		}
		G2.label[ X ] = T_strdup( label[ X ] );

		redraw_canvas_2d( &G2.x_axis );

		if ( CG.is_shown )
		{
			if ( CG.cut_dir == X )
			{
				if ( G2.label[ X ] != NULL && G.font != NULL )
					create_label_pixmap( &G2.cut_z_axis, Z, G2.label[ X ] );
				redraw_cut_axis( Z );
			}
			else
				redraw_cut_axis( X );
		}
	}

	for ( coord = Y; coord <= Z; coord++ )
		if ( *label[ coord ] != '\0' )
		{
			if ( G2.label[ coord ] != NULL )
			{
				T_free( G2.label[ coord ] );
				if ( G.font != NULL )
					XFreePixmap( G.d, G2.label_pm[ coord ] );
			}
			G2.label[ coord ] = T_strdup( label[ coord ] );

			if ( G.font != NULL )
				create_label_pixmap( coord == Y ? &G2.y_axis : &G2.z_axis,
									 coord, G2.label[ coord ] );
			redraw_canvas_2d( coord == Y ? &G2.y_axis : &G2.z_axis );

			if ( CG.is_shown )
			{
				if ( coord == Y )
				{
					if ( CG.cut_dir == Y )
					{
						if ( G2.label[ Y ] != NULL && G.font != NULL )
						{
							G2.label_pm[ Z + 3 ] = G2.label_pm[ Y ];
							G2.label_w[ Z + 3 ]  = G2.label_w[ Y ];
							G2.label_h[ Z + 3 ]   = G2.label_h[ Y ];
						}
						redraw_cut_axis( Z );
					}
					else
						redraw_cut_axis( X );
				}

				if ( coord == Z )
				{
					if ( G2.label[ Z ] != NULL && G.font != NULL )
					{
						G2.label_pm[ Y + 3 ] = G2.label_pm[ Z ];
						G2.label_w[ Y + 3 ]  = G2.label_w[ Z ];
						G2.label_h[ Y + 3 ]  = G2.label_h[ Z ];
					}
					redraw_cut_axis( Y );
				}
			}
		}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void rescale_1d( long new_nx )
{
	long i, k, count;
	long max_x = 0;
	Scaled_Point *sp;
	Marker_1D *m;


	/* Return immediately on unreasonable values */

	if ( new_nx < 0 )
		return;

	/* Find the maximum x-index currently used by a point or a marker */

	for ( k = 0; k < G1.nc; k++ )
		for ( count = G1.curve[ k ]->count, sp = G1.curve[ k ]->points, i = 0;
			  count > 0; sp++, i++ )
			if ( sp->exist )
			{
				if( i > max_x )
					max_x = i;
				count--;
			}

	for ( m = G1.marker_1d; m != NULL; m = m->next )
		if ( m->x_pos > max_x )
			max_x = m->x_pos;

	if ( max_x != 0 )
		max_x++;
	else
		max_x = DEFAULT_1D_X_POINTS;

	/* Make sure we don't rescale to less than the current number of
	   points (or the minumum value, if larger) */

	if ( new_nx < DEFAULT_1D_X_POINTS )
	{
		if ( max_x < DEFAULT_1D_X_POINTS )
			max_x = DEFAULT_1D_X_POINTS;
	}
	else if ( new_nx > max_x )
		max_x = new_nx;

	for ( k = 0; k < G1.nc; k++ )
	{
		G1.curve[ k ]->points = SCALED_POINT_P
			T_realloc( G1.curve[ k ]->points,
					   max_x * sizeof *G1.curve[ k ]->points );
		G1.curve[ k ]->xpoints = XPOINT_P
			T_realloc( G1.curve[ k ]->xpoints,
					   max_x * sizeof *G1.curve[ k ]->xpoints );

		for ( i = G1.nx, sp = G1.curve[ k ]->points + i; i < max_x;
			  sp++, i++ )
			sp->exist = UNSET;
	}

	G1.nx = max_x;

	for ( k = 0; k < G1.nc; k++ )
	{
		if ( G1.is_fs )
		{
			G1.curve[ k ]->s2d[ X ] = ( double ) ( G1.canvas.w - 1 )
									  / ( double ) ( G1.nx - 1 );
			if ( G1.is_scale_set )
				recalc_XPoints_of_curve_1d( G1.curve[ k ] );
		}
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void rescale_2d( void *new_dims )
{
	long i, j, k, l, count;
	long max_x = 0,
		 max_y = 0;
	Scaled_Point *sp, *old_sp, *osp;
	bool need_cut_redraw = UNSET;
	long new_nx, new_ny;


	memcpy( &new_nx, new_dims, sizeof new_nx );
	memcpy( &new_ny, ( char * ) new_dims + sizeof new_nx, sizeof new_ny );

	if ( new_nx < 0 && new_ny < 0 )
		return;

	/* Find the maximum x and y index used until now */

	for ( k = 0; k < G2.nc; k++ )
		for ( j = 0, count = G2.curve_2d[ k ]->count,
			  sp = G2.curve_2d[ k ]->points; count > 0; j++ )
			for ( i = 0; i < G2.nx && count > 0; sp++, i++ )
				if ( sp->exist )
				{
					max_y = j;
					if ( i > max_x )
						max_x = i;
					count--;
				}

	if ( max_x != 0 )
		max_x++;
	else
		max_x = DEFAULT_2D_X_POINTS;
	if ( max_y != 0 )
		 max_y++;
	else
		max_y = DEFAULT_2D_Y_POINTS;

	/* Figure out the correct new values - at least the current points must
	   still be displayable and at least the minimum sizes must be kept */

	if ( new_nx < 0 )
		new_nx = G2.nx;
	if ( new_nx < max_x )
		new_nx = max_x;

	if ( new_ny < 0 )
		new_ny = G2.ny;
	if ( new_ny < max_y )
		new_ny = max_y;

	/* Now we're in for the real fun... */

	for ( k = 0; k < G2.nc; k++ )
	{
		/* Reorganize the old elements to fit into the new array and clear
		   the the new elements in the already existing rows */

		old_sp = osp = G2.curve_2d[ k ]->points;
		sp = G2.curve_2d[ k ]->points = SCALED_POINT_P
			                          T_malloc( new_nx * new_ny * sizeof *sp );

		for ( j = 0; j < l_min( G2.ny, new_ny ); j++, osp += G2.nx )
		{
			memcpy( sp, osp, l_min( G2.nx, new_nx ) * sizeof *sp );
			if ( G2.nx < new_nx )
				for ( l = G2.nx, sp += G2.nx; l < new_nx; l++, sp++ )
					sp->exist = UNSET;
			else
				sp += new_nx;
		}

		for ( ; j < new_ny; j++ )
			for ( l = 0; l < new_nx; l++, sp++ )
				sp->exist = UNSET;

		T_free( old_sp );

		G2.curve_2d[ k ]->xpoints = XPOINT_P
			 T_realloc( G2.curve_2d[ k ]->xpoints,
						new_nx * new_ny * sizeof *G2.curve_2d[ k ]->xpoints );
		G2.curve_2d[ k ]->xpoints_s = XPOINT_P
			 T_realloc( G2.curve_2d[ k ]->xpoints_s,
					   new_nx * new_ny * sizeof *G2.curve_2d[ k ]->xpoints_s );

		if ( G2.curve_2d[ k ]->is_fs )
		{
			G2.curve_2d[ k ]->s2d[ X ] = ( double ) ( G2.canvas.w - 1 )
										 / ( double ) ( new_nx - 1 );
			G2.curve_2d[ k ]->s2d[ Y ] = ( double ) ( G2.canvas.h - 1 )
										 / ( double ) ( new_ny - 1 );
		}
	}

	if ( G2.nx != new_nx )
		 need_cut_redraw = cut_num_points_changed( X, new_nx );
	if ( G2.ny != new_ny )
		need_cut_redraw |= cut_num_points_changed( Y, new_ny );

	G2.nx = new_nx;
	G2.ny = new_ny;

	for ( k = 0; k < G2.nc; k++ )
		recalc_XPoints_of_curve_2d( G2.curve_2d[ k ] );

	/* Update the cut window if necessary */

	redraw_all_cut_canvases( );
}


/*-------------------------------------------------------------------------*/
/* Function for toggling 1D display between normal and sliding window mode */
/*-------------------------------------------------------------------------*/

void change_mode( long mode, long width )
{
	long curves;
	long i;
	Scaled_Point *sp;
	Marker_1D *m, *mn;
	Curve_1d *cv;


	if ( G.mode == mode )
	{
		print( WARN, "Display is already in \"%s\" mode.\n",
			   G.mode ? "SLIDING WINDOW" : "NORMAL" );
		return;
	}

	/* Clear all curves and markers */

	for ( curves = 0; curves < G1.nc; curves++ )
	{
		cv = G1.curve[ curves ];

		if ( width != G1.nx )
		{
			cv->points = SCALED_POINT_P T_realloc( cv->points,
												  width * sizeof *cv->points );
			cv->xpoints = XPOINT_P T_realloc( cv->xpoints,
											  width * sizeof *cv->xpoints );
		}

		for ( sp = cv->points, i = 0; i < width; sp++, i++ )
			sp->exist = UNSET;

		cv->can_undo = UNSET;
		cv->shift[ X ] = 0.0;
		cv->count = 0;
		cv->s2d[ X ] = ( double ) ( G1.canvas.w - 1 )
					   / ( double ) ( width - 1 );
	}

	for ( m = G1.marker_1d; m != NULL; m = mn )
	{
		XFreeGC( G.d, m->gc );
		mn = m->next;
		m = MARKER_1D_P T_free( m );
	}

	G1.marker_1d = NULL;

	G.mode = mode;
	G1.nx = width;

	fl_set_form_title( GUI.run_form_1d->run_1d, mode == NORMAL_DISPLAY ?
					   "fsc2: 1D-Display" : 
					   "fsc2: 1D-Display (sliding window)" );

	redraw_all_1d( );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
