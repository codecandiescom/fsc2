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


#include "rs_smb100a.h"


typedef struct
{
    double low_freq;
    double  pow;
} Pow_Range;

#if defined B101 || defined B102 || defined B103 || defined B106
static double min_pow = -145;
static Pow_Range max_pow[ ] = { {   9.0e3,  8.0 },
                                { 100.0e3, 13.0 },
                                { 300.0e3, 18.0 },
                                {   1.0e6, 30.0 } };
#elif defined B112
static double min_pow = -145;
static Pow_Range max_pow[ ] = { { 100.0e3,  3.0 },
                                { 200.0e3, 11.0 },
                                { 300.0e3, 18.0 },
                                {   1.0e6, 30.0 } };
#else
static double min_pow = -20;
static Pow_Range max_pow[ ] = { { 100.0e3,  5.0 },
                                { 200.0e3, 10.0 },
                                { 300.0e3, 13.0 },
                                {   1.0e6, 30.0 } };
#endif


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
pow_init( void )
{
    /* Set the minimun power and the (frequency-dependent) maximum powees -
       they differ between models */

    rs->pow.pow_resolution = 0.01;

    if ( FSC2_MODE == PREPARATION )
    {
        rs->pow.pow_has_been_set       = false;
        rs->pow.alc_state_has_been_set = false;
        rs->pow.mode_has_been_set      = false;
        rs->pow.off_mode_has_been_set  = false;
        rs->pow.user_max_pow_limit   = max_pow[ 4 ].pow;
        return;
    }

    if ( FSC2_MODE == TEST )
    {
        if ( ! rs->pow.pow_has_been_set )
        {
            rs->pow.pow = -10;
            rs->pow.pow_has_been_set = true;
        }

        if ( ! rs->pow.alc_state_has_been_set )
        {
            rs->pow.alc_state = ALC_STATE_AUTO;
            rs->pow.alc_state_has_been_set = true;
        }

        if ( ! rs->pow.mode_has_been_set )
        {
            rs->pow.mode = POWER_MODE_NORMAL;
            rs->pow.mode_has_been_set = true;
        }

        if ( ! rs->pow.off_mode_has_been_set )
        {
            rs->pow.off_mode  = OFF_MODE_FATT;
            rs->pow.off_mode_has_been_set = true;
        }

        return;
    }       

    // Switch off power offset and set RF-off mode to unchanged

    rs_write( "POW:OFFS 0" );

    if ( ! ( rs->pow.pow_has_been_set ^= 1 ) )
        pow_set_power( rs->pow.pow );
    else
       rs->pow.pow = query_double( "POW?" );

    if ( ! ( rs->pow.alc_state_has_been_set ^= 1 ) )
        pow_set_alc_state( rs->pow.alc_state );
    else
        rs->pow.alc_state = query_alc_state( "POW:ALC?" );
 
    if ( ! ( rs->pow.mode_has_been_set ^= 1 ) )
        pow_set_mode( rs->pow.mode );
    else
        rs->pow.mode = query_pow_mode( "POW:LMODE?" );

    if ( ! ( rs->pow.off_mode_has_been_set ^= 1 ) )
        pow_set_off_mode( rs->pow.off_mode );
    else
        rs->pow.off_mode = query_off_mode( "POW:ATT:RFOF:MODE?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum ALC_State
pow_alc_state( void )
{
    if ( ! rs->pow.alc_state_has_been_set )
    {
        print( FATAL, "ALC state hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

    return rs->pow.alc_state;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/


enum ALC_State
pow_set_alc_state( enum ALC_State state )
{
    if ( rs->pow.alc_state_has_been_set && state == rs->pow.alc_state )
        return rs->pow.alc_state;

    if (    state != ALC_STATE_ON
         && state != ALC_STATE_OFF
         && state != ALC_STATE_AUTO )
    {
        print( FATAL, "Invalid ALC state %d requested, use either \"ON\", "
               "\"OFF\" (sample&hold) or \"AUTO\".\n", state );
        THROW( EXCEPTION );
    }

    rs->pow.alc_state_has_been_set = true;

    if ( FSC2_MODE != EXPERIMENT )
        return rs->pow.alc_state = state;

    char cmd[ 14 ] = "POW:STAT ";
    if ( state == ALC_STATE_OFF )
        strcat( cmd, "OFF" );
    else if ( state == ALC_STATE_ON )
        strcat( cmd, "ON" );
    else
        strcat( cmd, "AUTO" );
    rs_write( cmd );

    return rs->pow.alc_state = state;
}


/*----------------------------------------------------*
 * Returns the power
 *----------------------------------------------------*/

double
pow_power( void )
{
    if ( ! rs->pow.pow_has_been_set )
    {
        print( FATAL, "Attenuation hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

    return rs->pow.pow;
}


/*----------------------------------------------------*
 * Sets a new power
 *----------------------------------------------------*/

double
pow_set_power( double pow )
{
    pow = pow_check_power( pow, freq_frequency( ) );

    if ( rs->pow.pow_has_been_set && pow == rs->pow.pow )
        return rs->pow.pow;

    rs->pow.pow_has_been_set = true;

    if ( FSC2_MODE != EXPERIMENT )
        return rs->pow.pow = pow;

    char cmd[ 100 ];
    sprintf( cmd, "POW %.2f", pow );
    rs_write( cmd );

    return rs->pow.pow = pow;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
pow_min_power( void )
{
    return min_pow;
}


/*----------------------------------------------------*
 * Returns the maximum power that can be set for
 * a frequency (or for the currently set frequency
 * if the frequency is negative).
 *----------------------------------------------------*/

double
pow_max_power( double freq )
{
    if ( freq < 0 )
        freq = freq_frequency( );

    freq = freq_check_frequency( freq );

    for ( int i = 3; i >= 0; i-- )
        if ( freq >= max_pow[ i ].low_freq )
            return max_pow[ i ].pow;

    fsc2_impossible( );
    return 0;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Power_Mode
pow_mode( void )
{
    if ( ! rs->pow.mode_has_been_set )
    {
        print( FATAL, "RF mode hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

    return rs->pow.mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Power_Mode
pow_set_mode( enum Power_Mode mode )
{
    if ( rs->pow.mode_has_been_set && mode == rs->pow.mode )
        return rs->pow.mode;

    if (    mode != POWER_MODE_NORMAL
         && mode != POWER_MODE_LOW_NOISE
         && mode != POWER_MODE_LOW_DISTORTION )
    {
        print( FATAL, "Invalid RF mode %d, use either \"NORMAL\", "
               "\"LOW_NOISE\" or \"LOW_DISTORTION\".\n", mode );
        THROW( EXCEPTION );
    }

    rs->pow.mode_has_been_set = true;

    if ( FSC2_MODE != EXPERIMENT )
        return rs->pow.mode = mode;

    char cmd[ 15  ] = "POW:LMODE ";
    switch ( mode )
    {
        case POWER_MODE_NORMAL :
            strcat( cmd, "NORM" );
            break;

        case POWER_MODE_LOW_NOISE :
            strcat( cmd, "LOWN" );
            break;

        case POWER_MODE_LOW_DISTORTION :
            strcat( cmd, "LOWD" );
            break;
    }
    rs_write( cmd );

    return rs->pow.mode = mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Off_Mode
pow_off_mode( void )
{
    if ( ! rs->pow.off_mode_has_been_set )
    {
        print( FATAL, "RF OFF mode hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

    return rs->pow.off_mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Off_Mode
pow_set_off_mode( enum Off_Mode mode )
{
    if ( rs->pow.off_mode_has_been_set && mode == rs->pow.off_mode )
        return rs->pow.off_mode;

    if ( mode != OFF_MODE_FATT && mode != OFF_MODE_UNCHANGED )
    {
        print( FATAL, "Invalid RF OFF mode %d, use either \"FULL_ATTENUATION\" "
               "or \"UNCHANGED\".\n", mode );
        THROW( EXCEPTION );
    }

    rs->pow.off_mode_has_been_set = true;

    if ( FSC2_MODE != EXPERIMENT )
        return rs->pow.off_mode = mode;

    char cmd[ 23 ] = "POW:ATT:RFOF:MODE ";
    strcat( cmd, mode == OFF_MODE_FATT ? "FATT" : "UNCH" );

    rs_write( cmd );
    return rs->pow.off_mode = mode;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
pow_maximum_power( void )
{
    return d_min( pow_max_power( freq_frequency( ) ),
                  rs->pow.user_max_pow_limit );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
pow_set_maximum_power( double user_max_pow )
{
    if ( user_max_pow < pow_min_power( ) )
    {
        print( FATAL, "Invalid minimum attenuation of %f dB, can't be "
               "set lower than %f dB.\n", - min_pow, - pow_min_power( ) );
        THROW( EXCEPTION );
    }

    return rs->pow.user_max_pow_limit = user_max_pow;
}


/*----------------------------------------------------*
 * Checks if the given power can be set for a certain
 * frequency (for negative frequencies the currently
 * set frequency is used) and returns it rounded to
 * the resolution the power can be set to.
 *----------------------------------------------------*/

double
pow_check_power( double pow,
                 double freq )
{
    double pmax = d_min( pow_max_power( freq ), rs->pow.user_max_pow_limit );
    double pmin = pow_min_power( );

    if (    pow >= pmax + 0.5 * rs->pow.pow_resolution
         || pow <  pmin - 0.5 * rs->pow.pow_resolution )
    {
        print( FATAL, "Requested attenuation of %.2f dB out of range, "
               "must be between %f and %f dB.\n", - pow, - pmin, - pmax);
        THROW( EXCEPTION );
    }

    return rs->pow.pow_resolution * lrnd( pow / rs->pow.pow_resolution );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
