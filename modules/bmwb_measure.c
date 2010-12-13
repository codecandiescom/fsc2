/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "bmwb.h"


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


    if ( meilhaus_ai_single( min, max, DETECTOR_CURRENT_AI, val ) )
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


    if ( meilhaus_ai_single( min, max, AFC_SIGNAL_AI, val ) )
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


/*---------------------------------------------------*
 * Returns an array with the measured tune mode data, made
 * to fit the buffer and with values between 0.0 and 1.0
 *---------------------------------------------------*/

size_t
measure_tune_mode( double * data,
                   size_t   size )
{
#if ! defined BMWB_TEST
	return 0;
#else
    size_t i;

    for ( i = 0; i < size; i++ )
        data[ i ] =   0.2 + ( 0.6 * i ) / size
                    + 0.1 * ( rand( ) / ( 1.0 * RAND_MAX ) - 0.5 );

	return 0;
#endif
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
    if ( meilhaus_ai_single( UNLOCKED_MIN, UNLOCKED_MAX,
                             UNLOCKED_SIGNAL_AI, val ) )
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
    if ( meilhaus_ai_single( UNCALIBRATED_MIN, UNCALIBRATED_MAX,
                             UNCALIBRATED_SIGNAL_AI, val ) )
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
