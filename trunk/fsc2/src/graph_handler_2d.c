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

static void press_handler_2d( FL_OBJECT *obj, Window window, XEvent *ev,
							  Canvas *c );
static void release_handler_2d( FL_OBJECT *obj, Window window, XEvent *ev,
								Canvas *c );
static void motion_handler_2d( FL_OBJECT *obj, Window window, XEvent *ev,
							   Canvas *c );
static bool change_x_range_2d( Canvas *c );
static bool change_y_range_2d( Canvas *c );
static bool change_xy_range_2d( Canvas *c );
static bool change_z_range_2d( Canvas *c );
static bool zoom_x_2d( Canvas *c );
static bool zoom_y_2d( Canvas *c );
static bool zoom_xy_2d( Canvas *c );
static bool zoom_z_2d( Canvas *c );
static bool shift_XPoints_of_curve_2d( Canvas *c, Curve_2d *cv );
static void reconfigure_window_2d( Canvas *c, int w, int h );
static void recalc_XPoints_2d( void );
static void make_color_scale( Canvas *c, Curve_2d *cv );
static inline unsigned long d2color( double a );
static void delete_marker_2d( long x_pos, long y_pos, long curve );


/*-----------------------------------------------------------*/
/* Handler for all kinds of X events the canvas may receive. */
/*-----------------------------------------------------------*/

int canvas_handler_2d( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					   void *udata )
{
	Canvas *c = ( Canvas * ) udata;


	switch ( ev->type )
    {
        case Expose :
            if ( ev->xexpose.count == 0 )     /* only react to last in queue */
				repaint_canvas_2d( c );
            break;

		case ConfigureNotify :
            if ( ( int ) c->w == w && ( int ) c->h == h )
				break;
            reconfigure_window_2d( c, w, h );
            break;

		case ButtonPress :
			press_handler_2d( obj, window, ev, c );
			break;

		case ButtonRelease :
			release_handler_2d( obj, window, ev, c );
			break;

		case MotionNotify :
			motion_handler_2d( obj, window, ev, c );
			break;
	}

	return 1;
}


/*-------------------------------------------------------------*/
/* Handler for events due to pressing one of the mouse buttons */
/*-------------------------------------------------------------*/

static void press_handler_2d( FL_OBJECT *obj, Window window, XEvent *ev,
							  Canvas *c )
{
	long i;
	Curve_2d *cv;
	int old_button_state = G.button_state;
	unsigned int keymask;


	/* In the axes areas two buttons pressed simultaneously doesn't have a
	   special meaning, so don't care about another button. Also don't react
	   if the pressed buttons have lost there meaning, there's no curve or
	   the curve has no scaling set yet */

	if ( ( c != &G2.canvas && G.raw_button_state != 0 ) ||
		 ( G.button_state == 0 && G.raw_button_state != 0 ) ||
		 G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
	{
		G.raw_button_state |= 1 << ( ev->xbutton.button - 1 );
		return;
	}

	G.raw_button_state |= ( 1 << ( ev->xbutton.button - 1 ) );

	/* Middle and right or all three buttons at once don't mean a thing */

	if ( G.raw_button_state >= 6 &&
		 G.raw_button_state != 8 && G.raw_button_state != 16 )
		return;

	G.button_state |= ( 1 << ( ev->xbutton.button - 1 ) );

	/* Find out which window gets the mouse events (all following mouse events
	   go to this window until all buttons are released) */

	if ( obj == GUI.run_form_2d->x_axis_2d )        /* in x-axis window */
		G.drag_canvas = DRAG_2D_X;
	if ( obj == GUI.run_form_2d->y_axis_2d )        /* in y-axis window */
		G.drag_canvas = DRAG_2D_Y;
	if ( obj == GUI.run_form_2d->z_axis_2d )        /* in z-axis window */
		G.drag_canvas = DRAG_2D_Z;
	if ( obj == GUI.run_form_2d->canvas_2d )        /* in canvas window */
		G.drag_canvas = DRAG_2D_C;

	fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

	switch ( G.button_state )
	{
		case 1 :                               /* left button */
			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			/* Set up variables for drawing the rubber boxes */

			switch ( G.drag_canvas )
			{
				case DRAG_2D_X :                       /* in x-axis window */
					if ( keymask & ShiftMask )
					{
						fl_set_cursor( window, G2.cursor[ ARROW_UP_CURSOR ] );
						G2.cut_select = CUT_SELECT_X;
					}
					else
						fl_set_cursor( window, G2.cursor[ ZOOM_BOX_CURSOR ] );

					c->box_x = c->ppos[ X ];
					c->box_w = 0;
					c->box_y = G.x_scale_offset + 1;
					c->box_h = G.enlarge_box_width;
					c->is_box = SET;
					break;

				case DRAG_2D_Y :                       /* in y-axis window */
					if ( keymask & ShiftMask )
					{
						fl_set_cursor( window,
									   G2.cursor[ ARROW_RIGHT_CURSOR ] );
						G2.cut_select = CUT_SELECT_Y;
					}
					else
						fl_set_cursor( window, G2.cursor[ ZOOM_BOX_CURSOR ] );

					c->box_x = c->w
						      - ( G.y_scale_offset + G.enlarge_box_width + 1 );
					c->box_y = c->ppos[ Y ];
					c->box_w = G.enlarge_box_width;
					c->box_h = 0;
					c->is_box = SET;
					break;

				case DRAG_2D_Z :                       /* in z-axis window */
					fl_set_cursor( window, G2.cursor[ ZOOM_BOX_CURSOR ] );

					c->box_x = G.z_scale_offset + 1;
					c->box_y = c->ppos[ Y ];
					c->box_w = G.enlarge_box_width;
					c->box_h = 0;
					c->is_box = SET;
					break;

				case DRAG_2D_C :                       /* in canvas window */
					fl_set_cursor( window, G2.cursor[ ZOOM_BOX_CURSOR ] );

					c->box_x = c->ppos[ X ];
					c->box_y = c->ppos[ Y ];
					c->box_w = c->box_h = 0;
					c->is_box = SET;
					break;
			}

			repaint_canvas_2d( c );
			break;

		case 2 :                               /* middle button */
			fl_set_cursor( window, G2.cursor[ MOVE_HAND_CURSOR ] );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			/* Store data for undo operation */

			for ( i = 0; i < G2.nc; i++ )
			{
				cv = G2.curve_2d[ i ];

				if ( cv->active )
					save_scale_state_2d( cv );
				else
					cv->can_undo = UNSET;
			}
			break;

		case 3:                                /* left and middle button */
			fl_set_cursor( window, G2.cursor[ CROSSHAIR_CURSOR ] );

			/* Don't draw the box anymore */

			G2.canvas.is_box = UNSET;
			repaint_canvas_2d( &G2.canvas );
			break;

		case 4 :                               /* right button */
			fl_set_cursor( window, G2.cursor[ ZOOM_LENS_CURSOR ] );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];
			break;

		case 5 :                               /* left and right button */
			fl_set_cursor( window, G2.cursor[ TARGET_CURSOR ] );

			if ( G2.canvas.is_box == UNSET && old_button_state != 4 )
			{
				G.start[ X ] = c->ppos[ X ];
				G.start[ Y ] = c->ppos[ Y ];
			}
			else
				G2.canvas.is_box = UNSET;

			repaint_canvas_2d( &G2.canvas );
			break;

		case 8 : case 16 :                     /* button 4/5/wheel */
			if ( G.drag_canvas < DRAG_2D_Z || G.drag_canvas >= DRAG_2D_C )
				break;

			fl_set_cursor( window, G2.cursor[ MOVE_HAND_CURSOR ] );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			/* Store data for undo operation */

			for ( i = 0; i < G2.nc; i++ )
			{
				cv = G2.curve_2d[ i ];

				if ( cv->active )
					save_scale_state_2d( cv );
				else
					cv->can_undo = UNSET;
			}
			break;
	}
}


/*---------------------------------------------------------------*/
/* Handler for events due to releasing one of the mouse buttons. */
/*---------------------------------------------------------------*/

static void release_handler_2d( FL_OBJECT *obj, Window window, XEvent *ev,
								Canvas *c )
{
	unsigned int keymask;
	bool scale_changed = UNSET;


	UNUSED_ARGUMENT( obj );

	/* If the released button didn't has a meaning just clear it from the
	   button state pattern and then forget about it */

	if ( ! ( ( 1 << ( ev->xbutton.button - 1 ) ) & G.button_state ) ||
		 G2.active_curve == -1 )
	{
		G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );
		return;
	}

	/* Get mouse position and restrict it to the canvas */

	fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

	if ( c->ppos[ X ] < 0 )
		c->ppos[ X ] = 0;
	if ( c->ppos[ X ] >= ( int ) G2.canvas.w )
		c->ppos[ X ] = G2.canvas.w - 1;

	if ( c->ppos[ Y ] < 0 )
		c->ppos[ Y ] = 0;
	if ( c != &G2.z_axis )
	{
		if ( c->ppos[ Y ] >= ( int ) G2.canvas.h )
			c->ppos[ Y ] = G2.canvas.h - 1;
	}
	else if ( c->ppos[ Y ] >= ( int ) c->h )         /* in z-axis window */
		c->ppos[ Y ] = c->h - 1;

	switch ( G.button_state )
	{
		case 1 :                               /* left mouse button */
			switch ( G.drag_canvas )
			{
				case DRAG_2D_X :                       /* x-axis window */
					if ( G2.cut_select == NO_CUT_SELECT )
						scale_changed = change_x_range_2d( c );
					else if ( G2.cut_select == CUT_SELECT_X &&
							  keymask & ShiftMask )
						cut_show( X, lrnd(
							( c->box_x + c->box_w )
							/ G2.curve_2d[ G2.active_curve ]->s2d[ X ]
							- G2.curve_2d[ G2.active_curve ]->shift[ X ] ) );
					break;

				case DRAG_2D_Y :                       /* in y-axis window */
					if ( G2.cut_select == NO_CUT_SELECT )
						scale_changed = change_y_range_2d( c );
					else if ( G2.cut_select == CUT_SELECT_Y &&
							  keymask & ShiftMask )
						cut_show( Y, lrnd(
							   ( G2.y_axis.h - 1.0 - ( c->box_y + c->box_h ) )
							   / G2.curve_2d[ G2.active_curve]->s2d[ Y ]
						       - G2.curve_2d[ G2.active_curve]->shift[ Y ] ) );
					break;

				case DRAG_2D_Z :                       /* in z-axis window */
					scale_changed = change_z_range_2d( c );
					break;

				case DRAG_2D_C :                       /* in canvas window */
					scale_changed = change_xy_range_2d( c );
					break;
			}
			c->is_box = UNSET;
			break;

		case 2 :                               /* middle mouse button */
			if ( G.drag_canvas < DRAG_2D_Z || G.drag_canvas > DRAG_2D_C )
				break;

			if ( G.drag_canvas & DRAG_2D_X & 3 )
				redraw_canvas_2d( &G2.x_axis );
			if ( G.drag_canvas & DRAG_2D_Y & 3 )
				redraw_canvas_2d( &G2.y_axis );
			if ( G.drag_canvas & DRAG_2D_Z & 3 )
				redraw_canvas_2d( &G2.z_axis );
			break;

		case 4 :                               /* right mouse button */
			switch ( G.drag_canvas )
			{
				case DRAG_2D_X :                       /* in x-axis window */
					scale_changed = zoom_x_2d( c );
					break;

				case DRAG_2D_Y :                       /* in y-axis window */
					scale_changed = zoom_y_2d( c );
					break;

				case DRAG_2D_Z :                       /* in z-axis window */
					scale_changed = zoom_z_2d( c );
					break;

				case DRAG_2D_C :                       /* in canvas window */
					scale_changed = zoom_xy_2d( c );
					break;
			}
			break;

		case 8 :                               /* button 4/wheel up */
			if ( G.drag_canvas == DRAG_2D_X )
			{
				G2.x_axis.ppos[ X ] = G.start[ X ] - G2.x_axis.w / 10;

				if ( G2.active_curve != -1 &&
					 G2.curve_2d[ G2.active_curve ]->is_scale_set )
					scale_changed = shift_XPoints_of_curve_2d( c,
											  G2.curve_2d[ G2.active_curve ] );
				
				redraw_canvas_2d( &G2.canvas );
				redraw_canvas_2d( &G2.x_axis );
			}

			if ( G.drag_canvas == DRAG_2D_Y )
			{
				G2.y_axis.ppos[ Y ] = G.start[ Y ] + G2.y_axis.h / 10;

				if ( G2.active_curve != -1 &&
					 G2.curve_2d[ G2.active_curve ]->is_scale_set )
					scale_changed = shift_XPoints_of_curve_2d( c,
											  G2.curve_2d[ G2.active_curve ] );
				
				redraw_canvas_2d( &G2.canvas );
				redraw_canvas_2d( &G2.y_axis );
			}

			if ( G.drag_canvas == DRAG_2D_Z )
			{
				G2.z_axis.ppos[ Y ] = G.start[ Y ] + G2.z_axis.h / 10;

				if ( G2.active_curve != -1 &&
					 G2.curve_2d[ G2.active_curve ]->is_scale_set )
					scale_changed = shift_XPoints_of_curve_2d( c,
											  G2.curve_2d[ G2.active_curve ] );
				
				redraw_canvas_2d( &G2.canvas );
				redraw_canvas_2d( &G2.z_axis );
			}
			break;

		case 16 :                              /* button 5/wheel down */
			if ( G.drag_canvas == DRAG_2D_X )
			{
				G2.x_axis.ppos[ X ] = G.start[ X ] + G2.x_axis.w / 10;

				if ( G2.active_curve != -1 &&
					 G2.curve_2d[ G2.active_curve ]->is_scale_set )
					scale_changed = shift_XPoints_of_curve_2d( c,
											  G2.curve_2d[ G2.active_curve ] );
				
				redraw_canvas_2d( &G2.canvas );
				redraw_canvas_2d( &G2.x_axis );
			}

			if ( G.drag_canvas == DRAG_2D_Y )
			{
				G2.y_axis.ppos[ Y ] = G.start[ Y ] - G2.y_axis.h / 10;

				if ( G2.active_curve != -1 &&
					 G2.curve_2d[ G2.active_curve ]->is_scale_set )
					scale_changed = shift_XPoints_of_curve_2d( c,
											  G2.curve_2d[ G2.active_curve ] );
				
				redraw_canvas_2d( &G2.canvas );
				redraw_canvas_2d( &G2.y_axis );
			}

			if ( G.drag_canvas == DRAG_2D_Z )
			{
				G2.z_axis.ppos[ Y ] = G.start[ Y ] - G2.z_axis.h / 10;

				if ( G2.active_curve != -1 &&
					 G2.curve_2d[ G2.active_curve ]->is_scale_set )
					scale_changed = shift_XPoints_of_curve_2d( c,
											  G2.curve_2d[ G2.active_curve ] );
				
				redraw_canvas_2d( &G2.canvas );
				redraw_canvas_2d( &G2.z_axis );
			}
			break;
	}

	G.button_state = 0;
	G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );
	G.drag_canvas = DRAG_NONE;

	G2.cut_select = NO_CUT_SELECT;
	fl_reset_cursor( window );

	if ( scale_changed )
	{
		if ( G2.curve_2d[ G2.active_curve ]->is_fs )
		{
			G2.curve_2d[ G2.active_curve ]->is_fs = UNSET;
			fl_set_button( GUI.run_form_2d->full_scale_button_2d, 0 );
			if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
				fl_set_object_helper( GUI.run_form_2d->full_scale_button_2d,
									  "Rescale curves to fit into the window\n"
									  "and switch on automatic rescaling" );
		}

		redraw_all_2d( );
	}

	if ( ! scale_changed || c != &G2.canvas )
		repaint_canvas_2d( c );
}


/*---------------------------------------------------*/
/* Handler for events due to movements of the mouse. */
/*---------------------------------------------------*/

static void motion_handler_2d( FL_OBJECT *obj, Window window, XEvent *ev,
							   Canvas *c )
{
	XEvent new_ev;
	unsigned int keymask;
	bool scale_changed = UNSET;


	UNUSED_ARGUMENT( obj );

	if ( G2.active_curve == -1 )
		return;

	/* Do event compression to avoid being flooded with motion events -
	   instead of handling them all individually only react to the latest
	   in a series of motion events for the current window */

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

			/* If we were in cut select mode and the shift button has
			   become released get out of this mode */

			if ( ( G2.cut_select == CUT_SELECT_X ||
				   G2.cut_select == CUT_SELECT_Y ) &&
				 ! ( keymask & ShiftMask ) )
			{
				G2.cut_select = CUT_SELECT_BREAK;
				fl_reset_cursor( window );
			}

			if ( G.drag_canvas >= DRAG_2D_Z && G.drag_canvas <= DRAG_2D_C )
			{
				if ( G.drag_canvas & DRAG_2D_X & 3 )     /* x-axis or canvas */
				{
					c->box_w = c->ppos[ X ] - G.start[ X ];

					if ( c->box_x + c->box_w >= ( int ) c->w )
						c->box_w = c->w - c->box_x - 1;

					if ( c->box_x + c->box_w < 0 )
						c->box_w = - c->box_x;
				}

				if ( G.drag_canvas & DRAG_2D_Y & 3 )     /* y-axis or canvas */
				{
					c->box_h = c->ppos[ Y ] - G.start[ Y ] ;

					if ( c->box_y + c->box_h >= ( int ) c->h )
						c->box_h = c->h - c->box_y - 1;

					if ( c->box_y + c->box_h < 0 )
						c->box_h = - c->box_y;
				}

				if ( G.drag_canvas == DRAG_2D_Z ) /* z-axis window */
				{
					c->box_h = c->ppos[ Y ] - G.start[ Y ] ;

					if ( c->box_y + c->box_h >= ( int ) c->h )
						c->box_h = c->h - c->box_y - 1;

					if ( c->box_y + c->box_h < 0 )
						c->box_h = - c->box_y;
				}
			}

			repaint_canvas_2d( c );
			break;

		case 2 :                               /* middle button */
			if ( G2.active_curve != -1 &&
				 G2.curve_2d[ G2.active_curve ]->is_scale_set )
				scale_changed = shift_XPoints_of_curve_2d( c,
											  G2.curve_2d[ G2.active_curve ] );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			if ( G2.curve_2d[ G2.active_curve ]->is_fs && scale_changed )
			{
				G2.curve_2d[ G2.active_curve ]->is_fs = UNSET;
				fl_set_button( GUI.run_form_2d->full_scale_button_2d, 0 );
				if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
					fl_set_object_helper(
									  GUI.run_form_2d->full_scale_button_2d,
									  "Rescale curves to fit into the window\n"
									  "and switch on automatic rescaling" );
			}

			redraw_canvas_2d( &G2.canvas );

			if ( G.drag_canvas < DRAG_2D_Z || G.drag_canvas > DRAG_2D_C )
				break;

			if ( G.drag_canvas & DRAG_2D_X & 3 )
				redraw_canvas_2d( &G2.x_axis );
			if ( G.drag_canvas & DRAG_2D_Y & 3 )
				redraw_canvas_2d( &G2.y_axis );
			if ( G.drag_canvas == DRAG_2D_Z )
				redraw_canvas_2d( &G2.z_axis );
			break;

		case 3 : case 5 :               /* left and (middle or right) button */
			repaint_canvas_2d( &G2.canvas );
			break;
	}
}


/*---------------------------------------------------------------*/
/* Stores the current scaling state of the currently shown curve */
/*---------------------------------------------------------------*/

void save_scale_state_2d( Curve_2d *cv )
{
	int i;


	for ( i = X; i <= Z; i++ )
	{
		cv->old_s2d[ i ] = cv->s2d[ i ];
		cv->old_shift[ i ] = cv->shift[ i ];
	}

	cv->old_z_factor = cv->z_factor;

	cv->can_undo = SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool change_x_range_2d( Canvas *c )
{
	Curve_2d *cv;
	double x1, x2;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 || G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
		return UNSET;

	cv = G2.curve_2d[ G2.active_curve ];

	x1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
	x2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

	save_scale_state_2d( cv );

	cv->s2d[ X ] = ( G2.canvas.w - 1 ) / fabs( x1 - x2 );
	cv->shift[ X ] = - d_min( x1, x2 );

	recalc_XPoints_of_curve_2d( cv );

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool change_y_range_2d( Canvas *c )
{
	Curve_2d *cv;
	double cy1, cy2;
	double new_s2d_y;

	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 || G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
		return UNSET;

	cv = G2.curve_2d[ G2.active_curve ];

	cy1 = ( G2.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ] - cv->shift[ Y ];
	cy2 = ( G2.canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ] - cv->shift[ Y ];

	new_s2d_y = ( G2.canvas.h - 1 ) / fabs( cy1 - cy2 );

	/* Keep the coordinates from getting out of the range of short int- X
	   only can deal with short ints */

	if ( new_s2d_y >= SHRT_MAX_HALF || new_s2d_y <= SHRT_MIN_HALF )
		return UNSET;

	save_scale_state_2d( cv );

	cv->s2d[ Y ] = new_s2d_y;
	cv->shift[ Y ] = - d_min( cy1, cy2 );

	recalc_XPoints_of_curve_2d( cv );

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool change_xy_range_2d( Canvas *c )
{
	bool scale_changed = UNSET;
	Curve_2d *cv;
	double cx1, cx2, cy1, cy2;


	if ( G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
		return UNSET;

	cv = G2.curve_2d[ G2.active_curve ];

	save_scale_state_2d( cv );
	cv->can_undo = UNSET;

	if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
	{
		cx1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
		cx2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

		cv->can_undo = SET;

		cv->s2d[ X ] = ( G2.canvas.w - 1 ) / fabs( cx1 - cx2 );
		cv->shift[ X ] = - d_min( cx1, cx2 );

		scale_changed = SET;
	}

	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
	{
		cy1 = ( G2.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
			  - cv->shift[ Y ];
		cy2 = ( G2.canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
			  - cv->shift[ Y ];

		cv->can_undo = SET;

		cv->s2d[ Y ] = ( G2.canvas.h - 1 ) / fabs( cy1 - cy2 );
		cv->shift[ Y ] = - d_min( cy1, cy2 );

		scale_changed = SET;
	}

	if ( scale_changed )
		recalc_XPoints_of_curve_2d( cv );

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool change_z_range_2d( Canvas *c )
{
	Curve_2d *cv;
	double z1, z2;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 || G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
		return UNSET;

	cv = G2.curve_2d[ G2.active_curve ];

	save_scale_state_2d( cv );

	z1 = ( G2.z_axis.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Z ] - cv->shift[ Z ];
	z2 = ( G2.z_axis.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Z ] - cv->shift[ Z ];

	cv->shift[ Z ] = - d_min( z1, z2 );
	cv->z_factor = 1.0 / fabs( z1 - z2 );
	cv->s2d[ Z ] = ( G2.z_axis.h - 1 ) * cv->z_factor;

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool zoom_x_2d( Canvas *c )
{
	Curve_2d *cv;
	double px;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 || G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
		return UNSET;

	cv = G2.curve_2d[ G2.active_curve ];

	save_scale_state_2d( cv );

	px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

	/* If the mouse was moved to lower values zoom the display by a factor
	   of up to 4 (if the mouse was moved over the whole length of the
	   scale) while keeping the point the move was started at the same
	   position. If the mouse was moved upwards demagnify by the inverse
	   factor. */

	if ( G.start[ X ] > c->ppos[ X ] )
		cv->s2d[ X ] *= 3 * d_min( 1.0, 
								   ( double ) ( G.start[ X ] - c->ppos[ X ] )
								   / G2.x_axis.w ) + 1;
	else
		cv->s2d[ X ] /= 3 * d_min( 1.0, 
								   ( double ) ( c->ppos[ X ] - G.start[ X ] )
								   / G2.x_axis.w ) + 1;

	cv->shift[ X ] = G.start[ X ] / cv->s2d[ X ] - px;

	recalc_XPoints_of_curve_2d( cv );

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool zoom_y_2d( Canvas *c )
{
	Curve_2d *cv;
	double py;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 || G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
		return UNSET;

	cv = G2.curve_2d[ G2.active_curve ];

	save_scale_state_2d( cv );

	/* Get the value in the interval [0, 1] corresponding to the mouse
	   position */

	py = ( G2.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ] - cv->shift[ Y ];

	/* If the mouse was moved to lower values zoom the display by a factor
	   of up to 4 (if the mouse was moved over the whole length of the
	   scale) while keeping the point the move was started at the same
	   position. If the mouse was moved upwards demagnify by the inverse
	   factor. */

	if ( G.start[ Y ] < c->ppos[ Y ] )
		cv->s2d[ Y ] *= 3 * d_min( 1.0,
								   ( double )  ( c->ppos[ Y ] - G.start[ Y ] )
								   / G2.y_axis.h ) + 1;
	else
		cv->s2d[ Y ] /= 3 * d_min( 1.0,
								   ( double ) ( G.start[ Y ] - c->ppos[ Y ] )
								   / G2.y_axis.h ) + 1;

	cv->shift[ Y ] = ( G2.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ] - py;

	recalc_XPoints_of_curve_2d( cv );

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool zoom_xy_2d( Canvas *c )
{
	bool scale_changed = UNSET;
	Curve_2d *cv;
	double px, py;
	double factor;


	if ( G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
		return UNSET;

	cv = G2.curve_2d[ G2.active_curve ];

	save_scale_state_2d( cv );
	cv->can_undo = UNSET;

	if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
	{
		cv->can_undo = SET;

		px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

		if ( G.start[ X ] > c->ppos[ X ] )
			cv->s2d[ X ] *= 3 * d_min( 1.0,
									 ( double ) ( G.start[ X ] - c->ppos[ X ] )
									 / G2.x_axis.w ) + 1;
		else
			cv->s2d[ X ] /= 3 * d_min( 1.0,
									 ( double ) ( c->ppos[ X ] - G.start[ X ] )
									 / G2.x_axis.w ) + 1;

		cv->shift[ X ] = G.start[ X ] / cv->s2d[ X ] - px;

		scale_changed = SET;
	}

	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
	{
		cv->can_undo = SET;

		py = ( G2.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
			 - cv->shift[ Y ];

		factor = 3 * d_min( 1.0, ( double ) ( c->ppos[ Y ] - G.start[ Y ] )
								 / G2.y_axis.h ) + 1;
		if ( G.start[ Y ] < c->ppos[ Y ] )
			cv->s2d[ Y ] *= factor;
		else
			cv->s2d[ Y ] /= factor;

		cv->shift[ Y ] = ( G2.canvas.h - 1.0 - G.start[ Y ] )
						 / cv->s2d[ Y ] - py;

			scale_changed = SET;
	}

	if ( scale_changed )
		recalc_XPoints_of_curve_2d( cv );

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static bool zoom_z_2d( Canvas *c )
{
	Curve_2d *cv;
	double pz;
	double factor;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 || G2.active_curve == -1 ||
		 ! G2.curve_2d[ G2.active_curve ]->is_scale_set )
		return UNSET;

	cv = G2.curve_2d[ G2.active_curve ];

	save_scale_state_2d( cv );

	/* Get the value in the interval [0, 1] corresponding to the mouse
	   position */

	pz = ( G2.z_axis.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Z ] - cv->shift[ Z ];

	/* If the mouse was moved to lower values zoom the display by a factor
	   of up to 4 (if the mouse was moved over the whole length of the
	   scale) while keeping the point the move was started at the same
	   position. If the mouse was moved upwards demagnify by the inverse
	   factor. */

	factor = 3 * d_min( 1.0, fabs( c->ppos[ Y ] - G.start[ Y ] )
							 / G2.z_axis.h ) + 1;

	if ( G.start[ Y ] < c->ppos[ Y ] )
	{
		cv->s2d[ Z ] *= factor;
		cv->z_factor *= factor;
	}
	else
	{
		cv->s2d[ Z ] /= factor;
		cv->z_factor /= factor;
	}

	cv->shift[ Z ] = ( G2.z_axis.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Z ] - pz;

	return SET;
}


/*-----------------------------------------------------------------------*/
/* This is basically a simplified version of 'recalc_XPoints_of_curve()' */
/* because we need to do much less calculations, i.e. just adding an     */
/* offset to all XPoints instead of going through all the scalings...    */
/*-----------------------------------------------------------------------*/

static bool shift_XPoints_of_curve_2d( Canvas *c, Curve_2d *cv )
{
	long i, count;
	int dx = 0,
		dy = 0,
		dz;
	int factor;
	Scaled_Point *sp = cv->points;
	XPoint *xp = cv->xpoints,
		   *xps = cv->xpoints_s;


	/* Additionally pressing the right mouse button increases the amount the
	   curves are shifted by a factor of 5 */

	factor = G.raw_button_state == 6 ? 5 : 1;

	/* Calculate scaled shift factors */

	if ( G.drag_canvas >= DRAG_2D_Z && G.drag_canvas <= DRAG_2D_C )
	{
		if ( G.drag_canvas & DRAG_2D_X & 3 )      /* x-axis or canvas window */
		{
			dx = factor * ( c->ppos[ X ] - G.start[ X ] );
			cv->shift[ X ] += dx / cv->s2d[ X ];
		}

		if ( G.drag_canvas & DRAG_2D_Y & 3 )      /* y-axis or canvas window */
		{
			dy = factor * ( c->ppos[ Y ] - G.start[ Y ] );
			cv->shift[ Y ] -= dy / cv->s2d[ Y ];
		}

		if ( G.drag_canvas == DRAG_2D_Z )         /* z-axis window */
		{
			dz = factor * ( c->ppos[ Y ] - G.start[ Y ] );
			cv->shift[ Z ] -= dz / cv->s2d[ Z ];
			return SET;
		}
	}

	/* Add the shifts to the XPoints */

	cv->up = cv->down = cv->left = cv->right = ( cv->count != 0 );

	for ( i = 0, count = cv->count;
		  i < G2.nx * G2.ny && count != 0; sp++, xp++, xps++, i++ )
		if ( sp->exist )
		{
			count--;

			xp->x = i2shrt( xp->x + dx );
			xp->y = i2shrt( xp->y + dy );
			xps->x = i2shrt( xp->x - ( cv->w >> 1 ) );
			xps->y = i2shrt( xp->y - ( cv->h >> 1 ) );

			cv->left &= ( xps->x + cv->w <= 0 );
			cv->right &= ( xps->x >= ( int ) G2.canvas.w );
			cv->up &= ( xps->y + cv->h <= 0 );
			cv->down &= ( xps->y >= ( int ) G2.canvas.h );
	}

	return SET;
}


/*-------------------------------------*/
/* Handles changes of the window size. */
/*-------------------------------------*/

static void reconfigure_window_2d( Canvas *c, int w, int h )
{
	long i;
	Curve_2d *cv;
	static bool is_reconf[ 3 ] = { UNSET, UNSET, UNSET };
	static bool need_redraw[ 3 ] = { UNSET, UNSET, UNSET };
	int old_w = c->w,
		old_h = c->h;


	/* Set the new canvas sizes */

	c->w = i2ushrt( w );
	c->h = i2ushrt( h );

	/* Calculate the new scale factors */

	if ( c == &G2.canvas )
	{
		for ( i = 0; i < G2.nc; i++ )
		{
			cv = G2.curve_2d[ i ];
			if ( ! cv->is_scale_set )
				continue;

			cv->s2d[ X ] *= ( w - 1.0 ) / ( old_w - 1 );
			cv->s2d[ Y ] *= ( h - 1.0 ) / ( old_h - 1 );

			if ( cv->can_undo )
			{
				cv->old_s2d[ X ] *= ( w - 1.0 ) / ( old_w - 1 );
				cv->old_s2d[ Y ] *= ( h - 1.0 ) / ( old_h - 1 );
			}
		}

		/* Recalculate data for drawing (has to be done after setting of
		   canvas sizes since they are needed in the recalculation) */

		recalc_XPoints_2d( );
	}

	/* We can't know the sequence the different canvases are reconfigured in
	   but, on the other hand, redrawing an axis canvas is useless before the
	   new scaling factors are set. Thus in the call for the redrawing the
	   canvas window we also need to redraw the axis windows which may have
	   gotten reconfigured before. */

	delete_pixmap( c );
	create_pixmap( c );

	if ( c == &G2.z_axis )
	{
		for ( i = 0; i < G2.nc; i++ )
		{
			cv = G2.curve_2d[ i ];
			if ( ! cv->is_scale_set )
				continue;

			cv->s2d[ Z ] *= ( h - 1.0 ) / ( old_h - 1 );

			if ( cv->can_undo )
				cv->old_s2d[ Z ] *= ( h - 1.0 ) / ( old_h - 1 );
		}
	}

	if ( c == &G2.canvas )
	{
		redraw_canvas_2d( c );

		if ( need_redraw[ X ] )
		{
			redraw_canvas_2d( &G2.x_axis );
			need_redraw[ X ] = UNSET;
		}
		else if ( w != old_w )
			is_reconf[ X ] = SET;

		if ( need_redraw[ Y ] )
		{
			redraw_canvas_2d( &G2.y_axis );
			need_redraw[ Y ] = UNSET;
		}
		else if ( h != old_h )
			is_reconf[ Y ] = SET;

		if ( need_redraw[ Z ] )
		{
			redraw_canvas_2d( &G2.z_axis );
			need_redraw[ Z ] = UNSET;
		}
		else if ( h != old_h )
			is_reconf[ Z ] = SET;
	}

	if ( c == &G2.x_axis )
	{
		if ( is_reconf[ X ] )
		{
			redraw_canvas_2d( c );
			is_reconf[ X ] = UNSET;
		}
		else
			need_redraw[ X ] = SET;
	}

	if ( c == &G2.y_axis )
	{
		if ( is_reconf[ Y ] )
		{
			redraw_canvas_2d( c );
			is_reconf[ Y ] = UNSET;
		}
		else
			need_redraw[ Y ] = SET;
	}

	if ( c == &G2.z_axis )
	{
		if ( is_reconf[ Z ] )
		{
			redraw_canvas_2d( c );
			is_reconf[ Z ] = UNSET;
		}
		else
			need_redraw[ Z ] = SET;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void recalc_XPoints_2d( void )
{
	long i;


	for ( i = 0; i < G2.nc; i++ )
		recalc_XPoints_of_curve_2d( G2.curve_2d[ i ] );
}


/*----------------------------------------------------------------*/
/* Recalculates the graphic data for a curve using the the curves */
/* settings for the scale and the offset.                         */
/*----------------------------------------------------------------*/

void recalc_XPoints_of_curve_2d( Curve_2d *cv )
{
	long i, j, count;
	Scaled_Point *sp;
	XPoint *xp  = cv->xpoints,
		   *xps = cv->xpoints_s;
	short dw, dh;


	cv->up = cv->down = cv->left = cv->right = ( cv->count != 0 );

	cv->w = d2ushrt( ceil( cv->s2d[ X ] ) );
	cv->h = d2ushrt( ceil( cv->s2d[ Y ] ) );
	dw = i2shrt( cv->w / 2 );
	dh = i2shrt( cv->h / 2 );

	for ( sp = cv->points, i = 0, count = cv->count;
		  i < G2.ny && count != 0; i++ )
		for ( j = 0; j < G2.nx && count != 0; sp++, xp++, xps++, j++ )
			if ( sp->exist )
			{
				count--;

				xp->x = d2shrt( cv->s2d[ X ] * ( j + cv->shift[ X ] ) );
				xp->y = i2shrt( G2.canvas.h ) - 1
				        - d2shrt( cv->s2d[ Y ] * ( i + cv->shift[ Y ] ) );
				xps->x = i2shrt( xp->x - dw );
				xps->y = i2shrt( xp->y - dh );

				cv->left  &= ( xps->x + cv->w <= 0 );
				cv->right &= ( xps->x >= ( int ) G2.canvas.w );
				cv->up    &= ( xps->y + cv->h <= 0 );
				cv->down  &= ( xps->y >= ( int ) G2.canvas.h );
			}
}


/*-----------------------------------------*/
/* Does a complete redraw of all canvases. */
/*-----------------------------------------*/

void redraw_all_2d( void )
{
	redraw_canvas_2d( &G2.canvas );
	redraw_canvas_2d( &G2.x_axis );
	redraw_canvas_2d( &G2.y_axis );
	redraw_canvas_2d( &G2.z_axis );
}


/*-------------------------------------*/
/* Does a complete redraw of a canvas. */
/*-------------------------------------*/

void redraw_canvas_2d( Canvas *c )
{
	long i, count;
	Curve_2d *cv;
	Scaled_Point *sp;
	XPoint *xps;
	Marker_2D *m;
	XPoint points[ 5 ];
	short int dw, dh;


	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

	if ( ! G.is_init )
	{
		repaint_canvas_2d( c );
		return;
	}

	if ( c == &G2.canvas )
	{
		/* First draw the active curve (unless the magnification isn't too
		   large) */

		if ( G2.active_curve != -1 &&
			 G2.curve_2d[ G2.active_curve ]->count > 1 &&
			 G2.curve_2d[ G2.active_curve ]->is_scale_set && 
			 G2.curve_2d[ G2.active_curve ]->w <= 2 * c->w &&
			 G2.curve_2d[ G2.active_curve ]->h <= 2 * c->h )
		{
			cv = G2.curve_2d[ G2.active_curve ];

			for ( sp = cv->points, xps = cv->xpoints_s, count = cv->count,
				  i = 0; i < G2.nx * G2.ny && count != 0; sp++, xps++, i++ )
			{
				if ( ! sp->exist )
					continue;

				count--;

				/* Don't draw if the rectangle isn't at least partially within
				   the canvas (this seems to be faster than X setting the
				   color and only then doing the clipping). */

				if ( xps->x > ( short int ) c->w ||
					 xps->y > ( short int ) c->h ||
					 - xps->x - 1 > cv->w || - xps->y - 1 > cv->h )
					continue;

				XSetForeground( G.d, cv->gc,
								d2color( cv->z_factor
										 * ( sp->v + cv->shift[ Z ] ) ) );
				XFillRectangle( G.d, c->pm, cv->gc,
								xps->x, xps->y, cv->w, cv->h );
			}

			/* Draw markers of the current curve */

			if ( cv->marker_2d != NULL )
			{
				dw = i2shrt( ( cv->w + 1 ) / 2 );
				dh = i2shrt( ( cv->h + 1 ) / 2 );
				if ( dw < 3 )
					dw = 3;
				if ( dh < 3 )
					dh = 3;

				points[ 1 ].x = 2 * dw;
				points[ 1 ].y = 0;

				points[ 2 ].x = 0;
				points[ 2 ].y = 2 * dh;

				points[ 3 ].x = - 2 * dw;
				points[ 3 ].y = 0;

				points[ 4 ].x = 0;
				points[ 4 ].y = - 2 * dh;

				for ( m = cv->marker_2d; m != NULL; m = m->next )
				{
					points[ 0 ].x =   d2shrt( cv->s2d[ X ] 
											  * ( m->x_pos + cv->shift[ X ] ) )
									- dw;
					if ( points[ 0 ].x + 2 * dw < 0 ||
						 points[ 0 ].x > ( short int ) c->w )
						 continue;

					points[ 0 ].y =   i2shrt( G2.canvas.h ) - 1
									- d2shrt( cv->s2d[ Y ] 
											  * ( m->y_pos + cv->shift[ Y ] ) )
									- dh;
					if ( points[ 1 ].y + 2 * dh < 0 ||
						 points[ 0 ].y > ( short int ) c->h )
						continue;

					XDrawLines( G.d, c->pm, m->gc, points, 5,
								CoordModePrevious );
				}
			}

			/* Now draw the out of range arrows */

			if ( cv->up )
				XCopyArea( G.d, cv->up_arrow, c->pm, c->gc, 0, 0,
						   G.up_arrow_w, G.up_arrow_h,
						   ( G2.canvas.w - G.up_arrow_w ) / 2, 5 );

			if ( cv->down )
				XCopyArea( G.d, cv->down_arrow, c->pm, c->gc, 0, 0,
						   G.down_arrow_w, G.down_arrow_h,
						   ( G2.canvas.w - G.down_arrow_w ) / 2,
						   G2.canvas.h - 5 - G.down_arrow_h );

			if ( cv->left )
				XCopyArea( G.d, cv->left_arrow, c->pm, c->gc, 0, 0,
						   G.left_arrow_w, G.left_arrow_h, 5,
						   ( G2.canvas.h - G.left_arrow_h ) / 2 );

			if ( cv->right )
				XCopyArea( G.d, cv->right_arrow, c->pm, c->gc, 0, 0,
						   G.right_arrow_w, G.right_arrow_h,
						   G2.canvas.w - 5 - G.right_arrow_w,
						   ( G2.canvas.h - G.right_arrow_h ) / 2 );
		}
	}

	if ( c == &G2.x_axis )
		redraw_axis_2d( X );

	if ( c == &G2.y_axis )
		redraw_axis_2d( Y );

	if ( c == &G2.z_axis )
		redraw_axis_2d( Z );

	/* Finally copy the pixmap onto the screen */

	repaint_canvas_2d( c );
}


/*-----------------------------------------------*/
/* Copies the background pixmap onto the canvas. */
/*-----------------------------------------------*/

void repaint_canvas_2d( Canvas *c )
{
	char buf[ 256 ];
	int x, y, x2, y2;
	long a_index, index_1, index_2;
	unsigned int w, h;
	Curve_2d *cv;
	Pixmap pm;
	double x_pos, y_pos, z_pos = 0, z_pos_1 = 0, z_pos_2 = 0;


	/* If no or either the middle or the left button is pressed no extra stuff
	   has to be drawn so just copy the pixmap with the curves into the
	   window. Also in the case that the graphics was never initialised this
	   is all to be done. */

	if ( ! ( G.button_state & 1 ) || ! G.is_init )
	{
		XCopyArea( G.d, c->pm, FL_ObjWin( c->obj ), c->gc,
				   0, 0, c->w, c->h, 0, 0 );
		return;
	}

	/* Otherwise use another level of buffering and copy the pixmap with
	   the surface into another pixmap */

	pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h,
						fl_get_canvas_depth( c->obj ) );
	XCopyArea( G.d, c->pm, pm, c->gc, 0, 0, c->w, c->h, 0, 0 );

	/* Draw the rubber box if needed (i.e. when the left button pressed
	   in the canvas currently to be drawn) */

	if ( G.button_state == 1 && c->is_box )
	{
		switch ( G2.cut_select )
		{
			case NO_CUT_SELECT :
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
				break;

			case CUT_SELECT_X :
				x = x2 = c->box_x + c->box_w;
				y = G.x_scale_offset + G.enlarge_box_width;
				y2 = 0;
				XDrawLine( G.d, pm, c->box_gc, x, y, x2, y2 );
				break;

			case CUT_SELECT_Y :
				y = y2 = c->box_y + c->box_h;
				x = c->w - ( G.y_scale_offset + G.enlarge_box_width + 1 );
				x2 = c->w - 1;
				XDrawLine( G.d, pm, c->box_gc, x, y, x2, y2 );
				break;
		}
	}

	/* If this is the canvas and the left and either the middle or the right
	   mouse button is pressed draw the current mouse position (converted to
	   real world coordinates) or the difference between the current position
	   and the point the buttons were pressed at into the left hand top corner
	   of the canvas. In the second case also draw some marker connecting the
	   initial and the current position. */

	if ( c == &G2.canvas )
	{
		if ( G.button_state == 3 && G2.active_curve != -1 &&
			 G2.curve_2d[ G2.active_curve ]->is_scale_set )
		{
			cv = G2.curve_2d[ G2.active_curve ];

			x_pos = ( c->ppos[ X ] + cv->w / 2 ) / cv->s2d[ X ]
				    - cv->shift[ X ];
			y_pos = ( G2.canvas.h - 1.0 - c->ppos[ Y ] + cv->h / 2 )
					/ cv->s2d[ Y ] - cv->shift[ Y ];

			if ( x_pos < 0 || floor( x_pos ) >= G2.nx ||
				 y_pos < 0 || floor( y_pos ) >= G2.ny ||
				 ! cv->is_scale_set )
				a_index = -1;
			else
			{
				a_index = G2.nx * lrnd( floor( y_pos ) )
						  + lrnd( floor( x_pos ) );

				if ( cv->points[ a_index ].exist )
					z_pos = cv->rwc_start[ Z ] + cv->rwc_delta[ Z ]
						    * cv->points[ a_index ].v;
				else
					a_index = -1;
			}

			x_pos = cv->rwc_start[ X ] + cv->rwc_delta[ X ]
				    * ( c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ] );
			y_pos = cv->rwc_start[ Y ] + cv->rwc_delta[ Y ]
				    * ( ( G2.canvas.h - 1.0 - c->ppos[ Y ] )
						/ cv->s2d[ Y ] - cv->shift[ Y ] );

			strcpy( buf, " x = " );
			make_label_string( buf + 5, x_pos,
							   irnd( floor( log10( fabs( cv->rwc_delta[ X ] )
												   / cv->s2d[ X ] ) ) - 2 ) );
			strcat( buf, "   y = " );
			make_label_string( buf + strlen( buf ), y_pos,
							   irnd( floor( log10( fabs( cv->rwc_delta[ Y ] )
												   / cv->s2d[ Y ] ) ) - 2 ) );
			if ( a_index != -1 )
			{
				strcat( buf, "   z = " );
				make_label_string( buf + strlen( buf ), z_pos,
					  irnd( floor( log10( fabs( cv->rwc_delta[ Z ] )
										  / cv->s2d[ Z ] ) ) - 2 ) );
			}
			strcat( buf, " " );

			if ( G.font != NULL )
				XDrawImageString( G.d, pm, cv->font_gc, 5,
								  G.font_asc + 5, buf, strlen( buf ) );
		}

		if ( G.button_state == 5 )            /* left and right mouse button */
		{
			if ( G2.active_curve != -1 &&
				 G2.curve_2d[ G2.active_curve ]->is_scale_set )
			{
				cv = G2.curve_2d[ G2.active_curve ];

				x_pos = ( c->ppos[ X ] + cv->w / 2 ) / cv->s2d[ X ]
					    - cv->shift[ X ];
				y_pos = ( G2.canvas.h - 1.0 - c->ppos[ Y ] + cv->h / 2 )
						/ cv->s2d[ Y ] - cv->shift[ Y ];

				if ( x_pos < 0 || floor( x_pos ) >= G2.nx ||
					 y_pos < 0 || floor( y_pos ) >= G2.ny ||
					 ! cv->is_scale_set )
					index_1 = -1;
				else
				{
					index_1 = G2.nx * lrnd( floor( y_pos ) )
						      + lrnd( floor( x_pos ) );

					if ( cv->points[ index_1 ].exist )
						z_pos_1 = cv->rwc_start[ Z ] + cv->rwc_delta[ Z ]
					            * ( cv->points[ index_1 ].v - cv->shift[ Z ] );
					else
						index_1 = -1;
				}

				x_pos = ( G.start[ X ] + cv->w / 2 ) / cv->s2d[ X ]
					    - cv->shift[ X ];
				y_pos = ( G2.canvas.h - 1.0 - G.start[ Y ] + cv->h / 2 )
						/ cv->s2d[ Y ] - cv->shift[ Y ];

				if ( x_pos < 0 || floor( x_pos ) >= G2.nx ||
					 y_pos < 0 || floor( y_pos ) >= G2.ny ||
					 ! cv->is_scale_set )
					index_2 = -1;
				else
				{
					index_2 = G2.nx * lrnd( floor( y_pos ) )
						      + lrnd( floor( x_pos ) );

					if ( cv->points[ index_2 ].exist )
						z_pos_2 = cv->rwc_start[ Z ] + cv->rwc_delta[ Z ]
					            * ( cv->points[ index_2 ].v - cv->shift[ Z ] );
					else
						index_2 = -1;
				}


				x_pos = cv->rwc_delta[ X ] * ( c->ppos[ X ] - G.start[ X ] )
						/ cv->s2d[ X ];
				y_pos = cv->rwc_delta[ Y ] * ( G.start[ Y ] - c->ppos[ Y ] )
						/ cv->s2d[ Y ];

				if ( index_1 == -1 || index_2 == -1 )
					sprintf( buf, " dx = %#g   dy = %#g ", x_pos, y_pos );
				else
					sprintf( buf, " dx = %#g   dy = %#g   dz = %#g ",
							 x_pos, y_pos, z_pos_1 - z_pos_2 );
				if ( G.font != NULL )
					XDrawImageString( G.d, pm, cv->font_gc, 5,
									  G.font_asc + 5, buf, strlen( buf ) );
			}

			XDrawArc( G.d, pm, c->font_gc,
					  G.start[ X ] - 5, G.start[ Y ] - 5, 10, 10, 0, 23040 );

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


/*---------------------------------------------------------*/
/* Does a rescale of the data for 2d graphics so that all  */
/* curves fit into the canvas and occupy the whole canvas. */
/*---------------------------------------------------------*/

void fs_rescale_2d( Curve_2d *cv )
{
	long i, count;
	double min = 1.0,
		   max = 0.0;
	double rw_min,
		   rw_max;
	double data;
	double new_rwc_delta_z;
	Scaled_Point *sp;


	if ( ! cv->is_scale_set )
		return;

	/* Find minimum and maximum value of all scaled data */

	for ( sp = cv->points, count = cv->count, i = 0;
		  i < G2.nx * G2.ny && count != 0; sp++, i++ )
		if ( sp->exist )
		{
			data = sp->v;
			max = d_max( data, max );
			min = d_min( data, min );
			count--;
		}

	/* If there are no points yet... */

	if ( min == 1.0 && max == 0.0 )
	{
		cv->rw_min = HUGE_VAL;
		cv->rw_max = - HUGE_VAL;
		cv->is_scale_set = UNSET;
		return;
	}

	/* Calculate new real world maximum and minimum */

	rw_min = cv->rwc_delta[ Z ] * min + cv->rw_min;
	rw_max = cv->rwc_delta[ Z ] * max + cv->rw_min;

	/* Calculate new scaling factor and rescale the scaled data as well as the
	   points for drawing */

	new_rwc_delta_z = rw_max - rw_min;

	cv->shift[ X ] = cv->shift[ Y ] = cv->shift[ Z ] = 0.0;
	cv->s2d[ X ] = ( G2.canvas.w - 1.0 ) / ( G2.nx - 1 );
	cv->s2d[ Y ] = ( G2.canvas.h - 1.0 ) / ( G2.ny - 1 );
	cv->s2d[ Z ] = G2.z_axis.h - 1.0;

	cv->up = cv->down = cv->left = cv->right = UNSET;

	for ( sp = cv->points, i = 0; i < G2.nx * G2.ny; sp++, i++ )
		if ( sp->exist )
			sp->v = ( cv->rwc_delta[ Z ] * sp->v + cv->rw_min - rw_min )
					/ new_rwc_delta_z;

	recalc_XPoints_of_curve_2d( cv );

	/* Store new minimum and maximum and the new scale factor */

	cv->rwc_delta[ Z ] = new_rwc_delta_z;
	cv->rw_min = cv->rwc_start[ Z ] = rw_min;
	cv->rw_max = rw_max;
	cv->z_factor = 1.0;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void make_scale_2d( Curve_2d *cv, Canvas *c, int coord )
{
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


	if ( coord == Z )
		make_color_scale( c, cv );

	/* The distance between the smallest ticks should be 'G.scale_tick_dist'
	   points - calculate the corresponding delta in real word units */

	rwc_delta = G.scale_tick_dist * fabs( cv->rwc_delta[ coord ] )
				/ cv->s2d[ coord ];

	/* Now scale this distance to the interval [ 1, 10 [ */

	mag = floor( log10( rwc_delta ) );
	order = pow( 10.0, mag );
	modf( rwc_delta / order, &rwc_delta );

	/* Get a 'smooth' value for the ticks distance, i.e. either 2, 2.5, 5 or
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

	d_delta_fine = cv->s2d[ coord ] * rwc_delta
				   / fabs( cv->rwc_delta[ coord ] );

	/* 'rwc_start' is the first value in the display (i.e. the smallest x or y
	   value still shown in the canvas), 'rwc_start_fine' the position of the
	   first small tick (both in real world coordinates) and, finally,
	   'd_start_fine' is the same position but in points */

	rwc_start = cv->rwc_start[ coord ]
		        - cv->shift[ coord ] * cv->rwc_delta[ coord ];
	if ( cv->rwc_delta[ coord ] < 0 )
		rwc_delta *= -1.0;

	modf( rwc_start / rwc_delta, &rwc_start_fine );
	rwc_start_fine *= rwc_delta;

	d_start_fine = cv->s2d[ coord ] * ( rwc_start_fine - rwc_start )
				   / cv->rwc_delta[ coord ];
	if ( lrnd( d_start_fine ) < 0 )
		d_start_fine += d_delta_fine;

	/* Calculate start index (in small tick counts) of first medium tick */

	modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
	rwc_start_medium *= medium_factor * rwc_delta;

	d_start_medium = cv->s2d[ coord ] * ( rwc_start_medium - rwc_start )
					 / cv->rwc_delta[ coord ];
	if ( lrnd( d_start_medium ) < 0 )
		d_start_medium += medium_factor * d_delta_fine;

	medium = lrnd( ( d_start_fine - d_start_medium ) / d_delta_fine );

	/* Calculate start index (in small tick counts) of first large tick */

	modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
	rwc_start_coarse *= coarse_factor * rwc_delta;

	d_start_coarse = cv->s2d[ coord ] * ( rwc_start_coarse - rwc_start )
					 / cv->rwc_delta[ coord ];
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
		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G2.active_curve ] ) );
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
		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G2.active_curve ] ) );
		XFillRectangle( G.d, c->pm, cv->gc, x, 0, 3, c->h );

		/* Draw all the ticks and numbers */

		for ( cur_p = c->h - 1.0 - d_start_fine; cur_p > - 0.5;
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
		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G2.active_curve ] ) );
		XFillRectangle( G.d, c->pm, cv->gc, x - 2, 0, 3, c->h );

		/* Draw all the ticks and numbers */

		for ( cur_p = c->h - 1.0 - d_start_fine; cur_p > -0.5;
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
	}
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void make_color_scale( Canvas *c, Curve_2d *cv )
{
	int i;
	unsigned int h;
	double col_inc;
	double h_inc;


	h = d2ushrt( ceil( ( double ) c->h / NUM_COLORS ) );
	h_inc = ( double ) c->h / NUM_COLORS;

	XDrawLine( G.d, c->pm, c->font_gc, G.z_line_offset, 0,
			   G.z_line_offset, c->h - 1 );
	XDrawLine( G.d, c->pm, c->font_gc, G.z_line_offset + G.z_line_width + 1,
			   0, G.z_line_offset + G.z_line_width + 1, c->h - 1 );

	col_inc = 1.0 / ( NUM_COLORS - 1 );

	for ( i = 0; i < NUM_COLORS; i++ )
	{
		XSetForeground( G.d, cv->gc, d2color( i * col_inc ) );
		XFillRectangle( G.d, c->pm, cv->gc, G.z_line_offset + 1,
						d2shrt( c->h - ( i + 1 ) * h_inc ),
						G.z_line_width, h );
	}
}


/*--------------------------------------------------------------------------*/
/* Returns the pixel value of an entry in XFORMs internal colour map from   */
/* the colours set in create_colors(). For values slightly above 1 as well  */
/* as for values just below 0 only one colour pixel value is returned while */
/* for intermediate values one of NUM_COLORS (as defined in global.h) is    */
/* returned, depending on the value.                                        */
/*--------------------------------------------------------------------------*/

static inline unsigned long d2color( double a )
{
	long c_index;


	c_index = lrnd( a * ( NUM_COLORS - 1 ) );

	if ( c_index < 0 )
		return G2.color_list[ NUM_COLORS ];
	else if ( c_index < NUM_COLORS )
		return G2.color_list[ c_index ];
	else
		return G2.color_list[ NUM_COLORS + 1 ];
}


/*-------------------------------------------------------*/
/* Gets called to create a marker at 'x_pos' and 'y_pos' */
/*-------------------------------------------------------*/

void set_marker_2d( long x_pos, long y_pos, long color, long curve )
{
	Marker_2D *m, *cm;
	XGCValues gcv;
	Curve_2d *cv;


	if ( color > MAX_CURVES + 1 )
	{
		delete_marker_2d( x_pos, y_pos, curve );
		return;
	}

	/* If no curve number was given and currently there's no curve
	   displayed we can't draw a marker */

	if ( curve < 0 )
	{
		if ( G2.active_curve == -1 )
			return;
		cv = G2.curve_2d[ G2.active_curve ];
	}
	else
		cv = G2.curve_2d[ curve ];

	m = MARKER_2D_P T_malloc( sizeof *m );
	m->next = 0;

	if ( cv->marker_2d == NULL )
		cv->marker_2d = m;
	else
	{
		cm = cv->marker_2d;
		while ( cm->next != NULL )
			cm = cm->next;
		cm->next = m;
	}

	gcv.line_style = LineSolid;
	gcv.line_width = 2;

	m->color = color;
	m->gc = XCreateGC( G.d, G2.canvas.pm, GCLineStyle | GCLineWidth, &gcv );

	if ( color == 0 )
		XSetForeground( G.d, m->gc, fl_get_pixel( FL_WHITE ) );
	else if ( color <= MAX_CURVES )
		XSetForeground( G.d, m->gc, fl_get_pixel( G.colors[ color - 1 ] ) );
	else
		XSetForeground( G.d, m->gc, fl_get_pixel( FL_BLACK ) );

	m->x_pos = x_pos;
	m->y_pos = y_pos;
}


/*------------------------------------------*/
/*------------------------------------------*/

static void delete_marker_2d( long x_pos, long y_pos, long curve )
{
	Marker_2D *m, *mp;
	Curve_2d *cv;


	/* If no curve number was given and currently there's no curve
	   displayed we can't remove a marker */

	if ( curve < 0 )
	{
		if ( G2.active_curve == -1 )
			return;
		cv = G2.curve_2d[ G2.active_curve ];
	}
	else
		cv = G2.curve_2d[ curve ];

	for ( mp = NULL, m = cv->marker_2d; m != NULL; mp = m, m = m->next )
	{
		if ( m->x_pos != x_pos || m->y_pos != y_pos )
			continue;

		XFreeGC( G.d, m->gc );
		if ( mp != NULL )
			mp->next = m->next;
		if ( m == cv->marker_2d )
			cv->marker_2d = m->next;
		T_free( m );
		return;
	}
}


/*-----------------------------------*/
/* Gets called to delete all markers */
/*-----------------------------------*/

void remove_markers_2d( void )
{
	Marker_2D *m, *mn;
	Curve_2d *cv;
	int i;


	for ( i = 0; i < G2.nc; i++ )
	{
		cv = G2.curve_2d[ i ];

		if ( cv->marker_2d == NULL )
			continue;

		for ( m = cv->marker_2d; m != NULL; m = mn )
		{
			XFreeGC( G.d, m->gc );
			mn = m->next;
			m = MARKER_2D_P T_free( m );
		}
		cv->marker_2d = NULL;

		if ( i == G2.active_curve )
			repaint_canvas_2d( &G2.canvas );
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
