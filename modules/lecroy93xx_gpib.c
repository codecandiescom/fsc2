/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
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


#include "lecroy93xx.h"


static unsigned char * lecroy93xx_get_data( long * len );

static unsigned int lecroy93xx_get_inr( void );

#if 0
static long lecroy93xx_get_int_value( int          ch,
                                     const char * name );
#endif

static double lecroy93xx_get_float_value( int          ch,
                                         const char * name );

static void lecroy93xx_get_prep( int              ch,
                                Window_T *       w,
                                unsigned char ** data,
                                long *           length,
                                double *         gain,
                                double *         offset );

static bool lecroy93xx_talk( const char * cmd,
                            char *       reply,
                            long *       length );

static void lecroy93xx_gpib_failure( void );


static unsigned int can_fetch = 0;



/*---------------------------------------------------------------*
 * Function called for initialization of device and to determine
 * its state
 *---------------------------------------------------------------*/

bool
lecroy93xx_init( const char * name )
{
    char buffer[ 100 ];
    long len = sizeof buffer;
    int i;


    if ( gpib_init_device( name, &lecroy93xx.device ) == FAILURE )
        return FAIL;

    /* Disable the local button, set digitizer to short form of replies,
       transmit data in one block in binary word (2 byte) format with LSB
       first. Then ask for the status byte to make sure the device reacts. */

    if (    gpib_write( lecroy93xx.device,
                        "CHDR OFF;CHLP OFF;CFMT DEF9,WORD,BIN;CORD LO", 44 )
                                                                     == FAILURE
         || gpib_write( lecroy93xx.device, "*STB?", 5 ) == FAILURE
         || gpib_read( lecroy93xx.device, buffer, &len ) == FAILURE )
    {
        gpib_local( lecroy93xx.device );
        return FAIL;
    }

    TRY
    {
        /* Figure out which traces are displayed (only 8 can be displayed
           at the same time and we must be able to check for this when
           the user asks for one more to be displayed) */

        lecroy93xx.num_used_channels = 0;

        for ( i = LECROY93XX_CH1; i <= LECROY93XX_CH_MAX; i++ )
        {
            lecroy93xx.is_displayed[ i ] = UNSET;
            if ( lecroy93xx_is_displayed( i ) )
            {
                lecroy93xx.is_displayed[ i ] = SET;
                lecroy93xx.num_used_channels++;
            }
        }

        for ( i = LECROY93XX_M1; i <= LECROY93XX_M4; i++ )
            lecroy93xx.is_displayed[ i ] = UNSET;

        for ( i = LECROY93XX_TA; i <= LECROY93XX_TD; i++ )
        {
            lecroy93xx.is_displayed[ i ] = UNSET;
            if ( lecroy93xx_is_displayed( i ) )
            {
                lecroy93xx.is_displayed[ i ] = SET;
                lecroy93xx.num_used_channels++;
            }
        }

        /* Make sure the internal timebase is used */

        if ( gpib_write( lecroy93xx.device, "SCLK INT", 8 ) == FAILURE )
            lecroy93xx_gpib_failure( );

        /* Set or get the timebase (including the index in the table of
           possible timebases) while also taking care of the mode, i.e.
           RIS or SS */

        if ( lecroy93xx.is_timebase )
            lecroy93xx_set_timebase( lecroy93xx.timebase );
        else
        {
            lecroy93xx.tb_index = -1;

            lecroy93xx.timebase = lecroy93xx_get_timebase( );

            for ( i = 0; i < lecroy93xx.num_tbas; i++ )
                if ( fabs( lecroy93xx.tbas[ i ] - lecroy93xx.timebase ) /
                     lecroy93xx.tbas[ i ] < 0.1 )
                {
                    lecroy93xx.tb_index = i;
                    break;
                }

            if ( lecroy93xx.tb_index < 0 )
            {
                print( FATAL, "Can't determine timebase.\n" );
                THROW( EXCEPTION );
            }
        }

        /* Either set the record length or query it */

        if ( lecroy93xx.is_mem_size )
            lecroy93xx_set_memory_size( lecroy93xx.mem_size );
        else
            lecroy93xx.mem_size = lecroy93xx_get_memory_size( );

        lecroy93xx.cur_hres =
                   lecroy93xx.hres[ lecroy93xx.ms_index ] + lecroy93xx.tb_index;

        /* Switch interleaved (RIS) mode on if the user asked for it and it
           can be done, otherwise switch it off */

        if (    lecroy93xx.is_interleaved
             && lecroy93xx.interleaved
             && lecroy93xx.cur_hres->cl_ris > 0 )
            lecroy93xx_set_interleaved( SET );

        if (    ( lecroy93xx.is_interleaved && ! lecroy93xx.interleaved )
             || lecroy93xx.cur_hres->cl_ris == 0 )
        {
            lecroy93xx_set_interleaved( UNSET );
            lecroy93xx.interleaved = UNSET;
        }

        /* Set (if required) the sensitivies, offsets coupling types and
           bandwidth limiters of all measurement channels */

        for ( i = LECROY93XX_CH1; i <= LECROY93XX_CH_MAX; i++ )
        {
            if ( lecroy93xx.is_sens[ i ] )
                lecroy93xx_set_sens( i, lecroy93xx.sens[ i ] );
            else
                lecroy93xx.sens[ i ] = lecroy93xx_get_sens( i );

            if ( lecroy93xx.is_offset[ i ] )
                lecroy93xx_set_offset( i, lecroy93xx.offset[ i ] );
            else
                lecroy93xx.offset[ i ] = lecroy93xx_get_offset( i );

            if ( lecroy93xx.is_coupling[ i ] )
                lecroy93xx_set_coupling( i, lecroy93xx.coupling[ i ] );
            else
                lecroy93xx.coupling[ i ] = lecroy93xx_get_coupling( i );

        }

        if ( lecroy93xx.is_bandwidth_limiter )
            lecroy93xx_set_bandwidth_limiter( lecroy93xx.bandwidth_limiter );
        else
            lecroy93xx.bandwidth_limiter = lecroy93xx_get_bandwidth_limiter( );

        /* Set (if required) the trigger source */

        if ( lecroy93xx.is_trigger_source )
            lecroy93xx_set_trigger_source( lecroy93xx.trigger_source );

        /* Set (if required) the trigger level, slope and coupling of the
           trigger channels */

        for ( i = 0; i < ( int ) NUM_ELEMS( trg_channels ); i++ )
        {
            if ( trg_channels[ i ] == LECROY93XX_LIN )
                continue;

            if ( lecroy93xx.is_trigger_level[ trg_channels[ i ] ] )
                lecroy93xx_set_trigger_level( trg_channels[ i ],
                                lecroy93xx.trigger_level[ trg_channels[ i ] ] );
            else
                lecroy93xx.trigger_level[ trg_channels[ i ] ] =
                              lecroy93xx_get_trigger_level( trg_channels[ i ] );

            if ( lecroy93xx.is_trigger_slope[ trg_channels[ i ] ] )
                lecroy93xx_set_trigger_slope( trg_channels[ i ],
                                lecroy93xx.trigger_slope[ trg_channels[ i ] ] );
            else
                lecroy93xx.trigger_slope[ trg_channels[ i ] ] =
                              lecroy93xx_get_trigger_slope( trg_channels[ i ] );

            if ( lecroy93xx.is_trigger_coupling[ trg_channels[ i ] ] )
                lecroy93xx_set_trigger_coupling( trg_channels[ i ],
                             lecroy93xx.trigger_coupling[ trg_channels[ i ] ] );
            else
                lecroy93xx.trigger_coupling[ trg_channels[ i ] ] =
                           lecroy93xx_get_trigger_coupling( trg_channels[ i ] );
        }

        /* Set (if required) the trigger delay */

        if ( lecroy93xx.is_trigger_delay )
            lecroy93xx_set_trigger_delay( lecroy93xx.trigger_delay );
        else
            lecroy93xx_get_trigger_delay( );

        /* Get or set the trigger mode */

        if ( lecroy93xx.is_trigger_mode )
            lecroy93xx_set_trigger_mode( lecroy93xx.trigger_mode );
        else
            lecroy93xx_get_trigger_mode( );

        /* Now that we know about the timebase and trigger delay settings
           we can also do the checks on the window and trigger delay settngs
           that may not have been possible during the test run */

        lecroy93xx_soe_checks( );
    }
    OTHERWISE
    {
        gpib_local( lecroy93xx.device );
        return FAIL;
    }

    return OK;
}


/*------------------------------------------------*
 * Function for determing the digitizers timebase
 *------------------------------------------------*/

double
lecroy93xx_get_timebase( void )
{
    char reply[ 30 ];
    long length = sizeof reply;
    double timebase;
    int i;


    lecroy93xx_talk( "TDIV?", reply, &length );
    reply[ length - 1 ] = '\0';
    timebase = T_atod( reply );

    for ( i = 0; i < lecroy93xx.num_tbas; i++ )
        if ( fabs( lecroy93xx.tbas[ i ] - timebase ) / timebase < 0.01 )
            break;

    if ( i == lecroy93xx.num_tbas )
    {
        print( FATAL, "Can't determine timebase.\n" );
        THROW( EXCEPTION );
    }

    lecroy93xx.timebase = timebase;
    lecroy93xx.tb_index = i;

    return lecroy93xx.timebase;
}


/*----------------------------------------------*
 * Function for setting the digitizers timebase
 *----------------------------------------------*/

bool
lecroy93xx_set_timebase( double timebase )
{
    char cmd[ 40 ];

    sprintf( cmd, "TDIV %.8g", timebase );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*------------------------------------------------------------*
 * Function for determining if digitizer is in RIS or SS mode
 *------------------------------------------------------------*/

bool
lecroy93xx_get_interleaved( void )
{
    char reply[ 30 ];
    long length = sizeof reply;


    lecroy93xx_talk( "ILVD?", reply, &length );
    lecroy93xx.interleaved = reply[ 1 ] == 'N';

    return lecroy93xx.interleaved;
}


/*------------------------------------------------*
 * Function for switching between RIS and SS mode
 *------------------------------------------------*/

bool
lecroy93xx_set_interleaved( bool state )
{
    char cmd[ 30 ] = "ILVD ";

    strcat( cmd, state ? "ON" : "OFF" );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*----------------------------------------------------------*
 * Function for determining the memory size currently used.
 *----------------------------------------------------------*/

long
lecroy93xx_get_memory_size( void )
{
    char reply[ 30 ];
    long length = sizeof reply;
    long mem_size;
    long i;
    char *end_p;


    lecroy93xx_talk( "MSIZ?", reply, &length );
    reply[ length - 1 ] = '\0';

    mem_size = strtol( reply, &end_p, 10 );

    if ( errno == ERANGE )
    {
        print( FATAL, "Long integer number out of range: %s.\n", reply );
        THROW( EXCEPTION );
    }

    if ( end_p == ( char * ) reply )
    {
        print( FATAL, "Not an integer number: %s.\n", reply );
        THROW( EXCEPTION );
    }

    if ( *end_p == 'K' )
        mem_size *= 1000;
    else if ( *end_p == 'M' )
        mem_size *= 1000000;

    for ( i = 0; i < lecroy93xx.num_mem_sizes; i++ )
        if ( lecroy93xx.mem_sizes[ i ] == mem_size )
            break;

    if ( i == lecroy93xx.num_mem_sizes )
    {
        print( FATAL, "Can't determine memory size.\n" );
        THROW( EXCEPTION );
    }

    lecroy93xx.mem_size = mem_size;
    lecroy93xx.ms_index = i;

    return mem_size;
}


/*--------------------------------------*
 * Function for setting the memory size
 *--------------------------------------*/

bool
lecroy93xx_set_memory_size( long mem_size )
{
    char cmd[ 30 ];


    sprintf( cmd, "MSIZ %ld", mem_size );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*------------------------------------------------------------------*
 * Function for determining the sensitivity (in V/div) of a channel
 *------------------------------------------------------------------*/

double
lecroy93xx_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = sizeof reply;


    fsc2_assert( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX );

    sprintf( cmd, "C%1d:VDIV?", channel + 1 );
    lecroy93xx_talk( cmd, reply, &length );
    reply[ length - 1 ] = '\0';
    return lecroy93xx.sens[ channel ] = T_atod( reply );
}


/*--------------------------------------------------------------*
 * Function for setting the sensitivity (in V/div) of a channel
 *--------------------------------------------------------------*/

bool
lecroy93xx_set_sens( int    channel,
                    double sens )
{
    char cmd[ 40 ];

    fsc2_assert( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX );

    sprintf( cmd, "C%1d:VDIV %.8g", channel + 1, sens );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*-----------------------------------------------------------*
 * Function for determining the offset voltage for a channel
 *-----------------------------------------------------------*/

double
lecroy93xx_get_offset( int channel )
{
    char buf[ 30 ];
    long length = sizeof buf;


    fsc2_assert( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX );

    sprintf( buf, "C%1d:OFST?", channel + 1 );
    lecroy93xx_talk( buf, buf, &length );
    buf[ length - 1 ] = '\0';
    return  lecroy93xx.offset[ channel ] = T_atod( buf );
}


/*-------------------------------------------------------*
 * Function for setting the offset voltage for a channel
 *-------------------------------------------------------*/

bool
lecroy93xx_set_offset( int    channel,
                      double offset )
{
    char cmd[ 40 ];


    fsc2_assert( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX );

    sprintf( cmd, "C%1d:OFST %.8g", channel + 1, offset );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*----------------------------------------------------------*
 * Function for determining the coupling type for a channel
 *----------------------------------------------------------*/

int
lecroy93xx_get_coupling( int channel )
{
    int type = LECROY93XX_CPL_INVALID;
    char buf[ 100 ];
    long length = sizeof buf;


    fsc2_assert( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX );

    sprintf( buf, "C%1d:CPL?", channel + 1 );
    lecroy93xx_talk( buf, buf, &length );
    buf[ length - 1 ] = '\0';

    if ( buf[ 0 ] == 'A' )
        type = LECROY93XX_CPL_AC_1_MOHM;
    else if ( buf[ 0 ] == 'G' )
        type = LECROY93XX_CPL_GND;
    else if ( buf[ 0 ] == 'D' && buf[ 1 ] == '1' )
        type = LECROY93XX_CPL_DC_1_MOHM;
    else if ( buf[ 0 ] == 'D' && buf[ 1 ] == '5' )
        type = LECROY93XX_CPL_DC_50_OHM;
    else if ( buf[ 0 ] == 'O' )          /* overload with 50 ohm DC coupling */
    {
        type = LECROY93XX_CPL_DC_50_OHM;
        print( SEVERE, "Signal overload detected for channel '%s', input "
               "automatically disconnected.\n",
               LECROY93XX_Channel_Names[ channel ] );
    }
    else
        fsc2_impossible( );

    return lecroy93xx.coupling[ channel ] = type;
}


/*------------------------------------------------------*
 * Function for setting the coupling type for a channel
 *------------------------------------------------------*/

bool
lecroy93xx_set_coupling( int channel,
                        int type )
{
    char cmd[ 30 ];
    char const *cpl[ ] = { "A1M", "D1M", "D50", "GND" };


    fsc2_assert( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX );
    fsc2_assert(    type >= LECROY93XX_CPL_AC_1_MOHM
                 && type <= LECROY93XX_CPL_GND );

    sprintf( cmd, "C%1d:CPL %s", channel + 1, cpl[ type ] );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*------------------------------------------------*
 * Function for determining the bandwidth limiter
 *------------------------------------------------*/

int
lecroy93xx_get_bandwidth_limiter( void )
{
    char buf[ 30 ];
    long length = sizeof buf;


    lecroy93xx_talk( "BWL?", buf, &length );
    if ( buf[ 1 ] == 'F' )           /* OFF */
        lecroy93xx.bandwidth_limiter = LECROY93XX_BWL_OFF;
    else if ( buf[ 1 ] == 'N' )      /* ON */
        lecroy93xx.bandwidth_limiter = LECROY93XX_BWL_ON;
    else if ( buf[ 0 ] == '2' )      /* 200MHZ */
        lecroy93xx.bandwidth_limiter = LECROY93XX_BWL_200MHZ;
    else
        fsc2_impossible( );

    return lecroy93xx.bandwidth_limiter;
}


/*--------------------------------------------------------------------*
 * Function for setting the bandwidth limiter for one of the channels
 *--------------------------------------------------------------------*/

bool
lecroy93xx_set_bandwidth_limiter( int bwl )
{
    char buf[ 50 ] = "BWL ";


    fsc2_assert( bwl >= LECROY93XX_BWL_OFF && bwl <= LECROY93XX_BWL_200MHZ );

    if ( bwl == LECROY93XX_BWL_OFF )
        strcat( buf, "OFF" );
    else if ( bwl == LECROY93XX_BWL_ON )
        strcat( buf, "ON" );
    else
        strcat( buf, "200MHZ" );

    if ( gpib_write( lecroy93xx.device, buf, strlen( buf ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*-------------------------------------------------------------*
 * Function for finding out the current trigger source channel
 *-------------------------------------------------------------*/

int
lecroy93xx_get_trigger_source( void )
{
    char reply[ 100 ];
    long length = sizeof reply;
    int src = LECROY93XX_UNDEF;
    char *ptr = reply + 7;


    lecroy93xx_talk( "TRSE?", reply, &length );
    reply[ length - 1 ] = '\0';

    if (    strncmp( reply, "STD,SR,", 7 )
         && strncmp( reply, "EDGE,SR,", 8 ) )
    {
        print( SEVERE, "Non-standard mode trigger, switching to standard "
               "edge trigger on to LINe input\n" );
        return lecroy93xx_set_trigger_source( LECROY93XX_LIN );
    }

    if ( *ptr == ',' )
        ptr++;

    if ( *ptr == 'C' )
        sscanf( ++ptr, "%d", &src );
    else if ( *ptr == 'L' )
        src = LECROY93XX_LIN;
    else if ( *ptr == 'E' && ptr[ 2 ] == '1' )
        src = LECROY93XX_EXT10;
    else if ( *ptr == 'E' && ptr[ 2 ] != '1' )
        src = LECROY93XX_EXT;
    else
        fsc2_impossible( );

    return lecroy93xx.trigger_source = src;
}


/*---------------------------------------------------------*
 * Function for setting the current trigger source channel
 *---------------------------------------------------------*/

bool
lecroy93xx_set_trigger_source( int channel )
{
    char cmd[ 40 ] = "TRSE STD,SR,";


    fsc2_assert(    (    channel >= LECROY93XX_CH1
                      && channel <= LECROY93XX_CH_MAX )
                 || channel == LECROY93XX_LIN
                 || channel == LECROY93XX_EXT
                 || channel == LECROY93XX_EXT10 );

    if ( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX )
        sprintf( cmd + 11, "C%1d", channel + 1 );
    else if ( channel == LECROY93XX_LIN )
        strcat( cmd, "LINE" );
    else if ( channel == LECROY93XX_EXT )
        strcat( cmd, "EX" );
    else
        strcat( cmd, "EX10" );

    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*-------------------------------------------------------------------*
 * Function for finding out the trigger level of one of the channels
 *-------------------------------------------------------------------*/

double
lecroy93xx_get_trigger_level( int channel )
{
    char buf[ 30 ];
    long length = sizeof buf;


    fsc2_assert(    (    channel >= LECROY93XX_CH1
                      && channel <= LECROY93XX_CH_MAX )
                 || channel == LECROY93XX_EXT
                 || channel == LECROY93XX_EXT10 );

    if ( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX )
        sprintf( buf, "C%1d:TRLV?", channel + 1 );
    else if ( channel == LECROY93XX_EXT )
        strcpy( buf, "EX:TRLV?" );
    else
        strcpy( buf, "EX10:TRLV?" );

    lecroy93xx_talk( buf, buf, &length );
    buf[ length - 1 ] = '\0';
    return lecroy93xx.trigger_level[ channel ] = T_atod( buf );
}


/*----------------------------------------------------------------*
 * Function for setting the trigger slope for one of the channels
 *----------------------------------------------------------------*/

bool
lecroy93xx_set_trigger_level( int    channel,
                             double level )
{
    char cmd[ 40 ];


    fsc2_assert(    (    channel >= LECROY93XX_CH1
                      && channel <= LECROY93XX_CH_MAX )
                 || channel == LECROY93XX_EXT
                 || channel == LECROY93XX_EXT10 );

    if ( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX )
        sprintf( cmd, "C%1d:TRLV ", channel + 1 );
    else if ( channel == LECROY93XX_EXT )
        strcpy( cmd, "EX:TRLV " );
    else
        strcpy( cmd, "EX10:TRLV " );

    sprintf( cmd + strlen( cmd ), "%.6g", level );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*-------------------------------------------------------------------*
 * Function for finding out the trigger slope of one of the channels
 *-------------------------------------------------------------------*/

bool
lecroy93xx_get_trigger_slope( int channel )
{
    char buf[ 30 ];
    long length = sizeof buf;


    fsc2_assert(    (    channel >= LECROY93XX_CH1
                      && channel <= LECROY93XX_CH_MAX )
                 || channel == LECROY93XX_LIN
                 || channel == LECROY93XX_EXT
                 || channel == LECROY93XX_EXT10 );

    if ( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX )
        sprintf( buf, "C%1d:TRSL?", channel + 1 );
    else if ( channel == LECROY93XX_LIN )
        strcpy( buf, "LINE:TRSL?" );
    else if ( channel == LECROY93XX_EXT )
        strcpy( buf, "EX:TRSL?" );
    else
        strcpy( buf, "EX10:TRSL?" );

    lecroy93xx_talk( ( const char *) buf, buf, &length );

    lecroy93xx.trigger_slope[ channel ] =
                                         buf[ 0 ] == 'P' ? POSITIVE : NEGATIVE;

    return lecroy93xx.trigger_slope[ channel ];
}


/*----------------------------------------------------------------*
 * Function for setting the trigger slope for one of the channels
 *----------------------------------------------------------------*/

bool
lecroy93xx_set_trigger_slope( int channel,
                             bool slope )
{
    char cmd[ 40 ];


    fsc2_assert(    (    channel >= LECROY93XX_CH1
                      && channel <= LECROY93XX_CH_MAX )
                 || channel == LECROY93XX_LIN
                 || channel == LECROY93XX_EXT
                 || channel == LECROY93XX_EXT10 );

    if ( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX )
        sprintf( cmd, "C%1d:TRSL ", channel + 1 );
    else if ( channel == LECROY93XX_LIN )
        strcpy( cmd, "LINE:TRSL " );
    else if ( channel == LECROY93XX_EXT )
        strcpy( cmd, "EX:TRSL " );
    else
        strcpy( cmd, "EX10:TRSL " );

    strcat( cmd, slope ? "POS" : "NEG" );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*-----------------------------------------------------------------------*
 * Function for finding out the trigger coupling for one of the channels
 *-----------------------------------------------------------------------*/

int
lecroy93xx_get_trigger_coupling( int channel )
{
    char buf[ 40 ];
    long length = sizeof buf;
    int cpl = -1;


    fsc2_assert(    (    channel >= LECROY93XX_CH1
                      && channel <= LECROY93XX_CH_MAX )
                 || channel == LECROY93XX_EXT
                 || channel == LECROY93XX_EXT10 );

    if ( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX )
        sprintf( buf, "C%1d:TRCP?", channel + 1 );
    else if ( channel == LECROY93XX_EXT )
        strcpy( buf, "EX:TRCP?" );
    else
        strcpy( buf, "EX10:TRCP?" );

    lecroy93xx_talk( ( const char *) buf, buf, &length );

    switch ( buf[ 0 ] )
    {
        case 'A' :
            cpl = LECROY93XX_TRG_CPL_AC;
            break;

        case 'D' :
            cpl = LECROY93XX_TRG_CPL_DC;
            break;

        case 'L' :
            cpl = LECROY93XX_TRG_CPL_LF_REJ;
            break;

        case 'H' :
            cpl = LECROY93XX_TRG_CPL_HF_REJ;
            break;

        default :
            fsc2_impossible( );
    }

    lecroy93xx.trigger_coupling[ channel ] = cpl;

    return cpl;
}


/*-------------------------------------------------------------------*
 * Function for setting the trigger coupling for one of the channels
 *-------------------------------------------------------------------*/

int
lecroy93xx_set_trigger_coupling( int channel,
                                int cpl )
{
    char cmd[ 40 ];
    const char *cpl_str[ ] = { "AC", "DC", "LFREJ", "HFREJ" };


    fsc2_assert(    (    channel >= LECROY93XX_CH1
                      && channel <= LECROY93XX_CH_MAX )
                 || channel == LECROY93XX_EXT
                 || channel == LECROY93XX_EXT10 );
    fsc2_assert( cpl >= LECROY93XX_TRG_AC && cpl <= LECROY93XX_TRG_HF_REJ );

    if ( channel >= LECROY93XX_CH1 && channel <= LECROY93XX_CH_MAX )
        sprintf( cmd, "C%1d:TRCP ", channel + 1 );
    else if ( channel == LECROY93XX_EXT )
        strcpy( cmd, "EX:TRCP " );
    else
        strcpy( cmd, "EX10:TRCP " );

    strcat( cmd, cpl_str[ cpl ] );

    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*---------------------------------------------------*
 * Function for finding out the current trigger mode
 *---------------------------------------------------*/

int
lecroy93xx_get_trigger_mode( void )
{
    char buf[ 40 ];
    long length = sizeof buf;
    int mode = -1;


    lecroy93xx_talk( "TRMD?", buf, &length );

    if ( buf[ 0 ] == 'A' )
        mode = LECROY93XX_TRG_MODE_AUTO;
    else if ( buf[ 0 ] == 'N' )
        mode = LECROY93XX_TRG_MODE_NORMAL;
    else if ( buf[ 1 ] == 'S' )
        mode = LECROY93XX_TRG_MODE_SINGLE;
    else if ( buf[ 1 ] == 'T' )
        mode = LECROY93XX_TRG_MODE_STOP;
    else
        fsc2_impossible( );

    return lecroy93xx.trigger_mode = mode;
}


/*---------------------------------------*
 * Function for setting the trigger mode
 *---------------------------------------*/

int
lecroy93xx_set_trigger_mode( int mode )
{
    char cmd[ 40 ] = "TRMD ";
    const char *mode_str[ ] = { "AUTO", "NORM", "SINGLE", "STOP" };


    fsc2_assert(    mode >= LECROY93XX_TRG_MODE_AUTO
                 && mode <= LECROY93XX_TRG_MODE_STOP );

    strcat( cmd, mode_str[ mode ] );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*----------------------------------------------------*
 * Function for determining the current trigger delay
 *----------------------------------------------------*/

double
lecroy93xx_get_trigger_delay( void )
{
    char reply[ 40 ];
    long length = sizeof reply;


    lecroy93xx_talk( "TRDL?", reply, &length );
    reply[ length - 1 ] = '\0';
    lecroy93xx.trigger_delay = T_atod( reply );

    /* Positive delays (i.e. when pretrigger is on) get returned from the
       device as a percentage of the full horizontal screen while for
       negative values it's already the delay time */

    if ( lecroy93xx.trigger_delay > 0.0 )
        lecroy93xx.trigger_delay *= 0.1 * lecroy93xx.timebase;

    return lecroy93xx.trigger_delay;
}


/*----------------------------------------*
 * Function for setting the trigger delay
 *----------------------------------------*/

bool
lecroy93xx_set_trigger_delay( double delay )
{
    char cmd[ 40 ];

    /* For positive delay (i.e. pretrigger) the delay must be set as a
       percentage of the full horizontal screen width */

    if ( delay > 0.0 )
        delay = 10.0 * delay / lecroy93xx.timebase;

    sprintf( cmd, "TRDL %.8g", delay );
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    return OK;
}


/*---------------------------------------------------------*
 * Function for checking if a certain channel is displayed
 *---------------------------------------------------------*/

bool
lecroy93xx_is_displayed( int ch )
{
    char cmd[ 30 ];
    long length = sizeof cmd;


    if ( ch >= LECROY93XX_CH1 && ch <= LECROY93XX_CH_MAX )
        sprintf( cmd, "C%d:TRA?", ch - LECROY93XX_CH1 + 1 );
    else if ( ch >= LECROY93XX_TA && ch <= LECROY93XX_TD )
        sprintf( cmd, "T%c:TRA?", ch - LECROY93XX_TA + 'A' );
    else if ( ch >= LECROY93XX_M1 && ch <= LECROY93XX_M4 )
    {
        print( FATAL, "A memory channel can't be displayed.\n");
        THROW( EXCEPTION );
    }
    else
    {
        print( FATAL, "Internal error detected.\n" );
        THROW( EXCEPTION );
    }

    lecroy93xx_talk( cmd, cmd, &length );
    return cmd[ 1 ] == 'N';
}


/*----------------------------------------------------*
 * Function to switch  display of a channel on or off
 *----------------------------------------------------*/

bool
lecroy93xx_display( int ch,
                   int on_off )
{
    char cmd[ 30 ];


    if ( ch >= LECROY93XX_CH1 && ch <= LECROY93XX_CH_MAX )
        sprintf( cmd, "C%d:TRA ", ch - LECROY93XX_CH1 + 1 );
    else if ( ch >= LECROY93XX_TA && ch <= LECROY93XX_TD )
        sprintf( cmd, "T%c:TRA ", ch - LECROY93XX_TA + 'A' );
    else if ( ch >= LECROY93XX_M1 && ch <= LECROY93XX_M4 )
    {
        print( FATAL, "Memory channels can't be displayed.\n" );
        THROW( EXCEPTION );
    }
    else
    {
        print( FATAL, "Internal error detected.\n" );
        THROW( EXCEPTION );
    }

    strcat( cmd, on_off ? "ON" : "OFF" );

    if (    on_off
         && lecroy93xx.num_used_channels >= LECROY93XX_MAX_USED_CHANNELS )
    {
        print( FATAL, "Can't switch on another trace, there are already as "
               "many as possible (%d) displayed.\n",
               LECROY93XX_MAX_USED_CHANNELS );
        THROW( EXCEPTION );
    }

    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    if ( on_off )
    {
        lecroy93xx.num_used_channels++;
        lecroy93xx.channels_in_use[ ch ] = SET;
    }
    else
    {
        lecroy93xx.num_used_channels--;
        lecroy93xx.channels_in_use[ ch ] = UNSET;
    }

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy93xx_finished( void )
{
    gpib_local( lecroy93xx.device );
}


/*-----------------------------------------*
 * Function for starting a new acquisition
 *-----------------------------------------*/

void
lecroy93xx_start_acquisition( void )
{
    int ch;
    char cmd[ 100 ];
    bool do_averaging = UNSET;


    /* Stop the digitizer (also switches to "STOPPED" trigger mode) */

    if ( gpib_write( lecroy93xx.device, "STOP", 4 ) == FAILURE )
        lecroy93xx_gpib_failure( );

    /* Set up the parameter to be used for averaging for the function channels
       (as far as they have been set by the user) */

    for ( ch = LECROY93XX_TA; ch <= LECROY93XX_TD; ch++ )
    {
        if ( ! lecroy93xx.is_avg_setup[ ch ] )
            continue;

        do_averaging = SET;

        snprintf( cmd, 100, "T%c:DEF EQN,'AVGS(C%ld)',MAXPTS,%ld,SWEEPS,%ld",
                  'A' + LECROY93XX_TA - ch,
                  lecroy93xx.source_ch[ ch ] - LECROY93XX_CH1 + 1,
                  lecroy93xx_curve_length( ),
                  lecroy93xx.num_avg[ ch ] );

        if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) )
             == FAILURE )
            lecroy93xx_gpib_failure( );

        /* If we want to use a trace it must be switched on (but not the
           channel that gets averaged) */

        if ( ! lecroy93xx_is_displayed( ch ) )
            lecroy93xx_display( ch, SET );

        /* Switch off horizontal zoom and shift - if it's on the curve fetched
           from the device isn't what one would expect... */

        sprintf( cmd, "T%c:HMAG 1;T%c:HPOS 5", 'A' + LECROY93XX_TA - ch,
                 'A' + LECROY93XX_TA - ch ) ;

        if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
            lecroy93xx_gpib_failure( );

        /* Finally reset what's currently stored in the trace (otherwise a
           new acquisition may not get started) */

        sprintf( cmd, "T%c:FRST", 'A' + LECROY93XX_TA - ch );

        if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
            lecroy93xx_gpib_failure( );
    }

    /* Reset the bits in the word that tells us later that the data in the
       corresponding channel are ready to be fetched */

    lecroy93xx_get_inr( );

    can_fetch &= ~ ( LECROY93XX_PROC_DONE_TA | LECROY93XX_PROC_DONE_TB |
                     LECROY93XX_PROC_DONE_TC | LECROY93XX_PROC_DONE_TD |
                     LECROY93XX_SIGNAL_ACQ );

    /* Switch digitizer back on to running state by switching to a trigger
       mode where the digitizer is running (i.e. typically NORMAL, but, if
       the user requested it, also AUTO, or, if there's no averaging setup,
       even SINGLE mode will do) */

    strcpy( cmd, "TRMD NORM" );
    if (    ! do_averaging
         && lecroy93xx.trigger_mode == LECROY93XX_TRG_MODE_SINGLE )
        strcpy( cmd + 5, "SINGLE" );
    else if ( lecroy93xx.trigger_mode == LECROY93XX_TRG_MODE_AUTO )
        strcpy( cmd + 5, "AUTO" );
    else
        lecroy93xx.trigger_mode = LECROY93XX_TRG_MODE_NORMAL;

    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

}


/*------------------------------------------------------------*
 * Function for fetching data from the digitizer - the calling
 * function then does the remaining specific manipulations on
 * these data
 *------------------------------------------------------------*/

static void
lecroy93xx_get_prep( int              ch,
                    Window_T *       w,
                    unsigned char ** volatile data,
                    long *           length,
                    double *         gain,
                    double *         offset )
{
    unsigned int volatile bit_to_test = 0;
    char cmd[ 100 ];
    char ch_str[ 3 ];
    bool is_mem_ch = UNSET;


    /* Figure out which channel is to be used and set a few variables
       needed later accordingly */

    if ( ch >= LECROY93XX_CH1 && ch <= LECROY93XX_CH_MAX )
    {
        bit_to_test = LECROY93XX_SIGNAL_ACQ;
        sprintf( ch_str, "C%d", ch - LECROY93XX_CH1 + 1 );
    }
    else if ( ch >= LECROY93XX_M1 && ch <= LECROY93XX_M4 )
    {
        is_mem_ch = SET;
        sprintf( ch_str, "M%d", ch - LECROY93XX_M1 + 1 );
    }
    else if ( ch >= LECROY93XX_TA && ch <= LECROY93XX_TD )
    {
        bit_to_test = LECROY93XX_PROC_DONE( ch );
        sprintf( ch_str, "T%c", ch - LECROY93XX_TA + 'A' );
    }
    else
        fsc2_impossible( );

#if 0
    /* We probably have to check if two or more channels are combined - I
       found no way this can be checked via the program and we can only look
       for the number of points and compare that with what we expect. To make
       things a bit more interesting, the device always seems to send us 2
       more points than it should and I don't know if that becomes 4 when two
       curves are combined ... */

    /* Get the number of byztes of the curve */

    len = lecroy93xx_get_int_value( ch, "WAVE_ARRAY_1" ) / 2;
#endif

    /* Set up the number of points to be fetched - take care: the device
       always measures an extra point before and after the displayed region,
       thus we start with one point later than we could get from it.*/

    if ( w != NULL )
        sprintf( cmd, "WFSU SP,0,NP,%ld,FP,%ld,SN,0",
                 w->num_points, w->start_num + 1 );
    else
        sprintf( cmd, "WFSU SP,0,NP,%ld,FP,1,SN,0",
                 lecroy93xx_curve_length( ) );

    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );

    /* When a non-memory curve is to be fetched and the acquisition isn't
       finished yet poll until the bit that tells that the acquisition for
       the requested channel is finished has become set */

    if (    ! is_mem_ch
         && ! ( can_fetch & bit_to_test ) )
        while ( ! ( ( can_fetch |= lecroy93xx_get_inr( ) ) & bit_to_test ) )
        {
            stop_on_user_request( );
            fsc2_usleep( 20000, UNSET );
        }

    TRY
    {
        /* Ask the device for the data... */

        strcpy( cmd, ch_str );
        strcat( cmd, ":WF? DAT1" );
        if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) )
             == FAILURE )
            lecroy93xx_gpib_failure( );

        /* ...and fetch 'em */

        *data = lecroy93xx_get_data( length );
        *length /= 2;          /* we got word sized (16 bit) data, LSB first */

        /* Get the gain factor and offset for the date we just fetched */

        *gain = lecroy93xx_get_float_value( ch, "VERTICAL_GAIN" );
        *offset = lecroy93xx_get_float_value( ch, "VERTICAL_OFFSET" );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( *data != NULL )
            T_free( *data );
        RETHROW;
    }
}


/*-----------------------------------------------------*
 * Function for fetching a curve from the oscilloscope
 *-----------------------------------------------------*/

void
lecroy93xx_get_curve( int        ch,
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

    lecroy93xx_get_prep( ch, w, &data, length, &gain, &offset );

    /* Calculate the voltages from the data, data are two byte (LSB first),
       two's complement integers, which then need to be scaled by gain and
       offset. */

    *array = T_malloc( *length * sizeof **array );

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

double
lecroy93xx_get_area( int        ch,
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

    lecroy93xx_get_prep( ch, w, &data, &length, &gain, &offset );

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

double
lecroy93xx_get_amplitude( int        ch,
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

    lecroy93xx_get_prep( ch, w, &data, &length, &gain, &offset );

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

void
lecroy93xx_copy_curve( long src,
                      long dest )
{
    char cmd[ 100 ] = "STO ";


    fsc2_assert(    ( src >= LECROY93XX_CH1 && src <= LECROY93XX_CH_MAX )
                 || ( src >= LECROY93XX_TA && src <= LECROY93XX_TD ) );
    fsc2_assert( dest >= LECROY93XX_M1 && dest <= LECROY93XX_M4 );


    if ( src >= LECROY93XX_CH1 && src <= LECROY93XX_CH_MAX )
        sprintf( cmd + strlen( cmd ), "C%ld,", src - LECROY93XX_CH1 + 1 );
    else
        sprintf( cmd + strlen( cmd ), "T%c,",
                 ( char ) ( src - LECROY93XX_TA + 'A' ) );

    sprintf( cmd + strlen( cmd ), "M%ld", dest - LECROY93XX_M1 + 1 );

    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );
}


/*----------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static unsigned char *
lecroy93xx_get_data( long * len )
{
    unsigned char *data;
    char len_str[ 10 ];
#if defined EXTRA_BYTE_AFTER_WAVEFORM
    long one = 1;
#endif


    /* First thing we read is something like "DAT1,#[0-9]" where the number
       following the '#' is the number of bytes to be read next */

    *len = 7;
    if ( gpib_read( lecroy93xx.device, len_str, len ) == FAILURE )
        lecroy93xx_gpib_failure( );

    len_str [ *len ] = '\0';
    *len = T_atol( len_str + 6 );

    fsc2_assert( *len > 0 );

    /* Now get the number of bytes to read */

    if ( gpib_read( lecroy93xx.device, len_str, len ) == FAILURE )
        lecroy93xx_gpib_failure( );

    len_str[ *len ] = '\0';
    *len = T_atol( len_str );

    fsc2_assert( *len > 0 );

    /* Obtain enough memory and then read the real data. Afterwards we
       need to read in one more extra byte the device sends. */

    data = T_malloc( *len );

    if ( gpib_read( lecroy93xx.device, ( char * ) data, len ) == FAILURE )
        lecroy93xx_gpib_failure( );

    /* Note: some models of this series send an extra byte after the
       waveform and we need to read this also - set the corresponding
       macro in the configuration file for the device! */

#if defined EXTRA_BYTE_AFTER_WAVEFORM
    if ( gpib_read( lecroy93xx.device, len_str, &one ) == FAILURE )
        lecroy93xx_gpib_failure( );
#endif

    return data;
}


/*----------------------------------------------------------------------*
 * Function for obtaining an integer value from the waveform descriptor
 *----------------------------------------------------------------------*/

#if 0
static long
lecroy93xx_get_int_value( int          ch,
                         const char * name )
{
    char cmd[ 100 ];
    long length = sizeof cmd;
    char * volatile ptr = cmd;
    long volatile val = 0;


    if ( ch >= LECROY93XX_CH1 && ch <= LECROY93XX_CH_MAX )
        sprintf( cmd, "C%d:INSP? '%s'", ch - LECROY93XX_CH1 + 1, name );
    else if ( ch >= LECROY93XX_M1 && ch <= LECROY93XX_M4 )
        sprintf( cmd, "M%c:INSP? '%s'", ch - LECROY93XX_M1 + 1, name );
    else if ( ch >= LECROY93XX_TA && ch <= LECROY93XX_TD )
        sprintf( cmd, "T%c:INSP? '%s'", ch - LECROY93XX_TA + 'A', name );
    else
        fsc2_impossible( );

    lecroy93xx_talk( cmd, cmd, &length );
    cmd[ length - 1 ] = '\0';

    while ( *ptr && *ptr++ != ':' )
        /* empty */ ;

    if ( ! *ptr )
        lecroy93xx_gpib_failure( );

    TRY
    {
        val = T_atol( ptr );
        TRY_SUCCESS;
    }
    OTHERWISE
        lecroy93xx_gpib_failure( );

    return val;
}
#endif


/*-------------------------------------------------------------------*
 * Function for obtaining a float value from the waveform descriptor
 *-------------------------------------------------------------------*/

static double
lecroy93xx_get_float_value( int          ch,
                            const char * name )
{
    char cmd[ 100 ];
    long length = sizeof cmd;

    if ( ch >= LECROY93XX_CH1 && ch <= LECROY93XX_CH_MAX )
        sprintf( cmd, "C%d:INSP? '%s'", ch - LECROY93XX_CH1 + 1, name );
    else if ( ch >= LECROY93XX_M1 && ch <= LECROY93XX_M4 )
        sprintf( cmd, "M%c:INSP? '%s'", ch - LECROY93XX_M1 + 1, name );
    else if ( ch >= LECROY93XX_TA && ch <= LECROY93XX_TD )
        sprintf( cmd, "T%c:INSP? '%s'", ch - LECROY93XX_TA + 'A', name );
    else
        fsc2_impossible( );

    lecroy93xx_talk( cmd, cmd, &length );
    cmd[ length - 1 ] = '\0';

    char * volatile ptr = strchr( cmd, ':' );
    if ( ! ptr || ! *++ptr )
        lecroy93xx_gpib_failure( );

    double volatile val = 0.0;
    TRY
    {
        val = T_atod( ptr );
        TRY_SUCCESS;
    }
    OTHERWISE
        lecroy93xx_gpib_failure( );

    return val;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool
lecroy93xx_command( const char * cmd )
{
    if ( gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE )
        lecroy93xx_gpib_failure( );
    return OK;
}


/*------------------------------------------------------------------------*
 * Function fetches (thereby reseting!) the INR register from the device.
 * It sets all bits in the INR element of the structure for the device
 * where a bit in the INR is set. Functions making use of the fact that
 * a bit is set must reset it when the action they take invalidates the
 * condition that led to the bit becoming set.
 *-----------------------------------------------------------------------*/

static unsigned int
lecroy93xx_get_inr( void )
{
    char reply[ 10 ];
    long length = sizeof reply;


    lecroy93xx_talk( "INR?", reply, &length );
    reply[ length - 1 ] = '\0';
    return ( unsigned int ) T_atoi( reply );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool lecroy93xx_talk( const char * cmd,
                            char *       reply,
                            long *       length )
{
    if (    gpib_write( lecroy93xx.device, cmd, strlen( cmd ) ) == FAILURE
         || gpib_read( lecroy93xx.device, reply, length ) == FAILURE )
        lecroy93xx_gpib_failure( );
    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lecroy93xx_gpib_failure( void )
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
