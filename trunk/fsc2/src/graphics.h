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

#define ZOOM_BOX_CURSOR    0
#define MOVE_HAND_CURSOR   1
#define ZOOM_LENS_CURSOR   2
#define CROSSHAIR_CURSOR   3
#define TARGET_CURSOR      4
#define ARROW_UP_CURSOR    5
#define ARROW_RIGHT_CURSOR 6
#define ARROW_LEFT_CURSOR  7

#define MAX_LABEL_LEN     128   /* maximum length of tick label string */

#define NUM_1D_COLS ( MAX_CURVES + 6 )



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


typedef struct MRKR_ {
	long position;
	long color;
	GC gc;
	struct MRKR_ *next;
} Marker;


typedef struct {
	bool is_used;
	FL_COLOR pixel;
	unsigned char rgb[ 3 ];
} G_Hash_Entry;

typedef G_Hash_Entry* G_Hash;


typedef struct {
	bool is_init;           /* has init_1d() or init2d() been run ? */
	bool is_fully_drawn;
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

	long nx_orig;
	long ny_orig;
	double rwc_start_orig[ 3 ];
	double rwc_delta_orig[ 3 ];
	char *label_orig[ 3 ];

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

	G_Hash color_hash;
	int color_hash_size;

	int cursor[ 7 ];              /* the different cursors */

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

    int scale_tick_dist;    /* mean minimum distance between ticks */
	int short_tick_len;     /* length of short ticks */
	int medium_tick_len;    /* length of medium ticks */
	int long_tick_len;      /* length of long ticks */
	int label_dist;         /* distance between label and scale line */
	int x_scale_offset;     /* x distance between scale line and window */
	int y_scale_offset;		/* y distance between scale line and window */
	int z_scale_offset;		/* z distance between scale line and window */
	int z_scale_width;
	int z_line_offset;		/* distance between colour scale and window */
	int z_line_width;       /* width of colour scale */
	int enlarge_box_width;	/* width of enlarge box */

	Marker *marker;         /* lined list of markers (1D only) */

} Graphics;


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
void graphics_free( void );
void make_label_string( char *lstr, double num, int res );
void create_label_pixmap( Canvas *c, int coord, char *label );
void switch_off_special_cursors( void );
void clear_curve( long curve );
void create_pixmap( Canvas *c );
void delete_pixmap( Canvas *c );
void redraw_axis( int coord );
void change_scale( int is_set, void *ptr );
void change_label( char **label );
void rescale( void *new_dims );
void redraw_canvas_2d( Canvas *c );


#endif   /* ! GRAPHICS_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
