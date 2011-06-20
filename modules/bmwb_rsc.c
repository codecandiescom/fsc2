/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#include "bmwb.h"
#include "bmwb_rsc.h"


/***************************************
 ***************************************/

FD_bmwb_rsc *
create_form_bmwb_rsc( void )
{
    FL_OBJECT *obj;
    FD_bmwb_rsc *fdui = fl_malloc( sizeof *fdui );
	double freq;
	char buf[ 100 ];


    fdui->vdata = fdui->cdata = NULL;
    fdui->ldata = 0;

	fdui->form = fl_bgn_form( FL_NO_BOX, 620, 365 );
	obj = fl_add_box( FL_UP_BOX, 0, 0, 620, 365, "" );

    fdui->dc_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 10, 10, 120, 40,
										   "Diode current" );
    fl_set_object_lalign( obj, FL_ALIGN_BOTTOM );

    fdui->afc_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 140, 10, 120, 40,
											"AFC signal" );
    fl_set_object_lalign( obj, FL_ALIGN_BOTTOM );

	fdui->tune_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 10, 80,
											 250, 230, "" );

    fdui->mode_select = obj = fl_add_select( FL_MENU_SELECT, 280, 15, 100, 30,
											 "Mode" );
    fl_add_select_items( obj, "Standby|Tune|Operate" );
	fl_set_object_callback( obj, mode_cb, 0 );
    fl_set_object_lalign( obj, FL_ALIGN_BOTTOM );
	fl_set_select_policy( obj, FL_POPUP_DRAG_SELECT );

	if ( bmwb.type == X_BAND )
	{
		fdui->unlocked_indicator = obj =
			fl_add_roundbutton( FL_PUSH_BUTTON, 400, 20, 100, 20, "Unlocked" );
		fl_set_object_color( obj, FL_WHITE, FL_FREE_COL1 );
		fl_set_button( obj, 1 );
		fl_deactivate_object( obj );

		fdui->uncalibrated_indicator = obj =
			fl_add_roundbutton( FL_PUSH_BUTTON, 500, 20, 100, 20,
								"Uncalibrated" );
		fl_set_object_color( obj, FL_WHITE, FL_FREE_COL2 );
		fl_set_button( obj, 1 );
		fl_deactivate_object( obj );
	}
	else
	{
		fdui->afc_indicator = obj = fl_add_roundbutton( FL_PUSH_BUTTON,
														400, 20, 100, 20,
														"AFC" );
		fl_set_object_color( obj, FL_WHITE, FL_FREE_COL3 );
		fl_set_button( obj, 1 );
		fl_deactivate_object( obj );
	}

	obj = fl_add_button( FL_NORMAL_BUTTON, 545, 50, 60, 25, "Quit" );
	fl_set_object_callback( obj, quit_handler, 0 );
	fl_set_button_mouse_buttons( obj, 1 );

    fdui->main_frame = fl_add_frame( FL_ENGRAVED_FRAME, 270, 80,
									 340, 200, "" );

	fdui->attenuation_group = fl_bgn_group( );

    fdui->attenuation_counter = obj = fl_add_counter( FL_NORMAL_COUNTER,
													  280, 90, 320, 30, ""  );
    fl_set_object_color( obj, FL_WHITE, FL_BLACK );
	fl_set_object_lsize( obj, FL_MEDIUM_SIZE );
	fl_set_object_lstyle( obj, FL_BOLD_STYLE );
    fl_set_object_callback( obj, attenuation_cb, 0 );
    fl_set_counter_precision( obj, 0 );
    fl_set_counter_bounds( obj, MIN_ATTENUATION, MAX_ATTENUATION );
	fl_set_counter_value( obj, bmwb.attenuation );

	obj = fl_add_text( FL_NORMAL_TEXT, 350, 120, 320, 20,
					   "Microwave attenuation [dB]" );


    obj = fl_add_button( FL_TOUCH_BUTTON, 280, 90, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, COARSE_DECREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 310, 90, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, FINE_DECREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 540, 90, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, FINE_INCREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 570, 90, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, COARSE_INCREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

	fl_end_group( );

	fdui->freq_group = fl_bgn_group( );

    fdui->freq_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER, 340, 151,
											 200, 30, "Microwave frequency" );
    fl_set_object_lsize( obj, FL_SMALL_SIZE );
    fl_set_object_color( obj, FL_WHITE, FL_COL1 );
    fl_set_object_callback( obj, freq_cb, 0 );
	fl_set_slider_value( obj, bmwb.freq );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 280, 151, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, COARSE_DECREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 310, 151, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, FINE_DECREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 540, 151, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, FINE_INCREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 570, 151, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, COARSE_INCREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

	freq = bmwb.min_freq + bmwb.freq * ( bmwb.max_freq - bmwb.min_freq );
	sprintf( buf, "(ca. %.3f GHz / %.0f G)", freq, FAC * freq );

	fdui->freq_text = obj = fl_add_text( FL_NORMAL_TEXT, 285, 199, 320, 20,
										 buf );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_lsize( obj, FL_SMALL_SIZE );

	fl_end_group( );

	fdui->signal_phase_group = fl_bgn_group( );

    fdui->signal_phase_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER,
													 340, 225, 200, 30, "" );

    fl_set_object_lsize( obj, FL_SMALL_SIZE );
    fl_set_object_color( obj, FL_WHITE, FL_COL1 );
    fl_set_object_callback( obj, signal_phase_cb, 0 );
	fl_set_slider_value( obj, bmwb.signal_phase );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	sprintf( buf, "Signal phase (%2d%%)", irnd( 100.0 * bmwb.signal_phase ) );
	fl_set_object_label( obj, buf );

    obj = fl_add_button( FL_TOUCH_BUTTON, 280, 225, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, COARSE_DECREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 310, 225, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, FINE_DECREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 540, 225, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, FINE_INCREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 570, 225, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, COARSE_INCREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

	fl_end_group( );

	fdui->secondary_frame = fl_add_frame( FL_ENGRAVED_FRAME, 270, 290,
										  340, 65, "" );

	fdui->bias_group = fl_bgn_group( );

    fdui->bias_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER, 340, 300,
											 200, 30, "" );
    fl_set_object_color( obj, FL_WHITE, FL_COL1 );
    fl_set_object_lsize( obj, FL_SMALL_SIZE );
    fl_set_object_callback( obj, bias_cb, 0 );
	fl_set_slider_value( obj, bmwb.bias );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	sprintf( buf, "Microwave bias (%2d%%)", irnd( 100.0 * bmwb.bias ) );
	fl_set_object_label( obj, buf );

    obj = fl_add_button( FL_TOUCH_BUTTON, 280, 300, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, COARSE_DECREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 310, 300, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, FINE_DECREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 540, 300, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, FINE_INCREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

    obj = fl_add_button( FL_TOUCH_BUTTON, 570, 300, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, COARSE_INCREASE );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

	fl_end_group( );

	fdui->iris_group = fl_bgn_group( );

	fdui->iris_up = obj = fl_add_button( FL_INOUT_BUTTON, 104, 320,
										 30, 30, "@2<" );
	fl_set_object_lsize( obj, FL_NORMAL_SIZE );
	fl_set_object_lalign( obj, FL_ALIGN_CENTER );
	fl_set_object_callback( obj, iris_cb, 1 );
	fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

	fdui->iris_down = obj = fl_add_button( FL_INOUT_BUTTON,
										   136, 320, 30, 30, "@8<" );
	fl_set_object_lalign( obj, FL_ALIGN_CENTER );
	fl_set_object_callback( obj, iris_cb, -1 );
	fl_set_object_return( obj, FL_RETURN_CHANGED );
	fl_set_button_mouse_buttons( obj, 1 );

	obj = fl_add_text( FL_NORMAL_TEXT, 170, 325, 50, 20, "Iris" );
	fl_set_object_lsize( obj, FL_SMALL_SIZE );

	fl_end_group( );

    fl_end_form( );

    fdui->form->fdui = fdui;

    return fdui;
}
