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
	bool is_connected;
	bool is_fs;
	long count;

	GC gc;

	double s2w_y;          /* scaled to window data scale factors */
	double d2s_y;          /* data to scaled data scale factors   */
	double fs_s2w_y;       /* scaled to window data scale factors */
	double fs_d2s_y;       /* data to scaled data scale factors   */
} Curve_1d;


typedef struct {
	FL_OBJECT *obj;
	Pixmap pm;
	GC gc;                  /* gc for pixmap */
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
	bool x_scale_changed;
	bool y_scale_changed;

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

	double rw_y_min;       /* minimum of real world y coordinates */
	double rw_y_max;       /* maximum of real world y coordinates */
	double fs_rw2s;        /* real world to scaled data (y) scale factor */

	double s2d_x;
	double s2d_y;

	Display *d;             /* pointer to display structure */
	Canvas x_axis;
	Canvas y_axis;
	Canvas canvas;

	Curve_1d *curve[ MAX_CURVES ];

} Graphics;



#include "fsc2.h"

void graphics_init( long dim, long nc, long nx, long ny,
					double rwc_x_start, double rwc_x_delta,
					double rwc_y_start, double rwc_y_delta,
					char *x_label, char *y_label );
void graphics_free( void );
void start_graphics( void );
void stop_graphics( void );
void accept_new_data( void );

#endif   /* ! GRAPHICS_HEADER */
