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


Var *square( Var *v );
Var *int_slice( Var *v );
Var *float_slice( Var *v );
Var *get_phase_cycled_area_1d( Var *v );
Var *get_phase_cycled_area_2d( Var *v );


static bool get_channel_number( Var *v, const char *func_name, long *channel );
static void pc_basic_check( const char *func_name, const char *func_1,
							bool *is_1, const char *func_2, bool *is_2,
							const char *str );



/****************************************************************/
/* Enter the definition of all needed functions below this line */
/****************************************************************/


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/
				 
Var *square( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		return vars_push( INT_VAR, v->INT * v->INT );
	else
		return vars_push( FLOAT_VAR, v->FLOAT * v->FLOAT );
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var *int_slice( Var *v )
{
	long *array;
	long size;
	Var *ret;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		size = v->INT;
	else
		size = lround( v->FLOAT );

	array = T_calloc( size, sizeof( long ) );
	ret = vars_push( INT_TRANS_ARR, array, size );
	T_free( array );

	return ret;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

Var *float_slice( Var *v )
{
	long *array;
	long size;
	Var *ret;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		size = v->INT;
	else
		size = lround( v->FLOAT );

	array = T_calloc( size, sizeof( double ) );
	ret = vars_push( FLOAT_TRANS_ARR, array, size );
	T_free( array );

	return ret;
}


/*-----------------------------------------------------------------*/
/* This function always uses the first acquisition sequence.       */
/* Expected arguments:                                             */
/* 1. 1 or 2 digitizer channel numbers (if the second argument is  */
/*    a digitizer channel or a window number can be seen from the  */
/*    value, window numbers always start at WINDOW_START_NUMBER,   */
/*    which should be much larger than the highest channel number. */
/* 3. Any number of digitizer windows (none means just return area */
/*    of full width curve)                                         */
/* Return value:                                                   */
/*    For zero or 1 window: Floating point variable with phase     */
/*    cycled area                                                  */
/*    For more than 1 window: Array of floating point values with  */
/*    phase cycled area for each window                            */
/*-----------------------------------------------------------------*/

Var *get_phase_cycled_area_1d( Var *v )
{
	Var *func_ptr;
	Var *vn;                                /* all purpose variable */
	static bool first_time = SET;
	static bool is_get_area;
	static bool is_get_area_fast;
	bool is_channel;
	long channel[ 2 ];
	long num_windows;
	long *win_list;
	long i, j;
	int access;
	double *data;
	int channels_needed = 1;
	Acquisition_Sequence *aseq = ASeq;      /* first acquisition sequence */


	/* The first time we get here do some basic checks, i.e. check that all
	   needed functions are there */

	if ( first_time )
	{
		pc_basic_check( "get_phase_cycled_area_1d", "digitizer_get_area",
						&is_get_area, "digitizer_get_area_fast",
						&is_get_area_fast, "an area" );
		first_time = UNSET;
	}

	/* Next step: check the parameter */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Missing "
				"arguments.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Find out how many channels the first acquisition sequence needs - if
	   two are needed there must be two channel arguments */

	for ( i = 0; i < aseq->len; i++ )
		if ( aseq->sequence[ i ] == ACQ_PLUS_B ||
			 aseq->sequence[ i ] == ACQ_MINUS_B )
			channels_needed = 2;

	/* The first parameter must be a channel number in any case, ask digitizer
	   module if it really is one */

	if ( ! get_channel_number( v, "get_phase_cycled_area_1d", &channel[ 0 ] ) )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Invalid digitizer "
				"channel number: %ld.\n", Fname, Lc, channel[ 0 ] );
		THROW( EXCEPTION );
	}

	/* If we need two channels (acquisition sequence uses A and B) also the
	   next argument must be a channel number. If only one channel is needed
	   the next argument can be either a channel number or a window number */

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		is_channel = get_channel_number( v , "get_phase_cycled_area_1d",
										 &channel[ 1 ] );

		if ( channels_needed == 2 && ! is_channel )
		{
			eprint( FATAL, "%s:%ld: get_phase_cycled_area_1d(): Two digitizer "
					"channel nunmbers are needed but second argument isn't "
					"one.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		if ( channels_needed == 1 && is_channel )
			eprint( WARN, "%s:%ld: get_phase_cycled_area_1d(): Superfluous "
					"digitizer channel number found as second argument.\n",
					Fname, Lc );

		if ( is_channel )
			v = vars_pop( v );
	}

	/* Now we've got to count the remaining arguments on the stack to find out
	   how many windows there are and build up the window list */

	num_windows = 1;

	if ( v != NULL )
	{
		vn = v;
		while ( ( vn = vn->next ) != NULL )
			num_windows++;
	}

	win_list = T_malloc( num_windows * sizeof( long ) );

	TRY
	{
		if ( v != NULL )
		{
			for ( i = 0; i < num_windows; i++ )
			{
				vars_check( v, INT_VAR | FLOAT_VAR );

				if ( v->type == INT_VAR )
					win_list[ i ] = v->val.lval;
				else
				{
					eprint( WARN, "%s:%ld: get_phase_cycled_area_1d(): "
							"Floating point value used as window number.\n",
							Fname, Lc );
					win_list[ i ] = lround( v->val.dval );
				}

				v = vars_pop( v );
			}
		}
		else
			win_list[ 0 ] = -1;              /* No window is to be used ! */

		/* Also get memory for the data */

		data = T_malloc( num_windows * sizeof( double ) );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( win_list );
		PASSTHROU( );
	}

	for ( i = 0; i < num_windows; i++ )
		data[ i ] = 0.0;

	/* Now we can really start. First we reset the phase sequence, then we
	   run through the complete phase cycle. */

	TRY
	{
		vars_pop( func_call( func_get( "pulser_phase_reset", &access ) ) );
		vars_pop( func_call( func_get( "pulser_update", &access ) ) );

		for ( i = 0; i < aseq->len; i++ )
		{
			vars_pop( func_call( func_get( "digitizer_start_acquisition",
										   &access) ) );

			for ( j = 0; j < num_windows; j++ )
			{
				/* Get pointer to the correct function */

				if ( is_get_area_fast )
					func_ptr = func_get( "digitizer_get_area_fast", &access );
				else
					func_ptr = func_get( "digitizer_get_area", &access );

				/* Push the correct channel number onto the stack */

				if ( aseq->sequence[ i ] == ACQ_PLUS_A ||
					 aseq->sequence[ i ] == ACQ_MINUS_A )
					vars_push( INT_VAR, channel[ 0 ] );
				else
					vars_push( INT_VAR, channel[ 1 ] );

				/* If window is to be used push its number onto stack */

				if ( win_list[ 0 ] != -1 )
					vars_push( INT_VAR, win_list[ j ] );
				
				/* Finally call the function */

				vn = func_call( func_ptr );

				/* Now add (or subtract the result */

				if ( aseq->sequence[ i ] == ACQ_PLUS_A ||
					 aseq->sequence[ i ] == ACQ_PLUS_B )
					data[ j ] += vn->val.dval;
				else
					data[ j ] -= vn->val.dval;

				vars_pop( vn );
			}

			vars_pop( func_call( func_get( "pulser_next_phase", &access ) ) );
			vars_pop( func_call( func_get( "pulser_update", &access ) ) );
		}

		if ( win_list[ 0 ] == -1 || num_windows == 1 )
			vn = vars_push( FLOAT_VAR, data[ 0 ] );
		else
			vn = vars_push( FLOAT_TRANS_ARR, data, num_windows );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( data );
		T_free( win_list );
		PASSTHROU( );
	}

	T_free( data );
	T_free( win_list );

	return vn;
}


/*-----------------------------------------------------------------*/
/* Expected arguments:                                             */
/* 1. 2 digitizer channel numbers (if the second argument is a     */
/*    digitizer channel or a window number can be seen from the    */
/*    value, window numbers always start at WINDOW_START_NUMBER,   */
/*    which should be much larger than the highest channel number. */
/* 3. Any number of digitizer windows (none means just return area */
/*    of full width curve)                                         */
/* Return value:                                                   */
/*    For zero or 1 window: Floating point variable with phase     */
/*    cycled area                                                  */
/*    For more than 1 window: Array of floating point values with  */
/*    phase cycled area for each window                            */
/*-----------------------------------------------------------------*/

Var *get_phase_cycled_area_2d( Var *v )
{
	Var *func_ptr;
	Var *vn;                                /* all purpose variable */
	static bool first_time = SET;
	static bool is_get_area;
	static bool is_get_area_fast;
	bool is_channel;
	bool need_A, need_B;
	long channel[ 2 ];
	long num_windows;
	long *win_list;
	long i, j;
	int access;
	double *data;
	int channels_needed;
	Acquisition_Sequence *aseq[ ] = { ASeq, ASeq + 1 };


	/* The first time we get here do some basic checks, i.e. check that all
	   needed functions are there */

	if ( first_time )
	{
		pc_basic_check( "get_phase_cycled_area_2d", "digitizer_get_area",
						&is_get_area, "digitizer_get_area_fast",
						&is_get_area_fast, "an area" );
		first_time = UNSET;
	}

	if ( ! aseq[ 1 ]->defined )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_2d(): Second "
				"acquisition sequence (B) hasn't been defined.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Next step: check the parameter */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_2d(): Missing "
				"arguments.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Find out how many channels both the acquisition sequences need - if
	   two are needed there must be two channel arguments */

	need_A = need_B = UNSET;
	for ( i = 0; i < aseq[ 0 ]->len; i++ )
		for ( j = 0; j < 2; j++ )
		{
			if ( aseq[ j ]->sequence[ i ] == ACQ_PLUS_A  ||
				 aseq[ j ]->sequence[ i ] == ACQ_MINUS_A )
				need_A = SET;
			if ( aseq[ j ]->sequence[ i ] == ACQ_PLUS_B  ||
				 aseq[ j ]->sequence[ i ] == ACQ_MINUS_B )
				need_B = SET;
		}
	channels_needed = ( need_A && need_B ) ? 1 : 2;

	/* The first parameter must be a channel number in any case, ask digitizer
	   module if it really is one */

	if ( ! get_channel_number( v, "get_phase_cycled_area_2d", &channel[ 0 ] ) )
	{
		eprint( FATAL, "%s:%ld: get_phase_cycled_area_2d(): Invalid digitizer "
				"channel number: %ld.\n", Fname, Lc, channel[ 0 ] );
		THROW( EXCEPTION );
	}

	/* If we need two channels (acquisition sequences use A and B) also the
	   next argument must be a channel number. If only one channel is needed
	   the next argument can be either a channel number or a window number */

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		is_channel = get_channel_number( v , "get_phase_cycled_area_2d",
										 &channel[ 1 ] );

		if ( channels_needed == 2 && ! is_channel )
		{
			eprint( FATAL, "%s:%ld: get_phase_cycled_area_2d(): Two digitizer "
					"channel nunmbers are needed but second argument isn't "
					"one.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		if ( channels_needed == 1 && is_channel )
			eprint( WARN, "%s:%ld: get_phase_cycled_area_2d(): Superfluous "
					"digitizer channel number found as second argument.\n",
					Fname, Lc );

		if ( is_channel )
			v = vars_pop( v );
	}

	/* Now we've got to count the remaining arguments on the stack to find out
	   how many windows there are and build up the window list */

	num_windows = 1;

	if ( v != NULL )
	{
		vn = v;
		while ( ( vn = vn->next ) != NULL )
			num_windows++;
	}

	win_list = T_malloc( num_windows * sizeof( long ) );

	TRY
	{
		if ( v != NULL )
		{
			for ( i = 0; i < num_windows; i++ )
			{
				vars_check( v, INT_VAR | FLOAT_VAR );
				TRY_SUCCESS;
			}

			if ( v->type == INT_VAR )
				win_list[ i ] = v->val.lval;
			else
			{
				eprint( WARN, "%s:%ld: get_phase_cycled_area_2d(): Floating "
						"point value used as window number.\n", Fname, Lc );
				win_list[ i ] = lround( v->val.dval );
			}

			v = vars_pop( v );
		}
		else
			win_list[ 0 ] = -1;              /* No window is to be used ! */

		/* Also get memory for the data */

		data = T_malloc( 2 * num_windows * sizeof( double ) );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( win_list );
		PASSTHROU( );
	}

	for ( i = 0; i < 2 * num_windows; i++ )
		data[ i ] = 0.0;

	/* Now we can really start. First we reset the phase sequence, then we
	   run through the complete phase cycle */

	TRY
	{
		vars_pop( func_call( func_get( "pulser_phase_reset", &access ) ) );
		vars_pop( func_call( func_get( "pulser_update", &access ) ) );

		for ( i = 0; i < aseq[ 0 ]->len; i++ )
		{
			vars_pop( func_call( func_get( "digitizer_start_acquisition",
										   &access) ) );

			for ( j = 0; j < num_windows; j++ )
			{
				/* Get pointer to the correct function */

				if ( is_get_area_fast )
					func_ptr = func_get( "digitizer_get_area_fast", &access );
				else
					func_ptr = func_get( "digitizer_get_area", &access );

				/* Push the correct channel number onto the stack */

				if ( aseq[ 0 ]->sequence[ i ] == ACQ_PLUS_A ||
					 aseq[ 0 ]->sequence[ i ] == ACQ_MINUS_A )
					vars_push( INT_VAR, channel[ 0 ] );
				else
					vars_push( INT_VAR, channel[ 1 ] );

				/* If window is to be used push its number onto stack */

				if ( win_list[ 0 ] != -1 )
					vars_push( INT_VAR, win_list[ j ] );

				/* Finally call the function */

				vn = func_call( func_ptr );

				/* Now add (or subtract the result */

				if ( aseq[ 0 ]->sequence[ i ] == ACQ_PLUS_A ||
					 aseq[ 0 ]->sequence[ i ] == ACQ_PLUS_B )
					data[ 2 * j ] += vn->val.dval;
				else
					data[ 2 * j ] -= vn->val.dval;

				/* If the second acquisition sequence uses the same digitizer
				   channel we don't have to measure the area again... */

				if ( ( ( aseq[ 0 ]->sequence[ i ] == ACQ_PLUS_A || 
						 aseq[ 0 ]->sequence[ i ] == ACQ_MINUS_A ) &&
					   ( aseq[ 1 ]->sequence[ i ] == ACQ_PLUS_A ||
						 aseq[ 1 ]->sequence[ i ] == ACQ_MINUS_A ) ) ||
					 ( ( aseq[ 0 ]->sequence[ i ] == ACQ_PLUS_B ||
						 aseq[ 0 ]->sequence[ i ] == ACQ_MINUS_B ) &&
					   ( aseq[ 1 ]->sequence[ i ] == ACQ_PLUS_B ||
						 aseq[ 1 ]->sequence[ i ] == ACQ_MINUS_B ) ) )
				{
					if ( aseq[ 1 ]->sequence[ i ] == ACQ_PLUS_A ||
						 aseq[ 1 ]->sequence[ i ] == ACQ_PLUS_B )
						data[ 2 * j + 1 ] += vn->val.dval;
					else
						data[ 2 * j + 1 ] -= vn->val.dval;

					vars_pop( vn );
					continue;
				}

				/* Otherwise we need to do a second measurement on the other
				   channel (but using the same window) */

				vars_pop( vn );

				/* Get pointer to the correct function */

				if ( is_get_area_fast )
					func_ptr = func_get( "digitizer_get_area_fast", &access );
				else
					func_ptr = func_get( "digitizer_get_area", &access );

				/* Push the correct channel number onto the stack */

				if ( aseq[ 1 ]->sequence[ i ] == ACQ_PLUS_A ||
					 aseq[ 1 ]->sequence[ i ] == ACQ_MINUS_A )
					vars_push( INT_VAR, channel[ 0 ] );
				else
					vars_push( INT_VAR, channel[ 1 ] );

				/* If window is to be used push its number onto stack */

				if ( win_list[ 0 ] != -1 )
					vars_push( INT_VAR, win_list[ j ] );

				/* Finally call the function */

				vn = func_call( func_ptr );

				/* Now add (or subtract the result */

				if ( aseq[ 1 ]->sequence[ i ] == ACQ_PLUS_A ||
					 aseq[ 1 ]->sequence[ i ] == ACQ_PLUS_B )
					data[ 2 * j + 1 ] += vn->val.dval;
				else
					data[ 2 * j + 1 ] -= vn->val.dval;

				vars_pop( vn );
			}

			vars_pop( func_call( func_get( "pulser_next_phase", &access ) ) );
			vars_pop( func_call( func_get( "pulser_update", &access ) ) );
		}

		if ( win_list[ 0 ] == -1 || num_windows == 1 )
			vn = vars_push( FLOAT_VAR, data[ 0 ] );
		else
			vn = vars_push( FLOAT_TRANS_ARR, data, num_windows * 2 );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( data );
		T_free( win_list );
		PASSTHROU( );
	}

	T_free( data );
	T_free( win_list );

	return vn;
}


/*-----------------------------------------------------------------------*/
/* Checks if the variable `v', used in the function `func_name`, holds a */
/* value that is a channel number to be used with the current digitizer. */
/* If it is the value is returned in `channel'.                          */
/*-----------------------------------------------------------------------*/

static bool get_channel_number( Var *v, const char *func_name, long *channel )
{
	Var *func_ptr;
	Var *vn;
	bool result;
	int access;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		*channel = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: %s(): Floating point number used as "
				"argument.\n", Fname, Lc, func_name );
		*channel = lround( v->val.dval );
	}

	if ( ( func_ptr = func_get( "digitizer_meas_channel_ok", &access ) )
		 == NULL )
	{
		eprint( FATAL, "%s:%ld: %s(): Digitizer module does not supply needed "
				"functions.\n", Fname, Lc, func_name );
		THROW( EXCEPTION );
	}

	vars_push( INT_VAR, *channel );
	vn = func_call( func_ptr );

	result = ( vn->val.lval != 0 );
	vars_pop( vn );
	return result;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

static void pc_basic_check( const char *func_name, const char *func_1,
							bool *is_1, const char *func_2, bool *is_2,
							const char *str )
{
	Var *func_ptr;
	int access;


	/* At the very start lets figure out if there are pulser functions for
	   phase cycling */

	if ( ( func_ptr = func_get( "pulser_next_phase", &access ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: %s(): No pulser module loaded supplying a "
				"function to do phase cycling.\n", Fname, Lc, func_name );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	if ( ( func_ptr = func_get( "pulser_phase_reset", &access ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: %s(): No pulser module loaded supplying a "
				"function to do phase cycling.\n", Fname, Lc, func_name );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	if ( ( func_ptr = func_get( "pulser_update", &access ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: %s(): No pulser module loaded supplying a "
				"function to do phase cycling.\n", Fname, Lc, func_name );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	/* Check that there's a function for starting the acquisition */

	if ( ( func_ptr = func_get( "digitizer_start_acquisition", &access ) )
		 == NULL )
	{
		eprint( FATAL, "%s:%ld: %s(): No digitizer module loaded supplying a "
				"function to do an acquisition.\n", Fname, Lc, func_name );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	/* Check that both the supplied functions are supported and sets flags
       accordingly */

	*is_1 = ( ( func_ptr = func_get( func_1, &access ) ) == NULL );
	vars_pop( func_ptr );

	*is_2 = ( ( func_ptr = func_get( func_2, &access) ) == NULL );
	vars_pop( func_ptr );

	if ( ! is_1 && ! *is_2 )
	{
		eprint( FATAL, "%s:%ld: %s(): No digitizer module loaded supplying a "
				"function to obtain %s.\n", Fname, Lc, func_name, str );
		THROW( EXCEPTION );
	}

	/* We also need to check that a phase sequence and at least one
	   acquisition sequence has been defined */

	if ( PSeq == NULL )
	{
		eprint( FATAL, "%s:%ld: %s(): No phase sequence has been defined.\n",
				Fname, Lc, func_name );
		THROW( EXCEPTION );
	}

	if ( ! ASeq->defined )
	{
		eprint( FATAL, "%s:%ld: %s(): First acquisition sequence (A) hasn't "
				"been defined.\n", Fname, Lc, func_name );
		THROW( EXCEPTION );
	}
}
