/*
  $Id$
*/


#include "fsc2.h"

void press_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c );
void release_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c );
void motion_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c );
void save_scale_state( Curve_1d *cv );
bool change_x_range( Canvas *c );
bool change_y_range( Canvas *c );
bool change_xy_range( Canvas *c );
bool zoom_x( Canvas *c );
bool zoom_y( Canvas *c );
bool zoom_xy( Canvas *c );
void shift_XPoints_of_curve( Canvas *c, Curve_1d *cv );


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

			repaint_canvas( c );
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
					save_scale_state( cv );
				else
					cv->can_undo = UNSET;
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

			repaint_canvas( &G.canvas );
			break;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void release_handler( FL_OBJECT *obj, Window window, XEvent *ev, Canvas *c )
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
					scale_changed = change_x_range( c );
					break;

				case 2 :                       /* in y-axis window */
					scale_changed = change_y_range( c );
					break;

				case 3 :                       /* in canvas window */
					scale_changed = change_xy_range( c );
					break;
			}
			c->is_box = UNSET;
			break;

		case 2 :                               /* middle mouse button */
			if ( G.drag_canvas & 1 )
				redraw_canvas( &G.x_axis );
			if ( G.drag_canvas & 2 )
				redraw_canvas( &G.y_axis );
			break;

		case 4 :                               /* right mouse button */
			switch ( G.drag_canvas )
			{
				case 1 :                       /* in x-axis window */
					scale_changed = zoom_x( c );
					break;

				case 2 :                       /* in y-axis window */
					scale_changed = zoom_y( c );
					break;

				case 3 :                       /* in canvas window */
					scale_changed = zoom_xy( c );
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

		redraw_all( );
//		redraw_canvas( &G.canvas );
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

			repaint_canvas( c );
			break;

		case 2 :                               /* middle button */
			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];
						   
				if ( ! cv->active )
					continue;

				/* Recalculate the offsets and shift curves in the canvas */

				shift_XPoints_of_curve( c, cv );
				scale_changed = SET;
			}

			G.start[ X ] = c->ppos[ X ];
			G.start[ Y ] = c->ppos[ Y ];

			if ( G.is_fs && scale_changed )
			{
				G.is_fs = UNSET;
				fl_set_button( run_form->full_scale_button, 0 );
			}

			redraw_canvas( &G.canvas );
			break;

		case 3 : case 5 :               /* left and (middle or right) button */
			repaint_canvas( &G.canvas );
			break;
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void save_scale_state( Curve_1d *cv )
{
	cv->old_s2d[ X ] = cv->s2d[ X ];
	cv->old_s2d[ Y ] = cv->s2d[ Y ];
	cv->old_shift[ X ] = cv->shift[ X ];
	cv->old_shift[ Y ] = cv->shift[ Y ];

	cv->can_undo = SET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_x_range( Canvas *c )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double x1, x2;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			if ( ! cv->active )
				continue;

			save_scale_state( cv );

			x1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
			x2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

			cv->shift[ X ] = - d_min( x1, x2 );
			cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) / fabs( x1 - x2 );

			recalc_XPoints_of_curve( cv );
			scale_changed = SET;
		}
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_y_range( Canvas *c )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double y1, y2;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			if ( ! cv->active )
				continue;

			save_scale_state( cv );

			y1 = ( ( double ) G.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
				 - cv->shift[ Y ];
			y2 = ( ( double ) G.canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
				 - cv->shift[ Y ];

			cv->shift[ Y ] = - d_min( y1, y2 );
			cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 ) / fabs( y1 - y2 );

			recalc_XPoints_of_curve( cv );
			scale_changed = SET;
		}
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool change_xy_range( Canvas *c )
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
			recalc_XPoints_of_curve( cv );
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_x( Canvas *c )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double px;


	if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			if ( ! cv->active )
				continue;

			save_scale_state( cv );

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

			recalc_XPoints_of_curve( cv );
			scale_changed = SET;
		}
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_y( Canvas *c )
{
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;
	double py;


	if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			if ( ! cv->active )
				continue;

			save_scale_state( cv );

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

			recalc_XPoints_of_curve( cv );
			scale_changed = SET;
		}
	}

	return scale_changed;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

bool zoom_xy( Canvas *c )
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
			recalc_XPoints_of_curve( cv );
	}

	return scale_changed;
}


/*-----------------------------------------------------------------------*/
/* This is basically a simplified version of `recalc_XPoints_of_curve()' */
/* because we need to do much less calculations, i.e. just add an offset */
/* to all XPoints instead of going through all the scalings...           */
/*-----------------------------------------------------------------------*/

void shift_XPoints_of_curve( Canvas *c, Curve_1d *cv )
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
			if ( cv->xpoints[ k++ ].y >= ( int ) G.canvas.h )
				cv->down = SET;
		}
	}
}

