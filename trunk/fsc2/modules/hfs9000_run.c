/*
  $Id$
*/


#include "hfs9000.h"


static PULSE *hfs9000_delete_pulse( PULSE *p );
static bool hfs9000_update_pulses( bool flag );
static void hfs9000_commit( FUNCTION *f, bool flag );


/*---------------------------------------------------------------------------
  Function is called in the experiment after pulses have been changed to
  update the pulser accordingly. No checking has to be done except in the
  test run.
----------------------------------------------------------------------------*/

void hfs9000_do_update( void )
{
	bool restart = UNSET;


	if ( ! hfs9000_is_needed )
		return;

	/* Resort the pulses and check that the new pulse settings are
	   reasonable and finally commit all changes */

	if ( hfs9000.is_running )
	{
		restart = SET;
		if ( ! TEST_RUN )
			hfs9000_run( STOP );
	}

	hfs9000_update_pulses( TEST_RUN );
	hfs9000.needs_update = UNSET;
	if ( restart && ! TEST_RUN )
		hfs9000_run( START );
}


/*---------------------------------------------------------------------------
  This function sorts the pulses and checks that the pulses don't overlap.
---------------------------------------------------------------------------*/

static bool hfs9000_update_pulses( bool flag )
{
	int i;
	FUNCTION *f;
	PULSE *p;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &hfs9000.function[ i ];

		/* Nothing to be done for unused functions */

		if ( ! f->is_used )
			continue;

		if ( f->num_pulses > 1 )
			qsort( f->pulses, f->num_pulses, sizeof( PULSE * ),
				   hfs9000_start_compare );

		/* Check the new pulse positions and lengths, if they're not ok stop
		   program if it's doing the test run, while for the real run just
		   reset the pulses to their previous positions and lengths */

		TRY
		{
			hfs9000_do_checks( f );
			TRY_SUCCESS;
		}
		CATCH( EXCEPTION )
		{
			if ( flag )
				THROW( EXCEPTION );

			for ( p = hfs9000_Pulses; p != NULL; p = p->next )
			{
				if ( p->is_old_pos )
					p->pos = p->old_pos;
				if ( p->is_old_len )
					p->len = p->old_len;
				p->is_active = IS_ACTIVE( p );
				p->needs_update = UNSET;
			}

			return FAIL;
		}

		hfs9000_commit( f, flag );
	}

	return OK;
}


/*------------------------------------------------------------------------
  Function is called after pulses have been changed to check if the new
  settings are still ok, i.e. that they still fit into the pulsers memory
  and don't overlap.
--------------------------------------------------------------------------*/

void hfs9000_do_checks( FUNCTION *f )
{
	PULSE *p;
	int i;

	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];

		/* Check that pulse still fits into the pulser s memory */

		if ( p->is_active )
		{
			f->max_seq_len = Ticks_max( f->max_seq_len, p->pos + p->len );
			if ( f->delay + f->max_seq_len > MAX_PULSER_BITS )
			{
				eprint( FATAL, SET, "%s: Pulse sequence for function `%s' "
						"does not fit into the pulsers memory. You could try "
						"a longer pulser time base.\n",
						pulser_struct.name, Function_Names[ f->self ] );
				THROW( EXCEPTION );
			}

			f->num_active_pulses = i + 1;
		}

		/* Check that pulse doesn't overlap with the next one - if their
		   distance becomes zero print out a warning in the test run */

		if ( i + 1 < f->num_pulses && f->pulses[ i + 1 ]->is_active )
		{
			if ( p->pos + p->len > f->pulses[ i + 1 ]->pos )
			{
				if ( hfs9000_IN_SETUP )
					eprint( FATAL, UNSET, "%s: Pulses %ld and %ld overlap.\n",
							pulser_struct.name, p->num,
							f->pulses[ i + 1 ]->num );
				else
					eprint( FATAL, SET, "%s: Pulses %ld and %ld begin to "
							"overlap.\n", pulser_struct.name,
							p->num, f->pulses[ i + 1 ]->num );
				THROW( EXCEPTION );
			}
			else if ( p->pos + p->len == f->pulses[ i + 1 ]->pos )
			{
				if ( hfs9000_IN_SETUP )
					eprint( SEVERE, UNSET, "%s: Distance between pulses %ld "
							"and %ld is zero.\n", pulser_struct.name,
							p->num, f->pulses[ i + 1 ]->num );
				else
					eprint( SEVERE, SET, "%s: Distance between pulses %ld "
							"and %ld becomes zero.\n", pulser_struct.name,
							p->num, f->pulses[ i + 1 ]->num );
			}
		}
	}
}


/*----------------------------------------------------------------------------
  Function creates all active pulses in the channels of the pulser assigned
  to the function passed as argument.
----------------------------------------------------------------------------*/

void hfs9000_set_pulses( FUNCTION *f )
{
	PULSE *p;
	Ticks start, end;
	int i;


	fsc2_assert( f->self != PULSER_CHANNEL_PHASE_1 &&
				 f->self != PULSER_CHANNEL_PHASE_2 );

	if ( f->channel->self == HFS9000_TRIG_OUT )
	{
		if ( f->pulses != NULL )
		{
			hfs9000_set_trig_out_pulse( );
			f->pulses[ 0 ]->was_active = f->pulses[ 0 ]->is_active = SET;
		}
		return;
	}

	/* Set the channels to zero. */

	hfs9000_set_constant( f->channel->self, 0, MAX_PULSER_BITS, 0 );

	/* Now simply run through all active pulses of the channel */

	for ( i = 0; i < f->num_pulses; i++ )
	{
		/* Set the area of the pulse itself */

		p = f->pulses[ i ];
		if ( ! p->is_active )
			continue;
		start = p->pos + f->delay;
		end = p->pos + p->len + f->delay;

		if ( start != end )
			hfs9000_set_constant( p->channel->self, start, end - start, 1 );

		p->was_active = p->is_active;
	}
}


/*----------------------------------------------------------------------------
  Function is called after the test run to reset all the variables describing
  the state of the pulser to their initial values
----------------------------------------------------------------------------*/

void hfs9000_full_reset( void )
{
	PULSE *p = hfs9000_Pulses;


	while ( p != NULL )
	{
		/* First we check if the pulse has been used at all, send a warning
           and delete it if it hasn't (unless we haven't ben told to keep
		   all pulses, even unused ones) */

		if ( ! p->has_been_active && ! hfs9000.keep_all )
		{
			eprint( WARN, UNSET, "%s: Pulse %ld is never used.\n",
					pulser_struct.name, p->num );
			p = hfs9000_delete_pulse( p );
			continue;
		}

		/* Reset pulse properies to their initial values */

		p->pos = p->initial_pos;
		p->is_pos = p->initial_is_pos;
		p->len = p->initial_len;
		p->is_len = p->initial_is_len;
		p->dpos = p->initial_dpos;
		p->is_dpos = p->initial_is_dpos;
		p->dlen = p->initial_dlen;
		p->is_dlen = p->initial_is_dlen;

		p->needs_update = UNSET;

		p->is_old_pos = p->is_old_len = UNSET;

		p->is_active = ( p->is_pos && p->is_len && p->len > 0 );

		p = p->next;
	}

	hfs9000.is_running = hfs9000.has_been_running;
}


/*----------------------------------------------------------------------------
  Function deletes a pulse and returns a pointer to the next pulse in the
  pulse list.
----------------------------------------------------------------------------*/

static PULSE *hfs9000_delete_pulse( PULSE *p )
{
	PULSE *pp;
	int i;


	/* First we've got to remove the pulse from its functions pulse list */

	for ( i = 0; i < p->function->num_pulses; i++ )
		if ( p->function->pulses[ i ] == p )
			break;

	fsc2_assert( i < p->function->num_pulses );             /* Paranoia */

	/* Put the last of the functions pulses into the slot for the pulse to
	   be deleted and shorten the list by one element */

	if ( i != p->function->num_pulses - 1 )
		p->function->pulses[ i ] = 
			p->function->pulses[ p->function->num_pulses - 1 ];
	p->function->pulses = T_realloc( p->function->pulses,
									--p->function->num_pulses
									* sizeof( PULSE * ) );

	/* If the deleted pulse was the last pulse of its function send a warning
	   and mark the function as useless */

	if ( p->function->num_pulses == 0 )
	{
		eprint( SEVERE, UNSET, "%s: Function `%s' isn't used at all because "
				"all its pulses are never used.\n", pulser_struct.name,
				Function_Names[ p->function->self ] );
		p->function->is_used = UNSET;
	}

	/* Now remove the pulse from the pulse list */

	if ( p->next != NULL )
		p->next->prev = p->prev;
	pp = p->next;
	if ( p->prev != NULL )
		p->prev->next = p->next;
	T_free( p );

	return pp;
}


/*----------------------------------------------------------------------------
  Changes the pulse pattern in the channels belonging to function 'f' so that
  the data in the pulser get in sync with the its internal representation.
  Some care has taken to minimize the number of commands and their length.
----------------------------------------------------------------------------*/

static void hfs9000_commit( FUNCTION *f, bool flag )
{
	PULSE *p;
	int i;
	Ticks start, len;
	int what;


	/* In a test run just reset the flags of all pulses that say that the
	   pulse needs updating */

	if ( flag )
	{
		for ( i = 0; i < f->num_pulses; i++ )
		{
			p = f->pulses[ i ];
			p->needs_update = UNSET;
			p->was_active = p->is_active;
			p->is_old_pos = p->is_old_len = UNSET;
		}

		return;
	}

	/* In a real run we now have to change the pulses. The only way to keep
	   the number and length of commands to be sent to the pulser at a minimum
	   while getting it right in every imaginable case is to create two images
	   of the pulser channel states, one before the changes and a second one
	   after the changes. These images are compared and only that parts where
	   differences are found are changed. Of course, that needs quite some
	   computer time but probable is faster, or at least easier to understand
	   and to debug, than any alternative I came up with...

	   First allocate memory for the old and the new states of the channels
	   used by the function */

	f->channel->old = T_calloc( 2 * hfs9000.max_seq_len, 1 );
	f->channel->new = f->channel->old + hfs9000.max_seq_len;

	/* Now loop over all pulses and pick the ones that need changes */

	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];

		if ( ! p->needs_update )
		{
			p->is_old_len = p->is_old_pos = UNSET;
			p->was_active = p->is_active;
			continue;
		}

		if ( f->channel->self != HFS9000_TRIG_OUT )
		{
			if ( p->is_old_pos || ( p->is_old_len && p->old_len != 0 ) )
				hfs9000_set( p->channel->old,
							 p->is_old_pos ? p->old_pos : p->pos,
							 p->is_old_len ? p->old_len : p->len, f->delay );
			if ( p->is_pos && p->is_len && p->len != 0 )
				hfs9000_set( p->channel->new, p->pos, p->len, f->delay );
		}

		p->channel->needs_update = SET;

		p->is_old_len = p->is_old_pos = UNSET;
		if ( p->is_active )
			p->was_active = SET;
		p->needs_update = UNSET;
	}

	/* Loop over all channels belonging to the function and for each channel
	   that needs to be changeda find all differences between the old and the
	   new state by repeatedly calling hfs9000_diff() - it returns +1 or -1 for
	   setting or resetting plus the start and length of the different area or
	   0 if no differences are found anymore. For each difference set the real
	   pulser channel accordingly. */

	if ( f->channel->needs_update )
	{
		if ( f->channel->self == HFS9000_TRIG_OUT )
			hfs9000_set_trig_out_pulse( );
		else
			while ( ( what = hfs9000_diff( f->channel->old, f->channel->new, 
										   &start, &len ) ) != 0 )
				hfs9000_set_constant( f->channel->self, start, len,
									  what == -1 ? 0 : 1 );
	}

	f->channel->needs_update = UNSET;
	T_free( f->channel->old );
}
