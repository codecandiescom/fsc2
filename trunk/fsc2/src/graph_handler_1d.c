/*
  $Id$
*/


#include "fsc2.h"

void press_handler_1d( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c );
void release_handler_1d( FL_OBJECT *obj, Window window, XEvent *ev,
						 Canvas *c );
void motion_handler_1d( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c );
void save_scale_state_1d( Curve_1d *cv );
bool change_x_range_1d( Canvas *c );
bool change_y_range_1d( Canvas *c );
bool change_xy_range_1d( Canvas *c );
bool zoom_x_1d( Canvas *c );
bool zoom_y_1d( Canvas *c );
bool zoom_xy_1d( Canvas *c );
void shift_XPoints_of_curve_1d( Canvas *c, Curve_1d *cv );


/*--------------------------------------------------------*/
/* Handler for all kinds of X events the canvas receives. */
/*--------------------------------------------------------*/

int canvas_handler_1d( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					   void *udata )
{
	Canvas *c = ( Canvas * ) udata;


	obj = obj;
	window = window;

	switch ( ev->type )
    {
        case Expose :
            if ( ev->xexpose.count == 0 )     /* only react to last in queue */
				repaint_canvas_1d( c );
            break;

		case ConfigureNotify :
            if ( ( int ) c->w == w && ( int ) c->h == h )
				break;
            reconfigure_window_1d( c, w, h );
            break;

		case ButtonPress :
			press_handler_1d( obj, window, ev, c );
			break;

		case ButtonRelease :
			release_handler_1d( obj, window, ev, c );
			break;

		case MotionNotify :
			motion_handler_1d( obj, window, ev, c );
			break;
	}

	return 1;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void press_handler_1d( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
{
	long i;
	Curve_1d *cv;
	int old_button_state = G.button_state;
	int dummy;


	/* In the axes areas two buttons pressed simultaneously doesn't has a
	special meaning, so don't care about another button. Also don't react if
	the pressed buttons have lost there meaning */

	if ( ( c != &G.canvas && G.raw_button_state != 0 ) ||
		 ( G.button_state == 0 && G.raw_button_state != 0 ) )
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
	   go to this windw until all buttons are released) */
	   

	if ( obj == run_form->x_axis )        /* in x-axis window */
		G.drag_canvas = 1;
	if ( obj == run_form->y_axis )        /* in y-axis window */
		G.drag_canvas = 2;
	if ( obj == run_form->canvas )        /* in canvas window */
		G.drag_canvas = 3;

	fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &dummy );

	switch ( G.button_state )
	{
		case 1 :                               /* left button */
			fl_set_cursor( window, G.cur_1 );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			/* Set up variables for drawing the rubber boxes */

			switch ( G.drag_canvas )
			{
				case 1 :                       /* in x-axis window */
					c->box_x = c->ppos[ X ];
					c->box_w = 0;
					c->box_y = 21;
					c->box_h = 5;
					c->is_box = SET;
					break;

				case 2 :                       /* in y-axis window */
					c->box_x = c->w - 27;
					c->box_y = c->ppos[ Y ];
					c->box_w = 5;
					c->box_h = 0;
					c->is_box = SET;
					break;

				case 3 :                       /* in canvas window */
					c->box_x = c->ppos[ X ];
					c->box_y = c->ppos[ Y ];
					c->box_w = c->box_h = 0;
					c->is_box = SET;
					break;
			}

			repaint_canvas_1d( c );
			break;

		case 2 :                               /* middle button */
			fl_set_cursor( window, G.cur_2 );

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			/* Store data for undo operation */

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				if ( cv->active )
					save_scale_state_1d( cv );
				else
					cv->can_undo = UNSET;
			}
			break;

		case 3:                                /* left and middle button */
			fl_set_cursor( window, G.cur_4 );

			/* Don't draw the box anymore */

			G.canvas.is_box = UNSET;
			repaint_canvas_1d( &G.canvas );
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

			repaint_canvas_1d( &G.canvas );
			break;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void release_handler_1d( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
{
	int dummy;
	bool scale_changed = UNSET;


	obj = obj;

	/* If the released button didn't has a meaning just clear it from the
	   button state pattern and then forget about it */

	if ( ! ( ( 1 << ( ev->xbutton.button - 1 ) ) & G.button_state ) )
	{
		G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );
		return;
	}

	/* Get mouse position and restrict it to the canvas */

	fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &dummy );

	if ( c->ppos[ X ] < 0 )
		c->ppos[ X ] = 0;
	if ( c->ppos[ X ] >= ( int ) G.canvas.w )
		c->ppos[ X ] = G.canvas.w - 1;

	if ( c->ppos[ Y ] < 0 )
		c->ppos[ Y ] = 0;
	if ( c->ppos[ Y ] >= ( int ) G.canvas.h )
		c->ppos[ Y ] = G.canvas.h - 1;

	switch ( G.button_state )
	{
		case 1 :                               /* left mouse button */
			switch ( G.drag_canvas )
			{
				case 1 :                       /* x-axis window */
					scale_changed = change_x_range_1d( c );
					break;

				case 2 :                       /* in y-axis window */
					scale_changed = change_y_range_1d( c );
					break;

				case 3 :                       /* in canvas window */
					scale_changed = change_xy_range_1d( c );
					break;
			}
			c->is_box = UNSET;
			break;

		case 2 :                               /* middle mouse button */
			if ( G.drag_canvas & 1 )
				redraw_canvas_1d( &G.x_axis );
			if ( G.drag_canvas & 2 )
				redraw_canvas_1d( &G.y_axis );
			break;

		case 4 :                               /* right mouse button */
			switch ( G.drag_canvas )
			{
				case 1 :                       /* in x-axis window */
					scale_changed = zoom_x_1d( c );
					break;

				case 2 :                       /* in y-axis window */
					scale_changed = zoom_y_1d( c );
					break;

				case 3 :                       /* in canvas window */
					scale_changed = zoom_xy_1d( c );
					break;
			}
			break;
	}

	G.button_state = 0;
	G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );

	fl_reset_cursor( window );

	if ( scale_changed )
	{
		if ( G.is_fs )
		{
			G.is_fs = UNSET;
			fl_set_button( run_form->full_scale_button, 0 );
		}			

		redraw_all_1d( );
	}

	if ( ! scale_changed || c != &G.canvas )
		repaint_canvas_1d( c );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void motion_handler_1d( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
{
	Curve_1d *cv;
	XEvent new_ev;
	long i;
	int dummy;
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

	fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &dummy );

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

			repaint_canvas_1d( c );
			break;

		case 2 :                               /* middle button */
			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];
						   
				if ( ! cv->active )
					continue;

				/* Recalculate the offsets and shift curves in the canvas */

				shift_XPoints_of_curve_1d( c, cv );
				scale_changed = SET;
			}

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			if ( G.is_fs && scale_changed )
			{
				G.is_fs = UNSET;
				fl_set_button( run_form->full_scale_button, 0 );
			}

			redraw_canvas_1d( &G.canvas );
			if ( G.drag_canvas & 1 )
				redraw_canvas_1d( &G.x_axis );
			if ( G.drag_canvas & 2 )
				redraw_canvas_1d( &G.y_axis );
			break;

		case 3 : case 5 :               /* left and (middle or right) button */
			repaint_canvas_1d( &G.canvas );
			break;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void save_scale_state_1d( Curve_1d *cv )
{
	cv->old_s2d[ X ] = cv->s2d[ X ];
	cv->old_s2d[ Y ] = cv->s2d[ Y ];
	cv->old_shift[ X ] = cv->shift[ X ];
	cv->old_shift[ Y ] = cv->shift[ Y ];

	cv->can_undo = SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_x_range_1d( Canvas *c )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double x1, x2;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 )
		return UNSET;

	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		if ( ! cv->active )
			continue;

		save_scale_state_1d( cv );

		x1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
		x2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

		cv->shift[ X ] = - d_min( x1, x2 );
		cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) / fabs( x1 - x2 );

		recalc_XPoints_of_curve_1d( cv );
		scale_changed = SET;
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_y_range_1d( Canvas *c )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double y1, y2;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 )
		return UNSET;

	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		if ( ! cv->active )
			continue;

		save_scale_state_1d( cv );

		y1 = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
			 - cv->shift[ Y ];
		y2 = ( ( double ) G.canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
			 - cv->shift[ Y ];

		cv->shift[ Y ] = - d_min( y1, y2 );
		cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 ) / fabs( y1 - y2 );

		recalc_XPoints_of_curve_1d( cv );
		scale_changed = SET;
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_xy_range_1d( Canvas *c )
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

		save_scale_state_1d( cv );
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
			recalc_XPoints_of_curve_1d( cv );
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_x_1d( Canvas *c )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double px;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 )
		return UNSET;

	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		if ( ! cv->active )
			continue;

		save_scale_state_1d( cv );

		px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

		/* If the mouse was moved to lower values zoom the display by a factor
		   of up to 4 (if the mouse was moved over the whole length of the
		   scale) while keeping the point the move was started at the same
		   position. If the mouse was movep upwards demagnify by the inverse
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

		recalc_XPoints_of_curve_1d( cv );
		scale_changed = SET;
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_y_1d( Canvas *c )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double py;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 )
		return UNSET;

	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		if ( ! cv->active )
			continue;

		save_scale_state_1d( cv );

		/* Get the value in the interval [0, 1] corresponding to the mouse
		   posaition */

		py = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
			 - cv->shift[ Y ];

		/* If the mouse was moved to lower values zoom the display by a factor
		   of up to 4 (if the mouse was moved over the whole length of the
		   scale) while keeping the point the move was started at the same
		   position. If the mouse was movep upwards demagnify by the inverse
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

		recalc_XPoints_of_curve_1d( cv );
		scale_changed = SET;
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_xy_1d( Canvas *c )
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

		save_scale_state_1d( cv );
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
			recalc_XPoints_of_curve_1d( cv );
	}

	return scale_changed;
}


/*-----------------------------------------------------------------------*/
/* This is basically a simplified version of `recalc_XPoints_of_curve()' */
/* because we need to do much less calculations, i.e. just adding an     */
/* offset to all XPoints instead of going through all the scalings...    */
/*-----------------------------------------------------------------------*/

void shift_XPoints_of_curve_1d( Canvas *c, Curve_1d *cv )
{
	long j, k;
	int dx = 0,
		dy = 0;
	int factor;


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

	for ( k = 0, j = 0; j < G.nx; j++ )
	{
		if ( cv->points[ j ].exist )
		{
			cv->xpoints[ k ].x = i2shrt( cv->xpoints[ k ].x + dx );
			cv->xpoints[ k ].y = i2shrt( cv->xpoints[ k ].y + dy );

			if ( cv->xpoints[ k ].x < 0 )
				cv->left = SET;
			if ( cv->xpoints[ k ].x >= ( int ) G.canvas.w )
				cv->right = SET;
			if ( cv->xpoints[ k ].y < 0 )
				cv->up = SET;
			if ( cv->xpoints[ k ].y >= ( int ) G.canvas.h )
				cv->down = SET;

			k++;
		}
	}
}


/*-------------------------------------*/
/* Handles changes of the window size. */
/*-------------------------------------*/

void reconfigure_window_1d( Canvas *c, int w, int h )
{
	long i;
	Curve_1d *cv;
	static bool is_reconf[ 2 ] = { UNSET, UNSET };
	static bool need_redraw[ 2 ] = { UNSET, UNSET };
	int old_w = c->w,
		old_h = c->h;


	/* Set the new canvas sizes */

	c->w = ( unsigned int ) w;
	c->h = ( unsigned int ) h;

	/* Calculate the new scale factors */

	if ( c == &G.canvas && G.is_scale_set )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			cv->s2d[ X ] *= ( double ) ( w - 1 ) / ( double ) ( old_w - 1 );
			cv->s2d[ Y ] *= ( double ) ( h - 1 ) / ( double ) ( old_h - 1 );

			if ( cv->can_undo )
			{
				cv->old_s2d[ X ] *=
					           ( double ) ( w - 1 ) / ( double ) ( old_w - 1 );
				cv->old_s2d[ Y ] *=
					           ( double ) ( h - 1 ) / ( double ) ( old_h - 1 );
			}
		}

		/* Recalculate data for drawing (has to be done after setting of canvas
		   sizes since they are needed in the recalculation) */

		recalc_XPoints_1d( );
	}


	/* We can't know the sequence the different canvases are reconfigured in
	   but, on the other hand, redrawing an axis canvas is useless before the
	   new scaling factors are set. Thus we need in the call for the canvas
	   window to redraw also axis windows which got reconfigured before. */


	delete_pixmap( c );
	create_pixmap( c );

	if ( c == &G.canvas )
	{
		redraw_canvas_1d( c );

		if ( need_redraw[ X ] )
		{
			redraw_canvas_1d( &G.x_axis );
			need_redraw[ X ] = UNSET;
		}
		else if ( w != old_w )
			is_reconf[ X ] = SET;

		if ( need_redraw[ Y ] )
		{
			redraw_canvas_1d( &G.y_axis );
			need_redraw[ Y ] = UNSET;
		}
		else if ( h != old_h )
			is_reconf[ Z ] = SET;
	}

	if ( c == &G.x_axis )
	{
		if ( is_reconf[ X ] )
		{
			redraw_canvas_1d( c );
			is_reconf[ X ] = UNSET;
		}
		else
			need_redraw[ X ] = SET;
	}

	if ( c == &G.y_axis )
	{
		if ( is_reconf[ Y ] )
		{
			redraw_canvas_1d( c );
			is_reconf[ Y ] = UNSET;
		}
		else
			need_redraw[ Y ] = SET;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void recalc_XPoints_1d( void )
{
	long i;


	for ( i = 0; i < G.nc; i++ )
		recalc_XPoints_of_curve_1d( G.curve[ i ] );
}


/*-----------------------------------------------------------------*/
/* Recalculates the graphics data for a curve using the the curves */
/* settings for the scale and the offset.                          */
/*-----------------------------------------------------------------*/

void recalc_XPoints_of_curve_1d( Curve_1d *cv )
{
	long j, k;


	cv->up = cv->down = cv->left = cv->right = UNSET;

	for ( k = j = 0; j < G.nx; j++ )
	{
		if ( cv->points[ j ].exist )
		{
			cv->xpoints[ k ].x = d2shrt( cv->s2d[ X ]
										            * ( j + cv->shift[ X ] ) );
			cv->xpoints[ k ].y = ( short ) G.canvas.h - 1 - 
			   d2shrt( cv->s2d[ Y ] * ( cv->points[ j ].v + cv->shift[ Y ] ) );

			if ( cv->xpoints[ k ].x < 0 )
				cv->left = SET;
			if ( cv->xpoints[ k ].x >= ( int ) G.canvas.w )
				cv->right = SET;
			if ( cv->xpoints[ k ].y < 0 )
				cv->up = SET;
			if ( cv->xpoints[ k ].y >= ( int ) G.canvas.h )
				cv->down = SET;

			k++;
		}
	}
}


/*-----------------------------------------*/
/* Does a complete redraw of all canvases. */
/*-----------------------------------------*/

void redraw_all_1d( void )
{
	redraw_canvas_1d( &G.canvas );
	redraw_canvas_1d( &G.x_axis );
	redraw_canvas_1d( &G.y_axis );
}


/*-------------------------------------*/
/* Does a complete redraw of a canvas. */
/*-------------------------------------*/

void redraw_canvas_1d( Canvas *c )
{
	long i;
	Curve_1d *cv;


	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

	if ( G.is_init )
	{
		if ( c == &G.canvas && G.is_scale_set )
		{
			/* Firt draw all curves */

			for ( i = G.nc - 1 ; i >= 0; i-- )
			{
				cv = G.curve[ i ];

				if ( cv->count <= 1 )
					continue;

				XDrawLines( G.d, c->pm, cv->gc, cv->xpoints, cv->count, 
							CoordModeOrigin );
			}

			/* Now draw the out of range arrows */

			for ( i = 0 ; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				if ( cv->count <= 1 )
					continue;

				if ( cv->up )
					XCopyArea( G.d, cv->up_arr, c->pm, c->gc,
							   0, 0, G.ua_w, G.ua_h,
							   G.canvas.w / 2 - 32 + 16 * i, 5 );

				if ( cv->down )
					XCopyArea( G.d, cv->down_arr, c->pm, c->gc,
							   0, 0, G.da_w, G.da_h,
							   G.canvas.w / 2 - 32 + 16 * i,
							   G.canvas.h - 5 - G.da_h );

				if ( cv->left )
					XCopyArea( G.d, cv->left_arr, c->pm, c->gc,
							   0, 0, G.la_w, G.la_h,
							   5, G.canvas.h / 2 -32 + 16 * i );

				if ( cv->right )
					XCopyArea( G.d, cv->right_arr, c->pm, c->gc,
							   0, 0, G.ra_w, G.ra_h, G.canvas.w - 5- G.ra_w,
							   G.canvas.h / 2 - 32 + 16 * i );
			}
		}

		if ( c == &G.x_axis )
			redraw_axis( X );

		if ( c == &G.y_axis )
			redraw_axis( Y );
	}

	/* Finally copy the pixmap onto the screen */

	repaint_canvas_1d( c );
}


/*-----------------------------------------------*/
/* Copies the background pixmap onto the canvas. */
/*-----------------------------------------------*/

void repaint_canvas_1d( Canvas *c )
{
	static int i;
	char buf[ 256 ];
	int x, y;
	unsigned int w, h;
	Curve_1d *cv;
	double x_pos, y_pos;


	/* If no or either the middle or the left button is pressed no extra stuff
	   has to be drawn so just copy the pixmp with the curves into the
	   window. Also in the case that the graphics was never initialized this
	   is all to be done. */

	if ( ! ( G.button_state & 1 ) || ! G.is_init )
	{
		XCopyArea( G.d, c->pm, FL_ObjWin( c->obj ), c->gc,
				   0, 0, c->w, c->h, 0, 0 );
		return;
	}

	/* Otherwise use another level of buffering and copy the pixmap with
	   the curves into another pixmap */

	XCopyArea( G.d, c->pm, G.pm, c->gc, 0, 0, c->w, c->h, 0, 0 );

	/* Draw the rubber box if needed (i.e. when the left button pressed
	   in the canvas currently to be drawn) */

	if ( G.button_state == 1 && c->is_box )
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

		XDrawRectangle( G.d, G.pm, c->box_gc, x, y, w, h );
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
			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				x_pos = G.rwc_start[ X ] + G.rwc_delta[ X ]
					        * ( c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ] );
				y_pos = G.rwc_start[ Y ] + G.rwc_delta[ Y ]
					       * ( ( ( double ) G.canvas.h - 1.0 - c->ppos[ Y ] ) /
									           cv->s2d[ Y ] - cv->shift[ Y ] );

				make_label_string( buf, x_pos, ( int ) floor( log10( fabs(
					G.rwc_delta[ X ] ) / cv->s2d[ X ] ) ) - 2 );
				strcat( buf, ", " ); 
				make_label_string( buf + strlen( buf ), y_pos,
								   ( int ) floor( log10( fabs(
								   G.rwc_delta[ Y ] ) / cv->s2d[ Y ] ) ) - 2 );

				if ( G.font != NULL )
					XDrawImageString( G.d, G.pm, cv->font_gc, 5,
									  ( G.font_asc + 3 ) * ( i + 1 ) +
									  G.font_desc * i + 2,
									  buf, strlen( buf ) );
			}
		}

		if ( G.button_state == 5 )
		{
			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				x_pos = G.rwc_delta[ X ] * ( c->ppos[ X ] - G.start[ X ] ) /
					                                              cv->s2d[ X ];
				y_pos = G.rwc_delta[ Y ] * ( c->ppos[ Y ] - G.start[ Y ] ) /
					                                              cv->s2d[ Y ];

				sprintf( buf, "%#g, %#g", x_pos, y_pos );
				if ( G.font != NULL )
					XDrawImageString( G.d, G.pm, cv->font_gc, 5,
									  ( G.font_asc + 3 ) * ( i + 1 ) +
									  G.font_desc * i + 2,
									  buf, strlen( buf ) );
			}

			XDrawArc( G.d, G.pm, G.curve[ 0 ]->gc,
					  G.start[ X ] - 5, G.start[ Y ] - 5, 10, 10, 0, 23040 );

			XDrawLine( G.d, G.pm, c->box_gc, G.start[ X ], G.start[ Y ],
					   c->ppos[ X ], G.start[ Y ] );
			XDrawLine( G.d, G.pm, c->box_gc, c->ppos[ X ], G.start[ Y ],
					   c->ppos[ X ], c->ppos[ Y ] );
		}
	}

	/* Finally copy the buffer pixmap onto the screen */

	XCopyArea( G.d, G.pm, FL_ObjWin( c->obj ), c->gc,
			   0, 0, c->w, c->h, 0, 0 );
	XFlush( G.d );
}


/*---------------------------------------------------------*/
/* Does a rescale of the data for 1d graphics so that all  */
/* curves fit into the canvas and occupy the whole canvas. */
/*---------------------------------------------------------*/

void fs_rescale_1d( void )
{
	long i, j, k;
	double min = 1.0,
		   max = 0.0;
	double rw_y_min,
		   rw_y_max;
	double data;
	double new_rwc_delta_y;
	Curve_1d *cv;


	if ( ! G.is_scale_set )
		return;

	/* Find minimum and maximum value of all scaled data */

	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		for ( j = 0; j < G.nx; j++ )
			if ( cv->points[ j ].exist )
			{
				data = cv->points[ j ].v;
				max = d_max( data, max );
				min = d_min( data, min );
			}
	}

	/* If there are no points yet... */

	if ( min == 1.0 && max == 0.0 )
	{
		G.rw_y_min = HUGE_VAL;
		G.rw_y_max = - HUGE_VAL;
		G.is_scale_set = UNSET;
		return;
	}

	/* Calculate new real world maximum and minimum */

	rw_y_min = G.rwc_delta[ Y ] * min + G.rw_y_min;
	rw_y_max = G.rwc_delta[ Y ] * max + G.rw_y_min;

	/* Calculate new scaling factor and rescale the scaled data as well as the
	   points for drawing */

	new_rwc_delta_y = rw_y_max - rw_y_min;

	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		cv->shift[ X ] = cv->shift[ Y ] = 0.0;
		cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
		cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 );

		cv->up = cv->down = cv->left = cv->right = UNSET;

		for ( k = 0, j = 0; j < G.nx; j++ )
			if ( cv->points[ j ].exist )
				cv->points[ j ].v = ( G.rwc_delta[ Y ] * cv->points[ j ].v 
								   + G.rw_y_min - rw_y_min ) / new_rwc_delta_y;

		recalc_XPoints_of_curve_1d( cv );
	}

	/* Store new minimum and maximum and the new scale factor */

	G.rwc_delta[ Y ] = new_rwc_delta_y;
	G.rw_y_min = G.rwc_start[ Y ] = rw_y_min;
	G.rw_y_max = rw_y_max;
}
