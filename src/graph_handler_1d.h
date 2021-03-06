/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


#pragma once
#if ! defined GRAPH_HANDLER_1D_HEADER
#define GRAPH_HANDLER_1D_HEADER


#include "fsc2.h"


int canvas_handler_1d( FL_OBJECT * /* obj    */,
                       Window      /* window */,
                       int         /* w      */,
                       int         /* h      */,
                       XEvent *    /* ev     */,
                       void *      /* udata  */  );

bool user_zoom_1d( double /* x       */,
                   bool   /* keep_x  */,
                   double /* xw      */,
                   bool   /* keep_xw */,
                   double /* y       */,
                   bool   /* keep_y  */,
                   double /* yw      */,
                   bool   /* keep_yw */  );

void recalc_XPoints_of_curve_1d( Curve_1d_T * /* cv */ );

void redraw_all_1d( void );

void redraw_canvas_1d( Canvas_T * /* c */ );

void repaint_canvas_1d( Canvas_T * /* c */ );

int get_mouse_pos_1d( double *       /* pa      */,
                      unsigned int * /* keymask */  );

void fs_rescale_1d( bool /* vert_only */ );

void make_scale_1d( Curve_1d_T * /* cv    */,
                    Canvas_T *   /* c     */,
                    int          /* coord */  );

void save_scale_state_1d( Curve_1d_T * /* cv */ );

void set_marker_1d( long /* position */,
                    long /* color    */  );

void remove_markers_1d( void );


#endif   /* ! GRAPH_HANDLER_1D_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
