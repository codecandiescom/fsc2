/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#if ! defined GRAPH_HANDLER_2D_HEADER
#define GRAPH_HANDLER_2D_HEADER


#include "fsc2.h"


int canvas_handler_2d( FL_OBJECT *obj, Window window, int w, int h, XEvent *ev,
					   void *udata );
void recalc_XPoints_of_curve_2d( Curve_2d *cv );
void redraw_all_2d( void );
void repaint_canvas_2d( Canvas *c );
void fs_rescale_2d( Curve_2d *cv );
void make_scale_2d( Curve_2d *cv, Canvas *c, int coord );
void save_scale_state_2d( Curve_2d *cv );


#endif   /* ! GRAPH_HANDLER_2D_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
