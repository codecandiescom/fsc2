/*
  $Id$
*/


#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/param.h>

#include "c1.xbm"
#include "c2.xbm"
#include "c3.xbm"
#include "ua.xbm"
#include "da.xbm"
#include "la.xbm"
#include "ra.xbm"


void setup_canvas( Canvas *c, FL_OBJECT *obj );
void create_pixmap( Canvas *c );
void delete_pixmap( Canvas *c );
int canvas_handler( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					void *udata );
void button_release_handler( Window win, int x_start, int y_start );
void reconfigure_window( Canvas *c, int w, int h );
void redraw_canvas( Canvas *c );
void recalc_XPoints( void );
void recalc_XPoints_of_curve( Curve_1d *cv );
void repaint_canvas( Canvas *c );
void fs_rescale_1d( void );
void fs_rescale_2d( void );
void accept_1d_data( long x_index, long curve, int type, void *ptr );
void accept_2d_data( long x_index, long y_index, long curve, int type,
					 void *ptr );



FL_COLOR colors[ MAX_CURVES ] = { FL_RED, FL_GREEN, FL_YELLOW, FL_CYAN };
int cursor_1,
	cursor_2,
	cursor_3;
XFontStruct *font;
int button_state;



void graphics_init( long dim, long nc, long nx, long ny,
					double rwc_x_start, double rwc_x_delta,
					double rwc_y_start, double rwc_y_delta,
					char *x_label, char *y_label )
{
	long i;


	/* The parent process does all the graphics stuff... */

	if ( I_am == CHILD )
	{
		writer( C_INIT_GRAPHICS, dim, nc, nx, ny, rwc_x_start, rwc_x_delta,
				rwc_y_start, rwc_y_delta, x_label, y_label );
		return;
	}

	G.x_label = G.y_label = NULL;
	for ( i = 0; i < MAX_CURVES; i++ )
		G.curve[ i ] = NULL;
	G.is_init = SET;

	/* Store dimension of experiment (1 or 2) and number of curves */

	G.dim = dim;
	G.nc = nc;

	/* For `nx', i.e. the number of points in x direction, being greater than
	   zero the user already knows the exact number of points, zero means
	   (s)he didn't got ny idea and a negative number is treated as a guess */

	if ( nx > 0 )
	{
		G.is_nx = SET;
		G.nx = nx;
	}
	else
	{
		G.is_nx = UNSET;
		if ( nx == 0 )
			G.nx = DEFAULT_X_POINTS;
		else
			G.nx = labs( nx );
	}

	/* Same for the number of points in y direction `ny' */

	if ( ny > 0 )
	{
		G.is_ny = SET;
		G.ny = ny;
	}
	else
	{
		G.is_ny = UNSET;
		if ( ny == 0 )
			G.ny = DEFAULT_Y_POINTS;
		else
			G.ny = labs( ny );
	}

	/* Check if there are `real world' coordinates for x and y direction (if
       both the start and the increment value are zero this means there aren't
       any) */

	G.rwc_x_start = rwc_x_start;
	G.rwc_x_delta = rwc_x_delta;

	if ( rwc_x_start == 0.0 && rwc_x_delta == 0.0 )
		G.is_rwc_x = UNSET;
	else
		G.is_rwc_x = SET;

	G.rwc_y_start = rwc_y_start;
	G.rwc_y_delta = rwc_y_delta;

	if ( rwc_y_start == 0.0 && rwc_y_delta == 0.0 )
		G.is_rwc_y = UNSET;
	else
		G.is_rwc_y = SET;

	/* Store the labels for x and y direction */

	if ( x_label != NULL )
	{
		if ( G.x_label != NULL )
			T_free( G.x_label );
		G.x_label = get_string_copy( x_label );
	}

	if ( dim == 2 && y_label != NULL )
	{
		if ( G.y_label != NULL )
			T_free( G.y_label );
		G.y_label = get_string_copy( y_label );
	}
}


void graphics_free( void )
{
	long i;
	Curve_1d *cv;


	if ( G.is_init == UNSET )
		return;

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


void free_graphics( void )
{
	G.is_init = UNSET;
	if ( G.x_label != NULL )
		T_free( G.x_label );
	if ( G.y_label != NULL )
		T_free( G.y_label );
}


/*----------------------------------------------------------------------*/
/* Initializes and shows the window for displaying measurement results. */
/*----------------------------------------------------------------------*/

void start_graphics( void )
{
	button_state = 0;

	/* Create the form for running experiments */

	run_form = create_form_run( );
	fl_set_object_helper( run_form->stop, "Stop the running program" );

	/* fdesign is unable to set the box type attributes for canvases... */

	fl_set_canvas_decoration( run_form->x_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->y_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->canvas, FL_NO_FRAME );

	/* Show only the needed buttons */

	if ( G.is_init && G.dim == 1 )
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
	else
	{
		fl_hide_object( run_form->curve_1_button );
		fl_hide_object( run_form->curve_2_button );
		fl_hide_object( run_form->curve_3_button );
		fl_hide_object( run_form->curve_4_button );
	}

	/* Finally draw the form */

	fl_show_form( run_form->run, FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
				  "fsc: Display" );
	fl_freeze_form( run_form->run );

	fl_remove_selected_xevent( FL_ObjWin( run_form->x_axis ),
							   PointerMotionMask | PointerMotionHintMask );
	fl_add_selected_xevent( FL_ObjWin( run_form->x_axis ),
							Button1MotionMask | Button2MotionMask );
	fl_remove_selected_xevent( FL_ObjWin( run_form->y_axis ),
								   PointerMotionMask | PointerMotionHintMask );
	fl_add_selected_xevent( FL_ObjWin( run_form->y_axis ),
							Button1MotionMask | Button2MotionMask );
	fl_remove_selected_xevent( FL_ObjWin( run_form->canvas ),
								   PointerMotionMask | PointerMotionHintMask );
	fl_add_selected_xevent( FL_ObjWin( run_form->canvas ),
							Button1MotionMask | Button2MotionMask );

	/* Set minimum size for display window */

	fl_winminsize( run_form->run->window, 400, 320 );

	fl_set_button( run_form->full_scale_button, 1 );
	G.is_fs = SET;

	G.d = FL_FormDisplay( run_form->run );

	cursor_1 = fl_create_bitmap_cursor( c1_bits, c1_bits, c1_width, c1_height,
										c1_x_hot, c1_y_hot );
	fl_set_cursor_color( cursor_1, FL_RED, FL_WHITE );

	cursor_2 = fl_create_bitmap_cursor( c2_bits, c2_bits, c2_width, c2_height,
										c2_x_hot, c2_y_hot );
	fl_set_cursor_color( cursor_2, FL_RED, FL_WHITE );

	cursor_3 = fl_create_bitmap_cursor( c3_bits, c3_bits, c3_width, c3_height,
										c3_x_hot, c3_y_hot );
	fl_set_cursor_color( cursor_3, FL_RED, FL_WHITE );

	setup_canvas( &G.x_axis, run_form->x_axis );
	setup_canvas( &G.y_axis, run_form->y_axis );
	setup_canvas( &G.canvas, run_form->canvas );

	font = XLoadQueryFont( G.d, "9x15" );

	if ( G.is_init )
	{
		long i, j;
		unsigned int depth = fl_get_canvas_depth( G.canvas.obj );
		Curve_1d *cv;


		G.scale_changed =  SET;

		G.rw_y_min = HUGE_VAL;
		G.rw_y_max = - HUGE_VAL;
		G.rw2s = 1.0;
		G.is_scale_set = UNSET;

		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ] = T_malloc( sizeof( Curve_1d ) );

			cv->points = NULL;
			cv->points = T_malloc( G.nx * sizeof( Scaled_Point ) );

			for ( j = 0; j < G.nx; j++ )
				cv->points->exist = UNSET;

			cv->xpoints = NULL;
			cv->xpoints = T_malloc( G.nx * sizeof( XPoint ) );

			cv->gc = XCreateGC( G.d, G.canvas.pm, 0, 0 );
			XSetForeground( G.d, cv->gc, fl_get_pixel( colors[ i ] ) );

			cv->up_arr =
				XCreatePixmapFromBitmapData( G.d, G.canvas.pm, ua_bits,
											 ua_width, ua_height,
											 fl_get_pixel( colors[ i ] ),
											 fl_get_pixel( FL_BLACK ), depth );
			cv->down_arr =
				XCreatePixmapFromBitmapData( G.d, G.canvas.pm, da_bits,
											 da_width, da_height,
											 fl_get_pixel( colors[ i ] ),
											 fl_get_pixel( FL_BLACK ), depth );
			cv->left_arr =
				XCreatePixmapFromBitmapData( G.d, G.canvas.pm, la_bits,
											 la_width, la_height,
											 fl_get_pixel( colors[ i ] ),
											 fl_get_pixel( FL_BLACK ), depth );
			cv->right_arr = 
				XCreatePixmapFromBitmapData( G.d, G.canvas.pm, ra_bits,
											 ra_width, ra_height,
											 fl_get_pixel( colors[ i ] ),
											 fl_get_pixel( FL_BLACK ), depth );

			cv->font_gc = XCreateGC( G.d, FL_ObjWin( G.canvas.obj ), 0, 0 );
			XSetFont( G.d, cv->font_gc, font->fid );
			XSetForeground( G.d, cv->font_gc, fl_get_pixel( colors[ i ] ) );

			cv->s2d_x =
				       ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
			cv->s2d_y = ( double ) ( G.canvas.h - 1 );
			cv->x_shift = cv->y_shift = 0.0;

			cv->count = 0;
			cv->active = SET;
			cv->can_undo = UNSET;
		}
	}

	redraw_canvas( &G.x_axis );
	redraw_canvas( &G.y_axis );
	redraw_canvas( &G.canvas );

	fl_raise_form( run_form->run );
	fl_unfreeze_form( run_form->run );
}


/*---------------------------------------------------------*/
/* Removes the window for displaying measurement results. */
/*---------------------------------------------------------*/

void stop_graphics( void )
{
	fl_remove_canvas_handler( run_form->x_axis, Expose, canvas_handler);
	fl_remove_canvas_handler( run_form->x_axis, ConfigureNotify,
							  canvas_handler);
	fl_remove_canvas_handler( run_form->y_axis, Expose, canvas_handler);
	fl_remove_canvas_handler( run_form->y_axis, ConfigureNotify,
							  canvas_handler);
	fl_remove_canvas_handler( run_form->canvas, Expose, canvas_handler);
	fl_remove_canvas_handler( run_form->canvas, ConfigureNotify,
							  canvas_handler);

	graphics_free( );

	XFreeFont( G.d, font );

	delete_pixmap( &G.x_axis );
	delete_pixmap( &G.y_axis );
	delete_pixmap( &G.canvas );

	if ( fl_form_is_visible( run_form->run ) )
		fl_hide_form( run_form->run );

	fl_free_form( run_form->run );
}


void setup_canvas( Canvas *c, FL_OBJECT *obj )
{
	c->obj = obj;

	c->x = obj->x;
	c->y = obj->y;
	c->w = obj->w;
	c->h = obj->h;
	create_pixmap( c );

	c->is_box = UNSET;

	fl_add_canvas_handler( c->obj, Expose, canvas_handler, ( void * ) c );
    fl_add_canvas_handler( c->obj, ConfigureNotify, canvas_handler,
						   ( void * ) c );
	fl_add_canvas_handler( c->obj, ButtonPress, canvas_handler, ( void * ) c );
	fl_add_canvas_handler( c->obj, ButtonRelease, canvas_handler,
						   ( void * ) c );
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


    XGetGeometry( G.d, FL_ObjWin( c->obj ), &root, &x, &y, &w, &h, &bw, &d );
    c->pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h, d );
    c->gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, 0 );

	c->box_gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, 0 );
	XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );

	if ( c == &G.canvas )
	{
		if ( G.is_init )
		{
			c->pm_sec = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w,
									   c->h, d );
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
		}
		else
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_INACTIVE ) );
	}
	else
		XSetForeground( G.d, c->gc, fl_get_pixel( c->obj->col1 ) );
}


/*-----------------------------------*/
/* Deletes the pixmap for buffering. */
/*-----------------------------------*/

void delete_pixmap( Canvas *c )
{
	if ( G.is_init )
	{
		XFreeGC( G.d, c->box_gc );
		XFreeGC( G.d, c->gc );
		XFreePixmap( G.d, c->pm );
		if ( c == &G.canvas )
			XFreePixmap( G.d, c->pm_sec );
	}
}


/*--------------------------------------------------------*/
/* Handler for all kinds of X events the canvas receives. */
/*--------------------------------------------------------*/

int canvas_handler( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					void *udata )
{
	Canvas *c = ( Canvas * ) udata;
	static int x_start,
		       y_start;
	long i;
	int x_cur,
		y_cur,
		dummy;
	Curve_1d *cv;


	switch ( ev->type )
    {
        case Expose :
			repaint_canvas( c );
            break;

		case ConfigureNotify :                   /* window was reconfigured */
            if ( ( int ) c->w == w && ( int ) c->h == h )
				break;
            reconfigure_window( c, w, h );
            break;

		case ButtonPress :
			fl_get_win_mouse( window, &x_start, &y_start, &dummy );
			button_state |= ( 1 << ( ev->xbutton.button - 1 ) );

			if ( button_state == 1 )                  /* left mouse button */
				fl_set_cursor( FL_ObjWin( obj ), cursor_1 );

			if ( button_state == 2 )                  /* middle mouse button */
			{
				fl_set_cursor( FL_ObjWin( obj ), cursor_2 );

				for ( i = 0; i < G.nc; i++ )
				{
					cv = G.curve[ i ];

					if ( ! cv->active )
					{
						cv->can_undo = UNSET;
						continue;
					}
					else
					{
						cv->old_s2d_x = cv->s2d_x;
						cv->old_s2d_y = cv->s2d_y;
						cv->old_x_shift = cv->x_shift;
						cv->old_y_shift = cv->y_shift;

						cv->can_undo = SET;
					}
				}
			}
/*
			if ( button_state == 3 )
			{
				if ( c == &G.x_axis || c == &G.y_axis )
					button_state = 1;
				else
				{
					fl_reset_cursor( FL_ObjWin( G.x_axis.obj ) );
					fl_reset_cursor( FL_ObjWin( G.y_axis.obj ) );
					fl_set_cursor( FL_ObjWin( G.canvas.obj ), XC_tcross );
					fl_set_cursor_color( XC_tcross, FL_RED, FL_BLACK );
					G.canvas.is_box = UNSET;
				}
			}
*/
			if ( button_state == 4 )              /* right mouse button */
				fl_set_cursor( FL_ObjWin( obj ), cursor_3 );

			if ( obj == run_form->x_axis )        /* in x-axis window */
			{
				G.drag_canvas = 1;

				if ( button_state == 1 )           /* left mouse button */
				{
					c->box_x = x_start;
					c->box_w = 0;
					c->box_y = c->h / 2 - 1;
					c->box_h = 2;
					c->is_box = SET;
					repaint_canvas( &G.x_axis );
				}

				fl_add_canvas_handler( obj, MotionNotify, canvas_handler,
									   udata );
			}

			if ( obj == run_form->y_axis )        /* in y-axis window */
			{
				G.drag_canvas = 2;

				if ( button_state == 1 )          /* left mouse button */
				{
					c->box_x = c->w / 2 - 1;
					c->box_w = 2;
					c->box_y = y_start;
					c->box_h = 0;
					c->is_box = SET;
					repaint_canvas( &G.y_axis );
				}

				fl_add_canvas_handler( obj, MotionNotify, canvas_handler,
									   udata );
			}

			if ( obj == run_form->canvas )        /* in canvas window */
			{
				G.drag_canvas = 3;

				if ( button_state == 1 )          /* left mouse button */
				{
					c->box_x = x_start;
					c->box_y = y_start;
					c->box_w = c->box_h = 0;
					c->is_box = SET;
					repaint_canvas( &G.canvas );
				}

				fl_add_canvas_handler( obj, MotionNotify, canvas_handler,
									   udata );
			}

			break;

		case ButtonRelease :
			button_release_handler( window, x_start, y_start );
			button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );

			break;

		case MotionNotify :
			fl_get_win_mouse( window, &x_cur, &y_cur, &dummy );

			if ( button_state == 1 )
			{
				if ( G.drag_canvas == 1 )
				{
					c->box_w = x_cur - x_start;

					if ( c->box_x + c->box_w >= ( int ) c->w )
						c->box_w = c->w - c->box_x - 1;

					if ( c->box_x + c->box_w < 0 )
						c->box_w = - c->box_x;

					repaint_canvas( &G.x_axis );
				}

				if ( G.drag_canvas == 2 )
				{
					c->box_h = y_cur - y_start ;

					if ( c->box_y + c->box_h >= ( int ) c->h )
						c->box_h = c->h - c->box_y - 1;

					if ( c->box_y + c->box_h < 0 )
						c->box_h = - c->box_y;

					repaint_canvas( &G.y_axis );
				}

				if ( G.drag_canvas == 3 )
				{
					c->box_w = x_cur - x_start;

					if ( c->box_x + c->box_w >= ( int ) c->w )
						c->box_w = c->w - c->box_x - 1;

					if ( c->box_x + c->box_w < 0 )
						c->box_w = - c->box_x;

					c->box_h = y_cur - y_start;

					if ( c->box_y + c->box_h >= ( int ) c->h )
						c->box_h = c->h - c->box_y - 1;

					if ( c->box_y + c->box_h < 0 )
						c->box_h = - c->box_y;

					repaint_canvas( &G.canvas );
				}
			}

			if ( button_state == 2 )
			{
				for ( i = 0; i < G.nc; i++ )
				{
					cv = G.curve[ i ];
						   
					if ( ! cv->active )
						continue;

					if ( G.drag_canvas & 1 )
						cv->x_shift += ( x_cur - x_start ) / cv->s2d_x;
					if ( G.drag_canvas & 2 )
						cv->y_shift -= ( y_cur - y_start ) / cv->s2d_y;

					recalc_XPoints_of_curve( cv );
				}

				x_start = x_cur;
				y_start = y_cur;

				if ( G.is_fs )
				{
					G.is_fs = UNSET;
					fl_set_button( run_form->full_scale_button, 0 );
				}

				redraw_canvas( &G.canvas );
			}

			if ( button_state == 3 )
				repaint_canvas( &G.canvas );

			break;
	}

	return 1;
}



void button_release_handler( Window win, int x_start, int y_start )
{
	double px, py,
		   x1, x2,
		   y1, y2;
	int x_cur,
		y_cur,
		dummy;
	long i;
	bool scale_changed = UNSET;
	Curve_1d *cv;


	/* Get mouse posiion and restrict it to the canvas */

	fl_get_win_mouse( win, &x_cur, &y_cur, &dummy );

	if ( x_cur < 0 )
		x_cur = 0;
	if ( x_cur >= ( int ) G.canvas.w )
		x_cur = G.canvas.w - 1;

	if ( y_cur < 0 )
		y_cur = 0;
	if ( y_cur >= ( int ) G.canvas.h )
		y_cur = G.canvas.h - 1;

	/* Distinguish between the three different areas and the type of the
	   current action */

	if ( G.drag_canvas == 1 )                 /* in x-axis window */
	{
		fl_remove_canvas_handler( G.x_axis.obj, MotionNotify, canvas_handler );
		fl_reset_cursor( FL_ObjWin( G.x_axis.obj ) );

		if ( button_state == 1 )               /* left mouse button */
		{
			/* Expand (with rubber box) along the x direction */

			if ( abs( x_start - x_cur ) > 4 )
			{
				for ( i = 0; i < G.nc; i++ )
				{
					cv = G.curve[ i ];

					if ( ! cv->active )
						continue;

					cv->old_s2d_x = cv->s2d_x;
					cv->old_s2d_y = cv->s2d_y;
					cv->old_x_shift = cv->x_shift;
					cv->old_y_shift = cv->y_shift;

					cv->can_undo = SET;

					x1 = x_start / cv->s2d_x - cv->x_shift;
					x2 = x_cur / cv->s2d_x - cv->x_shift;

					cv->x_shift = - d_min( x1, x2 );
					cv->s2d_x = ( double ) ( G.canvas.w - 1 ) /
						                                       fabs( x1 - x2 );

					recalc_XPoints_of_curve( cv );
					scale_changed = SET;
				}
			}

			G.x_axis.is_box = UNSET;
			redraw_canvas( &G.x_axis );
		}

		if ( button_state == 4 && abs( x_start - x_cur ) > 4 )
                                                       /* right mouse button */
		{
			/* Zoom in x direction */

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				if ( ! cv->active )
					continue;
				
				cv->old_s2d_x = cv->s2d_x;
				cv->old_s2d_y = cv->s2d_y;
				cv->old_x_shift = cv->x_shift;
				cv->old_y_shift = cv->y_shift;

				cv->can_undo = SET;

				px = x_start / cv->s2d_x - cv->x_shift;
				if ( x_start > x_cur )
					cv->s2d_x *= d_min( 4.0,
							       1.0 + 3.0 * ( double ) ( x_start - x_cur ) /
								                       ( double ) G.x_axis.w );
				else
					cv->s2d_x /= d_min( 4.0,
								   1.0 + 3.0 * ( double ) ( x_cur - x_start ) /
								                       ( double ) G.x_axis.w );

				cv->x_shift = x_start / cv->s2d_x - px;

				recalc_XPoints_of_curve( cv );
				scale_changed = SET;
			}
		}
	}

	if ( G.drag_canvas == 2 )                 /* in y-axis window */
	{
		fl_remove_canvas_handler( G.x_axis.obj, MotionNotify, canvas_handler );
		fl_reset_cursor( FL_ObjWin( G.y_axis.obj ) );

		if ( button_state == 1 )               /* left mouse button */
		{
			/* Expand (with rubber box) along the y direction */

			if ( abs( y_start - y_cur ) > 4 )
			{
				for ( i = 0; i < G.nc; i++ )
				{
					cv = G.curve[ i ];

					if ( ! cv->active )
						continue;

					cv->old_s2d_x = cv->s2d_x;
					cv->old_s2d_y = cv->s2d_y;
					cv->old_x_shift = cv->x_shift;
					cv->old_y_shift = cv->y_shift;

					cv->can_undo = SET;

					y1 = 1.0 - y_start / cv->s2d_y - cv->y_shift;
					y2 = 1.0 - y_cur / cv->s2d_y - cv->y_shift;

					cv->y_shift = 1.0 - d_max( y1, y2 );
					cv->s2d_y = ( double ) ( G.canvas.h - 1 )
						                                     / fabs( y1 - y2 );

					recalc_XPoints_of_curve( cv );
					scale_changed = SET;
				}
			}

			G.y_axis.is_box = UNSET;
			redraw_canvas( &G.y_axis );
		}
			
		if ( button_state == 4 && abs( y_start - y_cur ) > 4 )
			                                          /* right mouse button */
		{
			/* Zoom in y directrion */

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				if ( ! cv->active )
					continue;

				cv->old_s2d_x = cv->s2d_x;
				cv->old_s2d_y = cv->s2d_y;
				cv->old_x_shift = cv->x_shift;
				cv->old_y_shift = cv->y_shift;

				cv->can_undo = SET;

				py = 1.0 - y_start / cv->s2d_y - cv->y_shift;
				if ( y_start < y_cur )
					cv->s2d_y *= d_min( 4.0,
								   1.0 + 3.0 * ( double ) ( y_cur - y_start ) /
								                       ( double ) G.y_axis.h );
				else
					cv->s2d_y /= d_min( 4.0,
								   1.0 + 3.0 * ( double ) ( y_start - y_cur ) /
								                       ( double ) G.y_axis.h );

				cv->y_shift = 1.0 - y_start / cv->s2d_y - py;

				recalc_XPoints_of_curve( cv );
				scale_changed = SET;
			}
		}
	}

	if ( G.drag_canvas == 3 )                 /* in canvas window */
	{
		fl_remove_canvas_handler( G.canvas.obj, MotionNotify, canvas_handler );
		fl_reset_cursor( FL_ObjWin( G.canvas.obj ) );

		if ( button_state == 1 )               /* left mouse button */
		{
			/* Expand (with rubber box) in both directions */

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				if ( ! cv->active )
					continue;

				cv->old_s2d_x = cv->s2d_x;
				cv->old_s2d_y = cv->s2d_y;
				cv->old_x_shift = cv->x_shift;
				cv->old_y_shift = cv->y_shift;

				if ( abs( x_start - x_cur ) > 4 )
				{
					cv->can_undo = SET;

					x1 = x_start / cv->s2d_x - cv->x_shift;
					x2 = x_cur / cv->s2d_x - cv->x_shift;

					cv->x_shift = - d_min( x1, x2 );
					cv->s2d_x = ( double ) ( G.canvas.w - 1 ) /
						                                       fabs( x1 - x2 );

					scale_changed = SET;
				}

				if ( abs( y_start - y_cur ) > 4 )
				{
					cv->can_undo = SET;

					y1 = 1.0 - y_start / cv->s2d_y - cv->y_shift;
					y2 = 1.0 - y_cur / cv->s2d_y - cv->y_shift;

					cv->y_shift = 1.0 - d_max( y1, y2 );
					cv->s2d_y = ( double ) ( G.canvas.h - 1 ) /
						                                       fabs( y1 - y2 );

					scale_changed = SET;
				}

				if ( scale_changed )
					recalc_XPoints_of_curve( cv );

				G.canvas.is_box = UNSET;
				if ( ! scale_changed )
					redraw_canvas( &G.canvas );
			}
		}

		if ( button_state == 4 )               /* right mouse button */
		{
			/* Zoom in x and y direction */

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				if ( ! cv->active )
					continue;

				cv->old_s2d_x = cv->s2d_x;
				cv->old_s2d_y = cv->s2d_y;
				cv->old_x_shift = cv->x_shift;
				cv->old_y_shift = cv->y_shift;

				if ( abs( x_start - x_cur ) > 4 )
				{
					cv->can_undo = SET;

					px = x_start / cv->s2d_x - cv->x_shift;

					if ( x_start > x_cur )
						cv->s2d_x *= d_min( 4.0,
								   1.0 + 3.0 * ( double ) ( x_start - x_cur ) /
								                       ( double ) G.x_axis.w );
					else
						cv->s2d_x /= d_min( 4.0,
								   1.0 + 3.0 * ( double ) ( x_cur - x_start ) /
								                       ( double ) G.x_axis.w );

					cv->x_shift = x_start / cv->s2d_x - px;

					scale_changed = SET;
				}

				if ( abs( y_start - y_cur ) > 4 )
				{
					cv->can_undo = SET;

					py = 1.0 - y_start / cv->s2d_y - cv->y_shift;
					if ( y_start < y_cur )
						cv->s2d_y *= d_min( 4.0,
								   1.0 + 3.0 * ( double ) ( y_cur - y_start ) /
								                       ( double ) G.y_axis.h );
					else
						cv->s2d_y /= d_min( 4.0,
								   1.0 + 3.0 * ( double ) ( y_start - y_cur ) /
								                       ( double ) G.y_axis.h );

					cv->y_shift = 1.0 - y_start / cv->s2d_y - py;

					scale_changed = SET;
				}

				if ( scale_changed )
					recalc_XPoints_of_curve( cv );
			}
		}
	}

	if ( scale_changed )
	{
		if ( G.is_fs )
		{
			G.is_fs = UNSET;
			fl_set_button( run_form->full_scale_button, 0 );
		}				

		redraw_canvas( &G.canvas );
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

			cv->s2d_x *= ( double ) ( w - 1 ) / ( double ) ( c->w - 1 );
			cv->s2d_y *= ( double ) ( h - 1 ) / ( double ) ( c->h - 1 );

			if ( cv->can_undo )
			{
				cv->old_s2d_x *=
					            ( double ) ( w - 1 ) / ( double ) ( c->w - 1 );
				cv->old_s2d_y *=
					            ( double ) ( h - 1 ) / ( double ) ( c->h - 1 );
			}
		}
	}

	/* Set the new canvas sizes */

	c->w = ( unsigned int ) w;
	c->h = ( unsigned int ) h;

	/* Recalculate data for drawing (has to be done after setting of canvas
	   sizes since they are needed in the recalculation) */

	if ( c == &G.canvas && G.is_scale_set )
		recalc_XPoints( );

	delete_pixmap( c );
	create_pixmap( c );
	redraw_canvas( c );
}


/*-------------------------------------*/
/* Does a complete redraw of a canvas. */
/*-------------------------------------*/

void redraw_canvas( Canvas *c )
{
	long i;
	Curve_1d *cv;


	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

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

			XDrawLines( G.d, c->pm, cv->gc,
						cv->xpoints, cv->count,
						CoordModeOrigin );
		}

	repaint_canvas( c );
}


/*-----------------------------------------------*/
/* Copies the background pixmap onto the canvas. */
/*-----------------------------------------------*/

void repaint_canvas( Canvas *c )
{
	static int i;
	char buf[ 70 ];
	int x, y, dummy;
	unsigned int w, h;
	Curve_1d *cv;
	double x_pos, y_pos;


	if ( c != &G.canvas || ! c->is_box )
	{
		XCopyArea( G.d, c->pm, FL_ObjWin( c->obj ), c->gc,
				   0, 0, c->w, c->h, 0, 0 );
		return;
	}

	XCopyArea( G.d, c->pm, c->pm_sec, c->gc, 0, 0, c->w, c->h, 0, 0 );

	if ( c == &G.canvas && button_state == 3 )
	{
		fl_get_win_mouse( FL_ObjWin( c->obj ), &x, &y, &dummy );

		for ( i = G.nc - 1; i >= 0; i-- )
		{
			cv = G.curve[ i ];

			x_pos = x / cv->s2d_x - cv->x_shift;
			if ( G.is_rwc_x )
				x_pos = G.rwc_x_start + G.rwc_x_delta * x_pos;
			else
				x_pos += ARRAY_OFFSET;
			y_pos = ( 1.0 - y / cv->s2d_y - cv->y_shift ) / G.rw2s;

			sprintf( buf, "%#g, %#g", x_pos, y_pos );
			XDrawString( G.d, c->pm_sec, cv->font_gc, 20,
						 20 * ( i + 1 ), buf, strlen( buf ) );
		}
	}

	if ( c->is_box )
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

		XDrawRectangle( G.d, c->pm_sec, c->box_gc, x, y, w, h );
	}

	XCopyArea( G.d, c->pm_sec, FL_ObjWin( c->obj ), c->gc,
			   0, 0, c->w, c->h, 0, 0 );
}


void recalc_XPoints( void )
{
	long i;


	for ( i = 0; i < G.nc; i++ )
		recalc_XPoints_of_curve( G.curve[ i ] );
}


void recalc_XPoints_of_curve( Curve_1d *cv )
{
	long j, k;


	cv->up = cv->down = cv->left = cv->right = UNSET;

	for ( k = 0, j = 0; j < G.nx; j++ )
	{
		if ( cv->points[ j ].exist )
		{
			cv->xpoints[ k ].x = shrt( cv->s2d_x * ( j + cv->x_shift ) );
			cv->xpoints[ k ].y = shrt( cv->s2d_y
						   * ( 1.0 - ( cv->points[ j ].y + cv->y_shift ) ) );

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

		cv->s2d_x = cv->old_s2d_x;
		cv->s2d_y = cv->old_s2d_y;
		cv->x_shift = cv->old_x_shift;
		cv->y_shift = cv->old_y_shift;

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

				cv->old_s2d_x = cv->s2d_x;
				cv->old_s2d_y = cv->s2d_y;
				cv->old_x_shift = cv->x_shift;
				cv->old_y_shift = cv->y_shift;

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

		cv->x_shift = cv->y_shift = 0.0;
		cv->s2d_x = ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
		cv->s2d_y = ( double ) ( G.canvas.h - 1 );

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


void fs_rescale_2d( void )
{
}


/*---------------------------------------------------------------------------*/
/* This is the function that takes the new data from the message queue and   */
/* displays them. On a call of this function all data sets in the queue will */
/* be accepted, if there is a REQUEST in between it will be moved to the end */
/* of the queue.                                                             */
/*---------------------------------------------------------------------------*/

void accept_new_data( void )
{
	void *buf,
		 *ptr,
		 *ptr_next;
	int i;
	int nsets;
	long x_index,
		 y_index,
		 curve;
	int type;
	long len;
	int mq_next;
	int shm_id;


	while ( 1 )
	{
		/* Attach to the shared memory segment */

		if ( ( buf = shmat( Message_Queue[ message_queue_low ].shm_id,
							NULL, SHM_RDONLY ) ) == ( void * ) - 1 )
		{
			shmctl( Message_Queue[ message_queue_low ].shm_id,
					IPC_RMID, NULL );                  /* delete the segment */
			eprint( FATAL, "Internal communication error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
		}

		/* Skip the magic number at the start and the total length field */

		ptr = buf + 4 * sizeof( char ) + sizeof( long );

		/* Get the number of data sets */

		nsets = *( ( int * ) ptr);
		ptr += sizeof( int );

		/* Accept the new data from each data set */

		for ( i = 0; i < nsets; ptr = ptr_next, i++ )
		{
			x_index = *( ( long * ) ptr );
			ptr += sizeof( long );

			y_index = *( ( long * ) ptr );
			ptr += sizeof( long );

			curve = *( ( long * ) ptr );
			ptr += sizeof( long );

			type = *( ( int * ) ptr );
			ptr += sizeof( int );
			
			switch ( type )
			{
				case INT_VAR :
					ptr_next = ptr + sizeof( long );
					break;

				case FLOAT_VAR :
					ptr_next = ptr + sizeof( double );
					break;

				case INT_TRANS_ARR :
					len = *( ( long * ) ptr );
					ptr_next = ptr + ( len + 1 ) * sizeof( long );
					break;

				case FLOAT_TRANS_ARR :
					len = *( ( long * ) ptr );
					ptr_next = ptr + sizeof( long ) + len * sizeof( double );
					break;
			}

			TRY
			{
				if ( G.dim == 1 )
					accept_1d_data( x_index, curve, type, ptr );
				else
					accept_2d_data( x_index, y_index, curve, type, ptr );
				TRY_SUCCESS;
			}
			CATCH( EXCEPTION )
			{
				shmdt( ( void * ) buf );
				shmctl( Message_Queue[ message_queue_low ].shm_id,
						IPC_RMID, NULL );
				THROW( EXCEPTION );
			}
		}

		/* Finally detach from shared memory segment and remove it */

		shmdt( ( void * ) buf );
		shmctl( Message_Queue[ message_queue_low ].shm_id, IPC_RMID, NULL );

		/* Increment the queue pointer */

		message_queue_low = ( message_queue_low + 1 ) % QUEUE_SIZE;

		/* Return if all entries in the message queue are used up */

		if ( message_queue_low == message_queue_high )
			break;

		/* Accept next data set if next entry in message queue is a data set */

		if ( Message_Queue[ message_queue_low ].type == DATA )
			continue;

		/* If the new entry is a REQUEST but the following a DATA set exchange
		   the entries and accept the data - this way the drawing of all data
		   is done before REQUESTs are honored */

		mq_next = ( message_queue_low + 1 ) % QUEUE_SIZE;

		if ( mq_next == message_queue_high )
			break;                                  /* REQUEST is last entry */

		/* Swap current REQUEST with next DATA set */

		shm_id = Message_Queue[ mq_next ].shm_id;

		Message_Queue[ mq_next ].shm_id =
			                         Message_Queue[ message_queue_low ].shm_id;
		Message_Queue[ mq_next ].type = REQUEST;

		Message_Queue[ message_queue_low ].shm_id = shm_id;
		Message_Queue[ message_queue_low ].type = DATA;
	}

	/* Finally display the new data by redrawing the canvas */

	redraw_canvas( &G.canvas );
}


void accept_1d_data( long x_index, long curve, int type, void *ptr )
{
	long len;
	long *l_data;
	double *f_data;

	double rw_y_max,
		   rw_y_min;
	void *cur_ptr;
	double data;
	double new_y_scale;
	Curve_1d *cv;
	long i, j;


	/* Check that the curve number is ok */

	if ( curve >= G.nc )
	{
		eprint( FATAL, "$s:%ld: Curve %ld not declared.\n", Fname, Lc,
				curve + ARRAY_OFFSET );
		THROW( EXCEPTION );
	}

	/* Get the amount of new data and a pointer to the start of the data */

	switch ( type )
	{
		case INT_VAR :
			len = 1;
			l_data = ( long * ) ptr;
			break;

		case FLOAT_VAR :
			len = 1;
			f_data = ( double * ) ptr;
			break;

		case INT_TRANS_ARR :
			len = *( ( long * ) ptr );
			ptr += sizeof( long );
			l_data = ( long * ) ptr;
			break;

		case FLOAT_TRANS_ARR :
			len = *( ( long * ) ptr );
			ptr += sizeof( long );
			f_data = ( double * ) ptr;
			break;
	}

	/* If the number of points exceeds the size of the arrays for the curves
	   we print an error message if the number was fixed by the call to
	   init_1d(), otherwise we have to increase the sizes for all curves */

	if ( x_index + len > G.nx )
	{
		if ( G.is_nx )       /* number of points has been fixed by init_1d() */
		{
			eprint( WARN, "%s:%ld: More points than declared in "
					"`init_1d()'.\n", Fname, Lc );
			G.is_nx = UNSET;
		}

		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			cv->points = T_realloc( cv->points,
								  ( x_index + len ) * sizeof( Scaled_Point ) );
			cv->xpoints = T_realloc( cv->xpoints,
										( x_index + len ) * sizeof( XPoint ) );
			cv->s2d_x = ( double ) ( G.canvas.w - 1 ) / 
				                                       ( double ) ( G.nx - 1 );
		}

		G.scale_changed = SET;
	}

	/* Find maximum and minimum of old and new data */

	rw_y_max = G.rw_y_max;
	rw_y_min = G.rw_y_min;

	for ( cur_ptr = ptr, i = 0; i < len; i++ )
	{
		if ( type & ( INT_VAR | INT_TRANS_ARR ) )
		{
			data = ( double ) *( ( long * ) cur_ptr );
			cur_ptr += sizeof( long );
		}
		else
		{
			data = *( ( double * ) cur_ptr );
			cur_ptr += sizeof( double );
		}

		rw_y_max = d_max( data, rw_y_max );
		rw_y_min = d_min( data, rw_y_min );
	}

	/* If the minimum or maximum changed rescale all old scaled data */

	if ( G.rw_y_max < rw_y_max || G.rw_y_min > rw_y_min )
	{
		if ( G.is_scale_set )
		{
			new_y_scale = 1.0 / ( rw_y_max - rw_y_min );

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];
				for ( j = 0; j < G.nx; j++ )
				{
					if ( cv->points[ j ].exist )
						cv->points[ j ].y = new_y_scale
							* ( cv->points[ j ].y / G.rw2s
								+ G.rw_y_min - rw_y_min );
				}

				if ( ! G.is_fs )
				{
					cv->s2d_y *= G.rw2s / new_y_scale;
					cv->y_shift *= new_y_scale / G.rw2s;
					if ( rw_y_max > G.rw_y_max )
						cv->y_shift += ( rw_y_max - G.rw_y_max ) * new_y_scale;
				}
			}

			G.rw2s = new_y_scale;
		}

		if ( ! G.is_scale_set && rw_y_max != rw_y_min )
		{
			new_y_scale = 1.0 / ( rw_y_max - rw_y_min );

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];
				for ( j = 0; j < G.nx; j++ )
					if ( cv->points[ j ].exist )
						cv->points[ j ].y = new_y_scale
							                * ( cv->points[ j ].y - rw_y_min );
			}

			G.rw2s = new_y_scale;
			G.is_scale_set = SET;
		}

		G.rw_y_min = rw_y_min;
		G.rw_y_max = rw_y_max;
		G.scale_changed = SET;
	}

	/* Include the new data into the scaled data */

	for ( cur_ptr = ptr, i = x_index; i < x_index + len; i++ )
	{
		if ( type & ( INT_VAR | INT_TRANS_ARR ) )
		{
			data = ( double ) *( ( long * ) cur_ptr );
			cur_ptr += sizeof( long );
		}
		else
		{
			data = *( ( double * ) cur_ptr );
			cur_ptr += sizeof( double );
		}

		if ( G.is_scale_set )
			G.curve[ curve ]->points[ i ].y = G.rw2s * ( data - G.rw_y_min );
		else
			G.curve[ curve ]->points[ i ].y = data;

		/* Increase the point count if the point is new and mark it as set */

		if ( ! G.curve[ curve ]->points[ i ].exist )
		{
			G.curve[ curve ]->count++;
			G.curve[ curve ]->points[ i ].exist = SET;
		}
	}

	/* Calculate new points for display */

	if ( ! G.is_scale_set )
		return;

	for ( i = 0; i < G.nc; i++ )
	{
		if ( ! G.scale_changed && i != curve )
			continue;
		recalc_XPoints_of_curve( G.curve[ i ] );
	}

	G.scale_changed = UNSET;
}


void accept_2d_data( long x_index, long y_index, long curve, int type,
					 void *ptr )
{
}


void curve_button_callback( FL_OBJECT *obj, long data )
{
	obj = obj;
	G.curve[ data - 1 ]->active ^= SET;
}
