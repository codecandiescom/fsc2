/*
  $Id$
*/



#define TDS540_MAIN

#include "tds540.h"


static Var *get_area( Var *v, bool use_cursor );
static Var *get_curve( Var *v, bool use_cursor );
static Var *get_amplitude( Var *v, bool use_cursor );

/* This array must be set to the available record lengths of the digitizer
   and must always end with a 0 */

static long record_lengths[ ] = { 500, 1000, 2500, 5000, 15000, 50000, 0 };

/* List of all allowed time base values (in seconds) */

static double tb[ 32 ] = {                     500.0e-12,
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

/* Maximum and minimum sensitivity settings (in V) of the measurement
   channels*/

static double max_sens = 1e-3,
              min_sens = 10.0;


/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must really be
   reasonable ! */

#define TEST_REC_LEN      500
#define TEST_TIME_BASE    0.1
#define TEST_SENSITIVITY  0.01
#define TEST_NUM_AVG      16
#define TEST_TRIG_POS     0.1
#define TEST_TRIG_CHANNEL 0


/*******************************************/
/*   We start with the hook functions...   */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int tds540_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Initialize some variables in the digitizers structure */

	tds540.is_reacting = UNSET;
	tds540.w = NULL;
	tds540.is_timebase = UNSET;
	tds540.is_num_avg = UNSET;
	tds540.is_rec_len = UNSET;
	tds540.is_trig_pos = UNSET;
	tds540.num_windows = 0;
	tds540.data_source = TDS540_UNDEF;
	tds540.meas_source = TDS540_UNDEF;

	for ( i = TDS540_CH1; i <= TDS540_CH4; i++ )
		tds540.is_sens[ i ] = UNSET;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int tds540_exp_hook( void )
{
	if ( ! tds540_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Initialization of device failed: %s\n",
				DEVICE_NAME, gpib_error_msg );
		THROW( EXCEPTION );
	}

	tds540_do_pre_exp_checks( );

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int tds540_end_of_exp_hook( void )
{
	tds540_finished( );
	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void tds540_exit_hook( void )
{
	tds540_delete_windows( );
}



/*------------------------------------------*/
/*------------------------------------------*/

Var *digitizer_define_window( Var *v )
{
	double win_start, win_width = 0;
	bool is_win_width = UNSET;
	WINDOW *w;


	if ( tds540.num_windows >= MAX_NUM_OF_WINDOWS )
	{
		eprint( FATAL, "%s:%ld: %s: Maximum number of digitizer windows (%ld) "
				"exceeded.\n", Fname, Lc, DEVICE_NAME, MAX_NUM_OF_WINDOWS );
		THROW( EXCEPTION );
	}

	if ( v == NULL )
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
						"window in `digitizer_define_window'.\n",
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

	if ( tds540.w == NULL )
	{
		tds540.w = w = T_malloc( sizeof( WINDOW ) );
		w->prev = NULL;
	}
	else
	{
		w = tds540.w;
		while ( w->next != NULL )
			w = w->next;
		w->next = T_malloc( sizeof( WINDOW ) );
		w->next->prev = w;
		w = w->next;
	}

	w->next = NULL;
	w->num = tds540.num_windows++ + WINDOW_START_NUMBER;
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
	int TB = -1;
	int i;
	char *t;
	

	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds540.is_timebase )
				return vars_push( FLOAT_VAR, tds540.timebase );
			else
				return vars_push( FLOAT_VAR, TEST_TIME_BASE );
		}
		else if ( I_am == PARENT )
		{
			if ( tds540.is_timebase )
				return vars_push( FLOAT_VAR, tds540.timebase );

			eprint( FATAL, "%s:%ld: %s: Function `digitizer_timebase' with no "
					"argument can only be used in the EXPERIMENT section.\n",
					Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		tds540.timebase = tds540_get_timebase( );
		tds540.is_timebase = SET;
		return vars_push( FLOAT_VAR, tds540.timebase );
	}

	if ( I_am == CHILD || TEST_RUN )
	{
		eprint( FATAL, "%s:%ld: %s: Digitizer time base can only be set before"
				" the EXPERIMENT section starts.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( tds540.is_timebase )
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
				Fname, Lc, DEVICE_NAME, tds540_ptime( timebase ) );
		THROW( EXCEPTION );
	}

	/* Pick the allowed timebase nearest to the user supplied value */

	for ( i = 0; i < 31; i++ )
		if ( timebase >= tb[ i ] && timebase <= tb[ i + 1 ] )
		{
			TB = i +
				   ( ( tb[ i ] / timebase > timebase / tb[ i + 1 ] ) ? 0 : 1 );
			break;
		}

	if ( TB >= 0 &&                                         /* value found ? */
		 fabs( timebase - tb[ TB ] ) > timebase * 1.0e-2 )  /* error > 1% ?  */
	{
		t = T_strdup( tds540_ptime( timebase ) );
		eprint( WARN, "%s:%ld: %s: Can't set timebase to %s, using %s "
				"instead.\n", Fname, Lc, DEVICE_NAME,
				t, tds540_ptime( tb[ TB ] ) );
		T_free( t );
	}

	if ( TB < 0 )                                   /* not found yet ? */
	{
		t = T_strdup( tds540_ptime( timebase ) );

		if ( timebase < tb[ 0 ] )
		{
			timebase = tb[ 0 ];
			eprint( WARN, "%s:%ld: %s: Timebase of %s is too low, using %s "
					"instead.\n", Fname, Lc, DEVICE_NAME,
					t, tds540_ptime( timebase ) );
		}
		else
		{
		    timebase = tb[ 31 ];
			eprint( WARN, "%s:%ld: %s: Timebase of %s is too large, using "
					"%s instead.\n", Fname, Lc, DEVICE_NAME,
					t, tds540_ptime( timebase ) );
		}

		T_free( t );
	}

	tds540.timebase = timebase;
	tds540.is_timebase = SET;

	return vars_push( FLOAT_VAR, tds540.timebase );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_sensitivity( Var *v )
{
	long channel;
	double sens;
	

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing parameter in call of "
				"`digitizer_sensitivity'.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR );

	if ( v->val.lval < TDS540_CH1 || v->val.lval > TDS540_CH4 )
	{
		eprint( FATAL, "%s:%ld: %s: Can't set or obtain sensitivity for "
				"specified channel.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	channel = v->val.lval;

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds540.is_sens[ channel ] )
				return vars_push( FLOAT_VAR, tds540.sens[ channel ] );
			else
				return vars_push( FLOAT_VAR, TEST_SENSITIVITY );
		}
		else if ( I_am == PARENT )
		{
			if ( tds540.is_sens[ channel ] )
				return vars_push( FLOAT_VAR, tds540.sens[ channel ] );

			eprint( FATAL, "%s:%ld: %s: Function `digitizer_sensitivity' with "
					"no argument can only be used in the EXPERIMENT "
					"section.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		tds540.sens[ channel ] = tds540_get_sens( channel );
		tds540.is_sens[ channel ] = SET;
		return vars_push( FLOAT_VAR, tds540.sens[ channel ] );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	sens = VALUE( v );

	if ( sens < max_sens || sens > min_sens )
	{
		eprint( FATAL, "%s:%ld: %s: Sensitivity setting is out of range.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	tds540.sens[ channel ] = sens;
	tds540.is_sens[ channel ] = SET;

	if ( ! TEST_RUN )
		tds540_set_sens( channel, sens );

	if ( ( v = vars_pop( v ) ) != NULL )
		eprint( WARN, "%s:%ld: %s: Superfluous parameter in call of "
				"`digitizer_sensitivity'.\n", Fname, Lc, DEVICE_NAME );

	return vars_push( FLOAT_VAR, tds540.sens[ channel ] );
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
			if ( tds540.is_num_avg )
				return vars_push( INT_VAR, tds540.num_avg );
			else
				return vars_push( INT_VAR, TEST_NUM_AVG );
		}
		else if ( I_am == PARENT )
		{
			if ( tds540.is_num_avg )
				return vars_push( INT_VAR, tds540.num_avg );

			eprint( FATAL, "%s:%ld: %s: Function `digitizer_num_averages' "
					"with no argument can only be used in the EXPERIMENT "
					"section.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		tds540.num_avg = tds540_get_num_avg( );
		tds540.is_num_avg = SET;
		return vars_push( INT_VAR, tds540.num_avg );
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

	tds540.num_avg = num_avg;
	if ( I_am == CHILD )
		tds540_set_num_avg( num_avg );
	if ( ! TEST_RUN )                 // store value if in PREPARATIONS section
		tds540.is_num_avg = SET;

	return vars_push( INT_VAR, tds540.num_avg );
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
			if ( tds540.is_rec_len )
				return vars_push( INT_VAR, tds540.rec_len );
			else
				return vars_push( INT_VAR, TEST_REC_LEN );
		}
		else if ( I_am == PARENT )
		{
			if ( tds540.is_rec_len )
				return vars_push( INT_VAR, tds540.rec_len );

			eprint( FATAL, "%s:%ld: %s: Function `digitizer_record_length' "
					"with no argument can only be used in the EXPERIMENT "
					"section.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		if ( ! tds540_get_record_length( &rec_len ) )
			tds540_gpib_failure( );

		tds540.rec_len = rec_len;
		tds540.is_rec_len = SET;
		return vars_push( INT_VAR, tds540.rec_len );
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

	tds540.rec_len = record_lengths[ i ];
	tds540.is_rec_len = SET;

	if ( I_am == CHILD && ! tds540_set_record_length( tds540.rec_len ) )
		tds540_gpib_failure( );

	return vars_push( INT_VAR, tds540.rec_len );
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
			if ( tds540.is_trig_pos )
				return vars_push( FLOAT_VAR, tds540.trig_pos );
			else
				return vars_push( FLOAT_VAR, TEST_TRIG_POS );
		}
		else if ( I_am == PARENT )
		{
			if ( tds540.is_trig_pos )
				return vars_push( FLOAT_VAR, tds540.trig_pos );

			eprint( FATAL, "%s:%ld: %s: Function `digitizer_trigger_position' "
					"with no argument can only be used in the EXPERIMENT "
					"section.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		if ( ! tds540_get_trigger_pos( &trig_pos ) )
			tds540_gpib_failure( );

		tds540.trig_pos = trig_pos;
		tds540.is_trig_pos = SET;
		return vars_push( FLOAT_VAR, tds540.trig_pos );
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

	tds540.trig_pos = trig_pos;
	tds540.is_trig_pos = SET;

	if ( I_am == CHILD && ! tds540_set_trigger_pos( tds540.trig_pos ) )
		tds540_gpib_failure( );

	return vars_push( FLOAT_VAR, tds540.trig_pos );
}


/*----------------------------------------------------------------------*/
/* This is not a function that users should usually call but a function */
/* that allows other functions to check if a certain number stands for  */
/* channel that can be used in measurements.                            */
/*----------------------------------------------------------------------*/

Var *digitizer_meas_channel_ok( Var *v )
{
	vars_check( v, INT_VAR );

	if ( v->val.lval < TDS540_CH1 || v->val.lval > TDS540_REF4 )
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
			if ( tds540.is_trigger_channel )
				return vars_push( INT_VAR, tds540.trigger_channel );
			else
				return vars_push( INT_VAR, TEST_TRIG_CHANNEL );
		}
		else if ( I_am == PARENT )
		{
			if ( tds540.is_trigger_channel )
				return vars_push( INT_VAR, tds540.trigger_channel );

			eprint( FATAL, "%s:%ld: %s: Function `digitizer_trigger_channel' "
					"with no argument can only be used in the EXPERIMENT "
					"section.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}

		return vars_push( tds540_get_trigger_channel( ) );
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
        case TDS540_CH1 : case TDS540_CH2 : case TDS540_CH3 :
		case TDS540_CH4 : case TDS540_AUX : case TDS540_LIN :
			tds540.trigger_channel = v->val.lval;
			if ( I_am == CHILD )
				tds540_set_trigger_channel( Channel_Names[ v->val.lval ] );
			if ( ! TEST_RUN )
				tds540.is_trigger_channel = SET;
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
		tds540_start_aquisition( );
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
	for ( ch = 0; ch <= TDS540_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS540_REF4 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid channel specification.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	tds540.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );

		if ( ( w = tds540.w ) == NULL )
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
		return vars_push( FLOAT_VAR, tds540_get_area( ch, w, use_cursor ) );

	return vars_push( FLOAT_VAR, 1.234e-8 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_get_curve( Var *v )
{
	return get_curve( v, tds540.w != NULL ? SET : UNSET );
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
	for ( ch = 0; ch <= TDS540_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS540_REF4 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid channel specification.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	tds540.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds540.w ) == NULL )
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
		tds540_get_curve( ch, w, &array, &length, use_cursor );
		nv = vars_push( FLOAT_TRANS_ARR, array, length );
		T_free( array );
		return nv;
	}

	length = TEST_REC_LEN;
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
	for ( ch = 0; ch <= TDS540_REF4; ch++ )
		if ( ch == ( int ) v->val.lval )
			break;

	if ( ch > TDS540_REF4 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid channel specification.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	tds540.channels_in_use[ ch ] = SET;

	v = vars_pop( v );

	/* Now check if there's a variable with a window number and check it */

	if ( v != NULL )
	{
		vars_check( v, INT_VAR );
		if ( ( w = tds540.w ) == NULL )
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
						tds540_get_amplitude( ch, w, use_cursor ) );
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
		tds540_lock_state( lock );

	return vars_push( INT_VAR, lock ? 1 : 0 );
}
