/*
  $Id$
*/


#define DG2020_B_MAIN


#include "dg2020_b.h"
#include "gpib_if.h"



/*---------------------------------------------------------------------------
  This function is called directly after all modules are loaded. Its function
  is to initialize all global variables that are needed in the module.
---------------------------------------------------------------------------*/

int dg2020_b_init_hook( void )
{
	int i, j;


	/* Now test that the name entry in the pulser structure is NULL, otherwise
	   another pulser driver has already been installed. */

	if ( pulser_struct.name != NULL )
	{
		eprint( FATAL, "%s:%ld: While loading driver DG2020_B found that "
				"driver %s is already installed.\n",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	pulser_struct.name = DEVICE_NAME;

	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* We have to set up the global structure for the pulser, especially the
	   pointers for the functions that will get called from pulser.c */

	dg2020.needs_update = UNSET;
	dg2020.is_running = SET;

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

	dg2020.is_cw_mode = UNSET;

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
		dg2020.function[ i ].phase_setup = NULL;
		dg2020.function[ i ].next_phase = 0;
		dg2020.function[ i ].is_inverted = UNSET;
		dg2020.function[ i ].delay = 0;
		dg2020.function[ i ].is_delay = UNSET;
		dg2020.function[ i ].is_high_level = UNSET;
		dg2020.function[ i ].is_low_level = UNSET;
		dg2020.function[ i ].pm = NULL;
		dg2020.function[ i ].pcm = NULL;
		dg2020.function[ i ].pc_len = 1;
	}

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		dg2020.channel[ i ].self = i;
		dg2020.channel[ i ].function = NULL;
		dg2020.channel[ i ].needs_update = UNSET;
		dg2020.channel[ i ].old = dg2020.channel[ i ].new = NULL;
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
	if ( dg2020_Pulses == NULL && ! dg2020.is_cw_mode )
	{
		dg2020_is_needed = UNSET;
		eprint( WARN, "%s loaded but no pulses are defined.\n",
				pulser_struct.name );
		return 1;
	}

	/* Check consistency of pulse settings and do everything to setup the
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

int dg2020_b_end_of_test_hook( void )
{
	if ( ! dg2020_is_needed || dg2020.is_cw_mode )
		return 1;

	/* First we have to reset the internal representation back to its initial
	   state */

	dg2020_full_reset( );

	/* Now we've got to find out about the maximum sequence length and set
	   up padding to achieve the requested repeat time */

	dg2020.max_seq_len = dg2020_get_max_seq_len( );
	dg2020_calc_padding( );

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
	
	if ( ! dg2020_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Failure to initialize the pulser: %s\n",
				pulser_struct.name, gpib_error_msg );
		THROW( EXCEPTION );
	}

	if ( ! dg2020.is_cw_mode )
	{
		/* Now we have to tell the pulser about all the pulses */

		if ( ! dg2020_reorganize_pulses( UNSET ) )
			THROW( EXCEPTION );

		for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
			if ( dg2020.function[ i ].is_used )
				dg2020_set_pulses( &dg2020.function[ i ] );

		/* Finally tell the pulser to update (we're always running in manual
		   update mode) and than switch the pulser into run mode */

		dg2020_update_data( );
	}
	else
		dg2020_cw_setup( );

	dg2020_run( dg2020.is_running );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_b_end_of_exp_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

	dg2020_run( STOP );
	gpib_local( dg2020.device );

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

	for ( p = dg2020_Pulses; p != NULL; p = np )
	{
		T_free( p->channel );
		np = p->next;
		T_free( p );
	}

	dg2020_Pulses = NULL;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		if ( dg2020.function[ i ].pcm != NULL )
		{
			T_free( dg2020.function[ i ].pcm );
			dg2020.function[ i ].pcm = NULL;
		}
		
		if ( dg2020.function[ i ].pulses != NULL )
			T_free( dg2020.function[ i ].pulses );
	}
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_state( Var *v )
{
	bool state;


	if ( v == NULL )
		return vars_push( INT_VAR, ( long ) dg2020.is_running );

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == INT_VAR )
		state = v->val.lval != 0 ? SET : UNSET;
	else if ( v->type == FLOAT_VAR )
		state = v->val.dval != 0.0 ? SET : UNSET;
	else
	{
		if ( ! strcasecmp( v->val.sptr, "OFF" ) )
			 state = UNSET;
		else if ( ! strcasecmp( v->val.sptr, "ON" ) )
			state = SET;
		else
		{
			eprint( FATAL, "%s:%d: %s: Invalid argument in call of "
					"`pulser_state'.\n", Fname, Lc, DEVICE_NAME );
			THROW( EXCEPTION );
		}
	}

	if ( I_am == PARENT || TEST_RUN )
		return vars_push( INT_VAR, ( long ) ( dg2020.is_running = state ) );

	dg2020_run( state );
	return vars_push( INT_VAR, ( long ) dg2020.is_running );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_update( Var *v )
{
	v = v;


	if ( dg2020.is_cw_mode )
	{
		eprint( FATAL, "%s:%ld: %s: Function `pulser_update' can't be used "
				"in CW mode.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* Send all changes to the pulser */

	if ( dg2020.needs_update )
		dg2020_do_update( );

	/* If we're doing a real experiment also tell the pulser to start */

	if ( ! TEST_RUN )
		dg2020_run( START );

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *pulser_cw_mode( Var *v )
{
	v = v;


	/* We can't have any pulses in cw mode */

	if ( dg2020_Pulses != NULL )
	{
		eprint( FATAL, "%s:%ld: %s: CW mode and use of pulses is mutually "
				"exclusive.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	/* We need the function MICROWAVE defined for cw mode */

	if ( ! dg2020.function[ PULSER_CHANNEL_MW ].is_used )
	{
		eprint( FATAL, "%s:%ld: %s: Function `MICROWAVE' has not been defined "
				"as needed for CW mode.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == EXTERNAL )
	{
		eprint( FATAL, "%s:%ld: %s: External trigger mode can't be used "
				"in CW mode.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_repeat_time )
	{
		eprint( FATAL, "%s:%ld: %s: Repeat time/frequency can't be set "
				"in CW mode.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	dg2020.mem_size = 128;
	dg2020.is_cw_mode = SET;
	dg2020.trig_in_mode = INTERNAL;
	dg2020.is_trig_in_mode = SET;

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------------------*/
/* Public function to change the position of pulses. If called with no  */
/* argument all active pulses that have a position change time set will */
/* be moved, otherwise all pulses passed as arguments to the function.  */
/* Take care: The changes will only commited on the next call of the    */
/*            function pulser_update() !                                */
/*----------------------------------------------------------------------*/

Var *pulser_shift( Var *v )
{
	PULSE *p;


	if ( dg2020.is_cw_mode )
	{
		eprint( FATAL, "%s:%ld: %s: Function `pulser_shift' can't be used in "
				"CW mode.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to shift all active pulses that
	   have a position change time value set */

	if ( v == NULL )
		for ( p = dg2020_Pulses; p != NULL; p = p->next )
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
					"shifting it is impossible.\n",
					Fname, Lc, pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->is_dpos )
		{
			eprint( FATAL, "%s:%ld: %s: Amount of position change hasn't "
					"been defined for pulse %ld.\n",
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
					"%ld leads to an invalid  negative position of %s.\n",
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


/*-------------------------------------------------------------------------*/
/* Public function for incrementing the length of pulses. If called with   */
/* no argument all active pulses that have a length change defined are     */
/* incremented, oltherwise all pulses passed as arguments to the function. */
/* Take care: The changes will only commited on the next call of the       */
/*            function pulser_update() !                                   */
/*-------------------------------------------------------------------------*/

Var *pulser_increment( Var *v )
{
	PULSE *p;


	if ( dg2020.is_cw_mode )
	{
		eprint( FATAL, "%s:%ld: %s: Function `pulser_increment' can't be used "
				"in CW mode.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to increment all active pulses
	   that have a length change time value set */

	if ( v == NULL )
		for ( p = dg2020_Pulses; p != NULL; p = p->next )
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
					"incrementing it is impossibe.\n",
					Fname, Lc, pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->is_dlen )
		{
			eprint( FATAL, "%s:%ld: %s: Length change time hasn't been "
					"defined for pulse %ld.\n",
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
					"%ld leads to an invalid negative pulse length of %s.\n",
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
	int j;
	FUNCTION *f;


	if ( dg2020.is_cw_mode )
	{
		eprint( FATAL, "%s:%ld: %s: Function `pulser_next_phase' can't be "
				"used in CW mode.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		if ( dg2020_phs[ 0 ].function == NULL &&
			 dg2020_phs[ 1 ].function == NULL &&
			 TEST_RUN )
		{
			eprint( SEVERE, "%s:%ld: %s: No phase cycling is used any of the "
					"functions.\n", Fname, Lc, pulser_struct.name );
			return vars_push( INT_VAR, 0 );
		}
					
		if ( dg2020_phs[ 0 ].function != NULL )
			pulser_next_phase( vars_push( INT_VAR, 1 ) );
		if ( dg2020_phs[ 1 ].function != NULL )
			pulser_next_phase( vars_push( INT_VAR, 2 ) );
	}

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		if ( v->val.lval != 1 && v->val.lval != 2 )
		{
			eprint( FATAL, "%s:%ld: %s: Invalid phase number: %ld.\n",
					Fname, Lc, pulser_struct.name, v->val.lval );
			THROW( EXCEPTION );
		}

		f = dg2020_phs[ v->val.lval - 1 ].function;
		vars_pop( v );

		if ( ! f->is_used )
		{
			if ( TEST_RUN )
				eprint( SEVERE, "%s:%ld: %s: Function `%s' to be phase cycled "
						"is not used.\n", Fname, Lc, pulser_struct.name,
						Function_Names[ f->self ] );
			return vars_push( INT_VAR, 0 );
		}

		if ( ++f->next_phase >= f->pc_len )
			f->next_phase = 0;

		if ( ! TEST_RUN )
			for ( j = 0; j <= PHASE_CW - PHASE_PLUS_X; j++ )
				if ( f->phase_setup->is_set[ j ] &&
					 ! dg2020_channel_assign( 
						 f->pcm[ j * f->pc_len + f->next_phase ]->self,
						 f->phase_setup->pod[ j ]->self ) )
					return vars_push( INT_VAR, 0 );
	}

	dg2020.needs_update = SET;
	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_phase_reset( Var *v )
{
	int j;
	FUNCTION *f;


	if ( dg2020.is_cw_mode )
	{
		eprint( FATAL, "%s:%ld: %s: Function `pulser_next_reset' can't be "
				"used in CW mode.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	if ( v == NULL )
	{
		if ( dg2020_phs[ 0 ].function == NULL &&
			 dg2020_phs[ 1 ].function == NULL &&
			 TEST_RUN )
		{
			eprint( SEVERE, "%s:%ld: %s: No phase cycling is used any of the "
					"functions.\n", Fname, Lc, pulser_struct.name );
			return vars_push( INT_VAR, 0 );
		}
					
		if ( dg2020_phs[ 0 ].function != NULL )
			pulser_phase_reset( vars_push( INT_VAR, 1 ) );
		if ( dg2020_phs[ 1 ].function != NULL )
			pulser_phase_reset( vars_push( INT_VAR, 2 ) );
	}

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		if ( v->val.lval != 1 && v->val.lval != 2 )
		{
			eprint( FATAL, "%s:%ld: %s: Invalid phase number: %ld.\n",
					Fname, Lc, pulser_struct.name, v->val.lval );
			THROW( EXCEPTION );
		}

		f = dg2020_phs[ v->val.lval - 1 ].function;
		vars_pop( v );

		if ( ! f->is_used )
		{
			if ( TEST_RUN )
				eprint( SEVERE, "%s:%ld: %s: Function `%s' to be phase cycled "
						"is not used.\n", Fname, Lc, pulser_struct.name,
						Function_Names[ f->self ] );
			return vars_push( INT_VAR, 0 );
		}

		f->next_phase = 0;

		if ( ! TEST_RUN )
			for ( j = 0; j <= PHASE_CW - PHASE_PLUS_X; j++ )
				if ( f->phase_setup->is_set[ j ] && 
					 ! dg2020_channel_assign(
						 f->pcm[ j * f->pc_len + 0 ]->self,
						 f->phase_setup->pod[ j ]->self ) )
					return vars_push( INT_VAR, 0 );
	}

	dg2020.needs_update = SET;
	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_pulse_reset( Var *v )
{
	PULSE *p;


	if ( dg2020.is_cw_mode )
	{
		eprint( FATAL, "%s:%ld: %s: Function `pulser_pulse_reset' can't be "
				"used in CW mode.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( ! dg2020_is_needed )
		return vars_push( INT_VAR, 1 );

	/* An empty pulse list means that we have to reset all pulses (even the
       inactive ones) */

	if ( v == NULL )
	{
		if ( dg2020_phs[ 0 ].function != NULL ||
			 dg2020_phs[ 1 ].function != NULL )
			pulser_phase_reset( NULL );

		for ( p = dg2020_Pulses; p != NULL; p = p->next )
			if ( p->num >= 0 )
				pulser_pulse_reset( vars_push( INT_VAR, p->num ) );
	}

	/* Otherwise run through the supplied pulse list */

	for ( ; v != NULL; v = vars_pop( v ) )
	{
		vars_check( v, INT_VAR );
		p = dg2020_get_pulse( v->val.lval );

		/* Reset all changeable properties back to their initial values */

		if ( p->is_pos && ! p->is_old_pos )
		{
			p->old_pos = p->pos;
			p->is_old_pos = SET;
		}

		p->pos = p->initial_pos;
		p->is_pos = p->initial_is_pos;

		if ( p->is_len && ! p->is_old_len )
		{
			p->old_len = p->len;
			p->is_old_len = SET;
		}

		p->len = p->initial_len;
		p->is_len = p->initial_is_len;

		if ( p->initial_is_dpos )
			p->dpos = p->initial_dpos;
		p->is_dpos = p->initial_is_dpos;

		if ( p->initial_is_dlen )
			p->dlen = p->initial_dlen;
		p->is_dlen = p->initial_is_dlen;

		p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
		p->needs_update = NEEDS_UPDATE( p );

		if ( p->needs_update )
			dg2020.needs_update = SET;
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *pulser_lock_keyboard( Var *v )
{
	bool lock;


	if ( v == NULL )
		lock = SET;
	else
	{
		vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

		if ( v->type == INT_VAR )
			lock = v->val.lval == 0 ? UNSET : UNSET;
		else if ( v->type == FLOAT_VAR )
			lock = v->val.dval == 0.0 ? UNSET : UNSET;
		else
		{
			if ( ! strcasecmp( v->val.sptr, "OFF" ) )
				lock = UNSET;
			else if ( ! strcasecmp( v->val.sptr, "ON" ) )
				lock = SET;
			else
			{
				eprint( FATAL, "%s:%d: %s: Invalid argument in call of "
						"`pulser_lock_keyboard'.\n",
						Fname, Lc, DEVICE_NAME );
				THROW( EXCEPTION );
			}
		}
	}

	if ( ! TEST_RUN )
		dg2020_lock_state( lock );

	return vars_push( INT_VAR, lock ? 1 : 0 );
}
