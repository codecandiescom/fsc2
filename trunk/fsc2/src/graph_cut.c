/*
  $Id$
*/


#include "fsc2.h"


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


static bool is_shown  = UNSET;  /* set on fl_show_form() */
static bool is_mapped = UNSET;  /* while form is mapped */


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
	printf( "cut_repaint_canvas\n" );
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
	if ( is_shown )
	{
		/* Get rid of canvas related stuff (needs to be done *before*
		   hiding the form) */

		cut_canvas_off( &G.cut_x_axis, cut_form->cut_x_axis );
		cut_canvas_off( &G.cut_y_axis, cut_form->cut_y_axis );
		cut_canvas_off( &G.cut_z_axis, cut_form->cut_z_axis );
		cut_canvas_off( &G.cut_canvas, cut_form->cut_canvas );

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
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_fs_button_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;
}
