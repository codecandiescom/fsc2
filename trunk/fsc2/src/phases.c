/*
  $Id$
*/


#include "fsc2.h"


long cur_aseq;


/*-------------------------------------------------------*/
/* Called when an acquisition sequence keyword is found. */
/*-------------------------------------------------------*/

void acq_seq_start( long acq_num, long acq_type )
{
	assert( acq_num == 0 || acq_num == 1 );

	/* Check that this acquisition sequence isn't already defined */

	cur_aseq = acq_num;

	if ( ASeq[ acq_num ].defined )
	{
		eprint( FATAL, "%s:%ld: Acquisition sequence %c has already been "
				"defined.", Fname, Lc, ( char ) ( acq_num + 'X' ) );
		THROW( EXCEPTION );
	}

	/* initialize the acquisition sequence */

	ASeq[ acq_num ].defined = SET;
	ASeq[ acq_num ].sequence = T_malloc( sizeof( int ) );
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

	assert( acq_type >= ACQ_PLUS_A && acq_type <= ACQ_MINUS_B );

	/* add the new acquisition type */

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


	assert( phase_seq_num >= 0 );       /* again, let's be paranoid */

	/* Check that a phase sequence with this number isn't already defined */

	while ( cp != NULL )
	{
		if ( cp->num == phase_seq_num )
		{
			eprint( FATAL, "%s:%ld: Phase sequence %ld has already been "
					"defined.", Fname, Lc, cp->num );
			THROW( EXCEPTION );
		}
		cp = cp->next;
	}

	/* create new sequence, append it to end of list and  initialize it */

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
/*    * number of the phase sequence, 1 or 2                        */
/*    * either PHASE_PLUS_X, PHASE_MINUS_X, PHASE_PLUS_Y,           */
/*	    PHASE_MINUS_Y or PHASE_CW                                   */
/*------------------------------------------------------------------*/

void phases_add_phase( Phase_Sequence *p, int phase_type )
{
	assert ( phase_type >= PHASE_TYPES_MIN && phase_type <= PHASE_TYPES_MAX );

	/* append the new phase to the sequence */

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
	eprint( FATAL, "%s:%ld: Missing acquisition type list.",
			Fname, Lc );
	THROW( EXCEPTION );
}


/*---------------------------------------------*/
/* Called if a phase sequence keyword is found */
/* without a corresponding list of phases.     */
/*---------------------------------------------*/

void phase_miss_list( Phase_Sequence *p )
{
	eprint( FATAL, "%s:%ld: Missing list of phases for phase sequence %d.",
			Fname, Lc, p->num );
	THROW( EXCEPTION );
}


void phases_clear( void )
{
	Phase_Sequence *p, *pn;
	int i;


	for ( i = 0; i < 2; i++ )
	{
		ASeq[ i ].defined = UNSET;
		if ( ASeq[ i ].sequence != NULL )
			T_free( ASeq[ i ].sequence );
		ASeq[ i ].sequence = NULL;
	}

	for ( p = PSeq; p != NULL; p = pn )
	{
		pn = p->next;
		if ( p->sequence != NULL )
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


	/* return immediately if no sequences were defined at all */

	if ( ! ASeq[ 0 ].defined && ! ASeq[ 1 ].defined && PSeq == NULL )
		return;

	/* check that there is a acquisition sequence if there's a phase
	   sequence */

	if ( PSeq != NULL && ! ASeq[ 0 ].defined && ! ASeq[ 1 ].defined )
	{
		eprint( FATAL, "Phase sequences but no acquisition sequence "
				"defined in PHASES section." );
		THROW( EXCEPTION );
	}

	/* check that there is a phase sequence if there's a acquisition
	   sequence */

	if ( ( ASeq[ 0 ].defined || ASeq[ 1 ].defined ) && PSeq == NULL )
	{
		eprint( FATAL, "Aquisition sequences but no phase sequences defined "
				"in PHASES section." );
		THROW( EXCEPTION );
	}

	/* check that length of both acquisition sequences are identical
	   (if both were defined) */

	if ( ASeq[ 0 ].defined && ASeq[ 1 ].defined &&
		 ASeq[ 0 ].len != ASeq[ 1 ].len )
	{
		eprint( FATAL, "The lengths of both acqusition sequences are "
				"different." );
		THROW( EXCEPTION );
	}

	/* check that lengths of acquisition and phase sequences are identical */

	for ( p = PSeq; p != NULL; p = p->next )
	{
		if ( ASeq[ 0 ].len != p->len )
		{
			eprint( FATAL, "Lengths of phase sequence %d (%d) and the "
					"acquisition sequence(s) (%d) are different.",
					p->num, p->len, ASeq[0].len );
			THROW( EXCEPTION );
		}
	}
}
