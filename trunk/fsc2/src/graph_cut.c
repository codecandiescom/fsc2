/*
  $Id$
*/


#include "fsc2.h"


static void setup_cut_canvas( Canvas *c, FL_OBJECT *obj );
static int cut_canvas_handler( FL_OBJECT *obj, Window window, int w, int h,
							   XEvent *ev, void *udata );
static void repaint_cut_canvas( Canvas *c );
static void reconfigure_cut_window( Canvas *c, int w, int h );
static void press_cut_handler( FL_OBJECT *obj, Window window,
							   XEvent *ev, Canvas *c );
static void release_cut_handler( FL_OBJECT *obj, Window window,
								 XEvent *ev, Canvas *c );
static void motion_cut_handler( FL_OBJECT *obj, Window window,
								XEvent *ev, Canvas *c );
static void cut_canvas_off( Canvas *c, FL_OBJECT *obj );


static bool is_shown  = UNSET;
static bool is_mapped = UNSET;

/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void show_cut( int dir, int pos )
{
	if ( G.active_curve == -1 )
		return;

	if ( ! is_shown )	
	{
		fl_show_form( cut_form->cut,
					  FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
					  "fsc2: Cross section" );

		setup_cut_canvas( &G.cut_x_axis, cut_form->cut_x_axis );
		setup_cut_canvas( &G.cut_y_axis, cut_form->cut_y_axis );
		setup_cut_canvas( &G.cut_z_axis, cut_form->cut_z_axis );
		setup_cut_canvas( &G.cut_canvas, cut_form->cut_canvas );

		fl_winminsize( cut_form->cut->window, 500, 335 );

		is_shown = is_mapped = SET;
	}
	else if ( ! is_mapped )
	{
		XMapWindow( G.d, cut_form->cut->window );
		XMapSubwindows( G.d, cut_form->cut->window );
		is_mapped = SET;
	}

	fl_raise_form( cut_form->cut );
	XFlush( G.d );
}



/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void setup_cut_canvas( Canvas *c, FL_OBJECT *obj )
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

		/* Get motion events only when first or second button is pressed */

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
				repaint_cut_canvas( c );
            break;

		case ConfigureNotify :
            if ( ( int ) c->w == w && ( int ) c->h == h )
				break;
            reconfigure_cut_window( c, w, h );
            break;

		case ButtonPress :
			press_cut_handler( obj, window, ev, c );
			break;

		case ButtonRelease :
			release_cut_handler( obj, window, ev, c );
			break;

		case MotionNotify :
			motion_cut_handler( obj, window, ev, c );
			break;
	}

	return 1;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void repaint_cut_canvas( Canvas *c )
{
	printf( "repaint_cut_canvas\n" );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void reconfigure_cut_window( Canvas *c, int w, int h )
{
	printf( "reconfigure_cut_window\n" );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void press_cut_handler( FL_OBJECT *obj, Window window,
							   XEvent *ev, Canvas *c )
{
	printf( "press_cut_handler\n" );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void release_cut_handler( FL_OBJECT *obj, Window window,
								 XEvent *ev, Canvas *c )
{
	printf( "release_cut_handler\n");
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void motion_cut_handler( FL_OBJECT *obj, Window window,
								XEvent *ev, Canvas *c )
{
	printf( "motion_cut_handler\n" );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_form_close( void )
{
	if ( is_shown )
		fl_hide_form( cut_form->cut );
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


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_close_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;

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

	if ( G.is_init )
	{
		fl_remove_canvas_handler( obj, ConfigureNotify, ch );
		fl_remove_canvas_handler( obj, ButtonPress, ch );
		fl_remove_canvas_handler( obj, ButtonRelease, ch );
		fl_remove_canvas_handler( obj, MotionNotify, ch );

		XFreeGC( G.d, c->font_gc );
	}

	delete_pixmap( c );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_fs_button_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;
}
