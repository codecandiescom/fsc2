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

	double s2d[ 3 ];       /* scaled to display data scale factors */

	double shift[ 3 ];     /* offsets on scaled data */

	bool up,               /* flag, set if data don't fit into canvas */
		 down,
		 left,
		 right;

	Pixmap up_arr,
		   down_arr,
		   left_arr,
		   right_arr;

	bool can_undo;

	double old_s2d[ 3 ];
	double old_shift[ 3 ];

	GC font_gc;             /* gc for font */
} Curve_1d;


typedef struct {
	FL_OBJECT *obj;
	Pixmap pm;
	GC gc;                  /* gc for pixmap */

	int ppos[ 2 ];          /* last reported pointer position in canvas */

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

	GC font_gc;             /* gc for font */
} Canvas;	


typedef struct {
	bool is_init;
	bool is_drawn;
	bool is_warn;
	bool is_nx;
	bool is_ny;
	bool is_scale_set;
	bool scale_changed;
	bool is_fs;

	long dim;               /* dimensionality of display */
	long nc;                /* number of curves (in 1D experiments) */
	long nx;                /* points in x-direction */
	long ny;                /* points in y-direction */
	double rwc_start[ 3 ];
	double rwc_delta[ 3 ];
	char *label[ 3 ];       /* label for x-, y- and z-axis */

	double rw_y_min;        /* minimum of real world y coordinates */
	double rw_y_max;        /* maximum of real world y coordinates */

	Display *d;             /* pointer to display structure */

	Pixmap pm;

	Canvas x_axis;
	Canvas y_axis;
	Canvas z_axis;
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
	int font_asc, font_desc;

	int button_state,       /* usuable button states */
		raw_button_state;   /* the real button state */

	int start[ 3 ];

	Pixmap label_pm[ 3 ];
	unsigned int label_w[ 3 ],
		         label_h[ 3 ];
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
void redraw_all( void );
void redraw_canvas( Canvas *c );
void repaint_canvas( Canvas *c );
void switch_off_special_cursors( void );
void clear_curve( long curve );

#endif   /* ! GRAPHICS_HEADER */
