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

	/* Make sure that not more than a single modulation is on (or switch
	   off all */

	int mod_cnt = 0;

	mod_cnt += am_state( );
	mod_cnt += fm_state( );
	mod_cnt += pm_state( );
	mod_cnt += pulm_state( );

	if ( mod_cnt > 1 )
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


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
