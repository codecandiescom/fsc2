/*
  $Id$
*/

#if ! defined GRAPH_CUT_HEADER
#define GRAPH_CUT_HEADER


typedef struct {
	int cut_dir;             /* direction of cut (i.e. X, Y) */
	int cut_for_curve;       /* number of the curve the cut comes from */

	bool is_scale_set;       /* have scaling factors been calculated ? */
	bool scale_changed;      /* have scaling factors changed ? */
	bool is_fs;              /* state of full scale button */

	long nx;                 /* number of points to display */
	long index;              /* index in curve the cut is taken from */

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

void cut_show( int dir, int pos );
bool cut_data_rescaled( long curve );
bool cut_num_points_changed( int dir, long num_points );
bool cut_new_point( long curve, long x_index, long y_index, double val );
void cut_new_data_redraw( void );
void cut_new_curve_handler( void );
void cut_form_close( void );
void cut_undo_button_callback( FL_OBJECT *a, long b );
void cut_close_callback( FL_OBJECT *a, long b );
void cut_fs_button_callback( FL_OBJECT *a, long b );
void cut_clear_curve( long curve );


#endif   /* ! GRAPH_CUT_HEADER */
