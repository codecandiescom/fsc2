/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
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

Canvas_T tune_canvas,
         afc_canvas,
         dc_canvas;


static void init_canvas( void );
static void display_detector_current( void );
static void display_afc_signal( void );
static void display_unleveled( void );
static void display_uncalibrated( void );
static void display_afc_state( void );
static int canvas_expose( FL_OBJECT * obj,
                          Window      win,
                          int         win_width,
                          int         win_height,
                          XEvent    * xev,
                          void      * user_data );
static int close_handler( FL_FORM * a,
                          void    * b );


/*----------------------------*
 * Initialization of graphics 
 *----------------------------*/

void
graphics_init( void )
{
	const char *title = bmwb.type == X_BAND ?
                        "X-Band Microwave Bridge" : "Q-Band Microwave Bridge";

	fl_set_idle_callback( update_display, NULL );
	bmwb.rsc = create_form_bmwb_rsc( );
    fl_set_atclose( close_handler, NULL );
	fl_show_form( bmwb.rsc->form, FL_PLACE_CENTER, FL_FULLBORDER, title );

    init_canvas( );
}


/*------------------------------------------------------------------------*
 * Function for initializing the canvases used in the graphical display -
 * these are the canvas for showing the mode in tune mode, the canvas for
 * displaying the detector current and the one for showing the AFC signal
 *------------------------------------------------------------------------*/

static void
init_canvas( void )
{
    int i;


    /* Initialize the canvas for the tune mode display */

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

    /* Initialize the canvas for displaying the detector current */

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


/*------------------------------------*
 * Callback for bridge mode selection
 *------------------------------------*/

void
mode_cb( FL_OBJECT * obj,
		 long        data  UNUSED_ARG )
{
	FL_POPUP_RETURN *r;


    pthread_mutex_lock( &bmwb.mutex );

    r =  fl_get_select_item( obj );

	if ( r->val != bmwb.mode )
    {
		if ( set_mode( r->val ) )
            fl_set_select_item( obj, fl_get_select_item_by_value( obj,
                                                                  bmwb.mode ) );
        else
        {
            /* Clear canvas if old mode was tune mode */

            if ( bmwb.mode != MODE_TUNE )
                display_mode_pic( r->val );

            bmwb.mode = r->val;

            display_detector_current( );
            display_afc_signal( );

            if ( bmwb.type == X_BAND )
            {
                display_unleveled( );
                display_uncalibrated( );
            }
            else
                display_afc_state( );
        }
    }

    pthread_mutex_unlock( &bmwb.mutex );

	update_display( NULL, NULL );
}


/*-----------------------------------------------*
 * Callback for the frequency slider and buttons
 *-----------------------------------------------*/

void
freq_cb( FL_OBJECT * obj  UNUSED_ARG,
		 long        data )
{
	double val;
	double freq;
	char buf[ 100 ];


    pthread_mutex_lock( &bmwb.mutex );

    val = fl_get_slider_value( bmwb.rsc->freq_slider );

    /* If 'data' is non-zero we're called for one of the buttons,
       calculate the new frequency */

	switch ( data )
	{
		case COARSE_DECREASE :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_max( 0.0, val - 0.05 );
			break;

		case FINE_DECREASE :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_max( 0.0, val - 0.005 );
			break;

		case FINE_INCREASE :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_min( 1.0, val + 0.005 );
			break;

		case COARSE_INCREASE :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_min( 1.0, val + 0.05 );
			break;
	}

    set_mw_freq( val );
    val = bmwb.freq;

    fl_set_slider_value( bmwb.rsc->freq_slider, val );

    freq = bmwb.min_freq + val * ( bmwb.max_freq - bmwb.min_freq );
    sprintf( buf, "(ca. %.3f GHz / %.0f G)", freq, FAC * freq );
    fl_set_object_label( bmwb.rsc->freq_text, buf );

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

    /* If 'data' is non-zero this is for one of the buttons */

	switch ( data )
	{
		case COARSE_DECREASE :
			if ( val <= MIN_ATTENUATION )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_max( MIN_ATTENUATION, val - 5 );
			break;

		case FINE_DECREASE :
			if ( val <= MIN_ATTENUATION )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_max( MIN_ATTENUATION, val - 1 );
			break;

		case FINE_INCREASE :
			if ( val >= MAX_ATTENUATION )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_min( MAX_ATTENUATION, val + 1 );
			break;

		case COARSE_INCREASE :
			if ( val >= MAX_ATTENUATION )
            {
                pthread_mutex_unlock( &bmwb.mutex );
                return;
            }

			val = d_min( MAX_ATTENUATION, val + 5 );
			break;
	}

    set_mw_attenuation( irnd( val ) );

    fl_set_counter_value( bmwb.rsc->attenuation_counter, bmwb.attenuation );

    fl_set_object_label( bmwb.rsc->attenuation_label,
                         pretty_print_attenuation( ) );

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

    /* If 'data' is non-zero this is for one of the buttons */

	switch ( data )
	{
		case COARSE_DECREASE :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_max( 0.0, val - 0.05 );
			break;

		case FINE_DECREASE :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_max( 0.0, val - 0.01 );
			break;

		case FINE_INCREASE :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_min( 1.0, val + 0.01 );
			break;

		case COARSE_INCREASE :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }
			val = d_min( 1.0, val + 0.05 );
			break;
	}

	set_signal_phase( val );

    val = bmwb.signal_phase;

    fl_set_slider_value( bmwb.rsc->signal_phase_slider, val );

	sprintf( buf, "Signal phase (%2d%%)", irnd( 100.0 * val ) );
	fl_set_object_label( bmwb.rsc->signal_phase_slider, buf );

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

    /* If 'data' is non-zero this is for one of the buttons */

	switch ( data )
	{
		case COARSE_DECREASE :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_max( 0.0, val - 0.05 );
			break;

		case FINE_DECREASE :
			if ( val <= 0.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_max( 0.0, val - 0.01 );
			break;

		case FINE_INCREASE :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_min( 1.0, val + 0.01 );
			break;

		case COARSE_INCREASE :
			if ( val >= 1.0 )
            {
                pthread_mutex_unlock( &bmwb.mutex );
				return;
            }

			val = d_min( 1.0, val + 0.05 );
			break;
	}

	set_mw_bias( val );

	val = bmwb.bias;

    fl_set_slider_value( bmwb.rsc->bias_slider, val );

	sprintf( buf, "Microwave bias (%2d%%)", irnd( 100.0 * val ) );
	fl_set_object_label( bmwb.rsc->bias_slider, buf );

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


/*-------------------------------------------------------------*
 * Function that gets invoked when the close button is clicked
 *-------------------------------------------------------------*/

void
quit_handler( FL_OBJECT * obj   UNUSED_ARG,
              long        data  UNUSED_ARG )
{
    close_handler( NULL, 0 );
}


/*-------------------------------------------------------------*
 * Function that gets invoked when the close button is clicked
 * or the close entry of the window menu is used
 *-------------------------------------------------------------*/

static int
close_handler( FL_FORM * a  UNUSED_ARG,
               void    * b  UNUSED_ARG )
{
    if ( fl_show_question( "Really switch off microwave bridge?", 0 ) == 0 )
        return FL_IGNORE;

    pthread_mutex_lock( &bmwb.mutex );

    /* Kill all threads that may still be running */

    if ( bmwb.c_is_active )
    {
        pthread_cancel( bmwb.c_thread );
        bmwb.c_is_active = 0;
    }

    if ( bmwb.a_is_active )
    {
        pthread_cancel( bmwb.a_thread );
        bmwb.a_is_active = 0;
    }

    /* Switch bridge to standby mode, stop iris motor and set maximum
       attenuation */

	set_mode( MODE_STANDBY );
    set_iris( 0 );
	set_mw_attenuation( MAX_ATTENUATION );

    /* Store it's current state in a file */

	save_state( );

    /* Release the Meilhaus card */

	meilhaus_finish( );

    /* Close the graphical user interface */

	fl_finish( );

    /* Delete the socket file */

    raise_permissions( );
    unlink( P_tmpdir "/bmwb.uds" );
    lower_permissions( );

    pthread_mutex_unlock( &bmwb.mutex );

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
    if ( bmwb.mode == MODE_STANDBY )
        return 0;

    pthread_mutex_lock( &bmwb.mutex );

	display_detector_current( );
	display_afc_signal( );

	if ( bmwb.type == X_BAND )
	{
		display_unleveled( );
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
    double val = 0.0;
    double old_val = -1.0;
    double b = bmwb.type == Q_BAND ?
               DC_SIGNAL_SLOPE_Q_BAND : DC_SIGNAL_SLOPE_X_BAND;
    double m = bmwb.type == Q_BAND ?
               DC_SIGNAL_OFFSET_Q_BAND : DC_SIGNAL_OFFSET_X_BAND;

    if ( bmwb.mode != MODE_STANDBY )
    {
        if ( measure_dc_signal( &val ) )
        {
            error_handling( );
            return;
        }

        if ( old_val == val )
            return;

        old_val = val;

        /* The displayed detector current doesn't cover the whole range of
           the signal but only a subrange. The values used are from a
           linear least square fit of the measured values. */

        val = b * val + m;

        if ( val > 1.0 )
            val = 1.0;
        else if ( val < 0.0 )
            val = 0.0;
    }

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
    double val = 0.0;
    double old_val = -2.0;


    if ( bmwb.mode != MODE_STANDBY && measure_afc_signal( &val ) )
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
    FL_COORD w;
    FL_COORD h;
    double *data = 0;
    XPoint *xp;
    FL_COORD i;


    XCopyArea( tune_canvas.d, tune_canvas.pm, FL_ObjWin( tune_canvas.obj ),
               tune_canvas.gc, 0, 0, tune_canvas.w, tune_canvas.h, 0, 0 );

    if ( mode != MODE_TUNE )
        return;

    fl_get_object_size( tune_canvas.obj, &w, &h );

    if (    ! ( data = malloc( w * sizeof *data ) )
         || ! ( xp = malloc( w * sizeof *xp ) ) )
    {
        if ( data )
            free( data );
        return;
    }

    if ( ! measure_tune_mode( data, w ) )
    {
        for ( i = 0; i < w; i++ )
        {
            xp[ i ].x = i;
            xp[ i ].y = 10 + irnd( ( h - 20 ) * ( 1.0 - data[ i ] ) );
        }

        XDrawLines( tune_canvas.d, FL_ObjWin( tune_canvas.obj ), tune_canvas.gc,
                    xp, w, CoordModeOrigin );
    }

    free( xp );
    free( data );
}


/*----------------------------------------------*
 * Determines if the device says it's unleveled
 * and displays the result (X-band bridge only)
 *----------------------------------------------*/

static void
display_unleveled( void )
{
    double val = 0.0;
    static double old_val = -1.0;


    if ( bmwb.type == Q_BAND )
        return;

    if ( bmwb.mode != MODE_STANDBY && measure_unleveled_signal( &val ) )
    {
        error_handling( );
        return;
    }

    if ( val != old_val )
    {
        fl_mapcolor( FL_FREE_COL1, 173 + irnd( 82.0 * val ),
                     irnd( 173 * ( 1.0 - val ) ), irnd( 173 * ( 1.0 - val ) ) );
        fl_redraw_object( bmwb.rsc->unleveled_indicator );
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
    double val = 0.0;
    static double old_val = -1.0;


    if ( bmwb.type == Q_BAND )
        return;

    if ( bmwb.mode != MODE_STANDBY && measure_uncalibrated_signal( &val ) )
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
    int state = 0;
    int old_state = -1;


    if ( bmwb.type == X_BAND )
        return;

    if ( bmwb.mode != MODE_STANDBY && measure_afc_state( &state ) )
    {
        error_handling( );
        return;
    }

    if ( state != old_state )
    {
        fl_mapcolor( FL_FREE_COL3, state ? FL_RED : 173,
                     state ? 0 : 173, state ? 0 : 173 );
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
    pthread_mutex_lock( &bmwb.mutex );


    if ( obj == tune_canvas.obj )
        display_mode_pic( bmwb.mode );
    else if ( obj == dc_canvas.obj )
        display_detector_current( );
    else if ( obj == afc_canvas.obj )
        display_afc_signal( );

    pthread_mutex_unlock( &bmwb.mutex );

    return 0;
}


/*----------------------------------------------*
 * Disables or enables the user control objects
 *----------------------------------------------*/

void
lock_objects( int state )
{
    static int is_locked = 0;
    void ( * f ) ( FL_OBJECT * ) =
                           state ? fl_deactivate_object : fl_activate_object;
    FL_COLOR c = state ? FL_INACTIVE : FL_BLACK;
	

    if ( state == is_locked )
        return;
    is_locked = state;

    fl_freeze_form( bmwb.rsc->form );

    f( bmwb.rsc->mode_select );
    fl_set_object_lcol( bmwb.rsc->mode_select, c );
    fl_set_select_text_color( bmwb.rsc->mode_select, c );
    f( bmwb.rsc->freq_group );
    fl_set_object_lcol( bmwb.rsc->freq_group, c );
    fl_set_object_lcol( bmwb.rsc->freq_text, c );
    f( bmwb.rsc->attenuation_group );
    fl_set_object_lcol( bmwb.rsc->attenuation_group, c );
    f( bmwb.rsc->signal_phase_group );
    fl_set_object_lcol( bmwb.rsc->signal_phase_group, c );
    f( bmwb.rsc->bias_group );
    fl_set_object_lcol( bmwb.rsc->bias_group, c );

    f( bmwb.rsc->iris_group );
    fl_set_object_lcol( bmwb.rsc->iris_group, c );

    fl_unfreeze_form( bmwb.rsc->form );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
