/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
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


Graphics_1d_T G_1d;


static void press_handler_1d( FL_OBJECT * obj,
                              Window      window,
                              XEvent    * ev,
                              Canvas_T  * c );
static void release_handler_1d( FL_OBJECT * obj,
                                Window      window,
                                XEvent    * ev,
                                Canvas_T  * c );
static void motion_handler_1d( FL_OBJECT * obj,
                               Window      window,
                               XEvent    * ev,
                               Canvas_T  * c );
static bool change_x_range_1d( Canvas_T * c );
static bool change_y_range_1d( Canvas_T * c );
static bool change_xy_range_1d( Canvas_T * c );
static bool zoom_x_1d( Canvas_T * c );
static bool zoom_y_1d( Canvas_T * c );
static bool zoom_xy_1d( Canvas_T * c );
static bool shift_XPoints_of_curve_1d( Canvas_T   * c,
                                       Curve_1d_T * cv );
static void reconfigure_window_1d( Canvas_T * c,
                                   int        w,
                                   int        h );
static void recalc_XPoints_1d( void );
static void delete_marker_1d( long x_pos );


/*--------------------------------------------------------*
 * Handler for all kinds of X events the canvas receives.
 *--------------------------------------------------------*/

int
canvas_handler_1d( FL_OBJECT * obj,
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
                repaint_canvas_1d( c );
            break;

        case ConfigureNotify :
            if ( ( int ) c->w != w || ( int ) c->h != h )
            	reconfigure_window_1d( c, w, h );
            break;

        case ButtonPress :
            press_handler_1d( obj, window, ev, c );
            break;

        case ButtonRelease :
            release_handler_1d( obj, window, ev, c );
            break;

        case MotionNotify :
            motion_handler_1d( obj, window, ev, c );
            break;
    }

    return 1;
}


/*--------------------------------------------------------*
 * Handler that gets called for pressing one of the mouse
 * buttons in the axis areas or the camvas
 *--------------------------------------------------------*/

static void
press_handler_1d( FL_OBJECT * obj,
                  Window      window,
                  XEvent    * ev,
                  Canvas_T  * c )
{
    long i;
    Curve_1d_T *cv;
    int old_button_state = G.button_state;
    unsigned int keymask;
    bool active = UNSET;


    /* In the axis areas two buttons getting pressed simultaneously doesn't
       have a special meaning, so don't care about another button. Also don't
       react if the pressed buttons have lost there meaning */

    if (    ( c != &G_1d.canvas && G.raw_button_state != 0 )
         || ( G.button_state == 0 && G.raw_button_state != 0 ) )
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

    if ( obj == GUI.run_form_1d->x_axis_1d )        /* in x-axis window */
        G.drag_canvas = DRAG_1D_X;
    if ( obj == GUI.run_form_1d->y_axis_1d )        /* in y-axis window */
        G.drag_canvas = DRAG_1D_Y;
    if ( obj == GUI.run_form_1d->canvas_1d )        /* in canvas window */
        G.drag_canvas = DRAG_1D_C;

    fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

    for ( i = 0; i < G_1d.nc; i++ )
        active |= G_1d.curve[ i ]->active;

    switch ( G.button_state )
    {
        case 1 :                               /* left button */
            if ( ! active )
                break;

            fl_set_cursor( window, G_1d.cursor[ ZOOM_BOX_CURSOR ] );

            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];

            /* Set up variables for drawing the rubber boxes */

            switch ( G.drag_canvas )
            {
                case DRAG_1D_X :                       /* in x-axis window */
                    c->box_x = c->ppos[ X ];
                    c->box_w = 0;
                    c->box_y = G.x_scale_offset + 1;
                    c->box_h = G.enlarge_box_width;
                    c->is_box = SET;
                    break;

                case DRAG_1D_Y :                       /* in y-axis window */
                    c->box_x = c->w -
                               ( G.y_scale_offset + G.enlarge_box_width + 1 );
                    c->box_y = c->ppos[ Y ];
                    c->box_w = 5;
                    c->box_h = 0;
                    c->is_box = SET;
                    break;

                case DRAG_1D_C :                       /* in canvas window */
                    c->box_x = c->ppos[ X ];
                    c->box_y = c->ppos[ Y ];
                    c->box_w = c->box_h = 0;
                    c->is_box = SET;
                    break;
            }

            repaint_canvas_1d( c );
            break;

        case 2 :                               /* middle button */
            if ( ! active )
                break;

            fl_set_cursor( window, G_1d.cursor[ MOVE_HAND_CURSOR ] );

            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];

            /* Store data for undo operation */

            for ( i = 0; i < G_1d.nc; i++ )
            {
                cv = G_1d.curve[ i ];

                if ( cv->active )
                    save_scale_state_1d( cv );
                else
                    cv->can_undo = UNSET;
            }
            break;

        case 3:                                /* left and middle button */
            fl_set_cursor( window, G_1d.cursor[ CROSSHAIR_CURSOR ] );
            G.coord_display = 1;

            /* Don't draw the box anymore */

            G_1d.canvas.is_box = UNSET;
            repaint_canvas_1d( &G_1d.canvas );
            break;

        case 4 :                               /* right button */
            if ( ! active )
                break;

            fl_set_cursor( window, G_1d.cursor[ ZOOM_LENS_CURSOR ] );

            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];
            break;

        case 5 :                               /* left and right button */
            fl_set_cursor( window, G_1d.cursor[ TARGET_CURSOR ] );
            G.dist_display = 1;

            if ( G_1d.canvas.is_box == UNSET && old_button_state != 4 )
            {
                G.start[ X ] = c->ppos[ X ];
                G.start[ Y ] = c->ppos[ Y ];
            }
            else
                G_1d.canvas.is_box = UNSET;

            repaint_canvas_1d( &G_1d.canvas );
            break;

        case 8 : case 16 :                               /* button 4/5/wheel */
            if ( ! active )
                break;

            if ( G.drag_canvas != DRAG_1D_X && G.drag_canvas != DRAG_1D_Y )
                break;

            fl_set_cursor( window, G_1d.cursor[ MOVE_HAND_CURSOR ] );

            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];

            /* Store data for undo operation */

            for ( i = 0; i < G_1d.nc; i++ )
            {
                cv = G_1d.curve[ i ];

                if ( cv->active )
                    save_scale_state_1d( cv );
                else
                    cv->can_undo = UNSET;
            }
            break;
    }
}


/*---------------------------------------------------------*
 * Handler that gets called for releasing one of the mouse
 * buttons in the axis areas or the camvas
 *---------------------------------------------------------*/

static void
release_handler_1d( FL_OBJECT * obj  UNUSED_ARG,
                    Window      window,
                    XEvent    * ev,
                    Canvas_T  * c )
{
    unsigned int keymask;
    bool scale_changed = UNSET;
    long i;
    Curve_1d_T *cv;
    bool active = UNSET;


    /* If the released button didn't has a meaning just remove it from the
       button state pattern */

    if ( ! ( ( 1 << ( ev->xbutton.button - 1 ) ) & G.button_state ) )
    {
        G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );
        return;
    }

    /* Get mouse position and restrict it to the canvas */

    fl_get_win_mouse( window, &c->ppos[ X ], &c->ppos[ Y ], &keymask );

    if ( c->ppos[ X ] < 0 )
        c->ppos[ X ] = 0;
    if ( c->ppos[ X ] >= ( int ) G_1d.canvas.w )
        c->ppos[ X ] = G_1d.canvas.w - 1;

    if ( c->ppos[ Y ] < 0 )
        c->ppos[ Y ] = 0;
    if ( c->ppos[ Y ] >= ( int ) G_1d.canvas.h )
        c->ppos[ Y ] = G_1d.canvas.h - 1;

    for ( i = 0; i < G_1d.nc; i++ )
        active |= G_1d.curve[ i ]->active;

    switch ( G.button_state )
    {
        case 1 :                               /* left mouse button */
            if ( ! active )
                break;

            switch ( G.drag_canvas )
            {
                case DRAG_1D_X :                       /* x-axis window */
                    if ( ( scale_changed = change_x_range_1d( c ) ) )
                        redraw_canvas_1d( &G_1d.x_axis );
                    break;

                case DRAG_1D_Y :                       /* in y-axis window */
                    if ( ( scale_changed = change_y_range_1d( c ) ) )
                        redraw_canvas_1d( &G_1d.y_axis );
                    break;

                case DRAG_1D_C :                       /* in canvas window */
                    if ( ( scale_changed = change_xy_range_1d( c ) ) )
                    {
                        redraw_canvas_1d( &G_1d.x_axis );
                        redraw_canvas_1d( &G_1d.y_axis );
                    }
                    break;
            }
            c->is_box = UNSET;
            break;

        case 2 :                               /* middle mouse button */
            if ( ! active || G.drag_canvas > DRAG_1D_C )
                break;

            if ( G.drag_canvas & DRAG_1D_X )
                redraw_canvas_1d( &G_1d.x_axis );
            if ( G.drag_canvas & DRAG_1D_Y )
                redraw_canvas_1d( &G_1d.y_axis );
            break;

        case 4 :                               /* right mouse button */
            if ( ! active )
                break;

            switch ( G.drag_canvas )
            {
                case DRAG_1D_X :                       /* in x-axis window */
                    if ( ( scale_changed = zoom_x_1d( c ) ) )
                        redraw_canvas_1d( &G_1d.x_axis );
                    break;

                case DRAG_1D_Y :                       /* in y-axis window */
                    if ( ( scale_changed = zoom_y_1d( c ) ) )
                        redraw_canvas_1d( &G_1d.y_axis );
                    break;

                case DRAG_1D_C :                       /* in canvas window */
                    if ( ( scale_changed = zoom_xy_1d( c ) ) )
                    {
                        redraw_canvas_1d( &G_1d.x_axis );
                        redraw_canvas_1d( &G_1d.y_axis );
                    }
                    break;
            }
            break;

        case 8 :                               /* button 4/wheel up */
            if ( ! active )
                break;

            if ( G.drag_canvas == DRAG_1D_X )
            {
                G_1d.x_axis.ppos[ X ] = G.start[ X ] - G_1d.x_axis.w / 10;
                for ( i = 0; i < G_1d.nc; i++ )
                {
                    cv = G_1d.curve[ i ];

                    if ( ! cv->active )
                        continue;

                    /* Recalculate the offsets and shift curves in the
                       canvas */

                    if ( ( scale_changed =
                              shift_XPoints_of_curve_1d( &G_1d.x_axis, cv ) ) )
                        redraw_canvas_1d( &G_1d.x_axis );
                }
            }

            if ( G.drag_canvas == DRAG_1D_Y )
            {
                G_1d.y_axis.ppos[ Y ] = G.start[ Y ] + G_1d.y_axis.h / 10;
                for ( i = 0; i < G_1d.nc; i++ )
                {
                    cv = G_1d.curve[ i ];

                    if ( ! cv->active )
                        continue;

                    /* Recalculate the offsets and shift curves in the
                       canvas */

                    if ( ( scale_changed =
                              shift_XPoints_of_curve_1d( &G_1d.y_axis, cv ) ) )
                        redraw_canvas_1d( &G_1d.y_axis );
                }
            }
            break;

        case 16 :                              /* button 5/wheel down */
            if ( ! active )
                break;

            if ( G.drag_canvas == DRAG_1D_X )
            {
                G_1d.x_axis.ppos[ X ] = G.start[ X ] + G_1d.x_axis.w / 10;
                for ( i = 0; i < G_1d.nc; i++ )
                {
                    cv = G_1d.curve[ i ];

                    if ( ! cv->active )
                        continue;

                    /* Recalculate the offsets and shift curves in the
                       canvas */

                    if ( ( scale_changed =
                              shift_XPoints_of_curve_1d( &G_1d.x_axis, cv ) ) )
                        redraw_canvas_1d( &G_1d.x_axis );
                }
            }

            if ( G.drag_canvas == DRAG_1D_Y )
            {
                G_1d.y_axis.ppos[ Y ] = G.start[ Y ] - G_1d.y_axis.h / 10;
                for ( i = 0; i < G_1d.nc; i++ )
                {
                    cv = G_1d.curve[ i ];

                    if ( ! cv->active )
                        continue;

                    /* Recalculate the offsets and shift curves in the
                       canvas */

                    if ( ( scale_changed =
                              shift_XPoints_of_curve_1d( &G_1d.y_axis, cv ) ) )
                        redraw_canvas_1d( &G_1d.y_axis );
                }
            }
            break;
    }

    G.button_state = 0;
    G.raw_button_state &= ~ ( 1 << ( ev->xbutton.button - 1 ) );
    G.drag_canvas = DRAG_NONE;
    G.coord_display &= ~ 1;
    G.dist_display  &= ~ 1;

    fl_reset_cursor( window );

    if ( scale_changed )
    {
        if ( G_1d.is_fs )
        {
            G_1d.is_fs = UNSET;
            fl_set_button( GUI.run_form_1d->full_scale_button_1d, 0 );
            if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
                                      "Rescale curves to fit into the window\n"
                                      "and switch on automatic rescaling" );
        }

        redraw_canvas_1d( &G_1d.canvas );
    }

    if ( ! scale_changed || c != &G_1d.canvas )
        repaint_canvas_1d( c );
}


/*------------------------------------------------------------*
 * Handler that gets called for moving the mouse after one of
 * the buttons was pressed in the axis areas or the camvas
 *------------------------------------------------------------*/

static void
motion_handler_1d( FL_OBJECT * obj  UNUSED_ARG,
                   Window      window,
                   XEvent *    ev,
                   Canvas_T *  c )
{
    Curve_1d_T *cv;
    XEvent new_ev;
    long i;
    unsigned int keymask;
    bool scale_changed = UNSET;
    bool active = UNSET;


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

    for ( i = 0; i < G_1d.nc; i++ )
        active |= G_1d.curve[ i ]->active;

    switch ( G.button_state )
    {
        case 1 :                               /* left mouse button */
            if ( ! active )
                break;

            if ( G.drag_canvas <= DRAG_1D_C )
            {
                if ( G.drag_canvas & DRAG_1D_X )  /* x-axis or canvas window */
                {
                    c->box_w = c->ppos[ X ] - G.start[ X ];

                    if ( c->box_x + c->box_w >= ( int ) c->w )
                        c->box_w = c->w - c->box_x - 1;

                    if ( c->box_x + c->box_w < 0 )
                        c->box_w = - c->box_x;
                }

                if ( G.drag_canvas & DRAG_1D_Y )  /* y-axis or canvas window */
                {
                    c->box_h = c->ppos[ Y ] - G.start[ Y ];

                    if ( c->box_y + c->box_h >= ( int ) c->h )
                        c->box_h = c->h - c->box_y - 1;

                    if ( c->box_y + c->box_h < 0 )
                        c->box_h = - c->box_y;
                }
            }

            repaint_canvas_1d( c );
            break;

        case 2 :                               /* middle button */
            if ( ! active )
                break;

            for ( i = 0; i < G_1d.nc; i++ )
            {
                cv = G_1d.curve[ i ];

                if ( ! cv->active || G.drag_canvas > DRAG_1D_C )
                    continue;

                /* Recalculate the offsets and shift curves in the canvas */

                scale_changed = shift_XPoints_of_curve_1d( c, cv );
            }

            G.start[ X ] = c->ppos[ X ];
            G.start[ Y ] = c->ppos[ Y ];

            /* Switch off full scale button if necessary */

            if ( G_1d.is_fs && scale_changed )
            {
                G_1d.is_fs = UNSET;
                fl_set_button( GUI.run_form_1d->full_scale_button_1d, 0 );
                if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
                    fl_set_object_helper(
                                      GUI.run_form_1d->full_scale_button_1d,
                                      "Rescale curves to fit into the window\n"
                                      "and switch on automatic rescaling" );
            }

            redraw_canvas_1d( &G_1d.canvas );
            if ( G.drag_canvas > DRAG_1D_C )
                break;

            if ( G.drag_canvas & DRAG_1D_X )
                redraw_canvas_1d( &G_1d.x_axis );
            if ( G.drag_canvas & DRAG_1D_Y )
                redraw_canvas_1d( &G_1d.y_axis );
            break;

        case 3 : case 5 :               /* left and (middle or right) button */
            repaint_canvas_1d( &G_1d.canvas );
            break;
    }
}


/*----------------------------------------------------------*
 * Saves the current state of scaling before changes to the
 * scaling are applied - needed for the UNDO button.
 *----------------------------------------------------------*/

void
save_scale_state_1d( Curve_1d_T * cv )
{
    cv->old_s2d[ X ] = cv->s2d[ X ];
    cv->old_s2d[ Y ] = cv->s2d[ Y ];
    cv->old_shift[ X ] = cv->shift[ X ];
    cv->old_shift[ Y ] = cv->shift[ Y ];

    cv->can_undo = SET;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
change_x_range_1d( Canvas_T * c )
{
    long i;
    bool scale_changed = UNSET;
    Curve_1d_T *cv;
    double x1, x2;


    if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 )
        return UNSET;

    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        if ( ! cv->active )
            continue;

        save_scale_state_1d( cv );

        x1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
        x2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

        cv->shift[ X ] = - d_min( x1, x2 );
        cv->s2d[ X ] = ( G_1d.canvas.w - 1 ) / fabs( x1 - x2 );

        recalc_XPoints_of_curve_1d( cv );
        scale_changed = SET;
    }

    return scale_changed;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
change_y_range_1d( Canvas_T * c )
{
    long i;
    bool scale_changed = UNSET;
    Curve_1d_T *cv;
    double cy1, cy2;


    if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 )
        return UNSET;

    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        if ( ! cv->active )
            continue;

        save_scale_state_1d( cv );

        cy1 = ( G_1d.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
              - cv->shift[ Y ];
        cy2 = ( G_1d.canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
              - cv->shift[ Y ];

        cv->shift[ Y ] = - d_min( cy1, cy2 );
        cv->s2d[ Y ] = ( G_1d.canvas.h - 1 ) / fabs( cy1 - cy2 );

        recalc_XPoints_of_curve_1d( cv );
        scale_changed = SET;
    }

    return scale_changed;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

bool
user_zoom_1d( double x,
              bool   keep_x,
              double xw,
              bool   keep_xw,
              double y,
              bool   keep_y,
              double yw,
              bool   keep_yw )
{
    long i;
    Curve_1d_T *cv;


    if ( ! G_1d.is_scale_set )
        return FAIL;

    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        if ( ! cv->active )
            continue;

        save_scale_state_1d( cv );
        cv->can_undo = SET;

        if ( ! keep_x )
            cv->shift[ X ] = - x;

        if ( ! keep_xw )
            cv->s2d[ X ] = ( G_1d.canvas.w - 1 ) / --xw;

        if ( ! keep_y )
            cv->shift[ Y ] = ( G_1d.rw_min - y ) / G_1d.rwc_delta[ Y ];

        if ( ! keep_yw )
            cv->s2d[ Y ] = G_1d.rwc_delta[ Y ] * ( G_1d.canvas.h - 1 ) / yw;

        recalc_XPoints_of_curve_1d( cv );
    }

    if ( G_1d.is_fs )
    {
        G_1d.is_fs = UNSET;
        fl_set_button( GUI.run_form_1d->full_scale_button_1d, 0 );
        if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
            fl_set_object_helper( GUI.run_form_1d->full_scale_button_1d,
                                  "Rescale curves to fit into the window\n"
                                  "and switch on automatic rescaling" );
    }

    redraw_canvas_1d( &G_1d.x_axis );
    redraw_canvas_1d( &G_1d.y_axis );
    redraw_canvas_1d( &G_1d.canvas );

    return OK;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
change_xy_range_1d( Canvas_T * c )
{
    long i;
    bool scale_changed = UNSET;
    Curve_1d_T *cv;
    double cx1, cx2, cy1, cy2;


    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        if ( ! cv->active )
            continue;

        save_scale_state_1d( cv );
        cv->can_undo = UNSET;

        if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
        {
            cv->can_undo = SET;

            cx1 = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];
            cx2 = c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ];

            cv->shift[ X ] = - d_min( cx1, cx2 );
            cv->s2d[ X ] = ( G_1d.canvas.w - 1 ) / fabs( cx1 - cx2 );

            scale_changed = SET;
        }

        if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
        {
            cv->can_undo = SET;

            cy1 = ( G_1d.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
                  - cv->shift[ Y ];
            cy2 = ( G_1d.canvas.h - 1.0 - c->ppos[ Y ] ) / cv->s2d[ Y ]
                  - cv->shift[ Y ];

            cv->shift[ Y ] = - d_min( cy1, cy2 );
            cv->s2d[ Y ] = ( G_1d.canvas.h - 1 ) / fabs( cy1 - cy2 );

            scale_changed = SET;
        }

        if ( scale_changed )
            recalc_XPoints_of_curve_1d( cv );
    }

    return scale_changed;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
zoom_x_1d( Canvas_T * c )
{
    long i;
    bool scale_changed = UNSET;
    Curve_1d_T *cv;
    double px;


    if ( abs( G.start[ X ] - c->ppos[ X ] ) <= 4 )
        return UNSET;

    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        if ( ! cv->active )
            continue;

        save_scale_state_1d( cv );

        px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

        /* If the mouse was moved to lower values zoom the display by a factor
           of up to 4 (if the mouse was moved over the whole length of the
           scale) while keeping the point the move was started at the same
           position. If the mouse was moved upwards demagnify by the inverse
           factor. */

        if ( G.start[ X ] > c->ppos[ X ] )
            cv->s2d[ X ] *= 3 * d_min( 1.0,
                                     ( double ) ( G.start[ X ] - c->ppos[ X ] )
                                     / G_1d.x_axis.w ) + 1;
        else
            cv->s2d[ X ] /= 3 * d_min( 1.0,
                                     ( double ) ( c->ppos[ X ] - G.start[ X ] )
                                     / G_1d.x_axis.w ) + 1;

        cv->shift[ X ] = G.start[ X ] / cv->s2d[ X ] - px;

        recalc_XPoints_of_curve_1d( cv );
        scale_changed = SET;
    }

    return scale_changed;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
zoom_y_1d( Canvas_T * c )
{
    long i;
    bool scale_changed = UNSET;
    Curve_1d_T *cv;
    double py;


    if ( abs( G.start[ Y ] - c->ppos[ Y ] ) <= 4 )
        return UNSET;

    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        if ( ! cv->active )
            continue;

        save_scale_state_1d( cv );

        /* Get the value in the interval [0, 1] corresponding to the mouse
           position */

        py = ( G_1d.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
             - cv->shift[ Y ];

        /* If the mouse was moved to lower values zoom the display by a factor
           of up to 4 (if the mouse was moved over the whole length of the
           scale) while keeping the point the move was started at the same
           position. If the mouse was moved upwards demagnify by the inverse
           factor. */

        if ( G.start[ Y ] < c->ppos[ Y ] )
            cv->s2d[ Y ] *= 3 * d_min( 1.0,
                                     ( double ) ( c->ppos[ Y ] - G.start[ Y ] )
                                     / G_1d.y_axis.h ) + 1;
        else
            cv->s2d[ Y ] /= 3 * d_min( 1.0,
                                     ( double ) ( G.start[ Y ] - c->ppos[ Y ] )
                                     / G_1d.y_axis.h ) + 1;

        cv->shift[ Y ] = ( G_1d.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
                         - py;

        recalc_XPoints_of_curve_1d( cv );
        scale_changed = SET;
    }

    return scale_changed;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

static bool
zoom_xy_1d( Canvas_T * c )
{
    long i;
    bool scale_changed = UNSET;
    Curve_1d_T *cv;
    double px, py;


    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        if ( ! cv->active )
            continue;

        save_scale_state_1d( cv );
        cv->can_undo = UNSET;

        if ( abs( G.start[ X ] - c->ppos[ X ] ) > 4 )
        {
            cv->can_undo = SET;

            px = G.start[ X ] / cv->s2d[ X ] - cv->shift[ X ];

            if ( G.start[ X ] > c->ppos[ X ] )
                cv->s2d[ X ] *= 3 * d_min( 1.0,
                                     ( double ) ( G.start[ X ] - c->ppos[ X ] )
                                     / G_1d.x_axis.w ) + 1;
            else
                cv->s2d[ X ] /= 3 * d_min( 1.0,
                                     ( double ) ( c->ppos[ X ] - G.start[ X ] )
                                     / G_1d.x_axis.w ) + 1;

            cv->shift[ X ] = G.start[ X ] / cv->s2d[ X ] - px;

            scale_changed = SET;
        }

        if ( abs( G.start[ Y ] - c->ppos[ Y ] ) > 4 )
        {
            cv->can_undo = SET;

            py = ( G_1d.canvas.h - 1.0 - G.start[ Y ] ) / cv->s2d[ Y ]
                 - cv->shift[ Y ];

            if ( G.start[ Y ] < c->ppos[ Y ] )
                cv->s2d[ Y ] *= 3 * d_min( 1.0,
                                     ( double ) ( c->ppos[ Y ] - G.start[ Y ] )
                                     / G_1d.y_axis.h ) + 1;
            else
                cv->s2d[ Y ] /= 3 * d_min( 1.0,
                                     ( double ) ( G.start[ Y ] - c->ppos[ Y ] )
                                     / G_1d.y_axis.h ) + 1;

            cv->shift[ Y ] = ( G_1d.canvas.h - 1.0 - G.start[ Y ] )
                             / cv->s2d[ Y ] - py;

            scale_changed = SET;
        }

        if ( scale_changed )
            recalc_XPoints_of_curve_1d( cv );
    }

    return scale_changed;
}


/*-----------------------------------------------------------------------*
 * This is basically a simplified version of 'recalc_XPoints_of_curve()'
 * because we need to do much less calculations, i.e. just adding an
 * offset to all XPoints instead of going through all the scalings...
 *-----------------------------------------------------------------------*/

static bool
shift_XPoints_of_curve_1d( Canvas_T   * c,
                           Curve_1d_T * cv )
{
    long j, k;
    int dx = 0,
        dy = 0;
    int factor;


    cv->up = cv->down = cv->left = cv->right = UNSET;

    /* Additionally pressing the right mouse button increases the amount the
       curves are shifted by a factor of 5 */

    factor = G.raw_button_state == 6 ? 5 : 1;

    /* Calculate scaled shift factors */

    if ( G.drag_canvas <= DRAG_1D_C )
    {
        if ( G.drag_canvas & DRAG_1D_X )          /* x-axis or canvas window */
        {
            dx = factor * ( c->ppos[ X ] - G.start[ X ] );
            cv->shift[ X ] += dx / cv->s2d[ X ];
        }

        if ( G.drag_canvas & DRAG_1D_Y )          /* y-axis or canvas window */
        {
            dy = factor * ( c->ppos[ Y ] - G.start[ Y ] );
            cv->shift[ Y ] -= dy / cv->s2d[ Y ];
        }
    }

    /* Add the shifts to the XPoints */

    for ( k = 0, j = 0; j < G_1d.nx; j++ )
    {
        if ( cv->points[ j ].exist )
        {
            cv->xpoints[ k ].x = i2s15( cv->xpoints[ k ].x + dx );
            cv->xpoints[ k ].y = i2s15( cv->xpoints[ k ].y + dy );

            if ( cv->xpoints[ k ].x < 0 )
                cv->left = SET;
            if ( cv->xpoints[ k ].x >= ( int ) G_1d.canvas.w )
                cv->right = SET;
            if ( cv->xpoints[ k ].y < 0 )
                cv->up = SET;
            if ( cv->xpoints[ k ].y >= ( int ) G_1d.canvas.h )
                cv->down = SET;

            k++;
        }
    }

    return SET;
}


/*-------------------------------------*
 * Handles changes of the window size.
 *-------------------------------------*/

static void
reconfigure_window_1d( Canvas_T * c,
                       int        w,
                       int        h )
{
    long i;
    Curve_1d_T *cv;
    static bool is_reconf[ 2 ] = { UNSET, UNSET };
    static bool need_redraw[ 2 ] = { UNSET, UNSET };
    int old_w = c->w,
        old_h = c->h;


    /* Set the new canvas sizes */

    c->w = i2u15( w );
    c->h = i2u15( h );

    /* Calculate the new scale factors */

    if ( c == &G_1d.canvas && G.is_init )
    {
        if ( G_1d.is_scale_set )
        {
            for ( i = 0; i < G_1d.nc; i++ )
            {
                cv = G_1d.curve[ i ];

                cv->s2d[ X ] *= ( w - 1.0 ) / ( old_w - 1 );
                cv->s2d[ Y ] *= ( h - 1.0 ) / ( old_h - 1 );

                if ( cv->can_undo )
                {
                    cv->old_s2d[ X ] *= ( w - 1.0 ) / ( old_w - 1 );
                    cv->old_s2d[ Y ] *= ( h - 1.0 ) / ( old_h - 1 );
                }
            }

            /* Recalculate data for drawing (has to be done after setting of
               canvas sizes since they are needed in the recalculation) */

            recalc_XPoints_1d( );
        }
        else
            for ( i = 0; i < G_1d.nc; i++ )
            {
                G_1d.curve[ i ]->s2d[ X ] *= ( w - 1.0 ) / ( old_w - 1 );
                G_1d.curve[ i ]->s2d[ Y ] = h - 1;
            }
    }

    /* We can't know the sequence the different canvases are reconfigured
       but redrawing an axis canvases is useless before the new scaling
       factors are set. Thus we need in the call for the canvas window to
       redraw also axis windows which got reconfigured before. */

    XftDrawChange( c->xftdraw, None );
    delete_pixmap( c );
    create_pixmap( c );
    XftDrawChange( c->xftdraw, c->pm );

    if ( c == &G_1d.canvas )
    {
        redraw_canvas_1d( c );

        if ( need_redraw[ X ] )
        {
            redraw_canvas_1d( &G_1d.x_axis );
            need_redraw[ X ] = UNSET;
        }
        else if ( w != old_w )
            is_reconf[ X ] = SET;

        if ( need_redraw[ Y ] )
        {
            redraw_canvas_1d( &G_1d.y_axis );
            need_redraw[ Y ] = UNSET;
        }
        else if ( h != old_h )
            is_reconf[ Y ] = SET;
    }

    if ( c == &G_1d.x_axis )
    {
        if ( is_reconf[ X ] )
        {
            redraw_canvas_1d( c );
            is_reconf[ X ] = UNSET;
        }
        else
            need_redraw[ X ] = SET;
    }

    if ( c == &G_1d.y_axis )
    {
        if ( is_reconf[ Y ] )
        {
            redraw_canvas_1d( c );
            is_reconf[ Y ] = UNSET;
        }
        else
            need_redraw[ Y ] = SET;
    }
}


/*---------------------------------------------------------*
 * Recalculates the points to be displayed from the scaled
 * points for all curves.
 *---------------------------------------------------------*/

static void
recalc_XPoints_1d( void )
{
    long i;


    for ( i = 0; i < G_1d.nc; i++ )
        recalc_XPoints_of_curve_1d( G_1d.curve[ i ] );
}


/*----------------------------------------------------------------*
 * Recalculates the graphic data for a curve using the the curves
 * settings for the scale and the offset.
 *----------------------------------------------------------------*/

void
recalc_XPoints_of_curve_1d( Curve_1d_T * cv )
{
    long j;
    Scaled_Point_T *sp = cv->points;
    XPoint *xp = cv->xpoints;


    cv->up = cv->down = cv->left = cv->right = UNSET;

    for ( j = 0; j < G_1d.nx; sp++, j++ )
    {
        if ( ! sp->exist )
            continue;

        xp->x = s15rnd( cv->s2d[ X ] * ( j + cv->shift[ X ] ) );
        xp->y = s15rnd( G_1d.canvas.h - 1 - cv->s2d[ Y ]
                        * ( cv->points[ j ].v + cv->shift[ Y ] ) );

        cv->left  |= xp->x < 0;
        cv->right |= xp->x >= ( int ) G_1d.canvas.w;
        cv->up    |= xp->y < 0;
        cv->down  |= xp->y >= ( int ) G_1d.canvas.h;

        xp++;
    }
}


/*-----------------------------------------*
 * Does a complete redraw of all canvases.
 *-----------------------------------------*/

void
redraw_all_1d( void )
{
    redraw_canvas_1d( &G_1d.canvas );
    redraw_canvas_1d( &G_1d.x_axis );
    redraw_canvas_1d( &G_1d.y_axis );
}


/*-------------------------------------*
 * Does a complete redraw of a canvas.
 *-------------------------------------*/

void
redraw_canvas_1d( Canvas_T * c )
{
    long i;
    Curve_1d_T *cv = NULL;
    Marker_1d_T *m;
    short x;


    XFillRectangle( G.d, c->pm, c->gc, 0, 0, c->w, c->h );

    if ( G.is_init )
    {
        if ( c == &G_1d.canvas && G_1d.is_scale_set )
        {
            /* First draw the marker - to get the correct scaling one of the
               curves must be active */

            for ( i = 0; i < G_1d.nc; i++ )
                if ( G_1d.curve[ i ]->active )
                {
                    cv = G_1d.curve[ i ];
                    break;
                }

            if ( cv != NULL )
                for ( m = G_1d.marker_1d; m != NULL; m = m->next )
                {
                    x = s15rnd( cv->s2d[ X ]
                                * ( m->x_pos + cv->shift[ X ] ) );
                    XDrawLine( G.d, c->pm, m->gc, x, 0, x, c->h );
                }

            /* Now draw all curves */

            for ( i = G_1d.nc - 1 ; i >= 0; i-- )
            {
                cv = G_1d.curve[ i ];

                if ( cv->count <= 1 )
                    continue;

                XDrawLines( G.d, c->pm, cv->gc, cv->xpoints, cv->count,
                            CoordModeOrigin );
            }

            /* Finally draw the out of range arrows */

            for ( i = 0 ; i < G_1d.nc; i++ )
            {
                cv = G_1d.curve[ i ];

                if ( cv->count <= 1 )
                    continue;

                if ( cv->up )
                    XCopyArea( G.d, cv->up_arrow, c->pm, c->gc,
                               0, 0, G.up_arrow_w, G.up_arrow_h,
                               G_1d.canvas.w / 2 - 32 + 16 * i, 5 );

                if ( cv->down )
                    XCopyArea( G.d, cv->down_arrow, c->pm, c->gc,
                               0, 0, G.down_arrow_w, G.down_arrow_h,
                               G_1d.canvas.w / 2 - 32 + 16 * i,
                               G_1d.canvas.h - 5 - G.down_arrow_h );

                if ( cv->left )
                    XCopyArea( G.d, cv->left_arrow, c->pm, c->gc,
                               0, 0, G.left_arrow_w, G.left_arrow_h,
                               5, G_1d.canvas.h / 2 -32 + 16 * i );

                if ( cv->right )
                    XCopyArea( G.d, cv->right_arrow, c->pm, c->gc,
                               0, 0, G.right_arrow_w, G.right_arrow_h,
                               G_1d.canvas.w - 5 - G.right_arrow_w,
                               G_1d.canvas.h / 2 - 32 + 16 * i );
            }
        }

        if ( c == &G_1d.x_axis )
            redraw_axis_1d( X );

        if ( c == &G_1d.y_axis )
            redraw_axis_1d( Y );
    }

    /* Finally copy the pixmap onto the screen */

    repaint_canvas_1d( c );
}


/*-----------------------------------------------*
 * Copies the background pixmap onto the canvas.
 *-----------------------------------------------*/

void
repaint_canvas_1d( Canvas_T * c )
{
    long i;
    char buf[ 256 ];
    int x, y;
    unsigned int w, h;
    Curve_1d_T *cv;
    double x_pos, y_pos;
    Pixmap pm;


    /* If no or either the middle or the left button is pressed no extra stuff
       has to be drawn so just copy the pixmap with the curves into the
       window. Also in the case that the graphics was never initialised this
       is all to be done. */

    if ( ! ( G.button_state & 1 ) || ! G.is_init )
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

    /* Draw the rubber box if needed (i.e. when the left button pressed
       in the canvas currently to be drawn) */

    if ( G.button_state == 1 && c->is_box )
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

    /* If this is the canvas and the left and either the middle or the right
       mouse button is pressed draw the current mouse position (converted to
       real world coordinates) or the difference between the current position
       and the point the buttons were pressed at into the left hand top corner
       of the canvas. In the second case also draw some marker connecting the
       initial and the current position. */

    if ( c == &G_1d.canvas )
    {
        if ( G.coord_display == 1 )
        {
            for ( i = 0; i < G_1d.nc; i++ )
            {
                cv = G_1d.curve[ i ];

                x_pos = G_1d.rwc_start[ X ] + G_1d.rwc_delta[ X ]
                            * ( c->ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ] );
                y_pos = G_1d.rwc_start[ Y ] + G_1d.rwc_delta[ Y ]
                        * ( ( G_1d.canvas.h - 1.0 - c->ppos[ Y ] )
                            / cv->s2d[ Y ] - cv->shift[ Y ] );

                strcpy( buf, " x = " );
                make_label_string( buf + 5, x_pos,
                                irnd( floor( log10( fabs( G_1d.rwc_delta[ X ] )
                                                    / cv->s2d[ X ] ) ) - 2 ) );
                strcat( buf, "   y = " );
                make_label_string( buf + strlen( buf ), y_pos,
                            irnd( floor( log10( fabs( G_1d.rwc_delta[ Y ] )
                                                / cv->s2d[ Y ] ) ) - 2 ) );
                strcat( buf, " " );

                XFillRectangle( G.d, pm, c->axis_gc,
                                5, 5 + i * ( G.font_asc + G.font_desc + 4 ) - 2,
                                text_width( buf ),
                                G.font_asc + G.font_desc + 4 );
                XftDrawChange( c->xftdraw, pm );
                XftDrawStringUtf8( c->xftdraw, G.xftcolor + i, G.font,
                                   5,   ( G.font_asc + 2 ) * ( i + 1 )
                                      + G.font_desc * i + 2,
                                   ( XftChar8 const * ) buf, strlen( buf ) );
                XftDrawChange( c->xftdraw, c->pm );
            }
        }

        if ( G.dist_display == 1 )
        {
            for ( i = 0; i < G_1d.nc; i++ )
            {
                cv = G_1d.curve[ i ];

                x_pos = G_1d.rwc_delta[ X ] * ( c->ppos[ X ] - G.start[ X ] )
                        / cv->s2d[ X ];
                y_pos = G_1d.rwc_delta[ Y ] * ( G.start[ Y ] - c->ppos[ Y ] )
                        / cv->s2d[ Y ];
                sprintf( buf, " dx = %#g   dy = %#g ", x_pos, y_pos );

                XFillRectangle( G.d, pm, c->axis_gc,
                                5, 5 + i * ( G.font_asc + G.font_desc + 4 ) - 2,
                                text_width( buf ),
                                G.font_asc + G.font_desc + 4 );
                XftDrawChange( c->xftdraw, pm );
                XftDrawStringUtf8( c->xftdraw, G.xftcolor + i, G.font,
                                   5,   ( G.font_asc + 2 ) * ( i + 1 )
                                      + G.font_desc * i + 2,
                                   ( XftChar8 const * ) buf, strlen( buf ) );
                XftDrawChange( c->xftdraw, c->pm );
            }

            XDrawArc( G.d, pm, G_1d.curve[ 0 ]->gc,
                      G.start[ X ] - 5, G.start[ Y ] - 5, 10, 10, 0, 23040 );
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
 * Function for returning the current mouse position to the
 * user for the EDL function mouse_position()
 *----------------------------------------------------------*/

int
get_mouse_pos_1d( double       * pa,
                  unsigned int * keymask )
{
    Curve_1d_T *cv;
    long i;
    int ppos[ 2 ];


    fl_get_win_mouse( FL_ObjWin( G_1d.canvas.obj ),
                      ppos + X, ppos + Y, keymask );

    if (    ! G_1d.is_scale_set
         || ppos[ X ] < 0
         || ppos[ X ] > ( int ) G_1d.canvas.w - 1
         || ppos[ Y ] < 0
         || ppos[ Y ] > ( int ) G_1d.canvas.h - 1 )
        return 0;

    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        pa[ 2 * i + X ] = G_1d.rwc_start[ X ] + G_1d.rwc_delta[ X ]
                          * ( ppos[ X ] / cv->s2d[ X ] - cv->shift[ X ] );
        pa[ 2 * i + Y ] = G_1d.rwc_start[ Y ] + G_1d.rwc_delta[ Y ]
                          * ( ( G_1d.canvas.h - 1.0 - ppos[ Y ] ) /
                              cv->s2d[ Y ] - cv->shift[ Y ] );
    }

    return 1;
}


/*---------------------------------------------------------*
 * Does a rescale of the data for 1d graphics so that all
 * curves fit into the canvas and occupy the whole canvas.
 *---------------------------------------------------------*/

void
fs_rescale_1d( bool vert_only )
{
    long i, j;
    double min = 1.0,
           max = 0.0;
    double rw_min,
           rw_max;
    double data;
    double new_rwc_delta_y;
    Curve_1d_T *cv;


    if ( ! G_1d.is_scale_set )
        return;

    /* Find minimum and maximum value of all data */

    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        for ( j = 0; j < G_1d.nx; j++ )
            if ( cv->points[ j ].exist )
            {
                data = cv->points[ j ].v;
                max = d_max( data, max );
                min = d_min( data, min );
            }
    }

    /* If there are no points yet... */

    if ( min >= max )
    {
        G_1d.rw_min = HUGE_VAL;
        G_1d.rw_max = - HUGE_VAL;
        G_1d.is_scale_set = UNSET;
        return;
    }

    /* Calculate new real world maximum and minimum */

    rw_min = G_1d.rwc_delta[ Y ] * min + G_1d.rw_min;
    rw_max = G_1d.rwc_delta[ Y ] * max + G_1d.rw_min;

    /* Calculate new scaling factor and rescale the scaled data as well as the
       points for drawing */

    new_rwc_delta_y = rw_max - rw_min;

    for ( i = 0; i < G_1d.nc; i++ )
    {
        cv = G_1d.curve[ i ];

        cv->shift[ Y ] = 0.0;
        cv->s2d[ Y ] = G_1d.canvas.h - 1.0;

        if ( ! vert_only )
        {
            cv->shift[ X ] = 0.0;
            cv->s2d[ X ] = ( G_1d.canvas.w - 1.0 ) / ( G_1d.nx - 1 );
        }

        cv->up = cv->down = cv->left = cv->right = UNSET;

        for ( j = 0; j < G_1d.nx; j++ )
            if ( cv->points[ j ].exist )
                cv->points[ j ].v = ( G_1d.rwc_delta[ Y ] * cv->points[ j ].v
                                      + G_1d.rw_min - rw_min )
                                    / new_rwc_delta_y;

        recalc_XPoints_of_curve_1d( cv );
    }

    /* Store new minimum and maximum and the new scale factor */

    G_1d.rwc_delta[ Y ] = new_rwc_delta_y;
    G_1d.rw_min = G_1d.rwc_start[ Y ] = rw_min;
    G_1d.rw_max = rw_max;
}


/*---------------------------------------------------*
 * Function creates the axis scales for 1D displays.
 *---------------------------------------------------*/

void
make_scale_1d( Curve_1d_T * cv,
               Canvas_T   * c,
               int          coord )
{
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


    /* The distance between the smallest ticks should be 'G.scale_tick_dist'
       points - calculate the corresponding delta in real word units */

    rwc_delta = G.scale_tick_dist * fabs( G_1d.rwc_delta[ coord ] )
                / cv->s2d[ coord ];

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

    d_delta_fine = cv->s2d[ coord ] * rwc_delta
                   / fabs( G_1d.rwc_delta[ coord ] );

    /* 'rwc_start' is the first value in the display (i.e. the smallest x or y
       value still shown in the canvas), 'rwc_start_fine' the position of the
       first small tick (both in real world coordinates) and, finally,
       'd_start_fine' is the same position but in points */

    rwc_start = G_1d.rwc_start[ coord ]
                - cv->shift[ coord ] * G_1d.rwc_delta[ coord ];
    if ( G_1d.rwc_delta[ coord ] < 0 )
        rwc_delta *= -1.0;

    modf( rwc_start / rwc_delta, &rwc_start_fine );
    rwc_start_fine *= rwc_delta;

    d_start_fine = cv->s2d[ coord ]
                   * ( rwc_start_fine - rwc_start ) / G_1d.rwc_delta[ coord ];
    if ( lrnd( d_start_fine ) < 0 )
        d_start_fine += d_delta_fine;

    /* Calculate start index (in small tick counts) of first medium tick */

    modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
    rwc_start_medium *= medium_factor * rwc_delta;

    d_start_medium = cv->s2d[ coord ] * ( rwc_start_medium - rwc_start )
                     / G_1d.rwc_delta[ coord ];
    if ( lrnd( d_start_medium ) < 0 )
        d_start_medium += medium_factor * d_delta_fine;

    medium = irnd( ( d_start_fine - d_start_medium ) / d_delta_fine );

    /* Calculate start index (in small tick counts) of first large tick */

    modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
    rwc_start_coarse *= coarse_factor * rwc_delta;

    d_start_coarse = cv->s2d[ coord ] * ( rwc_start_coarse - rwc_start )
                     / G_1d.rwc_delta[ coord ];
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
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x, y - G.medium_tick_len );
            else                                       /* short line */
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x, y - G.short_tick_len );
        }
    }
    else
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
                                   y + G.font_asc / 2 ,
                                   ( XftChar8 const * ) lstr, strlen( lstr ) );
            }
            else if ( medium % medium_factor == 0 )    /* medium line */
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x + G.medium_tick_len, y );
            else                                      /* short line */
                XDrawLine( G.d, c->pm, c->axis_gc, x, y,
                           x + G.short_tick_len, y );
        }
    }
}


/*-------------------------------------------*
 * Gets called to create a marker at 'x_pos'
 *-------------------------------------------*/

void
set_marker_1d( long x_pos,
               long color )
{
    Marker_1d_T *m, *cm;
    XGCValues gcv;


    if ( color > MAX_CURVES + 1 )
    {
        delete_marker_1d( x_pos );
        return;
    }

    m = T_malloc( sizeof *m );
    m->next = NULL;

    if ( G_1d.marker_1d == NULL )
        G_1d.marker_1d = m;
    else
    {
        cm = G_1d.marker_1d;
        while ( cm->next != NULL )
            cm = cm->next;
        cm->next = m;
    }

    gcv.line_style = LineOnOffDash;

    m->color = color;
    m->gc = XCreateGC( G.d, G_1d.canvas.pm, GCLineStyle, &gcv );

    if ( color == 0 )
        XSetForeground( G.d, m->gc, fl_get_pixel( FL_WHITE ) );
    else if ( color <= MAX_CURVES )
        XSetForeground( G.d, m->gc, fl_get_pixel( G.colors[ color - 1 ] ) );
    else
        XSetForeground( G.d, m->gc, fl_get_pixel( FL_BLACK ) );

    if ( G.mode == NORMAL_DISPLAY )
        m->x_pos = x_pos;
    else
        m->x_pos = G_1d.curve[ 0 ]->count - 1;
}


/*------------------------------------------*
 *------------------------------------------*/

static void
delete_marker_1d( long x_pos )
{
    Marker_1d_T *m, *mp;


    for ( mp = NULL, m = G_1d.marker_1d; m != NULL; mp = m, m = m->next )
    {
        if ( m->x_pos != x_pos )
            continue;

        XFreeGC( G.d, m->gc );
        if ( mp != NULL )
            mp->next = m->next;
        if ( m == G_1d.marker_1d )
            G_1d.marker_1d = m->next;
        T_free( m );
        return;
    }
}


/*-----------------------------------*
 * Gets called to delete all markers
 *-----------------------------------*/

void
remove_markers_1d( void )
{
    Marker_1d_T *m, *mn;


    if ( G_1d.marker_1d == NULL )
        return;

    for ( m = G_1d.marker_1d; m != NULL; m = mn )
    {
        XFreeGC( G.d, m->gc );
        mn = m->next;
        T_free( m );
    }
    G_1d.marker_1d = NULL;

    repaint_canvas_1d( &G_1d.canvas );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
