/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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
static void tegam2714a_p_gpib_failure( void );


/*------------------------------*
 * Initialization of the device
 *------------------------------*/

bool tegam2714a_p_init( const char * name )
{
    char reply[ 100 ];
    long length = 100;
    char cmd[ 100 ];
    double ampl;
    double offset;


    if ( gpib_init_device( name, &tegam2714a_p.device ) == FAILURE )
        return FAIL;

	/* Switch off headers */

    if ( gpib_write( tegam2714a_p.device, "HDRS OFF\n", 9  ) == FAILURE )
        tegam2714a_p_gpib_failure( );	

	/* Check if we can read from the device by asking for the status byte */

    if ( gpib_write( tegam2714a_p.device, "*STB?\n", 6 ) == FAILURE ||
         gpib_read( tegam2714a_p.device, reply, &length ) == FAILURE )
        tegam2714a_p_gpib_failure( );

    /* Switch off output */

	tegam2714a_p_run( STOP );

    /* Set  the timebase */

    tegam2714a_p_set_timebase( tegam2714a_p.timebase );

    /* Check the voltage settings (the user may have requested only an
       low or high voltage */

    ampl = tegam2714a_p_get_amplitude( );
    offset = tegam2714a_p_get_offset( );

    if ( ampl   !=    tegam2714a_p.function.high_level
                    - tegam2714a_p.function.low_level ||
         offset != ( ! tegam2714a_p.function.is_inverted ?
                     tegam2714a_p.function.low_level :
                     tegam2714a_p.function.high_level ) )
        tegam2714a_p_set_levels( ampl, offset );

    /* Set trigger mode to TRIGGER, i.e. a waveform is output once after
       an external trigger */

    if ( gpib_write( tegam2714a_p.device, ":MODE TRIG\n", 11 ) == FAILURE )
        tegam2714a_p_gpib_failure( );

	/* Use the internal clock as reference */

    if ( gpib_write( tegam2714a_p.device, ":CLKSEL INT\n", 12 ) == FAILURE )
        tegam2714a_p_gpib_failure( );

	/* Switch off the filter */

    if ( gpib_write( tegam2714a_p.device, ":FILTER OFF\n", 12 ) == FAILURE )
        tegam2714a_p_gpib_failure( );

	/* That out of the way set the size to what we need */

    sprintf( cmd, ":WVFM:WAVE %d;SIZE %ld\n", tegam2714a_p.function.channel, 
             tegam2714a_p.max_seq_len );
    if ( gpib_write( tegam2714a_p.device, cmd, strlen( cmd ) ) == FAILURE )
        tegam2714a_p_gpib_failure( );

	/* Set up the waveform */

    tegam2714a_p_set_pulses( );

	/* Finally set device to use the newly created waveform */

    sprintf( cmd, ":FUNC WAVE %d\n", tegam2714a_p.function.channel );
    if ( gpib_write( tegam2714a_p.device, cmd, strlen( cmd ) ) == FAILURE )
        tegam2714a_p_gpib_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void tegam2714a_p_run( bool state )
{
	char cmd[ 50 ] = ":OUTSW ";


	strcat( cmd, state ? "ON\n" : "MUTE\n" );
    if ( gpib_write( tegam2714a_p.device, cmd, strlen( cmd ) ) == FAILURE )
        tegam2714a_p_gpib_failure( );

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
    if ( gpib_write( tegam2714a_p.device, cmd, strlen( cmd ) ) == FAILURE )
        tegam2714a_p_gpib_failure( );
}


/*-----------------------------------------------------------------*
 * Function for query of the timebase (via sample clock frequency)
 *-----------------------------------------------------------------*/

#if 0
static double tegam2714a_p_get_timebase( void )
{
    char reply[ 100 ];
    long length = 100;


    if ( gpib_write( tegam2714a_p.device, "SCLK?\n", 6 ) == FAILURE ||
         gpib_read( tegam2714a_p.device, reply, &length ) == FAILURE )
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
    if ( gpib_write( tegam2714a_p.device, cmd, strlen( cmd ) ) == FAILURE )
        tegam2714a_p_gpib_failure( );
}


/*--------------------------------------------*
 * Function for query of the output amplitude
 *--------------------------------------------*/

static double tegam2714a_p_get_amplitude( void )
{
    char reply[ 100 ];
    long length = 100;


    if ( gpib_write( tegam2714a_p.device, "AMPL?\n", 6 ) == FAILURE ||
         gpib_read( tegam2714a_p.device, reply, &length ) == FAILURE )
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
    if ( gpib_write( tegam2714a_p.device, cmd, strlen( cmd ) ) == FAILURE )
        tegam2714a_p_gpib_failure( );
}



/*-----------------------------------------*
 * Function for query of the output offset
 *-----------------------------------------*/

static double tegam2714a_p_get_offset( void )
{
    char reply[ 100 ];
    long length = 100;


    if ( gpib_write( tegam2714a_p.device, "OFST?\n", 6 ) == FAILURE ||
         gpib_read( tegam2714a_p.device, reply, &length ) == FAILURE )
        tegam2714a_p_gpib_failure( );

    reply[ length - 1 ] = '\0';

    return T_atod( reply );
}


/*----------------------------------------------*
 *----------------------------------------------*/

static void tegam2714a_p_set_levels( double old_ampl,
                                     double old_offset )
{
    double ampl   =   tegam2714a_p.function.high_level
                    - tegam2714a_p.function.low_level;
    double offset = ! tegam2714a_p.function.is_inverted ?
                    tegam2714a_p.function.low_level :
                    tegam2714a_p.function.high_level;

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

    buf =  CHAR_P T_malloc( len );
    bpt = buf + sprintf( buf, ":WVFM:WAVE %d;MEM %ld,#%1d%ld",
                         tegam2714a_p.function.channel,
                         start, tegam2714a_p_num_len( 2 * length ),
                         2 * length );

    if ( state == 0 )
    {
        for ( k = 0; k < length; k++ )
        {
            *bpt++ = 0x8f;
            *bpt++ = 0xff;
        }
        *bpt = '\n';
    }
    else
    {
        if ( ! tegam2714a_p.function.is_inverted )
        {
            for ( k = 0; k < length; k++ )
            {
                *bpt++ = 0x0f;
                *bpt++ = 0xff;
            }
            *bpt = '\n';
        }
        else
        {
            memset( bpt, 0, 2 * length );
            *( bpt + 2 * length ) = '\n';
        }
    }

    if ( gpib_write( tegam2714a_p.device, buf, len ) == FAILURE )
    {
        T_free( buf );
        tegam2714a_p_gpib_failure( );
    }

    T_free( buf );
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
