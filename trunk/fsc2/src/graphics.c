/*
  $Id$
*/


#include "fsc2.h"


void setup_canvas( Canvas *c, FL_OBJECT *obj );
void create_pixmap( Canvas *c );
void delete_pixmap( Canvas *c );
int canvas_handler( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					void *udata );
void reconfigure_window( Canvas *c, int w, int h );
void redraw_canvas( Canvas *c );
void repaint_canvas( Canvas *c );


void graphics_init( long dim, long nx, long ny,
					double rwc_x_start, double rwc_x_delta,
					double rwc_y_start, double rwc_y_delta,
					char *x_label, char *y_label )
{
	/* The parent process does all the graphics stuff... */

	if ( I_am == CHILD )
	{
		writer( C_INIT_GRAPHICS, dim, nx, ny, rwc_x_start, rwc_x_delta,
				rwc_y_start, rwc_y_delta, x_label, y_label );
		return;
	}

	G.dim = dim;

	G.nx = nx;
	G.ny = ny;

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

	G.is_init = SET;
}


/*----------------------------------------------------------------------*/
/* Initializes and shows the window for displaying measurement results. */
/*----------------------------------------------------------------------*/

void start_graphics( void )
{
	fl_show_form( run_form->run, FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
				  "fsc: Display" );

	G.is_drawn = SET;
	G.d = fl_get_display( );

	setup_canvas( &G.x_axis, run_form->x_axis );
	setup_canvas( &G.y_axis, run_form->y_axis );
	setup_canvas( &G.canvas, run_form->canvas );

	redraw_canvas( &G.x_axis );
	redraw_canvas( &G.y_axis );
	redraw_canvas( &G.canvas );

	fl_raise_form( run_form->run );
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

	delete_pixmap( &G.x_axis );
	delete_pixmap( &G.y_axis );
	delete_pixmap( &G.canvas );

	fl_set_object_label( run_form->stop, "Stop" );
	if ( fl_form_is_visible( run_form->run ) )
		fl_hide_form( run_form->run );
}


void setup_canvas( Canvas *c, FL_OBJECT *obj )
{
	c->id = fl_get_canvas_id( obj );
	c->obj = obj;

	c->x = obj->x;
	c->y = obj->y;
	c->w = obj->w;
	c->h = obj->h;
	create_pixmap( c );

	fl_add_canvas_handler( c->obj, Expose, canvas_handler, ( void * ) c );
    fl_add_canvas_handler( c->obj, ConfigureNotify, canvas_handler,
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

    XGetGeometry( G.d, c->id, &root, &x, &y, &w, &h, &bw, &d );
    c->pm = XCreatePixmap( G.d, c->id, c->w, c->h, d );
    c->gc = XCreateGC( G.d, c->id, 0, 0);

	if ( c->obj == run_form->canvas )
	{
		if ( G.is_init )
			XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
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
    XFreeGC( G.d, c->gc );
    XFreePixmap( G.d, c->pm );
}


/*--------------------------------------------------------*/
/* Handler for all kinds of X events the canvas receives. */
/*--------------------------------------------------------*/

int canvas_handler( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					void *udata )
{
	Canvas *c = ( Canvas * ) udata;


	obj = obj;
	window = window;
	udata = udata;

	switch ( ev->type )
    {
        case Expose :
			repaint_canvas( c );
            break;

		case ConfigureNotify :                /* window was reconfigure  */
            if ( ( int ) c->w == w && ( int ) c->h == h )
				break;
            reconfigure_window( c, w, h );
            break;
	}

	return 1;
}


/*-------------------------------------*/
/* Handles changes of the window size. */
/*-------------------------------------*/

void reconfigure_window( Canvas *c, int w, int h )
{

	c->w = ( unsigned int ) w;
	c->h = ( unsigned int ) h;

	if ( c->w > 0 && c->h > 0 )
	{
		delete_pixmap( c );
		create_pixmap( c );

		redraw_canvas( c );
	}
}


/*-------------------------------------*/
/* Does a complete redraw of a canvas. */
/*-------------------------------------*/

void redraw_canvas( Canvas *c )
{
	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );
	repaint_canvas( c );
}


/*-----------------------------------------------*/
/* Copies the background pixmap onto the canvas. */
/*-----------------------------------------------*/

void repaint_canvas( Canvas *c )
{
	XCopyArea( G.d, c->pm, c->id, c->gc, 0, 0, c->w, c->h, 0, 0 );
}


void fs_button_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;
}
