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
		fdui->form = fl_bgn_form( FL_NO_BOX, 880, 565 );
	else
		fdui->form = fl_bgn_form( FL_NO_BOX, 880, 490 );

    obj = fl_add_box( FL_UP_BOX, 0, 0, 880, 565, "" );

    fdui->dc_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 30, 30, 170, 50,
										   "Diode current" );
    fl_set_object_lalign( obj, FL_ALIGN_BOTTOM );
    fl_set_object_lsize( obj, FL_NORMAL_SIZE );
    fl_set_object_lstyle( obj, FL_BOLD_STYLE );

    fdui->afc_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 240, 30, 170, 50,
											"AFC signal" );
    fl_set_object_lalign( obj, FL_ALIGN_BOTTOM );
    fl_set_object_lsize( obj, FL_NORMAL_SIZE );
    fl_set_object_lstyle( obj, FL_BOLD_STYLE );

    fdui->tune_canvas = obj = fl_add_canvas( FL_NORMAL_CANVAS, 28, 120,
											 380, 350, "" );

    fdui->mode_select = obj = fl_add_select( FL_MENU_SELECT, 455, 35, 120, 30,
											 "Mode" );
    fl_add_select_items( obj, "Standby|Tune|Operate" );
	fl_set_object_callback( obj, mode_cb, 0 );
    fl_set_object_lsize( obj, FL_NORMAL_SIZE );
    fl_set_object_lstyle( obj, FL_BOLD_STYLE );
    fl_set_object_lalign( obj, FL_ALIGN_BOTTOM );
	fl_set_select_text_font( obj, FL_BOLD_STYLE, FL_NORMAL_SIZE );
	fl_popup_entry_set_font( fl_get_select_popup( obj ),
							 FL_BOLD_STYLE, FL_NORMAL_SIZE );
	fl_set_select_policy( obj, FL_POPUP_DRAG_SELECT );

	if ( bmwb.type == X_BAND )
	{
		fdui->unlocked_indicator = obj =
			fl_add_roundbutton( FL_PUSH_BUTTON, 600, 35, 120, 30, "Unlocked" );
		fl_set_object_lsize( obj, FL_NORMAL_SIZE );
		fl_set_object_lstyle( obj, FL_BOLD_STYLE );
		fl_set_object_color( obj, FL_WHITE, FL_FREE_COL1 );
		fl_set_button( obj, 1 );
		fl_deactivate_object( obj );

		fdui->uncalibrated_indicator = obj =
			fl_add_roundbutton( FL_PUSH_BUTTON, 720, 35, 120, 30,
								"Uncalibrated" );
		fl_set_object_lsize( obj, FL_NORMAL_SIZE );
		fl_set_object_lstyle( obj, FL_BOLD_STYLE );
		fl_set_object_color( obj, FL_WHITE, FL_FREE_COL2 );
		fl_set_button( obj, 1 );
		fl_deactivate_object( obj );
	}
	else
	{
		fdui->afc_indicator = obj = fl_add_roundbutton( FL_PUSH_BUTTON,
														600, 35, 80, 30,
														"AFC" );
		fl_set_object_lsize( obj, FL_NORMAL_SIZE );
		fl_set_object_lstyle( obj, FL_BOLD_STYLE );
		fl_set_object_color( obj, FL_WHITE, FL_FREE_COL3 );
		fl_set_button( obj, 1 );
		fl_deactivate_object( obj );
	}

    fdui->main_frame = fl_add_frame( FL_ENGRAVED_FRAME, 455, 98,
									 400, 230, "" );

	fdui->freq_group = fl_bgn_group( );

    fdui->freq_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER, 540, 119,
											 230, 30, "Microwave frequency" );
    fl_set_object_color( obj, FL_WHITE, FL_COL1 );
    fl_set_object_lsize( obj, FL_NORMAL_SIZE );
    fl_set_object_lstyle( obj, FL_BOLD_STYLE );
    fl_set_object_callback( obj, freq_cb, 0 );
	fl_set_slider_value( obj, bmwb.freq );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 510, 119, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, -1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 480, 119, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, -2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 770, 119, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, 1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 800, 119, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, freq_cb, 2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

	freq = bmwb.min_freq + bmwb.freq * ( bmwb.max_freq - bmwb.min_freq );
	sprintf( buf, "(%.3f GHz / %.0f G)", freq, FAC * freq );

	fdui->freq_text = obj = fl_add_text( FL_NORMAL_TEXT, 480, 169, 350, 20,
										 buf );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_lsize( obj, FL_SMALL_SIZE );
    fl_set_object_lstyle( obj, FL_BOLD_STYLE );

	fl_end_group( );

	fdui->attenuation_group = fl_bgn_group( );

    fdui->attenuation_counter = obj = fl_add_counter( FL_NORMAL_COUNTER,
													  480, 195, 350, 30,
												"Microwaver attenuation [dB]" );
    fl_set_object_color( obj, FL_WHITE, FL_BLACK );
    fl_set_object_lsize( obj, FL_NORMAL_SIZE );
    fl_set_object_lstyle( obj, FL_BOLD_STYLE );
    fl_set_object_callback( obj, attenuation_cb, 0 );
    fl_set_counter_precision( obj, 0 );
    fl_set_counter_bounds( obj, 0, 60 );
	fl_set_counter_value( obj, bmwb.attenuation );

    obj = fl_add_button( FL_TOUCH_BUTTON, 510, 195, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, -1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 480, 195, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, -2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 770, 195, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, 1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 800, 195, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, attenuation_cb, 2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

	fl_end_group( );

	fdui->signal_phase_group = fl_bgn_group( );

    fdui->signal_phase_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER,
													 539, 265, 230, 30, "" );

    fl_set_object_color( obj, FL_WHITE, FL_COL1 );
    fl_set_object_lsize( obj, FL_NORMAL_SIZE );
    fl_set_object_lstyle( obj, FL_BOLD_STYLE );
    fl_set_object_callback( obj, signal_phase_cb, 0 );
	fl_set_slider_value( obj, bmwb.signal_phase );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	sprintf( buf, "Signal phase (%d%%)", irnd( 100.0 * bmwb.signal_phase ) );
	fl_set_object_label( obj, buf );

    obj = fl_add_button( FL_TOUCH_BUTTON, 510, 265, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, -1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 480, 265, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, -2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 770, 265, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, 1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 800, 265, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, signal_phase_cb, 2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

	fl_end_group( );

	if ( bmwb.type == X_BAND )
		fdui->secondary_frame = fl_add_frame( FL_ENGRAVED_FRAME, 455, 336,
											  400, 153, "" );
	else
		fdui->secondary_frame = fl_add_frame( FL_ENGRAVED_FRAME, 455, 336,
											  406, 80, "" );

	fdui->bias_group = fl_bgn_group( );

    fdui->bias_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER, 543, 357,
											 230, 30, "" );
    fl_set_object_color( obj, FL_WHITE, FL_COL1 );
    fl_set_object_lsize( obj, FL_NORMAL_SIZE );
    fl_set_object_lstyle( obj, FL_BOLD_STYLE );
    fl_set_object_callback( obj, bias_cb, 0 );
	fl_set_slider_value( obj, bmwb.bias );
    fl_set_object_return( obj, FL_RETURN_CHANGED );
	sprintf( buf, "Microwave bias (%d%%)", irnd( 100.0 * bmwb.bias ) );
	fl_set_object_label( obj, buf );

    obj = fl_add_button( FL_TOUCH_BUTTON, 513, 357, 30, 30, "@<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, -1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 483, 357, 30, 30, "@<<" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, -2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 773, 357, 30, 30, "@>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, 1 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

    obj = fl_add_button( FL_TOUCH_BUTTON, 803, 357, 30, 30, "@>>" );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_callback( obj, bias_cb, 2 );
    fl_set_object_return( obj, FL_RETURN_CHANGED );

	fl_end_group( );

	if ( bmwb.type == X_BAND )
	{
		fdui->lock_phase_group = fl_bgn_group( );

		fdui->lock_phase_slider = obj = fl_add_slider( FL_HOR_BROWSER_SLIDER,
													   543, 420, 230, 30, "" );
		fl_set_object_color( obj, FL_WHITE, FL_COL1 );
		fl_set_object_lsize( obj, FL_NORMAL_SIZE );
		fl_set_object_lstyle( obj, FL_BOLD_STYLE );
		fl_set_object_callback( obj, lock_phase_cb, 0 );
		fl_set_slider_value( obj, bmwb.lock_phase );
		fl_set_object_return( obj, FL_RETURN_CHANGED );
		sprintf( buf, "Lock phase (%d%%)", irnd( 100.0 * bmwb.lock_phase ) );
		fl_set_object_label( obj, buf );

		obj = fl_add_button( FL_TOUCH_BUTTON, 513, 420, 30, 30, "@<" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, lock_phase_cb, -1 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		obj = fl_add_button( FL_TOUCH_BUTTON, 483, 420, 30, 30, "@<<" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, lock_phase_cb, -2 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		obj = fl_add_button( FL_TOUCH_BUTTON, 773, 420, 30, 30, "@>" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, lock_phase_cb, 1 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		obj = fl_add_button( FL_TOUCH_BUTTON, 803, 420, 30, 30, "@>>" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, lock_phase_cb, 2 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		fl_end_group( );
	}

	if ( bmwb.type == X_BAND )
	{
		fdui->iris_group = fl_bgn_group( );

		fdui->iris_down = obj = fl_add_button( FL_INOUT_BUTTON, 70, 500, 40, 40,
											   "@8<" );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, iris_cb, -1 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		fdui->iris_up = obj = fl_add_button( FL_INOUT_BUTTON, 110, 500,
											   40, 40, "@2<" );
		fl_set_object_lsize( obj, FL_NORMAL_SIZE );
		fl_set_object_lalign( obj, FL_ALIGN_CENTER );
		fl_set_object_callback( obj, iris_cb, 1 );
		fl_set_object_return( obj, FL_RETURN_CHANGED );

		obj = fl_add_text( FL_NORMAL_TEXT, 160, 505, 120, 30,
						   "Iris / Coupling" );
		fl_set_object_lsize( obj, FL_NORMAL_SIZE );
		fl_set_object_lstyle( obj, FL_BOLD_STYLE );

		fl_end_group( );
	}

	if ( bmwb.type == X_BAND )
		obj = fl_add_button( FL_NORMAL_BUTTON, 760, 510, 100, 35, "Quit" );
	else
		obj = fl_add_button( FL_NORMAL_BUTTON, 760, 440, 100, 35, "Quit" );
    fl_set_object_lsize( obj, FL_NORMAL_SIZE );
    fl_set_object_lalign( obj, FL_ALIGN_CENTER );
    fl_set_object_lstyle( obj, FL_BOLD_STYLE );
    fl_set_object_callback( obj, quit_cb, 0 );

    fl_end_form( );

    fdui->form->fdui = fdui;

    return fdui;
}
