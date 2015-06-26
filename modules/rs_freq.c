#include "rs.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
freq_init( void )
{
#if defined B101
	rs->freq.min_freq = 9.0e3;
	rs->freq.max_freq = 1.1e9;
#elif defined B102
\	rs->freq.min_freq = 9.0e3;
	rs->freq.max_freq = 2.2e9;
#elif defined B103 
	rs->freq.min_freq = 9.0e3;
	rs->freq.max_freq = 3.2e9;
#elif defined B106
	rs->freq.min_freq = 9.0e3;
	rs->freq.max_freq = 6.0e9;
#else
	rs->freq.min_freq = 100.0e3;
	rs->freq.max_freq = 12.75e9;
#endif

	rs->freq.freq_resolution = 0.01;

	if ( FSC2_MODE == PREPARATION )
	{
		rs->freq.freq_has_been_set = false;
		return;
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ! rs->freq.freq_has_been_set )
		{
			rs->freq.freq = 14.0e6;
			rs->freq.freq_has_been_set = true;
		}

		return;
	}

    // Set fixed CW frequency mode and switch off any multiplier or offset

    freq_list_mode( false );
    rs_write( "FREQ:MULT 1" );
    rs_write( "FREQ:OFFS 0" );

    // Determine the current RF frequency

	if ( rs->freq.freq_has_been_set )
	{
		rs->freq.freq_has_been_set = false;
		freq_set_frequency( rs->freq.freq );
	}
	else
	{
		rs->freq.freq = query_double( "FREQ?" );
		rs->freq.freq_has_been_set = true;
	}
}


/*----------------------------------------------------*
 * Assembles and sends the command for setting a RF frequency
 *----------------------------------------------------*/

double
freq_frequency( void )
{
    if ( ! rs->freq.freq_has_been_set )
    {
        print( FATAL, "Frequency hasn't been set yet.\n" );
        THROW( EXCEPTION );
    }

	return rs->freq.freq;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
freq_set_frequency( double freq )
{
	freq = freq_check_frequency( freq );

    if ( rs->freq.freq_has_been_set && freq == rs->freq.freq )
		return rs->freq.freq;

    /* If with the frequency to be set the FM or the PM amplitude
       would become too large reduce it to its new maximum value */

    double max_fm_dev = fm_max_deviation( freq, fm_mode( ) );
    if ( fm_deviation( ) > max_fm_dev )
        fm_set_deviation( max_fm_dev );

    double max_pm_dev = pm_max_deviation( freq, pm_mode( ) );
    if ( pm_deviation( ) > max_pm_dev )
        pm_set_deviation( max_pm_dev );

	rs->freq.freq_has_been_set = true;

	if ( FSC2_MODE != EXPERIMENT )
		return rs->freq.freq = freq;

    char cmd[ 100 ];
    sprintf( cmd, "FREQ %.2f", freq );
	rs_write( cmd );

    return rs->freq.freq = freq;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/
double
freq_min_frequency( void )
{
	return rs->freq.min_freq;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
freq_max_frequency( void )
{
	return rs->freq.max_freq;
}


/*----------------------------------------------------*
 * Checks if a frequency is within the range the device
 * can produce and returns the value rounded to the
 * frequency reso;ution of the device.
 *----------------------------------------------------*/

double
freq_check_frequency( double freq )
{
    if (    freq <  rs->freq.min_freq - 0.5 * rs->freq.freq_resolution
         || freq >= rs->freq.max_freq + 0.5 * rs->freq.freq_resolution )
	{
		print( FATAL, "Requested frequency of %.2 Hz out of range, "
			   "must be between %.2f and %.2f Hz.\n", freq,
			   rs->freq.min_freq, rs->freq.max_freq );
		THROW( EXCEPTION );
	}

    return rs->freq.freq_resolution * lrnd( freq / rs->freq.freq_resolution );
}


/*----------------------------------------------------*
 * Switches between list and cw frequency mode (called
 * from the LIST subsystem)
 *----------------------------------------------------*/

bool
freq_list_mode( bool on_off )
{
	if ( FSC2_MODE != EXPERIMENT )
		return on_off;

	char cmd[ 15 ] = "FREQ:MODE ";
	strcat( cmd, on_off ? "LIST" : "CW" );
    rs_write( cmd );
    return on_off;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
