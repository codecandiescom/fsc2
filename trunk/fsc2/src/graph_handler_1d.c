/*
  $Id$
*/


#include "fsc2.h"

void press_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c );
void release_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c );
void motion_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c );
void save_scale_state( Curve_1d *cv );
bool change_x_range( int x_cur );
bool change_y_range( int y_cur );
bool change_xy_range( int x_cur, int y_cur );
bool zoom_x( int x_cur );
bool zoom_y( int y_cur );
bool zoom_xy( int x_cur, int cur_y );


/*--------------------------------------------------------*/
/* Handler for all kinds of X events the canvas receives. */
/*--------------------------------------------------------*/

int canvas_handler( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					void *udata )
{
	Canvas *c = ( Canvas * ) udata;


	obj = obj;
	window = window;

	switch ( ev->type )
    {
        case Expose :
			repaint_canvas( c );
            break;

		case ConfigureNotify :
            if ( ( int ) c->w == w && ( int ) c->h == h )
				break;
            reconfigure_window( c, w, h );
            break;

		case ButtonPress :
			press_handler( obj, window, ev, c );
			break;

		case ButtonRelease :
			release_handler( obj, window, ev, c );
			break;

		case MotionNotify :
			motion_handler( obj, window, ev, c );
			break;
	}

	return 1;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void press_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
{
	long i;
	Curve_1d *cv;
	int old_button_state = G.button_state;
	int x_cur, y_cur, dummy;


	/* In the axes areas two buttons pressed simultaneously doesn't has a
	special meaning, so don't care about another button. Also don't react if
	the pressed buttons have lost there meaning */

	if ( ( c != &G.canvas && G.real_button_state != 0 ) ||
		 ( G.button_state == 0 && G.real_button_state != 0 ) )
	{
		G.real_button_state |= 1 << ( ev->xbutton.button - 1 );
		return;
	}

	G.real_button_state |= ( 1 << ( ev->xbutton.button - 1 ) );

	/* Middle and right or all three buttons at once don't mean a thing */

	if ( G.real_button_state >= 6 )
		return;

	G.button_state |= ( 1 << ( ev->xbutton.button - 1 ) );

	/* Find out which window gets the mouse events (all following mouse events
	   go to this windw until all buttons are released) */
	   

	if ( obj == run_form->x_axis )        /* in x-axis window */
		G.drag_canvas = 1;
	if ( obj == run_form->y_axis )        /* in y-axis window */
		G.drag_canvas = 2;
	if ( obj == run_form->canvas )        /* in canvas window */
		G.drag_canvas = 3;

	fl_get_win_mouse( window, &x_cur, &y_cur, &dummy );

	switch ( G.button_state )
	{
		case 1 :                               /* left button */
			fl_set_cursor( window, G.cur_1 );

			G.x_start = x_cur;
			G.y_start = y_cur;

			/* Set up variables for drawing the rubber boxes */

			switch ( G.drag_canvas )
			{
				case 1 :                       /* in x-axis window */
					c->box_x = x_cur;
					c->box_w = 0;
					c->box_y = c->h / 2 - 1;
					c->box_h = 2;
					c->is_box = SET;
					break;

				case 2 :                       /* in y-axis window */
					c->box_x = c->w / 2 - 1;
					c->box_y = y_cur;
					c->box_w = 2;
					c->box_h = 0;
					c->is_box = SET;
					break;

				case 3 :                       /* in canvas window */
					c->box_x = x_cur;
					c->box_y = y_cur;
					c->box_w = c->box_h = 0;
					c->is_box = SET;
					break;
			}

			repaint_canvas( c );
			break;

		case 2 :                               /* middle button */
			fl_set_cursor( window, G.cur_2 );

			G.x_start = x_cur;
			G.y_start = y_cur;

			/* Store data for undo operation */

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				if ( ! cv->active )
				{
					cv->can_undo = UNSET;
					continue;
				}
				else
					save_scale_state( cv );
			}
			break;

		case 3:                                /* left and middle button */
			fl_set_cursor( window, G.cur_4 );

			/* Don't draw the box anymore */

			G.canvas.is_box = UNSET;
			repaint_canvas( &G.canvas );
			break;

		case 4 :                               /* right button */
			fl_set_cursor( window, G.cur_3 );

			G.x_start = x_cur;
			G.y_start = y_cur;
			break;

		case 5 :                               /* left and right button */
			fl_set_cursor( window, G.cur_5 );

			if ( G.canvas.is_box == UNSET && old_button_state != 4 )
			{
				G.x_start = x_cur;
				G.y_start = y_cur;
			}
			else
				G.canvas.is_box = UNSET;

			repaint_canvas( &G.canvas );
			break;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void release_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
{
	int x_cur, y_cur, dummy;
	bool scale_changed = UNSET;


	obj = obj;

	/* If the released button didn't had a meaning forget about it */

	if ( ! ( ( 1 << ( ev->xbutton.button - 1 ) ) & G.button_state ) )
	{
		G.real_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );
		return;
	}

	/* Get mouse posiion and restrict it to the canvas */

	fl_get_win_mouse( window, &x_cur, &y_cur, &dummy );

	if ( x_cur < 0 )
		x_cur = 0;
	if ( x_cur >= ( int ) G.canvas.w )
		x_cur = G.canvas.w - 1;

	if ( y_cur < 0 )
		y_cur = 0;
	if ( y_cur >= ( int ) G.canvas.h )
		y_cur = G.canvas.h - 1;


	fl_reset_cursor( window );

	switch ( G.button_state )
	{
		case 1 :                               /* left mouse */
			switch ( G.drag_canvas )
			{
				case 1 :                       /* x-axis window */
					scale_changed = change_x_range( x_cur );
					break;

				case 2 :                       /* in y-axis window */
					scale_changed = change_y_range( y_cur );
					break;

				case 3 :                       /* in canvas window */
					scale_changed = change_xy_range( x_cur, y_cur );
			}
			c->is_box = UNSET;
			break;

		case 4 :                               /* right button */
			switch ( G.drag_canvas )
			{
				case 1 :                       /* in x-axis window */
					scale_changed = zoom_x( x_cur );
					break;

				case 2 :                       /* in y-axis window */
					scale_changed = zoom_y( y_cur );
					break;

				case 3 :                       /* in canvas window */
					scale_changed = zoom_xy( x_cur, y_cur );
					break;
			}
			break;
	}

	G.button_state = 0;
	G.real_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );

	if ( scale_changed )
	{
		if ( G.is_fs )
		{
			G.is_fs = UNSET;
			fl_set_button( run_form->full_scale_button, 0 );
		}			

		redraw_canvas( &G.canvas );
	}

	if ( ! scale_changed || c != &G.canvas )
		repaint_canvas( c );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void motion_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
{
	Curve_1d *cv;
	XEvent new_ev;
	long i;
	int x_cur, y_cur, dummy;
	bool scale_changed = UNSET;
	double factor = G.real_button_state == 6 ? 5.0 : 1.0;


	
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

	fl_get_win_mouse( window, &x_cur, &y_cur, &dummy );

	switch ( G.button_state )
	{
		case 1 :                               /* left mouse button */
			if ( G.drag_canvas & 1 )           /* x-axis or canvas window */
			{
				c->box_w = x_cur - G.x_start;

				if ( c->box_x + c->box_w >= ( int ) c->w )
					c->box_w = c->w - c->box_x - 1;

				if ( c->box_x + c->box_w < 0 )
					c->box_w = - c->box_x;
			}

			if ( G.drag_canvas & 2 )           /* y-axis or canvas window */
			{
				c->box_h = y_cur - G.y_start ;

				if ( c->box_y + c->box_h >= ( int ) c->h )
					c->box_h = c->h - c->box_y - 1;

				if ( c->box_y + c->box_h < 0 )
					c->box_h = - c->box_y;
			}

			repaint_canvas( c );
			break;

		case 2 :                               /* middle button */
			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];
						   
				if ( ! cv->active )
					continue;

				if ( G.drag_canvas & 1 )       /* x-axis or canvas window */
					cv->x_shift += factor * ( x_cur - G.x_start ) / cv->s2d_x;
				if ( G.drag_canvas & 2 )       /* y-axis or canvas window */
					cv->y_shift -= factor * ( y_cur - G.y_start ) / cv->s2d_y;

				recalc_XPoints_of_curve( cv );
				scale_changed = SET;
			}

			G.x_start = x_cur;
			G.y_start = y_cur;

			if ( G.is_fs && scale_changed )
			{
				G.is_fs = UNSET;
				fl_set_button( run_form->full_scale_button, 0 );
			}

			redraw_canvas( &G.canvas );
			break;

		case 3 : case 5 :                 /* left and middle or right button */
			repaint_canvas( &G.canvas );
			break;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void save_scale_state( Curve_1d *cv )
{
	cv->old_s2d_x = cv->s2d_x;
	cv->old_s2d_y = cv->s2d_y;
	cv->old_x_shift = cv->x_shift;
	cv->old_y_shift = cv->y_shift;

	cv->can_undo = SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_x_range( int x_cur )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double x1, x2;


	if ( abs( G.x_start - x_cur ) > 4 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			if ( ! cv->active )
				continue;

			save_scale_state( cv );

			x1 = G.x_start / cv->s2d_x - cv->x_shift;
			x2 = x_cur / cv->s2d_x - cv->x_shift;

			cv->x_shift = - d_min( x1, x2 );
			cv->s2d_x = ( double ) ( G.canvas.w - 1 ) / fabs( x1 - x2 );

			recalc_XPoints_of_curve( cv );
			scale_changed = SET;
		}
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_y_range( int y_cur )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double y1, y2;


	if ( abs( G.y_start - y_cur ) > 4 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			if ( ! cv->active )
				continue;

			save_scale_state( cv );

			y1 = 1.0 - G.y_start / cv->s2d_y - cv->y_shift;
			y2 = 1.0 - y_cur / cv->s2d_y - cv->y_shift;

			cv->y_shift = 1.0 - d_max( y1, y2 );
			cv->s2d_y = ( double ) ( G.canvas.h - 1 ) / fabs( y1 - y2 );

			recalc_XPoints_of_curve( cv );
			scale_changed = SET;
		}
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_xy_range( int x_cur, int y_cur )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double x1, x2, y1, y2;


	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		if ( ! cv->active )
			continue;

		save_scale_state( cv );
		cv->can_undo = UNSET;

		if ( abs( G.x_start - x_cur ) > 4 )
		{
			cv->can_undo = SET;

			x1 = G.x_start / cv->s2d_x - cv->x_shift;
			x2 = x_cur / cv->s2d_x - cv->x_shift;

			cv->x_shift = - d_min( x1, x2 );
			cv->s2d_x = ( double ) ( G.canvas.w - 1 ) / fabs( x1 - x2 );

			scale_changed = SET;
		}

		if ( abs( G.y_start - y_cur ) > 4 )
		{
			cv->can_undo = SET;

			y1 = 1.0 - G.y_start / cv->s2d_y - cv->y_shift;
			y2 = 1.0 - y_cur / cv->s2d_y - cv->y_shift;

			cv->y_shift = 1.0 - d_max( y1, y2 );
			cv->s2d_y = ( double ) ( G.canvas.h - 1 ) / fabs( y1 - y2 );

			scale_changed = SET;
		}

		if ( scale_changed )
			recalc_XPoints_of_curve( cv );
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_x( int x_cur )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double px;


	if ( abs( G.x_start - x_cur ) > 4 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			if ( ! cv->active )
				continue;

			save_scale_state( cv );

			px = G.x_start / cv->s2d_x - cv->x_shift;
			if ( G.x_start > x_cur )
				cv->s2d_x *= d_min( 4.0,
							     1.0 + 3.0 * ( double ) ( G.x_start - x_cur ) /
								                       ( double ) G.x_axis.w );
			else
				cv->s2d_x /= d_min( 4.0,
								 1.0 + 3.0 * ( double ) ( x_cur - G.x_start ) /
								                       ( double ) G.x_axis.w );

			cv->x_shift = G.x_start / cv->s2d_x - px;

			recalc_XPoints_of_curve( cv );
			scale_changed = SET;
		}
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_y( int y_cur )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double py;


	if ( abs( G.y_start - y_cur ) > 4 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			if ( ! cv->active )
				continue;

			save_scale_state( cv );

			py = 1.0 - G.y_start / cv->s2d_y - cv->y_shift;
			if ( G.y_start < y_cur )
				cv->s2d_y *= d_min( 4.0,
								 1.0 + 3.0 * ( double ) ( y_cur - G.y_start ) /
								                       ( double ) G.y_axis.h );
			else
				cv->s2d_y /= d_min( 4.0,
								 1.0 + 3.0 * ( double ) ( G.y_start - y_cur ) /
								                       ( double ) G.y_axis.h );

			cv->y_shift = 1.0 - G.y_start / cv->s2d_y - py;

			recalc_XPoints_of_curve( cv );
			scale_changed = SET;
		}
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_xy( int x_cur, int y_cur )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double px, py;


	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		if ( ! cv->active )
			continue;

		save_scale_state( cv );
		cv->can_undo = UNSET;

		if ( abs( G.x_start - x_cur ) > 4 )
		{
			cv->can_undo = SET;

			px = G.x_start / cv->s2d_x - cv->x_shift;

			if ( G.x_start > x_cur )
				cv->s2d_x *= d_min( 4.0,
								 1.0 + 3.0 * ( double ) ( G.x_start - x_cur ) /
								                       ( double ) G.x_axis.w );
			else
				cv->s2d_x /= d_min( 4.0,
								 1.0 + 3.0 * ( double ) ( x_cur - G.x_start ) /
								                       ( double ) G.x_axis.w );

			cv->x_shift = G.x_start / cv->s2d_x - px;

			scale_changed = SET;
		}

		if ( abs( G.y_start - y_cur ) > 4 )
		{
			cv->can_undo = SET;

			py = 1.0 - G.y_start / cv->s2d_y - cv->y_shift;
			if ( G.y_start < y_cur )
				cv->s2d_y *= d_min( 4.0,
								 1.0 + 3.0 * ( double ) ( y_cur - G.y_start ) /
								                       ( double ) G.y_axis.h );
			else
				cv->s2d_y /= d_min( 4.0,
								 1.0 + 3.0 * ( double ) ( G.y_start - y_cur ) /
								                       ( double ) G.y_axis.h );

			cv->y_shift = 1.0 - G.y_start / cv->s2d_y - py;

			scale_changed = SET;
		}
		
		if ( scale_changed )
			recalc_XPoints_of_curve( cv );
	}

	return scale_changed;
}
