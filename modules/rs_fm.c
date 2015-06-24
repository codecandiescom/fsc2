#include "rs.h"


/* List of maximum frequencu modulation deviations for different frequency
   ranges, with the first value being the upper limit of the frequency range
   (included in the range), while the following array contains the maximum
   modulation frequencies for low noise, normal and high deviation mode.*/

typedef struct
{
	double upper_freq;
	double devs[ 3 ];
} Mod_Range;

static Mod_Range mod_ranges[ ] = 
                            { {  23.4375e6, {  1.0e6,     1.0e6,   1.0e6 } },
                              {  46.875e6,  {  31.25e3,  62.5e3, 125.0e3 } }, 
                              {  93.75e6,   {  62.5e3,  125.0e3, 250.0e3 } },
                              { 187.5e6,    { 125.0e3,  250.0e3, 500.0e3 } },
                              { 375.0e6,    { 250.0e3,  500.0e3,   1.0e6 } },
                              { 750.0e6,    { 500.0e3,    1.0e6,   2.0e6 } },
                              {   1.5e9,    {   1.0e6,    2.0e6,   4.0e6 } },
                              {   3.0e9,    {   2.0e6,    4.0e6,   8.0e6 } },
                              {   6.375e9,  {   4.0e6,    8.0e6,  16.0e6 } },
                              {  12.75e9,   {   8.0e6,   16.0e6,  32.0e6 } } };


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
fm_init( void )
{
	if ( FSC2_MODE == PREPARATION )
	{
		rs->fm.state_has_been_set = false;
		return;
	}

	if ( FSC2_MODE == TEST )
	{
		if (  ! rs->fm.state_has_been_set )
			rs->fm.state    = false;

		rs->fm.dev      = 10e3;
		rs->fm.ext_coup = COUPLING_AC;
		rs->fm.source   = SOURCE_INT;
		rs->fm.mode     = MOD_MODE_NORMAL;

		return;
	}

	if ( rs->fm.state_has_been_set )
	{
		rs->fm.state = ! rs->fm.state;
		fm_set_state( ! rs->fm.state );
	}
	else
		rs->fm.state    = query_bool( "FM:STAT?" );

    rs->fm.dev      = query_double( "FM?" );
    rs->fm.ext_coup = query_coupling( "FM:EXT:COUP?" );
    rs->fm.source   = query_source( "FM:SOUR?" );
    rs->fm.mode     = query_mod_mode( "FM:MODE?" );

    /* If modulation deviation is larger than possible reduce it to the
       maximum value */

    double max_dev = fm_max_deviation( freq_frequency( ), rs->fm.mode );
    if ( rs->fm.dev > max_dev )
        fm_set_deviation( max_dev );
}


/*----------------------------------------------------*
 * Returns if FM is on or off
 *----------------------------------------------------*/

bool
fm_state( void )
{
	if ( FSC2_MODE == PREPARATION && ! rs->fm.state_has_been_set )
	{
		print( FATAL, "FM state hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->fm.state;
}


/*----------------------------------------------------*
 * Switches RF on or off
 *----------------------------------------------------*/

bool
fm_set_state( bool state )
{
    if ( state == rs->fm.state )
        return rs->fm.state;

	/* Only allow one type of modulation, so if FM is switched on
	   disable AM, PM and PULM */

    if ( state )
	{
		am_set_state( false );
		pm_set_state( false );
		if ( pulm_available( ) )
			pulm_set_state( false );
	}

	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->fm.state_has_been_set = true;
		return rs->fm.state = state;
	}

	char cmd[ ] = "FM:STATE x";
    cmd[ 9 ] = state ? '1' : '0';
    rs_write( cmd );
    return rs->fm.state = state;
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

double
fm_deviation( void )
{
	return rs->fm.dev;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
fm_set_deviation( double dev )
{
    if ( ( dev = fm_check_deviation( dev, freq_frequency( ), rs->fm.mode ) )
		                                                        == rs->fm.dev )
        return rs->fm.dev;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->fm.dev = dev;

    char cmd[ 100 ];
    sprintf( cmd, "FM %.1f", dev );

	rs_write( cmd );
    return rs->fm.dev = dev;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
fm_sensitivity( void )
{
	if ( FSC2_MODE != EXPERIMENT )
		return rs->fm.dev * ( rs->fm.source != SOURCE_INT_EXT ? 1 : 0.5 );
	return query_double( "FM:SENS?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
fm_coupling( void )
{
	return rs->fm.ext_coup;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
fm_set_coupling( enum Coupling coup )
{
    if ( rs->fm.ext_coup == coup )
        return rs->fm.ext_coup;

	if ( coup != COUPLING_AC && coup != COUPLING_DC )
	{
		print( FATAL, "Invalid coupling %d requested, use either \"AC\" or "
			   "\"DC\".\n", coup );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
        return rs->fm.ext_coup = coup;

    char cmd[ ] = "FM:EXT:COUP *C";
    cmd[ 12 ] = coup == COUPLING_AC ? 'A' : 'D';
    rs_write( cmd );
    return rs->fm.ext_coup = coup;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
fm_source( void )
{
	return rs->fm.source;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
fm_set_source( enum Source source )
{
    if ( rs->fm.source == source )
        return rs->fm.source;

	if (    source != SOURCE_INT
		 && source != SOURCE_EXT
		 && source != SOURCE_INT_EXT )
	{
		print( FATAL, "Invalid modulation source %d requested, use either "
			   "\"INT\", \"EXT\" or \"INT_EXT\".\n", source );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
		return rs->fm.source = source;

    char cmd[ 16 ] = "FM:SOUR ";
    switch ( source )
    {
        case SOURCE_INT :
            strcat( cmd, "INT" );
            break;

        case SOURCE_EXT :
            strcat( cmd, "Ext" );
            break;

        case SOURCE_INT_EXT :
            strcat( cmd, "INT,EXT" );
            break;
    }

    rs_write( cmd );
    return rs->fm.source = source;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Mode
fm_mode( void )
{
	return rs->fm.mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Mode
fm_set_mode( enum Mod_Mode mode )
{
    if ( rs->fm.mode == mode )
        return rs->fm.mode;

	if (    mode != MOD_MODE_NORMAL
		 && mode != MOD_MODE_LOW_NOISE
		 && mode != MOD_MODE_HIGH_DEVIATION )
	{
		print( FATAL, "Invalid modulation mode %d requested, use either "
			   "\"NORMAL\", \"LOW_NOISE\" or \"HIGH_DEVIATION\".\n", mode );
		THROW( EXCEPTION );
	}

    /* If with the new mode being set the modulation deviation would become
       too large reduce it to the maximum possible value */

    double max_dev = fm_max_deviation( freq_frequency( ), mode );
    if ( rs->fm.dev > max_dev )
	{
		print( WARN, "Adjusting FM deviation from %.2f Hz to  %.2f Hz.\n",
			   rs->fm.dev, max_dev );
        fm_set_deviation( max_dev );
	}

	if ( FSC2_MODE != EXPERIMENT )
        return rs->fm.mode = mode;

    char cmd[ 13 ] = "FM:MODE ";
    switch ( mode )
    {
        case MOD_MODE_NORMAL :
            strcat( cmd, "NORM" );
            break;

        case MOD_MODE_LOW_NOISE :
            strcat( cmd, "LNO" );
            break;

        case MOD_MODE_HIGH_DEVIATION :
            strcat( cmd, "HDEV" );
            break;
    }

    rs_write( cmd );
    return rs->fm.mode = mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
fm_max_deviation( double        freq,
				  enum Mod_Mode mode )
{
    if ( freq < 0 )
        freq = freq_frequency( );
	else
		freq = freq_check_frequency( freq );

    size_t ind;

    switch ( mode )
    {
        case MOD_MODE_LOW_NOISE :
            ind = 0;
            break;

        case MOD_MODE_NORMAL :
            ind = 1;
            break;

        case MOD_MODE_HIGH_DEVIATION :
            ind = 2;
            break;
    }

    for ( size_t i = 0; i < sizeof mod_ranges / sizeof *mod_ranges; i++ )
        if ( freq <= mod_ranges[ i ].upper_freq )
            return mod_ranges[ i ].devs[ ind ];

	fsc2_impossible( );
	return -1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
fm_check_deviation( double        dev,
					double        freq,
					enum Mod_Mode mode )
{
	double max_dev = fm_max_deviation( freq, mode );

    if ( dev > max_dev )
	{
		print( FATAL, "Requested FM deviation of %f Hz out of range, can't "
			   "be larger than %f Hz.\n", dev, max_dev );
		THROW( EXCEPTION );
	}

    return lrnd( dev );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
