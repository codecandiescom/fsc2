/*
  $Id$
*/


#if ! defined GRAPH_HANDLER_2D_HEADER
#define GRAPH_HANDLER_2D_HEADER


#include "fsc2.h"


int canvas_handler_2d( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					   void *udata );
void reconfigure_window_2d( Canvas *c, int w, int h );
void recalc_XPoints_2d( void );
void recalc_XPoints_of_curve_2d( Curve_2d *cv );
void redraw_all_2d( void );
void redraw_canvas_2d( Canvas *c );
void repaint_canvas_2d( Canvas *c );
void fs_rescale_2d( Curve_2d *cv );
void make_scale_2d( Curve_2d *cv, Canvas *c, int coord );
void save_scale_state_2d( Curve_2d *cv );


#endif   /* ! GRAPH_HANDLER_2D_HEADER */
