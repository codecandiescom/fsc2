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
static void create_label_pixmap( int coord );
static void setup_canvas( Canvas *c, FL_OBJECT *obj );
static void canvas_off( Canvas *c, FL_OBJECT *obj );
static void create_pixmap( Canvas *c );
static void delete_pixmap( Canvas *c );
static void fs_rescale_1d( void );
static void fs_rescale_2d( void );
static void redraw_axis( int coord );
static void make_scale( Curve_1d *cv, Canvas *c, int coord );
static void make_label_string( char *lstr, double num, int res );


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
	fl_set_object_helper( run_form->undo_button,
						  "Undo last rescaling operation" );
	fl_set_object_helper( run_form->full_scale_button,
						  "Switch off automatic rescaling" );
	if ( G.dim == 1 )
	{
		fl_set_object_helper( run_form->curve_1_button, 
							  "Extempt curve 1 from\nrescaling operations" );
		fl_set_object_helper( run_form->curve_2_button, 
							  "Extempt curve 2 from\nrescaling operations" );
		fl_set_object_helper( run_form->curve_3_button, 
							  "Extempt curve 3 from\nrescaling operations" );
		fl_set_object_helper( run_form->curve_4_button, 
							  "Extempt curve 4 from\nrescaling operations" );
	}

	/* fdesign is unable to set the box type attributes for canvases... */

	fl_set_canvas_decoration( run_form->x_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->y_axis, FL_FRAME_BOX );
	if ( G.dim == 2 )
		fl_set_canvas_decoration( run_form->z_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->canvas, FL_NO_FRAME );

	/* Show only the needed buttons */

	if ( G.is_init )
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

		if ( G.dim == 2 )
		{
			fl_set_object_size( run_form->canvas, run_form->canvas->w
								- run_form->z_axis->w - 5,
								run_form->canvas->h );
			fl_set_object_size( run_form->x_axis, run_form->x_axis->w
								- run_form->z_axis->w - 5,
								run_form->x_axis->h );
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
		G.font = XLoadQueryFont( G.d, "*-lucida-bold-r-normal-sans-14-*" );
		if ( G.font == NULL )
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
	if ( G.dim == 2 )
		setup_canvas( &G.canvas, run_form->z_axis );

	if ( G.is_init )
		G_struct_init( );

	redraw_all_1d( );

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
	G.scale_changed = SET;

	G.rw_y_min = HUGE_VAL;
	G.rw_y_max = - HUGE_VAL;
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
		if ( G.font != NULL )
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

	if ( G.label[ Y ] != NULL && G.font != NULL )
		create_label_pixmap( Y );

	first_time = UNSET;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void create_label_pixmap( int coord )
{
	Pixmap pm;
	int width, height;
	Canvas *c;
	int i, j, k;

	
	assert( coord == Y || coord == Z );

	if ( coord == Y )
		c = &G.y_axis;
	else
		c = &G.z_axis;

	/* Get size for intermediate pixmap */

	width = XTextWidth( G.font, G.label[ coord ],
						strlen( G.label[ coord ] ) ) + 10;
	height = G.font_asc + G.font_desc + 5;

	/* Create the intermediate pixmap, fill with color of the axis canvas and
	   draw  the text */

    pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), width, height,
						fl_get_canvas_depth( c->obj ) );

	XFillRectangle( G.d, pm, c->gc, 0, 0, width, height );
	XDrawString( G.d, pm, c->font_gc, 5, height - 1 - G.font_desc, 
				 G.label[ coord ], strlen( G.label[ coord ] ) );

	/* Create the real pixmap for the label */

    G.label_pm[ coord ] = XCreatePixmap( G.d, FL_ObjWin( c->obj ), height,
										width, fl_get_canvas_depth( c->obj ) );
	G.label_w[ coord ] = ( unsigned int ) height;
	G.label_h[ coord ] = ( unsigned int ) width;

	/* Now copy the contents of the intermediate pixmap to the final pixmap
	   but rotated by 90 degree ccw */

	for ( i = 0, k = width - 1; i < width; k--, i++ )
		for ( j = 0; j < height; j++ )
			XCopyArea( G.d, pm, G.label_pm[ coord ], c->gc, i, j, 1, 1, j, k );

	XFreePixmap( G.d, pm );
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
	int coord;
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

	if ( G.font != NULL )
		for ( coord = Y; coord <= Z; coord++ )
			if ( G.label[ coord ] != NULL )
				XFreePixmap( G.d, G.label_pm[ coord ] );

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
	if ( G.dim == 1 )
		fl_remove_canvas_handler( obj, Expose, canvas_handler_1d );

	if ( G.is_init )
	{
		if ( G.dim == 1 )
		{
			fl_remove_canvas_handler( obj, ConfigureNotify,
									  canvas_handler_1d );
			fl_remove_canvas_handler( obj, ButtonPress,
									  canvas_handler_1d );
			fl_remove_canvas_handler( obj, ButtonRelease,
									  canvas_handler_1d );
			fl_remove_canvas_handler( obj, MotionNotify,
									  canvas_handler_1d );
		}

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

	if ( G.dim == 1 )
		fl_add_canvas_handler( c->obj, Expose, canvas_handler_1d,
							   ( void * ) c );

	if ( G.is_init )
	{
		c->is_box = UNSET;

		fl_remove_selected_xevent( FL_ObjWin( obj ),
								   PointerMotionMask | PointerMotionHintMask );
		fl_add_selected_xevent( FL_ObjWin( obj ),
								Button1MotionMask | Button2MotionMask );

		if ( G.dim == 1 )
		{
			fl_add_canvas_handler( c->obj, ConfigureNotify, canvas_handler_1d,
								   ( void * ) c );
			fl_add_canvas_handler( c->obj, ButtonPress, canvas_handler_1d,
								   ( void * ) c );
			fl_add_canvas_handler( c->obj, ButtonRelease, canvas_handler_1d,
								   ( void * ) c );
			fl_add_canvas_handler( c->obj, MotionNotify, canvas_handler_1d,
								   ( void * ) c );
		}

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

void reconfigure_window_1d( Canvas *c, int w, int h )
{
	long i;
	Curve_1d *cv;
	static bool is_reconf[ 3 ] = { UNSET, UNSET, UNSET };
	static bool need_redraw[ 3 ] = { UNSET, UNSET, UNSET };
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

		if ( need_redraw[ Z ] )
		{
			redraw_canvas_1d( &G.z_axis );
			need_redraw[ Z ] = UNSET;
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

	if ( G.dim == 2 && c == &G.z_axis )
	{
		if ( is_reconf[ Z ] )
		{
			redraw_canvas_1d( c );
			is_reconf[ Z ] = UNSET;
		}
		else
			need_redraw[ Z ] = SET;
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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void redraw_axis( int coord )
{
	Canvas *c;
	Curve_1d *cv;
	int width;
	int i;


	if ( coord == X )
	{
		c = &G.x_axis;
		if ( G.label[ X ] != NULL && G.font != NULL )
		{
			width = XTextWidth( G.font, G.label[ X ], strlen( G.label[ X ] ) );
			XDrawString( G.d, c->pm, c->font_gc, c->w - width - 5,
						 c->h - 5 - G.font_desc, 
						 G.label[ X ], strlen( G.label[ X ] ) );
		}
	}
	else
	{
		if ( coord == Y )
			c = &G.y_axis;
		else
			c = &G.z_axis;
		
		if ( G.label[ coord ] != NULL && G.font != NULL )
			XCopyArea( G.d, G.label_pm[ coord ], c->pm, c->gc, 0, 0,
					   G.label_w[ coord ], G.label_h[ coord ], 0, 0 );
	}

	if ( ! G.is_scale_set )                   /* no scaling -> no scale */
		return;

	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];
		if ( cv->active )
			break;
	}

	if ( i == G.nc )                          /* no active curve -> no scale */
		return;

	make_scale( cv, c, coord );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void make_scale( Curve_1d *cv, Canvas *c, int coord )
{
	double rwc_delta,          /* distance between small ticks (in rwc) */
		   order,              /* and its order of magitude */
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


	/* The distance between the smallest ticks should be about 8 points -
	   calculate the corresponding delta in real word units */

	rwc_delta = 7.0 * fabs( G.rwc_delta[ coord ] ) / cv->s2d[ coord ];

	/* Now scale this distance to the interval [ 1, 10 [ */

	mag = floor( log10( rwc_delta ) );
	order = pow( 10.0, mag );
	modf( rwc_delta / order, &rwc_delta );

	/* Now get a `smooth' value for the ticks distance, i.e. either 2, 2.5,
	   5 or 10 and convert it to real world coordinates */

	if ( rwc_delta <= 2.0 )       /* in [ 1, 2 ] -> units of 2 */
	{
		medium_factor = 5;
		coarse_factor = 25;
		rwc_delta = 2.0 * order;
	}
	else if ( rwc_delta <= 3.0 )  /* in ] 2, 2.5 ] -> units of 2.5 */
	{
		medium_factor = 4;
		coarse_factor = 20;
		rwc_delta = 2.5 * order;
	}
	else if ( rwc_delta <= 6.0 )  /* in ] 3, 5 ] -> units of 5 */
	{
		medium_factor = 2;
		coarse_factor = 10;
		rwc_delta = 5.0 * order;
	}
	else                          /* in ] 5, 10 [ -> units of 10 */
	{
		medium_factor = 5;
		coarse_factor = 10;
		rwc_delta = 10.0 * order;
	}

	/* Calculate the distance between the small ticks in points */

	d_delta_fine = cv->s2d[ coord ] * rwc_delta / fabs( G.rwc_delta[ coord ] );


	/* `rwc_start' is the first value in the display (i.e. the smallest x or y
	   value still shown in the canvas), `rwc_start_fine' the position of the
	   first small tick (both in real world coordinates) and, finally,
	   `d_start_fine' is the same position but in points */

	rwc_start = G.rwc_start[ coord ]
		        - cv->shift[ coord ] * G.rwc_delta[ coord ];
	if ( G.rwc_delta[ coord ] < 0 )
		rwc_delta *= -1.0;

	modf( rwc_start / rwc_delta, &rwc_start_fine );
	rwc_start_fine *= rwc_delta;

	d_start_fine = cv->s2d[ coord ] * ( rwc_start_fine - rwc_start ) /
		                                                  G.rwc_delta[ coord ];
	if ( lround( d_start_fine ) < 0 )
		d_start_fine += d_delta_fine;


	/* Calculate start index for first medium tick */

	modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
	rwc_start_medium *= medium_factor * rwc_delta;

	d_start_medium = cv->s2d[ coord ] * ( rwc_start_medium - rwc_start ) /
			                                              G.rwc_delta[ coord ];
	if ( lround( d_start_medium ) < 0 )
		d_start_medium += medium_factor * d_delta_fine;

	medium = lround( ( d_start_fine - d_start_medium ) / d_delta_fine );

	/* Calculate start index for first large tick */

	modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
	rwc_start_coarse *= coarse_factor * rwc_delta;

	d_start_coarse = cv->s2d[ coord ] * ( rwc_start_coarse - rwc_start ) /
			                                              G.rwc_delta[ coord ];
	if ( lround( d_start_coarse ) < 0 )
	{
		d_start_coarse += coarse_factor * d_delta_fine;
		rwc_start_coarse += coarse_factor * rwc_delta;
	}

	coarse = lround( ( d_start_fine - d_start_coarse ) / d_delta_fine );


	/* Now, finally we got everything we need to draw the axis... */

	rwc_coarse = rwc_start_coarse;

	if ( coord == X )
	{
		y = 20;
		XFillRectangle( G.d, c->pm, cv->gc, 0, y - 2, c->w, 3 );

		for ( cur_p = d_start_fine; cur_p < c->w; 
			  medium++, coarse++, cur_p += d_delta_fine )
		{
			x = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )
			{
				XDrawLine( G.d, c->pm, c->font_gc, x, y + 3, x, y - 14 );
				if ( G.font == NULL )
				{
					rwc_coarse += coarse_factor * rwc_delta;
					continue;
				}

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				if ( x - ( width >> 1 ) - 10 > last )
				{
					XDrawString( G.d, c->pm, c->font_gc, x - ( width >> 1 ),
								 y + 7 + G.font_asc, lstr, strlen( lstr ) );
					last = x + ( width >> 1 );
				}
				rwc_coarse += coarse_factor * rwc_delta;
			}
			else if ( medium % medium_factor == 0 )
				XDrawLine( G.d, c->pm, c->font_gc, x, y, x, y - 10 );
			else
				XDrawLine( G.d, c->pm, c->font_gc, x, y, x, y - 5 );
		}
	}
	else
	{
		x = c->w - 21;
		XFillRectangle( G.d, c->pm, cv->gc, x, 0, 3, c->h );
		XDrawLine( G.d, c->pm, cv->gc, x, 0, x, c->h - 1 );

		for ( cur_p = c->h - 1 - d_start_fine; cur_p >= 0; 
			  medium++, coarse++, cur_p -= d_delta_fine )
		{
			y = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )
			{
				XDrawLine( G.d, c->pm, c->font_gc, x - 3, y, x + 14, y );

				if ( G.font == NULL )
				{
					rwc_coarse += coarse_factor * rwc_delta;
					continue;
				}

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				XDrawString( G.d, c->pm, c->font_gc, x - 7 - width,
							 y + ( G.font_asc >> 1 ) , lstr, strlen( lstr ) );
				rwc_coarse += coarse_factor * rwc_delta;
			}
			else if ( medium % medium_factor == 0 )
				XDrawLine( G.d, c->pm, c->font_gc, x, y, x + 10, y );
			else
				XDrawLine( G.d, c->pm, c->font_gc, x, y, x + 5, y );
		}
	}
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

	mag = ( int ) floor( log10( fabs( num ) ) );
	if ( mag >= 0 )
		mag++;

	/* n is the number of relevant digits */

	n = i_max( 1, mag - res );

	if ( mag > 5 )                              /* num > 10^5 */
		sprintf( lstr, "%1.*E", n - 1, num );
	else if ( mag > 0 )                         /* num >= 1 */
	{
		if ( res >= 0 )
			sprintf( lstr, "%*g", mag, num );
		else
			sprintf( lstr, "%*.*f\n", n, abs( res ), num );
	}
	else if ( mag > -4 && res >= - 4 )          /* num > 10^-4 */
		sprintf( lstr, "%1.*f", abs( res ), num );
	else                                        /* num < 10^-4 */
		if ( mag < res )
			sprintf( lstr, "0" );
		else
			sprintf(  lstr, "%1.*E", n - 1, num );
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
			repaint_canvas_1d( &G.x_axis );
		}

		if ( G.drag_canvas == 2 )
		{
			fl_reset_cursor( FL_ObjWin( G.y_axis.obj ) );
			G.y_axis.is_box = UNSET;
			repaint_canvas_1d( &G.y_axis );
		}

		if ( G.drag_canvas == 3 )
		{
			fl_reset_cursor( FL_ObjWin( G.canvas.obj ) );
			G.canvas.is_box = UNSET;
			repaint_canvas_1d( &G.canvas );
		}
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


/*-------------------------------------------*/
/* Undos the last action as far as possible. */
/*-------------------------------------------*/

void undo_button_callback( FL_OBJECT *a, long b )
{
	long i;
	bool is_undo = UNSET;
	Curve_1d *cv;
	double temp_s2d[ 2 ],
		   temp_shift[ 2 ];

	a = a;
	b = b;


	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve[ i ];

		if ( ! cv->can_undo )
			continue;

		temp_s2d[ X ] = cv->s2d[ X ];
		temp_s2d[ Y ] = cv->s2d[ Y ];
		temp_shift[ X ] = cv->shift[ X ];
		temp_shift[ Y ] = cv->shift[ Y ];

		cv->s2d[ X ] = cv->old_s2d[ X ];
		cv->s2d[ Y ] = cv->old_s2d[ Y ];
		cv->shift[ X ] = cv->old_shift[ X ];
		cv->shift[ Y ] = cv->old_shift[ Y ];

		cv->old_s2d[ X ] = temp_s2d[ X ];
		cv->old_s2d[ Y ] = temp_s2d[ Y ];
		cv->old_shift[ X ] = temp_shift[ X ];
		cv->old_shift[ Y ] = temp_shift[ Y ];

		is_undo = SET;

		recalc_XPoints_of_curve_1d( cv );
	}

	if ( is_undo && G.is_fs )
	{
		G.is_fs = UNSET;
		fl_set_button( run_form->full_scale_button, 0 );
	}

	if ( is_undo )
		redraw_all_1d( );
}


/*-------------------------------------------------------------*/
/* Rescales the display so that the canvas is completely used. */
/*-------------------------------------------------------------*/

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
			fl_set_object_helper( run_form->full_scale_button,
								  "Switch off automatic rescaling" );
		}
		else                     /* if button got switched off */
		{
			G.is_fs = UNSET;
			fl_set_object_helper( run_form->full_scale_button,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
		}
	}
	else
		fs_rescale_2d( );

	/* Redraw the graphic */

	redraw_all_1d( );
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


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void fs_rescale_2d( void )
{
}




void curve_button_callback( FL_OBJECT *obj, long data )
{
	char hstr[ 128 ];

	obj = obj;
	G.curve[ data - 1 ]->active ^= SET;

	if ( fl_get_button( obj ) )
		sprintf( hstr, "Extempt curve %ld from\nrescaling operations", data );
	else
		sprintf( hstr, "Apply rescaling operations\nto curve %ld", data );

	fl_set_object_helper( obj, hstr );

	redraw_canvas_1d( &G.x_axis );
	redraw_canvas_1d( &G.y_axis );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void clear_curve( long curve )
{
	long i;
	Curve_1d *cv = G.curve[ curve ];


	if ( curve < 0 || curve >= G.nc )
	{
		eprint( WARN, "%s:%ld: Can't clear curve %ld, curve does not exist.\n",
				Fname, Lc, curve + 1 );
		return;
	}

	if ( TEST_RUN )
		return;

	if ( I_am == PARENT )
	{
		for ( i = 0; i < G.nx; i++ )
			cv->points[ i ].exist = UNSET;
		cv->count = 0;
	}
	else
		writer( C_CLEAR_CURVE, curve );

	redraw_canvas_1d( &G.canvas );
}
