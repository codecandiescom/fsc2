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


	/* The very first thing we have to do in the init_hook is to set up the
	   global structure for pulsers. */

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

	pulser_struct.new_pulse = new_pulse;
	pulser_struct.set_pulse_function = set_pulse_function;
	pulser_struct.set_pulse_position = set_pulse_position;
	pulser_struct.set_pulse_length = set_pulse_length;
	pulser_struct.set_pulse_position_change = set_pulse_position_change;
	pulser_struct.set_pulse_length_change = set_pulse_length_change;
	pulser_struct.set_pulse_maxlen = set_pulse_maxlen;
	pulser_struct.set_pulse_replacements = set_pulse_replacements;

	pulser_struct.get_pulse_function = get_pulse_function;
	pulser_struct.get_pulse_position = get_pulse_position;
	pulser_struct.get_pulse_length = get_pulse_length;
	pulser_struct.get_pulse_position_change = get_pulse_position_change;
	pulser_struct.get_pulse_length_change = get_pulse_length_change;
	pulser_struct.get_pulse_maxlen = get_pulse_maxlen;


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
		dg2020.function[ i ].num_channels = 0;
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

	dg2020.repeat_time = double2ticks( time );
	dg2020.is_repeat_time = SET;

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

	cp->num = pnum;
	cp->is_function = UNSET;
	cp->is_pos = UNSET;
	cp->is_len = UNSET;
	cp->is_dpos = UNSET;
	cp->is_dlen = UNSET;
	cp->is_maxlen = UNSET;
	cp->num_repl = 0;

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

	p->maxlen = double2ticks( time );
	p->is_maxlen = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static bool set_pulse_replacements( long pnum, long num_repl, long *repl_list )
{
	PULSE *p = get_pulse( pnum );


	if ( p->num_repl )
	{
		eprint( FATAL, "%s:%ld: DG2020: Replacement pulses for pulse %ld "
				"have already been set.\n", Fname, Lc, pnum );
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

	if ( time >= 1.0 )
		sprintf( buffer, "%g s", time );
	else if ( time >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * time );
	else if ( time >= 1.e-6 )
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
	int i;

	/* Check that or each function that's used there's also a pod assigned to
       it */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		if ( dg2020.function[ i ].is_used && ! dg2020.function[ i ].is_needed )
		{
			eprint( WARN, "DG2020: No pulses have been assigned to function "
					"`%s'.\n", Function_Names[ i ] );
		}

		if ( dg2020.function[ i ].is_needed &&
			 dg2020.function[ i ].pod == NULL )
		{
			eprint( FATAL, "DG2020: No pod has been assigned to "
					"function `%s'.\n", Function_Names[ i ] );
			THROW( EXCEPTION );
		}
	}
}
