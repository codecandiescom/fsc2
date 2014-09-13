/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef FD_bmwb_rsc_h_
#define FD_bmwb_rsc_h_

#include <forms.h>


void mode_cb( FL_OBJECT *,
              long );
void freq_cb( FL_OBJECT *,
              long );
void attenuation_cb( FL_OBJECT *,
                     long );
void signal_phase_cb( FL_OBJECT *,
                      long );
void bias_cb( FL_OBJECT *,
              long );
void lock_phase_cb( FL_OBJECT *,
                    long );
void iris_cb( FL_OBJECT *,
              long );
void quit_handler( FL_OBJECT * ,
                   long );


typedef struct {
    FL_FORM   * form;
    void      * vdata;
    char      * cdata;
    long        ldata;
    FL_OBJECT * dc_canvas;
    FL_OBJECT * afc_canvas;
    FL_OBJECT * tune_canvas;
	FL_OBJECT * mode_select;
	FL_OBJECT * unleveled_indicator;
	FL_OBJECT * uncalibrated_indicator;
	FL_OBJECT * afc_indicator;
	FL_OBJECT * main_frame;
	FL_OBJECT * freq_group;
    FL_OBJECT * freq_slider;
	FL_OBJECT * freq_text;
	FL_OBJECT * attenuation_group;
    FL_OBJECT * attenuation_counter;
    FL_OBJECT * attenuation_label;
	FL_OBJECT * signal_phase_group;
    FL_OBJECT * signal_phase_slider;
	FL_OBJECT * secondary_frame;
	FL_OBJECT * bias_group;
    FL_OBJECT * bias_slider;
	FL_OBJECT * lock_phase_group;
    FL_OBJECT * lock_phase_slider;
	FL_OBJECT * iris_group;
    FL_OBJECT * iris_up;
    FL_OBJECT * iris_down;
} FD_bmwb_rsc;


typedef struct {
	Display   * d;
	FL_OBJECT * obj;
	Pixmap      pm;
    GC          gc;
	int         w,
	            h;
} Canvas_T;


FD_bmwb_rsc * create_form_bmwb_rsc( void );

#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
