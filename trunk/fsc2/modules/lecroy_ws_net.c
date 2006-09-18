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


#include "lecroy_ws.h"
#include "lecroy_vicp.h"


static unsigned char *lecroy_ws_get_data( long * len );

static bool lecroy_ws_can_fetch( int ch );

static unsigned int lecroy_ws_get_inr( void );

static long lecroy_ws_get_int_value( int          ch,
                                     const char * name );

static double lecroy_ws_get_float_value( int          ch,
                                         const char * name );

static void lecroy_ws_get_prep( int              ch,
                                Window_T *       w,
                                unsigned char ** data,
                                long *           len,
                                double *         gain,
                                double *         offset );

static int lecroy_ws_talk( const char * cmd,
                           char *       reply,
                           ssize_t *    length );

static void lecroy_ws_lan_failure( void );



/*---------------------------------------------------------------*
 * Function called for initialization of device and to determine
 * its state
 *---------------------------------------------------------------*/

bool lecroy_ws_init( const char * name )
{
    char buffer[ 100 ];
    ssize_t len;
    size_t i;
    bool with_eoi;


    /* Open a socket to the device */

    lecroy_vicp_init( name, NETWORK_ADDRESS, 100000, 1 );

    lecroy_vicp_set_timeout( READ, READ_TIMEOUT ); 
    lecroy_vicp_set_timeout( WRITE, WRITE_TIMEOUT );

    TRY
    {
        /* Disable the local button, set digitizer to short form of replies,
           transmit data in one block in binary word (2 byte) format with LSB
           first. Then ask for status byte to make sure the device reacts. */

        len = 51;
        if ( lecroy_vicp_write( "CHDR OFF;CHLP OFF;CFMT DEF9,WORD,BIN;"
                                "CORD LO;*STB?\n", &len, SET, UNSET )
             != SUCCESS )
            lecroy_ws_lan_failure( );

        len = 100;
        if ( lecroy_vicp_read( buffer, &len, &with_eoi, UNSET ) == FAILURE )
            lecroy_ws_lan_failure( );

        /* Figure out which traces are displayed (only 4 can be displayed
           at the same time and we must be able to check for this when
           the user asks for one more to be displayed) */

        lecroy_ws.num_used_channels = 0;

        for ( i = LECROY_WS_CH1; i <= LECROY_WS_CH_MAX; i++ )
        {
            lecroy_ws.is_displayed[ i ] = UNSET;
            if ( lecroy_ws_is_displayed( i ) )
            {
                lecroy_ws.is_displayed[ i ] = SET;
                lecroy_ws.num_used_channels++;
            }
        }

        lecroy_ws.is_displayed[ LECROY_WS_MATH ] = UNSET;
        if ( lecroy_ws_is_displayed( LECROY_WS_MATH ) )
        {
            lecroy_ws.is_displayed[ LECROY_WS_MATH ] = SET;
            lecroy_ws.num_used_channels++;
        }

        /* Make sure the internal timebase is used */

        len = 9;
        lecroy_vicp_write( "SCLK INT\n", &len, SET, UNSET );

        /* Set or get the timebase (including the index in the table of
           possible timebases) while also taking care of the mode, i.e.
           RIS or SS */

        if ( lecroy_ws.is_timebase )
            lecroy_ws_set_timebase( lecroy_ws.timebase );
        else
        {
            lecroy_ws.tb_index = -1;

            lecroy_ws.timebase = lecroy_ws_get_timebase( );

            for ( i = 0; i < NUM_TBAS; i++ )
                if ( fabs( TBAS( i ) - lecroy_ws.timebase ) / TBAS( i ) < 0.1 )
                {
                    lecroy_ws.tb_index = i;
                    break;
                }

            if ( lecroy_ws.tb_index < 0 )
            {
                print( FATAL, "Can't determine timebase.\n" );
                THROW( EXCEPTION );
            }
        }

        /* Memory sizes are a mess. The manual is nearly silent about them,
           the errata fr the manual is claiming that it can't be set but
           actuallz trying it reveals that there seem to be two memory
           sizes that can be set, either 10000 or 500000. But, strangly
           enough, the later setting results in curve lengths of only
           half that number, i.e. 250000 while setting it to 10000 gives
           zou a record length of 10000. To be precise, the exact numbers
           for the curve lengths are 10002 and 250002, so there are always
           two extra points... */

        lecroy_ws_set_memory_size( lecroy_ws.mem_size );

        /* Switch interleaved (RIS) mode on if the user asked for it and it
           can be done, otherwise switch it off */

        lecroy_ws.interleaved = lecroy_ws_get_interleaved( );

        if ( lecroy_ws.is_interleaved && lecroy_ws.interleaved &&
             lecroy_ws.cur_hres->ppd_ris > 0 )
            lecroy_ws_set_interleaved( SET );

        if ( ( lecroy_ws.is_interleaved && ! lecroy_ws.interleaved ) ||
             lecroy_ws.cur_hres->ppd_ris == 0 )
        {
            lecroy_ws_set_interleaved( UNSET );
            lecroy_ws.interleaved = UNSET;
        }

        /* Set (if required) the sensitivies, offsets coupling types and
           bandwidth limiters of all measurement channels. Since not all
           sensitivities are possible with every input impedance settings
           we need to check if conflicting settings had been used in the
           preparations or test section. */

        for ( i = LECROY_WS_CH1; i <= LECROY_WS_CH_MAX; i++ )
        {
            if ( lecroy_ws.is_coupling[ i ] )
                lecroy_ws_set_coupling( i, lecroy_ws.coupling[ i ] );
            else
            {
                lecroy_ws.coupling[ i ] = lecroy_ws_get_coupling( i );
                if ( lecroy_ws.need_high_imp[ i ] &&
                     lecroy_ws.coupling[ i ] == LECROY_WS_CPL_DC_50_OHM )
                {
                    print( FATAL, "Sensitivity setting requested either in "
                           "PREPARATIONS section or during TEST run would "
                           "require high input impedance for channel %s.\n",
                           LECROY_WS_Channel_Names[ i ] );
                    THROW( EXCEPTION );
                }
            }

            if ( lecroy_ws.is_sens[ i ] )
                lecroy_ws_set_sens( i, lecroy_ws.sens[ i ] );
            else
                lecroy_ws.sens[ i ] = lecroy_ws_get_sens( i );

            if ( lecroy_ws.is_offset[ i ] )
                lecroy_ws_set_offset( i, lecroy_ws.offset[ i ] );
            else
                lecroy_ws.offset[ i ] = lecroy_ws_get_offset( i );

            if ( lecroy_ws.is_bandwidth_limiter[ i ] )
                lecroy_ws_set_bandwidth_limiter( i,
                                            lecroy_ws.bandwidth_limiter[ i ] );
            else
                lecroy_ws.bandwidth_limiter[ i ] = 
                                          lecroy_ws_get_bandwidth_limiter( i );
        }

        /* Set (if required) the trigger source */

        if ( lecroy_ws.is_trigger_source )
            lecroy_ws_set_trigger_source( lecroy_ws.trigger_source );
        else
            lecroy_ws.trigger_source = lecroy_ws_get_trigger_source( );

        /* Set (if required) the trigger level, slope and coupling of the
           trigger channels */

        for ( i = 0; i < ( int ) NUM_ELEMS( trg_channels ); i++ )
        {
            if ( lecroy_ws.is_trigger_slope[ trg_channels[ i ] ] )
                lecroy_ws_set_trigger_slope( trg_channels[ i ],
                                lecroy_ws.trigger_slope[ trg_channels[ i ] ] );
            else
                lecroy_ws.trigger_slope[ trg_channels[ i ] ] =
                              lecroy_ws_get_trigger_slope( trg_channels[ i ] );

            if ( trg_channels[ i ] == LECROY_WS_LIN )
                continue;

            if ( lecroy_ws.is_trigger_level[ trg_channels[ i ] ] )
                lecroy_ws_set_trigger_level( trg_channels[ i ],
                                lecroy_ws.trigger_level[ trg_channels[ i ] ] );
            else
                lecroy_ws.trigger_level[ trg_channels[ i ] ] =
                              lecroy_ws_get_trigger_level( trg_channels[ i ] );

            if ( lecroy_ws.is_trigger_coupling[ trg_channels[ i ] ] )
                lecroy_ws_set_trigger_coupling( trg_channels[ i ],
                             lecroy_ws.trigger_coupling[ trg_channels[ i ] ] );
            else
                lecroy_ws.trigger_coupling[ trg_channels[ i ] ] =
                           lecroy_ws_get_trigger_coupling( trg_channels[ i ] );
        }

        /* Set (if required) the trigger delay */

        if ( lecroy_ws.is_trigger_delay )
            lecroy_ws_set_trigger_delay( lecroy_ws.trigger_delay );
        else
            lecroy_ws_get_trigger_delay( );

        /* Get or set the trigger mode */

        if ( lecroy_ws.is_trigger_mode )
            lecroy_ws_set_trigger_mode( lecroy_ws.trigger_mode );
        else
            lecroy_ws_get_trigger_mode( );

        /* Now that we know about the timebase and trigger delay settings
           we can also do the checks on the window and trigger delay settngs
           that may not have been possible during the test run */

        lecroy_ws_soe_checks( );
    }
    OTHERWISE
        return FAIL;
        
    return OK;
}


/*-------------------------------------------------*
 * Function for determing the digitizers timebase 
 *-------------------------------------------------*/

double lecroy_ws_get_timebase( void )
{
    char reply[ 30 ];
    ssize_t len = 30;
    double timebase;
    size_t i;


    if ( lecroy_ws_talk( "TDIV?\n", reply, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    reply[ len - 1 ] = '\0';
    timebase = T_atod( reply );

    for ( i = 0; i < NUM_TBAS; i++ )
        if ( fabs( TBAS( i ) - timebase ) / timebase < 0.01 )
            break;

    if ( i == NUM_TBAS )
    {
        print( FATAL, "Can't determine timebase.\n" );
        THROW( EXCEPTION );
    }

    lecroy_ws.timebase = timebase;
    lecroy_ws.tb_index = i;

    return lecroy_ws.timebase;
}


/*-----------------------------------------------*
 * Function for setting the digitizers timebase 
 *-----------------------------------------------*/

bool lecroy_ws_set_timebase( double timebase )
{
    char cmd[ 40 ] = "TDIV ";
    ssize_t len;


    gcvt( timebase, 8, cmd + strlen( cmd ) );
    strcat( cmd, "\n" );
    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*------------------------------------------------------------*
 * Function for determining if digitizer is in RIS or SS mode
 *------------------------------------------------------------*/

bool lecroy_ws_get_interleaved( void )
{
    char reply[ 30 ];
    ssize_t len = 30;


    if ( lecroy_ws_talk( "ILVD?\n", reply, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    lecroy_ws.interleaved = reply[ 1 ] == 'N';

    return lecroy_ws.interleaved;
}


/*------------------------------------------------*
 * Function for switching between RIS and SS mode
 *------------------------------------------------*/

bool lecroy_ws_set_interleaved( bool state )
{
    char cmd[ 10 ] = "ILVD ";
    ssize_t len;


    strcat( cmd, state ? "ON\n" : "OFF\n" );
    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*----------------------------------------------------------*
 * Function for determining the memory size currently used.
 *----------------------------------------------------------*/

long lecroy_ws_get_memory_size( void )
{
    char reply[ 30 ];
    ssize_t len = 30;
    long mem_size;


    if ( lecroy_ws_talk( "MSIZ?\n", reply, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    reply[ len - 1 ] = '\0';

    mem_size = lrnd( T_atod( reply ) );

    /* For the short memory size (10000) the record length is equal to that
       value but for the large value (500000) the record length is just
       half of that. */

    return mem_size;
}


/*--------------------------------------*
 * Function for setting the memory size
 *--------------------------------------*/

bool lecroy_ws_set_memory_size( long mem_size )
{
    char cmd[ 30 ];
    ssize_t len;


    sprintf( cmd, "MSIZ %ld\n", mem_size );
    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    /* The memory size influences the horizontal resolution */

    if ( mem_size == LECROY_WS_SHORT_MEM_SIZE )
        lecroy_ws.cur_hres = hres[ 0 ] + lecroy_ws.tb_index;
    else
        lecroy_ws.cur_hres = hres[ 1 ] + lecroy_ws.tb_index;

    return OK;
}


/*------------------------------------------------------------------*
 * Function for determining the sensitivity (in V/div) of a channel
 *------------------------------------------------------------------*/

double lecroy_ws_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    ssize_t len = 30;


    fsc2_assert( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX );

    sprintf( cmd, "C%1d:VDIV?\n", channel - LECROY_WS_CH1 + 1 );
    if ( lecroy_ws_talk( cmd, reply, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    reply[ len - 1 ] = '\0';
    return lecroy_ws.sens[ channel ] = T_atod( reply );
}


/*--------------------------------------------------------------*
 * Function for setting the sensitivity (in V/div) of a channel
 *--------------------------------------------------------------*/

bool lecroy_ws_set_sens( int    channel,
                          double sens )
{
    char cmd[ 40 ];
    ssize_t len;


    fsc2_assert( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX );

    sprintf( cmd, "C%1d:VDIV ", channel - LECROY_WS_CH1 + 1 );
    gcvt( sens, 8, cmd + strlen( cmd ) );
    strcat( cmd, "\n" );
    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*-----------------------------------------------------------*
 * Function for determining the offset voltage for a channel
 *-----------------------------------------------------------*/

double lecroy_ws_get_offset( int channel )
{
    char buf[ 30 ];
    ssize_t len = 30;


    fsc2_assert( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX );

    sprintf( buf, "C%1d:OFST?\n", channel - LECROY_WS_CH1 + 1 );
    if ( lecroy_ws_talk( buf, buf, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    buf[ len - 1 ] = '\0';
    return  lecroy_ws.offset[ channel ] = T_atod( buf );
}


/*-------------------------------------------------------*
 * Function for setting the offset voltage for a channel
 *-------------------------------------------------------*/

bool lecroy_ws_set_offset( int    channel,
                            double offset )
{
    char cmd[ 40 ];
    ssize_t len;


    fsc2_assert( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX );

    sprintf( cmd, "C%1d:OFST ", channel - LECROY_WS_CH1 + 1 );
    gcvt( offset, 8, cmd + strlen( cmd ) );
    strcat( cmd, "\n" );
    len = strlen( cmd ),
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*----------------------------------------------------------*
 * Function for determining the coupling type for a channel
 *----------------------------------------------------------*/

int lecroy_ws_get_coupling( int channel )
{
    int type = LECROY_WS_CPL_INVALID;
    char buf[ 100 ];
    ssize_t len = 100;


    fsc2_assert( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX );

    sprintf( buf, "C%1d:CPL?\n", channel - LECROY_WS_CH1 + 1 );
    if ( lecroy_ws_talk( buf, buf, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    buf[ len - 1 ] = '\0';

    if ( buf[ 0 ] == 'A' )
        type = LECROY_WS_CPL_AC_1_MOHM;
    else if ( buf[ 0 ] == 'G' )
        type = LECROY_WS_CPL_GND;
    else if ( buf[ 0 ] == 'D' && buf[ 1 ] == '1' )
        type = LECROY_WS_CPL_DC_1_MOHM;
    else if ( buf[ 0 ] == 'D' && buf[ 1 ] == '5' )
        type = LECROY_WS_CPL_DC_50_OHM;
    else if ( buf[ 0 ] == 'O' )          /* overload with 50 Ohm DC coupling */
    {
        type = LECROY_WS_CPL_DC_50_OHM;
        print( SEVERE, "Signal overload detected for channel '%s', input "
               "automatically disconnected.\n",
               LECROY_WS_Channel_Names[ channel ] );
    }
    else
        fsc2_impossible( );

    return lecroy_ws.coupling[ channel ] = type;
}


/*------------------------------------------------------*
 * Function for setting the coupling type for a channel
 *------------------------------------------------------*/

bool lecroy_ws_set_coupling( int channel,
                              int type )
{
    char cmd[ 30 ];
    char const *cpl[ ] = { "A1M", "D1M", "D50", "GND" };
    ssize_t len;


    fsc2_assert( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX );
    fsc2_assert( type >= LECROY_WS_CPL_AC_1_MOHM &&
                 type <= LECROY_WS_CPL_GND );

    sprintf( cmd, "C%1d:CPL %s\n", channel - LECROY_WS_CH1 + 1, cpl[ type ] );
    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*-----------------------------------------------------------------------*
 * Function for determining the bandwidth limiter of one of the channels
 *-----------------------------------------------------------------------*/

int lecroy_ws_get_bandwidth_limiter( int channel )
{
    char buf[ 30 ] = "BWL?\n";
    ssize_t len = 30;
    int mode = -1;
    char *ptr;
    char look_for[ 4 ];


    fsc2_assert( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX );

    if ( lecroy_ws_talk( buf, buf, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    buf[ len - 1 ] = '\0';

    /* We have to distinguish two cases: if all channels have the same limiter
       setting only a single value gets returned, otherwise the setting for
       each channel will be reported as a list of comma separated channel-value
       pairs */

    sprintf( look_for, "C%1d,", channel - LECROY_WS_CH1 + 1 );
    ptr = strstr( buf, look_for );

    if ( ptr == NULL )
        lecroy_ws_lan_failure( );

    if ( ptr[ 4 ] == 'F' )           /* OFF */
        mode = LECROY_WS_BWL_OFF;
    else if ( ptr[ 4 ] == 'N' )      /* ON */
        mode = LECROY_WS_BWL_ON;
#if defined LECROY_WS_BWL_200MHZ
    else if ( ptr[ 3 ] == '2' )      /* 200MHZ */
        mode = LECROY_WS_BWL_200MHZ;
#endif
    else
        fsc2_impossible( );

    lecroy_ws.bandwidth_limiter[ channel ] = mode;

    return lecroy_ws.bandwidth_limiter[ channel ];
}


/*--------------------------------------------------------------------*
 * Function for setting the bandwidth limiter for one of the channels
 *--------------------------------------------------------------------*/

bool lecroy_ws_set_bandwidth_limiter( int channel,
                                      int bwl )
{
    char buf[ 50 ];
    ssize_t len;


    fsc2_assert( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX );
    fsc2_assert( bwl >= LECROY_WS_BWL_OFF &&
#if defined LECROY_WS_BWL_200MHZ
                 bwl <= LECROY_WS_BWL_200MHZ
#else
                 bwl <= LECROY_WS_BWL_ON
#endif
                                               );

    sprintf( buf, "BWL C%d,", channel - LECROY_WS_CH1 + 1 );
    if ( bwl == LECROY_WS_BWL_OFF )
        strcat( buf, "OFF\n" );
    else if ( bwl == LECROY_WS_BWL_ON )
        strcat( buf, "ON\n" );
#if defined LECROY_WS_BWL_200MHZ
    else
        strcat( buf, "200MHZ\n" );
#endif
        
    len = strlen( buf );
    lecroy_vicp_write( buf, &len, SET, UNSET );

    return OK;
}


/*-------------------------------------------------------------*
 * Function for finding out the current trigger source channel
 *-------------------------------------------------------------*/

int lecroy_ws_get_trigger_source( void )
{
    char reply[ 100 ];
    ssize_t len = 100;
    int src;
    char *ptr = reply + 7;


    if ( lecroy_ws_talk( "TRSE?\n", reply, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    reply[ len - 1 ] = '\0';

    if ( strncmp( reply, "STD,SR,", 7 ) &&
         strncmp( reply, "EDGE,SR,", 8 ) )
    {
        print( SEVERE, "Non-standard mode trigger, switching to standard "
               "edge trigger on LINe input\n" );
        return lecroy_ws_set_trigger_source( LECROY_WS_LIN );
    }

    if ( *ptr == ',' )
        ptr++;

    if ( *ptr == 'C' )
        sscanf( ++ptr, "%d", &src );
    else if ( *ptr == 'L' )
        src = LECROY_WS_LIN;
    else if ( *ptr == 'E' && ptr[ 2 ] == '1' )
        src = LECROY_WS_EXT10;
    else if ( *ptr == 'E' && ptr[ 2 ] != '1' )
        src = LECROY_WS_EXT;
    else
        fsc2_impossible( );

    return lecroy_ws.trigger_source = src;
}


/*---------------------------------------------------------*
 * Function for setting the current trigger source channel
 *---------------------------------------------------------*/

bool lecroy_ws_set_trigger_source( int channel )
{
    char cmd[ 40 ] = "TRSE STD,SR,";
    ssize_t len;


    fsc2_assert( ( channel >= LECROY_WS_CH1 &&
                   channel <= LECROY_WS_CH_MAX ) ||
                 channel == LECROY_WS_LIN        ||
                 channel == LECROY_WS_EXT        ||
                 channel == LECROY_WS_EXT10 );

    if ( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX )
        sprintf( cmd + 11, "C%1d\n", channel - LECROY_WS_CH1 + 1 );
    else if ( channel == LECROY_WS_LIN )
        strcat( cmd, "LINE\n" );
    else if ( channel == LECROY_WS_EXT )
        strcat( cmd, "EX\n" );
    else
        strcat( cmd, "EX10\n" );

    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*-------------------------------------------------------------------*
 * Function for finding out the trigger level of one of the channels
 *-------------------------------------------------------------------*/

double lecroy_ws_get_trigger_level( int channel )
{
    char buf[ 30 ];
    ssize_t len = 30;


    fsc2_assert( ( channel >= LECROY_WS_CH1 &&
                   channel <= LECROY_WS_CH_MAX ) ||
                 channel == LECROY_WS_EXT ||
                 channel == LECROY_WS_EXT10 );

    if ( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX )
        sprintf( buf, "C%1d:TRLV?\n", channel - LECROY_WS_CH1 + 1 );
    else if ( channel == LECROY_WS_EXT )
        strcpy( buf, "EX:TRLV?\n" );
    else
        strcpy( buf, "EX10:TRLV?\n" );

    if ( lecroy_ws_talk( buf, buf, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    buf[ len - 1 ] = '\0';
    return lecroy_ws.trigger_level[ channel ] = T_atod( buf );
}


/*----------------------------------------------------------------*
 * Function for setting the trigger slope for one of the channels
 *----------------------------------------------------------------*/

bool lecroy_ws_set_trigger_level( int    channel,
                                  double level )
{
    char cmd[ 40 ];
    ssize_t len;


    fsc2_assert( ( channel >= LECROY_WS_CH1 &&
                   channel <= LECROY_WS_CH_MAX ) ||
                 channel == LECROY_WS_EXT        ||
                 channel == LECROY_WS_EXT10 );

    if ( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX )
        sprintf( cmd, "C%1d:TRLV ", channel - LECROY_WS_CH1 + 1 );
    else if ( channel == LECROY_WS_EXT )
        strcpy( cmd, "EX:TRLV " );
    else
        strcpy( cmd, "EX10:TRLV " );

    gcvt( level, 6, cmd + strlen( cmd ) );
    strcat( cmd, "\n" );
    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*-------------------------------------------------------------------*
 * Function for finding out the trigger slope of one of the channels
 *-------------------------------------------------------------------*/

bool lecroy_ws_get_trigger_slope( int channel )
{
    char buf[ 30 ];
    ssize_t len = 30;


    fsc2_assert( ( channel >= LECROY_WS_CH1 &&
                   channel <= LECROY_WS_CH_MAX ) ||
                 channel == LECROY_WS_LIN        ||
                 channel == LECROY_WS_EXT        ||
                 channel == LECROY_WS_EXT10 );

    if ( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX )
        sprintf( buf, "C%1d:TRSL?\n", channel - LECROY_WS_CH1 + 1 );
    else if ( channel == LECROY_WS_LIN )
        strcpy( buf, "LINE:TRSL?\n" );
    else if ( channel == LECROY_WS_EXT )
        strcpy( buf, "EX:TRSL?\n" );
    else
        strcpy( buf, "EX10:TRSL?\n" );

    if ( lecroy_ws_talk( buf, buf, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );

    lecroy_ws.trigger_slope[ channel ] = buf[ 0 ] == 'P' ? POSITIVE : NEGATIVE;

    return lecroy_ws.trigger_slope[ channel ];
}


/*----------------------------------------------------------------*
 * Function for setting the trigger slope for one of the channels
 *----------------------------------------------------------------*/

bool lecroy_ws_set_trigger_slope( int channel,
                                   bool slope )
{
    char cmd[ 40 ];
    ssize_t len;


    fsc2_assert( ( channel >= LECROY_WS_CH1 &&
                   channel <= LECROY_WS_CH_MAX ) ||
                 channel == LECROY_WS_LIN        ||
                 channel == LECROY_WS_EXT        ||
                 channel == LECROY_WS_EXT10 );

    if ( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX )
        sprintf( cmd, "C%1d:TRSL ", channel - LECROY_WS_CH1 + 1 );
    else if ( channel == LECROY_WS_LIN )
        strcpy( cmd, "LINE:TRSL " );
    else if ( channel == LECROY_WS_EXT )
        strcpy( cmd, "EX:TRSL " );
    else
        strcpy( cmd, "EX10:TRSL " );

    strcat( cmd, slope ? "POS\n" : "NEG\n" );
    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*-----------------------------------------------------------------------*
 * Function for finding out the trigger coupling for one of the channels
 *-----------------------------------------------------------------------*/

int lecroy_ws_get_trigger_coupling( int channel )
{
    char buf[ 40 ];
    ssize_t len = 40;
    int cpl = -1;


    fsc2_assert( ( channel >= LECROY_WS_CH1 &&
                   channel <= LECROY_WS_CH_MAX ) ||
                 channel == LECROY_WS_EXT        ||
                 channel == LECROY_WS_EXT10 );

    if ( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX )
        sprintf( buf, "C%1d:TRCP?\n", channel - LECROY_WS_CH1 + 1 );
    else if ( channel == LECROY_WS_EXT )
        strcpy( buf, "EX:TRCP?\n" );
    else
        strcpy( buf, "EX10:TRCP?\n" );

    if ( lecroy_ws_talk( buf, buf, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );

    switch ( buf[ 0 ] )
    {
        case 'A' :
            cpl = LECROY_WS_TRG_CPL_AC;
            break;

        case 'D' :
            cpl = LECROY_WS_TRG_CPL_DC;
            break;

        case 'L' :
            cpl = LECROY_WS_TRG_CPL_LF_REJ;
            break;

        case 'H' :
            cpl = LECROY_WS_TRG_CPL_HF_REJ;
            break;

        default :
            fsc2_impossible( );
    }

    lecroy_ws.trigger_coupling[ channel ] = cpl;

    return cpl;
}


/*-------------------------------------------------------------------*
 * Function for setting the trigger coupling for one of the channels
 *-------------------------------------------------------------------*/

int lecroy_ws_set_trigger_coupling( int channel,
                                    int cpl )
{
    char cmd[ 40 ];
    ssize_t len;
    const char *cpl_str[ ] = { "AC\n", "DC\n", "LFREJ\n", "HFREJ\n" };


    fsc2_assert( ( channel >= LECROY_WS_CH1 &&
                   channel <= LECROY_WS_CH_MAX ) ||
                 channel == LECROY_WS_EXT        ||
                 channel == LECROY_WS_EXT10 );
    fsc2_assert( cpl >= LECROY_WS_TRG_AC && cpl <= LECROY_WS_TRG_HF_REJ );

    if ( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX )
        sprintf( cmd, "C%1d:TRCP ", channel - LECROY_WS_CH1 + 1 );
    else if ( channel == LECROY_WS_EXT )
        strcpy( cmd, "EX:TRCP " );
    else
        strcpy( cmd, "EX10:TRCP " );

    strcat( cmd, cpl_str[ cpl ] );

    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*---------------------------------------------------*
 * Function for finding out the current trigger mode
 *---------------------------------------------------*/

int lecroy_ws_get_trigger_mode( void )
{
    char buf[ 40 ];
    ssize_t len = 40;
    int mode = -1;


    if ( lecroy_ws_talk( "TRMD?\n", buf, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );

    if ( buf[ 0 ] == 'A' )
        mode = LECROY_WS_TRG_MODE_AUTO;
    else if ( buf[ 0 ] == 'N' )
        mode = LECROY_WS_TRG_MODE_NORMAL;
    else if ( buf[ 1 ] == 'I' )
        mode = LECROY_WS_TRG_MODE_SINGLE;
    else if ( buf[ 1 ] == 'T' )
        mode = LECROY_WS_TRG_MODE_STOP;
    else
        fsc2_impossible( );

    return lecroy_ws.trigger_mode = mode;
}


/*---------------------------------------*
 * Function for setting the trigger mode 
 *---------------------------------------*/

int lecroy_ws_set_trigger_mode( int mode )
{
    char cmd[ 40 ] = "TRMD ";
    ssize_t len;
    const char *mode_str[ ] = { "AUTO\n", "NORM\n", "SINGLE\n", "STOP\n" };


    fsc2_assert( mode >= LECROY_WS_TRG_MODE_AUTO &&
                 mode <= LECROY_WS_TRG_MODE_STOP );

    strcat( cmd, mode_str[ mode ] );
    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*----------------------------------------------------*
 * Function for determining the current trigger delay
 *----------------------------------------------------*/

double lecroy_ws_get_trigger_delay( void )
{
    char reply[ 40 ];
    ssize_t len = 40;


    if ( lecroy_ws_talk( "TRDL?\n", reply, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    reply[ len - 1 ] = '\0';
    lecroy_ws.trigger_delay = T_atod( reply );

    /* Positive delays (i.e. when pretrigger is on) get returned from the
       device as a percentage of the full horizontal screen while for
       negative values it's already the delay time */

    if ( lecroy_ws.trigger_delay > 0.0 )
        lecroy_ws.trigger_delay *= 0.1 * lecroy_ws.timebase;

    return lecroy_ws.trigger_delay;
}


/*----------------------------------------*
 * Function for setting the trigger delay 
 *----------------------------------------*/

bool lecroy_ws_set_trigger_delay( double delay )
{
    char cmd[ 40 ] = "TRDL ";
    ssize_t len;


    /* For positive delay (i.e. pretrigger) the delay must be set as a
       percentage of the full horizontal screen width */

    if ( delay > 0.0 )
        delay = 10.0 * delay / lecroy_ws.timebase;

    gcvt( delay, 8, cmd + strlen( cmd ) );
    strcat( cmd, "\n" );
    len = strlen( cmd ),
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    return OK;
}


/*---------------------------------------------------------*
 * Function for checking if a certain channel is displayed
 *---------------------------------------------------------*/

bool lecroy_ws_is_displayed( int ch )
{
    char cmd[ 130 ];
    ssize_t len = 130;


    if ( ch >= LECROY_WS_CH1 && ch <= LECROY_WS_CH_MAX )
        sprintf( cmd, "C%d:TRA?\n", ch - LECROY_WS_CH1 + 1 );
    else if ( ch == LECROY_WS_MATH )
        sprintf( cmd, "F1:TRA?\n" );
    else
    {
        print( FATAL, "Internal error detected.\n" );
        THROW( EXCEPTION );
    }

    if ( lecroy_ws_talk( cmd, cmd, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    return cmd[ 1 ] == 'N';
}


/*----------------------------------------------------*
 * Function to switch  display of a channel on or off
 *----------------------------------------------------*/

bool lecroy_ws_display( int ch,
                        int on_off )
{
    char cmd[ 30 ];
    ssize_t len;
        

    if ( ch >= LECROY_WS_CH1 && ch <= LECROY_WS_CH_MAX )
        sprintf( cmd, "C%d:TRA ", ch - LECROY_WS_CH1 + 1 );
    else if ( ch == LECROY_WS_MATH )
        strcpy( cmd, "F1:TRA " );
    else if ( ch >= LECROY_WS_M1 && ch <= LECROY_WS_M4 )
    {
        print( FATAL, "Memory channels can't be displayed.\n" );
        THROW( EXCEPTION );
    }
    else
    {
        print( FATAL, "Internal error detected.\n" );
        THROW( EXCEPTION );
    }

    strcat( cmd, on_off ? "ON\n" : "OFF\n" );

    if ( on_off &&
         lecroy_ws.num_used_channels >= LECROY_WS_MAX_USED_CHANNELS )
    {
        print( FATAL, "Can't switch on another trace, there are already as "
               "many as possible (%d) displayed.\n",
               LECROY_WS_MAX_USED_CHANNELS );
        THROW( EXCEPTION );
    }

    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    if ( on_off )
    {
        lecroy_ws.num_used_channels++;
        lecroy_ws.channels_in_use[ ch ] = SET;
    }
    else
    {
        lecroy_ws.num_used_channels--;
        lecroy_ws.channels_in_use[ ch ] = UNSET;
    }

    return OK;
}


/*-----------------------------------------------*
 * Function for closing the socket to the device
 *-----------------------------------------------*/

void lecroy_ws_finished( void )
{
    lecroy_vicp_close( );
}


/*----------------------------------------------*
 * Function for setting (continuous) averaging
 * for one of the 'normal' channels.
 *---------------------------------------------*/

void lecroy_normal_channel_averaging( int  channel,
                                      long num_avg )
{
    char cmd[ 100 ];
    ssize_t len;


    fsc2_assert( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX );
    fsc2_assert( num_avg > 0 && num_avg < LECROY_WS_MAX_AVERAGES );


    sprintf( cmd, "VBS 'app.Acquisition.C%d.AverageSweeps=%ld'\n",
             channel - LECROY_WS_CH1 + 1, num_avg );
    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );
}


/*-----------------------------------------*
 * Function for starting a new acquisition
 *-----------------------------------------*/

void lecroy_ws_start_acquisition( void )
{
    char cmd[ 100 ];
    ssize_t len;
    bool do_averaging = UNSET;
    int ch;


    /* Stop the digitizer (also switches to "STOPPED" trigger mode) */

    len = 4;
    lecroy_vicp_write( "STOP\n", &len, SET, UNSET );

    /* Set up the parameter to be used for averaging for the function channels
       (as far as they have been set by the user) */

    for ( ch = LECROY_WS_CH1; ch <= LECROY_WS_CH_MAX; ch++ )
    {
        if ( lecroy_ws.is_avg_setup[ ch ] )
        {
            do_averaging = SET;
            
            lecroy_normal_channel_averaging( ch, lecroy_ws.num_avg[ ch ] );

            /* If we want to use a trace it must be switched on */

            if ( ! lecroy_ws_is_displayed( ch ) )
                lecroy_ws_display( ch, SET );

            /* Switch off horizontal zoom and shift - if it's on the curve
               fetched from the device isn't what one would expect... */

            sprintf( cmd, "C%1d:HMAG 1;C%1d:HPOS 5\n",
                     ch - LECROY_WS_CH1 + 1, ch - LECROY_WS_CH1 + 1 );
            len = strlen( cmd );
            lecroy_vicp_write( cmd, &len, SET, UNSET );

            /* Finally reset what's currently stored in the trace */

            sprintf( cmd, "C%1d:FRST\n", ch - LECROY_WS_CH1 + 1 );
            len = strlen( cmd );
            lecroy_vicp_write( cmd, &len, SET, UNSET );
        }
        else
            lecroy_normal_channel_averaging( ch, 1 );
    }

    if ( lecroy_ws.is_avg_setup[ LECROY_WS_MATH ] )
    {
        do_averaging = SET;
            
        snprintf( cmd, 100,
                  "F1:DEF EQN,'AVG(C%ld)',AVGTYPE,SUMMED,SWEEPS,%ld\n",
                  lecroy_ws.source_ch[ LECROY_WS_MATH ] - LECROY_WS_CH1 + 1,
                  lecroy_ws.num_avg[ LECROY_WS_MATH ] );

        len = strlen( cmd );
        lecroy_vicp_write( cmd, &len, SET, UNSET );

        lecroy_normal_channel_averaging( lecroy_ws.source_ch[ LECROY_WS_MATH ],
                                         1 );

        /* If we want to use a trace it must be switched on (but not
           necessarily the channel that gets averaged) */

        if ( ! lecroy_ws_is_displayed( LECROY_WS_MATH ) )
            lecroy_ws_display( LECROY_WS_MATH, SET );

        /* Switch off horizontal zoom and shift - if it's on the curve fetched
           from the device isn't what one would expect... */

        sprintf( cmd, "F1:HMAG 1;F1:HPOS 5\n" );
        len = strlen( cmd );
        lecroy_vicp_write( cmd, &len, SET, UNSET );

        /* Finally reset what's currently stored in the trace (otherwise a
           new acquisition may not get started) */

        sprintf( cmd, "F1:FRST\n" );
        len = strlen( cmd );
        lecroy_vicp_write( cmd, &len, SET, UNSET );
    }

    len = 5;
    lecroy_vicp_write( "CLSW\n", &len, SET, UNSET );

    /* Reset the bits in the word that tells us later when data from an
       normal acquisition channnel can be fetched */

    lecroy_ws_get_inr( );

    /* Switch digitizer back on to running state by switching to a trigger
       mode where the digitizer is running (i.e. typically NORMAL, but, if
       the user requested it, also AUTO, or, if there's no averaging setup,
       even SINGLE mode will do) */

    strcpy( cmd, "TRMD NORM\n" );
    if ( ! do_averaging &&
         lecroy_ws.trigger_mode == LECROY_WS_TRG_MODE_SINGLE )
        strcpy( cmd + 5, "SINGLE\n" );
    else if ( lecroy_ws.trigger_mode == LECROY_WS_TRG_MODE_AUTO )
        strcpy( cmd + 5, "AUTO\n" );
    else
        lecroy_ws.trigger_mode = LECROY_WS_TRG_MODE_NORMAL;

    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );
}


/*------------------------------------------------------------*
 * Function for fetching data from the digitizer - the calling
 * function than does the remaining specific manipulations on
 * these data
 *------------------------------------------------------------*/

static void lecroy_ws_get_prep( int              ch,
                                Window_T *       w,
                                unsigned char ** data,
                                long *           length,
                                double *         gain,
                                double *         offset )
{
    char cmd[ 100 ];
    ssize_t len;
    char ch_str[ 3 ];
    bool is_mem_ch = UNSET;


    CLOBBER_PROTECT( data );

    fsc2_assert( ( ch >= LECROY_WS_CH1 && ch <= LECROY_WS_CH_MAX )  ||
                 ( ch >= LECROY_WS_M1 && ch <= LECROY_WS_M4 ) ||
                 ch == LECROY_WS_MATH );

    /* Figure out which channel is to be used and set a few variables
       needed later accordingly */

    if ( ch >= LECROY_WS_CH1 && ch <= LECROY_WS_CH_MAX )
        sprintf( ch_str, "C%d", ch - LECROY_WS_CH1 + 1 );
    else if ( ch >= LECROY_WS_M1 && ch <= LECROY_WS_M4 )
    {
        is_mem_ch = SET;
        sprintf( ch_str, "M%d", ch - LECROY_WS_M1 + 1 );
    }
    else if ( ch == LECROY_WS_MATH )
        strcpy( ch_str, "F1" );

    /* Set up the number of points to be fetched - take care: the device
       always measures an extra point before and after the displayed region,
       thus we start with one point later than we could get from it. */

    if ( w != NULL )
        sprintf( cmd, "WFSU SP,0,NP,%ld,FP,%ld,SN,0\n",
                 w->num_points, w->start_num + 1 );
    else
        sprintf( cmd, "WFSU SP,0,NP,%ld,FP,1,SN,0\n",
                 lecroy_ws_curve_length( ) );

    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );

    /* When a non-memory curve is to be fetched and the acquisition isn't
       finished yet poll until the bit that tells that the acquisition for
       the requested channel is finished has become set */

    while ( ! is_mem_ch && ! lecroy_ws_can_fetch( ch ) )
    {
        stop_on_user_request( );
        fsc2_usleep( 20000, UNSET );
    }

    TRY
    {
        /* Ask the device for the data... */

        strcpy( cmd, ch_str );
        strcat( cmd, ":WF? DAT1\n" );
        len = strlen( cmd );
        lecroy_vicp_write( cmd, &len, SET, UNSET );

        /* ...and fetch 'em */

        *data = lecroy_ws_get_data( length );
        *length /= 2;          /* we got word sized (16 bit) data, LSB first */

        /* Get the gain factor and offset for the date we just fetched */

        *gain = lecroy_ws_get_float_value( ch, "VERTICAL_GAIN" );
        *offset = lecroy_ws_get_float_value( ch, "VERTICAL_OFFSET" );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( *data != NULL )
            T_free( *data );
        RETHROW( );
    }
}


/*--------------------------------------------------------------------*
 * Function for checking if data of a channel are ready to be fetched
 *--------------------------------------------------------------------*/

static bool lecroy_ws_can_fetch( int ch )
{
    if ( ch >= LECROY_WS_CH1 && ch <= LECROY_WS_CH_MAX &&
         ! lecroy_ws.is_avg_setup[ ch ] )
        return lecroy_ws_get_inr( ) & LECROY_WS_SIGNAL_ACQ;

    return lecroy_ws_get_int_value( ch, "SWEEPS_PER_ACQ" ) >=
                                                       lecroy_ws.num_avg[ ch ];
}


/*-----------------------------------------------------*
 * Function for fetching a curve from the oscilloscope
 *-----------------------------------------------------*/

void lecroy_ws_get_curve( int        ch,
                          Window_T * w,
                          double **  array,
                          long *     length )
{
    double gain, offset;
    unsigned char *data;
    unsigned char *dp;
    long i;
    int val;


    /* Get the curve from the device */

    lecroy_ws_get_prep( ch, w, &data, length, &gain, &offset );

    /* Calculate the voltages from the data, data are two byte (LSB first),
       two's complement integers, which then need to be scaled by gain and
       offset. */

    *array = DOUBLE_P T_malloc( *length * sizeof **array + 1 );

    for ( i = 0, dp = data; i < *length; dp += 2, i++ )
    {
        val = dp[ 0 ] + 0x100 * dp[ 1 ];

        if ( dp[ 1 ] & 0x80  )
            val -= 0x10000;

        ( *array )[ i ] = gain * val - offset;
    }

    T_free( data );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double lecroy_ws_get_area( int        ch,
                           Window_T * w )
{
    unsigned char *data;
    unsigned char *dp;
    long i;
    double gain, offset;
    double area = 0.0;
    int val;
    long length;


    /* Get the curve from the device */

    lecroy_ws_get_prep( ch, w, &data, &length, &gain, &offset );

    /* Calculate the voltages from the data, data are two byte (LSB first),
       two's complement integers, which then need to be scaled by gain and
       offset. */

    for ( i = 0, dp = data; i < length; dp += 2, i++ )
    {
        val = dp[ 0 ] + 0x100 * dp[ 1 ];

        if ( dp[ 1 ] & 0x80 ) 
            val -= 0x10000;

        area += gain * val - offset;
    }

    T_free( data );

    return area;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double lecroy_ws_get_amplitude( int        ch,
                                Window_T * w )
{
    unsigned char *data = NULL;
    unsigned char *dp;
    long i;
    double gain, offset;
    long min, max;
    long val;
    long length;


    /* Get the curve from the device */

    lecroy_ws_get_prep( ch, w, &data, &length, &gain, &offset );

    /* Calculate the maximum and minimum voltages from the data, data are two
       byte (LSB first), two's complement integers */

    min = LONG_MAX;
    max = LONG_MIN;

    for ( i = 0, dp = data; i < length; i++, dp += 2 )
    {
        val = dp[ 0 ] + 0x100 * dp[ 1 ];

        if ( dp[ 1 ] & 0x80 )
            val -= 0x10000;

        max = l_max( val, max );
        min = l_min( val, min );
    }

    T_free( data );

    /* Return difference between highest and lowest value (in volt units) */

    return gain * ( max - min );
}


/*----------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

void lecroy_ws_copy_curve( long src,
                           long dest )
{
    char cmd[ 100 ] = "STO ";
    ssize_t len;


    fsc2_assert( src >= LECROY_WS_CH1 && src <= LECROY_WS_CH_MAX );
    fsc2_assert( dest >= LECROY_WS_M1 && dest <= LECROY_WS_M4 );


    sprintf( cmd + strlen( cmd ), "C%ld,M%ld\n", src - LECROY_WS_CH1 + 1,
             dest - LECROY_WS_M1 + 1 );

    len = strlen( cmd );
    lecroy_vicp_write( cmd, &len, SET, UNSET );
}


/*----------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static unsigned char *lecroy_ws_get_data( long * length )
{
    unsigned char *data;
    char len_str[ 10 ];
    ssize_t len;
    bool with_eoi;
    ssize_t gotten;
    ssize_t to_get;


    /* First thing we read is something like "DAT1,#[0-9]" where the number
       following the '#' is the number of bytes to be read next - it's
       wrapped into a loop since for some undocumented reasons the device
       first sends only 5 bytes (the "DAT1" part) and only then, following
       another header, the "#[0-9]" bit. */

    len = to_get = 7;
    gotten = 0;

    while ( gotten < len )
    { 
        if ( lecroy_vicp_read( len_str + gotten, &to_get, &with_eoi, UNSET )
                                                                   == FAILURE )
            lecroy_ws_lan_failure( );
        gotten += to_get;
        to_get = len - gotten;
    }

    len_str [ len ] = '\0';
    len = T_atol( len_str + 6 );

    fsc2_assert( len > 0 );

    /* Now get the number of bytes to read */

    to_get = len;
    gotten = 0;

    while ( gotten < len )
    { 
        if ( lecroy_vicp_read( len_str + gotten, &to_get, &with_eoi, UNSET )
                                                                   == FAILURE )
            lecroy_ws_lan_failure( );
        gotten += to_get;
        to_get = len - gotten;
    }

    len_str[ len ] = '\0';
    len = *length = T_atol( len_str );

    fsc2_assert( len > 0 );

    /* Obtain enough memory (one more byte than we need for the data because
       a '\n' gets attached at the end) */

    data = UCHAR_P T_malloc( ++len );

    /* Get the data, we need to loop since the device may send them in
       several chunks */

    to_get = len;
    gotten = 0;

    while ( gotten < len )
    { 
        if ( lecroy_vicp_read( data + gotten, &to_get, &with_eoi, UNSET )
                                                                   == FAILURE )
            lecroy_ws_lan_failure( );
        gotten += to_get;
        to_get = len - gotten;
    }

    return data;
}


/*----------------------------------------------------------------------*
 * Function for obtaining an integer value from the waveform descriptor
 *---------------------------------------------------------------------*/

static long lecroy_ws_get_int_value( int          ch,
                                     const char * name )
{
    char cmd[ 100 ];
    size_t len = 100;
    char *ptr = cmd;


    if ( ch >= LECROY_WS_CH1 && ch <= LECROY_WS_CH_MAX )
        sprintf( cmd, "C%d:INSP? '%s'\n", ch - LECROY_WS_CH1 + 1, name );
    else if ( ch == LECROY_WS_MATH )
        sprintf( cmd, "F1:INSP? '%s'\n", name );
    else if ( ch >= LECROY_WS_M1 && ch <= LECROY_WS_M4 )
        sprintf( cmd, "M%c:INSP? '%s'\n", ch - LECROY_WS_M1 + 1, name );
    else
        fsc2_impossible( );

    if ( lecroy_ws_talk( cmd, cmd, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    cmd[ len - 1 ] = '\0';

    while ( *ptr && *ptr++ != ':' )
        /* empty */ ;

    if ( ! *ptr )
        lecroy_ws_lan_failure( );

    return T_atol( ptr );
}


/*-------------------------------------------------------------------*
 * Function for obtaining a float value from the waveform descriptor
 *-------------------------------------------------------------------*/

static double lecroy_ws_get_float_value( int          ch,
                                         const char * name )
{
    char cmd[ 100 ];
    ssize_t len = 100;
    char *ptr = cmd;


    if ( ch >= LECROY_WS_CH1 && ch <= LECROY_WS_CH_MAX )
        sprintf( cmd, "C%d:INSP? '%s'\n", ch - LECROY_WS_CH1 + 1, name );
    else if ( ch == LECROY_WS_MATH )
        sprintf( cmd, "F1:INSP? '%s'\n", name );
    else if ( ch >= LECROY_WS_M1 && ch <= LECROY_WS_M4 )
        sprintf( cmd, "M%c:INSP? '%s'\n", ch - LECROY_WS_M1 + 1, name );
    else
        fsc2_impossible( );

    if ( lecroy_ws_talk( cmd, cmd, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    cmd[ len - 1 ] = '\0';

    while ( *ptr && *ptr++ != ':' )
        /* empty */ ;

    if ( ! *ptr )
        lecroy_ws_lan_failure( );

    return T_atod( ptr );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

void lecroy_ws_lock_state( bool lock_state )
{
    lecroy_vicp_lock_out( lock_state );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool lecroy_ws_command( const char * cmd )
{
    ssize_t len = strlen( cmd );


    lecroy_vicp_write( cmd, &len, SET, UNSET );
    return OK;
}


/*------------------------------------------------------------------------*
 * Function fetches (thereby reseting!) the INR register from the device.
 * It sets all bits in the INR element of the structure for the device
 * where a bit in the INR is set. Functions making use of the fact that
 * a bit is set must reset it when the action they take invalidate the
 * condition that led to the bit becoming set.
 *-----------------------------------------------------------------------*/

static unsigned int lecroy_ws_get_inr( void )
{
    char reply[ 10 ] = "INR?";
    ssize_t len = 10;


    if ( lecroy_ws_talk( "INR?\n", reply, &len ) != SUCCESS )
        lecroy_ws_lan_failure( );
    reply[ len - 1 ] = '\0';
    return ( unsigned int ) T_atoi( reply );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static int lecroy_ws_talk( const char * cmd,
                           char *       reply,
                           ssize_t *    length )
{
    ssize_t len = strlen( cmd );
    int ret;
    bool with_eoi;


    CLOBBER_PROTECT( ret );

    TRY
    {
        if ( lecroy_vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
            THROW( EXCEPTION );
        ret = lecroy_vicp_read( reply, length, &with_eoi, UNSET );
        TRY_SUCCESS;
    }
    OTHERWISE
        lecroy_ws_lan_failure( );

    return ret;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void lecroy_ws_lan_failure( void )
{
    lecroy_vicp_close( );
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
