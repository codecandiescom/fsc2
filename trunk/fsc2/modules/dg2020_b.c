/*
  $Id$
*/


#define DG2020_B_MAIN


#include "dg2020_b.h"
#include "gpib.h"



/*---------------------------------------------------------------------------
  This function is called directly after all modules are loaded. Its main
  function is to initialize all global variables that are needed in the
  module.
---------------------------------------------------------------------------*/

int dg2020_b_init_hook( void )
{
	int i, j;


	/* Now test that the name entry in the pulser structure is NULL, otherwise
	   assume, that another pulser driver has already been installed. */

	if ( pulser_struct.name != NULL )
	{
		eprint( FATAL, "%s:%ld: While loading driver DG2020_B found that "
				"driver %s is already installed.",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	pulser_struct.name = get_string_copy( "DG2020_B" );

	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* We have to set up the global structure for the pulser, especially the
	   pointers for the functions that will get called from pulser.c */

	dg2020.needs_update = UNSET;
	dg2020.is_running = UNSET;

	pulser_struct.set_timebase = dg2020_store_timebase;

	pulser_struct.assign_function = dg2020_assign_function;
	pulser_struct.assign_channel_to_function =
		dg2020_assign_channel_to_function;
	pulser_struct.invert_function = dg2020_invert_function;
	pulser_struct.set_function_delay = dg2020_set_function_delay;
	pulser_struct.set_function_high_level = dg2020_set_function_high_level;
	pulser_struct.set_function_low_level = dg2020_set_function_low_level;

	pulser_struct.set_trigger_mode = dg2020_set_trigger_mode;
	pulser_struct.set_repeat_time = dg2020_set_repeat_time;
	pulser_struct.set_trig_in_level = dg2020_set_trig_in_level;
	pulser_struct.set_trig_in_slope = dg2020_set_trig_in_slope;
	pulser_struct.set_trig_in_impedance = dg2020_set_trig_in_impedance;

	pulser_struct.set_phase_reference = dg2020_set_phase_reference;

	pulser_struct.new_pulse = dg2020_new_pulse;
	pulser_struct.set_pulse_function = dg2020_set_pulse_function;
	pulser_struct.set_pulse_position = dg2020_set_pulse_position;
	pulser_struct.set_pulse_length = dg2020_set_pulse_length;
	pulser_struct.set_pulse_position_change = dg2020_set_pulse_position_change;
	pulser_struct.set_pulse_length_change = dg2020_set_pulse_length_change;
	pulser_struct.set_pulse_phase_cycle = dg2020_set_pulse_phase_cycle;
	pulser_struct.set_grace_period = NULL;

	pulser_struct.get_pulse_function = dg2020_get_pulse_function;
	pulser_struct.get_pulse_position = dg2020_get_pulse_position;
	pulser_struct.get_pulse_length = dg2020_get_pulse_length;
	pulser_struct.get_pulse_position_change = dg2020_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = dg2020_get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = dg2020_get_pulse_phase_cycle;

	pulser_struct.phase_setup_prep = dg2020_phase_setup_prep;
	pulser_struct.phase_setup = dg2020_phase_setup;

	pulser_struct.set_phase_switch_delay = NULL;

	/* Finally, we initialize variables that store the state of the pulser */

	dg2020.is_timebase = UNSET;

	dg2020.is_trig_in_mode = UNSET;
	dg2020.is_trig_in_slope = UNSET;
	dg2020.is_trig_in_level = UNSET;
	dg2020.is_repeat_time = UNSET;
	dg2020.is_neg_delay = UNSET;
	dg2020.neg_delay = 0;

	dg2020.block[ 0 ].is_used = dg2020.block[ 1 ].is_used = UNSET;

	for ( i = 0; i < MAX_PODS; i++ )
	{
		dg2020.pod[ i ].self = i;
		dg2020.pod[ i ].function = NULL;
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		dg2020.function[ i ].self = PULSER_CHANNEL_FUNC_MIN + i;
		dg2020.function[ i ].is_used = UNSET;
		dg2020.function[ i ].is_needed = UNSET;
		for ( j = 0; j < MAX_PODS_PER_FUNC; j++ )
			dg2020.function[ i ].pod[ j ] = NULL;
		dg2020.function[ i ].num_pods = 0;
		dg2020.function[ i ].num_channels = 0;
		dg2020.function[ i ].num_pulses = 0;
		dg2020.function[ i ].pulses = NULL;
		dg2020.function[ i ].needs_phases = UNSET;
		dg2020.function[ i ].phase_setup = NULL;
		dg2020.function[ i ].next_phase = 0;
		dg2020.function[ i ].is_inverted = UNSET;
		dg2020.function[ i ].delay = 0;
		dg2020.function[ i ].is_delay = UNSET;
		dg2020.function[ i ].is_high_level = UNSET;
		dg2020.function[ i ].is_low_level = UNSET;
	}

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		dg2020.channel[ i ].self = i;
		dg2020.channel[ i ].function = NULL;
	}

	for ( i = 0; i < 2; i++ )
	{
		dg2020_phs[ i ].function = NULL;
		for ( j = 0; j < PHASE_CW - PHASE_PLUS_X; j++ )
		{
			dg2020_phs[ i ].is_set[ j ] = UNSET;
			dg2020_phs[ i ].is_needed[ j ] = UNSET;
			dg2020_phs[ i ].pod[ j ] = NULL;
		}
	}

	dg2020_is_needed = SET;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_b_test_hook( void )
{
	if ( dg2020_Pulses == NULL )
	{
		dg2020_is_needed = UNSET;
		eprint( WARN, "%s loaded but no pulses are defined.",
				pulser_struct.name );
		return 1;
	}

	/* Check consistency of pulser settings and do everything to setup the
	   pulser for the test run */

	dg2020_IN_SETUP = SET;

	dg2020_init_setup( );

	dg2020_IN_SETUP = UNSET;

	/* We need some somewhat different functions for setting some of the
	   pulser properties */

//	pulser_struct.set_pulse_position = dg2020_change_pulse_position;
//	pulser_struct.set_pulse_length = dg2020_change_pulse_length;
//	pulser_struct.set_pulse_position_change =
//		dg2020_change_pulse_position_change;
//	pulser_struct.set_pulse_length_change = dg2020_change_pulse_length_change;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_b_end_of_test_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

	/* First we have to reset the internal representation back to its initial
	   state */

//	dg2020_full_reset( );

	/* Now we've got to find out about the maximum sequence length, set up
	   padding to achieve the repeat time and set the lengths of the last
	   phase pulses in the channels needing phase cycling */

//	  dg2020.max_seq_len = dg2020_get_max_seq_len( );
//	  dg2020_calc_padding( );
//	  dg2020_finalize_phase_pulses( PULSER_CHANNEL_PHASE_1 );
//	  dg2020_finalize_phase_pulses( PULSER_CHANNEL_PHASE_2 );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_b_exp_hook( void )
{
	int i;


	if ( ! dg2020_is_needed )
		return 1;

	/* Initialize the device */

//  if ( ! dg2020_init( DEVICE_NAME ) )
//	{
//		eprint( FATAL, "%s: Failure to initialize the pulser.",
//				pulser_struct.name );
//		THROW( EXCEPTION );
//	}

	/* Now we have to tell the pulser about all the pulses */

//	dg2020_reorganize_pulses( UNSET );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		if ( ! dg2020.function[ i ].is_used )
			continue;
//		dg2020_set_pulses( &dg2020.function[ i ] );
//		dg2020_clear_padding_block( &dg2020.function[ i ] );
	}

	/* Finally tell the pulser to update (we're always running in manual
	   update mode) and than switch the pulser into run mode */

//	dg2020_update_data( );
//	if ( ! dg2020_run( START ) )
//	{
//		eprint( FATAL, "%s:%ld: %s: Communication with pulser failed.",
//				Fname, Lc, pulser_struct.name );
//		THROW( EXCEPTION );
//	}

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_b_end_of_exp_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

//	gpib_local( dg2020.device );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void dg2020_b_exit_hook( void )
{
	PULSE *p, *np;
	int i;


	if ( ! dg2020_is_needed )
		return;

	/* free all the memory allocated within the module */

	for ( p = dg2020_Pulses; p != NULL; np = p->next, T_free( p ), p = np )
		;
	dg2020_Pulses = NULL;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		if ( dg2020.function[ i ].pulses != NULL )
			T_free( dg2020.function[ i ].pulses );
}


Var *pulser_update( Var *v)
{
	return vars_push( INT_VAR, 1 );
}

Var *pulser_next_phase( Var *v)
{
	return vars_push( INT_VAR, 1 );
}

Var *pulser_shift( Var *v)
{
	return vars_push( INT_VAR, 1 );
}

Var *pulser_increment( Var *v)
{
	return vars_push( INT_VAR, 1 );
}

