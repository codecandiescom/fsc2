/*
  $Id$
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
static void shift_XPoints_of_curve_2d( Canvas *c, Curve_2d *cv );
static void make_color_scale( Canvas *c, Curve_2d *cv );


/*-----------------------------------------------------------*/
/* Handler for all kinds of X events the canvas may receive. */
/*-----------------------------------------------------------*/

int canvas_handler_2d( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					   void *udata )
{
	Canvas *c = ( Canvas * ) udata;


	obj = obj;
	window = window;

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

void press_handler_2d( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
{
	long i;
	Curve_2d *cv;
	int old_button_state = G.button_state;
	int keymask;


	/* In the axes areas two buttons pressed simultaneously doesn't has a
	   special meaning, so don't care about another button. Also don't react
	   if the pressed buttons have lost there meaning */

	if ( ( c != &G.canvas && G.raw_button_state != 0 ) ||
		 ( G.button_state == 0 && G.raw_button_state != 0 ) ||
		 G.active_curve == -1 )
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
	   
	if ( obj == run_form->x_axis )        /* in x-axis window */
		G.drag_canvas = 1;
	if ( obj == run_form->y_axis )        /* in y-axis window */
		G.drag_canvas = 2;
	if ( obj == run_form->z_axis )        /* in z-axis window */
		G.drag_canvas = 4;
	if ( obj == run_form->canvas )        /* in canvas window */
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
					if ( keymask & ShiftMask )
					{
						fl_set_cursor( window, G.cur_6 );
						G.cut_select = CUT_SELECT_X;
					}
					else
						fl_set_cursor( window, G.cur_1 );

					c->box_x = c->ppos[ X ];
					c->box_w = 0;
					c->box_y = X_SCALE_OFFSET + 1;
					c->box_h = ENLARGE_BOX_WIDTH;
					c->is_box = SET;
					break;

				case 2 :                       /* in y-axis window */
					if ( keymask & ShiftMask )
					{
						fl_set_cursor( window, G.cur_7 );
						G.cut_select = CUT_SELECT_Y;
					}
					else
						fl_set_cursor( window, G.cur_1 );

					c->box_x = c->w
						       - ( Y_SCALE_OFFSET + ENLARGE_BOX_WIDTH + 1 );
					c->box_y = c->ppos[ Y ];
					c->box_w = ENLARGE_BOX_WIDTH;
					c->box_h = 0;
					c->is_box = SET;
					break;

				case 4 :                       /* in z-axis window */
					fl_set_cursor( window, G.cur_1 );

					c->box_x = Z_SCALE_OFFSET + 1;
					c->box_y = c->ppos[ Y ];
					c->box_w = ENLARGE_BOX_WIDTH;
					c->box_h = 0;
					c->is_box = SET;
					break;

				case 7 :                       /* in canvas window */
					fl_set_cursor( window, G.cur_1 );

					c->box_x = c->ppos[ X ];
					c->box_y = c->ppos[ Y ];
					c->box_w = c->box_h = 0;
					c->is_box = SET;
					break;
			}

			repaint_canvas_2d( c );
			break;

		case 2 :                               /* middle button */
			fl_set_cursor( window, G.cur_2 );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			/* Store data for undo operation */

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve_2d[ i ];

				if ( cv->active )
					save_scale_state_2d( cv );
				else
					cv->can_undo = UNSET;
			}
			break;

		case 3:                                /* left and middle button */
			fl_set_cursor( window, G.cur_4 );

			/* Don't draw the box anymore */

			G.canvas.is_box = UNSET;
			repaint_canvas_2d( &G.canvas );
			break;

		case 4 :                               /* right button */
			fl_set_cursor( window, G.cur_3 );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];
			break;

		case 5 :                               /* left and right button */
			fl_set_cursor( window, G.cur_5 );

			if ( G.canvas.is_box == UNSET && old_button_state != 4 )
			{
				G.start[ X ] = c->ppos[ X ];
				G.start[ Y ] = c->ppos[ Y ];
			}
			else
				G.canvas.is_box = UNSET;

			repaint_canvas_2d( &G.canvas );
			break;
	}
}


/*---------------------------------------------------------------*/
/* Handler for events due to releasing one of the mouse buttons. */
/*---------------------------------------------------------------*/

void release_handler_2d( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
{
	int keymask;
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
	if ( c->ppos[ X ] >= ( int ) G.canvas.w )
		c->ppos[ X ] = G.canvas.w - 1;

	if ( c->ppos[ Y ] < 0 )
		c->ppos[ Y ] = 0;
	if ( c != &G.z_axis )
	{
		if ( c->ppos[ Y ] >= ( int ) G.canvas.h )
			c->ppos[ Y ] = G.canvas.h - 1;
	}
	else if ( c->ppos[ Y ] >= ( int ) c->h )         /* in z-axis window */
		c->ppos[ Y ] = c->h - 1;

	switch ( G.button_state )
	{
		case 1 :                               /* left mouse button */
			switch ( G.drag_canvas )
			{
				case 1 :                       /* x-axis window */
					if ( G.cut_select == NO_CUT_SELECT )
						scale_changed = change_x_range_2d( c );
					else if ( G.cut_select == CUT_SELECT_X &&
							  keymask & ShiftMask )
						show_cut( X, c->box_x + c->box->w );
					break;

				case 2 :                       /* in y-axis window */
					if ( G.cut_select == NO_CUT_SELECT )
						scale_changed = change_y_range_2d( c );
					else if ( G.cut_select == CUT_SELECT_Y &&
							  keymask & ShiftMask )
						show_cut( Y, c->box_y + c->box_h );
					break;

				case 4 :
					scale_changed = change_z_range_2d( c );
					break;

				case 7 :                       /* in canvas window */
					scale_changed = change_xy_range_2d( c );
					break;
			}
			c->is_box = UNSET;
			break;

		case 2 :                               /* middle mouse button */
			if ( G.drag_canvas & 1 )
				redraw_canvas_2d( &G.x_axis );
			if ( G.drag_canvas & 2 )
				redraw_canvas_2d( &G.y_axis );
			if ( G.drag_canvas & 4 )
				redraw_canvas_2d( &G.z_axis );
			break;

		case 4 :                               /* right mouse button */
			switch ( G.drag_canvas )
			{
				case 1 :                       /* in x-axis window */
					scale_changed = zoom_x_2d( c );
					break;

				case 2 :                       /* in y-axis window */
					scale_changed = zoom_y_2d( c );
					break;

				case 4 :                       /* in z-axis window */
					scale_changed = zoom_z_2d( c );
					break;

				case 7 :                       /* in canvas window */
					scale_changed = zoom_xy_2d( c );
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
		if ( G.curve_2d[ G.active_curve ]->is_fs )
		{
			G.curve_2d[ G.active_curve ]->is_fs = UNSET;
			fl_set_button( run_form->full_scale_button, 0 );
			fl_set_object_helper( run_form->full_scale_button,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
		}

		redraw_all_2d( );
	}

	if ( ! scale_changed || c != &G.canvas )
		repaint_canvas_2d( c );
}


/*---------------------------------------------------*/
/* Handler for events due to movements of the mouse. */
/*---------------------------------------------------*/

void motion_handler_2d( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
{
	XEvent new_ev;
	int keymask;
	bool scale_changed = UNSET;

	
	obj = obj;

	/* We need to do event compression to avoid being flooded with motion
	   events - instead of handling them all individually we only react to
	   the latest in the series of motion events for the current window */

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

	if ( G.active_curve == -1 )
		return;

	fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

	switch ( G.button_state )
	{
		case 1 :                               /* left mouse button */

			/* If we were in cut select mode and the shift button has
			   become released get out of this mode */

			if ( ( G.cut_select == CUT_SELECT_X ||
				   G.cut_select == CUT_SELECT_Y ) &&
				 ! ( keymask & ShiftMask ) )
			{
				G.cut_select = CUT_SELECT_BREAK;
				fl_reset_cursor( window );
			}

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
			}

			repaint_canvas_2d( c );
			break;

		case 2 :                               /* middle button */
			if ( G.active_curve != -1 &&
				 G.curve_2d[ G.active_curve ]->is_scale_set )
			{
				shift_XPoints_of_curve_2d( c, G.curve_2d[ G.active_curve ] );
				scale_changed = SET;
			}

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			if ( G.curve_2d[ G.active_curve ]->is_fs && scale_changed )
			{
				G.curve_2d[ G.active_curve ]->is_fs = UNSET;
				fl_set_button( run_form->full_scale_button, 0 );
				fl_set_object_helper( run_form->full_scale_button,
									  "Rescale curves to fit into the window\n"
									  "and switch on automatic rescaling" );
			}

			redraw_canvas_2d( &G.canvas );
			if ( G.drag_canvas & 1 )
				redraw_canvas_2d( &G.x_axis );
			if ( G.drag_canvas & 2 )
				redraw_canvas_2d( &G.y_axis );
			if ( G.drag_canvas == 4 )
				redraw_canvas_2d( &G.z_axis );
			break;

		case 3 : case 5 :               /* left and (middle or right) button */
			repaint_canvas_2d( &G.canvas );
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

bool change_x_range_2d( Canvas *c )
{
	Curve_2d *cv;
	double x1, x2;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 || G.active_curve == -1 ||
		 ! G.curve_2d[ G.active_curve ]->is_scale_set )
		return UNSET;

	cv = G.curve_2d[ G.active_curve ];

	save_scale_state_2d( cv );

	x1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
	x2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

	cv->shift[ X ] = - d_min( x1, x2 );
	cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) / fabs( x1 - x2 );

	recalc_XPoints_of_curve_2d( cv );

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_y_range_2d( Canvas *c )
{
	Curve_2d *cv;
	double y1, y2;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 || G.active_curve == -1 ||
		 ! G.curve_2d[ G.active_curve ]->is_scale_set )
		return UNSET;

	cv = G.curve_2d[ G.active_curve ];

	save_scale_state_2d( cv );

	y1 = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
		 - cv->shift[ Y ];
	y2 = ( ( double ) G.canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
		 - cv->shift[ Y ];

	cv->shift[ Y ] = - d_min( y1, y2 );
	cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 ) / fabs( y1 - y2 );

	recalc_XPoints_of_curve_2d( cv );

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_xy_range_2d( Canvas *c )
{
	bool scale_changed = UNSET;
	Curve_2d *cv;
	double x1, x2, y1, y2;


	if ( G.active_curve == -1 ||
		 ! G.curve_2d[ G.active_curve ]->is_scale_set )
		return UNSET;

	cv = G.curve_2d[ G.active_curve ];

	save_scale_state_2d( cv );
	cv->can_undo = UNSET;

	if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
	{
		cv->can_undo = SET;

		x1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
		x2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

		cv->shift[ X ] = - d_min( x1, x2 );
		cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) / fabs( x1 - x2 );

		scale_changed = SET;
	}

	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
	{
		cv->can_undo = SET;

		y1 = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
			 - cv->shift[ Y ];
		y2 = ( ( double ) G.canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
			 - cv->shift[ Y ];

		cv->shift[ Y ] = - d_min( y1, y2 );
		cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 ) / fabs( y1 - y2 );

		scale_changed = SET;
	}

	if ( scale_changed )
		recalc_XPoints_of_curve_2d( cv );

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_z_range_2d( Canvas *c )
{
	Curve_2d *cv;
	double z1, z2;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 || G.active_curve == -1 ||
		 ! G.curve_2d[ G.active_curve ]->is_scale_set )
		return UNSET;

	cv = G.curve_2d[ G.active_curve ];

	save_scale_state_2d( cv );

	z1 = ( ( double ) G.z_axis.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Z ]
		 - cv->shift[ Z ];
	z2 = ( ( double ) G.z_axis.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Z ]
		 - cv->shift[ Z ];

	cv->shift[ Z ] = - d_min( z1, z2 );
	cv->z_factor = 1.0 / fabs( z1 - z2 );
	cv->s2d[ Z ] = ( double ) ( G.z_axis.h - 1 ) * cv->z_factor;

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_x_2d( Canvas *c )
{
	Curve_2d *cv;
	double px;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 || G.active_curve == -1 ||
		 ! G.curve_2d[ G.active_curve ]->is_scale_set )
		return UNSET;

	cv = G.curve_2d[ G.active_curve ];

	save_scale_state_2d( cv );

	px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

	/* If the mouse was moved to lower values zoom the display by a factor
	   of up to 4 (if the mouse was moved over the whole length of the
	   scale) while keeping the point the move was started at the same
	   position. If the mouse was moved upwards demagnify by the inverse
	   factor. */

	if ( G.start[ X ] > c->ppos[ X ] )
		cv->s2d[ X ] *= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( G.start[ X ] - c->ppos[ X ] ) /
								                       ( double ) G.x_axis.w );
	else
		cv->s2d[ X ] /= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( c->ppos[ X ] - G.start[ X ] ) /
								                       ( double ) G.x_axis.w );

	cv->shift[ X ] = G.start[ X ] / cv->s2d[ X ] - px;

	recalc_XPoints_of_curve_2d( cv );

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_y_2d( Canvas *c )
{
	Curve_2d *cv;
	double py;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 || G.active_curve == -1 ||
		 ! G.curve_2d[ G.active_curve ]->is_scale_set )
		return UNSET;

	cv = G.curve_2d[ G.active_curve ];

	save_scale_state_2d( cv );

	/* Get the value in the interval [0, 1] corresponding to the mouse
	   position */

	py = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
		 - cv->shift[ Y ];

	/* If the mouse was moved to lower values zoom the display by a factor
	   of up to 4 (if the mouse was moved over the whole length of the
	   scale) while keeping the point the move was started at the same
	   position. If the mouse was moved upwards demagnify by the inverse
	   factor. */

	if ( G.start[ Y ] < c->ppos[ Y ] )
		cv->s2d[ Y ] *= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( c->ppos[ Y ] - G.start[ Y ] ) /
								                       ( double ) G.y_axis.h );
	else
		cv->s2d[ Y ] /= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( G.start[ Y ] - c->ppos[ Y ] ) /
								                       ( double ) G.y_axis.h );

	cv->shift[ Y ] = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ] ) /
				                                             cv->s2d[ Y ] - py;

	recalc_XPoints_of_curve_2d( cv );

	return SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_xy_2d( Canvas *c )
{
	bool scale_changed = UNSET;
	Curve_2d *cv;
	double px, py;


	if ( G.active_curve == -1 ||
		 ! G.curve_2d[ G.active_curve ]->is_scale_set )
		return UNSET;

	cv = G.curve_2d[ G.active_curve ];

	save_scale_state_2d( cv );
	cv->can_undo = UNSET;

	if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
	{
		cv->can_undo = SET;

		px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

		if ( G.start[ X ] > c->ppos[ X ] )
			cv->s2d[ X ] *= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( G.start[ X ] - c->ppos[ X ] ) /
								                       ( double ) G.x_axis.w );
		else
			cv->s2d[ X ] /= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( c->ppos[ X ] - G.start[ X ] ) /
								                       ( double ) G.x_axis.w );

		cv->shift[ X ] = G.start[ X ] / cv->s2d[ X ] - px;

		scale_changed = SET;
	}

	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
	{
		cv->can_undo = SET;

		py = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
			 - cv->shift[ Y ];

		if ( G.start[ Y ] < c->ppos[ Y ] )
			cv->s2d[ Y ] *= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( c->ppos[ Y ] - G.start[ Y ] ) /
								                       ( double ) G.y_axis.h );
		else
			cv->s2d[ Y ] /= d_min( 4.0,
					   1.0 + 3.0 * ( double ) ( G.start[ Y ] - c->ppos[ Y ] ) /
								                       ( double ) G.y_axis.h );

		cv->shift[ Y ] = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ] ) /
				                                             cv->s2d[ Y ] - py;

			scale_changed = SET;
	}
	
	if ( scale_changed )
		recalc_XPoints_of_curve_2d( cv );

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_z_2d( Canvas *c )
{
	Curve_2d *cv;
	double pz;
	double factor;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 || G.active_curve == -1 ||
		 ! G.curve_2d[ G.active_curve ]->is_scale_set )
		return UNSET;

	cv = G.curve_2d[ G.active_curve ];

	save_scale_state_2d( cv );

	/* Get the value in the interval [0, 1] corresponding to the mouse
	   position */

	pz = ( ( double ) G.z_axis.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Z ]
		 - cv->shift[ Z ];

	/* If the mouse was moved to lower values zoom the display by a factor
	   of up to 4 (if the mouse was moved over the whole length of the
	   scale) while keeping the point the move was started at the same
	   position. If the mouse was moved upwards demagnify by the inverse
	   factor. */

	factor = d_min( 4.0,
				   1.0 + 3.0 * ( double ) fabs( c->ppos[ Y ] - G.start[ Y ] ) /
								                       ( double ) G.z_axis.h );

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

	cv->shift[ Z ] = ( ( double ) G.z_axis.h - 1.0 - G.start[ Y ] ) /
				                                             cv->s2d[ Z ] - pz;

	return SET;
}


/*-----------------------------------------------------------------------*/
/* This is basically a simplified version of `recalc_XPoints_of_curve()' */
/* because we need to do much less calculations, i.e. just adding an     */
/* offset to all XPoints instead of going through all the scalings...    */
/*-----------------------------------------------------------------------*/

void shift_XPoints_of_curve_2d( Canvas *c, Curve_2d *cv )
{
	long i;
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

	if ( G.drag_canvas == 4 )                     /* z-axis window */
	{
		dz = factor * ( c->ppos[ Y ] - G.start[ Y ] );
		cv->shift[ Z ] -= ( double ) dz / cv->s2d[ Z ];
		return;
	}

	/* Add the shifts to the XPoints */

	cv->up = cv->down = cv->left = cv->right = SET;

	for ( i = 0; i < G.nx * G.ny; sp++, xp++, xps++, i++ )
	{
		xp->x = i2shrt( xp->x + dx );
		xp->y = i2shrt( xp->y + dy );
		xps->x = xp->x - ( cv->w >> 1 );
		xps->y = xp->y - ( cv->h >> 1 );

		if ( sp->exist )
		{
			cv->left &= ( xps->x + cv->w <= 0 );
			cv->right &= ( xps->x >= ( int ) G.canvas.w );
			cv->up &= ( xps->y + cv->h <= 0 );
			cv->down &= ( xps->y >= ( int ) G.canvas.h );
		}
	}
}


/*-------------------------------------*/
/* Handles changes of the window size. */
/*-------------------------------------*/

void reconfigure_window_2d( Canvas *c, int w, int h )
{
	long i;
	Curve_2d *cv;
	static bool is_reconf[ 3 ] = { UNSET, UNSET, UNSET };
	static bool need_redraw[ 3 ] = { UNSET, UNSET, UNSET };
	int old_w = c->w,
		old_h = c->h;


	/* Set the new canvas sizes */

	c->w = ( unsigned int ) w;
	c->h = ( unsigned int ) h;

	/* Calculate the new scale factors */

	if ( c == &G.canvas )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve_2d[ i ];
			if ( ! cv->is_scale_set )
				continue;

			cv->s2d[ X ] *= ( double ) ( w - 1 ) / ( double ) ( old_w - 1 );
			cv->s2d[ Y ] *= ( double ) ( h - 1 ) / ( double ) ( old_h - 1 );

			if ( cv->can_undo )
			{
				cv->old_s2d[ X ] *= ( double ) ( w - 1 ) /
					                                  ( double ) ( old_w - 1 );
				cv->old_s2d[ Y ] *= ( double ) ( h - 1 ) /
					                                  ( double ) ( old_h - 1 );
			}
		}

		/* Recalculate data for drawing (has to be done after setting of canvas
		   sizes since they are needed in the recalculation) */

		recalc_XPoints_2d( );
	}

	/* We can't know the sequence the different canvases are reconfigured in
	   but, on the other hand, redrawing an axis canvas is useless before the
	   new scaling factors are set. Thus we need in the call for the canvas
	   window to redraw also axis windows which got reconfigured before. */

	delete_pixmap( c );
	create_pixmap( c );

	if ( c == &G.z_axis )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve_2d[ i ];
			if ( ! cv->is_scale_set )
				continue;

			cv->s2d[ Z ] *= ( double ) ( h - 1 ) / ( double ) ( old_h - 1 );

			if ( cv->can_undo )
				cv->old_s2d[ Z ] *= ( double ) ( h - 1 ) /
					                                  ( double ) ( old_h - 1 );
		}
	}

	if ( c == &G.canvas )
	{
		redraw_canvas_2d( c );

		if ( need_redraw[ X ] )
		{
			redraw_canvas_2d( &G.x_axis );
			need_redraw[ X ] = UNSET;
		}
		else if ( w != old_w )
			is_reconf[ X ] = SET;

		if ( need_redraw[ Y ] )
		{
			redraw_canvas_2d( &G.y_axis );
			need_redraw[ Y ] = UNSET;
		}
		else if ( h != old_h )
			is_reconf[ Y ] = SET;

		if ( need_redraw[ Z ] )
		{
			redraw_canvas_2d( &G.z_axis );
			need_redraw[ Z ] = UNSET;
		}
		else if ( h != old_h )
			is_reconf[ Z ] = SET;
	}

	if ( c == &G.x_axis )
	{
		if ( is_reconf[ X ] )
		{
			redraw_canvas_2d( c );
			is_reconf[ X ] = UNSET;
		}
		else
			need_redraw[ X ] = SET;
	}

	if ( c == &G.y_axis )
	{
		if ( is_reconf[ Y ] )
		{
			redraw_canvas_2d( c );
			is_reconf[ Y ] = UNSET;
		}
		else
			need_redraw[ Y ] = SET;
	}

	if ( c == &G.z_axis )
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

void recalc_XPoints_2d( void )
{
	long i;


	for ( i = 0; i < G.nc; i++ )
		recalc_XPoints_of_curve_2d( G.curve_2d[ i ] );
}


/*-----------------------------------------------------------------*/
/* Recalculates the graphics data for a curve using the the curves */
/* settings for the scale and the offset.                          */
/*-----------------------------------------------------------------*/

void recalc_XPoints_of_curve_2d( Curve_2d *cv )
{
	long i, j;
	Scaled_Point *sp;
	XPoint *xp, *xps;
	short dw, dh;


	cv->up = cv->down = cv->left = cv->right = SET;

	cv->w = ( unsigned short ) ceil( cv->s2d[ X ] );
	cv->h = ( unsigned short ) ceil( cv->s2d[ Y ] );
	dw = cv->w / 2;
	dh = cv->h / 2;

	for ( sp = cv->points, xp = cv->xpoints, xps = cv->xpoints_s, i = 0;
		  i < G.ny; i++ )
		for ( j = 0; j < G.nx; sp++, xp++, xps++, j++ )
		{
			xp->x = d2shrt( cv->s2d[ X ] * ( j + cv->shift[ X ] ) );
			xp->y = ( short ) G.canvas.h - 1 -
			                   d2shrt( cv->s2d[ Y ] * ( i + cv->shift[ Y ] ) );
			xps->x = xp->x - dw;
			xps->y = xp->y - dh;

			if ( sp->exist )
			{
				cv->left &= ( xps->x + cv->w <= 0 );
				cv->right &= ( xps->x >= ( int ) G.canvas.w );
				cv->up &= ( xps->y + cv->h <= 0 );
				cv->down &= ( xps->y >= ( int ) G.canvas.h );
			}
		}
}


/*-----------------------------------------*/
/* Does a complete redraw of all canvases. */
/*-----------------------------------------*/

void redraw_all_2d( void )
{
	redraw_canvas_2d( &G.canvas );
	redraw_canvas_2d( &G.x_axis );
	redraw_canvas_2d( &G.y_axis );
	redraw_canvas_2d( &G.z_axis );
}


/*-------------------------------------*/
/* Does a complete redraw of a canvas. */
/*-------------------------------------*/

void redraw_canvas_2d( Canvas *c )
{
	long i;
	Curve_2d *cv;
	Scaled_Point *sp;
	XPoint *xps;


	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

	if ( ! G.is_init )
		repaint_canvas_2d( c );

	if ( c == &G.canvas )
	{
		/* First draw the active curve */

		if ( G.active_curve != -1 &&
			 G.curve_2d[ G.active_curve ]->count > 1 &&
			 G.curve_2d[ G.active_curve ]->is_scale_set )
		{
			cv = G.curve_2d[ G.active_curve ];

			for ( sp = cv->points, xps = cv->xpoints_s, i = 0;
				  i < G.nx * G.ny; sp++, xps++, i++ )
			{
				if ( sp->exist )
					XSetForeground( G.d, cv->gc,
									d2color( cv->z_factor
											 * ( sp->v + cv->shift[ Z ] ) ) );
				else
					XSetForeground( G.d, cv->gc,
									fl_get_pixel( FL_INACTIVE ) );

				XFillRectangle( G.d, c->pm, cv->gc,
								xps->x, xps->y, cv->w, cv->h );
			}

			/* Now draw the out of range arrows */

			if ( cv->up )
				XCopyArea( G.d, cv->up_arrow, c->pm, c->gc, 0, 0,
						   G.up_arrow_w, G.up_arrow_h,
						   ( G.canvas.w - G.up_arrow_w ) / 2, 5 );

			if ( cv->down )
				XCopyArea( G.d, cv->down_arrow, c->pm, c->gc, 0, 0,
						   G.down_arrow_w, G.down_arrow_h,
						   ( G.canvas.w - G.down_arrow_w ) / 2,
						   G.canvas.h - 5 - G.down_arrow_h );

			if ( cv->left )
				XCopyArea( G.d, cv->left_arrow, c->pm, c->gc, 0, 0,
						   G.left_arrow_w, G.left_arrow_h, 5,
						   ( G.canvas.h - G.left_arrow_h ) / 2 );

			if ( cv->right )
				XCopyArea( G.d, cv->right_arrow, c->pm, c->gc, 0, 0,
						   G.right_arrow_w, G.right_arrow_h,
						   G.canvas.w - 5 - G.right_arrow_w,
						   ( G.canvas.h - G.right_arrow_h ) / 2 );
		}
	}

	if ( c == &G.x_axis )
		redraw_axis( X );

	if ( c == &G.y_axis )
		redraw_axis( Y );

	if ( c == &G.z_axis )
		redraw_axis( Z );

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
	long index, index_1, index_2;
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
	   the curves into another pixmap */
	
	pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h,
						fl_get_canvas_depth( c->obj ) );
	XCopyArea( G.d, c->pm, pm, c->gc, 0, 0, c->w, c->h, 0, 0 );

	/* Draw the rubber box if needed (i.e. when the left button pressed
	   in the canvas currently to be drawn) */

	if ( G.button_state == 1 && c->is_box )
	{
		switch ( G.cut_select )
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
				y = X_SCALE_OFFSET + ENLARGE_BOX_WIDTH;
				y2 = 0;
				XDrawLine( G.d, pm, c->box_gc, x, y, x2, y2 );
				break;

			case CUT_SELECT_Y :
				y = y2 = c->box_y + c->box_h;
				x = c->w - ( Y_SCALE_OFFSET + ENLARGE_BOX_WIDTH + 1 );
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

	if ( c == &G.canvas )
	{
		if ( G.button_state == 3 )
		{
			if ( G.active_curve != -1 &&
				 G.curve_2d[ G.active_curve ]->is_scale_set )
			{
				cv = G.curve_2d[ G.active_curve ];

				x_pos = ( c->ppos[ X ] + cv->w / 2 ) / cv->s2d[ X ]
					    - cv->shift[ X ];
				y_pos = ( ( double ) G.canvas.h - 1.0 - c->ppos[ Y ]
						  + cv->h / 2 ) / cv->s2d[ Y ] - cv->shift[ Y ];

				if ( x_pos < 0 || floor( x_pos ) >= G.nx ||
					 y_pos < 0 || floor( y_pos ) >= G.ny ||
					 ! cv->is_scale_set )
					index = -1;
				else
				{
					index = G.nx * ( long ) floor( y_pos )
						    + ( long ) floor( x_pos );

					if ( cv->points[ index ].exist )
						z_pos = cv->rwc_start[ Z ] + cv->rwc_delta[ Z ]
					            * cv->points[ index ].v;
					else
						index = -1;
				}

				x_pos = cv->rwc_start[ X ] + cv->rwc_delta[ X ]
					        * ( c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ] );
				y_pos = cv->rwc_start[ Y ] + cv->rwc_delta[ Y ]
					       * ( ( ( double ) G.canvas.h - 1.0 - c->ppos[ Y ] ) /
									           cv->s2d[ Y ] - cv->shift[ Y ] );

				strcpy( buf, " " );
				make_label_string( buf + 1, x_pos, ( int ) floor( log10( fabs(
					cv->rwc_delta[ X ] ) / cv->s2d[ X ] ) ) - 2 );
				strcat( buf, ",  " ); 
				make_label_string( buf + strlen( buf ), y_pos,
								   ( int ) floor( log10( fabs(
									 cv->rwc_delta[ Y ] ) /
														cv->s2d[ Y ] ) ) - 2 );
				if ( index != -1 )
				{
					strcat( buf, ",  " ); 
					make_label_string( buf + strlen( buf ), z_pos,
									   ( int ) floor( log10( fabs(
										                cv->rwc_delta[ Z ] ) /
														cv->s2d[ Z ] ) ) - 2 );
				}
				strcat( buf, " " );

				if ( G.font != NULL )
					XDrawImageString( G.d, pm, cv->font_gc, 5,
									  G.font_asc + 5, buf, strlen( buf ) );
			}
		}

		if ( G.button_state == 5 )
		{
			if ( G.active_curve != -1 &&
				 G.curve_2d[ G.active_curve ]->is_scale_set )
			{
				cv = G.curve_2d[ G.active_curve ];

				x_pos = ( c->ppos[ X ] + cv->w / 2 ) / cv->s2d[ X ]
					    - cv->shift[ X ];
				y_pos = ( ( double ) G.canvas.h - 1.0 - c->ppos[ Y ]
						  + cv->h / 2 ) / cv->s2d[ Y ] - cv->shift[ Y ];

				if ( x_pos < 0 || floor( x_pos ) >= G.nx ||
					 y_pos < 0 || floor( y_pos ) >= G.ny ||
					 ! cv->is_scale_set )
					index_1 = -1;
				else
				{
					index_1 = G.nx * ( long ) floor( y_pos )
						      + ( long ) floor( x_pos );

					if ( cv->points[ index_1 ].exist )
						z_pos_1 = cv->rwc_start[ Z ] + cv->rwc_delta[ Z ]
					            * ( cv->points[ index_1 ].v - cv->shift[ Z ] );
					else
						index_1 = -1;
				}

				x_pos = ( G.start[ X ] + cv->w / 2 ) / cv->s2d[ X ]
					    - cv->shift[ X ];
				y_pos = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ]
						  + cv->h / 2 ) / cv->s2d[ Y ] - cv->shift[ Y ];

				if ( x_pos < 0 || floor( x_pos ) >= G.nx ||
					 y_pos < 0 || floor( y_pos ) >= G.ny ||
					 ! cv->is_scale_set )
					index_2 = -1;
				else
				{
					index_2 = G.nx * ( long ) floor( y_pos )
						      + ( long ) floor( x_pos );

					if ( cv->points[ index_2 ].exist )
						z_pos_2 = cv->rwc_start[ Z ] + cv->rwc_delta[ Z ]
					            * ( cv->points[ index_2 ].v - cv->shift[ Z ] );
					else
						index_2 = -1;
				}


				x_pos = cv->rwc_delta[ X ] * ( c->ppos[ X ] - G.start[ X ] ) /
					                                              cv->s2d[ X ];
				y_pos = cv->rwc_delta[ Y ] * ( G.start[ Y ] - c->ppos[ Y ] ) /
					                                              cv->s2d[ Y ];

				if ( index_1 == -1 || index_2 == -1 )
					sprintf( buf, "%#g,  %#g ", x_pos, y_pos );
				else
					sprintf( buf, "%#g,  %#g,  %#g ", x_pos, y_pos,
							 z_pos_1 - z_pos_2 );
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
	long i;
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

	for ( sp = cv->points, i = 0; i < G.nx * G.ny; sp++, i++ )
		if ( sp->exist )
		{
			data = sp->v;
			max = d_max( data, max );
			min = d_min( data, min );
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
	cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
	cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 ) / ( double ) ( G.ny - 1 );
	cv->s2d[ Z ] = ( double ) ( G.z_axis.h - 1 );

	cv->up = cv->down = cv->left = cv->right = UNSET;

	for ( sp = cv->points, i = 0; i < G.nx * G.ny; sp++, i++ )
		if ( sp->exist )
			sp->v = ( cv->rwc_delta[ Z ] * sp->v + cv->rw_min - rw_min ) /
						                                       new_rwc_delta_z;

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
	char lstr[ 128 ];
	int width;
	short last = -1000;


	if ( coord == Z )
		make_color_scale( c, cv );

	/* The distance between the smallest ticks should be ca. `SCALE_TICK_DIST'
	   points - calculate the corresponding delta in real word units */

	rwc_delta = ( double ) SCALE_TICK_DIST * fabs( cv->rwc_delta[ coord ] ) /
		                                                      cv->s2d[ coord ];

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

	d_delta_fine = cv->s2d[ coord ] * rwc_delta /
		                                        fabs( cv->rwc_delta[ coord ] );

	/* `rwc_start' is the first value in the display (i.e. the smallest x or y
	   value still shown in the canvas), `rwc_start_fine' the position of the
	   first small tick (both in real world coordinates) and, finally,
	   `d_start_fine' is the same position but in points */

	rwc_start = cv->rwc_start[ coord ]
		        - cv->shift[ coord ] * cv->rwc_delta[ coord ];
	if ( cv->rwc_delta[ coord ] < 0 )
		rwc_delta *= -1.0;

	modf( rwc_start / rwc_delta, &rwc_start_fine );
	rwc_start_fine *= rwc_delta;

	d_start_fine = cv->s2d[ coord ] * ( rwc_start_fine - rwc_start ) /
		                                                cv->rwc_delta[ coord ];
	if ( lround( d_start_fine ) < 0 )
		d_start_fine += d_delta_fine;

	/* Calculate start index (in small tick counts) of first medium tick */

	modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
	rwc_start_medium *= medium_factor * rwc_delta;

	d_start_medium = cv->s2d[ coord ] * ( rwc_start_medium - rwc_start ) /
			                                            cv->rwc_delta[ coord ];
	if ( lround( d_start_medium ) < 0 )
		d_start_medium += medium_factor * d_delta_fine;

	medium = lround( ( d_start_fine - d_start_medium ) / d_delta_fine );

	/* Calculate start index (in small tick counts) of first large tick */

	modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
	rwc_start_coarse *= coarse_factor * rwc_delta;

	d_start_coarse = cv->s2d[ coord ] * ( rwc_start_coarse - rwc_start ) /
			                                            cv->rwc_delta[ coord ];
	if ( lround( d_start_coarse ) < 0 )
	{
		d_start_coarse += coarse_factor * d_delta_fine;
		rwc_start_coarse += coarse_factor * rwc_delta;
	}

	coarse = lround( ( d_start_fine - d_start_coarse ) / d_delta_fine );

	/* Now, finally we got everything we need to draw the axis... */

	rwc_coarse = rwc_start_coarse - coarse_factor * rwc_delta;

	if ( coord == X )
	{
		/* Draw coloured line of scale */

		y = X_SCALE_OFFSET;
		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );
		XFillRectangle( G.d, c->pm, cv->gc, 0, y - 2, c->w, 3 );

		/* Draw all the ticks and numbers */

		for ( cur_p = d_start_fine; cur_p < c->w; 
			  medium++, coarse++, cur_p += d_delta_fine )
		{
			x = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				XDrawLine( G.d, c->pm, c->font_gc, x, y + 3,
						   x, y - LONG_TICK_LEN );
				rwc_coarse += coarse_factor * rwc_delta;
				if ( G.font == NULL )
					continue;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				if ( x - width / 2 - 10 > last )
				{
					XDrawString( G.d, c->pm, c->font_gc, x - width / 2,
								 y + LABEL_DIST + G.font_asc, lstr,
								 strlen( lstr ) );
					last = x + width / 2;
				}
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x, y - MEDIUM_TICK_LEN );
			else                                       /* short line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x, y - SHORT_TICK_LEN );
		}
	}
	else if ( coord == Y )
	{
		/* Draw coloured line of scale */

		x = c->w - Y_SCALE_OFFSET;
		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );
		XFillRectangle( G.d, c->pm, cv->gc, x, 0, 3, c->h );

		/* Draw all the ticks and numbers */

		for ( cur_p = ( double ) c->h - 1.0 - d_start_fine; cur_p >= 0; 
			  medium++, coarse++, cur_p -= d_delta_fine )
		{
			y = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				XDrawLine( G.d, c->pm, c->font_gc, x - 3, y,
						   x + LONG_TICK_LEN, y );
				rwc_coarse += coarse_factor * rwc_delta;

				if ( G.font == NULL )
					continue;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				XDrawString( G.d, c->pm, c->font_gc, x - LABEL_DIST - width,
							 y + G.font_asc / 2, lstr, strlen( lstr ) );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x + MEDIUM_TICK_LEN, y );
			else                                      /* short line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x + SHORT_TICK_LEN, y );
		}
	}
	else
	{
		/* Draw coloured line of scale */

		x = Z_SCALE_OFFSET;
		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );
		XFillRectangle( G.d, c->pm, cv->gc, x - 2, 0, 3, c->h );

		/* Draw all the ticks and numbers */

		for ( cur_p = ( double ) c->h - 1.0 - d_start_fine; cur_p >= 0; 
			  medium++, coarse++, cur_p -= d_delta_fine )
		{
			y = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				XDrawLine( G.d, c->pm, c->font_gc, x + 3, y,
						   x - LONG_TICK_LEN, y );
				rwc_coarse += coarse_factor * rwc_delta;

				if ( G.font == NULL )
					continue;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				XDrawString( G.d, c->pm, c->font_gc, x + LABEL_DIST,
							 y + G.font_asc / 2, lstr, strlen( lstr ) );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x - MEDIUM_TICK_LEN, y );
			else                                      /* short line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x - SHORT_TICK_LEN, y );
		}
	}
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void make_color_scale( Canvas *c, Curve_2d *cv )
{
	int i;
	unsigned int h;
	double col_inc;
	double h_inc;


	h = ( unsigned int ) ceil( ( double ) c->h / ( double ) NUM_COLORS );
	h_inc = ( double ) c->h / ( double ) NUM_COLORS;

	XDrawLine( G.d, c->pm, c->font_gc, Z_LINE_OFFSET, 0,
			   Z_LINE_OFFSET, c->h - 1 );
	XDrawLine( G.d, c->pm, c->font_gc, Z_LINE_OFFSET + Z_LINE_WIDTH + 1,
			   0, Z_LINE_OFFSET + Z_LINE_WIDTH + 1, c->h - 1 );

	col_inc = 1.0 / ( double ) ( NUM_COLORS - 1 );

	for ( i = 0; i < NUM_COLORS; i++ )
	{
		XSetForeground( G.d, cv->gc, d2color( i * col_inc ) );
		XFillRectangle( G.d, c->pm, cv->gc, Z_LINE_OFFSET + 1,
						d2shrt( c->h - ( i + 1 ) * h_inc ), Z_LINE_WIDTH, h );
	}
}
