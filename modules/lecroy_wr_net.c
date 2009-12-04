/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#include "lecroy_wr_l.h"
#include "vicp.h"


static unsigned char * lecroy_wr_get_data( long * len );

static unsigned int lecroy_wr_get_inr( void );

#if 0
static long lecroy_wr_get_int_value( int          ch,
                                     const char * name );
#endif

static double lecroy_wr_get_float_value( int          ch,
                                         const char * name );

static void lecroy_wr_get_prep( int              ch,
                                Window_T       * w,
                                unsigned char ** data,
                                long           * length,
                                double         * gain,
                                double         * offset );

static int lecroy_wr_talk( const char * cmd,
						   char       * reply,
						   ssize_t    * length );

static void lecroy_wr_lan_failure( void );


static unsigned int can_fetch = 0;



/*---------------------------------------------------------------*
 * Function called for initialization of device and to determine
 * its state
 *---------------------------------------------------------------*/

bool
lecroy_wr_init( const char * name )
{
    char buf[ 100 ];
    ssize_t len = sizeof buf;
    int i;
    bool with_eoi;


    /* Open a socket to the device */

    vicp_open( name, NETWORK_ADDRESS, 100000, 1 );

    vicp_set_timeout( READ, READ_TIMEOUT );
    vicp_set_timeout( WRITE, WRITE_TIMEOUT );

	TRY
	{
		/* Disable the local button, set digitizer to short form of replies,
		   transmit data in one block in binary word (2 byte) format with LSB
		   first. Then ask for the status byte to make sure the device
		   reacts. */

		len = 51;
		if ( vicp_write( "CHDR OFF;CHLP OFF;CFMT DEF9,WORD,BIN;"
                         "CORD LO;*STB?\n", &len, SET, UNSET ) != SUCCESS )
            lecroy_wr_lan_failure( );

		len = sizeof buf;
		if ( vicp_read( buf, &len, &with_eoi, UNSET ) != SUCCESS )
            lecroy_wr_lan_failure( );

        /* Figure out which traces are displayed (only 8 can be displayed
           at the same time and we must be able to check for this when
           the user asks for one more to be displayed) */

        lecroy_wr.num_used_channels = 0;

        for ( i = LECROY_WR_CH1; i <= LECROY_WR_CH_MAX; i++ )
        {
            lecroy_wr.is_displayed[ i ] = UNSET;
            if ( lecroy_wr_is_displayed( i ) )
            {
                lecroy_wr.is_displayed[ i ] = SET;
                lecroy_wr.num_used_channels++;
            }
        }

        for ( i = LECROY_WR_M1; i <= LECROY_WR_M4; i++ )
            lecroy_wr.is_displayed[ i ] = UNSET;

        for ( i = LECROY_WR_TA; i <= LECROY_WR_TD; i++ )
        {
            lecroy_wr.is_displayed[ i ] = UNSET;
            if ( lecroy_wr_is_displayed( i ) )
            {
                lecroy_wr.is_displayed[ i ] = SET;
                lecroy_wr.num_used_channels++;
            }
        }

        /* Make sure the internal timebase is used */


		len = 9;
		if ( vicp_write( "SCLK INT\n", &len, SET, UNSET ) != SUCCESS )
            lecroy_wr_lan_failure( );

        /* Set or get the timebase (including the index in the table of
           possible timebases) while also taking care of the mode, i.e.
           RIS or SS */

        if ( lecroy_wr.is_timebase )
            lecroy_wr_set_timebase( lecroy_wr.timebase );
        else
        {
            lecroy_wr.tb_index = -1;

            lecroy_wr.timebase = lecroy_wr_get_timebase( );

            for ( i = 0; i < lecroy_wr.num_tbas; i++ )
                if ( fabs( lecroy_wr.tbas[ i ] - lecroy_wr.timebase ) /
                     lecroy_wr.tbas[ i ] < 0.1 )
                {
                    lecroy_wr.tb_index = i;
                    break;
                }

            if ( lecroy_wr.tb_index < 0 )
            {
                print( FATAL, "Can't determine timebase.\n" );
                THROW( EXCEPTION );
            }
        }

        /* Either set the record length or query it */

        if ( lecroy_wr.is_mem_size )
            lecroy_wr_set_memory_size( lecroy_wr.mem_size );
        else
            lecroy_wr.mem_size = lecroy_wr_get_memory_size( );

        lecroy_wr.cur_hres =
                     lecroy_wr.hres[ lecroy_wr.ms_index ] + lecroy_wr.tb_index;

        /* Switch interleaved (RIS) mode on if the user asked for it and it
           can be done, otherwise switch it off */

        if (    lecroy_wr.is_interleaved
             && lecroy_wr.interleaved
             && lecroy_wr.cur_hres->cl_ris > 0 )
            lecroy_wr_set_interleaved( SET );

        if (    ( lecroy_wr.is_interleaved && ! lecroy_wr.interleaved )
             || lecroy_wr.cur_hres->cl_ris == 0 )
        {
            lecroy_wr_set_interleaved( UNSET );
            lecroy_wr.interleaved = UNSET;
        }

        /* Set (if required) the sensitivies, offsets coupling types and
           bandwidth limiters of all measurement channels */

        for ( i = LECROY_WR_CH1; i <= LECROY_WR_CH_MAX; i++ )
        {
            if ( lecroy_wr.is_sens[ i ] )
                lecroy_wr_set_sens( i, lecroy_wr.sens[ i ] );
            else
                lecroy_wr.sens[ i ] = lecroy_wr_get_sens( i );

            if ( lecroy_wr.is_offset[ i ] )
                lecroy_wr_set_offset( i, lecroy_wr.offset[ i ] );
            else
                lecroy_wr.offset[ i ] = lecroy_wr_get_offset( i );

            if ( lecroy_wr.is_coupling[ i ] )
                lecroy_wr_set_coupling( i, lecroy_wr.coupling[ i ] );
            else
                lecroy_wr.coupling[ i ] = lecroy_wr_get_coupling( i );

            if ( lecroy_wr.is_bandwidth_limiter[ i ] )
                lecroy_wr_set_bandwidth_limiter( i,
                                            lecroy_wr.bandwidth_limiter[ i ] );
            else
                lecroy_wr.bandwidth_limiter[ i ] =
                                          lecroy_wr_get_bandwidth_limiter( i );
        }

        /* Set (if required) the trigger source */

        if ( lecroy_wr.is_trigger_source )
            lecroy_wr_set_trigger_source( lecroy_wr.trigger_source );

        /* Set (if required) the trigger level, slope and coupling of the
           trigger channels */

        for ( i = 0; i < ( int ) NUM_ELEMS( trg_channels ); i++ )
        {
            if ( trg_channels[ i ] == LECROY_WR_LIN )
                continue;

            if ( lecroy_wr.is_trigger_level[ trg_channels[ i ] ] )
                lecroy_wr_set_trigger_level( trg_channels[ i ],
                                lecroy_wr.trigger_level[ trg_channels[ i ] ] );
            else
                lecroy_wr.trigger_level[ trg_channels[ i ] ] =
                              lecroy_wr_get_trigger_level( trg_channels[ i ] );

            if ( lecroy_wr.is_trigger_slope[ trg_channels[ i ] ] )
                lecroy_wr_set_trigger_slope( trg_channels[ i ],
                                lecroy_wr.trigger_slope[ trg_channels[ i ] ] );
            else
                lecroy_wr.trigger_slope[ trg_channels[ i ] ] =
                              lecroy_wr_get_trigger_slope( trg_channels[ i ] );

            if ( lecroy_wr.is_trigger_coupling[ trg_channels[ i ] ] )
                lecroy_wr_set_trigger_coupling( trg_channels[ i ],
                             lecroy_wr.trigger_coupling[ trg_channels[ i ] ] );
            else
                lecroy_wr.trigger_coupling[ trg_channels[ i ] ] =
                           lecroy_wr_get_trigger_coupling( trg_channels[ i ] );
        }

        /* Set (if required) the trigger delay */

        if ( lecroy_wr.is_trigger_delay )
            lecroy_wr_set_trigger_delay( lecroy_wr.trigger_delay );
        else
            lecroy_wr_get_trigger_delay( );

        /* Get or set the trigger mode */

        if ( lecroy_wr.is_trigger_mode )
            lecroy_wr_set_trigger_mode( lecroy_wr.trigger_mode );
        else
            lecroy_wr_get_trigger_mode( );

        /* Now that we know about the timebase and trigger delay settings
           we can also do the checks on the window and trigger delay settngs
           that may not have been possible during the test run */

        lecroy_wr_soe_checks( );
    }
    OTHERWISE
    {
		vicp_close( );
        return FAIL;
    }

    return OK;
}


/*------------------------------------------------*
 * Function for determing the digitizers timebase
 *------------------------------------------------*/

double
lecroy_wr_get_timebase( void )
{
    char reply[ 30 ];
    ssize_t length = sizeof reply;
    double timebase;
    int i;


    lecroy_wr_talk( "TDIV?\n", reply, &length );
    reply[ length - 1 ] = '\0';
    timebase = T_atod( reply );

    for ( i = 0; i < lecroy_wr.num_tbas; i++ )
        if ( fabs( lecroy_wr.tbas[ i ] - timebase ) / timebase < 0.01 )
            break;

    if ( i == lecroy_wr.num_tbas )
    {
        print( FATAL, "Can't determine timebase.\n" );
        THROW( EXCEPTION );
    }

    lecroy_wr.timebase = timebase;
    lecroy_wr.tb_index = i;

    return lecroy_wr.timebase;
}


/*----------------------------------------------*
 * Function for setting the digitizers timebase
 *----------------------------------------------*/

bool
lecroy_wr_set_timebase( double timebase )
{
    char cmd[ 40 ] = "TDIV ";
	ssize_t len;


    strcat( gcvt( timebase, 8, cmd + strlen( cmd ) ), "\n" );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*------------------------------------------------------------*
 * Function for determining if digitizer is in RIS or SS mode
 *------------------------------------------------------------*/

bool
lecroy_wr_get_interleaved( void )
{
    char reply[ 30 ];
    ssize_t length = sizeof reply;


    lecroy_wr_talk( "ILVD?\n", reply, &length );
    lecroy_wr.interleaved = reply[ 1 ] == 'N';

    return lecroy_wr.interleaved;
}


/*------------------------------------------------*
 * Function for switching between RIS and SS mode
 *------------------------------------------------*/

bool
lecroy_wr_set_interleaved( bool state )
{
    char cmd[ 30 ] = "ILVD ";
	ssize_t len;


    strcat( cmd, state ? "ON\n" : "OFF\n" );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*----------------------------------------------------------*
 * Function for determining the memory size currently used.
 *----------------------------------------------------------*/

long
lecroy_wr_get_memory_size( void )
{
    char reply[ 30 ];
    ssize_t length = sizeof reply;
    long mem_size;
    double ms;
    long i;
    char *end_p;


    lecroy_wr_talk( "MSIZ?\n", reply, &length );
    reply[ length - 1 ] = '\0';

    ms = strtod( reply, &end_p );

    if ( ms > LONG_MAX )
    {
        print( FATAL, "Reported memory is too large: %s.\n", reply );
        THROW( EXCEPTION );
    }

    if ( end_p == ( char * ) reply )
    {
        print( FATAL, "Reported memory size not a number: %s.\n", reply );
        THROW( EXCEPTION );
    }

    mem_size = lround( ms );

    if ( *end_p == 'K' )
        mem_size *= 1000;
    else if ( *end_p == 'M' )
        mem_size *= 1000000;

    for ( i = 0; i < lecroy_wr.num_mem_sizes; i++ )
        if ( lecroy_wr.mem_sizes[ i ] == mem_size )
            break;

    if ( i == lecroy_wr.num_mem_sizes )
    {
        print( FATAL, "Can't determine memory size.\n" );
        THROW( EXCEPTION );
    }

    lecroy_wr.mem_size = mem_size;
    lecroy_wr.ms_index = i;

    return mem_size;
}


/*--------------------------------------*
 * Function for setting the memory size
 *--------------------------------------*/

bool
lecroy_wr_set_memory_size( long mem_size )
{
    char cmd[ 30 ];
	ssize_t len;


    sprintf( cmd, "MSIZ %ld\n", mem_size );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*------------------------------------------------------------------*
 * Function for determining the sensitivity (in V/div) of a channel
 *------------------------------------------------------------------*/

double
lecroy_wr_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    ssize_t length = sizeof reply;


    fsc2_assert( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX );

    sprintf( cmd, "C%1d:VDIV?\n", channel + 1 );
    lecroy_wr_talk( cmd, reply, &length );
    reply[ length - 1 ] = '\0';
    return lecroy_wr.sens[ channel ] = T_atod( reply );
}


/*--------------------------------------------------------------*
 * Function for setting the sensitivity (in V/div) of a channel
 *--------------------------------------------------------------*/

bool
lecroy_wr_set_sens( int    channel,
                    double sens )
{
    char cmd[ 40 ];
	ssize_t len;


    fsc2_assert( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX );

    sprintf( cmd, "C%1d:VDIV ", channel + 1 );
    strcat( gcvt( sens, 8, cmd + strlen( cmd ) ), "\n" );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*-----------------------------------------------------------*
 * Function for determining the offset voltage for a channel
 *-----------------------------------------------------------*/

double
lecroy_wr_get_offset( int channel )
{
    char buf[ 30 ];
    ssize_t length = sizeof buf;


    fsc2_assert( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX );

    sprintf( buf, "C%1d:OFST?\n", channel + 1 );
    lecroy_wr_talk( buf, buf, &length );
    buf[ length - 1 ] = '\0';
    return  lecroy_wr.offset[ channel ] = T_atod( buf );
}


/*-------------------------------------------------------*
 * Function for setting the offset voltage for a channel
 *-------------------------------------------------------*/

bool
lecroy_wr_set_offset( int    channel,
                      double offset )
{
    char cmd[ 40 ];
	ssize_t len;


    fsc2_assert( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX );

    sprintf( cmd, "C%1d:OFST ", channel + 1 );
    strcat( gcvt( offset, 8, cmd + strlen( cmd ) ), "\n" );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*----------------------------------------------------------*
 * Function for determining the coupling type for a channel
 *----------------------------------------------------------*/

int
lecroy_wr_get_coupling( int channel )
{
    int type = LECROY_WR_CPL_INVALID;
    char buf[ 100 ];
    ssize_t length = sizeof buf;


    fsc2_assert( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX );

    sprintf( buf, "C%1d:CPL?\n", channel + 1 );
    lecroy_wr_talk( buf, buf, &length );
    buf[ length - 1 ] = '\0';

    if ( buf[ 0 ] == 'A' )
        type = LECROY_WR_CPL_AC_1_MOHM;
    else if ( buf[ 0 ] == 'G' )
        type = LECROY_WR_CPL_GND;
    else if ( buf[ 0 ] == 'D' && buf[ 1 ] == '1' )
        type = LECROY_WR_CPL_DC_1_MOHM;
    else if ( buf[ 0 ] == 'D' && buf[ 1 ] == '5' )
        type = LECROY_WR_CPL_DC_50_OHM;
    else if ( buf[ 0 ] == 'O' )          /* overload with 50 ohm DC coupling */
    {
        type = LECROY_WR_CPL_DC_50_OHM;
        print( SEVERE, "Signal overload detected for channel '%s', input "
               "automatically disconnected.\n",
               LECROY_WR_Channel_Names[ channel ] );
    }
    else
        fsc2_impossible( );

    return lecroy_wr.coupling[ channel ] = type;
}


/*------------------------------------------------------*
 * Function for setting the coupling type for a channel
 *------------------------------------------------------*/

bool
lecroy_wr_set_coupling( int channel,
                        int type )
{
    char cmd[ 30 ];
	ssize_t len;
    char const *cpl[ ] = { "A1M", "D1M", "D50", "GND" };


    fsc2_assert( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX );
    fsc2_assert(    type >= LECROY_WR_CPL_AC_1_MOHM
                 && type <= LECROY_WR_CPL_GND );

    sprintf( cmd, "C%1d:CPL %s\n", channel + 1, cpl[ type ] );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*-----------------------------------------------------------------------*
 * Function for determining the bandwidth limiter of one of the channels
 *-----------------------------------------------------------------------*/

int
lecroy_wr_get_bandwidth_limiter( int channel )
{
    char buf[ 30 ] = "BWL?\n";
    ssize_t length = sizeof buf;
    int mode = -1;
    char *ptr = buf;
    int ch;
    size_t i;


    fsc2_assert( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX );

    lecroy_wr_talk( buf, buf, &length );
    buf[ length - 1 ] = '\0';

    /* We have to distinguish two cases: if the global bandwidth limiter is
       on or if all channels have the same limiter setting only a single
       value gets returned, otherwise the setting for each channel will be
       reported as a list of comma separated channel-value pairs */

    if ( ! strchr( buf, ',' ) )
    {
        if ( buf[ 1 ] == 'F' )           /* OFF */
            mode = LECROY_WR_BWL_OFF;
        else if ( buf[ 1 ] == 'N' )      /* ON */
            mode = LECROY_WR_BWL_ON;
        else if ( buf[ 0 ] == '2' )      /* 200MHZ */
            mode = LECROY_WR_BWL_200MHZ;
        else
            fsc2_impossible( );

        for ( i = 0; i <= LECROY_WR_CH_MAX; i++ )
            lecroy_wr.bandwidth_limiter[ i ] = mode;

        return mode;
    }

    for ( i = 0; i <= LECROY_WR_CH_MAX; i++ )
    {
        if (    sscanf( ptr + 1, "%d", &ch ) != 1
             || --ch < LECROY_WR_CH1
             || ch > LECROY_WR_CH_MAX )
        {
            print( FATAL, "Can't determine bandwidth limiter settings.\n" );
            THROW( EXCEPTION );
        }

        ptr += 3;

        if ( ptr[ 1 ] == 'F' )           /* OFF */
        {
            mode = LECROY_WR_BWL_OFF;
            ptr += 4;
        }
        else if ( ptr[ 1 ] == 'N' )      /* ON */
        {
            mode = LECROY_WR_BWL_ON;
            ptr += 3;
        }
        else if ( ptr[ 0 ] == '2' )      /* 200MHZ */
        {
            mode = LECROY_WR_BWL_200MHZ;
            ptr += 7;
        }
        else
            fsc2_impossible( );

        lecroy_wr.bandwidth_limiter[ ch ] = mode;
    }

    return lecroy_wr.bandwidth_limiter[ channel ];
}


/*--------------------------------------------------------------------*
 * Function for setting the bandwidth limiter for one of the channels
 *--------------------------------------------------------------------*/

bool
lecroy_wr_set_bandwidth_limiter( int channel,
                                 int bwl )
{
    char buf[ 50 ] = "GBWL?\n";
    ssize_t length = sizeof buf;
#if defined LECROY_WR_HAS_GLOBAL_BW
    int i;
#endif


    fsc2_assert( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX );
    fsc2_assert( bwl >= LECROY_WR_BWL_OFF && bwl <= LECROY_WR_BWL_200MHZ );

#if defined LECROY_WR_HAS_GLOBAL_BW

    /* We first need to check if the global bandwidth limiter is on or off. */

    lecroy_wr_talk( buf, buf, &length );
    buf[ length - 1 ] = '\0';

    fsc2_assert( buf[ 1 ] == 'N' || buf[ 1 ] == 'F' );

    /* If it's on and we are supposed to switch the channels bandwidth limit
       also to on nothing really needs to be done (beside setting the internal
       variable accordingly) */

    if ( buf[ 1 ] == 'N' && bwl == LECROY_WR_BWL_ON )
    {
        lecroy_wr.bandwidth_limiter[ channel ] = LECROY_WR_BWL_ON;
        return OK;
    }

    /* If the global bandwidth limiter is off we only have to set up the
       channel we're really interested in and are done */

    if ( buf[ 1 ] == 'F' )
    {
        sprintf( buf, "BWL C%1d,", channel + 1 );
        if ( bwl == LECROY_WR_BWL_OFF )
            strcat( buf, "OFF\n" );
        else if ( bwl == LECROY_WR_BWL_ON )
            strcat( buf, "ON\n" );
        else
            strcat( buf, "200MHZ\n" );

		length = strlen( buf );
		if ( vicp_write( buf, &length, SET, UNSET ) != SUCCESS )
            lecroy_wr_lan_failure( );

        return OK;
    }

    /* Otherwise we have to switch the global bandwidth limiter off and set
       the individual bandwidth limters to on for all channels we're not
       interested in and set up the remaining channel to what the user asked
       us to */

    strcpy( buf, "GBWL OFF\n" );
	length = strlen( buf );
	if ( vicp_write( buf, &length, SET, UNSET ) != SUCCESS )
		lecroy_wr_lan_failure( );

    strcpy( buf, "BWL " );

    for ( i = 0; i <= LECROY_WR_CH_MAX; i++ )
    {
        sprintf( buf + strlen( buf ), "C%1d,", i + 1 );
        if ( i != channel || bwl == LECROY_WR_BWL_ON )
            strcat( buf, "ON," );
        else if ( bwl == LECROY_WR_BWL_ON )
            strcat( buf, "OFF," );
        else
            strcat( buf, "200MHZ," );
    }

    buf[ strlen( buf ) - 1 ] = '\n';
#else
    sprintf( buf, "BWL C%1d,", channel + 1 );
    if ( bwl == LECROY_WR_BWL_ON )
        strcat( buf, "ON\n" );
    else if ( bwl == LECROY_WR_BWL_ON )
        strcat( buf, "OFF\n" );
    else
        strcat( buf, "200MHZ\n" );
#endif

	length = strlen( buf );
	if ( vicp_write( buf, &length, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*-------------------------------------------------------------*
 * Function for finding out the current trigger source channel
 *-------------------------------------------------------------*/

int
lecroy_wr_get_trigger_source( void )
{
    char reply[ 100 ];
    ssize_t length = sizeof reply;
    int src = LECROY_WR_UNDEF;
    char *ptr = reply + 7;


    lecroy_wr_talk( "TRSE?\n", reply, &length );
    reply[ length - 1 ] = '\0';

    if (    strncmp( reply, "STD,SR,", 7 )
         && strncmp( reply, "EDGE,SR,", 8 ) )
    {
        print( SEVERE, "Non-standard mode trigger, switching to standard "
               "edge trigger on to LINe input\n" );
        return lecroy_wr_set_trigger_source( LECROY_WR_LIN );
    }

    if ( *ptr == ',' )
        ptr++;

    if ( *ptr == 'C' )
        sscanf( ++ptr, "%d", &src );
    else if ( *ptr == 'L' )
        src = LECROY_WR_LIN;
    else if ( *ptr == 'E' && ptr[ 2 ] == '1' )
        src = LECROY_WR_EXT10;
    else if ( *ptr == 'E' && ptr[ 2 ] != '1' )
        src = LECROY_WR_EXT;
    else
        fsc2_impossible( );

    return lecroy_wr.trigger_source = src;
}


/*---------------------------------------------------------*
 * Function for setting the current trigger source channel
 *---------------------------------------------------------*/

bool
lecroy_wr_set_trigger_source( int channel )
{
    char cmd[ 40 ] = "TRSE STD,SR,";
	ssize_t len;


    fsc2_assert(    (    channel >= LECROY_WR_CH1
                      && channel <= LECROY_WR_CH_MAX )
                 || channel == LECROY_WR_LIN
                 || channel == LECROY_WR_EXT
                 || channel == LECROY_WR_EXT10 );

    if ( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX )
        sprintf( cmd + 11, "C%1d\n", channel + 1 );
    else if ( channel == LECROY_WR_LIN )
        strcat( cmd, "LINE\n" );
    else if ( channel == LECROY_WR_EXT )
        strcat( cmd, "EX\n" );
    else
        strcat( cmd, "EX10\n" );

	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*-------------------------------------------------------------------*
 * Function for finding out the trigger level of one of the channels
 *-------------------------------------------------------------------*/

double
lecroy_wr_get_trigger_level( int channel )
{
    char buf[ 30 ];
    ssize_t length = sizeof buf;


    fsc2_assert(    (    channel >= LECROY_WR_CH1
                      && channel <= LECROY_WR_CH_MAX )
                 || channel == LECROY_WR_EXT
                 || channel == LECROY_WR_EXT10 );

    if ( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX )
        sprintf( buf, "C%1d:TRLV?\n", channel + 1 );
    else if ( channel == LECROY_WR_EXT )
        strcpy( buf, "EX:TRLV?\n" );
    else
        strcpy( buf, "EX10:TRLV?\n" );

    lecroy_wr_talk( buf, buf, &length );
    buf[ length - 1 ] = '\0';
    return lecroy_wr.trigger_level[ channel ] = T_atod( buf );
}


/*----------------------------------------------------------------*
 * Function for setting the trigger slope for one of the channels
 *----------------------------------------------------------------*/

bool
lecroy_wr_set_trigger_level( int    channel,
                             double level )
{
    char cmd[ 40 ];
	ssize_t len;


    fsc2_assert(    (    channel >= LECROY_WR_CH1
                      && channel <= LECROY_WR_CH_MAX )
                 || channel == LECROY_WR_EXT
                 || channel == LECROY_WR_EXT10 );

    if ( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX )
        sprintf( cmd, "C%1d:TRLV ", channel + 1 );
    else if ( channel == LECROY_WR_EXT )
        strcpy( cmd, "EX:TRLV " );
    else
        strcpy( cmd, "EX10:TRLV " );

    strcat( gcvt( level, 6, cmd + strlen( cmd ) ), "\n" );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*-------------------------------------------------------------------*
 * Function for finding out the trigger slope of one of the channels
 *-------------------------------------------------------------------*/

bool
lecroy_wr_get_trigger_slope( int channel )
{
    char buf[ 30 ];
    ssize_t length = sizeof buf;


    fsc2_assert(    (    channel >= LECROY_WR_CH1
                      && channel <= LECROY_WR_CH_MAX )
                 || channel == LECROY_WR_LIN
                 || channel == LECROY_WR_EXT
                 || channel == LECROY_WR_EXT10 );

    if ( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX )
        sprintf( buf, "C%1d:TRSL?\n", channel + 1 );
    else if ( channel == LECROY_WR_LIN )
        strcpy( buf, "LINE:TRSL?\n" );
    else if ( channel == LECROY_WR_EXT )
        strcpy( buf, "EX:TRSL?\n" );
    else
        strcpy( buf, "EX10:TRSL?" );

    lecroy_wr_talk( ( const char *) buf, buf, &length );

    lecroy_wr.trigger_slope[ channel ] =
                                         buf[ 0 ] == 'P' ? POSITIVE : NEGATIVE;

    return lecroy_wr.trigger_slope[ channel ];
}


/*----------------------------------------------------------------*
 * Function for setting the trigger slope for one of the channels
 *----------------------------------------------------------------*/

bool
lecroy_wr_set_trigger_slope( int channel,
                             bool slope )
{
    char cmd[ 40 ];
	ssize_t len;


    fsc2_assert(    (    channel >= LECROY_WR_CH1
                      && channel <= LECROY_WR_CH_MAX )
                 || channel == LECROY_WR_LIN
                 || channel == LECROY_WR_EXT
                 || channel == LECROY_WR_EXT10 );

    if ( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX )
        sprintf( cmd, "C%1d:TRSL ", channel + 1 );
    else if ( channel == LECROY_WR_LIN )
        strcpy( cmd, "LINE:TRSL " );
    else if ( channel == LECROY_WR_EXT )
        strcpy( cmd, "EX:TRSL " );
    else
        strcpy( cmd, "EX10:TRSL " );

    strcat( cmd, slope ? "POS\n" : "NEG\n" );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*-----------------------------------------------------------------------*
 * Function for finding out the trigger coupling for one of the channels
 *-----------------------------------------------------------------------*/

int
lecroy_wr_get_trigger_coupling( int channel )
{
    char buf[ 40 ];
    ssize_t length = sizeof buf;
    int cpl = -1;


    fsc2_assert(    (    channel >= LECROY_WR_CH1
                      && channel <= LECROY_WR_CH_MAX )
                 || channel == LECROY_WR_EXT
                 || channel == LECROY_WR_EXT10 );

    if ( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX )
        sprintf( buf, "C%1d:TRCP?\n", channel + 1 );
    else if ( channel == LECROY_WR_EXT )
        strcpy( buf, "EX:TRCP?\n" );
    else
        strcpy( buf, "EX10:TRCP?\n" );

    lecroy_wr_talk( ( const char *) buf, buf, &length );

    switch ( buf[ 0 ] )
    {
        case 'A' :
            cpl = LECROY_WR_TRG_CPL_AC;
            break;

        case 'D' :
            cpl = LECROY_WR_TRG_CPL_DC;
            break;

        case 'L' :
            cpl = LECROY_WR_TRG_CPL_LF_REJ;
            break;

        case 'H' :
            cpl = LECROY_WR_TRG_CPL_HF_REJ;
            break;

        default :
            fsc2_impossible( );
    }

    lecroy_wr.trigger_coupling[ channel ] = cpl;

    return cpl;
}


/*-------------------------------------------------------------------*
 * Function for setting the trigger coupling for one of the channels
 *-------------------------------------------------------------------*/

int
lecroy_wr_set_trigger_coupling( int channel,
                                int cpl )
{
    char cmd[ 40 ];
    const char *cpl_str[ ] = { "AC\n", "DC\n", "LFREJ\n", "HFREJ\n" };
	ssize_t len;


    fsc2_assert(    (    channel >= LECROY_WR_CH1
                      && channel <= LECROY_WR_CH_MAX )
                 || channel == LECROY_WR_EXT
                 || channel == LECROY_WR_EXT10 );
    fsc2_assert( cpl >= LECROY_WR_TRG_AC && cpl <= LECROY_WR_TRG_HF_REJ );

    if ( channel >= LECROY_WR_CH1 && channel <= LECROY_WR_CH_MAX )
        sprintf( cmd, "C%1d:TRCP ", channel + 1 );
    else if ( channel == LECROY_WR_EXT )
        strcpy( cmd, "EX:TRCP " );
    else
        strcpy( cmd, "EX10:TRCP " );

    strcat( cmd, cpl_str[ cpl ] );

	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*---------------------------------------------------*
 * Function for finding out the current trigger mode
 *---------------------------------------------------*/

int
lecroy_wr_get_trigger_mode( void )
{
    char buf[ 40 ];
    ssize_t length = sizeof buf;
    int mode = -1;


    lecroy_wr_talk( "TRMD?\n", buf, &length );

    if ( buf[ 0 ] == 'A' )
        mode = LECROY_WR_TRG_MODE_AUTO;
    else if ( buf[ 0 ] == 'N' )
        mode = LECROY_WR_TRG_MODE_NORMAL;
    else if ( buf[ 1 ] == 'S' )
        mode = LECROY_WR_TRG_MODE_SINGLE;
    else if ( buf[ 1 ] == 'T' )
        mode = LECROY_WR_TRG_MODE_STOP;
    else
        fsc2_impossible( );

    return lecroy_wr.trigger_mode = mode;
}


/*---------------------------------------*
 * Function for setting the trigger mode
 *---------------------------------------*/

int
lecroy_wr_set_trigger_mode( int mode )
{
    char cmd[ 40 ] = "TRMD ";
    const char *mode_str[ ] = { "AUTO\n", "NORM\n", "SINGLE\n", "STOP\n" };
	ssize_t len;


    fsc2_assert(    mode >= LECROY_WR_TRG_MODE_AUTO
                 && mode <= LECROY_WR_TRG_MODE_STOP );

    strcat( cmd, mode_str[ mode ] );

	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*----------------------------------------------------*
 * Function for determining the current trigger delay
 *----------------------------------------------------*/

double
lecroy_wr_get_trigger_delay( void )
{
    char reply[ 40 ];
    ssize_t length = sizeof reply;


    lecroy_wr_talk( "TRDL?\n", reply, &length );
    reply[ length - 1 ] = '\0';
    lecroy_wr.trigger_delay = T_atod( reply );

    /* Positive delays (i.e. when pretrigger is on) get returned from the
       device as a percentage of the full horizontal screen while for
       negative values it's already the delay time */

    if ( lecroy_wr.trigger_delay > 0.0 )
        lecroy_wr.trigger_delay *= 0.1 * lecroy_wr.timebase;

    return lecroy_wr.trigger_delay;
}


/*----------------------------------------*
 * Function for setting the trigger delay
 *----------------------------------------*/

bool
lecroy_wr_set_trigger_delay( double delay )
{
    char cmd[ 40 ] = "TRDL ";
	ssize_t len;


    /* For positive delay (i.e. pretrigger) the delay must be set as a
       percentage of the full horizontal screen width */

    if ( delay > 0.0 )
        delay = 10.0 * delay / lecroy_wr.timebase;

    strcat( gcvt( delay, 8, cmd + strlen( cmd ) ), "\n" );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    return OK;
}


/*---------------------------------------------------------*
 * Function for checking if a certain channel is displayed
 *---------------------------------------------------------*/

bool
lecroy_wr_is_displayed( int ch )
{
    char cmd[ 30 ];
    ssize_t length = sizeof cmd;


    if ( ch >= LECROY_WR_CH1 && ch <= LECROY_WR_CH_MAX )
        sprintf( cmd, "C%1d:TRA?\n", ch - LECROY_WR_CH1 + 1 );
    else if ( ch >= LECROY_WR_TA && ch <= LECROY_WR_TD )
        sprintf( cmd, "T%c:TRA?\n", ch - LECROY_WR_TA + 'A' );
    else if ( ch >= LECROY_WR_M1 && ch <= LECROY_WR_M4 )
    {
        print( FATAL, "A memory channel can't be displayed.\n");
        THROW( EXCEPTION );
    }
    else
    {
        print( FATAL, "Internal error detected.\n" );
        THROW( EXCEPTION );
    }

    lecroy_wr_talk( cmd, cmd, &length );
    return cmd[ 1 ] == 'N';
}


/*----------------------------------------------------*
 * Function to switch  display of a channel on or off
 *----------------------------------------------------*/

bool
lecroy_wr_display( int ch,
                   int on_off )
{
    char cmd[ 30 ];
	ssize_t len;


    if ( ch >= LECROY_WR_CH1 && ch <= LECROY_WR_CH_MAX )
        sprintf( cmd, "C%1d:TRA ", ch - LECROY_WR_CH1 + 1 );
    else if ( ch >= LECROY_WR_TA && ch <= LECROY_WR_TD )
        sprintf( cmd, "T%c:TRA ", ch - LECROY_WR_TA + 'A' );
    else if ( ch >= LECROY_WR_M1 && ch <= LECROY_WR_M4 )
    {
        print( FATAL, "Memory channels can't be displayed.\n" );
        THROW( EXCEPTION );
    }
    else
    {
        print( FATAL, "Internal error detected.\n" );
        THROW( EXCEPTION );
    }

    if (    on_off
         && lecroy_wr.num_used_channels >= LECROY_WR_MAX_USED_CHANNELS )
    {
        print( FATAL, "Can't switch on another trace, there are already as "
               "many as possible (%d) displayed.\n",
               LECROY_WR_MAX_USED_CHANNELS );
        THROW( EXCEPTION );
    }

    strcat( cmd, on_off ? "ON\n" : "OFF\n" );

	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    if ( on_off )
    {
        lecroy_wr.num_used_channels++;
        lecroy_wr.channels_in_use[ ch ] = SET;
    }
    else
    {
        lecroy_wr.num_used_channels--;
        lecroy_wr.channels_in_use[ ch ] = UNSET;
    }

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy_wr_finished( void )
{
	vicp_close( );
}


/*-----------------------------------------*
 * Function for starting a new acquisition
 *-----------------------------------------*/

void
lecroy_wr_start_acquisition( void )
{
    int ch;
    char cmd[ 100 ];
    bool do_averaging = UNSET;
	ssize_t len;


    /* Stop the digitizer (also switches to "STOPPED" trigger mode) */

	len = 5;
	if ( vicp_write( "STOP\n", &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    /* Set up the parameter to be used for averaging for the function channels
       (as far as they have been set by the user) */

    for ( ch = LECROY_WR_TA; ch <= LECROY_WR_TD; ch++ )
    {
        if ( ! lecroy_wr.is_avg_setup[ ch ] )
            continue;

        do_averaging = SET;

        snprintf( cmd, 100, "T%c:DEF EQN,'AVGS(C%1ld)',MAXPTS,%ld,SWEEPS,%ld\n",
                  'A' + LECROY_WR_TA - ch,
                  lecroy_wr.source_ch[ ch ] - LECROY_WR_CH1 + 1,
                  lecroy_wr_curve_length( ),
                  lecroy_wr.num_avg[ ch ] );

		len = strlen( cmd );
		if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
            lecroy_wr_lan_failure( );

        /* If we want to use a trace it must be switched on (but not the
           channel that gets averaged) */

        if ( ! lecroy_wr_is_displayed( ch ) )
            lecroy_wr_display( ch, SET );

        /* Switch off horizontal zoom and shift - if it's on the curve fetched
           from the device isn't what one would expect... */

        sprintf( cmd, "T%c:HMAG 1;T%c:HPOS 5\n", 'A' + LECROY_WR_TA - ch,
                 'A' + LECROY_WR_TA - ch ) ;

		len = strlen( cmd );
		if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
            lecroy_wr_lan_failure( );

        /* Finally reset what's currently stored in the trace (otherwise a
           new acquisition may not get started) */

        sprintf( cmd, "T%c:FRST\n", 'A' + LECROY_WR_TA - ch );

		len = strlen( cmd );
		if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
            lecroy_wr_lan_failure( );
    }

    /* Reset the bits in the word that tells us later that the data in the
       corresponding channel are ready to be fetched */

    lecroy_wr_get_inr( );

    can_fetch &= ~ ( LECROY_WR_PROC_DONE_TA | LECROY_WR_PROC_DONE_TB |
                     LECROY_WR_PROC_DONE_TC | LECROY_WR_PROC_DONE_TD |
                     LECROY_WR_SIGNAL_ACQ );

    /* Switch digitizer back on to running state by switching to a trigger
       mode where the digitizer is running (i.e. typically NORMAL, but, if
       the user requested it, also AUTO, or, if there's no averaging setup,
       even SINGLE mode will do) */

    strcpy( cmd, "TRMD NORM" );
    if (    ! do_averaging
         && lecroy_wr.trigger_mode == LECROY_WR_TRG_MODE_SINGLE )
        strcpy( cmd + 5, "SINGLE" );
    else if ( lecroy_wr.trigger_mode == LECROY_WR_TRG_MODE_AUTO )
        strcpy( cmd + 5, "AUTO" );
    else
        lecroy_wr.trigger_mode = LECROY_WR_TRG_MODE_NORMAL;

	strcat( cmd, "\n" );
	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

}


/*------------------------------------------------------------*
 * Function for fetching data from the digitizer - the calling
 * function then does the remaining specific manipulations on
 * these data
 *------------------------------------------------------------*/

static void
lecroy_wr_get_prep( int              ch,
                    Window_T       * w,
                    unsigned char ** data,
                    long           * length,
                    double         * gain,
                    double         * offset )
{
    unsigned int bit_to_test;
    char cmd[ 100 ];
    char ch_str[ 3 ];
    bool is_mem_ch = UNSET;
	ssize_t len;


    CLOBBER_PROTECT( data );
    CLOBBER_PROTECT( bit_to_test );

    /* Figure out which channel is to be used and set a few variables
       needed later accordingly */

    if ( ch >= LECROY_WR_CH1 && ch <= LECROY_WR_CH_MAX )
    {
        bit_to_test = LECROY_WR_SIGNAL_ACQ;
        sprintf( ch_str, "C%1d", ch - LECROY_WR_CH1 + 1 );
    }
    else if ( ch >= LECROY_WR_M1 && ch <= LECROY_WR_M4 )
    {
        is_mem_ch = SET;
        sprintf( ch_str, "M%d", ch - LECROY_WR_M1 + 1 );
    }
    else if ( ch >= LECROY_WR_TA && ch <= LECROY_WR_TD )
    {
        bit_to_test = LECROY_WR_PROC_DONE( ch );
        sprintf( ch_str, "T%c", ch - LECROY_WR_TA + 'A' );
    }
    else
        fsc2_impossible( );

#if 0
    /* We probably have to check if two or mor channels are combined - I found
       no way this can be checked via the program and we can only look for
       the number of points and compare that with what we expect. To make
       things a bit more interesting, the device always seems to send us 2 more
       points than it should and I don't know if that become 4 when to curves
       are combined ... */

    /* Get the number of byztes of the curve */

    len = lecroy_wr_get_int_value( ch, "WAVE_ARRAY_1" ) / 2;
#endif

    /* Set up the number of points to be fetched - take care: the device
       always measures an extra point before and after the displayed region,
       thus we start with one point later than we could get from it.*/

    if ( w != NULL )
        sprintf( cmd, "WFSU SP,0,NP,%ld,FP,%ld,SN,0\n",
                 w->num_points, w->start_num + 1 );
    else
        sprintf( cmd, "WFSU SP,0,NP,%ld,FP,1,SN,0\n",
                 lecroy_wr_curve_length( ) );

	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    /* When a non-memory curve is to be fetched and the acquisition isn't
       finished yet poll until the bit that tells that the acquisition for
       the requested channel is finished has become set */

    if (    ! is_mem_ch
         && ! ( can_fetch & bit_to_test ) )
        while ( ! ( ( can_fetch |= lecroy_wr_get_inr( ) ) & bit_to_test ) )
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
		if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
            lecroy_wr_lan_failure( );

        /* ...and fetch 'em */

        *data = lecroy_wr_get_data( length );
        *length /= 2;          /* we got word sized (16 bit) data, LSB first */

        /* Get the gain factor and offset for the date we just fetched */

        *gain = lecroy_wr_get_float_value( ch, "VERTICAL_GAIN" );
        *offset = lecroy_wr_get_float_value( ch, "VERTICAL_OFFSET" );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( *data != NULL )
            T_free( *data );
        RETHROW( );
    }
}


/*-----------------------------------------------------*
 * Function for fetching a curve from the oscilloscope
 *-----------------------------------------------------*/

void
lecroy_wr_get_curve( int         ch,
                     Window_T  * w,
                     double   ** array,
                     long      * length )
{
    double gain, offset;
    unsigned char *data;
    unsigned char *dp;
    long i;
    int val;


    /* Get the curve from the device */

    lecroy_wr_get_prep( ch, w, &data, length, &gain, &offset );

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
lecroy_wr_get_area( int        ch,
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

    lecroy_wr_get_prep( ch, w, &data, &length, &gain, &offset );

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
lecroy_wr_get_amplitude( int        ch,
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

    lecroy_wr_get_prep( ch, w, &data, &length, &gain, &offset );

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
lecroy_wr_copy_curve( long src,
                      long dest )
{
    char cmd[ 100 ] = "STO ";
	ssize_t len;


    fsc2_assert(    ( src >= LECROY_WR_CH1 && src <= LECROY_WR_CH_MAX )
                 || ( src >= LECROY_WR_TA && src <= LECROY_WR_TD ) );
    fsc2_assert( dest >= LECROY_WR_M1 && dest <= LECROY_WR_M4 );


    if ( src >= LECROY_WR_CH1 && src <= LECROY_WR_CH_MAX )
        sprintf( cmd + strlen( cmd ), "C%1ld,", src - LECROY_WR_CH1 + 1 );
    else
        sprintf( cmd + strlen( cmd ), "T%c,",
                 ( char ) ( src - LECROY_WR_TA + 'A' ) );

    sprintf( cmd + strlen( cmd ), "M%ld\n", dest - LECROY_WR_M1 + 1 );

	len = strlen( cmd );
	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );
}


/*----------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static unsigned char *
lecroy_wr_get_data( long * len )
{
    unsigned char *data;
    char len_str[ 10 ];
    bool with_eoi;
    ssize_t length;


    /* First thing we read is something like "DAT1,", then "#[0-9]" where
       the number following the '#' is the number of bytes to be read next
       (doing two reads instead of one seems to be necessaty for the X-Stream
       oscilloscopes) */

    length = 5;
	if ( vicp_read( len_str, &length, &with_eoi, UNSET ) == FAILURE )
        lecroy_wr_lan_failure( );

    length = 2;
	if ( vicp_read( len_str, &length, &with_eoi, UNSET ) == FAILURE )
        lecroy_wr_lan_failure( );

    len_str [ length ] = '\0';
    length = T_atol( len_str + 1 );

    fsc2_assert( length > 0 );

    /* Now get the number of bytes to read */

	if ( vicp_read( len_str, &length, &with_eoi, UNSET ) == FAILURE )
        lecroy_wr_lan_failure( );

    len_str[ length ] = '\0';
    length = T_atol( len_str );

    fsc2_assert( length > 0 );

    /* Obtain enough memory and then read all the data */

    data = T_malloc( length );

	if ( vicp_read( ( char * ) data, &length, &with_eoi, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );

    *len = length;
    return data;
}


/*----------------------------------------------------------------------*
 * Function for obtaining an integer value from the waveform descriptor
 *----------------------------------------------------------------------*/

#if 0
static long
lecroy_wr_get_int_value( int          ch,
                         const char * name )
{
    char cmd[ 100 ];
    ssize_t length = sizeof cmd;
    char *ptr = cmd;
    long val = 0;


    CLOBBER_PROTECT( ptr );
    CLOBBER_PROTECT( val );

    if ( ch >= LECROY_WR_CH1 && ch <= LECROY_WR_CH_MAX )
        sprintf( cmd, "C%1d:INSP? '%s'\n", ch - LECROY_WR_CH1 + 1, name );
    else if ( ch >= LECROY_WR_M1 && ch <= LECROY_WR_M4 )
        sprintf( cmd, "M%c:INSP? '%s'\n", ch - LECROY_WR_M1 + 1, name );
    else if ( ch >= LECROY_WR_TA && ch <= LECROY_WR_TD )
        sprintf( cmd, "T%c:INSP? '%s'\n", ch - LECROY_WR_TA + 'A', name );
    else
        fsc2_impossible( );

    lecroy_wr_talk( cmd, cmd, &length );
    cmd[ length - 1 ] = '\0';

    while ( *ptr && *ptr++ != ':' )
        /* empty */ ;

    if ( ! *ptr )
        lecroy_wr_lan_failure( );

    TRY
    {
        val = T_atol( ptr );
        TRY_SUCCESS;
    }
    OTHERWISE
        lecroy_wr_lan_failure( );

    return val;
}
#endif


/*-------------------------------------------------------------------*
 * Function for obtaining a float value from the waveform descriptor
 *-------------------------------------------------------------------*/

static double
lecroy_wr_get_float_value( int          ch,
                           const char * name )
{
    char cmd[ 100 ];
    ssize_t length = sizeof cmd;
    char *ptr = cmd;
    double val = 0.0;


    CLOBBER_PROTECT( ptr );
    CLOBBER_PROTECT( val );

    if ( ch >= LECROY_WR_CH1 && ch <= LECROY_WR_CH_MAX )
        sprintf( cmd, "C%1d:INSP? '%s'\n", ch - LECROY_WR_CH1 + 1, name );
    else if ( ch >= LECROY_WR_M1 && ch <= LECROY_WR_M4 )
        sprintf( cmd, "M%c:INSP? '%s'\n", ch - LECROY_WR_M1 + 1, name );
    else if ( ch >= LECROY_WR_TA && ch <= LECROY_WR_TD )
        sprintf( cmd, "T%c:INSP? '%s'\n", ch - LECROY_WR_TA + 'A', name );
    else
        fsc2_impossible( );

    lecroy_wr_talk( cmd, cmd, &length );

    if ( length == 1 )
    {
        bool with_eoi;

        length = sizeof cmd;
        if ( vicp_read( cmd, &length, &with_eoi, UNSET ) != SUCCESS )
            lecroy_wr_lan_failure( );
    }

    cmd[ length - 1 ] = '\0';

    while ( *ptr && *ptr++ != ':' )
        /* empty */ ;

    if ( ! *ptr )
        lecroy_wr_lan_failure( );

    TRY
    {
        val = T_atod( ptr );
        TRY_SUCCESS;
    }
    OTHERWISE
        lecroy_wr_lan_failure( );

    return val;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool
lecroy_wr_command( const char * cmd )
{
	ssize_t len = strlen( cmd );


	if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
        lecroy_wr_lan_failure( );
    return OK;
}


/*------------------------------------------------------------------------*
 * Function fetches (thereby reseting!) the INR register from the device.
 * It sets all bits in the INR element of the structure for the device
 * where a bit in the INR is set. Functions making use of the fact that
 * a bit is set must reset it when the action they take invalidate the
 * condition that led to the bit becoming set.
 *-----------------------------------------------------------------------*/

static unsigned int
lecroy_wr_get_inr( void )
{
    char reply[ 10 ] = "INR?";
    ssize_t length = sizeof reply;


    lecroy_wr_talk( "INR?\n", reply, &length );
    reply[ length - 1 ] = '\0';
    return ( unsigned int ) T_atoi( reply );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static int
lecroy_wr_talk( const char * cmd,
                char       * reply,
                ssize_t    * length )
{
    ssize_t len = strlen( cmd );
    int ret = 0;
    bool with_eoi;


    CLOBBER_PROTECT( ret );

    TRY
    {
        if ( vicp_write( cmd, &len, SET, UNSET ) != SUCCESS )
            THROW( EXCEPTION );
        ret = vicp_read( reply, length, &with_eoi, UNSET );
        TRY_SUCCESS;
    }
    OTHERWISE
        lecroy_wr_lan_failure( );

    return ret;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lecroy_wr_lan_failure( void )
{
    vicp_close( );
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
