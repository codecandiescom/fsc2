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



#define TDS754A_MAIN

#include "tds754a.h"

const char generic_type[ ] = DEVICE_TYPE;


/* This array must be set to the available record lengths of the digitizer
   and must always end with a 0 */

static long record_lengths[ ] = { 500, 1000, 2500, 5000, 15000, 50000, 0 };

/* List of all possible time base values (in seconds) */

static double tb[ ] = {                     500.0e-12,
						  1.0e-9,   2.0e-9,   5.0e-9,
						 12.5e-9,  25.0e-9,  50.0e-9,
						100.0e-9, 200.0e-9, 500.0e-9,
						  1.0e-6,   2.0e-6,   5.0e-6,
						 10.0e-6,  20.0e-6,  50.0e-6,
						100.0e-6, 200.0e-6, 500.0e-6,
						  1.0e-3,   2.0e-3,   5.0e-3,
						 10.0e-3,  20.0e-3,  50.0e-3,
						100.0e-3, 200.0e-3, 500.0e-3,
						  1.0,      2.0,      5.0,
						 10.0 };

#define TB_ENTRIES ( sizeof tb / sizeof tb[ 0 ] )


/* Maximum and minimum sensitivity settings (in V) for the measurement
   channels.
   Take care: The minimum sensitivity of 10 V only works with 1 M Ohm input
   impedance, while for 50 Ohm the minimum sensitivity is only 1V.
   Unfortunately, this can only be tested after the digitizer is online. */

static double max_sens = 1e-3,
              min_sens = 10.0;


static Var *get_area( Var *v, bool use_cursor );
static Var *get_curve( Var *v, bool use_cursor );
static Var *get_amplitude( Var *v, bool use_cursor );


static struct {
	bool is_equal_width;

	bool is_timebase;
	double timebase;

	bool is_num_avg;
	long num_avg;

	bool is_rec_len;
	long rec_len;

	bool is_trig_pos;
	double trig_pos;

	int data_source;
	int meas_source;

	bool lock_state;

	bool is_sens[ MAX_CHANNELS ];
	double sens[ MAX_CHANNELS ];
} tds754a_store;


/*******************************************/
/*   We start with the hook functions...   */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int tds754a_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Initialize some variables in the digitizers structure */

	tds754a.is_reacting = UNSET;
	tds754a.w           = NULL;
	tds754a.is_timebase = UNSET;
	tds754a.is_num_avg  = UNSET;
	tds754a.is_rec_len  = UNSET;
	tds754a.is_trig_pos = UNSET;
	tds754a.num_windows = 0;
	tds754a.data_source = TDS754A_UNDEF;
	tds754a.meas_source = TDS754A_UNDEF;
	tds754a.lock_state  = SET;

	for ( i = TDS754A_CH1; i <= TDS754A_CH4; i++ )
		tds754a.is_sens[ i ] = UNSET;
	for ( i = TDS754A_MATH1; i <= TDS754A_REF4; i++ )
	{
		tds754a.is_sens[ i ] = SET;
		tds754a.sens[ i ] = 1.0;
	}

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int tds754a_exp_hook( void )
{
	int i;


	/* Store the state the digitizer was set to in the preparations section -
	   we need this when starting the same experiment again... */

	tds754a_store.is_equal_width = tds754a.is_equal_width;

	tds754a_store.is_timebase    = tds754a.is_timebase;
	tds754a_store.timebase       = tds754a.is_timebase;

	tds754a_store.is_num_avg     = tds754a.is_num_avg;
	tds754a_store.num_avg        = tds754a.num_avg;

	tds754a_store.is_rec_len     = tds754a.is_rec_len;
	tds754a_store.rec_len        = tds754a.rec_len;

	tds754a_store.is_trig_pos    = tds754a.is_trig_pos;
	tds754a_store.trig_pos       = tds754a.trig_pos;

	tds754a_store.data_source    = tds754a.data_source;
	tds754a_store.meas_source    = tds754a.meas_source;

	tds754a_store.lock_state     = tds754a.lock_state;

	for ( i = TDS754A_CH1; i <= TDS754A_CH4; i++ )
	{
		tds754a_store.is_sens[ i ] = tds754a.is_sens[ i ];
		tds754a_store.sens[ i ]    = tds754a.sens[ i ];
	}

	if ( ! tds754a_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Initialization of device failed: %s\n",
				DEVICE_NAME, gpib_error_msg );
		THROW( EXCEPTION )
	}

	tds754a_do_pre_exp_checks( );

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int tds754a_end_of_exp_hook( void )
{
	int i;

	tds754a_finished( );

	/* Reset the digitizer to the state it was set to in the preparations
	   section - we need this when starting the same experiment again... */

	tds754a.is_reacting          = UNSET;

	tds754a_store.is_equal_width = tds754a.is_equal_width;

	tds754a.is_timebase          = tds754a_store.is_timebase;
	tds754a.timebase             = tds754a_store.timebase;

	tds754a_store.is_num_avg     = tds754a.is_num_avg;
	tds754a.num_avg              = tds754a_store.num_avg;

	tds754a.is_rec_len           = tds754a_store.is_rec_len;
	tds754a.rec_len              = tds754a_store.rec_len;

	tds754a.is_trig_pos          = tds754a_store.is_trig_pos;
	tds754a.trig_pos             = tds754a_store.trig_pos;

	tds754a.data_source          = tds754a_store.data_source;
	tds754a.meas_source          = tds754a_store.meas_source;

	tds754a.lock_state = tds754a_store.lock_state;

	for ( i = TDS754A_CH1; i <= TDS754A_CH4; i++ )
	{
		tds754a.is_sens[ i ] = tds754a_store.is_sens[ i ];
		tds754a.sens[ i ]    = tds754a_store.sens[ i ];
	}

	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void tds754a_exit_hook( void )
{
	tds754a_delete_windows( );
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


	if ( tds754a.num_windows >= MAX_NUM_OF_WINDOWS )
	{
		eprint( FATAL, SET, "%s: Maximum number of digitizer windows (%ld) "
				"exceeded in %s().\n",
				DEVICE_NAME, MAX_NUM_OF_WINDOWS, Cur_Func );
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

			if ( ( TEST_RUN && win_width < 0.0 ) ||
				 ( ! TEST_RUN && win_width <= 0.0 ) )
			{
				eprint( FATAL, SET, "%s: Zero or negative  window width "
						"in %s().\n", DEVICE_NAME, Cur_Func );
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

	if ( tds754a.w == NULL )
	{
		tds754a.w = w = T_malloc( sizeof( WINDOW ) );
		w->prev = NULL;
	}
	else
	{
		w = tds754a.w;
		while ( w->next != NULL )
			w = w->next;
		w->next = T_malloc( sizeof( WINDOW ) );
		w->next->prev = w;
		w = w->next;
	}

	w->next = NULL;
	w->num = tds754a.num_windows++ + WINDOW_START_NUMBER;

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
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_timebase )
				return vars_push( FLOAT_VAR, tds754a.timebase );
			else
				return vars_push( FLOAT_VAR, TDS754A_TEST_TIME_BASE );
		}
		else if ( I_am == PARENT )
		{
			if ( tds754a.is_timebase )
				return vars_push( FLOAT_VAR, tds754a.timebase );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		tds754a.timebase = tds754a_get_timebase( );
		tds754a.is_timebase = SET;
		return vars_push( FLOAT_VAR, tds754a.timebase );
	}

	if ( I_am == CHILD || TEST_RUN )
	{
		eprint( FATAL, SET, "%s: Digitizer time base can only be set before "
				"the EXPERIMENT section starts.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	if ( tds754a.is_timebase )
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
				DEVICE_NAME, tds754a_ptime( timebase ) );
		THROW( EXCEPTION )
	}

	if ( timebase <= 0 )
	{
		eprint( FATAL, SET, "%s: Invalid zero or negative time base: %s.\n",
				DEVICE_NAME, tds754a_ptime( timebase ) );
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
		t = T_strdup( tds754a_ptime( timebase ) );
		eprint( WARN, SET, "%s: Can't set timebase to %s, using %s "
				"instead.\n", DEVICE_NAME, t, tds754a_ptime( tb[ TB ] ) );
		T_free( t );
	}

	if ( TB < 0 )                                   /* not found yet ? */
	{
		t = T_strdup( tds754a_ptime( timebase ) );

		if ( timebase < tb[ 0 ] )
		{
			TB = 0;
			eprint( WARN, SET, "%s: Timebase of %s is too low, using %s "
					"instead.\n", DEVICE_NAME, t, tds754a_ptime( tb[ TB ] ) );
		}
		else
		{
		    TB = TB_ENTRIES - 1;
			eprint( WARN, SET, "%s: Timebase of %s is too large, using %s "
					"instead.\n", DEVICE_NAME, t, tds754a_ptime( tb[ TB ] ) );
		}

		T_free( t );
	}

	tds754a.timebase = tb[ TB ];
	tds754a.is_timebase = SET;

	return vars_push( FLOAT_VAR, tds754a.timebase );
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

	if ( v->val.lval < TDS754A_CH1 || v->val.lval > TDS754A_CH4 )
	{
		eprint( FATAL, SET, "%s: Can't set or obtain sensitivity for "
				"specified channel in %s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	channel = v->val.lval;

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_sens[ channel ] )
				return vars_push( FLOAT_VAR, tds754a.sens[ channel ] );
			else
				return vars_push( FLOAT_VAR, TDS754A_TEST_SENSITIVITY );
		}
		else if ( I_am == PARENT )
		{
			if ( tds754a.is_sens[ channel ] )
				return vars_push( FLOAT_VAR, tds754a.sens[ channel ] );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		tds754a.sens[ channel ] = tds754a_get_sens( channel );
		tds754a.is_sens[ channel ] = SET;
		return vars_push( FLOAT_VAR, tds754a.sens[ channel ] );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	sens = VALUE( v );

	if ( sens < max_sens || sens > min_sens )
	{
		eprint( FATAL, SET, "%s: Sensitivity setting is out of range in "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	tds754a.sens[ channel ] = sens;
	tds754a.is_sens[ channel ] = SET;

	if ( ! TEST_RUN )
		tds754a_set_sens( channel, sens );

	if ( ( v = vars_pop( v ) ) != NULL )
		eprint( WARN, SET, "%s: Superfluous parameter in call of %s().\n",
				DEVICE_NAME, Cur_Func );

	return vars_push( FLOAT_VAR, tds754a.sens[ channel ] );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_num_averages( Var *v )
{
	long num_avg;
	

	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_num_avg )
				return vars_push( INT_VAR, tds754a.num_avg );
			else
				return vars_push( INT_VAR, TDS754A_TEST_NUM_AVG );
		}
		else if ( I_am == PARENT )
		{
			if ( tds754a.is_num_avg )
				return vars_push( INT_VAR, tds754a.num_avg );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		tds754a.num_avg = tds754a_get_num_avg( );
		tds754a.is_num_avg = SET;
		return vars_push( INT_VAR, tds754a.num_avg );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		num_avg = v->val.lval;
	else
	{
		eprint( WARN, SET, "%s: Floating point number used as number of "
				"averages in %s().\n", DEVICE_NAME, Cur_Func );
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
		eprint( FATAL, SET, "%s: Invalid negative number of averages: "
				"%ld.\n", DEVICE_NAME, num_avg );
		THROW( EXCEPTION )
	}

	tds754a.num_avg = num_avg;
	if ( I_am == CHILD )
		tds754a_set_num_avg( num_avg );
	if ( ! TEST_RUN )                 // store value if in PREPARATIONS section
		tds754a.is_num_avg = SET;

	return vars_push( INT_VAR, tds754a.num_avg );
}


/*-----------------------------------------------------------------------*/
/* This is not a function that users should usually call but a function  */
/* that allows the parser to convert symbolic channel names into numbers */
/*-----------------------------------------------------------------------*/

Var *digitizer_get_channel_number( Var *v )
{
	int i;


	vars_check( v, STR_VAR );

	for ( i = 0; i < MAX_CHANNELS; i++ )
		if ( ! strcmp( v->val.sptr, Channel_Names[ i ] ) )
			return vars_push( INT_VAR, i );

	return NULL;
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
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_rec_len )
				return vars_push( INT_VAR, tds754a.rec_len );
			else
				return vars_push( INT_VAR, TDS754A_TEST_REC_LEN );
		}
		else if ( I_am == PARENT )
		{
			if ( tds754a.is_rec_len )
				return vars_push( INT_VAR, tds754a.rec_len );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		if ( ! tds754a_get_record_length( &rec_len ) )
			tds754a_gpib_failure( );

		tds754a.rec_len = rec_len;
		tds754a.is_rec_len = SET;
		return vars_push( INT_VAR, tds754a.rec_len );
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

	tds754a.rec_len = record_lengths[ i ];
	tds754a.is_rec_len = SET;

	if ( I_am == CHILD && ! tds754a_set_record_length( tds754a.rec_len ) )
		tds754a_gpib_failure( );

	return vars_push( INT_VAR, tds754a.rec_len );
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
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_trig_pos )
				return vars_push( FLOAT_VAR, tds754a.trig_pos );
			else
				return vars_push( FLOAT_VAR, TDS754A_TEST_TRIG_POS );
		}
		else if ( I_am == PARENT )
		{
			if ( tds754a.is_trig_pos )
				return vars_push( FLOAT_VAR, tds754a.trig_pos );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		if ( ! tds754a_get_trigger_pos( &trig_pos ) )
			tds754a_gpib_failure( );

		tds754a.trig_pos = trig_pos;
		tds754a.is_trig_pos = SET;
		return vars_push( FLOAT_VAR, tds754a.trig_pos );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	trig_pos = VALUE( v );
	vars_pop( v );

	if ( trig_pos < 0.0 || trig_pos > 1.0 )
	{
		eprint( FATAL, SET, "%s: Invalid trigger position: %f, must be in "
				"interval [0,1].\n", DEVICE_NAME, trig_pos );
		THROW( EXCEPTION )
	}

	tds754a.trig_pos = trig_pos;
	tds754a.is_trig_pos = SET;

	if ( I_am == CHILD && ! tds754a_set_trigger_pos( tds754a.trig_pos ) )
		tds754a_gpib_failure( );

	return vars_push( FLOAT_VAR, tds754a.trig_pos );
}


/*----------------------------------------------------------------------*/
/* This is not a function that users should usually call but a function */
/* that allows other functions to check if a certain number stands for  */
/* channel that can be used in measurements.                            */
/*----------------------------------------------------------------------*/

Var *digitizer_meas_channel_ok( Var *v )
{
	vars_check( v, INT_VAR );

	if ( v->val.lval < TDS754A_CH1 || v->val.lval > TDS754A_REF4 )
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
	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_trigger_channel )
				return vars_push( INT_VAR, tds754a.trigger_channel );
			else
				return vars_push( INT_VAR, TDS754A_TEST_TRIG_CHANNEL );
		}
		else if ( I_am == PARENT )
		{
			if ( tds754a.is_trigger_channel )
				return vars_push( INT_VAR, tds754a.trigger_channel );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		return vars_push( tds754a_get_trigger_channel( ) );
	}

	vars_check( v, INT_VAR );

	if ( v->val.lval < 0 || v->val.lval >= MAX_CHANNELS )
	{
		eprint( FATAL, SET, "%s: Invalid trigger channel name in %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

    switch ( v->val.lval )
    {
        case TDS754A_CH1 : case TDS754A_CH2 : case TDS754A_CH3 :
		case TDS754A_CH4 : case TDS754A_AUX : case TDS754A_LIN :
			tds754a.trigger_channel = v->val.lval;
			if ( I_am == CHILD )
				tds754a_set_trigger_channel( Channel_Names[ v->val.lval ] );
			if ( ! TEST_RUN )
				tds754a.is_trigger_channel = SET;
            break;

		default :
			eprint( FATAL, SET, "%s: Channel %s can't be used as "
					"trigger channel in %s().\n", DEVICE_NAME,
					Channel_Names[ v->val.lval ], Cur_Func );
			THROW( EXCEPTION )
    }

	vars_pop( v );
	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_start_acquisition( Var *v )
{
	v = v;

	if ( ! TEST_RUN )
		tds754a_start_aquisition( );
	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_get_area( Var *v )
{
	return get_area( v, tds754a.w != NULL ? SET : UNSET );
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
		eprint( FATAL, SET, "%s: Missing arguments in call of function "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );
	for ( ch = 0; ch <= TDS754A_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS754A_REF4 )
	{
		eprint( FATAL, SET, "%s: Invalid channel specification in %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	tds754a.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );

		if ( ( w = tds754a.w ) == NULL )
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
	   value */

	if ( I_am == CHILD )
		return vars_push( FLOAT_VAR, tds754a_get_area( ch, w, use_cursor ) );

	return vars_push( FLOAT_VAR, 1.234e-8 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_get_curve( Var *v )
{
	return get_curve( v, tds754a.w != NULL ? SET : UNSET );
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
		eprint( FATAL, SET, "%s: Missing arguments in call of function "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );
	for ( ch = 0; ch <= TDS754A_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS754A_REF4 )
	{
		eprint( FATAL, SET, "%s: Invalid channel specification.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}

	tds754a.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds754a.w ) == NULL )
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

	if ( I_am == CHILD )
	{
		tds754a_get_curve( ch, w, &array, &length, use_cursor );
		nv = vars_push( FLOAT_ARR, array, length );
		T_free( array );
		return nv;
	}

	if ( tds754a.is_rec_len  )
		length = tds754a.rec_len;
	else
		length = TDS754A_TEST_REC_LEN;
	array = T_malloc( length * sizeof( double ) );
	for ( i = 0; i < length; i++ )
		array[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );
	nv = vars_push( FLOAT_ARR, array, length );
	nv->flags |= IS_DYNAMIC;
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
		eprint( FATAL, SET, "%s: Missing arguments in call of function "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );
	for ( ch = 0; ch <= TDS754A_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS754A_REF4 )
	{
		eprint( FATAL, SET, "%s: Invalid channel specification in %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	tds754a.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds754a.w ) == NULL )
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

	if ( I_am == CHILD )
	{
		nv = vars_push( FLOAT_VAR,
						tds754a_get_amplitude( ch, w, use_cursor ) );
		return nv;
	}

	nv = vars_push( FLOAT_VAR, 1.23e-7 );
	return nv;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *digitizer_run( Var *v )
{
	v = v;
	if ( ! TEST_RUN )
		tds754a_free_running( );
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
				eprint( FATAL, SET, "Invalid argument in call of "
						"function %s().\n", DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION )
			}
		}
	}

	if ( ! TEST_RUN )
		tds754a_lock_state( lock );

	tds754a.lock_state = lock;
	return vars_push( INT_VAR, lock ? 1 : 0 );
}
