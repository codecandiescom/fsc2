/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2004 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2.h"


PA_Seq_T PA_Seq = { NULL, { { UNSET, NULL, 0 }, { UNSET, NULL, 0 } } };


static long cur_aseq;


/*-------------------------------------------------------*/
/* Called when an acquisition sequence keyword is found. */
/*-------------------------------------------------------*/

void acq_seq_start( long acq_num, long acq_type )
{
	fsc2_assert( acq_num == 0 || acq_num == 1 );

	/* Check that this acquisition sequence isn't already defined */

	cur_aseq = acq_num;

	if ( PA_Seq.acq_seq[ acq_num ].defined )
	{
		print( FATAL, "Acquisition sequence numbered %ld has already been "
			   "defined.\n", acq_num );
		THROW( EXCEPTION );
	}

	/* Initialize the acquisition sequence */

	PA_Seq.acq_seq[ acq_num ].defined = SET;
	PA_Seq.acq_seq[ acq_num ].sequence =
				  INT_P T_malloc( sizeof *PA_Seq.acq_seq[ acq_num ].sequence );

	if ( acq_type == ACQ_PLUS_U && cur_aseq == 0 )
		acq_type = ACQ_PLUS_A;
	if ( acq_type == ACQ_MINUS_U && cur_aseq == 0 )
		acq_type = ACQ_MINUS_A;
	if ( acq_type == ACQ_PLUS_U && cur_aseq == 1 )
		acq_type = ACQ_PLUS_B;
	if ( acq_type == ACQ_MINUS_U && cur_aseq == 1 )
		acq_type = ACQ_MINUS_B;

	PA_Seq.acq_seq[ acq_num ].sequence[ 0 ] = ( int ) acq_type;
	PA_Seq.acq_seq[ acq_num ].len = 1;
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

	len = ++PA_Seq.acq_seq[ cur_aseq ].len;
	PA_Seq.acq_seq[ cur_aseq ].sequence =
		   INT_P T_realloc( PA_Seq.acq_seq[ cur_aseq ].sequence,
							len * sizeof PA_Seq.acq_seq[ cur_aseq ].sequence );
	PA_Seq.acq_seq[ cur_aseq ].sequence[ len - 1 ] = ( int ) acq_type;
}


/*----------------------------------------------*/
/* Called when a phase sequence token is found. */
/* ->                                           */
/*    * number of the phase sequence            */
/*----------------------------------------------*/

Phs_Seq_T *phase_seq_start( long phase_seq_num )
{
	Phs_Seq_T *cp = PA_Seq.phs_seq, *pn;


	fsc2_assert( phase_seq_num >= 0 );        /* again, let's be paranoid */

	/* Check that a phase sequence with this number isn't already defined */

	while ( cp != NULL )
	{
		if ( cp->num == phase_seq_num )
		{
			print( FATAL, "Phase sequence numbered %ld has already been "
				   "defined.\n", cp->num );
			THROW( EXCEPTION );
		}
		cp = cp->next;
	}

	/* Create new sequence, append it to end of list and initialize it */

	cp = PHS_SEQ_P T_malloc( sizeof *cp );

	if ( PA_Seq.phs_seq == NULL )
		PA_Seq.phs_seq = cp;
	else
	{
		pn = PA_Seq.phs_seq;
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


/*-------------------------------------------------------------------*/
/* Called for each element in a list of phases for a phase sequence  */
/* ->                                                                */
/* * pointer to phase sequence                                       */
/* * either PHASE_PLUS_X, PHASE_MINUS_X, PHASE_PLUS_Y, PHASE_MINUS_Y */
/*-------------------------------------------------------------------*/

void phases_add_phase( Phs_Seq_T *p, int phase_type )
{
	fsc2_assert ( phase_type >= 0 && phase_type < NUM_PHASE_TYPES );

	/* Append the new phase to the sequence */

	p->len++;
	p->sequence = INT_P T_realloc ( p->sequence,
									p->len * sizeof *p->sequence );
	p->sequence[ p->len - 1 ] = phase_type;
}


/*---------------------------------------------------*/
/* Called if an aquisition sequence keyword is found */
/* without a corresponding list of signs.            */
/*---------------------------------------------------*/

void acq_miss_list( void )
{
	print( FATAL, "Missing acquisition type list.\n" );
	THROW( EXCEPTION );
}


/*---------------------------------------------*/
/* Called if a phase sequence keyword is found */
/* without a corresponding list of phases.     */
/*---------------------------------------------*/

void phase_miss_list( Phs_Seq_T *p )
{
	print( FATAL, "Missing list of phases for phase sequence %d.\n", p->num );
	THROW( EXCEPTION );
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

void phases_clear( void )
{
	Phs_Seq_T *p, *pn;
	int i;


	for ( i = 0; i < 2; i++ )
	{
		PA_Seq.acq_seq[ i ].defined = UNSET;
		PA_Seq.acq_seq[ i ].sequence =
								  INT_P T_free( PA_Seq.acq_seq[ i ].sequence );
	}

	for ( p = PA_Seq.phs_seq; p != NULL; p = pn )
	{
		pn = p->next;
		T_free( p->sequence );
		T_free( p );
	}

	PA_Seq.phs_seq = NULL;
}


/*--------------------------------------------------*/
/* Called after a PHASES section is parsed to check */
/* that the results are reasonable.                 */
/*--------------------------------------------------*/

void phases_end( void )
{
	Phs_Seq_T *p;


	/* Return immediately if no sequences were defined at all */

	if ( ! PA_Seq.acq_seq[ 0 ].defined && ! PA_Seq.acq_seq[ 1 ].defined &&
		 PA_Seq.phs_seq == NULL )
		return;

	/* Check that there is a phase sequence if there's a acquisition
	   sequence */

	if ( ( PA_Seq.acq_seq[ 0 ].defined || PA_Seq.acq_seq[ 1 ].defined ) &&
		 PA_Seq.phs_seq == NULL )
	{
		eprint( FATAL, UNSET, "Aquisition sequence(s) defined but no phase "
				"sequences in PHASES section.\n" );
		THROW( EXCEPTION );
	}

	/* Return if neither phase sequences nor acquisition sequences are
	   defined */

	if ( PA_Seq.phs_seq == NULL )
		return;

	/* Check that the lengths of all phases sequences are identical */

	for ( p = PA_Seq.phs_seq->next; p != NULL; p = p->next )
		if ( p->len != PA_Seq.phs_seq->len )
		{
			eprint( FATAL, UNSET, "Lengths of phase sequences defined in "
					"PHASES section differ.\n" );
			THROW( EXCEPTION );
		}

	/* Check that lengths of acquisition and phase sequences are identical */

	if ( ( PA_Seq.acq_seq[ 0 ].defined &&
		   PA_Seq.acq_seq[ 0 ].len != PA_Seq.phs_seq->len ) ||
		 ( PA_Seq.acq_seq[ 1 ].defined &&
		   PA_Seq.acq_seq[ 1 ].len != PA_Seq.phs_seq->len ) )
	{
		eprint( FATAL, UNSET, "Lengths of phase and acquisition sequences "
				"defined in PHASES section differ.\n" );
		THROW( EXCEPTION );
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
