/*
  $Id$
*/


#include "dg2020.h"



static bool dg2020_is_needed;
static DG2020 dg2020;
static PULSE *Pulses;


/*----------------------------------------------------*/
/*----------------------------------------------------*/

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

	/* Than we have to do in the init_hook is to set up the global structure
	   for pulsers. */

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
		dg2020.function[ i ].self = i;
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
		eprint( WARN, "DG2020 loaded but no pulses defined.\n" );
		return 1;
	}

	check_consistency( );

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

int dg2020_exp_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void dg2020_exit_hook( void )
{
	if ( ! dg2020_is_needed )
		return;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_timebase( double timebase )
{
	if ( timebase < MIN_TIMEBASE || timebase > MAX_TIMEBASE )
	{
		char *min = get_string_copy( ptime( ( double ) MIN_TIMEBASE ) );
		char *max = get_string_copy( ptime( ( double ) MAX_TIMEBASE ) );

		eprint( FATAL, "%s:%ld: DG2020: Invalid time base of %s, valid range "
				"is %s to %s.\n", Fname, Lc, ptime( timebase ), min, max );
		T_free( min );
		T_free( max );
		THROW( EXCEPTION );
	}

	dg2020.is_timebase = SET;
	dg2020.timebase = timebase;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool assign_function( int function, long pod )
{
	FUNCTION *f = &dg2020.function[ function ];
	POD *p = &dg2020.pod[ pod ];
	

	if ( pod < 0 || pod > MAX_PODS )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid pod number: %ld, valid pod "
				"number are %d-%d.\n", Fname, Lc, pod, 0, MAX_PODS - 1 );
		THROW( EXCEPTION );
	}

	if ( p->function != NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Pod number %ld has already been "
				"assigned to function `%s'.\n", Fname, Lc, pod,
				Function_Names[ p->function->self ] );
		THROW( EXCEPTION );
	}

	f->is_used = SET;

	/* Except for the phase functions only one pod can be assigned */

	if ( f->pod != NULL )
	{
		if ( f->self != PULSER_CHANNEL_PHASE_1 && 
			 f->self != PULSER_CHANNEL_PHASE_2 )
		{
			eprint( FATAL, "%s:%ld: DG2020: A pod has already been assigned "
					"to function `%s'.\n", Fname, Lc,
					Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}

		if ( f->pod2 != NULL )
		{
			eprint( FATAL, "%s:%ld: DG2020: There have already been two pods "
					"assigned to function `%s'.\n", Fname, Lc,
					Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}

		f->pod2 = p;
	}
	else
		f->pod = p;
	
	p->function = f;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool assign_channel_to_function( int function, long channel )
{
	FUNCTION *f = &dg2020.function[ function ];
	CHANNEL *c = &dg2020.channel[ channel ];

	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid channel number: %ld, valid "
				"range is 0-%d.\n", Fname, Lc, channel, MAX_CHANNELS - 1 );
		THROW( EXCEPTION );
	}

	if ( c->function != NULL )
	{
		if ( c->function->self == function )
		{
			eprint( SEVERE, "%s:%ld: DG2020: Channel %ld is assigned twice "
					"to function `%s'.\n", Fname, Lc, channel,
					Function_Names[ c->function->self ] );
			return FAIL;
		}

		eprint( FATAL, "%s:%ld: DG2020: Channel %ld is already used for "
				"function `%s'.\n", Fname, Lc, channel,
				Function_Names[ c->function->self ] );
		THROW( EXCEPTION );
	}

	f->is_used = SET;
	f->channel[ f->num_channels++ ] = c;

	c->function = f;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool invert_function( int function )
{
	dg2020.function[ function ].is_used = SET;
	dg2020.function[ function ].is_inverted = SET;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_delay_function( int function, double delay )
{
	if ( dg2020.function[ function ].is_delay )
	{
		eprint( FATAL, "%s:%ld: DG2020: Delay for function `%s' has already "
				"been set.\n", Fname, Lc, Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	/* check that the delay value is reasonable */

	if ( delay < 0 )
	{
		eprint( FATAL, "%s:%ld: Invalid negative delay: %s\n", Fname, Lc,
				ptime( delay ) );
		THROW( EXCEPTION );
	}

	dg2020.function[ function ].delay = double2ticks( delay );
	dg2020.function[ function ].is_used = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_function_high_level( int function, double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_HIGH_VOLTAGE || voltage > MAX_POD_HIGH_VOLTAGE )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid high level of %g V for "
				"function `%s', valid range is %g V to %g V.\n", Fname, Lc,
				voltage, Function_Names[ function ], MIN_POD_HIGH_VOLTAGE,
				MAX_POD_HIGH_VOLTAGE );
		THROW( EXCEPTION );
	}
				
	if ( dg2020.function[ function ].is_low_level )
		check_pod_level_diff( voltage, dg2020.function[ function ].low_level );

	dg2020.function[ function ].high_level = voltage;
	dg2020.function[ function ].is_high_level = SET;
	dg2020.function[ function ].is_used = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_function_low_level( int function, double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_LOW_VOLTAGE || voltage > MAX_POD_LOW_VOLTAGE )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid low level of %g V for "
				"function `%s', valid range is %g V to %g V.\n", Fname, Lc,
				voltage, Function_Names[ function ], MIN_POD_LOW_VOLTAGE,
				MAX_POD_LOW_VOLTAGE );
		THROW( EXCEPTION );
	}
				
	if ( dg2020.function[ function ].is_high_level )
		check_pod_level_diff( dg2020.function[ function ].high_level,
							  voltage );

	dg2020.function[ function ].low_level = voltage;
	dg2020.function[ function ].is_low_level = SET;
	dg2020.function[ function ].is_used = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_trigger_mode( int mode )
{
	assert( mode == INTERNAL || mode == EXTERNAL );

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode != mode )
	{
		eprint( FATAL, "%s:%ld: DG2020: Trigger mode has already been set to "
				"%s.\n", Fname, Lc,
				mode == EXTERNAL ? "INTERNAL" : "EXTERNAL" );
		THROW( EXCEPTION );
	}

	if ( mode == INTERNAL )
	{
		if ( dg2020.is_trig_in_slope )
		{
			eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
					"a trigger slope is incompatible.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_trig_in_level )
		{
			eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
					"a trigger level is incompatible.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( dg2020.is_repeat_time )
		{
			eprint( FATAL, "%s:%ld: DG2020: External trigger mode and setting "
					"a repeat time or frequency is incompatible.\n",
					Fname, Lc );
			THROW( EXCEPTION );
		}
	}

	dg2020.trig_in_mode = mode;
	dg2020.is_trig_in_mode = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_trig_in_level( double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( dg2020.is_trig_in_level && dg2020.trig_in_level != voltage )
	{
		eprint( FATAL, "%s:%ld: DG2020: A different level of %g V for the "
				"trigger has already been set.\n", Fname, Lc,
				dg2020.trig_in_level );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
				"a trigger level is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_repeat_time )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time as well as "
				"a trigger level is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( voltage > MAX_TRIG_IN_LEVEL || voltage < MIN_TRIG_IN_LEVEL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid level for trigger of %g V, "
				"valid range is %g V to %g V.\n", Fname, Lc, MIN_TRIG_IN_LEVEL,
				MAX_TRIG_IN_LEVEL );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_level = voltage;
	dg2020.is_trig_in_level = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_trig_in_slope( int slope )
{
	assert( slope == POSITIVE || slope == NEGATIVE );

	if ( dg2020.is_trig_in_slope && dg2020.trig_in_slope != slope )
	{
		eprint( FATAL, "%s:%ld: DG2020: A different trigger slope (%s) "
				"has already been set.\n", Fname, Lc,
				slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
				"a trigger slope is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_repeat_time )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time as well as "
				"a trigger slope is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_slope = slope;
	dg2020.is_trig_in_slope = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_repeat_time( double time )
{
	if ( dg2020.is_repeat_time && dg2020.repeat_time != double2ticks( time ) )
	{
		eprint( FATAL, "%s:%ld: DG2020: A different repeat time/frequency of "
				"%s / %g Hz has already been set.\n", Fname, Lc,
				pticks( dg2020.repeat_time ),
				1.0 / ticks2double( dg2020.repeat_time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == EXTERNAL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time/frequency and "
				"trigger mode to internal is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_slope || dg2020.is_trig_in_level )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time/frequency and "
				"a trigger slope or level is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( time <= 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid repeat time %s.\n",
				Fname, Lc, ptime( time ) );
		THROW( EXCEPTION );
	}


	dg2020.repeat_time = double2ticks( time );
	dg2020.is_repeat_time = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_phase_reference( int phase, int function )
{
	FUNCTION *p, *f;

	p = &dg2020.function[ phase ];
	f = &dg2020.function[ function ];

	if ( p->phase_func != NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Phase function `%s' has already been "
				"associated with function `%s'.\n", Fname, Lc,
				Function_Names[ p->self ],
				Function_Names[ p->phase_func->self ] );
		THROW( EXCEPTION );
	}

	if ( f->phase_func != NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Function `%s' has already been "
				"associated with phase function `%s'.\n", Fname, Lc,
				Function_Names[ f->self ],
				Function_Names[ f->phase_func->self ] );
				THROW( EXCEPTION );
	}

	p->phase_func = f;
	f->phase_func = p;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool new_pulse( long pnum )
{
	PULSE *cp = Pulses;
	PULSE *lp;


	while ( cp != NULL )
	{
		if ( cp->num == pnum )
		{
			eprint( FATAL, "%s:%ld: DG2020: Can't create pulse with number "
					"%ld, it already exists.\n", Fname, Lc, pnum );
			THROW( EXCEPTION );
		}
		lp = cp;
		cp = cp->next;
	}

	cp = T_malloc( sizeof( PULSE ) );

	if ( Pulses == NULL )
	{
		Pulses = cp;
		cp->prev = NULL;
	}
	else
	{
		cp->prev = lp;
		lp->next = cp;
	}

	cp->next = NULL;
	cp->pc = NULL;

	cp->num = pnum;
	cp->is_function = UNSET;
	cp->is_pos = UNSET;
	cp->is_len = UNSET;
	cp->is_dpos = UNSET;
	cp->is_dlen = UNSET;
	cp->is_maxlen = UNSET;
	cp->num_repl = 0;
	cp->is_a_repl = UNSET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_pulse_function( long pnum, int function )
{
	PULSE *p = get_pulse( pnum );


	if ( p->is_function )
	{
		eprint( FATAL, "%s:%ld: DG2020: The function of pulse %ld has already "
				"been set to `%s'.\n", Fname, Lc, pnum,
				Function_Names[ p->function->self ] );
		THROW( EXCEPTION );
	}

	if ( function == PULSER_CHANNEL_PHASE_1 || 
		 function == PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, "%s:%ld: You can't set pulses for the PHASE function, "
				"all pulses needed will be created automatically.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	p->function = &dg2020.function[ function ];
	p->is_function = SET;
	p->function->is_needed = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_pulse_position( long pnum, double time )
{
	PULSE *p = get_pulse( pnum );


	if ( p->is_pos )
	{
		eprint( FATAL, "%s:%ld: DG2020: The start position of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pnum,
				pticks( p->pos ) );
		THROW( EXCEPTION );
	}

	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid (negative) start position "
				"for pulse %ld: %s.\n", Fname, Lc, pnum, ptime( time ) );
		THROW( EXCEPTION );
	}

	p->pos = double2ticks( time );
	p->is_pos = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_pulse_length( long pnum, double time )
{
	PULSE *p = get_pulse( pnum );

	if ( p->is_len )
	{
		eprint( FATAL, "%s:%ld: DG2020: The length of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pnum,
				pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( time <= 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid length for pulse "
				"%ld: %s.\n", Fname, Lc, pnum, ptime( time ) );
		THROW( EXCEPTION );
	}

	p->len = double2ticks( time );
	p->is_len = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_pulse_position_change( long pnum, double time )
{
	PULSE *p = get_pulse( pnum );


	if ( p->is_dpos )
	{
		eprint( FATAL, "%s:%ld: DG2020: The position change of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pnum,
				pticks( p->dpos ) );
		THROW( EXCEPTION );
	}

	p->dpos = double2ticks( time );
	p->is_dpos = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_pulse_length_change( long pnum, double time )
{
	PULSE *p = get_pulse( pnum );


	if ( p->is_dlen )
	{
		eprint( FATAL, "%s:%ld: DG2020: The length change of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pnum,
				pticks( p->len ) );
		THROW( EXCEPTION );
	}

	p->dlen = double2ticks( time );
	p->is_dlen = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_pulse_phase_cycle( long pnum, int cycle )
{
	PULSE *p = get_pulse( pnum );
	Phase_Sequence *pc = PSeq;


	if ( p->pc != NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Pulse %ld has already been assigned "
				"a phase cycle.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	while ( pc != NULL )
	{
		if ( pc->num == cycle )
			break;
		pc = pc->next;
	}

	if ( pc == NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Referenced phase sequence %d hasn't "
				"been defined.\n", Fname, Lc, cycle );
		THROW( EXCEPTION );
	}

	p->pc = pc;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_pulse_maxlen( long pnum, double time )
{
	PULSE *p = get_pulse( pnum );


	if ( p->is_maxlen )
	{
		eprint( FATAL, "%s:%ld: DG2020: The maximum length of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pnum,
				pticks( p->maxlen ) );
		THROW( EXCEPTION );
	}

	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid (negative) maximum length for "
				"pulse %ld: %s.\n", Fname, Lc, pnum, ptime( time ) );
		THROW( EXCEPTION );
	}

	p->maxlen = double2ticks( time );
	p->is_maxlen = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_pulse_replacements( long pnum, long num_repl, long *repl_list )
{
	PULSE *p = get_pulse( pnum );
	long i;


	if ( p->num_repl )
	{
		eprint( FATAL, "%s:%ld: DG2020: Replacement pulses for pulse %ld "
				"have already been set.\n", Fname, Lc, pnum );
		THROW( EXCEPTION );
	}

	/* Make sure the pulse isn't replaced by itself */

	for ( i = 0; i < num_repl; i++ )
		if ( repl_list[ i ] == pnum )
		{
			eprint( FATAL, "%s:%ld: DG2020: Pulse %ld can't be replaced by "
					"itself (see %ld. replacement pulse).\n",
					Fname, Lc, pnum, i + 1 );
			THROW( EXCEPTION );
		}

	p->num_repl = num_repl;
	p->repl_list = get_memcpy( repl_list, num_repl * sizeof( long ) );

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool get_pulse_function( long pnum, int *function )
{
	PULSE *p = get_pulse( pnum );


	if ( ! p->is_function )
	{
		eprint( FATAL, "%s:%ld: DG2020: The function of pulse %ld hasn't "
				"been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*function = p->function->self;
	return OK;
}

/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool get_pulse_position( long pnum, double *time )
{
	PULSE *p = get_pulse( pnum );


	if ( ! p->is_pos )
	{
		eprint( FATAL, "%s:%ld: DG2020: The start position of pulse %ld "
				"hasn't been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*time = ticks2double( p->pos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool get_pulse_length( long pnum, double *time )
{
	PULSE *p = get_pulse( pnum );


	if ( ! p->is_len )
	{
		eprint( FATAL, "%s:%ld: DG2020: The length of pulse %ld hasn't "
				"been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*time = ticks2double( p->len );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool get_pulse_position_change( long pnum, double *time )
{
	PULSE *p = get_pulse( pnum );


	if ( ! p->is_dpos )
	{
		eprint( FATAL, "%s:%ld: DG2020: The position change of pulse %ld "
				"hasn't been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*time = ticks2double( p->dpos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool get_pulse_length_change( long pnum, double *time )
{
	PULSE *p = get_pulse( pnum );


	if ( ! p->is_dlen )
	{
		eprint( FATAL, "%s:%ld: DG2020: The length change of pulse %ld hasn't "
				"been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*time = ticks2double( p->dlen );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool get_pulse_phase_cycle( long pnum, int *cycle )
{
	PULSE *p = get_pulse( pnum );


	if ( p->pc == NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: No phase cycle has been set for "
				"pulse %ld.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*cycle = p->pc->num;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool get_pulse_maxlen( long pnum, double *time )
{
	PULSE *p = get_pulse( pnum );


	if ( ! p->is_maxlen )
	{
		eprint( FATAL, "%s:%ld: DG2020: The maximum length of pulse %ld "
				"hasn't been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*time = ticks2double( p->maxlen );
	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static bool setup_phase( int func, PHS phs )
{
	assert( func == PULSER_CHANNEL_PHASE_1 || func == PULSER_CHANNEL_PHASE_2 );

	if ( dg2020.function[ func ].is_phs )
	{
		eprint( WARN, "%s:%ld: DG2020: Phase setup for function `%s' has "
				"already been done.\n", Fname, Lc, Function_Names[ func ] );
		return FAIL;
	}

	memcpy( &dg2020.function[ func].phs, &phs, sizeof( PHS ) );
	dg2020.function[ func ].is_phs = SET;

	return OK;
}

/*-----------------------------------------------------------------*/
/* Converts a time into the internal type of a time specification, */
/* i.e. a integer multiple of the time base                        */
/*-----------------------------------------------------------------*/

static Ticks double2ticks( double time )
{
	double ticks;


	if ( ! dg2020.is_timebase )
	{
		eprint( FATAL, "%s:%ld: DG2020: Can't set a time because no pulser "
				"time base has been set.\n",Fname, Lc );
		THROW( EXCEPTION );
	}

	ticks = time / dg2020.timebase;

	if ( fabs( ticks - lround( ticks ) ) > 1.0e-2 )
	{
		char *t = get_string_copy( ptime( time ) );
		eprint( FATAL, "%s:%ld: DG2020: Specified time of %s is not an "
				"integer multiple of the pulser time base of %s.\n",
				Fname, Lc, t, ptime( dg2020.timebase ) );
		T_free( t );
		THROW( EXCEPTION );
	}

	return ( Ticks ) lround( ticks );
}


/*-----------------------------------------------------*/
/* Does the exact opposite of the previous function... */
/*-----------------------------------------------------*/

static double ticks2double( Ticks ticks )
{
	assert( dg2020.is_timebase );
	return ( double ) ( dg2020.timebase * ticks );
}


/*----------------------------------------------------------------------*/
/* Checks if the difference of the levels specified for a pod connector */
/* are within the valid limits.                                         */
/*----------------------------------------------------------------------*/

static void check_pod_level_diff( double high, double low )
{
	if ( low > high )
	{
		eprint( FATAL, "%s:%ld: DG2020: Low voltage level is above high "
				"level, instead use keyword INVERT to invert the polarity.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( high - low > MAX_POD_VOLTAGE_SWING + 0.01 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Difference between high and low "
				"voltage of %g V is too big, maximum is %g V.\n", Fname, Lc,
				high - low, MAX_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}

	if ( high - low < MIN_POD_VOLTAGE_SWING - 0.01 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Difference between high and low "
				"voltage of %g V is too small, minimum is %g V.\n", Fname, Lc,
				high - low, MIN_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static PULSE *get_pulse( long pnum )
{
	PULSE *cp = Pulses;


	while ( cp != NULL )
	{
		if ( cp->num == pnum )
			break;
		cp = cp->next;
	}

	if ( cp == NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Referenced pulse %ld does not "
				"exist.\n", Fname, Lc, pnum );
		THROW( EXCEPTION );
	}

	return cp;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static const char *ptime( double time )
{
	static char buffer[ 128 ];

	if ( fabs( time ) >= 1.0 )
		sprintf( buffer, "%g s", time );
	else if ( fabs( time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * time );
	else if ( fabs( time ) >= 1.e-6 )
		sprintf( buffer, "%g us", 1.e6 * time );
	else
		sprintf( buffer, "%g ns", 1.e9 * time );

	return buffer;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static const char *pticks( Ticks ticks )
{
	return ptime( ticks2double( ticks ) );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void check_consistency( void )
{
	basic_pulse_check( );
	basic_functions_check( );
	distribute_channels( );
	pulse_start_setup( );
}



/*--------------------------------------------------------------------------
  Function runs through all pulses and checks that at least:
  1. the function is set and the function itself has been declared in the
     ASSIGNMENTS section
  2. the start position is set
  3. the length is set (only exception: if pulse function is DETECTION
     and no length is set it's more or less silently set to one tick)
  4. the sum of function delay, pulse start position and length does not
     exceed the pulsers memory
  5. that, if a maximum length is set, it's larger than the original length
     and there are replacement pulses
  6. if the pulse needs phase cycling its function is associated with a
     pulse function and the pulse has no replacement pulses
  7. if the pulse has replacement pulses check that 
     a. also a maximum length is set,
     b. the replacement pulses exist and
	    have the same function and that
	    dont't have replacement pulses on their own and
		don't need phase cycling.
--------------------------------------------------------------------------*/

static void basic_pulse_check( void )
{
	PULSE *p, *cp;
	int i;


	for ( p = Pulses; p != NULL; p = p->next )
	{
		/* Check the pulse function */

		if ( ! p->is_function )
		{
			eprint( FATAL, "DG2020: Pulse %ld has no function assigned to"
					"it.\n", p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->function->is_used )
		{
			eprint( FATAL, "DG2020: The function `%s' of pulse %ld hasn't "
					"been declared in the ASSIGNMENTS section.\n",
					Function_Names[ p->function->self ], p->num );
			THROW( EXCEPTION );
		}

		/* Check the start position */

		if ( ! p->is_pos )
		{
			eprint( FATAL, "DG2020: No start position has been set for "
					"pulse %ld.\n", p->num );
			THROW( EXCEPTION );
		}

		/* Check the pulse length */

		if ( ! p->is_len )
		{
			if ( p->function == &dg2020.function[ PULSER_CHANNEL_DET ] )
			{
				eprint( WARN, "DG2020: Length of detection pulse %ld is being "
						"set to %s\n", p->num, ptime( 1 ) );
				p->len = 1;
				p->is_len = SET;
			}
			else
			{
				eprint( FATAL, "DG2020: Length of pulse %ld has not been "
						"set.\n", p->num );
				THROW( EXCEPTION );
			}
		}

		/* Check that the pulse fits into the pulsers memory
		   (If you check the following line real carefully, you will find that
		   one less than the number of bits in the pulser channel is allowed -
		   this is due to a bug in the pulsers firmware: if the very first bit
		   in any of the channels is set to high the pulser outputs a pulse of
		   250 us length before starting to output the real data in the
		   channel, thus the first bit has always to be set to low, i.e. must
		   be unused. But this is only the 'best' case when the pulser is used
		   in repeat mode, in single mode setting the first bit of a channel
		   leads to an obviously endless high pulse, while not setting the
		   first bit keeps the pulser from working at all...) */

		if ( p->pos + p->len + p->function->delay >= MAX_PULSER_BITS )
		{
			eprint( FATAL, "DG2020: Pulse %ld does not fit into the pulsers "
					"memory. Maybe, you could try a longer pulser time "
					"base.\n", p->num );
			THROW( EXCEPTION );
		}

		/* Check the maximum length */

		if ( p->is_maxlen )
		{
			if ( p->maxlen <= p->len )
			{
				eprint( FATAL, "DG2020: For pulse %ld a maximum length has "
						"been set that isn't longer than the start length.\n",
						p->num );
				THROW( EXCEPTION );
			}

			if ( p->num_repl == 0 )
			{
				eprint( FATAL, "DG2020: For pulse %ld a maximum length has "
						"been set but no replacement pulses are defined.\n",
						p->num );
				THROW( EXCEPTION );
			}
		}

		/* Check phase cycling of pulse */

		if ( p->pc )
		{
			if ( p->function->phase_func == NULL )
			{
				eprint( FATAL, "DG2020: Pulse %ld needs phase cycling but its "
						"function (%s) isn't associated with a phase "
						"function.\n", p->num,
						Function_Names[ p->function->self ] );
				THROW( EXCEPTION );
			}

			if ( p->num_repl != 0 )
			{
				eprint( FATAL, "DG2020: Pulse %ld needs replacement pulses "
						"and phase cycling. This isn't implemented (yet?).\n",
						p->num );
				THROW( EXCEPTION );
			}
		}

		/* Check the replacement pulse settings */

		if ( p->num_repl != 0 )
		{
			/* No maximum length - no replacement pulses... */

			if ( ! p->is_maxlen )
			{
				eprint( FATAL, "DG2020: For pulse %ld has replacement pulses "
						"but no maximum length is set.\n", p->num );
				THROW( EXCEPTION );
			}

			/* Check the replacement pulses */

			for ( i = 0; i < p->num_repl; i++ )
			{
				cp = Pulses;         /* try to find the i. replacement pulse */
				while ( cp != NULL )
				{
					if ( cp->num == p->repl_list[ i ] )
						break;
					cp = cp->next;
				}

				if ( cp == NULL )             /* replacement pulse not found */
				{
					eprint( FATAL, "DG2020: The %d. replacement pulse (%ld) "
							"of pulse %ld does not exist.\n",
							i + 1, p->repl_list[ i ], p->num );
					THROW( EXCEPTION );
				}

				if ( p->function != cp->function )
				{
					eprint( FATAL, "DG2020: Pulse %ld and its %d. replacement "
							"pulse %ld have different functions.\n",
							p->num, i + 1, cp->num );
					THROW( EXCEPTION );
				}

				if ( cp->num_repl != 0 )
				{
					eprint( FATAL, "DG2020: Pulse %ld is the %d. replacement "
							"pulse for pulse %ld, so it can't have "
							"replacement pulses of its own.\n",
							cp->num, i + 1, p->num );
					THROW( EXCEPTION );
				}

				if ( cp->pc != NULL )
				{
					eprint( FATAL, "DG2020: Pulse %ld is the %d. replacement "
							"pulse of pulse %ld and needs phase cycling. This "
							"isn't implemented (yet?).\n",
							cp->num, i + 1, p->num );
					THROW( EXCEPTION );
				}

				cp->is_a_repl = SET;
			}
		}
	}
}


/*-----------------------------------------------------------------------------
  Function does a consistency check concerning the functions:
  1. Each function mentioned in the ASSIGMENTS section should have pulses
     assigned to it, otherwise it's not really needed. The exceptions
	 are the PHASES functions where the pulses are created automatically. On
	 the other hand, the PHASES functions aren't needed if no phase sequences
	 have been defined.
  2. Each function needs a pod again with the exception of the PHASES
     functions that need two.
  Phase functions that are associated with useless functions or functions
  that don't have pulses with phase cycling have to be removed.
  Next, a list of pulses for each channel is set up. Now we also can count
  the number of channels needed for the function.
  Each time when we find that a function isn't needed we always have to put
  back all channels that might have been assigned to the function. Also, if
  there are more channels assigned than needed, the superfluous ones are put
  back into the free pool.
-----------------------------------------------------------------------------*/

static void basic_functions_check( void )
{
	int i, j;
	FUNCTION *f;
	PULSE *cp;
	bool need_phases;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Don't do anything if the function has never been mentioned */

		if ( ! f->is_used )
			continue;

		/* Check if the function that has been defined is really needed (i.e.
		   has pulses assigned to it) - exceptions are the PHASE functions
		   that get pulses assigned to them automatically and that are only
		   needed if pulse sequences have been defined. If a function isn't
		   needed also put back into the pool the channels that may have been
		   assigned to the function. */

		if ( f->self != PULSER_CHANNEL_PHASE_1 &&
			 f->self != PULSER_CHANNEL_PHASE_2 )
		{
			/* Function has no pulses ? */

			if ( f->is_used && ! f->is_needed )
			{
				eprint( WARN, "DG2020: No pulses have been assigned to "
						"function `%s'.\n", Function_Names[ i ] );
				f->is_used = UNSET;

				for ( j = 0; j < f->num_channels; j++ )
				{
					f->channel[ j ]->function = NULL;
					f->channel[ j ] = NULL;
				}
				f->num_channels = 0;

				continue;
			}
		}
		else
		{
			/* No phase sequences have been defined ? */

			if ( PSeq == NULL )
			{
				eprint( WARN, "DG2020: Phase functions `%s' isn't be needed, "
						"because no phase sequences have been defined.\n",
						Function_Names[ f->self ] );
				f->is_used = UNSET;

				for ( j = 0; j < f->num_channels; j++ )
				{
					f->channel[ j ]->function = NULL;
					f->channel[ j ] = NULL;
				}
				f->num_channels = 0;

				continue;
			}

			/* Phase function isn't associated with a function ? */

			if ( f->phase_func == NULL )
			{
				eprint( WARN, "DG2020: Phase function `%s' isn't be needed, "
						"because it's not associated with a function.\n",
						Function_Names[ f->self ] );
				f->is_used = UNSET;

				for ( j = 0; j < f->num_channels; j++ )
				{
					f->channel[ j ]->function = NULL;
					f->channel[ j ] = NULL;
				}
				f->num_channels = 0;

				continue;
			}

			/* No secoond pod assigned to phase function ? */

			if ( f->pod2 == NULL)
			{
				eprint( FATAL, "DG2020: Function `%s' needs two pods "
						"assigned to it.\n", Function_Names[ i ] );
				THROW( EXCEPTION );
			}

			/* No phase setup has been done for the function */

			if ( ! f->is_phs )
			{
				eprint( FATAL, "DG2020: Missing data on how to convert the "
						"pulse phases into pod outputs for function `%s'.\n",
						Function_Names[ i ] );
				THROW( EXCEPTION );
			}
		}

		/* Make sure there's a pod assigned to the function */

		if ( f->pod == NULL )
		{
			eprint( FATAL, "DG2020: No pod has been assigned to "
					"function `%s'.\n", Function_Names[ i ] );
			THROW( EXCEPTION );
		}

		/* Assemble a list of all pulses assigned to the function and, while
		   doing so, also count the number of channels really needed */

		if ( f->self != PULSER_CHANNEL_PHASE_1 &&
			 f->self != PULSER_CHANNEL_PHASE_2 )
		{
			f->num_needed_channels = 1;
			need_phases = UNSET;

			for ( cp = Pulses; cp != NULL; cp = cp->next )
			{
				if ( cp->function != f )
					continue;

				f->num_pulses++;
				f->pulses = T_realloc( f->pulses,
									   f->num_pulses * sizeof( PULSE * ) );
				f->pulses[ f->num_pulses - 1 ] = cp;
				
				if ( cp->pc )
					need_phases = SET;

				if ( cp->num_repl != 0 )
					f->num_needed_channels = 2;
			}

			/* If none of the pulses needs phase cycling but a phase function
			   is associated with the function remove the association and put
			   all channels of the phase function back into the pool (we don't
			   need to check for the case that some pulses need phase cycling
			   but no phase function is associated with the function, because
			   this is already tested in the basic pulse check*/

			if ( need_phases && f->num_needed_channels == 2 )
			{
				eprint( FATAL, "DG2020: Some of the pulses of function `%s' "
						"need phase cycling while others need replacement "
						"pulses. This is not implemented (yet?).\n",
						Function_Names[ f->self ] );
				THROW( EXCEPTION );
			}

			if ( ! need_phases && f->phase_func != NULL )
			{
				eprint( WARN, "DG2020: Function `%s' is associated with phase "
						"function `%s' but none of its pulses needs phase "
						"cycling.\n", Function_Names[ f->self ],
						Function_Names[ f->phase_func->self ] );

				f->phase_func->is_used = UNSET;
				for ( j = 0; j < f->phase_func->num_channels; j++ )
				{
					f->phase_func->channel[ j ]->function = NULL;
					f->phase_func->channel[ j ] = NULL;
				}
				f->phase_func->num_channels = 0;
			}

			assert( f->num_pulses > 0 );    /* paranoia ? */
		}
		else
			f->num_needed_channels = PSeq->len;

		/* Put channels not needed back into the pool */

		if ( f->num_channels > f->num_needed_channels )
			eprint( WARN, "DG2020: For function `%s' only %d channel%s needed "
					"instead of the %d assigned ones.\n",
					Function_Names[ i ], f->num_needed_channels,
					f->num_needed_channels == 1 ? " is" : "s are",
					f->num_channels );

		while ( f->num_channels > f->num_needed_channels )
		{
			f->channel[ --f->num_channels ]->function = NULL;
			f->channel[ f->num_channels ] = NULL;
		}
	}

	/* Now we've got to run a second time through the pulses. We put back
       channels for functons that we found to be useless and also the channels
	   for phase functions associated with useless functions */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		if ( f->is_used )
			continue;

		for ( j = 0; j < f->num_channels; j++ )
		{
			f->channel[ j ]->function = NULL;
			f->channel[ j ] = NULL;
		}
		f->num_channels = 0;

		if ( f->self != PULSER_CHANNEL_PHASE_1 &&
			 f->self != PULSER_CHANNEL_PHASE_2 &&
			 f->phase_func != NULL )
		{
			eprint( WARN, "DG2020: Phase function `%s' isn't needed because "
					"function `%s' it is associated with is not used.\n",
					Function_Names[ f->phase_func->self ],
					Function_Names[ f->self ] );

			for ( j = 0; j < f->phase_func->num_channels; j++ )
			{
				f->phase_func->channel[ j ]->function = NULL;
				f->phase_func->channel[ j ] = NULL;
			}
			f->phase_func->num_channels = 0;

			f->phase_func->is_used = UNSET;
		}
	}
}


/*-----------------------------------------------------------------------------
  Function checks first if there are enough pulser channels and than assignes
  channels to funcions that haven't been assigned as many as needed
-----------------------------------------------------------------------------*/

static void distribute_channels( void )
{
	int i;
	FUNCTION *f;


	/* First count the number of channels we really need for the experiment */

	dg2020.needed_channels = 0;
	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];
		if ( ! f->is_used )
			continue;
		dg2020.needed_channels += f->num_needed_channels;
	}

	/* Now we've got to check if there are at least as many channels as are
	   needed for the experiment */

	if ( dg2020.needed_channels > MAX_CHANNELS )
	{
		eprint( FATAL, "DG2020: Running the experiment would require %d "
				"pulser channels but only %d are available.\n",
				dg2020.needed_channels, ( int ) MAX_CHANNELS );
		THROW( EXCEPTION );
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Don't do anything for unused functions */

		if ( ! f->is_used )
			continue;

		/* If the function needs more channels than it got assigned get them
		   now from the pool of free channels */

		while ( f->num_channels < f->num_needed_channels )
		{
			f->channel[ f->num_channels ] = get_next_free_channel( );
			f->channel[ f->num_channels ]->function = f;
			f->num_channels++;
		}
	}
}


/*-----------------------------------------------------------------------------
   Functions returns a pointer to a free channel - the channels with the
   highest numbers are used first since most users probably tend to use the
   low number channels for storing test pulse sequences that they don't like
   too much being overwritten just because they forgot to set a channel-to-
   function-assignment in their EDL program.
-----------------------------------------------------------------------------*/

static CHANNEL *get_next_free_channel( void )
{
	int i = MAX_CHANNELS - 1;

	while ( dg2020.channel[ i ].function != NULL )
		i--;

	assert( i >= 0 );                 /* this can't happen ;-) */

	return &dg2020.channel[ i ];
}


/*-----------------------------------------------------------------------------
  In this function the pulses for all the functions are sorted and further
  consistency checks are done.
-----------------------------------------------------------------------------*/

static void pulse_start_setup( void )
{
	int i, j;
	FUNCTION *f;
	PULSE *p;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		/* Set the inital state values of all pulses */

		for ( j = 0; j < f->num_pulses; j++ )
		{
			p = f->pulses[ j ];
			p->initial_pos = p->pos;
			p->initial_len = p->len;
			p->initial_dpos = p->dpos;
			p->initial_dlen = p->dlen;
		}

		/* Now sort the pulses of this channel by their start times, move
		   replacement pulses to the end of the list */

		qsort( f->pulses, f->num_pulses, sizeof( PULSE * ), start_compare );

		/* Check that the relevant pulses are separated */

		for ( j = 0; j < f->num_pulses; j++ )
		{
			p = f->pulses[ j ];
			if ( j + 1 == f->num_pulses || f->pulses[ j + 1 ]->is_a_repl )
				break;
			if ( p->len + p->pos >= f->pulses[ j + 1 ]->pos )
			{
				eprint( FATAL, "DG2020: Pulses %ld and %ld %s.\n",
						p->num, f->pulses[ j + 1 ]->num,
						p->len + p->pos == f->pulses[ j + 1 ]->pos ?
						"are not separated" : "overlap");
				THROW( EXCEPTION );
			}
		}
	}
}


static int start_compare( const void *A, const void *B )
{
	PULSE *a = *( PULSE ** ) A,
		  *b = *( PULSE ** ) B;

	if ( a->is_a_repl )
	{
		if ( b->is_a_repl )
			return 0;
		else
			return 1;
	}

	if ( b->is_a_repl || a->pos <= b->pos )
		return -1;

	return 1;
}
