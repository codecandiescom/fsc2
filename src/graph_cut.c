/*
 *  Copyright (C) 1999-2016 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2.h"

#include "cursor1.xbm"             /* bitmaps for cursors */
#include "cursor2.xbm"
#include "cursor3.xbm"
#include "cursor4.xbm"
#include "cursor5.xbm"
#include "cursor6.xbm"
#include "cursor7.xbm"
#include "cursor8.xbm"

#include "up_arrow.xbm"             /* arrow bitmaps */
#include "down_arrow.xbm"
#include "left_arrow.xbm"
#include "right_arrow.xbm"


Cut_Graphics_T G_cut;


static void cut_calc_curve( int  dir,
                            long p_index,
                            bool has_been_shown );
static void G_init_cut_curve( void );
static void cut_setup_canvas( Canvas_T  * c,
                              FL_OBJECT * obj );
static void cut_canvas_off( Canvas_T  * c,
                            FL_OBJECT * obj );
static int cut_form_close_handler( FL_FORM * a,
                                   void    * b );
static void cut_recalc_XPoints( void );
static void cut_integrate_point( long   p_index,
                                 double val );
static int cut_canvas_handler( FL_OBJECT * obj,
                               Window      window,
                               int         w,
                               int         h,
                               XEvent    * ev,
                               void      * udata );
static void cut_reconfigure_window( Canvas_T * c,
                                    int        w,
                                    int        h );
static void cut_press_handler( FL_OBJECT * obj,
                               Window      window,
                               XEvent    * ev,
                               Canvas_T  * c );
static void cut_release_handler( FL_OBJECT * obj,
                                 Window      window,
                                 XEvent    * ev,
                                 Canvas_T  * c );
static void cut_motion_handler( FL_OBJECT * obj,
                                Window      window,
                                XEvent    * ev,
                                Canvas_T  * c );
static void redraw_cut_canvas( Canvas_T * c );
static void repaint_cut_canvas( Canvas_T * c );
static void cut_make_scale( Canvas_T * c,
                            int        coord );
static void shift_XPoints_of_cut_curve( Canvas_T * c );
static bool cut_change_x_range( Canvas_T * c );
static bool cut_change_y_range( Canvas_T * c );
static bool cut_change_xy_range( Canvas_T * c );
static bool cut_zoom_x( Canvas_T * c );
static bool cut_zoom_y( Canvas_T * c );
static bool cut_zoom_xy( Canvas_T * c );
static void cut_save_scale_state( void );


extern FL_resource Xresources[ ];

static int cursor_c[ 8 ];

static struct {
    unsigned int WIN_MIN_WIDTH;
    unsigned int WIN_MIN_HEIGHT;
} GC_sizes;


static bool is_mapped = false;           /* set while form is mapped */


/*--------------------------------------------------------------------*
 * Function to initialize the minimum sizes of the cut graphic window
 *--------------------------------------------------------------------*/

void
cut_init( void )
{
    if ( GUI.G_Funcs.size == ( bool ) LOW )
    {
        GC_sizes.WIN_MIN_WIDTH  = 350;
        GC_sizes.WIN_MIN_HEIGHT = 220;
    }
    else
    {
        GC_sizes.WIN_MIN_WIDTH  = 500;
        GC_sizes.WIN_MIN_HEIGHT = 310;
    }
}


/*-----------------------------------------------------------------------*
 * Function gets called whenever in the 2D display the left mouse button
 * has been pressed with one of the shift keys pressed down in the x- or
 * y-axis canvas and than is released again (with the shift key is still
 * being hold down). It opens up a new window displaying a cross section
 * through the data at the positon the mouse button was released. If the
 * window with the cross section is already shown the cut at the mouse
 * position is shown.
 * dir: axis canvas the mouse button was pressed in (X or Y)
 *-----------------------------------------------------------------------*/

void
cut_show( int  dir,
          long u_index )
{
    int flags;
    int x, y;
    unsigned int h, w;
    Window win;
    Curve_1d_T *cv = &G_2d.cut_curve;


    /* Don't do anything if no curve is currently displayed or if mouse didn't
       got released in one of the axis canvases */

    if (    G_2d.active_curve == -1 || u_index < 0
         || ( dir == X && u_index >= G_2d.nx )
         || ( dir == Y && u_index >= G_2d.ny ) )
        return;

    /* If the cross section window hasn't been shown before create and
       initialize now, otherwise, it's just unmapped, so make it visible
       again */

    if ( ! G_cut.is_shown )
    {
        fl_set_object_shortcutkey( GUI.cut_form->change_button, XK_space );

        if (    ! GUI.cut_win_has_size
             && ! GUI.cut_win_has_pos
             && ( ( char * ) Xresources[ CUTGEOMETRY ].var ) != '\0' )
        {
            flags = XParseGeometry( ( char * ) Xresources[ CUTGEOMETRY ].var,
                                    &x, &y, &w, &h );
            if ( WidthValue & flags && HeightValue & flags )
            {
                GUI.cut_win_width = w;
                GUI.cut_win_height = h;
                GUI.cut_win_has_size = true;
            }

            if ( XValue & flags && YValue & flags )
            {
                GUI.cut_win_x = x;
                GUI.cut_win_y = y;
                GUI.cut_win_has_pos = true;
            }
        }

        if ( GUI.cut_win_has_size )
        {
            if ( GUI.cut_win_width < GC_sizes.WIN_MIN_WIDTH )
                GUI.cut_win_width = GC_sizes.WIN_MIN_WIDTH;

            if ( GUI.cut_win_height < GC_sizes.WIN_MIN_HEIGHT )
                    GUI.cut_win_height = GC_sizes.WIN_MIN_HEIGHT;

            if ( ! GUI.cut_win_has_pos )
                fl_set_form_size( GUI.cut_form->cut,
                                  GUI.cut_win_width, GUI.cut_win_height );
        }

        if ( GUI.cut_win_has_pos )
            fl_set_form_geometry( GUI.cut_form->cut, GUI.cut_win_x,
                                  GUI.cut_win_y, GUI.cut_win_width,
                                  GUI.cut_win_height );

        win = fl_show_form( GUI.cut_form->cut, GUI.cut_win_has_pos ?
                            FL_PLACE_POSITION : FL_PLACE_MOUSE | FL_FREE_SIZE,
                            FL_FULLBORDER, "fsc2: Cross section" );

        fl_addto_selected_xevent( win, FocusIn | FocusOut );
        fl_register_raw_callback( GUI.cut_form->cut, FL_ALL_EVENT,
                                  form_event_handler );

        fl_winminsize( GUI.cut_form->cut->window,
                       GC_sizes.WIN_MIN_WIDTH, GC_sizes.WIN_MIN_HEIGHT );
        fl_set_form_atclose( GUI.cut_form->cut, cut_form_close_handler, NULL );

        cut_setup_canvas( &G_2d.cut_x_axis, GUI.cut_form->cut_x_axis );
        cut_setup_canvas( &G_2d.cut_y_axis, GUI.cut_form->cut_y_axis );
        cut_setup_canvas( &G_2d.cut_z_axis, GUI.cut_form->cut_z_axis );
        cut_setup_canvas( &G_2d.cut_canvas, GUI.cut_form->cut_canvas );

        G_init_cut_curve( );
    }
    else if ( ! is_mapped )
    {
        fl_set_form_geometry( GUI.cut_form->cut, GUI.cut_win_x,
                              GUI.cut_win_y, GUI.cut_win_width,
                              GUI.cut_win_height );
        fl_show_form( GUI.cut_form->cut, FL_PLACE_POSITION,
                      FL_FULLBORDER, "fsc2: Cross section" );

        /* Make sure the colors for the curves elements are correct */

        XSetForeground( G.d, cv->gc,
                        fl_get_pixel( G.colors[ G_2d.active_curve ] ) );
        cv->up_arrow    = G_cut.up_arrows[    G_2d.active_curve ];
        cv->down_arrow  = G_cut.down_arrows[  G_2d.active_curve ];
        cv->left_arrow  = G_cut.left_arrows[  G_2d.active_curve ];
        cv->right_arrow = G_cut.right_arrows[ G_2d.active_curve ];
    }

    fl_raise_form( GUI.cut_form->cut );

    /* Set up the labels if the cut window does't not exist yet or the cut
       direction changed */

    if ( ! G_cut.is_shown || G_cut.cut_dir != dir )
    {
        if ( G_cut.is_shown )
        {
            if ( G_cut.cut_dir == X )                /* new direction is Y ! */
            {
                if( G_2d.label[ X ] != NULL )
                    XFreePixmap( G.d, G_2d.label_pm[ Z + 3 ] );
                if ( G_2d.label[ Y ] != NULL)
                {
                    G_2d.label_pm[ Z + 3 ] = G_2d.label_pm[ Y ];
                    G_2d.label_w[ Z + 3 ]  = G_2d.label_w[ Y ];
                    G_2d.label_h[ Z + 3 ]  = G_2d.label_h[ Y ];
                }
            }
            else if ( G_2d.label[ X ] != NULL )      /* new direction is X ! */
            {
                create_label_pixmap( &G_2d.cut_z_axis, Z, G_2d.label[ X ] );
            }
        }
        else
        {
            if ( dir == X )
            {
                if ( G_2d.label[ X ] != NULL )
                    create_label_pixmap( &G_2d.cut_z_axis, Z,
                                         G_2d.label[ X ] );
            }
            else if ( G_2d.label[ Y ] != NULL )
            {
                G_2d.label_pm[ Z + 3 ] = G_2d.label_pm[ Y ];
                G_2d.label_w[ Z + 3 ]  = G_2d.label_w[ Y ];
                G_2d.label_h[ Z + 3 ]  = G_2d.label_h[ Y ];
            }
        }

        fl_set_object_callback( GUI.cut_form->cut_print_button,
                                print_it, - dir );
    }

    /* Calculate all the points of the cross section curve */

    cut_calc_curve( dir, u_index, G_cut.has_been_shown[ G_2d.active_curve ] );

    if ( ! G_cut.has_been_shown[ G_2d.active_curve ] )
    {
        G_cut.is_fs[ G_2d.active_curve ] =
                                     G_2d.curve_2d[ G_2d.active_curve ]->is_fs;
        G_cut.has_been_shown[ G_2d.active_curve ] = true;
    }

    is_mapped = true;
    G_2d.is_cut = true;
    G_cut.is_shown = true;
    G_cut.curve = G_2d.active_curve;

    /* Draw the curve and the axes */

    redraw_all_cut_canvases( );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
cut_setup_canvas( Canvas_T  * c,
                  FL_OBJECT * obj )
{
    XSetWindowAttributes attributes;
    FL_HANDLE_CANVAS ch = cut_canvas_handler;


    c->obj = obj;

    /* Make sure the canvas width and height of at least 1 */

    c->w = obj->w > 0 ? obj->w : 1;
    c->h = obj->h > 0 ? obj->h : 1;

    create_pixmap( c );

    c->xftdraw = XftDrawCreate( G.d, c->pm, fl_get_visual(),
                                fl_get_colormap( ) );

    fl_add_canvas_handler( c->obj, Expose, ch, ( void * ) c );

    if ( G.is_init )
    {
        attributes.backing_store = NotUseful;
        attributes.background_pixmap = None;
        XChangeWindowAttributes( G.d, FL_ObjWin( c->obj ),
                                 CWBackingStore | CWBackPixmap,
                                 &attributes );
        c->is_box = false;

        fl_add_canvas_handler( c->obj, ConfigureNotify, ch, ( void * ) c );
        fl_add_canvas_handler( c->obj, ButtonPress, ch, ( void * ) c );
        fl_add_canvas_handler( c->obj, ButtonRelease, ch, ( void * ) c );
        fl_add_canvas_handler( c->obj, MotionNotify, ch, ( void * ) c );

        /* Get motion events only when first or second button is pressed
           (this got to be set *after* requesting motion events) */

        fl_remove_selected_xevent( FL_ObjWin( obj ),
                                   PointerMotionMask | PointerMotionHintMask |
                                   ButtonMotionMask );
        fl_addto_selected_xevent( FL_ObjWin( obj ),
                                  Button1MotionMask | Button2MotionMask );

        XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );

        if ( c == &G_2d.cut_canvas )
            XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
    }

    c->axis_gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, NULL );
    XSetForeground( G.d, c->axis_gc, fl_get_pixel( FL_BLACK ) );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static void
cut_canvas_off( Canvas_T  * c,
                FL_OBJECT * obj )
{
    FL_HANDLE_CANVAS ch = cut_canvas_handler;


    G_2d.is_cut = false;

    fl_remove_canvas_handler( obj, Expose, ch );

    fl_remove_canvas_handler( obj, ConfigureNotify, ch );
    fl_remove_canvas_handler( obj, ButtonPress, ch );
    fl_remove_canvas_handler( obj, ButtonRelease, ch );
    fl_remove_canvas_handler( obj, MotionNotify, ch );

    XFreeGC( G.d, c->axis_gc );
    XftDrawDestroy( c->xftdraw );

    delete_pixmap( c );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
G_init_cut_curve( void )
{
    static bool first_time = true;
    Curve_1d_T *cv = &G_2d.cut_curve;
    unsigned int depth;
    int i;


    /* Create cursors */

    if ( first_time )
    {
        cursor_c[ ZOOM_BOX_CURSOR ] =
                    fl_create_bitmap_cursor( cursor1_bits, cursor1_bits,
                                             cursor1_width, cursor1_height,
                                             cursor1_x_hot, cursor1_y_hot );
        cursor_c[ MOVE_HAND_CURSOR ] =
                    fl_create_bitmap_cursor( cursor2_bits, cursor2_bits,
                                             cursor2_width, cursor2_height,
                                             cursor2_x_hot, cursor2_y_hot );
        cursor_c[ ZOOM_LENS_CURSOR ] =
                    fl_create_bitmap_cursor( cursor3_bits, cursor3_bits,
                                             cursor3_width, cursor3_height,
                                             cursor3_x_hot, cursor3_y_hot );
        cursor_c[ CROSSHAIR_CURSOR ] =
                    fl_create_bitmap_cursor( cursor4_bits, cursor4_bits,
                                             cursor4_width, cursor4_height,
                                             cursor4_x_hot, cursor4_y_hot );
        cursor_c[ TARGET_CURSOR ] =
                    fl_create_bitmap_cursor( cursor5_bits, cursor5_bits,
                                             cursor5_width, cursor5_height,
                                             cursor5_x_hot, cursor5_y_hot );
        cursor_c[ ARROW_UP_CURSOR ] =
                    fl_create_bitmap_cursor( cursor6_bits, cursor6_bits,
                                             cursor6_width, cursor6_height,
                                             cursor6_x_hot, cursor6_y_hot );
        cursor_c[ ARROW_RIGHT_CURSOR ] =
                    fl_create_bitmap_cursor( cursor7_bits, cursor7_bits,
                                             cursor7_width, cursor7_height,
                                             cursor7_x_hot, cursor7_y_hot );
        cursor_c[ ARROW_LEFT_CURSOR ] =
                    fl_create_bitmap_cursor( cursor8_bits, cursor8_bits,
                                             cursor8_width, cursor8_height,
                                             cursor8_x_hot, cursor8_y_hot );
        first_time = false;
    }

    for ( i = 0; i < 8; i++ )
        fl_set_cursor_color( cursor_c[ i ], FL_RED, FL_WHITE );

    /* Clear the pointer to the still non-existent points */

    cv->points = NULL;
    cv->xpoints = NULL;

    /* Create a GC for drawing the curve and set its colour */

    cv->gc = XCreateGC( G.d, G_2d.cut_canvas.pm, 0, NULL );
    XSetForeground( G.d, cv->gc,
                    fl_get_pixel( G.colors[ G_2d.active_curve ] ) );

    /* Create pixmaps for the out-of-range arrows (in the colors associated
       with the four possible curves) */

    depth = fl_get_canvas_depth( G_2d.cut_canvas.obj );

    for ( i = 0; i < MAX_CURVES; i++ )
    {
        G_cut.up_arrows[ i ] =
            XCreatePixmapFromBitmapData( G.d, G_2d.cut_canvas.pm,
                                         up_arrow_bits,
                                         up_arrow_width, up_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_BLACK ), depth );
        G_cut.up_arrow_w = up_arrow_width;
        G_cut.up_arrow_h = up_arrow_width;

        G_cut.down_arrows[ i ] =
            XCreatePixmapFromBitmapData( G.d, G_2d.cut_canvas.pm,
                                         down_arrow_bits, down_arrow_width,
                                         down_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_BLACK ), depth );
        G_cut.down_arrow_w = down_arrow_width;
        G_cut.down_arrow_h = down_arrow_width;

        G_cut.left_arrows[ i ] =
            XCreatePixmapFromBitmapData( G.d, G_2d.cut_canvas.pm,
                                         left_arrow_bits, left_arrow_width,
                                         left_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_BLACK ), depth );
        G_cut.left_arrow_w = left_arrow_width;
        G_cut.left_arrow_h = left_arrow_width;

        G_cut.right_arrows[ i ] =
            XCreatePixmapFromBitmapData( G.d, G_2d.cut_canvas.pm,
                                         right_arrow_bits, right_arrow_width,
                                         right_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_BLACK ), depth );
        G_cut.right_arrow_w = right_arrow_width;
        G_cut.right_arrow_h = right_arrow_width;

        G_cut.is_fs[ i ] = true;
        G_cut.has_been_shown[ i ] = false;
        G_cut.can_undo[ i ] = false;
    }

    /* Set the pixmaps for the out-of-range arrow to the pixmaps with the
       color associated with the currently displayed curve */

    cv->up_arrow    = G_cut.up_arrows[    G_2d.active_curve ];
    cv->down_arrow  = G_cut.down_arrows[  G_2d.active_curve ];
    cv->left_arrow  = G_cut.left_arrows[  G_2d.active_curve ];
    cv->right_arrow = G_cut.right_arrows[ G_2d.active_curve ];

    G_cut.nx = 0;

    fl_set_object_shortcutkey( GUI.cut_form->top_button, XK_Page_Up );
    fl_set_object_shortcutkey( GUI.cut_form->bottom_button, XK_Page_Down );

    /* The cut windows y-axis is always the same as the promary windows
       z-axis, so we can re-use the label */

    if ( G_2d.label[ Z ] != NULL )
    {
        G_2d.label_pm[ Y + 3 ] = G_2d.label_pm[ Z ];
        G_2d.label_w[ Y + 3 ]  = G_2d.label_w[ Z ];
        G_2d.label_h[ Y + 3 ]  = G_2d.label_h[ Z ];
    }
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

static int
cut_form_close_handler( FL_FORM * a  UNUSED_ARG,
                        void    * b  UNUSED_ARG )
{
    cut_close_callback( GUI.cut_form->cut_close_button, 0 );
    return FL_IGNORE;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
cut_form_close( void )
{
    int i;


    if ( ! G_cut.is_shown )
        return;

    /* Get rid of canvas related stuff */

    cut_canvas_off( &G_2d.cut_x_axis, GUI.cut_form->cut_x_axis );
    cut_canvas_off( &G_2d.cut_y_axis, GUI.cut_form->cut_y_axis );
    cut_canvas_off( &G_2d.cut_z_axis, GUI.cut_form->cut_z_axis );
    cut_canvas_off( &G_2d.cut_canvas, GUI.cut_form->cut_canvas );

    /* Deallocate the pixmaps for the out-of-range arrows */

    for ( i = 0; i < MAX_CURVES; i++ )
    {
        XFreePixmap( G.d, G_cut.up_arrows[ i ] );
        XFreePixmap( G.d, G_cut.down_arrows[ i ] );
        XFreePixmap( G.d, G_cut.left_arrows[ i ] );
        XFreePixmap( G.d, G_cut.right_arrows[ i ] );
    }

    /* Get rid of pixmap for label */

    if ( G_cut.cut_dir == X && G_2d.label[ X ] != NULL )
        XFreePixmap( G.d, G_2d.label_pm[ Z + 3 ] );

    /* Hide the form (also storing its last position) if it's still shown */

    if ( GUI.cut_form && fl_form_is_visible( GUI.cut_form->cut ) && is_mapped )
    {
        get_form_position( GUI.cut_form->cut, &GUI.cut_win_x,
                           &GUI.cut_win_y );
        GUI.cut_win_has_pos = true;

        GUI.cut_win_width  = GUI.cut_form->cut->w;
        GUI.cut_win_height = GUI.cut_form->cut->h;
        GUI.cut_win_has_size = true;

        fl_hide_form( GUI.cut_form->cut );
    }

    fl_free_form( GUI.cut_form->cut );
    fl_free( GUI.cut_form );
    GUI.cut_form = NULL;

    G_cut.is_shown = is_mapped = false;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
cut_close_callback( FL_OBJECT * a  UNUSED_ARG,
                    long        b  UNUSED_ARG )
{
    int i;


    G.coord_display &= ~ 4;
    G.dist_display  &= ~ 4;
    delete_all_cut_markers( false );

    get_form_position( GUI.cut_form->cut, &GUI.cut_win_x, &GUI.cut_win_y );
    GUI.cut_win_has_pos = true;

    GUI.cut_win_width  = GUI.cut_form->cut->w;
    GUI.cut_win_height = GUI.cut_form->cut->h;
    GUI.cut_win_has_size = true;

    fl_hide_form( GUI.cut_form->cut );

    G_2d.is_cut = is_mapped = false;

    for ( i = 0; i < MAX_CURVES; i++ )
        G_cut.has_been_shown[ i ] = false;
}


/*----------------------------------------------------------*
 * Extracts the points of the 2d-surface in direction 'dir'
 * at point 'index', writes them into a 1d-curve and scales
 * this curve.
 *----------------------------------------------------------*/

static void
cut_calc_curve( int  dir,
                long p_index,
                bool has_been_shown )
{
    long i;
    Curve_1d_T *cv = &G_2d.cut_curve;
    Curve_2d_T *scv = G_2d.curve_2d[ G_2d.active_curve ];
    Scaled_Point_T *sp, *ssp;
    Marker_2d_T *m;


    /* Store direction of cross section - if called with an unreasonable
       direction (i.e. not X or Y) keep the old direction) */

    if ( dir == X || dir == Y )
        G_cut.cut_dir = dir;

    /* Set maximum number of points of cut curve number and also set the x
       scale factor if the number changed of points changed and we're in
       full scale mode */

    G_cut.nx = ( G_cut.cut_dir == X ) ? G_2d.ny : G_2d.nx;

    if ( ! G_cut.is_shown || ! is_mapped || ! has_been_shown )
    {
        if ( scv->is_fs )
        {
            G_cut.s2d[ G_2d.active_curve ][ X ] = cv->s2d[ X ] =
                                ( G_2d.cut_canvas.w - 1.0 ) / ( G_cut.nx - 1 );
            G_cut.s2d[ G_2d.active_curve ][ Y ] = cv->s2d[ Y ] =
                                                       G_2d.cut_canvas.h - 1.0;
            G_cut.shift[ G_2d.active_curve ][ X ] =
                                   G_cut.shift[ G_2d.active_curve ][ Y ] =
                                         cv->shift[ X ] = cv->shift[ Y ] = 0.0;

            G_cut.is_fs[ G_2d.active_curve ] = true;
            fl_set_button( GUI.cut_form->cut_full_scale_button, 1 );
            if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                                      "Switch off automatic rescaling" );
        }
        else
        {
            G_cut.s2d[ G_2d.active_curve ][ Y ] = cv->s2d[ Y ] =
                                          scv->s2d[ Z ] / ( G_2d.z_axis.h - 1 )
                                          * ( G_2d.cut_canvas.h - 1 );
            G_cut.shift[ G_2d.active_curve ][ Y ] =
                                              cv->shift[ Y ] = scv->shift[ Z ];

            if ( G_cut.cut_dir == X )
            {
                G_cut.s2d[ G_2d.active_curve ][ X ] = cv->s2d[ X ] =
                                          scv->s2d[ Y ] / ( G_2d.canvas.h - 1 )
                                          * ( G_2d.cut_canvas.w - 1 );
                G_cut.shift[ G_2d.active_curve ][ X ] = cv->shift[ X ] =
                                                               scv->shift[ Y ];
            }
            else
            {
                G_cut.s2d[ G_2d.active_curve ][ X ] = cv->s2d[ X ] =
                                          scv->s2d[ X ] / ( G_2d.canvas.w - 1 )
                                          * ( G_2d.cut_canvas.w - 1 );
                G_cut.shift[ G_2d.active_curve ][ X ] = cv->shift[ X ] =
                                                               scv->shift[ X ];
            }

            G_cut.is_fs[ G_2d.active_curve ] = false;
            fl_set_button( GUI.cut_form->cut_full_scale_button, 0 );
            if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                                      "Rescale curves to fit into the window\n"
                                      "and switch on automatic rescaling" );
        }

        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.cut_form->cut_close_button,
                                  "Removes this window" );
    }
    else if ( G_cut.is_fs[ G_2d.active_curve ] )
    {
        G_cut.s2d[ G_2d.active_curve ][ X ] =  cv->s2d[ X ] =
                                ( G_2d.cut_canvas.w - 1.0 ) / ( G_cut.nx - 1 );
        G_cut.s2d[ G_2d.active_curve ][ Y ] = cv->s2d[ Y ] =
                                                       G_2d.cut_canvas.h - 1.0;
        G_cut.shift[ G_2d.active_curve ][ X ] =
                                   G_cut.shift[ G_2d.active_curve ][ Y ]
                                       = cv->shift[ X ] = cv->shift[ Y ] = 0.0;
    }

    /* If the index is reasonable store it (if called with an index smaller
       than zero keep the old index) */

    if ( p_index >= 0 )
        G_cut.index = p_index;

    /* Allocate memory for storing of scaled data and points for display */

    cv->points = T_realloc( cv->points, G_cut.nx * sizeof *cv->points );
    cv->xpoints = T_realloc( cv->xpoints, G_cut.nx * sizeof *cv->xpoints );

    /* Extract the existing scaled data of the cut from the 2d curve */

    if ( G_cut.cut_dir == X )
    {
        for ( i = 0, sp = cv->points, ssp = scv->points + G_cut.index;
              i < G_cut.nx; ssp += G_2d.nx, sp++, i++ )
            if ( ssp->exist )
                memcpy( sp, ssp, sizeof *sp );
            else
                sp->exist = false;
    }
    else
    {
        for ( i = 0, sp = cv->points,
              ssp = scv->points + G_cut.index * G_2d.nx;
              i < G_cut.nx; ssp++, sp++, i++ )
            if ( ssp->exist )
                memcpy( sp, ssp, sizeof *sp );
            else
                sp->exist = false;
    }

    delete_all_cut_markers( false );

    for ( m = scv->marker_2d; m != NULL; m = m->next )
    {
        if ( G_cut.cut_dir == X && m->x_pos == G_cut.index )
            set_cut_marker( m->y_pos, m->color );
        else if ( G_cut.cut_dir == Y && m->y_pos == G_cut.index )
            set_cut_marker( m->x_pos, m->color );
    }

    cut_recalc_XPoints( );
}


/*-------------------------------------------------------*
 * The function calculates the points to be displayed
 * from the scaled points. As a side effect it also sets
 * the flags that indicate if out of range arrows have
 * be shown and counts the number of displayed points.
 *-------------------------------------------------------*/

static void
cut_recalc_XPoints( void )
{
    long k, j;
    Curve_1d_T *cv = &G_2d.cut_curve;
    Scaled_Point_T *sp = cv->points;
    XPoint *xp = cv->xpoints;


    cv->up = cv->down = cv->left = cv->right = false;

    for ( k = j = 0; j < G_cut.nx; sp++, j++ )
    {
        if ( ! sp->exist )
            continue;

        xp->x = s15rnd( cv->s2d[ X ] * ( j + cv->shift[ X ] ) );
        xp->y = i2s15( G_2d.cut_canvas.h ) - 1 -
               s15rnd( cv->s2d[ Y ] * ( cv->points[ j ].v + cv->shift[ Y ] ) );
        sp->xp_ref = k;

        cv->left  |= ( xp->x < 0 );
        cv->right |= ( xp->x >= ( int ) G_2d.cut_canvas.w );
        cv->up    |= ( xp->y < 0 );
        cv->down  |= ( xp->y >= ( int ) G_2d.cut_canvas.h );

        xp++;
        k++;
    }

    G_2d.cut_curve.count = k;
}


/*------------------------------------------------------*
 * Called whenever a different curve is to be displayed
 *------------------------------------------------------*/

void
cut_new_curve_handler( void )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    int i;


    if ( ! G_2d.is_cut )
        return;

    /* If curve shown until now (if there was one) wasn't displayed in full
       scale mode store its scaling factors */

    if ( G_cut.curve != -1 && ! G_cut.is_fs[ G_cut.curve ] )
    {
        for ( i = X; i <= Y; i++ )
        {
            G_cut.s2d[ G_cut.curve ][ i ] = cv->s2d[ i ];
            G_cut.shift[ G_cut.curve ][ i ] = cv->shift[ i ];
            G_cut.old_s2d[ G_cut.curve ][ i ] = cv->old_s2d[ i ];
            G_cut.old_shift[ G_cut.curve ][ i ] = cv->old_shift[ i ];
        }
    }

    /* Delete all markers that had been shown for the previous curve */

    delete_all_cut_markers( false );

    if ( G_2d.active_curve == -1 )
    {
        /* Remove the current curve */

        G_cut.curve = -1;
        cv->points = T_free( cv->points );
        cv->xpoints = T_free( cv->xpoints );
        cv->count = 0;
    }
    else
    {
        /* If new curve hasn't been shown yet use display mode of 2d curve
           for the curve */

        if ( ! G_cut.has_been_shown[ G_2d.active_curve ] )
            G_cut.is_fs[ G_2d.active_curve ] =
                                     G_2d.curve_2d[ G_2d.active_curve ]->is_fs;

        if ( G_cut.is_fs[ G_2d.active_curve ] )
        {
            fl_set_button( GUI.cut_form->cut_full_scale_button, 1 );
            if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                                      "Switch off automatic rescaling" );
        }
        else
        {
            /* Get stored scaling factors (if there are any) */

            if ( G_cut.has_been_shown[ G_2d.active_curve ] )
            {
                for ( i = X; i <= Y; i++ )
                {
                    cv->s2d[ i ] = G_cut.s2d[ G_2d.active_curve ][ i ];
                    cv->shift[ i ] = G_cut.shift[ G_2d.active_curve ][ i ];
                    cv->old_s2d[ i ] = G_cut.old_s2d[ G_2d.active_curve ][ i ];
                    cv->old_shift[ i ] =
                                     G_cut.old_shift[ G_2d.active_curve ][ i ];
                }
            }

            fl_set_button( GUI.cut_form->cut_full_scale_button, 0 );
            if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                                      "Rescale curve to fit into the window\n"
                                      "and switch on automatic rescaling" );
        }

        G_cut.curve = G_2d.active_curve;

        cv->up_arrow    = G_cut.up_arrows[ G_2d.active_curve ];
        cv->down_arrow  = G_cut.down_arrows[ G_2d.active_curve ];
        cv->left_arrow  = G_cut.left_arrows[ G_2d.active_curve ];
        cv->right_arrow = G_cut.right_arrows[ G_2d.active_curve ];

        XSetForeground( G.d, cv->gc,
                        fl_get_pixel( G.colors[ G_2d.active_curve ] ) );

        /* Get the data for the curve and set flag that says that everything
           necessary has been set */

        cut_calc_curve( -1, -1, G_cut.has_been_shown[ G_2d.active_curve ] );
        G_cut.has_been_shown[ G_2d.active_curve ] = true;
    }

    redraw_all_cut_canvases( );
}


/*---------------------------------------------------------------------------*
 * Function is called by accept_2d_data() whenever the z-scaling has changed
 * and thus also the y-scaling of the cut curve
 *---------------------------------------------------------------------------*/

bool
cut_data_rescaled( long   curve,
                   double y_min,
                   double y_max )
{
    long i;
    Curve_1d_T *cv  = &G_2d.cut_curve;
    Curve_2d_T *scv = G_2d.curve_2d[ G_2d.active_curve ];
    Scaled_Point_T *sp, *ssp;


    if ( ! G_2d.is_cut )
        return false;

    if ( ! G_cut.is_fs[ curve ] )
    {
        /* Calculate s2d[ Y ] and shift[ Y ] to fit the new scaling */

        G_cut.s2d[ curve ][ Y ] *=
                    ( y_max - y_min ) / G_2d.curve_2d[ curve ]->rwc_delta[ Z ];
        G_cut.shift[ curve ][ Y ] = ( G_2d.curve_2d[ curve ]->rwc_delta[ Z ]
                                      * G_cut.shift[ curve ][ Y ]
                                      - G_2d.curve_2d[ curve ]->rwc_start[ Z ]
                                      + y_min ) / ( y_max - y_min );

        if ( G_cut.can_undo[ curve ] )
        {
            G_cut.old_s2d[ curve ][ Y ] *=
                    ( y_max - y_min ) / G_2d.curve_2d[ curve ]->rwc_delta[ Z ];
            G_cut.old_shift[ curve ][ Y ] =
                           ( G_2d.curve_2d[ curve ]->rwc_delta[ Z ]
                             * G_cut.old_shift[ curve ][ Y ]
                             - G_2d.curve_2d[ curve ]->rwc_start[ Z ] + y_min )
                           / ( y_max - y_min );
        }

        if ( curve == G_2d.active_curve )
        {
            cv->s2d[ Y ] = G_cut.s2d[ curve ][ Y ];
            cv->shift[ Y ] = G_cut.shift[ curve ][ Y ];

            if ( G_cut.can_undo[ curve ] )
            {
                cv->old_s2d[ Y ] = G_cut.old_s2d[ curve ][ Y ];
                cv->old_shift[ Y ] = G_cut.old_shift[ curve ][ Y ];
            }
        }
    }

    if ( curve != G_2d.active_curve )
        return false;

    /* Extract the rescaled data of the cut from the 2d curve */

    if ( G_cut.cut_dir == X )
    {
        for ( i = 0, sp = cv->points, ssp = scv->points + G_cut.index;
              i < G_cut.nx; ssp += G_2d.nx, sp++, i++ )
            if ( ssp->exist )
            {
                sp->v = ssp->v;
                cv->xpoints[ sp->xp_ref ].y = i2s15( G_2d.cut_canvas.h ) - 1
                         - s15rnd( cv->s2d[ Y ] * ( sp->v + cv->shift[ Y ] ) );
            }
    }
    else
    {
        for ( i = 0, sp = cv->points,
              ssp = scv->points + G_cut.index * G_2d.nx;
              i < G_cut.nx; ssp++, sp++, i++ )
            if ( ssp->exist )
            {
                sp->v = ssp->v;
                cv->xpoints[ sp->xp_ref ].y = i2s15( G_2d.cut_canvas.h ) - 1
                         - s15rnd( cv->s2d[ Y ] * ( sp->v + cv->shift[ Y ] ) );
            }
    }

    /* Signal calling routine that redraw of the cut curve is needed */

    return true;
}


/*------------------------------------------------------------------------*
 * Function gets called by accept_2d_data() whenever the number of points
 * in x- or y-direction changes. The first argument is the direction the
 * data set changed (i.e. if there are new data in x- or in y-direction)
 * while the second is the new number of points in this direction. This
 * function does not get the new data, it just extends and initializes
 * the memory that will be used for them.
 *------------------------------------------------------------------------*/

bool
cut_num_points_changed( int  dir,
                        long num_points )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    Scaled_Point_T *sp;
    long k;


    /* Nothing to be done if the number of points didn't change in the cut
       direction or no curve is displayed */

    if ( dir == G_cut.cut_dir || ! G_2d.is_cut || G_cut.curve == -1 )
        return false;

    /* Extend the arrays for the (scaled) data and the array of XPoints */

    cv->points = T_realloc( cv->points, num_points * sizeof *cv->points );
    cv->xpoints = T_realloc( cv->xpoints, num_points * sizeof *cv->xpoints );

    /* The new entries are not set yet */

    for ( k = G_cut.nx, sp = cv->points + k; k < num_points; sp++, k++ )
        sp->exist = false;

    /* In full scale mode the x-axis scale must be reset and all points need to
       be rescaled */

    G_cut.nx = num_points;

    if ( G_cut.is_fs[ G_2d.active_curve ] )
    {
        G_cut.s2d[ G_2d.active_curve ][ X ] = cv->s2d[ X ] =
                              ( G_2d.cut_canvas.w - 1.0 ) / ( num_points - 1 );

        for ( sp = cv->points, k = 0; k < G_cut.nx; sp++, k++ )
            if ( sp->exist )
                cv->xpoints[ sp->xp_ref ].x = s15rnd( cv->s2d[ X ] * k );
    }

    /* Signal calling routine that redraw of cut curve is needed */

    return G_cut.is_fs[ G_2d.active_curve ];
}


/*--------------------------------------------------------------------------*
 * This function gets called with new data that might need to be displayed.
 * Before this function gets called the function cut_num_points_changed()
 * has already been executed if the new data don't overwrite existing data
 * but are for positions where no data have been shown yet.
 *--------------------------------------------------------------------------*/

bool
cut_new_points( long curve,
                long x_index,
                long y_index,
                long len )
{
    Scaled_Point_T *sp;
    long p_index;


    /* Nothing to be done if either the cross section isn't drawn or the new
       points don't belong to the curve currently shown */

    if ( ! G_2d.is_cut || G_cut.curve == -1 || curve != G_2d.active_curve )
        return false;

    /* We need a different handling for cuts in X and Y direction: if the cut
       is through the x-axis (vertical cut) we have to pick no more than one
       point from the new data while for cuts through the y-axis (horizontal
       cut) we either need all the data or none at all. */

    if ( G_cut.cut_dir == X )
    {
        if ( x_index > G_cut.index || x_index + len <= G_cut.index )
            return false;

        sp = G_2d.curve_2d[ curve ]->points + y_index * G_2d.nx + G_cut.index;
        cut_integrate_point( y_index, sp->v );
    }
    else
    {
        if ( y_index != G_cut.index )
            return false;

        /* All new points are on the cut */

        sp = G_2d.curve_2d[ curve ]->points + y_index * G_2d.nx + x_index;
        for ( p_index = x_index; p_index < x_index + len; sp++, p_index++ )
            cut_integrate_point( p_index, sp->v );
    }

    /* Signal calling routine that a redraw of the cut curve is needed */

    return true;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void
cut_integrate_point( long   p_index,
                     double val )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    Scaled_Point_T *cvp;
    long xp_index;
    long i, j;


    /* If this is a completely new point integrate it into the array of
       XPoints (which have to be sorted in ascending order of the
       x-coordinate), otherwise the XPoint just needs to be updated */

    if ( ! cv->points[ p_index ].exist )
    {
        /* Find next existing point to the left */

        for ( i = p_index - 1, cvp = cv->points + i; i >= 0 && ! cvp->exist;
              cvp--, i-- )
            /* empty */ ;

        if ( i == -1 )                    /* new points to be drawn is first */
        {
            xp_index = cv->points[ p_index ].xp_ref = 0;
            memmove( cv->xpoints + 1, cv->xpoints,
                     cv->count * sizeof *cv->xpoints );
            for ( cvp = cv->points + 1, j = 1; j < G_cut.nx; cvp++, j++ )
                if ( cvp->exist )
                    cvp->xp_ref++;
        }
        else if ( cv->points[ i ].xp_ref == cv->count - 1 )    /* ...is last */
        {
            xp_index = cv->points[ p_index ].xp_ref = cv->count;
        }
        else                                             /* ...is in between */
        {
            xp_index = cv->points[ p_index ].xp_ref =
                                                    cv->points[ i ].xp_ref + 1;
            memmove( cv->xpoints + xp_index + 1, cv->xpoints + xp_index,
                     ( cv->count - xp_index ) * sizeof *cv->xpoints );
            for ( j = p_index + 1, cvp = cv->points + j; j < G_cut.nx;
                  cvp++, j++ )
                if ( cvp->exist )
                    cvp->xp_ref++;
        }

        /* Calculate the x-coordinate of the new point and figure out if it
           exceeds the borders of the canvas */

        cv->xpoints[ xp_index ].x = s15rnd( cv->s2d[ X ]
                                            * ( p_index + cv->shift[ X ] ) );
        if ( cv->xpoints[ xp_index ].x < 0 )
            cv->left = true;
        if ( cv->xpoints[ xp_index ].x >= ( int ) G_2d.cut_canvas.w )
            cv->right = true;

        /* Increment the number of points belonging to the cut */

        cv->count++;
        cv->points[ p_index ].exist = true;
    }
    else
    {
        xp_index = cv->points[ p_index ].xp_ref;
    }

    /* Store the (new) points value */

    cv->points[ p_index ].v = val;

    /* Calculate the y-coordinate of the (new) point and figure out if it
       exceeds the borders of the canvas */

    cv->xpoints[ xp_index ].y = i2s15( G_2d.cut_canvas.h ) - 1
                              - s15rnd( cv->s2d[ Y ]
                              * ( cv->points[ p_index ].v + cv->shift[ Y ] ) );

    if ( cv->xpoints[ xp_index ].y < 0 )
        cv->up = true;
    if ( cv->xpoints[ xp_index ].y >= ( int ) G_2d.cut_canvas.h )
        cv->down = true;
}


/*------------------------------------------------------------*
 * Handler for all events the cut graphics window may receive
 *------------------------------------------------------------*/

static int
cut_canvas_handler( FL_OBJECT * obj,
                    Window      window,
                    int         w,
                    int         h,
                    XEvent    * ev,
                    void      * udata )
{
    Canvas_T *c = udata;


    switch ( ev->type )
    {
        case Expose :
            if ( ev->xexpose.count == 0 )     /* only react to last in queue */
                redraw_all_cut_canvases( );
            break;

        case ConfigureNotify :
            if ( ( int ) c->w != w || ( int ) c->h != h )
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


/*----------------------------------------------------------------*
 * Handler for ConfigureNotify events the cut graphic window gets
 *----------------------------------------------------------------*/

static void
cut_reconfigure_window( Canvas_T * c,
                        int        w,
                        int        h )
{
    long i;
    static bool is_reconf[ 3 ] = { false, false, false };
    static bool need_redraw[ 3 ] = { false, false, false };
    Curve_1d_T *cv = &G_2d.cut_curve;
    int old_w = c->w,
        old_h = c->h;


    /* Set the new canvas sizes */

    c->w = i2u15( w );
    c->h = i2u15( h );

    /* Calculate the new scale factors */

    if ( c == &G_2d.cut_canvas )
    {
        for ( i = 0; i < G_2d.nc; i++ )
        {
            G_cut.s2d[ i ][ X ] *= ( w - 1.0 ) / ( old_w - 1 );
            G_cut.s2d[ i ][ Y ] *= ( h - 1.0 ) / ( old_h - 1 );

            if ( G_cut.can_undo[ i ] )
            {
                G_cut.old_s2d[ i ][ X ] *= ( w - 1.0 ) / ( old_w - 1 );
                G_cut.old_s2d[ i ][ Y ] *= ( h - 1.0 ) / ( old_h - 1 );
            }
        }

        if ( G_2d.active_curve != -1 )
        {
            cv->s2d[ X ] *= ( w - 1.0 ) / ( old_w - 1 );
            cv->s2d[ Y ] *= ( h - 1.0 ) / ( old_h - 1 );

            if ( G_cut.can_undo[ G_2d.active_curve ] )
            {
                cv->old_s2d[ X ] *= ( w - 1.0 ) / ( old_w - 1 );
                cv->old_s2d[ Y ] *= ( h - 1.0 ) / ( old_h - 1 );
            }
        }

        /* Recalculate data for drawing (has to be done after setting of canvas
           sizes since they are needed in the recalculation) */

        cut_recalc_XPoints( );
    }

    /* Delete the old pixmap for the canvas and get a new one with the proper
       sizes. Also do some other small modifications necessary. */

    XftDrawChange( c->xftdraw, None );
    delete_pixmap( c );
    create_pixmap( c );
    XftDrawChange( c->xftdraw, c->pm );

    if ( c == &G_2d.cut_canvas )
        XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
    XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );

    /* We can't know the sequence the different canvases are reconfigured in
       but, on the other hand, redrawing an axis canvases is useless before
       the new scaling factors are set. Thus we need in the call for the
       canvas window to redraw also axis windows which got reconfigured
       before. */

    if ( c == &G_2d.cut_canvas )
    {
        redraw_cut_canvas( c );

        if ( need_redraw[ X ] )
        {
            redraw_cut_canvas( &G_2d.cut_x_axis );
            need_redraw[ X ] = false;
        }
        else if ( w != old_w )
        {
            is_reconf[ X ] = true;
        }

        if ( need_redraw[ Y ] )
        {
            redraw_cut_canvas( &G_2d.cut_y_axis );
            need_redraw[ Y ] = false;
        }
        else if ( h != old_h )
        {
            is_reconf[ Y ] = true;
        }

        if ( need_redraw[ Z ] )
        {
            redraw_cut_canvas( &G_2d.cut_z_axis );
            need_redraw[ Z ] = false;
        }
        else if ( h != old_h )
        {
            is_reconf[ Z ] = true;
        }
    }

    if ( c == &G_2d.cut_x_axis )
    {
        if ( is_reconf[ X ] )
        {
            redraw_cut_canvas( c );
            is_reconf[ X ] = false;
        }
        else
        {
            need_redraw[ X ] = true;
        }
    }

    if ( c == &G_2d.cut_y_axis )
    {
        if ( is_reconf[ Y ] )
        {
            redraw_cut_canvas( c );
            is_reconf[ Y ] = false;
        }
        else
        {
            need_redraw[ Y ] = true;
        }
    }

    if ( c == &G_2d.cut_z_axis )
    {
        if ( is_reconf[ Z ] )
        {
            redraw_cut_canvas( c );
            is_reconf[ Z ] = false;
        }
        else
        {
            need_redraw[ Z ] = true;
        }
    }
}


/*-----------------------------------------------------*
 * Handler for ButtonPress events the cut graphic gets
 *-----------------------------------------------------*/

static void
cut_press_handler( FL_OBJECT * obj,
                   Window      window,
                   XEvent    * ev,
                   Canvas_T  * c )
{
    int old_button_state = G.button_state;
    unsigned int keymask;


    /* In the axes areas pressing two buttons simultaneously doesn't have a
       special meaning, so don't care about another button. Also don't react
       if the pressed buttons have lost there meaning, there is no curve
       displayed or the curve has no scaling yet */

    if (    ( c != &G_2d.cut_canvas && G.raw_button_state != 0 )
         || ( G.button_state == 0 && G.raw_button_state != 0 )
         || G_2d.active_curve == -1
         || ! G_2d.curve_2d[ G_2d.active_curve ]->is_scale_set )
    {
        G.raw_button_state |= 1 << ( ev->xbutton.button - 1 );
        return;
    }

    G.raw_button_state |= ( 1 << ( ev->xbutton.button - 1 ) );

    /* Middle and right or all three buttons at once don't mean a thing */

    if (    G.raw_button_state >= 6
         && G.raw_button_state != 8
         && G.raw_button_state != 16 )
        return;

    G.button_state |= ( 1 << ( ev->xbutton.button - 1 ) );

    /* Find out which window gets the mouse events (all following mouse events
       go to this window until all buttons are released) */

    if ( obj == GUI.cut_form->cut_x_axis )        /* in x-axis window */
        G.drag_canvas = DRAG_CUT_X;
    if ( obj == GUI.cut_form->cut_y_axis )        /* in y-axis window */
        G.drag_canvas = DRAG_CUT_Y;
    if ( obj == GUI.cut_form->cut_z_axis )        /* in z-axis window */
        G.drag_canvas = DRAG_CUT_Z;
    if ( obj == GUI.cut_form->cut_canvas )        /* in canvas window */
        G.drag_canvas = DRAG_CUT_C;

    fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

    switch ( G.button_state )
    {
        case 1 :                               /* left button */
            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];

            /* Set up variables for drawing the rubber boxes */

            switch ( G.drag_canvas )
            {
                case DRAG_CUT_X :                       /* in x-axis window */
                    fl_set_cursor( window, cursor_c[ ZOOM_BOX_CURSOR ] );

                    c->box_x = c->ppos[ X ];
                    c->box_w = 0;
                    c->box_y = G.x_scale_offset + 1;
                    c->box_h = G.enlarge_box_width;
                    c->is_box = true;
                    break;

                case DRAG_CUT_Y :                       /* in y-axis window */
                    fl_set_cursor( window, cursor_c[ ZOOM_BOX_CURSOR ] );

                    c->box_x = c->w
                              - ( G.y_scale_offset + G.enlarge_box_width + 1 );
                    c->box_y = c->ppos[ Y ];
                    c->box_w = G.enlarge_box_width;
                    c->box_h = 0;
                    c->is_box = true;
                    break;

                case DRAG_CUT_Z :                       /* in z-axis window */
                    fl_set_cursor( window, cursor_c[ ARROW_LEFT_CURSOR ] );
                    G_2d.cut_select = CUT_SELECT_X;
                    c->box_x = c->box_h = 0;
                    c->box_y = c->ppos[ Y ];
                    c->box_w = ENLARGE_BOX_WIDTH;
                    c->is_box = true;
                    break;

                case DRAG_CUT_C :                       /* in canvas window */
                    fl_set_cursor( window, cursor_c[ ZOOM_BOX_CURSOR ] );

                    c->box_x = c->ppos[ X ];
                    c->box_y = c->ppos[ Y ];
                    c->box_w = c->box_h = 0;
                    c->is_box = true;
                    break;
            }

            repaint_cut_canvas( c );
            break;

        case 2 :                               /* middle button */
            if ( G.drag_canvas <= DRAG_CUT_Z )
                break;
            fl_set_cursor( window, cursor_c[ MOVE_HAND_CURSOR ] );

            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];

            cut_save_scale_state( );
            break;

        case 3:                                /* left and middle button */
            if ( G.drag_canvas <= DRAG_CUT_Z )
                break;
            fl_set_cursor( window, cursor_c[ CROSSHAIR_CURSOR ] );
            G.coord_display = 4;

            /* Don't draw the box anymore */

            G_2d.cut_canvas.is_box = false;
            repaint_cut_canvas( &G_2d.cut_canvas );
            break;

        case 4 :                               /* right button */
            if ( G.drag_canvas <= DRAG_CUT_Z )
                break;
            fl_set_cursor( window, cursor_c[ ZOOM_LENS_CURSOR ] );

            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];
            break;

        case 5 :                               /* left and right button */
            if ( G.drag_canvas <= DRAG_CUT_Z )
                break;
            fl_set_cursor( window, cursor_c[ TARGET_CURSOR ] );
            G.dist_display = 4;

            if ( G_2d.cut_canvas.is_box == false && old_button_state != 4 )
            {
                G.start[ X ] = c->ppos[ X ];
                G.start[ Y ] = c->ppos[ Y ];
            }
            else
            {
                G_2d.cut_canvas.is_box = false;
            }

            repaint_cut_canvas( &G_2d.cut_canvas );
            break;

        case 8 : case 16 :                     /* button 4/5/wheel */
            if ( G.drag_canvas < DRAG_CUT_X || G.drag_canvas > DRAG_CUT_Y )
                break;

            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];

            fl_set_cursor( window, cursor_c[ MOVE_HAND_CURSOR ] );
            cut_save_scale_state( );
            break;
    }
}


/*--------------------------------------------------------------*
 * Handler for ButtonRelease events the cut graphic window gets
 *--------------------------------------------------------------*/

static void
cut_release_handler( FL_OBJECT * obj  UNUSED_ARG,
                     Window      window,
                     XEvent    * ev,
                     Canvas_T  * c )
{
    unsigned int keymask;
    bool scale_changed = false;


    /* If the released button didn't has a meaning just clear it from the
       button state pattern and then forget about it */

    if (    ! ( ( 1 << ( ev->xbutton.button - 1 ) ) & G.button_state )
         || G_2d.active_curve == -1 )
    {
        G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );
        return;
    }

    /* Get mouse position and restrict it to the canvas */

    fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

    if ( c->ppos[ X ] < 0 )
        c->ppos[ X ] = 0;
    if ( c->ppos[ X ] >= ( int ) G_2d.cut_canvas.w )
        c->ppos[ X ] = G_2d.cut_canvas.w - 1;

    if ( c->ppos[ Y ] < 0 )
        c->ppos[ Y ] = 0;

    if ( c != &G_2d.cut_z_axis )
    {
        if ( c->ppos[ Y ] >= ( int ) G_2d.cut_canvas.h )
            c->ppos[ Y ] = G_2d.cut_canvas.h - 1;
    }
    else if ( c->ppos[ Y ] >= ( int ) c->h )         /* in z-axis window */
    {
        c->ppos[ Y ] = c->h - 1;
    }

    switch ( G.button_state )
    {
        case 1 :                               /* left mouse button */
            switch ( G.drag_canvas )
            {
                case DRAG_CUT_X :                       /* x-axis window */
                    scale_changed = cut_change_x_range( c );
                    break;

                case DRAG_CUT_Y :                       /* in y-axis window */
                    scale_changed = cut_change_y_range( c );
                    break;

                case DRAG_CUT_Z :                       /* in z-axis window */
                    cut_show( G_cut.cut_dir,
                              lrnd( ( c->h - 1.0 - c->ppos[ Y ] )
                                    * ( ( G_cut.cut_dir == X ?
                                          G_2d.nx : G_2d.ny ) - 1 )
                                    / c->h ) );
                    break;

                case DRAG_CUT_C :                       /* in canvas window */
                    scale_changed = cut_change_xy_range( c );
                    break;
            }

            c->is_box = false;
            break;

        case 2 :                               /* middle mouse button */
            if ( G.drag_canvas < DRAG_CUT_Z )
                break;
            if ( G.drag_canvas & DRAG_CUT_X & 3 )
                redraw_cut_canvas( &G_2d.cut_x_axis );
            if ( G.drag_canvas & DRAG_CUT_Y & 3 )
                redraw_cut_canvas( &G_2d.cut_y_axis );
            if ( G.drag_canvas == DRAG_CUT_Z )
                redraw_cut_canvas( &G_2d.cut_z_axis );
            break;

        case 4 :                               /* right mouse button */
            switch ( G.drag_canvas )
            {
                case DRAG_CUT_X :                       /* in x-axis window */
                    scale_changed = cut_zoom_x( c );
                    break;

                case DRAG_CUT_Y :                       /* in y-axis window */
                    scale_changed = cut_zoom_y( c );
                    break;

                case DRAG_CUT_C :                       /* in canvas window */
                    scale_changed = cut_zoom_xy( c );
                    break;
            }
            break;

        case 8 :                               /* button 4/wheel up */
            if ( G.drag_canvas == DRAG_CUT_X )
            {
                G_2d.cut_x_axis.ppos[ X ] =
                                         G.start[ X ] - G_2d.cut_x_axis.w / 10;

                /* Recalculate the offsets and shift curves in the
                   canvas */

                shift_XPoints_of_cut_curve( &G_2d.cut_x_axis );
                scale_changed = true;
            }

            if ( G.drag_canvas == DRAG_CUT_Y )
            {
                G_2d.cut_y_axis.ppos[ Y ] =
                                         G.start[ Y ] + G_2d.cut_y_axis.h / 10;

                /* Recalculate the offsets and shift curves in the
                   canvas */

                shift_XPoints_of_cut_curve( &G_2d.cut_y_axis );
                scale_changed = true;
            }

            if ( G.drag_canvas == DRAG_CUT_Z )
                cut_next_index( NULL, 0 );

            break;

        case 16 :                              /* button 5/wheel down */
            if ( G.drag_canvas == DRAG_CUT_X )
            {
                G_2d.cut_x_axis.ppos[ X ] =
                                         G.start[ X ] + G_2d.cut_x_axis.w / 10;

                /* Recalculate the offsets and shift curves in the
                   canvas */

                shift_XPoints_of_cut_curve( &G_2d.cut_x_axis );
                scale_changed = true;
            }

            if ( G.drag_canvas == DRAG_CUT_Y )
            {
                G_2d.cut_y_axis.ppos[ Y ] =
                                         G.start[ Y ] - G_2d.cut_y_axis.h / 10;

                /* Recalculate the offsets and shift curves in the
                   canvas */

                shift_XPoints_of_cut_curve( &G_2d.cut_y_axis );
                scale_changed = true;
            }

            if ( G.drag_canvas == DRAG_CUT_Z )
                cut_next_index( NULL, 1 );

            break;
    }

    G.button_state = 0;
    G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );
    G.drag_canvas = DRAG_NONE;
    G.coord_display &= ~ 4;
    G.dist_display  &= ~ 4;

    G_2d.cut_select = NO_CUT_SELECT;
    fl_reset_cursor( window );

    if ( scale_changed )
    {
        if ( G_cut.is_fs[ G_2d.active_curve ] )
        {
            G_cut.is_fs[ G_2d.active_curve ] = false;
            fl_set_button( GUI.cut_form->cut_full_scale_button, 0 );
            if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                                      "Rescale curve to fit into the window\n"
                                      "and switch on automatic rescaling" );
        }

        cut_recalc_XPoints( );
        redraw_all_cut_canvases( );
    }

    if ( ! scale_changed || c != &G_2d.cut_canvas )
        repaint_cut_canvas( c );
}


/*-------------------------------------------------------------*
 * Handler for MotionNotify events the cut graphic window gets
 *-------------------------------------------------------------*/

static void
cut_motion_handler( FL_OBJECT * obj  UNUSED_ARG,
                    Window      window,
                    XEvent    * ev,
                    Canvas_T  * c )
{
    XEvent new_ev;
    bool scale_changed = false;
    unsigned int keymask;
    long p_index;


    /* Do avoid having to treat lots and lots of motion events only react
       to the latest in a series of motion events for the current window */

    while ( fl_XEventsQueued( QueuedAfterReading ) )
    {
        fl_XPeekEvent( &new_ev );             /* look ahead for next event */

        /* Stop looking ahead if the next one isn't a motion event or is for
           a different window */

        if (    new_ev.type != MotionNotify
             || new_ev.xmotion.window != ev->xmotion.window )
            break;

        fl_XNextEvent( ev );                  /* get the next event */
    }

    fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

    switch ( G.button_state )
    {
        case 1 :                              /* left mouse button */
            if ( G.drag_canvas >= DRAG_CUT_Z )
            {
                if ( G.drag_canvas & DRAG_CUT_X & 3)     /* x-axis or canvas */
                {
                    c->box_w = c->ppos[ X ] - G.start[ X ];

                    if ( c->box_x + c->box_w >= ( int ) c->w )
                        c->box_w = c->w - c->box_x - 1;

                    if ( c->box_x + c->box_w < 0 )
                        c->box_w = - c->box_x;
                }

                if ( G.drag_canvas & DRAG_CUT_Y & 3 )    /* y-axis or canvas */
                {
                    c->box_h = c->ppos[ Y ] - G.start[ Y ];

                    if ( c->box_y + c->box_h >= ( int ) c->h )
                        c->box_h = c->h - c->box_y - 1;

                    if ( c->box_y + c->box_h < 0 )
                        c->box_h = - c->box_y;
                }

                if ( G.drag_canvas == DRAG_CUT_Z )       /* z-axis window */
                {
                    c->box_h = c->ppos[ Y ] - G.start[ Y ];

                    if ( c->box_y + c->box_h >= ( int ) c->h )
                        c->box_h = c->h - c->box_y - 1;

                    if ( c->box_y + c->box_h < 0 )
                        c->box_h = - c->box_y;
                    p_index = lrnd( ( c->h - 1.0 - c->ppos[ Y ] )
                                    * ( ( G_cut.cut_dir == X ?
                                          G_2d.nx : G_2d.ny ) - 1 )
                                    / c->h );
                    if ( p_index != G_cut.index )
                    {
                        cut_show( G_cut.cut_dir, p_index );
                        break;
                    }
                }
            }

            repaint_cut_canvas( c );
            break;

        case 2 :                               /* middle button */
            if ( G.drag_canvas <= DRAG_CUT_Z )
                break;

            shift_XPoints_of_cut_curve( c );
            scale_changed = true;

            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];

            /* Switch off full scale button if necessary */

            if ( G_cut.is_fs[ G_2d.active_curve ] && scale_changed )
            {
                G_cut.is_fs[ G_2d.active_curve ] = false;
                fl_set_button( GUI.cut_form->cut_full_scale_button, 0 );
                if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                    fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                                       "Rescale curve to fit into the window\n"
                                       "and switch on automatic rescaling" );
            }

            redraw_cut_canvas( &G_2d.cut_canvas );
            if ( G.drag_canvas & DRAG_CUT_X & 3 )
                redraw_cut_canvas( &G_2d.cut_x_axis );
            if ( G.drag_canvas & DRAG_CUT_Y & 3 )
                redraw_cut_canvas( &G_2d.cut_y_axis );
            break;

        case 3 : case 5 :               /* left and (middle or right) button */
            if ( G.drag_canvas <= DRAG_CUT_Z )
                break;
            repaint_cut_canvas( &G_2d.cut_canvas );
            break;
    }
}


/*-------------------------------------------------------*
 * Handler for the undo button in the cut graphic window
 *-------------------------------------------------------*/

void
cut_undo_button_callback( FL_OBJECT * a  UNUSED_ARG,
                          long        b  UNUSED_ARG )
{
    double temp_s2d,
           temp_shift;
    Curve_1d_T *cv = &G_2d.cut_curve;
    int j;


    if ( G_cut.curve == -1 || ! G_cut.can_undo[ G_2d.active_curve ] )
        return;

    for ( j = 0; j <= Y; j++ )
    {
        temp_s2d = cv->s2d[ j ];
        temp_shift = cv->shift[ j ];

        cv->s2d[ j ] = cv->old_s2d[ j ];
        cv->shift[ j ] = cv->old_shift[ j ];

        cv->old_s2d[ j ] = temp_s2d;
        cv->old_shift[ j ] = temp_shift;
    }

    cut_recalc_XPoints( );

    if ( G_cut.is_fs[ G_2d.active_curve ] )
    {
        G_cut.is_fs[ G_2d.active_curve ] = false;
        fl_set_button( GUI.cut_form->cut_full_scale_button, 0 );
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                                  "Rescale curve to fit into the window\n"
                                  "and switch on automatic rescaling" );
    }

    redraw_all_cut_canvases( );
}


/*-------------------------------------------------------------*
 * Handler for the full scale button in the cut graphic window
 *-------------------------------------------------------------*/

void
cut_fs_button_callback( FL_OBJECT * a  UNUSED_ARG,
                        long        b  UNUSED_ARG )
{
    int state;


    /* Rescaling is useless if no cut curve is shown */

    if ( ! G_2d.is_cut || G_cut.curve == -1 )
        return;

    /* Get new state of button */

    state = fl_get_button( GUI.cut_form->cut_full_scale_button );

    G_cut.is_fs[ G_cut.curve ] = state;

    if ( state )
    {
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                                  "Switch off automatic rescaling" );

        cut_calc_curve( -1, -1, true );
        redraw_all_cut_canvases( );
    }
    else if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
    {
        fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                              "Rescale curve to fit into the window\n"
                              "and switch on automatic rescaling" );
    }
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
redraw_all_cut_canvases( void )
{
    if ( ! G_2d.is_cut )
        return;

    redraw_cut_canvas( &G_2d.cut_canvas );
    redraw_cut_canvas( &G_2d.cut_x_axis );
    redraw_cut_canvas( &G_2d.cut_y_axis );
    redraw_cut_canvas( &G_2d.cut_z_axis );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static void
redraw_cut_canvas( Canvas_T * c )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    Marker_1d_T *m;
    short int x;


    /* Clear the canvas by drawing over its pixmap in the background color */

    XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

    /* For the main canvas draw the markers, then the curve and finally the
       out of range arrows */

    if ( c == &G_2d.cut_canvas && G_cut.curve != -1 && cv->count > 1 )
    {
        for ( m = G_2d.curve_2d[ G_2d.active_curve ]->cut_marker;
              m != NULL; m = m->next )
        {
            x = s15rnd( cv->s2d[ X ] * ( m->x_pos + cv->shift[ X ] ) );
            XDrawLine( G.d, c->pm, m->gc, x, 0, x, c->h );
        }

        XDrawLines( G.d, c->pm, cv->gc, cv->xpoints, cv->count,
                    CoordModeOrigin );

        if ( cv->up )
            XCopyArea( G.d, cv->up_arrow, c->pm, cv->gc, 0, 0,
                       G.up_arrow_w, G.up_arrow_h,
                       ( G_2d.cut_canvas.w - G.up_arrow_w ) / 2, 5 );

        if ( cv->down )
            XCopyArea( G.d, cv->down_arrow, c->pm, cv->gc, 0, 0,
                       G.down_arrow_w, G.down_arrow_h,
                       ( G_2d.cut_canvas.w - G.down_arrow_w ) / 2,
                       G_2d.cut_canvas.h - 5 - G.down_arrow_h );

        if ( cv->left )
            XCopyArea( G.d, cv->left_arrow, c->pm, cv->gc, 0, 0,
                       G.left_arrow_w, G.left_arrow_h, 5,
                       ( G_2d.cut_canvas.h - G.left_arrow_h ) / 2 );

        if ( cv->right )
            XCopyArea( G.d, cv->right_arrow, c->pm, cv->gc, 0, 0,
                       G.right_arrow_w, G.right_arrow_h,
                       G_2d.cut_canvas.w - 5 - G.right_arrow_w,
                       ( G_2d.cut_canvas.h - G.right_arrow_h ) / 2 );
    }

    if ( c == &G_2d.cut_x_axis )
        redraw_cut_axis( X );

    if ( c == &G_2d.cut_y_axis )
        redraw_cut_axis( Y );

    if ( c == &G_2d.cut_z_axis )
        redraw_cut_axis( Z );

    /* Finally copy the pixmap onto the screen */

    repaint_cut_canvas( c );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static void
repaint_cut_canvas( Canvas_T * c )
{
    Pixmap pm;
    Curve_1d_T *cv = &G_2d.cut_curve;
    Curve_2d_T *scv = G_2d.curve_2d[ G_2d.active_curve ];
    char buf[ 256 ];
    int x, y, x2, y2;
    unsigned int w, h;
    double x_pos, y_pos;
    int r_coord;


    /* If no or either the middle or the left button is pressed no extra stuff
       has to be drawn so just copy the pixmap with the curves into the
       window. */

    if ( ! ( G.button_state & 1 ) )
    {
        XCopyArea( G.d, c->pm, FL_ObjWin( c->obj ), c->gc,
                   0, 0, c->w, c->h, 0, 0 );
        return;
    }

    /* Otherwise use another level of buffering and copy the pixmap with
       the curves into another pixmap */

    pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h,
                        fl_get_canvas_depth( c->obj ) );
    XCopyArea( G.d, c->pm, pm, c->gc, 0, 0, c->w, c->h, 0, 0 );

    /* Draw the rubber box if needed (i.e. when the left button is pressed
       in the canvas currently to be drawn) */

    if ( G.button_state == 1 && c->is_box )
    {
        if ( G_2d.cut_select == NO_CUT_SELECT )
        {
            if ( c->box_w > 0 )
            {
                x = c->box_x;
                w = c->box_w;
            }
            else
            {
                x = c->box_x + c->box_w;
                w = - c->box_w;
            }

            if ( c->box_h > 0 )
            {
                y = c->box_y;
                h = c->box_h;
            }
            else
            {
                y = c->box_y + c->box_h;
                h = - c->box_h;
            }

            XDrawRectangle( G.d, pm, c->box_gc, x, y, w, h );
        }
        else
        {
            x = 0;
            x2 = G.z_scale_offset + G.enlarge_box_width + 1;
            y = y2 = c->box_y + c->box_h;
            XDrawLine( G.d, pm, c->box_gc, x, y, x2, y2 );
        }
    }

    /* If this is the canvas and the left and either the middle or the right
       mouse button is pressed draw the current mouse position (converted to
       real world coordinates) or the difference between the current position
       and the point the buttons were pressed at into the left hand top corner
       of the canvas. In the second case also draw some marker connecting the
       initial and the current position. */

    if ( c == &G_2d.cut_canvas )
    {
        if ( G.coord_display == 4 )
        {
            r_coord = G_cut.cut_dir == X ? Y : X;

            x_pos = scv->rwc_start[ r_coord ] + scv->rwc_delta[ r_coord ]
                    * ( c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ] );
            y_pos = scv->rwc_start[ Z ] + scv->rwc_delta[ Z ]
                    * ( ( G_2d.cut_canvas.h - 1.0 - c->ppos[ Y ] )
                        / cv->s2d[ Y ] - cv->shift[ Y ] );

            strcpy( buf, " x = " );
            make_label_string( buf + 5, x_pos,
                        irnd( floor( log10( fabs(
                        scv->rwc_delta[ r_coord ] ) / cv->s2d[ X ] ) ) - 2 ) );
            strcat( buf, "   y = " );
            make_label_string( buf + strlen( buf ), y_pos,
                              irnd( floor( log10( fabs(
                              scv->rwc_delta[ Z ] ) / cv->s2d[ Y ] ) ) - 2 ) );
            strcat( buf, " " );

            XFillRectangle( G.d, pm, c->gc, 5, 5, text_width( buf ) + 10,
                            G.font_asc + G.font_desc + 4 );
            XftDrawChange( c->xftdraw, pm );
            XftDrawStringUtf8( c->xftdraw, G.xftcolor + MAX_CURVES + 1, G.font,
                               10, G.font_asc + 7,
                               ( XftChar8 const * ) buf, strlen( buf ) );
            XftDrawChange( c->xftdraw, c->pm );
        }

        if ( G.dist_display == 4 )
        {
            r_coord = G_cut.cut_dir == X ? Y : X;

            x_pos = scv->rwc_delta[ r_coord ]
                    * ( c->ppos[ X ] - G.start[ X ] ) / cv->s2d[ X ];
            y_pos = scv->rwc_delta[ Z ]
                    * ( G.start[ Y ] - c->ppos[ Y ] ) / cv->s2d[ Y ];

            sprintf( buf, " dx = %#g   dy = %#g ", x_pos, y_pos );

            XFillRectangle( G.d, pm, c->gc, 5, 5, text_width( buf ) + 10,
                            G.font_asc + G.font_desc + 4 );
            XftDrawChange( c->xftdraw, pm );
            XftDrawStringUtf8( c->xftdraw, G.xftcolor + MAX_CURVES + 1, G.font,
                               10, G.font_asc + 7,
                               ( XftChar8 const * ) buf, strlen( buf ) );
            XftDrawChange( c->xftdraw, c->pm );

            XSetForeground( G.d, cv->gc, fl_get_pixel( FL_RED ) );
            XDrawArc( G.d, pm, cv->gc,
                      G.start[ X ] - 5, G.start[ Y ] - 5, 10, 10, 0, 23040 );
            XSetForeground( G.d, cv->gc,
                            fl_get_pixel( G.colors[ G_2d.active_curve ] ) );
            XDrawLine( G.d, pm, c->box_gc, G.start[ X ], G.start[ Y ],
                       c->ppos[ X ], G.start[ Y ] );
            XDrawLine( G.d, pm, c->box_gc, c->ppos[ X ], G.start[ Y ],
                       c->ppos[ X ], c->ppos[ Y ] );
        }
    }

    /* Finally copy the buffer pixmap onto the screen */

    XCopyArea( G.d, pm, FL_ObjWin( c->obj ), c->gc,
               0, 0, c->w, c->h, 0, 0 );
    XFreePixmap( G.d, pm );
    XFlush( G.d );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

int
get_mouse_pos_cut( double       * pa,
                   unsigned int * keymask )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    Curve_2d_T *scv;
    int r_coord;
    int ppos[ 2 ];


    fl_get_win_mouse( FL_ObjWin( G_2d.cut_canvas.obj ),
                      ppos + X, ppos + Y, keymask );

    if (    ! G_2d.is_cut
         || G_2d.active_curve == -1
         || ! G_2d.curve_2d[ G_2d.active_curve ]->is_scale_set
         || ppos[ X ] < 0
         || ppos[ X ] > ( int ) G_2d.cut_canvas.w - 1
         || ppos[ Y ] < 0
         || ppos[ Y ] > ( int ) G_2d.cut_canvas.h - 1 )
        return 0;

    scv = G_2d.curve_2d[ G_2d.active_curve ];

    r_coord = G_cut.cut_dir == X ? Y : X;

    pa[ X ] = scv->rwc_start[ r_coord ] + scv->rwc_delta[ r_coord ]
              * ( ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ] );
    pa[ Y ] = scv->rwc_start[ Z ] + scv->rwc_delta[ Z ]
              * ( ( G_2d.cut_canvas.h - 1.0 - ppos[ Y ] ) / cv->s2d[ Y ]
                  - cv->shift[ Y ] );

    if ( G_cut.cut_dir == X )
        pa[ Z ] = scv->rwc_delta[ X ] * G_cut.index + scv->rwc_start[ X ];
    else
        pa[ Z ] = scv->rwc_delta[ Y ] * G_cut.index + scv->rwc_start[ Y ];

    return 3;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
redraw_cut_axis( int coord )
{
    Canvas_T *c;
    int width;
    int r_coord;


    fsc2_assert( coord >= X && coord <= Z );

    /* First draw the label - for the x-axis it's just done by drawing the
       string while for the y- and z-axis we have to copy a pixmap since the
       label is a string rotated by 90 degree that has been drawn in advance */

    if ( coord == X )
    {
        c = &G_2d.cut_x_axis;
        XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

        if ( G_cut.cut_dir == X )
            r_coord = Y;
        else
            r_coord = X;

        if ( G_2d.label[ r_coord ] != NULL )
        {
            width = text_width( G_2d.label[ r_coord ] );
            XftDrawStringUtf8( c->xftdraw, G.xftcolor + MAX_CURVES, G.font,
                               c->w - width - 5, c->h - 2 - G.font_desc,
                               ( XftChar8 const * ) G_2d.label[ r_coord ],
                               strlen( G_2d.label[ r_coord ] ) );
        }
    }
    else if ( coord == Y )
    {
        c = &G_2d.cut_y_axis;
        XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

        if ( G_2d.label[ Z ] != NULL )
            XCopyArea( G.d, G_2d.label_pm[ Y + 3 ], c->pm, c->gc, 0, 0,
                       G_2d.label_w[ Y + 3 ], G_2d.label_h[ Y + 3 ], 0, 0 );
    }
    else
    {
        c = &G_2d.cut_z_axis;
        XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

        if ( G_2d.label[ coord ] != NULL )
            XCopyArea( G.d, G_2d.label_pm[ coord + 3 ], c->pm, c->gc, 0, 0,
                       G_2d.label_w[ coord + 3 ], G_2d.label_h[ coord + 3 ],
                       c->w - 5 - G_2d.label_w[ coord + 3 ], 0 );
    }

    if (    G_2d.active_curve != -1
         && G_2d.curve_2d[ G_2d.active_curve ]->is_scale_set )
        cut_make_scale( c, coord );

    XCopyArea( G.d, c->pm, FL_ObjWin( c->obj ), c->gc,
               0, 0, c->w, c->h, 0, 0 );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static void
cut_make_scale( Canvas_T * c,
                int        coord )
{
    Curve_1d_T *cv  = &G_2d.cut_curve;
    Curve_2d_T *cv2 = G_2d.curve_2d[ G_2d.active_curve ];
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
    XPoint triangle[ 4 ];


    if ( coord == X )
    {
        r_coord = G_cut.cut_dir == X ? Y : X;
        r_scale = cv->s2d[ X ];
    }
    else if ( coord == Y )
    {
        r_coord = Z;
        r_scale = cv->s2d[ Y ];
    }
    else
    {
        if ( G_cut.cut_dir == X )
        {
            r_coord = X;
            r_scale = ( c->h - 1.0 ) / ( G_2d.nx - 1 );
        }
        else
        {
            r_coord = Y;
            r_scale = ( c->h - 1.0 ) / ( G_2d.ny - 1 );
        }
    }

    /* The distance between the smallest ticks should be 'G.scale_tick_dist'
       points - calculate the corresponding delta in real word units */

    rwc_delta = G.scale_tick_dist * fabs( cv2->rwc_delta[ r_coord ] ) / r_scale;

    /* Now scale this distance to the interval [ 1, 10 [ */

    mag = floor( log10( rwc_delta ) );
    order = pow( 10.0, mag );
    modf( rwc_delta / order, &rwc_delta );

    /* Get a 'smooth' value for the ticks distance, i.e. either 2, 2.5, 5 or
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

    d_delta_fine = r_scale * rwc_delta / fabs( cv2->rwc_delta[ r_coord ] );

    /* 'rwc_start' is the first value in the display (i.e. the smallest x or y
       value still shown in the canvas), 'rwc_start_fine' the position of the
       first small tick (both in real world coordinates) and, finally,
       'd_start_fine' is the same position but in points */

    if ( coord != Z )
        rwc_start = cv2->rwc_start[ r_coord ]
                    - cv->shift[ coord ] * cv2->rwc_delta[ r_coord ];
    else                           /* z-axis always shows the complete range */
        rwc_start = cv2->rwc_start[ r_coord ];

    if ( cv2->rwc_delta[ r_coord ] < 0 )
        rwc_delta *= -1.0;

    modf( rwc_start / rwc_delta, &rwc_start_fine );
    rwc_start_fine *= rwc_delta;

    d_start_fine = r_scale * ( rwc_start_fine - rwc_start )
                   / cv2->rwc_delta[ r_coord ];
    if ( lrnd( d_start_fine ) < 0 )
        d_start_fine += d_delta_fine;

    /* Calculate start index (in small tick counts) of first medium tick */

    modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
    rwc_start_medium *= medium_factor * rwc_delta;

    d_start_medium = r_scale * ( rwc_start_medium - rwc_start )
                             / cv2->rwc_delta[ r_coord ];
    if ( lrnd( d_start_medium ) < 0 )
        d_start_medium += medium_factor * d_delta_fine;

    medium = irnd( ( d_start_fine - d_start_medium ) / d_delta_fine );

    /* Calculate start index (in small tick counts) of first large tick */

    modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
    rwc_start_coarse *= coarse_factor * rwc_delta;

    d_start_coarse = r_scale * ( rwc_start_coarse - rwc_start )
                             / cv2->rwc_delta[ r_coord ];
    if ( lrnd( d_start_coarse ) < 0 )
    {
        d_start_coarse += coarse_factor * d_delta_fine;
        rwc_start_coarse += coarse_factor * rwc_delta;
    }

    coarse = irnd( ( d_start_fine - d_start_coarse ) / d_delta_fine );

    /* Now, finally we got everything we need to draw the axis... */

    rwc_coarse = rwc_start_coarse - coarse_factor * rwc_delta;

    if ( coord == X )
    {
        /* Draw coloured line of scale */

        y = i2s15( G.x_scale_offset );
        XFillRectangle( G.d, c->pm, cv->gc, 0, y - 1, c->w, 2 );
        XDrawLine( G.d, c->pm, c->axis_gc, 0, y - 2, c->w, y - 2 );
        XDrawLine( G.d, c->pm, c->axis_gc, 0, y + 1, c->w, y + 1 );

        /* Draw all the ticks and numbers */

        for ( cur_p = d_start_fine; cur_p < c->w;
              medium++, coarse++, cur_p += d_delta_fine )
        {
            x = s15rnd( cur_p );

            if ( coarse % coarse_factor == 0 )         /* long line */
            {
                XDrawLine( G.d, c->pm, c->axis_gc, x, y + 3,
                           x, y - G.long_tick_len );
                rwc_coarse += coarse_factor * rwc_delta;

                make_label_string( lstr, rwc_coarse, ( int ) mag );
                width = text_width( lstr );
                if ( x - width / 2 - 10 > last )
                {
                    XftDrawStringUtf8( c->xftdraw, G.xftcolor + MAX_CURVES,
                                       G.font, x - width / 2,
                                       y + G.label_dist + G.font_asc,
                                       ( XftChar8 const * ) lstr,
                                       strlen( lstr ) );
                    last = i2s15( x + width / 2 );
                }
            }
            else if ( medium % medium_factor == 0 )    /* medium line */
            {
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x, y - G.medium_tick_len );
            }
            else                                       /* short line */
            {
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x, y - G.short_tick_len );
            }
        }
    }
    else if ( coord == Y )
    {
        /* Draw coloured line of scale */

        x = i2s15( c->w - G.y_scale_offset );
        XFillRectangle( G.d, c->pm, cv->gc, x, 0, 2, c->h );
        XDrawLine( G.d, c->pm, c->axis_gc, x - 1, 0, x - 1, c->h );
        XDrawLine( G.d, c->pm, c->axis_gc, x + 2, 0, x + 2, c->h );

        /* Draw all the ticks and numbers */

        for ( cur_p = c->h - 1.0 - d_start_fine; cur_p > - 0.5;
              medium++, coarse++, cur_p -= d_delta_fine )
        {
            y = s15rnd( cur_p );

            if ( coarse % coarse_factor == 0 )         /* long line */
            {
                XDrawLine( G.d, c->pm, c->axis_gc, x - 3, y,
                           x + G.long_tick_len, y );
                rwc_coarse += coarse_factor * rwc_delta;

                make_label_string( lstr, rwc_coarse, ( int ) mag );
                width = text_width( lstr );
                XftDrawStringUtf8( c->xftdraw, G.xftcolor + MAX_CURVES, G.font,
                                   x - G.label_dist - width,
                                   y + G.font_asc / 2,
                                   ( XftChar8 const * ) lstr, strlen( lstr ) );
            }
            else if ( medium % medium_factor == 0 )    /* medium line */
            {
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x + G.medium_tick_len, y );
            }
            else                                      /* short line */
            {
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x + G.short_tick_len, y );
            }
        }
    }
    else
    {
        /* Draw coloured line of scale */

        x = i2s15( G.z_scale_offset );
        XFillRectangle( G.d, c->pm, cv->gc, x - 1, 0, 2, c->h );
        XDrawLine( G.d, c->pm, c->axis_gc, x - 2, 0, x - 2, c->h );
        XDrawLine( G.d, c->pm, c->axis_gc, x + 1, 0, x + 1, c->h );

        /* Draw all the ticks and numbers */

        for ( cur_p = c->h - 1.0 - d_start_fine; cur_p > -0.5;
              medium++, coarse++, cur_p -= d_delta_fine )
        {
            y = s15rnd( cur_p );

            if ( coarse % coarse_factor == 0 )         /* long line */
            {
                XDrawLine( G.d, c->pm, c->axis_gc, x + 3, y,
                           x - G.long_tick_len, y );
                rwc_coarse += coarse_factor * rwc_delta;

                make_label_string( lstr, rwc_coarse, ( int ) mag );
                width = text_width( lstr );
                XftDrawStringUtf8( c->xftdraw, G.xftcolor + MAX_CURVES, G.font,
                                   x + G.label_dist, y + G.font_asc / 2,
                                   ( XftChar8 const * ) lstr, strlen( lstr ) );
            }
            else if ( medium % medium_factor == 0 )    /* medium line */
            {
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x - G.medium_tick_len, y );
            }
            else                                      /* short line */
            {
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x - G.short_tick_len, y );
            }
        }

        /* Finally draw the triangle indicating the position of the cut */

        triangle[ 0 ].x = i2s15( x - G.long_tick_len - 2 );

        if ( G_cut.cut_dir == X )
            triangle[ 0 ].y = s15rnd( ( G_2d.cut_z_axis.h - 1 )
                                      * ( 1.0 - G_cut.index /
                                          ( G_2d.nx - 1.0 ) ) );
        else
            triangle[ 0 ].y =
                         s15rnd( ( G_2d.cut_z_axis.h - 1 )
                                 * ( 1.0 - G_cut.index / ( G_2d.ny - 1.0 ) ) );

        triangle[ 1 ].x = - ( G.z_scale_offset - G.long_tick_len - 6 );
        triangle[ 1 ].y = i2s15( - G.long_tick_len / 3 ) - 2;
        triangle[ 2 ].x = 0;
        triangle[ 2 ].y = i2s15( 2 * ( G.long_tick_len / 3 ) ) + 4;

        XFillPolygon( G.d, c->pm, cv->gc, triangle, 3, Convex,
                      CoordModePrevious );

        triangle[ 3 ].x = -triangle[ 1 ].x;
        triangle[ 3 ].y = triangle[ 1 ].y;

        XDrawLines( G.d, c->pm, c->axis_gc, triangle, 4, CoordModePrevious );
    }
}


/*-------------------------------------------------------------------*
 * This is basically a simplified version of 'cut_recalc_XPoints()'
 * because we need to do much less calculations, i.e. just adding an
 * offset to all XPoints instead of going through all the scalings...
 *-------------------------------------------------------------------*/

static void
shift_XPoints_of_cut_curve( Canvas_T * c )
{
    long j, k;
    int dx = 0,
        dy = 0;
    int factor;
    Curve_1d_T *cv = &G_2d.cut_curve;


    cv->up = cv->down = cv->left = cv->right = false;

    /* Additionally pressing the right mouse button increases the amount the
       curves are shifted by a factor of 5 */

    factor = G.raw_button_state == 6 ? 5 : 1;

    /* Calculate scaled shift factors */

    if ( G.drag_canvas > DRAG_CUT_Z )
    {
        if ( G.drag_canvas & DRAG_CUT_X & 3 )     /* x-axis or canvas window */
        {
            dx = factor * ( c->ppos[ X ] - G.start[ X ] );
            cv->shift[ X ] += dx / cv->s2d[ X ];
        }

        if ( G.drag_canvas & DRAG_CUT_Y & 3 )     /* y-axis or canvas window */
        {
            dy = factor * ( c->ppos[ Y ] - G.start[ Y ] );
            cv->shift[ Y ] -= dy / cv->s2d[ Y ];
        }
    }

    /* Add the shifts to the XPoints */

    for ( k = 0, j = 0; j < G_cut.nx; j++ )
    {
        if ( ! cv->points[ j ].exist )
            continue;

        cv->xpoints[ k ].x = i2s15( cv->xpoints[ k ].x + dx );
        cv->xpoints[ k ].y = i2s15( cv->xpoints[ k ].y + dy );

        if ( cv->xpoints[ k ].x < 0 )
            cv->left = true;
        if ( cv->xpoints[ k ].x >= ( int ) G_2d.cut_canvas.w )
            cv->right = true;
        if ( cv->xpoints[ k ].y < 0 )
            cv->up = true;
        if ( cv->xpoints[ k ].y >= ( int ) G_2d.cut_canvas.h )
            cv->down = true;

        k++;
    }
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
cut_change_x_range( Canvas_T * c )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    double x1, x2;


    if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 )
        return false;

    cut_save_scale_state( );

    x1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
    x2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

    G_cut.shift[ G_cut.curve ][ X ] = cv->shift[ X ] = - d_min( x1, x2 );
    G_cut.s2d[ G_cut.curve ][ X ] = cv->s2d[ X ] =
                                   ( G_2d.cut_canvas.w - 1 ) / fabs( x1 - x2 );
    return true;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
cut_change_y_range( Canvas_T * c )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    double cy1, cy2;


    if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 )
        return false;

    cut_save_scale_state( );

    cy1 = ( G_2d.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
          - cv->shift[ Y ];
    cy2 = ( G_2d.cut_canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
          - cv->shift[ Y ];

    G_cut.shift[ G_2d.active_curve ][ Y ] =
                                          cv->shift[ Y ] = - d_min( cy1, cy2 );
    G_cut.s2d[ G_2d.active_curve ][ Y ] = cv->s2d[ Y ] =
                                 ( G_2d.cut_canvas.h - 1 ) / fabs( cy1 - cy2 );
    return true;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
cut_change_xy_range( Canvas_T * c )
{
    bool scale_changed = false;
    Curve_1d_T *cv = &G_2d.cut_curve;
    double cx1, cx2, cy1, cy2;


    cut_save_scale_state( );
    G_cut.can_undo[ G_2d.active_curve ] = false;

    if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
    {
        G_cut.can_undo[ G_2d.active_curve ] = true;

        cx1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
        cx2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

        G_cut.shift[ G_2d.active_curve ][ X ] =
                                          cv->shift[ X ] = - d_min( cx1, cx2 );
        G_cut.s2d[ G_2d.active_curve ][ X ] = cv->s2d[ X ] =
                                 ( G_2d.cut_canvas.w - 1 ) / fabs( cx1 - cx2 );

        scale_changed = true;
    }

    if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
    {
        G_cut.can_undo[ G_2d.active_curve ] = true;

        cy1 = ( G_2d.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
              - cv->shift[ Y ];
        cy2 = ( G_2d.cut_canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
              - cv->shift[ Y ];

        G_cut.shift[ G_2d.active_curve ][ Y ] =
                                          cv->shift[ Y ] = - d_min( cy1, cy2 );
        G_cut.s2d[ G_2d.active_curve ][ Y ] = cv->s2d[ Y ] =
                                 ( G_2d.cut_canvas.h - 1 ) / fabs( cy1 - cy2 );

        scale_changed = true;
    }

    return scale_changed;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
cut_zoom_x( Canvas_T * c )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    double px;


    if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 )
        return false;

    cut_save_scale_state( );

    px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

    /* If the mouse was moved to lower values zoom the display by a factor
       of up to 4 (if the mouse was moved over the whole length of the
       scale) while keeping the point the move was started at the same
       position. If the mouse was moved upwards demagnify by the inverse
       factor. */

    if ( G.start[ X ] > c->ppos[ X ] )
        cv->s2d[ X ] *= 3 * d_min( 1.0,
                                   ( double ) ( G.start[ X ] - c->ppos[ X ] )
                                   / G_2d.cut_x_axis.w ) + 1;
    else
        cv->s2d[ X ] /= 3 * d_min( 1.0,
                                   ( double ) ( c->ppos[ X ] - G.start[ X ] )
                                   / G_2d.cut_x_axis.w ) + 1;
    G_cut.s2d[ G_2d.active_curve ][ X ] = cv->s2d[ X ];

    G_cut.shift[ G_2d.active_curve ][ X ] = cv->shift[ X ] =
                                              G.start[ X ] / cv->s2d[ X ] - px;

    return true;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
cut_zoom_y( Canvas_T * c )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    double py;


    if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 )
        return false;

    cut_save_scale_state( );

    /* Get the value in the interval [0, 1] corresponding to the mouse
       position */

    py = ( G_2d.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
         - cv->shift[ Y ];

    /* If the mouse was moved to lower values zoom the display by a factor of
       up to 4 (if the mouse was moved over the whole length of the scale)
       while keeping the point the move was started at the same position. If
       the mouse was moved upwards demagnify by the inverse factor. */

    if ( G.start[ Y ] < c->ppos[ Y ] )
        cv->s2d[ Y ] *= 3 * d_min( 1.0,
                                   ( double ) ( c->ppos[ Y ] - G.start[ Y ] )
                                   / G_2d.cut_y_axis.h ) + 1;
    else
        cv->s2d[ Y ] /= 3 * d_min( 1.0,
                                   ( double ) ( G.start[ Y ] - c->ppos[ Y ] )
                                   / G_2d.cut_y_axis.h ) + 1;
    G_cut.s2d[ G_2d.active_curve ][ Y ] = cv->s2d[ Y ];

    G_cut.shift[ G_2d.active_curve ][ Y ] = cv->shift[ Y ] =
                ( G_2d.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ] - py;

    return true;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
cut_zoom_xy( Canvas_T * c )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    double px, py;
    bool scale_changed = false;


    cut_save_scale_state( );
    G_cut.can_undo[ G_2d.active_curve ] = false;

    if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
    {
        G_cut.can_undo[ G_2d.active_curve ] = true;

        px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

        if ( G.start[ X ] > c->ppos[ X ] )
            cv->s2d[ X ] *= 3 * d_min( 1.0,
                                     ( double ) ( G.start[ X ] - c->ppos[ X ] )
                                     / G_2d.cut_x_axis.w ) + 1;
        else
            cv->s2d[ X ] /= 3 * d_min( 1.0,
                                     ( double ) ( c->ppos[ X ] - G.start[ X ] )
                                     / G_2d.cut_x_axis.w ) + 1;
        G_cut.s2d[ G_2d.active_curve ][ X ] = cv->s2d[ X ];

        G_cut.shift[ G_2d.active_curve ][ X ] = cv->shift[ X ] =
                                              G.start[ X ] / cv->s2d[ X ] - px;

        scale_changed = true;
    }

    if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
    {
        G_cut.can_undo[ G_2d.active_curve ] = true;

        py = ( G_2d.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
             - cv->shift[ Y ];

        if ( G.start[ Y ] < c->ppos[ Y ] )
            cv->s2d[ Y ] *= 3 * d_min( 1.0,
                                     ( double ) ( c->ppos[ Y ] - G.start[ Y ] )
                                     / G_2d.cut_y_axis.h ) + 1;
        else
            cv->s2d[ Y ] /= 3 * d_min( 1.0,
                                     ( double ) ( G.start[ Y ] - c->ppos[ Y ] )
                                     / G_2d.cut_y_axis.h ) + 1;
        G_cut.s2d[ G_2d.active_curve ][ Y ] = cv->s2d[ Y ];

        G_cut.shift[ G_2d.active_curve ][ Y ] = cv->shift[ Y ] =
                ( G_2d.cut_canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ] - py;

        scale_changed = true;
    }

    return scale_changed;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static void
cut_save_scale_state( void )
{
    int i;
    Curve_1d_T *cv = &G_2d.cut_curve;


    for ( i = X; i <= Y; i++ )
    {
        cv->old_s2d[ i ] = cv->s2d[ i ];
        cv->old_shift[ i ] = cv->shift[ i ];
    }

    G_cut.can_undo[ G_2d.active_curve ] = true;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
cut_clear_curve( long curve )
{
    long i;
    Scaled_Point_T *sp;


    if ( ! G_2d.is_cut || curve != G_2d.active_curve )
        return;

    for ( sp = G_2d.cut_curve.points, i = 0; i < G_cut.nx; sp++, i++ )
        sp->exist = false;
    G_2d.cut_curve.count = 0;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
cut_next_index( FL_OBJECT * a  UNUSED_ARG,
                long        b )
{
    delete_all_cut_markers( false );
    switch( b )
    {
        case 0 :           /* go one curve up */
            cut_show( G_cut.cut_dir, G_cut.index + 1 );
            break;

        case 1 :           /* go one curve down */
            cut_show( G_cut.cut_dir, G_cut.index - 1 );
            break;

        case 2:            /* go to top curve */
            cut_show( G_cut.cut_dir,
                      G_cut.cut_dir == X ? G_2d.nx - 1 : G_2d.ny - 1 );
            break;

        case 3 :           /* go to bottom curve */
            cut_show( G_cut.cut_dir, 0 );
            break;
    }
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
cut_change_dir( FL_OBJECT * a  UNUSED_ARG,
                long        b  UNUSED_ARG )
{
    Curve_1d_T *cv = &G_2d.cut_curve;
    Curve_2d_T *scv = G_2d.curve_2d[ G_2d.active_curve ];
    long p_index;
    unsigned int keymask;
    int px, py;


    if ( G.raw_button_state != 3 )
        return;

    fl_get_win_mouse( GUI.cut_form->cut->window, &px, &py, &keymask );
    px -= GUI.cut_form->cut_canvas->x;

    if ( px < 0 || px > GUI.cut_form->cut_canvas->w )
        return;

    delete_all_cut_markers( false );

    p_index = lrnd( px / cv->s2d[ X ] - cv->shift[ X ] );

    if ( G_cut.cut_dir == X )
    {
        if ( p_index >= G_2d.ny )
            p_index = G_2d.ny - 1;
        if ( p_index < 0 )
            p_index = 0;
    }
    else
    {
        if ( p_index >= G_2d.nx )
            p_index = G_2d.nx - 1;
        if ( p_index < 0 )
            p_index = 0;
    }

    if ( ! G_cut.is_fs[ G_2d.active_curve ] )
    {
        if ( G_cut.cut_dir == Y )
        {
            G_cut.s2d[ G_2d.active_curve ][ X ] = cv->s2d[ X ] =
                                        scv->s2d[ Y ] / ( G_2d.canvas.h - 1.0 )
                                        * ( G_2d.cut_canvas.w - 1 );
            G_cut.shift[ G_2d.active_curve ][ X ] =
                                              cv->shift[ X ] = scv->shift[ Y ];
        }
        else
        {
            G_cut.s2d[ G_2d.active_curve ][ X ] = cv->s2d[ X ] =
                                        scv->s2d[ X ] / ( G_2d.canvas.w - 1.0 )
                                        * ( G_2d.cut_canvas.w - 1 );
            G_cut.shift[ G_2d.active_curve ][ X ] =
                                              cv->shift[ X ] = scv->shift[ X ];
        }
    }

    cut_show( G_cut.cut_dir == X ? Y : X, p_index );
}


/*-------------------------------------------*
 * Gets called to create a marker at 'x_pos'
 *-------------------------------------------*/

void
set_cut_marker( long x_pos,
                long color )
{
    Marker_1d_T *m, *cm;
    XGCValues gcv;
    Curve_2d_T *cv = G_2d.curve_2d[ G_2d.active_curve ];


    m = T_malloc( sizeof *m );
    m->next = NULL;

    if ( cv->cut_marker == NULL )
        cv->cut_marker = m;
    else
    {
        cm = cv->cut_marker;
        while ( cm->next != NULL )
            cm = cm->next;
        cm->next = m;
    }

    gcv.line_style = LineOnOffDash;

    m->color = color;
    m->gc = XCreateGC( G.d, G_2d.cut_canvas.pm, GCLineStyle, &gcv );

    if ( color == 0 )
        XSetForeground( G.d, m->gc, fl_get_pixel( FL_WHITE ) );
    else if ( color <= MAX_CURVES )
        XSetForeground( G.d, m->gc, fl_get_pixel( G.colors[ color - 1 ] ) );
    else
        XSetForeground( G.d, m->gc, fl_get_pixel( FL_BLACK ) );

    m->x_pos = x_pos;

    redraw_cut_canvas( &G_2d.cut_canvas );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
delete_cut_marker( long x_pos )
{
    Marker_1d_T *m, *mn = NULL;


    for ( m = G_2d.curve_2d[ G_cut.curve ]->cut_marker; m != NULL;
          m = m->next )
    {
        if ( m->x_pos == x_pos )
            break;
        mn = m;
    }

    if ( m == NULL )
        return;

    if ( mn == NULL )
        G_2d.curve_2d[ G_cut.curve ]->cut_marker = m->next;
    else
        mn->next = m->next;

    XFreeGC( G.d, m->gc );
    T_free( m );

    redraw_cut_canvas( &G_2d.cut_canvas );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void
delete_all_cut_markers( bool redraw_flag )
{
    Marker_1d_T *m, *mn;


    if (    G_cut.curve == -1
         || G_2d.curve_2d[ G_cut.curve ]->cut_marker == NULL )
        return;

    for ( m = G_2d.curve_2d[ G_cut.curve ]->cut_marker; m != NULL; m = mn )
    {
        XFreeGC( G.d, m->gc );
        mn = m->next;
        T_free( m );
    }

    G_2d.curve_2d[ G_cut.curve ]->cut_marker = NULL;

    if ( redraw_flag )
        redraw_cut_canvas( &G_2d.cut_canvas );
}


/*---------------------------------------------------------*
 * Callback for the invisible curve buttons (that can only
 * be used via the keyboard shortcuts. This function only
 * exists because of a bug in fdesign, that creates to
 * declarations of curve_button_callback_2d() if it's
 * set also as the callback for these buttons and not only
 * for the 2D graphics form.
 *---------------------------------------------------------*/

void
cut_curve_button_callback_2d( FL_OBJECT * obj,
                              long        data )
{
    return curve_button_callback_2d( obj, data );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
