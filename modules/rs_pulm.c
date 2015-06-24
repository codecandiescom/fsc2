#include "rs.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
pulm_init( void )
{
#if defined WITH_PULSE_MODULATION
	rs->pulm.has_pulse_mod = true;
#else
	rs->pulm.has_pulse_mod = false;
#endif

    if ( ! rs->pulm.has_pulse_mod )
		return;

	if ( FSC2_MODE == PREPARATION )
	{
		rs->pulm.state_has_been_set = false;
		return;
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ! rs->pulm.state_has_been_set )
			rs->pulm.state = false;

		rs->pulm.pol   = POLARITY_NORMAL;
		rs->pulm.imp   = IMPEDANCE_G50;

		return;
	}

	rs_write( "PULM:SOUR EXT" );

	if ( rs->pulm.state_has_been_set )
	{
		rs->pulm.state = ! rs->pulm.state;
		pulm_set_state( ! rs->pulm.state );
	}
	else
		rs->pulm.state = query_bool( "PULM:STAT?" );

	rs->pulm.pol   = query_pol( "PULM:POL?" );
	rs->pulm.imp   = query_imp( "PULM:TRIG:EXT:IMP?" );

	if (    rs->pulm.imp != IMPEDANCE_G50
		 && rs->pulm.imp != IMPEDANCE_G10K )
		bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
pulm_state( void )
{
	if ( ! rs->pulm.has_pulse_mod )
		return false;

	if ( FSC2_MODE == PREPARATION && ! rs->pm.state_has_been_set )
	{
		print( FATAL, "Pulse modulation state hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->pulm.state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
pulm_set_state( bool state )
{
    pulm_check_has_mod( );

	/* Only allow one type of modulation, so if PULM is switched on
	   disable AM, FM and FM */

	if ( state )
	{
		am_set_state( false );
		fm_set_state( false );
		pm_set_state( false );
	}	

    if ( state == rs->pulm.state )
        return rs->pulm.state;

	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->pulm.state_has_been_set = false;
		return rs->pulm.state = state;
	}

	char cmd[ ] = "PULM:STAT *";
	cmd[ 10 ] = state ? '1' : '0';

	rs_write( cmd );
    return rs->pulm.state = state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Polarity
pulm_polarity( void )
{
    pulm_check_has_mod( );
	return rs->pulm.pol;
}


/*----------------------------------------------------*
 * Set polarity: for Normal polarity RF is on while external
 * pulse is present, for Inverted RF is off while pulse is
 * present.
 *----------------------------------------------------*/

enum Polarity
pulm_set_polarity( enum Polarity pol )
{
    pulm_check_has_mod( );

    if ( pol == rs->pulm.pol )
        return rs->pulm.pol;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->pulm.pol = pol;

	char cmd[ 13 ] = "PULM:POL ";
    strcat( cmd, pol == POLARITY_NORMAL ? "NORM" : "INV" );

    rs_write( cmd );
    return rs->pulm.pol = pol;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
pulm_impedance( void )
{
    pulm_check_has_mod( );
	return rs->pulm.imp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
pulm_set_impedance( enum Impedance imp )
{
    pulm_check_has_mod( );

    if ( imp == rs->pulm.imp )
        return rs->pulm.imp;

    if ( imp != IMPEDANCE_G50 && imp != IMPEDANCE_G10K )
	{
		print( FATAL, "Invalid pulse modulatiuon input impedance %d requested, "
			   "use either \"G50\" or \"G10K\".\n", imp );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
		return rs->pulm.imp = imp;

	char cmd[ 23 ] = "PULM:TRIG:EXT:IMP ";
    strcat( cmd, imp == IMPEDANCE_G50 ? "G50" : "G10K" );

    rs_write( cmd );
    return rs->pulm.imp = imp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
pulm_check_has_mod( void )
{
    if ( ! rs->pulm.has_pulse_mod )
	{
		print( FATAL, "Function can't be used, module was not compiled "
			   "with support for pulse modulation.\n" );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
pulm_available( void )
{
	return rs->pulm.has_pulse_mod;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
