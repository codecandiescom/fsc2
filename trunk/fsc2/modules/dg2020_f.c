/*
  $Id$
*/


#define DG2020_F_MAIN


#include "dg2020_f.h"
#include "gpib.h"



/*---------------------------------------------------------------------------
  This function is called directly after all modules are loaded. Its main
  function is to initialize all global variables that are needed in the
  module.
---------------------------------------------------------------------------*/

int dg2020_f_init_hook( void )
{
	int i, j, k;


	/* Now test that the name entry in the pulser structure is NULL, otherwise
	   assume, that another pulser driver has already been installed. */

	if ( pulser_struct.name != NULL )
	{
		eprint( FATAL, "%s:%ld: While loading driver for DG2020_F found that "
				"driver %s is already installed.",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	pulser_struct.name = get_string_copy( "DG2020_F" );

	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

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
	pulser_struct.set_grace_period = dg2020_set_grace_period;

	pulser_struct.get_pulse_function = dg2020_get_pulse_function;
	pulser_struct.get_pulse_position = dg2020_get_pulse_position;
	pulser_struct.get_pulse_length = dg2020_get_pulse_length;
	pulser_struct.get_pulse_position_change = dg2020_get_pulse_position_change;
	pulser_struct.get_pulse_length_change = dg2020_get_pulse_length_change;
	pulser_struct.get_pulse_phase_cycle = dg2020_get_pulse_phase_cycle;

	pulser_struct.phase_setup_prep = dg2020_setup_phase_prep;
	pulser_struct.phase_setup = dg2020_setup_phase;

	pulser_struct.set_phase_switch_delay = dg2020_set_phase_switch_delay;

	/* Finally, we initialize variables that store the state of the pulser */

	dg2020.is_timebase = UNSET;

	dg2020.is_trig_in_mode = UNSET;
	dg2020.is_trig_in_slope = UNSET;
	dg2020.is_trig_in_level = UNSET;
	dg2020.is_repeat_time = UNSET;
	dg2020.is_neg_delay = UNSET;
	dg2020.neg_delay = 0;
	dg2020.is_grace_period = UNSET;

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
		dg2020.function[ i ].pod = NULL;
		dg2020.function[ i ].is_phs = UNSET;
		dg2020.function[ i ].is_psd = UNSET;
		dg2020.function[ i ].num_channels = 0;
		dg2020.function[ i ].num_pulses = 0;
		dg2020.function[ i ].pulses = NULL;
		dg2020.function[ i ].next_phase = 0;
		dg2020.function[ i ].phase_func = NULL;
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

	for ( i = 0; i < 3; i++ )
		for ( j = 0; j < 4; j++ )
			for ( k = 0; k < 2; k++ )
				phs[ i ].is_var[ j ][ k ] = UNSET;

	dg2020_is_needed = SET;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_f_test_hook( void )
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

	pulser_struct.set_pulse_position = dg2020_change_pulse_position;
	pulser_struct.set_pulse_length = dg2020_change_pulse_length;
	pulser_struct.set_pulse_position_change =
		dg2020_change_pulse_position_change;
	pulser_struct.set_pulse_length_change = dg2020_change_pulse_length_change;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_f_end_of_test_hook( void )
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

int dg2020_f_exp_hook( void )
{
	int i;


	if ( ! dg2020_is_needed )
		return 1;

	/* Initialize the device */

	if ( ! dg2020_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Failure to initialize the pulser.",
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	/* Now we have to tell the pulser about all the pulses */

	dg2020_reorganize_pulses( UNSET );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		if ( ! dg2020.function[ i ].is_used )
			continue;
		dg2020_set_pulses( &dg2020.function[ i ] );
		dg2020_clear_padding_block( &dg2020.function[ i ] );
	}

	/* Finally tell the pulser to update (we're always running in manual
	   update mode) and than switch the pulser into run mode */

	dg2020_update_data( );
	if ( ! dg2020_run( START ) )
	{
		eprint( FATAL, "%s:%ld: %s: Communication with pulser failed.",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_f_end_of_exp_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

    gpib_local( dg2020.device );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void dg2020_f_exit_hook( void )
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


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_update( Var *v )
{
	v = v;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* Send all changes to the pulser */

	if ( dg2020.needs_update )
		dg2020_do_update( );

	/* If we're doing a real experiment also tell the pulser to start */

	if ( ! TEST_RUN && ! dg2020_run( START ) )
	{
		eprint( FATAL, "%s:%ld: %s: Communication with pulser failed.",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_shift( Var *v )
{
	PULSE *p;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
		for( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dpos )
				pulser_shift( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		p = dg2020_get_pulse( v->val.lval );

		if ( ! p->is_pos )
		{
			eprint( FATAL, "%s:ld: %s: Pulse %ld has no position set, so "
					"shifting it is impossible.",
					Fname, Lc, pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->is_dpos )
		{
			eprint( FATAL, "%s:%ld: %s: Time for position change hasn't "
					"been defined for pulse %ld.",
					Fname, Lc, pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->is_old_pos )
		{
			p->old_pos = p->pos;
			p->is_old_pos = SET;
		}

		if ( ( p->pos += p->dpos ) < 0 )
		{
			eprint( FATAL, "%s:%ld: %s: Shifting the position of pulse "
					"%ld leads to an invalid  negative position of %s.",
					Fname, Lc, pulser_struct.name,
					p->num, dg2020_pticks( p->pos ) );
			THROW( EXCEPTION );
		}

		if ( p->pos == p->old_pos )       // nothing really changed ?
			p->is_old_pos = UNSET;

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
		p->needs_update = NEEDS_UPDATE( p );

		if ( p->needs_update )
			dg2020.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_increment( Var *v )
{
	PULSE *p;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
		for( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 && p->is_active && p->is_dlen )
				pulser_increment( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		p = dg2020_get_pulse( v->val.lval );

		if ( ! p->is_len )
		{
			eprint( FATAL, "%s:%ld: %s: Pulse %ld has no length set, so "
					"imcrementing it is impossibe.",
					Fname, Lc, pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->is_dlen )
		{
			eprint( FATAL, "%s:%ld: %s: Length change time hasn't been "
					"defined for pulse %ld.",
					Fname, Lc, pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}
	
		if ( ! p->is_old_len )
		{
			p->old_len = p->len;
			p->is_old_len = SET;
		}

		if ( ( p->len += p->dlen ) < 0 )
		{
			eprint( FATAL, "%s:%ld: %s: Incrementing the length of pulse "
					"%ld leads to an invalid negative pulse length of %s.",
					Fname, Lc, pulser_struct.name,
					p->num, dg2020_pticks( p->len ) );
			THROW( EXCEPTION );
		}

		if ( p->old_len == p->len )
			p->is_old_len = UNSET;

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
		p->needs_update = NEEDS_UPDATE( p );

		/* If the pulse was or is active we've got to update the pulser */

		if ( p->needs_update )
			dg2020.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_next_phase( Var *v )
{
	FUNCTION *f;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		if ( ! dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used &&
			 ! dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used &&
			TEST_RUN )
		{
			eprint( SEVERE, "%s:%ld: %s: No phase functions are in use.",
					Fname, Lc, pulser_struct.name );
			return vars_push( INT_VAR, 0 );
		}
					
		if ( dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used )
			pulser_next_phase( vars_push( INT_VAR, 1 ) );
		if ( dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used )
			pulser_next_phase( vars_push( INT_VAR, 2 ) );
	}

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		if ( v->val.lval != 1 && v->val.lval != 2 )
		{
			eprint( FATAL, "%s:%ld: %s: Invalid phase number: %ld.",
					Fname, Lc, pulser_struct.name, v->val.lval );
			THROW( EXCEPTION );
		}

		f = &dg2020.function[ v->val.lval == 1 ? PULSER_CHANNEL_PHASE_1 :
							  PULSER_CHANNEL_PHASE_2 ];
		vars_pop( v );

		if ( ! f->is_used && TEST_RUN )
		{
			eprint( SEVERE, "%s:%ld: %s: Phase function `%s' is not used.",
					Fname, Lc, pulser_struct.name, Function_Names[ f->self ] );
			return vars_push( INT_VAR, 0 );
		}

		if ( f->next_phase >= f->num_channels )
			f->next_phase = 0;

		if ( ! TEST_RUN )
		{
			if ( ! dg2020_channel_assign( f->channel[ f->next_phase++ ]->self,
										  f->pod->self ) ||
				 ! dg2020_channel_assign( f->channel[ f->next_phase++ ]->self,
										  f->pod2->self ) ||
				 ! dg2020_update_data( ) )
				return vars_push( INT_VAR, 0 );
		}
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_phase_reset( Var *v )
{
	FUNCTION *f;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		if ( ! dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used &&
			 ! dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used &&
			TEST_RUN )
		{
			eprint( SEVERE, "%s:%ld: %s: No phase functions are in use.",
					Fname, Lc, pulser_struct.name );
			return vars_push( INT_VAR, 0 );
		}
					
		if ( dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used )
			pulser_phase_reset( vars_push( INT_VAR, 1 ) );
		if ( dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used )
			pulser_phase_reset( vars_push( INT_VAR, 2 ) );
	}

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		if ( v->val.lval != 1 && v->val.lval != 2 )
		{
			eprint( FATAL, "%s:%ld: %s: Invalid phase number: %ld.",
					Fname, Lc, pulser_struct.name, v->val.lval );
			THROW( EXCEPTION );
		}

		f = &dg2020.function[ v->val.lval == 1 ? PULSER_CHANNEL_PHASE_1 :
							  PULSER_CHANNEL_PHASE_2 ];
		vars_pop( v );

		if ( ! f->is_used && TEST_RUN )
		{
			eprint( SEVERE, "%s:%ld: %s: Phase function `%s' is not used.",
					Fname, Lc, pulser_struct.name, Function_Names[ f->self ] );
			return vars_push( INT_VAR, 0 );
		}

		if ( ! TEST_RUN )
		{
			if ( ! dg2020_channel_assign( f->channel[ 0 ]->self,
										  f->pod->self ) ||
				 ! dg2020_channel_assign( f->channel[ 1 ]->self,
										  f->pod2->self ) ||
				 ! dg2020_update_data( ) )
				return vars_push( INT_VAR, 0 );
		}

		f->next_phase = 2;
	}

	return vars_push( INT_VAR, 1 );;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_pulse_reset( Var *v )
{
	PULSE *p;


	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to reset all pulses (even the
       inactive ones) */

	if ( v == NULL )
		for( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				pulser_pulse_reset( vars_push( INT_VAR, p->num ) );

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		p = dg2020_get_pulse( v->val.lval );

		/* Reset all changeable properties back to their initial values */

		if ( p->is_pos )
		{
			p->old_pos = p->pos;
			p->is_old_pos = SET;
		}
		p->pos = p->initial_pos;
		p->is_pos = p->initial_is_pos;

		if ( p->is_len )
		{
			p->old_len = p->len;
			p->is_old_len = SET;
		}
		p->len = p->initial_len;
		p->is_len = p->initial_is_len;

		p->dpos = p->initial_dpos;
		p->is_dpos = p->initial_is_dpos;

		p->dlen = p->initial_dlen;
		p->is_dlen = p->initial_is_dlen;

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
		p->needs_update = NEEDS_UPDATE( p );

		if ( p->needs_update )
			dg2020.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}
