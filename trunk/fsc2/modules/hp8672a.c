/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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


#define HP8672A_MAIN

#include "hp8672a.h"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


static HP8672A hp8672a_backup;



/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int hp8672a_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	hp8672a.device = -1;

	hp8672a.state = UNSET;
	hp8672a.is_10db = UNSET;

	hp8672a.freq_is_set = UNSET;
	hp8672a.step_freq_is_set = UNSET;
	hp8672a.start_freq_is_set = UNSET;
	hp8672a.attenuation_is_set = UNSET;
	hp8672a.min_attenuation = MIN_ATTEN;

	hp8672a.table_file = NULL;
	hp8672a.use_table = UNSET;
	hp8672a.att_table = NULL;
	hp8672a.att_table_len = 0;
	hp8672a.real_attenuation = MAX_ATTEN - 100.0;  /* invalid value ! */

	hp8672a.att_ref_freq = DEF_ATT_REF_FREQ;

	hp8672a.mod_type = UNDEFINED;
	hp8672a.mod_type_is_set = UNSET;
	for ( i = 0; i < NUM_MOD_TYPES; i++ )
		hp8672a.mod_ampl_is_set[ i ] = UNSET;

	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int hp8672a_test_hook( void )
{
	/* If a table has been set check that at the frequency the attenuations
	   refer to the table is defined and if it is get the attenuation at the
	   reference frequency */

	if ( hp8672a.use_table )
	{
		if ( hp8672a.att_ref_freq < hp8672a.min_table_freq ||
			 hp8672a.att_ref_freq > hp8672a.max_table_freq )
		{
			print( FATAL, "Reference frequency for attenuation settings of "
				   "%g MHz is not covered by the table.\n",
				   hp8672a.att_ref_freq );
			THROW( EXCEPTION );
		}

		hp8672a.att_at_ref_freq =
			                hp8672a_get_att_from_table( hp8672a.att_ref_freq );
	}

	/* Save the current state of the device structure which always has to be
	   reset to this state at the start of the experiment */

	hp8672a_backup = hp8672a;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int hp8672a_exp_hook( void )
{
	/* Restore device structure to the state at the start of the test run */

	hp8672a = hp8672a_backup;

	if ( ! hp8672a_init( DEVICE_NAME ) )
	{
		print( FATAL, "Initialization of device failed: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int hp8672a_end_of_exp_hook( void )
{
	HP8672A_INIT = UNSET;
	hp8672a_finished( );

	hp8672a = hp8672a_backup;

	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void hp8672a_exit_hook( void )
{
	HP8672A_INIT = UNSET;

	if ( hp8672a.table_file != NULL )
		hp8672a.table_file = CHAR_P T_free( hp8672a.table_file );

	if ( hp8672a.use_table && hp8672a.att_table != NULL )
		hp8672a.att_table = ATT_TABLE_ENTRY_P T_free( hp8672a.att_table );

	hp8672a.device = -1;
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


	if ( v == NULL )              /* i.e. return the current state */
		switch( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				return vars_push( INT_VAR, ( long ) hp8672a.state );

			case EXPERIMENT :
				return vars_push( INT_VAR,
								  ( long ) ( hp8672a.state =
											 hp8672a_get_output_state( ) ) );
		}

	state = get_boolean( v );
	too_many_arguments( v );

	hp8672a.state = state;

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( INT_VAR, ( long ) state );

	return vars_push( INT_VAR, ( long ) hp8672a_set_output_state( state ) );
}


/*---------------------------------------------------------------------*/
/* Function sets or returns (if called with no argument) the frequency */
/* of the synthesizer. If called for setting the frequency before the  */
/* experiment is started the frequency value is stored and set in the  */
/* setup phase of the experiment. The frequency set the first time the */
/* function is called is also set as the start frequency to be used in */
/* calls of 'synthesizer_reset_frequency'. The function can only be    */
/* called once in the PREPARATIONS section, further calls just result  */
/* in a warning and the new value isn't accepted.                      */
/*---------------------------------------------------------------------*/

Var *synthesizer_frequency( Var *v )
{
	double freq;
	double att;


	if ( v == NULL )
	{
		if ( ! hp8672a.freq_is_set )
		{
			print( FATAL, "RF frequency can't be determined.\n" );
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, hp8672a.freq );
	}

	freq = get_double( v, "RF frequency" );

	if ( freq < 0 )
	{
		print( FATAL, "Invalid negative RF frequency.\n" );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8672a.freq );
		else
			THROW( EXCEPTION );
	}

	too_many_arguments( v );

	/* In test run stop program if value is out of range while in real run
	   just keep the current value on errors */

	if ( freq < MIN_FREQ || freq > MAX_FREQ )
	{
		print( FATAL, "RF frequency (%f MHz) not within synthesizers range "
			   "(%f kHz - %g Mhz).\n", 1.0e-6 * freq, 1.0e-3 * MIN_FREQ,
			   1.0e-6 * MAX_FREQ );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8672a.freq );
		else
			THROW( EXCEPTION );
	}

	switch ( FSC2_MODE )
	{
		case PREPARATION :
			hp8672a.freq = hp8672a.start_freq = freq;
			hp8672a.freq_is_set = SET;
			hp8672a.start_freq_is_set = SET;
			break;

		case TEST :
			hp8672a.freq = freq;
			hp8672a.freq_is_set = SET;
			if ( ! hp8672a.start_freq_is_set )
			{
				hp8672a.start_freq = freq;
				hp8672a.start_freq_is_set = SET;
			}

			/* Calculate the attenuation needed to level out the non-flatness
			   of the RF field in the resonator if a table has been set - in
			   the test run we only do this to check that we stay within all
			   the limits */

			if ( hp8672a.use_table )
				hp8672a.real_attenuation = hp8672a_get_att( freq );
			break;

		case EXPERIMENT :
			if ( ! hp8672a.start_freq_is_set )
			{
				hp8672a.start_freq = freq;
				hp8672a.start_freq_is_set = SET;
			}

			/* Take care of setting the correct attenuation to level out the
			   non-flatness of the RF field in the resonator if a table has
			   been set */

			if ( hp8672a.use_table )
			{
				att = hp8672a_get_att( freq );
				if ( att != hp8672a.real_attenuation )
				{
					hp8672a_set_attenuation( att );
					hp8672a.real_attenuation = att;
				}
			}

			/* Finally set the frequency */

			hp8672a.freq = hp8672a_set_frequency( freq );
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


	if ( v == NULL )
	{
		if ( ! hp8672a.attenuation_is_set )
		{
			print( FATAL, "RF attenuation can't be determined.\n" );
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, hp8672a.attenuation );
	}

	att = get_double( v, "RF attenuation" );

	too_many_arguments( v );

	/* Check that attenuation is within valid range, if not throw exception
	   in test run, but in real run just don't change the attenuation */

	if ( att > hp8672a.min_attenuation || att < MAX_ATTEN )
	{
		print( FATAL, "RF attenuation (%g db) not within valid range (%g db "
			   "to %g db).\n", att, MAX_ATTEN, hp8672a.min_attenuation );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8672a.attenuation );
		else
			THROW( EXCEPTION );
	}

	switch ( FSC2_MODE )
	{
		case PREPARATION :
			if ( hp8672a.attenuation_is_set )
			{
				print( SEVERE, "RF attenuation has already been set to %g db, "
					   "keeping old value.\n", hp8672a.attenuation );
				return vars_push( FLOAT_VAR, hp8672a.attenuation );
			}

			hp8672a.attenuation = att;
			hp8672a.attenuation_is_set = SET;
			break;

		case TEST :
			hp8672a.attenuation = hp8672a.real_attenuation = att;
			hp8672a.attenuation_is_set = SET;
			break;

		case EXPERIMENT :
			hp8672a.attenuation = hp8672a.real_attenuation =
				                                hp8672a_set_attenuation( att );
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
		return vars_push( FLOAT_VAR, hp8672a.min_attenuation );

	min_atten = get_double( v, "minimum RF attenuation" );

	too_many_arguments( v );

	if ( min_atten > MIN_MIN_ATTEN )
	{
		print( FATAL, "Minimum attenuation must be below %g dB.\n",
			   MIN_MIN_ATTEN );
		THROW( EXCEPTION );
	}

	if ( min_atten < MAX_ATTEN )
	{
		print( FATAL, "Minimum attenuation must be more than %g dB.\n",
			   MAX_ATTEN );
		THROW( EXCEPTION );
	}

	hp8672a.min_attenuation = min_atten;

	return vars_push( FLOAT_VAR, hp8672a.min_attenuation );
}

/*-----------------------------------------------------------*/
/* Function sets or returns (if called with no argument) the */
/* step frequency for RF sweeps.                             */
/*-----------------------------------------------------------*/

Var *synthesizer_step_frequency( Var *v )
{
	if ( v != NULL )
	{
		/* Allow setting of the step frequency in the PREPARATIONS section
		   only once */

		if ( FSC2_MODE == PREPARATION && hp8672a.step_freq_is_set )
		{
			print( SEVERE, "RF step frequency has already been set to %f MHz, "
				   "keeping old value.\n", 1.0e-6 * hp8672a.step_freq );
			return vars_push( FLOAT_VAR, hp8672a.step_freq );
		}

		hp8672a.step_freq = get_double( v, "RF step frequency" );
		hp8672a.step_freq_is_set = SET;

		too_many_arguments( v );
	}
	else if ( ! hp8672a.step_freq_is_set )
	{
		print( FATAL, "RF step frequency has not been set yet.\n" );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, hp8672a.step_freq );
}


/*-------------------------------------------------------------*/
/* This function may only be called in the EXPERIMENT section! */
/*-------------------------------------------------------------*/

Var *synthesizer_sweep_up( Var *v )
{
	double att;


	v = v;

	if ( ! hp8672a.step_freq_is_set )
	{
		print( FATAL, "RF step frequency hasn't been set.\n" );
		THROW( EXCEPTION );
	}

	hp8672a.freq += hp8672a.step_freq;

	/* Check that frequency stays within the synthesizers range */

	if ( hp8672a.freq < MIN_FREQ )
	{
		print( FATAL, "RF frequency dropping below lower limit of %f kHz.\n",
			   1.0e-3 * MIN_FREQ );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8672a.freq );
		else
			THROW( EXCEPTION );
	}

	if ( hp8672a.freq > MAX_FREQ )
	{
		print( FATAL, "RF frequency increased above upper limit of %f MHz.\n",
			   1.0e-6 * MAX_FREQ );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8672a.freq );
		else
			THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
		hp8672a.real_attenuation = hp8672a_get_att( hp8672a.freq );
	else
	{
		att = hp8672a_get_att( hp8672a.freq );
		if ( att != hp8672a.real_attenuation )
		{
			hp8672a.real_attenuation = att;
			hp8672a_set_attenuation( att );
		}

		hp8672a_set_frequency( hp8672a.freq );
	}

	return vars_push( FLOAT_VAR, hp8672a.freq );
}


/*-------------------------------------------------------------*/
/* This function may only be called in the EXPERIMENT section! */
/*-------------------------------------------------------------*/

Var *synthesizer_sweep_down( Var *v )
{
	Var *nv;


	v = v;
	hp8672a.step_freq *= -1.0;
	nv = synthesizer_sweep_up( NULL );
	hp8672a.step_freq *= -1.0;
	return nv;
}


/*-------------------------------------------------------------*/
/* This function may only be called in the EXPERIMENT section! */
/*-------------------------------------------------------------*/

Var *synthesizer_reset_frequency( Var *v )
{
	v = v;

	if ( ! hp8672a.start_freq_is_set )
	{
		print( FATAL, "No RF frequency has been set yet, so can't do a "
			   "frequency reset.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
		hp8672a.freq = hp8672a.start_freq;
	else
		hp8672a.freq = hp8672a_set_frequency( hp8672a.start_freq );

	return vars_push( FLOAT_VAR, hp8672a.freq );
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
			hp8672a.table_file = T_strdup( DEFAULT_TABLE_FILE );
		else
			hp8672a.table_file = get_string( "%s%s%s", libdir, slash( libdir ),
											 DEFAULT_TABLE_FILE );

		if ( ( tfp = hp8672a_open_table( hp8672a.table_file ) ) == NULL )
		{
			print( FATAL, "Default table file '%s' not found.\n",
				   hp8672a.table_file );
			hp8672a.table_file = CHAR_P T_free( hp8672a.table_file );
			THROW( EXCEPTION );
		}
	}
	else
	{
		vars_check( v, STR_VAR );

		tfname = T_strdup( v->val.sptr );

		too_many_arguments( v );

		TRY
		{
			tfp = hp8672a_find_table( &tfname );
			hp8672a.table_file = tfname;
			TRY_SUCCESS;
		}
		CATCH( EXCEPTION )
		{
			T_free( tfname );
			RETHROW( );
		}
	}

	/* Now try to read in the table file */

	TRY
	{
		hp8672a_read_table( tfp );
		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
	{
		fclose( tfp );
		hp8672a.table_file = CHAR_P T_free( hp8672a.table_file );
		RETHROW( );
	}

	fclose( tfp );
	hp8672a.table_file = CHAR_P T_free( hp8672a.table_file );
	hp8672a.use_table = SET;

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *synthesizer_att_ref_freq( Var *v )
{
	double freq;


	/* Without an argument just return the reference frequency setting */

	if ( v == NULL )
		return vars_push( FLOAT_VAR, hp8672a.att_ref_freq );

	/* Otherwise check the supplied variable */

	freq = get_double( v, "RF attenuation reference frequency" );

	/* Check that the frequency is within the synthesizers range */

	if ( freq > MAX_FREQ || freq < MIN_FREQ )
	{
		print( FATAL, "Reference frequency for attenuation settings of %g MHz "
			   "is out of synthesizer range (%f kHz - %f MHz).\n",
			   hp8672a.att_ref_freq * 1.0e-6,
			   MIN_FREQ * 1.0e-3, MAX_FREQ * 1.0e-6 );
		if ( FSC2_MODE == EXPERIMENT )
			return vars_push( FLOAT_VAR, hp8672a.freq );
		else
			THROW( EXCEPTION );
	}

	too_many_arguments( v );

	hp8672a.att_ref_freq = freq;

	/* If a table has already been loaded calculate the attenuation at the
	   reference frequency */

	if ( hp8672a.use_table )
	{
		if ( hp8672a.att_ref_freq < hp8672a.min_table_freq ||
			 hp8672a.att_ref_freq > hp8672a.max_table_freq )
		{
			print( FATAL, "Reference frequency for attenuation settings of "
				   "%g MHz is not covered by the table.\n",
				   hp8672a.att_ref_freq );
			THROW( EXCEPTION );
		}

		hp8672a.att_at_ref_freq = hp8672a_get_att_from_table( freq );
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
	const char *str[ ] = { "amplitude", "type" };
	double ampl = -1.0;
	int what;
	int type = UNDEFINED;


	if ( v == NULL )
	{
		print( FATAL, "Use functions 'synthesizer_mod_(type|ampl)' "
			   "to determine modulation settings.\n" );
		THROW( EXCEPTION );
	}

	while ( v )
	{
		if ( ( 1 << ( ( res = hp8672a_set_mod_param( v, &ampl, &what ) )
					  - 1 ) ) & set )
			print( SEVERE, "Parameter for modulation %s set more than once in "
				   "call of 'synthesizer_modulation'.\n", str[ res ] );
		else
		{
			switch ( res )
			{
				case 1 :                /* setting modulation amplitude */
					break;

				case 2 :                /* setting modulation type */
					type = what;
					break;

				default :                 /* this definitely can't happen... */
					fsc2_assert( 1 == 0 );
			}
		}
		set |= ( 1 << ( res - 1 ) );
		v = vars_pop( v );
	}

	if ( type != UNDEFINED )
	{
		hp8672a.mod_type = type;
		hp8672a.mod_type_is_set = SET;
	}

	hp8672a.mod_ampl[ hp8672a.mod_type ] = hp8672_mod_ampl_check( ampl );
	hp8672a.mod_ampl_is_set[ hp8672a.mod_type ] = SET;

	if ( FSC2_MODE == EXPERIMENT )
		hp8672a_set_modulation( );

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *synthesizer_mod_type( Var *v )
{
	int res;


	if ( v == NULL )
	{
		if ( ! hp8672a.mod_type_is_set )
			return vars_push( INT_VAR, -1 );

		if ( hp8672a.mod_type != UNDEFINED )
		{
			hp8672a.mod_type_is_set = SET;
			return vars_push( INT_VAR, hp8672a.mod_type );
		}
		else
		{
			hp8672a.mod_type_is_set = UNSET;
			return vars_push( INT_VAR, -1 );
		}
	}

	vars_check( v, STR_VAR | INT_VAR );

	if ( v->type == INT_VAR )
	{
		res = ( int ) v->val.lval;

		if ( res < 0 || res >= NUM_MOD_TYPES )
		{
			print( FATAL, "Invalid modulation type %d.\n", res );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( ( res = is_in( v->val.sptr, mod_types, 3 ) ) == UNDEFINED )
		{
			print( FATAL, "Invalid modulation type '%s'.\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}

	too_many_arguments( v );

	hp8672a.mod_type = res;
	hp8672a.mod_type_is_set = SET;

	if ( FSC2_MODE == EXPERIMENT &&
		 hp8672a.mod_ampl_is_set[ hp8672a.mod_type ] )
		hp8672a_set_modulation( );

	return vars_push( INT_VAR, res );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

Var *synthesizer_mod_ampl( Var *v )
{
	double ampl;


	if ( v == NULL )
	{
		if ( ! hp8672a.mod_type_is_set )
		{
			print( FATAL, "Can't determine modulation amplitude as long as "
				   "modulation type isn't set.\n" );
			THROW( EXCEPTION );
		}

		if ( hp8672a.mod_type == MOD_TYPE_OFF )
		{
			print( FATAL, "Can't determine modulation amplitude when "
				   "modulation is off.\n" );
			THROW( EXCEPTION );
		}

		if ( ! hp8672a.mod_ampl_is_set[ hp8672a.mod_type ] )
		{
			print( FATAL, "Can't determinde modulation ampluitude.\n" );
			THROW( EXCEPTION );
		}

		return vars_push( INT_VAR, hp8672a.mod_ampl[ hp8672a.mod_type ] );
	}

	if ( ! hp8672a.mod_type_is_set )
	{
		print( FATAL, "Can't set modulation amplitude as long as "
			   "modulation type isn't set.\n" );
		THROW( EXCEPTION );
	}

	ampl = get_double( v, "modulation amplitude" );

	too_many_arguments( v );

	hp8672a.mod_ampl[ hp8672a.mod_type ] = hp8672_mod_ampl_check( ampl );
	hp8672a.mod_ampl_is_set[ hp8672a.mod_type ] = SET;

	if ( FSC2_MODE == EXPERIMENT )
		hp8672a_set_modulation( );

	if ( hp8672a.mod_type == MOD_TYPE_AM )
		return vars_push( FLOAT_VAR,
						  am_ampl[ hp8672a.mod_ampl[ hp8672a.mod_type ] ]  );
	return vars_push( FLOAT_VAR,
					  fm_ampl[ hp8672a.mod_ampl[ hp8672a.mod_type ] ]  );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *synthesizer_command( Var *v )
{
	static char *cmd;


	cmd = NULL;
	vars_check( v, STR_VAR );
	
	if ( FSC2_MODE == EXPERIMENT )
	{
		TRY
		{
			cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
			hp8672a_command( cmd );
			T_free( cmd );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( cmd );
			RETHROW( );
		}
	}

	return vars_push( INT_VAR, 1 );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
