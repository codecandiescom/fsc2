#include "rs.h"


static void am_prep_init( void );
static void am_test_init( void );
static void am_exp_init( void );


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
am_init( void )
{
	if ( FSC2_MODE == PREPARATION )
		am_prep_init( );
	else if ( FSC2_MODE == TEST )
		am_test_init( );
	else
		am_exp_init( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
am_prep_init( void )
{
	rs->am.depth_resolution = 0.1;

	rs->am.state = false;

	rs->am.depth_has_been_set    = false;
	rs->am.ext_coup_has_been_set = false;
	rs->am.source_has_been_set   = false;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
am_test_init( void )
{
	rs->am.state    = false;

	if ( ! rs->am.depth_has_been_set )
	{
		rs->am.depth = 20;
		rs->am.depth_has_been_set = true;
	}

	if ( ! rs->am.ext_coup_has_been_set )
	{
		rs->am.ext_coup = COUPLING_AC;
		rs->am.ext_coup_has_been_set = true;
	}

	if ( rs->am.source_has_been_set )
	{
		rs->am.source = SOURCE_INT;
		rs->am.source_has_been_set = true;
	}
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
am_exp_init( void )
{
    rs_write( "AM:TYPE LIN" );

	rs->am.state = query_bool( "AM:STAT?" );

	if ( rs->am.depth_has_been_set )
	{
		rs->am.depth_has_been_set = false;
		am_set_depth( rs->am.depth );
	}
	else
	{
		rs->am.depth = query_double( "AM?" );
		rs->am.depth_has_been_set = true;
	}

	if ( rs->am.ext_coup_has_been_set )
	{
		rs->am.ext_coup_has_been_set = false;
		am_set_coupling( rs->am.ext_coup );
	}
	else
	{
		rs->am.ext_coup = query_coupling( "AM:EXT:COUP?" );
		rs->am.ext_coup_has_been_set = true;
	}

	if ( rs->am.source_has_been_set )
	{
		rs->am.source_has_been_set = false;
		am_set_source( rs->am.source );
	}
	else
	{
		rs->am.source   = query_source( "AM:SOUR?" );
		if ( rs->am.source == SOURCE_INT_EXT )
			am_set_source( SOURCE_INT );
		else
			rs->am.source_has_been_set = true;
	}
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
am_state( void )
{
	return rs->am.state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
am_set_state( bool state )
{
    if ( state == rs->am.state )
        return rs->am.state;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->am.state = state;

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
	if ( ! rs->am.depth_has_been_set )
	{
		print( FATAL, "Amplitude modulation depth hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->am.depth;
}


/*----------------------------------------------------*
 * Note: negative values are allowed!
 *----------------------------------------------------*/

double
am_set_depth( double depth )
{
    if (    rs->am.depth_has_been_set
		 && ( depth = am_check_depth( depth ) ) == rs->am.depth )
        return rs->am.depth;

	rs->am.depth_has_been_set = true;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->am.depth = depth;

    char cmd[ 100 ];
    sprintf( cmd, "AM %.1f", depth );

    rs_write( cmd );
    return rs->am.depth = depth;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
am_coupling( void )
{
	if ( ! rs->am.ext_coup_has_been_set )
	{
		print( FATAL, "Amplitude modulation coupling hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->am.ext_coup;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
am_set_coupling( enum Coupling coup )
{
    if ( rs->am.ext_coup_has_been_set && rs->am.ext_coup == coup )
        return rs->am.ext_coup;

	if ( coup != COUPLING_AC && coup != COUPLING_DC )
	{
		print( FATAL, "Invalid coupling %d requested, use either \"AC\" or "
			   "\"DC\".\n", coup );
		THROW( EXCEPTION );
	}

	rs->am.ext_coup_has_been_set = true;

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
	if ( ! rs->am.source_has_been_set )
	{
		print( FATAL, "Amplitude modulation source hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->am.source;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
am_set_source( enum Source source )
{
    if ( rs->am.source_has_been_set && rs->am.source == source )
        return rs->am.source;

	if ( source != SOURCE_INT && source != SOURCE_EXT )
	{
		print( FATAL, "Invalid modulation source %d requested, use either "
			   "\"INTERN\" or \"EXTERN\".\n", source );
		THROW( EXCEPTION );
	}

	rs->am.source_has_been_set = true;

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
			fsc2_impossible( );
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
