/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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



#define TDS520_MAIN

#include "tds520.h"

const char generic_type[ ] = DEVICE_TYPE;


/* This array must be set to the available record lengths of the digitizer
   and must always end with a 0 */

static long record_lengths[ ] = { 500, 1000, 2500, 5000, 15000, 0 };

/* List of all allowed time base values (in seconds) */

static double tb[ ] = {                     500.0e-12,
						  1.0e-9,   2.0e-9,   5.0e-9,
						 10.0e-9,  20.0e-9,  50.0e-9,
						100.0e-9, 200.0e-9, 400.0e-9,
						  1.0e-6,   2.0e-6,   5.0e-6,
						 10.0e-6,  20.0e-6,  50.0e-6,
						100.0e-6, 200.0e-6, 500.0e-6,
						  1.0e-3,   2.0e-3,   5.0e-3,
						 10.0e-3,  20.0e-3,  50.0e-3,
						100.0e-3, 200.0e-3, 500.0e-3,
						  1.0,      2.0,      5.0,
						 10.0 };

#define TB_ENTRIES ( sizeof tb / sizeof tb[ 0 ] )

/* Maximum and minimum sensitivity settings (in V) of the measurement
   channels */

static double max_sens = 1e-3,
              min_sens = 10.0;


static Var *get_area( Var *v, bool use_cursor );
static Var *get_curve( Var *v, bool use_cursor );
static Var *get_amplitude( Var *v, bool use_cursor );


static TDS520 tds520_stored;


/*******************************************/
/*   We start with the hook functions...   */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int tds520_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Initialize some variables in the digitizers structure */

	tds520.is_reacting = UNSET;
	tds520.w           = NULL;
	tds520.is_timebase = UNSET;
	tds520.is_num_avg  = UNSET;
	tds520.is_rec_len  = UNSET;
	tds520.is_trig_pos = UNSET;
	tds520.num_windows = 0;
	tds520.data_source = TDS520_UNDEF;
	tds520.meas_source = TDS520_UNDEF;
	tds520.lock_state  = SET;

	for ( i = TDS520_CH1; i <= TDS520_CH2; i++ )
		tds520.is_sens[ i ] = UNSET;
	for ( i = TDS520_MATH1; i <= TDS520_REF4; i++ )
	{
		tds520.is_sens[ i ] = SET;
		tds520.sens[ i ] = 1.0;
	}

	tds520_stored.w = NULL;

	return 1;
}


/*-----------------------------------*/
/* Test hook function for the module */
/*-----------------------------------*/

int tds520_test_hook( void )
{
	/* Store the state of the digitizer structure it was set to in the
	   PREPARATIONS section */

	tds520_store_state( &tds520_stored, &tds520 );
	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int tds520_exp_hook( void )
{
	/* Reset the digitizer structure to the state it was set to in the
	   preparations section - changes done to it in the test run are to
	   be undone... */

	tds520_store_state( &tds520, &tds520_stored );

	tds520_do_pre_exp_checks( );

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int tds520_end_of_exp_hook( void )
{
	tds520_finished( );
	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void tds520_exit_hook( void )
{
	tds520_delete_windows( &tds520 );
	tds520_delete_windows( &tds520_stored );
}



/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------------------*/
/*------------------------------------------*/

Var *digitizer_define_window( Var *v )
{
	double win_start = 0,
		   win_width = 0;
	bool is_win_start = UNSET;
	bool is_win_width = UNSET;
	WINDOW *w;


	if ( tds520.num_windows >= MAX_NUM_OF_WINDOWS )
	{
		eprint( FATAL, SET, "%s: Maximum number of digitizer windows (%ld) "
				"exceeded.\n", DEVICE_NAME, MAX_NUM_OF_WINDOWS );
		THROW( EXCEPTION )
	}

	if ( v != NULL )
	{
		/* Get the start point of the window */

		vars_check( v, INT_VAR | FLOAT_VAR );
		win_start = VALUE( v );
		is_win_start = SET;
		v = vars_pop( v );

		/* If there's a second parameter take it to be the window width */

		if ( v != NULL )
		{
			vars_check( v, INT_VAR | FLOAT_VAR );
			win_width = VALUE( v );

			/* Allow window width to be zero in test run... */

			if ( ( FSC2_MODE == TEST && win_width < 0.0 ) ||
				 ( FSC2_MODE =! TEST && win_width <= 0.0 ) )
			{
				eprint( FATAL, SET, "%s: Zero or negative width for "
						"window in %s.\n", DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
			is_win_width = SET;

			if ( ( v = vars_pop( v ) ) != NULL )
			{
				eprint( WARN, SET, "%s: Superfluous arguments in call of "
						"function %s().\n", DEVICE_NAME, Cur_Func );

				while ( ( v = vars_pop( v ) ) != NULL )
					;
			}
		}
	}

	/* Create a new window structure and append it to the list of windows */

	if ( tds520.w == NULL )
	{
		tds520.w = w = T_malloc( sizeof( WINDOW ) );
		w->prev = NULL;
	}
	else
	{
		w = tds520.w;
		while ( w->next != NULL )
			w = w->next;
		w->next = T_malloc( sizeof( WINDOW ) );
		w->next->prev = w;
		w = w->next;
	}

	w->next = NULL;
	w->num = tds520.num_windows++ + WINDOW_START_NUMBER;

	if ( is_win_start )
		w->start = win_start;
	w->is_start = is_win_start;

	if ( is_win_width )
		w->width = win_width;
	w->is_width = is_win_width;

	w->is_used = UNSET;

	return vars_push( INT_VAR, w->num );
}


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
				if ( tds520.is_timebase )
					return vars_push( FLOAT_VAR, tds520.timebase );

				eprint( FATAL, SET, "%s: Function %s with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )

			case TEST :
				return vars_push( FLOAT_VAR, tds520.is_timebase ?
								  tds520.timebase : TDS520_TEST_TIME_BASE );

			case EXPERIMENT :
				tds520.is_timebase = SET;
				return vars_push( FLOAT_VAR,
								  tds520.timebase = tds520.timebase );
		}

	if ( tds520.is_timebase )
	{
		eprint( FATAL, SET, "%s: Digitizer time base has already been "
				"set.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	timebase = VALUE( v );
	vars_pop( v );

	if ( timebase <= 0 )
	{
		eprint( FATAL, SET, "%s: Invalid zero or negative time base: %s.\n",
				DEVICE_NAME, tds520_ptime( timebase ) );
		THROW( EXCEPTION )
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
		t = T_strdup( tds520_ptime( timebase ) );
		eprint( WARN, SET, "%s: Can't set timebase to %s, using %s "
				"instead.\n", DEVICE_NAME, t, tds520_ptime( tb[ TB ] ) );
		T_free( t );
	}

	if ( TB < 0 )                                   /* not found yet ? */
	{
		t = T_strdup( tds520_ptime( timebase ) );

		if ( timebase < tb[ 0 ] )
		{
			TB = 0;
			eprint( WARN, SET, "%s: Timebase of %s is too low, using %s "
					"instead.\n", DEVICE_NAME, t, tds520_ptime( tb[ TB ] ) );
		}
		else
		{
		    TB = TB_ENTRIES - 1;
			eprint( WARN, SET, "%s: Timebase of %s is too large, using %s "
					"instead.\n", DEVICE_NAME, t, tds520_ptime( tb[ TB ] ) );
		}

		T_free( t );
	}

	tds520.timebase = tb[ TB ];
	tds520.is_timebase = SET;

	if ( FSC2_MODE == EXPERIMENT )
		tds520_set_timebase( tds520.timebase );

	return vars_push( FLOAT_VAR, tds520.timebase );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_time_per_point( Var *v )
{
	v = v;

	return vars_push( FLOAT_VAR, tds520.timebase / TDS520_POINTS_PER_DIV );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_sensitivity( Var *v )
{
	long channel;
	double sens;
	

	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: Missing parameter in call of %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );
	channel = tds520_translate_channel( GENERAL_TO_TDS520, v->val.lval );

	if ( channel > TDS520_CH2 )
	{
		eprint( FATAL, SET, "%s: Can't set or obtain sensitivity for "
				"channel %s.\n", DEVICE_NAME, Channel_Names[ channel ] );
		THROW( EXCEPTION )
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( tds520.is_sens[ channel ] )
					return vars_push( FLOAT_VAR, tds520.sens[ channel ] );

				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )

			case TEST :
				return vars_push( FLOAT_VAR, tds520.is_sens[ channel ] ?
								  tds520.sens[ channel ] :
								  TDS520_TEST_SENSITIVITY );

			case EXPERIMENT :
				tds520.is_sens[ channel ] = SET;
				return vars_push( FLOAT_VAR,
							 tds520.sens[ channel ] = tds520.sens[ channel ] );
		}

	vars_check( v, INT_VAR | FLOAT_VAR );
	sens = VALUE( v );

	if ( sens < max_sens || sens > min_sens )
	{
		eprint( FATAL, SET, "%s: Sensitivity setting is out of range.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}

	if ( ( v = vars_pop( v ) ) != NULL )
		eprint( WARN, SET, "%s: Superfluous parameter in call of %s().\n",
				DEVICE_NAME, Cur_Func );

	tds520.sens[ channel ] = sens;
	tds520.is_sens[ channel ] = SET;

	if ( FSC2_MODE == EXPERIMENT )
		tds520_set_sens( channel, sens );

	return vars_push( FLOAT_VAR, tds520.sens[ channel ] );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_num_averages( Var *v )
{
	long num_avg;
	

	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( tds520.is_num_avg )
					return vars_push( INT_VAR, tds520.num_avg );

				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )

			case TEST :
				return vars_push( INT_VAR, tds520.is_num_avg ?
								  tds520.num_avg : TDS520_TEST_NUM_AVG );

			case EXPERIMENT :
				tds520.is_num_avg = SET;
				return vars_push( INT_VAR, tds520.num_avg = tds520.num_avg );
		}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		num_avg = v->val.lval;
	else
	{
		eprint( WARN, SET, "%s: Floating point number used as number "
				"of averages in %s().\n", DEVICE_NAME, Cur_Func );
		num_avg = lrnd( v->val.dval );
	}
	vars_pop( v );

	if ( num_avg == 0 )
	{
		eprint( FATAL, SET, "%s: Can't do zero averages. If you want "
				"to set sample mode specify 1 as number of averages.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}
	else if ( num_avg < 0 )
	{
		eprint( FATAL, SET, "%s: Negative number of averages (%ld) in "
				"%s().\n", DEVICE_NAME, num_avg, Cur_Func );
		THROW( EXCEPTION )
	}

	tds520.is_num_avg = SET;
	tds520.num_avg = num_avg;

	if ( FSC2_MODE == EXPERIMENT )
		tds520_set_num_avg( num_avg );

	return vars_push( INT_VAR, tds520.num_avg );
}


/*------------------------------------------------------------------*/
/* Function either sets or returns the current record length of the */
/* digitizer. When trying to set a record length that does not fit  */
/* the possible settings the next larger is used instead.           */
/*------------------------------------------------------------------*/

Var *digitizer_record_length( Var *v )
{
	long rec_len;
	int i;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				if ( tds520.is_rec_len )
					return vars_push( INT_VAR, tds520.rec_len );

				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )

			case TEST :
				return vars_push( INT_VAR, tds520.is_rec_len ?
								  tds520.rec_len : TDS520_TEST_REC_LEN );

			case EXPERIMENT :
				if ( ! tds520_get_record_length( &rec_len ) )
					tds520_gpib_failure( );

				tds520.rec_len = rec_len;
				tds520.is_rec_len = SET;
				return vars_push( INT_VAR, tds520.rec_len );
		}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "%s: Floating point value used as record "
				"length in %s().\n", DEVICE_NAME, Cur_Func );
		rec_len = lrnd( v->val.dval );
	}
	else
		rec_len = v->val.lval;

	i = 0;
	while ( 1 )
	{
		if ( record_lengths[ i ] == 0 )
		{
			eprint( FATAL, SET, "%s: Record length %ld too long in %s().\n",
					DEVICE_NAME, rec_len, Cur_Func );
			THROW( EXCEPTION )
		}

		if ( rec_len == record_lengths[ i ] )
			break;

		if ( rec_len < record_lengths[ i ] )
		{
			eprint( SEVERE, SET, "%s: Can't set record length to %ld, "
					"using next larger allowed value of %ld instead.\n",
					DEVICE_NAME, rec_len, record_lengths[ i ] );
			break;
		}

		i++;
	}

	tds520.rec_len = record_lengths[ i ];
	tds520.is_rec_len = SET;

	if ( FSC2_MODE == EXPERIMENT &&
		 ! tds520_set_record_length( tds520.rec_len ) )
		tds520_gpib_failure( );

	return vars_push( INT_VAR, tds520.rec_len );
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
				if ( tds520.is_trig_pos )
					return vars_push( FLOAT_VAR, tds520.trig_pos );

				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )

			case TEST :
				return vars_push( FLOAT_VAR, tds520.is_trig_pos ?
								  tds520.trig_pos : TDS520_TEST_TRIG_POS );


			case EXPERIMENT :
				if ( ! tds520_get_trigger_pos( &trig_pos ) )
					tds520_gpib_failure( );

				tds520.trig_pos = trig_pos;
				tds520.is_trig_pos = SET;
				return vars_push( FLOAT_VAR, tds520.trig_pos );
		}

	vars_check( v, INT_VAR | FLOAT_VAR );
	trig_pos = VALUE( v );
	vars_pop( v );

	if ( trig_pos < 0.0 || trig_pos > 1.0 )
	{
		eprint( FATAL, SET, "%s: Invalid trigger position: %f, must be in "
				"interval [0,1].\n", Fname, Lc, DEVICE_NAME, trig_pos );
		THROW( EXCEPTION )
	}

	tds520.trig_pos = trig_pos;
	tds520.is_trig_pos = SET;

	if ( FSC2_MODE == EXPERIMENT &&
		 ! tds520_set_trigger_pos( tds520.trig_pos ) )
		tds520_gpib_failure( );

	return vars_push( FLOAT_VAR, tds520.trig_pos );
}


/*----------------------------------------------------------------------*/
/* This is not a function that users should usually call but a function */
/* that allows other functions to check if a certain number stands for  */
/* channel that can be used in measurements.                            */
/*----------------------------------------------------------------------*/

Var *digitizer_meas_channel_ok( Var *v )
{
	long channel;


	vars_check( v, INT_VAR );
	channel = tds520_translate_channel( GENERAL_TO_TDS520, v->val.lval );

	if ( channel > TDS520_REF4 )
		return vars_push( INT_VAR, 0 );
	else
		return vars_push( INT_VAR, 1 );
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
				if ( tds520.is_trigger_channel )
					return vars_push( INT_VAR, tds520_translate_channel(
						         TDS520_TO_GENERAL, tds520.trigger_channel ) );

				eprint( FATAL, SET, "%s: Function %s() with no argument can "
						"only be used in the EXPERIMENT section.\n",
						DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )

			case TEST :
				return vars_push( INT_VAR, tds520_translate_channel(
					             TDS520_TO_GENERAL, tds520.is_trigger_channel ?
								 tds520.trigger_channel :
								 TDS520_TEST_TRIG_CHANNEL ) );

			case EXPERIMENT :
				return vars_push( INT_VAR, tds520_translate_channel(
					      TDS520_TO_GENERAL, tds520_get_trigger_channel( ) ) );
		}

	vars_check( v, INT_VAR );
	channel = tds520_translate_channel( GENERAL_TO_TDS520, v->val.lval );
	vars_pop( v );

    switch ( channel )
    {
        case TDS520_CH1 : case TDS520_CH2 : case TDS520_AUX1 :
		case TDS520_AUX2 : case TDS520_LIN :
			tds520.trigger_channel = channel;
			tds520.is_trigger_channel = SET;

			if ( FSC2_MODE == EXPERIMENT )
				tds520_set_trigger_channel( Channel_Names[ channel ] );
            break;

		default :
			eprint( FATAL, SET, "%s: Channel %s can't be used as trigger "
					"channel.\n", DEVICE_NAME, Channel_Names[ channel ] );
			THROW( EXCEPTION )
    }

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_start_acquisition( Var *v )
{
	v = v;

	if ( FSC2_MODE == EXPERIMENT )
		tds520_start_acquisition( );

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/


Var *digitizer_get_area( Var *v )
{
	return get_area( v, tds520.w != NULL ? SET : UNSET );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_get_area_fast( Var *v )
{
	return get_area( v, UNSET );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *get_area( Var *v, bool use_cursor )
{
	WINDOW *w;
	int ch;


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: Missing arguments in call of "
				"function $s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );
	ch = ( int ) tds520_translate_channel( GENERAL_TO_TDS520, v->val.lval );
	v = vars_pop( v );

	if ( ch > TDS520_REF4 )
	{
		eprint( FATAL, SET, "%s: Invalid channel %s used in %s().\n",
				DEVICE_NAME, Channel_Names[ ch ], Cur_Func );
		THROW( EXCEPTION )
	}

	tds520.channels_in_use[ ch ] = SET;

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );

		if ( ( w = tds520.w ) == NULL )
		{
			eprint( FATAL, SET, "%s: No measurement windows have been "
					"defined for %s().\n", DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		while ( w != NULL )
		{
			if ( w->num == v->val.lval )
			{
				w->is_used = SET;
				v = vars_pop( v );
				break;
			}
			w = w->next;
		}

		if ( w == NULL )
		{
			eprint( FATAL, SET, "%s: Measurement window has not been "
					"defined for %s().\n", DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}
	}
	else
		w = NULL;

	if ( v != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"%s().\n", DEVICE_NAME, Cur_Func );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   value */

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, 1.234e-8 );

	return vars_push( FLOAT_VAR, tds520_get_area( ch, w, use_cursor ) );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_get_curve( Var *v )
{
	return get_curve( v, tds520.w != NULL ? SET : UNSET );
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
	double *array;
	long length;
	Var *nv;


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: Missing arguments in call of "
				"function %s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );
	ch = ( int ) tds520_translate_channel( GENERAL_TO_TDS520, v->val.lval );
	v = vars_pop( v );

	if ( ch > TDS520_REF4 )
	{
		eprint( FATAL, SET, "%s: Invalid channel %s used in %s().\n",
				DEVICE_NAME, Channel_Names[ ch ], Cur_Func );
		THROW( EXCEPTION )
	}

	tds520.channels_in_use[ ch ] = SET;

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds520.w ) == NULL )
		{
			eprint( FATAL, SET, "%s: No measurement windows have been "
					"defined for %s().\n", DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		while ( w != NULL )
		{
			if ( w->num == v->val.lval )
			{
				w->is_used = SET;
				v = vars_pop( v );
				break;
			}
			w = w->next;
		}

		if ( w == NULL )
		{
			eprint( FATAL, SET, "%s: Measurement window has not been "
					"defined for %s().\n", DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}
	}
	else
		w = NULL;

	if ( v != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of %s().\n",
				DEVICE_NAME, Cur_Func );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   array */

	if ( FSC2_MODE == EXPERIMENT )
	{
		tds520_get_curve( ch, w, &array, &length, use_cursor );
		nv = vars_push( FLOAT_ARR, array, length );
	}
	else
	{
		if ( tds520.is_rec_len  )
			length = tds520.rec_len;
		else
			length = TDS520_TEST_REC_LEN;
		array = T_malloc( length * sizeof( double ) );
		for ( i = 0; i < length; i++ )
			array[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );
		nv = vars_push( FLOAT_ARR, array, length );
		nv->flags |= IS_DYNAMIC;
	}

	T_free( array );
	return nv;
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_get_amplitude( Var *v )
{
	return get_amplitude( v, SET );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_get_amplitude_fast( Var *v )
{
	return get_amplitude( v, UNSET );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static Var *get_amplitude( Var *v, bool use_cursor )
{
	WINDOW *w;
	int ch;
	Var *nv;


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: Missing arguments in call of %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );
	ch = ( int ) tds520_translate_channel( GENERAL_TO_TDS520, v->val.lval );
	v = vars_pop( v );

	if ( ch > TDS520_REF4 )
	{
		eprint( FATAL, SET, "%s: Invalid channel %s used in %s().\n",
				DEVICE_NAME, Channel_Names[ ch ],
				Cur_Func );
		THROW( EXCEPTION )
	}

	tds520.channels_in_use[ ch ] = SET;

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds520.w ) == NULL )
		{
			eprint( FATAL, SET, "%s: No measurement windows have been "
					"defined for %s().\n", DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		while ( w != NULL )
		{
			if ( w->num == v->val.lval )
			{
				w->is_used = SET;
				v = vars_pop( v );
				break;
			}
			w = w->next;
		}

		if ( w == NULL )
		{
			eprint( FATAL, SET, "%s: Measurement window has not been "
					"defined for %s().\n", DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}
	}
	else
		w = NULL;

	if ( v != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"function %s().\n", DEVICE_NAME, Cur_Func );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   array */

	if ( FSC2_MODE == EXPERIMENT )
		nv = vars_push( FLOAT_VAR, tds520_get_amplitude( ch, w, use_cursor ) );
	else
		nv = vars_push( FLOAT_VAR, 1.23e-7 );

	return nv;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_run( Var *v )
{
	v = v;

	if ( FSC2_MODE == EXPERIMENT )
		tds520_free_running( );

	return vars_push( INT_VAR,1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_lock_keyboard( Var *v )
{
	bool lock;


	if ( v == NULL )
		lock = SET;
	else
	{
		vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

		if ( v->type == INT_VAR )
			lock = v->val.lval != 0;
		else if ( v->type == FLOAT_VAR )
			lock = v->val.dval != 0.0;
		else
		{
			if ( ! strcasecmp( v->val.sptr, "OFF" ) )
				lock = UNSET;
			else if ( ! strcasecmp( v->val.sptr, "ON" ) )
				lock = SET;
			else
			{
				eprint( FATAL, SET, "%s: Invalid argument in call of "
						"function %s().\n", DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
		}
	}

	if ( FSC2_MODE == EXPERIMENT )
		tds520_lock_state( lock );

	tds520.lock_state = lock;
	return vars_push( INT_VAR, lock ? 1 : 0 );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
