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


static void cut_calc_curve( int dir, long index );
static void cut_recalc_XPoints( void );
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
static void cut_make_scale( Canvas *c, int coord );


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


/*-----------------------------------------------------------------------*/
/* Function gets called whenever in the 2D display the left mouse button */
/* has been pressed with one of the shift keys pressed down in the x- or */
/* y-axis canvas and than is released again (with the shift key is still */
/* being hold down). It opens up a new window displaying a cross section */
/* through the data at the positon the mouse button was released. If the */
/* window with the cross section is already shown the cut at the mouse   */
/* position is shown.                                                    */
/* dir: axis canvas the mouse button was pressed in (X or Y)             */
/* pos: x- or y- position the mouse was pressed at (in screen units)     */
/*      respectively)                                                    */
/*-----------------------------------------------------------------------*/

void cut_show( int dir, int pos )
{
	long index;
	Curve_1d *cv = &G.cut_curve;
	Curve_2d *scv = G.curve_2d[ G.active_curve ];


	/* Don't do anything if no curve is currently displayed */

	if ( G.active_curve == -1 )
		return;

	/* Don't do anything if we didn't end in one of the axis canvases */

	if ( pos < 0 ||
		 ( dir == X && ( unsigned int ) pos >= G.x_axis.w ) ||
		 ( dir == Y && ( unsigned int ) pos >= G.y_axis.h ) )
		return;

	/* If the cross section window hasn't been shown before create and
	   initialize now, otherwise, it's just unmapped, so make it visible
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
	}
	else if ( ! is_mapped )
	{
		XMapWindow( G.d, cut_form->cut->window );
		XMapSubwindows( G.d, cut_form->cut->window );
		is_mapped = SET;
	}

	fl_raise_form( cut_form->cut );

	/* Calculate index into curve where cut is to be shown */

	if ( dir == X )
		index = lround( ( double ) pos / scv->s2d[ X ] - scv->shift[ X ] );
	else
		index = lround( ( double ) ( G.canvas.h - 1 - pos ) / scv->s2d[ Y ]
						- scv->shift[ Y ] );

	/* Set up the labels and calculate the scaling factors if the cut window
	   didn't exist or the cut direction changed */

	if ( ! is_shown || CG.cut_dir != dir )
	{
		if ( is_shown )
		{
			if ( CG.cut_dir == X )                /* new direction is Y ! */
			{
				if( G.label[ X ] != NULL && G.font != NULL )
					XFreePixmap( G.d, G.label_pm[ Z + 3 ] );
				if ( G.label[ Y ] != NULL && G.font != NULL )
				{
					G.label_pm[ Z + 3 ] = G.label_pm[ Y ];
					G.label_w[ Z + 3 ] = G.label_w[ Y ];
					G.label_h[ Z + 3 ] = G.label_h[ Y ];
				}
			}
			else                                  /* new direction is X ! */
				if ( G.label[ X ] != NULL && G.font != NULL )
					create_label_pixmap( &G.cut_z_axis, Z, G.label[ X ] );
		}
		else
		{
			if ( dir == X )
			{
				if ( G.label[ X ] != NULL && G.font != NULL )
					create_label_pixmap( &G.cut_z_axis, Z, G.label[ X ] );
			}
			else if ( G.label[ Y ] != NULL && G.font != NULL )
			{
				G.label_pm[ Z + 3 ] = G.label_pm[ Y ];
				G.label_w[ Z + 3 ] = G.label_w[ Y ];
				G.label_h[ Z + 3 ] = G.label_h[ Y ];
			}
		}

		if ( scv->is_fs )
		{
			cv->s2d[ X ] = ( double ) ( G.cut_canvas.w - 1 ) /
						   ( double ) ( CG.nx - 1 );
			cv->s2d[ Y ] = ( double ) ( G.cut_canvas.h - 1 );
			cv->shift[ X ] = 0.0;
			cv->shift[ Y ] = 0.0;

			CG.is_fs = SET;
			fl_set_button( cut_form->cut_full_scale_button, 1 );
			fl_set_object_helper( cut_form->cut_full_scale_button,
								  "Switch off automatic rescaling" );
		}
		else
		{
			cv->s2d[ Y ] = scv->s2d[ Z ] / ( double ) ( G.z_axis.h - 1 )
			               * ( double ) ( G.cut_canvas.h - 1 );
			cv->shift[ Y ] = scv->shift[ Z ];

			cv->s2d[ X ] = ( double ) ( G.cut_canvas.w - 1 ) /
			               ( double ) ( CG.nx - 1 );
			cv->shift[ X ] = 0.0;

			CG.is_fs = UNSET;
			fl_set_button( cut_form->cut_full_scale_button, 0 );
			fl_set_object_helper( cut_form->cut_full_scale_button,
								  "Rescale curves to fit into the window\n"
								  "and switch on automatic rescaling" );
		}
	} else if ( index == CG.index )
		return;

	/* Calculate all the points of the cross section curve */

	G.is_cut = SET;
	is_shown = SET;

	cut_calc_curve( dir, index );

	/* Draw the curve and the axes */

	redraw_all_cut_canvases( );
}


/*----------------------------------------------------------*/
/* Extracts the points of the 2d-surface in direction 'dir' */
/* at point 'index', writes them into a 1d-curve and scales */
/* this curve.                                              */
/*----------------------------------------------------------*/

static void cut_calc_curve( int dir, long index )
{
	long i, j;
	Curve_1d *cv = &G.cut_curve;
	Curve_2d *scv = G.curve_2d[ G.active_curve ];


	/* Store direction of cross section - if called with an unreasonable
	   direction (i.e. not X or Y) keep the old direction) */

	if ( dir == X || dir == Y )
		CG.cut_dir = dir;

	/* Set maximum number of points of cut curve number and also set the x
	   scale factor if the number changed of points changed and we're in
	   full scale mode */

	if ( CG.cut_dir == X )
	{
		if ( CG.nx != G.ny && CG.is_fs )
			cv->s2d[ X ] = ( double ) ( G.cut_canvas.w - 1 ) /
				           ( double ) ( G.ny - 1 );
		CG.nx = G.ny;
	}
	else
	{
		if ( CG.nx != G.nx && CG.is_fs )
			cv->s2d[ X ] = ( double ) ( G.cut_canvas.w - 1 ) /
				           ( double ) ( G.nx - 1 );
		CG.nx = G.nx;
	}

	/* If the index is resonable store it (if called with index smaller than
	   zero keep the old index) */

	if ( index >= 0 )
		CG.index = index;

	/* Allocate memory for storing of scaled data and points for display */

	cv->points = T_realloc( cv->points, CG.nx * sizeof( Scaled_Point ) );
	cv->xpoints = T_realloc( cv->xpoints, CG.nx * sizeof( XPoint ) );

	/* Extract the existing scaled data of the cut from the 2d curve */

	if ( CG.cut_dir == X )
	{
		for ( i = 0, j = CG.index; i < CG.nx; j += G.nx, i++ )
		{
			if ( scv->points[ j ].exist )
				memcpy( cv->points + i, scv->points + j,
						sizeof( Scaled_Point ) );
			else
				cv->points[ i ].exist = UNSET;
		}
	}
	else
	{
		for ( i = 0, j = CG.index * G.nx; i < CG.nx; j++, i++ )
			if ( scv->points[ j ].exist )
				memcpy( cv->points + i, scv->points + j,
						sizeof( Scaled_Point ) );
			else
				cv->points[ i ].exist = UNSET;
	}

	cut_recalc_XPoints( );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void cut_recalc_XPoints( void )
{
	long k, j;
	Curve_1d *cv = &G.cut_curve;


	cv->up = cv->down = cv->left = cv->right = UNSET;

	for ( k = j = 0; j < CG.nx; j++ )
	{
		if ( cv->points[ j ].exist )
		{
			cv->xpoints[ k ].x = d2shrt( cv->s2d[ X ]
										 * ( j + cv->shift[ X ] ) );
			cv->xpoints[ k ].y = ( short ) G.cut_canvas.h - 1 - 
			   d2shrt( cv->s2d[ Y ] * ( cv->points[ j ].v + cv->shift[ Y ] ) );
			cv->points[ j ].xp_ref = k;

			if ( cv->xpoints[ k ].x < 0 )
				cv->left = SET;
			if ( cv->xpoints[ k ].x >= ( int ) G.cut_canvas.w )
				cv->right = SET;
			if ( cv->xpoints[ k ].y < 0 )
				cv->up = SET;
			if ( cv->xpoints[ k ].y >= ( int ) G.cut_canvas.h )
				cv->down = SET;

			k++;
		}
	}

	G.cut_curve.count = k;
}


/*------------------------------------------------------*/
/* Called whenever a different curve is to be displayed */
/*------------------------------------------------------*/

void cut_new_curve_handler( void )
{
	Curve_1d *cv = &G.cut_curve;


	if ( ! G.is_cut )
		return;

	if ( G.active_curve == -1 )
	{
		cv->points = T_free( cv->points );
		cv->xpoints = T_free( cv->xpoints );
		cv->count = 0;
	}
	else
	{
		fl_set_cursor_color( CG.cur_1, FL_BLACK, G.colors[ G.active_curve ] );
		fl_set_cursor_color( CG.cur_2, FL_BLACK, G.colors[ G.active_curve ] );
		fl_set_cursor_color( CG.cur_3, FL_BLACK, G.colors[ G.active_curve ] );
		fl_set_cursor_color( CG.cur_4, FL_BLACK, G.colors[ G.active_curve ] );
		fl_set_cursor_color( CG.cur_5, FL_BLACK, G.colors[ G.active_curve ] );
		fl_set_cursor_color( CG.cur_6, FL_BLACK, G.colors[ G.active_curve ] );
		fl_set_cursor_color( CG.cur_7, FL_BLACK, G.colors[ G.active_curve ] );
		fl_set_cursor_color( CG.cur_8, FL_BLACK, G.colors[ G.active_curve ] );

		cv->up_arrow    = CG.up_arrows[ G.active_curve ];
		cv->down_arrow  = CG.down_arrows[ G.active_curve ];
		cv->left_arrow  = CG.left_arrows[ G.active_curve ];
		cv->right_arrow = CG.right_arrows[ G.active_curve ];

		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );
		XSetForeground( G.d, cv->font_gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );

		cut_calc_curve( -1, -1 );
	}

	redraw_all_cut_canvases( );
}


/*---------------------------------------------------------------------------*/
/* Function is called by accept_2d_data() whenever the z-scaling has changed */
/*---------------------------------------------------------------------------*/

bool cut_data_rescaled( long curve )
{
	long i, j;
	Curve_1d *cv = &G.cut_curve;
	Curve_2d *scv = G.curve_2d[ G.active_curve ];


	if ( ! G.is_cut || curve != G.active_curve )
		return FAIL;

	/* Get the rescaled data of the cut from the 2d curve */

	if ( CG.cut_dir == X )
	{
		if ( ! CG.is_fs )
		{
			/* Calculate cv->s2d[ Y ] and cv->shift[ Y ] to fit the new
			   scaling*/
		}
		else
		{
			/* Set flag that tells us that we have to redraw the y-axis */
		}

		for ( i = 0, j = CG.index; i < CG.nx; j += G.nx, i++ )
			if ( scv->points[ j ].exist )
			{
				cv->points[ i ].v = scv->points[ j ].v;
				cv->xpoints[ cv->points[ i ].xp_ref ].y =
				           ( short ) G.cut_canvas.h - 1
				               - d2shrt( cv->s2d[ Y ] * ( cv->points[ i ].v
														  + cv->shift[ Y ] ) );
			}
	}
	else
	{
		if ( ! CG.is_fs )
		{
			/* Calculate cv->s2d[ Y ] and cv->shift[ Y ] to fit the new
			   scaling */
		}
		else
		{
			/* Set flag that tells us that we have to redraw the y-axis */
		}

		for ( i = 0, j = CG.index * G.nx; i < CG.nx; j++, i++ )
			if ( scv->points[ j ].exist )
			{
				cv->points[ i ].v = scv->points[ j ].v;
				cv->xpoints[ cv->points[ i ].xp_ref ].y =
				           ( short ) G.cut_canvas.h - 1
				               - d2shrt( cv->s2d[ Y ] * ( cv->points[ i ].v
														  + cv->shift[ Y ] ) );
			}
	}

	return OK;
}


/*-------------------------------------------------------*/
/* Function gets called by accept_2d_data() whenever the */
/* number of points in x- or y-direction changes         */
/*-------------------------------------------------------*/

bool cut_num_points_changed( int dir, long num_points )
{
	Curve_1d *cv = &G.cut_curve;
	Scaled_Point *sp;
	long k;


	if ( ! G.is_cut || dir == CG.cut_dir )
		return FAIL;

	/* Extend the arrays for the (scaled) data and the array of XPoints */

	cv->points = T_realloc( cv->points, num_points * sizeof( Scaled_Point ) );
	cv->xpoints = T_realloc( cv->xpoints, num_points * sizeof( XPoint ) );

	/* The new entries are not set yet */

	for ( k = CG.nx, sp = G.cut_curve.points + k ; k < num_points; sp++, k++ )
		sp->exist = UNSET;

	/* In full scale mode the x-axis needs to be rescaled */

	if ( CG.is_fs )
	{
		cv->s2d[ X ] = ( double ) ( G.cut_canvas.w - 1 ) /
			           ( double ) ( num_points - 1 );
		for ( sp = G.cut_curve.points, k = 0; k < CG.nx; sp++, k++ )
			if ( sp->exist )
				cv->xpoints[ sp->xp_ref ].x = d2shrt( cv->s2d[ X ] * k );

		/* Also set flag that tells us that we need to redraw the x-axis */
	}

	CG.nx = num_points;

	return CG.is_fs;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

bool cut_new_point( long curve, long x_index, long y_index, double val )
{
	long index;
	long xp_index;
	long i, j;
	Curve_1d *cv = &G.cut_curve;
	Scaled_Point *cvp;


	/* Nothing to be done if either the cross section isn't drawn, the new
	   point does not belong to the currently shown curve or if the new
	   point isn't laying on the cross section curve */

	if ( ! G.is_cut || curve != G.active_curve ||
		 ( CG.cut_dir == X && x_index != CG.index ) ||
		 ( CG.cut_dir == Y && y_index != CG.index ) )
		return FAIL;

	/* Calculate index of the new point in data set and set its value */

	index = CG.cut_dir == X ? y_index : x_index;
	cv->points[ index ].v = val;

	/* If this is a completely new point it as to be integrated into the
	   array of XPoints (which have to be sorted in ascending order of the
	   x-coordinate), otherwise the XPoint just needs to be updated */

	if ( ! cv->points[ index ].exist )
	{
		/* Find next existing point to the left */

		for ( i = index - 1, cvp = cv->points + i;
			  i >= 0 && ! cvp->exist; cvp--, i-- )
			;

		if ( i == -1 )                     /* new points to be drawn is first*/
		{
			xp_index = cv->points[ index ].xp_ref = 0;
			memmove( cv->xpoints + 1, cv->xpoints,
					 cv->count * sizeof( XPoint ) );
			for ( cvp = cv->points + 1, j = 1; j < CG.nx; cvp++, j++ )
				if ( cvp->exist )
					cvp->xp_ref++;
		}
		else if ( cv->points[ i ].xp_ref == cv->count - 1 )    /* ...is last */
			xp_index = cv->points[ index ].xp_ref = cv->count;
		else                                             /* ...is in between */
		{
			xp_index = cv->points[ index ].xp_ref = cv->points[ i ].xp_ref + 1;
			memmove( cv->xpoints + xp_index + 1, cv->xpoints + xp_index,
					 ( cv->count - xp_index ) * sizeof( XPoint ) );
			for ( j = index + 1, cvp = cv->points + j; j < CG.nx; cvp++, j++ )
				if ( cvp->exist )
					cvp->xp_ref++;
		}

		cv->points[ index ].exist = SET;
		cv->count++;

		/* Calculate the x-coordiante of the new point and figure out if it
		   exceeds the borders of the canvas */

		cv->xpoints[ xp_index ].x = d2shrt( cv->s2d[ X ]
											* ( index + cv->shift[ X ] ) );
		if ( cv->xpoints[ xp_index ].x < 0 )
			cv->left = SET;
		if ( cv->xpoints[ xp_index ].x >= ( int ) G.cut_canvas.w )
			cv->right = SET;
	}
	else
		xp_index = cv->points[ index ].xp_ref;

	/* Calculate the y-coordinate of the (new) point and figure out if it
	   exceeds the borders of the canvas */

	cv->xpoints[ xp_index ].y = ( short ) G.cut_canvas.h - 1 - 
		                d2shrt( cv->s2d[ Y ]
								* ( cv->points[ index ].v + cv->shift[ Y ] ) );

	if ( cv->xpoints[ xp_index ].y < 0 )
		cv->up = SET;
	if ( cv->xpoints[ xp_index ].y >= ( int ) G.cut_canvas.h )
		cv->down = SET;

	return OK;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

void cut_new_data_redraw( void )
{
	redraw_all_cut_canvases( );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void G_init_cut_curve( void )
{
	Curve_1d *cv = &G.cut_curve;
	unsigned int depth;
	int i;


	/* Create cursors */

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

	depth = fl_get_canvas_depth( G.cut_canvas.obj );

	for ( i = 0; i < MAX_CURVES; i++ )
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

	cv->can_undo = UNSET;

	CG.is_fs = SET;
	CG.nx = 0;

	/* The cut windows y-axis is always the same as the promary windows
	   z-axis, so we can re-use the label */

	if ( G.label[ Z ] != NULL && G.font != NULL )
	{
		G.label_pm[ Y + 3 ] = G.label_pm[ Z ];
		G.label_w[ Y + 3 ] = G.label_w[ Z ];
		G.label_h[ Y + 3 ] = G.label_h[ Z ];
	}
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

		for ( i = 0; i < MAX_CURVES; i++ )
		{
			XFreePixmap( G.d, CG.up_arrows[ i ] );
			XFreePixmap( G.d, CG.down_arrows[ i ] );
			XFreePixmap( G.d, CG.left_arrows[ i ] );
			XFreePixmap( G.d, CG.right_arrows[ i ] );
		}

		/* Get rid of pixmap for label */

		if ( CG.cut_dir == X && G.label[ X ] != NULL && G.font != NULL )
			XFreePixmap( G.d, G.label_pm[ Z + 3 ] );

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


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void redraw_all_cut_canvases( void )
{
	int coord;


	redraw_cut_canvas( &G.cut_canvas );
	for ( coord = X; coord <= Z; coord++ )
		redraw_cut_axis( coord );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void redraw_cut_canvas( Canvas *c )
{
	XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

	/* Finally copy the pixmap onto the screen */

	repaint_cut_canvas( c );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void repaint_cut_canvas( Canvas *c )
{
	Pixmap pm;
	Curve_1d *cv = &G.cut_curve;


	pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h,
						fl_get_canvas_depth( c->obj ) );
	XCopyArea( G.d, c->pm, pm, c->gc, 0, 0, c->w, c->h, 0, 0 );

	if ( c == &G.cut_canvas && cv->count > 1 )
		XDrawLines( G.d, pm, cv->gc, cv->xpoints, cv->count, 
					CoordModeOrigin );

	/* Finally copy the buffer pixmap onto the screen */

	XCopyArea( G.d, pm, FL_ObjWin( c->obj ), c->gc,
			   0, 0, c->w, c->h, 0, 0 );
	XFreePixmap( G.d, pm );
	XFlush( G.d );
}

/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void redraw_cut_center_canvas( Canvas *c )
{
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static void redraw_cut_axis( int coord )
{
	Canvas *c;
	int width;
	int r_coord;


	assert( coord >= X && coord <= Z );

	/* First draw the label - for the x-axis it's just done by drawing the
	   string while for the y- and z-axis we have to copy a pixmap since the
	   label is a string rotated by 90 degree that has been drawn in advance */

	if ( coord == X )
	{
		c = &G.cut_x_axis;
		XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

		if ( CG.cut_dir == X )
			r_coord = Y;
		else
			r_coord = X;

		if ( G.label[ r_coord ] != NULL && G.font != NULL )
		{
			width = XTextWidth( G.font, G.label[ r_coord ],
								strlen( G.label[ r_coord ] ) );
			XDrawString( G.d, c->pm, c->font_gc, c->w - width - 5,
						 c->h - 5 - G.font_desc, G.label[ r_coord ],
						 strlen( G.label[ r_coord ] ) );
		}
	}
	else if ( coord == Y )
	{
		c = &G.cut_y_axis;
		XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

		if ( G.label[ Z ] != NULL && G.font != NULL )
			XCopyArea( G.d, G.label_pm[ Y + 3 ], c->pm, c->gc, 0, 0,
					   G.label_w[ Y + 3 ], G.label_h[ Y + 3 ], 0, 0 );
	}
	else
	{
		c = &G.cut_z_axis;
		XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

		if ( G.label[ coord ] != NULL && G.font != NULL )
			XCopyArea( G.d, G.label_pm[ coord + 3 ], c->pm, c->gc, 0, 0,
					   G.label_w[ coord + 3 ], G.label_h[ coord + 3 ],
					   c->w - 5 - G.label_w[ coord + 3 ], 0 );
	}

	cut_make_scale( c, coord );
	XCopyArea( G.d, c->pm, FL_ObjWin( c->obj ), c->gc,
			   0, 0, c->w, c->h, 0, 0 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void cut_make_scale( Canvas *c, int coord )
{
	Curve_2d *cv = G.curve_2d[ G.active_curve ];
	double rwc_delta,          /* distance between small ticks (in rwc) */
		   order,              /* and its order of magnitude */
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
	char lstr[ MAX_LABEL_LEN + 1 ];
	int width;
	short last = -1000;
	int r_coord;
	double r_scale;
	XPoint triangle[ 3 ];


	/* The distance between the smallest ticks should be ca. `SCALE_TICK_DIST'
	   points - calculate the corresponding delta in real word units */

	if ( coord == X )
	{
		if ( CG.cut_dir == X )
		{
			r_coord = Y;
			r_scale = ( double ) c->w / ( double ) G.y_axis.h
				      * cv->s2d[ r_coord ];
		}
		else
		{
			r_coord = X;
			r_scale = ( double ) c->w / ( double ) G.x_axis.w
				      * cv->s2d[ r_coord ];
		}
	}
	else if ( coord == Y )
	{
		r_coord = Z;
		r_scale = ( double ) c->h / ( double ) G.z_axis.h
			      * cv->s2d[ r_coord ];
	}
	else
	{
		if ( CG.cut_dir == X )
		{
			r_coord = X;
			r_scale = ( double ) c->h / ( double ) G.x_axis.w
				     * ( double ) ( G.canvas.w - 1 ) / ( double ) ( G.nx - 1 );
		}
		else
		{
			r_coord = Y;
			r_scale = ( double ) c->h / ( double ) G.y_axis.h
				     * ( double ) ( G.canvas.h - 1 ) / ( double ) ( G.ny - 1 );
		}
	}

	rwc_delta = ( double ) SCALE_TICK_DIST * fabs( cv->rwc_delta[ r_coord ] ) /
		                                                               r_scale;

	/* Now scale this distance to the interval [ 1, 10 [ */

	mag = floor( log10( rwc_delta ) );
	order = pow( 10.0, mag );
	modf( rwc_delta / order, &rwc_delta );

	/* Get a `smooth' value for the ticks distance, i.e. either 2, 2.5, 5 or
	   10 and convert it to real world coordinates */

	if ( rwc_delta <= 2.0 )       /* in [ 1, 2 ] -> units of 2 */
	{
		medium_factor = 5;
		coarse_factor = 25;
		rwc_delta = 2.0 * order;
	}
	else if ( rwc_delta <= 3.0 )  /* in ] 2, 3 ] -> units of 2.5 */
	{
		medium_factor = 4;
		coarse_factor = 20;
		rwc_delta = 2.5 * order;
	}
	else if ( rwc_delta <= 6.0 )  /* in ] 3, 6 ] -> units of 5 */
	{
		medium_factor = 2;
		coarse_factor = 10;
		rwc_delta = 5.0 * order;
	}
	else                          /* in ] 6, 10 [ -> units of 10 */
	{
		medium_factor = 5;
		coarse_factor = 10;
		rwc_delta = 10.0 * order;
	}

	/* Calculate the final distance between the small ticks in points */

	d_delta_fine = r_scale * rwc_delta / fabs( cv->rwc_delta[ r_coord ] );

	/* `rwc_start' is the first value in the display (i.e. the smallest x or y
	   value still shown in the canvas), `rwc_start_fine' the position of the
	   first small tick (both in real world coordinates) and, finally,
	   `d_start_fine' is the same position but in points */

	if ( coord != Z )
		rwc_start = cv->rwc_start[ r_coord ]
			        - cv->shift[ r_coord ] * cv->rwc_delta[ r_coord ];
	else                           /* z-axis always shows the complete range */
		rwc_start = cv->rwc_start[ r_coord ];

	if ( cv->rwc_delta[ r_coord ] < 0 )
		rwc_delta *= -1.0;

	modf( rwc_start / rwc_delta, &rwc_start_fine );
	rwc_start_fine *= rwc_delta;

	d_start_fine = r_scale
		           * ( rwc_start_fine - rwc_start ) / cv->rwc_delta[ r_coord ];
	if ( lround( d_start_fine ) < 0 )
		d_start_fine += d_delta_fine;

	/* Calculate start index (in small tick counts) of first medium tick */

	modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
	rwc_start_medium *= medium_factor * rwc_delta;

	d_start_medium = r_scale * ( rwc_start_medium - rwc_start ) /
			                                          cv->rwc_delta[ r_coord ];
	if ( lround( d_start_medium ) < 0 )
		d_start_medium += medium_factor * d_delta_fine;

	medium = lround( ( d_start_fine - d_start_medium ) / d_delta_fine );

	/* Calculate start index (in small tick counts) of first large tick */

	modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
	rwc_start_coarse *= coarse_factor * rwc_delta;

	d_start_coarse = r_scale * ( rwc_start_coarse - rwc_start ) /
			                                          cv->rwc_delta[ r_coord ];
	if ( lround( d_start_coarse ) < 0 )
	{
		d_start_coarse += coarse_factor * d_delta_fine;
		rwc_start_coarse += coarse_factor * rwc_delta;
	}

	coarse = lround( ( d_start_fine - d_start_coarse ) / d_delta_fine );

	/* Now, finally we got everything we need to draw the axis... */

	rwc_coarse = rwc_start_coarse - coarse_factor * rwc_delta;

	if ( coord == X )
	{
		/* Draw coloured line of scale */

		y = X_SCALE_OFFSET;
		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );
		XFillRectangle( G.d, c->pm, cv->gc, 0, y - 2, c->w, 3 );

		/* Draw all the ticks and numbers */

		for ( cur_p = d_start_fine; cur_p < c->w; 
			  medium++, coarse++, cur_p += d_delta_fine )
		{
			x = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				XDrawLine( G.d, c->pm, c->font_gc, x, y + 3,
						   x, y - LONG_TICK_LEN );
				rwc_coarse += coarse_factor * rwc_delta;
				if ( G.font == NULL )
					continue;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				if ( x - width / 2 - 10 > last )
				{
					XDrawString( G.d, c->pm, c->font_gc, x - width / 2,
								 y + LABEL_DIST + G.font_asc, lstr,
								 strlen( lstr ) );
					last = x + width / 2;
				}
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x, y - MEDIUM_TICK_LEN );
			else                                       /* short line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x, y - SHORT_TICK_LEN );
		}
	}
	else if ( coord == Y )
	{
		/* Draw coloured line of scale */

		x = c->w - Y_SCALE_OFFSET;
		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );
		XFillRectangle( G.d, c->pm, cv->gc, x, 0, 3, c->h );

		/* Draw all the ticks and numbers */

		for ( cur_p = ( double ) c->h - 1.0 - d_start_fine; cur_p > -0.5; 
			  medium++, coarse++, cur_p -= d_delta_fine )
		{
			y = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				XDrawLine( G.d, c->pm, c->font_gc, x - 3, y,
						   x + LONG_TICK_LEN, y );
				rwc_coarse += coarse_factor * rwc_delta;

				if ( G.font == NULL )
					continue;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				XDrawString( G.d, c->pm, c->font_gc, x - LABEL_DIST - width,
							 y + G.font_asc / 2, lstr, strlen( lstr ) );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x + MEDIUM_TICK_LEN, y );
			else                                      /* short line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x + SHORT_TICK_LEN, y );
		}
	}
	else
	{
		/* Draw coloured line of scale */

		x = Z_SCALE_OFFSET;
		XSetForeground( G.d, cv->gc,
						fl_get_pixel( G.colors[ G.active_curve ] ) );
		XFillRectangle( G.d, c->pm, cv->gc, x - 2, 0, 3, c->h );

		/* Draw all the ticks and numbers */

		for ( cur_p = ( double ) c->h - 1.0 - d_start_fine; cur_p > -0.5;
			  medium++, coarse++, cur_p -= d_delta_fine )
		{
			y = d2shrt( cur_p );

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				XDrawLine( G.d, c->pm, c->font_gc, x + 3, y,
						   x - LONG_TICK_LEN, y );
				rwc_coarse += coarse_factor * rwc_delta;

				if ( G.font == NULL )
					continue;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				width = XTextWidth( G.font, lstr, strlen( lstr ) );
				XDrawString( G.d, c->pm, c->font_gc, x + LABEL_DIST,
							 y + G.font_asc / 2, lstr, strlen( lstr ) );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x - MEDIUM_TICK_LEN, y );
			else                                      /* short line */
				XDrawLine( G.d, c->pm, c->font_gc, x, y,
						   x - SHORT_TICK_LEN, y );
		}

		/* Finally draw the triangle indicating the position of the cut */

		triangle[ 0 ].x = x - LONG_TICK_LEN - 3;
		if ( CG.cut_dir == X )
			triangle[ 0 ].y = d2shrt( ( G.cut_z_axis.h - 1 ) *
					 ( 1.0 - ( double ) CG.index / ( double ) ( G.nx - 1 ) ) );
		else
			triangle[ 0 ].y = d2shrt( ( G.cut_z_axis.h - 1 ) *
					 ( 1.0 - ( double ) CG.index / ( double ) ( G.ny - 1 ) ) );
		triangle[ 1 ].x = - ( Z_SCALE_OFFSET - LONG_TICK_LEN - 10 );
		triangle[ 1 ].y = - LONG_TICK_LEN / 3;
		triangle[ 2 ].x = 0;
		triangle[ 2 ].y = 2 * ( LONG_TICK_LEN / 3 );
		XFillPolygon( G.d, c->pm, G.curve_2d[ 0 ]->gc, triangle, 3,
					  Convex, CoordModePrevious );
	}
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void cut_clear_curve( long curve )
{
	long i;
	Scaled_Point *sp;


	if ( ! G.is_cut || curve != G.active_curve )
		return;

	for ( sp = G.cut_curve.points, i = 0; i < CG.nx; sp++, i++ )
		sp->exist = UNSET;
	G.cut_curve.count = 0;
}
