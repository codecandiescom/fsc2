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
			eprint( FATAL, SET, "%s: Pulse %ld already exists.\n",
					pulser_struct.name, pnum );
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
	FUNCTION *f = &hfs9000.function[ function ];


	if ( function == PULSER_CHANNEL_PHASE_1 || 
		 function == PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, SET, "%s: Phase functions can't be used with this "
				"driver.\n", pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( p->is_function )
	{
		eprint( FATAL, SET, "%s: Function of pulse %ld has already been "
				"set to `%s'.\n", pulser_struct.name, pnum,
				Function_Names[ p->function->self ] );
		THROW( EXCEPTION );
	}

	if ( f->channel == NULL )
	{
		eprint( FATAL, SET, "%s: No channel has been assigned to function "
				"`%s'.\n", pulser_struct.name,
				Function_Names[ hfs9000.function[ function ].self ] );
		THROW( EXCEPTION );
	}

	/* There can be only one pulse for the function associated with Trigger
	   Out and its length is fixed (to 20 ns) */

	if ( f->channel->self == HFS9000_TRIG_OUT )
	{
		for ( ; pl != NULL; pl = pl->next )
		{
			if ( ! pl->is_function || ! pl->function->channel )
				continue;

			if ( pl->function->channel->self == HFS9000_TRIG_OUT )
			{
				eprint( FATAL, SET, "%s: There can be only one pulse for "
						"the Trigger Out channel.\n", pulser_struct.name );
				THROW( EXCEPTION );
			}
		}

		if ( ! p->is_len )
		{
			p->is_len = SET;
			p->len = 1;
			p->initial_is_len = SET;
			p->initial_len = p->len;
		}
		else
		{
			p->len = 1;
			p->initial_len = p->len;
			eprint( WARN, SET, "%s: Length of Trigger Out pulse has been "
					"set to fixed length of %s.\n", pulser_struct.name,
					hfs9000_ptime( HFS9000_TRIG_OUT_PULSE_LEN ) );
		}

		if ( p->is_dlen )
		{
			eprint( FATAL, SET, "%s: Trigger Out pulses are not allowed to "
					"have a pulse length change set.\n", pulser_struct.name );
			THROW( EXCEPTION );
		}
	}

	p->function = f;
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
		eprint( FATAL, SET, "%s: The start position of pulse %ld has "
				"already been set to %s.\n", pulser_struct.name,
				pnum, hfs9000_pticks( p->pos ) );
		THROW( EXCEPTION );
	}

	if ( time < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid (negative) start position for "
				"pulse %ld: %s.\n", pulser_struct.name, pnum,
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
		eprint( FATAL, SET, "%s: Length of pulse %ld has already been set "
				"to %s.\n", pulser_struct.name, pnum,
				hfs9000_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( time < 0.0 )
	{
		eprint( FATAL, SET, "%s: Invalid negative length set for pulse %ld: "
				"%s.\n", pulser_struct.name, pnum, hfs9000_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( p->is_function && p->function->channel &&
		 p->function->channel->self == HFS9000_TRIG_OUT )
	{
		p->len = 1;
		eprint( SEVERE, SET, "%s: Length of Trigger Out pulse %ld is fixed "
				"to 20 ns\n", pulser_struct.name, pnum );
	}
	else
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
		eprint( FATAL, SET, "%s: The position change of pulse %ld has "
				"already been set to %s.\n", pulser_struct.name,
				pnum, hfs9000_pticks( p->dpos ) );
		THROW( EXCEPTION );
	}

	if ( hfs9000_double2ticks( time ) == 0 )
	{
		eprint( SEVERE, SET, "%s: Zero position change set for pulse %ld.\n",
				pulser_struct.name, pnum );
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
		eprint( FATAL, SET, "%s: Length change of pulse %ld has already "
				"been set to %s.\n", pulser_struct.name,
				pnum, hfs9000_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( hfs9000_double2ticks( time ) == 0 )
	{
		eprint( SEVERE, SET, "%s: Zero length change set for pulse "
				"%ld.\n", pulser_struct.name, pnum );
		return FAIL;
	}

	if ( p->is_function && p->function->channel &&
		 p->function->channel->self == HFS9000_TRIG_OUT )
	{
		eprint( FATAL, SET, "%s: Length of Trigger Out pulse %ld can't be "
				"changed.\n", pulser_struct.name, pnum );
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
		eprint( FATAL, SET, "%s: The function of pulse %ld hasn't been "
				"set.\n", pulser_struct.name, pnum );
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
		eprint( FATAL, SET, "%s: The start position of pulse %ld hasn't "
				"been set.\n", pulser_struct.name, pnum );
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
		eprint( FATAL, SET, "%s: Length of pulse %ld hasn't been set.\n",
				pulser_struct.name, pnum );
		THROW(EXCEPTION );
	}

	if ( p->is_function && p->function->channel &&
		 p->function->channel->self == HFS9000_TRIG_OUT )
	{
		eprint( FATAL, SET, "%s: Length of Trigger Out pulse %ld can't "
				"be referenced.\n", pulser_struct.name, pnum );
		THROW( EXCEPTION );
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
		eprint( FATAL, SET, "%s: The position change of pulse %ld hasn't "
				"been set.\n", pulser_struct.name, pnum );
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
		eprint( FATAL, SET, "%s: Length change of pulse %ld hasn't been "
				"set.\n", pulser_struct.name, pnum );
		THROW(EXCEPTION );
	}

	*time = hfs9000_ticks2double( p->dlen );
	return OK;
}


/*-----------------------------------------------------------------------*/
/* Function for changing the pulse position while the experiment is run. */
/*-----------------------------------------------------------------------*/

bool hfs9000_change_pulse_position( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( time < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid (negative) start position for "
				"pulse %ld: %s.\n", pulser_struct.name, pnum,
				hfs9000_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( p->is_pos && hfs9000_double2ticks( time ) == p->pos )
	{
		eprint( WARN, SET, "%s: Old and new position of pulse %ld are "
				"identical.\n", pulser_struct.name, pnum );
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


/*---------------------------------------------------------------------*/
/* Function for changing the pulse length while the experiment is run. */
/*---------------------------------------------------------------------*/

bool hfs9000_change_pulse_length( long pnum, double time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( time < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid (negative) length for pulse %ld: "
				"%s.\n", pulser_struct.name, pnum, hfs9000_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( p->is_len && p->is_function && p->function->channel )
	{
		if ( p->function->channel->self == HFS9000_TRIG_OUT )
		{
			eprint( FATAL, SET, "%s: Length of Trigger Out pulse %ld can't "
					"be changed.\n", pulser_struct.name, pnum );
			THROW( EXCEPTION );
		}

		if ( p->len == hfs9000_double2ticks( time ) )
		{
			eprint( WARN, SET, "%s: Old and new length of pulse %ld are "
					"identical.\n", pulser_struct.name, pnum );
			return OK;
		}
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
		eprint( SEVERE, SET, "%s: Zero position change value for pulse "
				"%ld.\n", pulser_struct.name, pnum );
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


	if ( p->is_function && p->function->channel &&
		 p->function->channel->self == HFS9000_TRIG_OUT )
	{
		eprint( FATAL, SET, "%s: Length change of Trigger Out pulse %ld "
				"can't be set.\n", pulser_struct.name, pnum );
		THROW( EXCEPTION );
	}

	if ( hfs9000_double2ticks( time ) == 0 && TEST_RUN )
	{
		eprint( SEVERE, SET, "%s: Zero length change value for pulse "
				"%ld.\n", pulser_struct.name, pnum );
		return FAIL;
	}

	p->dlen = hfs9000_double2ticks( time );
	p->is_dlen = SET;

	return OK;
}
