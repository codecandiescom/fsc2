/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#include "bmwb.h"

static size_t
find_trigger( double * data,
              double * end_p,
              double   level );


/*------------------------------------------------------------------*
 * Returns the detector current signal as a value between 0 and 1.0
 *------------------------------------------------------------------*/

int
measure_dc_signal( double * val )
{
#if ! defined BMWB_TEST
    double min = bmwb.type == X_BAND ?
                 DC_SIGNAL_MIN_X_BAND : DC_SIGNAL_MIN_Q_BAND;
    double max = bmwb.type == X_BAND ?
                 DC_SIGNAL_MAX_X_BAND : DC_SIGNAL_MAX_Q_BAND;


    if ( meilhaus_ai_single( DETECTOR_CURRENT_AI, min, max, val ) )
        return 1;

    if ( *val < min )
    {
        fprintf( stderr, "Detector current signal is lower than expected "
                 "minimum (%.3f instead of %.3f V)\n", *val, min );
        *val = min;
    }
    else if ( *val > max )
    {
        fprintf( stderr, "Detector current signal is higher than expected "
                 "maximum (%.3f instead of %.3f V)\n", *val, max );
        *val = max;
    }

    *val = ( *val - min ) / ( max - min );
#else
    static double tval = 0;

    tval += 0.02 * ( rand( ) / ( 1.0 * RAND_MAX ) - 0.5 );
    if ( tval < 0.0 )
        tval = 0.0;
    if ( tval > 1.0 )
        tval = 1.0;

	*val = tval;
#endif

    return 0;
}


/*-----------------------------------------------------------------*
 * Returns the AFC signal strength as a value between -1.0 and 1.0
 *-----------------------------------------------------------------*/

int
measure_afc_signal( double * val )
{
#if ! defined BMWB_TEST
    double min = bmwb.type == X_BAND ?
                 AFC_SIGNAL_MIN_X_BAND : AFC_SIGNAL_MIN_Q_BAND;
    double max = bmwb.type == X_BAND ?
                 AFC_SIGNAL_MAX_X_BAND : AFC_SIGNAL_MAX_Q_BAND;


    if ( meilhaus_ai_single( AFC_SIGNAL_AI, min, max, val ) )
        return 1;

    if ( *val < min )
    {
        fprintf( stderr, "AFC signal is lower than expected minimum "
                 "(%.3f instead of %.3f V)\n", *val, min );
        *val = min;
    }
    else if ( *val > max )
    {
        fprintf( stderr, "AFC signal is higher than expected maximum "
                 "(%.3f instead of %.3f V)\n", *val, max );
        *val = max;
    }

    *val = ( *val - min ) / ( max - min );
#else
	static double tval = 0.0;

    tval += 0.02 * ( ( rand( ) / ( 1.0 * RAND_MAX ) ) - 0.5 );
    if ( tval < -1.0 )
        tval = -1.0;
    else if ( tval > 1.0 )
        tval = 1.0;

	*val = tval;
#endif

    return 0;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

#if defined BMWB_TEST
static void
generate_dummy_data( double * x,
                     double * y,
                     size_t   len,
                     size_t   size )
{
    size_t i = 0;
    int r = ( size - 1 ) * random( ) / ( double ) RAND_MAX;
    size_t p = size * ( 1 - 0.1 * random( ) / ( double ) RAND_MAX );
    double min_x_exp = 0.9 * ( bmwb.type == X_BAND ?
                           X_BAND_MIN_TUNE_X_VOLTS : Q_BAND_MIN_TUNE_X_VOLTS ),
           max_x_exp = 0.9 * ( bmwb.type == X_BAND ?
                           X_BAND_MAX_TUNE_X_VOLTS : Q_BAND_MAX_TUNE_X_VOLTS );
    double x_diff = ( max_x_exp - min_x_exp ) / p;
    double min_y_exp = bmwb.type == X_BAND ?
                       X_BAND_MIN_TUNE_Y_VOLTS : Q_BAND_MIN_TUNE_Y_VOLTS,
           max_y_exp = bmwb.type == X_BAND ?
                       X_BAND_MAX_TUNE_Y_VOLTS : Q_BAND_MAX_TUNE_Y_VOLTS;
    double inc = 3.1415 / p;
    double pos = 0.0;
    double f = ( 60.0 - bmwb.attenuation ) * ( max_y_exp - min_y_exp ) / 60.0;
    

    for ( i = 0; i < len; pos += inc, i++ )
    {
        x[ i ] =   x_diff * (   ( i + r ) % p
                              + 3 * ( 1.0 - random( ) / ( double ) RAND_MAX ) )
                 + min_x_exp;
        if ( ( i + r ) % p == 0 )
            pos = 0.0;
        y[ i ] =   f * ( sin( pos ) - 0.1 * random( ) / ( double ) RAND_MAX )
                 + min_y_exp;
    }
}
#endif


/*---------------------------------------------------------*
 * Returns an array with the measured tune mode data, made
 * to fit the buffer and with values between 0.0 and 1.0
 *---------------------------------------------------------*/

#define EXTRA_FREQ_FACTOR   1.2
#define AVERAGES            10
#define MAX_BAD             20

int
measure_tune_mode( double * data,
                   size_t   size )
{
    size_t i;
    double *x, *y;
    size_t data_len;
    double x_min = HUGE_VAL,
           x_max = -HUGE_VAL;
    double min_x_exp = bmwb.type == X_BAND ?
                       X_BAND_MIN_TUNE_X_VOLTS : Q_BAND_MIN_TUNE_X_VOLTS,
           max_x_exp = bmwb.type == X_BAND ?
                       X_BAND_MAX_TUNE_X_VOLTS : Q_BAND_MAX_TUNE_X_VOLTS;
    double start_trigger_level,
           end_trigger_level;
    size_t start[ AVERAGES + MAX_BAD ],
           end[ AVERAGES + MAX_BAD ];
    double min_width = HUGE_VAL;
    size_t bad_count = 0;
    double min_y_exp = bmwb.type == X_BAND ?
                       X_BAND_MIN_TUNE_Y_VOLTS : Q_BAND_MIN_TUNE_Y_VOLTS,
           max_y_exp = bmwb.type == X_BAND ?
                       X_BAND_MAX_TUNE_Y_VOLTS : Q_BAND_MAX_TUNE_Y_VOLTS;


    /* Get enough memory for two curves - we want to do a number of averages
       plus a few extra scans for missed triggers and we also can't count on
       the measurement starting at the lowest point of the sawtooth, so
       acquiring quite a bit more data keeps us on the safe side. */

    data_len = EXTRA_FREQ_FACTOR * ( AVERAGES + 1 + MAX_BAD ) * size;

    if ( ! ( x = malloc( 2 * data_len * sizeof *x ) ) )
        return 1;
    y = x + data_len;

    /* Get the two curves from the Meilhaus card - we need a bit higher
       frequency since we're really only interested in the part were the
       sawtooth wave is going up and in that range we need about 'size'
       samples */

#if ! defined BMWB_TEST
    if ( meilhaus_ai_get_curves( TUNE_MODE_X_SIGNAL_AI, x, min_x_exp, max_x_exp,
                                 TUNE_MODE_Y_SIGNAL_AI, y, min_y_exp, max_y_exp,
                                 data_len,
                                   EXTRA_FREQ_FACTOR * size
                                 * ( bmwb.type == X_BAND ?
                                     X_BAND_TUNE_FREQ : Q_BAND_TUNE_FREQ ) ) )
    {
        free( x );
        return 1;
    }
#else
    generate_dummy_data( x, y, data_len, size );
#endif

    /* Find the maximum and minimum value in the x-values and check that
       they are within the range we expect */

    for ( i = 0; i < data_len; i++ )
    {
        x_min = d_min( x[ i ], x_min );
        x_max = d_max( x[ i ], x_max );
    }

    if ( x_min < min_x_exp || x_max > max_x_exp )
    {
        free( x );
        return 1;
    }

    /* Start and end trigger are 5% above the minimum and 5% below the
       maximum value, if we go nearer to the extrema we may loose too
       many triggers, if we go further way we drop too many good data */

    start_trigger_level = x_min + 0.05 * ( x_max - x_min );
    end_trigger_level   = x_max - 0.05 * ( x_max - x_min );

    /* Find the positions of the start and end trigger level in the
       data set, bail out if not enough can be found. Don't try to find
       as many triggers as there in theory could be, some may be missed
       due to noise, */

    for ( i = 0; i < AVERAGES + MAX_BAD / 2; i++ )
    {
        if (    ( start[ i ] = find_trigger( x + ( i == 0 ? 0 : end[ i - 1 ] ),
                                             x + data_len,
                                             start_trigger_level ) ) )
        {
            start[ i ] += i == 0 ? 0 : end[ i - 1 ];

            if ( ( end[ i ]   = find_trigger( x + start[ i ], x + data_len,
                                              end_trigger_level ) ) )
            {
                end[ i ] += start[ i ];
                min_width = d_min( end[ i ] - start[ i ], min_width );
                continue;
            }
        }

        free( x );
        return 1;
    }

    /* Some intervals may be too long due to a missing end trigger, remove
       them, bail out if we end up with not enough data ranges */

    i = 0;
    while ( i < AVERAGES )
    {
        if ( end[ i ] - start[ i ] - min_width > 0.05 * min_width )
        {
            if ( ++bad_count < MAX_BAD / 2 )
            {
                memmove( start + i, start + i + 1,
                         ( AVERAGES + MAX_BAD / 2 - i - 1 ) * sizeof *start );
                memmove( end + i, end + i + 1,
                         ( AVERAGES + MAX_BAD / 2 - i - 1 ) * sizeof *end );
                continue;
            }
            else
            {
                free( x );
                return 1;
            }
        }

        i++;
    }

    /* If we manage to get here we've got at least AVERAGE valid ranges,
       clear the array of data to be returned */

    for ( i = 0; i < size; i++ )
        data[ i ] = 0.0;

    /* The caller needs exactly 'size' points but the ranges may be shorter
       or longer, so interpolate while averaging over the "good" ranges */

    for ( i = 0; i < AVERAGES; i++ )
    {
        size_t j;
        double inc = ( end[ i ] - start[ i ] + 1.0 ) / ( size - 1.0 );
        double pos = start[ i ] + inc;

        data[ 0 ]        += y[ start[ i ] ];
        data[ size - 1 ] += y[ end[ i ] ];

        for ( j = 1; j < size - 1; pos += inc, j++ )
        {
            size_t p0 = pos,
                   p1 = pos + 1;
            data[ j ] +=   ( 1.0 - pos + p0 ) * y[ p0 ]
                         + ( 1.0 - p1 + pos ) * y[ p1 ];
        }
    }

    free( x );

    /* Finally rescale the data to the interval [0,1] */

    for ( i = 0; i < size; i++ )
        data[ i ] =   ( data[ i ] / AVERAGES - min_y_exp )
                    / ( max_y_exp - min_y_exp );

	return 0;
}


/*-------------------------------------------------------------------------*
 * Function for finding the position of a trigger level (at raising slope)
 * in a rather well-behaved data set (i.e. not too much noise). Start
 * looking at 'data' but not further than 'end_p'. Return 0 if no fitting
 * position could be found, otherwise the index of the point at which the
 * trigger level was reached.
 *-------------------------------------------------------------------------*/

static size_t
find_trigger( double * data,
              double * end_p,
              double   level )
{
    double *start_p = data++;
    double s1, s2;
    double a, b;


    /* Only start looking at 3rd point, we will rarely miss a trigger
       because of that and we need the points at the fringes when doing
       linear interpolation */

    while ( ++data < end_p - 2 )
    {
        /* Do least square fit through 5 points to avoid getting thrown
           off by noise */

        s1 = data[ -1 ] + 2 * data[ 0 ] + 3 * data[ 1 ] + 4 * data[ 2 ];
        s2 = data[ - 2 ] + data[ -1 ] + data[ 0 ] + data[ 1 ] + data[ 2 ];
 
        a = 0.02 * ( 5 * s1 - 10.0 * s2 );

        /* If slope is positive check if the level is between the fit for
           the 2nd and 4rd point, if that's the case we accept that as the
           trigger position */

        if ( a > 0.0 )
        {
            b = 0.2 * s2 - 2 * a;

            if ( a + b < level && 3 * a + b > level )
                return data - start_p;
        }
    }

    return 0;
}


/*----------------------------------------------------*
 * Returns a value between 0 or 1.0, indicating the
 * "unlocked-ness" of the bridge (X-band bridge only)
 *----------------------------------------------------*/

int
measure_unlocked_signal( double * val )
{
    if ( bmwb.type != X_BAND )
    {
        sprintf( bmwb.error_msg, "Cant determine unlocked signal for "
                 "Q-band bridge.\n" );
        return 1;
    }

#if ! defined BMWB_TEST
    if ( meilhaus_ai_single( UNLOCKED_SIGNAL_AI, UNLOCKED_MIN, UNLOCKED_MAX,
                             val ) )
        return 1;

    if ( *val < UNLOCKED_MIN )
    {
        fprintf( stderr, "Unlocked signal is lower than expected minimum "
                 "(%.3f instead of %.3f V)\n", *val, UNLOCKED_MIN );
        *val = UNLOCKED_MIN;
    }
    else if ( *val > UNLOCKED_MAX )
    {
        fprintf( stderr, "Unlocked signal is higher than expected maximum "
                 "(%.3f instead of %.3f V)\n", *val, UNLOCKED_MAX );
        *val = UNLOCKED_MAX;
    }
#else
	*val = 0.0;
#endif

	return ( *val - UNLOCKED_MIN ) / ( UNLOCKED_MAX - UNLOCKED_MIN );

	return 0;
}


/*-------------------------------------------------------*
 * Returns a value between 0 and 1.0, indicating the
 * "uncalibrated-ness" of the bridge (X-band bridge only)
 *--------------------------------------------------------*/

int
measure_uncalibrated_signal( double * val )
{
    if ( bmwb.type != X_BAND )
    {
        sprintf( bmwb.error_msg, "Cant determine uncalibrated signal for "
                 "Q-band bridge.\n" );
        return 1;
    }

#if ! defined BMWB_TEST
    if ( meilhaus_ai_single( UNCALIBRATED_SIGNAL_AI, UNCALIBRATED_MIN,
                             UNCALIBRATED_MAX, val ) )
        return 1;

    if ( *val < UNCALIBRATED_MIN )
    {
        fprintf( stderr, "Uncalibrated signal is lower than expected minimum "
                 "(%.3f instead of %.3f V)\n", *val, UNCALIBRATED_MIN );
        *val = UNCALIBRATED_MIN;
    }
    else if ( *val > UNCALIBRATED_MAX )
    {
        fprintf( stderr, "Uncalibrated signal is higher than expected maximum "
                 "(%.3f instead of %.3f V)\n", *val, UNCALIBRATED_MAX );
        *val = UNCALIBRATED_MAX;
    }
#else
	*val = 0.0;
#endif

	return ( *val - UNCALIBRATED_MIN )
           / ( UNCALIBRATED_MAX - UNCALIBRATED_MIN );
}


/*----------------------------------------------------*
 * Determine if AFC is on or off (Q-band bridge only)
 *----------------------------------------------------*/

int
measure_afc_state( int * state )
{
    unsigned char v = 0;


    if ( bmwb.type == X_BAND )
    {
        sprintf( bmwb.error_msg, "Can't determine AFC state for X-band "
                 "bridge.\n" );
        return 1;
    }

    if ( meilhaus_dio_in( DIO_A, &v ) )
        return 1;

    *state = v & AFC_STATE_BIT ? 1 : 0;

    return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
