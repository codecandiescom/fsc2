/*
  $Id$
*/


#include "fsc2.h"

#include "c1.xbm"             /* bitmaps for cursors */
#include "c2.xbm"
#include "c3.xbm"
#include "c4.xbm"
#include "c5.xbm"

#include "ua.xbm"             /* up, down, left and right arrow bitmaps */
#include "da.xbm"
#include "la.xbm"
#include "ra.xbm"


static void G_struct_init( void );
static void setup_canvas( Canvas *c, FL_OBJECT *obj );
static void canvas_off( Canvas *c, FL_OBJECT *obj );
static void create_pixmap( Canvas *c );
static void delete_pixmap( Canvas *c );
static void fs_rescale_1d( void );
static void fs_rescale_2d( void );
static void redraw_x_axis( void );
static void make_x_scale( Curve_1d *c );


/*----------------------------------------------------------------------*/
/* Initializes and shows the window for displaying measurement results. */
/*----------------------------------------------------------------------*/

void start_graphics( void )
{
	XCharStruct font_prop;
	int dummy;


	/* Create the form for running experiments */

	run_form = create_form_run( );
	fl_set_object_helper( run_form->stop, "Stop the running program" );

	/* fdesign is unable to set the box type attributes for canvases... */

	fl_set_canvas_decoration( run_form->x_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->y_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->canvas, FL_NO_FRAME );

	/* Show only the needed buttons */

	if ( G.is_init )
	{
		if ( G.dim == 1 )
		{
			if ( G.nc < 4 )
				fl_hide_object( run_form->curve_4_button );
			if ( G.nc < 3 )
				fl_hide_object( run_form->curve_3_button );
			if ( G.nc < 2 )
			{
				fl_hide_object( run_form->curve_2_button );
				fl_hide_object( run_form->curve_1_button );
			}
		}
	}
	else                                 /* no graphics initialisation */
	{
		fl_hide_object( run_form->curve_1_button );
		fl_hide_object( run_form->curve_2_button );
		fl_hide_object( run_form->curve_3_button );
		fl_hide_object( run_form->curve_4_button );

		fl_hide_object( run_form->full_scale_button );
		fl_hide_object( run_form->undo_button );
	}

	/* Finally draw the form */

	fl_show_form( run_form->run, FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
				  "fsc: Display" );

	G.d = FL_FormDisplay( run_form->run );

	/* Set minimum size for display window */

	fl_winminsize( run_form->run->window, 400, 320 );
	fl_set_button( run_form->full_scale_button, 1 );

	/* Load a font hopefully available on all machines */

	if ( G.is_init )
	{
		G.font = XLoadQueryFont( G.d, "lucidasanstypewriter-14" );
		if ( G.font == NULL )
			G.font = XLoadQueryFont( G.d, "9x15" );

		if ( G.font != NULL )
			XTextExtents( G.font, "Xy", 2, &dummy, &G.font_asc, &G.font_desc,
						  &font_prop );
	}

	setup_canvas( &G.x_axis, run_form->x_axis );
	setup_canvas( &G.y_axis, run_form->y_axis );
	setup_canvas( &G.canvas, run_form->canvas );

	if ( G.is_init )
		G_struct_init( );

	redraw_canvas( &G.x_axis );
	redraw_canvas( &G.y_axis );
	redraw_canvas( &G.canvas );

	fl_raise_form( run_form->run );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void G_struct_init( void )
{
	static bool first_time = SET;
	long i, j;
	unsigned int depth = fl_get_canvas_depth( G.canvas.obj );
	Curve_1d *cv;
	int x, y;
	unsigned int keymask;


	/* Get the current mouse button state (useful and raw) */

	G.button_state = G.raw_button_state = 0;

	fl_get_mouse( &x, &y, &keymask );

	if ( keymask & Button1Mask )
		G.raw_button_state |= 1;
	if ( keymask & Button2Mask )
		G.raw_button_state |= 2;
	if ( keymask & Button3Mask )
		G.raw_button_state |= 4;

	/* On the first call create the different cursors */

	if ( first_time )
	{
		G.cur_1 = fl_create_bitmap_cursor( c1_bits, c1_bits, c1_width,
										   c1_height, c1_x_hot, c1_y_hot );
		G.cur_2 = fl_create_bitmap_cursor( c2_bits, c2_bits, c2_width,
										   c2_height, c2_x_hot, c2_y_hot );
		G.cur_3 = fl_create_bitmap_cursor( c3_bits, c3_bits, c3_width,
										   c3_height, c3_x_hot, c3_y_hot );
		G.cur_4 = fl_create_bitmap_cursor( c4_bits, c4_bits, c4_width,
										   c4_height, c4_x_hot, c4_y_hot );
		G.cur_5 = fl_create_bitmap_cursor( c5_bits, c5_bits, c5_width,
										   c5_height, c5_x_hot, c5_y_hot );

		fl_set_cursor_color( G.cur_1, FL_RED, FL_WHITE );
		fl_set_cursor_color( G.cur_2, FL_RED, FL_WHITE );
		fl_set_cursor_color( G.cur_3, FL_RED, FL_WHITE );
		fl_set_cursor_color( G.cur_4, FL_RED, FL_WHITE );
		fl_set_cursor_color( G.cur_5, FL_RED, FL_WHITE );
	}

	/* Define colors for the curves */

	G.colors[ 0 ] = FL_TOMATO;
	G.colors[ 1 ] = FL_GREEN;
	G.colors[ 2 ] = FL_YELLOW;
	G.colors[ 3 ] = FL_CYAN;

	G.is_fs = SET;
	G.scale_changed =  SET;

	G.rw_y_min = HUGE_VAL;
	G.rw_y_max = - HUGE_VAL;
	G.rw2s = 1.0;
	G.is_scale_set = UNSET;

	for ( i = 0; i < G.nc; i++ )
	{
		/* Allocate memory for the curve and its data */

		cv = G.curve[ i ] = T_malloc( sizeof( Curve_1d ) );

		cv->points = NULL;
		cv->points = T_malloc( G.nx * sizeof( Scaled_Point ) );

		for ( j = 0; j < G.nx; j++ )        /* no points are known in yet */
			cv->points[ j ].exist = UNSET;

		cv->xpoints = NULL;
		cv->xpoints = T_malloc( G.nx * sizeof( XPoint ) );

		/* Create a GC for drawing the curve and set its color */

		cv->gc = XCreateGC( G.d, G.canvas.pm, 0, 0 );
		XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ i ] ) );

		/* Create pixmaps for the out-of-display arrows */

		cv->up_arr =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, ua_bits,
										 ua_width, ua_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		cv->down_arr =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, da_bits,
										 da_width, da_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		cv->left_arr =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, la_bits,
										 la_width, la_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		cv->right_arr = 
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, ra_bits,
										 ra_width, ra_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		/* Create a GC for the font and set the appropriate color */

		cv->font_gc = XCreateGC( G.d, FL_ObjWin( G.canvas.obj ), 0, 0 );
		XSetFont( G.d, cv->font_gc, G.font->fid );
		XSetForeground( G.d, cv->font_gc, fl_get_pixel( G.colors[ i ] ) );
		XSetBackground( G.d, cv->font_gc, fl_get_pixel( FL_BLACK ) );

		/* Set the scaling factors for the curve */

		cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
		cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 );
		cv->shift[ X ] = cv->shift[ Y ] = 0.0;

		cv->count = 0;
		cv->active = SET;
		cv->can_undo = UNSET;
	}

	first_time = UNSET;
}


/*--------------------------------------------------------*/
/* Removes the window for displaying measurement results. */
/*--------------------------------------------------------*/

void stop_graphics( void )
{
	if ( G.is_init )
	{
		graphics_free( );

		XFreeFont( G.d, G.font );

		canvas_off( &G.x_axis, run_form->x_axis );
		canvas_off( &G.y_axis, run_form->y_axis );
		canvas_off( &G.canvas, run_form->canvas );
	}

	if ( fl_form_is_visible( run_form->run ) )
			fl_hide_form( run_form->run );

		fl_free_form( run_form->run );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void graphics_free( void )
{
	long i;
	Curve_1d *cv;


	if ( G.dim == 1 )
	{
		/* Deallocate memory for storing scaled data and XPoints */

		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			XFreeGC( G.d, cv->gc );

			XFreePixmap( G.d, cv->up_arr );
			XFreePixmap( G.d, cv->down_arr );
			XFreePixmap( G.d, cv->left_arr );
			XFreePixmap( G.d, cv->right_arr );

			XFreeGC( G.d, cv->font_gc );

			if ( cv != NULL )
			{
				if ( cv->points != NULL )
					T_free( cv->points );

				if ( cv->xpoints != NULL )
					T_free( cv->xpoints );

				T_free( cv );
			}
		}
	}
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void free_graphics( void )
{
	G.is_init = UNSET;
	if ( G.label[ X ] != NULL )
		T_free( G.label[ X ] );
	if ( G.label[ Y ] != NULL )
		T_free( G.label[ Y ] );
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

void canvas_off( Canvas *c, FL_OBJECT *obj )
{
	fl_remove_canvas_handler( obj, Expose, canvas_handler);

	if ( G.is_init )
	{
		fl_remove_canvas_handler( obj, ConfigureNotify, canvas_handler);
		fl_remove_canvas_handler( obj, ButtonPress, canvas_handler);
		fl_remove_canvas_handler( obj, ButtonRelease, canvas_handler);
		fl_remove_canvas_handler( obj, MotionNotify, canvas_handler);

		XFreeGC( G.d, c->font_gc );
	}

	delete_pixmap( c );
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void setup_canvas( Canvas *c, FL_OBJECT *obj )
{
	c->obj = obj;

	c->x = obj->x;
	c->y = obj->y;
	c->w = obj->w;
	c->h = obj->h;
	create_pixmap( c );

	fl_add_canvas_handler( c->obj, Expose, canvas_handler, ( void * ) c );

	if ( G.is_init )
	{
		c->is_box = UNSET;

		fl_remove_selected_xevent( FL_ObjWin( obj ),
								   PointerMotionMask | PointerMotionHintMask );
		fl_add_selected_xevent( FL_ObjWin( obj ),
								Button1MotionMask | Button2MotionMask );

		fl_add_canvas_handler( c->obj, ConfigureNotify, canvas_handler,
							   ( void * ) c );
		fl_add_canvas_handler( c->obj, ButtonPress, canvas_handler,
							   ( void * ) c );
		fl_add_canvas_handler( c->obj, ButtonRelease, canvas_handler,
							   ( void * ) c );
		fl_add_canvas_handler( c->obj, MotionNotify, canvas_handler,
							   ( void * ) c );

		c->font_gc = XCreateGC( G.d, FL_ObjWin( obj ), 0, 0 );
		XSetForeground( G.d, c->font_gc, fl_get_pixel( FL_BLACK ) );
		XSetFont( G.d, c->font_gc, G.font->fid );
	}
}


/*---------------------------------*/
/* Creates a pixmap for buffering. */
/*---------------------------------*/

void create_pixmap( Canvas *c )
{
    Window root;
    int x, y;               /* x- and y-position */
    unsigned int w,         /* width */
		         h,         /* height */
		         bw,        /* border width */
		         d;         /* depth */
	char dashes[ ] = { 2, 2 };


    XGetGeometry( G.d, FL_ObjWin( c->obj ), &root, &x, &y, &w, &h, &bw, &d );
    c->gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, 0 );
    c->pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h, d );

	if ( c == &G.canvas )
	{
		if ( G.is_init )
		{
			G.pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w,
								  c->h, d );
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
		}
		else
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_INACTIVE ) );
	}
	else
		XSetForeground( G.d, c->gc, fl_get_pixel( FL_LEFT_BCOL ) );

	if ( G.is_init )
	{
		c->box_gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, 0 );

		XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );
		XSetLineAttributes( G.d, c->box_gc, 0, LineOnOffDash, CapButt,
							JoinMiter );
		XSetDashes( G.d, c->box_gc, 0, dashes, 2 );
	}
}


/*-----------------------------------*/
/* Deletes the pixmap for buffering. */
/*-----------------------------------*/

void delete_pixmap( Canvas *c )
{
	XFreeGC( G.d, c->gc );
	XFreePixmap( G.d, c->pm );

	if ( G.is_init )
	{
		XFreeGC( G.d, c->box_gc );
		if ( c == &G.canvas )
			XFreePixmap( G.d, G.pm );
	}
}


/*-------------------------------------*/
/* Handles changes of the window size. */
/*-------------------------------------*/

void reconfigure_window( Canvas *c, int w, int h )
{
	long i;
	Curve_1d *cv;


	/* Calculate the new scale factors */

	if ( c == &G.canvas && G.is_scale_set )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			cv->s2d[ X ] *= ( double ) ( w - 1 ) / ( double ) ( c->w - 1 );
			cv->s2d[ Y ] *= ( double ) ( h - 1 ) / ( double ) ( c->h - 1 );

			if ( cv->can_undo )
			{
				cv->old_s2d[ X ] *=
					            ( double ) ( w - 1 ) / ( double ) ( c->w - 1 );
				cv->old_s2d[ Y ] *=
					            ( double ) ( h - 1 ) / ( double ) ( c->h - 1 );
			}
		}
	}

	/* Set the new canvas sizes */

	c->w = ( unsigned int ) w;
	c->h = ( unsigned int ) h;

	/* Recalculate data for drawing (has to be done after setting of canvas
	   sizes since they are needed in the recalculation) */

	if ( G.is_init && c == &G.canvas && G.is_scale_set )
		recalc_XPoints( );

	if ( c != &G.canvas )
		return;

	delete_pixmap( &G.canvas );
	create_pixmap( &G.canvas );
	redraw_canvas( &G.canvas );

	delete_pixmap( &G.y_axis );
	create_pixmap( &G.y_axis );
	redraw_canvas( &G.y_axis );

	delete_pixmap( &G.x_axis );
	create_pixmap( &G.x_axis );
	redraw_canvas( &G.x_axis );
}


/*-------------------------------------*/
/* Does a complete redraw of a canvas. */
/*-------------------------------------*/

void redraw_canvas( Canvas *c )
{
	long i;
	Curve_1d *cv;


	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

	if ( G.is_init )
	{
		if ( c == &G.canvas && G.is_scale_set )
			for ( i = G.nc - 1 ; i >= 0; i-- )
			{
				cv = G.curve[ i ];

				if ( cv->count <= 1 )
					continue;

				if ( cv->up )
					XCopyArea( G.d, cv->up_arr, c->pm, c->gc,
							   0, 0, ua_width, ua_height,
							   G.canvas.w / 2 - 32 + 16 * i, 5 );

				if ( cv->down )
					XCopyArea( G.d, cv->down_arr, c->pm, c->gc,
							   0, 0, da_width, da_height,
							   G.canvas.w / 2 - 32 + 16 * i,
							   G.canvas.h - 5 - da_height);

				if ( cv->left )
					XCopyArea( G.d, cv->left_arr, c->pm, c->gc,
							   0, 0, la_width, la_height,
							   5, G.canvas.h / 2 -32 + 16 * i );

				if ( cv->right )
					XCopyArea( G.d, cv->right_arr, c->pm, c->gc,
							   0, 0, ra_width, ra_height,
							   G.canvas.w - 5- ra_width,
							   G.canvas.h / 2 - 32 + 16 * i );

				XDrawLines( G.d, c->pm, cv->gc, cv->xpoints, cv->count, 
							CoordModeOrigin );
			}

		if ( c == &G.x_axis )
			redraw_x_axis( );
	}

	repaint_canvas( c );
}


void redraw_x_axis( void )
{
	Canvas *c = &G.x_axis;
	Curve_1d *cv;
	int width;
	int i;


	if ( G.label[ X ] != NULL && G.font != NULL )
	{
		width = XTextWidth( G.font, G.label[ X ], strlen( G.label[ X ] ) );
		XDrawString( G.d, c->pm, c->font_gc, c->w - width - 5,
					 c->h - 5 - G.font_desc,
					 G.label[ X ], strlen( G.label[ X ] ) );
	}

	if ( ! G.is_scale_set )
		return;

	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];
		if ( cv->active )
			break;
	}

	if ( i == G.nc )                          /* no active curve -> no scale */
		return;

	XDrawLine( G.d, c->pm, cv->gc, 0, c->h / 2, c->w - 1, c->h / 2 );

	make_x_scale( cv );
}


void make_x_scale( Curve_1d *cv )
{
	double rwc_delta,          /* distance between small ticks (in rwc) */
		   order;              /* and its order of magitude */
	double d_delta_fine,       /* distance between small ticks (in points) */
		   d_start_fine,       /* position of first small tick (in points) */
		   d_start_medium,     /* position of first medium tick (in points) */
		   d_start_coarse,     /* position of first large tick (in points) */
		   cur_x;              /* loop variable with position */
	int medium_factor,         /* number of small tick spaces between medium */
		coarse_factor;         /* and large tick spaces */
	int medium,                /* loop counters for medium and large ticks */
		coarse;
	double rwc_start,          /* rwc value of first point */
		   rwc_start_fine,     /* rwc value of first small tick */
		   rwc_start_medium,   /* rwc value of first medium tick */
		   rwc_start_coarse;   /* rwc value of first large tick */
	short x, y;


	/* The distance between the smallest ticks should be about 8 points -
	   calculate the corresponding delta in real word units */

	rwc_delta = 8.0 * fabs( G.rwc_delta[ X ] ) / cv->s2d[ X ];

	/* Now scale this distance to the interval [ 1, 10 [ */

	order = pow( 10.0, floor( log10( rwc_delta ) ) );
	modf( rwc_delta / order, &rwc_delta );

	/* Now get a `smooth' value for the ticks distance, i.e. either 2, 2.5,
	   5 or 10 and convert it to real world coordinates */

	if ( rwc_delta <= 2.0 )       /* in [ 1, 2 ] -> units of 2 */
	{
		medium_factor = 5;
		coarse_factor = 25;
		rwc_delta = 2.0 * order;
	}
	else if ( rwc_delta <= 3.0 )  /* in ] 2, 3 ] -> units of 2.5 */
	{
		medium_factor = 2;
		coarse_factor = 4;
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


	/* Calculate the distance between the small ticks (in points) */

	d_delta_fine = cv->s2d[ X ] * rwc_delta / fabs( G.rwc_delta[ X ] );


	/* `rwc_start' is the leftmost value in the display, `rwc_start_fine' the
	   position of the leftmost small tick (in real world coordinates) and,
	   finally, `d_start_fine' the same position but in points */

	rwc_start = G.rwc_start[ X ] - cv->shift[ X ] * G.rwc_delta[ X ];

	modf( rwc_start / rwc_delta, &rwc_start_fine );
	rwc_start_fine *= rwc_delta;

	d_start_fine = cv->s2d[ X ] * ( rwc_start_fine - rwc_start ) /
		                                                      G.rwc_delta[ X ];
	if ( d_start_fine < 0 )
		d_start_fine += d_delta_fine;


	/* Calculate start index for leftmost medium tick */

	modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
	rwc_start_medium *= medium_factor * rwc_delta;

	d_start_medium = cv->s2d[ X ] * ( rwc_start_medium - rwc_start ) /
			                                                  G.rwc_delta[ X ];
	if ( d_start_medium < 0 )
		d_start_medium += medium_factor * d_delta_fine;

	medium = rnd( ( d_start_fine - d_start_medium ) / d_delta_fine );

	/* Calculate start index for leftmost large tick */

	modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
	rwc_start_coarse *= coarse_factor * rwc_delta;

	d_start_coarse = cv->s2d[ X ] * ( rwc_start_coarse - rwc_start ) /
			                                                  G.rwc_delta[ X ];
	if ( d_start_coarse < 0 )
		d_start_coarse += coarse_factor * d_delta_fine;

	coarse = rnd( ( d_start_fine - d_start_coarse ) / d_delta_fine );


	/* Now, finally we can strt drawing the axis... */

	for ( cur_x = d_start_fine; cur_x < G.x_axis.w;
		  medium++, coarse++, cur_x += d_delta_fine )
	{
		x = d2shrt( cur_x );
		y = G.x_axis.h / 2;

		if ( coarse % coarse_factor == 0 )
			XDrawLine( G.d, G.x_axis.pm, cv->gc, x, y + 3, x, y - 12 );
		else if ( medium % medium_factor == 0 )
			XDrawLine( G.d, G.x_axis.pm, cv->gc, x, y, x, y - 9 );
		else
			XDrawLine( G.d, G.x_axis.pm, cv->gc, x, y, x, y - 5 );
	}

}


/*-----------------------------------------------*/
/* Copies the background pixmap onto the canvas. */
/*-----------------------------------------------*/

void repaint_canvas( Canvas *c )
{
	static int i;
	char buf[ 70 ];
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

	if ( c == &G.canvas )
	{
		if ( G.button_state == 3 )
		{
			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				x_pos = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];
				x_pos = G.rwc_start[ X ] + G.rwc_delta[ X ] * x_pos;
				y_pos = ( 1.0 - c->ppos[ Y ] / cv->s2d[ Y ] 
						  - cv->shift[ Y ] ) / G.rw2s;

				sprintf( buf, "%#g, %#g", x_pos, y_pos );
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
				y_pos = ( c->ppos[ Y ] - G.start[ Y ] ) /
					                                 ( cv->s2d[ Y ] * G.rw2s );

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

	XCopyArea( G.d, G.pm, FL_ObjWin( c->obj ), c->gc,
			   0, 0, c->w, c->h, 0, 0 );

	XFlush( G.d );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void switch_off_special_cursors( void )
{
	if ( G.is_init && G.button_state != 0 )
	{
		G.button_state = G.raw_button_state = 0;

		if ( G.drag_canvas == 1 )
		{
			fl_reset_cursor( FL_ObjWin( G.x_axis.obj ) );
			G.x_axis.is_box = UNSET;
			repaint_canvas( &G.x_axis );
		}

		if ( G.drag_canvas == 2 )
		{
			fl_reset_cursor( FL_ObjWin( G.y_axis.obj ) );
			G.y_axis.is_box = UNSET;
			repaint_canvas( &G.y_axis );
		}

		if ( G.drag_canvas == 3 )
		{
			fl_reset_cursor( FL_ObjWin( G.canvas.obj ) );
			G.canvas.is_box = UNSET;
			repaint_canvas( &G.canvas );
		}
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void recalc_XPoints( void )
{
	long i;


	for ( i = 0; i < G.nc; i++ )
		recalc_XPoints_of_curve( G.curve[ i ] );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void recalc_XPoints_of_curve( Curve_1d *cv )
{
	long j, k;


	cv->up = cv->down = cv->left = cv->right = UNSET;

	for ( k = 0, j = 0; j < G.nx; j++ )
	{
		if ( cv->points[ j ].exist )
		{
			cv->xpoints[ k ].x = d2shrt( cv->s2d[ X ]
										            * ( j + cv->shift[ X ] ) );
			cv->xpoints[ k ].y = d2shrt( cv->s2d[ Y ]
						  * ( 1.0 - ( cv->points[ j ].y + cv->shift[ Y ] ) ) );

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


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void undo_button_callback( FL_OBJECT *a, long b )
{
	long i;
	bool is_undo = UNSET;
	Curve_1d *cv;


	a = a;
	b = b;


	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		if ( ! cv->can_undo )
			continue;

		cv->s2d[ X ] = cv->old_s2d[ X ];
		cv->s2d[ Y ] = cv->old_s2d[ Y ];
		cv->shift[ X ] = cv->old_shift[ X ];
		cv->shift[ Y ] = cv->old_shift[ Y ];

		cv->can_undo = UNSET;
		is_undo = SET;

		recalc_XPoints_of_curve( cv );
	}

	if ( is_undo && G.is_fs )
	{
		G.is_fs = UNSET;
		fl_set_button( run_form->full_scale_button, 0 );
	}

	if ( is_undo )
		redraw_canvas( &G.canvas );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void fs_button_callback( FL_OBJECT *a, long b )
{
	int state;
	int i;
	Curve_1d *cv;


	a = a;
	b = b;

	/* Rescaling is useless if graphic isn't initialized */

	if ( ! G.is_init )
		return;

	/* Get new state of button */

	state = fl_get_button( run_form->full_scale_button );

	if ( G.dim == 1 )
	{
		if ( state == 1 )        /* if button got switched on */
		{
			/* Store data of previous state... */

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				cv->old_s2d[ X ] = cv->s2d[ X ];
				cv->old_s2d[ Y ] = cv->s2d[ Y ];
				cv->old_shift[ X ] = cv->shift[ X ];
				cv->old_shift[ Y ] = cv->shift[ Y ];

				cv->can_undo = SET;
			}

			/* ... and rescale to full scale */

			G.is_fs = SET;
			fs_rescale_1d( );
		}
		else                     /* if button got switched off */
			G.is_fs = UNSET;
	}
	else
		fs_rescale_2d( );

	/* Redraw the graphic */

	redraw_canvas( &G.canvas );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void fs_rescale_1d( void )
{
	long i, j, k;
	double min = 1.0,
		   max = 0.0;
	double rw_y_min,
		   rw_y_max;
	double data;
	double new_y_scale;
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
				data = cv->points[ j ].y;
				max = d_max( data, max );
				min = d_min( data, min );
			}
	}

	/* Calculate new real world maximum and minimum */

	rw_y_min = min / G.rw2s + G.rw_y_min;
	rw_y_max = max / G.rw2s + G.rw_y_min;

	/* Calculate new scaling factor and rescale the scaled data as well as the
	   points for drawing */

	new_y_scale = 1.0 / ( rw_y_max - rw_y_min );

	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		cv->shift[ X ] = cv->shift[ Y ] = 0.0;
		cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
		cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 );

		cv->up = cv->down = cv->left = cv->right = UNSET;

		for ( k = 0, j = 0; j < G.nx; j++ )
			if ( cv->points[ j ].exist )
				cv->points[ j ].y = new_y_scale * ( cv->points[ j ].y /
											  G.rw2s + G.rw_y_min - rw_y_min );

		recalc_XPoints_of_curve( cv );
	}

	/* Store new minimum and maximum and the new scale factor */

	G.rw2s = new_y_scale;
	G.rw_y_min = rw_y_min;
	G.rw_y_max = rw_y_max;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void fs_rescale_2d( void )
{
}




void curve_button_callback( FL_OBJECT *obj, long data )
{
	obj = obj;
	G.curve[ data - 1 ]->active ^= SET;
}
