/*
  $Id$
*/


#include "fsc2.h"


void create_pixmap( void );
void delete_pixmap( void );
int canvas_handler( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					void *udata );
void reconfigure_window( unsigned int w, unsigned int h );
void redraw( void );
void repaint( void );



void graphics_init( long dim, long nx, long ny, char *x_label, char *y_label )
{
	/* The parent process does all the graphics stuff... */

	if ( I_am == CHILD )
	{
		writer( C_INIT_GRAPHICS, dim, nx, ny, x_label, y_label );
		return;
	}

	G.dim = dim;
	G.nx = nx;
	G.ny = ny;

	if ( x_label != NULL )
	{
		if ( G.x_label != NULL )
			T_free( G.x_label );
		G.x_label = get_string_copy( x_label );
	}

	if ( dim == 3 && y_label != NULL )
	{
		if ( G.y_label != NULL )
			T_free( G.y_label );
		G.y_label = get_string_copy( y_label );
	}

	G.is_init = SET;
/*
	if ( dim == 2 )
		G.data = T_malloc( 4 * nx * sizeof ( int ) );
	else
		G.data = T_malloc( 4 * nx * ny * sizeof( int ) );
*/
}


/*----------------------------------------------------------------------*/
/* Initializes and shows the window for displaying measurement results. */
/*----------------------------------------------------------------------*/

void start_graphics( void )
{
	fl_deactivate_object( run_form->save );
	fl_set_object_lcol( run_form->save, FL_INACTIVE_COL );
	fl_show_form( run_form->run, FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
				  "fsc: Run" );

	G.is_drawn = SET;
	G.id = fl_get_canvas_id( run_form->canvas );
	G.d = fl_get_display( );

	G.x = run_form->canvas->x;
	G.y = run_form->canvas->y;
	G.w = run_form->canvas->w;
	G.h = run_form->canvas->h;

	fl_add_canvas_handler( run_form->canvas, Expose, canvas_handler, NULL );
    fl_add_canvas_handler( run_form->canvas, ConfigureNotify, canvas_handler,
                           NULL );

	create_pixmap( );
	redraw( );

	fl_set_cursor( G.id, XC_left_ptr );
}


/*---------------------------------------------------------*/
/* Removes the window for displaying measurement results. */
/*---------------------------------------------------------*/

void stop_graphics( void )
{
   fl_remove_canvas_handler( run_form->canvas, Expose, canvas_handler);
   fl_remove_canvas_handler( run_form->canvas, ConfigureNotify,
							 canvas_handler);
	delete_pixmap( );

   if ( fl_form_is_visible( run_form->run ) )
	   fl_hide_form( run_form->run );
}


/*----------------------------------------*/
/* Creates a pixmap for double buffering. */
/*----------------------------------------*/

void create_pixmap( void )
{
    Window root;
    int x, y;               /* x- and y-position */
    unsigned int w,         /* width */
		         h,         /* height */
		         bw,        /* border width */
		         d;         /* depth */

    XGetGeometry( G.d, G.id, &root, &x, &y, &w, &h, &bw, &d );
    G.pm = XCreatePixmap( G.d, G.id, G.w, G.h, d );
    G.gc = XCreateGC( G.d, G.id, 0, 0);

	if ( G.is_init )
		XSetForeground( G.d, G.gc, fl_get_flcolor( FL_WHITE ) );
	else
		XSetForeground( G.d, G.gc, fl_get_flcolor( FL_INACTIVE ) );
}


/*------------------------------------------*/
/* Deletes the pixmap for double buffering. */
/*------------------------------------------*/

void delete_pixmap( void )
{
    XFreeGC( G.d, G.gc );
    XFreePixmap( G.d, G.pm );
}


/*--------------------------------------------------------*/
/* Handler for all kinds of X events the canvas receives. */
/*--------------------------------------------------------*/

int canvas_handler( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					void *udata )
{
	obj = obj;
	window = window;
	udata = udata;

	switch ( ev->type )
    {
        case Expose :
            if ( ev->xexpose.window != G.id )
                break;
            if ( ev->xexpose.count == 0 )     /* only react to last in queue */
                repaint( );
            break;

		case ConfigureNotify :                /* window was reconfigure  */
            if ( ( int ) G.w == w && ( int ) G.h == h )
				break;
            reconfigure_window( ( unsigned int ) w, ( unsigned int ) h );
            break;

	}

	return 1;
}


/*-------------------------------------*/
/* Handles changes of the window size. */
/*-------------------------------------*/

void reconfigure_window( unsigned int w, unsigned int h )
{
	G.w = w;
	G.h = h;
	delete_pixmap( );
	create_pixmap( );
	redraw( );
}


/*---------------------------------------*/
/* Does a complete redraw of the canvas. */
/*---------------------------------------*/

void redraw( void )
{
	XFillRectangle( G.d, G.pm, G.gc, 0, 0, G.w, G.h );
	repaint( );
}


/*-----------------------------------------------*/
/* Copies the background pixmap onto the canvas. */
/*-----------------------------------------------*/

void repaint( void )
{
    XCopyArea( G.d, G.pm, G.id, G.gc, 0, 0, G.w, G.h, 0, 0 );
}
