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
		 
#include "up_arrow.xbm"             /* arrow bitmaps */
#include "down_arrow.xbm"
#include "left_arrow.xbm"
#include "right_arrow.xbm"


static int run_form_close_handler( FL_FORM *a, void *b );
static void G_struct_init( void );
static void G_init_curves_1d( void );
static void G_init_curves_2d( void );
static void setup_canvas( Canvas *c, FL_OBJECT *obj );
static void canvas_off( Canvas *c, FL_OBJECT *obj );


static Graphics *G_stored;
static int cur_1,
	       cur_2,
	       cur_3,
	       cur_4,
	       cur_5,
	       cur_6,
	       cur_7;
static int display_x, display_y, display_w, display_h;
static bool display_has_been_shown = UNSET;

extern FL_resource xresources[ ];



#if ( SIZE == HI_RES )
#define WIN_MIN_1D_WIDTH   400
#define WIN_MIN_2D_WIDTH   500
#define WIN_MIN_HEIGHT     435
#define DEFAULT_AXISFONT_1 "*-lucida-bold-r-normal-sans-14-*"
#define DEFAULT_AXISFONT_2 "lucidasanstypewriter-14"
#define DEFAULT_AXISFONT_3 "9x15"
#else
#define WIN_MIN_1D_WIDTH   300
#define WIN_MIN_2D_WIDTH   350
#define WIN_MIN_HEIGHT     380
#define DEFAULT_AXISFONT_1 "*-lucida-bold-r-normal-sans-10-*"
#define DEFAULT_AXISFONT_2 "lucidasanstypewriter-10"
#define DEFAULT_AXISFONT_3 "9x10"
#endif


/*----------------------------------------------------------------------*/
/* Initializes and shows the window for displaying measurement results. */
/*----------------------------------------------------------------------*/

void start_graphics( void )
{
	XCharStruct font_prop;
	int dummy;
	char *pixmap_file;
	int flags;
	bool needs_pos = UNSET;


	/* Create the forms for running experiments */

	run_form = create_form_run( );
	if ( G.dim == 2 )
		cut_form = create_form_cut( );

	/* They still need some modifications - first set the pixmaps and
	   help texts for the Undo and Print buttons */

	pixmap_file = get_string( strlen( auxdir ) + strlen( "/undo.xpm" ) );
	strcpy( pixmap_file, auxdir );
	if ( auxdir[ strlen( auxdir ) - 1 ] != '/' )
		strcat( pixmap_file, "/" );
	strcat( pixmap_file, "undo.xpm" );

	G.font = NULL;

	if ( access( pixmap_file, R_OK ) == 0 )
	{
		fl_set_pixmapbutton_file( run_form->undo_button, pixmap_file );
		fl_set_object_helper( run_form->undo_button,
							  "Undo last rescaling operation" );

		if ( G.dim == 2 )
		{
			fl_set_pixmapbutton_file( cut_form->cut_undo_button, pixmap_file );
			fl_set_object_helper( cut_form->cut_undo_button,
								  "Undo last rescaling operation" );
		}
	}
	T_free( pixmap_file );

	pixmap_file = get_string( strlen( auxdir ) + strlen( "/printer.xpm" ) );
	strcpy( pixmap_file, auxdir );
	if ( auxdir[ strlen( auxdir ) - 1 ] != '/' )
		strcat( pixmap_file, "/" );
	strcat( pixmap_file, "printer.xpm" );
	if ( access( pixmap_file, R_OK ) == 0 )
	{
		fl_set_pixmapbutton_file( run_form->print_button, pixmap_file );
		fl_set_object_helper( run_form->print_button, "Print window" );

		if ( G.dim == 2 )
		{
			fl_set_pixmapbutton_file( cut_form->cut_print_button,
									  pixmap_file );
			fl_set_object_helper( cut_form->cut_print_button, "Print window" );
		}
	}
	T_free( pixmap_file );

	fl_set_object_helper( run_form->stop, "Stop the experiment" );
	fl_set_object_helper( run_form->full_scale_button,
						  "Switch off automatic rescaling" );
	if ( G.dim == 2 )
	{
		fl_set_object_helper( cut_form->cut_close_button,
							  "Close the window" );
		fl_set_object_helper( cut_form->cut_full_scale_button,
							  "Switch off automatic rescaling" );
	}

	/* Store the current state of the Graphics structure - to be restored
	   after the experiment */

	G_stored = get_memcpy( &G, sizeof( Graphics ) );

	if ( G.dim == 1 )
	{
		fl_set_object_helper( run_form->curve_1_button, 
							  "Exempt curve 1 from\nrescaling operations" );
		fl_set_object_helper( run_form->curve_2_button, 
							  "Exempt curve 2 from\nrescaling operations" );
		fl_set_object_helper( run_form->curve_3_button, 
							  "Exempt curve 3 from\nrescaling operations" );
		fl_set_object_helper( run_form->curve_4_button, 
							  "Exempt curve 4 from\nrescaling operations" );

		fl_set_object_callback( run_form->print_button, print_1d, 1 );
	}
	else
	{
		fl_set_object_helper( run_form->curve_1_button, "Hide curve 1" );
		fl_set_object_helper( run_form->curve_2_button, "Show curve 2" );
		fl_set_object_helper( run_form->curve_3_button, "Show curve 3" );
		fl_set_object_helper( run_form->curve_4_button, "Show curve 4" );

		fl_set_button( run_form->curve_1_button, 1 );
		fl_set_button( run_form->curve_2_button, 0 );
		fl_set_button( run_form->curve_3_button, 0 );
		fl_set_button( run_form->curve_4_button, 0 );

		fl_set_object_callback( run_form->print_button, print_2d, 0 );

		G.active_curve = 0;
	}

	/* fdesign is unable to set the box type attributes for canvases... */

	fl_set_canvas_decoration( run_form->x_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->y_axis, FL_FRAME_BOX );
	fl_set_canvas_decoration( run_form->canvas, FL_NO_FRAME );

	if ( G.dim == 1 )
		fl_delete_object( run_form->z_axis );
	else if ( G.dim == 2 )
	{
		fl_set_canvas_decoration( run_form->z_axis, FL_FRAME_BOX );
		fl_set_canvas_decoration( cut_form->cut_x_axis, FL_FRAME_BOX );
		fl_set_canvas_decoration( cut_form->cut_y_axis, FL_FRAME_BOX );
		fl_set_canvas_decoration( cut_form->cut_z_axis, FL_FRAME_BOX );
		fl_set_canvas_decoration( cut_form->cut_canvas, FL_NO_FRAME );
	}

	/* Show only buttons really needed */

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

		fl_hide_object( run_form->print_button );
		fl_hide_object( run_form->full_scale_button );
		fl_hide_object( run_form->undo_button );
	}

	/* Finally draw the form */

	if ( ! display_has_been_shown &&
		 * ( ( char * ) xresources[ DISPLAYGEOMETRY ].var ) != '\0' )
	{
		flags = XParseGeometry( ( char * ) xresources[ DISPLAYGEOMETRY ].var,
								&display_x, &display_y,
								&display_w, &display_h );
		if ( WidthValue & flags && HeightValue & flags )
		{
			if ( G.dim == 1 )
			{
				if ( display_w < WIN_MIN_1D_WIDTH )
					display_w = WIN_MIN_1D_WIDTH;
			}
			else
			{
				if ( display_w < WIN_MIN_2D_WIDTH )
					display_w = WIN_MIN_2D_WIDTH;
			}

			if ( display_h < WIN_MIN_HEIGHT )
				display_h = WIN_MIN_HEIGHT;

			fl_set_form_size( run_form->run, display_w, display_h );
		}

		if ( XValue & flags && YValue & flags )
		{
			display_x += border_offset_x - 1;
			display_y += border_offset_y - 1;

			fl_set_form_position( run_form->run, display_x, display_y );
			needs_pos = SET;
		}
	}

	if ( display_has_been_shown )
	{
		if ( G.dim == 2 )
		{
			if ( display_w < WIN_MIN_2D_WIDTH )
				display_w = WIN_MIN_2D_WIDTH;
		}

		fl_set_form_geometry( run_form->run, display_x, display_y,
							  display_w, display_h );
		needs_pos = SET;
	}

	fl_show_form( run_form->run, needs_pos ?
				  FL_PLACE_POSITION : FL_PLACE_MOUSE | FL_FREE_SIZE, 
				  FL_FULLBORDER, "fsc2: Display" );
	display_has_been_shown = SET;

	G.d = FL_FormDisplay( run_form->run );

	/* Set minimum size for display window and switch on full scale button */

	if ( G.dim == 1 )
		fl_winminsize( run_form->run->window,
					   WIN_MIN_1D_WIDTH, WIN_MIN_HEIGHT );
	else
		fl_winminsize( run_form->run->window,
					   WIN_MIN_2D_WIDTH, WIN_MIN_HEIGHT );

	fl_set_button( run_form->full_scale_button, 1 );

	/* Load a font hopefully available on all machines (we try at least three,
	 in principal this should be made user configurable) */

	if ( G.is_init )
	{
		if ( * ( ( char * ) xresources[ AXISFONT ].var ) != '\0' )
			G.font = XLoadQueryFont( G.d,
									 ( char * ) xresources[ AXISFONT ].var );

		if ( G.font == NULL )
			G.font = XLoadQueryFont( G.d, DEFAULT_AXISFONT_1 );

		if ( G.font == NULL )
			G.font = XLoadQueryFont( G.d, DEFAULT_AXISFONT_2 );

		if ( G.font == NULL )
			G.font = XLoadQueryFont( G.d, DEFAULT_AXISFONT_3 );

		if ( G.font != NULL )
			XTextExtents( G.font, "Xp", 2, &dummy, &G.font_asc, &G.font_desc,
						  &font_prop );

		/* Create the canvas axes */
		
		setup_canvas( &G.x_axis, run_form->x_axis );
		setup_canvas( &G.y_axis, run_form->y_axis );
		if ( G.dim == 2 )
			setup_canvas( &G.z_axis, run_form->z_axis );

		/* Create the canvas itself */

		setup_canvas( &G.canvas, run_form->canvas );
	}

	if ( G.is_init )
	{
		fl_set_form_atclose( run_form->run, run_form_close_handler, NULL );
		G_struct_init( );
	}

	if ( G.dim == 1 )
		redraw_all_1d( );

	fl_raise_form( run_form->run );
	XFlush( G.d );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int run_form_close_handler( FL_FORM *a, void *b )
{
	a = a;
	b = b;

	if ( child_pid == 0 )          /* if child has already exited */
		stop_measurement( run_form->stop, 0 );
	return FL_IGNORE;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void G_struct_init( void )
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

	/* Create the  different cursors */

	if ( first_time )
	{
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
	}

	G.cur_1 = cur_1;
	G.cur_2 = cur_2;
	G.cur_3 = cur_3;
	G.cur_4 = cur_4;
	G.cur_5 = cur_5;
	G.cur_6 = cur_6;
	G.cur_7 = cur_7;

	/* On the first call also create the colours needed for 2D displays */

	if ( first_time )
		create_colors( );

	/* Define colours for the curves (in principal, this should be made
       user-configurable...) */

	G.colors[ 0 ] = FL_TOMATO;
	G.colors[ 1 ] = FL_GREEN;
	G.colors[ 2 ] = FL_YELLOW;
	G.colors[ 3 ] = FL_CYAN;

	G.is_fs = SET;
	G.scale_changed = UNSET;

	G.rw_min = HUGE_VAL;
	G.rw_max = - HUGE_VAL;
	G.is_scale_set = UNSET;

	if ( G.label[ Y ] != NULL && G.font != NULL )
		create_label_pixmap( &G.y_axis, Y, G.label[ Y ] );

	if ( G.dim == 2 && G.label[ Z ] != NULL && G.font != NULL )
		create_label_pixmap( &G.z_axis, Z, G.label[ Z ] );

	if ( G.dim == 1 )
		G_init_curves_1d( );
	else
		G_init_curves_2d( );

	first_time = UNSET;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void G_init_curves_1d( void )
{
	int i, j;
	Curve_1d *cv;
	unsigned int depth;


	for ( i = 0; i < G.nc; i++ )
		G.curve[ i ] = NULL;

	depth = fl_get_canvas_depth( G.canvas.obj );

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
		cv->xpoints = NULL;

		/* Create a GC for drawing the curve and set its colour */

		cv->gc = XCreateGC( G.d, G.canvas.pm, 0, 0 );
		XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ i ] ) );

		/* Create pixmaps for the out-of-display arrows */

		cv->up_arrow =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, up_arrow_bits,
										 up_arrow_width, up_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );
		G.up_arrow_w = up_arrow_width;
		G.up_arrow_h = up_arrow_width;

		cv->down_arrow =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, down_arrow_bits,
										 down_arrow_width, down_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		G.down_arrow_w = down_arrow_width;
		G.down_arrow_h = down_arrow_width;

		cv->left_arrow =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, left_arrow_bits,
										 left_arrow_width, left_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		G.left_arrow_w = left_arrow_width;
		G.left_arrow_h = left_arrow_width;

		cv->right_arrow = 
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, right_arrow_bits,
										 right_arrow_width, right_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_BLACK ), depth );

		G.right_arrow_w = right_arrow_width;
		G.right_arrow_h = right_arrow_width;

		/* Create a GC for the font and set the appropriate colour */

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

		/* Finally get memory for the data */

		cv->points = T_malloc( G.nx * sizeof( Scaled_Point ) );

		for ( j = 0; j < G.nx; j++ )           /* no points are known in yet */
			cv->points[ j ].exist = UNSET;

		cv->xpoints = T_malloc( G.nx * sizeof( XPoint ) );
	}
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void G_init_curves_2d( void )
{
	int i, j;
	Curve_2d *cv;
	Scaled_Point *sp;
	unsigned int depth;


	for ( i = 0; i < G.nc; i++ )
		G.curve_2d[ i ] = NULL;

	depth = fl_get_canvas_depth( G.canvas.obj );

	fl_set_cursor_color( G.cur_1, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_2, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_3, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_4, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_5, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_6, FL_BLACK, FL_WHITE );
	fl_set_cursor_color( G.cur_7, FL_BLACK, FL_WHITE );

	for ( i = 0; i < G.nc; i++ )
	{
		/* Allocate memory for the curve */

		cv = G.curve_2d[ i ] = T_malloc( sizeof( Curve_2d ) );

		cv->points = NULL;
		cv->xpoints = NULL;
		cv->xpoints_s = NULL;

		/* Create a GC for drawing the curve and set its colour */

		cv->gc = XCreateGC( G.d, G.canvas.pm, 0, 0 );
		XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ i ] ) );

		/* Create pixmaps for the out-of-display arrows */

		cv->up_arrow =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, up_arrow_bits,
										 up_arrow_width, up_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );
		G.up_arrow_w = up_arrow_width;
		G.up_arrow_h = up_arrow_width;

		cv->down_arrow =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, down_arrow_bits,
										 down_arrow_width, down_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );

		G.down_arrow_w = down_arrow_width;
		G.down_arrow_h = down_arrow_width;

		cv->left_arrow =
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, left_arrow_bits,
										 left_arrow_width, left_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );

		G.left_arrow_w = left_arrow_width;
		G.left_arrow_h = left_arrow_width;

		cv->right_arrow = 
			XCreatePixmapFromBitmapData( G.d, G.canvas.pm, right_arrow_bits,
										 right_arrow_width, right_arrow_height,
										 fl_get_pixel( G.colors[ i ] ),
										 fl_get_pixel( FL_INACTIVE ), depth );

		G.right_arrow_w = right_arrow_width;
		G.right_arrow_h = right_arrow_width;

		/* Create a GC for the font and set the appropriate colour */

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

		/* Now get also memory for the data */

		cv->points = T_malloc( G.nx * G.ny * sizeof( Scaled_Point ) );

		for ( sp = cv->points, j = 0; j < G.nx * G.ny; sp++, j++ )
			sp->exist = UNSET;

		cv->xpoints = T_malloc( G.nx * G.ny * sizeof( XPoint ) );
		cv->xpoints_s = T_malloc( G.nx * G.ny * sizeof( XPoint ) );

	}
}


/*----------------------------------------------------------------------*/
/* This routine creates a pixmap with the label for either the y- or z- */
/* axis (set `coord' to 1 for y and 2 for z). The problem is that it's  */
/* not possible to draw rotated text so we have to write the text to a  */
/* pixmap and then rotate this pixmap 'by hand'.                        */
/*----------------------------------------------------------------------*/

void create_label_pixmap( Canvas *c, int coord, char *label )
{
	Pixmap pm;
	int width, height;
	int i, j, k;
	int r_coord = coord;

	
	/* Make sure we don't do something stupid... */

	assert( ( coord == Y && c == &G.y_axis ) ||
			( coord == Z && ( c == &G.z_axis || c == &G.cut_z_axis ) ) );

	/* Distinguish between labels for the primary window and the cut window
	   (this function is never called for the cut windows y-axis) */

	if ( c == &G.cut_z_axis )
		r_coord += 3;

	/* Get size for intermediate pixmap */

	width = XTextWidth( G.font, label, strlen( label ) ) + 10;
	height = G.font_asc + G.font_desc + 5;

	/* Create the intermediate pixmap, fill with colour of the axis canvas and
	   draw the text */

    pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), width, height,
						fl_get_canvas_depth( c->obj ) );

	XFillRectangle( G.d, pm, c->gc, 0, 0, width, height );
	XDrawString( G.d, pm, c->font_gc, 5, height - 1 - G.font_desc, 
				 label, strlen( label ) );

	/* Create the real pixmap for the label */

    G.label_pm[ r_coord ] = XCreatePixmap( G.d, FL_ObjWin( c->obj ), height,
										   width,
										   fl_get_canvas_depth( c->obj ) );
	G.label_w[ r_coord ] = ( unsigned int ) height;
	G.label_h[ r_coord ] = ( unsigned int ) width;

	/* Now copy the contents of the intermediate pixmap to the final pixmap
	   but rotated by 90 degree ccw */

	for ( i = 0, k = width - 1; i < width; k--, i++ )
		for ( j = 0; j < height; j++ )
			XCopyArea( G.d, pm, G.label_pm[ r_coord ], c->gc,
					   i, j, 1, 1, j, k );

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

		if ( G.font )
			XFreeFont( G.d, G.font );

		if ( run_form )
		{
			canvas_off( &G.x_axis, run_form->x_axis );
			canvas_off( &G.y_axis, run_form->y_axis );
			canvas_off( &G.canvas, run_form->canvas );
		}

		if ( G.dim == 2 )
		{
			cut_form_close( );
			if ( run_form )
				canvas_off( &G.z_axis, run_form->z_axis );
		}
	}

	if ( run_form && fl_form_is_visible( run_form->run ) )
	{
		display_x = run_form->run->x;
		display_y = run_form->run->y;
		display_w = run_form->run->w;
		display_h = run_form->run->h;

		fl_hide_form( run_form->run );
	}
	else
		display_has_been_shown = UNSET;

	if ( run_form )
		fl_free_form( run_form->run );

	if ( G_stored )
	{
		memcpy( &G, G_stored, sizeof( Graphics ) );
		T_free( G_stored );
	}
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void graphics_free( void )
{
	long i;
	int coord;
	Curve_1d *cv;
	Curve_2d *cv2;


	/* Deallocate memory for pixmaps, scaled data and XPoints */
	/* The function must also work correctly when it is called because
	   we ran out of memory. The way things are organized after allocating
	   memory for a curve first the graphical elements are created and
	   afterwards memory for the data are allocated. */

	if ( G.dim == 1 )
		for ( i = 0; i < G.nc; i++ )
		{
			if ( ( cv = G.curve[ i ] ) == NULL )
				 break;

			XFreeGC( G.d, cv->gc );
			XFreePixmap( G.d, cv->up_arrow );
			XFreePixmap( G.d, cv->down_arrow );
			XFreePixmap( G.d, cv->left_arrow );
			XFreePixmap( G.d, cv->right_arrow );
			XFreeGC( G.d, cv->font_gc );

			T_free( cv->points );
			T_free( cv->xpoints );
			T_free( cv );
			G.curve_2d[ i ] = NULL;
		}
	else
		for ( i = 0; i < G.nc; i++ )
		{
			if ( ( cv2 = G.curve_2d[ i ] ) == NULL )
				break;

			XFreeGC( G.d, cv2->gc );
			XFreePixmap( G.d, cv2->up_arrow );
			XFreePixmap( G.d, cv2->down_arrow );
			XFreePixmap( G.d, cv2->left_arrow );
			XFreePixmap( G.d, cv2->right_arrow );
			XFreeGC( G.d, cv2->font_gc );

			T_free( cv2->points );
			T_free( cv2->xpoints );
			T_free( cv2->xpoints_s );
			T_free( cv2 );
			G.curve_2d[ i ] = NULL;
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

static void canvas_off( Canvas *c, FL_OBJECT *obj )
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

static void setup_canvas( Canvas *c, FL_OBJECT *obj )
{
	XSetWindowAttributes attributes;
	FL_HANDLE_CANVAS ch;


	if ( G.dim == 1 )
		ch = canvas_handler_1d;
	else
		ch = canvas_handler_2d;

	c->obj = obj;

	/* We need to make sure the canvas hasn't a size less than 1 otherwise
	   hell will break loose... */

	if ( obj->w > 0 )
		c->w = obj->w;
	else
		c->w = 1;
	if ( obj->h > 0 )
		c->h = obj->h;
	else
		c->h = 1;

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void redraw_axis( int coord )
{
	Canvas *c;
	Curve_1d *cv = NULL;
	int width;
	int i;


	/* First draw the label - for the x-axis it's just done by drawing the
	   string while for the y- and z-axis we have to copy a pixmap since the
	   label is a string rotated by 90 degree that has been drawn in advance */

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
		snprintf( lstr, MAX_LABEL_LEN, "%1.*E", n - 1, num );
	else if ( mag > 0 )                         /* num >= 1 */
	{
		if ( res >= 0 )
			snprintf( lstr, MAX_LABEL_LEN, "%*g", mag, num );
		else
			snprintf( lstr, MAX_LABEL_LEN, "%*.*f", n, abs( res ), num );
	}
	else if ( mag > -4 && res >= - 4 )          /* num > 10^-4 */
		snprintf( lstr, MAX_LABEL_LEN, "%1.*f", abs( res ), num );
	else                                        /* num < 10^-4 */
		if ( mag < res )
			snprintf( lstr, MAX_LABEL_LEN, "0" );
		else
			snprintf(  lstr, MAX_LABEL_LEN, "%1.*E", n, num );
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


/*--------------------------------------------*/
/* Undoes the last action as far as possible. */
/*--------------------------------------------*/

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
			fl_set_object_helper( run_form->full_scale_button,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
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
			fl_set_object_helper( run_form->full_scale_button,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
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

	/* Rescaling is useless if graphic isn't initialised */

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
								  "Rescale curves to fit into the window\n"
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
			sprintf( hstr, "Exempt curve %ld from\nrescaling operations",
					 data );
		else
			sprintf( hstr, "Include curve %ld into\nrescaling operations",
					 data );

		fl_set_object_helper( obj, hstr );

		/* Redraw both axis to make sure the axis for the first active button
		   is shown */

		redraw_canvas_1d( &G.x_axis );
		redraw_canvas_1d( &G.y_axis );
	}
	else
	{
		/* We get a negative curve number if this handler is called via the
		   hidden curve buttons in the cross section window and have to set
		   the correct state of the buttons in the main display window */

		if ( data < 0 )
		{
			data = - data;

			if ( data > G.nc )    /* button for non-existing curve triggered */
				return;           /* via the keyboard */

			switch( data )
			{
				case 1:
					fl_set_button( run_form->curve_1_button,
								   G.active_curve == 0 ? 0 : 1 );
					break;

				case 2:
					fl_set_button( run_form->curve_2_button,
								   G.active_curve == 1 ? 0 : 1 );
					break;

				case 3:
					fl_set_button( run_form->curve_3_button,
								   G.active_curve == 2 ? 0 : 1 );
					break;

				case 4:
					fl_set_button( run_form->curve_4_button,
								   G.active_curve == 3 ? 0 : 1 );
					break;
			}
		}

		/* Make buttons work like radio buttons but also allow switching off
		   all of them */

		if ( data - 1 == G.active_curve )     /* shown curve is switched off */
			G.active_curve = -1;
		else
		{
			switch ( G.active_curve )
			{
				case 0 :
					fl_set_button( run_form->curve_1_button, 0 );
					fl_set_object_helper( run_form->curve_1_button,
										  "Show curve 1" );
					break;

				case 1 :
					fl_set_button( run_form->curve_2_button, 0 );
					fl_set_object_helper( run_form->curve_2_button,
										  "Show curve 2" );
					break;

				case 2 :
					fl_set_button( run_form->curve_3_button, 0 );
					fl_set_object_helper( run_form->curve_3_button,
										  "Show curve 3" );
					break;

				case 3 :
					fl_set_button( run_form->curve_4_button, 0 );
					fl_set_object_helper( run_form->curve_4_button,
										  "Show curve 4" );
					break;
			}

			sprintf( hstr, "Hide curve %ld", data );
			fl_set_object_helper( obj, hstr );

			G.active_curve = data - 1;

			fl_set_button( run_form->full_scale_button,
						   G.curve_2d[ G.active_curve ]->is_fs );
		}

		redraw_all_2d( );
		cut_new_curve_handler( );
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


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void change_scale( int is_set, double *vals )
{
	int i;


	if ( is_set & 1 )
	{
		G.rwc_start[ X ] = vals[ 0 ];

		if ( G.dim == 2 )
			for ( i = 0; i < G.nc; i++ )
				G.curve_2d[ i ]->rwc_start[ X ] = vals[ 0 ];
	}

	if ( is_set & 2 )
	{
		G.rwc_delta[ X ] = vals[ 1 ];

		if ( G.dim == 2 )
			for ( i = 0; i < G.nc; i++ )
				G.curve_2d[ i ]->rwc_delta[ X ] = vals[ 1 ];
	}

	if ( G.dim == 1 )
		return;

	if ( is_set & 4 )
	{
		G.rwc_start[ Y ] = vals[ 2 ];

		for ( i = 0; i < G.nc; i++ )
			G.curve_2d[ i ]->rwc_start[ Y ] = vals[ 2 ];
	}

	if ( is_set & 8 )
	{
		G.rwc_delta[ Y ] = vals[ 3 ];

		for ( i = 0; i < G.nc; i++ )
			G.curve_2d[ i ]->rwc_delta[ Y ] = vals[ 3 ];
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void rescale( long new_nx, long new_ny )
{
	long i, j, k;
	Curve_1d *cv1;
	Curve_2d *cv;
	long max_x = 0,
		 max_y = 0;
	Scaled_Point *sp, *old_sp, *osp;
	bool need_cut_redraw = UNSET;


	if ( G.dim == 1 )
	{
		/* Return immediately on unreasonable values */

		if ( new_nx < 0 )
			return;

		/* Find the maximum x-index currently used by a point */

		for ( k = 0; k < G.nc; k++ )
			for ( sp = G.curve[ k ]->points, i = 0; i < G.nx; sp++, i++ )
				if ( sp->exist && i > max_x )
					max_x = i;
		max_x++;

		printf( "max_x = %ld\n", max_x );

		/* Make sure we don't rescale to less than the current number of
		   points (or the minumum value, if larger) */

		if ( new_nx == 0 || new_nx < DEFAULT_1D_X_POINTS )
		{
			if ( max_x < DEFAULT_1D_X_POINTS )
				max_x = DEFAULT_1D_X_POINTS;
		}
		else if ( new_nx > max_x )
			max_x = new_nx;

		for ( k = 0; k < G.nc; k++ )
		{
			cv1 = G.curve[ k ];

			cv1->points = T_realloc( cv1->points,
									 max_x * sizeof( Scaled_Point ) );
			cv1->xpoints = T_realloc( cv1->xpoints,
									  max_x * sizeof( XPoint ) );

			for ( i = G.nx, sp = G.curve[ k ]->points + i; i < max_x;
				  sp++, i++ )
				sp->exist = UNSET;
		}

		G.nx = max_x;

		for ( k = 0; k < G.nc; k++ )
		{
			cv1 = G.curve[ k ];
			if ( G.is_fs )
			{
				cv1->s2d[ X ] = ( double ) ( G.canvas.w - 1 )
				               / ( double ) ( G.nx - 1 );
				if ( G.is_scale_set )
					recalc_XPoints_of_curve_1d( cv1 );
			}
		}

		G.nx = max_x;
		return;
	}

	/* Here starts handling for 2D graphics, do nothing if both values are
	   less than 0 */

	if ( new_nx < 0 && new_ny < 0 )
		return;

	/* Find the maximum x and y coordinate */

	for ( k = 0; k < G.nc; k++ )
		for ( sp = G.curve_2d[ k ]->points, j = 0; j < G.ny; j++ )
			for ( i = 0; i < G.nx; sp++, i++ )
				if ( sp->exist )
				{
					max_y = j;
					if ( i > max_x )
						max_x = i;
				}
	max_x++;
	max_y++;

	/* Return immediately if both new values won't let all points displayed
	   anymore */

	if ( new_nx != 0 && new_nx <= max_x && new_ny != 0 && new_ny <= max_y )
		return;

	/* Figure out the correct new value */

	if ( new_nx < 0 )               /* don't change */
	{
		new_nx = G.nx;
		if ( G.nx < DEFAULT_2D_X_POINTS )
			new_nx = DEFAULT_2D_X_POINTS;
	}
	else if ( new_nx == 0 )         /* keep current value (at least minimum) */
	{
		new_nx = G.nx;
		if ( G.nx < DEFAULT_2D_X_POINTS )
			new_nx = DEFAULT_2D_X_POINTS;
	}
	else if ( new_nx < max_x )      /* show at least all current points */
		new_nx = max_x;
			
	if ( new_ny < 0 )               /* don't change */
	{
		new_ny = G.ny;
		if ( G.ny < DEFAULT_2D_Y_POINTS )
			new_ny = DEFAULT_2D_Y_POINTS;
	}
	else if ( new_ny == 0 )         /* keep current value (at least minimum) */
	{
		new_ny = G.nx;
		if ( G.ny < DEFAULT_2D_Y_POINTS )
			new_ny = DEFAULT_2D_Y_POINTS;
	}
	else if ( new_ny < max_y )      /* show at least all current points */
		new_ny = max_y;

	/* If everything will remain unchanged return */

	if ( G.nx >= new_nx && G.ny >= new_ny )
		return;

	/* Now we're really in for the fun... */

	for ( k = 0; k < G.nc; k++ )
	{
		cv = G.curve_2d[ k ];
			
		old_sp = osp = cv->points;
		sp = cv->points = T_malloc( new_nx * new_ny * sizeof( Scaled_Point ) );

		for ( j = 0; j < new_ny; j++, osp += G.nx, sp +=new_nx )
			memcpy( sp, osp, G.nx * sizeof( Scaled_Point ) );

		T_free( old_sp );

		cv->xpoints = T_realloc( cv->xpoints,
								 new_nx * new_ny * sizeof( XPoint ) );
		cv->xpoints_s = T_realloc( cv->xpoints_s,
								   new_nx * new_ny * sizeof( XPoint ) );

		/* Reorganise the old elements to fit into the new array and clear
		   the the new elements in the already existing rows */

		if ( cv->is_fs )
		{
			cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) /
			                                         ( double ) ( new_nx - 1 );
			cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 ) /
				                                     ( double ) ( new_ny - 1 );
		}
	}

	if ( G.nx != new_nx )
		 need_cut_redraw = cut_num_points_changed( X, new_nx );
	if ( G.ny != new_ny )
		need_cut_redraw |= cut_num_points_changed( Y, new_ny );

	G.nx = new_nx;
	G.ny = new_ny;

	for ( k = 0; k < G.nc; k++ )
		recalc_XPoints_of_curve_2d( G.curve_2d[ k ] );

	/* Update the cut window if necessary */

	if ( need_cut_redraw )
		redraw_all_cut_canvases( );
}
