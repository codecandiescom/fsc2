/*
  $Id$
*/


#include "dg2020.h"



/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool new_pulse( long pnum )
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
	cp->channel = NULL;
	cp->need_update = UNSET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool set_pulse_function( long pnum, int function )
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

bool set_pulse_position( long pnum, double time )
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

bool set_pulse_length( long pnum, double time )
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

bool set_pulse_position_change( long pnum, double time )
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

bool set_pulse_length_change( long pnum, double time )
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

bool set_pulse_phase_cycle( long pnum, int cycle )
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

bool set_pulse_maxlen( long pnum, double time )
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

bool set_pulse_replacements( long pnum, long num_repl, long *repl_list )
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

bool get_pulse_function( long pnum, int *function )
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

bool get_pulse_position( long pnum, double *time )
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

bool get_pulse_length( long pnum, double *time )
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

bool get_pulse_position_change( long pnum, double *time )
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

bool get_pulse_length_change( long pnum, double *time )
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

bool get_pulse_phase_cycle( long pnum, int *cycle )
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

bool get_pulse_maxlen( long pnum, double *time )
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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool change_pulse_position( long pnum, double time )
{
	PULSE *p = get_pulse( pnum );


	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid (negative) start position "
				"for pulse %ld: %s.\n", Fname, Lc, pnum, ptime( time ) );
		THROW( EXCEPTION );
	}

	p->old_pos = p->pos;
	p->pos = double2ticks( time );

	p->need_update = SET;
	dg2020.need_update = SET;

	/* stop the pulser */
	
	if ( ! TEST_RUN && ! dg2020_run( STOP ) )
	{
		eprint( FATAL, "%s:%ld: DG2020: Communication with pulser "
				"failed.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool change_pulse_length( long pnum, double time )
{
	PULSE *p = get_pulse( pnum );


	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid (negative) length "
				"for pulse %ld: %s.\n", Fname, Lc, pnum, ptime( time ) );
		THROW( EXCEPTION );
	}

	p->old_len = p->len;
	p->len = double2ticks( time );

	p->need_update = SET;
	dg2020.need_update = SET;

	/* stop the pulser */
	
	if ( ! TEST_RUN && ! dg2020_run( STOP ) )
	{
		eprint( FATAL, "%s:%ld: DG2020: Communication with pulser "
				"failed.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool change_pulse_position_change( long pnum, double time )
{
	PULSE *p = get_pulse( pnum );


	p->dpos = double2ticks( time );
	p->is_dpos = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool change_pulse_length_change( long pnum, double time )
{
	PULSE *p = get_pulse( pnum );


	p->dlen = double2ticks( time );
	p->is_dlen = SET;

	return OK;
}
