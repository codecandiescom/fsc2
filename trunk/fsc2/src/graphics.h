/*
  $Id$
*/

#if ! defined GRAPHICS_HEADER
#define GRAPHICS_HEADER


typedef struct {
	bool is_init;
	bool is_drawn;
	bool is_warn;
	long dim;               /* dimensionality of display */
	long nx;                /* points in x-direction */
	long ny;                /* points in y-direction */
	char *x_label;          /* label for x-axis */
	char *y_label;          /* label for y-axis */
	Window id;              /* window id */
	Display *d;             /* pointer to display structure */
	Pixmap pm;              /* pixmap for double buffering */
	GC gc;                  /* gc for pixmap */
	int x,                  /* x- and y- position of canvas */
		y;
	unsigned int w,         /* width and height of canvas */
		         h;
} Graphics;



#include "fsc2.h"

void graphics_init( long dim, long nx, long ny, char *x_label, char *y_label );
void start_graphics( void );
void stop_graphics( void );

#endif   /* ! GRAPHICS_HEADER */
