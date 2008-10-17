/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


#include "lecroy9400.h"


static bool is_acquiring = UNSET;

static bool lecroy9400_talk( const char * cmd,
                             char *       reply,
                             long *       length );
static void lecroy9400_gpib_failure( void );
static void lecroy9400_invalid_reply( void );


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_init( const char * name )
{
    char buffer[ 100 ];
    long len = 100;
    int i;


    is_acquiring = UNSET;

    if ( gpib_init_device( name, &lecroy9400.device ) == FAILURE )
        return FAIL;

    /* Set digitizer to short form of replies, make it only send a line feed
       at the end of replies, switch off debugging, transmit data in one block
       without an END block marker (#I) and set the data format to binary with
       2 byte length. Then ask it for the status byte 1 to test if the device
       reacts. */

    if (    gpib_write( lecroy9400.device,
                      "CHDR,OFF;CPRM,OFF;CTRL,LF;CHLP,OFF;CBLS,-1;CFMT,A,WORD",
                        54 ) == FAILURE
         || gpib_write( lecroy9400.device, "STB,6,?", 7 ) == FAILURE
         || gpib_read( lecroy9400.device, buffer, &len ) == FAILURE )
    {
        gpib_local( lecroy9400.device );
        return FAIL;
    }

    lecroy9400.is_reacting = SET;
    lecroy9400.data_size = 1;

    /* Set or get the time base, if necessary change it to a valid value */

    if ( lecroy9400.is_timebase )
        lecroy9400_set_timebase( lecroy9400.timebase );
    else
    {
        lecroy9400.timebase = lecroy9400_get_timebase( );

        if ( lecroy9400.timebase < tb[ 0 ] )
        {
            lecroy9400_set_timebase( lecroy9400.timebase = tb[ 0 ] );
            print( SEVERE, "Timebase too fast, changing to %s.\n",
                   lecroy9400_ptime( lecroy9400.timebase ) );
        }
        else if ( lecroy9400.timebase > tb[ NUM_ELEMS( tb ) - 1 ] )
        {
            lecroy9400_set_timebase( lecroy9400.timebase
                                     = tb[ NUM_ELEMS( tb ) - 1 ] );
            print( SEVERE, "Timebase too slow, changing to %s.\n",
                   lecroy9400_ptime( lecroy9400.timebase ) );
        }

        lecroy9400.tb_index = lecroy9400_get_tb_index( lecroy9400.timebase );
    }

    /* After we're now reasonable sure that the time constant is 50 ns/div
       or longer we can switch off interleaved mode */

    if ( gpib_write( lecroy9400.device, "IL,OFF", 6 ) == FAILURE )
        return FAIL;

    /* Set or get the normal channels sensitivities and couplings */

    for ( i = LECROY9400_CH1; i <= LECROY9400_CH2; i++ )
    {
        if ( lecroy9400.is_sens[ i ] )
            lecroy9400_set_sens( i, lecroy9400.sens[ i ] );
        else
            lecroy9400.sens[ i ] = lecroy9400_get_sens( i );

        if ( lecroy9400.is_coupl[ i ] )
            lecroy9400_set_coupling( i, lecroy9400.coupl[ i ] );
        else
            lecroy9400.coupl[ i ] = lecroy9400_get_coupling( i );
    }

    /* Get or set the bandwidth limiter */

    if ( lecroy9400.is_bandwidth_limiter )
        lecroy9400_set_bandwidth_limiter( lecroy9400.bandwidth_limiter );
    else
        lecroy9400.bandwidth_limiter = lecroy9400_get_bandwidth_limiter( );

    /* Get or set the trigger mode */

    if ( lecroy9400.is_trigger_mode )
        lecroy9400_set_trigger_mode( lecroy9400.trigger_mode );
    else
        lecroy9400.trigger_mode = lecroy9400_get_trigger_mode( );

    /* Get or set the trigger source channel */

    if ( lecroy9400.is_trigger_channel )
        lecroy9400_set_trigger_source( lecroy9400.trigger_channel );
    else
        lecroy9400.trigger_channel = lecroy9400_get_trigger_source( );

    /* Get or set the trigger source channel */

    if ( lecroy9400.is_trigger_level )
        lecroy9400_set_trigger_level( lecroy9400.trigger_level );
    else
        lecroy9400.trigger_level = lecroy9400_get_trigger_level( );

    /* Get or set the trigger coupling */

    if ( lecroy9400.is_trigger_coupling )
        lecroy9400_set_trigger_coupling( lecroy9400.trigger_coupling );
    else
        lecroy9400.trigger_coupling = lecroy9400_get_trigger_coupling( );

    /* Get or set the trigger slope */

    if ( lecroy9400.is_trigger_slope )
        lecroy9400_set_trigger_slope( lecroy9400.trigger_slope );
    else
        lecroy9400.trigger_slope = lecroy9400_get_trigger_slope( );

    /* Set (if required) the trigger delay */

    if ( lecroy9400.is_trigger_delay )
        lecroy9400_set_trigger_delay( lecroy9400.trigger_delay );
    else
        lecroy9400.trigger_delay = lecroy9400_get_trigger_delay( );

    /* Now get the waveform descriptors for all channels */

    for ( i = LECROY9400_CH1; i <= LECROY9400_FUNC_F; i++ )
        lecroy9400_get_desc( i, SET );

    /* Set up averaging for function channels */

    for ( i = LECROY9400_FUNC_E; i <= LECROY9400_FUNC_F; i++ )
        if ( lecroy9400.is_num_avg[ i ] )
            lecroy9400_set_up_averaging( i, lecroy9400.source_ch[ i ],
                                            lecroy9400.num_avg[ i ],
                                            lecroy9400.is_reject[ i ],
                                            lecroy9400.rec_len[ i ] );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_timebase( void )
{
    char reply[ 30 ];
    long length = 30;


    lecroy9400_talk( "TD,?", reply, &length );
    reply[ length - 1 ] = '\0';
    return T_atod( reply );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_timebase( double timebase )
{
    char cmd[ 40 ] = "TD,";


    gcvt( timebase, 6, cmd + strlen( cmd ) );
    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    lecroy9400.tb_index = lecroy9400_get_tb_index( timebase );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

int
lecroy9400_get_trigger_source( void )
{
    char reply[ 30 ];
    long length = 30;
    int src = LECROY9400_UNDEF;


    lecroy9400_talk( "TRS,?", reply, &length );

    if ( reply[ 0 ] == 'C' )
        src = reply[ 1 ] == '1' ? LECROY9400_CH1 : LECROY9400_CH2;
    else if ( reply[ 0 ] == 'L' )
        src = LECROY9400_LIN;
    else if ( reply[ 0 ] == 'E' )
        src = reply[ 1 ] == 'X' ? LECROY9400_EXT : LECROY9400_EXT10;
    else
        lecroy9400_invalid_reply( );

    return src;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_trigger_source( int channel )
{
    char cmd[ 40 ] = "TRS,";


    fsc2_assert(    channel == LECROY9400_CH1 || channel == LECROY9400_CH2
                 || (    channel >= LECROY9400_LIN
                      && channel <= LECROY9400_EXT10 ) );

    if ( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 )
        sprintf( cmd + 4, "C%1d", channel + 1 );
    else if ( channel == LECROY9400_LIN )
        strcat( cmd, "LI" );
    else if ( channel == LECROY9400_EXT )
        strcat( cmd, "EX" );
    else
        strcat( cmd, "E/10" );

    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    lecroy9400.trigger_level = lecroy9400_get_trigger_level( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

int
lecroy9400_get_trigger_mode( void )
{
    char reply[ 30 ];
    long length = 30;
    int mode = LECROY9400_UNDEF;


    lecroy9400_talk( "TRM,?", reply, &length );

    if ( reply[ 0 ] == 'A' )
        mode = TRG_MODE_AUTO;
    else if ( reply[ 0 ] == 'N' )
        mode = TRG_MODE_NORMAL;
    else if ( reply[ 0 ] == 'S' )
        mode = reply[ 1 ] == 'I' ? TRG_MODE_SINGLE : TRG_MODE_SEQ;
    else
        lecroy9400_invalid_reply( );;

    return mode;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_trigger_mode( int mode )
{
    const char *cmd[ ] = { "TRM,AU", "TRM,NO", "TRM,SI", "TRM,SE" };


    fsc2_assert( mode >= TRG_MODE_AUTO && mode <= TRG_MODE_SEQ );

    if ( gpib_write( lecroy9400.device, ( char * ) cmd[ mode ],
                     strlen( cmd[ mode ] ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

int
lecroy9400_get_trigger_coupling( void )
{
    char reply[ 30 ];
    long length = 30;
    int cpl = LECROY9400_UNDEF;


    lecroy9400_talk( "TRC,?", reply, &length );

    if ( reply[ 0 ] == 'A' )
        cpl = TRG_CPL_AC;
    else if ( reply[ 0 ] == 'D' )
        cpl = TRG_CPL_DC;
    else if ( reply[ 0 ] == 'L' )
        cpl = TRG_CPL_LF_REJ;
    else if ( reply[ 0 ] == 'H' )
        cpl = TRG_CPL_HF_REJ;
    else
        lecroy9400_invalid_reply( );;

    return cpl;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_trigger_coupling( int cpl )
{
    const char *cmd[ ] = { "TRC,AC", "TRC,DC", "TRC,LF", "TRC,HF" };


    fsc2_assert( cpl >= TRG_CPL_AC && cpl <= TRG_CPL_HF_REJ );

    if ( gpib_write( lecroy9400.device, ( char * ) cmd[ cpl ],
                     strlen( cmd[ cpl ] ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_trigger_level( void )
{
    char reply[ 30 ];
    long length = 30;


    lecroy9400_talk( "TRL,?", reply, &length );
    reply[ length - 1 ] = '\0';
    return T_atod( reply );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_trigger_level( double level )
{
    char cmd[ 40 ] = "TRL,";


    gcvt( level, 6, cmd + strlen( cmd ) );
    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

int
lecroy9400_get_trigger_slope( void )
{
    char reply[ 30 ];
    long length = 30;
    int slope = LECROY9400_UNDEF;


    lecroy9400_talk( "TRP,?", reply, &length );
    if ( reply[ 0 ] == 'N' )
        slope = TRG_SLOPE_NEG;
    else if ( reply[ 0 ] == 'P' )
        slope = reply[ 3 ] != '_' ? TRG_SLOPE_POS : TRG_SLOPE_POS_NEG;
    else
        lecroy9400_invalid_reply( );;

    return slope;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_trigger_slope( int slope )
{
    const char *cmd[ ] = { "TRP,PO", "TRP,NE", "TRP,PN" };


    fsc2_assert( slope >= TRG_SLOPE_POS && slope <= TRG_SLOPE_POS_NEG );

    if ( gpib_write( lecroy9400.device, ( char * ) cmd[ slope ],
                     strlen( cmd[ slope ] ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = 30;


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dVD,?", channel + 1 );
    lecroy9400_talk( cmd, reply, &length );
    reply[ length - 1 ] = '\0';
    lecroy9400.sens[ channel ] = T_atod( reply );

    return lecroy9400.sens[ channel ];
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_sens( int    channel,
                     double sens )
{
    char cmd[ 40 ];


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dVD,", channel + 1 );
    gcvt( sens, 8, cmd + strlen( cmd ) );
    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    lecroy9400_get_desc( channel, SET );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_offset( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = 30;


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dOF,?", channel + 1 );
    lecroy9400_talk( cmd, reply, &length );
    reply[ length - 1 ] = '\0';
    lecroy9400.offset[ channel ] = T_atod( reply );

    return lecroy9400.offset[ channel ];
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_offset( int    channel,
                       double offset )
{
    char cmd[ 40 ];


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dOF,", channel + 1 );
    gcvt( offset, 8, cmd + strlen( cmd ) );
    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    lecroy9400_get_desc( channel, SET );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

int
lecroy9400_get_coupling( int channel )
{
    char cmd[ 20 ];
    int type = LECROY9400_UNDEF;
    char reply[ 100 ];
    long length = 100;


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dCP,?", channel + 1 );
    lecroy9400_talk( cmd, reply, &length );

    if ( reply[ 0 ] == 'A' )
        type = AC_1_MOHM;
    else if ( reply[ 0 ] == 'D' )
        type = reply[ 3 ] == '1' ? DC_1_MOHM : DC_50_OHM;
    else if ( reply[ 0 ] == 'G' )
        type = GND;
    else
        lecroy9400_invalid_reply( );

    return type;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_coupling( int channel,
                         int type )
{
    char cmd[ 30 ];
    char const *cpl[ ] = { "A1M", "D1M", "D50", "GND" };


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );
    fsc2_assert( type >= AC_1_MOHM && type <= GND );

    sprintf( cmd, "C%1dCP,%s", channel + 1, cpl[ type ] );
    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    lecroy9400_get_desc( channel, SET );
    lecroy9400_get_sens( channel );

    return OK;
}


/*-------------------------------------------------------------*
 * Function to determine if the bandwidth limiter is on or off
 *-------------------------------------------------------------*/

int
lecroy9400_get_bandwidth_limiter( void )
{
    char buf[ 30 ] = "BW,?";
    long length = 30;

    lecroy9400_talk( ( const char * ) buf, buf, &length );
    return buf[ 1 ] == 'N';
}


/*----------------------------------------------------*
 * Function to switch the bandwidth limiter on or off
 *----------------------------------------------------*/

void
lecroy9400_set_bandwidth_limiter( bool state )
{
    char cmd[ 30 ] = "BW,";

    strcat( cmd, state ? "ON" : "OFF" );
    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_is_displayed( int channel )
{
    char reply[ 30 ];
    long length = 30;
    char const *cmds[ ] = { "TRC1,?", "TRC2,?", "TRMC,?",
                            "TRMD,?", "TRFE,?", "TRFF,?" };


    fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

    lecroy9400_talk( cmds[ channel - LECROY9400_CH1 ], reply, &length );
    reply[ length - 1 ] = '\0';
    return strcmp( reply, "ON" ) ? UNSET : SET;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_display( int channel,
                    int on_off )
{
    char cmds[ ][ 9 ] = { "TRC1,", "TRC2,",
                          "TRMC,", "TRMD,",
                          "TRFE,", "TRFF," };


    fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

    if ( on_off && lecroy9400.num_used_channels >= MAX_USED_CHANNELS )
    {
        print( FATAL, "More than %d channels are needed to run the "
               "experiment.\n", MAX_USED_CHANNELS );
        THROW( EXCEPTION );
    }

    strcat( cmds[ channel - LECROY9400_CH1 ], on_off ? "ON" : "OFF" );

    if ( gpib_write( lecroy9400.device, cmds[ channel - LECROY9400_CH1 ],
                     strlen( cmds[ channel - LECROY9400_CH1 ] ) ) == FAILURE )
        lecroy9400_gpib_failure( );

    if ( on_off )
    {
        lecroy9400.num_used_channels++;
        lecroy9400.channels_in_use[ channel ] = SET;
    }
    else
    {
        lecroy9400.num_used_channels--;
        lecroy9400.channels_in_use[ channel ] = UNSET;
    }

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

long
lecroy9400_get_num_avg( int channel )
{
    int i;
    long num_avg;


    fsc2_assert(    channel == LECROY9400_FUNC_E
                 || channel == LECROY9400_FUNC_F );

    /* Check that the channel is in averaging mode (in which case the byte
       at offset 65 of the waveform description is 0), if not return -1 */

    if ( ( int ) lecroy9400.wv_desc[ channel ][ 65 ] != 0 )
        return -1;

    /* 32 bit word (MSB first) at offset 74 of the waveform description is
       the number of averages to be done */

    for ( num_avg = 0, i = 74; i < 78; i++ )
        num_avg = num_avg * 256 + ( long ) lecroy9400.wv_desc[ channel ][ i ];

    return num_avg;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_get_desc( int  channel,
                     bool do_stop )
{
    long len = MAX_DESC_LEN;
    char const *cmds[ ] = { "RD,C1.DE", "RD,C2.DE",
                            "RD,MC.DE", "RD,MD.DE",
                            "RD,FE.DE", "RD,FF.DE" };


    fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

    /* If the device is armed and waiting for a trigger it won't return
       on a commabd that asks for a waveform descriptor. Since the device
       may just be in this state without any triggers coming this could
       lead to a spurious GPIB timeout. So, unless the caller tells us,
       stop it */

    if (    do_stop
         && gpib_write( lecroy9400.device, "STOP", 4 ) == FAILURE )
        lecroy9400_gpib_failure( );

    lecroy9400_talk( cmds[ channel ],
                     ( char * ) lecroy9400.wv_desc[ channel ], &len );

	/* The first 4 bytes are "junk" we don't need */

    memmove( lecroy9400.wv_desc[ channel ], lecroy9400.wv_desc[ channel ] + 4,
             len - 4 );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_trigger_delay( void )
{
    char reply[ 40 ];
    long length = 40;
    double delay;


    lecroy9400_talk( "TRD,?", reply, &length );
    reply[ length - 1 ] = '\0';
    delay = T_atod( reply );

    /* Positive delays (i.e. when pre-trigger is on) get returned as
       a percentage of the full horizontal screen (i.e. 10 times the
       timebase) while for negative values it's already the delay time */

    if ( delay > 0.0 )
        delay = 0.1 * lecroy9400.timebase;

    return delay;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_set_trigger_delay( double delay )
{
    char cmd[ 40 ] = "TRD,";


    /* For positive delay (i.e. pre-trigger) the delay must be set as a
       percentage of the full horizontal screen width (i.e. 10 times the
       timebase) */

    if ( delay > 0.0 )
        delay = 10.0 * delay / lecroy9400.timebase;

    gcvt( delay, 8, cmd + strlen( cmd ) );
    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_set_up_averaging( long channel,
                             long source,
                             long num_avg,
                             bool reject,
                             long rec_len )
{
    char cmd[ 100 ];
    size_t i;


    fsc2_assert(    channel >= LECROY9400_FUNC_E
                 && channel <= LECROY9400_FUNC_F );
    fsc2_assert( source >= LECROY9400_CH1 && source <= LECROY9400_CH2 );

    lecroy9400.channels_in_use[ channel ] = SET;
    lecroy9400.channels_in_use[ source ] = SET;
    lecroy9400.source_ch[ channel ] = source;
    lecroy9400.num_avg[ channel ] = num_avg;
    lecroy9400.is_num_avg[ channel ] = SET;
    lecroy9400.rec_len[ channel ] = rec_len;
    lecroy9400.is_reject[ channel ] = reject;

    if ( FSC2_MODE != EXPERIMENT )
        return;

    /* We need to send the digitizer the number of points it got to average
       over. This number isn't identical to the number of points on the
       screen, so we pick one of the list of allowed values that's at least
       as large as the number of points we're going to fetch. */

    for ( i = 0; i < NUM_ELEMS( cl ) && rec_len > cl[ i ]; i++ )
        /* empty */ ;
    rec_len = cl[ i ];

    snprintf( cmd, 100, "SEL,%s;RDF,AVG,SUMMED,%ld,%s,%ld,%s,0",
              channel == LECROY9400_FUNC_E ? "FE" : "FF", rec_len,
              source == LECROY9400_CH1 ? "C1" : "C2", num_avg,
              reject ? "ON" : "OFF" );
    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );
}



/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_finished( void )
{
    gpib_local( lecroy9400.device );
    is_acquiring = UNSET;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_start_acquisition( void )
{
    if ( lecroy9400.channels_in_use[ LECROY9400_FUNC_E ] )
    {
        if ( lecroy9400.num_used_channels >= MAX_USED_CHANNELS )


        if ( ! lecroy9400_is_displayed(
                                  lecroy9400.source_ch[ LECROY9400_FUNC_E ] ) )
             lecroy9400_display( lecroy9400.source_ch[ LECROY9400_FUNC_E ],
                                 1 );
        if ( ! lecroy9400_is_displayed( LECROY9400_FUNC_E ) )
            lecroy9400_display( LECROY9400_FUNC_E, 1 );

        if ( gpib_write( lecroy9400.device, "SEL,FE;ARST", 11 )
             == FAILURE )
            lecroy9400_gpib_failure( );
    }

    if ( lecroy9400.channels_in_use[ LECROY9400_FUNC_F ] )
    {
        /* Switch off some math channels if necessary */

        if ( lecroy9400.num_used_channels >= MAX_USED_CHANNELS - 2 )
            lecroy9400_display( LECROY9400_MEM_C, 0 );

        if ( lecroy9400.num_used_channels >= MAX_USED_CHANNELS - 1 )
            lecroy9400_display( LECROY9400_MEM_D, 0 );

        if ( ! lecroy9400_is_displayed(
                                  lecroy9400.source_ch[ LECROY9400_FUNC_F ] ) )
             lecroy9400_display( lecroy9400.source_ch[ LECROY9400_FUNC_F ],
                                 1 );
        if ( ! lecroy9400_is_displayed( LECROY9400_FUNC_F ) )
            lecroy9400_display( LECROY9400_FUNC_F, 1 );

        if ( gpib_write( lecroy9400.device, "SEL,FF;ARST", 11 )
                         == FAILURE )
            lecroy9400_gpib_failure( );
    }

    is_acquiring = SET;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_get_curve( int        ch,
                      Window_T * w  UNUSED_ARG,
                      double **  array,
                      long *     length,
                      bool       use_cursor  UNUSED_ARG )
{
    unsigned char *data;
    unsigned char *dp;
    char cmd[ 100 ];
    long len;
    long i;
    double gain_fac, vgain_fac, offset_shift;


    if ( ch >= LECROY9400_CH1 && ch <= LECROY9400_CH2 )
    {
        print( FATAL, "Getting normal, non-averaged channels is not "
               "yet implemented.\n" );
        THROW( EXCEPTION );
    }

    if ( ch >= LECROY9400_MEM_C && ch <= LECROY9400_MEM_D )
    {
        print( FATAL, "Getting memory channels is not yet implemented.\n" );
        THROW( EXCEPTION );
    }

    fsc2_assert( ch == LECROY9400_FUNC_E || ch == LECROY9400_FUNC_F );

    if ( ! lecroy9400.is_num_avg[ ch ] )
    {
        print( FATAL, "Averaging has not been initialized for channel %s.\n",
               LECROY9400_Channel_Names[ ch ] );
        THROW( EXCEPTION );
    }

    if ( ! is_acquiring )
    {
        print( FATAL, "No acquisition has been started.\n" );
        THROW( EXCEPTION );
    }

    /* Poll the device until averaging is finished, i.e. get the channel
       description and check if there are as many averages as we need */

    while ( 1 )
    {
        long cur_avg;
        long j;

        stop_on_user_request( );

        fsc2_usleep( 20000, UNSET );

        lecroy9400_get_desc( ch, UNSET );
        for ( cur_avg = 0, j = 0; j < 4; j++ )
            cur_avg = cur_avg * 256 +
                ( long ) lecroy9400.wv_desc[ ch ][ 98 + j ];

        if ( cur_avg >= lecroy9400.num_avg[ ch ] )
            break;
    }

    *length = lecroy9400.rec_len[ ch ];

    len = 2 * *length + 10;
    data = T_malloc( len );

    snprintf( cmd, 100, "RD,%s.DA,1,%ld,0,0",
              ch == LECROY9400_FUNC_E ? "FE" : "FF", *length );
    lecroy9400_talk( cmd, ( char * ) data, &len );

    *length = ( ( long ) data[ 2 ] * 256 + ( long ) data[ 3 ] ) / 2;

    /* Make sure we didn't get less data than the header of the data set
       tells us. */

    if ( len < 2 * *length + 4 )
    {
        is_acquiring = UNSET;
        T_free( data );
        print( FATAL, "Received invalid data from digitizer.\n" );
        THROW( EXCEPTION );
    }

    TRY
    {
        *array = T_malloc( *length * sizeof **array );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
    {
        is_acquiring = UNSET;
        T_free( data );
        RETHROW( );
    }

    gain_fac = sset[ ( int ) lecroy9400.wv_desc[ ch ][ 0 ] - 22 ];
    vgain_fac = 200.0 / ( ( int ) lecroy9400.wv_desc[ ch ][ 5 ] + 80 );
    offset_shift = 0.04
               * ( ( double ) lecroy9400.wv_desc[ ch ][ 4 ] * 256.0
                   + ( double ) lecroy9400.wv_desc[ ch ][ 5 ] - 200 );

    for ( dp = data + 4, i = 0; i < *length; dp += 2, i++ )
        *( *array + i ) = ( double ) dp[ 0 ] * 256.0 + ( double ) dp[ 1 ];

    for ( i = 0; i < *length; i++ )
        *( *array + i ) = gain_fac * ( ( *( *array + i ) - 32768.0 ) / 8192.0
                                       - offset_shift ) * vgain_fac;

    T_free( data );
    is_acquiring = UNSET;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool
lecroy9400_command( const char * cmd )
{
    if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy9400_gpib_failure( );
    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
lecroy9400_talk( const char * cmd,
                 char *       reply,
                 long *       length )
{
    if (    gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE
         || gpib_read( lecroy9400.device, reply, length ) == FAILURE )
        lecroy9400_gpib_failure( );
    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lecroy9400_gpib_failure( void )
{
    is_acquiring = UNSET;

    print( FATAL, "Communication with device failed.\n" );
    THROW( EXCEPTION );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lecroy9400_invalid_reply( void )
{
    is_acquiring = UNSET;

    print( FATAL, "Device sent invalid data.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
