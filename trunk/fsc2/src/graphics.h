/*
  $Id$
*/

#if ! defined GRAPHICS_HEADER
#define GRAPHICS_HEADER


typedef struct {
	FL_OBJECT *obj;
	Window id;
	Pixmap pm;
	GC gc;                  /* gc for pixmap */
	int x,
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

	long dim;               /* dimensionality of display */
	long nx;                /* points in x-direction */
	long ny;                /* points in y-direction */
	double rwc_x_start;
	double rwc_x_delta;
	double rwc_y_start;
	double rwc_y_delta;
	char *x_label;          /* label for x-axis */
	char *y_label;          /* label for y-axis */

	Display *d;             /* pointer to display structure */
	Canvas x_axis;
	Canvas y_axis;
	Canvas canvas;
} Graphics;



#include "fsc2.h"

void graphics_init( long dim, long nx, long ny,
					double rwc_x_start, double rwc_x_delta,
					double rwc_y_start, double rwc_y_delta,
					char *x_label, char *y_label );
void start_graphics( void );
void stop_graphics( void );

#endif   /* ! GRAPHICS_HEADER */
