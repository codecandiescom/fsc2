/*
  $Id$
*/


#include "fsc2.h"

int User_Functions_init_hook( void );
int User_Functions_test_hook( void );
int User_Functions_end_of_test_hook( void );
int User_Functions_exp_hook( void );
int User_Functions_end_of_exp_hook( void );
void User_Functions_exit_hook( void );


Var *square( Var *var );
Var *int_slice( Var *var );
Var *float_slice( Var *var );



/* Here examples for init and exit hook functions - the init hook function
   will be called directly after all libraries are loaded while the exit
   hook function is called immediately before the library is unloaded.
*/

int User_Functions_init_hook( void )
{
	return 1;
}

int User_Functions_test_hook( void )
{
	return 1;
}

int User_Functions_end_of_test_hook( void )
{
	return 1;
}

int User_Functions_exp_hook( void )
{
	return 1;
}

int User_Functions_end_of_exp_hook( void )
{
	return 1;
}

void User_Functions_exit_hook( void )
{
}



/****************************************************************/
/* Enter the definition of all needed functions below this line */
/****************************************************************/


				 
Var *square( Var *var )
{
	vars_check( var, INT_VAR | FLOAT_VAR );

	if ( var->type == INT_VAR )
		return vars_push( INT_VAR, var->INT * var->INT );
	else
		return vars_push( FLOAT_VAR, var->FLOAT * var->FLOAT );
}


Var *int_slice( Var *var )
{
	long *array;
	long size;
	Var *ret;


	vars_check( var, INT_VAR | FLOAT_VAR );

	if ( var->type == INT_VAR )
		size = var->INT;
	else
		size = ( long ) var->FLOAT;

	array = T_calloc( size, sizeof( long ) );
	ret = vars_push( INT_TRANS_ARR, array, size );
	T_free( array );

	return ret;
}


Var *float_slice( Var *var )
{
	long *array;
	long size;
	Var *ret;


	vars_check( var, INT_VAR | FLOAT_VAR );

	if ( var->type == INT_VAR )
		size = var->INT;
	else
		size = ( long ) var->FLOAT;

	array = T_calloc( size, sizeof( double ) );
	ret = vars_push( FLOAT_TRANS_ARR, array, size );
	T_free( array );

	return ret;
}


Var *get_phase_cycled_area_1d( Var *v )
{
	Var *func_ptr;
	Var *vn;                         /* all purpose variable */
	bool is_get_area;
	bool is_get_area_fast;
	long acq_seq;
	long channel;
	long num_windows;
	long *win_list;
	long seq_len;
	long i;


	/* At the very start lets figure out if there are functions for phase
       cycling and for obtaining an area */

	if ( ( funct_ptr = func_get( "pulser_next_phase" ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): No pulser module "
				"loaded supplying a function to do phase cycling.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	if ( ( func_ptr = func_get( "pulser_phase_reset" ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): No pulser module "
				"loaded supplying a function to do phase cycling.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	if ( ( func_ptr = func_get( "pulser_update" ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): No pulser module "
				"loaded supplying a function to do phase cycling.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	if ( ( func_ptr = func_get( "digitizer_start_acquisition" ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): No digitizer "
				"module loaded supplying a function to do an acquisition.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	is_get_area = ( ( func_ptr = func_get( "digitizer_get_area" ) ) == NULL ) ?
		                                                           UNSET : SET;
	if ( is_get_area )
		vars_pop( func_ptr );

	is_get_area_fast = ( ( func_ptr = func_get( "digitizer_get_area_fast" ) )
						 == NULL ) ? UNSET : SET;
	if ( is_get_area_fast )
		vars_pop( func_ptr );

	if ( ! is_get_area && ! is_get_area_fast )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): No digitizer "
				"module loaded supplying a function to obtain an area.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* We also need to check that a phase sequence and at least one
	   acquisition sequence has been defined */

	if ( PSeq == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): No phase sequence "
				"has been defined.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	seq_len = PSeq->len;

	if ( ! ASeq[ 0 ].defined && ! ASeq[ 1 ].defined )

	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): No acquisition "
				"sequence has been defined.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Next step: check the parameter */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Missing "
				"arguments.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* First parameter is the acqusition sequence number */

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == INT_VAR )
		acq_seq = v->val.lval;
	else if ( v->type == INT_VAR )
	{
		eprint( WARN, "%s:%ld: get_phase_cycled_area_1d(): Floating point "
				"value used as acquisition sequence number.\n", Fname, Lc );
		acq_seq = lround( v->val.dval );
	}
	else
	{
		if ( ! strcmp( v->val.sptr, "A" ) || ! strcmp( v->val.sptr, "a" ) )
			acq_seq = 1;
		else if ( ! strcmp( v->val.sptr, "B" ) || 
				  ! strcmp( v->val.sptr, "b" ) )
			acq_seq = 2;
		else
		{
			eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Invalid "
					"acqusisition type string: %s\n", Fname, Lc, v->val.sptr );
			THROW( EXCEPTION );
		}
	}

	if ( acq_seq != 1 && acq_seq != 2 )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Invalid "
				"acquisition sequence number: %ld.\n", Fname, Lc, acq_seq );
		THROW( EXCEPTION );
	}

	acq_seq--;

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Missing second"
				"argument (i.e. chnnel number).\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Check that the acquisition sequence contains only references to the
	   channel associated with the acquisition sequence, i.e. if the
	   acquisition sequence is B only the B channel may be used (and vice
	   versa) */

	for ( i = 0; i < ASeq[ acq_seq ].len; i++ )
	{
		if ( acq_seq == 0 && 
			 ( ASeq[ acq_seq ].sequence[ i ] == ACQ_PLUS_B ||
			   ASeq[ acq_seq ].sequence[ i ] == ACQ_MINUS_B ) )
		{
			eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Acquisition "
					"sequence A would need access to B channel.\n",
					Fname, LC );
			THROW( EXCEPTION );
		}
		if ( acq_seq == 1 && 
			 ( ASeq[ acq_seq ].sequence[ i ] == ACQ_PLUS_A ||
			   ASeq[ acq_seq ].sequence[ i ] == ACQ_MINUS_A ) )
		{
			eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Acquisition "
					"sequence B would need access to A channel.\n",
					Fname, LC );
			THROW( EXCEPTION );
		}
	}

	/* The next parameter is the channel number - hopefully, it was specified
	   by a symbol and the digitier module has already ok'ed it, but we can't
	   be sure... */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		channel = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: get_phase_cycled_area_1d(): Floating point "
				"number used as channel number.\n", Fname, Lc );
		channel = lround( v->val.dval );
	}

	if ( ( func_ptr = func_get( "digitizer_meas_channel_ok" ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Digitizer module "
				"does not supply function needed.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_push( INT_VAR, channel );
	vn = vars_call( func_ptr );

	if ( vn->val.lval == 0 )
	{
		vars_pop( vn );
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Invalid channel "
				"number: %ld.\n", Fname, Lc, channel );
		THROW( EXCEPTION );
	}

	vars_pop( vn );

	/* Now we got to count the remaining arguments on the stack to find out
	   how many windows there are and build up the window list */

	num_windows = 1;
	vn = v = vars_pop( v );

	while ( ( vn = vn->next ) != NULL )
		num_windows++;

	win_list = T_malloc( num_windows * sizeof( long ) );

	if ( v !== NULL )
	{
		for ( i = 0; i < num_windows; i++ )
		{
			TRY
			{
				vars_check( v, INT_VAR | FLOAT_VAR );
				TRY_SUCCESS;
			}
			CATCH( EXCEPTION )
			{
				T_free( win_list );
				THROW( EXCEPTION );
			}

			if ( v->type == INT_VAR )
				win_list[ i ] = v->val.lval;
			else
			{
				eprint( WARN, "%s:%ld: get_phase_cycled_area_1d(): Floating "
						"point value used as window number.\n", Fname, c );
				win_list[ i ] = lround( v->val.dval );
			}

			v = vars_pop( v );
		}
	}
	else
		win_list[ 0 ] = -1;              /* No window to be used ! */


	/* Also get memory for the data */

	data = T_malloc( num_windows * sizeof( double ) );
	for ( i = 0; i < num_windows; i++ )
		data[ i ] = 0.0;

	/* Now we can really start. First we reset the phase sequence, then we
	   run through the complete phase cycle */

	vars_pop( vars_call( func_get( "pulser_phase_reset" ) ) );
	vars_pop( vars_call( func_get( "pulser_update" ) ) );

	for ( i = 0; i < seq_len; i++ )
	{
		vars_pop( vars_call( func_get( "digitizer_start_acquisition" ) ) );
		for ( j = 0; j < num_windows; j++ )
		{
			if ( is_get_area_fast )
			{
				if ( win_list[ i ] >= 0 )
					vn = digitizer_get_area_fast(
						vars_push( INT_VAR, channel ),
						vars_push( INT_VAR, win_list[ j ] ) );
				else
					vn = digitizer_get_area_fast(
						vars_push( INT_VAR, channel ) );
			}
			else
			{
				if ( win_list[ i ] >= 0 )
					vn = digitizer_get_area(
						vars_push( INT_VAR, channel ),
						vars_push( INT_VAR, win_list[ j ] ) );
				else
					vn = digitizer_get_area_( vars_push( INT_VAR, channel ) );
			}
		}

		if ( ASeq[ acq_seq ].sequence[ i ] = ACQ_PLUS_A ||
			 ASeq[ acq_seq ].sequence[ i ] = ACQ_PLUS_B )
			data[ i ] += vn->val.dval;
		else
			data[ i ] -= vn->val.dval;

		vars_pop( vn );

		vars_pop( vars_call( func_get( "pulser_next_phase" ) ) );
		vars_pop( vars_call( func_get( "pulser_update" ) ) );
	}

	vn = vars_push( FLOAT_TRANS_ARR, data, seq_len );
	T_free( data );
	T_free( win_list );

	return vn;
}
