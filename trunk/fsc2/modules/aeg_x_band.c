/*
  $Id$
*/

#include "fsc2.h"
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


/* Definitions for serial port access - apply changes here */

#define SERIAL_PORT     0            /* serial port number (i.e. COM1) */
#define SERIAL_BAUDRATE B1200        /* baud rate of field controller */
#define SERIAL_TIME     50000        /* time in us set at timer box front
										panel - currently set to 50 ms and
										probably not to be changed ! */


/* Exported functions */

int aeg_x_band_init_hook( void );
int aeg_x_band_test_hook( void );
int aeg_x_band_exp_hook( void );
int aeg_x_band_end_of_exp_hook( void );
void aeg_x_band_exit_hook( void );

Var *magnet_setup( Var *v );
Var *magnet_fast_init( Var *v );
Var *set_field( Var *v );
Var *sweep_up( Var *v );
Var *sweep_down( Var *v );
Var *reset_field( Var *v );



/* Locally used functions */

static double aeg_x_band_field_check( double field, bool *err_flag );
static bool magnet_init( void );
static bool magnet_goto_field( double field, double error );
static bool magnet_goto_field_rec( double field, double error, int rec,
								   double *hint );
static void magnet_sweep( int dir );
static bool magnet_do( int command );


/* Maximum and minimum field settings (also depending on field meter)
   In principle it would be better if this could be asked from the gaussmeter
   but some of them (at least the ER035M) know about it only after the
   the exp_hook function has been run... */

#define AEG_X_BAND_MIN_FIELD_STEP              1.1e-3
#define AEG_X_BAND_WITH_ER035M_MIN_FIELD       1460
#define AEG_X_BAND_WITH_ER035M_MAX_FIELD       19900
#define AEG_X_BAND_WITH_BH15_MIN_FIELD         -50
#define AEG_X_BAND_WITH_BH15_MAX_FIELD         23000


#define DEVICE_NAME "AEG_X_BAND"     /* name of device */

typedef struct
{
	double field;           /* the start field given by the user */
	double field_step;      /* the field steps to be used */

	bool is_field;          /* flag, set if start field is defined */
	bool is_field_step;     /* flag, set if field step size is defined */

	double mini_step;       /* the smallest possible field step */

	double target_field;    /* used internally */
	double act_field;       /* used internally */

	double meas_field;      /* result of last field measurement */
	double max_deviation;   /* maximum acceptable deviation between measured */
	                        /* and requested field */
	double step;            /* the current step width (in bits) */
	int int_step;           /* used internally */

	bool is_opened;
    int fd;                 /* file descriptor for serial port */
    struct termios old_tio, /* serial port terminal interface structures */
                   new_tio;

	bool fast_init;         /* if set do a fast initialization */
} Magnet;


static Magnet magnet;
static char serial_port[ ] = "/dev/ttyS*";

enum {
	   SERIAL_INIT,
	   SERIAL_TRIGGER,
	   SERIAL_VOLTAGE,
	   SERIAL_EXIT
};



/*****************************************************************************/
/*                                                                           */
/*                  hook functions                                           */
/*                                                                           */
/*****************************************************************************/


/*----------------------------------------------------------------*/
/* Here we check if also a driver for a field meter is loaded and */
/* test if this driver will be loaded before the magnet driver.   */
/*----------------------------------------------------------------*/

int aeg_x_band_init_hook( void )
{
	bool *is_gaussmeter;
	int ret;


	/* Check if there's a field meter */

	if ( ! exist_device( "er035m" ) &&
		 ! exist_device( "er035m_s" ) &&
		 ! exist_device( "bh15" ) )
	{
		eprint( FATAL, "%s: Can't find a gaussmeter - must be listed before "
				"the magnet driver.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* We need the field meter driver called *before* the magnet driver since
	   the field meter is needed in the initialization of the magnet.
	   Probably we should implement a solution that brings the devices
	   automatically into the correct sequence instead of this hack, but
	   that's not as simple as it might look... */

	if ( exist_device( "er035m" ) )
		ret = get_lib_symbol( "er035m", "is_gaussmeter",
							  ( void ** ) &is_gaussmeter );
	else if ( exist_device( "er035m_s" ) )
		ret = get_lib_symbol( "er035m_s", "is_gaussmeter",
							  ( void ** ) &is_gaussmeter );
	else
		ret = get_lib_symbol( "bh15", "is_gaussmeter",
							  ( void ** ) &is_gaussmeter );

	assert( ret != LIB_ERR_NO_LIB );      /* this can't happen....*/

	if ( ret == LIB_ERR_NO_SYM )
	{
		eprint( FATAL, "fsc2: INTERNAL ERROR detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	if ( ! *is_gaussmeter )
	{
		eprint( FATAL, "%s: Problem in DEVICES section: driver for gaussmeter "
				"must be listed before magnet driver.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Check that the functions exported by the field meter driver(s) can be
	   accessed (so we don't have to do it later again and again) */

	if ( ! exist_function( "find_field" ) )
	{
		eprint( FATAL, "%s: No function available to do field measurements.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( ! exist_function( "field_meter_wait" ) )
	{
		eprint( FATAL, "%s: Function needed for field measurements not "
				"available.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( ! exist_function( "field_resolution" ) )
	{
		eprint( FATAL, "%s: Function to determine field measurement "
				"resolution is missing.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Claim the serial port */

	if ( SERIAL_PORT >= NUM_SERIAL_PORTS || SERIAL_PORT < 0 )
	{
		eprint( FATAL, "%s: Serial port number %d out of valid range "
				"(0-%d).\n", DEVICE_NAME, SERIAL_PORT, NUM_SERIAL_PORTS - 1 );
		THROW( EXCEPTION );
	}

	if ( need_Serial_Port[ SERIAL_PORT ] )
	{
		eprint( FATAL, "%s: Serial port %d (i.e. /dev/ttyS%d or COM%d) is "
				"already in use by another device.\n", DEVICE_NAME,
				SERIAL_PORT, SERIAL_PORT, SERIAL_PORT + 1 );
		THROW( EXCEPTION );
	}

	need_Serial_Port[ SERIAL_PORT ] = SET;
	*strrchr( serial_port, '*' ) = ( char ) ( SERIAL_PORT + '0' );

	magnet.is_field = UNSET;
	magnet.is_field_step = UNSET;
	magnet.is_opened = UNSET;
	magnet.fast_init = UNSET;

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int aeg_x_band_test_hook( void )
{
	return 1;
}


/*---------------------------------------------------------------------*/
/* Opens connection to the power supply and calibrates the field sweep */
/*---------------------------------------------------------------------*/

int aeg_x_band_exp_hook( void )
{
	Var *v;
	int acc;


	/* Get the maximum deviation from requested field depending on the
	   resolution of the field meter */

	v = func_call( func_get( "field_resolution", &acc ) );
	magnet.max_deviation = VALUE( v );
	vars_pop( v );

	/* Try to initialize the magnet power supply controller */

	if ( ! magnet_init( ) )
	{
		eprint( FATAL, "%s: Can't access the X-band magnet power supply.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	magnet.is_opened = SET;

	/* When same EDL file is run again always use fast initialization mode */
	/* Is this really a good idea ????*/

	magnet.fast_init = SET;

	return 1;
}


/*---------------------------------------------------------------*/
/* Closes the connection to the power supply after an experiment */
/*---------------------------------------------------------------*/

int aeg_x_band_end_of_exp_hook( void )
{
	/* reset the serial port */

	if ( magnet.is_opened )
		magnet_do( SERIAL_EXIT );

	magnet.is_opened = UNSET;

	return 1;
}


/*--------------------------------------------------------------------*/
/* Just make sure the connection to the power supply is really closed */
/*--------------------------------------------------------------------*/

void aeg_x_band_exit_hook( void )
{
	aeg_x_band_end_of_exp_hook( );
}


/*****************************************************************************/
/*                                                                           */
/*              exported functions, i.e. EDL functions                       */
/*                                                                           */
/*****************************************************************************/


/*-------------------------------------------------------------------*/
/* Function for registering the start field and the field step size. */
/*-------------------------------------------------------------------*/

Var *magnet_setup( Var *v )
{
	bool err_flag = UNSET;


	/* check that both variables are reasonable */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing parameter in call of function "
				"`magnet_setup'.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used for magnetic field.\n",
				Fname, Lc, DEVICE_NAME );

	if ( v->next == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing field step size in call of "
				"`magnet_setup'.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v->next, INT_VAR | FLOAT_VAR );
	if ( v->next->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used for field step width.\n",
				Fname, Lc, DEVICE_NAME );

	/* Check that new field value is still within bounds */

	aeg_x_band_field_check( VALUE( v ), &err_flag );
	if ( err_flag )
		THROW( EXCEPTION );

	if ( VALUE( v->next ) < AEG_X_BAND_MIN_FIELD_STEP )
	{
		eprint( FATAL, "%s:%ld: %s: Field sweep step size (%lf G) too small, "
				"minimum is %f G.\n", Fname, Lc, DEVICE_NAME, VALUE( v->next ),
				( double ) AEG_X_BAND_MIN_FIELD_STEP );
		THROW( EXCEPTION );
	}
		
	magnet.field = VALUE( v );
	magnet.field_step = VALUE( v->next );
	magnet.is_field = magnet.is_field_step = SET;

	while ( ( v = vars_pop( v ) ) )
		;

	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------------------*/
/* If called the function reduces the time needed for calibrating the */
/* magnet sweep but unfortunately also reducing the accuracy.         */
/*--------------------------------------------------------------------*/

Var *magnet_fast_init( Var *v )
{
	while ( ( v = vars_pop( v ) ) )
		;

	magnet.fast_init = SET;
	return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------*/
/*-----------------------------------------------------*/

Var *set_field( Var *v )
{
	double field;
	bool err_flag = UNSET;
	double error = 0.0;


	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing parameter in function "
				"`set_field'.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used for magnetic field.\n",
				Fname, Lc, DEVICE_NAME );

	/* Check the new field value and reduce value if necessary */

	field = aeg_x_band_field_check( VALUE( v ), &err_flag );

	/* The second argument can be the maximum error which should be less than 
	   10% of the absolut requested field value */

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check( v, INT_VAR | FLOAT_VAR );
		if ( v->type == INT_VAR )
			eprint( WARN, "%s:%ld: %s: Integer value used for magnetic field "
					"precision.\n", Fname, Lc, DEVICE_NAME );
		error = fabs( VALUE( v ) );

		if ( error > 0.1 * fabs( field ) )
			eprint( SEVERE, "%s:%ld: %s: Field precision is larger than 10% "
					"of field value.\n", Fname, Lc, DEVICE_NAME );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous parameter in call of "
				"function `set_field'.\n", Fname, Lc, DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, field );

	if ( ! magnet_goto_field( field, error ) )
	{
		eprint( FATAL, "%s: Can't reach requested field of %lf G.\n",
				DEVICE_NAME, field );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, magnet.act_field );
}


/*-----------------------------------------------------*/
/*-----------------------------------------------------*/

Var *sweep_up( Var *v )
{
	bool err_flag = UNSET;


	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous parameter in call of "
				"`sweep_up'.\n", Fname, Lc, DEVICE_NAME );

		while ( ( v = vars_pop( v ) ) )
			;
	}

	if ( ! magnet.is_field_step )
	{
		eprint( FATAL, "%s:%ld: %s: Sweep step size has not been defined.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Check that new field value is still within bounds */

	aeg_x_band_field_check( magnet.act_field + magnet.field_step, &err_flag );
	if ( err_flag )
		return vars_push( FLOAT_VAR, magnet.act_field );

	if ( TEST_RUN )
	{
		magnet.target_field += magnet.field_step;
		return vars_push( FLOAT_VAR, magnet.target_field );
	}

	magnet_sweep( 1 );
	return vars_push( FLOAT_VAR, magnet.act_field );
}


/*-----------------------------------------------------*/
/*-----------------------------------------------------*/

Var *sweep_down( Var *v )
{
	bool err_flag = UNSET;


	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous parameter in call of "
				"`sweep_down'.\n", Fname, Lc, DEVICE_NAME );

		while ( ( v = vars_pop( v ) ) )
			;
	}

	if ( ! magnet.is_field_step )
	{
		eprint( FATAL, "%s:%ld: %s: Sweep step size has not been defined.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Check that new field value is still within bounds */

	aeg_x_band_field_check( magnet.act_field - magnet.field_step, &err_flag );
	if ( err_flag )
		return vars_push( FLOAT_VAR, magnet.act_field );

	if ( TEST_RUN )
	{
		magnet.target_field -= magnet.field_step;
		return vars_push( FLOAT_VAR, magnet.target_field );
	}

	magnet_sweep( -1 );
	return vars_push( FLOAT_VAR, magnet.act_field );
}


/*-----------------------------------------------------*/
/*-----------------------------------------------------*/

Var *reset_field( Var *v )
{
	while ( ( v = vars_pop( v ) ) )
		;

	if ( ! magnet.is_field )
	{
		eprint( FATAL, "%s:%ld: %s: Start field has not been defined.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
	{
		magnet.target_field = magnet.field;
		return vars_push( FLOAT_VAR, magnet.target_field );
	}

	magnet_goto_field( magnet.field, 0.0 );
	return vars_push( FLOAT_VAR, magnet.act_field );
}


/*****************************************************************************/
/*                                                                           */
/*            internally used functions                                      */
/*                                                                           */
/*****************************************************************************/


static double aeg_x_band_field_check( double field, bool *err_flag )
{
	if ( exist_device( "er035m" ) )
	{
		if ( field < AEG_X_BAND_WITH_ER035M_MIN_FIELD )
		{
			eprint( FATAL, "%s:%ld: %s: Start field (%lf G) too low for "
					"Bruker ER035M gaussmeter, minimum is %d G.\n",
					Fname, Lc, DEVICE_NAME, field,
					( int ) AEG_X_BAND_WITH_ER035M_MIN_FIELD );
			if ( ! TEST_RUN )
			{
				*err_flag = SET;
				return AEG_X_BAND_WITH_ER035M_MIN_FIELD;
			}
			else
				THROW( EXCEPTION );
		}
        
		if ( field > AEG_X_BAND_WITH_ER035M_MAX_FIELD )
		{
			eprint( FATAL, "%s:%ld: %s: Start field (%lf G) too high for "
					"Bruker ER035M gaussmeter, maximum is %d G.\n",
					Fname, Lc, DEVICE_NAME, field,
					( int ) AEG_X_BAND_WITH_ER035M_MAX_FIELD );
			if ( ! TEST_RUN )
			{
				*err_flag = SET;
				return AEG_X_BAND_WITH_ER035M_MAX_FIELD;
			}
			else
				THROW( EXCEPTION );
		}
	}

	if ( exist_device( "bh15" ) )
	{
		if ( field < AEG_X_BAND_WITH_BH15_MIN_FIELD )
		{
			eprint( FATAL, "%s:%ld: %s: Start field (%lf G) too low for "
					"Bruker BH15 field controller, minimum is %d G.\n",
					Fname, Lc, DEVICE_NAME, field,
					( int ) AEG_X_BAND_WITH_BH15_MIN_FIELD );
			if ( ! TEST_RUN )
			{
				*err_flag = SET;
				return AEG_X_BAND_WITH_BH15_MIN_FIELD;
			}
			else
				THROW( EXCEPTION );
		}
        
		if ( field > AEG_X_BAND_WITH_BH15_MAX_FIELD )
		{
			eprint( FATAL, "%s:%ld: %s: Start field (%lf G) too high for "
					"Bruker BH15 field controller, maximum is %d G.\n",
					Fname, Lc, DEVICE_NAME, field,
					( int ) AEG_X_BAND_WITH_BH15_MAX_FIELD );
			if ( ! TEST_RUN )
			{
				*err_flag = SET;
				return AEG_X_BAND_WITH_BH15_MAX_FIELD;
			}
			else
				THROW( EXCEPTION );
		}
	}

	return field;
}


#define sign( x ) ( ( ( x ) >= 0.0 ) ? 1.0 : -1.0 )

/* MAGNET_ZERO_STEP is the data to be send to the magnet to achieve a sweep
   speed of exactly zero - which is 0x800 minus `half a bit' :-)
   MAGNET_ZERO_STEP and MAGNET_MAX_STEP must always add up to something not
   more than 0xFFF and MAGNET_ZERO_STEP minus MAGNET_MAX_STEP must always be
   zero or larger ! */

#define MAGNET_ZERO_STEP  ( 0x800 - 0.5 )  /* data for zero sweep speed */
#define MAGNET_MAX_STEP   ( 0x7FF + 0.5 )  /* maximum sweep speed setting */


#define MAGNET_TEST_STEPS      16     /* number of steps to do in test */
#define MAGNET_FAST_TEST_STEPS 4      /* number of steps to do in fast test */
#define MAGNET_TEST_WIDTH      0x400  /* sweep speed setting for test */
#define MAGNET_MAX_TRIES       3      /* number of retries after failure of 
										 magnet field convergence to target
										 point */



/* The sweep of the magnet is done by applying a voltage for a certain time
   (to be adjusted manually on the front panel but which should be left at
   the current setting of 50 ms) to the internal sweep circuit.  There are
   two types of data to be sent to the home build interface card: First, the
   voltage to be set and, second, a trigger to really set the voltage.

   1. The DAC is a 12-bit converter, thus two bytes have to be sent to the
      interface card. The first byte must have bit 6 set to tell the inter-
	  face card that this is the first byte. In bit 0 to 3 of the first byte
	  sent the upper 4 bits of the 12-bit data are encoded, while bit 4
	  contains the next lower bit (bit 7) of the 12-data word to be sent.
	  In the second byte sent to the interface card bit 7 has to be set to
	  tell the card that this is the second byte, followed by the remaining
	  lower 7 bits of the data. Thus in order to send the 12-bit data

                       xxxx yzzz zzzz

	  the 2-byte sequence to be sent to the interface card is (binary)

                  1.   010y xxxx
				  2.   1zzz zzzz

  2. To trigger the output of the voltage bit 5 of the byte sent to the
     interface card has to be set, while bit 6 and 7 have to be zeros - all
	 remaining bits don't matter. So, the (binary) trigger command byte is

                       0010 0000

  3. The maximum positive step width is achieved by setting the DAC to
     0x0, the maximum negative step width by setting it to 0xFFF.

  The sweep according to the data send to the magnet power supply will
  last for the time adjusted on the controllers front panel (in ms).
  This time is currently set to 50 ms and shouldn't be changed without
  good reasons.

*/


/*--------------------------------------------------------------------------*/
/* magnet_init() first initializes the serial interface and then tries to   */
/* figure out what's the current minimal step size is for the magnet - this */
/* is necessary for every new sweep since the user can adjust the step size */
/* by setting the sweep rate on the magnets front panel (s/he also might    */
/* change the time steps but lets hope he doesn't since there's no way to   */
/* find out about it...). So, we have to make sure that the setting at the  */
/* front panel is the maximum setting of 5000 Oe/min. Finally we try to go  */
/* to the start field.                                                      */
/*--------------------------------------------------------------------------*/

static bool magnet_init( void )
{
	double start_field;
	int i;
	Var *v;
	int acc;
	int test_steps;


	/* First step: Initialization of the serial interface */

	if ( ! magnet_do( SERIAL_INIT ) )
		return FAIL;

	/* Next step: We do MAGNET_FAST_TEST_STEPS or MAGNET_TEST_STEPS steps
	   with a step width of MAGNET_TEST_WIDTH. Then we measure the field to
	   determine the field change per bit ratio `nmr.mini_step'. */

	if ( magnet.fast_init )
		test_steps = MAGNET_FAST_TEST_STEPS;
	else
		test_steps = MAGNET_TEST_STEPS;

try_again:

	vars_pop( func_call( func_get( "field_meter_wait", &acc ) ) );

	v = func_call( func_get( "find_field", &acc ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	start_field = magnet.meas_field;
	magnet.step = MAGNET_TEST_WIDTH;
	magnet_do( SERIAL_VOLTAGE );

	for ( i = 0; i < test_steps; ++i )
	{
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );

		magnet_do( SERIAL_TRIGGER );
		vars_pop( func_call( func_get( "field_meter_wait", &acc ) ) );
	}

	vars_pop( func_call( func_get( "field_meter_wait", &acc ) ) );

	v = func_call( func_get( "find_field", &acc ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	/* Calculate the smallest possible step width (in field units) */

	magnet.mini_step = ( magnet.meas_field - start_field ) /
		                         ( double ) ( MAGNET_TEST_WIDTH * test_steps );

	/* Now lets do the same, just in the opposite direction to (hopefully)
	   increase accuracy */

	start_field = magnet.meas_field;
	magnet.step = - MAGNET_TEST_WIDTH;
	magnet_do( SERIAL_VOLTAGE );

	for ( i = 0; i < test_steps; ++i )
	{
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );

		magnet_do( SERIAL_TRIGGER );
		vars_pop( func_call( func_get( "field_meter_wait", &acc ) ) );
	}

	vars_pop( func_call( func_get( "field_meter_wait", &acc ) ) );

	v = func_call( func_get( "find_field", &acc ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	magnet.mini_step -= ( magnet.meas_field - start_field ) /
		                         ( double ) ( MAGNET_TEST_WIDTH * test_steps );
	magnet.mini_step *= 0.5;

	/* Check that the sweep speed selector on the magnets front panel is set
	   to the maximum, i.e. 5000 Oe/min - otherwise ask user to change the
	   setting and try again */

	if ( fabs( magnet.mini_step ) < 0.012 * SERIAL_TIME / 1.0e6 )
	{
		if ( 1 != show_choices( "Please set sweep speed on magnet front\n"
								"panel to maximum value of 5000 Oe/min.\n"
								"Also make sure remote control is enabled!",
								2, "Abort", "Done", "", 3 ) )
			goto try_again;
		return FAIL;
	}

	/* Finally using this ratio we go to the start field */

	if ( magnet.is_field )
		return magnet_goto_field( magnet.field, 0.0 );
	else
		return OK;

}


/*-------------------------------------------------------------------------*/
/* On the one hand this function is a wrapper to hide the recursiveness of */
/* magnet_goto_field_rec(), on the other hand there's something more: The  */
/* function stores the size of the last field step as well as the number   */
/* mini_steps that were needed to achieve the field step. When the next    */
/* field step has the same size (within the error specified by the user)   */
/* this number of mini_steps is used instead of the one that would result  */
/* when using the factor determined in the initialization. This way the    */
/* errors for large field steps (where the factor doesn't work well) can   */
/* be compensated and the sweep can be done faster.                        */
/*-------------------------------------------------------------------------*/

static bool magnet_goto_field( double field, double error )
{
	static double last_field_step = -1.0;
	static double last_mini_steps = 0.0;


	if ( last_mini_steps != 0.0 &&
		 fabs( field - magnet.act_field ) <= fabs( last_field_step ) + error &&
		 fabs( field - magnet.act_field ) >= fabs( last_field_step ) - error )
	{
		if ( last_field_step != 0.0 )
			 last_mini_steps *= fabs( field - magnet.act_field ) /
			                    fabs( last_field_step );
		if ( sign( field - magnet.act_field ) != sign( last_field_step ) )
			last_mini_steps *= -1.0;
	}
	else
		last_mini_steps = 0.0;

	if ( ! magnet_goto_field_rec( field, error, 0, &last_mini_steps ) )
		return FAIL;

	last_field_step = magnet.meas_field - magnet.act_field;
	magnet.act_field = magnet.target_field = magnet.meas_field;
	
	return OK;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static bool magnet_goto_field_rec( double field, double error, int rec,
								   double *hint )
{
	double mini_steps;
	int steps;
	double remainder;
	double max_dev;
	int i;
	static double last_diff;  /* field difference in last step */
	static int try;           /* number of steps without conversion */
	Var *v;
	int acc;


	/* Determine the field between the target field and the current field */

	v = func_call( func_get( "find_field", &acc ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	/* Calculate the number of steps we need using the smallest possible
	   field increment (i.e. 1 bit) */

	if ( rec == 0 && *hint != 0.0 )
	{
		mini_steps = *hint;
		*hint = 0;
	}
	else
		mini_steps = ( field - magnet.meas_field ) / magnet.mini_step;

	*hint += mini_steps;

	/* How many steps do we need using the maximum step size and how many
	   steps with the minimum step size remain ? */

	steps = ( int ) floor( fabs( mini_steps ) / MAGNET_MAX_STEP );
	remainder = mini_steps - sign( mini_steps ) * steps * MAGNET_MAX_STEP;

	/* Now do all the steps to reach the target field */

	if ( steps > 0 )
	{
		magnet.step = sign( mini_steps ) * MAGNET_MAX_STEP;
		magnet_do( SERIAL_VOLTAGE );

		for ( i = 0; i < steps; ++i )
		{
			if ( DO_STOP )
				THROW( USER_BREAK_EXCEPTION );

			magnet_do( SERIAL_TRIGGER );
		}
	}

	if ( DO_STOP )
		THROW( USER_BREAK_EXCEPTION );

	if ( ( magnet.step = remainder ) != 0.0 )
	{
		magnet_do( SERIAL_VOLTAGE );
		magnet_do( SERIAL_TRIGGER );
	}

	/* Finally we check if the field value is reached. If it isn't we do
	   further corrections by calling the routine itself. To avoid running
	   in an endless recursion with the difference between the actual field
	   and the target field becoming larger and larger we allow it to become
	   larger only MAGNET_MAX_TRIES times - after that we have to assume
	   that something has gone wrong (maybe some luser switched the magnet
	   from remote to local? ) */

	vars_pop( func_call( func_get( "field_meter_wait", &acc ) ) );

	v = func_call( func_get( "find_field", &acc ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	if ( rec == 0 )               /* at the very first call of the routine */
	{
		last_diff = HUGE_VAL;
		try = 0;
	}
	else                          /* if we're already in the recursion */
	{                                                       
		if ( fabs( field - magnet.meas_field ) > last_diff )/* got it worse? */
		{
			if ( ++try >= MAGNET_MAX_TRIES )
				return FAIL;
		}
		else                                /* difference got smaller */
		{
			try = 0;
		    last_diff = fabs( field - magnet.meas_field );
		}
	}

	/* Deviation from target field has to be no more than `max_deviation'
	   otherwise try another (hopefully smaller) step */

	max_dev = magnet.max_deviation > fabs( error ) ?
		      magnet.max_deviation : error;

	if ( fabs( field - magnet.meas_field ) > max_dev &&
		 ! magnet_goto_field_rec( field, error, 1, hint ) )
		return FAIL;

	return OK;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void magnet_sweep( int dir )
{
	int steps, i;
	double mini_steps;
	double remainder;
	double over_shot;
	

	/* <PARANOIA> check for unreasonable input </PARANOIA> */

	assert( dir == 1 || dir == -1 );

	/* Problem: while there can be arbitrary sweep steps requested by the
	   user, the real sweep steps are discreet. This might sum up to a
	   noticeable error after several sweep steps. So, to increase accuracy,
	   at each sweep step the field that should be reached is calculated
	   (magnet.target_field) as well as the field actually reached by the
	   current sweep step (magnet.act_field). The difference between these
	   values is used as a correction in the next sweep step (and should
	   never be larger than one bit). */

	over_shot = magnet.act_field - magnet.target_field;
   	mini_steps = ( dir * magnet.field_step - over_shot ) / magnet.mini_step;

	/* If the target field can be achieved by a single step... */

	if ( fabs( mini_steps ) <= MAGNET_MAX_STEP )
	{
		if ( magnet.step != mini_steps )
		{
			magnet.step = mini_steps;
			magnet_do( SERIAL_VOLTAGE );
		}
		magnet_do( SERIAL_TRIGGER );

		magnet.act_field +=   ( MAGNET_ZERO_STEP - magnet.int_step )
			                * magnet.mini_step;
		magnet.target_field += dir * magnet.field_step;
		return;
	}

	/* ...otherwise we need several steps with MAGNET_MAX_STEP chunks plus a
	   last step for the remainder.
	   First calculate how many steps we need we using the maximum step size
	   and how large a step for the rest. */

	steps = ( int ) floor( fabs( mini_steps ) / MAGNET_MAX_STEP );
	remainder = mini_steps - sign( mini_steps ) * steps * MAGNET_MAX_STEP;

	/* Now do all the steps to reach the target field */

	if ( steps > 0 )
	{
		magnet.step = sign( mini_steps ) * MAGNET_MAX_STEP;
		magnet_do( SERIAL_VOLTAGE );

		for ( i = 0; i < steps; ++i )
		{
			if ( DO_STOP )
				THROW( USER_BREAK_EXCEPTION );

			magnet_do( SERIAL_TRIGGER );
			magnet.act_field +=   ( MAGNET_ZERO_STEP - magnet.int_step )
				                * magnet.mini_step;
		}
	}

	if ( DO_STOP )
		THROW( USER_BREAK_EXCEPTION );

	if ( ( magnet.step = remainder ) != 0.0 )
	{
		magnet_do( SERIAL_VOLTAGE );
		magnet_do( SERIAL_TRIGGER );
	}

	magnet.act_field +=   ( MAGNET_ZERO_STEP - magnet.int_step )
		                * magnet.mini_step;
	magnet.target_field += dir * magnet.field_step;
}


/*---------------------------------------------------------------------------*/
/* This is the most basic routine for controlling the field - there are four */
/* basic commands, i.e. initializing the serial interface, setting a sweep   */
/* voltage, triggering a field sweep and finally resetting the serial inter- */
/* face.                                                                     */
/*---------------------------------------------------------------------------*/

bool magnet_do( int command )
{
	unsigned char data[ 2 ];
	int volt;


	switch ( command )
	{
		case SERIAL_INIT :               /* open and initialize serial port */
			if ( ( magnet.fd =
				  open( serial_port, O_WRONLY | O_NOCTTY | O_NONBLOCK ) ) < 0 )
				return FAIL;

			tcgetattr( magnet.fd, &magnet.old_tio );
			memcpy( &magnet.new_tio, &magnet.old_tio,
					sizeof( struct termios ) );

			/* Switch off parity checking and use of 2 stop bits and clear
			   character size mask, then set character size mask to CS8,
			   allow flow control and finally set the baud rate */

			magnet.new_tio.c_cflag &= ~ ( PARENB | CSTOPB | CSIZE );
			magnet.new_tio.c_cflag |= CS8 | CRTSCTS;
			cfsetospeed( &magnet.new_tio, SERIAL_BAUDRATE );

			tcflush( magnet.fd, TCIFLUSH );
			tcsetattr( magnet.fd, TCSANOW, &magnet.new_tio );
			break;

		case SERIAL_TRIGGER :                 /* send trigger pattern */
			data[ 0 ] = 0x20;
			write( magnet.fd, ( void * ) &data, 1 );
			usleep( SERIAL_TIME );
			break;

		case SERIAL_VOLTAGE :                 /* send voltage data pattern */
			magnet.int_step = volt = lround( MAGNET_ZERO_STEP - magnet.step );
		    data[ 0 ] = ( unsigned char ) 
				( 0x40 | ( ( volt >> 8 ) & 0xF ) | ( ( volt >> 3 ) & 0x10 ) );
			data[ 1 ] = ( unsigned char ) ( 0x80 | ( volt & 0x07F ) );
			write( magnet.fd, &data, 2 );
			break;

		case SERIAL_EXIT :                    /* reset and close serial port */
			tcflush( magnet.fd, TCIFLUSH );
			tcsetattr( magnet.fd, TCSANOW, &magnet.old_tio );
			close( magnet.fd );
			break;

		default :
			eprint( FATAL, "%s: INTERNAL ERROR detected at %s:%d.\n",
					DEVICE_NAME, __FILE__, __LINE__ );
			THROW( EXCEPTION );
	}

	return OK;
}
