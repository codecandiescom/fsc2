/*
  $Id$
*/


#include "fsc2.h"


/* local functions */

static Var *get_pulse_by_addr_prop( Pulse *p, int prop );
static bool pulse_exist( Pulse *p );
static void pulse_set_func( Pulse *p, Var *v );
static void pulse_set_pos( Pulse *p, Var *v );
static void pulse_set_len( Pulse *p, Var *v );
static void pulse_set_dpos( Pulse *p, Var *v );
static void pulse_set_dlen( Pulse *p, Var *v );
static void pulse_set_maxlen( Pulse *p, Var *v );
static void pulse_set_repl( Pulse *p, Var *v );
static void sort_pulse_list( void );


/*----------------------------------------------*/
/* Extracts the pulse number from a pulse name. */
/* Don't call with inval string, i.e. one that  */
/* doesn't contain a number.                    */
/* ->                                           */
/*    * pulse name string                       */
/* <-                                           */
/*    * pulse number                            */
/*----------------------------------------------*/

int get_pulse_num( char *txt )
{
	long num;


	while ( txt != NULL && ! isdigit( *txt ) )
		txt++;

	assert( txt != NULL );          /* Paranoia ? */

	num = strtol( txt, NULL, 10 );
	if ( errno == ERANGE || num >= MAX_PULSE_NUM )
	{
		eprint( FATAL, "%s:%ld: Pulse number (%s) out of range "
				"(allowed: 0-%d).\n", Fname, Lc, txt, MAX_PULSE_NUM - 1 );
		THROW( EXCEPTION );
	}

	return ( int ) num;
}


/*-------------------------------------------------------*/
/* Creates a new pulse and appends it to the pulse list. */
/*-------------------------------------------------------*/

Pulse *pulse_new( char *txt )
{
	Pulse *p;
	int num = get_pulse_num( txt );


	/* Check that the pulse does not already exists */

	if ( pulse_find( num ) != NULL )
	{
		eprint( FATAL, "%s:%ld: Pulse %d already exists.\n",
				Fname, Lc, num );
		THROW( EXCEPTION );
	}

	/* Allocate memory for the new pulse, set its number and link it
	   to the top of the pulse list */

	p = T_malloc( sizeof( Pulse ) );

	p->prev = NULL;
	p->next = Plist;

	if ( Plist != NULL )
		Plist->prev = p;

	/* Set pulse number and associate it with no function yet */

	p->num = num;
	p->set_flags = 0;
	p->rp = NULL;
	p->nrp = NULL;
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

	/* Check that the pulse number is in the allowed range */

	if ( num < 0 || num >= MAX_PULSE_NUM )
	{
		eprint( FATAL, "%s:%ld: Pulse number (%d) out of range "
				"(allowed: 0-%d).\n", Fname, Lc, num, MAX_PULSE_NUM - 1 );
		THROW( EXCEPTION );
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
/* with simple integer or float numbers of a pulse.           */
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
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	switch ( type )
	{
		case P_FUNC :
			vars_check( v, INT_VAR );
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

		case P_REPL :
			pulse_set_repl( p, v );
			break;

		default :                 /* this should never happen... */
			assert( 1 == 0 );
	}

	vars_pop( v );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *get_pulse_by_addr_prop( Pulse *p, int prop )
{
	const char *prop_strings[ ] = { "Function", "Start", "Length",
									"Position change", "Length change",
									"Maximum Length" };
	int i, j;


	/* Test that the referenced pulse exists */

	if ( ! pulse_exist( p ) )
	{
		eprint( FATAL, "%s:%ld: Pulse does not exist.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Make sure the accessed property of the pulse has been set */

	if ( ! ( p->set_flags & prop ) )
	{
		for ( j = 0, i = prop; ! ( i & 1 ) && i != 0; i >>=  1 )
			j++;

		/* Check for unreasonable value of property variable */

		if ( i != 1 )
		{
			eprint( FATAL, "fsc2: INTERNAL ERROR detected at %s:%d\n",
					__FILE__, __LINE__ );
			exit( EXIT_FAILURE );
		}

		/* </PARANOIA> */

		eprint( FATAL, "%s:%ld: %s of pulse %d has not been set.\n", 
				Fname, Lc, prop_strings[ j ], p->num );
		THROW( EXCEPTION );
	}

	/* Now return the appropriate data */

	switch ( prop )
	{
		case P_FUNC :
			return vars_push( INT_VAR, p->func );

		case P_POS :
			return vars_push( FLOAT_VAR, p->pos * 1.0e-9 );

		case P_LEN :
			return vars_push( FLOAT_VAR, p->len * 1.0e-9 );

		case P_DPOS :
			return vars_push( FLOAT_VAR, p->dpos * 1.0e-9 );

		case P_DLEN :
			return vars_push( FLOAT_VAR, p->dlen * 1.0e-9 );

		case P_MAXLEN :
			return vars_push( FLOAT_VAR, p->maxlen * 1.0e-9 );
	}

	assert( 1 == 0 );      /* this should never happen... */
	return NULL;
}


/*-----------------------------------------------------------*/
/* Returns the specified property of a pulse (as a variable) */
/*-----------------------------------------------------------*/

Var *pulse_get_prop( char *txt, int prop )
{
	int num;
	Pulse *cp;


	num = get_pulse_num( txt );
	cp = pulse_find( num );
	if ( cp == NULL )
	{
		eprint( FATAL, "%s:%ld: Referenced pulse %d does not exist.\n",
				Fname, Lc, num );
		THROW( EXCEPTION );
	}

	return get_pulse_by_addr_prop( cp, prop );
}


/*--------------------------*/
/* Checks if a pulse exists */
/*--------------------------*/

bool pulse_exist( Pulse *p )
{
	Pulse *n;

	for ( n = Plist; n != NULL; n = n->next )
		if ( n == p )
			return OK;

	return FAIL;
}


/*------------------------------*/
/* Sets the function of a pulse */
/*------------------------------*/

void pulse_set_func( Pulse *p, Var *v )
{
	if ( v->type != INT_VAR ||
		 v->val.lval < PULSER_CHANNEL_FUNC_MIN ||
		 v->val.lval > PULSER_CHANNEL_FUNC_MAX )
	{
		eprint( FATAL, "%s:%ld: Invalid function type for pulse %d.\n",
				Fname, Lc, p->num );
		THROW( EXCEPTION );
	}

	if ( p->set_flags & P_FUNC )
	{
		eprint( SEVERE, "%s:%ld: Function of pulse %d has already been set, "
				" leaving it unchanged.\n", Fname, Lc, p->num );
		return;
	}

	p->func = v->val.lval;
	p->set_flags |= P_FUNC;
}


/*------------------------------*/
/* Sets the position of a pulse */
/*------------------------------*/

void pulse_set_pos( Pulse *p, Var *v )
{
	double val;


	if ( p->set_flags & P_POS )
	{
		eprint( SEVERE, "%s:%ld: Start position of pulse %d has already been "
				"set, leaving it unchanged.\n", Fname, Lc, p->num );
		return;
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	val = ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval;

	if ( val < 0.0 )
	{
		eprint( FATAL, "%s:%ld: Negative start position of pulse %d.\n",
				Fname, Lc, p->num );
		THROW( EXCEPTION );
	}

	if ( val > LONG_MAX * 1.0e-9 )
	{
		eprint( FATAL, "%s:%ld: Start position of pulse %d is too large, "
				"maximum is %f s.\n", Fname, Lc, p->num, LONG_MAX * 1.0e-9 );
		THROW( EXCEPTION );
	}

	p->pos = lround( 1.0e9 * val );
	p->set_flags |= P_POS;
}


/*----------------------------*/
/* Sets the length of a pulse */
/*----------------------------*/

void pulse_set_len( Pulse *p, Var *v )
{
	double val;


	if ( p->set_flags & P_LEN )
	{
		eprint( SEVERE, "%s:%ld: Length of pulse %d has already been set, "
				"leaving it unchanged.\n", Fname, Lc, p->num );
		return;
	}

	val = ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval;

	if ( val < 0.0 )
	{
		eprint( FATAL, "%s:%ld: Negative length of pulse %d.\n",
				Fname, Lc, p->num );
		THROW( EXCEPTION );
	}

	if ( val > LONG_MAX * 1.0e-9 )
	{
		eprint( FATAL, "%s:%ld: Length of pulse %d is too large, maximum is "
		               "%f s.\n", Fname, Lc, p->num, LONG_MAX * 1.0e-9 );
		THROW( EXCEPTION );
	}

	p->len = lround( val * 1.0e9 );
	p->set_flags |= P_LEN;
}


/*-------------------------------------*/
/* Sets the position change of a pulse */
/*-------------------------------------*/

void pulse_set_dpos( Pulse *p, Var *v )
{
	double val;


	if ( p->set_flags & P_DPOS )
	{
		eprint( SEVERE, "%s:%ld: Position increment of pulse %d is already "
				"set, using previously defined value.\n", Fname, Lc, p->num );
		return;
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	val = ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval;

	if ( val > LONG_MAX * 1.0e-9 || val < LONG_MIN * 1.0e-9 )
	{
		eprint( FATAL, "%s:%ld: Position change of pulse %d is too large, "
				"maximum is %f s.\n", Fname, Lc, p->num, LONG_MAX * 1.0e-9 );
		THROW( EXCEPTION );
	}

	p->dpos = lround( val * 1.0e9 );
	p->set_flags |= P_DPOS;
}


/*-----------------------------------*/
/* Sets the length change of a pulse */
/*-----------------------------------*/

void pulse_set_dlen( Pulse *p, Var *v )
{
	double val;


	if ( p->set_flags & P_DLEN )
	{
		eprint( SEVERE, "%s:%ld: Length increment of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		return;
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	val = ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval;

	if ( val > LONG_MAX * 1.0e-9 || val < LONG_MIN * 1.0e-9 )
	{
		eprint( FATAL, "%s:%ld: Length change of pulse %d is too large, "
				"maximum is %f s.\n", Fname, Lc, p->num, LONG_MAX * 1.0e-9 );
		THROW( EXCEPTION );
	}

	p->dlen = lround( val * 1.0e9 );
	p->set_flags |= P_DLEN;
}


/*-----------------------------------*/
/* Sets the aximum length of a pulse */
/*-----------------------------------*/

void pulse_set_maxlen( Pulse *p, Var *v )
{
	double val;


	if ( p->set_flags & P_MAXLEN )
	{
		eprint( SEVERE, "%s:%ld: Maximum length of pulse %d is already set, "
				"using previously defined value.\n", Fname, Lc, p->num );
		return;
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	val = ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval;

	if ( val > LONG_MAX * 1e-9 || val < LONG_MIN * 1.0e-9 )
	{
		eprint( FATAL, "%s:%ld: Maximum length of pulse %d is too large, "
				"maximum is %f s.\n", Fname, Lc, p->num, LONG_MAX * 1.0e-9 );
		THROW( EXCEPTION );
	}

	p->maxlen = lround( val * 1.0e9 );
	p->set_flags |= P_MAXLEN;
}


/*-------------------------------------------------------------------*/
/* Adds (another) pulse to the list of numbers of replacement pulses */
/*-------------------------------------------------------------------*/

void pulse_set_repl( Pulse *p, Var *v )
{
	/* No checking needed, already done by parser... */

	p->nrp = T_realloc( p->nrp, ( p->n_rp + 1 ) * sizeof( int ) );
	p->nrp[ p->n_rp++ ] = ( int ) v->val.lval;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void basic_pulse_check( void )
{
	Pulse *cp;
	int i;


	if ( Plist == NULL )
		return;

	sort_pulse_list( );

	for ( cp = Plist; cp != NULL; cp = cp->next )
	{
		/* Check the pulse replacement setup */

		if ( cp->n_rp != 0 )
		{
			if ( ! ( cp->set_flags & P_MAXLEN ) )
			{
				eprint( WARN, "Replacement pulses for pulse %d have been set "
						"but no maximum pulse length.\n", cp->num );
				cp->n_rp = 0;
				T_free( cp->nrp );
				cp->nrp = NULL;
				goto cont_check;
			}

			if ( cp->func != PULSER_CHANNEL_TWT_GATE &&
				 cp->func != PULSER_CHANNEL_RF_GATE )
			{
				eprint( FATAL, "Function of pulse %d is neither TWT_GATE nor "
						"RF_GATE but replacement pulses are defined.\n",
						cp->num );
				THROW( EXCEPTION );
			}

			cp->rp = T_malloc( cp->n_rp * sizeof( Pulse * ) );

			if ( cp->len > cp->maxlen )
			{
				eprint( WARN, "Length of pulse %d is already at start longer "
						"than the maximum length.\n", cp->num );
				cp->is_active = UNSET;
			}
			else
				cp->is_active = SET;

			for ( i = 0; i < cp->n_rp; i++ )
			{
				cp->rp[ i ] = pulse_find( cp->nrp[ i ] );

				if ( cp->rp[ i ] == cp )
				{
					eprint( FATAL, "Pulse %d can't be replaced by itself.\n",
							cp->num );
					THROW( EXCEPTION );
				}

				if ( cp->rp[ i ] == NULL )
				{
					eprint( FATAL, "Replacement pulse %d for pulse %d has not "
							"been defined.\n", cp->nrp[ i ], cp->num );
					THROW( EXCEPTION );
				}

				if ( cp->rp[ i ]->n_rp != 0 )
				{
					eprint( FATAL, "Replacement pulse %d for pulse %d itself "
							"has replacement pulses defined.\n",
							cp->nrp[ i ], cp->num );
					THROW( EXCEPTION );
				}

				if ( cp->rp[ i ]->set_flags & P_FUNC )
				{
					if ( cp->rp[ i ]->func != cp->func )
					{
						eprint( FATAL, "Function of replacement pulse %d is "
								"different from the one it has to replace "
								"(pulse %d).\n", cp->nrp[ i ], cp->num );
						THROW( EXCEPTION );
					}
				}
				else     /* function of replacement pulse has not been set ? */
				{
					Var *v;

					pulse_set_func( cp->rp[ i ],
									v = vars_push( INT_VAR, cp->func ) );
					vars_pop( v );
				}

				cp->rp[ i ]->is_active
					                  = ( cp->len > cp->maxlen ) ? SET : UNSET;
			}

			T_free( cp->nrp );
			cp->nrp = NULL;
		}
	}

    /* Now check if at least all relevant properties are set */

cont_check:

	for ( cp = Plist; cp != NULL; cp = cp->next )
	{
		if ( ! ( cp->set_flags & P_FUNC ) )
		{
			eprint( FATAL, "Function of pulse %d has not been defined.\n",
					cp->num );
			THROW( EXCEPTION );
		}

		if ( ! ( cp->set_flags & P_POS ) )
		{
			eprint( FATAL, "Start position of pulse %d has not been "
					"defined.\n", cp->num );
			THROW( EXCEPTION );
		}

		if ( ! ( cp->set_flags & P_LEN ) )
		{
			eprint( FATAL, "Length of pulse %d has not been defined.\n",
					cp->num );
			THROW( EXCEPTION );
		}

		if ( cp->set_flags & P_MAXLEN && cp->n_rp == 0 )
		{
			eprint( FATAL, "Maximum length of pulse %d has been defined "
					"but no replacement pulses.\n",
					cp->num );
			THROW( EXCEPTION );
		}
	}
}


/*--------------------------------------------------------------*/
/* Save or restore starting values of the pulses (for test run) */
/*--------------------------------------------------------------*/

void save_restore_pulses( bool flag )
{
	Pulse *cp;

	if ( flag )
		for ( cp = Plist; cp != NULL; cp = cp->next )
		{
			cp->store.pos       = cp->pos;
			cp->store.len       = cp->len;
			cp->store.dpos      = cp->dpos;
			cp->store.dlen      = cp->dlen;
			cp->store.is_active = cp->is_active;
		}
	else
		for ( cp = Plist; cp != NULL; cp = cp->next )
		{
			cp->pos       = cp->store.pos;
			cp->len       = cp->store.len;
			cp->dpos      = cp->store.dpos;
			cp->dlen      = cp->store.dlen;
			cp->is_active = cp->store.is_active;
		}
}


/*------------------------------------------------------------*/
/* Sorts the pulse list according to the pulse numbers - this */
/* is only done to print out possible error messages in a     */
/* reasonable sequence.                                       */
/*------------------------------------------------------------*/

void sort_pulse_list( void )
{
	Pulse *new_Plist = Plist;
	Pulse *cp, *cpn, *cpl, *cplp;


	cp = Plist->next;                   /* get second element */
	if ( cp == NULL )                   /* just one pulse in the list ? */
		return;

	new_Plist->next->prev = NULL;       
	new_Plist->next = NULL;
	
	for ( ; cp != NULL; cp = cpn )
	{
		cpn = cp->next;
		if ( cp->next != NULL )
			cp->next->prev = NULL;
		
		for ( cpl = new_Plist; cpl != NULL; cpl = cpl->next )
		{
			if ( cp->num < cpl->num )
				break;
			else
				cplp = cpl;
		}

		if ( cpl == new_Plist )     /* pulse at very start of list ? */
		{
			cp->prev = NULL;
			cp->next = new_Plist;
			cp->next->prev = cp;
			new_Plist = cp;
			continue;
		}

		if ( cpl == NULL )          /* pulse at end of list ? */
		{
			cplp->next = cp;
			cp->prev = cplp;
			cp->next = NULL;
			continue;
		}

		cp->next = cplp->next;
		cplp->next = cp->next->prev = cp;
		cp->prev = cplp;
		
	}

	Plist = new_Plist;
}


/*-----------------------------------------------------------------------*/
/* Deletes all pulses from tje pulse list and frees the allocated memory */
/*-----------------------------------------------------------------------*/

void delete_pulses( void )
{
	Pulse *cp, *cpn;

	for ( cp = Plist; cp != NULL; cp = cpn )
	{
		if ( cp->rp != NULL )
			T_free( cp->rp );
		if ( cp->nrp != NULL )
			T_free( cp->nrp );
		cpn = cp->next;
		T_free( cp );
	}

	Plist = NULL;
}
