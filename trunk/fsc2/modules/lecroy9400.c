/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "lecroy9400.h"


/*--------------------------------*/
/* global variables of the module */
/*--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

LECROY9400 lecroy9400;

const char *LECROY9400_Channel_Names[ 9 ] = { "CH1", "CH2", "MEM_C", "MEM_D",
											  "FUNC_E", "FUNC_F", "LINE",
											  "EXT", "EXT10" };

double sset[ 10 ] = { 5.0e-3, 1.0e-2, 2.0e-2, 5.0e-2, 1.0e-1, 2.0e-1, 5.0e-1,
					  1.0, 2.0, 5.0 };

/* List of all timebases (in s/div) - currently only timebases that can be
   used in single shot mode are supported (i.e. neither random interleaved
   sampling nor roll mode) */

double tb[ 21 ] = {                      50.0e-9,
					100.0e-9, 200.0e-9, 500.0e-9,
					  1.0e-6,   2.0e-6,   5.0e-6,
					 10.0e-9,  20.0e-9,  50.0e-9,
					100.0e-6, 200.0e-6, 500.0e-6,
					  1.0e-3,   2.0e-3,   5.0e-3,
					 10.0e-3,  20.0e-3,  50.0e-3,
					100.0e-3, 200.0e-3 };

double tpp[ 21 ] = {                      10.0e-9,
					  10.0e-9,  10.0e-9,  10.0e-9,
					  10.0e-9,  10.0e-9,  10.0e-9,
					  10.0e-9,  10.0e-9,  20.0e-9,
					  40.0e-9,  80.0e-9, 200.0e-9,
					 400.0e-9, 800.0e-9,   2.0e-6,
					   4.0e-6,   8.0e-6,  20.0e-6,
					  40.0e-6,  80.0e-6 };

/* List of the corresponding sample rates, i.e. the time (in s) per point */

double sr[ 21 ] = {						 10.0e-9,
					 10.0e-9,  10.0e-9,  10.0e-9,
					 10.0e-9,  10.0e-9,  10.0e-9,
					 10.0e-9,  10.0e-9,  20.0e-9,
					 40.0e-9,  80.0e-9, 200.0e-9,
					400.0e-9, 800.0e-9,   2.0e-6,
					  4.0e-6,   8.0e-6,  20.0e-6,
					 40.0e-6,  80.0e-6 };

/* List of points per division */

int ppd[ 21 ] = { 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 2500,
				  2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500,
				  2500, 2500, 2500 };

/* List of number of averages that can be done using the WP01 Waveform
   Processing option */

long na[ 16 ] = { 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000,
				  20000, 50000, 100000, 200000, 500000, 1000000 };


/* Maximum curve lengths */

long cl[ 10 ] = { 50, 125, 250, 625, 1250, 2500, 6250, 12500, 25000, 32000 };


/* Memory limits for averaging, i.e. the maximum number of points that can be
   averaged for the different time bases - not too surprising this is always
   10 times the number of points per division, i.e. the number of points
   displayed */

long ml[ 21 ] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 25000,
				  25000, 25000, 25000, 25000, 25000, 25000, 25000, 25000,
				  25000, 25000, 25000 };

bool lecroy9400_IN_SETUP = UNSET;


static LECROY9400 lecroy9400_stored;

static Var *get_curve( Var *v, bool use_cursor );



/*******************************************/
/*   We start with the hook functions...   */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int lecroy9400_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Initialize some variables in the digitizers structure */

	lecroy9400.is_reacting = UNSET;
	lecroy9400.num_used_channels = 0;

	lecroy9400.w           = NULL;
	lecroy9400.is_timebase = UNSET;
	lecroy9400.is_trig_pos = UNSET;
	lecroy9400.num_windows = 0;
	lecroy9400.data_source = LECROY9400_UNDEF;
	lecroy9400.meas_source = LECROY9400_UNDEF;
	lecroy9400.lock_state  = SET;

	for ( i = LECROY9400_CH1; i <= LECROY9400_FUNC_F; i++ )
		lecroy9400.channels_in_use[ i ] = UNSET;
	for ( i = LECROY9400_CH1; i <= LECROY9400_CH2; i++ )
		lecroy9400.is_sens[ i ] = UNSET;
	for ( i = LECROY9400_MEM_C; i <= LECROY9400_FUNC_F; i++ )
	{
		lecroy9400.is_sens[ i ] = SET;
		lecroy9400.sens[ i ] = 1.0;
	}
	for ( i = LECROY9400_FUNC_E; i <= LECROY9400_FUNC_F; i++ )
	{
		lecroy9400.source_ch[ i ]  = LECROY9400_CH1 + i - LECROY9400_FUNC_E;
		lecroy9400.is_num_avg[ i ] = UNSET;
		lecroy9400.rec_len[ i ]    = UNDEFINED_REC_LEN;
		lecroy9400.is_reject[ i ]  = UNSET;
	}

	lecroy9400_stored.w = NULL;

	return 1;
}


/*-----------------------------------*/
/* Test hook function for the module */
/*-----------------------------------*/

int lecroy9400_test_hook( void )
{
	lecroy9400_store_state( &lecroy9400_stored, &lecroy9400 );
	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int lecroy9400_exp_hook( void )
{
	/* Reset structure describing the state of the digitizer to the one
	   it had before the test run was started */

	lecroy9400_store_state( &lecroy9400, &lecroy9400_stored );

	lecroy9400_IN_SETUP = SET;
	if ( ! lecroy9400_init( DEVICE_NAME ) )
	{
		print( FATAL, "Initialization of device failed: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}
	lecroy9400_IN_SETUP = UNSET;

	lecroy9400_do_pre_exp_checks( );

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int lecroy9400_end_of_exp_hook( void )
{
	lecroy9400_finished( );
	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void lecroy9400_exit_hook( void )
{
#if 0
	lecroy9400_delete_windows( &lecroy9400 );
	lecroy9400_delete_windows( &lecroy9400_stored );
#endif
}



/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------------------*/
/*------------------------------------------*/

#if 0
Var *digitizer_define_window( Var *v )
{
	double win_start = 0,
		   win_width = 0;
	bool is_win_start = UNSET;
	bool is_win_width = UNSET;
	WINDOW *w;


	if ( lecroy9400.num_windows >= MAX_NUM_OF_WINDOWS )
	{
		print( FATAL, "Maximum number of digitizer windows (%ld) exceeded.\n",
			   MAX_NUM_OF_WINDOWS );
		THROW( EXCEPTION );
	}

	if ( v != NULL )
	{
		/* Get the start point of the window */

		win_start = get_double( v, "window start position" );
		is_win_start = SET;

		/* If there's a second parameter take it to be the window width */

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			win_width = get_double( v, "window width" );

			/* Allow window width to be zero in test run... */

			if ( ( FSC2_MODE == TEST && win_width < 0.0 ) ||
				 ( FSC2_MODE != TEST && win_width <= 0.0 ) )
			{
				print( FATAL, "Zero or negative window width.\n" );
				THROW( EXCEPTION );
			}
			is_win_width = SET;

			too_many_arguments( v );
		}
	}

	/* Create a new window structure and append it to the list of windows */

	if ( lecroy9400.w == NULL )
	{
		lecroy9400.w = w = T_malloc( sizeof( WINDOW ) );
		w->prev = NULL;
	}
	else
	{
		w = lecroy9400.w;
		while ( w->next != NULL )
			w = w->next;
		w->next = T_malloc( sizeof( WINDOW ) );
		w->next->prev = w;
		w = w->next;
	}

	w->next = NULL;
	w->num = lecroy9400.num_windows++ + WINDOW_START_NUMBER;

	if ( is_win_start )
		w->start = win_start;
	w->is_start = is_win_start;

	if ( is_win_width )
		w->width = win_width;
	w->is_width = is_win_width;

	return vars_push( INT_VAR, w->num );
}
#endif


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_timebase( Var *v )
{
	double timebase;
	int TB = -1;
	unsigned int i;
	char *t;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				return vars_push( FLOAT_VAR, lecroy9400.is_timebase?
								  lecroy9400.timebase :
								  tb[ LECROY9400_TEST_TB_ENTRY ] );

			case EXPERIMENT :
				lecroy9400.timebase = lecroy9400_get_timebase( );
				lecroy9400.tb_index =
					            lecroy9400_get_tb_index( lecroy9400.timebase );
				lecroy9400.is_timebase = SET;
				return vars_push( FLOAT_VAR, lecroy9400.timebase );
		}

	if ( FSC2_MODE != PREPARATION )
	{
		print( FATAL, "Digitizer time base can only be set in the "
			   "PREPARATION section.\n" );
		THROW( EXCEPTION );
	}

	if ( lecroy9400.is_timebase )
	{
		print( FATAL, "Digitizer time base has already been set.\n" );
		THROW( EXCEPTION );
	}

	timebase = get_double( v, "time base" );

	if ( timebase <= 0 )
	{
		print( FATAL, "Invalid zero or negative time base: %s.\n",
			   lecroy9400_ptime( timebase ) );
		THROW( EXCEPTION );
	}

	/* Pick the allowed timebase nearest to the user supplied value */

	for ( i = 0; i < TB_ENTRIES - 1; i++ )
		if ( timebase >= tb[ i ] && timebase <= tb[ i + 1 ] )
		{
			TB = i +
				   ( ( tb[ i ] / timebase > timebase / tb[ i + 1 ] ) ? 0 : 1 );
			break;
		}

	if ( TB >= 0 &&                                         /* value found ? */
		 fabs( timebase - tb[ TB ] ) > timebase * 1.0e-2 )  /* error > 1% ?  */
	{
		t = T_strdup( lecroy9400_ptime( timebase ) );
		print( WARN, "Can't set timebase to %s, using %s instead.\n",
			   t, lecroy9400_ptime( tb[ TB ] ) );
		T_free( t );
	}

	if ( TB < 0 )                                   /* not found yet ? */
	{
		t = T_strdup( lecroy9400_ptime( timebase ) );

		if ( timebase < tb[ 0 ] )
		{
			TB = 0;
			print( WARN, "Timebase of %s is too low, using %s instead.\n",
				   t, lecroy9400_ptime( tb[ TB ] ) );
		}
		else
		{
		    TB = TB_ENTRIES - 1;
			print( WARN, "Timebase of %s is too large, using %s instead.\n",
				   t, lecroy9400_ptime( tb[ TB ] ) );
		}

		T_free( t );
	}

	lecroy9400.timebase = tb[ TB ];
	lecroy9400.tb_index = TB;
	lecroy9400.is_timebase = SET;

	return vars_push( FLOAT_VAR, lecroy9400.timebase );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_time_per_point( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( FLOAT_VAR, tpp[ lecroy9400.tb_index ] );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_sensitivity( Var *v )
{
	long channel;
	double sens;
	int coupl;


	if ( v == NULL )
	{
		print( FATAL, "No channel specified.\n" );
		THROW( EXCEPTION );
	}

	channel = lecroy9400_translate_channel( GENERAL_TO_LECROY9400,
						       get_strict_long( v, "channel number" ), UNSET );

	if ( channel > LECROY9400_CH2 )
	{
		print( FATAL, "Can't set or obtain sensitivity for channel %s.\n",
			   LECROY9400_Channel_Names[ channel ] );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				return vars_push( FLOAT_VAR, lecroy9400.is_sens[ channel ] ?
								             lecroy9400.sens[ channel ] :
								             LECROY9400_TEST_SENSITIVITY );

			case EXPERIMENT :
				lecroy9400.sens[ channel ] = lecroy9400_get_sens( channel );
				lecroy9400.is_sens[ channel ] = SET;
				return vars_push( FLOAT_VAR, lecroy9400.sens[ channel ] );
		}

	sens = get_double( v, "sensitivity" );

	too_many_arguments( v );

	/* Check that the sensitivity setting isn't out of range */

	if ( sens < LECROY9400_MAX_SENS )
	{
		print( FATAL, "Sensitivity setting is out of range.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ! lecroy9400.is_coupl[ channel ] )
			coupl = AC_1_MOHM;
		else
			coupl = lecroy9400.coupl[ channel ];
	}
	else
	{
		if ( ! lecroy9400.is_coupl[ channel ] )
		{
			coupl = lecroy9400.coupl[ channel ] =
				lecroy9400_get_coupling( channel );
			lecroy9400.is_coupl[ channel ] = SET;
		}
		else
			coupl = lecroy9400.coupl[ channel ];
	}

	if ( coupl == DC_50_OHM && sens > LECROY9400_MIN_SENS_50 )
	{
		print( FATAL, "Sensitivity is out of range for the current impedance "
			   "of 50 Ohm for %s.\n", LECROY9400_Channel_Names[ channel ] );
		THROW( EXCEPTION );
	}
	else if ( coupl != DC_50_OHM && sens > LECROY9400_MIN_SENS_1M )
	{
		print( FATAL, "Sensitivity setting is out of range.\n" );
		THROW( EXCEPTION );
	}

	lecroy9400.sens[ channel ] = sens;
	lecroy9400.is_sens[ channel ] = SET;

	if ( FSC2_MODE == EXPERIMENT )
		lecroy9400_set_sens( channel, sens );

	return vars_push( FLOAT_VAR, lecroy9400.sens[ channel ] );
}


/*------------------------------------------------------------------------*/
/* Function for setting up averaging: 1. argument is one of the function  */
/* channel doing the averaging, 2. argument is the source channel, either */
/* channel 1 or 2, 3. argument is the number of averages to be done, 4.   */
/* optional argument is either a number (as a truth value) or one of the  */
/* strings "ON" or "OFF" to indicate if overflow rejection should be used */
/* (defaults to off) and the 5. optional argument is the number of points */
/* to average (defaults to a number at least as large as the maximum      */
/* number of points that can be returned according to the time resolution */
/* setting, if set only that many data points will be returned in acqui-  */
/* sitions).                                                              */
/*------------------------------------------------------------------------*/

Var *digitizer_averaging( Var *v )
{
	long channel;
	long source_ch;
	long num_avg;
	long rec_len;
	bool reject;
	unsigned int i;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Get the channel to use for averaging */

	channel = lecroy9400_translate_channel( GENERAL_TO_LECROY9400,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( channel != LECROY9400_FUNC_E && channel != LECROY9400_FUNC_F )
	{
		print( FATAL, "Averaging can only be done using channels %s and %s.\n",
			   LECROY9400_Channel_Names[ LECROY9400_FUNC_E ],
			   LECROY9400_Channel_Names[ LECROY9400_FUNC_F ] );
		THROW( EXCEPTION );
	}

	/* Get the source channel */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing source channel argument.\n" );
		THROW( EXCEPTION );
	}

	source_ch = lecroy9400_translate_channel( GENERAL_TO_LECROY9400,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( source_ch != LECROY9400_CH1 && source_ch != LECROY9400_CH2 )
	{
		print( FATAL, "Averaging can only be done using channels %s and %s as "
			   "source channels.\n",
			   LECROY9400_Channel_Names[ LECROY9400_CH1 ],
			   LECROY9400_Channel_Names[ LECROY9400_CH2 ] );
		THROW( EXCEPTION );
	}

	/* Get the number of averages to use - adjust value if necessary to one
	   of the possible numbers of averages as given by the array 'na' */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing number of averages.\n" );
		THROW( EXCEPTION );
	}

	num_avg = get_long( v, "number of averages" );

	if ( num_avg <= 0 )
	{
		print( FATAL, "Zero or negative number of averages (%ld).\n",
			   num_avg );
		THROW( EXCEPTION );
	}

	i = 0;
	while ( 1 )
	{
		if ( i >= ( int ) NA_ENTRIES )
		{
			print( FATAL, "Number of averages (%ld) too large.\n", num_avg );
			THROW( EXCEPTION );
		}

		if ( num_avg == na[ i ] )
			break;

		if ( num_avg < na[ i ] )
		{
			print( SEVERE, "Can't set number of averages to %ld, using next "
				   "larger allowed value of %ld instead.\n",
				   num_avg, na[ i ] );
			num_avg = na[ i ];
			break;
		}

		i++;
	}

	/* If there is a further argument this has to be the overflow rejection
	   setting, otherwise overflow rejection is switched off and the record
	   length is set the to the default value. */

	reject = UNSET;
	rec_len = UNDEFINED_REC_LEN;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		reject = get_boolean( v );

		/* Last (optional) value is the number of points to use in averaging */

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			rec_len = get_long( v, "record length" );

			if ( rec_len <= 0 )
			{
				print( FATAL, "Invalid zero or negative record length.\n" );
				THROW( EXCEPTION );
			}

			if ( FSC2_MODE != EXPERIMENT && rec_len > ml[  ML_ENTRIES - 1 ] )
			{
				print( FATAL, "Record length %ld too long.\n",
					   rec_len );
				THROW( EXCEPTION );
			}
		}

		too_many_arguments( v );
	}

	lecroy9400_set_up_averaging( channel, source_ch, num_avg,
								 reject, rec_len );

	return vars_push( INT_VAR, 1L );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_num_averages( Var *v )
{
	long channel;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	if ( v->next != NULL )
	{
		print( FATAL, "Function can only be used for queries.\n" );
		THROW( EXCEPTION );
	}

	channel = lecroy9400_translate_channel( GENERAL_TO_LECROY9400,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( channel != LECROY9400_FUNC_E && channel != LECROY9400_FUNC_F )
	{
		print( FATAL, "Averaging can only be done using channels %s and %s.\n",
			   LECROY9400_Channel_Names[ LECROY9400_FUNC_E ],
			   LECROY9400_Channel_Names[ LECROY9400_FUNC_F ] );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( lecroy9400.is_num_avg[ channel ] )
			return vars_push( INT_VAR, lecroy9400.num_avg[ channel ] );
		else
			return vars_push( INT_VAR, LECROY9400_TEST_NUM_AVG );
	}
	else if ( FSC2_MODE == PREPARATION )
	{
		if ( lecroy9400.is_num_avg[ channel ] )
			return vars_push( INT_VAR, lecroy9400.num_avg[ channel ] );

		print( FATAL, "Function can only be used in the EXPERIMENT "
			   "section.\n" );
		THROW( EXCEPTION );
	}

	lecroy9400.num_avg[ channel ] = lecroy9400_get_num_avg( channel );
	if ( lecroy9400.num_avg[ channel ] != -1 )
		lecroy9400.is_num_avg[ channel ] = SET;
	return vars_push( INT_VAR, lecroy9400.num_avg[ channel ] );
}


/*--------------------------------------------------------------*/
/* Function returns the current record length of the digitizer. */
/*--------------------------------------------------------------*/

Var *digitizer_record_length( Var *v )
{
	long channel;


	if ( v == NULL )
	{
		print( FATAL, "Missing argument.\n" );
		THROW( EXCEPTION );
	}

	channel = lecroy9400_translate_channel( GENERAL_TO_LECROY9400,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( v->next == NULL )
	{
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( lecroy9400.rec_len[ channel ] == UNDEFINED_REC_LEN )
					no_query_possible( );
				return vars_push( INT_VAR,
								  lecroy9400.rec_len[ channel ] );

			case TEST :
				return vars_push( INT_VAR,
					lecroy9400.rec_len[ channel ] == UNDEFINED_REC_LEN ?
					lecroy9400.rec_len[ channel ] : LECROY9400_TEST_REC_LEN );

			case EXPERIMENT :
				return vars_push( INT_VAR, lecroy9400.rec_len[ channel ] );
		}
	}

	print( FATAL, "Currently the record length can only be queried\n" );
	THROW( EXCEPTION );

	return vars_push( INT_VAR, -1L );
}


/*---------------------------------------------------------------------*/
/* Function either sets or returns the amount of pretrigger as a value */
/* between 0 and 1, which, when multiplied by the record length gives  */
/* the number of points recorded before the trigger.                   */
/*---------------------------------------------------------------------*/

Var *digitizer_trigger_position( Var *v )
{
	double trig_pos;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				return vars_push( FLOAT_VAR, lecroy9400.is_trig_pos ?
								             lecroy9400.trig_pos :
								             LECROY9400_TEST_TRIG_POS );

			case EXPERIMENT :
				lecroy9400.trig_pos = lecroy9400_get_trigger_pos( );
				lecroy9400.is_trig_pos = SET;
				return vars_push( FLOAT_VAR, lecroy9400.trig_pos );
		}

	trig_pos = get_double( v, "trigger position" );

	if ( trig_pos < 0.0 || trig_pos > 1.0 )
	{
		print( FATAL, "Invalid trigger position: %f, must be in interval "
			   "[0,1].\n", trig_pos );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	lecroy9400.trig_pos = trig_pos;
	lecroy9400.is_trig_pos = SET;

	if ( FSC2_MODE == EXPERIMENT &&
		 ! lecroy9400_set_trigger_pos( lecroy9400.trig_pos ) )
		lecroy9400_gpib_failure( );

	return vars_push( FLOAT_VAR, lecroy9400.trig_pos );
}


/*----------------------------------------------------------------------*/
/* This is not a function that users should usually call but a function */
/* that allows other functions to check if a certain number stands for  */
/* channel that can be used in measurements. Normally, an exception     */
/* gets thrown (and an error message gets printed) when the channel     */
/* number isn't ok. Only when the function gets called with a second    */
/* argument it returns with either 0 or 1, indicating false or true.    */
/*----------------------------------------------------------------------*/

Var *digitizer_meas_channel_ok( Var *v )
{
	long channel;
	bool flag;


	flag = v->next != NULL;

	channel = lecroy9400_translate_channel( GENERAL_TO_LECROY9400,
								get_strict_long( v, "channel number" ), flag );

	if ( channel < 0 || channel > LECROY9400_FUNC_F )
		return vars_push( INT_VAR, 0L );
	else
		return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------*/
/* digitizer_set_trigger_channel() sets the channel that is used for */
/* triggering.                                                       */
/*-------------------------------------------------------------------*/

Var *digitizer_trigger_channel( Var *v )
{
	long channel;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				if ( lecroy9400.is_trigger_channel )
					return vars_push( INT_VAR, lecroy9400_translate_channel(
										  LECROY9400_TO_GENERAL,
										  lecroy9400.trigger_channel,
										  UNSET ) );
				else
					return vars_push( INT_VAR, lecroy9400_translate_channel(
										  LECROY9400_TO_GENERAL,
										  LECROY9400_TEST_TRIG_CHANNEL,
										  UNSET ) );
				break;

			case EXPERIMENT :
				return vars_push( INT_VAR, lecroy9400_translate_channel(
									  LECROY9400_TO_GENERAL,
									  lecroy9400_get_trigger_source( ),
									  UNSET) );
		}

	channel = lecroy9400_translate_channel( GENERAL_TO_LECROY9400,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( channel >= MAX_CHANNELS )
	{
		print( FATAL, "Invalid trigger channel name.\n" );
		THROW( EXCEPTION );
	}

    switch ( channel )
    {
        case LECROY9400_CH1 : case LECROY9400_CH2 : case LECROY9400_LIN :
		case LECROY9400_EXT : case LECROY9400_EXT10 :
			lecroy9400.trigger_channel = channel;
			lecroy9400.is_trigger_channel = SET;
			if ( FSC2_MODE == EXPERIMENT )
				lecroy9400_set_trigger_source( channel );
            break;

		default :
			print( FATAL, "Channel %s can't be used as trigger channel.\n",
				   LECROY9400_Channel_Names[ channel ] );
			THROW( EXCEPTION );
    }

	too_many_arguments( v );

	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_start_acquisition( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( FSC2_MODE == EXPERIMENT )
		lecroy9400_start_acquisition( );

	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_get_curve( Var *v )
{
	return get_curve( v, lecroy9400.w != NULL ? SET : UNSET );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_get_curve_fast( Var *v )
{
	return get_curve( v, UNSET );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *get_curve( Var *v, bool use_cursor )
{
	WINDOW *w;
	int ch, i;
	double *array = NULL;
	long length;
	Var *nv;
#if 0
	int j = 0;
#endif


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	ch = ( int ) lecroy9400_translate_channel( GENERAL_TO_LECROY9400,
							   get_strict_long( v, "channel number" ), UNSET );

	if ( ch < LECROY9400_CH1 ||
		 ( ch > LECROY9400_CH2 && ch < LECROY9400_FUNC_E ) ||
		 ch > LECROY9400_FUNC_F )
	{
		print( FATAL, "Invalid channel specification.\n" );
		THROW( EXCEPTION );
	}

	lecroy9400.channels_in_use[ ch ] = SET;

#if 0
	/* Now check if there's a variable with a window number and check it */

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		long win_num;

		j++;

		if ( lecroy9400.w == NULL )
		{
			print( FATAL, "No measurement windows have been defined.\n" );
			THROW( EXCEPTION );
		}

		win_num = get_strict_long( v, "window number" );

		for ( w = lecroy9400.w; w != NULL && w->num != win_num; w = w->next )
			/* empty */ ;

		if ( w == NULL )
		{
			print( FATAL, "%d. measurement window has not been defined.\n",
				   j );
			THROW( EXCEPTION );
		}
	}
	else
#endif
		w = NULL;

	too_many_arguments( v );

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   array */

	if ( FSC2_MODE == EXPERIMENT )
	{
		lecroy9400_get_curve( ch, w, &array, &length, use_cursor );
		nv = vars_push( FLOAT_ARR, array, length );
	}
	else
	{
		if ( lecroy9400.rec_len[ ch ] == UNDEFINED_REC_LEN )
			length = LECROY9400_TEST_REC_LEN;
		else
			length = lecroy9400.rec_len[ ch ];
		array = DOUBLE_P T_malloc( length * sizeof *array );
		for ( i = 0; i < length; i++ )
			array[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );
		nv = vars_push( FLOAT_ARR, array, length );
		nv->flags |= IS_DYNAMIC;
	}

	T_free( array );
	return nv;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_run( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( FSC2_MODE == EXPERIMENT )
		lecroy9400_free_running( );

	return vars_push( INT_VAR,1L );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_command( Var *v )
{
	char *cmd = NULL;


	CLOBBER_PROTECT( cmd );

	vars_check( v, STR_VAR );
	
	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
			lecroy9400_command( cmd );
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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
