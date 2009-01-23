/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#if ! defined GRAPHICS_EDL_HEADER
#define GRAPHICS_EDL_HEADER


#include "fsc2.h"


void clear_curve_1d( long /* curve */ );

void clear_curve_2d( long /* curve */ );

void create_pixmap( Canvas_T * /* c */ );

void delete_pixmap( Canvas_T * /* c */ );

void redraw_axis_1d( int /* coord */ );

void redraw_axis_2d( int /* coord */ );

void change_scale_1d( int    /* is_set */,
                      void * /* ptr    */  );

void change_scale_2d( int    /* is_set */,
                      void * /* ptr    */  );

void change_label_1d( char ** /* label */ );

void change_label_2d( char ** /* label */ );

void rescale_1d( long /* new_nx */ );

void rescale_2d( long * /* new_dims */ );

void change_mode( long /* mode  */,
                  long /* width */  );

#endif   /* ! GRAPHICS_EDL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
