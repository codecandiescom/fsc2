/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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
			print( FATAL, "Can't create pulse with number %ld, it already "
				   "exists.\n", pnum );
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
		print( FATAL, "The function of pulse %ld has already been set to "
			   "`%s'.\n", pnum, Function_Names[ p->function->self ] );
		THROW( EXCEPTION );
	}

	if ( function == PULSER_CHANNEL_PHASE_1 || 
		 function == PULSER_CHANNEL_PHASE_2 )
	{
		print( FATAL, "Pulses for the PHASE function can't be set, all phase "
			   "pulses are created automatically.\n" );
		THROW( EXCEPTION );
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
		print( FATAL, "The start position of pulse %ld has already been set "
			   "to %s.\n", pnum, dg2020_pticks( p->pos ) );
		THROW( EXCEPTION );
	}

	if ( p_time < 0 )
	{
		print( FATAL, "Invalid (negative) start position for pulse %ld: "
			   "%s.\n", pnum, dg2020_ptime( p_time ) );
		THROW( EXCEPTION );
	}

	p->pos = dg2020_double2ticks( p_time );
	p->is_pos = SET;

	if ( ! p->initial_is_pos && FSC2_MODE == PREPARATION )
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
		print( FATAL, "The length of pulse %ld has already been set to %s.\n",
			   pnum, dg2020_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( p_time < 0.0 )
	{
		print( FATAL, "Invalid negative length set for pulse %ld: %s.\n",
			   pnum, dg2020_ptime( p_time ) );
		THROW( EXCEPTION );
	}

	if ( p_time != 0.0 )
	{
		p->len = dg2020_double2ticks( p_time );
		p->is_len = SET;
	}

	if ( ! p->initial_is_len && FSC2_MODE == PREPARATION )
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
		print( FATAL, "The position change of pulse %ld has already been set "
			   "to %s.\n", pnum, dg2020_pticks( p->dpos ) );
		THROW( EXCEPTION );
	}

	if ( dg2020_double2ticks( p_time ) == 0 )
	{
		print( SEVERE, "Zero position change value for pulse %ld.\n", pnum );
		return FAIL;
	}

	p->dpos = dg2020_double2ticks( p_time );
	p->is_dpos = SET;

	if ( ! p->initial_is_dpos && FSC2_MODE == PREPARATION )
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
		print( FATAL, "The length change of pulse %ld has already been set to "
			   "%s.\n", pnum, dg2020_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( dg2020_double2ticks( p_time ) == 0 )
	{
		print( SEVERE, "Zero length change value for pulse %ld.\n", pnum );
		return FAIL;
	}

	p->dlen = dg2020_double2ticks( p_time );
	p->is_dlen = SET;

	if ( ! p->initial_is_dlen && FSC2_MODE == PREPARATION )
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
		print( FATAL, "Pulse %ld has already been assigned a phase cycle.\n",
			   pnum );
		THROW( EXCEPTION );
	}

	if ( phase_numbers[ 0 ] != -1 && phase_numbers[ 0 ] != cycle &&
		 phase_numbers[ 1 ] != -1 && phase_numbers[ 1 ] != cycle )
	{
		print( FATAL, "Only two different phase sequences can be used for "
			   "pulses. \n" );
		THROW( EXCEPTION );
	}

	while ( pc != NULL )
	{
		if ( pc->num == cycle )
			break;
		pc = pc->next;
	}

	if ( pc == NULL )
	{
		print( FATAL, "Referenced phase sequence %d hasn't been defined.\n",
			   cycle );
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
		print( FATAL, "The function of pulse %ld hasn't been set.\n", pnum );
		THROW( EXCEPTION );
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
		print( FATAL, "The start position of pulse %ld hasn't been set.\n",
			   pnum );
		THROW( EXCEPTION );
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
		print( FATAL, "The length of pulse %ld hasn't been set.\n", pnum );
		THROW( EXCEPTION );
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
		print( FATAL, "The position change of pulse %ld hasn't been set.\n",
			   pnum );
		THROW( EXCEPTION );
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
		print( FATAL, "The length change of pulse %ld hasn't been set.\n",
			   pnum );
		THROW( EXCEPTION );
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
		print( FATAL, "No phase cycle has been set for pulse %ld.\n", pnum );
		THROW( EXCEPTION );
	}

	*cycle = p->pc->num;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_position( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );
	static Ticks new_pos = 0;


	if ( p_time < 0 )
	{
		print( FATAL, "Invalid (negative) start position for pulse %ld: %s.\n",
			   pnum, dg2020_ptime( p_time ) );
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	TRY
	{
		new_pos = dg2020_double2ticks( p_time );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	if ( p->is_pos && new_pos == p->pos )
	{
		print( WARN, "Old and new position of pulse %ld are identical.\n",
			   pnum );
		return OK;
	}

	if ( p->is_pos && ! p->is_old_pos  )
	{
		p->old_pos = p->pos;
		p->is_old_pos = SET;
	}

	p->pos = new_pos;
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
	static Ticks new_len = 0;


	if ( p_time < 0 )
	{
		print( FATAL, "Invalid (negative) length for pulse %ld: %s.\n",
			   pnum, dg2020_ptime( p_time ) );
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	TRY
	{
		new_len = dg2020_double2ticks( p_time );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	if ( p->is_len && p->len == new_len )
	{
		print( WARN, "Old and new length of pulse %ld are identical.\n",
			   pnum );
		return OK;
	}

	if ( p->is_len && ! p->is_old_len )
	{
		p->old_len = p->len;
		p->is_old_len = SET;
	}

	p->len = new_len;
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
	static Ticks new_dpos = 0;


	TRY
	{
		new_dpos = dg2020_double2ticks( p_time );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	if ( new_dpos == 0 && FSC2_MODE == TEST )
	{
		print( SEVERE, "Zero position change value for pulse %ld.\n", pnum );
		return FAIL;
	}

	p->dpos = new_dpos;
	p->is_dpos = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_change_pulse_length_change( long pnum, double p_time )
{
	PULSE *p = dg2020_get_pulse( pnum );
	static Ticks new_dlen = 0;


	TRY
	{
		new_dlen = dg2020_double2ticks( p_time );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	if ( new_dlen == 0 && FSC2_MODE == TEST )
	{
		print( SEVERE, "Zero length change value for pulse %ld.\n", pnum );
		return FAIL;
	}

	p->dlen = new_dlen;
	p->is_dlen = SET;

	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
