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


#include "hfs9000.h"


static void hfs9000_gpib_failure( void );
static void hfs9000_setup_trig_in( void );


/*------------------------------------------------------------*
 * Intialization of the device at the start of the experiment
 *------------------------------------------------------------*/

bool
hfs9000_init( const char * name )
{
    int i;
    char cmd[ 100 ];
    char reply[ 100 ];
    long len = sizeof reply;


    if ( gpib_init_device( name, &hfs9000.device ) == FAILURE )
        return FAIL;

    hfs9000_command( "HEADER OFF\n" );

    strcpy( cmd, "FPAN:MESS \"***  REMOTE  -  Keyboard disabled !  ***\"\n");
    hfs9000_command( cmd );

    /* Get current run state and stop the pulser */

    hfs9000_command( "TBAS:RUN?\n" );
    if ( gpib_read( hfs9000.device, reply, &len ) == FAILURE )
        hfs9000_gpib_failure( );

    hfs9000.has_been_running = reply[ 0 ] == '1';

    hfs9000_command( "TBAS:RUN OFF\n" );

    /* Set timebase for pulser */

    if ( hfs9000.is_timebase )
    {
        strcpy( cmd, "TBAS:PER " );
        strcat( gcvt( hfs9000.timebase, 9, cmd + strlen( cmd) ), "\n" );
        hfs9000_command( cmd );
    }
    else
    {
        print( FATAL, "Time base of pulser has not been set.\n" );
        THROW( EXCEPTION );
    }

    /* Switch off all channels */

    for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
    {
        if (    hfs9000.channel[ i ].function == NULL
             || ! hfs9000.channel[ i ].function->is_used )
            continue;

        sprintf( cmd, "PGEN%c:CH%c:OUTP OFF\n",
                 CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ) );
        hfs9000_command( cmd );
    }

    /* Set lead delay to zero for all used channels */

    strcpy( cmd, "PGEN*:CH*:LDEL MIN\n" );
    for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
    {
        if (    hfs9000.channel[ i ].function == NULL
             || ! hfs9000.channel[ i ].function->is_used )
            continue;
        cmd[ 4 ] = CHANNEL_LETTER( i );
        cmd[ 8 ] = CHANNEL_NUMBER( i );
        hfs9000_command( cmd );
    }

    /* Set duty cycle to 100% for all used channels */

    strcpy( cmd, "PGEN*:CH*:DCYC 100\n" );
    for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
    {
        if (    hfs9000.channel[ i ].function == NULL
             || ! hfs9000.channel[ i ].function->is_used )
            continue;
        cmd[ 4 ] = CHANNEL_LETTER( i );
        cmd[ 8 ] = CHANNEL_NUMBER( i );
        hfs9000_command( cmd );
    }

    /* Set polarity for all used channels */

    for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
    {
        if (    hfs9000.channel[ i ].function == NULL
             || ! hfs9000.channel[ i ].function->is_used )
            continue;
        sprintf( cmd, "PGEN%c:CH%c:POL %s\n",
                 CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ),
                 hfs9000.channel[ i ].function->is_inverted ?
                 "COMP" : "NORM" );
        hfs9000_command( cmd );
    }

    /* Set raise/fall times for pulses to maximum speed for all channels */

    for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
    {
        if (    hfs9000.channel[ i ].function == NULL
             || ! hfs9000.channel[ i ].function->is_used )
            continue;
        sprintf( cmd, "PGEN%c:CH%c:TRANS MIN\n",
                 CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ) );
        hfs9000_command( cmd );
    }

    /* Set pulse type to RZ (Return to Zero) for all used channels */

    strcpy( cmd, "PGEN*:CH*:TYPE RZ\n");
    for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
    {
        if (    hfs9000.channel[ i ].function == NULL
             || ! hfs9000.channel[ i ].function->is_used )
            continue;
        cmd[ 4 ] = CHANNEL_LETTER( i );
        cmd[ 8 ] = CHANNEL_NUMBER( i );
        hfs9000_command( cmd );
    }

    /* Set channel voltage levels */

    for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
    {
        if (    hfs9000.channel[ i ].function == NULL
             || ! hfs9000.channel[ i ].function->is_used )
            continue;

        if ( hfs9000.channel[ i ].function->is_high_level )
        {
            sprintf( cmd, "PGEN%c:CH%c:HLIM MAX\n",
                     CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ) );
            hfs9000_command( cmd );

            sprintf( cmd, "PGEN%c:CH%c:HIGH ",
                     CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ) );
            strcat( gcvt( hfs9000.channel[ i ].function->high_level,
                          5, cmd + strlen( cmd ) ), "\n" );
            hfs9000_command( cmd );
        }

        if ( hfs9000.channel[ i ].function->is_low_level )
        {
            sprintf( cmd, "PGEN%c:CH%c:LLIM MIN\n",
                     CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ) );
            hfs9000_command( cmd );

            sprintf( cmd, "PGEN%c:CH%c:LOW ",
                     CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ) );
            strcat( gcvt( hfs9000.channel[ i ].function->low_level,
                          5, cmd + strlen( cmd ) ), "\n" );
            hfs9000_command( cmd );
        }
    }

    /* Set channel names */

    for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
    {
        if (    hfs9000.channel[ i ].function == NULL
             || ! hfs9000.channel[ i ].function->is_used )
            continue;

        sprintf( cmd, "PGEN%c:CH%c:SIGN \"%s\"\n",
                 CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ),
                 Function_Names[ hfs9000.channel[ i ].function->self ] );
        hfs9000_command( cmd );
    }

    /* Switch all used channels on, all other off */

    for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
    {
        if (    hfs9000.channel[ i ].function == NULL
             || ! hfs9000.channel[ i ].function->is_used )
            continue;

        sprintf( cmd, "PGEN%c:CH%c:OUTP %s\n",
                 CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ),
                 hfs9000.channel[ i ].function->is_used ? "ON" : "OFF" );
        hfs9000_command( cmd );
    }

    hfs9000_setup_trig_in( );

    /* Per default use the whole pattern length of the pulser, only when the
       user specified a maximum pattern length explicitely use the user
       supplied value. */

    hfs9000_command( "VECT:STAR 0\n" );
    if ( ! hfs9000.is_max_seq_len )
    {
        hfs9000_command( "VECT:END 65535\n" );
        hfs9000_command( "VECT:LOOP 65535\n" );
    }
    else
    {
        sprintf( cmd, "VECT:END %ld\n", hfs9000.max_seq_len - 1 );
        hfs9000_command( cmd );
        sprintf( cmd, "VECT:LOOP %ld\n", hfs9000.max_seq_len - 1 );
        hfs9000_command( cmd );
    }

    return OK;
}


/*-------------------------------------------------------------------*
 * Sets up the way the pulser runs. If the pulser is to run without
 * an external trigger in event it is switched to ABURST mode (i.e.
 * the pulse sequence is repeated automatically with a delay for the
 * re-arm time of ca. 15 us). If the pulser has to react to trigger
 * in events it is run in BURST mode, i.e. the pulse sequence is
 * output after a trigger in event. In this case also the parameter
 * for the trigger in are set. Because there's a delay of ca. 130 ns
 * between the trigger in and the pulser outputting the data this is
 * somewhat reduced by setting the maximum possible negative channel
 * delay for all used channels.
 *-------------------------------------------------------------------*/

static void
hfs9000_setup_trig_in( void )
{
    char cmd[ 100 ];
    int i;


    if ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL )
    {
        hfs9000_command( "TBAS:TIN:INP OFF\n" );
        hfs9000_command( "TBAS:MODE ABUR\n" );
    }
    else
    {
        hfs9000_command( "TBAS:TIN:INP ON\n" );
        hfs9000_command( "TBAS:MODE BURS\n" );

        if ( hfs9000.is_trig_in_slope )
        {
            sprintf( cmd, "TBAS:TIN:SLOP %s\n",
                     hfs9000.trig_in_slope == POSITIVE ? "POS" : "NEG" );
            hfs9000_command( cmd );
        }

        if ( hfs9000.is_trig_in_level )
        {
            strcpy( cmd, "TBAS:TIN:LEV " );
            strcat( gcvt( hfs9000.trig_in_level, 8, cmd + strlen( cmd ) ),
                    "\n" );
            hfs9000_command( cmd );
        }

        /* Set all channel delays to maximum negative value (-60 ns) */

        for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
        {
            if (    hfs9000.channel[ i ].function == NULL
                 || ! hfs9000.channel[ i ].function->is_used )
                continue;

            sprintf( cmd, "PGEN%c:CH%c:CDEL MIN\n",
                     CHANNEL_LETTER( i ), CHANNEL_NUMBER( i ) );
            hfs9000_command( cmd );
        }

        if (    hfs9000.channel[ HFS9000_TRIG_OUT ].function == NULL
             || hfs9000.channel[ HFS9000_TRIG_OUT ].function->is_used )
            hfs9000_command( "TBAS:TOUT:PRET 60E-9\n" );
    }
}


/*-------------------------------------------------------------------------*
 * Sets 'length' slots, starting at 'start', of a channel to either 1 or 0
 *-------------------------------------------------------------------------*/

bool
hfs9000_set_constant( int   channel,
                      Ticks start,
                      Ticks length,
                      int   state )
{
    char cmd[ 100 ];


    sprintf( cmd, "*WAI;:PGEN%c:CH%c:BDATA:FILL %ld,%ld,%s\n",
             CHANNEL_LETTER( channel ), CHANNEL_NUMBER( channel ),
             start, length, state ? "#HFF" : "0" );
    hfs9000_command( cmd );

    return OK;
}


/*-------------------------------------*
 * Sets the trigger out pulse position
 *-------------------------------------*/

bool
hfs9000_set_trig_out_pulse( void )
{
    Function_T *f = hfs9000.channel[ HFS9000_TRIG_OUT ].function;
    Pulse_T *p = f->pulses[ 0 ];
    char cmd[ 100 ];


    sprintf( cmd, "TBAS:TOUT:PER %ld\n", p->pos + f->delay );
    hfs9000_command( cmd );

    return OK;
}


/*-----------------------------------------------------------------*
 * Sets the run mode of the the pulser - either running or stopped
 * after waiting for previous commands to finish (that's what the
 * "*WAI;" bit in the command is about)
 * ->
 *  * state to be set: 1 = START, 0 = STOP
 * <-
 *  * 1: ok, 0: error
 *-----------------------------------------------------------------*/

bool
hfs9000_run( bool flag )
{
    char cmd[ 100 ];


    sprintf( cmd, "*WAI;:TBAS:RUN %s\n", flag ? "ON" : "OFF" );
    hfs9000_command( cmd );
    hfs9000.is_running = flag;
    return OK;
}


/*-----------------------------------------------*
 * Determines if a channel is switched on or off
 *-----------------------------------------------*/

bool
hfs9000_get_channel_state( int channel )
{
    char cmd[ 100 ];
    char reply[ 100 ];
    long len = sizeof reply;


    fsc2_assert ( IS_NORMAL_CHANNEL( channel ) );

    sprintf( cmd, "PGEN%c:CH%c:OUT?\n",
             CHANNEL_LETTER( channel ), CHANNEL_NUMBER( channel ) );
    hfs9000_command( cmd );
    if ( gpib_read( hfs9000.device, reply, &len ) == FAILURE )
        hfs9000_gpib_failure( );
    return reply[ 0 ] == '1';
}


/*------------------------------*
 * Switches a channel on or off
 *------------------------------*/

bool
hfs9000_set_channel_state( int  channel,
                           bool flag )
{
    char cmd[ 100 ];


    fsc2_assert ( IS_NORMAL_CHANNEL( channel ) );

    sprintf( cmd, "*WAI;:PGEN%c:CH%c:OUTP %s\n",
             CHANNEL_LETTER( channel ), CHANNEL_NUMBER( channel ),
             flag ? "ON" : "OFF" );
    hfs9000_command( cmd );

    return OK;
}


/*-------------------------------*
 * Sends a command to the device
 *-------------------------------*/

bool
hfs9000_command( const char * cmd )
{
    if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
        hfs9000_gpib_failure( );
    return OK;
}


/*---------------------------------------------------------------*
 * Function to poll the device until all operations are complete
 *---------------------------------------------------------------*/

bool
hfs9000_operation_complete( void )
{
    char reply[ 10 ];
    long len;


    do
    {
        stop_on_user_request( );
        len = sizeof reply;
        if (    gpib_write( hfs9000.device, "*OPC?\n", 6 ) == FAILURE
             || gpib_read( hfs9000.device, reply, &len ) == FAILURE )
            hfs9000_gpib_failure( );
    } while ( reply[ 0 ] != '1' );

    return OK;
}


/*---------------------------------------------------*
 * Called on failures to communicate with the device
 *---------------------------------------------------*/

static void
hfs9000_gpib_failure( void )
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
