#include "fsc2.h"





/*----------------------------------------------*/
/* Extracts the pulse number from a pulse name. */
/* ->                                           */
/*    * pulse name string                       */
/* <-                                           */
/*    * pulse number or -1 on error             */
/*----------------------------------------------*/

Pulse *n2p( char *txt )
{
	char *tp, *t;
	int num;


	if ( *t == '.' )
		return Cur_Pulse;

	tp = t = get_string_copy( txt );
	while ( *t && *t != '.' )
		t++;

	if ( *t == '.' )
	    *txt = '\0';

	while( isdigit( *--t ) )
		;

	if ( isdigit( *++t ) )
	    num = atoi( t );
	else
		num = -1;          /* this should never happen... */

	free( tp );
	return pulse_find( num );
}


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

	p = T_malloc( sizeof( Pulse ) );

	p->prev = NULL;
	p->next = Plist;

	if ( Plist != NULL )
		Plist->prev = p;

	/* set pulse number and associate it with no function yet */

	p->num = num;
	p->set_flags = 0;
	p->n_rp = 0;

	Plist = p;

	return p;
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
			return p;
		p = p->next;
	}

	return NULL;
}


/*------------------------------------------------------------*/
/* This function is the main interface for setting properties */
/* with simple interger or float numbers of a pulse.          */
/* ->                                                         */
/*    * pointer to pulse                                      */
/*    * type of data to be set (cf. pulse.h for constants)    */
/*    * pointer with variable for data                        */
/*------------------------------------------------------------*/

void pulse_set( Pulse *p, int type, Var *v )
{
	if ( ! pulse_exist( p ) )
	{
		eprint( FATAL, "%s:%ld: Pulse does not exist.\n", Fname, Lc );
		THROW( PREPARATIONS_EXCEPTION );
	}

	vars_check( v, INT_VAR );

	switch ( type )
	{
		case P_FUNC :
			pulse_set_func( p, v );
			break;

		case P_POS :
			pulse_set_pos( p, v );
			break;

		case P_LEN :
			pulse_set_len( p, v );
			break;

		case P_DPOS :
			pulse_set_dpos( p, v );
			break;

		case P_DLEN :
			pulse_set_dlen( p, v );
			break;

		case P_MAXLEN :
			pulse_set_maxlen( p, v );
			break;

		default :                 /* this should never happen... */
			assert( 1 == 0 );
	}

	vars_pop( v );
}


Var *pulse_get_by_addr( Pulse *p, int type )
{
	const char *type_strings[ ] = { "Function", "Start", "Length",
									"Position change", "Length change",
									"Maximum Length" };
	int i, j;


	/* test that the referenced pulse exists */

	if ( ! pulse_exist( p ) )
	{
		eprint( FATAL, "%s:%ld: Pulse does not exist.\n", Fname, Lc );
		THROW( PREPARATIONS_EXCEPTION );
	}

	/* make sure the accessed property of the pulse has been set */

	if ( ! ( p->set_flags & type ) )
	{
		for ( j = 0, i = type; ! ( i & 1 ) && i != 0; i >>=  1 )
			j++;

		/* <PARANOIA> check for unreasonable value of type variable */

		if ( i != 1 )
		{
			eprint( FATAL, "fsc2: INTERNAL ERROR detected at %s.\n",
					__FILE__, __LINE__ );
			exit( EXIT_FAILURE );
		}

		/* </PARANOIA> */

		eprint( FATAL, "%s:%ld: %s of pulse %d has not been set.\n", 
				Fname, Lc, type_strings[ j ], p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	/* now return the appropriate data */

	switch ( type )
	{
		case P_FUNC :
			return vars_push( INT_VAR, p->func );

		case P_POS :
			if ( p->pos % Time_Unit )
				return vars_push( FLOAT_VAR,( double ) p->pos /
								            ( double ) Time_Unit );
			else
				return vars_push( INT_VAR, p->pos / Time_Unit );

		case P_LEN :
			if ( p->len % Time_Unit )
				return vars_push( FLOAT_VAR, ( double ) p->len /
								             ( double ) Time_Unit );
			else
				return vars_push( INT_VAR, p->len / Time_Unit );

		case P_DPOS :
			if ( p->dpos % Time_Unit )
				return vars_push( FLOAT_VAR, ( double ) p->dpos /
								             ( double ) Time_Unit );
			else
				return vars_push( INT_VAR, p->dpos / Time_Unit );

		case P_DLEN :
			if ( p->dlen % Time_Unit )
				return vars_push( FLOAT_VAR, ( double ) p->dlen /
								             ( double ) Time_Unit );
			else
				return vars_push( INT_VAR, p->dlen / Time_Unit );

		case P_MAXLEN :
			if ( p->maxlen % Time_Unit )
				return vars_push( FLOAT_VAR, ( double ) p->maxlen /
								             ( double ) Time_Unit );
			else
				return vars_push( INT_VAR, p->maxlen / Time_Unit );
	}

	assert( 1 == 0 );      /* this should never happen... */
	return NULL;
}


Var *pulse_get_by_num( int pnum, int type )
{
	Pulse *p;


	if ( ( p = pulse_find( pnum ) ) == NULL )
	{
		eprint( FATAL, "%s:%l: Pulse with number %d does not exists.\n",
				Fname, Lc, pnum );
		THROW( PREPARATIONS_EXCEPTION );
	}

	return pulse_get_by_addr( p, type );
}


bool pulse_exist( Pulse *p )
{
	Pulse *n = Plist;

	while ( n != NULL )
	{
		if ( n == p )
			return OK;
		n = n->next;
	}

	return FAIL;
}


void pulse_set_func( Pulse *p, Var *v )
{
	if ( v->type != INT_VAR )
	{
		eprint( FATAL, "%s:%ld: Invalid function type for pulse %d.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	if ( p->set_flags & P_FUNC )
	{
		eprint( SEVERE, "%s:%ld: Function of pulse %d has already been set, "
				" leaving it unchanged.\n", Fname, Lc, p->num );
		return;
	}


	if ( v->val.lval < PULSER_CHANNEL_MW ||
		 v->val.lval > PULSER_CHANNEL_RF_GATE )
	{
		eprint( FATAL, "%s:%ld: Invalid function type for pulse %d.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->func = v->val.lval;
	p->set_flags |= P_FUNC;
}


void pulse_set_pos( Pulse *p, Var *v )
{
	long val;


	if ( p->set_flags & P_POS )
	{
		eprint( SEVERE, "%s:%ld: Start position of pulse %d has already been "
				"set, leaving it unchanged.\n", Fname, Lc, p->num );
		return;
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
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
	p->set_flags |= P_POS;
}


void pulse_set_len( Pulse *p, Var *v )
{
	long val;


	if ( p->set_flags & P_LEN )
	{
		eprint( SEVERE, "%s:%ld: Length of pulse %d has already been set, "
				"leaving it unchanged.\n", Fname, Lc, p->num );
		return;
	}

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
	p->set_flags |= P_LEN;
}


void pulse_set_dpos( Pulse *p, Var *v )
{
	long val;


	if ( p->set_flags & P_DPOS )
	{
		eprint( SEVERE, "%s:%ld: Position change of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		return;
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	val = v->type == INT_VAR ? v->val.lval : rnd( v->val.dval );

	if ( val > LONG_MAX || val < LONG_MIN)
	{
		eprint( FATAL, "%s:%ld: Position change of pulse %d too large.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->dpos = val;
	p->set_flags |= P_DPOS;
}


void pulse_set_dlen( Pulse *p, Var *v )
{
	long val;


	if ( p->set_flags & P_DLEN )
	{
		eprint( SEVERE, "%s:%ld: Length change of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		return;
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	val = v->type == INT_VAR ? v->val.lval : rnd( v->val.dval );

	if ( val > LONG_MAX || val < LONG_MIN)
	{
		eprint( FATAL, "%s:%ld: Length change of pulse %d too large.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->dlen = val;
	p->set_flags |= P_DLEN;
}


void pulse_set_maxlen( Pulse *p, Var *v )
{
	long val;


	if ( p->set_flags & P_MAXLEN )
	{
		eprint( SEVERE, "%s:%ld: Maximum length of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		return;
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	val = v->type == INT_VAR ? v->val.lval : rnd( v->val.dval );

	if ( val > LONG_MAX || val < LONG_MIN)
	{
		eprint( FATAL, "%s:%ld: Maximum length of pulse %d too large.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->maxlen = val;
	p->set_flags |= P_MAXLEN;
}


