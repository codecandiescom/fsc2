/*
  $Id$
*/


#define HFS9000_MAIN

#include "hfs9000.h"
#include "gpib_if.h"



/*---------------------------------------------------------------------------
  This function is called directly after all modules are loaded. Its function
  is to initialize all global variables that are needed in the module.
---------------------------------------------------------------------------*/

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
	pulser_struct.set_repeat_time = hfs9000_set_repeat_time;
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

	for ( i = 0; i < MAX_CHANNELS; i++ )
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
		hfs9000.function[ i ].pulses = NULL;
		hfs9000.function[ i ].is_inverted = UNSET;
		hfs9000.function[ i ].delay = 0;
		hfs9000.function[ i ].is_delay = UNSET;
		hfs9000.function[ i ].is_high_level = UNSET;
		hfs9000.function[ i ].is_low_level = UNSET;
	}

	hfs9000_is_needed = SET;

	return 1;
}
