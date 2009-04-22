/*
 *  Copyright (C) 1999-2009 Anton Savitsky
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


#include "fsc2_module.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "ag54830b.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define AG54830B_TEST_REC_LEN        1000   /* in points */
#define AG54830B_TEST_NUM_AVG        16
#define AG54830B_TEST_TIME_BASE      0.1    /* in seconds*/
#define AG54830B_TEST_TIME_PER_POINT 1e-4   /* in seconds*/
#define AG54830B_TEST_SENSITIVITY    0.01   /* in Volts/div*/
#define AG54830B_TEST_CHANNEL_STATE  0      /* OFF*/
#define AG54830B_TEST_TRIG_POS       0.1    /* Pretrigger in Time Ranges*/

#define AG54830B_UNDEF -1
#define AG54830B_CH1    0          /* Normal channels     */
#define AG54830B_CH2    1
#define AG54830B_F1     2          /* Function channels */
#define AG54830B_F2     3
#define AG54830B_F3     4
#define AG54830B_F4     5
#define AG54830B_M1     6          /* Memory channels   */
#define AG54830B_M2     7
#define AG54830B_M3     8
#define AG54830B_M4     9
#define AG54830B_LIN    10         /* Line In (for triggger only) */
#define MAX_CHANNELS    11         /* number of channel names */

#define NUM_NORMAL_CHANNELS       ( AG54830B_CH2 + 1 )
#define NUM_MEAS_CHANNELS         ( AG54830B_F4  + 1 )
#define NUM_DISPLAYABLE_CHANNELS  ( AG54830B_M4  + 1 )


/* Declaration of exported functions */

int ag54830b_init_hook( void );
int ag54830b_exp_hook( void );
int ag54830b_end_of_exp_hook( void );

Var_T *digitizer_name(              Var_T * v );
Var_T *digitizer_show_channel(      Var_T * v );
Var_T *digitizer_timebase(          Var_T * v );
Var_T *digitizer_time_per_point(    Var_T * v );
Var_T *digitizer_sensitivity(       Var_T * v );
Var_T *digitizer_num_averages(      Var_T * v );
Var_T *digitizer_record_length(     Var_T * v );
Var_T *digitizer_trigger_position(  Var_T * v );
//Var_T *digitizer_meas_channel_ok( Var_T * v );
//Var_T *digitizer_trigger_channel( Var_T * v );
Var_T *digitizer_start_acquisition( Var_T * v );
Var_T *digitizer_get_curve(         Var_T * v );
Var_T *digitizer_get_curve_fast(    Var_T * v );
Var_T *digitizer_run(               Var_T * v );
Var_T *digitizer_get_xorigin(       Var_T * v );
Var_T *digitizer_get_xincrement(    Var_T * v );
Var_T *digitizer_get_yorigin(       Var_T * v );
Var_T *digitizer_get_yincrement(    Var_T * v );
Var_T *digitizer_command(           Var_T * v );


/* Locally used functions */

static bool   ag54830b_init( const char *name );
static bool   ag54830b_command( const char * cmd );
static bool   ag54830b_command_retry( const char * cmd );
static bool   ag54830b_talk( const char * cmd,
							 char       * reply,
							 long        *length );
static void   ag54830b_failure( void );
static double ag54830b_get_timebase( void );
static void   ag54830b_set_timebase( double timebase );
static long   ag54830b_get_num_avg( void );
static void   ag54830b_set_num_avg( long num_avg );
static int    ag54830b_get_acq_mode( void );
static void   ag54830b_set_record_length( long num_points );
static long   ag54830b_get_record_length( void );
static double ag54830b_get_sens( int channel );
static void   ag54830b_set_sens( int    channel,
								 double sens );
static long   ag54830b_translate_channel( int  dir,
										  long channel,
										  bool flag );
static void   ag54830b_acq_completed( void );
static void   ag54830b_get_curve( int       channel,
								  double ** data,
								  long    * length,
								  bool      get_scaling );
static double ag54830b_get_xorigin( int channel );
static double ag54830b_get_xincrement( int channel );
static double ag54830b_get_yorigin( int channel );
static double ag54830b_get_yincrement( int channel );
static void   ag54830b_set_trigger_pos( double pos );
static double ag54830b_get_trigger_pos( void );
static bool   ag54830b_display_channel_state( int channel );


static struct {
	int device;

	double timebase;
	bool is_timebase;

	double time_per_point;
	bool is_time_per_point;

	double sens[ NUM_NORMAL_CHANNELS ];
	double is_sens[ NUM_NORMAL_CHANNELS ];

	long num_avg;
	bool is_num_avg;

	long rec_len;
	bool is_rec_len;

	int meas_source;          /* channel selected as measurements source */
	int data_source;          /* channel selected as data source */

	double trig_pos;
	bool is_trig_pos;

	bool channel_is_on[ NUM_DISPLAYABLE_CHANNELS ];
	bool channels_in_use[ NUM_DISPLAYABLE_CHANNELS ];
} ag54830b;


enum {
	GENERAL_TO_AG54830B,
	AG54830B_TO_GENERAL
};

static bool acquisition_is_running;

/* Agilent 54830b limits */

static long min_rec_len = 16;
static long max_rec_len = 32768;
static long min_num_avg = 1;
static long max_num_avg = 4096;
static double min_timebase = 500e-12;
static double max_timebase = 20;
static double min_sens = 5;
static double min_sens50 = 1;
static double max_sens = 1e-3;

static const char *AG54830B_Channel_Names[ MAX_CHANNELS  ] = {
											"CHAN1", "CHAN2", "FUNC1", "FUNC2",
								 			"FUNC3", "FUNC4", "WMEM1", "WMEM2",
								 			"WMEM3", "WMEM4", "LINE" };
static bool in_init = UNSET;


/*-----------------------------------*
 * Init hook function for the module
 *-----------------------------------*/

int
ag54830b_init_hook( void )
{
	int i;

    /* Set global variable to indicate that GPIB bus is needed */

	Need_GPIB = SET;

	/* Initialize some variables in the digitizers structure */
	ag54830b.is_num_avg        = UNSET;
	ag54830b.is_time_per_point = UNSET;
	ag54830b.is_rec_len        = UNSET;
	ag54830b.is_trig_pos       = UNSET;
	ag54830b.data_source       = AG54830B_UNDEF;

	for ( i = 0; i < NUM_DISPLAYABLE_CHANNELS; i++ )
		ag54830b.channels_in_use[ i ] = UNSET;
    /* Reset several variables in the structure describing the device */

	ag54830b.device = -1;

	return 1;
}


/*--------------------------------------------------*
 * Start of experiment hook function for the module
 *--------------------------------------------------*/

int
ag54830b_exp_hook( void )
{
	/* Initialize the digitizer*/

	if ( ! ag54830b_init( DEVICE_NAME ) )
	{
		print( FATAL, "Initialization of device failed: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*
 * End of experiment hook function for the module
 *------------------------------------------------*/

int
ag54830b_end_of_exp_hook( void )
{
	/* Switch digitizer back to local mode */
	gpib_clear_device( ag54830b.device );
	ag54830b_command( ":CDIS;:RUN\n" );

	if ( ag54830b.device >= 0 )
		gpib_local( ag54830b.device );

	ag54830b.device = -1;

	return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_name( Var_T * v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------*
 * Switches the channe ON or OFF
 *-------------------------------*/

Var_T *
digitizer_show_channel( Var_T * v )
{
	long channel;
	bool state;
	char cmd[ 30 ];

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	channel = ag54830b_translate_channel( GENERAL_TO_AG54830B,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( channel >= MAX_CHANNELS )
		{
			print( FATAL, "Invalid channel number: %ld.\n",
				   ag54830b_translate_channel( AG54830B_TO_GENERAL,
											   channel, UNSET ) );
			THROW( EXCEPTION );
		}

		if( channel >= NUM_DISPLAYABLE_CHANNELS )
		{
			print( FATAL, "Channel \"%s\" can't be displayed.\n",
				   AG54830B_Channel_Names[ channel ] );
			THROW( EXCEPTION );
		}

	if ( ( v = vars_pop( v ) ) == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( ! ag54830b.channel_is_on[ channel ] )
					  no_query_possible( );
				return vars_push( INT_VAR, ag54830b.channel_is_on[ channel ] );

			case TEST :
				return vars_push( INT_VAR,
								  ag54830b.channels_in_use[ channel ] ?
								  ag54830b.channel_is_on[ channel ] :
								  AG54830B_TEST_CHANNEL_STATE );

			case EXPERIMENT :
				ag54830b.channel_is_on[ channel ] =
									 ag54830b_display_channel_state( channel );
				ag54830b.channels_in_use[ channel ] = SET;
				return vars_push( INT_VAR, ag54830b.channel_is_on[ channel ] );
		}

	  state = get_boolean( v );
	  /*print( SEVERE, "state is %ld   .\n", state);*/

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		if ( ag54830b_display_channel_state( channel ) != state )
			{
				sprintf( cmd, ":%s:DISP %i\n",
						 AG54830B_Channel_Names[ channel ], state );
				ag54830b_command( cmd );
			};

	ag54830b.channel_is_on[ channel ] = state;
	ag54830b.channels_in_use[ channel ] = SET;

	return vars_push( INT_VAR, ag54830b.channel_is_on[ channel ] );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var_T *
digitizer_timebase( Var_T * v )
{
	double timebase;

	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( ! ag54830b.is_timebase )
					no_query_possible( );
				return vars_push( FLOAT_VAR, ag54830b.timebase );

			case TEST :
				return vars_push( FLOAT_VAR, ag54830b.is_timebase ?
								 ag54830b.timebase : AG54830B_TEST_TIME_BASE );

			case EXPERIMENT :
				ag54830b.timebase = ag54830b_get_timebase( );
				return vars_push( FLOAT_VAR, ag54830b.timebase );
		}

	timebase = get_double( v, "time base" );

	if ( timebase <= 0 )
	{
		print( FATAL, "Invalid zero or negative time base: %6.3e.\n",
			    timebase );
		THROW( EXCEPTION );
	}

	if ( timebase > max_timebase )
		{
			print( SEVERE, "Timebase %6.3e too long, using %6.3e instead.\n",
				  timebase, max_timebase);
			timebase = max_timebase;
		}

		if ( timebase < min_timebase )
		{
			print( SEVERE, "Timebase %6.3e too short, using %6.3e instead.\n",
				    timebase, min_timebase );
			timebase = min_timebase;
	    }

	too_many_arguments( v );

	ag54830b.is_timebase = SET;

	if ( FSC2_MODE == EXPERIMENT )
		ag54830b_set_timebase( timebase );

	ag54830b.timebase = timebase;


	return vars_push( FLOAT_VAR, ag54830b.timebase );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var_T *
digitizer_time_per_point( Var_T * v )
{
	double tpp;

	UNUSED_ARGUMENT( v );
	tpp=0;
	switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( ! ag54830b.is_time_per_point )
					no_query_possible( );
				return vars_push( FLOAT_VAR, ag54830b.time_per_point );

			case TEST :
				return vars_push( FLOAT_VAR, ag54830b.is_time_per_point ?
								  ag54830b.timebase :
								  AG54830B_TEST_TIME_PER_POINT );

			case EXPERIMENT :
				ag54830b.timebase = ag54830b_get_timebase( );
				ag54830b.rec_len = ag54830b_get_record_length( );
				tpp=ag54830b.timebase *10.0 / ag54830b.rec_len;
				return vars_push( FLOAT_VAR, tpp );
		}

	return vars_push( FLOAT_VAR, tpp );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var_T *
digitizer_sensitivity( Var_T * v )
{
	long channel;
	double sens;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	channel = ag54830b_translate_channel( GENERAL_TO_AG54830B,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( channel >= NUM_NORMAL_CHANNELS )
	{
		print( FATAL, "Can't set or obtain sensitivity for channel %s.\n",
			   AG54830B_Channel_Names[ channel ] );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( ! ag54830b.is_sens[ channel ] )
					no_query_possible( );
				return vars_push( FLOAT_VAR, ag54830b.sens[ channel ] );

			case TEST :
				return vars_push( FLOAT_VAR, ag54830b.is_sens[ channel ] ?
								  ag54830b.sens[ channel ] :
								  AG54830B_TEST_SENSITIVITY );

			case EXPERIMENT :
				ag54830b.sens[ channel ] = ag54830b_get_sens( channel );
				ag54830b.is_sens[ channel ] = SET;
				return vars_push( FLOAT_VAR, ag54830b.sens[ channel ] );
		}

	sens = get_double( v, "sensitivity" );

	if ( sens < max_sens || sens > min_sens )
	{
		print( FATAL, "Sensitivity setting is out of range.\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		ag54830b_set_sens( channel, sens );

	ag54830b.sens[ channel ] = sens;
	ag54830b.is_sens[ channel ] = SET;

	return vars_push( FLOAT_VAR, ag54830b.sens[ channel ] );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var_T *
digitizer_num_averages( Var_T * v )
{
	long num_avg;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( ! ag54830b.is_num_avg )
					no_query_possible( );
				return vars_push( INT_VAR, ag54830b.num_avg );

			case TEST :
				return vars_push( INT_VAR, ag54830b.is_num_avg ?
								  ag54830b.num_avg : AG54830B_TEST_NUM_AVG );

			case EXPERIMENT :
				ag54830b.num_avg = ag54830b_get_num_avg( );
				return vars_push( INT_VAR, ag54830b.num_avg );
		}

	num_avg = get_long( v, "number of averages" );

	if ( num_avg > max_num_avg )
		{
			print( SEVERE, "Number of Averages %ld too big, using %ld "
				   "instead.\n", num_avg, max_num_avg );
			num_avg = max_num_avg;
		}

		if ( num_avg < min_num_avg )
		{
			print( SEVERE, "Record length %ld too small, using %ld instead.\n",
				   num_avg, min_num_avg );
			num_avg = min_num_avg;
	    }


	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		ag54830b_set_num_avg( num_avg );

	ag54830b.num_avg = num_avg;
	ag54830b.is_num_avg = SET;

	return vars_push( INT_VAR, ag54830b.num_avg );
}


/*------------------------------------------------------------------*
 * Function either sets or returns the current record length of the
 * digitizer. When trying to set a record length that does not fit
 * the possible settings the next larger is used instead.
 *------------------------------------------------------------------*/

Var_T *
digitizer_record_length( Var_T * v )
{
	long rec_len;


	if ( v == NULL )
	{

		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( ! ag54830b.is_rec_len )
					no_query_possible( );
				return vars_push( INT_VAR, ag54830b.rec_len );

			case TEST :
				return vars_push( INT_VAR, ag54830b.is_rec_len ?
								  ag54830b.rec_len : AG54830B_TEST_REC_LEN );

			case EXPERIMENT :
				ag54830b.rec_len = ag54830b_get_record_length( );
				return vars_push( INT_VAR, ag54830b.rec_len );
		}
	}

	rec_len = get_long( v, "record length" );

	if ( rec_len > max_rec_len )
	{
		print( SEVERE, "Record length  %ld too long, using %ld instead.\n",
			   rec_len, max_rec_len );
		rec_len = max_rec_len;
	}

	if ( rec_len < min_rec_len )
	{
		print( SEVERE, "Record length %ld too shot, using %ld instead.\n",
			   rec_len, min_rec_len );
		rec_len = min_rec_len;
	}

	ag54830b.is_rec_len = SET;

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		ag54830b_set_record_length( rec_len );
	else
		ag54830b.rec_len = rec_len;

	return vars_push( INT_VAR, ag54830b.rec_len );
}


/*------------------------------------------------------------------*
 * Function sets or gets trigger position (0-1) for full time scale
 *------------------------------------------------------------------*/

Var_T *
digitizer_trigger_position( Var_T * v )
{
	double trig_pos;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( ! ag54830b.is_trig_pos )
					no_query_possible( );
				return vars_push( FLOAT_VAR, ag54830b.trig_pos );

			case TEST :
				return vars_push( FLOAT_VAR, ag54830b.is_trig_pos ?
								 ag54830b.trig_pos : AG54830B_TEST_TRIG_POS );

			case EXPERIMENT :
				ag54830b.trig_pos = ag54830b_get_trigger_pos( );
				return vars_push( FLOAT_VAR, ag54830b.trig_pos );
		}

	trig_pos = get_double( v, "trigger position" );

	if ( trig_pos < 0.0 || trig_pos > 1.0 )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			print( FATAL, "Invalid trigger position %f, must be in interval "
				   "[0,1].\n", trig_pos );
			THROW( EXCEPTION );
		}

		print( SEVERE, "Invalid trigger position %f, using %f instead.\n",
			   trig_pos, trig_pos < 0 ? 0.0 : 1.0 );
		trig_pos = trig_pos < 0 ? 0.0 : 1.0;
	}

	too_many_arguments( v );

	ag54830b.is_trig_pos = SET;

	if ( FSC2_MODE == EXPERIMENT )
		ag54830b_set_trigger_pos( trig_pos );

	ag54830b.trig_pos = trig_pos;


	return vars_push( FLOAT_VAR, ag54830b.trig_pos );
}


/*-----------------------------*
 *  Function starts acqisition
 *-----------------------------*/

Var_T *
digitizer_start_acquisition( Var_T * v )
{
	UNUSED_ARGUMENT( v );

	if ( FSC2_MODE == EXPERIMENT )


	{
	    /*ag54830b_command( ":CDISPLAY;*CLS\n");*/
	    /*ag54830b_command( "*CLS; *SRE 32; *ESE 1\n");*/
	    /*gpib_clear_device( ag54830b.device );*/
		/*fsc2_usleep( 4000, UNSET );*/
		ag54830b_command_retry( "*CLS;:DIGITIZE;*OPC\n");
	    /*ag54830b_command( ":CHANNEL1:DISPLAY ON\n" );*/
		acquisition_is_running = SET;
		ag54830b_acq_completed();
	}

	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_get_curve( Var_T * v )
{
	int ch, i;
	double *array;
	long length;
	Var_T *nv;


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	ch = ( int ) ag54830b_translate_channel( GENERAL_TO_AG54830B,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( ch >= NUM_DISPLAYABLE_CHANNELS )
	{
		print( FATAL, "Invalid channel %s.\n", AG54830B_Channel_Names[ ch ] );
		THROW( EXCEPTION );
	}

	ag54830b.channels_in_use[ ch ] = SET;

	too_many_arguments( v );

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   array */

	if ( FSC2_MODE == EXPERIMENT )
	{
		ag54830b_get_curve( ch,  &array, &length, SET );
		nv = vars_push( FLOAT_ARR, array, length );
	}
	else
	{
		if ( ag54830b.is_rec_len  )
			length = ag54830b.rec_len;
		else
			length = AG54830B_TEST_REC_LEN;
		array = T_malloc( length * sizeof *array );
		for ( i = 0; i < length; i++ )
			array[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );
		nv = vars_push( FLOAT_ARR, array, length );
		nv->flags |= IS_DYNAMIC;
	}

	T_free( array );
	return nv;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
digitizer_get_curve_fast( Var_T * v )
{
	int ch, i;
	double *array;
	long length;
	Var_T *nv;


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	ch = ( int ) ag54830b_translate_channel( GENERAL_TO_AG54830B,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( ch >= NUM_DISPLAYABLE_CHANNELS )
	{
		print( FATAL, "Invalid channel %s.\n", AG54830B_Channel_Names[ ch ] );
		THROW( EXCEPTION );
	}

	ag54830b.channels_in_use[ ch ] = SET;

	too_many_arguments( v );

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   array */

	if ( FSC2_MODE == EXPERIMENT )
	{
		ag54830b_get_curve( ch,  &array, &length, UNSET );
		nv = vars_push( FLOAT_ARR, array, length );
	}
	else
	{
		if ( ag54830b.is_rec_len  )
			length = ag54830b.rec_len;
		else
			length = AG54830B_TEST_REC_LEN;
		array = T_malloc( length * sizeof *array );
		for ( i = 0; i < length; i++ )
			array[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );
		nv = vars_push( FLOAT_ARR, array, length );
		nv->flags |= IS_DYNAMIC;
	}

	T_free( array );
	return nv;
}


/*----------------------------------------------------*
 * Function switches digitizer to continuouse running
 *----------------------------------------------------*/

Var_T *
digitizer_run( Var_T * v )
{
	UNUSED_ARGUMENT( v );

	if ( FSC2_MODE == EXPERIMENT )
		ag54830b_command( ":CDIS;:RUN\n" );

	return vars_push( INT_VAR, 1L );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var_T *
digitizer_get_xorigin( Var_T * v )
{
	long channel;
	double xorg = 1.0;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	channel = ag54830b_translate_channel( GENERAL_TO_AG54830B,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( channel >= NUM_DISPLAYABLE_CHANNELS )
	{
		print( FATAL, "Invalid channel %s.\n",
			   AG54830B_Channel_Names[ channel ] );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		xorg = ag54830b_get_xorigin( channel );

	return vars_push( FLOAT_VAR, xorg );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var_T *
digitizer_get_xincrement( Var_T * v )
{
	long channel;
	double xinc = 1.0;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	channel = ag54830b_translate_channel( GENERAL_TO_AG54830B,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( channel >= NUM_DISPLAYABLE_CHANNELS )
	{
		print( FATAL, "Invalid channel %s.\n",
			   AG54830B_Channel_Names[ channel ] );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		xinc = ag54830b_get_xincrement(channel);

	return vars_push( FLOAT_VAR, xinc );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var_T *
digitizer_get_yorigin( Var_T * v )
{
	long channel;
	double yorg = 1.0;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	channel = ag54830b_translate_channel( GENERAL_TO_AG54830B,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( channel >= NUM_DISPLAYABLE_CHANNELS )
	{
		print( FATAL, "Invalid channel %s.\n",
			   AG54830B_Channel_Names[ channel ] );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		yorg = ag54830b_get_yorigin(channel);

	return vars_push( FLOAT_VAR, yorg );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var_T *
digitizer_get_yincrement( Var_T * v )
{
	long channel;
	double yinc = 1.0;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	channel = ag54830b_translate_channel( GENERAL_TO_AG54830B,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( channel >= NUM_DISPLAYABLE_CHANNELS )
	{
		print( FATAL, "Invalid channel %s.\n",
			   AG54830B_Channel_Names[ channel ] );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		yinc = ag54830b_get_yincrement(channel);

	return vars_push( FLOAT_VAR, yinc );
}

/*-----------------------------------------------------------*
 * Function for sending a GPIB command directly to digitizer
 *-----------------------------------------------------------*/

Var_T *
digitizer_command( Var_T * v )
{
	char *cmd = NULL;


	CLOBBER_PROTECT( cmd );

	vars_check( v, STR_VAR );

	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
			ag54830b_command( cmd );
			T_free( cmd );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( cmd );
			RETHROW( );
		}
	}

	return vars_push( INT_VAR, 1L );
}


/************************************************************************/

/*----------------------------------------------------*
 * Internal functions for initialization of digitizer
 *----------------------------------------------------*/

static bool
ag54830b_init( const char * name )
{
	if ( gpib_init_device( name, &ag54830b.device ) == FAILURE )
        return FAIL;

	gpib_clear_device( ag54830b.device );
	/* ag54830b_command(":ACQ:COMP 100\n"); */
	ag54830b_command(":WAV:BYT LSBF\n"); /*set byte order*/
	ag54830b_command(":WAV:FORM WORD\n"); /*set transfer format*/
	ag54830b_command(":WAV:VIEW ALL\n"); /*set all acquared data*/
	ag54830b_command(":TIM:REF LEFT\n"); /*set reference point to left*/
	ag54830b_command(":ACQ:COMP 100\n");
	in_init = SET;

	return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
ag54830b_command( const char * cmd )
{
	if ( gpib_write( ag54830b.device, cmd, strlen( cmd ) ) == FAILURE )
		ag54830b_failure( );

	fsc2_usleep( 4000, UNSET );

	return OK;
}

/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
ag54830b_command_retry( const char * cmd )
{
	if ( gpib_write( ag54830b.device, cmd, strlen( cmd ) ) == FAILURE )
		{
			print( SEVERE, "One more try.\n");
			fsc2_usleep( 4000, UNSET );
			if ( gpib_write( ag54830b.device, cmd, strlen( cmd ) ) == FAILURE )
			ag54830b_failure( );
		}


	/*if ( gpib_write( ag54830b.device, cmd, strlen( cmd ) ) == FAILURE )
	  ag54830b_failure( );*/

	fsc2_usleep( 4000, UNSET );

	return OK;
}

/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
ag54830b_talk( const char * cmd,
			   char       * reply,
			   long       * length )
{
	if ( gpib_write( ag54830b.device, cmd, strlen( cmd ) ) == FAILURE )
		ag54830b_failure( );

	fsc2_usleep( 4000, UNSET );

	if ( gpib_read( ag54830b.device, reply, length ) == FAILURE )
		ag54830b_failure( );

	fsc2_usleep( 4000, UNSET );

	return OK;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void
ag54830b_failure( void )
{
	print( FATAL, "Communication with Agilent failed.\n" );
	THROW( EXCEPTION );
}


/*---------------------------------*
 * Returns the digitizers timebase
 *---------------------------------*/

static double
ag54830b_get_timebase( void )
{
	char reply[ 30 ];
	long length = sizeof reply;


	ag54830b_talk( "TIMEBASE:SCALE?\n", reply, &length );
	reply[ length - 1 ] = '\0';
	return T_atod( reply );
}


/*------------------------------------*
 * Sets the timebase of the digitizer
 *------------------------------------*/

static void
ag54830b_set_timebase( double timebase )
{
	char cmd[ 40 ];


	sprintf( cmd, "TIMEBASE:SCALE  %8.3E\n", timebase );
	ag54830b_command( cmd );
}


/*-----------------------------------------*
 * Function returns the number of averages
 *-----------------------------------------*/

static long
ag54830b_get_num_avg( void )
{
	char reply[ 30 ];
	long length = sizeof reply;


	if ( ag54830b_get_acq_mode( ) == 1 )
	{
		ag54830b_talk(":ACQ:AVERAGE:COUNT?\n", reply, &length );
		reply[ length - 1 ] = '\0';
		return T_atol( reply );
	}
	else                            	/* digitizer is in sample mode */
		return 1;
}

/*--------------------------------------*
 * Function sets the number of averages
 *--------------------------------------*/

static void
ag54830b_set_num_avg( long num_avg )
{
	char cmd[ 30 ];


	/* With 1 as the number of acquisitions switch to sample mode, for all
	   others set the number of acquisitions and switch to average mode */

	if ( num_avg == 1 )
		ag54830b_command( ":ACQ:AVER OFF\n" );
	else
	{
		sprintf( cmd, ":ACQ:AVER:COUN %ld\n", num_avg );
		ag54830b_command( cmd );
		ag54830b_command( ":ACQ:AVER ON\n" );
	}

	/* Finally restart the digitizer */

	/*ag54830b_command( ":RUN\n" );*/
}


/*-------------------------------------------------------------------------*
 * Function returns the data acquisition mode. If the digitizer is neither
 * in average nor in sample mode, it is switched to sample mode.
 *-------------------------------------------------------------------------*/

static int
ag54830b_get_acq_mode( void )
{
	char reply[ 30 ];
	long length = sizeof reply;


	ag54830b_talk( ":ACQ:AVER?\n", reply, &length );
	return T_atol( reply );

/*  if ( reply == 1 )	 digitizer is in average mode */
/*		return AVERAGE;*/

/*	if ( reply == 0 )	 sample mode set it */
/*		return SAMPLE; */
}


/*----------------------------------------------*
 * Function returns if acquisition is complited
 *----------------------------------------------*/

static void
ag54830b_acq_completed( void )
{
	unsigned char stb = 0;

	if ( acquisition_is_running )
	{
		while ( 1 )
		{
			fsc2_usleep( 20000, UNSET );
			if ( gpib_serial_poll( ag54830b.device, &stb ) == FAILURE )
				ag54830b_failure( );

			if ( stb & 0x20 )
			{
				acquisition_is_running = UNSET;
				fsc2_usleep( 10000, UNSET );
				break;
			}

			stop_on_user_request( );
			/*fsc2_usleep( 10000, UNSET );*/
		}
	}
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
ag54830b_get_curve( int       channel,
					double ** data,
					long    * length,
					bool      get_scaling )
{
	char cmd[ 50 ];
	char reply[ 32 ];
	long blength = 32;
	char *buffer = NULL;
	long i;
	double yinc;
	double yorg;
	long bites;
	char cDATA[ 2 ];
	long BytesToRead;
	char header_str[ 10 ];


	CLOBBER_PROTECT ( buffer );

	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

	*data = NULL;

	/* Calculate the scale factor for converting the data returned by the
	   digitizer (2-byte integers) into real voltage levels */

    /* Set the source of the data */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command_retry( cmd );
		/*ag54830b_command(":WAV:BYT LSBF\n"); set byte order*/
		/*ag54830b_command(":WAV:FORM WORD\n"); set transfer format*/
		ag54830b.data_source = channel;
	 }

	/* Calculate how long the curve (with header) is going to be and allocate
       enough memory (data are 2-byte integers) and get all the adat bites*/

	TRY
	{
		ag54830b_command_retry(":WAV:DATA?\n");

		do
		{
			bites = 1;
			if ( gpib_read( ag54830b.device, cDATA, &bites ) == FAILURE )
				ag54830b_failure( );
		} while ( cDATA[0] != '#' ); /* find #*/

		bites = 1;
		if ( gpib_read( ag54830b.device, cDATA, &bites ) == FAILURE )
			ag54830b_failure( );

		cDATA[ 1 ] = '\0';
		BytesToRead = T_atoi(cDATA); /* read header number of bytes */

		if ( gpib_read( ag54830b.device, header_str, &BytesToRead )
			 													   == FAILURE )
			ag54830b_failure( );

		header_str[ BytesToRead ] = '\0';
		BytesToRead = T_atoi( header_str ); /* number of data bytes to read */

		if ( BytesToRead == 0 )
		{
		print( FATAL, "No signal in channel %s.\n",
			   AG54830B_Channel_Names[ channel ] );
		THROW( EXCEPTION );
		}
        /*print( SEVERE, "BytesToRead are %ld   .\n", BytesToRead);*/

		buffer = T_malloc( BytesToRead );

		/* read all data into buffer */

		if ( gpib_read( ag54830b.device, buffer, &BytesToRead ) == FAILURE )
			ag54830b_failure( );

		*length = BytesToRead / 2;
		*data = T_malloc( *length * sizeof **data );

        /* read termination */

		bites = 2;
		if ( gpib_read( ag54830b.device, cDATA, &bites ) == FAILURE )
			ag54830b_failure( );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( buffer )
			T_free( buffer );
		if ( *data )
			T_free( *data );
		RETHROW( );
	}

	/*print( SEVERE, "Channel is %ld   .\n", channel);*/
	/* get scaling */

	yinc = 1.0;
	yorg = 1.0;

	if (    get_scaling
		 && channel >= AG54830B_CH1
		 && channel < NUM_DISPLAYABLE_CHANNELS )
	{
		blength = sizeof reply;
		ag54830b_talk(":WAV:YINC?\n", reply, &blength ); /* get y-increment */
		reply[ blength - 1 ] = '\0';
		yinc = T_atod( reply );
		fsc2_usleep( 1000, UNSET );

		blength = sizeof reply;
		ag54830b_talk(":WAV:YOR?\n", reply, &blength );  /* get y-origin*/
		reply[ blength - 1 ] = '\0';
		yorg = T_atod( reply );
		fsc2_usleep( 1000, UNSET );
	}

	/* ....and copy them to the final destination (the data are INTEL format
	   2-byte integers, so the following requires sizeof( short ) == 2 and
	   only works on a machine with INTEL format - there got to be better ways
	   to do it...) Also scale data so that we get the real measured
	   voltage. */

	fsc2_assert( sizeof( short ) == 2 );

	for ( i = 0; i < BytesToRead / 2; i++ )
		*( *data + i ) =
					 ( yinc * ( double ) * ( ( short * ) buffer + i ) ) + yorg;

	T_free( buffer );
}


/*--------------------------------------------*
 * Function returns the current record length
 *--------------------------------------------*/

static long
ag54830b_get_record_length( void )
{
    char reply[ 30 ];
    long length = sizeof reply;


	ag54830b_talk( ":ACQ:POIN?\n", reply, &length );
    reply[ length - 1 ] = '\0';

    return T_atol( reply );
}


/*---------------------------------*
 * Function sets the record length
 *---------------------------------*/

static void
ag54830b_set_record_length( long num_points )
{
    char cmd[ 100 ];


	sprintf( cmd, ":ACQ:POINTS %ld\n", num_points );
	ag54830b_command( cmd );
}


/*-----------------------------------------------------------*
 * Returns the sensitivity of the channel passed as argument
 *-----------------------------------------------------------*/

static double
ag54830b_get_sens( int channel )
{
    char cmd[ 30 ];
    char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert( channel >= AG54830B_CH1 && channel < NUM_NORMAL_CHANNELS );

	sprintf( cmd, "%s:SCAL?\n", AG54830B_Channel_Names[ channel ] );
	ag54830b_talk( cmd, reply, &length );

    reply[ length - 1 ] = '\0';
	ag54830b.sens[ channel ] = T_atod( reply );

	return ag54830b.sens[ channel ];
}


/*--------------------------------------------------------*
 * Sets the sensitivity of one of the digitizers channels
 *--------------------------------------------------------*/

static void
ag54830b_set_sens( int channel, double sens )
{
    char cmd[ 40 ];
	char reply[ 40 ];
	long length = sizeof reply;


	fsc2_assert( channel >= AG54830B_CH1 && channel < NUM_NORMAL_CHANNELS );

	/* Some digitizers allow high sensitivities only for 1 MOhm input
	   impedance, not for 50 Ohm */

	if ( sens > min_sens50 )
	{
		sprintf( cmd, "CHAN%1d:INP?\n", channel );
		ag54830b_talk( cmd, reply, &length );

		if ( ! strncmp( reply, "DC50", 4 ) )
		{
			if ( in_init )
			{
				print( FATAL, "Sensitivity of %f V for channel %s too low "
					   "with input impedance set to  50 Ohm.\n",
					   AG54830B_Channel_Names[ channel ], sens );
				THROW( EXCEPTION );
			}

			print( SEVERE, "Sensitivity of %f V for channel %s too low "
				   "with input impedance set to 50 Ohm.\n",
				    AG54830B_Channel_Names[ channel ], sens );
			return;
		}
	}

	sprintf( cmd, "%s:SCAL %8.3E\n", AG54830B_Channel_Names[ channel ], sens );
	ag54830b_command( cmd );
}


/*-------------------------------------------------------*
 * Returns the Xorigin of the channel passed as argument
 *-------------------------------------------------------*/

static double
ag54830b_get_xorigin( int channel )
{
	char cmd[ 30 ];
	char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

	/* Set the source of the data if not set */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command( cmd );
		ag54830b.data_source = channel;
	 }

	ag54830b_talk( ":WAV:XOR?\n", reply, &length );
	ag54830b.data_source=channel;
    reply[ length - 1 ] = '\0';

	return  T_atod( reply );
}


/*----------------------------------------------------------*
 * Returns the Xincrement of the channel passed as argument
 *----------------------------------------------------------*/

static double
ag54830b_get_xincrement( int channel )
{
	char cmd[ 30 ];
	char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

    /* Set the source of the data if not set */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command( cmd );
		ag54830b.data_source = channel;
	}

	ag54830b_talk(  ":WAV:XINC?\n", reply, &length );
	ag54830b.data_source=channel;
    reply[ length - 1 ] = '\0';

	return  T_atod( reply );
}


/*-------------------------------------------------------*
 * Returns the Yorigin of the channel passed as argument
 *-------------------------------------------------------*/

static double
ag54830b_get_yorigin( int channel )
{
	char cmd[ 30 ];
	char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

	/* Set the source of the data if not set */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command( cmd );
		ag54830b.data_source = channel;
	 }

	ag54830b_talk( ":WAV:YOR?\n", reply, &length );
	ag54830b.data_source=channel;
    reply[ length - 1 ] = '\0';

	return  T_atod( reply );
}


/*----------------------------------------------------------*
 * Returns the Xincrement of the channel passed as argument
 *----------------------------------------------------------*/

static double
ag54830b_get_yincrement( int channel )
{
	char cmd[ 30 ];
	char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

    /* Set the source of the data if not set */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command( cmd );
		ag54830b.data_source = channel;
	 }

	ag54830b_talk(  ":WAV:YINC?\n", reply, &length );
	ag54830b.data_source=channel;
    reply[ length - 1 ] = '\0';

	return  T_atod( reply );
}


/*--------------------------------------------*
 * Function tests if a channel is switched on
 *--------------------------------------------*/

static bool
ag54830b_display_channel_state( int channel )
{
	char cmd[ 30 ];
    char reply[ 10 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

	sprintf( cmd, ":%s:DISP?\n", AG54830B_Channel_Names[ channel ] );
	ag54830b_talk( cmd, reply, &length );

	reply[ length - 1 ] = '\0';
	ag54830b.channel_is_on[ channel ] = T_atoi( reply );
	return ag54830b.channel_is_on[ channel ];
}


/*--------------------------------------------------------------*
 * Sets the trigger position, range of paramters is [0,1] where
 * 0 means no pretrigger while 1 indicates maximum pretrigger
 *--------------------------------------------------------------*/

static void
ag54830b_set_trigger_pos( double pos )
{
    char   reply[ 30 ];
    long   length = sizeof reply;
	double time_pos;
	double time_range;
	char   cmd[ 30 ];


	ag54830b_talk( ":TIM:RANG?\n", reply, &length );
    reply[ length - 1 ] = '\0';
	time_range = T_atod( reply );
	time_pos = - pos*time_range;
	/*print( SEVERE, "time_pos are  %6.3e   .\n", time_pos);*/
	sprintf( cmd, ":TIM:POS %8.3E\n", time_pos );
	ag54830b_command( cmd );
}


/*-------------------------------------------------------------------*
 * Returns the current trigger position in the intervall [0,1] where
 * 0 means no pretrigger while 1 indicates maximum pretrigger
 *-------------------------------------------------------------------*/

static double
ag54830b_get_trigger_pos( void )
{
    char   reply[ 30 ];
    long   length = sizeof reply;
	double time_pos;
	double time_range;


	ag54830b_talk( ":TIM:POS?\n", reply, &length );
    reply[ length - 1 ] = '\0';
	time_pos = - T_atod( reply );
	length = sizeof reply;
	ag54830b_talk( ":TIM:RANG?\n", reply, &length );
    reply[ length - 1 ] = '\0';
	time_range = T_atod( reply );

    return time_pos/time_range ;
}


/*--------------------------------------------------------------*
 * The function is used to translate back and forth between the
 * channel numbers the way the user specifies them in the EDL
 * program and the channel numbers as specified in the header
 * file. When the channel number can't be mapped correctly, the
 * way the function reacts depends on the value of the third
 * argument: If this is UNSET, an error message gets printed
 * and an exception is thrown. If it is SET -1 is returned to
 * indicate the error.
 *--------------------------------------------------------------*/

static long
ag54830b_translate_channel( int  dir,
							long channel,
							bool flag )
{
	if ( dir == GENERAL_TO_AG54830B )
	{
		switch ( channel )
		{
			case CHANNEL_CH1 :
				return AG54830B_CH1;

			case CHANNEL_CH2 :
				return AG54830B_CH2;

			case CHANNEL_F1 :
				return AG54830B_F1;

			case CHANNEL_F2 :
				return AG54830B_F2;

			case CHANNEL_F3 :
				return AG54830B_F3;

			case CHANNEL_F4 :
				return AG54830B_F4;

			case CHANNEL_M1 :
				return AG54830B_M1;

			case CHANNEL_M2 :
				return AG54830B_M2;

			case CHANNEL_M3 :
				return AG54830B_M3;

			case CHANNEL_M4 :
				return AG54830B_M4;

			case CHANNEL_LINE :
				return AG54830B_LIN;
		}

		if ( channel > CHANNEL_INVALID && channel < NUM_CHANNEL_NAMES )
		{
			if ( flag )
				return -1;
			print( FATAL, "Digitizer has no channel %s.\n",
				   Channel_Names[ channel ] );
			THROW( EXCEPTION );
		}

		if ( flag )
			return -1;
		print( FATAL, "Invalid channel number %ld.\n", channel );
		THROW( EXCEPTION );
	}
	else
	{
		switch ( channel )
		{
			case AG54830B_CH1 :
				return CHANNEL_CH1;

			case AG54830B_CH2 :
				return CHANNEL_CH2;

			case AG54830B_F1 :
				return CHANNEL_F1;

			case AG54830B_F2 :
				return CHANNEL_F2;

			case AG54830B_F3 :
				return CHANNEL_F3;

			case AG54830B_F4 :
				return CHANNEL_F4;

			case AG54830B_M1 :
				return CHANNEL_M1;

			case AG54830B_M2 :
				return CHANNEL_M2;

			case AG54830B_M3 :
				return CHANNEL_M3;

			case AG54830B_M4 :
				return CHANNEL_M4;

			case AG54830B_LIN :
				return CHANNEL_LINE;

			default :
				print( FATAL, "Internal error detected at %s:%d.\n",
					   __FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	return -1;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
