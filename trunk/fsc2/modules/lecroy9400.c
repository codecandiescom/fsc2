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


#define LECROY9400_MAIN

#include "lecroy9400.h"


const char generic_type[ ] = DEVICE_TYPE;




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
	lecroy9400.w           = NULL;
	lecroy9400.is_timebase = UNSET;
	lecroy9400.is_num_avg  = UNSET;
	lecroy9400.is_rec_len  = UNSET;
	lecroy9400.is_trig_pos = UNSET;
	lecroy9400.num_windows = 0;
	lecroy9400.data_source = LECROY9400_UNDEF;
	lecroy9400.meas_source = LECROY9400_UNDEF;
	lecroy9400.lock_state  = SET;

	for ( i = LECROY9400_CH1; i <= LECROY9400_CH2; i++ )
		lecroy9400.is_sens[ i ] = UNSET;
	for ( i = LECROY9400_MEM_C; i <= LECROY9400_FUNC_F; i++ )
	{
		lecroy9400.is_sens[ i ] = SET;
		lecroy9400.sens[ i ] = 1.0;
	}

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int lecroy9400_exp_hook( void )
{
	int i;


	/* Store the state the digitizer was set to in the preparations section -
	   we need this when starting the same experiment again... */

	lecroy9400_store.is_timebase = lecroy9400.is_timebase;
	lecroy9400_store.tb_index    = lecroy9400.tb_index;
	lecroy9400_store.timebase    = lecroy9400.is_timebase;

	lecroy9400_store.is_num_avg  = lecroy9400.is_num_avg;
	lecroy9400_store.num_avg     = lecroy9400.num_avg;

	lecroy9400_store.is_rec_len  = lecroy9400.is_rec_len;
	lecroy9400_store.rec_len     = lecroy9400.rec_len;

	lecroy9400_store.is_trig_pos = lecroy9400.is_trig_pos;
	lecroy9400_store.trig_pos    = lecroy9400.trig_pos;

	lecroy9400_store.data_source = lecroy9400.data_source;
	lecroy9400_store.meas_source = lecroy9400.meas_source;

	for ( i = LECROY9400_CH1; i <= LECROY9400_CH2; i++ )
	{
		lecroy9400_store.is_sens[ i ] = lecroy9400.is_sens[ i ];
		lecroy9400_store.sens[ i ]    = lecroy9400.sens[ i ];
	}

	if ( ! lecroy9400_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Initialization of device failed: %s\n",
				DEVICE_NAME, gpib_error_msg );
		THROW( EXCEPTION )
	}

	lecroy9400_do_pre_exp_checks( );

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int lecroy9400_end_of_exp_hook( void )
{
	int i;


	lecroy9400_finished( );

	/* Reset the digitizer to the state it was set to in the preparations
	   section - we need this when starting the same experiment again... */

	lecroy9400.is_reacting      = UNSET;

	lecroy9400.is_timebase      = lecroy9400_store.is_timebase;
	lecroy9400.timebase         = lecroy9400_store.timebase;

	lecroy9400_store.is_num_avg = lecroy9400.is_num_avg;
	lecroy9400.num_avg          = lecroy9400_store.num_avg;

	lecroy9400.is_rec_len       = lecroy9400_store.is_rec_len;
	lecroy9400.rec_len          = lecroy9400_store.rec_len;

	lecroy9400.is_trig_pos      = lecroy9400_store.is_trig_pos;
	lecroy9400.trig_pos         = lecroy9400_store.trig_pos;

	lecroy9400.data_source      = lecroy9400_store.data_source;
	lecroy9400.meas_source      = lecroy9400_store.meas_source;

	lecroy9400.lock_state       = lecroy9400_store.lock_state;

	for ( i = LECROY9400_CH1; i <= LECROY9400_CH2; i++ )
	{
		lecroy9400.is_sens[ i ] = lecroy9400_store.is_sens[ i ];
		lecroy9400.sens[ i ]    = lecroy9400_store.sens[ i ];
	}

	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void lecroy9400_exit_hook( void )
{
	lecroy9400_delete_windows( );
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


	if ( lecroy9400.num_windows >= MAX_NUM_OF_WINDOWS )
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

			if ( ( TEST_RUN && win_width < 0.0 ) ||
				 ( ! TEST_RUN && win_width <= 0.0 ) )
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

	w->is_used = UNSET;

	return vars_push( INT_VAR, w->num );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_timebase( Var *v )
{
	double timebase;
	int TB = -1;
	int i;
	char *t;
	

	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( lecroy9400.is_timebase )
				return vars_push( FLOAT_VAR, lecroy9400.timebase );
			else
				return vars_push( FLOAT_VAR, LECROY9400_TEST_TIME_BASE );
		}
		else if ( I_am == PARENT )
		{
			if ( lecroy9400.is_timebase )
				return vars_push( FLOAT_VAR, lecroy9400.timebase );

			eprint( FATAL, SET, "%s: Function %s with no argument can only "
					"be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		lecroy9400.timebase = lecroy9400_get_timebase( );
		lecroy9400.tb_index = lecroy9400_get_tb_index( lecroy9400.timebase );
		lecroy9400.is_timebase = SET;
		return vars_push( FLOAT_VAR, lecroy9400.timebase );
	}

	if ( I_am == CHILD || TEST_RUN )
	{
		eprint( FATAL, SET, "%s: Digitizer time base can only be set before "
				"the EXPERIMENT section starts.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	if ( lecroy9400.is_timebase )
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
				DEVICE_NAME, lecroy9400_ptime( timebase ) );
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
		t = T_strdup( lecroy9400_ptime( timebase ) );
		eprint( WARN, SET, "%s: Can't set timebase to %s, using %s "
				"instead.\n", DEVICE_NAME, t, lecroy9400_ptime( tb[ TB ] ) );
		T_free( t );
	}

	if ( TB < 0 )                                   /* not found yet ? */
	{
		t = T_strdup( lecroy9400_ptime( timebase ) );

		if ( timebase < tb[ 0 ] )
		{
			TB = 0;
			eprint( WARN, SET, "%s: Timebase of %s is too low, using %s "
					"instead.\n", DEVICE_NAME, t,
					lecroy9400_ptime( tb[ TB ] ) );
		}
		else
		{
		    TB = TB_ENTRIES - 1;
			eprint( WARN, SET, "%s: Timebase of %s is too large, using %s "
					"instead.\n", DEVICE_NAME, t,
					lecroy9400_ptime( tb[ TB ] ) );
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

Var *digitizer_sensitivity( Var *v )
{
	long channel;
	double sens;
	

	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: No channel specified in call of %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );

	if ( v->val.lval < LECROY9400_CH1 || v->val.lval > LECROY9400_CH2 )
	{
		eprint( FATAL, SET, "%s: Can't set or obtain sensitivity for "
				"specified channel.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	channel = v->val.lval;

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( TEST_RUN )
		{
			if ( lecroy9400.is_sens[ channel ] )
				return vars_push( FLOAT_VAR, lecroy9400.sens[ channel ] );
			else
				return vars_push( FLOAT_VAR, LECROY9400_TEST_SENSITIVITY );
		}
		else if ( I_am == PARENT )
		{
			if ( lecroy9400.is_sens[ channel ] )
				return vars_push( FLOAT_VAR, lecroy9400.sens[ channel ] );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		lecroy9400.sens[ channel ] = lecroy9400_get_sens( channel );
		lecroy9400.is_sens[ channel ] = SET;
		return vars_push( FLOAT_VAR, lecroy9400.sens[ channel ] );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	sens = VALUE( v );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous parameter%s in call of %s().\n",
				DEVICE_NAME, v->next != NULL ? "s" : "", Cur_Func );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Check that the sensitivity setting is'nt out of range */

	if ( sens < LECROY9400_MAX_SENS )
	{
		eprint( FATAL, SET, "%s: Sensitivity setting is out of range.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}

	if ( TEST_RUN )
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
		eprint( FATAL, SET, "%s: Sensitivity is out of range for the "
				"current impedance of 50 Ohm for %s.\n", DEVICE_NAME,
				Channel_Names[ channel ] );
		THROW( EXCEPTION )
	}
	else if ( coupl != DC_50_OHM && sens > LECROY9400_MIN_SENS_1M )
	{
		eprint( FATAL, SET, "%s: Sensitivity setting is out of range.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}

	lecroy9400.sens[ channel ] = sens;
	lecroy9400.is_sens[ channel ] = SET;

	if ( ! TEST_RUN )
		lecroy9400_set_sens( channel, sens );

	return vars_push( FLOAT_VAR, lecroy9400.sens[ channel ] );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_num_averages( Var *v )
{
	long num_avg;
	

	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: Insufficient number of arguments in call of "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	/* If there's no second argument this is a query and the argument is
	   a channel number. Otherwise the first number is the number of averages,
	   the second the channel number. */

	if ( v->next == NULL )
		vars_check( INT_VAR, v->next == NULL ? v : v->next );

	channel = VALUE( v->next == NULL ? v : v->next );

	if ( channel != LECROY9400_FUNC_E && channel != LECROY9400_FUNC_F )
	{
		eprint( FATAL, SET, "%s: Averaging can only be done with channel %s "
				"and %s.\n", DEVICE_NAME, Channel_Names[ LECROY9400_FUNC_E ],
				Channel_Names[ LECROY9400_FUNC_F ] );
		THROW( EXCEPTION )
	}

	if ( v->next == NULL )                    /* Query form of call */
	{
		if ( TEST_RUN )
		{
			if ( lecroy9400.is_num_avg[ channel ] )
				return vars_push( INT_VAR, lecroy9400.num_avg[ channel ] );
			else
				return vars_push( INT_VAR, LECROY9400_TEST_NUM_AVG );
		}
		else if ( I_am == PARENT )
		{
			if ( lecroy9400.is_num_avg[ channel ] )
				return vars_push( INT_VAR, lecroy9400.num_avg[ channel ] );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		lecroy9400.num_avg[ channel ] = lecroy9400_get_num_avg( channel );
		if ( lecroy9400.num_avg[ channel ] != -1 )
			lecroy9400.is_num_avg[ channel ] = SET;
		return vars_push( INT_VAR, lecroy9400.num_avg[ channel ] );
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
	v = vars_pop( vars_pop( v ) );

	if ( num_avg == 0 )
	{
		eprint( FATAL, SET, "%s: Can't do zero averages. If you want "
				"single acqusitions specify 1 as number of averages.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}
	else if ( num_avg < 0 )
	{
		eprint( FATAL, SET, "%s: Negative number of averages (%ld) in "
				"%s().\n", DEVICE_NAME, num_avg, Cur_Func );
		THROW( EXCEPTION )
	}

	/* Third argument must be the channel to be averaged, either CH1 or CH2 */

	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: Channel to be averaged is missing in call of "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );

	src_channel = v->val.lval;
	if ( src_channel != LECROY9400_CH1 && src_channel != LECROY9400_CH2 )
	{
		eprint( FATAL, SET, "%s: Invalid source channel for averaging in "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	lecroy9400.num_avg[ channel ] = num_avg;
	lecroy9400.avg_src[ channel ] = src_channel;
	if ( I_am == CHILD )
		lecroy9400_set_num_avg( num_avg );
	if ( ! TEST_RUN )                 // store value if in PREPARATIONS section
		lecroy9400.is_num_avg = SET;

	return vars_push( INT_VAR, lecroy9400.num_avg );
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
		return vars_push( INT_VAR, 32000 );

	eprint( FATAL, SET, "%s: Digitizer does not allow to set record "
			"lengths.\n", DECVICE_NAME );
	THROW( EXCEPTION )

	return NULL;
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
			if ( lecroy9400.is_trig_pos )
				return vars_push( FLOAT_VAR, lecroy9400.trig_pos );
			else
				return vars_push( FLOAT_VAR, LECROY9400_TEST_TRIG_POS );
		}
		else if ( I_am == PARENT )
		{
			if ( lecroy9400.is_trig_pos )
				return vars_push( FLOAT_VAR, lecroy9400.trig_pos );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		if ( ! lecroy9400_get_trigger_pos( &trig_pos ) )
			lecroy9400_gpib_failure( );

		lecroy9400.trig_pos = trig_pos;
		lecroy9400.is_trig_pos = SET;
		return vars_push( FLOAT_VAR, lecroy9400.trig_pos );
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

	lecroy9400.trig_pos = trig_pos;
	lecroy9400.is_trig_pos = SET;

	if ( I_am == CHILD && ! lecroy9400_set_trigger_pos( lecroy9400.trig_pos ) )
		lecroy9400_gpib_failure( );

	return vars_push( FLOAT_VAR, lecroy9400.trig_pos );
}


/*----------------------------------------------------------------------*/
/* This is not a function that users should usually call but a function */
/* that allows other functions to check if a certain number stands for  */
/* channel that can be used in measurements.                            */
/*----------------------------------------------------------------------*/

Var *digitizer_meas_channel_ok( Var *v )
{
	vars_check( v, INT_VAR );

	if ( v->val.lval < LECROY9400_CH1 || v->val.lval > LECROY9400_FUNC_F )
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
			if ( lecroy9400.is_trigger_channel )
				return vars_push( INT_VAR, lecroy9400.trigger_channel );
			else
				return vars_push( INT_VAR, LECROY9400_TEST_TRIG_CHANNEL );
		}
		else if ( I_am == PARENT )
		{
			if ( lecroy9400.is_trigger_channel )
				return vars_push( INT_VAR, lecroy9400.trigger_channel );

			eprint( FATAL, SET, "%s: Function %s() with no argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		return vars_push( lecroy9400_get_trigger_channel( ) );
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
        case LECROY9400_CH1 : case LECROY9400_CH2 : case LECROY9400_LIN :
		case LECROY9400_EXT : case LECROY9400_EXT10 :
			lecroy9400.trigger_channel = v->val.lval;
			if ( I_am == CHILD )
				lecroy9400_set_trigger_channel( Channel_Names[ v->val.lval ] );
			if ( ! TEST_RUN )
				lecroy9400.is_trigger_channel = SET;
            break;

		default :
			eprint( FATAL, SET, "%s: Channel %s can't be used as "
					"trigger channel.\n", DEVICE_NAME,
					Channel_Names[ v->val.lval ] );
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
		lecroy9400_start_aquisition( );
	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/


Var *digitizer_get_area( Var *v )
{
	return get_area( v, lecroy9400.w != NULL ? SET : UNSET );
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
	for ( ch = 0; ch <= LECROY9400_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > LECROY9400_REF4 )
	{
		eprint( FATAL, SET, "%s: Invalid channel specification in %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	lecroy9400.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );

		if ( ( w = lecroy9400.w ) == NULL )
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

	if ( I_am == CHILD )
		return vars_push( FLOAT_VAR, lecroy9400_get_area( ch, w, use_cursor ) );

	return vars_push( FLOAT_VAR, 1.234e-8 );
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
	for ( ch = 0; ch <= LECROY9400_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > LECROY9400_REF4 )
	{
		eprint( FATAL, SET, "%s: Invalid channel specification in %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	lecroy9400.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = lecroy9400.w ) == NULL )
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

	if ( I_am == CHILD )
	{
		lecroy9400_get_curve( ch, w, &array, &length, use_cursor );
		nv = vars_push( FLOAT_ARR, array, length );
		T_free( array );
		return nv;
	}

	if ( lecroy9400.is_rec_len  )
		length = lecroy9400.rec_len;
	else
		length = LECROY9400_TEST_REC_LEN;
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
		eprint( FATAL, SET, "%s: Missing arguments in call of %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR );
	for ( ch = 0; ch <= LECROY9400_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > LECROY9400_REF4 )
	{
		eprint( FATAL, SET, "%s: Invalid channel specification in %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	lecroy9400.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = lecroy9400.w ) == NULL )
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
		nv = vars_push( FLOAT_VAR, lecroy9400_get_amplitude( ch, w,
															 use_cursor ) );
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
		lecroy9400_free_running( );
	return vars_push( INT_VAR,1 );
}
