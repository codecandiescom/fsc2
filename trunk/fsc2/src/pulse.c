#include "fsc2.h"


/* Creates a new pulse and appends it to the pulse list. */

Pulse *pulse_new( int num )
{
	Pulse *p;


	/* check that the pulse does not already exists */

	if ( pulse_find( num ) != NULL )
	{
		eprint( FATAL, "%s:%ld: Pulse with number %d already exists.\n",
				Fname, Lc, num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	/* allocate memory for the new pulse, set its number and link it
	   to the top of the pulse list */

	p = ( Pulse * ) T_malloc( sizeof( Pulse ) );

	p->prev = NULL;
	p->next = Plist;

	if ( Plist != NULL )
		Plist->prev = p;

	/* set pulse number and associate it with no function yet */

	p->num = num;
	p->func = PULSER_CHANNEL_NO_TYPE;
	p->pos = p->len = p->dpos = p->dlen = p->maxlen = 0;
	p->rp = NULL;
	p->n_rp = 0;

	Plist = p;

	return( p );
}



/*-----------------------------------------------------------------*/
/* Trys to find a pulse according to its number in the pulse list, */
/* also checks that the number is in the allowed range.            */
/* ->                                                              */
/*    number of pulse                                              */
/* <-                                                              */
/*    pointer to pulse structure or NULL if not found              */
/*-----------------------------------------------------------------*/

Pulse *pulse_find( int num )
{
	Pulse *p = Plist;

	/* check that the pulse number is in the allowed range */

	if ( num < 0 || num >= MAX_PULSE_NUM )
	{
		eprint( FATAL, "%s:%ld: Pulse number %d out of range (0-%d).\n",
				Fname, Lc, num, MAX_PULSE_NUM - 1 );
		THROW( PREPARATIONS_EXCEPTION );
	}

	while ( p != NULL )
	{
		if ( p->num == num )
			return( p );
		p = p->next;
	}

	return( NULL );
}


void pulse_set_func( Pulse *p, long func )
{
	if ( p->func != PULSER_CHANNEL_NO_TYPE )
	{
		eprint( FATAL, "%s:%ld: Function of pulse %d is already set.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	if ( func < PULSER_CHANNEL_MW || func > PULSER_CHANNEL_RF_GATE )
	{
		eprint( FATAL, "%s:%ld: Invalid function type for pulse %d.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->func = func;
}


void pulse_set_start( Pulse *p, Var *v )
{
	long val;


	if ( p->pos != 0 )
	{
		eprint( SEVERE, "%s:%ld: Start position of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		vars_pop( v );
		return;
	}

	vars_check( v );
	val = v->type == INT_VAR ? v->val.lval : rnd( v->val.dval );

	if ( val < 0 )
	{
		eprint( FATAL, "%s:%ld: Negative start position of pulse %d.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	if ( val > LONG_MAX )
	{
		eprint( FATAL, "%s:%ld: Start position of pulse %d too large.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->pos = val;
	vars_pop( v );
}


void pulse_set_len( Pulse *p, Var *v )
{
	long val;


	if ( p->len != 0 )
	{
		eprint( SEVERE, "%s:%ld: Length of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		vars_pop( v );
		return;
	}

	vars_check( v );
	val = v->type == INT_VAR ? v->val.lval : rnd( v->val.dval );

	if ( val < 0 )
	{
		eprint( FATAL, "%s:%ld: Negative length of pulse %d.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	if ( val > LONG_MAX )
	{
		eprint( FATAL, "%s:%ld: Length of pulse %d too large.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->len = val;
	vars_pop( v );
}


void pulse_set_dpos( Pulse *p, Var *v )
{
	long val;


	if ( p->dpos != 0 )
	{
		eprint( SEVERE, "%s:%ld: Position change of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		vars_pop( v );
		return;
	}

	vars_check( v );
	val = v->type == INT_VAR ? v->val.lval : rnd( v->val.dval );

	if ( val > LONG_MAX || val < LONG_MIN)
	{
		eprint( FATAL, "%s:%ld: Position change of pulse %d too large.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->dpos = val;
	vars_pop( v );
}


void pulse_set_dlen( Pulse *p, Var *v )
{
	long val;


	if ( p->dlen != 0 )
	{
		eprint( SEVERE, "%s:%ld: Length change of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		vars_pop( v );
		return;
	}

	vars_check( v );
	val = v->type == INT_VAR ? v->val.lval : rnd( v->val.dval );

	if ( val > LONG_MAX || val < LONG_MIN)
	{
		eprint( FATAL, "%s:%ld: Length change of pulse %d too large.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->dlen = val;
	vars_pop( v );
}


void pulse_set_maxlen( Pulse *p, Var *v )
{
	long val;


	if ( p->maxlen != 0 )
	{
		eprint( SEVERE, "%s:%ld: Maximum length of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		vars_pop( v );
		return;
	}

	vars_check( v );
	val = v->type == INT_VAR ? v->val.lval : rnd( v->val.dval );

	if ( val > LONG_MAX || val < LONG_MIN)
	{
		eprint( FATAL, "%s:%ld: Maximum length of pulse %d too large.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->maxlen = val;
	vars_pop( v );
}
