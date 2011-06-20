/*
 *  Copyright (C) 1999-2010 Anton Savitsky / Jens Thoms Toerring
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


#include "ag54830b.h"


static void ag54830b_failure( void );

static bool in_init = UNSET;


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool
ag54830b_init( const char * name )
{
	if ( gpib_init_device( name, &ag54830b.device ) == FAILURE )
        return FAIL;

	gpib_clear_device( ag54830b.device );

	ag54830b_command( ":WAV:BYT LSBF\n" );   /* set byte order */
	ag54830b_command( ":WAV:FORM WORD\n" );  /* set transfer format */
	ag54830b_command( ":WAV:VIEW ALL\n" );   /* set all acquared data */
	ag54830b_command( ":TIM:REF LEFT\n" );   /* set reference point to left */
	ag54830b_command( ":ACQ:COMP 100\n" );

	return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool
ag54830b_command( const char * cmd )
{
	if ( gpib_write( ag54830b.device, cmd, strlen( cmd ) ) == FAILURE )
		ag54830b_failure( );

	fsc2_usleep( 4000, UNSET );

	return OK;
}

/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool
ag54830b_command_retry( const char * cmd )
{
	/* Try to write and on failure wait 4ms and retry once more */

	if ( gpib_write( ag54830b.device, cmd, strlen( cmd ) ) == FAILURE )
	{
		print( SEVERE, "Trouble sending data to device, trying again.\n" );
		fsc2_usleep( 4000, UNSET );
		if ( gpib_write( ag54830b.device, cmd, strlen( cmd ) ) == FAILURE )
			ag54830b_failure( );
	}

	fsc2_usleep( 4000, UNSET );

	return OK;
}

/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool
ag54830b_talk( const char * cmd,
			   char       * reply,
			   long       * length )
{
	if ( gpib_write( ag54830b.device, cmd, strlen( cmd ) ) == FAILURE )
		ag54830b_failure( );

	fsc2_usleep( 4000, UNSET );

	if ( gpib_read( ag54830b.device, reply, length ) == FAILURE )
		ag54830b_failure( );

	fsc2_usleep( 4000, UNSET );

	return OK;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void
ag54830b_failure( void )
{
	print( FATAL, "Communication with Agilent failed.\n" );
	THROW( EXCEPTION );
}


/*---------------------------------*
 * Returns the digitizers timebase
 *---------------------------------*/

double
ag54830b_get_timebase( void )
{
	char reply[ 30 ];
	long length = sizeof reply;


	ag54830b_talk( "TIMEBASE:SCALE?\n", reply, &length );
	reply[ length - 1 ] = '\0';
	return T_atod( reply );
}


/*------------------------------------*
 * Sets the timebase of the digitizer
 *------------------------------------*/

void
ag54830b_set_timebase( double timebase )
{
	char cmd[ 40 ];


	sprintf( cmd, "TIMEBASE:SCALE  %8.3E\n", timebase );
	ag54830b_command( cmd );
}


/*-----------------------------------------*
 * Function returns the number of averages
 *-----------------------------------------*/

long
ag54830b_get_num_avg( void )
{
	char reply[ 30 ];
	long length = sizeof reply;


	if ( ag54830b_get_acq_mode( ) == 1 )
	{
		ag54830b_talk(":ACQ:AVERAGE:COUNT?\n", reply, &length );
		reply[ length - 1 ] = '\0';
		return T_atol( reply );
	}
	else                            	/* digitizer is in sample mode */
		return 1;
}

/*--------------------------------------*
 * Function sets the number of averages
 *--------------------------------------*/

void
ag54830b_set_num_avg( long num_avg )
{
	char cmd[ 30 ];


	/* With 1 as the number of acquisitions switch to sample mode, for all
	   others set the number of acquisitions and switch to average mode */

	if ( num_avg == 1 )
		ag54830b_command( ":ACQ:AVER OFF\n" );
	else
	{
		sprintf( cmd, ":ACQ:AVER:COUN %ld\n", num_avg );
		ag54830b_command( cmd );
		ag54830b_command( ":ACQ:AVER ON\n" );
	}
}


/*-------------------------------------------------------------------------*
 * Function returns the data acquisition mode. If the digitizer is neither
 * in average nor in sample mode, it is switched to sample mode.
 *-------------------------------------------------------------------------*/

int
ag54830b_get_acq_mode( void )
{
	char reply[ 30 ];
	long length = sizeof reply;


	ag54830b_talk( ":ACQ:AVER?\n", reply, &length );
	return T_atol( reply );
}


/*-----------------------------------------------*
 * Function returns if acquisition is complited.
 *-----------------------------------------------*/

void
ag54830b_acq_completed( void )
{
	unsigned char stb = 0;


	if ( ag54830b.acquisition_is_running )
	{
		while ( 1 )
		{
			fsc2_usleep( 20000, UNSET );
			if ( gpib_serial_poll( ag54830b.device, &stb ) == FAILURE )
				ag54830b_failure( );

			if ( stb & 0x20 )
			{
				ag54830b.acquisition_is_running = UNSET;
				fsc2_usleep( 10000, UNSET );
				break;
			}

			stop_on_user_request( );
		}
	}
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
ag54830b_get_curve( int       channel,
					double ** data,
					long    * length,
					bool      get_scaling )
{
	char cmd[ 50 ];
	char reply[ 32 ];
	long blength = sizeof reply;
	char *buffer = NULL;
	long i;
	double yinc;
	double yorg;
	long bytes;
	char cdata[ 2 ];
	long bytes_to_read;
	char header_str[ 10 ];


	CLOBBER_PROTECT ( buffer );

	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

	*data = NULL;

    /* Set the source of the data */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command_retry( cmd );
		ag54830b.data_source = channel;
	 }

	/* Calculate how long the curve (with header) is going to be and allocate
       enough memory (data are 2-byte integers) and get all the data bytes*/

	ag54830b_command_retry(":WAV:DATA?\n");

	do
	{
		bytes = 1;
		if ( gpib_read( ag54830b.device, cdata, &bytes ) == FAILURE )
			ag54830b_failure( );
	} while ( *cdata != '#' );

	bytes = 1;
	if ( gpib_read( ag54830b.device, cdata, &bytes ) == FAILURE )
		ag54830b_failure( );

	cdata[ 1 ] = '\0';
	bytes_to_read = T_atoi(cdata); /* read header number of bytes */

	if ( gpib_read( ag54830b.device, header_str, &bytes_to_read )
			 													   == FAILURE )
		ag54830b_failure( );

	header_str[ bytes_to_read ] = '\0';
	bytes_to_read = T_atoi( header_str );    /* number of bytes to read */

	if ( bytes_to_read == 0 )
	{
		print( FATAL, "No signal in channel %s.\n",
			   AG54830B_Channel_Names[ channel ] );
		THROW( EXCEPTION );
	}

	buffer = T_malloc( bytes_to_read );

	/* Read all data into the buffer */

	TRY
	{
		if ( gpib_read( ag54830b.device, buffer, &bytes_to_read ) == FAILURE )
			ag54830b_failure( );

		*length = bytes_to_read / 2;
		*data = T_malloc( *length * sizeof **data );

        /* Read termination */

		bytes = 2;
		if ( gpib_read( ag54830b.device, cdata, &bytes ) == FAILURE )
			ag54830b_failure( );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( buffer )
			T_free( buffer );
		if ( *data )
			T_free( *data );
		RETHROW( );
	}

	/* Get scaling */

	yinc = 1.0;
	yorg = 1.0;

	if (    get_scaling
		 && channel >= AG54830B_CH1
		 && channel < NUM_DISPLAYABLE_CHANNELS )
	{
		blength = sizeof reply;
		ag54830b_talk( ":WAV:YINC?\n", reply, &blength ); /* get y-increment */
		reply[ blength - 1 ] = '\0';
		yinc = T_atod( reply );
		fsc2_usleep( 1000, UNSET );

		blength = sizeof reply;
		ag54830b_talk( ":WAV:YOR?\n", reply, &blength );  /* get y-origin */
		reply[ blength - 1 ] = '\0';
		yorg = T_atod( reply );
		fsc2_usleep( 1000, UNSET );
	}

	/* ....and copy them to the final destination (the data are INTEL format
	   2-byte integers, so the following requires sizeof( short ) == 2 and
	   only works on a machine with INTEL format - there got to be better ways
	   to do it...) Also scale data so that we get the real measured
	   voltage. */

	fsc2_assert( sizeof( short ) == 2 );

	for ( i = 0; i < bytes_to_read / 2; i++ )
		*( *data + i ) =
					 ( yinc * ( double ) * ( ( short * ) buffer + i ) ) + yorg;

	T_free( buffer );
}


/*--------------------------------------------*
 * Function returns the current record length
 *--------------------------------------------*/

long
ag54830b_get_record_length( void )
{
    char reply[ 30 ];
    long length = sizeof reply;


	ag54830b_talk( ":ACQ:POIN?\n", reply, &length );
    reply[ length - 1 ] = '\0';

    return T_atol( reply );
}


/*---------------------------------*
 * Function sets the record length
 *---------------------------------*/

void
ag54830b_set_record_length( long num_points )
{
    char cmd[ 100 ];


	sprintf( cmd, ":ACQ:POINTS %ld\n", num_points );
	ag54830b_command( cmd );
}


/*-----------------------------------------------------------*
 * Returns the sensitivity of the channel passed as argument
 *-----------------------------------------------------------*/

double
ag54830b_get_sens( int channel )
{
    char cmd[ 30 ];
    char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert( channel >= AG54830B_CH1 && channel < NUM_NORMAL_CHANNELS );

	sprintf( cmd, "%s:SCAL?\n", AG54830B_Channel_Names[ channel ] );
	ag54830b_talk( cmd, reply, &length );

    reply[ length - 1 ] = '\0';
	ag54830b.sens[ channel ] = T_atod( reply );

	return ag54830b.sens[ channel ];
}


/*-----------------------------------------------------*
 * Sets the sensitivity of one of a digitizers channel
 *-----------------------------------------------------*/

void
ag54830b_set_sens( int    channel,
				   double sens )
{
    char cmd[ 40 ];
	char reply[ 40 ];
	long length = sizeof reply;


	fsc2_assert( channel >= AG54830B_CH1 && channel < NUM_NORMAL_CHANNELS );

	/* Some digitizers allow high sensitivities only for 1 MOhm input
	   impedance, not for 50 Ohm */

	if ( sens > MIN_SENS50 )
	{
		sprintf( cmd, "CHAN%1d:INP?\n", channel );
		ag54830b_talk( cmd, reply, &length );

		if ( ! strncmp( reply, "DC50", 4 ) )
		{
			if ( in_init )
			{
				print( FATAL, "Sensitivity of %f V for channel %s too low "
					   "with input impedance set to  50 Ohm.\n",
					   AG54830B_Channel_Names[ channel ], sens );
				THROW( EXCEPTION );
			}

			print( SEVERE, "Sensitivity of %f V for channel %s too low "
				   "with input impedance set to 50 Ohm.\n",
				    AG54830B_Channel_Names[ channel ], sens );
			return;
		}
	}

	sprintf( cmd, "%s:SCAL %8.3E\n", AG54830B_Channel_Names[ channel ], sens );
	ag54830b_command( cmd );
}


/*--------------------------------------------------------*
 * Returns the x-origin of the channel passed as argument
 *--------------------------------------------------------*/

double
ag54830b_get_xorigin( int channel )
{
	char cmd[ 30 ];
	char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

	/* Set the source of the data if not set */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command( cmd );
		ag54830b.data_source = channel;
	}

	ag54830b_talk( ":WAV:XOR?\n", reply, &length );
	ag54830b.data_source=channel;
    reply[ length - 1 ] = '\0';

	return T_atod( reply );
}


/*-----------------------------------------------------------*
 * Returns the x-increment of the channel passed as argument
 *-----------------------------------------------------------*/

double
ag54830b_get_xincrement( int channel )
{
	char cmd[ 30 ];
	char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

    /* Set the source of the data if not set */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command( cmd );
		ag54830b.data_source = channel;
	}

	ag54830b_talk(  ":WAV:XINC?\n", reply, &length );
	ag54830b.data_source=channel;
    reply[ length - 1 ] = '\0';

	return T_atod( reply );
}


/*--------------------------------------------------------*
 * Returns the y-origin of the channel passed as argument
 *--------------------------------------------------------*/

double
ag54830b_get_yorigin( int channel )
{
	char cmd[ 30 ];
	char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

	/* Set the source of the data if not set */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command( cmd );
		ag54830b.data_source = channel;
	}

	ag54830b_talk( ":WAV:YOR?\n", reply, &length );
	ag54830b.data_source=channel;
    reply[ length - 1 ] = '\0';

	return T_atod( reply );
}


/*-----------------------------------------------------------*
 * Returns the x-increment of the channel passed as argument
 *----------------=-------------------------------------------*/

double
ag54830b_get_yincrement( int channel )
{
	char cmd[ 30 ];
	char reply[ 30 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

    /* Set the source of the data if not set */

	if ( channel != ag54830b.data_source )
	{
		sprintf( cmd, ":WAV:SOUR %s\n", AG54830B_Channel_Names[ channel ] );
		ag54830b_command( cmd );
		ag54830b.data_source = channel;
	}

	ag54830b_talk(  ":WAV:YINC?\n", reply, &length );
	ag54830b.data_source=channel;
    reply[ length - 1 ] = '\0';

	return T_atod( reply );
}


/*--------------------------------------------*
 * Function tests if a channel is switched on
 *--------------------------------------------*/

bool
ag54830b_display_channel_state( int channel )
{
	char cmd[ 30 ];
    char reply[ 10 ];
    long length = sizeof reply;


	fsc2_assert(    channel >= AG54830B_CH1
				 && channel < NUM_DISPLAYABLE_CHANNELS );

	sprintf( cmd, ":%s:DISP?\n", AG54830B_Channel_Names[ channel ] );
	ag54830b_talk( cmd, reply, &length );

	reply[ length - 1 ] = '\0';
	ag54830b.channel_is_on[ channel ] = T_atoi( reply );
	return ag54830b.channel_is_on[ channel ];
}


/*--------------------------------------------------------------*
 * Sets the trigger position, range of paramters is [0,1] where
 * 0 means no pretrigger while 1 indicates maximum pretrigger
 *--------------------------------------------------------------*/

void
ag54830b_set_trigger_pos( double pos )
{
    char   reply[ 30 ];
    long   length = sizeof reply;
	double time_pos;
	double time_range;
	char   cmd[ 30 ];


	ag54830b_talk( ":TIM:RANG?\n", reply, &length );
    reply[ length - 1 ] = '\0';
	time_range = T_atod( reply );

	time_pos = - pos * time_range;
	sprintf( cmd, ":TIM:POS %8.3E\n", time_pos );
	ag54830b_command( cmd );
}


/*-------------------------------------------------------------------*
 * Returns the current trigger position in the intervall [0,1] where
 * 0 means no pretrigger while 1 indicates maximum pretrigger
 *-------------------------------------------------------------------*/

double
ag54830b_get_trigger_pos( void )
{
    char   reply[ 30 ];
    long   length = sizeof reply;
	double time_pos;
	double time_range;


	ag54830b_talk( ":TIM:POS?\n", reply, &length );
    reply[ length - 1 ] = '\0';
	time_pos = - T_atod( reply );

	length = sizeof reply;
	ag54830b_talk( ":TIM:RANG?\n", reply, &length );
    reply[ length - 1 ] = '\0';
	time_range = T_atod( reply );

    return time_pos / time_range ;
}


/*--------------------------------------------------------------*
 * The function is used to translate back and forth between the
 * channel numbers the way the user specifies them in the EDL
 * program and the channel numbers as specified in the header
 * file. When the channel number can't be mapped correctly, the
 * way the function reacts depends on the value of the third
 * argument: If this is UNSET, an error message gets printed
 * and an exception is thrown. If it is SET -1 is returned to
 * indicate the error.
 *--------------------------------------------------------------*/

long
ag54830b_translate_channel( int  dir,
							long channel,
							bool flag )
{
	if ( dir == GENERAL_TO_AG54830B )
	{
		switch ( channel )
		{
			case CHANNEL_CH1 :
				return AG54830B_CH1;

			case CHANNEL_CH2 :
				return AG54830B_CH2;

			case CHANNEL_F1 :
				return AG54830B_F1;

			case CHANNEL_F2 :
				return AG54830B_F2;

			case CHANNEL_F3 :
				return AG54830B_F3;

			case CHANNEL_F4 :
				return AG54830B_F4;

			case CHANNEL_M1 :
				return AG54830B_M1;

			case CHANNEL_M2 :
				return AG54830B_M2;

			case CHANNEL_M3 :
				return AG54830B_M3;

			case CHANNEL_M4 :
				return AG54830B_M4;

			case CHANNEL_LINE :
				return AG54830B_LIN;
		}

		if ( channel > CHANNEL_INVALID && channel < NUM_CHANNEL_NAMES )
		{
			if ( flag )
				return -1;
			print( FATAL, "Digitizer has no channel %s.\n",
				   Channel_Names[ channel ] );
			THROW( EXCEPTION );
		}

		if ( flag )
			return -1;
		print( FATAL, "Invalid channel number %ld.\n", channel );
		THROW( EXCEPTION );
	}
	else
	{
		switch ( channel )
		{
			case AG54830B_CH1 :
				return CHANNEL_CH1;

			case AG54830B_CH2 :
				return CHANNEL_CH2;

			case AG54830B_F1 :
				return CHANNEL_F1;

			case AG54830B_F2 :
				return CHANNEL_F2;

			case AG54830B_F3 :
				return CHANNEL_F3;

			case AG54830B_F4 :
				return CHANNEL_F4;

			case AG54830B_M1 :
				return CHANNEL_M1;

			case AG54830B_M2 :
				return CHANNEL_M2;

			case AG54830B_M3 :
				return CHANNEL_M3;

			case AG54830B_M4 :
				return CHANNEL_M4;

			case AG54830B_LIN :
				return CHANNEL_LINE;

			default :
				print( FATAL, "Internal error detected at %s:%d.\n",
					   __FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	return -1;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
