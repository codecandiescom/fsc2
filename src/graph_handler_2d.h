/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
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


#if ! defined GRAPH_HANDLER_2D_HEADER
#define GRAPH_HANDLER_2D_HEADER


#include "fsc2.h"


int canvas_handler_2d( FL_OBJECT * /* obj    */,
                       Window      /* window */,
                       int         /* w      */,
                       int         /* h      */,
                       XEvent *    /* ev     */,
                       void *      /* udata  */  );

bool user_zoom_2d( long   /* curve   */,
                   double /* x       */,
                   bool   /* keep_x  */,
                   double /* xw      */,
                   bool   /* keep_xw */,
                   double /* y       */,
                   bool   /* keep_y  */,
                   double /* yw      */,
                   bool   /* keep_yw */,
                   double /* z       */,
                   bool   /* keep_z  */,
                   double /* zw      */,
                   bool   /* keep_zw */  );

void recalc_XPoints_of_curve_2d( Curve_2d_T * /* cv */ );

void redraw_all_2d( void );

void redraw_canvas_2d( Canvas_T * /* c */ );

void repaint_canvas_2d( Canvas_T * /* c */ );

int get_mouse_pos_2d( double *       /* pa      */,
                      unsigned int * /* keymask */  );

void fs_rescale_2d( Curve_2d_T * /* cv     */,
                    bool         /* z_only */  );

void make_scale_2d( Curve_2d_T * /* cv    */,
                    Canvas_T *   /* c     */,
                    int          /* coord */  );

void save_scale_state_2d( Curve_2d_T * /* cv */ );

void set_marker_2d( long /* x_pos */,
                    long /* y_pos */,
                    long /* color */,
                    long /* curve */  );

void remove_markers_2d( long * /* curves */ );


#endif   /* ! GRAPH_HANDLER_2D_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
