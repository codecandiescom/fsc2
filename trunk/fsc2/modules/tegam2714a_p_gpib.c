/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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


#include "tegam2714a_p.h"
#include "gpib_if.h"


static void tegam2714a_p_set_timebase( double timebase );
#if 0
static double tegam2714a_p_get_timebase( void );
#endif
static double tegam2714a_p_get_amplitude( void );
static void tegam2714a_p_set_offset( double offset );
static double tegam2714a_p_get_offset( void );
static void tegam2714a_p_set_levels( double old_ampl,
                                     double old_offset );
static void tegam2714a_p_write( const char * buf,
                                long         length );
static void tegam2714a_p_gpib_failure( void );


/*------------------------------*
 * Initialization of the device
 *------------------------------*/

void tegam2714a_p_init( const char * name )
{
    char reply[ 100 ];
    long length = 100;
    char cmd[ 100 ];
    char *ptr;
    size_t i;
    double ampl;
    double offset;


    if ( gpib_init_device( name, &tegam2714a_p.device ) == FAILURE )
    {
        print( FATAL, "Initialization of device failed: %s\n",
               gpib_error_msg );
        THROW( EXCEPTION );
    }

	/* Switch off headers */

    tegam2714a_p_write( ":CONF:HDRS OFF\n", 15 );

	/* Check if we can read from the device by asking for the status byte */

    tegam2714a_p_write( "*STB?\n", 6 );
    if ( gpib_read( tegam2714a_p.device, reply, &length ) == FAILURE )
        tegam2714a_p_gpib_failure( );

    /* Set the timebase */

    tegam2714a_p_set_timebase( tegam2714a_p.timebase );

    /* Check the voltage settings (the user may have requested only an
       low or high voltage */

    ampl = tegam2714a_p_get_amplitude( );
    offset = tegam2714a_p_get_offset( );

    if ( ampl   != 2.0 * (   tegam2714a_p.function.high_level
                           - tegam2714a_p.function.low_level ) ||
         offset != tegam2714a_p.function.low_level )
        tegam2714a_p_set_levels( ampl, offset );

	/* Use the internal clock as reference */

    tegam2714a_p_write( ":CLKSEL INT\n", 12 );

	/* Switch off the filter */

    tegam2714a_p_write( ":FILTER OFF\n", 12 );

    /* Create the empty waveform that we use to "switch off" the device */

    sprintf( cmd, ":WVFM:WAVE %d;SIZE %ld\n", EMPTY_WAVEFORM_NUMBER,
             MIN_PULSER_BITS );
    tegam2714a_p_write( cmd, strlen( cmd ) );

    ptr = cmd + sprintf( cmd, ":WVFM:WAVE %d;MEM 0,#264",
                         EMPTY_WAVEFORM_NUMBER );
    if ( tegam2714a_p.function.is_inverted )
        for ( i = 0; i < MIN_PULSER_BITS; i++ )
        {
            *ptr++ = 0x0f;
            *ptr++ = 0xff;
        }
    else
        for ( i = 0; i < MIN_PULSER_BITS; i++ )
        {
            *ptr++ = 0x08;
            *ptr++ = 0x00;
        }
    *ptr++ = '\n';

    tegam2714a_p_write( cmd, ptr - cmd );

    sprintf( cmd, ":FUNC WAVE,%d\n", EMPTY_WAVEFORM_NUMBER );
    tegam2714a_p_write( cmd, strlen( cmd ) );

    /* Set trigger mode to TRIGGER, i.e. a waveform is output once after
       receipt of a trigger */

    tegam2714a_p_write( ":MODE TRIG\n", 11 );

    /* In order to make sure the device is "off" send it a trigger command */

    tegam2714a_p_write( "*TRG\n", 5 );
    tegam2714a_p.is_running = UNSET;

	/* That out of the way set the size for the "real" waveform */

    sprintf( cmd, ":WVFM:WAVE %d;SIZE %ld\n", tegam2714a_p.function.channel,
             tegam2714a_p.max_seq_len );
    tegam2714a_p_write( cmd, strlen( cmd ) );

    /* Finally set up all pulses */

    tegam2714a_p_pulse_setup( );

    tegam2714a_p_write( ":FUNC WAVE,98\n", 14 );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void tegam2714a_p_run( bool state )
{
	char cmd[ 50 ];


    sprintf( cmd, ":FUNC WAVE,%d\n",
             state ? tegam2714a_p.function.channel : EMPTY_WAVEFORM_NUMBER );
    tegam2714a_p_write( cmd, strlen( cmd ) );

    /* If the device is to be switched off send it a trigger to make sure
       an empty waveform currently still in the process of being output gets
       aborted. */

    if ( ! state )
        tegam2714a_p_write( "*TRG\n", 5 );

    tegam2714a_p.is_running = state;
}


/*----------------------------------------------------------------*
 * Function for setting the timebase (via sample clock frequency)
 *----------------------------------------------------------------*/

static void tegam2714a_p_set_timebase( double timebase )
{
    char cmd[ 100 ];


    fsc2_assert( timebase >= MIN_TIMEBASE && timebase <= MAX_TIMEBASE );

    sprintf( cmd, ":SCLK %g\n", 1.0 / timebase );
    tegam2714a_p_write( cmd, strlen( cmd ) );
}


/*-----------------------------------------------------------------*
 * Function for query of the timebase (via sample clock frequency)
 *-----------------------------------------------------------------*/

#if 0
static double tegam2714a_p_get_timebase( void )
{
    char reply[ 100 ];
    long length = 100;


    tegam2714a_p_write( "SCLK?\n", 6 );
    if ( gpib_read( tegam2714a_p.device, reply, &length ) == FAILURE )
        tegam2714a_p_gpib_failure( );

    reply[ length - 1 ] = '\0';

    return 1.0 / T_atod( reply );
}
#endif


/*-------------------------------------------*
 * Function for setting the output amplitude
 *-------------------------------------------*/

void tegam2714a_p_set_amplitude( double ampl )
{
    char cmd[ 100 ];


    fsc2_assert( ampl >= 0.0 && ampl <= MAX_AMPLITUDE );

    sprintf( cmd, ":AMPL %g\n", ampl );
    tegam2714a_p_write( cmd, strlen( cmd ) );

    fprintf( stderr, "AMPL: should %f, got %f\n", ampl,
             tegam2714a_p_get_amplitude( ) );
}


/*--------------------------------------------*
 * Function for query of the output amplitude
 *--------------------------------------------*/

static double tegam2714a_p_get_amplitude( void )
{
    char reply[ 100 ];
    long length = 100;


    tegam2714a_p_write( "AMPL?\n", 6 );
    if ( gpib_read( tegam2714a_p.device, reply, &length ) == FAILURE )
        tegam2714a_p_gpib_failure( );

    reply[ length - 1 ] = '\0';

    return T_atod( reply );
}


/*----------------------------------------*
 * Function for setting the output offset
 *----------------------------------------*/

static void tegam2714a_p_set_offset( double offset )
{
    char cmd[ 100 ];


    fsc2_assert( offset >= - MAX_AMPLITUDE && offset <= MAX_AMPLITUDE );

    sprintf( cmd, ":OFST %g\n", offset );
    tegam2714a_p_write( cmd, strlen( cmd ) );

    fprintf( stderr, "OFFSET: should %f, got %f\n", offset,
             tegam2714a_p_get_offset( ) );
}



/*-----------------------------------------*
 * Function for query of the output offset
 *-----------------------------------------*/

static double tegam2714a_p_get_offset( void )
{
    char reply[ 100 ];
    long length = 100;


    tegam2714a_p_write( "OFST?\n", 6 );
    if ( gpib_read( tegam2714a_p.device, reply, &length ) == FAILURE )
        tegam2714a_p_gpib_failure( );

    reply[ length - 1 ] = '\0';

    return T_atod( reply );
}


/*----------------------------------------------*
 *----------------------------------------------*/

static void tegam2714a_p_set_levels( double old_ampl,
                                     double old_offset )
{
    double ampl   = 2.0 * (   tegam2714a_p.function.high_level
                            - tegam2714a_p.function.low_level );
    double offset = tegam2714a_p.function.low_level;

    if ( old_ampl > 0.999 )
    {
        if ( ampl + fabs( old_offset ) < MAX_AMPLITUDE )
        {
            tegam2714a_p_set_amplitude( ampl );
            tegam2714a_p_set_offset( offset );
        }
        else
        {
            tegam2714a_p_set_offset( offset );
            tegam2714a_p_set_amplitude( ampl );
        }
    }
    else if ( old_ampl > 0.99 )
    {
        if ( ampl + fabs( old_offset ) < 1.0 )
        {
            tegam2714a_p_set_amplitude( ampl );
            tegam2714a_p_set_offset( offset );
        }
        else
        {
            tegam2714a_p_set_offset( offset );
            tegam2714a_p_set_amplitude( ampl );
        }
    }
    else 
    {
        if ( ampl + fabs( old_offset ) < 0.1 )
        {
            tegam2714a_p_set_amplitude( ampl );
            tegam2714a_p_set_offset( offset );
        }
        else
        {
            tegam2714a_p_set_offset( offset );
            tegam2714a_p_set_amplitude( ampl );
        }
    }
}


/*----------------------------------------------*
 * Sets 'length' slots, starting at 'start', to
 * either maximum (4095) or minimum (0) value
 *----------------------------------------------*/

void  tegam2714a_p_set_constant( Ticks start,
                                 Ticks length,
                                 int   state )
{
    char *buf;
    unsigned char *bpt;
    size_t len =   20 + tegam2714a_p_num_len( tegam2714a_p.function.channel )
                 + tegam2714a_p_num_len( start )
                 + tegam2714a_p_num_len( 2 * length ) + 2 * length;
    Ticks k;
    

    fsc2_assert( start >= 0 && start + length < MAX_PULSER_BITS );

    buf = CHAR_P T_malloc( len );
    bpt =   ( unsigned char * ) buf
          + sprintf( buf, ":WVFM:WAVE %d;MEM %ld,#%1d%ld",
                     tegam2714a_p.function.channel,
                     start, tegam2714a_p_num_len( 2 * length ), 2 * length );

    if ( state == 0 )
    {
        if ( tegam2714a_p.function.is_inverted )
            for ( k = 0; k < length; k++ )
            {
                *bpt++ = 0x0f;
                *bpt++ = 0xff;
            }
        else
            for ( k = 0; k < length; k++ )
            {
                *bpt++ = 0x08;
                *bpt++ = 0x00;
            }
    }
    else
    {
        if ( tegam2714a_p.function.is_inverted )
            for ( k = 0; k < length; k++ )
            {
                *bpt++ = 0x08;
                *bpt++ = 0x00;
            }
        else
            for ( k = 0; k < length; k++ )
            {
                *bpt++ = 0x0f;
                *bpt++ = 0xff;
            }
    }

    *bpt = '\n';

    if ( gpib_write( tegam2714a_p.device, buf, len ) == FAILURE )
    {
        T_free( buf );
        tegam2714a_p_gpib_failure( );
    }

    T_free( buf );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static void tegam2714a_p_write( const char * buf,
                                long         length )
{
    if ( gpib_write( tegam2714a_p.device, buf, length ) == FAILURE )
        tegam2714a_p_gpib_failure( );
}


/*---------------------------------------------------------------*
 * Funcion to be called when the pulser does not react properly.
 *---------------------------------------------------------------*/

static void tegam2714a_p_gpib_failure( void )
{
    print( FATAL, "Communication with device failed.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
