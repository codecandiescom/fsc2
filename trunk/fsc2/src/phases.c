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


#include "fsc2.h"


static long cur_aseq;


/*-------------------------------------------------------*/
/* Called when an acquisition sequence keyword is found. */
/*-------------------------------------------------------*/

void acq_seq_start( long acq_num, long acq_type )
{
	fsc2_assert( acq_num == 0 || acq_num == 1 );

	/* Check that this acquisition sequence isn't already defined */

	cur_aseq = acq_num;

	if ( ASeq[ acq_num ].defined )
	{
		eprint( FATAL, SET, "Acquisition sequence %c has already been "
				"defined.\n", ( char ) ( acq_num + 'X' ) );
		THROW( EXCEPTION )
	}

	/* Initialize the acquisition sequence */

	ASeq[ acq_num ].defined = SET;
	ASeq[ acq_num ].sequence = T_malloc( sizeof( int ) );

	if ( acq_type == ACQ_PLUS_U && cur_aseq == 0 )
		acq_type = ACQ_PLUS_A;
	if ( acq_type == ACQ_MINUS_U && cur_aseq == 0 )
		acq_type = ACQ_MINUS_A;
	if ( acq_type == ACQ_PLUS_U && cur_aseq == 1 )
		acq_type = ACQ_PLUS_B;
	if ( acq_type == ACQ_MINUS_U && cur_aseq == 1 )
		acq_type = ACQ_MINUS_B;

	ASeq[ acq_num ].sequence[ 0 ] = ( int ) acq_type;

	ASeq[ acq_num ].len = 1;
}


/*-------------------------------------------------------------------------*/
/* Called for each element in a list of signs for an acquisition sequence. */
/* ->                                                                      */
/*    * either ACQ_PLUS_A, ACQ_MINUS_A, ACQ_PLUS_B or ACQ_MINUS_B          */
/*-------------------------------------------------------------------------*/

void acq_seq_cont( long acq_type )
{
	int len;

	/* Make real sure the acquisition type is reasonable */

	fsc2_assert( acq_type >= ACQ_PLUS_U && acq_type <= ACQ_MINUS_B );

	/* Add the new acquisition type */

	if ( acq_type == ACQ_PLUS_U && cur_aseq == 0 )
		acq_type = ACQ_PLUS_A;
	if ( acq_type == ACQ_MINUS_U && cur_aseq == 0 )
		acq_type = ACQ_MINUS_A;
	if ( acq_type == ACQ_PLUS_U && cur_aseq == 1 )
		acq_type = ACQ_PLUS_B;
	if ( acq_type == ACQ_MINUS_U && cur_aseq == 1 )
		acq_type = ACQ_MINUS_B;

	len = ++ASeq[ cur_aseq ].len;
	ASeq[ cur_aseq ].sequence = T_realloc( ASeq[ cur_aseq ].sequence,
										   len * sizeof( int ) );
	ASeq[ cur_aseq ].sequence[ len - 1 ] = ( int ) acq_type;
}


/*----------------------------------------------*/
/* Called when a phase sequence token is found. */
/* ->                                           */
/*    * number of the phase sequence            */
/*----------------------------------------------*/

Phase_Sequence *phase_seq_start( long phase_seq_num )
{
	Phase_Sequence *cp = PSeq, *pn;


	fsc2_assert( phase_seq_num >= 0 );        /* again, let's be paranoid */

	/* Check that a phase sequence with this number isn't already defined */

	while ( cp != NULL )
	{
		if ( cp->num == phase_seq_num )
		{
			eprint( FATAL, SET, "Phase sequence %ld has already been "
					"defined.\n", cp->num );
			THROW( EXCEPTION )
		}
		cp = cp->next;
	}

	/* Create new sequence, append it to end of list and initialize it */

	cp = T_malloc( sizeof( Phase_Sequence ) );

	if ( PSeq == NULL )
		PSeq = cp;
	else
	{
		pn = PSeq;
		while ( pn->next != NULL )
			pn = pn->next;
		pn->next = cp;
	}
	cp->next = NULL;

	cp->num = phase_seq_num;
	cp->len = 0;
	cp->sequence = NULL;

	return cp;
}


/*------------------------------------------------------------------*/
/* Called for each element in a list of phases for a phase sequence */
/* ->                                                               */
/*    * pointer to phase sequence                                   */
/*    * either PHASE_PLUS_X, PHASE_MINUS_X, PHASE_PLUS_Y,           */
/*	    PHASE_MINUS_Y or PHASE_CW                                   */
/*------------------------------------------------------------------*/

void phases_add_phase( Phase_Sequence *p, int phase_type )
{
	fsc2_assert ( phase_type >= PHASE_TYPES_MIN &&
				  phase_type <= PHASE_TYPES_MAX );

	/* Append the new phase to the sequence */

	p->len++;
	p->sequence = T_realloc ( p->sequence, p->len * sizeof( int ) );
	p->sequence[ p->len - 1 ] = phase_type;
}


/*---------------------------------------------------*/
/* Called if an aquisition sequence keyword is found */
/* without a corresponding list of signs.            */
/*---------------------------------------------------*/

void acq_miss_list( void )
{
	eprint( FATAL, SET, "Missing acquisition type list.\n" );
	THROW( EXCEPTION )
}


/*---------------------------------------------*/
/* Called if a phase sequence keyword is found */
/* without a corresponding list of phases.     */
/*---------------------------------------------*/

void phase_miss_list( Phase_Sequence *p )
{
	eprint( FATAL, SET, "Missing list of phases for phase sequence %d.\n",
			p->num );
	THROW( EXCEPTION )
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

void phases_clear( void )
{
	Phase_Sequence *p, *pn;
	int i;


	for ( i = 0; i < 2; i++ )
	{
		ASeq[ i ].defined = UNSET;
		ASeq[ i ].sequence = T_free( ASeq[ i ].sequence );
	}

	for ( p = PSeq; p != NULL; p = pn )
	{
		pn = p->next;
		T_free( p->sequence );
		T_free( p );
	}

	PSeq = NULL;
}


/*--------------------------------------------------*/
/* Called after a PHASES section is parsed to check */
/* that the results are reasonable.                 */
/*--------------------------------------------------*/

void phases_end( void )
{
	Phase_Sequence *p;


	/* Return immediately if no sequences were defined at all */

	if ( ! ASeq[ 0 ].defined && ! ASeq[ 1 ].defined && PSeq == NULL )
		return;

	/* Check that there is a phase sequence if there's a acquisition
	   sequence */

	if ( ( ASeq[ 0 ].defined || ASeq[ 1 ].defined ) && PSeq == NULL )
	{
		eprint( FATAL, UNSET, "Aquisition sequence(s) defined but no phase "
				"sequences in PHASES section.\n" );
		THROW( EXCEPTION )
	}

	/* Return if neither phase sequences nor acquisition sequences are
	   defined */

	if ( PSeq == NULL )
		return;

	/* Check that the lengths of all phases sequences are identical */

	for ( p = PSeq->next; p != NULL; p = p->next )
		if ( p->len != PSeq->len )
		{
			eprint( FATAL, UNSET, "Lengths of phase sequences defined in "
					"PHASES section differ.\n" );
			THROW( EXCEPTION )
		}

	/* Check that lengths of acquisition and phase sequences are identical */

	if ( ( ASeq[ 0 ].defined && ASeq[ 0 ].len != PSeq->len ) ||
		 ( ASeq[ 1 ].defined && ASeq[ 1 ].len != PSeq->len ) )
	{
		eprint( FATAL, UNSET, "Lengths of phase and acquisition sequences "
				"defined in PHASES section differ.\n" );
		THROW( EXCEPTION )
	}
}
