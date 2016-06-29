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

#include "cursor1.xbm"             /* bitmaps for different cursors */
#include "cursor2.xbm"
#include "cursor3.xbm"
#include "cursor4.xbm"
#include "cursor5.xbm"
#include "cursor6.xbm"
#include "cursor7.xbm"

#include "up_arrow.xbm"             /* arrow bitmaps */
#include "down_arrow.xbm"
#include "left_arrow.xbm"
#include "right_arrow.xbm"


static void fonts_init( void );
static void set_default_sizes( void );
static void set_defaults( void );
static void forms_adapt( void );
static int run_form_close_handler( FL_FORM * a,
                                   void *    b );
static void G_struct_init( void );
static void G_init_curves_1d( void );
static void G_init_curves_2d( void );
static void setup_canvas( Canvas_T  * c,
                          FL_OBJECT * obj );
static void canvas_off( Canvas_T  * c,
                        FL_OBJECT * obj );
static void graphics_free( void );


Graphics_T G;

extern FL_resource Xresources[ ];

static Graphics_T *G_stored = NULL;
static Graphics_1d_T *G_1d_stored = NULL;
static Graphics_2d_T *G_2d_stored = NULL;

static bool display_has_been_shown = false;

static struct {
    unsigned int   win_min_1d_width;
    unsigned int   win_min_2d_width;
    unsigned int   win_min_height;
    unsigned int   curve_button_height;

    int            small_font_size;

    const char   * default_axisfont;

    int            display_x,
                   display_y;
    unsigned int   display_w,
                   display_h;
} GI;


/*----------------------------------------------------------------------*
 * Initializes and shows the window for displaying measurement results.
 *----------------------------------------------------------------------*/

void
start_graphics( void )
{
    if ( ! display_has_been_shown )
        set_defaults( );

    G.focus = 0;

    G.font = NULL;

    G.coord_display = 0;
    G.dist_display  = 0;

    G_cut.is_shown = false;
    G_cut.curve = -1;
    G_cut.index = 0;

    if ( G.dim & 1 )
    {
        G_1d.nx = G_1d.nx_orig;
        G_1d.rwc_start[ X ] = G_1d.rwc_start_orig[ X ];
        G_1d.rwc_delta[ X ] = G_1d.rwc_delta_orig[ X ];

        for ( int i = X; i <= Y; i++ )
            G_1d.label[ i ] = T_strdup( G_1d.label_orig[ i ] );

        G_1d.marker_1d = NULL;
    }

    if ( G.dim & 2 )
    {
        for ( long i = 0; i < G_2d.nc; i++ )
            G_2d.curve_2d[ i ] = NULL;

        G_2d.nx = G_2d.nx_orig;
        G_2d.ny = G_2d.ny_orig;

        for ( int i = X; i <= Y; i++ )
        {
            G_2d.rwc_start[ i ] = G_2d.rwc_start_orig[ i ];
            G_2d.rwc_delta[ i ] = G_2d.rwc_delta_orig[ i ];
        }

        for ( int i = X; i <= Z; i++ )
            G_2d.label[ i ] = T_strdup( G_2d.label_orig[ i ] );

        cut_init( );
    }

    /* Create the forms for running experiments */

    if ( G.dim & 1 || ! G.is_init )
        GUI.run_form_1d = GUI.G_Funcs.create_form_run_1d( );
    if ( G.dim & 2 )
    {
        GUI.run_form_2d = GUI.G_Funcs.create_form_run_2d( );
        GUI.cut_form = GUI.G_Funcs.create_form_cut( );
    }

    /* Do some changes to the forms */

    forms_adapt( );

    /* Store the current state of the Graphics structure - to be restored
       after the experiment */

    G_stored = get_memcpy( &G, sizeof G );
    G_1d_stored = get_memcpy( &G_1d, sizeof G_1d );
    G_2d_stored = get_memcpy( &G_2d, sizeof G_2d );

    G_2d.active_curve = 0;

    set_default_sizes( );

    /* Finally draw the form */

    if ( ! G.is_init || G.dim & 1 )
    {
        Window w = fl_show_form( GUI.run_form_1d->run_1d,
                                 GUI.display_1d_has_pos
                                 ? FL_PLACE_POSITION
                                 : FL_PLACE_MOUSE | FL_FREE_SIZE,
                                 FL_FULLBORDER,
                                 G.mode == NORMAL_DISPLAY ?
                                 "fsc2: 1D-Display" :
                                 "fsc2: 1D-Display (sliding window)" );
        G.d = FL_FormDisplay( GUI.run_form_1d->run_1d );
        fl_addto_selected_xevent( w, FocusIn | FocusOut );
        fl_register_raw_callback( GUI.run_form_1d->run_1d, FL_ALL_EVENT,
                                  form_event_handler );
    }

    if ( G.dim & 2 )
    {
        Window w = fl_show_form( GUI.run_form_2d->run_2d,
                                 GUI.display_2d_has_pos
                                 ? FL_PLACE_POSITION
                                 : FL_PLACE_MOUSE | FL_FREE_SIZE,
                                 FL_FULLBORDER, "fsc2: 2D-Display" );
        G.d = FL_FormDisplay( GUI.run_form_1d->run_1d );
        fl_addto_selected_xevent( w, FocusIn | FocusOut );
        fl_register_raw_callback( GUI.run_form_2d->run_2d, FL_ALL_EVENT,
                                  form_event_handler );
    }

    display_has_been_shown = true;

    /* Set minimum sizes for display windows and switch on full scale button */

    if ( G.dim & 1 || ! G.is_init )
    {
        unsigned int diff;

        if ( ! G.is_init || G_1d.nc == 1 )
            diff = MAX_CURVES * GI.curve_button_height;
        else
            diff = ( MAX_CURVES - G_1d.nc ) * GI.curve_button_height;
        fl_winminsize( GUI.run_form_1d->run_1d->window,
                       GI.win_min_1d_width, GI.win_min_height - diff );
    }

    if ( G.dim & 2 )
    {
        unsigned int diff;

        if ( G_2d.nc == 1 )
            diff = MAX_CURVES * GI.curve_button_height;
        else
            diff = ( MAX_CURVES - G_2d.nc ) * GI.curve_button_height;
        fl_winminsize( GUI.run_form_2d->run_2d->window,
                       GI.win_min_2d_width, GI.win_min_height - diff );
    }

    if ( G.dim & 1 )
        fl_set_button( GUI.run_form_1d->full_scale_button_1d, 1 );

    if ( G.dim & 2 )
        fl_set_button( GUI.run_form_2d->full_scale_button_2d, 1 );

    /* Define colors for the curves (this could be made user-configurable)
       - it must happen before font initialization */

    G.colors[ 0 ] = FL_TOMATO;
    G.colors[ 1 ] = FL_GREEN;
    G.colors[ 2 ] = FL_YELLOW;
    G.colors[ 3 ] = FL_CYAN;

    /* Load fonts */

    fonts_init( );

    /* Create the canvases for the axes and for drawing the data */

    if ( ! G.is_init || G.dim & 1 )
    {
        setup_canvas( &G_1d.x_axis, GUI.run_form_1d->x_axis_1d );
        setup_canvas( &G_1d.y_axis, GUI.run_form_1d->y_axis_1d );
        setup_canvas( &G_1d.canvas, GUI.run_form_1d->canvas_1d );
    }

    if ( G.dim & 2 )
    {
        setup_canvas( &G_2d.x_axis, GUI.run_form_2d->x_axis_2d );
        setup_canvas( &G_2d.y_axis, GUI.run_form_2d->y_axis_2d );
        setup_canvas( &G_2d.z_axis, GUI.run_form_2d->z_axis_2d );
        setup_canvas( &G_2d.canvas, GUI.run_form_2d->canvas_2d );
    }

    if ( ! G.is_init || G.dim & 1 )
        fl_set_form_atclose( GUI.run_form_1d->run_1d,
                             run_form_close_handler, NULL );

    if ( G.is_init )
    {
        if ( G.dim & 2 )
            fl_set_form_atclose( GUI.run_form_2d->run_2d,
                                 run_form_close_handler, NULL );
        G_struct_init( );
    }

    /* Make sure the window gets drawn correctly immediately */

    if ( ! G.is_init || G.dim & 1 )
        redraw_all_1d( );

    if ( G.dim & 2 )
        redraw_all_2d( );

    if ( ! G.is_init || G.dim & 1 )
        fl_raise_form( GUI.run_form_1d->run_1d );
    else
        fl_raise_form( GUI.run_form_2d->run_2d );

    G.is_fully_drawn = true;
}


/*-----------------------------------------------------------------------*
 * Loads user defined font for the axis and label. If this font can't be
 * loaded a default font is used instead.
 *-----------------------------------------------------------------------*/

static void
fonts_init( void )
{
    if ( ! G.is_init )
        return;

    /* Load either a user requested font (first try as an "old" Xlfd font)
       or, if none is specified, use the defautl font */

    if ( * ( ( char * ) Xresources[ AXISFONT ].var ) != '\0' )
    {
        G.font =  XftFontOpenXlfd( G.d, DefaultScreen( G.d ),
                                   ( char * ) Xresources[ AXISFONT ].var );
        if ( ! G.font )
            G.font = XftFontOpenName( G.d, DefaultScreen( G.d ),
                                      ( char * ) Xresources[ AXISFONT ].var );
    }
    else
        G.font = XftFontOpenName( G.d, DefaultScreen( G.d ),
                                  GI.default_axisfont );

    G.font_asc  = G.font->ascent;
    G.font_desc = G.font->descent;

    XRenderColor xrcolor;
    xrcolor.alpha = 0xffff;

    for ( long i = 0; i < MAX_CURVES; i++ )
    {
        int red,
            green,
            blue;

        fl_getmcolor( G.colors[ i ], &red, &green, &blue );
        xrcolor.red   = ( ( unsigned short ) red )   << 8;
        xrcolor.green = ( ( unsigned short ) green ) << 8;
        xrcolor.blue  = ( ( unsigned short ) blue )  << 8;

        XftColorAllocValue( G.d, fl_get_visual(), fl_get_colormap( ),
                            &xrcolor, G.xftcolor + i );
    }

    xrcolor.red = xrcolor.green = xrcolor.blue = 0;
    XftColorAllocValue( G.d, fl_get_visual(), fl_get_colormap( ),
                        &xrcolor, G.xftcolor + MAX_CURVES );

    xrcolor.red = xrcolor.green = xrcolor.blue = 0xffff;
    XftColorAllocValue( G.d, fl_get_visual(), fl_get_colormap( ),
                        &xrcolor, G.xftcolor + MAX_CURVES + 1 );
}


/*--------------------------------------------------------------*
 * Function tries to figure out reasonable values for the sizes
 * and positions of the display windows.
 *--------------------------------------------------------------*/

static void
set_default_sizes( void )
{
    /* If the display windows have never been shown before evaluate the
       positions and sizes specified on the command line */

    bool has_pos = false;
    bool has_size = false;

    if ( ! display_has_been_shown )
    {
        if ( * ( ( char * ) Xresources[ DISPLAYGEOMETRY ].var ) != '\0' )
        {
            int flags = XParseGeometry( Xresources[ DISPLAYGEOMETRY ].var,
                                        &GI.display_x, &GI.display_y,
                                        &GI.display_w, &GI.display_h );

            if ( WidthValue & flags && HeightValue & flags )
                has_size = true;

            if ( XValue & flags && YValue & flags )
                has_pos = true;
        }

        if ( * ( ( char * ) Xresources[ DISPLAY1DGEOMETRY ].var ) != '\0' )
        {
            int x, y;
            unsigned int w, h;
            int flags = XParseGeometry( Xresources[ DISPLAY1DGEOMETRY ].var,
                                        &x, &y, &w, &h );

            if ( WidthValue & flags && HeightValue & flags )
            {
                GUI.display_1d_width    = w;
                GUI.display_1d_height   = h;
                GUI.display_1d_has_size = true;
            }

            if ( XValue & flags && YValue & flags )
            {
                GUI.display_1d_x       = x;
                GUI.display_1d_y       = y;
                GUI.display_1d_has_pos = true;
            }
        }

        if ( * ( ( char * ) Xresources[ DISPLAY2DGEOMETRY ].var ) != '\0' )
        {
            int x, y;
            unsigned int w, h;
            int flags = XParseGeometry( Xresources[ DISPLAY2DGEOMETRY ].var,
                                        &x, &y, &w, &h );

            if ( WidthValue & flags && HeightValue & flags )
            {
                GUI.display_2d_width    = w;
                GUI.display_2d_height   = h;
                GUI.display_2d_has_size = true;
            }

            if ( XValue & flags && YValue & flags )
            {
                GUI.display_2d_x       = x;
                GUI.display_2d_y       = y;
                GUI.display_2d_has_pos = true;
            }
        }
    }

    if ( has_pos )
    {
        if ( ( G.dim & 1 || ! G.is_init ) && ! GUI.display_1d_has_pos )
        {
            GUI.display_1d_x       = GI.display_x;
            GUI.display_1d_y       = GI.display_y;
            GUI.display_1d_has_pos = true;
        }

        if ( G.dim & 2 && ! GUI.display_2d_has_pos )
        {
            GUI.display_2d_x       = GI.display_x;
            GUI.display_2d_y       = GI.display_y;
            GUI.display_2d_has_pos = true;
        }
    }

    if ( has_size || GUI.display_1d_has_size )
    {
        unsigned int diff;

        if ( ! G.is_init || G_1d.nc == 1 )
            diff = MAX_CURVES * GI.curve_button_height;
        else
            diff = ( MAX_CURVES - G_1d.nc ) * GI.curve_button_height;

        if ( has_size )
            GUI.display_1d_width = GI.display_w;

        if ( GUI.display_1d_width < GI.win_min_1d_width )
            GUI.display_1d_width = GI.win_min_1d_width;

        if ( GUI.display_1d_height < GI.win_min_height - diff )
            GUI.display_1d_height = GI.win_min_height - diff;
    }

    if ( has_size || GUI.display_2d_has_size )
    {
        unsigned int diff;

        if ( ! G.is_init || G_2d.nc == 1 )
            diff = MAX_CURVES * GI.curve_button_height;
        else
            diff = ( MAX_CURVES - G_2d.nc ) * GI.curve_button_height;

        if ( has_size )
            GUI.display_2d_width = GI.display_w;

        if ( GUI.display_2d_width < GI.win_min_2d_width )
            GUI.display_2d_width = GI.win_min_2d_width;

        if ( GUI.display_2d_height < GI.win_min_height - diff )
            GUI.display_2d_height = GI.win_min_height - diff;
    }

    if ( G.dim & 1 || ! G.is_init )
    {
        if ( GUI.display_1d_has_size )
            fl_set_form_size( GUI.run_form_1d->run_1d,
                              ( FL_Coord ) GUI.display_1d_width,
                              ( FL_Coord ) GUI.display_1d_height );
        if ( GUI.display_1d_has_pos )
            fl_set_form_position( GUI.run_form_1d->run_1d,
                                  ( FL_Coord ) GUI.display_1d_x,
                                  ( FL_Coord ) GUI.display_1d_y );
    }

    if ( G.dim & 2 )
    {
        if ( GUI.display_2d_has_size )
            fl_set_form_size( GUI.run_form_2d->run_2d,
                              ( FL_Coord ) GUI.display_2d_width,
                              ( FL_Coord ) GUI.display_2d_height );
        if ( GUI.display_2d_has_pos )
            fl_set_form_position( GUI.run_form_2d->run_2d,
                                  ( FL_Coord ) GUI.display_2d_x,
                                  ( FL_Coord ) GUI.display_2d_y );
    }
}


/*-------------------------------------------------*
 * Function for setting lots of default values for
 * properties of the display windows.
 *-------------------------------------------------*/

static void
set_defaults( void )
{
    if ( GUI.G_Funcs.size == ( bool ) LOW )
    {
        GI.win_min_1d_width    = 300;
        GI.win_min_2d_width    = 350;
        GI.win_min_height      = 360;
        GI.curve_button_height = 35;
        GI.small_font_size     = FL_TINY_SIZE;
        GI.default_axisfont    = "DejaVu Sans Condensed-8";

        G.scale_tick_dist      =  4;
        G.short_tick_len       =  3;
        G.medium_tick_len      =  6;
        G.long_tick_len        =  8;
        G.label_dist           =  5;
        G.x_scale_offset       = 12;
        G.y_scale_offset       = 12;
        G.z_scale_offset       = 25;
        G.z_line_offset        =  5;
        G.z_line_width         =  8;
        G.enlarge_box_width    =  3;
    }
    else
    {
        GI.win_min_1d_width    = 400;
        GI.win_min_2d_width    = 500;
        GI.win_min_height      = 460;
        GI.curve_button_height = 40;
        GI.small_font_size     = FL_SMALL_SIZE;
        GI.default_axisfont    = "DejaVu Sans Condensed-10";

        G.scale_tick_dist      =   6;
        G.short_tick_len       =   5;
        G.medium_tick_len      =  10;
        G.long_tick_len        =  14;
        G.label_dist           =   7;
        G.x_scale_offset       =  20;
        G.y_scale_offset       =  21;
        G.z_scale_offset       =  46;
        G.z_line_offset        =  10;
        G.z_line_width         =  14;
        G.enlarge_box_width    =   5;
    }
}


/*---------------------------------------------------------*
 * Function for adapting the forms for the display windows
 * to the current requirements.
 *---------------------------------------------------------*/

static void
forms_adapt( void )
{
    if ( ! G.is_init )
    {
        fl_hide_object( GUI.run_form_1d->curve_1_button_1d );
        fl_hide_object( GUI.run_form_1d->curve_2_button_1d );
        fl_hide_object( GUI.run_form_1d->curve_3_button_1d );
        fl_hide_object( GUI.run_form_1d->curve_4_button_1d );

        fl_hide_object( GUI.run_form_1d->undo_button_1d );
        fl_hide_object( GUI.run_form_1d->print_button_1d );
        fl_hide_object( GUI.run_form_1d->full_scale_button_1d );
    }
    else
    {
        char * pixmap_file;

        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            pixmap_file = T_strdup( lauxdir "undo.xpm" );
        else
            pixmap_file = T_strdup( auxdir "undo.xpm" );

        if ( access( pixmap_file, R_OK ) == 0 )
        {
            if ( G.dim & 1 )
                fl_set_pixmapbutton_file( GUI.run_form_1d->undo_button_1d,
                                          pixmap_file );
            if ( G.dim & 2 )
                fl_set_pixmapbutton_file( GUI.run_form_2d->undo_button_2d,
                                          pixmap_file );

            if ( G.dim & 1 )
                fl_set_object_lsize( GUI.run_form_1d->undo_button_1d,
                                     GI.small_font_size );
            if ( G.dim & 2 )
                fl_set_object_lsize( GUI.run_form_2d->undo_button_2d,
                                     GI.small_font_size );

            if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            {
                if ( G.dim & 1 )
                    fl_set_object_helper( GUI.run_form_1d->undo_button_1d,
                                          "Undo last rescaling operation" );
                if ( G.dim & 2 )
                    fl_set_object_helper( GUI.run_form_2d->undo_button_2d,
                                          "Undo last rescaling operation" );
            }

            if ( G.dim & 2 )
            {
                fl_set_pixmapbutton_file( GUI.cut_form->cut_undo_button,
                                          pixmap_file );
                fl_set_object_lsize( GUI.cut_form->cut_undo_button,
                                     GI.small_font_size );
                if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                    fl_set_object_helper( GUI.cut_form->cut_undo_button,
                                          "Undo last rescaling operation" );
            }
        }

        T_free( pixmap_file );

        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
            pixmap_file = T_strdup( lauxdir "printer.xpm" );
        else
            pixmap_file = T_strdup( auxdir "printer.xpm" );

        if ( access( pixmap_file, R_OK ) == 0 )
        {
            if ( G.dim & 1 )
                fl_set_pixmapbutton_file( GUI.run_form_1d->print_button_1d,
                                          pixmap_file );
            if ( G.dim & 2 )
                fl_set_pixmapbutton_file( GUI.run_form_2d->print_button_2d,
                                          pixmap_file );

            if ( G.dim & 1 )
                fl_set_object_lsize( GUI.run_form_1d->print_button_1d,
                                     GI.small_font_size );
            if ( G.dim & 2 )
                fl_set_object_lsize( GUI.run_form_2d->print_button_2d,
                                     GI.small_font_size );

            if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            {
                if ( G.dim & 1 )
                    fl_set_object_helper( GUI.run_form_1d->print_button_1d,
                                          "Print window" );
                if ( G.dim & 2 )
                    fl_set_object_helper( GUI.run_form_2d->print_button_2d,
                                          "Print window" );
            }

            if ( G.dim & 2 )
            {
                fl_set_pixmapbutton_file( GUI.cut_form->cut_print_button,
                                          pixmap_file );
                fl_set_object_lsize( GUI.cut_form->cut_print_button,
                                     GI.small_font_size );
                if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                    fl_set_object_helper( GUI.cut_form->cut_print_button,
                                          "Print window" );
            }
        }

        T_free( pixmap_file );

        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
        {
            if ( G.dim & 1 )
                fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
                                      "Switch off automatic rescaling" );
            if ( G.dim & 2 )
                fl_set_object_helper( GUI.run_form_2d->full_scale_button_2d,
                                      "Switch off automatic rescaling" );
        }
    }

    if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
    {
        if ( GUI.stop_button_mask == 0 )
        {
            if ( G.dim & 1 || ! G.is_init )
                fl_set_object_helper( GUI.run_form_1d->stop_1d,
                                      "Stop the experiment" );
            if ( G.dim & 2 )
                fl_set_object_helper( GUI.run_form_2d->stop_2d,
                                      "Stop the experiment" );
        }
        else if ( GUI.stop_button_mask == FL_LEFT_MOUSE )
        {
            if ( G.dim & 1 || ! G.is_init )
                fl_set_object_helper( GUI.run_form_1d->stop_1d,
                                      "Stop the experiment\n"
                                      "Use left mouse button" );
            if ( G.dim & 2 )
                fl_set_object_helper( GUI.run_form_2d->stop_2d,
                                      "Stop the experiment\n"
                                      "Use left mouse button" );
        }
        else if ( GUI.stop_button_mask == FL_MIDDLE_MOUSE )
        {
            if ( G.dim & 1 || ! G.is_init )
                fl_set_object_helper( GUI.run_form_1d->stop_1d,
                                      "Stop the experiment\n"
                                      "Use middle mouse button" );
            if ( G.dim & 2 )
                fl_set_object_helper( GUI.run_form_2d->stop_2d,
                                      "Stop the experiment\n"
                                      "Use middle mouse button" );
        }
        else if ( GUI.stop_button_mask == FL_RIGHT_MOUSE )
        {
            if ( G.dim & 1 || ! G.is_init )
                fl_set_object_helper( GUI.run_form_1d->stop_1d,
                                      "Stop the experiment\n"
                                      "Use right mouse button" );
            if ( G.dim & 2 )
                fl_set_object_helper( GUI.run_form_2d->stop_2d,
                                      "Stop the experiment\n"
                                      "Use right mouse button" );
        }
    }

    if ( G.dim == 2 && ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
    {
        fl_set_object_helper( GUI.cut_form->cut_close_button,
                              "Close the window" );
        fl_set_object_helper( GUI.cut_form->cut_full_scale_button,
                              "Switch off automatic rescaling" );
    }

    if ( G.dim & 1 )
    {
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
        {
            fl_set_object_helper( GUI.run_form_1d->curve_1_button_1d,
                                 "Exempt curve 1 from\nrescaling operations" );
            fl_set_object_helper( GUI.run_form_1d->curve_2_button_1d,
                                 "Exempt curve 2 from\nrescaling operations" );
            fl_set_object_helper( GUI.run_form_1d->curve_3_button_1d,
                                 "Exempt curve 3 from\nrescaling operations" );
            fl_set_object_helper( GUI.run_form_1d->curve_4_button_1d,
                                 "Exempt curve 4 from\nrescaling operations" );
        }

        fl_set_object_callback( GUI.run_form_1d->print_button_1d,
                                print_it, 1 );
    }

    if ( G.dim & 2 )
    {
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
        {
            fl_set_object_helper( GUI.run_form_2d->curve_1_button_2d,
                                  "Hide curve 1" );
            fl_set_object_helper( GUI.run_form_2d->curve_2_button_2d,
                                  "Show curve 2" );
            fl_set_object_helper( GUI.run_form_2d->curve_3_button_2d,
                                  "Show curve 3" );
            fl_set_object_helper( GUI.run_form_2d->curve_4_button_2d,
                                  "Show curve 4" );
        }

        fl_set_button( GUI.run_form_2d->curve_1_button_2d, 1 );
        fl_set_button( GUI.run_form_2d->curve_2_button_2d, 0 );
        fl_set_button( GUI.run_form_2d->curve_3_button_2d, 0 );
        fl_set_button( GUI.run_form_2d->curve_4_button_2d, 0 );

        fl_set_object_callback( GUI.run_form_2d->print_button_2d,
                                print_it, 2 );
    }

    /* fdesign is unable to set the box type attributes for canvases... */

    if ( G.dim & 1 || ! G.is_init )
    {
        fl_set_canvas_decoration( GUI.run_form_1d->x_axis_1d, FL_FRAME_BOX );
        fl_set_canvas_decoration( GUI.run_form_1d->y_axis_1d, FL_FRAME_BOX );
        fl_set_canvas_decoration( GUI.run_form_1d->canvas_1d, FL_NO_FRAME );
    }

    if ( G.dim & 2 )
    {
        fl_set_canvas_decoration( GUI.run_form_2d->x_axis_2d, FL_FRAME_BOX );
        fl_set_canvas_decoration( GUI.run_form_2d->y_axis_2d, FL_FRAME_BOX );
        fl_set_canvas_decoration( GUI.run_form_2d->canvas_2d, FL_NO_FRAME );
        fl_set_canvas_decoration( GUI.run_form_2d->z_axis_2d, FL_FRAME_BOX );

        fl_set_canvas_decoration( GUI.cut_form->cut_x_axis, FL_FRAME_BOX );
        fl_set_canvas_decoration( GUI.cut_form->cut_y_axis, FL_FRAME_BOX );
        fl_set_canvas_decoration( GUI.cut_form->cut_z_axis, FL_FRAME_BOX );
        fl_set_canvas_decoration( GUI.cut_form->cut_canvas, FL_NO_FRAME );
    }

    /* Show only buttons really needed */

    if ( G.is_init )
    {
        if ( G.dim & 1 )
        {
            if ( G_1d.nc < 4 )
                fl_hide_object( GUI.run_form_1d->curve_4_button_1d );
            if ( G_1d.nc < 3 )
                fl_hide_object( GUI.run_form_1d->curve_3_button_1d );
            if ( G_1d.nc < 2 )
            {
                fl_hide_object( GUI.run_form_1d->curve_2_button_1d );
                fl_hide_object( GUI.run_form_1d->curve_1_button_1d );
            }
        }

        if ( G.dim & 2 )
        {
            if ( G_2d.nc < 4 )
                fl_hide_object( GUI.run_form_2d->curve_4_button_2d );
            if ( G_2d.nc < 3 )
                fl_hide_object( GUI.run_form_2d->curve_3_button_2d );
            if ( G_2d.nc < 2 )
            {
                fl_hide_object( GUI.run_form_2d->curve_2_button_2d );
                fl_hide_object( GUI.run_form_2d->curve_1_button_2d );
            }
        }
    }
}


/*------------------------------------------------------------------*
 * Function that gets invoked when the close button (not the one in
 * the display window but the one from the windows menu) is clicked
 * on. While the experiment is still running it ignores the event,
 * while, when the experiment is already finished, it does the same
 * as clicking  on the "Close" button within the display window.
 *------------------------------------------------------------------*/

int
run_form_close_handler( FL_FORM * a  UNUSED_ARG,
                        void    * b  UNUSED_ARG )
{
    if ( Fsc2_Internals.child_pid == 0 )      /* if child has already exited */
    {
        if ( ! G.is_init || G.dim & 1 )
            run_close_button_callback( GUI.run_form_1d->stop_1d, 0 );
        else if ( G.dim & 2 )
            run_close_button_callback( GUI.run_form_2d->stop_2d, 0 );
    }

    return FL_IGNORE;
}


/*---------------------------------------------------------*
 * Function initializes several structures for storing the
 * internal state of the graphics during an experiment.
 *---------------------------------------------------------*/

static void
G_struct_init( void )
{
    static bool first_time = true;
    static int cursor_1d[ 8 ];
    static int cursor_2d[ 8 ];
    /* Get the current mouse button states */

    G.button_state = 0;
    G.button_state = 0;

    G.drag_canvas = DRAG_NONE;

    int x, y;
    unsigned int keymask;
    fl_get_mouse( &x, &y, &keymask );

    if ( keymask & Button1Mask )
        G.raw_button_state |= 1;
    if ( keymask & Button2Mask )
        G.raw_button_state |= 2;
    if ( keymask & Button3Mask )
        G.raw_button_state |= 4;

    /* Create the different cursors and the colors needed for 2D displays */

    if ( first_time )
    {
        cursor_1d[ ZOOM_BOX_CURSOR ] = G_1d.cursor[ ZOOM_BOX_CURSOR ] =
                    fl_create_bitmap_cursor( cursor1_bits, cursor1_bits,
                                             cursor1_width, cursor1_height,
                                             cursor1_x_hot, cursor1_y_hot );
        cursor_1d[ MOVE_HAND_CURSOR ] = G_1d.cursor[ MOVE_HAND_CURSOR ] =
                    fl_create_bitmap_cursor( cursor2_bits, cursor2_bits,
                                             cursor2_width, cursor2_height,
                                             cursor2_x_hot, cursor2_y_hot );
        cursor_1d[ ZOOM_LENS_CURSOR ] = G_1d.cursor[ ZOOM_LENS_CURSOR ] =
                    fl_create_bitmap_cursor( cursor3_bits, cursor3_bits,
                                             cursor3_width, cursor3_height,
                                             cursor3_x_hot, cursor3_y_hot );
        cursor_1d[ CROSSHAIR_CURSOR ] = G_1d.cursor[ CROSSHAIR_CURSOR ] =
                    fl_create_bitmap_cursor( cursor4_bits, cursor4_bits,
                                             cursor4_width, cursor4_height,
                                             cursor4_x_hot, cursor4_y_hot );
        cursor_1d[ TARGET_CURSOR ] = G_1d.cursor[ TARGET_CURSOR ] =
                    fl_create_bitmap_cursor( cursor5_bits, cursor5_bits,
                                             cursor5_width, cursor5_height,
                                             cursor5_x_hot, cursor5_y_hot );
        cursor_1d[ ARROW_UP_CURSOR ] = G_1d.cursor[ ARROW_UP_CURSOR ] =
                    fl_create_bitmap_cursor( cursor6_bits, cursor6_bits,
                                             cursor6_width, cursor6_height,
                                             cursor6_x_hot, cursor6_y_hot );
        cursor_1d[ ARROW_RIGHT_CURSOR ] = G_1d.cursor[ ARROW_RIGHT_CURSOR ] =
                    fl_create_bitmap_cursor( cursor7_bits, cursor7_bits,
                                             cursor7_width, cursor7_height,
                                             cursor7_x_hot, cursor7_y_hot );

        cursor_2d[ ZOOM_BOX_CURSOR ] = G_2d.cursor[ ZOOM_BOX_CURSOR ] =
                    fl_create_bitmap_cursor( cursor1_bits, cursor1_bits,
                                             cursor1_width, cursor1_height,
                                             cursor1_x_hot, cursor1_y_hot );
        cursor_2d[ MOVE_HAND_CURSOR ] = G_2d.cursor[ MOVE_HAND_CURSOR ] =
                    fl_create_bitmap_cursor( cursor2_bits, cursor2_bits,
                                             cursor2_width, cursor2_height,
                                             cursor2_x_hot, cursor2_y_hot );
        cursor_2d[ ZOOM_LENS_CURSOR ] = G_2d.cursor[ ZOOM_LENS_CURSOR ] =
                    fl_create_bitmap_cursor( cursor3_bits, cursor3_bits,
                                             cursor3_width, cursor3_height,
                                             cursor3_x_hot, cursor3_y_hot );
        cursor_2d[ CROSSHAIR_CURSOR ] = G_2d.cursor[ CROSSHAIR_CURSOR ] =
                    fl_create_bitmap_cursor( cursor4_bits, cursor4_bits,
                                             cursor4_width, cursor4_height,
                                             cursor4_x_hot, cursor4_y_hot );
        cursor_2d[ TARGET_CURSOR ] = G_2d.cursor[ TARGET_CURSOR ] =
                    fl_create_bitmap_cursor( cursor5_bits, cursor5_bits,
                                             cursor5_width, cursor5_height,
                                             cursor5_x_hot, cursor5_y_hot );
        cursor_2d[ ARROW_UP_CURSOR ] = G_2d.cursor[ ARROW_UP_CURSOR ] =
                    fl_create_bitmap_cursor( cursor6_bits, cursor6_bits,
                                             cursor6_width, cursor6_height,
                                             cursor6_x_hot, cursor6_y_hot );
        cursor_2d[ ARROW_RIGHT_CURSOR ] = G_2d.cursor[ ARROW_RIGHT_CURSOR ] =
                    fl_create_bitmap_cursor( cursor7_bits, cursor7_bits,
                                             cursor7_width, cursor7_height,
                                             cursor7_x_hot, cursor7_y_hot );

        create_colors( );

        /* When use of the HTTP server is compiled in also create a hash of
           colors for speeding up creating an image file from the canvases */

#if defined WITH_HTTP_SERVER
        create_color_hash( );
        G_stored->color_hash = G.color_hash;
        G_stored->color_hash_size = G.color_hash_size;
#endif
    }
    else
        for ( int i = 0; i < 7; i++ )
        {
            G_1d.cursor[ i ] = cursor_1d[ i ];
            G_2d.cursor[ i ] = cursor_2d[ i ];
        }

    G_1d.is_fs = true;
    G_2d.is_fs = true;

    G_1d.rw_min = HUGE_VAL;
    G_1d.rw_max = - HUGE_VAL;
    G_1d.is_scale_set = false;

    G_2d.rw_min = HUGE_VAL;
    G_2d.rw_max = - HUGE_VAL;
    G_2d.is_scale_set = false;

    /* Create pixmaps for labels that need to be rotated by 90 degrees */

    if ( G.dim & 1 && G_1d.label[ Y ] != NULL )
        create_label_pixmap( &G_1d.y_axis, Y, G_1d.label[ Y ] );

    if ( G.dim & 2 && G_2d.label[ Y ] != NULL )
        create_label_pixmap( &G_2d.y_axis, Y, G_2d.label[ Y ] );

    if ( G.dim & 2 && G_2d.label[ Z ] != NULL )
        create_label_pixmap( &G_2d.z_axis, Z, G_2d.label[ Z ] );

    /* Initialize lots of stuff for the curves */

    if ( G.dim & 1 )
        G_init_curves_1d( );

    if ( G.dim & 2 )
        G_init_curves_2d( );

    first_time = false;
}


/*-----------------------------------------*
 * Function for initializing the 1D curves
 *-----------------------------------------*/

static void
G_init_curves_1d( void )
{
    for ( long  i = 0; i < G_1d.nc; i++ )
        G_1d.curve[ i ] = NULL;

    unsigned int depth = fl_get_canvas_depth( G_1d.canvas.obj );

    for ( int i = 0; i < 5; i++ )
        fl_set_cursor_color( G_1d.cursor[ i ], FL_RED, FL_WHITE );

    for ( long i = 0; i < G_1d.nc; i++ )
    {
        /* Allocate memory for the curve and its data */

        Curve_1d_T * cv = G_1d.curve[ i ] = T_malloc( sizeof *cv );

        cv->points = NULL;
        cv->xpoints = NULL;

        /* Create a GC for drawing the curve and set its color */

        cv->gc = XCreateGC( G.d, G_1d.canvas.pm, 0, NULL );
        XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ i ] ) );

        /* Create pixmaps for the out-of-display arrows */

        cv->up_arrow =
            XCreatePixmapFromBitmapData( G.d, G_1d.canvas.pm, up_arrow_bits,
                                         up_arrow_width, up_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_BLACK ), depth );
        G.up_arrow_w = up_arrow_width;
        G.up_arrow_h = up_arrow_width;

        cv->down_arrow =
            XCreatePixmapFromBitmapData( G.d, G_1d.canvas.pm, down_arrow_bits,
                                         down_arrow_width, down_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_BLACK ), depth );

        G.down_arrow_w = down_arrow_width;
        G.down_arrow_h = down_arrow_width;

        cv->left_arrow =
            XCreatePixmapFromBitmapData( G.d, G_1d.canvas.pm, left_arrow_bits,
                                         left_arrow_width, left_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_BLACK ), depth );

        G.left_arrow_w = left_arrow_width;
        G.left_arrow_h = left_arrow_width;

        cv->right_arrow =
            XCreatePixmapFromBitmapData( G.d, G_1d.canvas.pm, right_arrow_bits,
                                         right_arrow_width, right_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_BLACK ), depth );

        G.right_arrow_w = right_arrow_width;
        G.right_arrow_h = right_arrow_width;

        /* Set the scaling factors for the curve */

        cv->s2d[ X ] = ( double ) ( G_1d.canvas.w - 1 ) / ( G_1d.nx - 1 );
        cv->s2d[ Y ] = ( double ) ( G_1d.canvas.h - 1 );

        cv->shift[ X ] = cv->shift[ Y ] = 0.0;

        cv->count = 0;
        cv->active = true;
        cv->can_undo = false;

        /* Finally get memory for the data */

        cv->points = T_malloc( G_1d.nx * sizeof *cv->points );

        for ( long j = 0; j < G_1d.nx; j++ )      /* no points are known yet */
            cv->points[ j ].exist = false;

        cv->xpoints = T_malloc( G_1d.nx * sizeof *cv->xpoints );
    }
}


/*-----------------------------------------*
 * Function for initializing the 2D curves
 *-----------------------------------------*/

static void
G_init_curves_2d( void )
{
    for ( long i = 0; i < G_2d.nc; i++ )
        G_2d.curve_2d[ i ] = NULL;

    unsigned int depth = fl_get_canvas_depth( G_2d.canvas.obj );

    for ( long i = 0; i < NUM_COLORS + 2; i++ )
    {
        G_2d.gcs[ i ] = XCreateGC( G.d, G_2d.canvas.pm, 0, NULL );
        XSetForeground( G.d, G_2d.gcs[ i ], fl_get_pixel( FL_FREE_COL1 + i ) );
        XSetLineAttributes( G.d, G_2d.gcs[ i ], 0, LineSolid, CapButt,
                            JoinBevel );
    }

    for ( int i = 0; i < 7; i++ )
        fl_set_cursor_color( G_2d.cursor[ i ], FL_BLACK, FL_WHITE );

    for ( long i = 0; i < G_2d.nc; i++ )
    {
        /* Allocate memory for the curve */

        Curve_2d_T * cv = G_2d.curve_2d[ i ] = T_malloc( sizeof *cv );

        cv->points = NULL;
        cv->xpoints = NULL;

        cv->marker_2d  = NULL;
        cv->cut_marker = NULL;

        cv->active = false;

        cv->needs_recalc = false;
        cv->w = cv->h = 0;

        /* Create a GC for drawing the curve and set its color */

        cv->gc = XCreateGC( G.d, G_2d.canvas.pm, 0, NULL );
        XSetForeground( G.d, cv->gc, fl_get_pixel( G.colors[ i ] ) );

        /* Create pixmaps for the out-of-display arrows */

        cv->up_arrow =
            XCreatePixmapFromBitmapData( G.d, G_2d.canvas.pm, up_arrow_bits,
                                         up_arrow_width, up_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_INACTIVE ), depth );
        G.up_arrow_w = up_arrow_width;
        G.up_arrow_h = up_arrow_width;

        cv->down_arrow =
            XCreatePixmapFromBitmapData( G.d, G_2d.canvas.pm, down_arrow_bits,
                                         down_arrow_width, down_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_INACTIVE ), depth );

        G.down_arrow_w = down_arrow_width;
        G.down_arrow_h = down_arrow_width;

        cv->left_arrow =
            XCreatePixmapFromBitmapData( G.d, G_2d.canvas.pm, left_arrow_bits,
                                         left_arrow_width, left_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_INACTIVE ), depth );

        G.left_arrow_w = left_arrow_width;
        G.left_arrow_h = left_arrow_width;

        cv->right_arrow =
            XCreatePixmapFromBitmapData( G.d, G_2d.canvas.pm, right_arrow_bits,
                                         right_arrow_width, right_arrow_height,
                                         fl_get_pixel( G.colors[ i ] ),
                                         fl_get_pixel( FL_INACTIVE ), depth );

        G.right_arrow_w = right_arrow_width;
        G.right_arrow_h = right_arrow_width;

        /* Set the scaling factors for the curve */

        cv->s2d[ X ] = ( double ) ( G_2d.canvas.w - 1 ) / ( G_2d.nx - 1 );
        cv->s2d[ Y ] = ( double ) ( G_2d.canvas.h - 1 ) / ( G_2d.ny - 1 );
        cv->s2d[ Z ] = ( double ) ( G_2d.z_axis.h - 1 );

        cv->shift[ X ] = cv->shift[ Y ] = cv->shift[ Z ] = 0.0;

        cv->z_factor = 1.0;

        cv->rwc_start[ X ] = G_2d.rwc_start[ X ];
        cv->rwc_start[ Y ] = G_2d.rwc_start[ Y ];
        cv->rwc_delta[ X ] = G_2d.rwc_delta[ X ];
        cv->rwc_delta[ Y ] = G_2d.rwc_delta[ Y ];

        cv->count = 0;
        cv->can_undo = false;

        cv->is_fs = true;

        cv->rw_min = HUGE_VAL;
        cv->rw_max = - HUGE_VAL;
        cv->is_scale_set = false;

        /* Now get also memory for the data */

        cv->points = T_malloc( G_2d.nx * G_2d.ny * sizeof *cv->points );

        Scaled_Point_T * sp = cv->points;
        for ( long j = 0; j < G_2d.nx * G_2d.ny; sp++, j++ )
            sp->exist = false;

        cv->xpoints = T_malloc( G_2d.nx * G_2d.ny * sizeof *cv->xpoints );
    }

    G_2d.curve_2d[ 0 ]->active = true;      /* first curve is the active one */
}


/*----------------------------------------------------------------------*
 * This routine creates a pixmap with the label for either the y- or z-
 * axis (set 'coord' to 1 for y and 2 for z). The problem is that it's
 * not possible to draw rotated text so we have to write the text to a
 * pixmap and then rotate this pixmap 'by hand'.
 *----------------------------------------------------------------------*/

void
create_label_pixmap( Canvas_T * c,
                     int        coord,
                     char     * label )
{

    /* Make sure we don't do something stupid... */

    fsc2_assert(    (    coord == Y
                      && ( c == &G_1d.y_axis || c == &G_2d.y_axis ) )
                 || (    coord == Z
                      && ( c == &G_2d.z_axis || c == &G_2d.cut_z_axis ) ) );

    /* Distinguish between labels for the primary window and the cut window
       (this function is never called for the cut windows y-axis) */

    int r_coord = coord;
    if ( c == &G_2d.cut_z_axis )
        r_coord += 3;

    /* Get size for a temporary pixmap */

    int width = text_width( label ) + 10;
    int height = G.font_asc + G.font_desc + 5;

    if ( width > USHRT_MAX )
        width = USHRT_MAX;
    if ( height > USHRT_MAX )
        height = USHRT_MAX;

    /* Create the temporary pixmap, fill with the color of the axis canvas
       and draw the text */

    Pixmap pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), width, height,
                               fl_get_canvas_depth( c->obj ) );

    XFillRectangle( G.d, pm, c->gc, 0, 0, width, height );
    XftDrawChange( c->xftdraw, pm );
    XftDrawStringUtf8( c->xftdraw, G.xftcolor + MAX_CURVES, G.font,
                       5, height - 1 - G.font_desc,
                       ( XftChar8 const * ) label, strlen( label ) );
    XftDrawChange( c->xftdraw, c->pm );

    /* Create the real pixmap for the label and copy the contents of the
       temporary pixmap to the final pixmap, rotated by 90 degree ccw */

    if ( c == &G_1d.y_axis )
    {
        G_1d.label_pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), height, width,
                                       fl_get_canvas_depth( c->obj ) );

        G_1d.label_w = i2u15( height );
        G_1d.label_h = i2u15( width );

        int k = width - 1;
        for ( int i = 0; i < width; k--, i++ )
            for ( int j = 0; j < height; j++ )
                XCopyArea( G.d, pm, G_1d.label_pm, c->gc, i, j, 1, 1, j, k );
    }
    else
    {
        G_2d.label_pm[ r_coord ] =
                                XCreatePixmap( G.d, FL_ObjWin( c->obj ),
                                               height, width,
                                               fl_get_canvas_depth( c->obj ) );

        G_2d.label_w[ r_coord ] = i2u15( height );
        G_2d.label_h[ r_coord ] = i2u15( width );

        int k = width - 1;
        for ( int i = 0; i < width; k--, i++ )
            for ( int j = 0; j < height; j++ )
                XCopyArea( G.d, pm, G_2d.label_pm[ r_coord ], c->gc,
                           i, j, 1, 1, j, k );
    }

    /* Get rid of the temporary pixmap */

    XFreePixmap( G.d, pm );
}


/*--------------------------------------------------------*
 * Removes the window for displaying measurement results.
 *--------------------------------------------------------*/

void
stop_graphics( void )
{
    G.is_fully_drawn = false;

    if ( G.is_init )
    {
        graphics_free( );

        if ( G.dim & 1 )
            for ( int i = X; i <= Y; i++ )
                if ( G_1d.label[ i ] )
                    G_1d.label[ i ] = T_free( G_1d.label[ i ] );
        if ( G.dim & 2 )
            for ( int i = X; i <= Z; i++ )
                if ( G_2d.label[ i ] )
                    G_2d.label[ i ] = T_free( G_2d.label[ i ] );

        XftFontClose( G.d, G.font );

        for ( long i = 0; i < MAX_CURVES + 2; i++ )
            XftColorFree( G.d, fl_get_visual(), fl_get_colormap( ),
                          G.xftcolor + i );

        if ( GUI.run_form_1d )
        {
            if ( G_1d.x_axis.obj )
            {
                canvas_off( &G_1d.x_axis, GUI.run_form_1d->x_axis_1d );
                G_1d.x_axis.obj = NULL;
            }
            if ( G_1d.y_axis.obj )
            {
                canvas_off( &G_1d.y_axis, GUI.run_form_1d->y_axis_1d );
                G_1d.y_axis.obj = NULL;
            }
            if ( G_1d.canvas.obj )
            {
                canvas_off( &G_1d.canvas, GUI.run_form_1d->canvas_1d );
                G_1d.canvas.obj = NULL;
            }
        }

        if ( GUI.run_form_2d )
        {
            if ( G_2d.x_axis.obj )
            {
                canvas_off( &G_2d.x_axis, GUI.run_form_2d->x_axis_2d );
                G_2d.x_axis.obj = NULL;
            }
            if ( G_2d.y_axis.obj )
            {
                canvas_off( &G_2d.y_axis, GUI.run_form_2d->y_axis_2d );
                G_2d.y_axis.obj = NULL;
            }
            if ( G_2d.canvas.obj )
            {
                canvas_off( &G_2d.canvas, GUI.run_form_2d->canvas_2d );
                G_2d.canvas.obj = NULL;
            }

            if ( G_2d.z_axis.obj )
            {
                canvas_off( &G_2d.z_axis, GUI.run_form_2d->z_axis_2d );
                G_2d.z_axis.obj = NULL;
            }

            cut_form_close( );
        }
    }

    /* Store positions and sizes of display windows and remove the windows. */

    if ( GUI.run_form_1d && fl_form_is_visible( GUI.run_form_1d->run_1d ) )
    {
        get_form_position( GUI.run_form_1d->run_1d, &GUI.display_1d_x,
                           &GUI.display_1d_y );
        GUI.display_1d_has_pos = true;

        GUI.display_1d_width = GUI.run_form_1d->run_1d->w;
        GUI.display_1d_height = GUI.run_form_1d->run_1d->h;

        GUI.display_1d_has_pos = GUI.display_1d_has_size = true;

        fl_hide_form( GUI.run_form_1d->run_1d );
        fl_free_form( GUI.run_form_1d->run_1d );
        fl_free( GUI.run_form_1d );
        GUI.run_form_1d = NULL;
    }

    if ( GUI.run_form_2d && fl_form_is_visible( GUI.run_form_2d->run_2d ) )
    {
        get_form_position( GUI.run_form_2d->run_2d, &GUI.display_2d_x,
                           &GUI.display_2d_y );
        GUI.display_2d_has_pos = true;

        GUI.display_2d_width = GUI.run_form_2d->run_2d->w;
        GUI.display_2d_height = GUI.run_form_2d->run_2d->h;

        GUI.display_2d_has_pos = GUI.display_2d_has_size = true;

        fl_hide_form( GUI.run_form_2d->run_2d );
        fl_free_form( GUI.run_form_2d->run_2d );
        fl_free( GUI.run_form_2d );
        GUI.run_form_2d = NULL;
    }

    Marker_1d_T * m = G_1d.marker_1d;
    while ( m )
    {
        Marker_1d_T * mn = m->next;
        XFreeGC( G.d, m->gc );
        T_free( m );
        m = mn;
    }

    if ( G_stored )
    {
        memcpy( &G, G_stored, sizeof G );
        G_stored = T_free( G_stored );
    }

    if ( G_1d_stored )
    {
        memcpy( &G_1d, G_1d_stored, sizeof G_1d );
        G_1d_stored = T_free( G_1d_stored );

        for ( int i = X; i <= Y; i++ )
            G_1d.label[ i ] = NULL;
    }

    if ( G_2d_stored )
    {
        memcpy( &G_2d, G_2d_stored, sizeof G_2d );
        G_2d_stored = T_free( G_2d_stored );

        for ( int i = X; i <= Z; i++ )
            G_2d.label[ i ] = NULL;
    }
}


/*--------------------------------------------*
 * Deallocates the memory needed for graphics
 *--------------------------------------------*/

static void
graphics_free( void )
{
    /* Deallocate memory for pixmaps, scaled data and XPoints. The function
       must also work correctly when it is called because we run out of
       memory. The way things are organized after allocating memory for a
       curve first the graphical elements are created and afterwards memory
       for the data is allocated. */

    if ( G.dim & 1 )
        for ( long i = 0; i < G_1d.nc; i++ )
        {
            Curve_1d_T * cv = G_1d.curve[ i ];

            if ( ! cv )
                 break;

            XFreeGC( G.d, cv->gc );
            XFreePixmap( G.d, cv->up_arrow );
            XFreePixmap( G.d, cv->down_arrow );
            XFreePixmap( G.d, cv->left_arrow );
            XFreePixmap( G.d, cv->right_arrow );

            T_free( cv->points );
            T_free( cv->xpoints );
            cv = T_free( cv );
        }

    if ( G.dim & 2 )
    {
        for ( long i = 0; i < NUM_COLORS + 2; i++ )
            XFreeGC( G.d, G_2d.gcs[ i ] );

        for ( long i = 0; i < G_2d.nc; i++ )
        {
            Curve_2d_T * cv2  = G_2d.curve_2d[ i ];

            if ( ! cv2 )
                break;

            XFreeGC( G.d, cv2->gc );
            XFreePixmap( G.d, cv2->up_arrow );
            XFreePixmap( G.d, cv2->down_arrow );
            XFreePixmap( G.d, cv2->left_arrow );
            XFreePixmap( G.d, cv2->right_arrow );

            Marker_2d_T * m2 = cv2->marker_2d;
            while ( m2 )
            {
                Marker_2d_T * mn2 = m2->next;
                XFreeGC( G.d, m2->gc );
                m2 = T_free( m2 );
                m2 = mn2;
            }

            T_free( cv2->points );
            T_free( cv2->xpoints );
            cv2 = T_free( cv2 );
        }
    }

    if ( G.dim & 1 )
        if ( G_1d.label[ Y ] )
            XFreePixmap( G.d, G_1d.label_pm );

    if ( G.dim & 2 )
        for ( int coord = Y; coord <= Z; coord++ )
            if ( G_2d.label[ coord ] )
                XFreePixmap( G.d, G_2d.label_pm[ coord ] );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static void
canvas_off( Canvas_T  * c,
            FL_OBJECT * obj )
{
    FL_HANDLE_CANVAS ch;
    if ( c == &G_1d.x_axis || c == &G_1d.y_axis || c == &G_1d.canvas )
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
    }

    XFreeGC( G.d, c->axis_gc );
    XftDrawDestroy( c->xftdraw );

    delete_pixmap( c );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
setup_canvas( Canvas_T  * c,
              FL_OBJECT * obj )
{
    FL_HANDLE_CANVAS ch;
    if ( c == &G_1d.x_axis || c == &G_1d.y_axis || c == &G_1d.canvas )
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

    c->xftdraw = XftDrawCreate( G.d, c->pm, fl_get_visual(),
                                fl_get_colormap( ) );

    fl_add_canvas_handler( c->obj, Expose, ch, c );

    XSetWindowAttributes attributes;

    attributes.backing_store = NotUseful;
    attributes.background_pixmap = None;
    XChangeWindowAttributes( G.d, FL_ObjWin( c->obj ),
                             CWBackingStore | CWBackPixmap,
                             &attributes );
    c->is_box = false;

    fl_add_canvas_handler( c->obj, ConfigureNotify, ch, c );

    if ( G.is_init )
    {
        fl_add_canvas_handler( c->obj, ButtonPress, ch, c );
        fl_add_canvas_handler( c->obj, ButtonRelease, ch, c );
        fl_add_canvas_handler( c->obj, MotionNotify, ch, c );

        /* Get motion events only when first or second button is pressed
           (this got to be set *after* requesting motion events) */

        fl_remove_selected_xevent( FL_ObjWin( obj ),
                                   PointerMotionMask | PointerMotionHintMask |
                                   ButtonMotionMask );
        fl_addto_selected_xevent( FL_ObjWin( obj ),
                                  Button1MotionMask | Button2MotionMask );
    }

    c->axis_gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, NULL );
    XSetForeground( G.d, c->axis_gc, fl_get_pixel( FL_BLACK ) );
}


/*----------------------------------------------*
 * Creates a pixmap for a canvas for buffering.
 *----------------------------------------------*/

void
create_pixmap( Canvas_T * c )
{
    c->gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, NULL );
    c->pm = XCreatePixmap( G.d, FL_ObjWin( c->obj ), c->w, c->h,
                           fl_get_canvas_depth( c->obj ) );

    if ( c == &G_1d.canvas || c == &G_2d.canvas )
    {
        if ( G.is_init && c == &G_1d.canvas )
            XSetForeground( G.d, c->gc, fl_get_pixel( FL_BLACK ) );
        else
            XSetForeground( G.d, c->gc, fl_get_pixel( FL_INACTIVE ) );
    }
    else
        XSetForeground( G.d, c->gc, fl_get_pixel( FL_LEFT_BCOL ) );

    if ( G.is_init )
    {
        c->box_gc = XCreateGC( G.d, FL_ObjWin( c->obj ), 0, NULL );

        if ( c == &G_1d.canvas || c == &G_1d.x_axis || c == &G_1d.y_axis )
            XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_RED ) );
        else
            XSetForeground( G.d, c->box_gc, fl_get_pixel( FL_BLACK ) );

        XSetLineAttributes( G.d, c->box_gc, 0, LineOnOffDash, CapButt,
                            JoinMiter );

        char dashes[ ] = { 2, 2 };
        XSetDashes( G.d, c->box_gc, 0, dashes, 2 );
    }
}


/*------------------------------------------------*
 * Deletes the pixmap for a canvas for buffering.
 *------------------------------------------------*/

void
delete_pixmap( Canvas_T * c )
{
    XFreeGC( G.d, c->gc );
    XFreePixmap( G.d, c->pm );

    if ( G.is_init )
        XFreeGC( G.d, c->box_gc );
}


/*----------------------------------------------------*
 * Redraws the axis areas of a the 1D display window.
 *----------------------------------------------------*/

void
redraw_axis_1d( int coord )
{
    Canvas_T *c;
    Curve_1d_T *cv = NULL;

    fsc2_assert( coord == X || coord == Y );

    /* First draw the label - for the x-axis it's just done by drawing the
       string while for the y-axis we have to copy a pixmap since the label
       is a string rotated by 90 degree that has been drawn in advance */

    if ( coord == X )
    {
        c = &G_1d.x_axis;

        if ( G_1d.label[ X ] != NULL )
        {
            int width = text_width( G_1d.label[ X ] );
            XftDrawStringUtf8( c->xftdraw, G.xftcolor + MAX_CURVES, G.font,
                               c->w - width - 5, c->h - 2 - G.font_desc,
                                ( XftChar8 const * ) G_1d.label[ X ],
                               strlen( G_1d.label[ X ] ) );
        }
    }
    else
    {
        c = &G_1d.y_axis;

        if ( G_1d.label[ coord ] != NULL )
            XCopyArea( G.d, G_1d.label_pm, c->pm, c->gc, 0, 0,
                       G_1d.label_w, G_1d.label_h, 0, 0 );
    }

    if ( ! G_1d.is_scale_set )
        return;

    /* Find out the active curve for the axis */

    long i;
    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        if ( cv->active )
            break;
    }

    if ( i == G_1d.nc )                       /* no active curve -> no scale */
        return;

    make_scale_1d( cv, c, coord );
}


/*----------------------------------------------------*
 * Redraws the axis areas of a the 2D display window.
 *----------------------------------------------------*/

void
redraw_axis_2d( int coord )
{
    fsc2_assert( coord == X || coord == Y || coord == Z );

    /* First draw the label - for the x-axis it's just done by drawing the
       string while for the y- and z-axis we have to copy a pixmap since the
       label is a string rotated by 90 degree that has been drawn in advance */

    Canvas_T * c;

    if ( coord == X )
    {
        c = &G_2d.x_axis;
        if ( G_2d.label[ X ] != NULL )
        {
            int width = text_width( G_2d.label[ X ] );
            XftDrawStringUtf8( c->xftdraw, G.xftcolor + MAX_CURVES, G.font,
                               c->w - width - 5, c->h - 2 - G.font_desc,
                                ( XftChar8 const * ) G_2d.label[ X ],
                               strlen( G_2d.label[ X ] ) );
        }
    }
    else if ( coord == Y )
    {
        c = &G_2d.y_axis;
        if ( G_2d.label[ coord ] != NULL )
            XCopyArea( G.d, G_2d.label_pm[ coord ], c->pm, c->gc, 0, 0,
                       G_2d.label_w[ coord ], G_2d.label_h[ coord ], 0, 0 );
    }
    else
    {
        c = &G_2d.z_axis;
        if ( G_2d.label[ coord ] != NULL )
            XCopyArea( G.d, G_2d.label_pm[ coord ], c->pm, c->gc, 0, 0,
                       G_2d.label_w[ coord ], G_2d.label_h[ coord ],
                       c->w - 5 - G_2d.label_w[ coord ], 0 );
    }

    if (    G_2d.active_curve == -1
         || ! G_2d.curve_2d[ G_2d.active_curve ]->is_scale_set )
        return;

    /* Find out the active curve for the axis */

    if ( G_2d.active_curve != -1 )
        make_scale_2d( G_2d.curve_2d[ G_2d.active_curve ], c, coord );
}


/*--------------------------------------------------*
 * Creates number strings for axis labels with the
 * correct number of digits after the decimal point
 *--------------------------------------------------*/

void
make_label_string( char   * lstr,
                   double   num,
                   int      res )
{
    if ( num == 0.0 )
    {
        sprintf( lstr, "0" );
        return;
    }

   res++;

    int mag = irnd( floor( log10( fabs( num ) ) ) );
    if ( mag >= 0 )
        mag++;

    /* n is the number of relevant digits */

    int n = i_max( 1, mag - res );

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


/*---------------------------------------------------------------------------*
 * This routine is called from functions (like the function showing the file
 * selector box) to switch off the special states entered by pressing one or
 * more of the mouse buttons. Otherwise, after these functions are finished
 * (usually with all mouse buttons released) the drawing routines would
 * think that the buttons are still pressed, forcing the user to press the
 * buttons again just to receive the ButtonRelease event.
 *---------------------------------------------------------------------------*/

void
switch_off_special_cursors( void )
{
    if ( ! G.is_init )
        return;

    if ( G.button_state != 0 )
    {
        G.button_state = G.raw_button_state = 0;

        if ( G.drag_canvas == DRAG_1D_X )         /* 1D x-axis window */
        {
            fl_reset_cursor( FL_ObjWin( G_1d.x_axis.obj ) );
            G_1d.x_axis.is_box = false;
            repaint_canvas_1d( &G_1d.x_axis );
        }

        if ( G.drag_canvas == DRAG_1D_Y )         /* 1D y-axis window */
        {
            fl_reset_cursor( FL_ObjWin( G_1d.y_axis.obj ) );
            G_1d.y_axis.is_box = false;
            repaint_canvas_1d( &G_1d.y_axis );
        }

        if ( G.drag_canvas == DRAG_1D_C )         /* 1D canvas window */
        {
            fl_reset_cursor( FL_ObjWin( G_1d.canvas.obj ) );
            G_1d.canvas.is_box = false;
            repaint_canvas_1d( &G_1d.canvas );
        }

        if ( G.drag_canvas == DRAG_2D_X )         /* 2D x-axis window */
        {
            fl_reset_cursor( FL_ObjWin( G_2d.x_axis.obj ) );
            G_2d.x_axis.is_box = false;
            repaint_canvas_2d( &G_2d.x_axis );
        }

        if ( G.drag_canvas == DRAG_2D_Y )         /* 2D y-axis window */
        {
            fl_reset_cursor( FL_ObjWin( G_2d.y_axis.obj ) );
            G_2d.y_axis.is_box = false;
            repaint_canvas_2d( &G_1d.y_axis );
        }

        if ( G.drag_canvas == DRAG_2D_Z )         /* 2D z-axis window */
        {
            fl_reset_cursor( FL_ObjWin( G_2d.z_axis.obj ) );
            G_2d.z_axis.is_box = false;
            repaint_canvas_2d( &G_2d.z_axis );
        }

        if ( G.drag_canvas == DRAG_2D_C )         /* 2D canvas window */
        {
            fl_reset_cursor( FL_ObjWin( G_2d.canvas.obj ) );
            G_2d.canvas.is_box = false;
            repaint_canvas_2d( &G_2d.canvas );
        }
    }
}


/*-------------------------------------------------------------*
 * Undoes the last action in the 1D window as far as possible.
 *-------------------------------------------------------------*/

void
undo_button_callback_1d( FL_OBJECT * a  UNUSED_ARG,
                         long        b  UNUSED_ARG )
{
    bool is_undo = false;

    for ( long i = 0; i < G_1d.nc; i++ )
    {
        Curve_1d_T * cv = G_1d.curve[ i ];

        if ( ! cv->can_undo )
            continue;

        for ( int j = X; j <= Y; j++ )
        {
            double temp_s2d = cv->s2d[ j ];
            double temp_shift = cv->shift[ j ];

            cv->s2d[ j ] = cv->old_s2d[ j ];
            cv->shift[ j ] = cv->old_shift[ j ];

            cv->old_s2d[ j ] = temp_s2d;
            cv->old_shift[ j ] = temp_shift;
        }

        is_undo = true;

        recalc_XPoints_of_curve_1d( cv );
    }

    if ( is_undo && G_1d.is_fs )
    {
        G_1d.is_fs = false;
        fl_set_button( GUI.run_form_1d->full_scale_button_1d, 0 );
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
                                  "Rescale curves to fit into the window\n"
                                  "and switch on automatic rescaling" );
    }

    if ( is_undo )
        redraw_all_1d( );
}


/*-------------------------------------------------------------*
 * Undoes the last action in the 2D window as far as possible.
 *-------------------------------------------------------------*/

void
undo_button_callback_2d( FL_OBJECT * a  UNUSED_ARG,
                         long        b  UNUSED_ARG )
{
    if (    G_2d.active_curve == -1
         || ! G_2d.curve_2d[ G_2d.active_curve ]->can_undo )
        return;

    Curve_2d_T * cv2 = G_2d.curve_2d[ G_2d.active_curve ];

    for ( int j = X; j <= Z; j++ )
    {
        double temp_s2d = cv2->s2d[ j ];
        double temp_shift = cv2->shift[ j ];

        cv2->s2d[ j ] = cv2->old_s2d[ j ];
        cv2->shift[ j ] = cv2->old_shift[ j ];

        cv2->old_s2d[ j ] = temp_s2d;
        cv2->old_shift[ j ] = temp_shift;
    }


    double temp_z_factor = cv2->z_factor;
    cv2->z_factor = cv2->old_z_factor;
    cv2->old_z_factor = temp_z_factor;

    if ( cv2->is_fs )
    {
        cv2->is_fs = false;
        fl_set_button( GUI.run_form_2d->full_scale_button_2d, 0 );
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.run_form_2d->full_scale_button_2d,
                                  "Rescale curves to fit into the window\n"
                                  "and switch on automatic rescaling" );
    }

    cv2->needs_recalc = true;
    redraw_all_2d( );
}


/*------------------------------------------------------------------*
 * Rescales the 1D display so that the canvas is completely used in
 * vertical direction (the horizontal scaling is left unchanged)
 *------------------------------------------------------------------*/

void
fs_vert_rescale_1d( void )
{
    /* Store states of the scales... */

    for (long  i = 0; i < G_1d.nc; i++ )
        save_scale_state_1d( G_1d.curve[ i ] );

    /* ... and rescale to full scale */

    fs_rescale_1d( true );
    redraw_all_1d( );
}


/*---------------------------------------------------------------*
 * Rescales the 2D display so that the canvas is completely used
 * in z-direction (the other directions are left unchanged)
 *----------------------------------------------------------------*/

void
fs_vert_rescale_2d( void )
{
    if ( G_2d.active_curve != -1 )
    {
        save_scale_state_2d( G_2d.curve_2d[ G_2d.active_curve ] );
        fs_rescale_2d( G_2d.curve_2d[ G_2d.active_curve ], true );
        redraw_all_2d( );
    }
}


/*-------------------------------------------*
 * Callback for the FS button for 1D display
 *-------------------------------------------*/

void
fs_button_callback_1d( FL_OBJECT * a  UNUSED_ARG,
                       long        b  UNUSED_ARG )
{
    /* Rescaling is useless if graphic isn't initialized */

    if ( ! G.is_init )
        return;

    /* Get new state of button */

    int state = fl_get_button( GUI.run_form_1d->full_scale_button_1d );

    if ( state == 1 )        /* full scale got switched on */
    {
        G_1d.is_fs = true;

        /* Store data of previous state... */

        for ( long i = 0; i < G_1d.nc; i++ )
            save_scale_state_1d( G_1d.curve[ i ] );

        /* ... and rescale to full scale */

        fs_rescale_1d( false );
        redraw_all_1d( );
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
                                  "Switch off automatic rescaling" );
    }
    else        /* full scale got switched off */
    {
        G_1d.is_fs = false;
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
                                  "Rescale curves to fit into the window\n"
                                  "and switch on automatic rescaling" );
    }
}


/*-------------------------------------------*
 * Callback for the FS button for 2D display
 *-------------------------------------------*/

void
fs_button_callback_2d( FL_OBJECT * a  UNUSED_ARG,
                       long        b  UNUSED_ARG )
{
    /* Rescaling is useless if graphic isn't initialized */

    if ( ! G.is_init )
        return;

    /* Get new state of button */

    int state = fl_get_button( GUI.run_form_2d->full_scale_button_2d );

    if ( state == 1 )        /* full scale got switched on */
    {
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.run_form_2d->full_scale_button_2d,
                                  "Switch off automatic rescaling" );

        if ( G_2d.active_curve != -1 )
        {
            save_scale_state_2d( G_2d.curve_2d[ G_2d.active_curve ] );
            G_2d.curve_2d[ G_2d.active_curve ]->is_fs = true;
            fs_rescale_2d( G_2d.curve_2d[ G_2d.active_curve ], false );
            redraw_all_2d( );
        }
    }
    else                     /* full scale got switched off */
    {
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.run_form_2d->full_scale_button_2d,
                                  "Rescale curves to fit into the window\n"
                                  "and switch on automatic rescaling" );
        if ( G_2d.active_curve != -1 )
            G_2d.curve_2d[ G_2d.active_curve ]->is_fs = false;
    }
}


/*------------------------------------------------------------------*
 * Callback function for the curve buttons in the 1D display window
 *------------------------------------------------------------------*/

void
curve_button_callback_1d( FL_OBJECT * obj,
                          long        data )
{
    /* Change the help string for the button */

    char hstr[ 128 ];
    if ( ( G_1d.curve[ data - 1 ]->active = fl_get_button( obj ) ) )
        sprintf( hstr, "Exempt curve %ld from\nrescaling operations", data );
    else
        sprintf( hstr, "Include curve %ld into\nrescaling operations", data );

    if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
        fl_set_object_helper( obj, hstr );

    /* Redraw both axis to make sure the axis for the first active button
       is shown. If markers are shown also redraw the canvas. */

    redraw_canvas_1d( &G_1d.x_axis );
    redraw_canvas_1d( &G_1d.y_axis );
    if ( G_1d.marker_1d != NULL )
        redraw_canvas_1d( &G_1d.canvas );
}


/*------------------------------------------------------------------*
 * Callback function for the curve buttons in the 2D display window
 *------------------------------------------------------------------*/

void
curve_button_callback_2d( FL_OBJECT * obj,
                          long        data )
{
    if (    G.drag_canvas == DRAG_2D_X  || G.drag_canvas == DRAG_2D_Y
         || G.drag_canvas == DRAG_2D_Z  || G.drag_canvas == DRAG_2D_C
         || G.drag_canvas == DRAG_CUT_X || G.drag_canvas == DRAG_CUT_Y
         || G.drag_canvas == DRAG_CUT_Z || G.drag_canvas == DRAG_CUT_C )
    {
        G.button_state = 0;
        G.coord_display &= ~ 6;
        G.dist_display  &= ~ 6;
        G_2d.cut_select = NO_CUT_SELECT;

        switch ( G.drag_canvas )
        {
            case DRAG_2D_X :
                fl_reset_cursor( FL_ObjWin( GUI.run_form_2d->x_axis_2d ) );
                break;

            case DRAG_2D_Y :
                fl_reset_cursor( FL_ObjWin( GUI.run_form_2d->y_axis_2d ) );
                break;

            case DRAG_2D_Z :
                fl_reset_cursor( FL_ObjWin( GUI.run_form_2d->z_axis_2d ) );
                break;

            case DRAG_2D_C :
                fl_reset_cursor( FL_ObjWin( GUI.run_form_2d->canvas_2d ) );
                break;

            case DRAG_CUT_X :
                fl_reset_cursor( FL_ObjWin( GUI.cut_form->cut_x_axis ) );
                break;

            case DRAG_CUT_Y :
                fl_reset_cursor( FL_ObjWin( GUI.cut_form->cut_y_axis ) );
                break;

            case DRAG_CUT_Z :
                fl_reset_cursor( FL_ObjWin( GUI.cut_form->cut_z_axis ) );
                break;

            case DRAG_CUT_C :
                fl_reset_cursor( FL_ObjWin( GUI.cut_form->cut_canvas ) );
                break;
        }

        G.drag_canvas = DRAG_NONE;
    }

    /* We get a negative curve number if this handler is called via the
       hidden curve buttons in the cross section window and have to set
       the correct state of the buttons in the main display window */

    if ( data < 0 )
    {
        data = - data;

        if ( data > G_2d.nc )  /* button for non-existing curve triggered */
            return;            /* via the keyboard */

        switch( data )
        {
            case 1:
                obj = GUI.run_form_2d->curve_1_button_2d;
                fl_set_button( obj, G_2d.active_curve == 0 ? 0 : 1 );
                break;

            case 2:
                obj = GUI.run_form_2d->curve_2_button_2d;
                fl_set_button( obj, G_2d.active_curve == 1 ? 0 : 1 );
                break;

            case 3:
                obj = GUI.run_form_2d->curve_3_button_2d;
                fl_set_button( obj, G_2d.active_curve == 2 ? 0 : 1 );
                break;

            case 4:
                obj = GUI.run_form_2d->curve_4_button_2d;
                fl_set_button( obj, G_2d.active_curve == 3 ? 0 : 1 );
                break;
        }
    }
    else
    {
        int bstate = fl_get_button( obj );
        if (    (   bstate && data - 1 == G_2d.active_curve )
             || ( ! bstate && G_2d.active_curve == -1 ) )
            return;
    }

    /* Make buttons work like radio buttons but also allow switching off
       all of them */

    if ( data - 1 == G_2d.active_curve )      /* curve is to be switched off */
    {
        G_2d.curve_2d[ G_2d.active_curve ]->active = false;
        G_2d.active_curve = -1;

        char hstr[ 128 ];
        sprintf( hstr, "Show curve %ld", data );
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( obj, hstr );
    }
    else
    {
        if ( G_2d.active_curve != -1 )
            G_2d.curve_2d[ G_2d.active_curve ]->active = false;

        switch ( G_2d.active_curve )
        {
            case 0 :
                fl_set_button( GUI.run_form_2d->curve_1_button_2d, 0 );
                if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                    fl_set_object_helper( GUI.run_form_2d->curve_1_button_2d,
                                          "Show curve 1" );
                break;

            case 1 :
                fl_set_button( GUI.run_form_2d->curve_2_button_2d, 0 );
                if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                    fl_set_object_helper( GUI.run_form_2d->curve_2_button_2d,
                                          "Show curve 2" );
                break;

            case 2 :
                fl_set_button( GUI.run_form_2d->curve_3_button_2d, 0 );
                if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                    fl_set_object_helper( GUI.run_form_2d->curve_3_button_2d,
                                          "Show curve 3" );
                break;

            case 3 :
                fl_set_button( GUI.run_form_2d->curve_4_button_2d, 0 );
                if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                    fl_set_object_helper( GUI.run_form_2d->curve_4_button_2d,
                                          "Show curve 4" );
                break;
        }

        char hstr[ 128 ];
        sprintf( hstr, "Hide curve %ld", data );
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( obj, hstr );

        G_2d.active_curve = ( int ) ( data - 1 );
        G_2d.curve_2d[ G_2d.active_curve ]->active = true;

        fl_set_button( GUI.run_form_2d->full_scale_button_2d,
                       G_2d.curve_2d[ G_2d.active_curve ]->is_fs );
    }

    redraw_all_2d( );
    cut_new_curve_handler( );
}


/*--------------------------------------------------------------*
 * Function for dealing with X events for the different display
 * windows in order to keep track which one currently has the
 * focus (required for the function for determining the mouse
 * position).
 *--------------------------------------------------------------*/

int
form_event_handler( FL_FORM * form,
                    void    * xevent )
{
    if ( ( ( XEvent * ) xevent )->type == FocusIn )
    {
        if ( G.dim & 1 && form == GUI.run_form_1d->run_1d )
            G.focus |= WINDOW_1D;
        else if ( G.dim & 2 )
        {
            if ( form == GUI.run_form_2d->run_2d )
                G.focus |= WINDOW_2D;
            else if ( form == GUI.cut_form->cut )
                G.focus |= WINDOW_CUT;
        }
    }
    else if ( ( ( XEvent * ) xevent )->type == FocusOut )
    {
        if ( G.dim & 1 && form == GUI.run_form_1d->run_1d )
            G.focus &= ~ WINDOW_1D;
        else if ( G.dim & 2 ) {
            if ( form == GUI.run_form_2d->run_2d )
                G.focus &= ~ WINDOW_2D;
            else if ( form == GUI.cut_form->cut )
                G.focus &= ~ WINDOW_CUT;
        }
    }

    return 0;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

int
text_width( const char * txt )
{

    XGlyphInfo extents;


    XftTextExtentsUtf8( G.d, G.font, ( XftChar8 * ) txt, strlen( txt ),
                        &extents );

    return extents.xOff;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
