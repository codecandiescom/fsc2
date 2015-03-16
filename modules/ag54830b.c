/*
 *  Copyright (C) 1999-2014 Anton Savitsky / Jens Thoms Toerring
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


#include "ag54830b.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


AG54830B_T ag54830b;
const char *AG54830B_Channel_Names[ MAX_CHANNELS  ] = {
											"CHAN1", "CHAN2", "FUNC1", "FUNC2",
								 			"FUNC3", "FUNC4", "WMEM1", "WMEM2",
								 			"WMEM3", "WMEM4", "LINE" };




/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

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

    /* Reset several variables in the structure describing the device */

	for ( i = 0; i < NUM_DISPLAYABLE_CHANNELS; i++ )
		ag54830b.channels_in_use[ i ] = UNSET;

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
		print( FATAL, "Initialization of device failed: %s.\n",
			   gpib_last_error( ) );
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
digitizer_name( Var_T * v  UNUSED_ARG )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------*
 * Switches a channel on or off
 *------------------------------*/

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

	too_many_arguments( v );

	if (    FSC2_MODE == EXPERIMENT
		 && ag54830b_display_channel_state( channel ) != state )
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

	if ( timebase > MAX_TIMEBASE )
	{
		print( SEVERE, "Timebase %6.3e too long, using %6.3e instead.\n",
			   timebase, MAX_TIMEBASE );
		timebase = MAX_TIMEBASE;
	}

	if ( timebase < MIN_TIMEBASE )
	{
		print( SEVERE, "Timebase %6.3e too short, using %6.3e instead.\n",
			   timebase, MIN_TIMEBASE );
		timebase = MIN_TIMEBASE;
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
digitizer_time_per_point( Var_T * v  UNUSED_ARG )
{
	double tpp = 0.0;


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

	if ( sens < MAX_SENS || sens > MIN_SENS )
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

	if ( num_avg > MAX_NUM_AVG )
	{
		print( SEVERE, "Number of Averages %ld too big, using %ld "
			   "instead.\n", num_avg, MAX_NUM_AVG );
		num_avg = MAX_NUM_AVG;
	}

	if ( num_avg < MIN_NUM_AVG )
	{
		print( SEVERE, "Record length %ld too small, using %ld instead.\n",
			   num_avg, MIN_NUM_AVG );
		num_avg = MIN_NUM_AVG;
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

	rec_len = get_long( v, "record length" );

	if ( rec_len > MAX_REC_LEN )
	{
		print( SEVERE, "Record length  %ld too long, using %ld instead.\n",
			   rec_len, MAX_REC_LEN );
		rec_len = MAX_REC_LEN;
	}

	if ( rec_len < MIN_REC_LEN )
	{
		print( SEVERE, "Record length %ld too short, using %ld instead.\n",
			   rec_len, MIN_REC_LEN );
		rec_len = MIN_REC_LEN;
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


/*------------------------------*
 *  Function starts acquisition
 *------------------------------*/

Var_T *
digitizer_start_acquisition( Var_T * v  UNUSED_ARG )
{
	if ( FSC2_MODE == EXPERIMENT )
	{
		ag54830b_command_retry( "*CLS;:DIGITIZE;*OPC\n");
		ag54830b.acquisition_is_running = SET;
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


/*-----------------------------------------------------*
 *  Function switches digitizer to continuouse running
 *-----------------------------------------------------*/

Var_T *
digitizer_run( Var_T * v  UNUSED_ARG )
{
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
			RETHROW;
		}
	}

	return vars_push( INT_VAR, 1L );
}


 /*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
