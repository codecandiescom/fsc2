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


void setup_canvas( Canvas *c, FL_OBJECT *obj );
void create_pixmap( Canvas *c );
void delete_pixmap( Canvas *c );
int canvas_handler( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					void *udata );
void reconfigure_window( Canvas *c, int w, int h );
void redraw_canvas( Canvas *c );
void recalc_XPoints( void );
void repaint_canvas( Canvas *c );
void fs_rescale_1d( void );
void fs_rescale_2d( void );
void accept_1d_data( long x_index, long curve, int type, void *ptr );
void accept_2d_data( long x_index, long y_index, long curve, int type,
					 void *ptr );



int cursor_1,
	cursor_2,
	cursor_3;


void graphics_init( long dim, long nc, long nx, long ny,
					double rwc_x_start, double rwc_x_delta,
					double rwc_y_start, double rwc_y_delta,
					char *x_label, char *y_label )
{
	long i; long j;


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


	/* Get memory for the structures for storing the data and displaying the
	   curves (2D curves are not yet implemented) */

	if ( dim == 1 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			G.curve[ i ] = T_malloc( sizeof( Curve_1d ) );
			G.curve[ i ]->points = NULL;
			G.curve[ i ]->points = T_malloc( G.nx * sizeof( Scaled_Point ) );
			for ( j = 0; j < G.nx; j++ )
				G.curve[ i ]->points->exist = UNSET;
			G.curve[ i ]->xpoints = NULL;
			G.curve[ i ]->xpoints = T_malloc( G.nx * sizeof( XPoint ) );
		}
	}
}


void graphics_free( void )
{
	long i;


	if ( G.is_init == UNSET )
		return;

	if ( G.x_label != NULL )
		T_free( G.x_label );
	if ( G.y_label != NULL )
		T_free( G.y_label );

	if ( G.dim == 1 )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			if ( G.curve[ i ] != NULL )
			{
				if ( G.curve[ i ]->points != NULL )
					T_free( G.curve[ i ]->points );

				if ( G.curve[ i ]->xpoints != NULL )
					T_free( G.curve[ i ]->xpoints );

				T_free( G.curve[ i ] );
			}
		}
	}

	G.is_init = UNSET;
}


/*----------------------------------------------------------------------*/
/* Initializes and shows the window for displaying measurement results. */
/*----------------------------------------------------------------------*/

void start_graphics( void )
{
	int i, j;
	FL_COLOR colors[ MAX_CURVES ] = { FL_RED, FL_GREEN, FL_YELLOW, FL_BLUE };


	/* Create the form for running experiments */

	run_form = create_form_run( );
	fl_set_button_shortcut( run_form->stop, "S", 1 );
	fl_set_object_helper( run_form->stop, "Stop the running program" );

	/* fdesign is unable to set the box type attributes for canvases... */

	fl_set_canvas_decoration( run_form->x_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->y_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->canvas, FL_NO_FRAME );

	/* Show only the needed buttons */

	if ( G.is_init && G.dim == 1 )
	{
		G.x_shift = G.y_shift = 0;

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

	/* Set minimum size for display window */

	fl_winminsize( run_form->run->window, 400, 320 );

	G.d = FL_FormDisplay( run_form->run );

	setup_canvas( &G.x_axis, run_form->x_axis );
	setup_canvas( &G.y_axis, run_form->y_axis );
	setup_canvas( &G.canvas, run_form->canvas );

	cursor_1 = fl_create_bitmap_cursor( c1_bits, c1_bits, c1_width, c1_height,
										c1_x_hot, c1_y_hot );
	fl_set_cursor_color( cursor_1, FL_RED, FL_WHITE );

	cursor_2 = fl_create_bitmap_cursor( c2_bits, c2_bits, c2_width, c2_height,
										c2_x_hot, c2_y_hot );
	fl_set_cursor_color( cursor_2, FL_RED, FL_WHITE );

	cursor_3 = fl_create_bitmap_cursor( c3_bits, c3_bits, c3_width, c3_height,
										c3_x_hot, c3_y_hot );
	fl_set_cursor_color( cursor_3, FL_RED, FL_WHITE );


	if ( G.is_init )
	{
		G.x_scale_changed = G.y_scale_changed = SET;

		G.rw_y_min = HUGE_VAL;
		G.rw_y_max = - HUGE_VAL;
		G.fs_rw2s = 1.0;
		G.is_scale_set = UNSET;

		for ( i = 0; i < G.nc; i++ )
		{
			G.curve[ i ]->gc = XCreateGC( G.d, G.canvas.pm, 0, 0 );
			XSetForeground( G.d, G.curve[ i ]->gc,
							fl_get_pixel( colors[ i ] ) );

			for ( j = 0; j < G.nx; j++ )
				G.curve[ i ]->points[ j ].exist = UNSET;

			G.curve[ i ]->count = 0;
			G.curve[ i ]->is_fs = SET;
		}

		G.s2d_x = ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
		G.s2d_y = ( double ) ( G.canvas.h - 1 );
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
	long i;


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

	if ( G.is_init )
		for ( i = 0; i < G.nc; i++ )
			XFreeGC( G.d, G.curve[ i ]->gc );

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
    c->gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, 0);

	if ( c == &G.canvas )
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
	static int x_start,
		       y_start,
		       button = 0;


	obj = obj;
	window = window;
	udata = udata;

	switch ( ev->type )
    {
        case Expose :
			repaint_canvas( c );
            break;

		case ConfigureNotify :                   /* window was reconfigure  */
            if ( ( int ) c->w == w && ( int ) c->h == h )
				break;
            reconfigure_window( c, w, h );
            break;

		case ButtonPress :
			x_start = ev->xbutton.x; 		     /* store position of mouse */
			y_start = ev->xbutton.y;
			button |= ( 1 << ( ev->xbutton.button - 1 ) );

			fprintf( stderr, "press: button = %d\n", button );
			fflush( stderr );

			if ( button == 1 )
			{
				fl_set_cursor( FL_ObjWin( G.canvas.obj ), cursor_1 );
				fl_set_cursor( FL_ObjWin( G.x_axis.obj ), cursor_1 );
				fl_set_cursor( FL_ObjWin( G.y_axis.obj ), cursor_1 );
			}
			if ( button == 2 )
			{
				fl_set_cursor( FL_ObjWin( G.canvas.obj ), cursor_2 );
				fl_set_cursor( FL_ObjWin( G.x_axis.obj ), cursor_2 );
				fl_set_cursor( FL_ObjWin( G.y_axis.obj ), cursor_2 );
			}
			if ( button == 4 )
			{
				fl_set_cursor( FL_ObjWin( G.canvas.obj ), cursor_3 );
				fl_set_cursor( FL_ObjWin( G.x_axis.obj ), cursor_3 );
				fl_set_cursor( FL_ObjWin( G.y_axis.obj ), cursor_3 );
			}

			fl_add_canvas_handler( G.canvas.obj, MotionNotify, canvas_handler,
								   ( void * ) c );
			fl_remove_selected_xevent( FL_ObjWin( G.canvas.obj ),
								   PointerMotionMask | PointerMotionHintMask );
			fl_add_selected_xevent( FL_ObjWin( G.canvas.obj ),
				   Button1MotionMask | Button2MotionMask | Button3MotionMask );

			fl_add_canvas_handler( G.x_axis.obj, MotionNotify, canvas_handler,
								   ( void * ) c );
			fl_remove_selected_xevent( FL_ObjWin( G.x_axis.obj ),
								   PointerMotionMask | PointerMotionHintMask );
			fl_add_selected_xevent( FL_ObjWin( G.x_axis.obj ),
				   Button1MotionMask | Button2MotionMask | Button3MotionMask );

			fl_add_canvas_handler( G.y_axis.obj, MotionNotify, canvas_handler,
								   ( void * ) c );
			fl_remove_selected_xevent( FL_ObjWin( G.y_axis.obj ),
								   PointerMotionMask | PointerMotionHintMask );
			fl_add_selected_xevent( FL_ObjWin( G.y_axis.obj ),
				   Button1MotionMask | Button2MotionMask | Button3MotionMask );
			break;

		case ButtonRelease :
			button &= ~ ( 1 << ( ev->xbutton.button - 1 ) );

			fprintf( stderr, "release: button = %d\n", button );
			fflush( stderr );

			fl_remove_canvas_handler( G.canvas.obj, MotionNotify,
									  canvas_handler );
			fl_remove_canvas_handler( G.x_axis.obj, MotionNotify,
									  canvas_handler );
			fl_remove_canvas_handler( G.y_axis.obj, MotionNotify,
									  canvas_handler );
			fl_reset_cursor( FL_ObjWin( G.canvas.obj ) );
			fl_reset_cursor( FL_ObjWin( G.x_axis.obj ) );
			fl_reset_cursor( FL_ObjWin( G.y_axis.obj ) );
			break;

		case MotionNotify :
			if ( button == 2 )
			{
				G.x_shift += ev->xbutton.x - x_start;
				x_start = ev->xbutton.x;

				G.y_shift += ev->xbutton.y - y_start;
				y_start = ev->xbutton.y;

				recalc_XPoints( );
				redraw_canvas( &G.canvas );
			}

			break;
	}

	return 1;
}


/*-------------------------------------*/
/* Handles changes of the window size. */
/*-------------------------------------*/

void reconfigure_window( Canvas *c, int w, int h )
{
	if ( c == &G.canvas && G.is_scale_set && c->w > 0 && c->h > 0 )
	{
		G.s2d_x *= ( double ) ( w - 1 ) / ( double ) ( c->w - 1 );
		G.s2d_y *= ( double ) ( h - 1 ) / ( double ) ( c->h - 1 );

		if ( G.is_scale_set )
			recalc_XPoints( );
	}

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
	long i;


	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

	if ( c == &G.canvas && G.is_scale_set )
		for ( i = 0; i < G.nc; i++ )
			if ( G.curve[ i ]->count > 1 )
				XDrawLines( G.d, c->pm, G.curve[ i ]->gc,
							G.curve[ i ]->xpoints, G.curve[ i ]->count,
							CoordModeOrigin );

	repaint_canvas( c );
}



void recalc_XPoints( void )
{
	long i, j, k;


	for ( i = 0; i < G.nc; i++ )
		for ( k = 0, j = 0; j < G.nx; j++ )
		{
			if ( G.curve[ i ]->points[ j ].exist )
			{
				G.curve[ i ]->xpoints[ k ].x = ( short )
					( rnd( G.s2d_x * ( double ) j ) + G.x_shift );
				G.curve[ i ]->xpoints[ k ].y = ( short )
					rnd( G.s2d_y * ( 1.0 - G.curve[ i ]->points[ j ].y )
						 + G.y_shift );
				k++;
			}
		}
}



/*-----------------------------------------------*/
/* Copies the background pixmap onto the canvas. */
/*-----------------------------------------------*/

void repaint_canvas( Canvas *c )
{
	XCopyArea( G.d, c->pm, FL_ObjWin( c->obj ), c->gc,
			   0, 0, c->w, c->h, 0, 0 );
}


void fs_button_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;


	/* Rescaling is useless if graphic isn't initialized */

	if ( ! G.is_init )
		return;

	if ( G.dim == 1 )
		fs_rescale_1d( );
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


	G.x_shift = G.y_shift = 0;
	G.s2d_x = ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
	G.s2d_y = ( double ) ( G.canvas.h - 1 );

	if ( ! G.is_scale_set )
		return;

	/* Find minimum and maximum value of all scaled data */

	for ( i = 0; i < G.nc; i++ )
		for ( j = 0; j < G.nx; j++ )
			if ( G.curve[ i ]->points[ j ].exist )
			{
				data = G.curve[ i ]->points[ j ].y;
				max = d_max( data, max );
				min = d_min( data, min );
			}

	/* Calculate new real world maximum and minimum */

	rw_y_min = min / G.fs_rw2s + G.rw_y_min;
	rw_y_max = max / G.fs_rw2s + G.rw_y_min;

	/* Calculate new scaling factor and rescale the scaled data as well as the
	   points for drawing */

	new_y_scale = 1.0 / ( rw_y_max - rw_y_min );

	for ( i = 0; i < G.nc; i++ )
		for ( k = 0, j = 0; j < G.nx; j++ )
			if ( G.curve[ i ]->points[ j ].exist )
			{
				G.curve[ i ]->points[ j ].y = new_y_scale *
					( G.curve[ i ]->points[ j ].y / G.fs_rw2s 
					  + G.rw_y_min - rw_y_min );

				G.curve[ i ]->xpoints[ k ].x =
					( short ) rnd( G.s2d_x * ( double ) j );
				G.curve[ i ]->xpoints[ k ].y = ( short )
					rnd( G.s2d_y * ( 1.0 - G.curve[ i ]->points[ j ].y ) );
				k++;
			}

	/* Store new minimum and maximum and the new scale factor */

	G.fs_rw2s  = new_y_scale;
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

	long i, j, k;


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
			G.curve[ i ]->points = T_realloc( G.curve[ i ]->points,
								  ( x_index + len ) * sizeof( Scaled_Point ) );
			G.curve[ i ]->xpoints = T_realloc( G.curve[ i ]->xpoints,
										( x_index + len ) * sizeof( XPoint ) );
		}

		G.s2d_x = ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
		G.x_scale_changed = SET;
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
				for ( j = 0; j < G.nx; j++ )
					if ( G.curve[ i ]->points[ j ].exist )
						G.curve[ i ]->points[ j ].y = new_y_scale *
							( G.curve[ i ]->points[ j ].y / G.fs_rw2s
							  + G.rw_y_min - rw_y_min );

			G.fs_rw2s  = new_y_scale;
		}

		if ( ! G.is_scale_set && rw_y_max != rw_y_min )
		{
			new_y_scale = 1.0 / ( rw_y_max - rw_y_min );

			for ( i = 0; i < G.nc; i++ )
				for ( j = 0; j < G.nx; j++ )
					if ( G.curve[ i ]->points[ j ].exist )
						G.curve[ i ]->points[ j ].y = new_y_scale *
							( G.curve[ i ]->points[ j ].y - rw_y_min );

			G.fs_rw2s  = new_y_scale;
			G.is_scale_set = SET;
		}

		G.rw_y_min = rw_y_min;
		G.rw_y_max = rw_y_max;

		G.y_scale_changed = SET;
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
			G.curve[ curve ]->points[ i ].y =
				G.fs_rw2s * ( data - G.rw_y_min );
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
		if ( ! G.x_scale_changed && ! G.y_scale_changed && i != curve )
			continue;
		for ( k = 0, j = 0; j < G.nx; j++ )
		{
			if ( G.curve[ i ]->points[ j ].exist )
			{
				G.curve[ i ]->xpoints[ k ].x =
					( short ) ( rnd( G.s2d_x * ( double ) j ) + G.x_shift );
				G.curve[ i ]->xpoints[ k ].y = ( short )
					rnd( G.s2d_y * ( 1.0 - G.curve[ i ]->points[ j ].y )
						 + G.y_shift );
				k++;
			}
		}
	}

	G.x_scale_changed = G.y_scale_changed = UNSET;
}


void accept_2d_data( long x_index, long y_index, long curve, int type,
					 void *ptr )
{
}


void curve_button_callback( FL_OBJECT *obj, long data )
{
}
