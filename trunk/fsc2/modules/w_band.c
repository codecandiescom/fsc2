/*
  $Id$
*/


#include "fsc2.h"


/* Exported functions */

int w_band_init_hook( void );
int w_band_exp_hook( void );
int w_band_end_of_exp_hook( void );
void w_band_exit_hook( void );



#define DEVICE_NAME "W_BAND"         /* name of device */

#define W_BAND_MAX_CUURENT    10.0
#define V_TO_A_FACTOR	     -97.5	/* Conversion factor of voltage at DAC



typedef struct {
	double curtrent;          /* the start current given by the user */
	double current_step;      /* the current steps to be used */

	bool is_current;          /* flag, set if start current is defined */
	bool is_current_step;     /* flag, set if current step size is defined */
}



/*----------------------------------------------------------------*/
/* Here we check if also a driver for a field meter is loaded and */
/* test if this driver will be loaded before the magnet driver.   */
/*----------------------------------------------------------------*/

int w_band_init_hook( void )
{
	int ret;
	Var *func_ptr;
	int access;


	/* Check if there's a lock-in amplifier with a DAC */

	if ( ! exist_device( "sr510" ) &&
		 ! exist_device( "sr530" ) &&
		 ! exist_device( "sr810" ) &&
		 ! exist_device( "sr830" ) )
	{
		eprint( FATAL, "%s: Can't find a lock-in amplifier with a DAC.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* We need the lock-in driver called *before* the magnet driver since
	   the lock-ins DAC is needed in the initialization of the magnet.
	   Probably we should implement a solution that brings the devices
	   automatically into the correct sequence instead of this hack, but
	   that's not as simple as it might look... */

	if ( ( func_ptr = func_get( "lockin_dac_voltage", &access ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: No lock-in amplifier module loaded "
				"supplying a function for setting a DAC.\n",
				Fname, Lc, func_name );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Set some variables in the magnets structure */

	magnet.is_current = UNSET;
	magnet.is_current_step = UNSET;

	return 1;
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
	/* Check that both variables are reasonable */

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used for magnet current.\n",
				Fname, Lc, DEVICE_NAME );

	vars_check( v->next, INT_VAR | FLOAT_VAR );
	if ( v->next->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used for magnet curtrent "
				"step width.\n", Fname, Lc, DEVICE_NAME );

	/* Check that new field value is still within bounds */

	w_band_current_check( VALUE( v ), NULL );

	if ( VALUE( v->next ) < 1.0 )
	{
		eprint( FATAL, "%s:%ld: %s: Field sweep step size (%lf G) too "
				"small, minimum is %f G.\n", Fname, Lc, DEVICE_NAME,
				VALUE( v->next ), ( double ) AEG_S_BAND_MIN_FIELD_STEP );
		THROW( EXCEPTION );
	}
		
	magnet.field = VALUE( v );
	magnet.field_step = VALUE( v->next );
	magnet.is_field = magnet.is_field_step = SET;

	return vars_push( INT_VAR, 1 );
}



static double w_band_current_check( double current, bool *err_flag )
{

	if ( fabs( current ) > W_BAND_MAX_CURRENT )
	{
		if ( ! TEST_RUN )
		{
			eprint( FATAL, "%s:%ld: %s: Magnet current %f A out of range.\n",
					Fname, Lc, DEVICE_NAME, current );
			*err_flag = SET;
			return current > 0.0 ? 10.0 : -10.0;
		}

		eprint( FATAL, "%s: Magnet current %f A out of range.\n",
				DEVICE_NAME, current );
		THROW( EXCEPTION );
	}

	return current;
}
