/*
  $Id$
*/


#include "dg2020.h"



/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_new_pulse( long pnum )
{
	PULSE *cp = dg2020_Pulses;
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

	if ( dg2020_Pulses == NULL )
	{
		dg2020_Pulses = cp;
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

	cp->is_pos = cp->is_len = cp->is_dpos = cp->is_dlen = UNSET;
	cp->initial_is_pos = cp->initial_is_len = cp->initial_is_dpos
		= cp->initial_is_dlen = UNSET;
	cp->is_old_pos = cp->is_old_len = UNSET;
	
	cp->channel = NULL;
	
	cp->needs_update = UNSET;
	cp->has_been_active = UNSET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_pulse_function( long pnum, int function )
{
	PULSE *p = dg2020_get_pulse( pnum );


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

bool dg2020_set_pulse_position( long pnum, double time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p->is_pos )
	{
		eprint( FATAL, "%s:%ld: DG2020: The start position of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pnum,
				dg2020_pticks( p->pos ) );
		THROW( EXCEPTION );
	}

	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid (negative) start position "
				"for pulse %ld: %s.\n", Fname, Lc, pnum,
				dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}

	p->pos = p->initial_pos = dg2020_double2ticks( time );
	p->is_pos = p->initial_is_pos = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_pulse_length( long pnum, double time )
{
	PULSE *p = dg2020_get_pulse( pnum );

	if ( p->is_len )
	{
		eprint( FATAL, "%s:%ld: DG2020: The length of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pnum,
				dg2020_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( time <= 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid length for pulse "
				"%ld: %s.\n", Fname, Lc, pnum, dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}

	p->len = p->initial_len = dg2020_double2ticks( time );
	p->is_len = p->initial_is_len = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_pulse_position_change( long pnum, double time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p->is_dpos )
	{
		eprint( FATAL, "%s:%ld: DG2020: The position change of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pnum,
				dg2020_pticks( p->dpos ) );
		THROW( EXCEPTION );
	}

	if ( dg2020_double2ticks( time ) == 0 )
	{
		eprint( SEVERE, "%s:%ld: DG2020: Zero position change value for pulse "
				"%ld. Useless value isn't set.\n", Fname, Lc, pnum );
		return FAIL;
	}

	p->dpos = p->initial_dpos = dg2020_double2ticks( time );
	p->is_dpos = p->initial_is_dpos = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_pulse_length_change( long pnum, double time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p->is_dlen )
	{
		eprint( FATAL, "%s:%ld: DG2020: The length change of pulse %ld has "
				"already been set to %s.\n", Fname, Lc, pnum,
				dg2020_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( dg2020_double2ticks( time ) == 0 )
	{
		eprint( SEVERE, "%s:%ld: DG2020: Zero length change value for pulse "
				"%ld. Useless value isn't set.\n", Fname, Lc, pnum );
		return FAIL;
	}

	p->dlen = p->initial_dlen = dg2020_double2ticks( time );
	p->is_dlen = p->initial_is_dlen = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_pulse_phase_cycle( long pnum, int cycle )
{
	PULSE *p = dg2020_get_pulse( pnum );
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

bool dg2020_get_pulse_function( long pnum, int *function )
{
	PULSE *p = dg2020_get_pulse( pnum );


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

bool dg2020_get_pulse_position( long pnum, double *time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( ! p->is_pos )
	{
		eprint( FATAL, "%s:%ld: DG2020: The start position of pulse %ld "
				"hasn't been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*time = dg2020_ticks2double( p->pos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_get_pulse_length( long pnum, double *time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( ! p->is_len )
	{
		eprint( FATAL, "%s:%ld: DG2020: The length of pulse %ld hasn't "
				"been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*time = dg2020_ticks2double( p->len );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_get_pulse_position_change( long pnum, double *time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( ! p->is_dpos )
	{
		eprint( FATAL, "%s:%ld: DG2020: The position change of pulse %ld "
				"hasn't been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*time = dg2020_ticks2double( p->dpos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_get_pulse_length_change( long pnum, double *time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( ! p->is_dlen )
	{
		eprint( FATAL, "%s:%ld: DG2020: The length change of pulse %ld hasn't "
				"been set.\n", Fname, Lc, pnum );
		THROW(EXCEPTION );
	}

	*time = dg2020_ticks2double( p->dlen );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_get_pulse_phase_cycle( long pnum, int *cycle )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p->pc == NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: No phase cycle has been set for "
				"pulse %ld.\n", Fname, Lc, pnum );
		THROW( EXCEPTION );
	}

	*cycle = p->pc->num;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_position( long pnum, double time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid (negative) start position "
				"for pulse %ld: %s.\n", Fname, Lc, pnum,
				dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020_double2ticks( time ) == p->pos )
	{
		eprint( WARN, "%s:%ld: DG2020: Old and new position of pulse %ld are "
				"identical.\n", Fname, Lc, pnum );
		return OK;
	}

	if ( ! p->is_old_pos )
	{
		p->old_pos = p->pos;
		p->is_old_pos = SET;
	}

	p->pos = dg2020_double2ticks( time );

	/* If a previously inactive pulse gets a position set and also has a
	   non-zero length it becomes active */

	if ( ! p->is_pos && p->is_len && p->len > 0 )
		p->is_active = p->has_been_active = SET;

	p->is_pos = SET;

	/* If the pulse is active we've got to update the pulser */

	if ( p->is_active )
	{
		p->needs_update = SET;
		dg2020.needs_update = SET;

		/* stop the pulser */

		if ( ! TEST_RUN && ! dg2020_run( STOP ) )
		{
			eprint( FATAL, "%s:%ld: DG2020: Communication with pulser "
					"failed.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_length( long pnum, double time )
{
	PULSE *p = dg2020_get_pulse( pnum );
	bool was_active = p->is_active;


	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid (negative) length "
				"for pulse %ld: %s.\n", Fname, Lc, pnum,
				dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( p->len == dg2020_double2ticks( time ) )
	{
		eprint( WARN, "%s:%ld: DG2020: Old and new length of pulse %ld are "
				"identical.\n", Fname, Lc, pnum );
		return OK;
	}

	if ( ! p->is_old_len )
	{
		p->old_len = p->len;
		p->is_old_len = SET;
	}

	p->len = dg2020_double2ticks( time );
	p->is_len = SET;

	/* If a previously inactive pulse gets a non-zero length and also has a
	   position set to it it autoatically becomes active */

	if ( ! p->is_active && p->is_pos && p->len > 0 )
		p->is_active = p->has_been_active = SET;
	else
		p->is_active = UNSET;

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

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_position_change( long pnum, double time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( dg2020_double2ticks( time ) == 0 && TEST_RUN )
	{
		eprint( SEVERE, "%s:%ld: DG2020: Zero position change value for pulse "
				"%ld. Useless value isn't set.\n", Fname, Lc, pnum );
		return FAIL;
	}

	p->dpos = dg2020_double2ticks( time );
	p->is_dpos = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_length_change( long pnum, double time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( dg2020_double2ticks( time ) == 0 && TEST_RUN )
	{
		eprint( SEVERE, "%s:%ld: DG2020: Zero length change value for pulse "
				"%ld. Useless value isn't set.\n", Fname, Lc, pnum );
		return FAIL;
	}

	p->dlen = dg2020_double2ticks( time );
	p->is_dlen = SET;

	return OK;
}
