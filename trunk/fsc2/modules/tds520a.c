/*
  $Id$
*/



#define TDS520A_MAIN

#include "tds520a.h"



static Var *get_area( Var *v, bool use_cursor );
static Var *get_curve( Var *v, bool use_cursor );
static Var *get_amplitude( Var *v, bool use_cursor );

/* This array must be set to the possible record lengths of the digitizer
   and must always end with a 0 */

static long record_lengths[ ] = { 500, 1000, 2000, 5000, 15000, 0 };


/*******************************************/
/*   We start with the hook functions...   */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int tds520a_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Initialize some variables in the digitizers structure */

	tds520a.w = NULL;
	tds520a.is_timebase = UNSET;
	tds520a.is_num_avg = UNSET;
	tds520a.num_windows = 0;
	tds520a.data_source = TDS520A_UNDEF;
	tds520a.meas_source = TDS520A_UNDEF;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int tds520a_exp_hook( void )
{
	if ( ! tds520a_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Initialization of device failed.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	tds520a_do_pre_exp_checks( );

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int tds520a_end_of_exp_hook( void )
{
	tds520a_finished( );
	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void tds520a_exit_hook( void )
{
	tds520a_delete_windows( );
}



/*------------------------------------------*/
/*------------------------------------------*/

Var *digitizer_define_window( Var *v )
{
	double win_start, win_width = 0;
	bool is_win_width = UNSET;
	WINDOW *w;


	if ( tds520a.num_windows >= MAX_NUM_OF_WINDOWS )
	{
		eprint( FATAL, "%s:%ld: %s: Maximum number of digitizer windows (%ld) "
				"exceeded.\n", Fname, Lc, DEVICE_NAME, MAX_NUM_OF_WINDOWS );
		THROW( EXCEPTION );
	}

	if ( v == NULL || v->next == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing parameter in call of function "
				"`digitizer_define_window', need at least start position.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Get the start point of the window */

	vars_check( v, INT_VAR | FLOAT_VAR );
	win_start = VALUE( v );
	v = vars_pop( v );

	/* If there's a second parameter take it to be the window width */

	if ( v != NULL )
	{
			vars_check( v, INT_VAR | FLOAT_VAR );
			win_width = VALUE( v );

			if ( win_width <= 0.0 )
			{
				eprint( FATAL, "%s:%ld: %s: Zero or negative width for "
						"window in `digitizer_define_window`.\n",
						Fname, Lc, DEVICE_NAME );
				THROW( EXCEPTION );
			}
			is_win_width = SET;

			if ( ( v = vars_pop( v ) ) != NULL )
			{
				eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of "
						"function `digitizer_define_window'.\n",
						Fname, Lc, DEVICE_NAME );

				while ( ( v = vars_pop( v ) ) != NULL )
					;
			}
	}

	/* Create a new window structure and append it to the list of windows */

	if ( tds520a.w == NULL )
	{
		tds520a.w = w = T_malloc( sizeof( WINDOW ) );
		w->prev = NULL;
	}
	else
	{
		w = tds520a.w;
		while ( w->next != NULL )
			w = w->next;
		w->next = T_malloc( sizeof( WINDOW ) );
		w->next->prev = w;
		w = w->next;
	}

	w->next = NULL;
	w->num = tds520a.num_windows++ + WINDOW_START_NUMBER;
	w->start = win_start;

	if ( is_win_width )
	{
		w->width = win_width;
		w->is_width = SET;
	}
	else
		w->is_width = UNSET;

	w->is_used = UNSET;
	

	return vars_push( INT_VAR, w->num );
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
			if ( tds520a.is_timebase )
				return vars_push( FLOAT_VAR, tds520a.timebase );
			else
				return vars_push( FLOAT_VAR, 0.1 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: %s: Function `digitizer_timebase' with no "
					"argument can only be used in the EXPERIMENT section.\n",
					Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		tds520a.timebase = tds520a_get_timebase( );
		tds520a.is_timebase = SET;
		return vars_push( FLOAT_VAR, tds520a.timebase );
	}

	if ( I_am == CHILD || TEST_RUN )
	{
		eprint( FATAL, "%s:%ld: %s: Digitizer time base can only be set before"
				" the EXPERIMENT section starts.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( tds520a.is_timebase )
	{
		eprint( FATAL, "%s:%ld: %s: Digitizer time base has already been "
				"set.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	timebase = VALUE( v );
	vars_pop( v );

	if ( timebase <= 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid zero or negative time base: %s.\n",
				Fname, Lc, DEVICE_NAME, tds520a_ptime( timebase ) );
		THROW( EXCEPTION );
	}

	tds520a.timebase = timebase;
	tds520a.is_timebase = SET;

	return vars_push( FLOAT_VAR, tds520a.timebase );
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
			if ( tds520a.is_num_avg )
				return vars_push( INT_VAR, tds520a.num_avg );
			else
				return vars_push( INT_VAR, 16 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: %s: Function `digitizer_num_averages' "
					"with no argument can only be used in the EXPERIMENT "
					"section.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		tds520a.num_avg = tds520a_get_num_avg( );
		tds520a.is_num_avg = SET;
		return vars_push( INT_VAR, tds520a.num_avg );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		num_avg = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: %s: Floating point number used as number "
				"of averages.\n", Fname, Lc, DEVICE_NAME );
		num_avg = lround( v->val.dval );
	}
	vars_pop( v );

	if ( num_avg == 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Can't do zero averages. If you want "
				"to set sample mode specify 1 as number of averages.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}
	else if ( num_avg < 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid negative number of averages: "
				"%ld.\n", Fname, Lc, DEVICE_NAME, num_avg );
		THROW( EXCEPTION );
	}

	tds520a.num_avg = num_avg;
	if ( I_am == CHILD )
		tds520a_set_num_avg( num_avg );
	if ( ! TEST_RUN )                 // store value if in PREPARATIONS section
		tds520a.is_num_avg = SET;

	return vars_push( INT_VAR, tds520a.num_avg );
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


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *digitizer_record_length( Var *v )
{
	long rec_len;
	int i;


	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds520a.is_rec_len )
				return vars_push( INT_VAR, tds520a.rec_len );
			else
				return vars_push( INT_VAR, 500 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: %s: Function `digitizer_record_length' "
					"with no argument can only be used in the EXPERIMENT "
					"section.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		if ( ! tds520a_get_record_length( &rec_len ) )
			tds520a_gpib_failure( );

		tds520a.rec_len = rec_len;
		tds520a.is_rec_len = SET;
		return vars_push( INT_VAR, tds520a.rec_len );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, "%s:%ld: %s: Floating point value used as record "
				"length.\n", Fname, Lc, DEVICE_NAME );
		rec_len = lround( v->val.dval );
	}
	else
		rec_len = v->val.lval;

	i = 0;
	while ( 1 )
	{
		if ( record_lengths[ i ] == 0 )
		{
			eprint( FATAL, "%s:%ld: %s: Record length %ld too long.\n",
					Fname, Lc, DEVICE_NAME, rec_len );
			THROW( EXCEPTION );
		}

		if ( rec_len == record_lengths[ i ] )
			break;

		if ( rec_len < record_lengths[ i ] )
		{
			eprint( SEVERE, "%s:%ld: %s: Can't set record length to %ld, "
					"using next larger allowed value of %ld instead.\n",
					Fname, Lc, DEVICE_NAME, rec_len, record_lengths[ i ] );
			break;
		}

		i++;
	}

	tds520a.rec_len = record_lengths[ i ];
	tds520a.is_rec_len = SET;

	if ( I_am == CHILD && ! tds520a_set_record_length( tds520a.rec_len ) )
		tds520a_gpib_failure( );

	return vars_push( INT_VAR, tds520a.rec_len );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *digitizer_trigger_position( Var *v )
{
	double trig_pos;


	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds520a.is_trig_pos )
				return vars_push( FLOAT_VAR, tds520a.trig_pos );
			else
				return vars_push( FLOAT_VAR, 0.1 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: %s: Function `digitizer_trigger_position' "
					"with no argument can only be used in the EXPERIMENT "
					"section.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		if ( ! tds520a_get_trigger_pos( &trig_pos ) )
			tds520a_gpib_failure( );

		tds520a.trig_pos = trig_pos;
		tds520a.is_trig_pos = SET;
		return vars_push( FLOAT_VAR, tds520a.trig_pos );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	trig_pos = VALUE( v );
	vars_pop( v );

	if ( trig_pos < 0.0 || trig_pos > 1.0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid trigger position: %f, must be in "
				"interval [0,1].\n", Fname, Lc, DEVICE_NAME, trig_pos );
		THROW( EXCEPTION );
	}

	tds520a.trig_pos = trig_pos;
	tds520a.is_trig_pos = SET;

	if ( I_am == CHILD && ! tds520a_set_trigger_pos( tds520a.trig_pos ) )
		tds520a_gpib_failure( );

	return vars_push( FLOAT_VAR, tds520a.trig_pos );
}


/*----------------------------------------------------------------------*/
/* This is not a function that users should usually call but a function */
/* that allows other functions to check if a certain number stands for  */
/* channel that can be used in measurements.                            */
/*----------------------------------------------------------------------*/

Var *digitizer_meas_channel_ok( Var *v )
{
	vars_check( v, INT_VAR );

	if ( v->val.lval < TDS520A_CH1 || v->val.lval > TDS520A_REF4 )
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
			if ( tds520a.is_trigger_channel )
				return vars_push( INT_VAR, tds520a.trigger_channel );
			else
				return vars_push( INT_VAR, 0 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: %s: Function `digitizer_trigger_channel' "
					"with no argument can only be used in the EXPERIMENT "
					"section.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		return vars_push( tds520a_get_trigger_channel( ) );
	}

	vars_check( v, INT_VAR );

	if ( v->val.lval < 0 || v->val.lval >= MAX_CHANNELS )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid trigger channel name.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

    switch ( v->val.lval )
    {
        case TDS520A_CH1 : case TDS520A_CH2 : case TDS520A_AUX1 :
		case TDS520A_AUX2 : case TDS520A_LIN :
			tds520a.trigger_channel = v->val.lval;
			if ( I_am == CHILD )
				tds520a_set_trigger_channel( Channel_Names[ v->val.lval ] );
			if ( ! TEST_RUN )
				tds520a.is_trigger_channel = SET;
            break;

		default :
			eprint( FATAL, "%s:%ld: %s: Channel %s can't be used as "
					"trigger channel.\n", Fname, Lc, DEVICE_NAME,
					Channel_Names[ v->val.lval ] );
			THROW( EXCEPTION );
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
		tds520a_start_aquisition( );
	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_get_area( Var *v )
{
	return get_area( v, SET );
}

Var *digitizer_get_area_fast( Var *v )
{
	return get_area( v, UNSET );
}

static Var *get_area( Var *v, bool use_cursor )
{
	WINDOW *w;
	int ch;


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing arguments in call of "
				"function `digitizer_get_area'.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR );
	for ( ch = 0; ch <= TDS520A_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS520A_REF4 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid channel specification.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	tds520a.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );

		if ( ( w = tds520a.w ) == NULL )
		{
			eprint( FATAL, "%s:%ld: %s: No measurement windows have been "
					"defined.\n", Fname, Lc, DEVICE_NAME );
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
			eprint( FATAL, "%s:%ld: %s: Measurement window has not "
					"been defined.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}
	}
	else
		w = NULL;

	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of "
				"function `digitizer_get_area'.\n", Fname, Lc, DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   value */

	if ( I_am == CHILD )
		return vars_push( FLOAT_VAR, tds520a_get_area( ch, w, use_cursor ) );

	return vars_push( FLOAT_VAR, 1.234e-8 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_get_curve( Var *v )
{
	return get_curve( v, SET );
}

Var *digitizer_get_curve_fast( Var *v )
{
	return get_curve( v, UNSET );
}

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
		eprint( FATAL, "%s:%ld: %s: Missing arguments in call of "
				"function `digitizer_get_area'.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR );
	for ( ch = 0; ch <= TDS520A_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS520A_REF4 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid channel specification.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	tds520a.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds520a.w ) == NULL )
		{
			eprint( FATAL, "%s:%ld: %s: No measurement windows have been "
					"defined.\n", Fname, Lc, DEVICE_NAME );
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
			eprint( FATAL, "%s:%ld: %s: Measurement window has not "
					"been defined.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}
	}
	else
		w = NULL;

	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of "
				"function `digitizer_get_area'.\n", Fname, Lc, DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   array */

	if ( I_am == CHILD )
	{
		tds520a_get_curve( ch, w, &array, &length, use_cursor );
		nv = vars_push( FLOAT_TRANS_ARR, array, length );
		T_free( array );
		return nv;
	}

	length = 123;
	array = T_malloc( length * sizeof( double ) );
	for ( i = 0; i < length; i++ )
		array[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );
	nv = vars_push( FLOAT_TRANS_ARR, array, length );
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

Var *digitizer_get_amplitude_fast( Var *v )
{
	return get_amplitude( v, UNSET );
}

static Var *get_amplitude( Var *v, bool use_cursor )
{
	WINDOW *w;
	int ch;
	Var *nv;


	/* The first variable got to be a channel number */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing arguments in call of "
				"function `digitizer_get_amplitude'.\n", Fname, Lc,
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR );
	for ( ch = 0; ch <= TDS520A_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS520A_REF4 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid channel specification.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	tds520a.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds520a.w ) == NULL )
		{
			eprint( FATAL, "%s:%ld: %s: No measurement windows have been "
					"defined.\n", Fname, Lc, DEVICE_NAME );
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
			eprint( FATAL, "%s:%ld: %s: Measurement window has not "
					"been defined.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}
	}
	else
		w = NULL;

	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of "
				"function `digitizer_get_area'.\n", Fname, Lc, DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Talk to digitizer only in the real experiment, otherwise return a dummy
	   array */

	if ( I_am == CHILD )
	{
		nv = vars_push( FLOAT_VAR,
						tds520a_get_amplitude( ch, w, use_cursor ) );
		return nv;
	}

	nv = vars_push( FLOAT_VAR, 1.23e-7 );
	return nv;
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
		vars_check( v, INT_VAR | FLOAT_VAR );

		if ( v->type == INT_VAR )
			lock = v->val.lval == 0 ? UNSET : UNSET;
		else
			lock = v->val.dval == 0.0 ? UNSET : UNSET;
	}

	if ( ! TEST_RUN )
		tds520a_lock_state( lock );

	return vars_push( INT_VAR, lock ? 1 : 0 );
}
