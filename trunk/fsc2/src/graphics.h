/*
  $Id$
*/


#if ! defined GRAPHICS_HEADER
#define GRAPHICS_HEADER

#define MAX_CURVES  4

#define DEFAULT_X_POINTS  64
#define DEFAULT_Y_POINTS  32


typedef	struct {
	double y;
	bool exist;
} Scaled_Point;


typedef struct {
	Scaled_Point *points;
	XPoint *xpoints;
	long count;            /* points in curve */

	GC gc;

	bool active;

	double s2d_x;          /* scaled to display data x scale factors */
	double s2d_y;          /* scaled to display data y scale factors */

	double x_shift;        /* offset on scaled x data */
	double y_shift;        /* offset on scaled y data */

	bool up,               /* flag, set if data don't fit into canvas */
		 down,
		 left,
		 right;

	Pixmap up_arr,
		   down_arr,
		   left_arr,
		   right_arr;

	bool can_undo;

	double old_s2d_x;
	double old_s2d_y;
	double old_x_shift;
	double old_y_shift;

	GC font_gc;             /* gc for font */
} Curve_1d;


typedef struct {
	FL_OBJECT *obj;
	Pixmap pm;
	GC gc;                  /* gc for pixmap */

	int x_cur,              /* last reported pointer position in canvas */
		y_cur;

	bool is_box;
	GC box_gc;
	int box_x,
		box_y,
		box_w,
		box_h;

	int x,                  /* position of canvas */
		y;
	unsigned int w,         /* width and height of canvas */
		         h;
} Canvas;	


typedef struct {
	bool is_init;
	bool is_drawn;
	bool is_warn;
	bool is_rwc_x;
	bool is_rwc_y;
	bool is_nx;
	bool is_ny;
	bool is_scale_set;
	bool scale_changed;
	bool is_fs;

	long dim;               /* dimensionality of display */
	long nc;                /* number of curves (in 1D experiments) */
	long nx;                /* points in x-direction */
	long ny;                /* points in y-direction */
	double rwc_x_start;
	double rwc_x_delta;
	double rwc_y_start;
	double rwc_y_delta;
	char *x_label;          /* label for x-axis */
	char *y_label;          /* label for y-axis */

	double rw_y_min;        /* minimum of real world y coordinates */
	double rw_y_max;        /* maximum of real world y coordinates */
	double rw2s;            /* real world to scaled data (y) scale factor */

	Display *d;             /* pointer to display structure */

	Pixmap pm;

	Canvas x_axis;
	Canvas y_axis;
	Canvas canvas;

	int drag_canvas;

	Curve_1d *curve[ MAX_CURVES ];

	FL_COLOR colors[ MAX_CURVES ];

	int cur_1,              /* the different cursors */
	    cur_2,
	    cur_3,
	    cur_4,
	    cur_5;

	XFontStruct *font;

	int button_state,       /* usuable button states */
		raw_button_state;   /* the real button state */

	int x_start,
		y_start;

} Graphics;



#include "fsc2.h"
#include "fsc2.h"

void start_graphics( void );
void stop_graphics( void );
void graphics_free( void );
void free_graphics( void );
void reconfigure_window( Canvas *c, int w, int h );
void recalc_XPoints( void );
void recalc_XPoints_of_curve( Curve_1d *cv );
void redraw_canvas( Canvas *c );
void repaint_canvas( Canvas *c );
void switch_off_special_cursors( void );

#endif   /* ! GRAPHICS_HEADER */
