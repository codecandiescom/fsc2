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
			print( FATAL, "Pulse %ld already exists.\n", pnum );
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
		print( FATAL, "Phase functions can't be used with this driver.\n" );
		THROW( EXCEPTION );
	}

	if ( p->is_function )
	{
		print( FATAL, "Function of pulse %ld has already been set to `%s'.\n",
			   pnum, Function_Names[ p->function->self ] );
		THROW( EXCEPTION );
	}

	if ( f->channel == NULL )
	{
		print( FATAL, "No channel has been assigned to function `%s'.\n",
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
				print( FATAL, "There can be only one pulse for the Trigger "
					   "Out channel.\n" );
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
			print( WARN, "Length of Trigger Out pulse has been set to fixed "
				   "length of %s.\n",
				   hfs9000_ptime( HFS9000_TRIG_OUT_PULSE_LEN ) );
		}

		if ( p->is_dlen )
		{
			print( FATAL, "Trigger Out pulses are not allowed to have a pulse "
				   "length change set.\n" );
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

bool hfs9000_set_pulse_position( long pnum, double p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( p->is_pos )
	{
		print( FATAL, "The start position of pulse %ld has already been set "
			   "to %s.\n", pnum, hfs9000_pticks( p->pos ) );
		THROW( EXCEPTION );
	}

	if ( p_time < 0 )
	{
		print( FATAL, "Invalid (negative) start position for pulse %ld: %s.\n",
			   pnum, hfs9000_ptime( p_time ) );
		THROW( EXCEPTION );
	}

	p->pos = hfs9000_double2ticks( p_time );
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

bool hfs9000_set_pulse_length( long pnum, double p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( p->is_len )
	{
		print( FATAL, "Length of pulse %ld has already been set to %s.\n",
			   pnum, hfs9000_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( p_time < 0.0 )
	{
		print( FATAL, "Invalid negative length set for pulse %ld: %s.\n",
			   pnum, hfs9000_ptime( p_time ) );
		THROW( EXCEPTION );
	}

	if ( p->is_function && p->function->channel &&
		 p->function->channel->self == HFS9000_TRIG_OUT )
	{
		p->len = 1;
		print( SEVERE, "Length of Trigger Out pulse %ld is fixed to 20 ns\n",
			   pnum );
	}
	else
		p->len = hfs9000_double2ticks( p_time );
	p->is_len = SET;

	if ( ! p->initial_is_len && FSC2_MODE == PREPARATION )
	{
		p->initial_len = hfs9000_double2ticks( p_time );
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

bool hfs9000_set_pulse_position_change( long pnum, double p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( p->is_dpos )
	{
		print( FATAL, "The position change of pulse %ld has already been set "
			   "to %s.\n", pnum, hfs9000_pticks( p->dpos ) );
		THROW( EXCEPTION );
	}

	if ( hfs9000_double2ticks( p_time ) == 0 )
	{
		print( SEVERE, "Zero position change set for pulse %ld.\n", pnum );
		return FAIL;
	}

	p->dpos = hfs9000_double2ticks( p_time );
	p->is_dpos = SET;

	if ( ! p->initial_is_dpos && FSC2_MODE == PREPARATION )
	{
		p->initial_dpos = hfs9000_double2ticks( p_time );
		p->initial_is_dpos = SET;
	}

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_pulse_length_change( long pnum, double p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( p->is_dlen )
	{
		print( FATAL, "Length change of pulse %ld has already been set to "
			   "%s.\n", pnum, hfs9000_pticks( p->len ) );
		THROW( EXCEPTION );
	}

	if ( hfs9000_double2ticks( p_time ) == 0 )
	{
		print( SEVERE, "Zero length change set for pulse %ld.\n", pnum );
		return FAIL;
	}

	if ( p->is_function && p->function->channel &&
		 p->function->channel->self == HFS9000_TRIG_OUT )
	{
		print( FATAL, "Length of Trigger Out pulse %ld can't be changed.\n",
			   pnum );
		THROW( EXCEPTION );
	}

	p->dlen = hfs9000_double2ticks( p_time );
	p->is_dlen = SET;

	if ( ! p->initial_is_dlen && FSC2_MODE == PREPARATION )
	{
		p->initial_dlen = hfs9000_double2ticks( p_time );
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
		print( FATAL, "The function of pulse %ld hasn't been set.\n", pnum );
		THROW( EXCEPTION );
	}

	*function = p->function->self;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_get_pulse_position( long pnum, double *p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( ! p->is_pos )
	{
		print( FATAL, "Start position of pulse %ld hasn't been set.\n", pnum );
		THROW( EXCEPTION );
	}

	*p_time = hfs9000_ticks2double( p->pos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_get_pulse_length( long pnum, double *p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( ! p->is_len )
	{
		print( FATAL, "Length of pulse %ld hasn't been set.\n", pnum );
		THROW( EXCEPTION );
	}

	if ( p->is_function && p->function->channel &&
		 p->function->channel->self == HFS9000_TRIG_OUT )
	{
		print( FATAL, "Length of Trigger Out pulse %ld can't be referenced.\n",
			   pnum );
		THROW( EXCEPTION );
	}

	*p_time = hfs9000_ticks2double( p->len );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_get_pulse_position_change( long pnum, double *p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( ! p->is_dpos )
	{
		print( FATAL, "Position change of pulse %ld hasn't been set.\n",
			   pnum );
		THROW( EXCEPTION );
	}

	*p_time = hfs9000_ticks2double( p->dpos );
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_get_pulse_length_change( long pnum, double *p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );


	if ( ! p->is_dlen )
	{
		print( FATAL, "Length change of pulse %ld hasn't been set.\n", pnum );
		THROW( EXCEPTION );
	}

	*p_time = hfs9000_ticks2double( p->dlen );
	return OK;
}


/*-----------------------------------------------------------------------*/
/* Function for changing the pulse position while the experiment is run. */
/*-----------------------------------------------------------------------*/

bool hfs9000_change_pulse_position( long pnum, double p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );
	static Ticks new_pos = 0;


	if ( p_time < 0 )
	{
		print( FATAL, "Invalid (negative) start position for pulse %ld: %s.\n",
			   pnum, hfs9000_ptime( p_time ) );
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	TRY
	{
		new_pos = hfs9000_double2ticks( p_time );
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

	if ( p->is_pos && ! p->is_old_pos )
	{
		p->old_pos = p->pos;
		p->is_old_pos = SET;
	}

	p->pos = new_pos;
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

bool hfs9000_change_pulse_length( long pnum, double p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );
	static Ticks new_len = 0;


	if ( p_time < 0 )
	{
		print( FATAL, "Invalid (negative) length for pulse %ld: %s.\n",
			   pnum, hfs9000_ptime( p_time ) );
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	TRY
	{
		new_len = hfs9000_double2ticks( p_time );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	if ( p->is_len && p->is_function && p->function->channel )
	{
		if ( p->function->channel->self == HFS9000_TRIG_OUT )
		{
			print( FATAL, "Length of Trigger Out pulse %ld can't be "
				   "changed.\n", pnum );
			if ( FSC2_MODE == EXPERIMENT )
				return FAIL;
			else
				THROW( EXCEPTION );
		}

		if ( p->len == new_len )
		{
			print( WARN, "Old and new length of pulse %ld are identical.\n",
				   pnum );
			return OK;
		}
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
		hfs9000.needs_update = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_change_pulse_position_change( long pnum, double p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );
	static Ticks new_dpos = 0;


	TRY
	{
		new_dpos = hfs9000_double2ticks( p_time );
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

bool hfs9000_change_pulse_length_change( long pnum, double p_time )
{
	PULSE *p = hfs9000_get_pulse( pnum );
	static Ticks new_dlen = 0;


	if ( p->is_function && p->function->channel &&
		 p->function->channel->self == HFS9000_TRIG_OUT )
	{
		print( FATAL, "Length change of Trigger Out pulse %ld can't be set.\n",
			   pnum );
		if ( FSC2_MODE == EXPERIMENT )
			return FAIL;
		else
			THROW( EXCEPTION );
	}

	TRY
	{
		new_dlen = hfs9000_double2ticks( p_time );
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
