/*
  $Id$
*/


#include "hfs9000.h"


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_new_pulse( long pnum )
{
	PULSE *cp = hfs9000_Pulses;
	PULSE *lp = NULL;


	while ( cp != NULL )
	{
		if ( cp->num == pnum )
		{
			eprint( FATAL, "%s:%ld: %s: Can't create pulse with number %ld, "
					"it already exists.\n",
					Fname, Lc, pulser_struct.name, pnum );
			THROW( EXCEPTION );
		}
		lp = cp;
		cp = cp->next;
	}

	cp = T_malloc( sizeof( PULSE ) );

	if ( hfs9000_Pulses == NULL )
	{
		hfs9000_Pulses = cp;
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

	cp->is_pos = cp->is_len = cp->is_dpos = cp->is_dlen = UNSET;
	cp->initial_is_pos = cp->initial_is_len = cp->initial_is_dpos
		= cp->initial_is_dlen = UNSET;
	cp->is_old_pos = cp->is_old_len = UNSET;

	cp->channel = NULL;
	cp->needs_update = UNSET;
	cp->has_been_active = cp->was_active = UNSET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_pulse_function( long pnum, int function )
{
	PULSE *p = hfs9000_get_pulse( pnum );
	PULSE *pl = hfs9000_Pulses;


	if ( function == PULSER_CHANNEL_PHASE_1 || 
		 function == PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, "%s:%ld: %s: Phase functions can't be used with this "
				"driver.\n", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( p->is_function )
	{
		eprint( FATAL, "%s:%ld: %s: Function of pulse %ld has already been "
				"set to `%s'.\n", Fname, Lc, pulser_struct.name, pnum,
				Function_Names[ p->function->self ] );
		THROW( EXCEPTION );
	}

	if ( hfs9000.function[ function ].channel == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: No channel has been assigned to function "
				"`%s'.\n", Fname, Lc, pulser_struct.name,
				Function_Names[ hfs9000.function[ function ].self ] );
		THROW( EXCEPTION );
	}

	/* There can be only one pulse for the function associated with the
	   TRIGGER_OUT channel and its length has to be either zero or equal the
	   pulsers timebase */

	if ( hfs9000.function[ function ].channel->self == HFS9000_TRIG_OUT )
	{
		for ( ; pl != NULL; pl = pl->next )
		{
			if ( ! pl->is_function )
				continue;

			if ( pl->function->channel->self == HFS9000_TRIG_OUT )
			{
				eprint( FATAL, "%s:%ld: %s: There can be only one pulse for "
						"the TRIGGER_OUT channel.\n", Fname, Lc,
						pulser_struct.name );
				THROW( EXCEPTION );
			}
		}

		if ( p->is_len && p->len > 1 )
		{
			eprint( FATAL, "%s:%ld: %s: Length of trigger out pulse has to be "
					"either 0 or %s.\n", Fname, Lc, pulser_struct.name,
					hfs9000_ptime( hfs9000.timebase ) );
			THROW( EXCEPTION );
		}

		if ( p->is_dlen )
		{
			eprint( FATAL, "%s:%ld: %s: Trigger out pulses are not allowed to "
					"have a pulse length change set.\n", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}
	}

	p->function = &hfs9000.function[ function ];
	p->channel = p->function->channel;
	p->is_function = SET;
	p->function->is_needed = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_pulse_position( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( p->is_pos )
	{
		eprint( FATAL, "%s:%ld: %s: The start position of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pulser_struct.name,
				pnum, hfs9000_pticks( p->pos ) );
		THROW( EXCEPTION );
	}

	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid (negative) start position for "
				"pulse %ld: %s.\n", Fname, Lc, pulser_struct.name, pnum,
				hfs9000_ptime( time ) );
		THROW( EXCEPTION );
	}

	p->pos = hfs9000_double2ticks( time );
	p->is_pos = SET;

	if ( ! p->initial_is_pos && ! TEST_RUN && I_am == PARENT )
	{
		p->initial_pos = p->pos;
		p->initial_is_pos = SET;
	}
	else if ( ! p->is_old_pos )
	{
		p->old_pos = p->pos;
		p->is_old_pos = SET;
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_pulse_length( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( p->is_len )
	{
		eprint( FATAL, "%s:%ld: %s: Length of pulse %ld has already been set "
				"to %s.\n", Fname, Lc, pulser_struct.name, pnum,
				hfs9000_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( time < 0.0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid negative length set for "
				"pulse %ld: %s.\n",
				Fname, Lc, pulser_struct.name, pnum, hfs9000_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( p->function != NULL && p->function->self == HFS9000_TRIG_OUT &&
		 ( time > 1.01 * HFS9000_TRIG_OUT_PULSE_LEN ||
		   time < 0.99 * HFS9000_TRIG_OUT_PULSE_LEN )
	{
		eprint( FATAL, "%s:%ld: %s: Length of pulse for TRIGGER_OUT can be "
				"only either 0 or %s.\n", Fname, Lc, pulser_struct.name,
				hfs9000_ptime( HFS9000_TRIG_OUT_PULSE_LEN ) );
		THROW( EXCEPTION );
	}

	p->len = hfs9000_double2ticks( time );
	p->is_len = SET;

	if ( ! p->initial_is_len && ! TEST_RUN && I_am == PARENT )
	{
		p->initial_len = hfs9000_double2ticks( time );
		p->initial_is_len = SET;
	}
	else if ( ! p->is_old_len )
	{
		p->old_len = p->len;
		p->is_old_len = SET;
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_pulse_position_change( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( p->is_dpos )
	{
		eprint( FATAL, "%s:%ld: %s: The position change of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pulser_struct.name,
				pnum, hfs9000_pticks( p->dpos ) );
		THROW( EXCEPTION );
	}

	if ( hfs9000_double2ticks( time ) == 0 )
	{
		eprint( SEVERE, "%s:%ld: %s: Zero position change set for pulse "
				"%ld.\n", Fname, Lc, pulser_struct.name, pnum );
		return FAIL;
	}

	p->dpos = hfs9000_double2ticks( time );
	p->is_dpos = SET;

	if ( ! p->initial_is_dpos && ! TEST_RUN && I_am == PARENT )
	{
		p->initial_dpos = hfs9000_double2ticks( time );
		p->initial_is_dpos = SET;
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_pulse_length_change( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( p->is_dlen )
	{
		eprint( FATAL, "%s:%ld: %s: Length change of pulse %ld has already "
				"been set to %s.\n", Fname, Lc, pulser_struct.name,
				pnum, hfs9000_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( hfs9000_double2ticks( time ) == 0 )
	{
		eprint( SEVERE, "%s:%ld: %s: Zero length change set for pulse "
				"%ld.\n", Fname, Lc, pulser_struct.name, pnum );
		return FAIL;
	}

	if ( p->function && p->function->self == HFS9000_TRIG_OUT )
	{
		eprint( FATAL, "%s:%ld: %s: For pulses on TRIG_OUT no pulse length "
				"change can be set.\n", Fname, Lc, pulser_struct.name, pnum );
		THROW( EXCEPTION );
	}

	p->dlen = hfs9000_double2ticks( time );
	p->is_dlen = SET;

	if ( ! p->initial_is_dlen && ! TEST_RUN && I_am == PARENT )
	{
		p->initial_dlen = hfs9000_double2ticks( time );
		p->initial_is_dlen = SET;
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_get_pulse_function( long pnum, int *function )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( ! p->is_function )
	{
		eprint( FATAL, "%s:%ld: %s: The function of pulse %ld hasn't been "
				"set.\n", Fname, Lc, pulser_struct.name, pnum );
		THROW(EXCEPTION );
	}

	*function = p->function->self;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_get_pulse_position( long pnum, double *time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( ! p->is_pos )
	{
		eprint( FATAL, "%s:%ld: %s: The start position of pulse %ld hasn't "
				"been set.\n", Fname, Lc, pulser_struct.name, pnum );
		THROW(EXCEPTION );
	}

	*time = hfs9000_ticks2double( p->pos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_get_pulse_length( long pnum, double *time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( ! p->is_len )
	{
		eprint( FATAL, "%s:%ld: %s: Length of pulse %ld hasn't been set.\n",
				Fname, Lc, pulser_struct.name, pnum );
		THROW(EXCEPTION );
	}

	*time = hfs9000_ticks2double( p->len );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_get_pulse_position_change( long pnum, double *time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( ! p->is_dpos )
	{
		eprint( FATAL, "%s:%ld: %s: The position change of pulse %ld hasn't "
				"been set.\n", Fname, Lc, pulser_struct.name, pnum );
		THROW(EXCEPTION );
	}

	*time = hfs9000_ticks2double( p->dpos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_get_pulse_length_change( long pnum, double *time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( ! p->is_dlen )
	{
		eprint( FATAL, "%s:%ld: %s: Length change of pulse %ld hasn't been "
				"set.\n", Fname, Lc, pulser_struct.name, pnum );
		THROW(EXCEPTION );
	}

	*time = hfs9000_ticks2double( p->dlen );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_change_pulse_position( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid (negative) start position for "
				"pulse %ld: %s.\n", Fname, Lc, pulser_struct.name, pnum,
				hfs9000_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( p->is_pos && hfs9000_double2ticks( time ) == p->pos )
	{
		eprint( WARN, "%s:%ld: %s: Old and new position of pulse %ld are "
				"identical.\n", Fname, Lc, pulser_struct.name, pnum );
		return OK;
	}

	if ( p->is_pos && ! p->is_old_pos )
	{
		p->old_pos = p->pos;
		p->is_old_pos = SET;
	}

	p->pos = hfs9000_double2ticks( time );
	p->is_pos = SET;

	p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
	p->needs_update = NEEDS_UPDATE( p );

	if ( p->needs_update )
		hfs9000.needs_update = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_change_pulse_length( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid (negative) length for pulse %ld: "
				"%s.\n", Fname, Lc, pulser_struct.name, pnum,
				hfs9000_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( p->function->self == HFS9000_TRIG_OUT &&
		 ( time > 1.01 * HFS9000_TRIG_OUT_PULSE_LEN ||
		   time < 0.99 * HFS9000_TRIG_OUT_PULSE_LEN )
	{
		eprint( FATAL, "%s:%ld: %s: Length of pulse for TRIGGER_OUT can only "
				"be either 0 or %s.\n", Fname, Lc, pulser_struct.name,
				hfs9000_ptime( HFS9000_TRIG_OUT_PULSE_LEN ) );
		THROW( EXCEPTION );
	}

	if ( p->is_len && p->len == hfs9000_double2ticks( time ) )
	{
		eprint( WARN, "%s:%ld: %s: Old and new length of pulse %ld are "
				"identical.\n", Fname, Lc, pulser_struct.name, pnum );
		return OK;
	}

	if ( p->is_len && ! p->is_old_len )
	{
		p->old_len = p->len;
		p->is_old_len = SET;
	}

	p->len = hfs9000_double2ticks( time );
	p->is_len = SET;

	p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
	p->needs_update = NEEDS_UPDATE( p );

	if ( p->needs_update )
		hfs9000.needs_update = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_change_pulse_position_change( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( hfs9000_double2ticks( time ) == 0 && TEST_RUN )
	{
		eprint( SEVERE, "%s:%ld: %s: Zero position change value for pulse "
				"%ld.\n", Fname, Lc, pulser_struct.name, pnum );
		return FAIL;
	}

	p->dpos = hfs9000_double2ticks( time );
	p->is_dpos = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_change_pulse_length_change( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( p->function->self == HFS9000_TRIG_OUT )
	{
		eprint( FATAL, "%s:%ld: %s: For pulses on TRIG_OUT no pulse length "
				"change can be set.\n", Fname, Lc, pulser_struct.name, pnum );
		THROW( EXCEPTION );
	}

	if ( hfs9000_double2ticks( time ) == 0 && TEST_RUN )
	{
		eprint( SEVERE, "%s:%ld: %s: Zero length change value for pulse "
				"%ld.\n", Fname, Lc, pulser_struct.name, pnum );
		return FAIL;
	}

	p->dlen = hfs9000_double2ticks( time );
	p->is_dlen = SET;

	return OK;
}
