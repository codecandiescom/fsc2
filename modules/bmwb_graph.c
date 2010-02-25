/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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


#include "bmwb.h"

Canvas_T tune_canvas,
         afc_canvas,
         dc_canvas;

static void init_canvas( void );
static void display_detector_current( void );
static void display_afc_signal( void );
static void display_unlocked( void );
static void display_uncalibrated( void );
static void display_afc_state( void );
static int canvas_expose( FL_OBJECT * obj,
                          Window      win,
                          int         win_width,
                          int         win_height,
                          XEvent    * xev,
                          void      * user_data );


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
graphics_init( void )
{
	const char *title = bmwb.type == X_BAND ?
                        "X-Band Microwave Bridge" : "Q-Band Microwave Bridge";

	fl_set_idle_callback( update_display, NULL );
	bmwb.rsc = create_form_bmwb_rsc( );
	fl_show_form( bmwb.rsc->form, FL_PLACE_CENTER, FL_FULLBORDER, title );

    init_canvas( );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static void
init_canvas( void )
{
    int i;


    /* Initialize the canvase for the tune mode display */

    tune_canvas.obj = bmwb.rsc->tune_canvas;
    tune_canvas.d   = FL_FormDisplay( bmwb.rsc->form );
    tune_canvas.gc  = XCreateGC( tune_canvas.d,
                                 FL_ObjWin( tune_canvas.obj ), 0, 0 );
    XSetGraphicsExposures( tune_canvas.d, tune_canvas.gc, False );
    fl_get_object_size( tune_canvas.obj, &tune_canvas.w, &tune_canvas.h );

    tune_canvas.pm = XCreatePixmap( tune_canvas.d, FL_ObjWin( tune_canvas.obj ),
                                    tune_canvas.w, tune_canvas.h,
                                    fl_get_canvas_depth( tune_canvas.obj ) );

    XSetForeground( tune_canvas.d, tune_canvas.gc,
                    fl_get_pixel( FL_BLACK ) );
    XFillRectangle( tune_canvas.d, tune_canvas.pm, tune_canvas.gc, 0, 0,
                    tune_canvas.w, tune_canvas.h );
    XSetForeground( tune_canvas.d, tune_canvas.gc,
                    fl_get_pixel( FL_RED ) );
    XSetLineAttributes( tune_canvas.d, tune_canvas.gc, 0, LineOnOffDash,
                        CapButt, JoinRound );
    XDrawLine( tune_canvas.d, tune_canvas.pm, tune_canvas.gc,
               tune_canvas.w / 2, 0, tune_canvas.w / 2, tune_canvas.h );
    
    XCopyArea( tune_canvas.d, tune_canvas.pm, FL_ObjWin( tune_canvas.obj ),
               tune_canvas.gc, 0, 0, tune_canvas.w, tune_canvas.h, 0, 0 );

    XSetLineAttributes( tune_canvas.d, tune_canvas.gc, 3, LineSolid,
                        CapButt, JoinRound );

    fl_add_canvas_handler( tune_canvas.obj, Expose, canvas_expose, NULL );

    /* Initialize the canvase for displaying the detector current */

    dc_canvas.obj = bmwb.rsc->dc_canvas;
    dc_canvas.d   = FL_FormDisplay( bmwb.rsc->form );
    dc_canvas.gc  = XCreateGC( dc_canvas.d, FL_ObjWin( dc_canvas.obj ),
                               0, 0 );
    XSetGraphicsExposures( dc_canvas.d, dc_canvas.gc, False );
    fl_get_object_size( dc_canvas.obj, &dc_canvas.w, &dc_canvas.h );

    dc_canvas.pm = XCreatePixmap( dc_canvas.d, FL_ObjWin( dc_canvas.obj ),
                                  dc_canvas.w, dc_canvas.h,
                                  fl_get_canvas_depth( dc_canvas.obj ) );

    XSetForeground( dc_canvas.d, dc_canvas.gc,
                    fl_get_pixel( FL_WHITE ) );
    XFillRectangle( dc_canvas.d, dc_canvas.pm, dc_canvas.gc, 0, 0,
                    dc_canvas.w, dc_canvas.h );

    XSetForeground( dc_canvas.d, dc_canvas.gc,
                    fl_get_pixel( FL_GREEN ) );
    XFillRectangle( dc_canvas.d, dc_canvas.pm, dc_canvas.gc, 2,
                    dc_canvas.h / 2 - 1, dc_canvas.w - 4, 2 );
    for ( i = 0; i < 4; i++ )
        XFillRectangle( dc_canvas.d, dc_canvas.pm, dc_canvas.gc,
                        2 + i * ( ( dc_canvas.w - 6 ) / 4 ),
                        dc_canvas.h / 2 - 6, 2, 15 );
    XFillRectangle( dc_canvas.d, dc_canvas.pm, dc_canvas.gc,
                    dc_canvas.w - 4, dc_canvas.h / 2 - 6, 2, 15 );

    XCopyArea( dc_canvas.d, dc_canvas.pm, FL_ObjWin( dc_canvas.obj ),
               dc_canvas.gc, 0, 0, dc_canvas.w, dc_canvas.h, 0, 0 );

    XSetForeground( dc_canvas.d, dc_canvas.gc,
                    fl_get_pixel( FL_BLUE ) );

    fl_add_canvas_handler( dc_canvas.obj, Expose, canvas_expose, NULL );

    /* Initialize the canvas for displayig the AFC signal */

    afc_canvas.obj = bmwb.rsc->afc_canvas;
    afc_canvas.d   = FL_FormDisplay( bmwb.rsc->form );
    afc_canvas.gc  = XCreateGC( afc_canvas.d, FL_ObjWin( afc_canvas.obj ),
                                0, 0 );
    XSetGraphicsExposures( afc_canvas.d, afc_canvas.gc, False );
    fl_get_object_size( afc_canvas.obj, &afc_canvas.w, &afc_canvas.h );

    afc_canvas.pm = XCreatePixmap( afc_canvas.d, FL_ObjWin( afc_canvas.obj ),
                                   afc_canvas.w, afc_canvas.h,
                                   fl_get_canvas_depth( afc_canvas.obj ) );

    XSetForeground( afc_canvas.d, afc_canvas.gc,
                    fl_get_pixel( FL_WHITE ) );
    XFillRectangle( afc_canvas.d, afc_canvas.pm, afc_canvas.gc, 0, 0,
                    afc_canvas.w, afc_canvas.h );

    XSetForeground( afc_canvas.d, afc_canvas.gc,
                    fl_get_pixel( FL_GREEN ) );
    XFillRectangle( afc_canvas.d, afc_canvas.pm, afc_canvas.gc, 2,
                    afc_canvas.h / 2 - 1, afc_canvas.w - 4, 2 );
    for ( i = 0; i < 4; i++ )
        XFillRectangle( afc_canvas.d, afc_canvas.pm, afc_canvas.gc,
                        2 + i * ( ( afc_canvas.w - 6 ) / 4 ),
                        afc_canvas.h / 2 - 6, 2, 12 );
    XFillRectangle( afc_canvas.d, afc_canvas.pm, afc_canvas.gc,
                    afc_canvas.w - 4, afc_canvas.h / 2 - 6, 2, 15 );

    XCopyArea( afc_canvas.d, afc_canvas.pm, FL_ObjWin( afc_canvas.obj ),
               afc_canvas.gc, 0, 0, afc_canvas.w, afc_canvas.h, 0, 0 );

    XSetForeground( afc_canvas.d, afc_canvas.gc,
                    fl_get_pixel( FL_BLUE ) );

    fl_add_canvas_handler( afc_canvas.obj, Expose, canvas_expose, NULL );
}


/*----------------------------*
 * Callback for mode selector
 *----------------------------*/

void
mode_cb( FL_OBJECT * obj,
		 long        data  UNUSED_ARG )
{
	FL_POPUP_RETURN *r;

    pthread_mutex_lock( &bmwb.mutex );

    r =  fl_get_select_item( obj );

	if ( r->val != bmwb.mode )
    {
        /* Clear canvas if old mode was tune mode */

        if ( bmwb.mode == MODE_TUNE )
            display_mode_pic( r->val );

        bmwb.mode = r->val;
		set_mode( bmwb.mode );
    }

    pthread_mutex_unlock( &bmwb.mutex );

	update_display( NULL, NULL );
}


/*-------------------------------------------*
 * Callback for frequency slider and buttons
 *-------------------------------------------*/

void
freq_cb( FL_OBJECT * obj  UNUSED_ARG,
		 long        data )
{
	double val;
	double freq;
	char buf[ 100 ];


    pthread_mutex_lock( &bmwb.mutex );

    val = fl_get_slider_value( bmwb.rsc->freq_slider );

	switch ( data )
	{
		case 1 :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( 1.0, val + 0.005 );
			break;

		case 2 :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( 1.0, val + 0.05 );
			break;

		case -1 :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( 0.0, val - 0.005 );
			break;

		case -2 :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( 0.0, val - 0.05 );
			break;
	}

	if ( data != 0 )
		fl_set_slider_value( bmwb.rsc->freq_slider, val );

	freq = bmwb.min_freq + val * ( bmwb.max_freq - bmwb.min_freq );
	sprintf( buf, "(%.3f GHz / %.0f G)", freq, FAC * freq );
	fl_set_object_label( bmwb.rsc->freq_text, buf );

	set_mw_freq( val );

    pthread_mutex_unlock( &bmwb.mutex );

	update_display( NULL, NULL );
}


/*--------------------------------------------------------*
 * Callback for microwave attenuation counter and buttons
 *--------------------------------------------------------*/

void
attenuation_cb( FL_OBJECT * obj  UNUSED_ARG,
				long        data )
{
	double val;


    pthread_mutex_lock( &bmwb.mutex );

    val = fl_get_counter_value( bmwb.rsc->attenuation_counter );

	switch ( data )
	{
		case 1 :
			if ( val >= MAX_ATTENUATION )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( MAX_ATTENUATION, val + 1 );
			break;

		case 2 :
			if ( val >= MAX_ATTENUATION )
            {
                pthread_mutex_unlock( &bmwb.mutex );
                return;
            }
			val = d_min( MAX_ATTENUATION, val + 10 );
			break;

		case -1 :
			if ( val <= MIN_ATTENUATION )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( MIN_ATTENUATION, val - 1 );
			break;

		case -2 :
			if ( val <= MIN_ATTENUATION )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( MIN_ATTENUATION, val - 10 );
			break;
	}

	if ( data != 0 )
		fl_set_counter_value( bmwb.rsc->attenuation_counter, val );

	set_mw_attenuation( irnd( val ) );

    pthread_mutex_unlock( &bmwb.mutex );

	update_display( NULL, NULL );
}


/*----------------------------------------------*
 * Callback for signal phase slider and buttons
 *----------------------------------------------*/

void
signal_phase_cb( FL_OBJECT * obj  UNUSED_ARG,
				 long        data )
{
	double val;
    char buf[ 50 ];


    pthread_mutex_lock( &bmwb.mutex );

    val = fl_get_slider_value( bmwb.rsc->signal_phase_slider );

	switch ( data )
	{
		case 1 :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( 1.0, val + 0.01 );
			break;

		case 2 :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( 1.0, val + 0.05 );
			break;

		case -1 :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( 0.0, val - 0.01 );
			break;

		case -2 :
			if ( val <= 0.01 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( 0.0, val - 0.05 );
			break;
	}

	if ( data != 0 )
		fl_set_slider_value( bmwb.rsc->signal_phase_slider, val );

	sprintf( buf, "Signal phase (%d%%)", irnd( 100.0 * val ) );
	fl_set_object_label( bmwb.rsc->signal_phase_slider, buf );

	set_signal_phase( val );

    pthread_mutex_unlock( &bmwb.mutex );

	update_display( NULL, NULL );
}


/*------------------------------------------------*
 * Callback for microwave bias slider and buttons
 *------------------------------------------------*/

void
bias_cb( FL_OBJECT * obj  UNUSED_ARG,
		 long        data )
{
	double val;
    char buf[ 50 ];


    pthread_mutex_lock( &bmwb.mutex );

    val = fl_get_slider_value( bmwb.rsc->bias_slider );

	switch ( data )
	{
		case 1 :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( 1.0, val + 0.01 );
			break;

		case 2 :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( 1.0, val + 0.05 );
			break;

		case -1 :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( 0.0, val - 0.01 );
			break;

		case -2 :
			if ( val <= 0.01 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( 0.0, val - 0.05 );
			break;
	}

	if ( data != 0 )
		fl_set_slider_value( bmwb.rsc->bias_slider, val );

	sprintf( buf, "Microwave bias (%d%%)", irnd( 100.0 * val ) );
	fl_set_object_label( bmwb.rsc->bias_slider, buf );

	set_mw_bias( val );

    pthread_mutex_unlock( &bmwb.mutex );

	update_display( NULL, NULL );
}


/*--------------------------------------------*
 * Callback for lock phase slider and buttons
 *--------------------------------------------*/

void
lock_phase_cb( FL_OBJECT * obj  UNUSED_ARG,
			   long        data )
{
	double val;
    char buf[ 50 ];


    pthread_mutex_lock( &bmwb.mutex );

    val = fl_get_slider_value( bmwb.rsc->lock_phase_slider );

	switch ( data )
	{
		case 1 :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( 1.0, val + 0.01 );
			break;

		case 2 :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( 1.0, val + 0.05 );
			break;

		case -1 :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( 0.0, val - 0.01 );
			break;

		case -2 :
			if ( val <= 0.01 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_max( 0.0, val - 0.05 );
			break;
	}

	if ( data != 0 )
		fl_set_slider_value( bmwb.rsc->lock_phase_slider, val );

	sprintf( buf, "Lock phase (%d%%)", irnd( 100.0 * val ) );
	fl_set_object_label( bmwb.rsc->lock_phase_slider, buf );

	set_lock_phase( val );

    pthread_mutex_unlock( &bmwb.mutex );

	update_display( NULL, NULL );
}


/*------------------------------------------------*
 * Callback for the iris buttons, called when one
 * of the iris buttons is pressed and released
 *------------------------------------------------*/

void
iris_cb( FL_OBJECT * obj  UNUSED_ARG,
		 long        data )
{
	static int state = 0;


    pthread_mutex_lock( &bmwb.mutex );

	if (    data == 1
		 && fl_get_button( bmwb.rsc->iris_up ) != state )
	{
		state ^= 1;
		set_iris( state );
	}
	else if (    data == -1
			  && fl_get_button( bmwb.rsc->iris_down ) != - state )
	{
		state = - ( - state ^ 1 );
		set_iris( state );
	}

    pthread_mutex_unlock( &bmwb.mutex );

	update_display( NULL, NULL );
}


/*------------------------------*
 * Callback for the quit button
 *------------------------------*/

void
quit_cb( FL_OBJECT * obj   UNUSED_ARG,
		 long        data  UNUSED_ARG )
{
    pthread_mutex_lock( &bmwb.mutex );

	set_mode( MODE_STANDBY );
	save_state( );
	meilhaus_finish( );
	fl_finish( );
    raise_permissions( );
    unlink( P_tmpdir "/bmwb.uds" );
    lower_permissions( );
	exit( EXIT_SUCCESS );
}


/*-----------------------------------------------*
 * Updates the detector current, AFC signal and,
 * when in tune mode, the tune canvas
 *-----------------------------------------------*/

int
update_display( XEvent * xev   UNUSED_ARG ,
				void   * data  UNUSED_ARG )
{
    pthread_mutex_lock( &bmwb.mutex );

	display_detector_current( );
	display_afc_signal( );

	if ( bmwb.type == X_BAND )
	{
		display_unlocked( );
		display_uncalibrated( );
	}
	else
		display_afc_state( );

	if ( bmwb.mode == MODE_TUNE )
		display_mode_pic( bmwb.mode );

    pthread_mutex_unlock( &bmwb.mutex );

	return 0;
}


/*-----------------------------------------------*
 * Displays the measured detector current signal
 *-----------------------------------------------*/

static void
display_detector_current( void )
{
    int x;
    double val;
    double old_val = -1.0;
    double b, m;

    if ( measure_dc_signal( &val ) )
    {
        error_handling( );
        return;
    }

    if ( old_val == val )
        return;

    old_val = val;

    /* The displayed detector current doesn't cover the whole range of
       the signal but only a subrange. The following values are from a
       linear least square fit of the measured values. */

    if ( bmwb.type == Q_BAND )
    {
        b = 1.7954;
        m = 0.0467;
    }
    else
    {
        b =  2.4665;
        m = -0.1372;
    }

    val = b * val + m;

    if ( val > 1.0 )
        val = 1.0;
    else if ( val < 0.0 )
        val = 0.0;

    XCopyArea( dc_canvas.d, dc_canvas.pm, FL_ObjWin( dc_canvas.obj ),
               dc_canvas.gc, 0, 0, dc_canvas.w, dc_canvas.h, 0, 0 );

    x = 3 + irnd( val * ( dc_canvas.w - 6 ) );
    XFillRectangle( dc_canvas.d, FL_ObjWin( dc_canvas.obj ), dc_canvas.gc,
                    x - 2, 2, 4, dc_canvas.h - 4 );
}


/*-------------------------------------------*
 * Displays the measured AFC signal strength
 *-------------------------------------------*/

static void
display_afc_signal( void )
{
    int x;
    double val;
    double old_val = -2.0;

    if ( measure_afc_signal( &val ) )
    {
        error_handling( );
        return;
    }

    if ( old_val != val )
    {
        XCopyArea( afc_canvas.d, afc_canvas.pm, FL_ObjWin( afc_canvas.obj ),
                   afc_canvas.gc, 0, 0, afc_canvas.w, afc_canvas.h, 0, 0 );

        x = 3 + irnd( 0.5 * ( val + 1.0 ) * ( afc_canvas.w - 6 ) );
        XFillRectangle( afc_canvas.d, FL_ObjWin( afc_canvas.obj ),
                        afc_canvas.gc, x - 2, 2, 4, afc_canvas.h - 4 );

        old_val = val;
    }
}


/*--------------------------------------*
 * Displays the measured tune mode data
 *--------------------------------------*/

void
display_mode_pic( int mode )
{
    XCopyArea( tune_canvas.d, tune_canvas.pm, FL_ObjWin( tune_canvas.obj ),
               tune_canvas.gc, 0, 0, tune_canvas.w, tune_canvas.h, 0, 0 );

    if ( mode != MODE_TUNE )
        return;
}


/*---------------------------------------------------*
 * Determines if the device says it's locked or unlocked
 * and displays the result (X-band bridge only)
 *---------------------------------------------------*/

static void
display_unlocked( void )
{
    double val;
    static double old_val = -1.0;


    if ( measure_unlocked_signal( &val ) )
    {
        error_handling( );
        return;
    }

    if ( val != old_val )
    {
        fl_mapcolor( FL_FREE_COL1, 173 + irnd( 82.0 * val ),
                     irnd( 173 * ( 1.0 - val ) ), irnd( 173 * ( 1.0 - val ) ) );
        fl_redraw_object( bmwb.rsc->unlocked_indicator );
        old_val = val;
    }
}


/*--------------------------------------------*
 * Displays the degree of "uncalibrated-ness" 
 * (X-band bridge only)
 *--------------------------------------------*/

static void
display_uncalibrated( void )
{
    double val;
    static double old_val = -1.0;


    if ( measure_uncalibrated_signal( &val ) )
    {
        error_handling( );
        return;
    }

    if ( val != old_val )
    {
        fl_mapcolor( FL_FREE_COL2, 173 + irnd( 82.0 * val ),
                     irnd( 173 * ( 1.0 - val ) ), irnd( 173 * ( 1.0 - val ) ) );
        fl_redraw_object( bmwb.rsc->uncalibrated_indicator );
        old_val = val;
    }
}


/*---------------------------------------*
 * Displays if AFC is switched on or off
 * (Q-band bridge only)
 *---------------------------------------*/

static void
display_afc_state( void )
{
    int state;
    int old_state = -1;


    if ( measure_afc_state( &state ) )
    {
        error_handling( );
        return;
    }

    if ( state != old_state )
    {
        fl_mapcolor( FL_FREE_COL3, 255 * state, 0, 0 );
        fl_redraw_object( bmwb.rsc->afc_indicator );
        old_state = state;
    }
}


/*---------------------------------------*
 * Handler for canvas expose events
 *---------------------------------------*/

static int
canvas_expose( FL_OBJECT * obj,
               Window      win         UNUSED_ARG,
               int         win_width   UNUSED_ARG,
               int         win_height  UNUSED_ARG,
               XEvent    * xev         UNUSED_ARG,
               void      * user_data   UNUSED_ARG )
{
    if ( obj == tune_canvas.obj )
        display_mode_pic( bmwb.mode );
    else if ( obj == dc_canvas.obj )
        display_detector_current( );
    else if ( obj == afc_canvas.obj )
        display_afc_signal( );

    return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
