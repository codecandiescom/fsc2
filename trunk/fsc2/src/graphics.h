/*
  $Id$
*/

#if ! defined GRAPHICS_HEADER
#define GRAPHICS_HEADER


typedef struct {
	bool is_init;
	long dim;
	char *x_label;
	char *y_label;
} GRAPHICS;


#include "fsc2.h"

void graphics_init( long dim, char *x_label, char *y_label );

#endif   /* ! GRAPHICS_HEADER */
