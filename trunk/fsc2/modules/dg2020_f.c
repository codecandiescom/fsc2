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

	dg2020.needs_update = UNSET;
	dg2020.is_running = UNSET;

	pulser_struct.set_timebase = dg2020_store_timebase;

	pulser_struct.assign_function = dg2020_assign_function;
	pulser_struct.assign_channel_to_function =
		dg2020_assign_channel_to_function;
	pulser_struct.invert_function = dg2020_invert_function;
	pulser_struct.set_delay_function = dg2020_set_delay_function;
	pulser_struct.set_function_high_level = dg2020_set_function_high_level;
	pulser_struct.set_function_low_level = dg2020_set_function_low_level;

	pulser_struct.set_trigger_mode = dg2020_set_trigger_mode;
	pulser_struct.set_repeat_time = dg2020_set_repeat_time;
	pulser_struct.set_trig_in_level = dg2020_set_trig_in_level;
	pulser_struct.set_trig_in_slope = dg2020_set_trig_in_slope;

	pulser_struct.set_phase_reference = dg2020_set_phase_reference;

	pulser_struct.new_pulse = dg2020_new_pulse;
	pulser_struct.set_pulse_function = dg2020_set_pulse_function;
	pulser_struct.set_pulse_position = dg2020_set_pulse_position;
	pulser_struct.set_pulse_length = dg2020_set_pulse_length;
	pulser_struct.set_pulse_position_change = dg2020_set_pulse_position_change;
	pulser_struct.set_pulse_length_change = dg2020_set_pulse_length_change;
	pulser_struct.set_pulse_phase_cycle = dg2020_set_pulse_phase_cycle;

	pulser_struct.get_pulse_function = dg2020_get_pulse_function;
	pulser_struct.get_pulse_position = dg2020_get_pulse_position;
	pulser_struct.get_pulse_length = dg2020_get_pulse_length;
	pulser_struct.get_pulse_position_change = dg2020_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = dg2020_get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = dg2020_get_pulse_phase_cycle;

	pulser_struct.setup_phase = dg2020_setup_phase;

	pulser_struct.set_phase_switch_delay = dg2020_set_phase_switch_delay;

	/* Finally, we initialize variables that store the state of the pulser */

	dg2020.is_timebase = UNSET;

	dg2020.is_trig_in_mode = UNSET;
	dg2020.is_trig_in_slope = UNSET;
	dg2020.is_trig_in_level = UNSET;
	dg2020.is_repeat_time = UNSET;
	
	dg2020.block[ 0 ].is_used = dg2020.block[ 1 ].is_used = UNSET;

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
		dg2020.function[ i ].is_psd = UNSET;
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
	if ( dg2020_Pulses == NULL )
	{
		dg2020_is_needed = UNSET;
		eprint( WARN, "DG2020 loaded but no pulses are defined.\n" );
		return 1;
	}

	/* Check consistency of pulser settings and do everything to setup the
	   pulser for the test run */

	dg2020_init_setup( );

	/* We need some somewhat different functions for setting some of the
	   pulser properties */

	pulser_struct.set_pulse_position = dg2020_change_pulse_position;
	pulser_struct.set_pulse_length = dg2020_change_pulse_length;
	pulser_struct.set_pulse_position_change =
		dg2020_change_pulse_position_change;
	pulser_struct.set_pulse_length_change = dg2020_change_pulse_length_change;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_end_of_test_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

	/* First we have to reset the internal representation back to its initial
	   state */

	dg2020_full_reset( );

	/* Now we've got to find out about the maximum sequence length, set up
	   padding to achieve the repeat time and set the lengths of the last
	   phase pulses in the channels needing phase cycling */

	dg2020.max_seq_len = dg2020_get_max_seq_len( );
	dg2020_calc_padding( );
	dg2020_finalize_phase_pulses( PULSER_CHANNEL_PHASE_1 );
	dg2020_finalize_phase_pulses( PULSER_CHANNEL_PHASE_2 );

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
	PULSE *p, *np;
	int i;


	if ( ! dg2020_is_needed )
		return;

	/* free all the memory allocated within the module */

	for ( p = dg2020_Pulses; p != NULL; np = p->next, T_free( p ), p = np )
		;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		if ( dg2020.function[ i ].pulses != NULL )
			T_free( dg2020.function[ i ].pulses );
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_start( Var *v )
{
	v = v;


	if ( dg2020.needs_update )
		dg2020_do_update( );

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_shift( Var *v )
{
	PULSE *p;


	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
		for( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num > 0 && p->is_active && p->dpos != 0 )
				pulser_shift( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		p = dg2020_get_pulse( v->val.lval );

		if ( ! p->is_dpos )
		{
			eprint( FATAL, "%s:%ld: DG2020: Time for position change hasn't "
					"been defined for pulse %ld.\n", Fname, Lc, p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->is_old_pos )
		{
			p->old_pos = p->pos;
			p->is_old_pos = SET;
		}

		if ( ( p->pos += p->dpos ) < 0 )
		{
			eprint( FATAL, "%s:%ld: DG2020: Shifting the position of pulse "
					"%ld leads to an invalid  negative position of %s.\n",
					Fname, Lc, p->num, dg2020_pticks( p->pos ) );
			THROW( EXCEPTION );
		}

		/* If the pulse is active we've got to update the pulser */

		if ( p->is_active )
		{
			p->needs_update = dg2020.needs_update = SET;

			/* stop the pulser */

			if ( ! TEST_RUN && ! dg2020_run( STOP ) )
			{
				eprint( FATAL, "%s:%ld: DG2020: Communication with pulser "
						"failed.\n", Fname, Lc );
				THROW( EXCEPTION );
			}
		}
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_increment( Var *v )
{
	PULSE *p;
	bool was_active = UNSET;


	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
		for( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num > 0 && p->is_active && p->dlen != 0 )
				pulser_increment( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		p = dg2020_get_pulse( v->val.lval );

		if ( ! p->is_dlen )
		{
			eprint( FATAL, "%s:%ld: DG2020: Length change time hasn't been "
					"defined for pulse %ld.\n", Fname, Lc, p->num );
			THROW( EXCEPTION );
		}
	
		if ( ! p->is_old_len )
		{
			p->old_len = p->len;
			p->is_old_len = SET;
		}

		if ( ( p->len += p->dlen ) < 0 )
		{
			eprint( FATAL, "%s:%ld: DG2020: Incrementing the pulse length "
					"of pulse %ld leads to an invalid negative pulse length "
					"of %s.\n", Fname, Lc, p->num, dg2020_pticks( p->len ) );
			THROW( EXCEPTION );
		}

		was_active = p->is_active;

		/* If the pulse wasn't active but has now got a non-zero length and has
		   a defined position as well it becomes active */

		if ( ! p->is_active && p->len > 0 && p->is_pos )
			p->is_active = p->has_been_active = SET;

		/* If the pulse was or is active we've got to update the pulser */

		if ( was_active || p->is_active )
		{
			p->needs_update = dg2020.needs_update = SET;

			/* stop the pulser */

			if ( ! TEST_RUN && ! dg2020_run( STOP ) )
			{
				eprint( FATAL, "%s:%ld: DG2020: Communication with pulser "
						"failed.\n", Fname, Lc );
				THROW( EXCEPTION );
			}
		}
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_reset( Var *v )
{
	PULSE *p;
	bool was_active;


	/* An empty pulse list means that we have to reset all pulses (even the
       inactive ones) */

	if ( v == NULL )
		for( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				pulser_reset( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		p = dg2020_get_pulse( v->val.lval );

		was_active = p->is_active;

		/* Reset all changeable properties back to their initial values */

		p->pos = p->initial_pos;
		p->is_pos = p->initial_is_pos;
		p->len = p->initial_len;
		p->is_len = p->initial_is_len;
		p->dpos = p->initial_dpos;
		p->is_dpos = p->initial_is_dpos;
		p->dlen = p->initial_dlen;
		p->is_dlen = p->initial_is_dlen;

		/* Pulse gets active if it has a position and a non-zero length */

		p->is_active = ( p->is_pos && p->is_len && p->len > 0 );

		/* If the pulse was or is active we've got to update the pulser */

		if ( was_active || p->is_active )
		{
			p->needs_update = dg2020.needs_update = SET;

			/* stop the pulser */

			if ( ! TEST_RUN && ! dg2020_run( STOP ) )
			{
				eprint( FATAL, "%s:%ld: DG2020: Communication with pulser "
						"failed.\n", Fname, Lc );
				THROW( EXCEPTION );
			}
		}
	}

	return vars_push( INT_VAR, 1 );
}
