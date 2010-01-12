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


static void graphics_init( void );


int update_display( XEvent * xev,
					void   * data );
void set_mw_freq( double val );
void set_mw_attenuation( int val );
void set_signal_phase( double val );
void set_mw_bias( double val );
void set_lock_phase( double val );
void set_iris( int state );
void set_mode( int mode );
void measure_and_show_detector_current( void );
void measure_and_show_afc_signal( void );
void measure_and_show_mode_pic( void );
void measure_and_show_unlocked( void );
void measure_and_show_uncalibrated( void );
void measure_and_show_afc( void );
void load_state( void );
void save_state( void );


BMWB bmwb;


static int gi = 0;


/*---------------------------------------------------*
 *---------------------------------------------------*/

int
main( int     argc,
	  char ** argv )
{
    bmwb.EUID = geteuid( );
    bmwb.EGID = getegid( );

    lower_permissions( );

    if ( ! fl_initialize( &argc, argv, "bmwb", NULL, 0 ) )
	{
		fprintf( stderr, "Failed to intialize graphics.\n" );
        return EXIT_FAILURE;
	}

	if ( meilhaus_init( ) )
	{
		fl_finish( );
		return EXIT_FAILURE;
	}

	load_state( );

	graphics_init( );

	fl_do_forms( );

	return 0;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static void
graphics_init( void )
{
	const char *title = bmwb.type == X_BAND ?
                        "X-Band Microwave Bridge" : "Q-Band Microwave Bridge";

	fl_set_idle_callback( update_display, NULL );
	bmwb.rsc = create_form_bmbw_rsc( );
	fl_show_form( bmwb.rsc->form, FL_PLACE_CENTER, FL_FULLBORDER, title );
    gi = 1;
}


/*---------------------------------------------------*
 * Callback for the mode selector
 *---------------------------------------------------*/

void
mode_cb( FL_OBJECT * obj,
		 long        data  UNUSED_ARG )
{
	FL_POPUP_RETURN *r =  fl_get_select_item( obj );

	if ( r->val != bmwb.mode )
		set_mode( r->val );
}


/*---------------------------------------------------*
 * Callback for frequency slider and buttons
 *---------------------------------------------------*/

void
freq_cb( FL_OBJECT * obj  UNUSED_ARG,
		 long        data )
{
	double val = fl_get_slider_value( bmwb.rsc->freq_slider );


	switch ( data )
	{
		case 1 :
			if ( val >= 1.0 )
				return;
			val = d_min( 1.0, val + 0.01 );
			break;

		case 2 :
			if ( val >= 1.0 )
				return;
			val = d_min( 1.0, val + 0.05 );
			break;

		case -1 :
			if ( val <= 0.0 )
				return;
			val = d_max( 0.0, val - 0.01 );
			break;

		case -2 :
			if ( val <= 0.0 )
				return;
			val = d_max( 0.0, val - 0.05 );
			break;
	}

	if ( data != 0 )
		fl_set_slider_value( bmwb.rsc->freq_slider, val );

	set_mw_freq( val );
}


/*---------------------------------------------------*
 * Callback for microwave attenuation counter and buttons
 *---------------------------------------------------*/

void
attenuation_cb( FL_OBJECT * obj  UNUSED_ARG,
				long        data )
{
	double val = fl_get_counter_value( bmwb.rsc->attenuation_counter );


	switch ( data )
	{
		case 1 :
			if ( val >= MAX_ATTENUATION )
				return;
			val = d_min( MAX_ATTENUATION, val + 1 );
			break;

		case 2 :
			if ( val >= MAX_ATTENUATION )
				return;
			val = d_min( MAX_ATTENUATION, val + 5 );
			break;

		case -1 :
			if ( val <= MIN_ATTENUATION )
				return;
			val = d_max( MIN_ATTENUATION, val - 1 );
			break;

		case -2 :
			if ( val <= MIN_ATTENUATION )
				return;
			val = d_max( MIN_ATTENUATION, val - 5 );
			break;
	}

	if ( data != 0 )
		fl_set_counter_value( bmwb.rsc->attenuation_counter, val );

	set_mw_attenuation( irnd( val ) );
}


/*---------------------------------------------------*
 * Callback for the microwave attenuation button hidden
 * and on top of the attenuation counter's data fielda
 *---------------------------------------------------*/

void
attenuation_button_cb( FL_OBJECT * obj   UNUSED_ARG,
					   long        data  UNUSED_ARG )
{
	/* popup window to edit the attenuation setting */
	update_display( NULL, NULL );
}


/*---------------------------------------------------*
 * Callback for signal phase slider and buttons
 *---------------------------------------------------*/

void
signal_phase_cb( FL_OBJECT * obj  UNUSED_ARG,
				 long        data )
{
	double val = fl_get_slider_value( bmwb.rsc->signal_phase_slider );


	switch ( data )
	{
		case 1 :
			if ( val >= 1.0 )
				return;
			val = d_min( 1.0, val + 0.01 );
			break;

		case 2 :
			if ( val >= 1.0 )
				return;
			val = d_min( 1.0, val + 0.05 );
			break;

		case -1 :
			if ( val <= 0.0 )
				return;
			val = d_max( 0.0, val - 0.01 );
			break;

		case -2 :
			if ( val <= 0.01 )
				return;
			val = d_max( 0.0, val - 0.05 );
			break;
	}

	if ( data != 0 )
		fl_set_slider_value( bmwb.rsc->signal_phase_slider, val );

	set_signal_phase( val );
}


/*---------------------------------------------------*
 * Callback for microwave bias slider and buttons
 *--------------------------------------------------*/

void
bias_cb( FL_OBJECT * obj  UNUSED_ARG,
		 long        data )
{
	double val = fl_get_slider_value( bmwb.rsc->bias_slider );


	switch ( data )
	{
		case 1 :
			if ( val >= 1.0 )
				return;
			val = d_min( 1.0, val + 0.01 );
			break;

		case 2 :
			if ( val >= 1.0 )
				return;
			val = d_min( 1.0, val + 0.05 );
			break;

		case -1 :
			if ( val <= 0.0 )
				return;
			val = d_max( 0.0, val - 0.01 );
			break;

		case -2 :
			if ( val <= 0.01 )
				return;
			val = d_max( 0.0, val - 0.05 );
			break;
	}

	if ( data != 0 )
		fl_set_slider_value( bmwb.rsc->bias_slider, val );

	set_mw_bias( val );
}


/*---------------------------------------------------*
 * Callback for lock phase slider and buttons
 *---------------------------------------------------*/

void
lock_phase_cb( FL_OBJECT * obj  UNUSED_ARG,
			   long        data )
{
	double val = fl_get_slider_value( bmwb.rsc->lock_phase_slider );


	switch ( data )
	{
		case 1 :
			if ( val >= 1.0 )
				return;
			val = d_min( 1.0, val + 0.01 );
			break;

		case 2 :
			if ( val >= 1.0 )
				return;
			val = d_min( 1.0, val + 0.05 );
			break;

		case -1 :
			if ( val <= 0.0 )
				return;
			val = d_max( 0.0, val - 0.01 );
			break;

		case -2 :
			if ( val <= 0.01 )
				return;
			val = d_max( 0.0, val - 0.05 );
			break;
	}

	if ( data != 0 )
		fl_set_slider_value( bmwb.rsc->lock_phase_slider, val );

	set_lock_phase( val );
}


/*---------------------------------------------------*
 * Callback for the iris buttons
 *---------------------------------------------------*/

void
iris_cb( FL_OBJECT * obj  UNUSED_ARG,
		 long        data )
{
	static int state = 0;


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
}


/*---------------------------------------------------*
 * Callback for the quit button
 *---------------------------------------------------*/

void
quit_cb( FL_OBJECT * obj   UNUSED_ARG,
		 long        data  UNUSED_ARG )
{
	set_mode( MODE_STANDBY );
	save_state( );
	meilhaus_finish( );
	fl_finish( );
	exit( EXIT_SUCCESS );
}


/*---------------------------------------------------*
 * Updates the detector current, AFC signal and,
 * when in tune mode, the tune canvas
 *---------------------------------------------------*/

int
update_display( XEvent * xev   UNUSED_ARG ,
				void   * data  UNUSED_ARG )
{
	measure_and_show_detector_current( );
	measure_and_show_afc_signal( );

	if ( bmwb.type == X_BAND )
	{
		measure_and_show_unlocked( );
		measure_and_show_uncalibrated( );
	}
	else
		measure_and_show_afc( );

	if ( bmwb.mode == MODE_TUNE )
		measure_and_show_mode_pic( );

	return 0;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
set_mw_freq( double val )
{
	double freq;
	char buf[ 100 ];

	bmwb.freq = val;

	freq = bmwb.min_freq + val * ( bmwb.max_freq - bmwb.min_freq );
	sprintf( buf, "(%.3f GHz / %.0f G)", freq, FAC * freq );
	fl_set_object_label( bmwb.rsc->freq_text, buf );

	update_display( NULL, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
set_mw_attenuation( int val )
{
	bmwb.attenuation = val;

	update_display( NULL, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
set_signal_phase( double val )
{
	bmwb.signal_phase = val;

	update_display( NULL, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
set_mw_bias( double val )
{
	bmwb.bias = val;

	update_display( NULL, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
set_lock_phase( double val )
{
	bmwb.lock_phase = val;

	update_display( NULL, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
set_iris( int state )
{
	update_display( NULL, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
set_mode( int mode )
{
	/* Switch to new mode */

	/* Clear canvas if old mode was tune mode */

	if ( bmwb.mode == MODE_TUNE )
	{
	}

	bmwb.mode = mode;
	update_display( NULL, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
measure_and_show_detector_current( void )
{
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
measure_and_show_afc_signal( void )
{
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
measure_and_show_mode_pic( void )
{
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
measure_and_show_unlocked( void )
{
    if ( gi )
    {
        fl_mapcolor( FL_FREE_COL1, 255 * bmwb.unlocked_state, 0, 0 );
        fl_redraw_object( bmwb.rsc->unlocked_indicator );
    }
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
measure_and_show_uncalibrated( void )
{
    static int i = 0;

    bmwb.uncalibrated = 1.5 * sin( i++ / 30.0 ) + 1.0;

    if ( gi )
    {
        int r = irnd( 255 * ( bmwb.uncalibrated - UNCALIBRATED_MIN )
                      / ( UNCALIBRATED_MAX - UNCALIBRATED_MIN ) );

        fl_mapcolor( FL_FREE_COL2, r, 0, 0 );
        fl_redraw_object( bmwb.rsc->uncalibrated_indicator );
    }
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

void
measure_and_show_afc( void )
{

    if ( gi )
    {
        fl_mapcolor( FL_FREE_COL3, 255 * bmwb.afc_state, 0, 0 );
        fl_redraw_object( bmwb.rsc->afc_indicator );
    }
}


/*---------------------------------------------------*
 * Reloads the last safed state of the bridge from a file
 *---------------------------------------------------*/

void
load_state( void )
{
	FILE *fp = NULL;
	char *fn;


	bmwb.type = X_BAND;   /* TEST ONLY */
    bmwb.afc_state = 1;   /* TEST ONLY */

	bmwb.mode = -1;
	set_mode( MODE_STANDBY );
	set_mw_attenuation( MAX_ATTENUATION );

	if ( bmwb.type == X_BAND )
	{
		bmwb.min_freq = X_BAND_MIN_FREQ;
		bmwb.max_freq = X_BAND_MAX_FREQ;
	}
	else
	{
		bmwb.min_freq = Q_BAND_MIN_FREQ;
		bmwb.max_freq = Q_BAND_MAX_FREQ;
	}

	if ( bmwb.type == X_BAND )
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_X_BAND_STATE_FILE );
	else
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_Q_BAND_STATE_FILE );

    if (    ! fn
		 || ( fp = bmwb_fopen( fn, "r" ) ) == NULL
		 || fscanf( fp, "%lf %lf %lf %lf", &bmwb.freq, &bmwb.signal_phase,
					&bmwb.bias, &bmwb.lock_phase ) != 4
		 || ! ( bmwb.freq >= 0.0 || bmwb.freq <= 1.0 )
		 || ! ( bmwb.signal_phase >= 0.0 || bmwb.signal_phase <= 1.0 )
		 || ! ( bmwb.bias >= 0.0 || bmwb.bias <= 1.0 )
		 || ! ( bmwb.lock_phase >= 0.0 || bmwb.lock_phase <= 1.0 ) )
	{
		bmwb.freq         = 0.0;
		bmwb.signal_phase = 0.0;
		bmwb.bias         = 0.0;
		bmwb.lock_phase   = 0.0;
    }

	if ( fn )
		free( fn );

	if ( fp )
		fclose( fp );
}


/*---------------------------------------------------*
 * Safes the current state of the bridge to a file
 *---------------------------------------------------*/

void
save_state( void )
{
	char *fn;
	FILE *fp;


	if ( bmwb.type == X_BAND )
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_X_BAND_STATE_FILE );
	else
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_Q_BAND_STATE_FILE );

    if ( fn && ( fp = bmwb_fopen( fn, "r" ) ) != NULL )
	{
		fprintf( fp, "%f %f %f %f\n", bmwb.freq, bmwb.signal_phase,
				 bmwb.bias, bmwb.lock_phase );
		fclose( fp );
	}

	if ( fn )
		free( fn );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
