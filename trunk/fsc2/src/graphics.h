/*
  $Id$
*/


#if ! defined GRAPHICS_HEADER
#define GRAPHICS_HEADER


#define MAX_CURVES  4


#define DEFAULT_1D_X_POINTS  32

#define DEFAULT_2D_X_POINTS  16
#define DEFAULT_2D_Y_POINTS  16

#define NO_CUT_SELECT     0
#define CUT_SELECT_X      1
#define CUT_SELECT_Y      2
#define CUT_SELECT_BREAK -1

#define MAX_LABEL_LEN     128   /* maximum length of tick label string */


#if ( SIZE == HI_RES )
#define SCALE_TICK_DIST   6     /* mean minimum distance between ticks */
#define SHORT_TICK_LEN    5     /* length of short ticks */
#define MEDIUM_TICK_LEN  10     /* length of medium ticks */
#define LONG_TICK_LEN    14     /* length of long ticks */
#define LABEL_DIST        7     /* distance between label and scale line */
#define X_SCALE_OFFSET   20     /* x distance between scale line and window */
#define Y_SCALE_OFFSET   21     /* y distance between scale line and window */
#define Z_SCALE_OFFSET   46     /* z distance between scale line and window */
#define Z_LINE_OFFSET    10     /* distance bewteen colour scale and window */
#define Z_LINE_WIDTH     14     /* width of colour scale */
#define ENLARGE_BOX_WIDTH 5     /* width of enlarge box */
#else
#define SCALE_TICK_DIST   4
#define SHORT_TICK_LEN    3
#define MEDIUM_TICK_LEN   6
#define LONG_TICK_LEN     8
#define LABEL_DIST        5
#define X_SCALE_OFFSET   12
#define Y_SCALE_OFFSET   12
#define Z_SCALE_OFFSET   25
#define Z_SCALE_WIDTH     8
#define Z_LINE_OFFSET     5
#define Z_LINE_WIDTH      8
#define ENLARGE_BOX_WIDTH 3
#endif


typedef	struct {
	double v;               /* value of the point (in interval [0,1] */
	bool exist;             /* set if value has been set at all */
	long xp_ref;            /* index of the associated XPoint */
} Scaled_Point;


typedef struct {
	Scaled_Point *points;
	XPoint *xpoints;
	long count;             /* points in curve */

	GC gc;

	bool active;

	double s2d[ 2 ];        /* scaled to display data scale factors */
	double shift[ 2 ];      /* offsets on scaled data */

	bool up,                /* flag, set if data don't fit into canvas */
		 down,
		 left,
		 right;

	Pixmap up_arrow,
		   down_arrow,
		   left_arrow,
		   right_arrow;

	bool can_undo;

	double old_s2d[ 2 ];
	double old_shift[ 2 ];

	GC font_gc;             /* gc for font */
} Curve_1d;


typedef struct {
	bool is_fs;
	bool is_scale_set;
	bool scale_changed;

	Scaled_Point *points;
	XPoint *xpoints,
		   *xpoints_s;
	long count;             /* points in curve */

	unsigned short w, h;

	GC gc;

	bool active;

	double s2d[ 3 ];        /* scaled to display data scale factors */
	double shift[ 3 ];      /* offsets on scaled data */

	double rwc_start[ 3 ];  /* real world coordinate start values */
	double rwc_delta[ 3 ];  /* real world coordinate increment values */

	double z_factor;

	double rw_min;          /* minimum of real world y- or z-coordinates */
	double rw_max;          /* maximum of real world y- or z-coordinates */

	bool up,                /* flag, set if data don't fit into canvas */
		 down,
		 left,
		 right;

	Pixmap up_arrow,
		   down_arrow,
		   left_arrow,
		   right_arrow;

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
	long nc;                /* number of curves */
	long nx;                /* points in x-direction */
	long ny;                /* points in y-direction */
	double rwc_start[ 3 ];  /* real world coordinate start values */
	double rwc_delta[ 3 ];  /* real world coordinate increment values */
	char *label[ 6 ];       /* label for x-, y- and z-axis */

	double rw_min;          /* minimum of real world y- or z-coordinates */
	double rw_max;          /* maximum of real world y- or z-coordinates */

	Display *d;             /* pointer to display structure */

	Canvas x_axis;
	Canvas y_axis;
	Canvas z_axis;
	Canvas canvas;

	Canvas cut_x_axis;
	Canvas cut_y_axis;
	Canvas cut_z_axis;
	Canvas cut_canvas;

	int drag_canvas;        /* canvas that currently gets the mouse events */
	int cut_drag_canvas;    /* canvas that currently gets the mouse events */

	Curve_1d *curve[ MAX_CURVES ];
	Curve_2d *curve_2d[ MAX_CURVES ];
	Curve_1d cut_curve;

	FL_COLOR colors[ MAX_CURVES ];

	int cur_1,                    /* the different cursors */
	    cur_2,
	    cur_3,
	    cur_4,
	    cur_5,
		cur_6,
		cur_7;

	XFontStruct *font;            /* font used for drawing texts */
	int font_asc, font_desc;

	int button_state,             /* usuable button states */
		raw_button_state;         /* the real button state */

	int start[ 2 ];               /* start position of mouse movemnt */

	Pixmap label_pm[ 6 ];         /* used for drawing of rotated text */
	unsigned int label_w[ 6 ],
		         label_h[ 6 ];

	unsigned int up_arrow_w,      /* sizes of out of range markers */
		         up_arrow_h,
		         down_arrow_w,
		         down_arrow_h,
		         left_arrow_w,
		         left_arrow_h,
		         right_arrow_w,
		         right_arrow_h;

	int active_curve;       /* curve shown in 2d display (or -1 if none) */

	bool is_cut;            /* set when cross section window is shown */
	int cut_select;
} Graphics;



#include "fsc2.h"


void start_graphics( void );
void stop_graphics( void );
void graphics_free( void );
void make_label_string( char *lstr, double num, int res );
void create_label_pixmap( Canvas *c, int coord, char *label );
void switch_off_special_cursors( void );
void clear_curve( long curve );
void create_pixmap( Canvas *c );
void delete_pixmap( Canvas *c );
void redraw_axis( int coord );
void change_scale( int is_set, double *vals );
void change_label( char **label );
void rescale( long new_nx, long new_ny );
void redraw_canvas_2d( Canvas *c );


#endif   /* ! GRAPHICS_HEADER */
