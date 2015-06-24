#include "rs.h"


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
        rs->pow.pow_has_been_set = false;
        rs->pow.alc_state_has_been_set = false;
        rs->pow.mode_has_been_set = false;
        rs->pow.off_mode_has_been_set = false;
        return;
    }

	if ( FSC2_MODE == TEST )
	{
        if ( ! rs->pow.pow_has_been_set )
            rs->pow.pow       = -10;

        if ( ! rs->pow.alc_state_has_been_set )
            rs->pow.alc_state = ALC_STATE_AUTO;

        if ( ! rs->pow.mode_has_been_set )
            rs->pow.mode      = POWER_MODE_NORMAL;

        if ( ! rs->pow.off_mode_has_been_set )
            rs->pow.off_mode  = OFF_MODE_FATT;

		return;
	}		

    // Switch off power offset and set RF-off mode to unchanged

    rs_write( "POW:OFFS 0" );

    if ( rs->pow.pow_has_been_set )
    {
        double pow = rs->pow.pow;
        rs->pow.pow = -100000;
        pow_set_power( pow );
    }
    else
        rs->pow.pow       = query_double( "POW?" );

    if ( rs->pow.alc_state_has_been_set )
    {
        int alc_state = rs->pow.alc_state;
        rs->pow.alc_state = -1;
        pow_set_alc_state( alc_state );
    }
    else
        rs->pow.alc_state = query_alc_state( "POW:ALC?" );

    if ( rs->pow.mode_has_been_set )
    {
        int mode = rs->pow.mode;
        rs->pow.mode = -1;
        pow_set_mode( mode );
    }
    else
        rs->pow.mode      = query_pow_mode( "POW:LMODE?" );

    if ( rs->pow.off_mode_has_been_set )
    {
        int off_mode = rs->pow.off_mode;
        rs->pow.off_mode = -1;
        pow_set_off_mode( off_mode );
    }
    else
        rs->pow.off_mode  = query_off_mode( "POW:ATT:RFOF:MODE?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum ALC_State
pow_alc_state( void )
{
    if ( FSC2_MODE == PREPARATION && ! rs->pow.alc_state_has_been_set )
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
	if ( state == rs->pow.alc_state )
		return rs->pow.alc_state;

	if (    state != ALC_STATE_ON
		 && state != ALC_STATE_OFF
		 && state != ALC_STATE_AUTO )
	{
		print( FATAL, "Invalid ALC state %d requested, use either \"ON\", "
			   "\"OFF\" (sample&hold) or \"AUTO\".\n", state );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
    {
        rs->pow.alc_state_has_been_set = true;
		return rs->pow.alc_state = state;
    }

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
    if ( FSC2_MODE == PREPARATION && ! rs->pow.pow_has_been_set )
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
    if ( ( pow = pow_check_power( pow, pow_mode( ) ) ) == rs->pow.pow )
        return rs->pow.pow;

	if ( FSC2_MODE != EXPERIMENT )
    {
        rs->pow.pow_has_been_set = true;
		return rs->pow.pow = pow;
    }

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
    if ( FSC2_MODE == PREPARATION && ! rs->pow.mode_has_been_set )
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
    if ( mode == rs->pow.mode )
        return rs->pow.mode;

	if (    mode != POWER_MODE_NORMAL
		 && mode != POWER_MODE_LOW_NOISE
		 && mode != POWER_MODE_LOW_DISTORTION )
	{
		print( FATAL, "Invalid RF mode %d, use either \"NORMAL\", "
               "\"LOW_NOISE\" or \"LOW_DISTORTION\".\n", mode );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
    {
        rs->pow.mode_has_been_set = true;
		return rs->pow.mode = mode;
    }

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
    if ( FSC2_MODE == PREPARATION && ! rs->pow.off_mode_has_been_set )
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
    if ( mode == rs->pow.off_mode )
        return rs->pow.off_mode;

	if ( mode != OFF_MODE_FATT && mode != OFF_MODE_UNCHANGED )
	{
		print( FATAL, "Invalid RF OFF mode %d, use either \"FULL_ATTENUATION\" "
			   "or \"UNCHANGED\".\n", mode );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
    {
        rs->pow.off_mode_has_been_set = true;
		return rs->pow.off_mode = mode;
    }

	char cmd[ 23 ] = "POW:ATT:RFOF:MODE ";
	strcat( cmd, mode == OFF_MODE_FATT ? "FATT" : "UNCH" );

	rs_write( cmd );
	return rs->pow.off_mode = mode;
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
	double pmax = pow_max_power( freq );
	double pmin = pow_min_power( );

    if (    pow >= pmax + 0.5 * rs->pow.pow_resolution
		 || pow <  pmin - 0.5 * rs->pow.pow_resolution )
	{
		print( FATAL, "Requested attenuation of %.2f dBm out of range, "
			   "must be between %f and %f dBm.\n", pow, pmin, pmax);
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
