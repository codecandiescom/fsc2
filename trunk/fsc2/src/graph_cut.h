/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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

#if ! defined GRAPH_CUT_HEADER
#define GRAPH_CUT_HEADER


typedef struct {
	int cut_dir;             /* direction of cut (i.e. X, Y) */
	int curve;               /* number of the curve the cut comes from */

	bool is_shown;

	bool is_scale_set;       /* have scaling factors been calculated ? */
	bool scale_changed;      /* have scaling factors changed ? */

	long nx;                 /* number of points to display */
	long index;              /* index in curve the cut is taken from */

	bool is_fs[ MAX_CURVES ];
	bool has_been_shown[ MAX_CURVES ];
	bool can_undo[ MAX_CURVES ];
	double s2d[ MAX_CURVES ][ 2 ];
	double shift[ MAX_CURVES ][ 2 ];
	double old_s2d[ MAX_CURVES ][ 2 ];
	double old_shift[ MAX_CURVES ][ 2 ];

	int cur_1,
		cur_2,
		cur_3,
		cur_4,
		cur_5,
		cur_6,
		cur_7,
	    cur_8;

	Pixmap label_pm[ 3 ];    /* used for drawing of rotated text */
	                         /* the second pixmap (text at y axis) is always
								identical to the third label pixmap (z axis)
								of the main display window, so don't free! */

	Pixmap up_arrows[ 4 ],
		   down_arrows[ 4 ],
		   left_arrows[ 4 ],
		   right_arrows[ 4 ];

	unsigned int up_arrow_w,    /* sizes of out of range markers */
		         up_arrow_h,
		         down_arrow_w,
		         down_arrow_h,
		         left_arrow_w,
		         left_arrow_h,
		         right_arrow_w,
		         right_arrow_h;

} Cut_Graphics;


#include "fsc2.h"


void cut_init( void );
void cut_show( int dir, long index );
bool cut_data_rescaled( long curve, double y_min, double y_max );
bool cut_num_points_changed( int dir, long num_points );
bool cut_new_points( long curve, long x_index, long y_index, long len );
void redraw_all_cut_canvases( void );
void redraw_cut_axis( int coord );
void cut_new_curve_handler( void );
void cut_form_close( void );
void cut_undo_button_callback( FL_OBJECT *a, long b );
void cut_close_callback( FL_OBJECT *a, long b );
void cut_fs_button_callback( FL_OBJECT *a, long b );
void cut_clear_curve( long curve );


#endif   /* ! GRAPH_CUT_HEADER */
