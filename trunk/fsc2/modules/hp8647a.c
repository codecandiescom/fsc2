/*
  $Id$
*/


#include "fsc2.h"
#include "gpib.h"


#define DEVICE_NAME "HP8647A"      /* compare entry in /etc/gpib.conf ! */

#define MIN_FREQ  2.5e5            /* 250 kHz  */
#define MAX_FREQ  1.0e9            /* 1000 MHz */
#define MIN_ATTEN 10.0             /* +10 db   */
#define MAX_ATTEN -136.0           /* -136 db  */


/* declaration of exported functions */

int hp8647a_init_hook( void );
int hp8647a_test_hook( void );
int hp8647a_end_of_test_hook( void );
int hp8647a_exp_hook( void );
int hp8647a_end_of_exp_hook( void );
void hp8647a_exit_hook( void );


Var *synthesizer_set_frequency( Var *v );
Var *synthesizer_set_step_frequency( Var *v );
Var *synthesizer_get_frequency( Var *v );
Var *synthesizer_sweep_up( Var *v );
Var *synthesizer_sweep_down( Var *v );
Var *synthesizer_reset_frequency( Var *v );



typedef struct
{
	int device;

	double start_freq;
	bool SF;               /* start frequency needs setting ? */
	double freq_step;
	bool FS;               /* frequency steps needs setting ? */

	double freq;
} HP8647A;


static HP8647A hp8647a;

static bool hp8647a_init( const char *name );
static double hp8647a_set_frequency( double freq );
static double hp8647a_get_frequency( void );


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
	hp8647a.freq = -1.0;         /* mark as unset yet */
	hp8647a.SF = UNSET;
	hp8647a.freq_step = 0.0;
	hp8647a.FS = SET;

	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int hp8647a_test_hook( void )
{
	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int hp8647a_end_of_test_hook( void )
{
	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int hp8647a_exp_hook( void )
{
	if ( ! hp8647a_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Initialization of device failed.", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int hp8647a_end_of_exp_hook( void )
{
//	hp8647a_finished( );

	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void hp8647a_exit_hook( void )
{
}




Var *synthesizer_set_frequency( Var *v )
{
	double freq;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer variable used as frequency in call "
				"of function `synthesizer_set_frequency'.", Fname, Lc,
				DEVICE_NAME );

	freq = VALUE( v );

	if ( freq < 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid negative frequency in call of "
				"function `synthesizer_set_frequency'.",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( freq < MIN_FREQ || freq > MAX_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: Frequency (%f Hz) not within valid range "
				"of %f kHz to %g Mhz.", Fname, Lc, DEVICE_NAME,
				1.0e-3 * MIN_FREQ, 1.0e-6 * MAX_FREQ );
		THROW( EXCEPTION );
	}

	
	hp8647a.freq = hp8647a_set_frequency( freq );
	return vars_push( FLOAT_VAR, freq );
}


Var *synthesizer_get_frequency( Var *v )
{
	v = v;
	return vars_push( FLOAT_VAR, hp8647a_get_frequency( ) );
}



Var *synthesizer_set_step_frequency( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer variable used as step frequency in "
				"call of function `synthesizer_set_step_frequency'.", Fname,
				Lc, DEVICE_NAME );

	hp8647a.freq_step = VALUE( v );
	return vars_push( FLOAT_VAR, hp8647a.freq_step );
}


Var *synthesizer_sweep_up( Var *v )
{
	double new_freq = hp8647a.freq + hp8647a.freq_step;


	v = v;
	if ( new_freq < MIN_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: Frequency is getting below lower limit of "
				"%f kHz in function `synthesizer_sweep_up'.", Fname, Lc,
				DEVICE_NAME, 1.0e-3 * MIN_FREQ );
		THROW( EXCEPTION );
	}

	if ( new_freq > MAX_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: Frequency is getting above upper limit of "
				"%f MHz in function `synthesizer_sweep_up'.", Fname, Lc,
				DEVICE_NAME, 1.0e-6 * MAX_FREQ );
		THROW( EXCEPTION );
	}

	hp8647a.freq = hp8647a_set_frequency( new_freq );
	return vars_push( FLOAT_VAR, hp8647a.freq );
}



Var *synthesizer_sweep_down( Var *v )
{
	double new_freq = hp8647a.freq - hp8647a.freq_step;


	v = v;
	if ( new_freq < MIN_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: Frequency is getting below lower limit of "
				"%f kHz in function `synthesizer_sweep_up'.", Fname, Lc,
				DEVICE_NAME, 1.0e-3 * MIN_FREQ );
		THROW( EXCEPTION );
	}

	if ( new_freq > MAX_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: Frequency is getting above upper limit of "
				"%f MHz in function `synthesizer_sweep_up'.", Fname, Lc,
				DEVICE_NAME, 1.0e-6 * MAX_FREQ );
		THROW( EXCEPTION );
	}

	hp8647a.freq = hp8647a_set_frequency( new_freq );
	return vars_push( FLOAT_VAR, hp8647a.freq );
}


Var *synthesizer_reset_frequency( Var *v )
{
	v = v;
	hp8647a.freq = hp8647a_set_frequency( hp8647a.start_freq );
	return vars_push( FLOAT_VAR, hp8647a.freq );
}


static bool hp8647a_init( const char *name )
{
	assert( hp8647a.device < 0 );

	if ( gpib_init_device( name, &hp8647a.device ) == FAILURE )
        return FAIL;

	/* If the start frequency needs to be set do it now, otherwise get the
	   frequency set at the synthesier and store it */

	if ( hp8647a.SF )
		hp8647a.freq = hp8647a_set_frequency( hp8647a.start_freq );
	else
		hp8647a.start_freq = hp8647a.freq = hp8647a_get_frequency( );

	return OK;
}


static double hp8647a_set_frequency( double freq )
{
	char cmd[ 100 ] = "FREQ:CW ";


	assert( freq >= MIN_FREQ && freq <= MAX_FREQ );

	sprintf( cmd, "%f", freq );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the synthesizer.",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return freq;
}


static double hp8647a_get_frequency( void )
{
	char buffer[ 100 ];
	long length = 100;

	if ( gpib_write( hp8647a.device, "FREQ:CW?", 8 ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the synthesizer.",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}


	return T_atof( buffer );
}
