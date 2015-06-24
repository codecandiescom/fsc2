#include "rs.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
am_init( void )
{
	rs->am.depth_resolution = 0.1;

	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->am.state_has_been_set = false;
		return;
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ! rs->am.state_has_been_set )
			rs->am.state    = false;

		rs->am.depth    = 20;
		rs->am.ext_coup = COUPLING_AC;
		rs->am.source   = SOURCE_INT;

		return;
	}

    rs_write( "AM:TYPE LIN" );

	if ( rs->am.state_has_been_set )
	{
		rs->am.state = ! rs->am.state;
		am_set_state( ! rs->am.state );
	}
	else
		rs->am.state    = query_bool( "AM:STAT?" );

    rs->am.depth    = query_double( "AM?" );
    rs->am.ext_coup = query_coupling( "AM:EXT:COUP?" );
    rs->am.source   = query_source( "AM:SOUR?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
am_state( void )
{
	if ( FSC2_MODE == PREPARATION && ! rs->am.state_has_been_set )
	{
		print( FATAL, "AM state hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->am.state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
am_set_state( bool state )
{
    if ( state == rs->am.state )
        return rs->am.state;

	/* Only allow one type of modulation, so if AM is switched on
	   disable FM, PM and PULM */

	if ( state )
	{
		fm_set_state( false );
		pm_set_state( false );
		if ( pulm_available( ) )
			pulm_set_state( false );
	}	

	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->am.state_has_been_set = true;
		return rs->am.state = state;
	}

	char cmd[ 11 ] = "AM:STATE ";
	cmd[ 10 ] = state ? '1' : '0';
    rs_write( cmd );
    return rs->am.state = state;
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

double
am_depth( void )
{
	return rs->am.depth;
}


/*----------------------------------------------------*
 * Note: negative values are allowed!
 *----------------------------------------------------*/

double
am_set_depth( double depth )
{
    if ( ( depth = am_check_depth( depth ) ) == rs->am.depth )
        return rs->am.depth;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->am.depth = depth;

    char cmd[ 100 ];
    sprintf( cmd, "AM %.1f", depth );

    rs_write( cmd );
    return rs->am.depth = depth;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
am_sensitivity( void )
{
	if ( FSC2_MODE != EXPERIMENT )
		return rs->am.depth * ( rs->am.source != SOURCE_INT_EXT ? 1 : 0.5 );
	return query_double( "AM:SENS?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
am_coupling( void )
{
	return rs->am.ext_coup;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
am_set_coupling( enum Coupling coup )
{
    if ( rs->am.ext_coup == coup )
        return rs->am.ext_coup;

	if ( coup != COUPLING_AC && coup != COUPLING_DC )
	{
		print( FATAL, "Invalid coupling %d requested, use either \"AC\" or "
			   "\"DC\".\n", coup );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
		return rs->am.ext_coup = coup;

	char cmd[ ] = "AM:EXT:COUP *C";
	cmd[ 12 ] = coup == COUPLING_AC ? 'A' : 'D';
    rs_write( cmd );
    return rs->am.ext_coup = coup;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
am_source( void )
{
	return rs->am.source;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
am_set_source( enum Source source )
{
    if ( rs->am.source == source )
        return rs->am.source;

	if (    source != SOURCE_INT
		 && source != SOURCE_EXT
		 && source != SOURCE_INT_EXT )
	{
		print( FATAL, "Invalid modulation source %d requested, use either "
			   "\"INT\", \"EXT\" or \"INT_EXT\".\n", source );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
		return rs->am.source = source;

    char cmd[ 16 ] = "AM:SOUR ";
    switch ( source )
    {
        case SOURCE_INT :
            strcat( cmd, "INT" );
            break;

        case SOURCE_EXT :
            strcat( cmd, "EXT" );
            break;

        case SOURCE_INT_EXT :
            strcat( cmd, "INT,EXT" );
            break;
    }

    rs_write( cmd );
    return rs->am.source = source;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
am_check_depth( double depth )
{
    if ( fabs( depth ) >= 100 + 0.5 * rs->am.depth_resolution )
	{
		print( FATAL, "Requested AM modulation depth of %.1f%% is invalid, "
			   "can't be larger than 100%%.\n", depth );
		THROW( EXCEPTION );
	}

    return rs->am.depth_resolution * round( depth / rs->am.depth_resolution );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
