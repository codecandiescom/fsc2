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

	if ( bmwb.type == X_BAND )
	{
		fdui->form = fl_bgn_form( FL_NO_BOX, 620, 420 );
		obj = fl_add_box( FL_UP_BOX, 0, 0, 620, 420, "" );
	}
	else
	{
		fdui->form = fl_bgn_form( FL_NO_BOX, 620, 365 );
		obj = fl_add_box( FL_UP_BOX, 0, 0, 620, 365, "" );
	}

    fdui->dc_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 10, 10, 120, 40,
										   "Diode current" );
    fl_set_object_lalign( obj, FL_ALIGN_BOTTOM );

    fdui->afc_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 140, 10, 120, 40,
											"AFC signal" );
    fl_set_object_lalign( obj, FL_ALIGN_BOTTOM );

	if ( bmwb.type == X_BAND )
		fdui->tune_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 10, 80,
												 250, 290, "" );
	else
		fdui->tune_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 10, 80,
												 250, 275, "" );

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

    fdui->main_frame = fl_add_frame( FL_ENGRAVED_FRAME, 270, 80,
									 340, 200, "" );

	fdui->freq_group = fl_bgn_group( );

    fdui->freq_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER, 340, 90,
											 200, 30, "Microwave frequency" );
    fl_set_object_lsize( obj, FL_SMALL_SIZE );
    fl_set_object_color( obj, FL_WHITE, FL_COL1 );
    fl_set_object_callback( obj, freq_cb, 0 );
	fl_set_slider_value( obj, bmwb.freq );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 280, 90, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, -2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 310, 90, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, -1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 540, 90, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, 1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 570, 90, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, 2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

	freq = bmwb.min_freq + bmwb.freq * ( bmwb.max_freq - bmwb.min_freq );
	sprintf( buf, "(%.3f GHz / %.0f G)", freq, FAC * freq );

	fdui->freq_text = obj = fl_add_text( FL_NORMAL_TEXT, 280, 140, 320, 20,
										 buf );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_lsize( obj, FL_SMALL_SIZE );

	fl_end_group( );

	fdui->attenuation_group = fl_bgn_group( );

    fdui->attenuation_counter = obj = fl_add_counter( FL_NORMAL_COUNTER,
													  280, 165, 320, 30,
												 "Microwave attenuation [dB]" );
    fl_set_object_color( obj, FL_WHITE, FL_BLACK );
    fl_set_object_callback( obj, attenuation_cb, 0 );
    fl_set_counter_precision( obj, 0 );
    fl_set_counter_bounds( obj, 0, 60 );
	fl_set_counter_value( obj, bmwb.attenuation );

    obj = fl_add_button( FL_TOUCH_BUTTON, 280, 165, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, -2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 310, 165, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, -1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 540, 165, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, 1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 570, 165, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, 2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

	fl_end_group( );

	fdui->signal_phase_group = fl_bgn_group( );

    fdui->signal_phase_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER,
													 340, 225, 200, 30, "" );

    fl_set_object_lsize( obj, FL_SMALL_SIZE );
    fl_set_object_color( obj, FL_WHITE, FL_COL1 );
    fl_set_object_callback( obj, signal_phase_cb, 0 );
	fl_set_slider_value( obj, bmwb.signal_phase );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	sprintf( buf, "Signal phase (%d%%)", irnd( 100.0 * bmwb.signal_phase ) );
	fl_set_object_label( obj, buf );

    obj = fl_add_button( FL_TOUCH_BUTTON, 280, 225, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, -2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 310, 225, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, -1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 540, 225, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, 1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 570, 225, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, 2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

	fl_end_group( );

	if ( bmwb.type == X_BAND )
		fdui->secondary_frame = fl_add_frame( FL_ENGRAVED_FRAME, 270, 290,
											  340, 120, "" );
	else
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
	sprintf( buf, "Microwave bias (%d%%)", irnd( 100.0 * bmwb.bias ) );
	fl_set_object_label( obj, buf );

    obj = fl_add_button( FL_TOUCH_BUTTON, 280, 300, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, -2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 310, 300, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, -1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 540, 300, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, 1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 570, 300, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, 2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

	fl_end_group( );

	if ( bmwb.type == X_BAND )
	{
		fdui->lock_phase_group = fl_bgn_group( );

		fdui->lock_phase_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER,
													   340, 355, 200, 30, "" );
		fl_set_object_color( obj, FL_WHITE, FL_COL1 );
		fl_set_object_lsize( obj, FL_SMALL_SIZE );
		fl_set_object_callback( obj, lock_phase_cb, 0 );
		fl_set_slider_value( obj, bmwb.lock_phase );
		fl_set_object_return( obj, FL_RETURN_CHANGED );
		sprintf( buf, "Lock phase (%d%%)", irnd( 100.0 * bmwb.lock_phase ) );
		fl_set_object_label( obj, buf );

		obj = fl_add_button( FL_TOUCH_BUTTON, 280, 355, 30, 30, "@<<" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, lock_phase_cb, -2 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		obj = fl_add_button( FL_TOUCH_BUTTON, 310, 355, 30, 30, "@<" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, lock_phase_cb, -1 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		obj = fl_add_button( FL_TOUCH_BUTTON, 540, 355, 30, 30, "@>" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, lock_phase_cb, 1 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		obj = fl_add_button( FL_TOUCH_BUTTON, 570, 355, 30, 30, "@>>" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, lock_phase_cb, 2 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		fl_end_group( );
	}

	if ( bmwb.type == X_BAND )
	{
		fdui->iris_group = fl_bgn_group( );

		fdui->iris_down = obj = fl_add_button( FL_INOUT_BUTTON,
											   60, 380, 30, 30, "@8<" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, iris_cb, -1 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		fdui->iris_up = obj = fl_add_button( FL_INOUT_BUTTON, 90, 380,
											   30, 30, "@2<" );
		fl_set_object_lsize( obj, FL_NORMAL_SIZE );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, iris_cb, 1 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		obj = fl_add_text( FL_NORMAL_TEXT, 130, 385, 100, 20,
						   "Iris / Coupling" );
		fl_set_object_lsize( obj, FL_SMALL_SIZE );

		fl_end_group( );
	}

    fl_end_form( );

    fdui->form->fdui = fdui;

    return fdui;
}
