/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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
#include "cursor8.xbm"

#include "up_arrow.xbm"             /* arrow bitmaps */
#include "down_arrow.xbm"
#include "left_arrow.xbm"
#include "right_arrow.xbm"


static void cut_calc_curve( int dir, long p_index, bool has_been_shown );
static void G_init_cut_curve( void );
static void cut_setup_canvas( Canvas *c, FL_OBJECT *obj );
static void cut_canvas_off( Canvas *c, FL_OBJECT *obj );
static int cut_form_close_handler( FL_FORM *a, void *b );
static void cut_recalc_XPoints( void );
static void cut_integrate_point( long p_index, double val );
static int cut_canvas_handler( FL_OBJECT *obj, Window window, int w, int h,
							   XEvent *ev, void *udata );
static void cut_reconfigure_window( Canvas *c, int w, int h );
static void cut_press_handler( FL_OBJECT *obj, Window window,
							   XEvent *ev, Canvas *c );
static void cut_release_handler( FL_OBJECT *obj, Window window,
								 XEvent *ev, Canvas *c );
static void cut_motion_handler( FL_OBJECT *obj, Window window,
								XEvent *ev, Canvas *c );
static void redraw_cut_canvas( Canvas *c );
static void repaint_cut_canvas( Canvas * );
static void cut_make_scale( Canvas *c, int coord );
static void shift_XPoints_of_cut_curve( Canvas *c );
static bool cut_change_x_range( Canvas *c );
static bool cut_change_y_range( Canvas *c );
static bool cut_change_xy_range( Canvas *c );
static bool cut_zoom_x( Canvas *c );
static bool cut_zoom_y( Canvas *c );
static bool cut_zoom_xy( Canvas *c );
static void cut_save_scale_state( void );


extern FL_resource xresources[ ];


static struct {
	unsigned int WIN_MIN_WIDTH;
	unsigned int WIN_MIN_HEIGHT;
} GC_sizes;


static bool is_mapped = UNSET;  /* set while form is mapped */
Cut_Graphics CG;
static int cur_1,
	       cur_2,
	       cur_3,
	       cur_4,
	       cur_5,
	       cur_6,
	       cur_7,
	       cur_8;
static int cut_x, cut_y;
static unsigned int cut_w, cut_h;
static bool cut_has_been_shown = UNSET;


/*--------------------------------------------------------------------*/
/* Function to initialize the minimum sizes of the cut graphic window */
/*--------------------------------------------------------------------*/

void cut_init( void )
{
	if ( G_Funcs.size == LOW )
	{
		GC_sizes.WIN_MIN_WIDTH   = 350;
		GC_sizes.WIN_MIN_HEIGHT  = 250;
	}
	else
	{
		GC_sizes.WIN_MIN_WIDTH   = 500;
		GC_sizes.WIN_MIN_HEIGHT  = 350;
	}
}


/*-----------------------------------------------------------------------*/
/* Function gets called whenever in the 2D display the left mouse button */
/* has been pressed with one of the shift keys pressed down in the x- or */
/* y-axis canvas and than is released again (with the shift key is still */
/* being hold down). It opens up a new window displaying a cross section */
/* through the data at the positon the mouse button was released. If the */
/* window with the cross section is already shown the cut at the mouse   */
/* position is shown.                                                    */
/* dir: axis canvas the mouse button was pressed in (X or Y)             */
/*-----------------------------------------------------------------------*/

void cut_show( int dir, long u_index )
{
	bool needs_pos = UNSET;
	int flags;


	/* Don't do anything if no curve is currently displayed or if mouse didn't
       got released in one of the axis canvases */

	if ( G.active_curve == -1 || u_index < 0 ||
		 ( dir == X && u_index >= G.nx ) || ( dir == Y && u_index >= G.ny ) )
		return;

	/* If the cross section window hasn't been shown before create and
	   initialize now, otherwise, it's just unmapped, so make it visible
	   again */

	if ( ! CG.is_shown )
	{
		fl_set_object_shortcutkey( cut_form->change_button, XK_space );

		if ( ! cut_has_been_shown &&
			 * ( ( char * ) xresources[ CUTGEOMETRY ].var ) != '\0' )
		{
			flags = XParseGeometry( ( char * ) xresources[ CUTGEOMETRY ].var,
									&cut_x, &cut_y, &cut_w, &cut_h );
			if ( WidthValue & flags && HeightValue & flags )
			{
				if ( cut_w < GC_sizes.WIN_MIN_WIDTH )
					cut_w = GC_sizes.WIN_MIN_WIDTH;

				if ( cut_h < GC_sizes.WIN_MIN_HEIGHT )
					cut_h = GC_sizes.WIN_MIN_HEIGHT;

				fl_set_form_size( cut_form->cut, cut_w, cut_h );
			}

			if ( XValue & flags && YValue & flags )
			{
				cut_x += border_offset_x - 1;
				cut_y += border_offset_y - 1;

				fl_set_form_position( cut_form->cut, cut_x, cut_y );
				needs_pos = SET;
			}
		}

		if ( cut_has_been_shown )
		{
			fl_set_form_geometry( cut_form->cut, cut_x, cut_y, cut_w, cut_h );
			needs_pos = SET;
		}

		fl_show_form( cut_form->cut, needs_pos ?
					  FL_PLACE_POSITION : FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "fsc2: Cross section" );
		cut_has_been_shown = SET;

		fl_winminsize( cut_form->cut->window,
					   GC_sizes.WIN_MIN_WIDTH, GC_sizes.WIN_MIN_HEIGHT );
		fl_set_form_atclose( cut_form->cut, cut_form_close_handler, NULL );

		cut_setup_canvas( &G.cut_x_axis, cut_form->cut_x_axis );
		cut_setup_canvas( &G.cut_y_axis, cut_form->cut_y_axis );
		cut_setup_canvas( &G.cut_z_axis, cut_form->cut_z_axis );
		cut_setup_canvas( &G.cut_canvas, cut_form->cut_canvas );

		G_init_cut_curve( );

	}
	else if ( ! is_mapped )
	{
		XMapWindow( G.d, cut_form->cut->window );
		XMoveWindow( G.d, cut_form->cut->window, cut_x + 1, cut_y + 1 );
	}

	fl_raise_form( cut_form->cut );

	/* Set up the labels if the cut window does't not exist yet or the cut
	   direction changed */

	if ( ! CG.is_shown || CG.cut_dir != dir )
	{
		if ( CG.is_shown )
		{
			if ( CG.cut_dir == X )                /* new direction is Y ! */
			{
				if( G.label[ X ] != NULL && G.font != NULL )
					XFreePixmap( G.d, G.label_pm[ Z + 3 ] );
				if ( G.label[ Y ] != NULL && G.font != NULL )
				{
					G.label_pm[ Z + 3 ] = G.label_pm[ Y ];
					G.label_w[ Z + 3 ] = G.label_w[ Y ];
					G.label_h[ Z + 3 ] = G.label_h[ Y ];
				}
			}
			else                                  /* new direction is X ! */
				if ( G.label[ X ] != NULL && G.font != NULL )
					create_label_pixmap( &G.cut_z_axis, Z, G.label[ X ] );
		}
		else
		{
			if ( dir == X )
			{
				if ( G.label[ X ] != NULL && G.font != NULL )
					create_label_pixmap( &G.cut_z_axis, Z, G.label[ X ] );
			}
			else if ( G.label[ Y ] != NULL && G.font != NULL )
			{
				G.label_pm[ Z + 3 ] = G.label_pm[ Y ];
				G.label_w[ Z + 3 ] = G.label_w[ Y ];
				G.label_h[ Z + 3 ] = G.label_h[ Y ];
			}
		}

		fl_set_object_callback( cut_form->cut_print_button, print_1d, - dir );
	}

	/* Calculate all the points of the cross section curve */

	cut_calc_curve( dir, u_index, CG.has_been_shown[ G.active_curve ] );

	if ( ! CG.has_been_shown[ G.active_curve ] )
	{
		CG.is_fs[ G.active_curve ] = G.curve_2d[ G.active_curve ]->is_fs;
		CG.has_been_shown[ G.active_curve ] = SET;
	}

	is_mapped = SET;
	G.is_cut = SET;
	CG.is_shown = SET;
	CG.curve = G.active_curve;

	/* Draw the curve and the axes */

	redraw_all_cut_canvases( );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void cut_setup_canvas( Canvas *c, FL_OBJECT *obj )
{
	XSetWindowAttributes attributes;
	FL_HANDLE_CANVAS ch = cut_canvas_handler;


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

	if ( G.is_init )
	{
		attributes.backing_store = NotUseful;
		attributes.background_pixmap = None;
		XChangeWindowAttributes( G.d, FL_ObjWin( c->obj ),
								 CWBackingStore | CWBackPixmap,
								 &attributes );
		c->is_box = UNSET;

		fl_add_canvas_handler( c->obj, ConfigureNotify, ch, ( void * ) c );
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

		XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );

		if ( c == &G.cut_canvas )
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void cut_canvas_off( Canvas *c, FL_OBJECT *obj )
{
	FL_HANDLE_CANVAS ch = cut_canvas_handler;


	fl_remove_canvas_handler( obj, Expose, ch );

	fl_remove_canvas_handler( obj, ConfigureNotify, ch );
	fl_remove_canvas_handler( obj, ButtonPress, ch );
	fl_remove_canvas_handler( obj, ButtonRelease, ch );
	fl_remove_canvas_handler( obj, MotionNotify, ch );

	XFreeGC( G.d, c->font_gc );

	delete_pixmap( c );

	G.is_cut = UNSET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void G_init_cut_curve( void )
{
	Curve_1d *cv = &G.cut_curve;
	unsigned int depth;
	int i;


	/* Create cursors */

	cur_1 = fl_create_bitmap_cursor( cursor1_bits, cursor1_bits,
									 cursor1_width, cursor1_height,
									 cursor1_x_hot, cursor1_y_hot );
	cur_2 = fl_create_bitmap_cursor( cursor2_bits, cursor2_bits,
									 cursor2_width, cursor2_height,
									 cursor2_x_hot, cursor2_y_hot );
	cur_3 = fl_create_bitmap_cursor( cursor3_bits, cursor3_bits,
									 cursor3_width, cursor3_height,
									 cursor3_x_hot, cursor3_y_hot );
	cur_4 = fl_create_bitmap_cursor( cursor4_bits, cursor4_bits,
									 cursor4_width, cursor4_height,
									 cursor4_x_hot, cursor4_y_hot );
	cur_5 = fl_create_bitmap_cursor( cursor5_bits, cursor5_bits,
									 cursor5_width, cursor5_height,
									 cursor5_x_hot, cursor5_y_hot );
	cur_6 = fl_create_bitmap_cursor( cursor6_bits, cursor6_bits,
									 cursor6_width, cursor6_height,
									 cursor6_x_hot, cursor6_y_hot );
	cur_7 = fl_create_bitmap_cursor( cursor7_bits, cursor7_bits,
									 cursor7_width, cursor7_height,
									 cursor7_x_hot, cursor7_y_hot );
	cur_8 = fl_create_bitmap_cursor( cursor8_bits, cursor8_bits,
									 cursor8_width, cursor8_height,
									 cursor8_x_hot, cursor8_y_hot );
	CG.cur_1 = cur_1;
	CG.cur_2 = cur_2;
	CG.cur_3 = cur_3;
	CG.cur_4 = cur_4;
	CG.cur_5 = cur_5;
	CG.cur_6 = cur_6;
	CG.cur_7 = cur_7;
	CG.cur_8 = cur_8;

	fl_set_cursor_color( CG.cur_1, FL_RED, FL_WHITE	);
	fl_set_cursor_color( CG.cur_2, FL_RED, FL_WHITE	);
	fl_set_cursor_color( CG.cur_3, FL_RED, FL_WHITE	);
	fl_set_cursor_color( CG.cur_4, FL_RED, FL_WHITE	);
	fl_set_cursor_color( CG.cur_5, FL_RED, FL_WHITE	);
	fl_set_cursor_color( CG.cur_6, FL_RED, FL_WHITE	);
	fl_set_cursor_color( CG.cur_7, FL_RED, FL_WHITE	);
	fl_set_cursor_color( CG.cur_8, FL_RED, FL_WHITE	);

	/* Allocate memory for the curve and its data */

	cv->points = NULL;
	cv->xpoints = NULL;

	/* Create a GC for drawing the curve and set its colour */

	cv->gc = XCreateGC( G.d, G.cut_canvas.pm, 0, 0 );
	XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ G.active_curve ] ) );

	/* Create pixmaps for the out-of-range arrows (in the colors associated
	   with the four possible curves) */

	depth = fl_get_canvas_depth( G.cut_canvas.obj );

	for ( i = 0; i < MAX_CURVES; i++ )
	{
		CG.up_arrows[ i ] =
			XCreatePixmapFromBitmapData( G.d, G.cut_canvas.pm, up_arrow_bits,
										 up_arrow_width, up_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		CG.up_arrow_w = up_arrow_width;
		CG.up_arrow_h = up_arrow_width;

		CG.down_arrows[ i ] =
			XCreatePixmapFromBitmapData( G.d, G.cut_canvas.pm, down_arrow_bits,
										 down_arrow_width, down_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		CG.down_arrow_w = down_arrow_width;
		CG.down_arrow_h = down_arrow_width;

		CG.left_arrows[ i ] =
			XCreatePixmapFromBitmapData( G.d, G.cut_canvas.pm, left_arrow_bits,
										 left_arrow_width, left_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		CG.left_arrow_w = left_arrow_width;
		CG.left_arrow_h = left_arrow_width;

		CG.right_arrows[ i ] = 
			XCreatePixmapFromBitmapData( G.d, G.cut_canvas.pm,
										 right_arrow_bits, right_arrow_width,
										 right_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		CG.right_arrow_w = right_arrow_width;
		CG.right_arrow_h = right_arrow_width;

		CG.is_fs[ i ] = SET;
		CG.has_been_shown[ i ] = UNSET;
		CG.can_undo[ i ] = UNSET;
	}

	/* Set the pixmaps for the out-of-range arrow to the pixmaps with the
	   color associated with the currently displayed curve */

	cv->up_arrow    = CG.up_arrows[    G.active_curve ];
	cv->down_arrow  = CG.down_arrows[  G.active_curve ];
	cv->left_arrow  = CG.left_arrows[  G.active_curve ];
	cv->right_arrow = CG.right_arrows[ G.active_curve ];

	/* Create a GC for the font and set the appropriate colour */

	cv->font_gc = XCreateGC( G.d, FL_ObjWin( G.cut_canvas.obj ), 0, 0 );
	if ( G.font != NULL )
		XSetFont( G.d, cv->font_gc, G.font->fid );
	XSetForeground( G.d, cv->font_gc,
					fl_get_pixel( G.colors[ G.active_curve ] ) );
	XSetBackground( G.d, cv->font_gc, fl_get_pixel( FL_BLACK ) );

	CG.nx = 0;

	fl_set_object_shortcutkey( cut_form->top_button, XK_Page_Up );
	fl_set_object_shortcutkey( cut_form->bottom_button, XK_Page_Down );

	/* The cut windows y-axis is always the same as the promary windows
	   z-axis, so we can re-use the label */

	if ( G.label[ Z ] != NULL && G.font != NULL )
	{
		G.label_pm[ Y + 3 ] = G.label_pm[ Z ];
		G.label_w[ Y + 3 ]  = G.label_w[ Z ];
		G.label_h[ Y + 3 ]  = G.label_h[ Z ];
	}
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static int cut_form_close_handler( FL_FORM *a, void *b )
{
	a = a;
	b = b;

	cut_close_callback( cut_form->cut_close_button, 0 );
	return FL_IGNORE;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_form_close( void )
{
	int i;
	Curve_1d *cv = &G.cut_curve;


	if ( ! CG.is_shown )
		return;

	/* Get rid of canvas related stuff (needs to be done *before*
	   hiding the form) */

	cut_canvas_off( &G.cut_x_axis, cut_form->cut_x_axis );
	cut_canvas_off( &G.cut_y_axis, cut_form->cut_y_axis );
	cut_canvas_off( &G.cut_z_axis, cut_form->cut_z_axis );
	cut_canvas_off( &G.cut_canvas, cut_form->cut_canvas );

	/* Deallocate the pixmaps for the out-of-range arrows */

	for ( i = 0; i < MAX_CURVES; i++ )
	{
		XFreePixmap( G.d, CG.up_arrows[ i ] );
		XFreePixmap( G.d, CG.down_arrows[ i ] );
		XFreePixmap( G.d, CG.left_arrows[ i ] );
		XFreePixmap( G.d, CG.right_arrows[ i ] );
	}

	/* Get rid of pixmap for label */

	if ( CG.cut_dir == X && G.label[ X ] != NULL && G.font != NULL )
		XFreePixmap( G.d, G.label_pm[ Z + 3 ] );

	/* Get rid of GCs and memory allocated for the curve */

	XFreeGC( G.d, cv->font_gc );
	cv->points  = T_free( cv->points );
	cv->xpoints = T_free( cv->xpoints );

	cut_x = cut_form->cut->x;
	cut_y = cut_form->cut->y;
	cut_w = cut_form->cut->w;
	cut_h = cut_form->cut->h;

	fl_hide_form( cut_form->cut );

	fl_free_form( cut_form->cut );

	CG.is_shown = is_mapped = UNSET;
}


/*-------------------------------------------------------------*/
/* Instead of calling fl_hide_form() directly unmap the window */
/* to make it easier to show it again (otherwise we'd have to  */
/* first delete and later to recreate lots of stuff).          */
/*-------------------------------------------------------------*/

void cut_close_callback( FL_OBJECT *a, long b )
{
	int i;


	a = a;
	b = b;

	G.is_cut = is_mapped = UNSET;

	cut_x = cut_form->cut->x;
	cut_y = cut_form->cut->y;
	cut_w = cut_form->cut->w;
	cut_h = cut_form->cut->h;

	XUnmapWindow( G.d, cut_form->cut->window );

	for ( i = 0; i < MAX_CURVES; i++ )
		CG.has_been_shown[ i ] = UNSET;
}


/*----------------------------------------------------------*/
/* Extracts the points of the 2d-surface in direction 'dir' */
/* at point 'index', writes them into a 1d-curve and scales */
/* this curve.                                              */
/*----------------------------------------------------------*/

static void cut_calc_curve( int dir, long p_index, bool has_been_shown )
{
	long i;
	Curve_1d *cv = &G.cut_curve;
	Curve_2d *scv = G.curve_2d[ G.active_curve ];
	Scaled_Point *sp, *ssp;


	/* Store direction of cross section - if called with an unreasonable
	   direction (i.e. not X or Y) keep the old direction) */

	if ( dir == X || dir == Y )
		CG.cut_dir = dir;

	/* Set maximum number of points of cut curve number and also set the x
	   scale factor if the number changed of points changed and we're in
	   full scale mode */

	CG.nx = ( CG.cut_dir == X ) ? G.ny : G.nx;

	if ( ! CG.is_shown || ! is_mapped || ! has_been_shown )
	{
		if ( scv->is_fs )
		{
			CG.s2d[ G.active_curve ][ X ] = cv->s2d[ X ] = 
				                    ( double ) ( G.cut_canvas.w - 1 ) /
				                    ( double ) ( CG.nx - 1 );
			CG.s2d[ G.active_curve ][ Y ] = cv->s2d[ Y ] =
				                    ( double ) ( G.cut_canvas.h - 1 );
			CG.shift[ G.active_curve ][ X ] = CG.shift[ G.active_curve ][ Y ]
				                       = cv->shift[ X ] = cv->shift[ Y ] = 0.0;

			CG.is_fs[ G.active_curve ] = SET;
			fl_set_button( cut_form->cut_full_scale_button, 1 );
			if ( ! ( cmdline_flags & NO_BALLOON ) )
				fl_set_object_helper( cut_form->cut_full_scale_button,
									  "Switch off automatic rescaling" );
		}
		else
		{
			CG.s2d[ G.active_curve ][ Y ] = cv->s2d[ Y ] =
				                  scv->s2d[ Z ] / ( double ) ( G.z_axis.h - 1 )
						          * ( double ) ( G.cut_canvas.h - 1 );
			CG.shift[ G.active_curve ][ Y ] = cv->shift[ Y ] = scv->shift[ Z ];

			if ( CG.cut_dir == X )
			{
				CG.s2d[ G.active_curve ][ X ] = cv->s2d[ X ] =
					              scv->s2d[ Y ] / ( double ) ( G.canvas.h - 1 )
							      * ( double ) ( G.cut_canvas.w - 1 );
				CG.shift[ G.active_curve ][ X ] = cv->shift[ X ] =
					                                           scv->shift[ Y ];
			}
			else
			{
				CG.s2d[ G.active_curve ][ X ] = cv->s2d[ X ] =
					              scv->s2d[ X ] / ( double ) ( G.canvas.w - 1 )
							      * ( double ) ( G.cut_canvas.w - 1 );
				CG.shift[ G.active_curve ][ X ] = cv->shift[ X ] =
					                                           scv->shift[ X ];
			}

			CG.is_fs[ G.active_curve ] = UNSET;
			fl_set_button( cut_form->cut_full_scale_button, 0 );
			if ( ! ( cmdline_flags & NO_BALLOON ) )
				fl_set_object_helper( cut_form->cut_full_scale_button,
									  "Rescale curves to fit into the window\n"
									  "and switch on automatic rescaling" );
		}
	}
	else if ( CG.is_fs[ G.active_curve ] )
	{
		CG.s2d[ G.active_curve ][ X ] =  cv->s2d[ X ] =
			      ( double ) ( G.cut_canvas.w - 1 ) / ( double ) ( CG.nx - 1 );
		CG.s2d[ G.active_curve ][ Y ] = cv->s2d[ Y ] =
			                                 ( double ) ( G.cut_canvas.h - 1 );
		CG.shift[ G.active_curve ][ X ] = CG.shift[ G.active_curve ][ Y ]
				                       = cv->shift[ X ] = cv->shift[ Y ] = 0.0;
	}

	/* If the index is reasonable store it (if called with index smaller than
	   zero keep the old index) */

	if ( p_index >= 0 )
		CG.index = p_index;

	/* Allocate memory for storing of scaled data and points for display */

	cv->points = T_realloc( cv->points, CG.nx * sizeof( Scaled_Point ) );
	cv->xpoints = T_realloc( cv->xpoints, CG.nx * sizeof( XPoint ) );

	/* Extract the existing scaled data of the cut from the 2d curve */

	if ( CG.cut_dir == X )
	{
		for ( i = 0, sp = cv->points, ssp = scv->points + CG.index;
			  i < CG.nx; ssp += G.nx, sp++, i++ )
			if ( ssp->exist )
				memcpy( sp, ssp, sizeof( Scaled_Point ) );
			else
				sp->exist = UNSET;
	}
	else
	{
		for ( i = 0, sp = cv->points, ssp = scv->points + CG.index * G.nx;
			  i < CG.nx; ssp++, sp++, i++ )
			if ( ssp->exist )
				memcpy( sp, ssp, sizeof( Scaled_Point ) );
			else
				sp->exist = UNSET;
	}

	cut_recalc_XPoints( );
}


/*-------------------------------------------------------*/
/* The function calculates the points as to be displayed */
/* from the scaled points. As a side effect it also sets */
/* the flags that indicate if out of range arrows have   */
/* be shown and counts the number of displayed points.   */
/*-------------------------------------------------------*/

static void cut_recalc_XPoints( void )
{
	long k, j;
	Curve_1d *cv = &G.cut_curve;
	Scaled_Point *sp = cv->points;
	XPoint *xp = cv->xpoints;


	cv->up = cv->down = cv->left = cv->right = UNSET;

	for ( k = j = 0; j < CG.nx; sp++, j++ )
	{
		if ( ! sp->exist )
			continue;

		xp->x = d2shrt( cv->s2d[ X ] * ( j + cv->shift[ X ] ) );
		xp->y = i2shrt( G.cut_canvas.h ) - 1 -
			   d2shrt( cv->s2d[ Y ] * ( cv->points[ j ].v + cv->shift[ Y ] ) );
		sp->xp_ref = k;

		cv->left  |= ( xp->x < 0 );
		cv->right |= ( xp->x >= ( int ) G.cut_canvas.w );
		cv->up    |= ( xp->y < 0 );
		cv->down  |= ( xp->y >= ( int ) G.cut_canvas.h );

		xp++;
		k++;
	}

	G.cut_curve.count = k;
}


/*------------------------------------------------------*/
/* Called whenever a different curve is to be displayed */
/*------------------------------------------------------*/

void cut_new_curve_handler( void )
{
	Curve_1d *cv = &G.cut_curve;
	int i;


	if ( ! G.is_cut )
		return;

	/* If curve shown until now (if there was one) wasn't displayed in full
	   scale mode store its scaling factors */

	if ( CG.curve != -1 && ! CG.is_fs[ CG.curve ] )
	{
		for ( i = X; i <= Y; i++ )
		{
			CG.s2d[ CG.curve ][ i ] = cv->s2d[ i ];
			CG.shift[ CG.curve ][ i ] = cv->shift[ i ];
			CG.old_s2d[ CG.curve ][ i ] = cv->old_s2d[ i ];
			CG.old_shift[ CG.curve ][ i ] = cv->old_shift[ i ];
		}
	}

	if ( G.active_curve == -1 )
	{
		/* Remove the current curve */

		CG.curve = -1;
		cv->points = T_free( cv->points );
		cv->xpoints = T_free( cv->xpoints );
		cv->count = 0;
	}
	else
	{
		/* If new curve hasn't been shown yet use display mode of 2d curve
		   for the curve */

		if ( ! CG.has_been_shown[ G.active_curve ] )
			CG.is_fs[ G.active_curve ] = G.curve_2d[ G.active_curve ]->is_fs;

		if ( CG.is_fs[ G.active_curve ] )
		{
			fl_set_button( cut_form->cut_full_scale_button, 1 );
			if ( ! ( cmdline_flags & NO_BALLOON ) )
				fl_set_object_helper( cut_form->cut_full_scale_button,
									  "Switch off automatic rescaling" );
		}
		else
		{
			/* Get stored scaling factors (if there are any) */

			if ( CG.has_been_shown[ G.active_curve ] )
			{
				for ( i = X; i <= Y; i++ )
				{
					cv->s2d[ i ] = CG.s2d[ G.active_curve ][ i ];
					cv->shift[ i ] = CG.shift[ G.active_curve ][ i ];
					cv->old_s2d[ i ] = CG.old_s2d[ G.active_curve ][ i ];
					cv->old_shift[ i ] = CG.old_shift[ G.active_curve ][ i ];
				}
			}

			fl_set_button( cut_form->cut_full_scale_button, 0 );
			if ( ! ( cmdline_flags & NO_BALLOON ) )
				fl_set_object_helper( cut_form->cut_full_scale_button,
									  "Rescale curve to fit into the window\n"
									  "and switch on automatic rescaling" );
		}

		CG.curve = G.active_curve;

		cv->up_arrow    = CG.up_arrows[ G.active_curve ];
		cv->down_arrow  = CG.down_arrows[ G.active_curve ];
		cv->left_arrow  = CG.left_arrows[ G.active_curve ];
		cv->right_arrow = CG.right_arrows[ G.active_curve ];

		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );
		XSetForeground( G.d, cv->font_gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );

		/* Get the data for the curve and set flag that says that everything
		   necessary has been set */

		cut_calc_curve( -1, -1, CG.has_been_shown[ G.active_curve ] );
		CG.has_been_shown[ G.active_curve ] = SET;
	}

	redraw_all_cut_canvases( );
}


/*---------------------------------------------------------------------------*/
/* Function is called by accept_2d_data() whenever the z-scaling has changed */
/* and thus also the y-scaling of the cut curve                              */
/*---------------------------------------------------------------------------*/

bool cut_data_rescaled( long curve, double y_min, double y_max )
{
	long i;
	Curve_1d *cv  = &G.cut_curve;
	Curve_2d *scv = G.curve_2d[ G.active_curve ];
	Scaled_Point *sp, *ssp;


	if ( ! G.is_cut )
		return FAIL;

	if ( ! CG.is_fs[ curve ] )
	{
		/* Calculate s2d[ Y ] and shift[ Y ] to fit the new scaling */

		CG.s2d[ curve ][ Y ] *=
			           ( y_max - y_min ) / G.curve_2d[ curve ]->rwc_delta[ Z ];
		CG.shift[ curve ][ Y ] = 
			     ( G.curve_2d[ curve ]->rwc_delta[ Z ] * CG.shift[ curve ][ Y ]
				   - G.curve_2d[ curve ]->rwc_start[ Z ] + y_min ) 
				 / ( y_max - y_min );

		if ( CG.can_undo[ curve ] )
		{
			CG.old_s2d[ curve ][ Y ] *=
			           ( y_max - y_min ) / G.curve_2d[ curve ]->rwc_delta[ Z ];
			CG.old_shift[ curve ][ Y ] = 
			     ( G.curve_2d[ curve ]->rwc_delta[ Z ]
				   * CG.old_shift[ curve ][ Y ]
				   - G.curve_2d[ curve ]->rwc_start[ Z ] + y_min ) 
				 / ( y_max - y_min );
		}

		if ( curve == G.active_curve )
		{
			cv->s2d[ Y ] = CG.s2d[ curve ][ Y ];
			cv->shift[ Y ] = CG.shift[ curve ][ Y ];

			if ( CG.can_undo[ curve ] )
			{
				cv->old_s2d[ Y ] = CG.old_s2d[ curve ][ Y ];
				cv->old_shift[ Y ] = CG.old_shift[ curve ][ Y ];
			}
		}
	}

	if ( curve != G.active_curve )
		return FAIL;

	/* Extract the rescaled data of the cut from the 2d curve */

	if ( CG.cut_dir == X )
	{
		for ( i = 0, sp = cv->points, ssp = scv->points + CG.index;
			  i < CG.nx; ssp += G.nx, sp++, i++ )
			if ( ssp->exist )
			{
				sp->v = ssp->v;
				cv->xpoints[ sp->xp_ref ].y =
					i2shrt( G.cut_canvas.h ) - 1
					     - d2shrt( cv->s2d[ Y ] * ( sp->v + cv->shift[ Y ] ) );
			}
	}
	else
	{
		for ( i = 0, sp = cv->points, ssp = scv->points + CG.index * G.nx;
			  i < CG.nx; ssp++, sp++, i++ )
			if ( ssp->exist )
			{
				sp->v = ssp->v;
				cv->xpoints[ sp->xp_ref ].y =
					i2shrt( G.cut_canvas.h )- 1
					     - d2shrt( cv->s2d[ Y ] * ( sp->v + cv->shift[ Y ] ) );
			}
	}

	/* Signal calling routine that redraw of the cut curve is needed */

	return OK;
}


/*------------------------------------------------------------------------*/
/* Function gets called by accept_2d_data() whenever the number of points */
/* in x- or y-direction changes. The first argument is the direction the  */
/* data set changed (i.e. if there are new data in x- or in y-direction)  */
/* while the second is the new number of points in this direction.        */
/* This function does not get the new data, it just has to extend and     */
/* initialize the memory that will be used for them.                      */
/*------------------------------------------------------------------------*/

bool cut_num_points_changed( int dir, long num_points )
{
	Curve_1d *cv = &G.cut_curve;
	Scaled_Point *sp;
	long k;


	/* Nothing to be done if the number of points didn't change in the cut
	   direction or no curve is displayed */

	if ( dir == CG.cut_dir || ! G.is_cut || CG.curve == -1 )
		return FAIL;

	/* Extend the arrays for the (scaled) data and the array of XPoints */

	cv->points = T_realloc( cv->points, num_points * sizeof( Scaled_Point ) );
	cv->xpoints = T_realloc( cv->xpoints, num_points * sizeof( XPoint ) );

	/* The new entries are not set yet */

	for ( k = CG.nx, sp = G.cut_curve.points + k; k < num_points; sp++, k++ )
		sp->exist = UNSET;

	/* In full scale mode the x-axis scale must be reset and all points need to
	   be rescaled */

	CG.nx = num_points;

	if ( CG.is_fs[ G.active_curve ] )
	{
		CG.s2d[ G.active_curve ][ X ] = cv->s2d[ X ] =
			 ( double ) ( G.cut_canvas.w - 1 ) / ( double ) ( num_points - 1 );
		for ( sp = G.cut_curve.points, k = 0; k < CG.nx; sp++, k++ )
			if ( sp->exist )
				cv->xpoints[ sp->xp_ref ].x = d2shrt( cv->s2d[ X ] * k );
	}

	/* Signal calling routine that redraw of cut curve is needed */

	return CG.is_fs[ G.active_curve ];
}


/*--------------------------------------------------------------------------*/
/* This function gets called with new data that might need to be displayed. */
/* Before this function gets called the function cut_num_points_changed()   */
/* has already been executed if the new data don't overwrite existing data  */
/* but are for positions where no data have been shown yet.                 */
/*--------------------------------------------------------------------------*/

bool cut_new_points( long curve, long x_index, long y_index, long len )
{
	Scaled_Point *sp;
	long p_index;


	/* Nothing to be done if either the cross section isn't drawn or the new
	   points do not belong to the currently shown curve */

	if ( ! G.is_cut || CG.curve == -1 || curve != G.active_curve )
		return FAIL;

	/* We need a different handling for cuts in X and Y direction. If the cut
	   is in through the x-axis we have to pick no more than one point from
	   the new data while for cuts through the y-axis we need either all the
	   data or none at all */

	if ( CG.cut_dir == X )
	{
		if ( x_index > CG.index || x_index + len <= CG.index )
			return FAIL;

		sp = G.curve_2d[ curve ]->points + y_index * G.nx + CG.index;

#ifndef NDEBUG
		/* There were two crashes, probably from the sp->v in the following
		   cut_integrate_point() call, which I didn't find the reason for
		   yet. Here I just try to avoid the problem in order to keep
		   experiments from crashing. */

		if ( sp == NULL )
		{
			eprint( SEVERE, UNSET, "Internal error detected at %s:%d, %ld.\n"
					"Please send a bug report immediately, including this "
					"error message!\n", __FILE__, __LINE__, CG.index );
			return FAIL;
		}
#endif

		cut_integrate_point( y_index, sp->v );
	}
	else
	{
		if ( y_index != CG.index )
			return FAIL;

		/* All new points are on the cut */

		sp = G.curve_2d[ curve ]->points + y_index * G.nx + x_index;
		for ( p_index = x_index; p_index < x_index + len; sp++, p_index++ )
			cut_integrate_point( p_index, sp->v );
	}

	/* Signal calling routine that a redraw of the cut curve is needed */

	return OK;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void cut_integrate_point( long p_index, double val )
{
	Curve_1d *cv = &G.cut_curve;
	Scaled_Point *cvp;
	long xp_index;
	long i, j;


	/* If this is a completely new point integrate it into the array of
	   XPoints (which have to be sorted in ascending order of the
	   x-coordinate), otherwise the XPoint just needs to be updated */

	if ( ! cv->points[ p_index ].exist )
	{
		/* Find next existing point to the left */

		for ( i = p_index - 1, cvp = cv->points + i; i >= 0 && ! cvp->exist;
			  cvp--, i-- )
			;

		if ( i == -1 )                    /* new points to be drawn is first */
		{
			xp_index = cv->points[ p_index ].xp_ref = 0;
			memmove( cv->xpoints + 1, cv->xpoints,
					 cv->count * sizeof( XPoint ) );
			for ( cvp = cv->points + 1, j = 1; j < CG.nx; cvp++, j++ )
				if ( cvp->exist )
					cvp->xp_ref++;
		}
		else if ( cv->points[ i ].xp_ref == cv->count - 1 )    /* ...is last */
			xp_index = cv->points[ p_index ].xp_ref = cv->count;
		else                                             /* ...is in between */
		{
			xp_index = cv->points[ p_index ].xp_ref =
				                                    cv->points[ i ].xp_ref + 1;
			memmove( cv->xpoints + xp_index + 1, cv->xpoints + xp_index,
					 ( cv->count - xp_index ) * sizeof( XPoint ) );
			for ( j = p_index + 1, cvp = cv->points + j; j < CG.nx;
				  cvp++, j++ )
				if ( cvp->exist )
					cvp->xp_ref++;
		}

		/* Calculate the x-coordinate of the new point and figure out if it
		   exceeds the borders of the canvas */

		cv->xpoints[ xp_index ].x = d2shrt( cv->s2d[ X ]
											* ( p_index + cv->shift[ X ] ) );
		if ( cv->xpoints[ xp_index ].x < 0 )
			cv->left = SET;
		if ( cv->xpoints[ xp_index ].x >= ( int ) G.cut_canvas.w )
			cv->right = SET;

		/* Increment the number of points belonging to the cut */

		cv->count++;
		cv->points[ p_index ].exist = SET;
	}
	else
		xp_index = cv->points[ p_index ].xp_ref;

	/* Store the (new) points value */

	cv->points[ p_index ].v = val;

	/* Calculate the y-coordinate of the (new) point and figure out if it
	   exceeds the borders of the canvas */

	cv->xpoints[ xp_index ].y = i2shrt( G.cut_canvas.h ) - 1
							- d2shrt( cv->s2d[ Y ]
							  * ( cv->points[ p_index ].v + cv->shift[ Y ] ) );

	if ( cv->xpoints[ xp_index ].y < 0 )
		cv->up = SET;
	if ( cv->xpoints[ xp_index ].y >= ( int ) G.cut_canvas.h )
		cv->down = SET;
}


/*------------------------------------------------------------*/
/* Handler for all events the cut graphics window may receive */
/*------------------------------------------------------------*/

static int cut_canvas_handler( FL_OBJECT *obj, Window window, int w, int h,
							   XEvent *ev, void *udata )
{
	Canvas *c = ( Canvas * ) udata;


	switch ( ev->type )
    {
        case Expose :
            if ( ev->xexpose.count == 0 )     /* only react to last in queue */
				redraw_all_cut_canvases( );
            break;

		case ConfigureNotify :
            if ( ( int ) c->w == w && ( int ) c->h == h )
				break;
            cut_reconfigure_window( c, w, h );
            break;

		case ButtonPress :
			cut_press_handler( obj, window, ev, c );
			break;

		case ButtonRelease :
			cut_release_handler( obj, window, ev, c );
			break;

		case MotionNotify :
			cut_motion_handler( obj, window, ev, c );
			break;
	}

	return 1;
}


/*----------------------------------------------------------------*/
/* Handler for ConfigureNotify events the cut graphic window gets */
/*----------------------------------------------------------------*/

static void cut_reconfigure_window( Canvas *c, int w, int h )
{
	int i;
	static bool is_reconf[ 3 ] = { UNSET, UNSET, UNSET };
	static bool need_redraw[ 3 ] = { UNSET, UNSET, UNSET };
	Curve_1d *cv = &G.cut_curve;
	int old_w = c->w,
		old_h = c->h;


	/* Set the new canvas sizes */

	c->w = i2ushrt( w );
	c->h = i2ushrt( h );

	/* Calculate the new scale factors */

	if ( c == &G.cut_canvas )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			CG.s2d[ i ][ X ] *= ( double ) ( w - 1 )
				                / ( double ) ( old_w - 1 );
			CG.s2d[ i ][ Y ] *= ( double ) ( h - 1 )
				                / ( double ) ( old_h - 1 );

			if ( CG.can_undo[ i ] )
			{
				CG.old_s2d[ i ][ X ] *=
					           ( double ) ( w - 1 ) / ( double ) ( old_w - 1 );
				CG.old_s2d[ i ][ Y ] *=
					           ( double ) ( h - 1 ) / ( double ) ( old_h - 1 );
			}
		}

		if ( G.active_curve != -1 )
		{
			cv->s2d[ X ] *= ( double ) ( w - 1 ) / ( double ) ( old_w - 1 );
			cv->s2d[ Y ] *= ( double ) ( h - 1 ) / ( double ) ( old_h - 1 );

			if ( CG.can_undo[ G.active_curve ] )
			{
				cv->old_s2d[ X ] *= ( double ) ( w - 1 )
					                / ( double ) ( old_w - 1 );
				cv->old_s2d[ Y ] *= ( double ) ( h - 1 )
					                / ( double ) ( old_h - 1 );
			}
		}

		/* Recalculate data for drawing (has to be done after setting of canvas
		   sizes since they are needed in the recalculation) */

		cut_recalc_XPoints( );
	}

	/* Delete the old pixmap for the canvas and get a new one with the proper
	   sizes. Also do some small changes necessary. */

	delete_pixmap( c );
	create_pixmap( c );
	if ( c == &G.cut_canvas )
		XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
	XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );

	/* We can't know the sequence the different canvases are reconfigured in
	   but, on the other hand, redrawing an axis canvases is useless before
	   the new scaling factors are set. Thus we need in the call for the
	   canvas window to redraw also axis windows which got reconfigured
	   before. */

	if ( c == &G.cut_canvas )
	{
		redraw_cut_canvas( c );

		if ( need_redraw[ X ] )
		{
			redraw_cut_canvas( &G.cut_x_axis );
			need_redraw[ X ] = UNSET;
		}
		else if ( w != old_w )
			is_reconf[ X ] = SET;

		if ( need_redraw[ Y ] )
		{
			redraw_cut_canvas( &G.cut_y_axis );
			need_redraw[ Y ] = UNSET;
		}
		else if ( h != old_h )
			is_reconf[ Y ] = SET;

		if ( need_redraw[ Z ] )
		{
			redraw_cut_canvas( &G.cut_z_axis );
			need_redraw[ Z ] = UNSET;
		}
		else if ( h != old_h )
			is_reconf[ Z ] = SET;
	}

	if ( c == &G.cut_x_axis )
	{
		if ( is_reconf[ X ] )
		{
			redraw_cut_canvas( c );
			is_reconf[ X ] = UNSET;
		}
		else
			need_redraw[ X ] = SET;
	}

	if ( c == &G.cut_y_axis )
	{
		if ( is_reconf[ Y ] )
		{
			redraw_cut_canvas( c );
			is_reconf[ Y ] = UNSET;
		}
		else
			need_redraw[ Y ] = SET;
	}

	if ( c == &G.cut_z_axis )
	{
		if ( is_reconf[ Z ] )
		{
			redraw_cut_canvas( c );
			is_reconf[ Z ] = UNSET;
		}
		else
			need_redraw[ Z ] = SET;
	}
}


/*-----------------------------------------------------*/
/* Handler for ButtonPress events the cut graphic gets */
/*-----------------------------------------------------*/

static void cut_press_handler( FL_OBJECT *obj, Window window,
							   XEvent *ev, Canvas *c )
{
	int old_button_state = G.button_state;
	unsigned int keymask;


	/* In the axes areas pressing two buttons simultaneously doesn't has a
	   special meaning, so don't care about another button. Also don't react
	   if the pressed buttons have lost there meaning, there is no curve
	   displayed or the curve has no scaling yet */

	if ( ( c != &G.cut_canvas && G.raw_button_state != 0 ) ||
		 ( G.button_state == 0 && G.raw_button_state != 0 ) ||
		 G.active_curve == -1 ||
		 ! G.curve_2d[ G.active_curve ]->is_scale_set
		)
	{
		G.raw_button_state |= 1 << ( ev->xbutton.button - 1 );
		return;
	}

	G.raw_button_state |= ( 1 << ( ev->xbutton.button - 1 ) );

	/* Middle and right or all three buttons at once don't mean a thing */

	if ( G.raw_button_state >= 6 )
		return;

	G.button_state |= ( 1 << ( ev->xbutton.button - 1 ) );

	/* Find out which window gets the mouse events (all following mouse events
	   go to this window until all buttons are released) */

	if ( obj == cut_form->cut_x_axis )        /* in x-axis window */
		G.drag_canvas = 1;
	if ( obj == cut_form->cut_y_axis )        /* in y-axis window */
		G.drag_canvas = 2;
	if ( obj == cut_form->cut_z_axis )        /* in z-axis window */
		G.drag_canvas = 4;
	if ( obj == cut_form->cut_canvas )        /* in canvas window */
		G.drag_canvas = 7;

	fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

	switch ( G.button_state )
	{
		case 1 :                               /* left button */
			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			/* Set up variables for drawing the rubber boxes */

			switch ( G.drag_canvas )
			{
				case 1 :                       /* in x-axis window */
					fl_set_cursor( window, CG.cur_1 );

					c->box_x = c->ppos[ X ];
					c->box_w = 0;
					c->box_y = G.x_scale_offset + 1;
					c->box_h = G.enlarge_box_width;
					c->is_box = SET;
					break;

				case 2 :                       /* in y-axis window */
					fl_set_cursor( window, CG.cur_1 );

					c->box_x = c->w
						      - ( G.y_scale_offset + G.enlarge_box_width + 1 );
					c->box_y = c->ppos[ Y ];
					c->box_w = G.enlarge_box_width;
					c->box_h = 0;
					c->is_box = SET;
					break;

				case 4 :                       /* in z-axis window */
					fl_set_cursor( window, CG.cur_8 );
					G.cut_select = CUT_SELECT_X;
					c->box_x = c->box_h = 0;
					c->box_y = c->ppos[ Y ];
					c->box_w = ENLARGE_BOX_WIDTH;
					c->is_box = SET;
					break;

				case 7 :                       /* in canvas window */
					fl_set_cursor( window, CG.cur_1 );

					c->box_x = c->ppos[ X ];
					c->box_y = c->ppos[ Y ];
					c->box_w = c->box_h = 0;
					c->is_box = SET;
					break;
			}

			repaint_cut_canvas( c );
			break;

		case 2 :                               /* middle button */
			if ( G.drag_canvas == 4 )
				break;
			fl_set_cursor( window, CG.cur_2 );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			cut_save_scale_state( );
			break;

		case 3:                                /* left and middle button */
			if ( G.drag_canvas == 4 )
				break;
			fl_set_cursor( window, CG.cur_4 );

			/* Don't draw the box anymore */

			G.cut_canvas.is_box = UNSET;
			repaint_cut_canvas( &G.cut_canvas );
			break;

		case 4 :                               /* right button */
			if ( G.drag_canvas == 4 )
				break;
			fl_set_cursor( window, CG.cur_3 );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];
			break;

		case 5 :                               /* left and right button */
			if ( G.drag_canvas == 4 )
				break;
			fl_set_cursor( window, CG.cur_5 );

			if ( G.cut_canvas.is_box == UNSET && old_button_state != 4 )
			{
				G.start[ X ] = c->ppos[ X ];
				G.start[ Y ] = c->ppos[ Y ];
			}
			else
				G.cut_canvas.is_box = UNSET;

			repaint_cut_canvas( &G.cut_canvas );
			break;
	}
}


/*--------------------------------------------------------------*/
/* Handler for ButtonRelease events the cut graphic window gets */
/*--------------------------------------------------------------*/

static void cut_release_handler( FL_OBJECT *obj, Window window,
								 XEvent *ev, Canvas *c )
{
	unsigned int keymask;
	bool scale_changed = UNSET;


	obj = obj;

	/* If the released button didn't has a meaning just clear it from the
	   button state pattern and then forget about it */

	if ( ! ( ( 1 << ( ev->xbutton.button - 1 ) ) & G.button_state ) ||
		 G.active_curve == -1 )
	{
		G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );
		return;
	}

	/* Get mouse position and restrict it to the canvas */

	fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

	if ( c->ppos[ X ] < 0 )
		c->ppos[ X ] = 0;
	if ( c->ppos[ X ] >= ( int ) G.cut_canvas.w )
		c->ppos[ X ] = G.cut_canvas.w - 1;

	if ( c->ppos[ Y ] < 0 )
		c->ppos[ Y ] = 0;

	if ( c != &G.cut_z_axis )
	{
		if ( c->ppos[ Y ] >= ( int ) G.cut_canvas.h )
			c->ppos[ Y ] = G.cut_canvas.h - 1;
	}
	else if ( c->ppos[ Y ] >= ( int ) c->h )         /* in z-axis window */
		c->ppos[ Y ] = c->h - 1;

	switch ( G.button_state )
	{
		case 1 :                               /* left mouse button */
			switch ( G.drag_canvas )
			{
				case 1 :                       /* x-axis window */
					scale_changed = cut_change_x_range( c );
					break;

				case 2 :                       /* in y-axis window */
					scale_changed = cut_change_y_range( c );
					break;

				case 4 :                       /* in z-axis window */
					cut_show( CG.cut_dir,
							  lrnd( ( double ) ( c->h - 1 - c->ppos[ Y ] )
								    * ( ( CG.cut_dir == X ? G.nx : G.ny ) - 1 )
									/ c->h ) );
					break;

				case 7 :                       /* in canvas window */
					scale_changed = cut_change_xy_range( c );
					break;
			}

			c->is_box = UNSET;
			break;

		case 2 :                               /* middle mouse button */
			if ( G.drag_canvas & 1 )
				redraw_cut_canvas( &G.cut_x_axis );
			if ( G.drag_canvas & 2 )
				redraw_cut_canvas( &G.cut_y_axis );
			if ( G.drag_canvas & 4 )
				redraw_cut_canvas( &G.cut_z_axis );
			break;

		case 4 :                               /* right mouse button */
			switch ( G.drag_canvas )
			{
				case 1 :                       /* in x-axis window */
					scale_changed = cut_zoom_x( c );
					break;

				case 2 :                       /* in y-axis window */
					scale_changed = cut_zoom_y( c );
					break;

				case 7 :                       /* in canvas window */
					scale_changed = cut_zoom_xy( c );
					break;
			}
			break;
	}

	G.button_state = 0;
	G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );

	G.cut_select = NO_CUT_SELECT;
	fl_reset_cursor( window );

	if ( scale_changed )
	{
		if ( CG.is_fs[ G.active_curve ] )
		{
			CG.is_fs[ G.active_curve ] = UNSET;
			fl_set_button( cut_form->cut_full_scale_button, 0 );
			if ( ! ( cmdline_flags & NO_BALLOON ) )
				fl_set_object_helper( cut_form->cut_full_scale_button,
									  "Rescale curve to fit into the window\n"
									  "and switch on automatic rescaling" );
		}

		cut_recalc_XPoints( );
		redraw_all_cut_canvases( );
	}

	if ( ! scale_changed || c != &G.cut_canvas )
		repaint_cut_canvas( c );
}


/*-------------------------------------------------------------*/
/* Handler for MotionNotify events the cut graphic window gets */
/*-------------------------------------------------------------*/

static void cut_motion_handler( FL_OBJECT *obj, Window window,
								XEvent *ev, Canvas *c )
{
	XEvent new_ev;
	bool scale_changed = UNSET;
	unsigned int keymask;
	long p_index;

	
	obj = obj;

	/* We do event compression to avoid being flooded with motion events -
	   instead of handling them all individually we only react to the latest
	   in the series of motion events for the current window */

	while ( fl_XEventsQueued( QueuedAfterReading ) > 0 )
	{
		fl_XPeekEvent( &new_ev );             /* look ahead for next event */

		/* Stop looking ahead if the next one isn't a motion event or is for
		   a different window */

		if ( new_ev.type != MotionNotify ||
			 new_ev.xmotion.window != ev->xmotion.window )
			break;

		fl_XNextEvent( ev );                  /* get the next event */
	}

	fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

	switch ( G.button_state )
	{
		case 1 :                               /* left mouse button */
			if ( G.drag_canvas & 1 )           /* x-axis or canvas window */
			{
				c->box_w = c->ppos[ X ] - G.start[ X ];

				if ( c->box_x + c->box_w >= ( int ) c->w )
					c->box_w = c->w - c->box_x - 1;

				if ( c->box_x + c->box_w < 0 )
					c->box_w = - c->box_x;
			}

			if ( G.drag_canvas & 2 )           /* y-axis or canvas window */
			{
				c->box_h = c->ppos[ Y ] - G.start[ Y ] ;

				if ( c->box_y + c->box_h >= ( int ) c->h )
					c->box_h = c->h - c->box_y - 1;

				if ( c->box_y + c->box_h < 0 )
					c->box_h = - c->box_y;
			}

			if ( G.drag_canvas == 4 )           /* z-axis window */
			{
				c->box_h = c->ppos[ Y ] - G.start[ Y ] ;

				if ( c->box_y + c->box_h >= ( int ) c->h )
					c->box_h = c->h - c->box_y - 1;

				if ( c->box_y + c->box_h < 0 )
					c->box_h = - c->box_y;
				p_index = lrnd( ( double ) ( c->h - 1 - c->ppos[ Y ] )
								* ( ( CG.cut_dir == X ? G.nx : G.ny ) - 1 )
								/ c->h );
				if ( p_index != CG.index )
				{
					cut_show( CG.cut_dir, p_index );
					break;
				}
			}

			repaint_cut_canvas( c );
			break;

		case 2 :                               /* middle button */
			shift_XPoints_of_cut_curve( c );
			scale_changed = SET;

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			/* Switch off full scale button if necessary */

			if ( CG.is_fs[ G.active_curve ] && scale_changed )
			{
				CG.is_fs[ G.active_curve ] = UNSET;
				fl_set_button( cut_form->cut_full_scale_button, 0 );
				if ( ! ( cmdline_flags & NO_BALLOON ) )
					fl_set_object_helper( cut_form->cut_full_scale_button,
									   "Rescale curve to fit into the window\n"
									   "and switch on automatic rescaling" );
			}

			redraw_cut_canvas( &G.cut_canvas );
			if ( G.drag_canvas & 1 )
				redraw_cut_canvas( &G.cut_x_axis );
			if ( G.drag_canvas & 2 )
				redraw_cut_canvas( &G.cut_y_axis );
			break;

		case 3 : case 5 :               /* left and (middle or right) button */
			repaint_cut_canvas( &G.cut_canvas );
			break;
	}
}


/*-------------------------------------------------------*/
/* Handler for the undo button in the cut graphic window */
/*-------------------------------------------------------*/

void cut_undo_button_callback( FL_OBJECT *a, long b )
{
	double temp_s2d,
		   temp_shift;
	Curve_1d *cv = &G.cut_curve;
	int j;


	a = a;
	b = b;

	if ( CG.curve == -1 || ! CG.can_undo[ G.active_curve ] )
		return;

	for ( j = 0; j <= Y; j++ )
	{
		temp_s2d = cv->s2d[ j ];
		temp_shift = cv->shift[ j ];

		cv->s2d[ j ] = cv->old_s2d[ j ];
		cv->shift[ j ] = cv->old_shift[ j ];

		cv->old_s2d[ j ] = temp_s2d;
		cv->old_shift[ j ] = temp_shift;
	}

	cut_recalc_XPoints( );

	if ( CG.is_fs[ G.active_curve ] )
	{
		CG.is_fs[ G.active_curve ] = UNSET;
		fl_set_button( cut_form->cut_full_scale_button, 0 );
		if ( ! ( cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( cut_form->cut_full_scale_button,
								  "Rescale curve to fit into the window\n"
								  "and switch on automatic rescaling" );
	}

	redraw_all_cut_canvases( );
}


/*-------------------------------------------------------------*/
/* Handler for the full scale button in the cut graphic window */
/*-------------------------------------------------------------*/

void cut_fs_button_callback( FL_OBJECT *a, long b )
{
	int state;


	a = a;
	b = b;

	/* Rescaling is useless if no cut curve is shown */

	if ( ! G.is_cut || CG.curve == -1 )
		return;

	/* Get new state of button */

	state = fl_get_button( cut_form->cut_full_scale_button );

	CG.is_fs[ CG.curve ] = state;

	if ( state )
	{
		if ( ! ( cmdline_flags & NO_BALLOON ) )
			fl_set_object_helper( cut_form->cut_full_scale_button,
								  "Switch off automatic rescaling" );
		
		cut_calc_curve( -1, -1, SET );
		redraw_all_cut_canvases( );
	}
	else if ( ! ( cmdline_flags & NO_BALLOON ) )
		fl_set_object_helper( cut_form->cut_full_scale_button,
							  "Rescale curve to fit into the window\n"
							  "and switch on automatic rescaling" );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void redraw_all_cut_canvases( void )
{
	if ( ! G.is_cut )
		return;

	redraw_cut_canvas( &G.cut_canvas );
	redraw_cut_canvas( &G.cut_x_axis );
	redraw_cut_canvas( &G.cut_y_axis );
	redraw_cut_canvas( &G.cut_z_axis );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void redraw_cut_canvas( Canvas *c )
{
	Curve_1d *cv = &G.cut_curve;


	/* Clear the canvas by drawing over its pixmap in the background color */

	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

	/* For the main canvas draw the curve and the out of range errors */

	if ( c == &G.cut_canvas && CG.curve != -1 && cv->count > 1 )
	{
		XDrawLines( G.d, c->pm, cv->gc, cv->xpoints, cv->count, 
					CoordModeOrigin );

		if ( cv->up )
			XCopyArea( G.d, cv->up_arrow, c->pm, c->gc, 0, 0,
					   G.up_arrow_w, G.up_arrow_h,
					   ( G.cut_canvas.w - G.up_arrow_w ) / 2, 5 );

		if ( cv->down )
			XCopyArea( G.d, cv->down_arrow, c->pm, c->gc, 0, 0,
					   G.down_arrow_w, G.down_arrow_h,
					   ( G.cut_canvas.w - G.down_arrow_w ) / 2,
					   G.cut_canvas.h - 5 - G.down_arrow_h );

		if ( cv->left )
			XCopyArea( G.d, cv->left_arrow, c->pm, c->gc, 0, 0,
					   G.left_arrow_w, G.left_arrow_h, 5,
					   ( G.cut_canvas.h - G.left_arrow_h ) / 2 );

		if ( cv->right )
			XCopyArea( G.d, cv->right_arrow, c->pm, c->gc, 0, 0,
					   G.right_arrow_w, G.right_arrow_h,
					   G.cut_canvas.w - 5 - G.right_arrow_w,
					   ( G.cut_canvas.h - G.right_arrow_h ) / 2 );
	}

	if ( c == &G.cut_x_axis )
		redraw_cut_axis( X );

	if ( c == &G.cut_y_axis )
		redraw_cut_axis( Y );

	if ( c == &G.cut_z_axis )
		redraw_cut_axis( Z );

	/* Finally copy the pixmap onto the screen */

	repaint_cut_canvas( c );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void repaint_cut_canvas( Canvas *c )
{
	Pixmap pm;
	Curve_1d *cv = &G.cut_curve;
	Curve_2d *scv = G.curve_2d[ G.active_curve ];
	char buf[ 256 ];
	int x, y, x2, y2;
	unsigned int w, h;
	double x_pos, y_pos;
	int r_coord;


	/* If no or either the middle or the left button is pressed no extra stuff
	   has to be drawn so just copy the pixmap with the curves into the
	   window. */

	if ( ! ( G.button_state & 1 ) )
	{
		XCopyArea( G.d, c->pm, FL_ObjWin( c->obj ), c->gc,
				   0, 0, c->w, c->h, 0, 0 );
		return;
	}

	/* Otherwise use another level of buffering and copy the pixmap with
	   the curves into another pixmap */
	
	pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h,
						fl_get_canvas_depth( c->obj ) );
	XCopyArea( G.d, c->pm, pm, c->gc, 0, 0, c->w, c->h, 0, 0 );

	/* Draw the rubber box if needed (i.e. when the left button is pressed
	   in the canvas currently to be drawn) */

	if ( G.button_state == 1 && c->is_box )
	{
		if ( G.cut_select == NO_CUT_SELECT )
		{
			if ( c->box_w > 0 )
			{
				x = c->box_x;
				w = c->box_w;
			}
			else
			{
				x = c->box_x + c->box_w;
				w = - c->box_w;
			}

			if ( c->box_h > 0 )
			{
				y = c->box_y;
				h = c->box_h;
			}
			else
			{
				y = c->box_y + c->box_h;
				h = - c->box_h;
			}

			XDrawRectangle( G.d, pm, c->box_gc, x, y, w, h );
		}
		else
		{
			x = 0;
			x2 = G.z_scale_offset + G.enlarge_box_width + 1;
			y = y2 = c->box_y + c->box_h;
			XDrawLine( G.d, pm, c->box_gc, x, y, x2, y2 );
		}
	}

	/* If this is the canvas and the left and either the middle or the right
	   mouse button is pressed draw the current mouse position (converted to
	   real world coordinates) or the difference between the current position
	   and the point the buttons were pressed at into the left hand top corner
	   of the canvas. In the second case also draw some marker connecting the
	   initial and the current position. */

	if ( c == &G.cut_canvas )
	{
		if ( G.button_state == 3 )
		{
			r_coord = CG.cut_dir == X ? Y : X;

			x_pos = scv->rwc_start[ r_coord ] + scv->rwc_delta[ r_coord ]
				    * ( c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ] );
			y_pos = scv->rwc_start[ Z ] + scv->rwc_delta[ Z ]
					* ( ( ( double ) G.cut_canvas.h - 1.0 - c->ppos[ Y ] )
						/ cv->s2d[ Y ] - cv->shift[ Y ] );

			strcpy( buf, " x = " );
			make_label_string( buf + 5, x_pos,
					    irnd( floor( log10( fabs(
				        scv->rwc_delta[ r_coord ] ) / cv->s2d[ X ] ) ) - 2 ) );
			strcat( buf, "   y = " ); 
			make_label_string( buf + strlen( buf ), y_pos,
							  irnd( floor( log10( fabs(
							  scv->rwc_delta[ Z ] ) / cv->s2d[ Y ] ) ) - 2 ) );
			strcat( buf, " " );

			if ( G.font != NULL )
				XDrawImageString( G.d, pm, cv->font_gc, 5,
								  G.font_asc + 5, buf, strlen( buf ) );
		}

		if ( G.button_state == 5 )
		{
			r_coord = CG.cut_dir == X ? Y : X;

			x_pos = scv->rwc_delta[ r_coord ]
				    * ( c->ppos[ X ] - G.start[ X ] ) / cv->s2d[ X ];
			y_pos = scv->rwc_delta[ Z ]
					* ( G.start[ Y ] - c->ppos[ Y ] ) / cv->s2d[ Y ];

			sprintf( buf, " dx = %#g   dy = %#g ", x_pos, y_pos );
			if ( G.font != NULL )
				XDrawImageString( G.d, pm, cv->font_gc, 5,
								  G.font_asc + 5, buf, strlen( buf ) );

			XSetForeground( G.d, G.cut_curve.gc, fl_get_pixel( FL_RED ) );
			XDrawArc( G.d, pm, G.cut_curve.gc,
					  G.start[ X ] - 5, G.start[ Y ] - 5, 10, 10, 0, 23040 );
			XSetForeground( G.d, G.cut_curve.gc,
							fl_get_pixel( G.colors[ G.active_curve ] ) );
			XDrawLine( G.d, pm, c->box_gc, G.start[ X ], G.start[ Y ],
					   c->ppos[ X ], G.start[ Y ] );
			XDrawLine( G.d, pm, c->box_gc, c->ppos[ X ], G.start[ Y ],
					   c->ppos[ X ], c->ppos[ Y ] );
		}
	}

	/* Finally copy the buffer pixmap onto the screen */

	XCopyArea( G.d, pm, FL_ObjWin( c->obj ), c->gc,
			   0, 0, c->w, c->h, 0, 0 );
	XFreePixmap( G.d, pm );
	XFlush( G.d );
}

/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void redraw_cut_axis( int coord )
{
	Canvas *c;
	int width;
	int r_coord;


	fsc2_assert( coord >= X && coord <= Z );

	/* First draw the label - for the x-axis it's just done by drawing the
	   string while for the y- and z-axis we have to copy a pixmap since the
	   label is a string rotated by 90 degree that has been drawn in advance */

	if ( coord == X )
	{
		c = &G.cut_x_axis;
		XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

		if ( CG.cut_dir == X )
			r_coord = Y;
		else
			r_coord = X;

		if ( G.label[ r_coord ] != NULL && G.font != NULL )
		{
			width = XTextWidth( G.font, G.label[ r_coord ],
								strlen( G.label[ r_coord ] ) );
			XDrawString( G.d, c->pm, c->font_gc, c->w - width - 5,
						 c->h - 5 - G.font_desc, G.label[ r_coord ],
						 strlen( G.label[ r_coord ] ) );
		}
	}
	else if ( coord == Y )
	{
		c = &G.cut_y_axis;
		XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

		if ( G.label[ Z ] != NULL && G.font != NULL )
			XCopyArea( G.d, G.label_pm[ Y + 3 ], c->pm, c->gc, 0, 0,
					   G.label_w[ Y + 3 ], G.label_h[ Y + 3 ], 0, 0 );
	}
	else
	{
		c = &G.cut_z_axis;
		XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

		if ( G.label[ coord ] != NULL && G.font != NULL )
			XCopyArea( G.d, G.label_pm[ coord + 3 ], c->pm, c->gc, 0, 0,
					   G.label_w[ coord + 3 ], G.label_h[ coord + 3 ],
					   c->w - 5 - G.label_w[ coord + 3 ], 0 );
	}

	if ( G.active_curve != -1 && G.curve_2d[ G.active_curve ]->is_scale_set )
		cut_make_scale( c, coord );
	XCopyArea( G.d, c->pm, FL_ObjWin( c->obj ), c->gc,
			   0, 0, c->w, c->h, 0, 0 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void cut_make_scale( Canvas *c, int coord )
{
	Curve_1d *cv  = &G.cut_curve;
	Curve_2d *cv2 = G.curve_2d[ G.active_curve ];
	double rwc_delta,          /* distance between small ticks (in rwc) */
		   order,              /* and its order of magnitude */
		   mag;
	double d_delta_fine,       /* distance between small ticks (in points) */
		   d_start_fine,       /* position of first small tick (in points) */
		   d_start_medium,     /* position of first medium tick (in points) */
		   d_start_coarse,     /* position of first large tick (in points) */
		   cur_p;              /* loop variable with position */
	int medium_factor,         /* number of small tick spaces between medium */
		coarse_factor;         /* and large tick spaces */
	int medium,                /* loop counters for medium and large ticks */
		coarse;
	double rwc_start,          /* rwc value of first point */
		   rwc_start_fine,     /* rwc value of first small tick */
		   rwc_start_medium,   /* rwc value of first medium tick */
		   rwc_start_coarse;   /* rwc value of first large tick */
	double rwc_coarse;
	short x, y;
	char lstr[ MAX_LABEL_LEN + 1 ];
	int width;
	short last = -1000;
	int r_coord;
	double r_scale;
	XPoint triangle[ 3 ];


	if ( coord == X )
	{
		r_coord = CG.cut_dir == X ? Y : X;
		r_scale = cv->s2d[ X ];
	}
	else if ( coord == Y )
	{
		r_coord = Z;
		r_scale = cv->s2d[ Y ];
	}
	else
	{
		if ( CG.cut_dir == X )
		{
			r_coord = X;
			r_scale = ( double ) ( c->h - 1 ) / ( double ) ( G.nx - 1 );
		}
		else
		{
			r_coord = Y;
			r_scale = ( double ) ( c->h - 1 ) / ( double ) ( G.ny - 1 );
		}
	}

	/* The distance between the smallest ticks should be `G.scale_tick_dist'
	   points - calculate the corresponding delta in real word units */

	rwc_delta = ( double ) G.scale_tick_dist
		        * fabs( cv2->rwc_delta[ r_coord ] ) / r_scale;

	/* Now scale this distance to the interval [ 1, 10 [ */

	mag = floor( log10( rwc_delta ) );
	order = pow( 10.0, mag );
	modf( rwc_delta / order, &rwc_delta );

	/* Get a `smooth' value for the ticks distance, i.e. either 2, 2.5, 5 or
	   10 and convert it to real world coordinates */

	if ( rwc_delta <= 2.0 )       /* in [ 1, 2 ] -> units of 2 */
	{
		medium_factor = 5;
		coarse_factor = 25;
		rwc_delta = 2.0 * order;
	}
	else if ( rwc_delta <= 3.0 )  /* in ] 2, 3 ] -> units of 2.5 */
	{
		medium_factor = 4;
		coarse_factor = 20;
		rwc_delta = 2.5 * order;
	}
	else if ( rwc_delta <= 6.0 )  /* in ] 3, 6 ] -> units of 5 */
	{
		medium_factor = 2;
		coarse_factor = 10;
		rwc_delta = 5.0 * order;
	}
	else                          /* in ] 6, 10 [ -> units of 10 */
	{
		medium_factor = 5;
		coarse_factor = 10;
		rwc_delta = 10.0 * order;
	}

	/* Calculate the final distance between the small ticks in points */

	d_delta_fine = r_scale * rwc_delta / fabs( cv2->rwc_delta[ r_coord ] );

	/* `rwc_start' is the first value in the display (i.e. the smallest x or y
	   value still shown in the canvas), `rwc_start_fine' the position of the
	   first small tick (both in real world coordinates) and, finally,
	   `d_start_fine' is the same position but in points */

	if ( coord != Z )
		rwc_start = cv2->rwc_start[ r_coord ]
			        - cv->shift[ coord ] * cv2->rwc_delta[ r_coord ];
	else                           /* z-axis always shows the complete range */
		rwc_start = cv2->rwc_start[ r_coord ];

	if ( cv2->rwc_delta[ r_coord ] < 0 )
		rwc_delta *= -1.0;

	modf( rwc_start / rwc_delta, &rwc_start_fine );
	rwc_start_fine *= rwc_delta;

	d_start_fine = r_scale * ( rwc_start_fine - rwc_start )
		           / cv2->rwc_delta[ r_coord ];
	if ( lrnd( d_start_fine ) < 0 )
		d_start_fine += d_delta_fine;

	/* Calculate start index (in small tick counts) of first medium tick */

	modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
	rwc_start_medium *= medium_factor * rwc_delta;

	d_start_medium = r_scale * ( rwc_start_medium - rwc_start )
			                 / cv2->rwc_delta[ r_coord ];
	if ( lrnd( d_start_medium ) < 0 )
		d_start_medium += medium_factor * d_delta_fine;

	medium = lrnd( ( d_start_fine - d_start_medium ) / d_delta_fine );

	/* Calculate start index (in small tick counts) of first large tick */

	modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
	rwc_start_coarse *= coarse_factor * rwc_delta;

	d_start_coarse = r_scale * ( rwc_start_coarse - rwc_start )
			                 / cv2->rwc_delta[ r_coord ];
	if ( lrnd( d_start_coarse ) < 0 )
	{
		d_start_coarse += coarse_factor * d_delta_fine;
		rwc_start_coarse += coarse_factor * rwc_delta;
	}

	coarse = lrnd( ( d_start_fine - d_start_coarse ) / d_delta_fine );

	/* Now, finally we got everything we need to draw the axis... */

	rwc_coarse = rwc_start_coarse - coarse_factor * rwc_delta;

	if ( coord == X )
	{
		/* Draw coloured line of scale */

		y = i2shrt( G.x_scale_offset );
		XFillRectangle( G.d, c->pm, cv->gc, 0, y - 2, c->w, 3 );

		/* Draw all the ticks and numbers */

		for ( cur_p = d_start_fine; cur_p < c->w; 
			  medium++, coarse++, cur_p += d_delta_fine )
		{
			x = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				XDrawLine( G.d, c->pm, c->font_gc, x, y + 3,
						   x, y - G.long_tick_len );
				rwc_coarse += coarse_factor * rwc_delta;
				if ( G.font == NULL )
					continue;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				if ( x - width / 2 - 10 > last )
				{
					XDrawString( G.d, c->pm, c->font_gc, x - width / 2,
								 y + G.label_dist + G.font_asc, lstr,
								 strlen( lstr ) );
					last = i2shrt( x + width / 2 );
				}
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x, y - G.medium_tick_len );
			else                                       /* short line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x, y - G.short_tick_len );
		}
	}
	else if ( coord == Y )
	{
		/* Draw coloured line of scale */

		x = i2shrt( c->w - G.y_scale_offset );
		XFillRectangle( G.d, c->pm, cv->gc, x, 0, 3, c->h );

		/* Draw all the ticks and numbers */

		for ( cur_p = ( double ) c->h - 1.0 - d_start_fine; cur_p > -0.5; 
			  medium++, coarse++, cur_p -= d_delta_fine )
		{
			y = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				XDrawLine( G.d, c->pm, c->font_gc, x - 3, y,
						   x + G.long_tick_len, y );
				rwc_coarse += coarse_factor * rwc_delta;

				if ( G.font == NULL )
					continue;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				XDrawString( G.d, c->pm, c->font_gc, x - G.label_dist - width,
							 y + G.font_asc / 2, lstr, strlen( lstr ) );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x + G.medium_tick_len, y );
			else                                      /* short line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x + G.short_tick_len, y );
		}
	}
	else
	{
		/* Draw coloured line of scale */

		x = i2shrt( G.z_scale_offset );
		XFillRectangle( G.d, c->pm, cv->gc, x - 2, 0, 3, c->h );

		/* Draw all the ticks and numbers */

		for ( cur_p = ( double ) c->h - 1.0 - d_start_fine; cur_p > -0.5;
			  medium++, coarse++, cur_p -= d_delta_fine )
		{
			y = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				XDrawLine( G.d, c->pm, c->font_gc, x + 3, y,
						   x - G.long_tick_len, y );
				rwc_coarse += coarse_factor * rwc_delta;

				if ( G.font == NULL )
					continue;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				XDrawString( G.d, c->pm, c->font_gc, x + G.label_dist,
							 y + G.font_asc / 2, lstr, strlen( lstr ) );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x - G.medium_tick_len, y );
			else                                      /* short line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x - G.short_tick_len, y );
		}

		/* Finally draw the triangle indicating the position of the cut */

		triangle[ 0 ].x = i2shrt( x - G.long_tick_len - 3 );
		if ( CG.cut_dir == X )
			triangle[ 0 ].y = d2shrt( ( G.cut_z_axis.h - 1 ) *
					 ( 1.0 - ( double ) CG.index / ( double ) ( G.nx - 1 ) ) );
		else
			triangle[ 0 ].y = d2shrt( ( G.cut_z_axis.h - 1 ) *
					 ( 1.0 - ( double ) CG.index / ( double ) ( G.ny - 1 ) ) );
		triangle[ 1 ].x = - ( G.z_scale_offset - G.long_tick_len - 10 );
		triangle[ 1 ].y = i2shrt( - G.long_tick_len / 3 );
		triangle[ 2 ].x = 0;
		triangle[ 2 ].y = i2shrt( 2 * ( G.long_tick_len / 3 ) );
		XFillPolygon( G.d, c->pm, G.curve_2d[ 0 ]->gc, triangle, 3,
					  Convex, CoordModePrevious );
	}
}


/*-------------------------------------------------------------------*/
/* This is basically a simplified version of `cut_recalc_XPoints()'  */
/* because we need to do much less calculations, i.e. just adding an */
/* offset to all XPoints instead of going through all the scalings...*/
/*-------------------------------------------------------------------*/

static void shift_XPoints_of_cut_curve( Canvas *c )
{
	long j, k;
	int dx = 0,
		dy = 0;
	int factor;
	Curve_1d *cv = &G.cut_curve;


	cv->up = cv->down = cv->left = cv->right = UNSET;

	/* Additionally pressing the right mouse button increases the amount the
	   curves are shifted by a factor of 5 */

	factor = G.raw_button_state == 6 ? 5 : 1;

	/* Calculate scaled shift factors */

	if ( G.drag_canvas & 1 )                      /* x-axis or canvas window */
	{
		dx = factor * ( c->ppos[ X ] - G.start[ X ] );
		cv->shift[ X ] += ( double ) dx / cv->s2d[ X ];
	}

	if ( G.drag_canvas & 2 )                      /* y-axis or canvas window */
	{
		dy = factor * ( c->ppos[ Y ] - G.start[ Y ] );
		cv->shift[ Y ] -= ( double ) dy / cv->s2d[ Y ];
	}

	/* Add the shifts to the XPoints */

	for ( k = 0, j = 0; j < CG.nx; j++ )
	{
		if ( ! cv->points[ j ].exist )
			continue;

		cv->xpoints[ k ].x = i2shrt( cv->xpoints[ k ].x + dx );
		cv->xpoints[ k ].y = i2shrt( cv->xpoints[ k ].y + dy );

		if ( cv->xpoints[ k ].x < 0 )
			cv->left = SET;
		if ( cv->xpoints[ k ].x >= ( int ) G.cut_canvas.w )
			cv->right = SET;
		if ( cv->xpoints[ k ].y < 0 )
			cv->up = SET;
		if ( cv->xpoints[ k ].y >= ( int ) G.cut_canvas.h )
			cv->down = SET;

		k++;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool cut_change_x_range( Canvas *c )
{
	Curve_1d *cv = &G.cut_curve;
	double x1, x2;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 )
		return UNSET;

	cut_save_scale_state( );

	x1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
	x2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

	CG.shift[ CG.curve ][ X ] = cv->shift[ X ] = - d_min( x1, x2 );
	CG.s2d[ CG.curve ][ X ] = cv->s2d[ X ] =
		                   ( double ) ( G.cut_canvas.w - 1 ) / fabs( x1 - x2 );
	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool cut_change_y_range( Canvas *c )
{
	Curve_1d *cv = &G.cut_curve;
	double cy1, cy2;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 )
		return UNSET;

	cut_save_scale_state( );

	cy1 = ( ( double ) G.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
		  - cv->shift[ Y ];
	cy2 = ( ( double ) G.cut_canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
		  - cv->shift[ Y ];

	CG.shift[ G.active_curve ][ Y ] = cv->shift[ Y ] = - d_min( cy1, cy2 );
	CG.s2d[ G.active_curve ][ Y ] = cv->s2d[ Y ] =
		                                      ( double ) ( G.cut_canvas.h - 1 )
		                                      / fabs( cy1 - cy2 );
	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool cut_change_xy_range( Canvas *c )
{
	bool scale_changed = UNSET;
	Curve_1d *cv = &G.cut_curve;
	double cx1, cx2, cy1, cy2;


	cut_save_scale_state( );
	CG.can_undo[ G.active_curve ] = UNSET;

	if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
	{
		CG.can_undo[ G.active_curve ] = SET;

		cx1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
		cx2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

		CG.shift[ G.active_curve ][ X ] = cv->shift[ X ] = - d_min( cx1, cx2 );
		CG.s2d[ G.active_curve ][ X ] = cv->s2d[ X ] =
			                                  ( double ) ( G.cut_canvas.w - 1 )
			                                  / fabs( cx1 - cx2 );

		scale_changed = SET;
	}

	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
	{
		CG.can_undo[ G.active_curve ] = SET;

		cy1 = ( ( double ) G.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
			  - cv->shift[ Y ];
		cy2 = ( ( double ) G.cut_canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
			  - cv->shift[ Y ];

		CG.shift[ G.active_curve ][ Y ] = cv->shift[ Y ] = - d_min( cy1, cy2 );
		CG.s2d[ G.active_curve ][ Y ] = cv->s2d[ Y ] =
			                                  ( double ) ( G.cut_canvas.h - 1 )
			                                  / fabs( cy1 - cy2 );

		scale_changed = SET;
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool cut_zoom_x( Canvas *c )
{
	Curve_1d *cv = &G.cut_curve;
	double px;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 )
		return UNSET;

	cut_save_scale_state( );

	px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

	/* If the mouse was moved to lower values zoom the display by a factor
	   of up to 4 (if the mouse was moved over the whole length of the
	   scale) while keeping the point the move was started at the same
	   position. If the mouse was moved upwards demagnify by the inverse
	   factor. */

	if ( G.start[ X ] > c->ppos[ X ] )
		cv->s2d[ X ] *= d_min( 4.0,
					    1.0 + 3.0 * ( double ) ( G.start[ X ] - c->ppos[ X ] )
						/ ( double ) G.cut_x_axis.w );
	else
		cv->s2d[ X ] /= d_min( 4.0,
					    1.0 + 3.0 * ( double ) ( c->ppos[ X ] - G.start[ X ] )
						/ ( double ) G.cut_x_axis.w );
	CG.s2d[ G.active_curve ][ X ] = cv->s2d[ X ];

	CG.shift[ G.active_curve ][ X ] = cv->shift[ X ] =
		                                      G.start[ X ] / cv->s2d[ X ] - px;

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool cut_zoom_y( Canvas *c )
{
	Curve_1d *cv = &G.cut_curve;
	double py;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 )
		return UNSET;

	cut_save_scale_state( );

	/* Get the value in the interval [0, 1] corresponding to the mouse
	   position */

	py = ( ( double ) G.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
		 - cv->shift[ Y ];
		
	/* If the mouse was moved to lower values zoom the display by a factor of
	   up to 4 (if the mouse was moved over the whole length of the scale)
	   while keeping the point the move was started at the same position. If
	   the mouse was moved upwards demagnify by the inverse factor. */

	if ( G.start[ Y ] < c->ppos[ Y ] )
		cv->s2d[ Y ] *= d_min( 4.0,
					    1.0 + 3.0 * ( double ) ( c->ppos[ Y ] - G.start[ Y ] )
						/ ( double ) G.cut_y_axis.h );
	else
		cv->s2d[ Y ] /= d_min( 4.0,
					    1.0 + 3.0 * ( double ) ( G.start[ Y ] - c->ppos[ Y ] )
						/ ( double ) G.cut_y_axis.h );
	CG.s2d[ G.active_curve ][ Y ] = cv->s2d[ Y ];

	CG.shift[ G.active_curve ][ Y ] = cv->shift[ Y ] =
		( ( double ) G.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ] - py;

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool cut_zoom_xy( Canvas *c )
{
	Curve_1d *cv = &G.cut_curve;
	double px, py;
	bool scale_changed = UNSET;


	cut_save_scale_state( );
	CG.can_undo[ G.active_curve ] = UNSET;

	if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
	{
		CG.can_undo[ G.active_curve ] = SET;

		px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

		if ( G.start[ X ] > c->ppos[ X ] )
			cv->s2d[ X ] *= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( G.start[ X ] - c->ppos[ X ] )
								   / ( double ) G.cut_x_axis.w );
		else
			cv->s2d[ X ] /= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( c->ppos[ X ] - G.start[ X ] )
								   / ( double ) G.cut_x_axis.w );
		CG.s2d[ G.active_curve ][ X ] = cv->s2d[ X ];

		CG.shift[ G.active_curve ][ X ] = cv->shift[ X ] =
			                                  G.start[ X ] / cv->s2d[ X ] - px;

		scale_changed = SET;
	}

	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
	{
		CG.can_undo[ G.active_curve ] = SET;

		py = ( ( double ) G.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
			 - cv->shift[ Y ];

		if ( G.start[ Y ] < c->ppos[ Y ] )
			cv->s2d[ Y ] *= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( c->ppos[ Y ] - G.start[ Y ] )
								   / ( double ) G.cut_y_axis.h );
		else
			cv->s2d[ Y ] /= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( G.start[ Y ] - c->ppos[ Y ] )
								   / ( double ) G.cut_y_axis.h );
		CG.s2d[ G.active_curve ][ Y ] = cv->s2d[ Y ];

		CG.shift[ G.active_curve ][ Y ] = cv->shift[ Y ] =
			                 ( ( double ) G.cut_canvas.h - 1.0 - G.start[ Y ] )
			                 / cv->s2d[ Y ] - py;

		scale_changed = SET;
	}
		
	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void cut_save_scale_state( void )
{
	int i;
	Curve_1d *cv = &G.cut_curve;


	for ( i = X; i <= Y; i++ )
	{
		cv->old_s2d[ i ] = cv->s2d[ i ];
		cv->old_shift[ i ] = cv->shift[ i ];
	}

	CG.can_undo[ G.active_curve ] = SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_clear_curve( long curve )
{
	long i;
	Scaled_Point *sp;


	if ( ! G.is_cut || curve != G.active_curve )
		return;

	for ( sp = G.cut_curve.points, i = 0; i < CG.nx; sp++, i++ )
		sp->exist = UNSET;
	G.cut_curve.count = 0;

	G.cut_curve.points = T_free( G.cut_curve.points );
	G.cut_curve.xpoints = T_free( G.cut_curve.xpoints );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_next_index( FL_OBJECT *a, long b )
{
	a = a;


	switch( b )
	{
		case 0 :           /* go one curve up */
			cut_show( CG.cut_dir, CG.index + 1 );
			break;

		case 1 :           /* go one curve down */
			cut_show( CG.cut_dir, CG.index - 1 );
			break;

		case 2:            /* go to top curve */
			cut_show( CG.cut_dir, CG.cut_dir == X ? G.nx - 1 : G.ny - 1 );
			break;

		case 3 :           /* go to bottom curve */
			cut_show( CG.cut_dir, 0 );
			break;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_change_dir( FL_OBJECT *a, long b )
{
	Curve_1d *cv = &G.cut_curve;
	Curve_2d *scv = G.curve_2d[ G.active_curve ];
	long p_index;
	unsigned int keymask;
	int px, py;



	a = a;
	b = b;

	if ( G.raw_button_state != 3 )
		return;

	fl_get_win_mouse( cut_form->cut->window, &px, &py, &keymask );
	px -= cut_form->cut_canvas->x;

	if ( px < 0 || px > cut_form->cut_canvas->w )
		return;

	p_index = lrnd( px / cv->s2d[ X ] - cv->shift[ X ] );

	if ( CG.cut_dir == X )
	{
		if ( p_index >= G.ny )
			p_index = G.ny - 1;
		if ( p_index < 0 )
			p_index = 0;
	}
	else
	{
		if ( p_index >= G.nx )
			p_index = G.nx - 1;
		if ( p_index < 0 )
			p_index = 0;
	}

	if ( ! CG.is_fs[ G.active_curve ] )
	{
		if ( CG.cut_dir == Y )
		{
			CG.s2d[ G.active_curve ][ X ] = cv->s2d[ X ] =
								 scv->s2d[ Y ] / ( double ) ( G.canvas.h - 1 ) 
								 * ( double ) ( G.cut_canvas.w - 1 );
			CG.shift[ G.active_curve ][ X ] = cv->shift[ X ] = scv->shift[ Y ];
		}
		else
		{
			CG.s2d[ G.active_curve ][ X ] = cv->s2d[ X ] =
					              scv->s2d[ X ] / ( double ) ( G.canvas.w - 1 )
							      * ( double ) ( G.cut_canvas.w - 1 );
			CG.shift[ G.active_curve ][ X ] = cv->shift[ X ] = scv->shift[ X ];
		}
	}

	cut_show( CG.cut_dir == X ? Y : X, p_index );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
