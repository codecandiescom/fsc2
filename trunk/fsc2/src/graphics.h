/*
  $Id$
*/


#if ! defined GRAPHICS_HEADER
#define GRAPHICS_HEADER

#define MAX_CURVES  4

#define DEFAULT_X_POINTS  64
#define DEFAULT_Y_POINTS  32


typedef	struct {
	double v;
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
	bool is_fs;
	bool is_scale_set;
	bool scale_changed;

	Scaled_Point *points;
	XPoint *xpoints,
		   *xpoints_s;
	long count;            /* points in curve */

	unsigned short w, h;

	GC gc;

	bool active;

	double s2d[ 3 ];       /* scaled to display data scale factors */
	double shift[ 3 ];     /* offsets on scaled data */

	double rwc_start[ 3 ];  /* real world coordinate start values */
	double rwc_delta[ 3 ];  /* real world coordinate increment values */

	double z_factor;

	double rw_min;          /* minimum of real world y- or z-coordinates */
	double rw_max;          /* maximum of real world y- or z-coordinates */

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
	double old_z_factor;

	GC font_gc;             /* gc for font */
} Curve_2d;


typedef struct {
	FL_OBJECT *obj;         /* canvas object pointer */
	Pixmap pm;              /* pixmap for double buffering */
	GC gc;                  /* GC for pixmap */

	int ppos[ 2 ];          /* last reported pointer position in canvas */

	bool is_box;            /* is zoom box currently been shown ? */
	GC box_gc;              /* GC for zoom box */
	int box_x,              /* coordinates of zoom box */
		box_y,
		box_w,
		box_h;

	unsigned int w,         /* width and height of canvas */
		         h;

	GC font_gc;             /* gc for font */
} Canvas;	


typedef struct {
	bool is_init;           /* has init_1d() or init2d() been run ? */
	bool is_warn;
	bool is_scale_set;      /* have scaling factors been calculated ? */
	bool scale_changed;     /* have scaling factors changed ? */
	bool is_fs;             /* state of full scale button */

	long dim;               /* dimensionality of display */
	long nc;                /* number of curves (in 1D experiments) */
	long nx;                /* points in x-direction */
	long ny;                /* points in y-direction */
	double rwc_start[ 3 ];  /* real world coordinate start values */
	double rwc_delta[ 3 ];  /* real world coordinate increment values */
	char *label[ 3 ];       /* label for x-, y- and z-axis */

	double rw_min;          /* minimum of real world y- or z-coordinates */
	double rw_max;          /* maximum of real world y- or z-coordinates */

	Display *d;             /* pointer to display structure */

	Pixmap pm;              /* pixmap for double buffering */

	Canvas x_axis;
	Canvas y_axis;
	Canvas z_axis;
	Canvas canvas;

	int drag_canvas;        /* canvas that currently gets the mouse events */

	Curve_1d *curve[ MAX_CURVES ];
	Curve_2d *curve_2d[ MAX_CURVES ];

	FL_COLOR colors[ MAX_CURVES ];

	int cur_1,              /* the different cursors */
	    cur_2,
	    cur_3,
	    cur_4,
	    cur_5;

	XFontStruct *font;      /* font used for drawing texts */
	int font_asc, font_desc;

	int button_state,       /* usuable button states */
		raw_button_state;   /* the real button state */

	int start[ 2 ];         /* start position of mouse movemnt */

	Pixmap label_pm[ 3 ];   /* used for drawing of rotated text */
	unsigned int label_w[ 3 ],
		         label_h[ 3 ];

	unsigned int ua_w,      /* sizes of out of range markers */
		         ua_h,
		         da_w,
		         da_h,
		         la_w,
		         la_h,
		         ra_w,
		         ra_h;

	int active_curve;       /* curve shown in 2d display (or -1 if none) */
} Graphics;



#include "fsc2.h"


void start_graphics( void );
void stop_graphics( void );
void graphics_free( void );
void free_graphics( void );
void make_label_string( char *lstr, double num, int res );
void switch_off_special_cursors( void );
void clear_curve( long curve );
void create_pixmap( Canvas *c );
void delete_pixmap( Canvas *c );
void redraw_axis( int coord );

#endif   /* ! GRAPHICS_HEADER */
