/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#define HP8647A_MAIN

#include "hp8647a.h"
#include "gpib_if.h"


const char generic_type[ ] = DEVICE_TYPE;


static HP8647A hp8647a_backup;



/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int hp8647a_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	hp8647a.device = -1;

	hp8647a.state = UNSET;

	hp8647a.freq_is_set = UNSET;
	hp8647a.step_freq_is_set = UNSET;
	hp8647a.start_freq_is_set = UNSET;
	hp8647a.attenuation_is_set = UNSET;
	hp8647a.min_attenuation = MIN_ATTEN;

	hp8647a.table_file = NULL;
	hp8647a.use_table = UNSET;
	hp8647a.att_table = NULL;
	hp8647a.att_table_len = 0;
	hp8647a.real_attenuation = MAX_ATTEN - 100.0;  // invalid value !

	hp8647a.att_ref_freq = DEF_ATT_REF_FREQ;

	hp8647a.mod_type = UNDEFINED;
	hp8647a.mod_type_is_set = UNSET;
	for ( i = 0; i < NUM_MOD_TYPES; i++ )
	{
		hp8647a.mod_source_is_set[ i ] = UNSET;
		hp8647a.mod_ampl_is_set[ i ] = UNSET;
	}

	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int hp8647a_test_hook( void )
{
	/* If a table has been set check that at the frequency the attenuations
	   refer to the table is defined and if it is get the attenuation at the
	   reference frequency */

	if ( hp8647a.use_table )
	{
		if ( hp8647a.att_ref_freq < hp8647a.min_table_freq ||
			 hp8647a.att_ref_freq > hp8647a.max_table_freq )
		{
			eprint( FATAL, UNSET, "%s: Reference frequency for attenuation "
					"settings of %g MHz is not covered by the table.\n",
					DEVICE_NAME, hp8647a.att_ref_freq );
			THROW( EXCEPTION )
		}

		hp8647a.att_at_ref_freq =
			                hp8647a_get_att_from_table( hp8647a.att_ref_freq );
	}

	/* Save the current state of the device structure which always has to be
	   reset to this state at the start of the experiment */

	memcpy( &hp8647a_backup, &hp8647a, sizeof( HP8647A ) );

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int hp8647a_exp_hook( void )
{
	/* Restore device structure to the state at the start of the test run */

	memcpy( &hp8647a, &hp8647a_backup, sizeof( HP8647A ) );

	if ( ! hp8647a_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Initialization of device failed: %s\n",
				DEVICE_NAME, gpib_error_msg );
		THROW( EXCEPTION )
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int hp8647a_end_of_exp_hook( void )
{
	HP8647A_INIT = UNSET;
	hp8647a_finished( );

	memcpy( &hp8647a, &hp8647a_backup, sizeof( HP8647A ) );

	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void hp8647a_exit_hook( void )
{
	HP8647A_INIT = UNSET;
	if ( hp8647a.table_file != NULL )
	{
		T_free( hp8647a.table_file );
		hp8647a.table_file = NULL;
	}

	if ( hp8647a.use_table && hp8647a.att_table != NULL )
	{
		T_free( hp8647a.att_table );
		hp8647a.att_table = NULL;
	}

	hp8647a.device = -1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *synthesizer_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *synthesizer_state( Var *v )
{
	bool state;
	int res;
	const char *on_off_str[ ] = { "ON", "OFF" };


	if ( v == NULL )              /* i.e. return the current state */
		switch( FSC2_MODE )
		{
			case PREPARATION :
			eprint( FATAL, SET, "%s: Function `synthesizer_state' with no "
					"argument can only be used in the EXPERIMENT section.\n",
					DEVICE_NAME );
			THROW( EXCEPTION )

			case TEST :
				return vars_push( INT_VAR, ( long ) hp8647a.state );

			case EXPERIMENT :
				return vars_push( INT_VAR,
								  ( long ) ( hp8647a.state =
											 hp8647a_get_output_state( ) ) );
		}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, SET, "%s: Float variable used for synthesizer "
				"state.\n", DEVICE_NAME );
		state = ( v->val.dval != 0.0 );
	}
	else if ( v->type == INT_VAR )
		state = ( v->val.lval != 0 );
	else
	{
		if ( ( res = is_in( v->val.sptr, on_off_str, 2 ) ) == -1 )
		{
			eprint( FATAL, SET, "Invalid parameter \"s\" in call of "
					"function `synthesizer_state'.\n", DEVICE_NAME,
					v->val.sptr );
			THROW( EXCEPTION )
		}

		state = res ? UNSET : SET;
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"function `synthesizer_state'.\n", DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	hp8647a.state = state;

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( INT_VAR, ( long ) state );

	return vars_push( INT_VAR, ( long ) hp8647a_set_output_state( state ) );
}

/*---------------------------------------------------------------------*/
/* Function sets or returns (if called with no argument) the frequency */
/* of the synthesizer. If called for setting the frequency before the  */
/* experiment is started the frequency value is stored and set in the  */
/* setup phase of the experiment. The frequency set the first time the */
/* function is called is also set as the start frequency to be used in */
/* calls of `synthesizer_reset_frequency'. The function can only be    */
/* called once in the PREPARATIONS section, further calls just result  */
/* in a warning and the new value isn't accepted.                      */
/*---------------------------------------------------------------------*/

Var *synthesizer_frequency( Var *v )
{
	double freq;
	double att;


	if ( v == NULL )              /* i.e. return the current frequency */
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			if ( ! hp8647a.freq_is_set )
			{
				eprint( FATAL, SET, "%s: RF frequency hasn't been set "
						"yet.\n", DEVICE_NAME );
				THROW( EXCEPTION )
			}
			else
				return vars_push( FLOAT_VAR, hp8647a.freq );
		}

		hp8647a.freq = hp8647a_get_frequency( );
		return vars_push( FLOAT_VAR, hp8647a.freq );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer value used as RF frequency.\n",
				DEVICE_NAME );

	freq = VALUE( v );

	if ( freq < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid negative RF frequency.\n",
				DEVICE_NAME );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8647a.freq );
		else
			THROW( EXCEPTION )
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"function `synthesizer_frequency'.\n", DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* In test run stop program if value is out of range while in real run
	   just keep the current value on errors */

	if ( freq < MIN_FREQ || freq > MAX_FREQ )
	{
		eprint( FATAL, SET, "%s: RF frequency (%f MHz) not within "
				"synthesizers range (%f kHz - %g Mhz).\n",
				DEVICE_NAME, 1.0e-6 * freq, 1.0e-3 * MIN_FREQ,
				1.0e-6 * MAX_FREQ );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8647a.freq );
		else
			THROW( EXCEPTION )
	}

	switch ( FSC2_MODE )
	{
		case PREPARATION :
			hp8647a.freq = hp8647a.start_freq = freq;
			hp8647a.freq_is_set = SET;
			hp8647a.start_freq_is_set = SET;
			break;

		case TEST :
			hp8647a.freq = freq;
			hp8647a.freq_is_set = SET;
			if ( ! hp8647a.start_freq_is_set )
			{
				hp8647a.start_freq = freq;
				hp8647a.start_freq_is_set = SET;
			}

			/* Calculate the attenuation needed to level out the non-flatness
			   of the RF field in the resonator if a table has been set - in
			   the test run we only do this to check that we stay within all
			   the limits */

			hp8647a.real_attenuation = hp8647a_get_att( freq );
			break;

		case EXPERIMENT :
			if ( ! hp8647a.start_freq_is_set )
			{
				hp8647a.start_freq = freq;
				hp8647a.start_freq_is_set = SET;
			}

			/* Take care of setting the correct attenuation to level out the
			   non- flatness of the RF field in the resonator if a table has
			   been set */

			att = hp8647a_get_att( freq );
			if ( att != hp8647a.real_attenuation )
			{
				hp8647a_set_attenuation( att );
				hp8647a.real_attenuation = att;
			}

			/* Finally set the frequency */

			hp8647a.freq = hp8647a_set_frequency( freq );
	}

	return vars_push( FLOAT_VAR, freq );
}


/*-----------------------------------------------------------------------*/
/* Function sets or returns (if called with no argument) the attenuation */
/* of the synthesizer. If called for setting the attenuation before the  */
/* experiment is started the attenuation value is stored and set in the  */
/* setup phase of the attenuation. The function can only be called once  */
/* in the PREPARATIONS section, further calls just result in a warning   */
/* and the new value isn't accepted.                                     */
/*-----------------------------------------------------------------------*/

Var *synthesizer_attenuation( Var *v )
{
	double att;


	if ( v == NULL )              /* i.e. return the current attenuation */
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			if ( ! hp8647a.attenuation_is_set )
			{
				eprint( FATAL, SET, "%s: RF attenuation has not been set "
						"yet.\n", DEVICE_NAME );
				THROW( EXCEPTION )
			}
			else
				return vars_push( FLOAT_VAR, hp8647a.attenuation );
		}

		hp8647a.attenuation = MAX_ATTEN -100.0;
		return vars_push( FLOAT_VAR, hp8647a.attenuation );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer variable used as RF attenuation.\n",
				DEVICE_NAME );

	att = VALUE( v );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"function `synthesizer_attenuation'.\n", DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Check that attenuation is within valid range, if not throw exception
	   in test run, but in real run just don't change the attenuation */

	if ( att > hp8647a.min_attenuation || att < MAX_ATTEN )
	{
		eprint( FATAL, SET, "%s: RF attenuation (%g db) not within valid "
				"range (%g db to %g db).\n", DEVICE_NAME, att,
				MAX_ATTEN, hp8647a.min_attenuation );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8647a.attenuation );
		else
			THROW( EXCEPTION )
	}

	switch ( FSC2_MODE )
	{
		case PREPARATION :
			if ( hp8647a.attenuation_is_set )
			{
				eprint( SEVERE, SET, "%s: RF attenuation has already been set "
						"in the PREPARATIONS section to %g db, keeping old "
						"value.\n", DEVICE_NAME, hp8647a.attenuation );
				return vars_push( FLOAT_VAR, hp8647a.attenuation );
			}

			hp8647a.attenuation = att;
			hp8647a.attenuation_is_set = SET;
			break;

		case TEST :
			hp8647a.attenuation = hp8647a.real_attenuation = att;
			hp8647a.attenuation_is_set = SET;
			break;

		case EXPERIMENT :
			hp8647a.attenuation = hp8647a.real_attenuation =
				                                hp8647a_set_attenuation( att );
			break;
	}

	return vars_push( FLOAT_VAR, att );
}


/*-------------------------------------------------------------*/
/* Sets (or returns) the minimum attentuation that can be set. */
/*-------------------------------------------------------------*/

Var *synthesizer_minimum_attenuation( Var *v )
{
	double min_atten;


	if ( v == NULL )          /* i.e. return the current minimum attenuation */
		return vars_push( FLOAT_VAR, hp8647a.min_attenuation );


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer variable used as minumum RF "
				"attenuation.\n", DEVICE_NAME );

	min_atten = VALUE( v );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"function `synthesizer_minimum_attenuation'.\n", DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}


	if ( min_atten > MIN_MIN_ATTEN )
	{
		eprint( FATAL, SET, "%s: Minimum attenuation must be below "
				"%g dB.\n", DEVICE_NAME, MIN_MIN_ATTEN );
		THROW( EXCEPTION )
	}

	if ( min_atten < MAX_ATTEN )
	{
		eprint( FATAL, SET, "%s: Minimum attenuation must be more than "
				"%g dB.\n", DEVICE_NAME, MAX_ATTEN );
		THROW( EXCEPTION )
	}

	hp8647a.min_attenuation = min_atten;

	return vars_push( FLOAT_VAR, hp8647a.min_attenuation );
}

/*-----------------------------------------------------------*/
/* Function sets or returns (if called with no argument) the */
/* step frequency for RF sweeps.                             */
/*-----------------------------------------------------------*/

Var *synthesizer_step_frequency( Var *v )
{
	if ( v != NULL )
	{
		vars_check( v, INT_VAR | FLOAT_VAR );

		/* Allow setting of the step frequency in the PREPARATIONS section
		   only once */

		if ( FSC2_MODE == PREPARATION && hp8647a.step_freq_is_set )
		{
			eprint( SEVERE, SET, "%s: RF step frequency has already been "
					"set in the PREPARATIONS section to %f MHz, keeping old "
					"value.\n", DEVICE_NAME, 1.0e-6 * hp8647a.step_freq );
			while ( ( v = vars_pop( v ) ) != NULL )
				;
			return vars_push( FLOAT_VAR, hp8647a.step_freq );
		}

		if ( v->type == INT_VAR )
			eprint( WARN, SET, "%s: Integer variable used as RF step "
					"frequency.\n", DEVICE_NAME );

		hp8647a.step_freq = VALUE( v );

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			eprint( WARN, SET, "%s: Superfluous arguments in call of "
					"function `synthesizer_step_frequency'.\n", DEVICE_NAME );
			while ( ( v = vars_pop( v ) ) != NULL )
				;
		}

		hp8647a.step_freq_is_set = SET;
	}
	else if ( ! hp8647a.step_freq_is_set )
	{
		eprint( FATAL, SET, "%s: RF step frequency has not been set "
				"yet.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	return vars_push( FLOAT_VAR, hp8647a.step_freq );
}


/*-------------------------------------------------------------*/
/* This function may only be called in the EXPERIMENT section! */
/*-------------------------------------------------------------*/

Var *synthesizer_sweep_up( Var *v )
{
	double att;


	v = v;

	if ( ! hp8647a.step_freq_is_set )
	{
		eprint( FATAL, SET, "%s: RF step frequency hasn't been set.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}

	hp8647a.freq += hp8647a.step_freq;

	/* Check that frequency stays within the synthesizers range */

	if ( hp8647a.freq < MIN_FREQ )
	{
		eprint( FATAL, SET, "%s: RF frequency is dropping below lower "
				"limit of %f kHz.\n", DEVICE_NAME, 1.0e-3 * MIN_FREQ );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8647a.freq );
		else
			THROW( EXCEPTION )
	}

	if ( hp8647a.freq > MAX_FREQ )
	{
		eprint( FATAL, SET, "%s: RF frequency is increased above upper "
				"limit of %f MHz.\n", DEVICE_NAME, 1.0e-6 * MAX_FREQ );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8647a.freq );
		else
			THROW( EXCEPTION )
	}

	if ( FSC2_MODE == TEST )
		hp8647a.real_attenuation = hp8647a_get_att( hp8647a.freq );
	else
	{
		att = hp8647a_get_att( hp8647a.freq );
		if ( att != hp8647a.real_attenuation )
		{
			hp8647a.real_attenuation = att;
			hp8647a_set_attenuation( att );
		}			

		hp8647a_set_frequency( hp8647a.freq );
	}

	return vars_push( FLOAT_VAR, hp8647a.freq );
}


/*-------------------------------------------------------------*/
/* This function may only be called in the EXPERIMENT section! */
/*-------------------------------------------------------------*/

Var *synthesizer_sweep_down( Var *v )
{
	Var *nv;


	v = v;
	hp8647a.step_freq *= -1.0;
	nv = synthesizer_sweep_up( NULL );
	hp8647a.step_freq *= -1.0;
	return nv;
}


/*-------------------------------------------------------------*/
/* This function may only be called in the EXPERIMENT section! */
/*-------------------------------------------------------------*/

Var *synthesizer_reset_frequency( Var *v )
{
	v = v;

	if ( ! hp8647a.start_freq_is_set )
	{
		eprint( FATAL, SET, "%s: No RF frequency has been set yet, so "
				"can't do a frequency reset.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	if ( FSC2_MODE == TEST )
		hp8647a.freq = hp8647a.start_freq;
	else
		hp8647a.freq = hp8647a_set_frequency( hp8647a.start_freq );

	return vars_push( FLOAT_VAR, hp8647a.freq );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *synthesizer_use_table( Var *v )
{
	static FILE *tfp = NULL;
	char *tfname;


	/* Try to figure out the name of the table file - if no argument is given
	   use the default table file, otherwise use the user supplied file name */

	if ( v == NULL )
	{
		if ( DEFAULT_TABLE_FILE[ 0 ] ==  '/' )
			hp8647a.table_file = T_strdup( DEFAULT_TABLE_FILE );
		else
			hp8647a.table_file = get_string( "%s%s%s", libdir, slash( libdir ),
											 DEFAULT_TABLE_FILE );

		if ( ( tfp = hp8647a_open_table( hp8647a.table_file ) ) == NULL )
		{
			eprint( FATAL, SET, "%s: Default table file `%s' not found.\n",
					DEVICE_NAME, hp8647a.table_file );
			T_free( hp8647a.table_file );
			hp8647a.table_file = NULL;
			THROW( EXCEPTION )
		}
	}
	else
	{
		vars_check( v, STR_VAR );

		tfname = T_strdup( v->val.sptr );

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			eprint( WARN, SET, "%s: Superfluous arguments in call of "
					"function `synthesizer_use_table'.\n", DEVICE_NAME );
			while ( ( v = vars_pop( v ) ) != NULL )
				;
		}

		TRY
		{
			tfp = hp8647a_find_table( &tfname );
			hp8647a.table_file = tfname;
			TRY_SUCCESS;
		}
		CATCH( EXCEPTION )
		{
			T_free( tfname );
			PASSTHROU( )
		}
	}

	TRY
	{
		hp8647a_read_table( tfp );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		fclose( tfp );
		T_free( hp8647a.table_file );
		hp8647a.table_file = NULL;
		PASSTHROU( )
	}

	fclose( tfp );
	T_free( hp8647a.table_file );
	hp8647a.table_file = NULL;
	hp8647a.use_table = SET;

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *synthesizer_att_ref_freq( Var *v )
{
	double freq;


	/* Without an argument just return the reference frequency setting */

	if ( v == NULL )
		return vars_push( FLOAT_VAR, hp8647a.att_ref_freq );

	/* Otherwise check the supplied variable */

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer variable used as RF frequency.\n",
				DEVICE_NAME );

	freq = VALUE( v );

	/* Get rid of the variables */

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"function `synthesizer_use_table'.\n", DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	/* Check that the frequency is within the synthesizers range */

	if ( freq > MAX_FREQ || freq < MIN_FREQ )
	{
		eprint( FATAL, UNSET, "%s: Reference frequency for attenuation "
				"settings of %g MHz is out of synthesizer range (%f kHz - "
				"%f MHz).\n", DEVICE_NAME, hp8647a.att_ref_freq * 1.0e-6,
				MIN_FREQ * 1.0e-3, MAX_FREQ * 1.0e-6 );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8647a.freq );
		else
			THROW( EXCEPTION )
	}

	hp8647a.att_ref_freq = freq;	

	/* If a table has already been loaded calculate the attenuation at the
	   reference frequency */

	if ( hp8647a.use_table )
	{
		if ( hp8647a.att_ref_freq < hp8647a.min_table_freq ||
			 hp8647a.att_ref_freq > hp8647a.max_table_freq )
		{
			eprint( FATAL, UNSET, "%s: Reference frequency for attenuation "
					"settings of %g MHz is not covered by the table.\n",
					DEVICE_NAME, hp8647a.att_ref_freq );
			THROW( EXCEPTION )
		}
		
		hp8647a.att_at_ref_freq = hp8647a_get_att_from_table( freq );
	}

	return vars_push( FLOAT_VAR, freq );
}


/*-----------------------------------------------------------------*/
/* Function for setting some or all modulation parameters at once. */
/* The sequence the parameters are set in don't matter. If the     */
/* function succeeds 1 (as variable) is returned, otherwise an     */
/* exception is thrown.                                            */
/*-----------------------------------------------------------------*/

Var *synthesizer_modulation( Var *v )
{
	int res;
	int set = 0;
	const char *str[ ] = { "amplitude", "type", "source" };
	double ampl = -1.0;
	int what;
	int type = UNDEFINED,
		source = UNDEFINED;


	if ( v == NULL )
	{
		eprint( FATAL, UNSET, "%s: Use functions "
				"`synthesizer_mod_(type|source|ampl)' to determine modulation "
				"settings.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	while ( v )
	{
		if ( ( 1 << ( ( res = hp8647a_set_mod_param( v, &ampl, &what ) )
					  - 1 ) ) & set )
			eprint( SEVERE, SET, "%s: Parameter for modulation %s set more "
					"than once in call of `synthesizer_modulation'.\n",
					DEVICE_NAME, str[ res ] );
		else
		{
			switch ( res )
			{
				case 1 :                /* setting modulatopn amplitude */
					break;

				case 2 :                /* setting modulation type */
					type = what;
					break;

				case 3 :                /* setting modulation source */
					source = what;
					break;

				default :                 /* this definitely can't happen... */
					fsc2_assert( 1 == 0 );
			}
		}
		set |= ( 1 << ( res - 1 ) );
		v = vars_pop( v );
	}

	if ( type != UNDEFINED || hp8647a.mod_type_is_set == SET )
	{
		if ( type != UNDEFINED )
		{
			hp8647a.mod_type = hp8647a_set_mod_type( type );
			hp8647a.mod_type_is_set = SET;
		}

		if ( source != UNDEFINED )
		{
			hp8647a.mod_source[ hp8647a.mod_type ] =
							hp8647a_set_mod_source( hp8647a.mod_type, source );
			hp8647a.mod_source_is_set[ hp8647a.mod_type ] = SET;
		}

		if ( ampl >= 0.0 )
		{
			hp8647a.mod_ampl[ hp8647a.mod_type ] =
								hp8647a_set_mod_ampl( hp8647a.mod_type, ampl );
			hp8647a.mod_ampl_is_set[ hp8647a.mod_type ] = SET;
		}
	}
	else
	{
		if ( source != UNDEFINED )
		{
			eprint( FATAL, SET, "%s: Can't set modulation source as long "
					"as modulation type hasn't been set.\n", DEVICE_NAME );
			THROW( EXCEPTION )
		}

		if ( ampl >= 0.0 )
		{
			eprint( FATAL, SET, "%s: Can't set modulation amplitude as long "
					"as modulation type hasn't been set.\n", DEVICE_NAME );
			THROW( EXCEPTION )
		}
	}

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *synthesizer_mod_type( Var *v )
{
	int res;


	if ( v == NULL )
	{
		if ( ! hp8647a.mod_type_is_set )
			return vars_push( INT_VAR, -1 );

		if ( FSC2_MODE == EXPERIMENT )
			hp8647a.mod_type = hp8647a_get_mod_type( );

		if ( hp8647a.mod_type != UNDEFINED )
		{
			hp8647a.mod_type_is_set = SET;
			return vars_push( INT_VAR, hp8647a.mod_type );
		}
		else
		{
			hp8647a.mod_type_is_set = UNSET;
			return vars_push( INT_VAR, -1 );
		}
	}

	vars_check( v, STR_VAR | INT_VAR );

	if ( v->type == INT_VAR )
	{
		res = ( int ) v->val.lval;

		if ( res < 0 || res >= NUM_MOD_TYPES )
		{
			eprint( FATAL, SET, "%s: Invalid modulation type %d.\n",
					DEVICE_NAME, res );
			THROW( EXCEPTION )
		}
	}
	else
	{
		if ( ( res = is_in( v->val.sptr, mod_types, 4 ) ) == UNDEFINED )
		{
			eprint( FATAL, SET, "%s: Invalid modulation type `%s'.\n",
					DEVICE_NAME, v->val.sptr );
			THROW( EXCEPTION )
		}
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"`synthesizer_mod_type'.\n", DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	hp8647a.mod_type = hp8647a_set_mod_type( res );
	hp8647a.mod_type_is_set = SET;

	return vars_push( INT_VAR, res );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *synthesizer_mod_source( Var *v )
{
	int source = 0;
	const char *sources[ ] = { "EXT AC", "AC", "EXT DC", "DC",
							   "INT 1kHz", "INT 1 kHz", "INT 1", "1kHz",
							   "1 kHz", "1", "INT 400Hz", "INT 400 Hz",
							   "INT 400", "400Hz", "400 Hz", "400" };
	

	if ( v == NULL )
	{
		if ( ! hp8647a.mod_type_is_set )
		{
			eprint( FATAL, SET, "%s: Can't determine modulation source as "
					"long as modulation type isn't set.\n", DEVICE_NAME );
			THROW( EXCEPTION )
		}

		if ( hp8647a.mod_type == MOD_TYPE_OFF )
		{
			eprint( FATAL, SET, "%s: Can't determine modulation source "
					"when modulation is off.\n", DEVICE_NAME );
			THROW( EXCEPTION )
		}

		if ( FSC2_MODE == EXPERIMENT )
			hp8647a.mod_source[ hp8647a.mod_type ] =
			                        hp8647a_get_mod_source( hp8647a.mod_type );

		hp8647a.mod_source_is_set[ hp8647a.mod_type ] = SET;
		return vars_push( INT_VAR, hp8647a.mod_source[ hp8647a.mod_type ] );
	}

	if ( ! hp8647a.mod_type_is_set )
	{
		eprint( FATAL, SET, "%s: Can't set modulation source as long as "
				"modulation type isn't set.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	vars_check( v, STR_VAR | INT_VAR );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"`synthesizer_mod_source'.\n", DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( v->type == INT_VAR )
	{
		source = ( int ) v->val.lval;
		if ( source < 0 || source >= NUM_MOD_SOURCES )
		{
			eprint( FATAL, SET, "%s: Invalid modulation source "
					"parameter %d.\n", DEVICE_NAME, source );
			THROW( EXCEPTION )
		}
	}
	else
	{
		switch ( is_in( v->val.sptr, sources, 16 ) )
		{
			case 0 : case 1 :
				source = MOD_SOURCE_AC;
				break;

			case 2 : case 3 :
				source = MOD_SOURCE_DC;
				break;

			case 4 : case 5 : case 6 : case 7 : case 8 : case 9 :
				source = MOD_SOURCE_1k;
				break;

			case 10 : case 11 : case 12 : case 13 : case 14 : case 15 :
				source = MOD_SOURCE_400;
				break;

			default :
				eprint( FATAL, SET, "%s: Invalid modulation source `%s'.\n",
						DEVICE_NAME, v->val.sptr );
				THROW( EXCEPTION )
		}
	}

	if ( hp8647a.mod_type == MOD_TYPE_OFF )
	{
		eprint( FATAL, SET, "%s: Can't set modulation source while "
				"modulation is off.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	hp8647a.mod_source[ hp8647a.mod_type ] =
		                    hp8647a_set_mod_source( hp8647a.mod_type, source );
	hp8647a.mod_source_is_set[ hp8647a.mod_type ] = SET;

	return vars_push( INT_VAR, hp8647a.mod_source );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *synthesizer_mod_ampl( Var *v )
{
	double ampl;


	if ( v == NULL )
	{
		if ( ! hp8647a.mod_type_is_set )
		{
			eprint( FATAL, SET, "%s: Can't determine modulation amplitude "
					"as long as modulation type isn't set.\n", DEVICE_NAME );
			THROW( EXCEPTION )
		}

		if ( hp8647a.mod_type == MOD_TYPE_OFF )
		{
			eprint( FATAL, SET, "%s: Can't determine modulation amplitude "
					"when modulation is off.\n", DEVICE_NAME );
			THROW( EXCEPTION )
		}

		hp8647a.mod_ampl[ hp8647a.mod_type ] =
			                          hp8647a_get_mod_ampl( hp8647a.mod_type );

		hp8647a.mod_ampl_is_set[ hp8647a.mod_type ] = SET;

		return vars_push( INT_VAR, hp8647a.mod_ampl[ hp8647a.mod_type ] );
	}

	if ( ! hp8647a.mod_type_is_set )
	{
		eprint( FATAL, SET, "%s: Can't set modulation amplitude as long as "
				"modulation type isn't set.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer value used as modulation "
				"amplitude.\n", DEVICE_NAME );

	ampl = VALUE( v );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of "
				"function `synthesizer_mod_ampl'.\n", DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( hp8647a.mod_type == MOD_TYPE_OFF )
	{
		eprint( FATAL, SET, "%s: Can't set modulation amplitude while "
				"modulation is off.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	hp8647a.mod_ampl[ hp8647a.mod_type ] =
		                              hp8647a_get_mod_ampl( hp8647a.mod_type );
	hp8647a.mod_ampl_is_set[ hp8647a.mod_type ] = SET;

	return vars_push( FLOAT_VAR, hp8647a.mod_ampl[ hp8647a.mod_type ] );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
