/*
  $Id$
*/



#define TDS754A_MAIN

#include "tds754a.h"




/*******************************************/
/*   We start with the hook functions...   */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int tds754a_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Initialize some variables in the digitizers structure */

	tds754a.w = NULL;
	tds754a.is_timebase = UNSET;
	tds754a.is_num_avg = UNSET;
	tds754a.num_windows = 0;
	tds754a.data_source = TDS754A_UNDEF;
	tds754a.meas_source = TDS754A_UNDEF;

	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int tds754a_test_hook( void )
{
	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int tds754a_end_of_test_hook( void )
{
	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int tds754a_exp_hook( void )
{
   	WINDOW *w;
	double width;


	if ( ! tds754a_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "TDS754A: Initialization of device failed." );
		THROW( EXCEPTION );
	}

	tds754a_do_pre_exp_checks( );

	/* Test if for all window the width is set */

	for ( w = tds754a.w; w != NULL; w = w->next )
		if ( ! w->is_width )
			break;

	/* If not get the distance of the cursors on the digitizers screen
	   and use it as the default width */

	if ( w != NULL )
	{
		tds754a_get_cursor_distance( &width );

			for ( w = tds754a.w; w != NULL; w = w->next )
				if ( ! w->is_width )
				{
					w->width = width;
					w->is_width = SET;
				}
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int tds754a_end_of_exp_hook( void )
{
#ifndef MAX_DEBUG
	tds754a_finished( );
#endif

	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void tds754a_exit_hook( void )
{
	tds754a_delete_windows( );
}



/*------------------------------------------*/
/*------------------------------------------*/

Var *digitizer_define_window( Var *v )
{
	long win_num;
	double win_start, win_width;
	bool is_win_width = UNSET;
	WINDOW *w;


	if ( v == NULL || v->next == NULL )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Missing parameter in call of "
				"function `digitizer_define_window', need at least two.",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Get the window number */

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		win_num = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: TDS754A: Floating point number used as window "
				"number.", Fname, Lc );
		win_num = lround( v->val.dval );
	}

	v = vars_pop( v );

	/* Test if the window number non-negative and no window with this number
	   already exists */

	if ( win_num < 0 )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid negative window number: "
				"%ld.", Fname, Lc, win_num );
		THROW( EXCEPTION );
	}

	for ( w = tds754a.w; w != NULL; w = w->next )
		if ( ( long ) w->num == win_num )
		{
			eprint( FATAL, "%s:%ld: Window %ld has already been defined.",
					Fname, Lc, win_num );
			THROW( EXCEPTION );
		}

	/* Now get the start point of the window */

	vars_check( v, INT_VAR | FLOAT_VAR );
	win_start = VALUE( v );
	v = vars_pop( v );

	/* Finally, if there's another parameter take it to be the window
	   width */

	if ( v != NULL )
	{
			vars_check( v, INT_VAR | FLOAT_VAR );
			win_width = VALUE( v );
			is_win_width = SET;

			v = vars_pop( v );

			if ( v != NULL )
			{
				eprint( WARN, "%s:%ld: Superfluous arguments in call of "
						"function `digitizer_define_window'.", Fname, Lc );

				while ( ( v = vars_pop( v ) ) != NULL )
					;
			}
	}

	/* Create a new window structure and append it to the list of windows */

	if ( tds754a.w == NULL )
		tds754a.w = w = T_malloc( sizeof( WINDOW ) );
	else
	{
		w = tds754a.w;
		while ( w->next != NULL )
			w = w->next;
		w->next = T_malloc( sizeof( WINDOW ) );
		w = w->next;
	}

	w->next = NULL;
	w->num = win_num;
	w->start = win_start;

	if ( is_win_width )
	{
		w->width = win_width;
		w->is_width = SET;
	}
	else
		w->is_width = UNSET;

	w->is_used = UNSET;
	tds754a.num_windows++;

	return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_timebase( Var *v )
{
	double timebase;
	

	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_timebase )
				return vars_push( FLOAT_VAR, tds754a.timebase );
			else
				return vars_push( FLOAT_VAR, 0.1 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: TDS754A: Function `digitizer_timebase' "
					"with no argument can only be used in the EXPERIMENT "
					"section.", Fname, Lc );
			THROW( EXCEPTION );
		}

		tds754a.timebase = tds754a_get_timebase( );
		tds754a.is_timebase = SET;
		return vars_push( FLOAT_VAR, tds754a.timebase );
	}

	if ( I_am == CHILD || TEST_RUN )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Digitizer time base can only be set "
				"before the EXPERIMENT section starts.", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( tds754a.is_timebase )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Digitizer time base has already been "
				"set.", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	timebase = VALUE( v );
	vars_pop( v );

	if ( timebase <= 0 )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid zero or negative time base: "
				"%s.",Fname, Lc, tds754a_ptime( timebase ) );
		THROW( EXCEPTION );
	}

	tds754a.timebase = timebase;
	tds754a.is_timebase = SET;

	return vars_push( FLOAT_VAR, tds754a.timebase );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_num_averages( Var *v )
{
	double num_avg;
	

	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_num_avg )
				return vars_push( INT_VAR, tds754a.num_avg );
			else
				return vars_push( INT_VAR, 16 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: TDS754A: Function "
					"`digitizer_num_averages' with no argument can only be "
					"used in the EXPERIMENT section.", Fname, Lc );
			THROW( EXCEPTION );
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
		eprint( WARN, "%s:%ld: TDS754A: Floating point number used as number "
				"of averages.", Fname, Lc );
		num_avg = lround( v->val.dval );
	}
	vars_pop( v );

	if ( num_avg < 0 )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid negative number of averages: "
				"%ld.", Fname, Lc, num_avg );
		THROW( EXCEPTION );
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
				return vars_push( INT_VAR, 0 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: TDS754A: Function "
					"`digitizer_trigger_channel' with no argument can only be "
					"used in the EXPERIMENT section.", Fname, Lc );
			THROW( EXCEPTION );
		}

		return vars_push( tds754a_get_trigger_channel( ) );
	}

	vars_check( v, INT_VAR );

	if ( v->val.lval < 0 || v->val.lval >= MAX_CHANNELS )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid trigger channel name.\n",
				Fname, Lc );
		THROW( EXCEPTION );
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
			eprint( FATAL, "%s:%ld: TDS754A: Channel %s can't be used as "
					"trigger channel.\n", Fname, Lc,
					Channel_Names[ v->val.lval ] );
			THROW( EXCEPTION );
    }

	vars_pop( v );
	return vars_push( INT_VAR, 1 );
}


Var *digitizer_start_acquisition( Var *v )
{
	v = v;
	tds754a_start_aquisition( );
	return vars_push( INT_VAR, 1 );
}

Var *digitizer_get_area( Var *v )
{
	WINDOW *w;
	int ch;


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Missing arguments in call of "
				"function `digitizer_get_area'.", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR );
	for ( ch = 0; ch <= TDS754A_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS754A_REF4 )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid channel specification.",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	tds754a.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds754a.w ) == NULL )
		{
			eprint( FATAL, "%s:%ld: TDS754A: No measurement windows have been "
					"defined.", Fname, Lc );
			THROW( EXCEPTION );
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
			eprint( FATAL, "%s:%ld: TDS754A: Measurement window %ld has not "
					"been defined.", Fname, Lc, v->val.lval );
			THROW( EXCEPTION );
		}
	}
	else
		w = NULL;

	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: TDS754A: Superfluous arguments in call of "
				"function `digitizer_get_area'.", Fname, Lc );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   value */

	if ( I_am == CHILD )
		return vars_push( FLOAT_VAR, tds754a_get_area( ch, w ) );

	return vars_push( FLOAT_VAR, 1.234e-8 );
}


Var *digitizer_get_curve( Var *v )
{
	WINDOW *w;
	int ch, i;
	double *array;
	long length;
	Var *nv;


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Missing arguments in call of "
				"function `digitizer_get_area'.", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR );
	for ( ch = 0; ch <= TDS754A_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS754A_REF4 )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid channel specification.",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	tds754a.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds754a.w ) == NULL )
		{
			eprint( FATAL, "%s:%ld: TDS754A: No measurement windows have been "
					"defined.", Fname, Lc );
			THROW( EXCEPTION );
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
			eprint( FATAL, "%s:%ld: TDS754A: Measurement window %ld has not "
					"been defined.", Fname, Lc, v->val.lval );
			THROW( EXCEPTION );
		}
	}
	else
		w = NULL;

	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: TDS754A: Superfluous arguments in call of "
				"function `digitizer_get_area'.", Fname, Lc );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   array */

	if ( I_am == CHILD )
	{
		tds754a_get_curve( ch, w, &array, &length );
		nv = vars_push( FLOAT_TRANS_ARR, array, length );
		T_free( array );
		return nv;
	}

	length = 123;
	array = T_malloc( length * sizeof( long ) );
	for ( i = 0; i < length; i++ )
		array[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );
	nv = vars_push( FLOAT_TRANS_ARR, array, length );
	T_free( array );
	return nv;
}
