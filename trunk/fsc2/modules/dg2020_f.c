/*
  $Id$
*/


#define DG2020_MAIN


#include "dg2020.h"



/*---------------------------------------------------------------------------
  This function is called directly after all modules are loaded. Its main
  function is to initialize all global variables that are needed in the
  module.
---------------------------------------------------------------------------*/

int dg2020_init_hook( void )
{
	int i;


	/* First we test that the name entry in the pulser structure is NULL,
	   otherwise we've got to assume, that another pulser driver has already
	   been installed. */

	if ( pulser_struct.name != NULL )
	{
		eprint( FATAL, "%s:%ld: While loading driver for DG2020 found that "
				"driver for pulser %s is already installed.\n",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	pulser_struct.name = get_string_copy( "DG2020" );

	/* Than we have to set up the global structure for the pulser, especially
	   we have to set the pointers for the functions that will get called from
	   pulser.c */

	dg2020.need_update = UNSET;
	dg2020.is_running = UNSET;

	pulser_struct.set_timebase = set_timebase;

	pulser_struct.assign_function = assign_function;
	pulser_struct.assign_channel_to_function = assign_channel_to_function;
	pulser_struct.invert_function = invert_function;
	pulser_struct.set_delay_function = set_delay_function;
	pulser_struct.set_function_high_level = set_function_high_level;
	pulser_struct.set_function_low_level = set_function_low_level;

	pulser_struct.set_trigger_mode = set_trigger_mode;
	pulser_struct.set_repeat_time = set_repeat_time;
	pulser_struct.set_trig_in_level = set_trig_in_level;
	pulser_struct.set_trig_in_slope = set_trig_in_slope;

	pulser_struct.set_phase_reference = set_phase_reference;

	pulser_struct.new_pulse = new_pulse;
	pulser_struct.set_pulse_function = set_pulse_function;
	pulser_struct.set_pulse_position = set_pulse_position;
	pulser_struct.set_pulse_length = set_pulse_length;
	pulser_struct.set_pulse_position_change = set_pulse_position_change;
	pulser_struct.set_pulse_length_change = set_pulse_length_change;
	pulser_struct.set_pulse_phase_cycle = set_pulse_phase_cycle;
	pulser_struct.set_pulse_maxlen = set_pulse_maxlen;
	pulser_struct.set_pulse_replacements = set_pulse_replacements;

	pulser_struct.get_pulse_function = get_pulse_function;
	pulser_struct.get_pulse_position = get_pulse_position;
	pulser_struct.get_pulse_length = get_pulse_length;
	pulser_struct.get_pulse_position_change = get_pulse_position_change;
	pulser_struct.get_pulse_length_change = get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = get_pulse_phase_cycle;
	pulser_struct.get_pulse_maxlen = get_pulse_maxlen;

	pulser_struct.setup_phase = setup_phase;

	/* Finally, we initialize variables that store the state of the pulser */

	dg2020.is_timebase = UNSET;

	dg2020.is_trig_in_mode = UNSET;
	dg2020.is_trig_in_slope = UNSET;
	dg2020.is_trig_in_level = UNSET;
	dg2020.is_repeat_time = UNSET;
	
	for ( i = 0; i < MAX_PODS; i++ )
	{
		dg2020.pod[ i ].function = NULL;
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		dg2020.function[ i ].self = PULSER_CHANNEL_FUNC_MIN + i;
		dg2020.function[ i ].is_used = UNSET;
		dg2020.function[ i ].is_needed = UNSET;
		dg2020.function[ i ].pod = NULL;
		dg2020.function[ i ].is_phs = UNSET;
		dg2020.function[ i ].num_channels = 0;
		dg2020.function[ i ].num_pulses = 0;
		dg2020.function[ i ].pulses = NULL;
		dg2020.function[ i ].phase_func = NULL;
		dg2020.function[ i ].is_inverted = UNSET;
		dg2020.function[ i ].delay = 0;
		dg2020.function[ i ].is_delay = UNSET;
		dg2020.function[ i ].is_high_level = UNSET;
		dg2020.function[ i ].is_low_level = UNSET;
	}

	for ( i = 0; i < MAX_CHANNELS; i++ )
		dg2020.channel[ i ].function = NULL;

	dg2020_is_needed = SET;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_test_hook( void )
{
	if ( Pulses == NULL )
	{
		dg2020_is_needed = UNSET;
		eprint( WARN, "DG2020 loaded but no pulses are defined.\n" );
		return 1;
	}

	/* Check consistency of pulser settings and do everything to setup the
	   pulser for the test run */

	init_setup( );

	/* We need some somewhat different functions for setting some of the
	   pulser properties */

	pulser_struct.set_pulse_position = change_pulse_position;
	pulser_struct.set_pulse_length = change_pulse_length;
	pulser_struct.set_pulse_position_change = change_pulse_position_change;
	pulser_struct.set_pulse_length_change = change_pulse_length_change;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_exp_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

	/* Initialize the device */

	if ( ! dg2020_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "DG2020: Failure to initialize the pulser.\n" );
		THROW( EXCEPTION );
	}

	/* Now we have to really setup the pulser */



	/* Finally we switch the pulser into run mode */

	if ( ! dg2020_run( START ) )
	{
		eprint( FATAL, "%s:%ld: DG2020: Communication with pulser "
				"failed.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_end_of_exp_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void dg2020_exit_hook( void )
{
	FUNCTION *f;
	PULSE *p, *np;
	int i;


	if ( ! dg2020_is_needed )
		return;

	/* free all the memory allocated within the module */

	for ( p = Pulses; p != NULL; p = np )
	{
		if ( p->channel != NULL )
			T_free( p->channel );
		
		if ( p->num_repl != 0 )
			T_free( p->repl_list );

		np = p->next;
		T_free( p );
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		if ( f->pulses != NULL )
			T_free( f->pulses );
	}
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_start( Var *v )
{
	v = v;

	if ( dg2020.need_update )
	{
		if ( TEST_RUN )
			do_checks( );
		else
			do_update( );
	}

	/* If we're doing a real experiment also tell the pulser to start */

	if ( ! TEST_RUN )
	{
		if ( ! dg2020_run( START ) )
		{
			eprint( FATAL, "%s:%ld: DG2020: Communication with pulser "
					"failed.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}

	return vars_push( INT_VAR, 1 );
}



