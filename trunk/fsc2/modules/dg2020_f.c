/*
  $Id$
*/



#define DG20202_MINIMUM_TIMEBASE 5          /* 5 ns */
#define DG20202_MAXIMUM_TIMEBASE 100000000  /* 0.1 s */


#include "fsc2.h"


int dg2020_init_hook( void );
int dg2020_test_hook( void );
int dg2020_exp_hook( void );
void dg2020_exit_hook( void );

Var *pulser_set_timebase( Var *v );



static void pulse_setup_test( void );


static bool dg2020_is_needed;

typedef struct
{
	long timebase;
} DG2020;

static DG2020 dg2020 = { 5 };


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

	pulse_setup_test( );
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


Var *pulser_set_timebase( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( VALUE( v ) < 0 )
	{
		eprint( FATAL, "dg2020: Negative time timebase for pulser.\n" );
		THROW( EXCEPTION );
	}

	if ( VALUE( v ) > DG20202_MINIMUM_TIMEBASE )
	{
		eprint( FATAL, "dg2020: Timebase for pulser too small, maximum is "
				"%ld ns.\n", DG20202_MINIMUM_TIMEBASE );
		THROW( EXCEPTION );
	}

	if ( VALUE( v ) > DG20202_MAXIMUM_TIMEBASE )
	{
		eprint( FATAL, "dg2020: Timebase for pulser too large, maximum is "
				"%ld ns.\n", DG20202_MAXIMUM_TIMEBASE );
		THROW( EXCEPTION );
	}

	dg2020.timebase = VALUE( v );
	return vars_push( INT_VAR, 1 );
}


void pulse_setup_test( void )
{
}
