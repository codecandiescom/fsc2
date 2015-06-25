#include "rs.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
lfo_init( void )
{
    rs->lfo.min_freq        = 0.1;
    rs->lfo.max_freq        = 1.0e6;
    rs->lfo.freq_resolution = 0.1;
    rs->lfo.max_volts       = 4;
    rs->lfo.volt_resolution = 0.01;

	if ( FSC2_MODE == PREPARATION )
	{
		rs->lfo.freq_has_been_set = false;
		rs->lfo.volts_has_been_set = false;
		rs->lfo.imp_has_been_set = false;
		return;
	}

	if ( FSC2_MODE != EXPERIMENT )
	{
		if ( ! rs->lfo.freq_has_been_set )
			rs->lfo.freq  = 10.0e3;
		
		if ( ! rs->lfo.volts_has_been_set )
			rs->lfo.volts = 1;

		if ( ! rs->lfo.imp_has_been_set )
			rs->lfo.imp   = IMPEDANCE_LOW;

		return;
	}

    rs_write( "LFO:FREQ:MODE CW" );
    rs_write( "LFO:SHAP SINE" );
    rs_write( "LFO:STATE ON" );

	if ( rs->lfo.freq_has_been_set )
	{
		double freq = rs->lfo.freq;
		rs->lfo.freq = -1;
		lfo_set_frequency( freq );
	}
	else
	{
		rs->lfo.freq  = query_double( "LFO:FREQ?" );

		if (    rs->lfo.freq < rs->lfo.min_freq
			 || rs->lfo.freq > rs->lfo.max_freq )
			bad_data( );
	}

	if ( rs->lfo.volts_has_been_set )
	{
		double volts = rs->lfo.volts;
		rs->lfo.volts = -10000;
		lfo_set_voltage( volts );
	}
	else
	{
		rs->lfo.volts = query_double( "LFO:VOLT?" );

		if ( rs->lfo.volts < 0 || rs->lfo.volts > rs->lfo.max_volts )
			bad_data( );
	}

	if ( rs->lfo.imp_has_been_set )
	{
		enum Impedance imp = rs->lfo.imp;
		rs->lfo.imp = -1;
		lfo_set_impedance( imp );
	}
	else
	{
		rs->lfo.imp = query_imp( "LFO:SIMP?" );

		if ( rs->lfo.imp != IMPEDANCE_LOW && rs->lfo.imp != IMPEDANCE_G600 )
			bad_data( );
	}
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
lfo_frequency( void )
{
    if ( FSC2_MODE == PREPARATION && ! rs->lfo.freq_has_been_set )
    {
        print( FATAL, "Modulation frequency hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

	return rs->lfo.freq;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
lfo_set_frequency( double freq )
{
    if ( freq == rs->lfo.freq )
        return rs->lfo.freq;

    if (    freq <  rs->lfo.min_freq - 0.5 * rs->lfo.freq_resolution
         || freq >= rs->lfo.max_freq + 0.5 * rs->lfo.freq_resolution )
	{
		print( FATAL, "Requested modulation frequency of %.1 Hz out of "
			   "range, must be between %.1f and %.1f Hz.\n",
			   freq, rs->lfo.min_freq, rs->lfo.max_freq );
		THROW( EXCEPTION );
	}

    freq = rs->lfo.freq_resolution * lrnd( freq / rs->lfo.freq_resolution );

	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->lfo.freq_has_been_set = true;
		return rs->lfo.freq = freq;
	}

    char cmd[ 100 ];
    sprintf( cmd, "LFO:FREQ %.1f", freq );

    rs_write( cmd );
    return rs->lfo.freq = freq;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
lfo_voltage( void )
{
    if ( FSC2_MODE == PREPARATION && ! rs->lfo.volts_has_been_set )
    {
        print( FATAL, "Modulation output voltage hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

	return rs->lfo.volts;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
lfo_set_voltage( double volts )
{
    if ( volts == rs->lfo.volts )
        return rs->lfo.volts;

    if (    volts < 0
		 || volts >= rs->lfo.max_volts + 0.5 * rs->lfo.volt_resolution )
	{
		print( FATAL, "Requested modulation voltage of %.1 V out of range, "
			   "must not be negative or larger than %.1 V.\n", volts,
			   rs->lfo.max_volts );
		THROW( EXCEPTION );
	}

    volts = rs->lfo.volt_resolution * lrnd( volts / rs->lfo.volt_resolution );

	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->lfo.volts_has_been_set = true;
		return rs->lfo.volts = volts;
	}

	char cmd[ 100 ];
    sprintf( cmd, "LFO:VOLT %.3f", volts );

    rs_write( cmd );
    return rs->lfo.volts = volts;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
lfo_impedance( void )
{
    if ( FSC2_MODE == PREPARATION && ! rs->lfo.imp_has_been_set )
    {
        print( FATAL, "Modulation output impedance hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

	return rs->lfo.imp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
lfo_set_impedance( enum Impedance imp )
{
    if ( imp == rs->lfo.imp )
        return rs->lfo.imp;

    if ( imp != IMPEDANCE_LOW && imp != IMPEDANCE_G600 )
	{
		print( FATAL, "Requested modulation output impedance not possible, "
			   "use either \"LOW\" or \"G600\".\n" );
		THROW( EXCEPTION );
	}
			   
	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->lfo.imp_has_been_set = true;
		return rs->lfo.imp = imp;
	}

    char cmd[ 14 ] = "LFO:SIMP ";
    strcat( cmd, imp == IMPEDANCE_LOW ? "LOW" : "G600" );
    
    rs_write( cmd );
    return rs->lfo.imp = imp;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
