/* -*-C-*-
 *
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


#include "lecroy_wr2.h"


static unsigned char *lecroy_wr2_get_data( long * len );

static unsigned int lecroy_wr2_get_inr( void );
#if 0
static int lecroy_wr2_get_int_value( int          ch,
									 const char * name );
#endif
static double lecroy_wr2_get_float_value( int          ch,
										  const char * name );

static void lecroy_wr2_get_prep( int              ch,
								 Window_T *       w,
								 unsigned char ** data,
								 long *           length,
								 double *         gain,
								 double *         offset,
								 const char *     what );

static unsigned char *lecroy_wr2_get_data( long * len );

static bool lecroy_wr2_talk( const char * cmd,
							 char *       reply,
							 long *       length );

static void lecroy_wr2_gpib_failure( void );


static unsigned int can_fetch;
static int trg_channels[ ] = {
#if defined LECROY_WR2_CH4_AS_TRG
								LECROY_WR2_CH4,
#endif
								LECROY_WR2_LIN,
#if defined LECROY_WR2_EXT
								LECROY_WR2_EXT,
#endif
#if defined LECROY_WR2_EXT10
								LECROY_WR2_EXT10

#endif
							  };


/*---------------------------------------------------------------*
 * Function called for initialization of device and to determine
 * its state
 *---------------------------------------------------------------*/

bool lecroy_wr2_init( const char * name )
{
	char buffer[ 100 ];
	long len = 100;
	int i;


	if ( gpib_init_device( name, &lecroy_wr2.device ) == FAILURE )
        return FAIL;

    /* Disable the local button, set digitizer to short form of replies,
	   transmit data in one block in binary word (2 byte) format with LSB
	   first. Then ask for the status byte to make sure the device reacts. */

    if ( gpib_local_lockout( lecroy_wr2.device ) == FAILURE  ||
		 gpib_write( lecroy_wr2.device,
					 "CHDR OFF;CHLP OFF;CFMT DEF9,WORD,BIN;CORD LO;ACAL", 49 )
		                                                          == FAILURE ||
		 gpib_write( lecroy_wr2.device, "*STB?", 5 ) == FAILURE ||
		 gpib_read( lecroy_wr2.device, buffer, &len ) == FAILURE )
	{
		gpib_local( lecroy_wr2.device );
        return FAIL;
	}

	TRY
	{
		/* Figure out which traces are displayed (only 4 can be displayed
		   at the same time and we must be able to check for this when
		   th user asks for one more to be displayed) */

		lecroy_wr2.num_used_channels = 0;

		for ( i = LECROY_WR2_CH1; i <= LECROY_WR2_CH_MAX; i++ )
		{
			lecroy_wr2.is_displayed[ i ] = UNSET;
			if ( lecroy_wr2_is_displayed( i ) )
			{
				lecroy_wr2.is_displayed[ i ] = SET;
				lecroy_wr2.num_used_channels++;
			}
		}

		for ( i = LECROY_WR2_M1; i <= LECROY_WR2_M4; i++ )
		{
			lecroy_wr2.is_displayed[ i ] = UNSET;
			if ( lecroy_wr2_is_displayed( i ) )
			{
				lecroy_wr2.is_displayed[ i ] = SET;
				lecroy_wr2.num_used_channels++;
			}
		}

		for ( i = LECROY_WR2_TA; i <= LECROY_WR2_TD; i++ )
		{
			lecroy_wr2.is_displayed[ i ] = UNSET;
			if ( lecroy_wr2_is_displayed( i ) )
			{
				lecroy_wr2.is_displayed[ i ] = SET;
				lecroy_wr2.num_used_channels++;
			}
		}

		/* Make sure the internal timebase is used */

		if ( gpib_write( lecroy_wr2.device, "SCLK INT", 8 ) == FAILURE )
			lecroy_wr2_gpib_failure( );

		/* Set or get the timebase (including the index in the table of
		   possible timebases) while also taking care of the mode, i.e.
		   RIS or SS */

		if ( lecroy_wr2.is_timebase )
			lecroy_wr2_set_timebase( lecroy_wr2.timebase );
		else
		{
			lecroy_wr2.tb_index = -1;

			lecroy_wr2.timebase = lecroy_wr2_get_timebase( );

			for ( i = 0; i < lecroy_wr2.num_tbas; i++ )
				if ( abs( lecroy_wr2.tbas[ i ] - lecroy_wr2.timebase ) /
					 lecroy_wr2.timebase < 0.1 )
				{
					lecroy_wr2.tb_index = i;
					break;
				}

			if ( lecroy_wr2.tb_index < 0 )
			{
				print( FATAL, "Can't determine timebase.\n" );
				THROW( EXCEPTION );
			}
		}

		/* Either set the record length or query it */

		if ( lecroy_wr2.is_mem_size )
			lecroy_wr2_set_memory_size( lecroy_wr2.mem_size );
		else
			lecroy_wr2.mem_size = lecroy_wr2_get_memory_size( );

		/* Switch interleaved (RIS) mode on if the user asked for it and it
		   can be done, otherwise switch it off */

		if ( lecroy_wr2.is_interleaved && lecroy_wr2.interleaved &&
			 lecroy_wr2.cur_hres->ppd_ris > 0 )
			lecroy_wr2_set_interleaved( SET );

		if ( ( lecroy_wr2.is_interleaved && ! lecroy_wr2.interleaved ) ||
			 lecroy_wr2.cur_hres->ppd_ris == 0 )
		{
			lecroy_wr2_set_interleaved( UNSET );
			lecroy_wr2.interleaved = UNSET;
		}

		/* Set (if required) the sensitivies, offsets coupling types and
		   bandwidth limiters of all measurement channels */

		for ( i = LECROY_WR2_CH1; i <= LECROY_WR2_CH_MAX; i++ )
		{
			if ( lecroy_wr2.is_sens[ i ] )
				lecroy_wr2_set_sens( i, lecroy_wr2.sens[ i ] );

			if ( lecroy_wr2.is_offset[ i ] )
				lecroy_wr2_set_offset( i, lecroy_wr2.offset[ i ] );

			if ( lecroy_wr2.is_coupling[ i ] )
				lecroy_wr2_set_coupling( i, lecroy_wr2.coupling[ i ] );

			if ( lecroy_wr2.is_bandwidth_limiter[ i ] )
				lecroy_wr2_set_bandwidth_limiter( i,
					                      lecroy_wr2.bandwidth_limiter[ i ] );
		}

		/* Set (if required) the trigger source */

		if ( lecroy_wr2.is_trigger_source )
			lecroy_wr2_set_trigger_source( lecroy_wr2.trigger_source );

		/* Set (if required) the trigger level, slope and coupling of the
		   trigger channels */

		for ( i = 0; i < ( int ) NUM_ELEMS( trg_channels ); i++ )
		{
			if ( trg_channels[ i ] == LECROY_WR2_LIN )
				continue;

			if ( lecroy_wr2.is_trigger_level[ trg_channels[ i ] ] )
				lecroy_wr2_set_trigger_level( trg_channels[ i ],
							lecroy_wr2.trigger_level[ trg_channels[ i ] ] );

			if ( lecroy_wr2.is_trigger_slope[ trg_channels[ i ] ] )
				lecroy_wr2_set_trigger_slope( trg_channels[ i ],
							lecroy_wr2.trigger_slope[ trg_channels[ i ] ] );

			if ( lecroy_wr2.is_trigger_coupling[ trg_channels[ i ] ] )
				lecroy_wr2_set_trigger_coupling( trg_channels[ i ],
						 lecroy_wr2.trigger_coupling[ trg_channels[ i ] ] );
		}

		/* Set (if required) the trigger delay */

		if ( lecroy_wr2.is_trigger_delay )
			lecroy_wr2_set_trigger_delay( lecroy_wr2.trigger_delay );
		else
			lecroy_wr2_get_trigger_delay( );

		/* Get or set the trigger mode */

		if ( lecroy_wr2.is_trigger_mode )
			lecroy_wr2_set_trigger_mode( lecroy_wr2.trigger_mode );
		else
			lecroy_wr2_get_trigger_mode( );
	}
	OTHERWISE
	{
		gpib_local( lecroy_wr2.device );
        return FAIL;
	}
		
	return OK;
}


/*-------------------------------------------------*
 * Function for determing the digitizers timebase 
 *-------------------------------------------------*/

double lecroy_wr2_get_timebase( void )
{
	char reply[ 30 ];
	long length = 30;
	double timebase;
	int i;


	lecroy_wr2_talk( "TDIV?", reply, &length );
	reply[ length - 1 ] = '\0';
	timebase = T_atod( reply );

	for ( i = 0; i < lecroy_wr2.num_tbas; i++ )
		if ( abs( lecroy_wr2.tbas[ i ] - timebase ) / timebase < 0.01 )
			break;

	if ( i == lecroy_wr2.num_tbas )
	{
		print( FATAL, "Can't determine timebase.\n" );
		THROW( EXCEPTION );
	}

	lecroy_wr2.timebase = timebase;
	lecroy_wr2.tb_index = i;

	return lecroy_wr2.timebase;
}


/*-----------------------------------------------*
 * Function for setting the digitizers timebase 
 *-----------------------------------------------*/

bool lecroy_wr2_set_timebase( double timebase )
{
	char cmd[ 40 ] = "TDIV ";


	gcvt( timebase, 8, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*------------------------------------------------------------*
 * Function for determining if digitizer is in RIS or SS mode
 *------------------------------------------------------------*/

bool lecroy_wr2_get_interleaved( void )
{
	char reply[ 30 ];
	long length = 30;


	lecroy_wr2_talk( "ILVD?", reply, &length );
	lecroy_wr2.interleaved = reply[ 1 ] == 'N';

	return lecroy_wr2.interleaved;
}


/*------------------------------------------------*
 * Function for switching between RIS and SS mode
 *------------------------------------------------*/

bool lecroy_wr2_set_interleaved( bool state )
{
	char cmd[ 30 ] = "ILVD ";

	strcat( cmd, state ? "ON" : "OFF" );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------*
 * Function for determining the memory size currently used.
 *----------------------------------------------------------*/

long lecroy_wr2_get_memory_size( void )
{
	char reply[ 30 ];
	long length = 30;
	long mem_size;
	long i;


	lecroy_wr2_talk( "MSIZ?", reply, &length );
	reply[ length - 1 ] = '\0';

	mem_size =  T_atol( reply );

	for ( i = 0; i < lecroy_wr2.num_mem_sizes; i++ )
		if ( lecroy_wr2.mem_sizes[ i ] == mem_size )
			break;

	if ( i == lecroy_wr2.num_mem_sizes )
	{
		print( FATAL, "Can't determine memory size.\n" );
		THROW( EXCEPTION );
	}

	lecroy_wr2.mem_size = mem_size;
	lecroy_wr2.ms_index = i;

	return mem_size;
}


/*--------------------------------------*
 * Function for setting the memory size
 *--------------------------------------*/

bool lecroy_wr2_set_memory_size( long mem_size )
{
	char cmd[ 30 ];

	sprintf( cmd, "MSIZ %ld\n", mem_size );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*------------------------------------------------------------------*
 * Function for determining the sensitivity (in V/div) of a channel
 *------------------------------------------------------------------*/

double lecroy_wr2_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = 30;


	fsc2_assert( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX );

	sprintf( cmd, "C%1d:VDIV?", channel + 1 );
	lecroy_wr2_talk( cmd, reply, &length );
    reply[ length - 1 ] = '\0';
	return lecroy_wr2.sens[ channel ] = T_atod( reply );
}


/*--------------------------------------------------------------*
 * Function for setting the sensitivity (in V/div) of a channel
 *--------------------------------------------------------------*/

bool lecroy_wr2_set_sens( int    channel,
						  double sens )
{
    char cmd[ 40 ];


	fsc2_assert( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX );

	sprintf( cmd, "C%1d:VDIV ", channel + 1 );
	gcvt( sens, 8, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------*
 * Function for determining the offset voltage for a channel
 *-----------------------------------------------------------*/

double lecroy_wr2_get_offset( int channel )
{
    char buf[ 30 ];
    long length = 30;


	fsc2_assert( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX );

	sprintf( buf, "C%1d:OFST?", channel + 1 );
	lecroy_wr2_talk( ( const char * ) buf, buf, &length );
    buf[ length - 1 ] = '\0';
	return  lecroy_wr2.offset[ channel ] = T_atod( buf );
}


/*-------------------------------------------------------*
 * Function for setting the offset voltage for a channel
 *-------------------------------------------------------*/

bool lecroy_wr2_set_offset( int    channel,
							double offset )
{
    char cmd[ 40 ];


	fsc2_assert( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX );

	sprintf( cmd, "C%1d:OFST ", channel + 1 );
	gcvt( offset, 8, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------*
 * Function for determining the coupling type for a channel
 *----------------------------------------------------------*/

int lecroy_wr2_get_coupling( int channel )
{
	int type = LECROY_WR2_INVALID_COUPL;
	char buf[ 100 ];
	long length = 100;


	fsc2_assert( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX );

	sprintf( buf, "C%1d:CPL?", channel + 1 );
	lecroy_wr2_talk( ( const char * ) buf, buf, &length );
    buf[ length - 1 ] = '\0';

	if ( buf[ 0 ] == 'A' )
		type = LECROY_WR2_AC_1_MOHM;
	else if ( buf[ 0 ] == 'G' )
		type = LECROY_WR2_GND;
	else if ( buf[ 0 ] == 'D' && buf[ 1 ] == '1' )
		type = LECROY_WR2_DC_1_MOHM;
	else if ( buf[ 0 ] == 'D' && buf[ 1 ] == '5' )
		type = LECROY_WR2_DC_50_OHM;
	else if ( buf[ 0 ] == 'O' )          /* overload with 50 ohm DC coupling */
	{
		type = LECROY_WR2_DC_50_OHM;
		print( SEVERE, "Signal overload detected for channel '%s', input "
			   "automatically disconnected.\n",
			   LECROY_WR2_Channel_Names[ channel ] );
	}

	fsc2_assert( type != LECROY_WR2_INVALID_COUPL );  /* call me paranoid... */

	return lecroy_wr2.coupling[ channel ] = type;
}


/*------------------------------------------------------*
 * Function for setting the coupling type for a channel
 *------------------------------------------------------*/

bool lecroy_wr2_set_coupling( int channel,
							  int type )
{
    char cmd[ 30 ];
	char const *cpl[ ] = { "A1M", "D1M", "D50", "GND" };


	fsc2_assert( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX );
	fsc2_assert( type >= LECROY_WR2_AC_1_MOHM && type <= LECROY_WR2_GND );

	sprintf( cmd, "C%1d:CPL %s", channel + 1, cpl[ type ] );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------------*
 * Function for determining the bandwidth limiter of one of the channels
 *-----------------------------------------------------------------------*/

int lecroy_wr2_get_bandwidth_limiter( int channel )
{
	char buf[ 30 ] = "BWL?";
	long length = 30;
	int mode = -1;
	char *ptr;
	const char *delim = " ";
	int ch;


	fsc2_assert( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX );

	lecroy_wr2_talk( ( const char * ) buf, buf, &length );
	buf[ length - 1 ] = '\0';

	/* We have to distinguish two cases: if the global bandwith limiter is
	   on or if all channels have the same limiter setting only a single
	   value gets returned, otherwise the setting for each channel will be
	   reported as a list of comma separated channel-value pairs */

	if ( ! strchr( buf, ',' ) )
	{
		size_t i;


		if ( buf[ 1 ] == 'F' )           /* OFF */
			mode = LECROY_WR2_BWL_OFF;
		else if ( buf[ 1 ] == 'N' )      /* ON */
			mode = LECROY_WR2_BWL_ON;
		else if ( buf[ 0 ] == '2' )      /* 200MHZ */
			mode = LECROY_WR2_BWL_200MHZ;

		fsc2_assert( mode >= 0 );

		for ( i = 0; i <= LECROY_WR2_CH_MAX; i++ )
			lecroy_wr2.bandwidth_limiter[ i ] = mode;

		return mode;
	}

	if ( ( ptr = strtok( buf, delim ) ) == NULL )
	{
		print( FATAL, "Can't determine bandwidth limiter settings.\n" );
		THROW( EXCEPTION );
	}

	delim = ",";

	do
	{
		if ( sscanf( ptr + 1, "%d", &ch ) != 1 ||
			 ( ptr = strtok( NULL, delim ) ) == NULL )
		{
			print( FATAL, "Can't determine bandwidth limiter settings.\n" );
			THROW( EXCEPTION );
		}

		fsc2_assert( --ch >= LECROY_WR2_CH1 && ch <= LECROY_WR2_CH_MAX );
		
		if ( ptr[ 1 ] == 'F' )           /* OFF */
			mode = LECROY_WR2_BWL_OFF;
		else if ( ptr[ 1 ] == 'N' )      /* ON */
			mode = LECROY_WR2_BWL_ON;
		else if ( ptr[ 0 ] == '2' )      /* 200MHZ */
			mode = LECROY_WR2_BWL_200MHZ;

		fsc2_assert( mode >= 0 );

		lecroy_wr2.bandwidth_limiter[ ch ] = mode;
	} while ( ( ptr = strtok( NULL, delim ) ) != NULL );

	return lecroy_wr2.bandwidth_limiter[ channel ];
}


/*--------------------------------------------------------------------*
 * Function for setting the bandwidth limiter for one of the channels
 *--------------------------------------------------------------------*/

bool lecroy_wr2_set_bandwidth_limiter( int channel,
									   int bwl )
{
	char buf[ 50 ] = "GBWL?";
	long length;
	int i;


	fsc2_assert( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX );
	fsc2_assert( bwl >= LECROY_WR2_BWL_OFF && bwl <= LECROY_WR2_BWL_200MHZ );


	/* We first need to check if the global bandwidth limiter is on or off. */

	lecroy_wr2_talk( ( const char * ) buf, buf, &length );
	buf[ length - 1 ] = '\0';

	fsc2_assert( buf[ 1 ] == 'N' || buf[ 1 ] == 'F' );

	/* If it's on and we are supposed to switch the channels bandwidth limit
	   also to on nothing really needs to be done (beside setting the internal
	   variable accordingly) */

	if ( buf[ 1 ] == 'N' && bwl == LECROY_WR2_BWL_ON )
	{
		lecroy_wr2.bandwidth_limiter[ channel ] = LECROY_WR2_BWL_ON;
		return OK;
	}

	/* If the global bandwidth limiter is off we only have to set up the
	   channel we're really interested in and are done */

	if ( buf[ 1 ] == 'F' )
	{
		sprintf( buf, "BWL C%d,", channel + 1 );
		if ( bwl == LECROY_WR2_BWL_OFF )
			strcat( buf, "OFF" );
		else if ( bwl == LECROY_WR2_BWL_ON )
			strcat( buf, "ON" );
		else
			strcat( buf, "200MHZ" );
		
		if ( gpib_write( lecroy_wr2.device, buf, strlen( buf ) ) == FAILURE )
			lecroy_wr2_gpib_failure( );

		return OK;
	}

	/* Otherwise we have to switch the global bandwidth limiter off and set
	   the individual bandwidth limters to on for all channels we're not
	   interested in and set up the remaining channel to what the user asked
	   us to */

	strcpy( buf, "GBWL OFF" );
	if ( gpib_write( lecroy_wr2.device, buf, strlen( buf ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	strcpy( buf, "BWL " );

	for ( i = 0; i <= LECROY_WR2_CH_MAX; i++ )
	{
		sprintf( buf + strlen( buf ), "C%d,", i );
		if ( i != channel || bwl == LECROY_WR2_BWL_ON )
			strcat( buf, "ON," );
		else if ( bwl == LECROY_WR2_BWL_ON )
			strcat( buf, "OFF," );
		else
			strcat( buf, "200MHZ," );
	}

	buf[ strlen( buf ) - 2 ] = '\0';

	if ( gpib_write( lecroy_wr2.device, buf, strlen( buf ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*-------------------------------------------------------------*
 * Function for finding out the current trigger source channel
 *-------------------------------------------------------------*/

int lecroy_wr2_get_trigger_source( void )
{
	char reply[ 100 ];
	long length = 100;
	int src = LECROY_WR2_UNDEF;
	char *ptr = reply + 7;


	lecroy_wr2_talk( "TRSE?", reply, &length );
	reply[ length - 1 ] = '\0';

	if ( strncmp( reply, "STD,SR,", 7 ) &&
		 strncmp( reply, "EDGE,SR,", 8 ) )
	{
		print( SEVERE, "Non-standard mode trigger, switching to standard "
			   "edge trigger on to LINe input\n" );
		return lecroy_wr2_set_trigger_source( LECROY_WR2_LIN );
	}

	if ( *ptr == ',' )
		ptr++;

	if ( *ptr == 'C' )
		sscanf( ++ptr, "%d", &src );
	else if ( *ptr == 'L' )
		src = LECROY_WR2_LIN;
	else if ( *ptr == 'E' && ptr[ 2 ] == '1' )
		src = LECROY_WR2_EXT10;
	else if ( *ptr == 'E' && ptr[ 2 ] != '1' )
		src = LECROY_WR2_EXT;

	fsc2_assert( src != LECROY_WR2_UNDEF );

	return lecroy_wr2.trigger_source = src;
}


/*---------------------------------------------------------*
 * Function for setting the current trigger source channel
 *---------------------------------------------------------*/

bool lecroy_wr2_set_trigger_source( int channel )
{
	char cmd[ 40 ] = "TRSE STD,SR,";


	fsc2_assert( ( channel >= LECROY_WR2_CH1 &&
				   channel <= LECROY_WR2_CH_MAX ) ||
				 channel == LECROY_WR2_LIN        ||
				 channel == LECROY_WR2_EXT        ||
				 channel == LECROY_WR2_EXT10 );

	if ( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX )
		sprintf( cmd + 11, "C%1d", channel + 1 );
	else if ( channel == LECROY_WR2_LIN )
		strcat( cmd, "LINE" );
	else if ( channel == LECROY_WR2_EXT )
		strcat( cmd, "EX" );
	else
		strcat( cmd, "EX10" );

	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*-------------------------------------------------------------------*
 * Function for finding out the trigger level of one of the channels
 *-------------------------------------------------------------------*/

double lecroy_wr2_get_trigger_level( int channel )
{
	char buf[ 30 ];
	long length = 30;


	fsc2_assert( ( channel >= LECROY_WR2_CH1 &&
				   channel <= LECROY_WR2_CH_MAX ) ||
				 channel == LECROY_WR2_EXT || channel == LECROY_WR2_EXT10 );

	if ( ( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX )
	   )
		sprintf( buf, "C%1d:TRLV?", channel + 1 );
	else if ( channel == LECROY_WR2_EXT )
		strcpy( buf, "EX:TRLV?" );
	else
		strcpy( buf, "EX10:TRLV?" );

	lecroy_wr2_talk( ( const char * ) buf, buf, &length );
	buf[ length - 1 ] = '\0';
	return lecroy_wr2.trigger_level[ channel ] == T_atod( buf );
}


/*----------------------------------------------------------------*
 * Function for setting the trigger slope for one of the channels
 *----------------------------------------------------------------*/

bool lecroy_wr2_set_trigger_level( int    channel,
								   double level )
{
	char cmd[ 40 ];


	fsc2_assert( ( channel >= LECROY_WR2_CH1 &&
				   channel <= LECROY_WR2_CH_MAX ) ||
				 channel == LECROY_WR2_EXT        ||
				 channel == LECROY_WR2_EXT10 );

	if ( ( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX )
	   )
		sprintf( cmd, "C%1d:TRLV ", channel + 1 );
	else if ( channel == LECROY_WR2_EXT )
		strcpy( cmd, "EX:TRLV " );
	else
		strcpy( cmd, "EX10:TRLV " );

	gcvt( level, 6, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*-------------------------------------------------------------------*
 * Function for finding out the trigger slope of one of the channels
 *-------------------------------------------------------------------*/

bool lecroy_wr2_get_trigger_slope( int channel )
{
	char buf[ 30 ];
	long length = 30;


	fsc2_assert( ( channel >= LECROY_WR2_CH1 &&
				   channel <= LECROY_WR2_CH_MAX ) ||
				 channel == LECROY_WR2_LIN        ||
				 channel == LECROY_WR2_EXT        ||
				 channel == LECROY_WR2_EXT10 );

	if ( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX )
		sprintf( buf, "C%1d:TRSL?", channel + 1 );
	else if ( channel == LECROY_WR2_EXT )
		strcpy( buf, "EX:TRSL?" );
	else
		strcpy( buf, "EX10:TRSL?" );

	lecroy_wr2_talk( ( const char *) buf, buf, &length );

	lecroy_wr2.trigger_slope[ channel ] =
		                                 buf[ 0 ] == 'P' ? POSITIVE : NEGATIVE;

	return lecroy_wr2.trigger_slope[ channel ];
}


/*----------------------------------------------------------------*
 * Function for setting the trigger slope for one of the channels
 *----------------------------------------------------------------*/

bool lecroy_wr2_set_trigger_slope( int channel,
								   bool slope )
{
	char cmd[ 40 ];


	fsc2_assert( ( channel >= LECROY_WR2_CH1 &&
				   channel <= LECROY_WR2_CH_MAX ) ||
				 channel == LECROY_WR2_LIN        ||
				 channel == LECROY_WR2_EXT        ||
				 channel == LECROY_WR2_EXT10 );

	if ( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX )
		sprintf( cmd, "C%1d:TRSL ", channel + 1 );
	else if ( channel == LECROY_WR2_LIN )
		strcpy( cmd, "LINE:TRSL " );
	else if ( channel == LECROY_WR2_EXT )
		strcpy( cmd, "EX:TRSL " );
	else
		strcpy( cmd, "EX10:TRSL " );

	strcat( cmd, slope ? "POS" : "NEG" );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------------*
 * Function for finding out the trigger coupling for one of the channels
 *-----------------------------------------------------------------------*/

int lecroy_wr2_get_trigger_coupling( int channel )
{
	char buf[ 40 ];
	long length = 40;
	int cpl = -1;


	fsc2_assert( ( channel >= LECROY_WR2_CH1 &&
				   channel <= LECROY_WR2_CH_MAX ) ||
				 channel == LECROY_WR2_EXT        ||
				 channel == LECROY_WR2_EXT10 );

	if ( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX )
		sprintf( buf, "C%1d:TRCP?", channel + 1 );
	else if ( channel == LECROY_WR2_EXT )
		strcpy( buf, "EX:TRCP?" );
	else
		strcpy( buf, "EX10:TRCP?" );

	lecroy_wr2_talk( ( const char *) buf, buf, &length );

	switch ( buf[ 0 ] )
	{
		case 'A' :
			cpl = LECROY_WR2_TRG_CPL_AC;
			break;

		case 'D' :
			cpl = LECROY_WR2_TRG_CPL_DC;
			break;

		case 'L' :
			cpl = LECROY_WR2_TRG_CPL_LF_REJ;
			break;

		case 'H' :
			cpl = LECROY_WR2_TRG_CPL_HF_REJ;
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	lecroy_wr2.trigger_coupling[ channel ] = cpl;

	return cpl;
}


/*-------------------------------------------------------------------*
 * Function for setting the trigger coupling for one of the channels
 *-------------------------------------------------------------------*/

int lecroy_wr2_set_trigger_coupling( int channel,
									 int cpl )
{
	char cmd[ 40 ];
	const char *cpl_str[ ] = { "AC", "DC", "LFREJ", "HFREJ" };


	fsc2_assert( ( channel >= LECROY_WR2_CH1 &&
				   channel <= LECROY_WR2_CH_MAX ) ||
				 channel == LECROY_WR2_EXT        ||
				 channel == LECROY_WR2_EXT10 );
	fsc2_assert( cpl >= LECROY_WR2_TRG_AC && cpl <= LECROY_WR2_TRG_HF_REJ );

	if ( channel >= LECROY_WR2_CH1 && channel <= LECROY_WR2_CH_MAX )
		sprintf( cmd, "C%1d:TRCP ", channel + 1 );
	else if ( channel == LECROY_WR2_EXT )
		strcpy( cmd, "EX:TRCP " );
	else
		strcpy( cmd, "EX10:TRCP " );

	strcat( cmd, cpl_str[ cpl ] );

	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*---------------------------------------------------*
 * Function for finding out the current trigger mode
 *---------------------------------------------------*/

int lecroy_wr2_get_trigger_mode( void )
{
	char buf[ 40 ];
	long length = 40;
	int mode = -1;


	lecroy_wr2_talk( "TRMD?", buf, &length );

	if ( buf[ 0 ] == 'A' )
		mode = LECROY_WR2_TRG_MODE_AUTO;
	else if ( buf[ 0 ] == 'N' )
		mode = LECROY_WR2_TRG_MODE_NORMAL;
	else if ( buf[ 1 ] == 'S' )
		mode = LECROY_WR2_TRG_MODE_SINGLE;
	else if ( buf[ 1 ] == 'T' )
		mode = LECROY_WR2_TRG_MODE_STOP;

	fsc2_assert( 1 == 0 );

	return lecroy_wr2.trigger_mode = mode;
}


/*---------------------------------------*
 * Function for setting the trigger mode 
 *---------------------------------------*/

int lecroy_wr2_set_trigger_mode( int mode )
{
	char cmd[ 40 ] = "TRMD ";
	const char *mode_str[ ] = { "AUTO", "NORM", "SINGLE", "STOP" };


	fsc2_assert( mode >= LECROY_WR2_TRG_MODE_AUTO &&
				 mode <= LECROY_WR2_TRG_MODE_STOP );

	strcat( cmd, mode_str[ mode ] );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*----------------------------------------------------*
 * Function for determining the current trigger delay
 *----------------------------------------------------*/

double lecroy_wr2_get_trigger_delay( void )
{
	char reply[ 40 ];
	long length = 40;


	lecroy_wr2_talk( "TRDL?", reply, &length );
	reply[ length - 1 ] = '\0';
	lecroy_wr2.trigger_delay = T_atod( reply );

	/* Positive delays (i.e. when pretrigger is on) get returned from the
	   device as a percentage of the full horizontal screen while for
	   negative values it's already the delay time */

	if ( lecroy_wr2.trigger_delay > 0.0 )
		lecroy_wr2.trigger_delay = 0.1 * lecroy_wr2.timebase;

	return lecroy_wr2.trigger_delay;
}


/*----------------------------------------*
 * Function for setting the trigger delay 
 *----------------------------------------*/

bool lecroy_wr2_set_trigger_delay( double delay )
{
	char cmd[ 40 ] = "TRDL ";


	/* For positive delay (i.e. pretrigger) the delay must be set as a
	   percentage of the full horizontal screen width */

	if ( delay > 0.0 )
		delay = 10.0 * delay / lecroy_wr2.timebase;

	gcvt( delay, 8, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return OK;
}


/*---------------------------------------------------------*
 * Function for checking if a certain channel is displayed
 *---------------------------------------------------------*/

bool lecroy_wr2_is_displayed( int ch )
{
	char cmd[ 30 ];
	long length = 30;


	if ( ch >= LECROY_WR2_CH1 && ch <= LECROY_WR2_CH_MAX )
		sprintf( cmd, "C%d:TRA?", ch - LECROY_WR2_CH1 + 1 );
	else if ( ch >= LECROY_WR2_TA && ch <= LECROY_WR2_TB )
		sprintf( cmd, "T%c:TRA?", ch - LECROY_WR2_TA + 'A' );
	else if ( ch >= LECROY_WR2_M1 && ch <= LECROY_WR2_M4 )
		sprintf( cmd, "M%d:TRA?", ch - LECROY_WR2_M1 + 1 );
	else
	{
		print( FATAL, "Internal error detected.\n" );
		THROW( EXCEPTION );
	}

	lecroy_wr2_talk( ( const char * ) cmd, cmd, &length );
	return cmd[ 1 ] == 'N';
}


/*----------------------------------------------------*
 * Function to switch  display of a channel on or off
 *----------------------------------------------------*/

bool lecroy_wr2_display( int ch,
						 int on_off )
{
	char cmd[ 30 ];
		

	if ( ch >= LECROY_WR2_CH1 && ch <= LECROY_WR2_CH_MAX )
		sprintf( cmd, "C%d:TRA ", ch - LECROY_WR2_CH1 + 1 );
	else if ( ch >= LECROY_WR2_TA && ch <= LECROY_WR2_TD )
		sprintf( cmd, "E%c:TRA ", ch - LECROY_WR2_TA + 'A' );
	else if ( ch >= LECROY_WR2_M1 && ch <= LECROY_WR2_M4 )
		sprintf( cmd, "M%d:TRA ", ch - LECROY_WR2_M1 + 1 );
	else
	{
		print( FATAL, "Internal error detected.\n" );
		THROW( EXCEPTION );
	}

	strcat( cmd, on_off ? "ON" : "OFF" );

	if ( on_off &&
		 lecroy_wr2.num_used_channels >= LECROY_WR2_MAX_USED_CHANNELS )
	{
		print( FATAL, "Can't switch on another trace, there are already as "
			   "many as possible (%d) displayed.\n",
			   LECROY_WR2_MAX_USED_CHANNELS );
		THROW( EXCEPTION );
	}

	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	if ( on_off )
	{
		lecroy_wr2.num_used_channels++;
		lecroy_wr2.channels_in_use[ ch ] = SET;
	}
	else
	{
		lecroy_wr2.num_used_channels--;
		lecroy_wr2.channels_in_use[ ch ] = UNSET;
	}

	return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void lecroy_wr2_finished( void )
{
	gpib_local( lecroy_wr2.device );
}


/*-----------------------------------------*
 * Function for starting a new acquisition
 *-----------------------------------------*/

void lecroy_wr2_start_acquisition( void )
{
	int ch;
	char cmd[ 100 ];
	bool do_averaging = UNSET;


	/* Stop the digitizer (also switches to "STOPPED" trigger mode) */

	if ( gpib_write( lecroy_wr2.device, "STOP", 4 ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	/* Set up the parameter to be used for averaging for the function channels
	   (as far as they have been set by the user) */

	for ( ch = LECROY_WR2_TA; ch <= LECROY_WR2_TD; ch++ )
	{
		if ( ! lecroy_wr2.is_avg_setup[ ch ] )
			continue;

		do_averaging = SET;

		snprintf( cmd, 100, "T%c:DEF EQN,AVGS CH%ld,MAXPTS,%ld,SWEEPS,%ld",
				  'A' + LECROY_WR2_TA - ch,
				  lecroy_wr2.source_ch[ ch ] - LECROY_WR2_CH1 + 1,
				  10 * ( lecroy_wr2.is_interleaved ?
						 lecroy_wr2.cur_hres->ppd_ris :
						 lecroy_wr2.cur_hres->ppd ),
				  lecroy_wr2.num_avg[ ch ] );

		if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) )
			 == FAILURE )
		lecroy_wr2_gpib_failure( );
	}

	/* Switch digitizer back on to running state by switching to a trigger
	   mode where the digitizer is running (i.e. typically NORM, but, if
	   the user requested it, also AUTO, or, if there's no averaging setup,
	   even SINGLE mode will do) */

	strcpy( cmd, "TRMD NORM" );
	if ( ! do_averaging &&
		 lecroy_wr2.trigger_mode == LECROY_WR2_TRG_MODE_SINGLE )
		strcpy( cmd + 5, "SINGLE" );
	else if ( lecroy_wr2.trigger_mode == LECROY_WR2_TRG_MODE_AUTO )
		strcpy( cmd + 5, "AUTO" );
	else
		lecroy_wr2.trigger_mode == LECROY_WR2_TRG_MODE_NORM;

	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	/* Reset the bits in the word that tells us later that the data in the
	   corresponding channel are ready to be fetched */

	can_fetch &= ~ ( LECROY_WR2_PROC_DONE_TA | LECROY_WR2_PROC_DONE_TB |
					 LECROY_WR2_PROC_DONE_TC | LECROY_WR2_PROC_DONE_TD |
					 LECROY_WR2_SIGNAL_ACQ );
}


/*------------------------------------------------------------*
 * Function for fetching data from the digitizer - the calling
 * function than does the remaining specific manipulations on
 * these data
 *------------------------------------------------------------*/

static void lecroy_wr2_get_prep( int              ch,
								 Window_T *       w,
								 unsigned char ** data,
								 long *           length,
								 double *         gain,
								 double *         offset,
								 const char *     what )
{
	unsigned int bit_to_test;
	char cmd[ 100 ];
	char ch_str[ 3 ];
	bool is_mem_ch = UNSET;


	CLOBBER_PROTECT( data );
	CLOBBER_PROTECT( bit_to_test );

	/* Figure out which channel is to be used and set a few variables
	   needed later accordingly */

	if ( ch >= LECROY_WR2_CH1 && ch <= LECROY_WR2_CH_MAX )
	{
		if ( ! lecroy_wr2.is_displayed[ ch ] )
		{
			print( FATAL, "Can't fetch %s for channel %s since it isn't "
				   "switched on.\n", what, LECROY_WR2_Channel_Names[ ch ] );
			THROW( EXCEPTION );
		}

		bit_to_test = LECROY_WR2_SIGNAL_ACQ;
		sprintf( ch_str, "C%d", ch - LECROY_WR2_CH1 + 1 );
	}
	else if ( ch >= LECROY_WR2_M1 && ch <= LECROY_WR2_M4 )
	{
		is_mem_ch = SET;
		sprintf( ch_str, "M%d", ch - LECROY_WR2_M1 + 1 );
	}
	else if ( ch >= LECROY_WR2_TA && ch <= LECROY_WR2_TD )
	{
		if ( ! lecroy_wr2.is_avg_setup[ ch ] )
		{
			print( FATAL, "Averaging has not been initialized for "
				   "channel %s.\n", LECROY_WR2_Channel_Names[ ch ] );
			THROW( EXCEPTION );
		}

		bit_to_test = LECROY_WR2_PROC_DONE( ch );
		sprintf( ch_str, "T%c", ch - LECROY_WR2_TA + 'A' );
	}
	else
	{
		fsc2_assert( 1 == 0 );
	}

	/* Set up the number of points to be fetched */

	if ( w != NULL )
		sprintf( cmd, "WFSU SP,0,NP,%ld,FP,%ld,SN,0",
				 w->num_points, w->start_num );
	else
		sprintf( cmd, "WFSU SP,0,NP,0,FP,0,SN,0" );

	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	/* When a non-memory curve is to be fetched and the acquisition isn't
	   finished yet poll until the bit that tells that the acquisition for
	   the requested channel is finished has become set */

	if ( ! is_mem_ch &&
		 ! ( can_fetch & bit_to_test ) )
	{
		while ( ! ( ( can_fetch |= lecroy_wr2_get_inr( ) ) & bit_to_test ) )
		{
			stop_on_user_request( );
			fsc2_usleep( 20000, UNSET );
		}
	}

	TRY
	{
		/* Ask the device for the data */

		strcpy( cmd, ch_str );
		strcat( cmd, ":WF? DAT1" );
		if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) )
			 == FAILURE )
			lecroy_wr2_gpib_failure( );

		/* And fetch 'em and cut back on the number of data - the device
		   seems to send 4 bytes too many, at least when asked for a
		   complete curve */

		*data = NULL;
		*data = lecroy_wr2_get_data( length );
		*length /= 2;          /* we got word sized (16 bit) data, LSB first */

		if ( w == NULL && *length > lecroy_wr2_curve_length( ) )
			*length = lecroy_wr2_curve_length( );

		/* Get the gain factor and offset for the date we just fetched */

		*gain = lecroy_wr2_get_float_value( ch, "VERTICAL_GAIN" );
		*offset = lecroy_wr2_get_float_value( ch, "VERTICAL_OFFSET" );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( *data != NULL )
			T_free( *data );
		RETHROW( );
	}
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void lecroy_wr2_get_curve( int        ch,
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

	lecroy_wr2_get_prep( ch, w, &data, length, &gain, &offset, "curve" );

	/* Calculate the voltages from the data, data are two byte (LSB first),
	   two's complement integers, which then need to be scaled by gain and
	   offset. */

	*array = DOUBLE_P T_malloc( *length * sizeof **array );

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

double lecroy_wr2_get_area( int        ch,
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

	lecroy_wr2_get_prep( ch, w, &data, &length, &gain, &offset, "area" );

	/* Calculate the voltages from the data, data are two byte (LSB first),
	   two's complement integers, which then need to be scaled by gain and
	   offset. */

	for ( i = 0, dp = data; i < length; dp += 2, i++ )
	{
		val = dp[ 0 ] + 0x100 * dp[ 1 ];

		if ( dp[ 1 ] & 0x80 ) 
			val -= 0x10000;

		area += gain * val - offset
	}

	T_free( data );

	return area;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double lecroy_wr2_get_amplitude( int        ch,
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

	lecroy_wr2_get_prep( ch, w, &data, &length, &gain, &offset,
							"amplitude" );

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


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void lecroy_wr2_free_running( void )
{
}


/*----------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

void lecroy_wr2_copy_curve( long src,
							long dest )
{
	char cmd[ 100 ] = "STO ";


	fsc2_assert( ( src >= LECROY_WR2_CH1 && src <= LECROY_WR2_CH_MAX ) ||
				 ( src >= LECROY_WR2_TA && src <= LECROY_WR2_TD ) );
	fsc2_assert( dest >= LECROY_WR2_M1 && dest <= LECROY_WR2_M4 );


	if ( src >= LECROY_WR2_CH1 && src <= LECROY_WR2_CH_MAX )
		sprintf( cmd + strlen( cmd ), "C%ld,", src - LECROY_WR2_CH1 + 1 );
	else
		sprintf( cmd + strlen( cmd ), "T%c,",
				 ( char ) ( src - LECROY_WR2_TA + 'A' ) );

	sprintf( cmd + strlen( cmd ), "M%ld", dest - LECROY_WR2_M1 + 1 );

	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );
}


/*----------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static unsigned char *lecroy_wr2_get_data( long * len )
{
	unsigned char *data;
	char len_str[ 10 ];


	/* First thing we read is something like "#[0-9]" where the number
	   following the '#' is the number of bytes to be read next */

	*len = 2;
	if ( gpib_read( lecroy_wr2.device, len_str, len ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	len_str [ *len - 1 ] = '\0';
	*len = T_atol( len_str + 1 );

	fsc2_assert( *len > 0 );

	/* Now get the number of bytes to read */

	if ( gpib_read( lecroy_wr2.device, len_str, len ) == FAILURE )
		lecroy_wr2_gpib_failure( );
	
	len_str[ *len - 1 ] = '\0';
	*len = T_atol( len_str );

	fsc2_assert( *len > 0 );

	/* Obtain enough memory and then read the real data */

	data = UCHAR_P T_malloc( *len );

	if ( gpib_read( lecroy_wr2.device, data, len ) == FAILURE )
		lecroy_wr2_gpib_failure( );

	return data;
}


/*----------------------------------------------------------------------*
 * Function for obtaining an integer value from the waveform descriptor
 *---------------------------------------------------------------------*/
#if 0
static int lecroy_wr2_get_int_value( int          ch,
									 const char * name )
{
	char cmd[ 100 ];
	long length = 100;


	if ( ch >= LECROY_WR2_CH1 && ch <= LECROY_WR2_CH_MAX )
		sprintf( cmd, "C%d:INSP? '%s'", ch - LECROY_WR2_CH1 + 1, name );
	else if ( ch >= LECROY_WR2_M1 && ch <= LECROY_WR2_M4 )
		sprintf( cmd, "M%c:INSP? '%s'", ch - LECROY_WR2_M1 + 1, name );
	else if ( ch >= LECROY_WR2_TA && ch <= LECROY_WR2_TD )
		sprintf( cmd, "T%c:INSP? '%s'", ch - LECROY_WR2_TA + 'A', name );
	else
	{
		fsc2_assert( 1 == 0 );
	}

	lecroy_wr2_talk( cmd, cmd, &length );
	cmd[ length - 1 ] = '\0';

	return T_atoi( cmd );
}
#endif

/*-------------------------------------------------------------------*
 * Function for obtaining a float value from the waveform descriptor
 *-------------------------------------------------------------------*/

static double lecroy_wr2_get_float_value( int          ch,
										  const char * name )
{
	char cmd[ 100 ];
	long length = 100;


	if ( ch >= LECROY_WR2_CH1 && ch <= LECROY_WR2_CH_MAX )
		sprintf( cmd, "C%d:INSP? '%s'", ch - LECROY_WR2_CH1 + 1, name );
	else if ( ch >= LECROY_WR2_M1 && ch <= LECROY_WR2_M4 )
		sprintf( cmd, "M%c:INSP? '%s'", ch - LECROY_WR2_M1 + 1, name );
	else if ( ch >= LECROY_WR2_TA && ch <= LECROY_WR2_TD )
		sprintf( cmd, "T%c:INSP? '%s'", ch - LECROY_WR2_TA + 'A', name );
	else
	{
		fsc2_assert( 1 == 0 );
	}

	lecroy_wr2_talk( cmd, cmd, &length );
	cmd[ length - 1 ] = '\0';

	return T_atod( cmd );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool lecroy_wr2_command( const char * cmd )
{
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy_wr2_gpib_failure( );
	return OK;
}


/*------------------------------------------------------------------------*
 * Function fetches (thereby reseting!) the INR register from the device.
 * It sets all bits in the INR element of the structure for the device
 * where a bit in the INR is set. Functions making use of the fact that
 * a bit is set must reset it when the action they take invalidate the
 * condition that led to the bit becoming set.
 *-----------------------------------------------------------------------*/

static unsigned int lecroy_wr2_get_inr( void )
{
	char reply[ 10 ] = "INR?";
	long length = 10;


	lecroy_wr2_talk( "INR?", reply, &length );
	reply[ length - 1 ] = '\0';
	return ( unsigned int ) T_atoi( reply );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool lecroy_wr2_talk( const char * cmd,
							 char *       reply,
							 long *       length )
{
	if ( gpib_write( lecroy_wr2.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( lecroy_wr2.device, reply, length ) == FAILURE )
		lecroy_wr2_gpib_failure( );
	return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void lecroy_wr2_gpib_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
