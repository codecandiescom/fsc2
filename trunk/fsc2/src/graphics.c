/*
  $Id$
*/


#include "fsc2.h"

#include "c1.xbm"             /* bitmaps for cursors */
#include "c2.xbm"
#include "c3.xbm"
#include "c4.xbm"
#include "c5.xbm"

#include "ua.xbm"             /* arrow bitmaps */
#include "da.xbm"
#include "la.xbm"
#include "ra.xbm"


static void G_struct_init( void );
static void create_label_pixmap( int coord );
static void setup_canvas( Canvas *c, FL_OBJECT *obj );
static void canvas_off( Canvas *c, FL_OBJECT *obj );
static void fs_rescale_2d( void );


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
	else
	{
		fl_set_object_helper( run_form->curve_1_button, "Show curve 1" );
		fl_set_object_helper( run_form->curve_2_button, "Show curve 2" );
		fl_set_object_helper( run_form->curve_3_button, "Show curve 3" );
		fl_set_object_helper( run_form->curve_4_button, "Show curve 4" );

		fl_set_button( run_form->curve_2_button, 0 );
		fl_set_button( run_form->curve_3_button, 0 );
		fl_set_button( run_form->curve_4_button, 0 );

		G.active_curve = 0;
	}

	/* fdesign is unable to set the box type attributes for canvases... */

	fl_set_canvas_decoration( run_form->x_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->y_axis, FL_FRAME_BOX );
	if ( G.dim == 1 )
		fl_delete_object( run_form->z_axis );
	else
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
	if ( G.dim == 2 )
		setup_canvas( &G.canvas, run_form->z_axis );
	setup_canvas( &G.canvas, run_form->canvas );

	if ( G.is_init )
		G_struct_init( );

	if ( G.dim == 1 )
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
		G.ua_w = ua_width;
		G.ua_h = ua_width;

		cv->down_arr =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, da_bits,
										 da_width, da_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		G.da_w = da_width;
		G.da_h = da_width;

		cv->left_arr =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, la_bits,
										 la_width, la_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		G.la_w = la_width;
		G.la_h = la_width;

		cv->right_arr = 
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, ra_bits,
										 ra_width, ra_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		G.ra_w = ra_width;
		G.ra_h = ra_width;

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
	XSetWindowAttributes attributes;


	c->obj = obj;

	c->w = obj->w;
	c->h = obj->h;
	create_pixmap( c );

	if ( G.dim == 1 )
		fl_add_canvas_handler( c->obj, Expose, canvas_handler_1d,
							   ( void * ) c );

	if ( G.is_init )
	{

		attributes.backing_store = NotUseful;
		attributes.background_pixmap = None;
		XChangeWindowAttributes( G.d, FL_ObjWin( c->obj ),
								 CWBackingStore | CWBackPixmap,
								 &attributes );

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void redraw_axis( int coord )
{
	Canvas *c;
	Curve_1d *cv;
	int width;
	int i;


	/* First draw the label - for the x-axis it's just done by drawing the
	   string while for the y- and z-axis we have to copy a pixmap since the
	   label is a string rotated by 90 degree which we have drawn in advance */

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

	/* Find out the active curve for the label */

	if ( G.dim == 1 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];
			if ( cv->active )
				break;
		}

		if ( i == G.nc )                      /* no active curve -> no scale */
			return;
	}
	else
		cv = G.curve[ G.active_curve ];

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

	if ( G.dim == 1 )
	{
		/* Change the help string for the button */

		if ( fl_get_button( obj ) )
			sprintf( hstr, "Extempt curve %ld from\nrescaling operations",
					 data );
		else
			sprintf( hstr, "Apply rescaling operations\nto curve %ld", data );

		fl_set_object_helper( obj, hstr );

		/* Redraw both axis to make sure the axis fo the first active button
		   is shown */

		redraw_canvas_1d( &G.x_axis );
		redraw_canvas_1d( &G.y_axis );
	}
	else
	{
		

		if ( data == G.active_curve )         /* shown curve is switched off */
			G.active_curve = -1;
		else
		{
			switch ( G.active_curve )
			{
				case 0 :
					fl_set_button( run_form->curve_1_button, 0 );
					break;

				case 1 :
					fl_set_button( run_form->curve_2_button, 0 );
					break;

				case 2 :
					fl_set_button( run_form->curve_3_button, 0 );
					break;

				case 3 :
					fl_set_button( run_form->curve_4_button, 0 );
					break;
			}

			G.active_curve = data;
		}
	}

}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void clear_curve( long curve )
{
	long i;
	Curve_1d *cv = G.curve[ curve ];


	for ( i = 0; i < G.nx; i++ )
		cv->points[ i ].exist = UNSET;
	cv->count = 0;
}
