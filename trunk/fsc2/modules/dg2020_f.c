/*
  $Id$
*/


#include "fsc2.h"


int dg2020_init_hook( void );
int dg2020_test_hook( void );
int dg2020_exp_hook( void );
void dg2020_exit_hook( void );


static void pulse_setup( void );


static bool dg2020_is_needed;



int dg2020_init_hook( void )
{
	dg2020_is_needed = SET;
	return 1;
}


int dg2020_test_hook( void )
{
	if ( Plist == NULL )
	{
		dg2020_is_needed = UNSET;
		eprint( WARN, "DG2020 loaded but no pulses defined.\n" );
		return 1;
	}

	pulse_setup( );
	return 1;
}

int dg2020_exp_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

	return 1;
}

void dg2020_exit_hook( void )
{
	if ( ! dg2020_is_needed )
		return;
}



void pulse_setup( void )
{
}
