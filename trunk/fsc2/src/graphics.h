/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#if ! defined GRAPHICS_HEADER
#define GRAPHICS_HEADER


#define MAX_CURVES  4


#define DEFAULT_1D_X_POINTS  32
#define MIN_1D_X_POINTS       2

#define DEFAULT_2D_X_POINTS  16
#define DEFAULT_2D_Y_POINTS  16
#define MIN_2D_X_POINTS       2
#define MIN_2D_Y_POINTS       2

#define NO_CUT_SELECT     0
#define CUT_SELECT_X      1
#define CUT_SELECT_Y      2
#define CUT_SELECT_BREAK -1

#define ZOOM_BOX_CURSOR    0
#define MOVE_HAND_CURSOR   1
#define ZOOM_LENS_CURSOR   2
#define CROSSHAIR_CURSOR   3
#define TARGET_CURSOR      4
#define ARROW_UP_CURSOR    5
#define ARROW_RIGHT_CURSOR 6
#define ARROW_LEFT_CURSOR  7

#define MAX_LABEL_LEN     128       /* maximum length of label strings */

#define DRAG_NONE   0

#define DRAG_1D_X   1
#define DRAG_1D_Y   2
#define DRAG_1D_C   3

#define DRAG_2D_Z   4
#define DRAG_2D_X   5
#define DRAG_2D_Y   6
#define DRAG_2D_C   7

#define DRAG_CUT_Z  8
#define DRAG_CUT_X  9
#define DRAG_CUT_Y 10
#define DRAG_CUT_C 11

#define NORMAL_DISPLAY 0
#define SLIDING_DISPLAY 1

#define NUM_1D_COLS ( MAX_CURVES + 6 )


typedef struct Scaled_Point Scaled_Point_T;
typedef struct Marker_1d Marker_1d_T;
typedef struct Marker_2d Marker_2d_T;
typedef struct Curve_1d Curve_1d_T;
typedef struct Curve_2d Curve_2d_T;
typedef struct Canvas Canvas_T;
typedef struct G_Hash_Entry G_Hash_Entry_T;
typedef struct G_Hash_Entry* G_Hash_T;
typedef struct Graphics Graphics_T;
typedef struct Graphics_1d Graphics_1d_T;
typedef struct Graphics_2d Graphics_2d_T;


struct Scaled_Point {
	double v;               /* value of the point (in interval [0,1] */
	bool exist;             /* set if value has been set at all */
	long xp_ref;            /* index of the associated XPoint */
};


struct Marker_1d {
	long x_pos;
	long color;
	GC gc;
	Marker_1d_T *next;
};


struct Marker_2d {
	long x_pos;
	long y_pos;
	long color;
	GC gc;
	Marker_2d_T *next;
};


struct Curve_1d {
	Scaled_Point_T *points;
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
};


struct Curve_2d {
	bool is_fs;
	bool is_scale_set;

	Scaled_Point_T *points;
	XPoint *xpoints;
	long count;             /* points in curve */

	bool needs_recalc;

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

	Marker_2d_T *marker_2d;
	Marker_1d_T *cut_marker;  /* linked list of markers in cut through curve */

	GC font_gc;             /* gc for font */
};


struct Canvas {
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
};


struct G_Hash_Entry {
	bool is_used;
	FL_COLOR pixel;
	unsigned char rgb[ 3 ];
};


struct Graphics {
	bool is_init;           /* has init_1d() or init_2d() been run ? */
	bool is_fully_drawn;
	bool is_warn;

	int mode;               /* relevant for 1d only - normal or sliding mode */

	long dim;               /* dimensionality of display, 1 for 1d only,
							   2 for 2d only, 3 for both 1d and 2d */

	Display *d;             /* pointer to display structure */

	FL_COLOR colors[ MAX_CURVES ];

	XFontStruct *font;            /* font used for drawing texts */
	int font_asc, font_desc;

	int coord_display;            /* set when coordinates are shown in one
									 of the display windows */
	int dist_display;             /* set when differences between coordinates
									 are shown in one of the display windows */

	unsigned int up_arrow_w,      /* sizes of out of range markers */
		         up_arrow_h,
		         down_arrow_w,
		         down_arrow_h,
		         left_arrow_w,
		         left_arrow_h,
		         right_arrow_w,
		         right_arrow_h;

	int button_state,             /* usuable button states */
		raw_button_state;         /* the real button state */

	int start[ 2 ];               /* start position of mouse movement */

	int drag_canvas;        /* canvas that currently gets the mouse events */

    int scale_tick_dist;    /* mean minimum distance between ticks */
	int short_tick_len;     /* length of short ticks */
	int medium_tick_len;    /* length of medium ticks */
	int long_tick_len;      /* length of long ticks */
	int label_dist;         /* distance between label and scale line */
	int x_scale_offset;     /* x distance between scale line and window */
	int y_scale_offset;		/* y distance between scale line and window */
	int z_scale_offset;		/* z distance between scale line and window */
	int z_line_offset;		/* distance between colour scale and window */
	int z_line_width;       /* width of colour scale */
	int enlarge_box_width;	/* width of enlarge box */

	G_Hash_T color_hash;
	unsigned int color_hash_size;

};


struct Graphics_1d {

	bool is_scale_set;      /* have scaling factors been calculated ? */
	bool is_fs;             /* state of full scale button */

	long nc;                /* number of curves */
	long nx;                /* points in x-direction */
	long ny;                /* points in y-direction */
	double rwc_start[ 2 ];  /* real world coordinate start values */
	double rwc_delta[ 2 ];  /* real world coordinate increment values */
	char *label[ 2 ];       /* label for x- and y-axis */

    long nx_orig;
    long ny_orig;
    double rwc_start_orig[ 2 ];
    double rwc_delta_orig[ 2 ];
    char *label_orig[ 2 ];

	double rw_min;          /* minimum of real world y- or z-coordinates */
	double rw_max;          /* maximum of real world y- or z-coordinates */

	Pixmap label_pm;        /* used for drawing of rotated text of the */
	unsigned int label_w,   /* y-axis label */
		         label_h;

	int cursor[ 7 ];        /* the different cursors */

	Canvas_T x_axis;
	Canvas_T y_axis;
	Canvas_T canvas;

	Curve_1d_T *curve[ MAX_CURVES ];

	Marker_1d_T *marker_1d;   /* linked list of markers */
};


struct Graphics_2d {

	bool is_scale_set;      /* have scaling factors been calculated ? */
	bool is_fs;             /* state of full scale button */

	long nc;                /* number of curves */
	long nx;                /* points in x-direction */
	long ny;                /* points in y-direction */
	double rwc_start[ 3 ];  /* real world coordinate start values */
	double rwc_delta[ 3 ];  /* real world coordinate increment values */
	char *label[ 6 ];       /* label for x-, y- and z-axis */

    long nx_orig;
    long ny_orig;
    double rwc_start_orig[ 3 ];
    double rwc_delta_orig[ 3 ];
    char *label_orig[ 3 ];

	double rw_min;          /* minimum of real world y- or z-coordinates */
	double rw_max;          /* maximum of real world y- or z-coordinates */

	Pixmap label_pm[ 6 ];         /* used for drawing of rotated text */
	unsigned int label_w[ 6 ],    /* for both the 2D and cut window Y */
				 label_h[ 6 ];    /* and Z labels */

	int cursor[ 7 ];        /* the different cursors */

	GC gcs[ NUM_COLORS + 2 ];

	Canvas_T x_axis;
	Canvas_T y_axis;
	Canvas_T z_axis;
	Canvas_T canvas;

	Canvas_T cut_x_axis;
	Canvas_T cut_y_axis;
	Canvas_T cut_z_axis;
	Canvas_T cut_canvas;

	Curve_2d_T *curve_2d[ MAX_CURVES ];
	Curve_1d_T cut_curve;

	int active_curve;       /* curve shown in 2d display (or -1 if none) */

	bool is_cut;            /* set when cross section window is shown */
	int cut_select;
};


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


#include "fsc2.h"


void start_graphics( void );
void stop_graphics( void );
void make_label_string( char *lstr, double num, int res );
void create_label_pixmap( Canvas_T *c, int coord, char *label );
void switch_off_special_cursors( void );
void clear_curve_1d( long curve );
void clear_curve_2d( long curve );
void create_pixmap( Canvas_T *c );
void delete_pixmap( Canvas_T *c );
void redraw_axis_1d( int coord );
void redraw_axis_2d( int coord );
void change_scale_1d( int is_set, void *ptr );
void change_scale_2d( int is_set, void *ptr );
void change_label_1d( char **label );
void change_label_2d( char **label );
void rescale_1d( long new_nx );
void rescale_2d( long *new_dims );
void fs_vert_rescale_1d( void );
void fs_vert_rescale_2d( void );
void redraw_canvas_2d( Canvas_T *c );
void change_mode( long mode, long width );


#endif   /* ! GRAPHICS_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
