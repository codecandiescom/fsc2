/*
  $Id$
*/


#define HFS9000_MAIN

#include "hfs9000.h"
#include "gpib_if.h"


/*----------------------------------------------------------------------
  This function is called directly after all modules are loaded. It
  initializes all global variables that are needed in the module.
----------------------------------------------------------------------*/

int hfs9000_init_hook( void )
{
	int i;


	/* Now test that the name entry in the pulser structure is NULL, otherwise
	   assume, that another pulser driver has already been installed. */

	if ( pulser_struct.name != NULL )
	{
		eprint( FATAL, "%s:%ld: While loading driver HFS9000 found that "
				"driver %s is already installed.\n",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	pulser_struct.name = get_string_copy( "HFS9000" );

	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* We have to set up the global structure for the pulser, especially the
	   pointers for the functions that will get called from pulser.c */

	hfs9000.needs_update = UNSET;
	hfs9000.is_running = UNSET;

	pulser_struct.set_timebase = hfs9000_store_timebase;

	pulser_struct.assign_function = NULL;
	pulser_struct.assign_channel_to_function
		= hfs9000_assign_channel_to_function;
	pulser_struct.invert_function = hfs9000_invert_function;
	pulser_struct.set_function_delay = hfs9000_set_function_delay;
	pulser_struct.set_function_high_level = hfs9000_set_function_high_level;
	pulser_struct.set_function_low_level = hfs9000_set_function_low_level;

	pulser_struct.set_trigger_mode = hfs9000_set_trigger_mode;
	pulser_struct.set_repeat_time = NULL;
	pulser_struct.set_trig_in_level = hfs9000_set_trig_in_level;
	pulser_struct.set_trig_in_slope = hfs9000_set_trig_in_slope;
	pulser_struct.set_trig_in_impedance = NULL;

	pulser_struct.set_phase_reference = NULL;

	pulser_struct.new_pulse = hfs9000_new_pulse;
	pulser_struct.set_pulse_function = hfs9000_set_pulse_function;
	pulser_struct.set_pulse_position = hfs9000_set_pulse_position;
	pulser_struct.set_pulse_length = hfs9000_set_pulse_length;
	pulser_struct.set_pulse_position_change =
		hfs9000_set_pulse_position_change;
	pulser_struct.set_pulse_length_change = hfs9000_set_pulse_length_change;
	pulser_struct.set_pulse_phase_cycle = NULL;
	pulser_struct.set_grace_period = NULL;

	pulser_struct.get_pulse_function = hfs9000_get_pulse_function;
	pulser_struct.get_pulse_position = hfs9000_get_pulse_position;
	pulser_struct.get_pulse_length = hfs9000_get_pulse_length;
	pulser_struct.get_pulse_position_change =
		hfs9000_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = hfs9000_get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = NULL;

	pulser_struct.phase_setup_prep = NULL;
	pulser_struct.phase_setup = NULL;

	pulser_struct.set_phase_switch_delay = NULL;

	/* Finally, we initialize variables that store the state of the pulser */

	hfs9000.is_timebase = UNSET;

	hfs9000.is_trig_in_mode = UNSET;
	hfs9000.is_trig_in_slope = UNSET;
	hfs9000.is_trig_in_level = UNSET;
	hfs9000.is_repeat_time = UNSET;
	hfs9000.is_neg_delay = UNSET;
	hfs9000.neg_delay = 0;

	for ( i = 0; i <= MAX_CHANNELS; i++ )
	{
		hfs9000.channel[ i ].self = i;
		hfs9000.channel[ i ].function = NULL;
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		hfs9000.function[ i ].self = PULSER_CHANNEL_FUNC_MIN + i;
		hfs9000.function[ i ].is_used = UNSET;
		hfs9000.function[ i ].is_needed = UNSET;
		hfs9000.function[ i ].channel = NULL;
		hfs9000.function[ i ].num_pulses = 0;
		hfs9000.function[ i ].num_active_pulses = 0;
		hfs9000.function[ i ].pulses = NULL;
		hfs9000.function[ i ].max_seq_len = 0;
		hfs9000.function[ i ].is_inverted = UNSET;
		hfs9000.function[ i ].delay = 0;
		hfs9000.function[ i ].is_delay = UNSET;
		hfs9000.function[ i ].is_high_level = UNSET;
		hfs9000.function[ i ].is_low_level = UNSET;
	}

	hfs9000_is_needed = SET;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int hfs9000_test_hook( void )
{
	if ( hfs9000_Pulses == NULL )
	{
		hfs9000_is_needed = UNSET;
		eprint( WARN, "%s loaded but no pulses are defined.\n",
				pulser_struct.name );
		return 1;
	}

	/* Check consistency of pulse settings and do everything to setup the
	   pulser for the test run */

	hfs9000_IN_SETUP = SET;
	hfs9000_init_setup( );
	hfs9000_IN_SETUP = UNSET;

	/* We need some somewhat different functions for setting some of the
	   pulser properties */

	pulser_struct.set_pulse_position = hfs9000_change_pulse_position;
	pulser_struct.set_pulse_length = hfs9000_change_pulse_length;
	pulser_struct.set_pulse_position_change =
		hfs9000_change_pulse_position_change;
	pulser_struct.set_pulse_length_change = hfs9000_change_pulse_length_change;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int hfs9000_end_of_test_hook( void )
{
	if ( ! hfs9000_is_needed )
		return 1;

	/* First we have to reset the internal representation back to its initial
	   state */

	hfs9000_full_reset( );

	/* Now we've got to find out about the maximum sequence length and set
	   up padding to achieve the requested repeat time */

	hfs9000.max_seq_len = hfs9000_get_max_seq_len( );
	hfs9000_calc_padding( );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int hfs9000_exp_hook( void )
{
	int i;


	if ( ! hfs9000_is_needed )
		return 1;

	/* Initialize the device */
	
	if ( ! hfs9000_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Failure to initialize the pulser.\n",
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	/* Now we have to tell the pulser about all the pulses */

	for ( i = 0; i <= MAX_CHANNELS; i++ )
		if ( hfs9000.channel[ i ].function->is_used )
			hfs9000_set_pulses( hfs9000.channel[ i ].function );

	hfs9000_run( START );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int hfs9000_end_of_exp_hook( void )
{
	if ( ! hfs9000_is_needed )
		return 1;

	gpib_local( hfs9000.device );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void hfs9000_exit_hook( void )
{
	PULSE *p, *np;
	int i;


	if ( ! hfs9000_is_needed )
		return;

	/* Free all the memory allocated within the module */

	for ( p = hfs9000_Pulses; p != NULL; p = np )
	{
		np = p->next;
		T_free( p );
	}

	hfs9000_Pulses = NULL;

	for ( i = 0; i <= MAX_CHANNELS; i++ )
		if ( hfs9000.channel[ i ].function != NULL &&
			 hfs9000.channel[ i ].function->pulses != NULL )
			T_free( hfs9000.channel[ i ].function->pulses );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_update( Var *v )
{
	v = v;


	if ( ! hfs9000_is_needed )
		return vars_push( INT_VAR, 1 );

	/* Send all changes to the pulser */

	if ( hfs9000.needs_update )
		hfs9000_do_update( );

	/* If we're doing a real experiment also tell the pulser to start */

	if ( ! TEST_RUN )
		hfs9000_run( START );

	return vars_push( INT_VAR, 1 );
}


