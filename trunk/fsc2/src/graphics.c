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
static void G_init_curves_1d( void );
static void G_init_curves_2d( bool first_time );
static void create_label_pixmap( int coord );
static void setup_canvas( Canvas *c, FL_OBJECT *obj );
static void create_colors( void );
static void canvas_off( Canvas *c, FL_OBJECT *obj );


/*----------------------------------------------------------------------*/
/* Initializes and shows the window for displaying measurement results. */
/*----------------------------------------------------------------------*/

void start_graphics( void )
{
	XCharStruct font_prop;
	int dummy;


	/* Create the form for running experiments */

	run_form = create_form_run( );

	/* It still need some modifications... */

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

	/* Set minimum size for display window switch on the full scale button */

	if ( G.dim == 1 )
		fl_winminsize( run_form->run->window, 400, 335 );
	else
		fl_winminsize( run_form->run->window, 500, 335 );
	fl_set_button( run_form->full_scale_button, 1 );

	/* Load a font hopefully available on all machines (we try at least three,
	 in principal this should be made user configurable) */

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

	/* Create the canvas and the axes */

	setup_canvas( &G.x_axis, run_form->x_axis );
	setup_canvas( &G.y_axis, run_form->y_axis );
	if ( G.dim == 2 )
	{
		setup_canvas( &G.z_axis, run_form->z_axis );
	}
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
	}

	/* Define colors for the curves (in principal this should be made
       configurable by the user) */

	G.colors[ 0 ] = FL_TOMATO;
	G.colors[ 1 ] = FL_GREEN;
	G.colors[ 2 ] = FL_YELLOW;
	G.colors[ 3 ] = FL_CYAN;

	G.is_fs = SET;
	G.scale_changed = UNSET;

	G.rw_min = HUGE_VAL;
	G.rw_max = - HUGE_VAL;
	G.is_scale_set = UNSET;

	if ( G.dim == 1 )
		G_init_curves_1d( );
	else
		G_init_curves_2d( first_time );

	if ( G.label[ Y ] != NULL && G.font != NULL )
		create_label_pixmap( Y );

	if ( G.dim == 2 && G.label[ Z ] != NULL && G.font != NULL )
		create_label_pixmap( Z );

	first_time = UNSET;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void G_init_curves_1d( void )
{
	int i, j;
	Curve_1d *cv;
	unsigned int depth = fl_get_canvas_depth( G.canvas.obj );



	fl_set_cursor_color( G.cur_1, FL_RED, FL_WHITE );
	fl_set_cursor_color( G.cur_2, FL_RED, FL_WHITE );
	fl_set_cursor_color( G.cur_3, FL_RED, FL_WHITE );
	fl_set_cursor_color( G.cur_4, FL_RED, FL_WHITE );
	fl_set_cursor_color( G.cur_5, FL_RED, FL_WHITE );

	for ( i = 0; i < G.nc; i++ )
	{
		/* Allocate memory for the curve and its data */

		cv = G.curve[ i ] = T_malloc( sizeof( Curve_1d ) );

		cv->points = NULL;
		cv->points = T_malloc( G.nx * sizeof( Scaled_Point ) );

		for ( j = 0; j < G.nx; j++ )           /* no points are known in yet */
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
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void G_init_curves_2d( bool first_time )
{
	int i, j;
	Curve_2d *cv;
	Scaled_Point *sp;
	unsigned int depth = fl_get_canvas_depth( G.canvas.obj );


	if ( first_time )
		create_colors( );

	fl_set_cursor_color( G.cur_1, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_2, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_3, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_4, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_5, FL_BLACK, FL_WHITE );

	for ( i = 0; i < G.nc; i++ )
	{
		/* Allocate memory for the curve and its data */

		cv = G.curve_2d[ i ] = T_malloc( sizeof( Curve_2d ) );

		cv->points = NULL;
		cv->points = T_malloc( G.nx * G.ny * sizeof( Scaled_Point ) );

		for ( sp = cv->points, j = 0; j < G.nx * G.ny; sp++, j++ )
			sp->exist = UNSET;

		cv->xpoints = NULL;
		cv->xpoints = T_malloc( G.nx * G.ny * sizeof( XPoint ) );
		cv->xpoints_s = NULL;
		cv->xpoints_s = T_malloc( G.nx * G.ny * sizeof( XPoint ) );

		/* Create a GC for drawing the curve and set its color */

		cv->gc = XCreateGC( G.d, G.canvas.pm, 0, 0 );
		XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ i ] ) );

		/* Create pixmaps for the out-of-display arrows */

		cv->up_arr =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, ua_bits,
										 ua_width, ua_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );
		G.ua_w = ua_width;
		G.ua_h = ua_width;

		cv->down_arr =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, da_bits,
										 da_width, da_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );

		G.da_w = da_width;
		G.da_h = da_width;

		cv->left_arr =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, la_bits,
										 la_width, la_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );

		G.la_w = la_width;
		G.la_h = la_width;

		cv->right_arr = 
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, ra_bits,
										 ra_width, ra_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );

		G.ra_w = ra_width;
		G.ra_h = ra_width;

		/* Create a GC for the font and set the appropriate color */

		cv->font_gc = XCreateGC( G.d, FL_ObjWin( G.canvas.obj ), 0, 0 );
		if ( G.font != NULL )
			XSetFont( G.d, cv->font_gc, G.font->fid );
		XSetForeground( G.d, cv->font_gc, fl_get_pixel( FL_WHITE ) );
		XSetBackground( G.d, cv->font_gc, fl_get_pixel( FL_BLACK ) );

		/* Set the scaling factors for the curve */

		cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
		cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 ) / ( double ) ( G.ny - 1 );
		cv->s2d[ Z ] = ( double ) ( G.z_axis.h - 1 );

		cv->shift[ X ] = cv->shift[ Y ] = cv->shift[ Z ] = 0.0;

		cv->z_factor = 1.0;

		cv->rwc_start[ X ] = G.rwc_start[ X ];
		cv->rwc_start[ Y ] = G.rwc_start[ Y ];
		cv->rwc_delta[ X ] = G.rwc_delta[ X ];
		cv->rwc_delta[ Y ] = G.rwc_delta[ Y ];

		cv->count = 0;
		cv->active = SET;
		cv->can_undo = UNSET;

		cv->is_fs = SET;
		cv->scale_changed = UNSET;

		cv->rw_min = HUGE_VAL;
		cv->rw_max = - HUGE_VAL;
		cv->is_scale_set = UNSET;
	}
}


/*----------------------------------------------------------------------*/
/* This routine creates a pixmap with the label for either the y- or z- */
/* axis (set `coord' to 1 for y and 2 for z). The problem is that it's  */
/* not possible to draw rotated text so we have to write the text to a  */
/* pixmap and then rotate this pixmap 'by hand'.                        */
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
	   draw the text */

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

		if ( G.dim == 2 )
			canvas_off( &G.z_axis, run_form->z_axis );
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
	Curve_2d *cv2;


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
				G.curve_2d[ i ] = NULL;
			}
		}
	}
	else
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv2 = G.curve_2d[ i ];

			XFreeGC( G.d, cv2->gc );

			XFreePixmap( G.d, cv2->up_arr );
			XFreePixmap( G.d, cv2->down_arr );
			XFreePixmap( G.d, cv2->left_arr );
			XFreePixmap( G.d, cv2->right_arr );

			XFreeGC( G.d, cv2->font_gc );

			if ( cv2 != NULL )
			{
				if ( cv2->points != NULL )
					T_free( cv2->points );

				if ( cv2->xpoints != NULL )
					T_free( cv2->xpoints );
				if ( cv2->xpoints_s != NULL )
					T_free( cv2->xpoints_s );

				T_free( cv2 );
				G.curve_2d[ i ] = NULL;
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
	int i;


	G.is_init = UNSET;

	for ( i = X; i <=Z; i++ )
		if ( G.label[ i ] != NULL )
		{
			T_free( G.label[ i ] );
			G.label[ i ] = NULL;
		}
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

void canvas_off( Canvas *c, FL_OBJECT *obj )
{
	FL_HANDLE_CANVAS ch;


	if ( G.dim == 1 )
		ch = canvas_handler_1d;
	else
		ch = canvas_handler_2d;

	fl_remove_canvas_handler( obj, Expose, ch );

	if ( G.is_init )
	{
		if ( G.dim == 1 )
		{
			fl_remove_canvas_handler( obj, ConfigureNotify, ch );
			fl_remove_canvas_handler( obj, ButtonPress, ch );
			fl_remove_canvas_handler( obj, ButtonRelease, ch );
			fl_remove_canvas_handler( obj, MotionNotify, ch );
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
	FL_HANDLE_CANVAS ch;


	if ( G.dim == 1 )
		ch = canvas_handler_1d;
	else
		ch = canvas_handler_2d;

	c->obj = obj;

	c->w = obj->w;
	c->h = obj->h;
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

		fl_remove_selected_xevent( FL_ObjWin( obj ),
								   PointerMotionMask | PointerMotionHintMask );
		fl_add_selected_xevent( FL_ObjWin( obj ),
								Button1MotionMask | Button2MotionMask );

		fl_add_canvas_handler( c->obj, ConfigureNotify, ch, ( void * ) c );
			fl_add_canvas_handler( c->obj, ButtonPress, ch, ( void * ) c );
			fl_add_canvas_handler( c->obj, ButtonRelease, ch, ( void * ) c );
			fl_add_canvas_handler( c->obj, MotionNotify, ch, ( void * ) c );

		c->font_gc = XCreateGC( G.d, FL_ObjWin( obj ), 0, 0 );
		XSetForeground( G.d, c->font_gc, fl_get_pixel( FL_BLACK ) );
		XSetFont( G.d, c->font_gc, G.font->fid );
	}
}


/*----------------------------------------------*/
/* Creates a pixmap for a canvas for buffering. */
/*----------------------------------------------*/

void create_pixmap( Canvas *c )
{
	char dashes[ ] = { 2, 2 };


    c->gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, 0 );
    c->pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h,
						   fl_get_canvas_depth( c->obj ) );

	if ( c == &G.canvas )
	{
		if ( G.is_init && G.dim == 1 )
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
		else
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_INACTIVE ) );
	}
	else
		XSetForeground( G.d, c->gc, fl_get_pixel( FL_LEFT_BCOL ) );

	if ( G.is_init )
	{
		c->box_gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, 0 );

		if ( G.dim == 1 )
			XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );
		else
			XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_BLACK ) );
		XSetLineAttributes( G.d, c->box_gc, 0, LineOnOffDash, CapButt,
							JoinMiter );
		XSetDashes( G.d, c->box_gc, 0, dashes, 2 );
	}
}


/*------------------------------------------------*/
/* Deletes the pixmap for a canvas for buffering. */
/*------------------------------------------------*/

void delete_pixmap( Canvas *c )
{
	XFreeGC( G.d, c->gc );
	XFreePixmap( G.d, c->pm );

	if ( G.is_init )
		XFreeGC( G.d, c->box_gc );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void create_colors( void )
{
    FL_COLOR i;
	int rgb[ 3 ];


	/* Create the colors between blue and red */

	for ( i = 0; i < NUM_COLORS; i++ )
	{
		i2rgb( ( double ) i / ( double ) ( NUM_COLORS - 1 ), rgb );
		fl_mapcolor( i + FL_FREE_COL1 + 1, 
					 rgb[ RED ], rgb[ GREEN ], rgb[ BLUE ] );
	}

	/* Create a creamy kind of white and a dark violet for values too large
	   or too small */

	fl_mapcolor( NUM_COLORS + FL_FREE_COL1 + 1, 255, 248, 220 );
	fl_mapcolor( NUM_COLORS + FL_FREE_COL1 + 2, 96, 0, 96 );
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
	   label is a string rotated by 90 degree thta has been drawn in advance */

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
	else if ( coord == Y )
	{
		c = &G.y_axis;
		if ( G.label[ coord ] != NULL && G.font != NULL )
			XCopyArea( G.d, G.label_pm[ coord ], c->pm, c->gc, 0, 0,
					   G.label_w[ coord ], G.label_h[ coord ], 0, 0 );
	}
	else
	{
		c = &G.z_axis;
		if ( G.label[ coord ] != NULL && G.font != NULL )
			XCopyArea( G.d, G.label_pm[ coord ], c->pm, c->gc, 0, 0,
					   G.label_w[ coord ], G.label_h[ coord ],
					   c->w - 5 - G.label_w[ coord ], 0 );
	}

	if ( ( G.dim == 1 && ! G.is_scale_set ) ||
		 ( G.dim == 2 && ( G.active_curve == -1 ||
						   ! G.curve_2d[ G.active_curve ]->is_scale_set ) ) )
		return;

	/* Find out the active curve for the axis */

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
		make_scale_1d( cv, c, coord );
	}
	else if ( G.active_curve != -1 )
		make_scale_2d( G.curve_2d[ G.active_curve ], c, coord );
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


/*---------------------------------------------------------------------------*/
/* This routine is called from functions (like the function showing the file */
/* selector box) to switch off the special states entered by pressing one or */
/* more of the mouse buttons. Otherwise, after these functions are finished  */
/* (usually with all mouse buttons released) the drawing routines would      */
/* think that the buttons are still pressed, forcing the user to press the   */
/* buttons again, just to get the ButtonRelease event.                       */
/*---------------------------------------------------------------------------*/

void switch_off_special_cursors( void )
{
	if ( G.is_init && G.button_state != 0 )
	{
		G.button_state = G.raw_button_state = 0;

		if ( G.drag_canvas == 1 )         /* x-axis window */
		{
			fl_reset_cursor( FL_ObjWin( G.x_axis.obj ) );
			G.x_axis.is_box = UNSET;
			if ( G.dim == 1 )
				repaint_canvas_1d( &G.x_axis );
			else
				repaint_canvas_2d( &G.x_axis );
		}

		if ( G.drag_canvas == 2 )         /* y-axis window */
		{
			fl_reset_cursor( FL_ObjWin( G.y_axis.obj ) );
			G.y_axis.is_box = UNSET;
			if ( G.dim == 1 )
				repaint_canvas_1d( &G.y_axis );
			else
				repaint_canvas_2d( &G.y_axis );			
		}

		if ( G.drag_canvas == 3 )         /* canvas window in 1D mode */
		{
			fl_reset_cursor( FL_ObjWin( G.canvas.obj ) );
			G.canvas.is_box = UNSET;
			repaint_canvas_1d( &G.canvas );
		}

		if ( G.drag_canvas == 4 )         /* z-axis window */
		{
			fl_reset_cursor( FL_ObjWin( G.z_axis.obj ) );
			G.canvas.is_box = UNSET;
			repaint_canvas_2d( &G.z_axis );
		}

		if ( G.drag_canvas == 7 )         /* canvas window in 2D mode */
		{
			fl_reset_cursor( FL_ObjWin( G.canvas.obj ) );
			G.canvas.is_box = UNSET;
			repaint_canvas_2d( &G.canvas );
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
	Curve_2d *cv2;
	double temp_s2d,
		   temp_shift;
	double temp_z_factor;
	int j;

	a = a;
	b = b;


	if ( G.dim == 1 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			if ( ! cv->can_undo )
				continue;

			for ( j = 0; j <= Y; j++ )
			{
				temp_s2d = cv->s2d[ j ];
				temp_shift = cv->shift[ j ];

				cv->s2d[ j ] = cv->old_s2d[ j ];
				cv->shift[ j ] = cv->old_shift[ j ];

				cv->old_s2d[ j ] = temp_s2d;
				cv->old_shift[ j ] = temp_shift;
			}

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
	else
	{
		if ( G.active_curve == -1 || ! G.curve_2d[ G.active_curve ]->can_undo )
			return;

		cv2 = G.curve_2d[ G.active_curve ];

		for ( j = 0; j <= Z; j++ )
		{
			temp_s2d = cv2->s2d[ j ];
			temp_shift = cv2->shift[ j ];

			cv2->s2d[ j ] = cv2->old_s2d[ j ];
			cv2->shift[ j ] = cv2->old_shift[ j ];

			cv2->old_s2d[ j ] = temp_s2d;
			cv2->old_shift[ j ] = temp_shift;
		}


		temp_z_factor = cv2->z_factor;
		cv2->z_factor = cv2->old_z_factor;
		cv2->old_z_factor = temp_z_factor;

		recalc_XPoints_of_curve_2d( cv2 );

		if ( cv2->is_fs )
		{
			cv2->is_fs = UNSET;
			fl_set_button( run_form->full_scale_button, 0 );
		}

		redraw_all_2d( );
	}
}


/*-------------------------------------------------------------*/
/* Rescales the display so that the canvas is completely used. */
/*-------------------------------------------------------------*/

void fs_button_callback( FL_OBJECT *a, long b )
{
	int state;
	int i;


	a = a;
	b = b;

	/* Rescaling is useless if graphic isn't initialized */

	if ( ! G.is_init )
		return;

	/* Get new state of button */

	state = fl_get_button( run_form->full_scale_button );

	if ( G.dim == 1 )            /* for 1D display */
	{
		if ( state == 1 )        /* full scale got switched on */
		{
			G.is_fs = SET;

			/* Store data of previous state... */

			for ( i = 0; i < G.nc; i++ )
				save_scale_state_1d( G.curve[ i ] );

			/* ... and rescale to full scale */

			fs_rescale_1d( );
			redraw_all_1d( );
			fl_set_object_helper( run_form->full_scale_button,
								  "Switch off automatic rescaling" );
		}
		else        /* full scale got switched off */
		{
			G.is_fs = UNSET;
			fl_set_object_helper( run_form->full_scale_button,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
		}

	}
	else                         /* for 2D display */
	{
		if ( state == 1 )        /* full scale got switched on */
		{
			fl_set_object_helper( run_form->full_scale_button,
								  "Switch off automatic rescaling" );

			if ( G.active_curve != -1 )
			{
				save_scale_state_2d( G.curve_2d[ G.active_curve ] );
				G.curve_2d[ G.active_curve ]->is_fs = SET;
				fs_rescale_2d( G.curve_2d[ G.active_curve ] );
				redraw_all_2d( );
			}
		}
		else                     /* full scale got switched off */
		{	
			fl_set_object_helper( run_form->full_scale_button,
								  "Rescale curve to fit into the window\n"
								  "and switch on automatic rescaling" );

			if ( G.active_curve != -1 )
				G.curve_2d[ G.active_curve ]->is_fs = UNSET;
		}
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void curve_button_callback( FL_OBJECT *obj, long data )
{
	char hstr[ 128 ];


	obj = obj;

	if ( G.dim == 1 )
	{
		G.curve[ data - 1 ]->active ^= SET;
		/* Change the help string for the button */

		if ( fl_get_button( obj ) )
			sprintf( hstr, "Extempt curve %ld from\nrescaling operations",
					 data );
		else
			sprintf( hstr, "Apply rescaling operations\nto curve %ld", data );

		fl_set_object_helper( obj, hstr );

		/* Redraw both axis to make sure the axis for the first active button
		   is shown */

		redraw_canvas_1d( &G.x_axis );
		redraw_canvas_1d( &G.y_axis );
	}
	else
	{
		/* Make buttons work like radio buttons but also allow to switch off
		   all of them */

		if ( data - 1 == G.active_curve )     /* shown curve is switched off */
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

			G.active_curve = data - 1;

			fl_set_button( run_form->full_scale_button,
						   G.curve_2d[ G.active_curve ]->is_fs );
		}

		redraw_all_2d( );
	}

}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void clear_curve( long curve )
{
	long i;
	Scaled_Point *sp;


	if ( G.dim == 1 )
	{
		for ( sp = G.curve[ curve ]->points, i = 0; i < G.nx; sp++, i++ )
			sp->exist = UNSET;
		G.curve[ curve ]->count = 0;
	}
	else
	{
		for ( sp = G.curve_2d[ curve ]->points, i = 0; i < G.nx * G.ny;
			  sp++, i++ )
			sp->exist = UNSET;
		G.curve_2d[ curve ]->count = 0;
	}
}
