/*
  $Id$
*/


#include "fsc2.h"

#include "cursor_1.xbm"             /* bitmaps for cursors */
#include "cursor_2.xbm"
#include "cursor_3.xbm"
#include "cursor_4.xbm"
#include "cursor_5.xbm"
#include "cursor_6.xbm"
#include "cursor_7.xbm"
#include "cursor_8.xbm"

#include "up_arrow.xbm"             /* arrow bitmaps */
#include "down_arrow.xbm"
#include "left_arrow.xbm"
#include "right_arrow.xbm"


static void G_init_cut_curve( void );
static int cut_form_close_handler( FL_FORM *a, void *b );
static void cut_setup_canvas( Canvas *c, FL_OBJECT *obj );
static int cut_canvas_handler( FL_OBJECT *obj, Window window, int w, int h,
							   XEvent *ev, void *udata );
static void cut_repaint_canvas( Canvas *c );
static void cut_reconfigure_window( Canvas *c, int w, int h );
static void cut_press_handler( FL_OBJECT *obj, Window window,
							   XEvent *ev, Canvas *c );
static void cut_release_handler( FL_OBJECT *obj, Window window,
								 XEvent *ev, Canvas *c );
static void cut_motion_handler( FL_OBJECT *obj, Window window,
								XEvent *ev, Canvas *c );
static void cut_canvas_off( Canvas *c, FL_OBJECT *obj );
static void redraw_all_cut_canvases( void );
static void redraw_cut_canvas( Canvas *c );
static void repaint_cut_canvas( Canvas * );
static void redraw_cut_center_canvas( Canvas *c );
static void redraw_cut_axis( int coord );


static bool is_shown  = UNSET;  /* set on fl_show_form() */
static bool is_mapped = UNSET;  /* while form is mapped */
static Cut_Graphics CG;
static int cur_1,
	       cur_2,
	       cur_3,
	       cur_4,
	       cur_5,
	       cur_6,
	       cur_7,
	       cur_8;


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_show( int dir, int pos )
{
	/* Don't do anything if no curve is currently displayed */

	if ( G.active_curve == -1 )
		return;

	/* If the cross section window hasn't been shown before create and
	   initialize it now, otherwise, if it's just unmapped make it visible
	   again */

	if ( ! is_shown )
	{
		fl_show_form( cut_form->cut,
					  FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
					  "fsc2: Cross section" );

		cut_setup_canvas( &G.cut_x_axis, cut_form->cut_x_axis );
		cut_setup_canvas( &G.cut_y_axis, cut_form->cut_y_axis );
		cut_setup_canvas( &G.cut_z_axis, cut_form->cut_z_axis );
		cut_setup_canvas( &G.cut_canvas, cut_form->cut_canvas );

		fl_winminsize( cut_form->cut->window, 500, 335 );
		fl_set_form_atclose( cut_form->cut, cut_form_close_handler, NULL );

		G_init_cut_curve( );

		is_shown = is_mapped = SET;
	}
	else if ( ! is_mapped )
	{
		XMapWindow( G.d, cut_form->cut->window );
		XMapSubwindows( G.d, cut_form->cut->window );
		is_mapped = SET;
	}

	G.is_cut = SET;

	fl_raise_form( cut_form->cut );
	redraw_all_cut_canvases( );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void G_init_cut_curve( void )
{
	Curve_1d *cv = &G.cut_curve;
	unsigned int depth;
	int i;


	depth = fl_get_canvas_depth( G.cut_canvas.obj );

	cur_1 = fl_create_bitmap_cursor( cursor_1_bits, cursor_1_bits,
									 cursor_1_width, cursor_1_height,
									 cursor_1_x_hot, cursor_1_y_hot );
	cur_2 = fl_create_bitmap_cursor( cursor_2_bits, cursor_2_bits,
									 cursor_2_width, cursor_2_height,
									 cursor_2_x_hot, cursor_2_y_hot );
	cur_3 = fl_create_bitmap_cursor( cursor_3_bits, cursor_3_bits,
									 cursor_3_width, cursor_3_height,
									 cursor_3_x_hot, cursor_3_y_hot );
	cur_4 = fl_create_bitmap_cursor( cursor_4_bits, cursor_4_bits,
									 cursor_4_width, cursor_4_height,
									 cursor_4_x_hot, cursor_4_y_hot );
	cur_5 = fl_create_bitmap_cursor( cursor_5_bits, cursor_5_bits,
									 cursor_5_width, cursor_5_height,
									 cursor_5_x_hot, cursor_5_y_hot );
	cur_6 = fl_create_bitmap_cursor( cursor_6_bits, cursor_6_bits,
									 cursor_6_width, cursor_6_height,
									 cursor_6_x_hot, cursor_6_y_hot );
	cur_7 = fl_create_bitmap_cursor( cursor_7_bits, cursor_7_bits,
									 cursor_7_width, cursor_7_height,
									 cursor_7_x_hot, cursor_7_y_hot );
	cur_8 = fl_create_bitmap_cursor( cursor_8_bits, cursor_8_bits,
									 cursor_8_width, cursor_8_height,
									 cursor_8_x_hot, cursor_8_y_hot );
	CG.cur_1 = cur_1;
	CG.cur_2 = cur_2;
	CG.cur_3 = cur_3;
	CG.cur_4 = cur_4;
	CG.cur_5 = cur_5;
	CG.cur_6 = cur_6;
	CG.cur_7 = cur_7;
	CG.cur_8 = cur_8;

	fl_set_cursor_color( CG.cur_1, FL_BLACK, G.colors[ G.active_curve ] );
	fl_set_cursor_color( CG.cur_2, FL_BLACK, G.colors[ G.active_curve ] );
	fl_set_cursor_color( CG.cur_3, FL_BLACK, G.colors[ G.active_curve ] );
	fl_set_cursor_color( CG.cur_4, FL_BLACK, G.colors[ G.active_curve ] );
	fl_set_cursor_color( CG.cur_5, FL_BLACK, G.colors[ G.active_curve ] );
	fl_set_cursor_color( CG.cur_6, FL_BLACK, G.colors[ G.active_curve ] );
	fl_set_cursor_color( CG.cur_7, FL_BLACK, G.colors[ G.active_curve ] );
	fl_set_cursor_color( CG.cur_8, FL_BLACK, G.colors[ G.active_curve ] );

	/* Allocate memory for the curve and its data */

	cv->points = NULL;
	cv->xpoints = NULL;

	/* Create a GC for drawing the curve and set its colour */

	cv->gc = XCreateGC( G.d, G.cut_canvas.pm, 0, 0 );
	XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ G.active_curve ] ) );

	/* Create pixmaps for the out-of-range arrows (in the colors associated
	   with the four possible curves) */

	for ( i = 0; i < 4; i++ )
	{
		CG.up_arrows[ i ] =
			XCreatePixmapFromBitmapData( G.d, G.cut_canvas.pm, up_arrow_bits,
										 up_arrow_width, up_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		CG.up_arrow_w = up_arrow_width;
		CG.up_arrow_h = up_arrow_width;

		CG.down_arrows[ i ] =
			XCreatePixmapFromBitmapData( G.d, G.cut_canvas.pm, down_arrow_bits,
										 down_arrow_width, down_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		CG.down_arrow_w = down_arrow_width;
		CG.down_arrow_h = down_arrow_width;

		CG.left_arrows[ i ] =
			XCreatePixmapFromBitmapData( G.d, G.cut_canvas.pm, left_arrow_bits,
										 left_arrow_width, left_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		CG.left_arrow_w = left_arrow_width;
		CG.left_arrow_h = left_arrow_width;

		CG.right_arrows[ i ] = 
			XCreatePixmapFromBitmapData( G.d, G.cut_canvas.pm,
										 right_arrow_bits, right_arrow_width,
										 right_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		CG.right_arrow_w = right_arrow_width;
		CG.right_arrow_h = right_arrow_width;
	}

	/* Set the pixmaps for the out-of-range arrow to the pixmaps with the
	   color associated with the currently displayed curve */

	cv->up_arrow    = CG.up_arrows[ G.active_curve ];
	cv->down_arrow  = CG.down_arrows[ G.active_curve ];
	cv->left_arrow  = CG.left_arrows[ G.active_curve ];
	cv->right_arrow = CG.right_arrows[ G.active_curve ];

	/* Create a GC for the font and set the appropriate colour */

	cv->font_gc = XCreateGC( G.d, FL_ObjWin( G.cut_canvas.obj ), 0, 0 );
	if ( G.font != NULL )
		XSetFont( G.d, cv->font_gc, G.font->fid );
	XSetForeground( G.d, cv->font_gc,
					fl_get_pixel( G.colors[ G.active_curve ] ) );
	XSetBackground( G.d, cv->font_gc, fl_get_pixel( FL_BLACK ) );

	cv->count = 0;
	cv->active = SET;
	cv->can_undo = UNSET;

	CG.label_pm[ Y ] = G.label_pm[ Z ];
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static int cut_form_close_handler( FL_FORM *a, void *b )
{
	a = a;
	b = b;

	cut_close_callback( cut_form->cut_close_button, 0 );
	return FL_IGNORE;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void cut_setup_canvas( Canvas *c, FL_OBJECT *obj )
{
	XSetWindowAttributes attributes;
	FL_HANDLE_CANVAS ch = cut_canvas_handler;


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

		fl_add_canvas_handler( c->obj, ConfigureNotify, ch, ( void * ) c );
		fl_add_canvas_handler( c->obj, ButtonPress, ch, ( void * ) c );
		fl_add_canvas_handler( c->obj, ButtonRelease, ch, ( void * ) c );
		fl_add_canvas_handler( c->obj, MotionNotify, ch, ( void * ) c );

		/* Get motion events only when first or second button is pressed
		   (this got to be set *after* requesting motion events) */

		fl_remove_selected_xevent( FL_ObjWin( obj ),
								   PointerMotionMask | PointerMotionHintMask |
			                       ButtonMotionMask );
		fl_add_selected_xevent( FL_ObjWin( obj ),
								Button1MotionMask | Button2MotionMask );

		c->font_gc = XCreateGC( G.d, FL_ObjWin( obj ), 0, 0 );
		XSetForeground( G.d, c->font_gc, fl_get_pixel( FL_BLACK ) );
		XSetFont( G.d, c->font_gc, G.font->fid );

		XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );

		if ( c == &G.cut_canvas )
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static int cut_canvas_handler( FL_OBJECT *obj, Window window, int w, int h,
							   XEvent *ev, void *udata )
{
	Canvas *c = ( Canvas * ) udata;


	switch ( ev->type )
    {
        case Expose :
            if ( ev->xexpose.count == 0 )     /* only react to last in queue */
				cut_repaint_canvas( c );
            break;

		case ConfigureNotify :
            if ( ( int ) c->w == w && ( int ) c->h == h )
				break;
            cut_reconfigure_window( c, w, h );
            break;

		case ButtonPress :
			cut_press_handler( obj, window, ev, c );
			break;

		case ButtonRelease :
			cut_release_handler( obj, window, ev, c );
			break;

		case MotionNotify :
			cut_motion_handler( obj, window, ev, c );
			break;
	}

	return 1;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void cut_repaint_canvas( Canvas *c )
{
	redraw_all_cut_canvases( );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void cut_reconfigure_window( Canvas *c, int w, int h )
{
	printf( "cut_reconfigure_window\n" );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void cut_press_handler( FL_OBJECT *obj, Window window,
							   XEvent *ev, Canvas *c )
{
	printf( "cut_press_handler\n" );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void cut_release_handler( FL_OBJECT *obj, Window window,
								 XEvent *ev, Canvas *c )
{
	printf( "cut_release_handler\n");
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void cut_motion_handler( FL_OBJECT *obj, Window window,
								XEvent *ev, Canvas *c )
{
	printf( "cut_motion_handler\n" );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_form_close( void )
{
	int i;
	Curve_1d *cv = &G.cut_curve;


	if ( is_shown )
	{
		/* Get rid of canvas related stuff (needs to be done *before*
		   hiding the form) */

		cut_canvas_off( &G.cut_x_axis, cut_form->cut_x_axis );
		cut_canvas_off( &G.cut_y_axis, cut_form->cut_y_axis );
		cut_canvas_off( &G.cut_z_axis, cut_form->cut_z_axis );
		cut_canvas_off( &G.cut_canvas, cut_form->cut_canvas );

		/* Deallocate the pixmaps for the out-of-range arrows */

		for ( i = 0; i < 4; i++ )
		{
			XFreePixmap( G.d, CG.up_arrows[ i ] );
			XFreePixmap( G.d, CG.down_arrows[ i ] );
			XFreePixmap( G.d, CG.left_arrows[ i ] );
			XFreePixmap( G.d, CG.right_arrows[ i ] );
		}

		/* Get rid of GCs and memory allocated for the curve */

		XFreeGC( G.d, cv->font_gc );
		cv->points  = T_free( cv->points );
		cv->xpoints = T_free( cv->xpoints );

		fl_hide_form( cut_form->cut );
	}

	fl_free_form( cut_form->cut );
	is_shown = is_mapped = UNSET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_undo_button_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;
}


/*---------------------------------------------------------------*/
/* Instead of calling fl_hide_form() we directly unmap the forms */
/* window, otherwise there were some "bad window" messages on    */
/* showing the form again (obviously on redrawing the canvases)  */
/*---------------------------------------------------------------*/

void cut_close_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;

	G.is_cut = UNSET;
	XUnmapSubwindows( G.d, cut_form->cut->window );
	XUnmapWindow( G.d, cut_form->cut->window );
	is_mapped = UNSET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void cut_canvas_off( Canvas *c, FL_OBJECT *obj )
{
	FL_HANDLE_CANVAS ch = cut_canvas_handler;


	fl_remove_canvas_handler( obj, Expose, ch );

	fl_remove_canvas_handler( obj, ConfigureNotify, ch );
	fl_remove_canvas_handler( obj, ButtonPress, ch );
	fl_remove_canvas_handler( obj, ButtonRelease, ch );
	fl_remove_canvas_handler( obj, MotionNotify, ch );

	XFreeGC( G.d, c->font_gc );

	delete_pixmap( c );

	G.is_cut = UNSET;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_fs_button_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;
}


static void redraw_all_cut_canvases( void )
{
	redraw_cut_canvas( &G.cut_canvas );
	redraw_cut_canvas( &G.cut_x_axis );
	redraw_cut_canvas( &G.cut_y_axis );
	redraw_cut_canvas( &G.cut_z_axis );
}


static void redraw_cut_canvas( Canvas *c )
{
	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

	/* Finally copy the pixmap onto the screen */

	repaint_cut_canvas( c );
}


static void repaint_cut_canvas( Canvas *c )
{
	Pixmap pm;


	pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h,
						fl_get_canvas_depth( c->obj ) );
	XCopyArea( G.d, c->pm, pm, c->gc, 0, 0, c->w, c->h, 0, 0 );

	/* Finally copy the buffer pixmap onto the screen */

	XCopyArea( G.d, pm, FL_ObjWin( c->obj ), c->gc,
			   0, 0, c->w, c->h, 0, 0 );
	XFreePixmap( G.d, pm );
	XFlush( G.d );
}

static void redraw_cut_center_canvas( Canvas *c )
{
}

static void redraw_cut_axis( int coord )
{
}
