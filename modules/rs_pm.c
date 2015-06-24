#include "rs.h"


/* List of maximum frequencu modulation deviations for different frequency
   ranges, with the first value being the upper limit of the frequency range
   (included in the range), while the following array contains the maximum
   modulation frequencies for low noise, normal and high deviation mode.*/

typedef struct
{
	double  upper_freq;
	double devs[ 3 ];
} Mod_Range;

static Mod_Range mod_ranges[ ] = 
                                { {  23.4375e6, {  2.5,    2.5,     2.5  } },
                                  {  46.875e6,  {  0.3125, 0.125,   1.25 } }, 
                                  {  93.75e6,   {  0.625,  0.25,    2.5  } },
                                  { 187.5e6,    {  1.25,   0.5,     5.0  } },
                                  { 375.0e6,    {  2.5,    1.0,    10.0  } },
                                  { 750.0e6,    {  5.0,    2.0,    20.0  } },
                                  {   1.5e9,    { 10.0,    4.0,    40.0  } },
                                  {   3.0e9,    { 20.0,    8.0,    80.0  } },
                                  {   6.375e9,  { 40.0,   16.0,   160.0  } },
                                  {   12.75e9,  { 80.0,   32.0,   320.0  } } };


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
pm_init( void )
{
	if ( FSC2_MODE == PREPARATION )
	{
		rs->pm.state_has_been_set = false;
		return;
	}

	if ( FSC2_MODE != EXPERIMENT )
	{
		if ( ! rs->pm.state_has_been_set )
			rs->pm.state    = false;

		rs->pm.dev      = 0.2;
		rs->pm.ext_coup = COUPLING_AC;
		rs->pm.source   = SOURCE_INT;
		rs->pm.mode     = MOD_MODE_NORMAL;

		return;
	}

	if ( rs->pm.state_has_been_set )
	{
		rs->pm.state = ! rs->pm.state;
		pm_set_state( ! rs->pm.state );
	}
	else
		rs->pm.state    = query_bool( "PM:STAT?" );

    rs->pm.dev      = query_double( "PM?" );
    rs->pm.ext_coup = query_coupling( "PM:EXT:COUP?" );
    rs->pm.source   = query_source( "PM:SOUR?" );
    rs->pm.mode     = query_mod_mode( "PM:MODE?" );

    /* If modulation deviation is larger than possible reduce it to the
       maximum value */

    double max_dev = pm_max_deviation( freq_frequency( ), rs->pm.mode );
    if ( rs->pm.dev > max_dev )
        pm_set_deviation( max_dev );
}


/*----------------------------------------------------*
 * Returns if PM is on or off
 *----------------------------------------------------*/

bool
pm_state( void )
{
	if ( FSC2_MODE == PREPARATION && ! rs->pm.state_has_been_set )
	{
		print( FATAL, "PM state hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->pm.state;
}


/*----------------------------------------------------*
 * Switches RF on or off
 *----------------------------------------------------*/

bool
pm_set_state( bool state )
{
    if ( state == rs->pm.state )
        return rs->pm.state;

	/* Only allow one type of modulation, so if PM is switched on
	   disable AM, FM and PULM */

    if ( state )
	{
		am_set_state( false );
		fm_set_state( false );
		if ( pulm_available( ) )
			pulm_set_state( false );
	}

	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->pm.state_has_been_set = true;
		return rs->pm.state = state;
	}

	char cmd[ ] = "PM:STATE x";
    cmd[ 9 ] = state ? '1' : '0';
    rs_write( cmd );
    return rs->pm.state = state;
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

double
pm_deviation( void )
{
	return rs->pm.dev;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
pm_set_deviation( double dev )
{
    if ( ( dev = pm_check_deviation( dev, freq_frequency(), rs->pm.mode ) )
		                                                        == rs->pm.dev )
        return rs->pm.dev;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->pm.dev = dev;

    char cmd[ 100 ];
    sprintf( cmd, "PM %.1f", dev );

	rs_write( cmd );
    return rs->pm.dev = dev;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
pm_sensitivity( void )
{
	if ( FSC2_MODE != EXPERIMENT )
		return rs->pm.dev * ( rs->pm.source != SOURCE_INT_EXT ? 1 : 0.5 );
	return query_double( "PM:SENS?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
pm_coupling( void )
{
	return rs->fm.ext_coup;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
pm_set_coupling( enum Coupling coup )
{
    if ( rs->pm.ext_coup == coup )
        return rs->pm.ext_coup;

	if ( coup != COUPLING_AC && coup != COUPLING_DC )
	{
		print( FATAL, "Invalid coupling %d requested, use either \"AC\" or "
			   "\"DC\".\n", coup );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
        return rs->pm.ext_coup = coup;

    char cmd[ ] = "PM:EXT:COUP *C";
    cmd[ 12 ] = coup == COUPLING_AC ? 'A' : 'D';
    rs_write( cmd );
    return rs->pm.ext_coup = coup;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
pm_source( void )
{
	return rs->pm.source;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
pm_set_source( enum Source source )
{
    if ( rs->pm.source == source )
        return rs->pm.source;

	if (    source != SOURCE_INT
		 && source != SOURCE_EXT
		 && source != SOURCE_INT_EXT )
	{
		print( FATAL, "Invalid modulation source %d requested, use either "
			   "\"INT\", \"EXT\" or \"INT_EXT\".\n", source );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
		return rs->pm.source = source;

    char cmd[ 16 ] = "PM:SOUR ";
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
    return rs->pm.source = source;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Mode
pm_mode( void )
{
	return rs->pm.mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Mode
pm_set_mode( enum Mod_Mode mode )
{
    if ( rs->pm.mode == mode )
        return rs->pm.mode;

	if (    mode != MOD_MODE_NORMAL
		 && mode != MOD_MODE_LOW_NOISE
		 && mode != MOD_MODE_HIGH_DEVIATION )
	{
		print( FATAL, "Invalid modulation mode %d requested, use either "
			   "\"NORMAL\", \"LOW_NOISE\" or \"HIGH_DEVIATION\".\n", mode );
		THROW( EXCEPTION );
	}

    /* If with the new mode being set the modulation deviation would become
       too karge reduce it to the maximum possible value */

    double max_dev = pm_max_deviation( freq_frequency( ), mode );
    if ( rs->pm.dev > max_dev )
	{
		print( WARN, "Adjusting PM deviation from %.2f Hz to  %.2f Hz.\n",
			   rs->pm.dev, max_dev );
        pm_set_deviation( max_dev );
	}

	if ( FSC2_MODE != EXPERIMENT )
        return rs->pm.mode = mode;

    char cmd[ 13 ] = "PM:MODE ";
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
    return rs->pm.mode = mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
pm_max_deviation( double        freq,
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
pm_check_deviation( double        dev,
					double        freq,
					enum Mod_Mode mode )
{
	double max_dev = pm_max_deviation( freq, mode );

    if ( dev > max_dev )
	{
		print( FATAL, "Requested PM deviation of %f rad out of range, can't "
			   "be larger than %f rad.\n", dev, max_dev );
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
