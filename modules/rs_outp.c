#include "rs.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
outp_init( void )
{
    if ( FSC2_MODE == PREPARATION )
    {
        rs->outp.state_has_been_set = false;
        return;
    }

	if ( FSC2_MODE == TEST )
	{
        if ( ! rs->outp.state_has_been_set )
            rs->outp.state = false;
		rs->outp.imp = IMPEDANCE_G50;
		return;
	}

    /* Switch to auto-attenuator mode to be able to use the full attenuator
       range */

	rs_write( "OUTP:AMOD AUTO" );

    /* Switch RF power off (or to minimum level) during ALC searches */

    rs_write( "OUTP:ALC:SEAR:MODE MIN" );

    if ( rs->outp.state_has_been_set )
        rs->outp.state = query_bool( "OUTP:STATE?" );
    else
    {
        rs->outp.state = ! rs->outp.state;
        outp_set_state( ! rs->outp.state );
    }

    rs->outp.imp   = query_imp( "OUTP:IMP?" );

    if (    rs->outp.imp != IMPEDANCE_G50
         && rs->outp.imp != IMPEDANCE_G1K
         && rs->outp.imp != IMPEDANCE_G10K )  
        bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
outp_state( void )
{
    if ( FSC2_MODE == PREPARATION && ! rs->outp.state_has_been_set )
    {
        print( FATAL, "Output state hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

	return rs->outp.state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
outp_set_state( bool state )
{
    if ( state == rs->outp.state )
        return rs->outp.state;

    // Check if we're currently processing a list and, if yes, stop it.

    if ( ! state )
        list_stop( false );

	if ( FSC2_MODE == EXPERIMENT )
	{
		char cmd[ ] = "OUTP:STATE x";
		cmd[ 11 ] = state ? '1' : '0';

		rs_write( cmd );
	}

    if ( FSC2_MODE == PREPARATION )
        rs->outp.state_has_been_set = true;

	return rs->outp.state = state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
outp_impedance( void )
{
	return rs->outp.imp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
outp_protection_is_tripped( void )
{
	if ( FSC2_MODE != EXPERIMENT )
		return false;

    return query_bool( "OUTP:PROT:TRIP?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
outp_reset_protection( void )
{
	if ( FSC2_MODE != EXPERIMENT )
		return rs->outp.state;

    rs_write( "OUTP:PROT:CLE" );
    return rs->outp.state = query_bool( "OUTP:STATE?" );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
