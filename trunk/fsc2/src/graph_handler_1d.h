/*
  $Id$
*/


#if ! defined GRAPH_HANDLER_1D__HEADER
#define GRAPH_HANDLER_1D_HEADER


#include "fsc2.h"


int canvas_handler_1d( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					   void *udata );
void reconfigure_window_1d( Canvas *c, int w, int h );
void recalc_XPoints_1d( void );
void recalc_XPoints_of_curve_1d( Curve_1d *cv );
void redraw_all_1d( void );
void redraw_canvas_1d( Canvas *c );
void repaint_canvas_1d( Canvas *c );
void fs_rescale_1d( void );


#endif   /* ! GRAPH_HANDLER_1D_HEADER */
