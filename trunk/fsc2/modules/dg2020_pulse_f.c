/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "dg2020_f.h"



/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_new_pulse( long pnum )
{
	PULSE *cp = dg2020_Pulses;
	PULSE *lp = NULL;


	while ( cp != NULL )
	{
		if ( cp->num == pnum )
		{
			eprint( FATAL, SET, "%s: Can't create pulse with number %ld, "
					"it already exists.\n", pulser_struct.name, pnum );
			THROW( EXCEPTION )
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
	cp->has_been_active = cp->was_active = UNSET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_pulse_function( long pnum, int function )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p->is_function )
	{
		eprint( FATAL, SET, "%s: The function of pulse %ld has already "
				"been set to `%s'.\n", pulser_struct.name, pnum,
				Function_Names[ p->function->self ] );
		THROW( EXCEPTION )
	}

	if ( function == PULSER_CHANNEL_PHASE_1 || 
		 function == PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, SET, "You can't set pulses for the PHASE function, "
				"all pulses needed will be created automatically.\n" );
		THROW( EXCEPTION )
	}

	p->function = &dg2020.function[ function ];
	p->is_function = SET;
	p->function->is_needed = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_pulse_position( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p->is_pos )
	{
		eprint( FATAL, SET, "%s: The start position of pulse %ld has "
				"already been set to %s.\n", pulser_struct.name,
				pnum, dg2020_pticks( p->pos ) );
		THROW( EXCEPTION )
	}

	if ( p_time < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid (negative) start position for "
				"pulse %ld: %s.\n", pulser_struct.name, pnum,
				dg2020_ptime( p_time ) );
		THROW( EXCEPTION )
	}

	p->pos = dg2020_double2ticks( p_time );
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

bool dg2020_set_pulse_length( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );

	if ( p->is_len )
	{
		eprint( FATAL, SET, "%s: The length of pulse %ld has already been "
				"set to %s.\n", pulser_struct.name, pnum,
				dg2020_pticks( p->len ) );
		THROW( EXCEPTION )
	}

	if ( p_time < 0.0 )
	{
		eprint( FATAL, SET, "%s: Invalid negative length set for pulse %ld: "
				"%s.\n", pulser_struct.name, pnum, dg2020_ptime( p_time ) );
		THROW( EXCEPTION )
	}

	if ( p_time != 0.0 )
	{
		p->len = dg2020_double2ticks( p_time );
		p->is_len = SET;
	}

	if ( ! p->initial_is_len && ! TEST_RUN && I_am == PARENT )
	{
		p->initial_len = dg2020_double2ticks( p_time );
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

bool dg2020_set_pulse_position_change( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p->is_dpos )
	{
		eprint( FATAL, SET, "%s: The position change of pulse %ld has "
				"already been set to %s.\n", pulser_struct.name,
				pnum, dg2020_pticks( p->dpos ) );
		THROW( EXCEPTION )
	}

	if ( dg2020_double2ticks( p_time ) == 0 )
	{
		eprint( SEVERE, SET, "%s: Zero position change value for pulse "
				"%ld.\n", pulser_struct.name, pnum );
		return FAIL;
	}

	p->dpos = dg2020_double2ticks( p_time );
	p->is_dpos = SET;

	if ( ! p->initial_is_dpos && ! TEST_RUN && I_am == PARENT )
	{
		p->initial_dpos = dg2020_double2ticks( p_time );
		p->initial_is_dpos = SET;
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_pulse_length_change( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p->is_dlen )
	{
		eprint( FATAL, SET, "%s: The length change of pulse %ld has "
				"already been set to %s.\n", pulser_struct.name,
				pnum, dg2020_pticks( p->len ) );
		THROW( EXCEPTION )
	}

	if ( dg2020_double2ticks( p_time ) == 0 )
	{
		eprint( SEVERE, SET, "%s: Zero length change value for pulse "
				"%ld.\n", pulser_struct.name, pnum );
		return FAIL;
	}

	p->dlen = dg2020_double2ticks( p_time );
	p->is_dlen = SET;

	if ( ! p->initial_is_dlen && ! TEST_RUN && I_am == PARENT )
	{
		p->initial_dlen = dg2020_double2ticks( p_time );
		p->initial_is_dlen = SET;
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_pulse_phase_cycle( long pnum, long cycle )
{
	PULSE *p = dg2020_get_pulse( pnum );
	Phase_Sequence *pc = PSeq;


	if ( p->pc != NULL )
	{
		eprint( FATAL, SET, "%s: Pulse %ld has already been assigned a "
				"phase cycle.\n", pulser_struct.name, pnum );
		THROW( EXCEPTION )
	}

	while ( pc != NULL )
	{
		if ( pc->num == cycle )
			break;
		pc = pc->next;
	}

	if ( pc == NULL )
	{
		eprint( FATAL, SET, "%s: Referenced phase sequence %d hasn't been "
				"defined.\n", pulser_struct.name, cycle );
		THROW( EXCEPTION )
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
		eprint( FATAL, SET, "%s: The function of pulse %ld hasn't been "
				"set.\n", pulser_struct.name, pnum );
		THROW( EXCEPTION )
	}

	*function = p->function->self;
	return OK;
}

/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_get_pulse_position( long pnum, double *p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( ! p->is_pos )
	{
		eprint( FATAL, SET, "%s: The start position of pulse %ld hasn't "
				"been set.\n", pulser_struct.name, pnum );
		THROW( EXCEPTION )
	}

	*p_time = dg2020_ticks2double( p->pos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_get_pulse_length( long pnum, double *p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( ! p->is_len )
	{
		eprint( FATAL, SET, "%s: The length of pulse %ld hasn't been "
				"set.\n", pulser_struct.name, pnum );
		THROW( EXCEPTION )
	}

	*p_time = dg2020_ticks2double( p->len );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_get_pulse_position_change( long pnum, double *p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( ! p->is_dpos )
	{
		eprint( FATAL, SET, "%s: The position change of pulse %ld hasn't "
				"been set.\n", pulser_struct.name, pnum );
		THROW( EXCEPTION )
	}

	*p_time = dg2020_ticks2double( p->dpos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_get_pulse_length_change( long pnum, double *p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( ! p->is_dlen )
	{
		eprint( FATAL, SET, "%s: The length change of pulse %ld hasn't "
				"been set.\n", pulser_struct.name, pnum );
		THROW( EXCEPTION )
	}

	*p_time = dg2020_ticks2double( p->dlen );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_get_pulse_phase_cycle( long pnum, long *cycle )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p->pc == NULL )
	{
		eprint( FATAL, SET, "No phase cycle has been set for pulse %ld.\n",
				pulser_struct.name, pnum );
		THROW( EXCEPTION )
	}

	*cycle = p->pc->num;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_position( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p_time < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid (negative) start position for "
				"pulse %ld: %s.\n", pulser_struct.name, pnum,
				dg2020_ptime( p_time ) );
		THROW( EXCEPTION )
	}

	if ( p->is_pos && dg2020_double2ticks( p_time ) == p->pos )
	{
		eprint( WARN, SET, "%s: Old and new position of pulse %ld are "
				"identical.\n", pulser_struct.name, pnum );
		return OK;
	}

	if ( p->is_pos && ! p->is_old_pos  )
	{
		p->old_pos = p->pos;
		p->is_old_pos = SET;
	}

	p->pos = dg2020_double2ticks( p_time );
	p->is_pos = SET;

	p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
	p->needs_update = NEEDS_UPDATE( p );

	if ( p->needs_update )
		dg2020.needs_update = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_length( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( p_time < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid (negative) length for pulse %ld: "
				"%s.\n", pulser_struct.name, pnum,
				dg2020_ptime( p_time ) );
		THROW( EXCEPTION )
	}

	if ( p->is_len && p->len == dg2020_double2ticks( p_time ) )
	{
		eprint( WARN, SET, "%s: Old and new length of pulse %ld are "
				"identical.\n", pulser_struct.name, pnum );
		return OK;
	}

	if ( p->is_len && ! p->is_old_len )
	{
		p->old_len = p->len;
		p->is_old_len = SET;
	}

	p->len = dg2020_double2ticks( p_time );
	p->is_len = SET;

	p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
	p->needs_update = NEEDS_UPDATE( p );

	if ( p->needs_update )
		dg2020.needs_update = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_position_change( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( dg2020_double2ticks( p_time ) == 0 && TEST_RUN )
	{
		eprint( SEVERE, SET, "%s: Zero position change value for pulse "
				"%ld.\n", pulser_struct.name, pnum );
		return FAIL;
	}

	p->dpos = dg2020_double2ticks( p_time );
	p->is_dpos = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_length_change( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );


	if ( dg2020_double2ticks( p_time ) == 0 && TEST_RUN )
	{
		eprint( SEVERE, SET, "%s: Zero length change value for pulse "
				"%ld.\n", pulser_struct.name, pnum );
		return FAIL;
	}

	p->dlen = dg2020_double2ticks( p_time );
	p->is_dlen = SET;

	return OK;
}
