#include "fsc2.h"


/*---------------------------------------------------------------------*/
/* Called before the PHASES section is parsed to reset some variables. */
/*---------------------------------------------------------------------*/

void phases_init( void )
{
	int i;


	ASeq.defined = UNSET;

	for ( i = 0; i < MAX_PHASE_SEQ_LEN; ++i )
		PSeq[ i ].defined = UNSET;
}


/*-------------------------------------------------------*/
/* Called when an acquisition sequence keyword is found. */
/*-------------------------------------------------------*/

void acq_seq_start( long acq_type )
{
	/* Check that this acquisition sequence isn't already defined */

	if ( ASeq.defined )
		eprint( SEVERE, "%s:%ld: Multiple definition of acquisition "
				"sequence, using new definition.\n", Fname, Lc );

	/* initialize the acquisition sequence */

	ASeq.defined = SET;
	ASeq.sequence[ 0 ] = acq_type;
	ASeq.len = 1;
}


/*-------------------------------------------------------------------------*/
/* Called for each element in a list of signs for an acquisition sequence. */
/* ->                                                                      */
/*    * either ACQ_PLUS or ACQ_MINUS                                       */
/*-------------------------------------------------------------------------*/

void acq_seq_cont( long acq_type )
{
	/* Make real sure the acquisition type is reasonable */

	assert( acq_type == ACQ_PLUS || acq_type == ACQ_MINUS );

	/* Check that the sequence doesn't get too long */

	if ( ASeq.len >= MAX_PHASE_SEQ_LEN )
	{
		eprint( FATAL, "%s:%ld: Maximum acquisition sequence length of %d "
				"exceeded.\n ", Fname, Lc, MAX_PHASE_SEQ_LEN );
		THROW( PHASES_EXCEPTION );
	}

	/* add the new acquisition type */

	ASeq.sequence[ ASeq.len++ ] = acq_type;
}


/*----------------------------------------------*/
/* Called when a phase sequence token is found. */
/* ->                                           */
/*    * number of the phase sequence            */
/*----------------------------------------------*/

long phase_seq_start( long phase_seq_num )
{
	/* Again, let's be paranoid */

	assert( phase_seq_num >= 0 && phase_seq_num < MAX_PHASE_SEQ_LEN );

	/* Check that this phase sequence isn't already used */

	if ( PSeq[ phase_seq_num ].defined )
		eprint( SEVERE, "%s:%ld: Multiple definition of phase sequence "
				"#%ld, using new definition.\n",
				Fname, Lc, phase_seq_num + 1 );

	/* initialize the phase sequence */

	PSeq[ phase_seq_num ].defined = SET;
	PSeq[ phase_seq_num ].len = 0;
	return( phase_seq_num );
}


/*------------------------------------------------------------------*/
/* Called for each element in a list of phases for a phase sequence */
/* ->                                                               */
/*    * number of the phase sequence, 1 or 2                        */
/*    * either PHASE_PLUS_X, PHASE_MINUS_X, PHASE_PLUS_Y or         */
/*      PHASE_MINUS_Y                                               */
/*------------------------------------------------------------------*/

void phases_add_phase( long phase_seq_num, int phase_type )
{
	/* Again, let's be paranoid */

	assert( phase_seq_num >= 0 && phase_seq_num < MAX_PHASE_SEQ_LEN );
	assert ( phase_type >= PHASE_PLUS_X && phase_type <= PHASE_MINUS_Y );

	/* make sure the phase sequence doesn't get too long */

	if ( PSeq[ phase_seq_num ].len >= MAX_PHASE_SEQ_LEN )
	{
		eprint( FATAL, "%s:%ld: Maximum sequence length of %d for phase "
				"sequence #%ld exceeded.\n ",
				Fname, Lc, MAX_PHASE_SEQ_LEN, phase_seq_num + 1 );
		THROW( PHASES_EXCEPTION );
	}

	PSeq[ phase_seq_num ].sequence[ PSeq[ phase_seq_num ].len++ ] = phase_type;
}


/*---------------------------------------------------*/
/* Called if an aquisition sequence keyword is found */
/* without a corresponding list of signs.            */
/*---------------------------------------------------*/

void acq_miss_list( void )
{
	eprint( FATAL, "%s:%ld: Missing acquisition type list.\n",
			Fname, Lc );
	THROW( PHASES_EXCEPTION );
}


/*---------------------------------------------*/
/* Called if a phase sequence keyword is found */
/* without a corresponding list of phases.     */
/*---------------------------------------------*/

void phase_miss_list( long phase_seq_num )
{
	eprint( FATAL, "%s:%ld: Missing list of phases for phase sequence #%ld.\n",
			Fname, Lc, phase_seq_num + 1 );
	THROW( PHASES_EXCEPTION );
}


/*--------------------------------------------------*/
/* Called after a PHASES section is parsed to check */
/* that the results are reasonable.                 */
/*--------------------------------------------------*/

void phases_end( void )
{
	int i;
	bool are_phases_defined = UNSET;


	for ( i = 0; i < MAX_PHASE_SEQ_LEN; ++i )
		if ( PSeq[ i ].defined )
		{
			are_phases_defined = SET;
			break;
		}

	/* return immediately if no sequences were defined */

	if ( ! ASeq.defined && ! are_phases_defined )
		return;

	/* check that there is a acquisition sequence if there's a phase
	   sequence */

	if ( are_phases_defined && ! ASeq.defined )
	{
		eprint( FATAL, "No acquisition sequence defined in PHASES "
				"section.\n" );
		THROW( PHASES_EXCEPTION );
	}

	/* check that there is a phase sequence if there's a acquisition
	   sequence */

	if ( ASeq.defined && ! are_phases_defined )
	{
		eprint( FATAL, "No phase sequence defined in PHASES section.\n" );
		THROW( PHASES_EXCEPTION );
	}

	/* check that lengths of acquisition and phase sequences are identical */

	for ( i = 0; i < MAX_PHASE_SEQ_LEN; ++i )
	{
		if ( ! PSeq[ i ].defined )
			continue;

		if ( ASeq.len != PSeq[ i ].len )
		{
			eprint( FATAL, "Different length of phase and acquisition "
					"sequences in PHASES section.\n" );
			THROW( PHASES_EXCEPTION );
		}
	}

	/* Again, a check that never should fail, but... */

	assert( ASeq.len != 0 );
}


/*---------------------------------------------------------------------*/
/* Called after parsing the file to check that the phases setup is ok. */
/*---------------------------------------------------------------------*/

bool check_phase_setup( void )
{
	/* Check that there are at least as many channels assigned for phase
	   sequences as the phase and acquisition sequences are long */

	if ( ASeq.len > assignment[ PULSER_CHANNEL_PHASE_X ].num_channels )
	{
		eprint( FATAL, "The number of channels assigned for phases (%d) in "
				"the ASSIGNMENTS section is too small for the phase sequence "
				"length (%d) defined in the PHASES section.\n",
				assignment[ PULSER_CHANNEL_PHASE_X ].num_channels, ASeq.len );
		return( FAIL );
	}

	return( OK );
}
