/*
  $Id$
*/



#include "dg2020.h"



/*---------------------------------------------------------------------------
  Function is called in test runs after pulses have been changed to check
  if the new settings are still ok. The function also has to take care of
  apply the resulting necessary changes to the phase cycling pulses.
----------------------------------------------------------------------------*/

void do_checks( void )
{
}


/*---------------------------------------------------------------------------
  Function is called in the experiment after pulses have been changed to
  update the pulser accordingly. No checking has to be done because this has
  already been done in the test run. But similar to test runs also the
  pulses used for phase cycling have to be updated (both in memory and
  in reality in the pulser).
----------------------------------------------------------------------------*/

void do_update( void )
{



	/* Finally commit all changes */

	dg2020_update_data( );
}
