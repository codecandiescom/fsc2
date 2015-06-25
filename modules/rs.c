#include "rs.h"


rs_smb100a_T * rs;
rs_smb100a_T   rs_prep,
               rs_test,
               rs_exp;


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_smb100a_init_hook( void )
{
	Need_LAN = true;

	rs_init( &rs_prep );
	return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_smb100a_test_hook( void )
{
	rs_test = rs_prep;

	rs_init( &rs_test );
	return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_smb100a_end_of_test_hook( void )
{
	rs_cleanup( );
	return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_smb100a_exp_hook( void )
{
	rs_exp = rs_prep;

	rs_init( &rs_exp );

	// Ascertain that not more than one modulation is on (or switch off all)

	if ( am_state( ) + fm_state( ) + pm_state( ) + pulm_state( ) > 1 )
	{
		am_set_state( false );
		fm_set_state( false );
		pm_set_state( false );
		if ( pulm_available( ) )
			pulm_set_state( false );
	}

	return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_smb100a_end_of_exp_hook( void )
{
	rs_cleanup( );
	return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_name( Var_T * v  UNUSED_ARG )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_state( Var_T * v )
{
	if ( ! v )
		return vars_push( INT_VAR, outp_state( ) ? 1L : 0L );

	bool state = get_boolean( v );
	too_many_arguments( v );

	return vars_push( INT_VAR, outp_set_state( state ) ? 1L : 0L );
}
		

/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_frequency( Var_T * v )
{
	if ( ! v )
		return vars_push( FLOAT_VAR, freq_frequency( ) );

	double freq = get_double( v, NULL );
	too_many_arguments( v );

	return vars_push( FLOAT_VAR, freq_set_frequency( freq ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_attenuation( Var_T * v )
{
	if ( ! v )
		return vars_push( FLOAT_VAR, pow_power( ) );

	double pow = get_double( v, NULL );
	too_many_arguments( v );

	return vars_push( FLOAT_VAR, pow_set_power( pow ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_min_attenuation( Var_T * v )
{
	if ( ! v )
		return vars_push( FLOAT_VAR, pow_max_power( freq_frequency( ) ) );

	double freq = get_double( v, NULL );
	too_many_arguments( v );

	return vars_push( FLOAT_VAR, pow_max_power( freq ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_max_attenuation( Var_T * v )
{
	too_many_arguments( v );

	return vars_push( FLOAT_VAR, pow_min_power( ) );
}

		
/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_automatic_level_control( Var_T * v )
{
	if ( ! v )
		return vars_push( INT_VAR, ( long ) pow_alc_state( ) );

	long alc_state;

	if ( v->type == STR_VAR )
	{
		if ( ! strcasecmp( v->val.sptr, "OFF" ) )
			alc_state = ALC_STATE_OFF;
		else if ( ! strcasecmp( v->val.sptr, "ON" ) )
			alc_state = ALC_STATE_ON;
		else if ( ! strcasecmp( v->val.sptr, "AUTO" ) )
			alc_state = ALC_STATE_AUTO;
		else
		{
			print( FATAL, "Invalid ALC state \"%s\".\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}
	else
		alc_state = get_strict_long( v, NULL );

	too_many_arguments( v );

	return vars_push( INT_VAR, ( long ) pow_set_alc_state( alc_state ) );
}

		
/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_output_impedance( Var_T * v  UNUSED_ARG )
{
	return vars_push( INT_VAR, impedance_to_int( outp_impedance( ) ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_protection_tripped( Var_T * v  UNUSED_ARG )
{
	return vars_push( INT_VAR, outp_protection_is_tripped( ) ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_reset_protection( Var_T * v  UNUSED_ARG )
{
	outp_reset_protection( );
	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_rf_mode( Var_T * v )
{
	if ( ! v )
		return vars_push( INT_VAR, ( long ) pow_mode( ) );

	long mode;

	if ( v->type == STR_VAR )
	{
		if (    ! strcasecmp( v->val.sptr, "NORMAL" )
			 || ! strcasecmp( v->val.sptr, "NORM" ) )
			mode = POWER_MODE_NORMAL;
		else if (    ! strcasecmp( v->val.sptr, "LOW_NOISE")
				  || ! strcasecmp( v->val.sptr, "LOWN" ) )
			mode = POWER_MODE_LOW_NOISE;
		else if (    ! strcasecmp( v->val.sptr, "LOW_DISTORTION")
				  || ! strcasecmp( v->val.sptr, "LOWD" ) )
			mode = POWER_MODE_LOW_DISTORTION;
		else
		{
			print( FATAL, "Invalid RD mode \"%s\".\n" );
			THROW( EXCEPTION );
		}
	}
	else
		mode = get_strict_long( v, NULL );

	too_many_arguments( v );

	return vars_push( INT_VAR, ( long ) pow_set_mode( mode ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_rf_off_mode( Var_T * v )
{
	if ( ! v )
		return vars_push( INT_VAR, ( long ) pow_off_mode( ) );

	long mode;

	if ( v->type == STR_VAR )
	{
		if (    ! strcasecmp( v->val.sptr, "UNCHANGED" )
			 || ! strcasecmp( v->val.sptr, "UNCH" ) )
			mode = OFF_MODE_UNCHANGED;
		else if (    ! strcasecmp( v->val.sptr, "FULL_ATTENUATION")
				  || ! strcasecmp( v->val.sptr, "FATT" ) )
			mode = OFF_MODE_FATT;
		else
		{
			print( FATAL, "Invalid RF OFF mode \"%s\".\n" );
			THROW( EXCEPTION );
		}
	}
	else
		mode = get_strict_long( v, NULL );

	too_many_arguments( v );

	return vars_push( INT_VAR, ( long ) pow_set_off_mode( mode ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_mod_freq( Var_T * v )
{
	if ( ! v )
		return vars_push( FLOAT_VAR, lfo_frequency( ) );

	double freq = get_double( v, "modulation frequency" );
	too_many_arguments( v );

	return vars_push( FLOAT_VAR, lfo_set_frequency( freq ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_mod_output_impedance( Var_T * v )
{
	if ( ! v )
		return vars_push( INT_VAR, impedance_to_int( lfo_impedance( ) ) );

	long imp;

	if ( v->type == STR_VAR )
	{
		if (    ! strcasecmp( v->val.sptr, "LOW" )
			 || ! strcasecmp( v->val.sptr, "10" ) )
			imp = IMPEDANCE_LOW;
		else if (    ! strcasecmp( v->val.sptr, "G50" )
			      || ! strcasecmp( v->val.sptr, "50" ) )
			imp = IMPEDANCE_G50;
		else
		{
			print( FATAL, "Invalid modulation output impedance \"%s\", use "
				   "either \"LOW\" or \"G50\".\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}
	else
	{
		imp = int_to_impedance( get_long( v, "modulation output impedance" ) );

		if ( imp != IMPEDANCE_LOW && imp != IMPEDANCE_G50 )
		{
			print( FATAL, "Can't use impedance of %d Ohm, must be either "
				   "\"LOW\" (10 Ohm) or \"G50\" (50 Ohm).\n",
				   impedance_to_int( imp ) );
			THROW( EXCEPTION );
		}
	}

	return vars_push( INT_VAR, lfo_set_impedance( imp ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_mod_output_voltage( Var_T * v )
{
	if ( ! v )
		return vars_push( FLOAT_VAR, lfo_voltage( ) );

	double volts = get_double( v, "modulation output voltage" );
	too_many_arguments( v );

	return vars_push( FLOAT_VAR, lfo_set_voltage( volts ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_mod_type( Var_T * v )
{
	if ( ! v )
		return vars_push( INT_VAR, ( long ) mod_type( ) );

	long type;

	if ( v->type == STR_VAR )
	{
		if (    ! strcasecmp( v->val.sptr, "AM" )
			 || ! strcasecmp( v->val.sptr, "AMPLITUDE" ) )
			type = MOD_TYPE_AM;
		else if (    ! strcasecmp( v->val.sptr, "FM" )
				  || ! strcasecmp( v->val.sptr, "FREQUENCY" ) )
			type = MOD_TYPE_FM;
		else if (    ! strcasecmp( v->val.sptr, "PM" )
				  || ! strcasecmp( v->val.sptr, "PHASE" ) )
			type = MOD_TYPE_PM;
		else if (    ! strcasecmp( v->val.sptr, "PULM" )
				  || ! strcasecmp( v->val.sptr, "PULSE" ) )
			type = MOD_TYPE_PULM;
		else
		{
			print( FATAL, "Invalid modulation type \"%s\".\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}
	else
		type = get_strict_long( v, "modulattion tyoe" );

	too_many_arguments( v );

	return vars_push( INT_VAR, ( long ) mod_set_type( type ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_mod_state( Var_T * v )
{
	if ( ! v )
		return vars_push( INT_VAR, mod_state( ) ? 1L : 0L );

	bool state = get_boolean( v );
	too_many_arguments( v );

	return vars_push( INT_VAR, mod_set_state( state ) ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_mod_amp( Var_T * v )
{
	if ( ! v )
		return vars_push( FLOAT_VAR, mod_amplitude( ) );

	double amp = get_double( v, "modulation amplitude" );
	too_many_arguments( v );

	return vars_push( FLOAT_VAR, mod_set_amplitude( amp ) );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_mod_source( Var_T * v )
{
	if ( ! v )
	{
		if ( mod_type( ) == MOD_TYPE_PULM )
			return vars_push( INT_VAR, ( long ) MOD_SOURCE_EXT_DC );

		enum Source source  = mod_source( );

		if ( source == SOURCE_INT )
			return vars_push( INT_VAR, ( long ) MOD_SOURCE_INT );

		vars_push( INT_VAR,
				   ( long ) ( mod_coupling( ) == COUPLING_AC ?
							  MOD_SOURCE_EXT_AC : MOD_SOURCE_EXT_DC ) );
	}

	if ( mod_type( ) == MOD_TYPE_PULM )
	{
		print( FATAL, "Can't set modulation source for pulse modulation, "
			   "it's always EXT.\n" );
		THROW( EXCEPTION );
	}

	long ms;

	if ( v->type == STR_VAR )
	{
		if (    ! strcasecmp( v->val.sptr, "AC" )
			 || ! strcasecmp( v->val.sptr, "EXT_AC" ) )
			ms = MOD_SOURCE_EXT_AC;
		else if (    ! strcasecmp( v->val.sptr, "DC" )
			      || ! strcasecmp( v->val.sptr, "EXT_DC" ) )
			ms = MOD_SOURCE_EXT_DC;
		else if (    ! strcasecmp( v->val.sptr, "INT" ) )
			ms = MOD_SOURCE_INT;
		else
		{
			print( FATAL, "Invalid modulation source \"%s\".\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}
	else
	{
		ms = get_strict_long( v, "modulation source" );
		if (    ms != MOD_SOURCE_EXT_AC
			 && ms != MOD_SOURCE_EXT_DC
			 && ms != MOD_SOURCE_INT )
		{
			print( FATAL, "Invalid modulation source %s.\n", mod_source );
			THROW( EXCEPTION );
		}	
	}				

	if ( ms == MOD_SOURCE_INT && ( v = vars_pop( v ) ) )
		lfo_set_frequency( get_double( v, "modulation frequency" ) );

	too_many_arguments( v );

	if ( ms == MOD_SOURCE_INT )
		mod_set_source( SOURCE_INT );
	else
	{
		mod_set_source( SOURCE_EXT );
		mod_set_coupling( ms == MOD_SOURCE_EXT_AC ?
						  COUPLING_AC : COUPLING_DC );
	}

	return vars_push( INT_VAR, ms );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
