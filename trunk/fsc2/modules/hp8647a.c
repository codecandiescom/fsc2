/*
  $Id$
*/


#define HP8647A_MAIN

#include "hp8647a.h"
#include "gpib.h"


static HP8647A hp8647a_backup;


static bool hp8647a_init( const char *name );
static void hp8647a_finished( void );
static bool hp8647a_set_output_state( bool state );
static bool hp8647a_get_output_state( void );
static double hp8647a_set_frequency( double freq );
static double hp8647a_get_frequency( void );
static double hp8647a_set_attenuation( double att );
static double hp8647a_get_attenuation( void );
static void hp8647a_comm_failure( void );


/*******************************************/
/*   the hook functions...                 */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int hp8647a_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	hp8647a.device = -1;

	hp8647a.state = UNSET;

	hp8647a.freq_is_set = UNSET;
	hp8647a.step_freq_is_set = UNSET;
	hp8647a.start_freq_is_set = UNSET;
	hp8647a.attenuation_is_set = UNSET;

	hp8647a.table_file = NULL;
	hp8647a.use_table = UNSET;
	hp8647a.att_table = NULL;
	hp8647a.att_table_len = 0;

	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int hp8647a_test_hook( void )
{
	memcpy( &hp8647a_backup, &hp8647a, sizeof( HP8647A ) );
	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int hp8647a_end_of_test_hook( void )
{
	memcpy( &hp8647a, &hp8647a_backup, sizeof( HP8647A ) );
	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int hp8647a_exp_hook( void )
{
	if ( ! hp8647a_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Initialization of device failed.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int hp8647a_end_of_exp_hook( void )
{
	hp8647a_finished( );
	hp8647a.device = -1;

	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void hp8647a_exit_hook( void )
{
	if ( hp8647a.table_file )
		T_free( hp8647a.table_file );
	if ( hp8647a.att_table )
		T_free( hp8647a.att_table );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *synthesizer_state( Var *v )
{
	bool state;


	if ( v == NULL )              /* i.e. return the current state */
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, ( long ) hp8647a.state );
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: %s: Function `synthesizer_state' with no "
					"argument can only be used in the EXPERIMENT section.\n",
					Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}
		else
			return vars_push( INT_VAR,
							  ( long ) ( hp8647a.state =
										 hp8647a_get_output_state( ) ) );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type != INT_VAR )
	{
		eprint( WARN, "%s:%ld: %s: Float variable used for synthesizer "
				"state.\n", Fname, Lc, DEVICE_NAME );
		state = ( v->val.dval != 0.0 );
	}
	else
		state = ( v->val.lval != 0 );

	hp8647a.state = state;
	if ( TEST_RUN || I_am == PARENT )
		return vars_push( INT_VAR, ( long )state );

	return vars_push( INT_VAR, ( long ) hp8647a_set_output_state( state ) );
}

/*---------------------------------------------------------------------*/
/* Function sets or returns (if called with no argument) the frequency */
/* of the synthesizer. If called for setting the frequency before the  */
/* experiment is started the frequency value is stored and set in the  */
/* setup phase of the experiment. The frequency set the first time the */
/* function is called is also set as the start frequency to be used in */
/* calls of `synthesizer_reset_frequency'. The function can only be    */
/* called once in the PREPARATIONS section, further calls just result  */
/* in a warning and the new value isn't accepted.                      */
/*---------------------------------------------------------------------*/

Var *synthesizer_frequency( Var *v )
{
	double freq;


	if ( v == NULL )              /* i.e. return the current frequency */
	{
		if ( TEST_RUN || I_am == PARENT )
		{
			if ( ! hp8647a.freq_is_set )
			{
				eprint( FATAL, "%s:%ld: %s: RF frequency hasn't been set "
						"yet.\n", Fname, Lc, DEVICE_NAME );
				THROW( EXCEPTION );
			}
			else
				return vars_push( FLOAT_VAR, hp8647a.freq );
		}
		else
		{
			hp8647a.freq = hp8647a_get_frequency( );
			return vars_push( FLOAT_VAR, hp8647a.freq );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer variable used as RF frequency.\n",
				Fname, Lc, DEVICE_NAME );

	freq = VALUE( v );

	if ( freq < 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid negative RF frequency.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( freq < MIN_FREQ || freq > MAX_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: RF frequency (%f MHz) not within valid "
				"range (%f kHz to %g Mhz).\n", Fname, Lc, DEVICE_NAME,
				1.0e-6 * freq, 1.0e-3 * MIN_FREQ, 1.0e-6 * MAX_FREQ );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of "
				"function `synthesizer_frequency'.\n", Fname, Lc,
				DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( TEST_RUN )                      /* In test run of experiment */
	{
		hp8647a.freq = freq;
		hp8647a.freq_is_set = SET;
		if ( ! hp8647a.start_freq_is_set )
		{
			hp8647a.start_freq = freq;
			hp8647a.start_freq_is_set = SET;
		}
	}
	else if ( I_am == PARENT )           /* in PREPARATIONS section */
	{
		if ( hp8647a.freq_is_set )
		{
			eprint( SEVERE, "%s:%ld: %s: RF frequency has already been set in "
					"the PREPARATIONS section to %f MHz, keeping old value.\n",
					Fname, Lc, DEVICE_NAME, 1.0e-6 * hp8647a.freq );
			return vars_push( FLOAT_VAR, hp8647a.freq );
		}

		hp8647a.freq = hp8647a.start_freq = freq;
		hp8647a.freq_is_set = SET;
		hp8647a.start_freq_is_set = SET;
	}
	else
	{
		hp8647a.freq = hp8647a_set_frequency( freq );
		if ( ! hp8647a.start_freq_is_set )
		{
			hp8647a.start_freq = freq;
			hp8647a.start_freq_is_set = SET;
		}
	}

	return vars_push( FLOAT_VAR, freq );
}


/*-----------------------------------------------------------------------*/
/* Function sets or returns (if called with no argument) the attenuation */
/* of the synthesizer. If called for setting the attenuation before the  */
/* experiment is started the attenuation value is stored and set in the  */
/* setup phase of the attenuation. The function can only be called once  */
/* in the PREPARATIONS section, further calls just result in a warning   */
/* and the new value isn't accepted.                                     */
/*-----------------------------------------------------------------------*/

Var *synthesizer_attenuation( Var *v )
{
	double att;


	if ( v == NULL )              /* i.e. return the current attenuation */
	{
		if ( TEST_RUN || I_am == PARENT )
		{
			if ( ! hp8647a.attenuation_is_set )
			{
				eprint( FATAL, "%s:%ld: %s: RF attenuation has not been set "
						"yet.\n", Fname, Lc, DEVICE_NAME );
				THROW( EXCEPTION );
			}
			else
				return vars_push( FLOAT_VAR, hp8647a.attenuation );
		}
		else
		{
			hp8647a.attenuation = hp8647a_get_attenuation( );
			return vars_push( FLOAT_VAR, hp8647a.attenuation );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer variable used as RF attenuation.\n",
				Fname, Lc, DEVICE_NAME );

	att = VALUE( v );

	if ( att > MIN_ATTEN || att < MAX_ATTEN )
	{
		eprint( FATAL, "%s:%ld: %s: RF attenuation (%g db) not within valid "
				"range (%g dB to %g dB).\n", Fname, Lc, DEVICE_NAME, att,
				MAX_ATTEN, MIN_ATTEN );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of "
				"function `synthesizer_attenuation'.\n",
				Fname, Lc, DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( TEST_RUN )                      /* In test run of experiment */
	{
		hp8647a.attenuation = att;
		hp8647a.attenuation_is_set = SET;
	}
	else if ( I_am == PARENT )           /* in PREPARATIONS section */
	{
		if ( hp8647a.attenuation_is_set )
		{
			eprint( SEVERE, "%s:%ld: %s: RF attenuation has already been set "
					"in the PREPARATIONS section to %g dB, keeping old "
					"value.\n", Fname, Lc, DEVICE_NAME, hp8647a.attenuation );
			return vars_push( FLOAT_VAR, hp8647a.attenuation );
		}

		hp8647a.attenuation = att;
		hp8647a.attenuation_is_set = SET;
	}
	else
		hp8647a.attenuation = hp8647a_set_attenuation( att );

	return vars_push( FLOAT_VAR, att );
}


/*-----------------------------------------------------------*/
/* Function sets or returns (if called with no argument) the */
/* step frequency for RF sweeps.                             */
/*-----------------------------------------------------------*/

Var *synthesizer_step_frequency( Var *v )
{
	if ( v != NULL )
	{
		vars_check( v, INT_VAR | FLOAT_VAR );

		/* Allow setting of the step frequency in the PREPARATIONS section
		   onl[y once */

		if ( ! TEST_RUN && I_am == PARENT && hp8647a.step_freq_is_set )
		{
			eprint( SEVERE, "%s:%ld: %s: RF step frequency has already been "
					"set in the PREPARATIONS section to %f MHz, keeping old "
					"value.\n", Fname, Lc, DEVICE_NAME,
					1.0e-6 * hp8647a.step_freq );
			while ( ( v = vars_pop( v ) ) != NULL )
				;
			return vars_push( FLOAT_VAR, hp8647a.step_freq );
		}

		if ( v->type == INT_VAR )
			eprint( WARN, "%s:%ld: %s: Integer variable used as RF step "
					"frequency.\n", Fname, Lc, DEVICE_NAME );

		hp8647a.step_freq = VALUE( v );

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of "
					"function `synthesizer_step_frequency'.\n", Fname,
					Lc, DEVICE_NAME );
			while ( ( v = vars_pop( v ) ) != NULL )
				;
		}

		hp8647a.step_freq_is_set = SET;
	}
	else if ( ! hp8647a.step_freq_is_set )
	{
		eprint( FATAL, "%s:%ld: %s: RF step frequency has notz been set "
				"yet.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, hp8647a.step_freq );
}


/*-------------------------------------------------------------*/
/* This function may only be called in the EXPERIMENT section! */
/*-------------------------------------------------------------*/

Var *synthesizer_sweep_up( Var *v )
{
	double new_freq;


	v = v;

	if ( ! hp8647a.step_freq_is_set )
	{
		eprint( FATAL, "%s:%ld: %s: RF step frequency hasn't been set.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	new_freq = hp8647a.freq + hp8647a.step_freq;

	if ( new_freq < MIN_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: RF frequency is dropping below lower "
				"limit of %f kHz.\n", Fname, Lc, DEVICE_NAME,
				1.0e-3 * MIN_FREQ );
		THROW( EXCEPTION );
	}

	if ( new_freq > MAX_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: RF frequency is increased above upper "
				"limit of %f MHz.\n", Fname, Lc, DEVICE_NAME,
				1.0e-6 * MAX_FREQ );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		hp8647a.freq = new_freq;
	else
		hp8647a.freq = hp8647a_set_frequency( new_freq );

	return vars_push( FLOAT_VAR, hp8647a.freq );
}


/*-------------------------------------------------------------*/
/* This function may only be called in the EXPERIMENT section! */
/*-------------------------------------------------------------*/

Var *synthesizer_sweep_down( Var *v )
{
	Var *nv;


	v = v;
	hp8647a.step_freq *= -1.0;
	nv = synthesizer_sweep_up( NULL );
	hp8647a.step_freq *= -1.0;
	return nv;
}


/*-------------------------------------------------------------*/
/* This function may only be called in the EXPERIMENT section! */
/*-------------------------------------------------------------*/

Var *synthesizer_reset_frequency( Var *v )
{
	v = v;

	if ( ! hp8647a.start_freq_is_set )
	{
		eprint( FATAL, "%s:%ld: %s: No RF frequency has been set yet, so "
				"can't do a frequency reset.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		hp8647a.freq = hp8647a.start_freq;
	else
		hp8647a.freq = hp8647a_set_frequency( hp8647a.start_freq );

	return vars_push( FLOAT_VAR, hp8647a.freq );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *synthesizer_use_table( Var *v )
{
	FILE *tfp;
	char *tfname;


	/* Try to figure out the name of the table file - if no argument is given
	   use the default table file, otherwise use the user supplied file name */

	if ( v == NULL )
	{
		hp8647a.table_file = get_string( strlen( libdir ) +
										 strlen( DEFAULT_TABLE_FILE ) + 2 );
		strcpy( hp8647a.table_file, libdir );
		if ( libdir[ strlen( libdir ) - 1 ] != '/' )
			strcat( hp8647a.table_file, "/" );
		strcat( hp8647a.table_file, DEFAULT_TABLE_FILE );

		if ( ( tfp = hp8647a_open_table( hp8647a.table_file ) ) == NULL )
		{
			eprint( FATAL, "%s:%ld: %s: Default table file `%s' not found.\n",
					Fname, Lc, DEVICE_NAME, hp8647a.table_file );
			T_free( hp8647a.table_file );
			THROW( EXCEPTION );
		}
	}
	else
	{
		vars_check( v, STR_VAR );

		tfname = get_string_copy( v->val.sptr );
		v = vars_pop( v );

		if ( v != NULL )
		{
			eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of "
					"function `synthesizer_use_table'.\n",
					Fname, Lc, DEVICE_NAME );
			while ( ( v = vars_pop( v ) ) != NULL )
				;
		}

		TRY
		{
			tfp = hp8647a_find_table( tfname );
			TRY_SUCCESS;
		}
		CATCH( EXCEPTION )
		{
			T_free( hp8647a.table_file );
			hp8647a.table_file = NULL;
			PASSTHROU( );
		}
	}

	TRY
	{
		hp8647a_read_table( tfp );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		fclose( tfp );
		T_free( hp8647a.table_file );
		hp8647a.table_file = NULL;
		PASSTHROU( );
	}

	fclose( tfp );
	T_free( hp8647a.table_file );
	hp8647a.table_file = NULL;
	return vars_push( INT_VAR, 1 );
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static bool hp8647a_init( const char *name )
{
	if ( gpib_init_device( name, &hp8647a.device ) == FAILURE )
        return FAIL;

	/* If frequency and attenuation need to be set do it now, otherwise get
	   frequency and attenuation set at the synthesizer and store it */

	if ( hp8647a.freq_is_set )
		hp8647a_set_frequency( hp8647a.freq );
	else
		hp8647a.freq = hp8647a_get_frequency( );

	if ( hp8647a.attenuation_is_set )
		hp8647a_set_attenuation( hp8647a.attenuation );
	else
		hp8647a.attenuation = hp8647a_get_attenuation( );

	hp8647a_set_output_state( hp8647a.state );

	return OK;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static void hp8647a_finished( void )
{
	gpib_local( hp8647a.device );
	if ( hp8647a.att_table != NULL )
	{
		T_free( hp8647a.att_table );
		hp8647a.att_table = NULL;
	}
}


static bool hp8647a_set_output_state( bool state )
{
	char cmd[ 100 ];


	sprintf( cmd, "OUTP:STAT %s", state ? "ON" : "OFF" );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );

	return state;
}


static bool hp8647a_get_output_state( void )
{
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_write( hp8647a.device, "OUTP:STAT?", 10 ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
		hp8647a_comm_failure( );

	return buffer[ 0 ] == '1';
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static double hp8647a_set_frequency( double freq )
{
	char cmd[ 100 ];


	assert( freq >= MIN_FREQ && freq <= MAX_FREQ );

	sprintf( cmd, "FREQ:CW %f", freq );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );

	return freq;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static double hp8647a_get_frequency( void )
{
	char buffer[ 100 ];
	long length = 100;


	if ( gpib_write( hp8647a.device, "FREQ:CW?", 8 ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
		hp8647a_comm_failure( );

	return T_atof( buffer );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static double hp8647a_set_attenuation( double att )
{
	char cmd[ 100 ];


	assert( att >= MAX_ATTEN && att <= MIN_ATTEN );

	sprintf( cmd, "POW:AMPL %6.1f", att );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );

	return att;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static double hp8647a_get_attenuation( void )
{
	char buffer[ 100 ];
	long length = 100;

	if ( gpib_write( hp8647a.device, "POW:AMPL?", 9 ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
		hp8647a_comm_failure( );

	return T_atof( buffer );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static void hp8647a_comm_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.\n", DEVICE_NAME );
	THROW( EXCEPTION );
}
