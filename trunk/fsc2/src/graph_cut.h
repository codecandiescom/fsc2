/*
  $Id$
*/

#if ! defined GRAPH_CUT_HEADER
#define GRAPH_CUT_HEADER


typedef struct {
	int cut_dir;            /* direction of cut (i.e. X, Y) */

	bool is_scale_set;      /* have scaling factors been calculated ? */
	bool scale_changed;     /* have scaling factors changed ? */
	bool is_fs;             /* state of full scale button */

	int cur_1,
		cur_2,
		cur_3,
		cur_4,
		cur_5,
		cur_6,
		cur_7,
	    cur_8;

	Pixmap label_pm[ 3 ];   /* used for drawing of rotated text */

	unsigned int up_arrow_w,      /* sizes of out of range markers */
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
void cut_form_close( void );
void cut_undo_button_callback( FL_OBJECT *a, long b );
void cut_close_callback( FL_OBJECT *a, long b );
void cut_fs_button_callback( FL_OBJECT *a, long b );


#endif   /* ! GRAPH_CUT_HEADER */
