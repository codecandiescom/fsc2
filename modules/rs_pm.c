/* -*-C-*-
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "rs.h"


static void pm_prep_init( void );
static void pm_test_init( void );
static void pm_exp_init( void );


/* List of maximum phase modulation deviations for different RF frequency
   ranges, with the first value being the upper limit of the RF frequency range
   (included in the range), while the following array contains the maximum
   modulation phases for low noise, normal and high deviation mode.*/

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
		pm_prep_init( );
	else if ( FSC2_MODE == TEST )
		pm_test_init( );
	else
		pm_exp_init( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
pm_prep_init( void )
{
	rs->pm.state = false;

	rs->pm.dev_has_been_set      = false;
	rs->pm.ext_coup_has_been_set = false;
	rs->pm.source_has_been_set   = false;
	rs->pm.mode_has_been_set     = false;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
pm_test_init( void )
{
	if ( ! rs->pm.dev_has_been_set )
	{
		rs->pm.dev = 10e3;
		rs->pm.dev_has_been_set = true;
	}

	if ( ! rs->pm.ext_coup_has_been_set )
	{
		rs->pm.ext_coup = COUPLING_AC;
		rs->pm.ext_coup_has_been_set = true;
	}

	if ( ! rs->pm.source_has_been_set )
	{
		rs->pm.source = SOURCE_INT;
		rs->pm.source_has_been_set = true;
	}

	if ( ! rs->pm.mode_has_been_set )
	{
		rs->pm.mode = MOD_MODE_NORMAL;
		rs->pm.mode_has_been_set = true;
	}
}
		

/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
pm_exp_init( void )
{
	rs->pm.state    = query_bool( "PM:STAT?" );

	if ( ! ( rs->pm.ext_coup_has_been_set ^= 1 ) )
		pm_set_coupling( rs->pm.ext_coup );
	else
		rs->pm.ext_coup = query_coupling( "PM:EXT:COUP?" );

	if ( ! ( rs->pm.source_has_been_set ^= 1 ) )
		pm_set_source( rs->pm.source );
	else if ( ( rs->pm.source = query_source( "PM:SOUR?" ) ) == SOURCE_INT_EXT )
        pm_set_source( SOURCE_INT );

	if ( ! ( rs->pm.mode_has_been_set ^= 1 ) )
		pm_set_mode( rs->pm.mode );
	else
		rs->pm.mode = query_mod_mode( "PM:MODE?" );

    /* If modulation deviation is larger than possible reduce it to the
       maximum value */

    double max_dev = pm_max_deviation( freq_frequency( ), pm_mode( ) );

	if ( ! ( rs->pm.dev_has_been_set ^= 1 ) )
	{
		if ( rs->pm.dev > max_dev )
		{
			print( WARN, "Adjusting PM deviation from %.1f Hz to %.1f Hz.\n",
				   rs->pm.dev, max_dev );
			rs->pm.dev = max_dev;
		}

		pm_set_deviation( rs->pm.dev );
	}
	else if ( ( rs->pm.dev = query_double( "PM?" ) ) > max_dev )
    {
        print( WARN, "Adjusting PM deviation from %.1f Hz to %.1f Hz.\n",
               rs->pm.dev, max_dev );
        pm_set_deviation( max_dev );
    }
}


/*----------------------------------------------------*
 * Returns if PM is on or off
 *----------------------------------------------------*/

bool
pm_state( void )
{
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

	if ( FSC2_MODE != EXPERIMENT )
		return rs->pm.state = state;

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
	if ( ! rs->pm.dev_has_been_set )
	{
		print( FATAL, "Phase modulation deviation hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->pm.dev;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
pm_set_deviation( double dev )
{
    if ( rs->pm.mode_has_been_set && rs->freq.freq_has_been_set )
		dev = pm_check_deviation( dev, freq_frequency( ), pm_mode( ) );

    if ( rs->pm.dev_has_been_set && rs->pm.dev == dev )
        return rs->pm.dev;

	rs->pm.dev_has_been_set = true;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->pm.dev = dev;

    char cmd[ 100 ];
    sprintf( cmd, "PM %.1f", dev );
	rs_write( cmd );

    return rs->pm.dev = dev;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
pm_coupling( void )
{
	if ( ! rs->pm.ext_coup_has_been_set )
	{
		print( FATAL, "Phase modulation coupling hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->pm.ext_coup;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
pm_set_coupling( enum Coupling coup )
{
    if ( rs->pm.ext_coup_has_been_set && rs->pm.ext_coup == coup )
        return rs->pm.ext_coup;

	if ( coup != COUPLING_AC && coup != COUPLING_DC )
	{
		print( FATAL, "Invalid coupling %d requested, use either \"AC\" or "
			   "\"DC\".\n", coup );
		THROW( EXCEPTION );
	}

	rs->pm.ext_coup_has_been_set = true;

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
	if ( ! rs->pm.source_has_been_set )
	{
		print( FATAL, "Phase modulation source hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->pm.source;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
pm_set_source( enum Source source )
{
    if ( rs->pm.source_has_been_set && rs->pm.source == source )
        return rs->pm.source;

	if ( source != SOURCE_INT && source != SOURCE_EXT )
	{
		print( FATAL, "Invalid modulation source %d requested, use either "
			   "\"INTERN\" or \"EXTERN\".\n", source );
		THROW( EXCEPTION );
	}

	rs->pm.source_has_been_set = true;

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
			fsc2_impossible( );					\
    }
    rs_write( cmd );

    return rs->pm.source = source;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Mode
pm_mode( void )
{
	if ( ! rs->pm.mode_has_been_set )
	{
		print( FATAL, "Phase modulation mode hasn't been set yet.\n" );
		THROW( EXCEPTION );
	}

	return rs->pm.mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Mode
pm_set_mode( enum Mod_Mode mode )
{
    if ( rs->pm.mode_has_been_set && rs->pm.mode == mode )
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
       too large reduce it to the maximum possible value */

    if ( rs->freq.freq_has_been_set )
	{
		double max_dev = pm_max_deviation( freq_frequency( ), mode );

		if ( max_dev > rs->pm.dev )
		{
			print( WARN, "Adjusting PM deviation from %.1f Hz to %.1f Hz.\n",
				   rs->pm.dev, max_dev );
			pm_set_deviation( max_dev );
		}
	}

	rs->pm.mode_has_been_set = true;

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
		print( FATAL, "Requested PM deviation of %f Hz out of range, can't "
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
